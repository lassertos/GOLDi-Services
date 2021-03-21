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

/*
 *  this struct contains the information needed to fetch images using ffmpeg
 *  device  - this contains the device-string needed by ffmpeg e.g. v4l2 or x11grab
 *  address - this contains the address of the used camera 
 *  active  - is used to indicate whether we should send images
 *  both strings are zero-terminated
 */
struct
{
    char*           device;    
    char*           address;
    unsigned int    active:1;
    unsigned int    id;
} CameraData;

/*
 * the sigint handler, can also be used for cleanup after execution 
 * sig - the signal the program received, can be ignored for our purposes
 */
static void sigint_handler(int sig)
{
    exit(0);
}

static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
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
                            CameraData.device = "v4l2";
                        }

                        CameraData.address = malloc(strlen(cameraAddressJSON->valuestring)+1);
                        strcpy(CameraData.address, cameraAddressJSON->valuestring);

                        CameraData.id = cameraIDJSON->valueint;

                        JSONDelete(msgJSON);
                        sendMessageIPC(communicationService, IPCMSGTYPE_INITWEBCAMSERVICEFINISHED, serializeInt(1), 1);
                        break;
                    }

                    case IPCMSGTYPE_STARTEXPERIMENT:
                    {
                        CameraData.active = 1;
                        break;
                    }

                    case IPCMSGTYPE_STOPEXPERIMENT:
                    {
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
        else
        {
            break;
        }
    }
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

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData 
{
    GstElement *pipeline, *videosource, *videoconvert, *appsink;
    websocketConnection* wsc;

    GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

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

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

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

    CustomData data;
    GstBus *bus;

    /* Initialize cumstom data structure */
    memset (&data, 0, sizeof (data));
    data.wsc = &wsc;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    data.videosource = gst_element_factory_make("v4l2src", "video-source");
    data.appsink = gst_element_factory_make("appsink", "app-sink");
    data.videoconvert = gst_element_factory_make("jpegenc", "video-convert");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.videosource || !data.videoconvert|| !data.appsink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Configure appsink */
    g_object_set (data.appsink, "emit-signals", TRUE, NULL);
    g_signal_connect (data.appsink, "new-sample", G_CALLBACK (new_sample), &data);

    /* Link all elements that can be automatically linked because they have "Always" pads */
    gst_bin_add_many (GST_BIN (data.pipeline), data.videosource, data.videoconvert, data.appsink, NULL);
    if (gst_element_link_many (data.videosource, data.videoconvert, data.appsink, NULL) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus (data.pipeline);
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);
    gst_object_unref (bus);

    /* Start playing the pipeline */
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

    /* Create a GLib Main Loop and set it to run */
    data.main_loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (data.main_loop);

    /* Free resources */
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    pthread_join(communicationService->thread, NULL);
    pthread_join(wsc.thread, NULL);
    return 0;
}