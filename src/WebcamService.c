#include <pthread.h>
#include "interfaces/websockets.h"
#include "interfaces/ipcsockets.h"
#include "parsers/json.h"
#include "logging/log.h"
#include <gstreamer-1.0/gst/gst.h>
#include <gst/audio/audio.h>
#include <gstreamer-1.0/gst/app/gstappsink.h>
#include <gstreamer-1.0/gst/video/video.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static IPCSocketConnection* communicationService;
static websocketConnection wsc;

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData 
{
    GstElement *pipeline, *videosource, *videoconvert, *appsink;
    websocketConnection* wsc;

    GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/*
 *  this struct contains the information needed to fetch images using ffmpeg
 *  source  - this contains the source to be used by gstreamer
 *  address - this contains the address of the used camera 
 *  active  - is used to indicate whether we should send images
 *  both strings are zero-terminated
 */
struct
{
    char*           source;    
    char*           address;
    unsigned int    active:1;
    unsigned int    id;
    CustomData      gstdata;
} CameraData;

/*
 * the sigint handler, can also be used for cleanup after execution 
 * sig - the signal the program received, can be ignored for our purposes
 */
static void sigint_handler(int sig)
{
    exit(0);
}

/* The appsink has received a buffer */
static GstFlowReturn new_sample (GstElement *sink, CustomData *data) 
{
    GstSample *sample;

    /* Retrieve the buffer */
    g_signal_emit_by_name (sink, "pull-sample", &sample);
    if (sample) {
        /* The only thing we do in this example is print a * to indicate a received buffer */
        GstBuffer* jpegBuffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(jpegBuffer, &map, GST_MAP_READ);
        char* data = (char*)map.data;
        unsigned int filesize;
        char* encodedcontent = encodeBase64(data, map.size);
        JSON* msgJSON = JSONCreateObject();
        JSONAddNumberToObject(msgJSON, "id", CameraData.id);
        JSONAddStringToObject(msgJSON, "img", encodedcontent);
        sendMessageWebsocket(wsc.wsi, encodedcontent);
        if(strlen(encodedcontent) > 0)
            free(encodedcontent);

        gst_buffer_unmap(jpegBuffer, &map);
        return GST_FLOW_OK;
    }

    return GST_FLOW_ERROR;
}

/* This function is called when an error message is posted on the bus */
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) 
{
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);

    g_main_loop_quit (data->main_loop);
}

static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    while(ipcsc->open)
    {
        if(hasMessages(ipcsc))
        {
            Message msg = receiveMessageIPC(ipcsc);
            //log_debug("\nMESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s", msg.type, msg.length, msg.content);
            switch (msg.type)
            {
                case IPCMSGTYPE_INITWEBCAMSERVICE:
                {
                    JSON* msgJSON = JSONParse(msg.content);
                    JSON* cameraTypeJSON = JSONGetObjectItem(msgJSON, "Type");
                    JSON* cameraAddressJSON = JSONGetObjectItem(msgJSON, "Address");
                    JSON* cameraIDJSON = JSONGetObjectItem(msgJSON, "ID");

                    if (!strncmp(cameraTypeJSON->valuestring, "USB", 3))
                    {
                        CameraData.source = "v4l2src";
                    }
                    else if (!strncmp(cameraTypeJSON->valuestring, "IP", 2))
                    {
                        CameraData.source = "udpsrc";
                    }

                    CameraData.address = malloc(strlen(cameraAddressJSON->valuestring)+1);
                    strcpy(CameraData.address, cameraAddressJSON->valuestring);

                    CameraData.id = cameraIDJSON->valueint;

                    GstBus *bus;

                    /* Initialize cumstom data structure */
                    memset (&CameraData.gstdata, 0, sizeof (CameraData.gstdata));
                    CameraData.gstdata.wsc = &wsc;

                    const char* source = CameraData.source;

                    /* Create the elements */
                    CameraData.gstdata.videosource = gst_element_factory_make(source, "video-source");
                    CameraData.gstdata.appsink = gst_element_factory_make("appsink", "app-sink");
                    CameraData.gstdata.videoconvert = gst_element_factory_make("jpegenc", "video-convert");

                    /* Create the empty pipeline */
                    CameraData.gstdata.pipeline = gst_pipeline_new("test-pipeline");

                    if (!CameraData.gstdata.pipeline || !CameraData.gstdata.videosource || !CameraData.gstdata.videoconvert|| !CameraData.gstdata.appsink) {
                        g_printerr ("Not all elements could be created.\n");
                        return -1;
                    }

                    /* Configure appsink */
                    g_object_set (CameraData.gstdata.appsink, "emit-signals", TRUE, NULL);
                    g_signal_connect (CameraData.gstdata.appsink, "new-sample", G_CALLBACK (new_sample), &CameraData.gstdata);

                    /* Link all elements that can be automatically linked because they have "Always" pads */
                    gst_bin_add_many (GST_BIN (CameraData.gstdata.pipeline), CameraData.gstdata.videosource, CameraData.gstdata.videoconvert, CameraData.gstdata.appsink, NULL);
                    if (gst_element_link_many (CameraData.gstdata.videosource, CameraData.gstdata.videoconvert, CameraData.gstdata.appsink, NULL) != TRUE) {
                        g_printerr ("Elements could not be linked.\n");
                        gst_object_unref (CameraData.gstdata.pipeline);
                        return -1;
                    }

                    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
                    bus = gst_element_get_bus (CameraData.gstdata.pipeline);
                    gst_bus_add_signal_watch (bus);
                    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &CameraData.gstdata);
                    gst_object_unref (bus);

                    CameraData.gstdata.main_loop = g_main_loop_new(NULL, FALSE);

                    JSONDelete(msgJSON);
                    sendMessageIPC(communicationService, IPCMSGTYPE_INITWEBCAMSERVICEFINISHED, serializeInt(1), 1);
                    break;
                }

                case IPCMSGTYPE_STARTEXPERIMENT:
                {
                    /* Start playing the pipeline */
                    gst_element_set_state(CameraData.gstdata.pipeline, GST_STATE_PLAYING);

                    /* set GLib Main Loop to run */
                    g_main_loop_run(CameraData.gstdata.main_loop);
                    CameraData.active = 1;
                    break;
                }

                case IPCMSGTYPE_STOPEXPERIMENT:
                {
                    /* Stop playing the pipeline and stop GLib Main Loop */
                    gst_element_set_state(CameraData.gstdata.pipeline, GST_STATE_NULL);
                    g_main_loop_quit(CameraData.gstdata.main_loop);
                    CameraData.active = 0;
                    break;
                }

                case IPCMSGTYPE_INTERRUPTED:
                {
                    CameraData.active = 0;
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    free(msg.content);
                    return -1;
                    break;
                }

                case IPCMSGTYPE_CLOSEDCONNECTION:
                {
                    CameraData.active = 0;
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    free(msg.content);
                    return 0;
                    break;
                }

                default:
                {
                    break;
                }
            }
            free(msg.content);   
        }
    }
    return 0;
}

static int handleWebsocketMessage(struct lws* wsi, char* message)
{
    int result = 0;
    printf("%s\n", message);
    // JSON* msgJSON = JSONParse(message);
    // JSON* msgCommand = JSONGetObjectItem(msgJSON, "Command");

    // switch (msgCommand->valueint)
    // {
    //     default:
    //     {
    //         result = -1;
    //         break;
    //     }
    // }

    // free(message);
    // cJSON_Delete(msgJSON);
    return result;
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* IPC socket creation */
    int fd = createIPCSocket(WEBCAM_SERVICE);
    communicationService = acceptIPCConnection(fd, messageHandlerIPC);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    /* Websocket creation */
    if(websocketPrepareContext(&wsc, WEBSOCKET_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_WEBCAMPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wsc.connectionEstablished);

    pthread_join(communicationService->thread, NULL);
    pthread_join(wsc.thread, NULL);
    /* Free resources */
    gst_element_set_state(CameraData.gstdata.pipeline, GST_STATE_NULL);
    gst_object_unref(CameraData.gstdata.pipeline);
    return 0;
}