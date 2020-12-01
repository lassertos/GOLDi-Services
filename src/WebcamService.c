#include <pthread.h>
#include "interfaces/websockets.h"
#include "interfaces/ipcsockets.h"
#include "parsers/json.h"
#include "logging/log.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static const char* ffmpegCommandBlueprint = "ffmpeg -video_size 640x480 -framerate 20 -f %s -i %s -y -frames:v 1 /tmp/GOLDiServices/WebcamService/currentFrame.jpg";
static char* ffmpegCommand;
static IPCSocketConnection* communicationService;
static websocketConnection wsc;
static volatile unsigned int initialized = 0;

/*
 *  this struct contains the information needed to fetch images using ffmpeg
 *  device  - this contains the device-string needed by ffmpeg e.g. v4l2 or x11grab
 *  address - this contains the address of the used camera 
 *  active  - is used to indicate whether we should send images
 *  both strings are zero-terminated
 */
struct
{
    char*        device;    
    char*        address;
    unsigned int id;
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
                        //creating temporary folder for images TODO maybe add to utils
                        struct stat st = {0};

                        if (stat("/tmp/GOLDiServices", &st) == -1) 
                        {
                            if (mkdir("/tmp/GOLDiServices", 0755) == -1)
                            {
                                //TODO errorhandling
                            }
                        }

                        if (stat("/tmp/GOLDiServices/WebcamService", &st) == -1) 
                        {
                            if (mkdir("/tmp/GOLDiServices/WebcamService", 0755) == -1)
                            {
                                //TODO errorhandling
                            }
                        } 

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
                        
                        //currently all the options of the configurationfile are unused TODO readd functionality 
                        ffmpegCommand = malloc(strlen(ffmpegCommandBlueprint) + strlen(CameraData.device) + strlen(CameraData.address) + 1);
                        sprintf(ffmpegCommand, ffmpegCommandBlueprint, CameraData.device, CameraData.address);

                        JSONDelete(msgJSON);
                        sendMessageIPC(ipcsc, IPCMSGTYPE_INITWEBCAMSERVICEFINISHED, serializeInt(1), 1);

                        initialized = 1;
                        break;
                    }

                    case IPCMSGTYPE_INTERRUPTED:
                    {
                        ipcsc->open = 0;
                        closeIPCConnection(ipcsc);
                        free(msg.content);
                        return -1;
                        break;
                    }

                    case IPCMSGTYPE_CLOSEDCONNECTION:
                    {
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

unsigned int getLengthOfNumber(unsigned long long number)
{
    int length = 1;
    while ((number / 10) > 0)
    {
        length++;
        number /= 10;
    }
    return length;
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
    printf("creating websocket\n");
    if(websocketPrepareContext(&wsc, WEBCAM_PROTOCOL, GOLDi_SERVERADDRESS, GOLDi_WEBCAMPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

    while(!(wsc.connectionEstablished && initialized));

    //delete possible leftover images
    DIR *theFolder = opendir("/tmp/GOLDiServices/WebcamService");
    struct dirent *next_file;
    char filepath[256];
    while ( (next_file = readdir(theFolder)) != NULL )
    {
        // build the path for each file in the folder
        sprintf(filepath, "%s/%s", "/tmp/GOLDiServices/WebcamService", next_file->d_name);
        remove(filepath);
    }
    closedir(theFolder);
    
    pid_t pid;
    switch (pid=fork())
    {
        case -1:   /* error at fork() */ 
            break;
        case  0:   /* childprocess */
            //"-input_format", "yuyv422",
            execl("/usr/bin/ffmpeg", "/usr/bin/ffmpeg", "-f", "v4l2", "-video_size", "640x480", "-i", "/dev/video0", "-vf", "fps=30", "/tmp/GOLDiServices/WebcamService/img%01d.jpg", NULL);
            exit(1);
            break;
        default:   /* parentprocess */
            break;
    }
    sleep(1);
    unsigned int filesize;
    unsigned long long index = 1;
    while (1)
    {
        //system(ffmpegCommand);
        char* filename = malloc(strlen("/tmp/GOLDiServices/WebcamService/img.jpg") + getLengthOfNumber(index) + 1);
        sprintf(filename, "/tmp/GOLDiServices/WebcamService/img%lld.jpg", index);
        char* filename2 = malloc(strlen("/tmp/GOLDiServices/WebcamService/img.jpg") + getLengthOfNumber(index) + 1);
        sprintf(filename2, "/tmp/GOLDiServices/WebcamService/img%lld.jpg", index+1);
        struct stat st = {0};
        while (stat(filename2, &st) < 0)
        {
            usleep(5000);
        }
        char* filecontent = readFile(filename, &filesize);
        char* encodedcontent = encodeBase64(filecontent, filesize);

        JSON* msgJSON = JSONCreateObject();
        JSONAddNumberToObject(msgJSON, "id", CameraData.id);
        JSONAddStringToObject(msgJSON, "img", encodedcontent == 0 ? "" : encodedcontent);

        char* message = JSONPrint(msgJSON);
        sendMessageWebsocket(wsc.wsi, message);
        usleep(25000);
        free(message);

        JSONDelete(msgJSON);
        free(filecontent);
        free(encodedcontent);
        remove(filename);
        free(filename);
        free(filename2);
        index++;
    }

    exit(0);

    pthread_join(communicationService->thread, NULL);
    pthread_join(wsc.thread, NULL);

	return 0;
}