#include <pthread.h>
#include "interfaces/websockets.h"
#include "interfaces/ipcsockets.h"
#include "parsers/json.h"
#include "logging/log.h"

static const char* ffmpegCommandBlueprint = "ffmpeg -video_size 640x480 -framerate 20 -f %s -i %s -y -frames:v 1 /tmp/GOLDiServices/WebcamService/currentFrame.jpg";
static char* ffmpegCommand;
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
                        log_debug("received initialization message");
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
                        ffmpegCommand = malloc(strlen(ffmpegCommandBlueprint) + strlen(CameraData.device) + strlen(CameraData.address) + 1);
                        sprintf(ffmpegCommand, ffmpegCommandBlueprint, CameraData.device, CameraData.address);
                        sendMessageIPC(ipcsc, IPCMSGTYPE_INITWEBCAMSERVICEFINISHED, serializeInt(1), 1);
                        break;
                    }

                    case IPCMSGTYPE_STARTEXPERIMENT:
                    {
                        log_debug("received start experiment message");
                        CameraData.active = 1;
                        break;
                    }

                    case IPCMSGTYPE_STOPEXPERIMENT:
                    {
                        log_debug("received stop experiment message");
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
                        log_error("received message of unknown type");
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

//TODO maybe delete if it causes problems
static int handleWebsocketMessage(struct lws* wsi, char* message)
{
    int result = 0;
    JSON* msgJSON = JSONParse(message);
    JSON* msgCommand = JSONGetObjectItem(msgJSON, "Command");

    switch (msgCommand->valueint)
    {
        default:
        {
            result = -1;
            break;
        }
    }

    free(message);
    cJSON_Delete(msgJSON);
    return result;
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

    /* IPC socket creation */
    int fd = createIPCSocket(WEBCAM_SERVICE);
    communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
    {
        log_error("connection to Communication Service could not be established");
        return -1;
    }

    /* Websocket creation */
    if(websocketPrepareContext(&wsc, WEBCAM_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_WEBCAMPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!wsc.connectionEstablished);

    unsigned int filesize;
    while (1)
    {
        if (CameraData.active)
        {
            system(ffmpegCommand);
            char* filecontent = readFile("/tmp/GOLDiServices/WebcamService/currentFrame.jpg", &filesize);
            char* encodedcontent = encodeBase64(filecontent, filesize);
            JSON* msgJSON = JSONCreateObject();
            JSONAddNumberToObject(msgJSON, "id", CameraData.id);
            JSONAddStringToObject(msgJSON, "img", encodedcontent);
            sendMessageWebsocket(wsc.wsi, encodedcontent);
            free(filecontent);
            if(strlen(encodedcontent) > 0);
                free(encodedcontent);
            usleep(30000);
        }
    }

    pthread_join(communicationService->thread, NULL);
    pthread_join(wsc.thread, NULL);

	return 0;
}