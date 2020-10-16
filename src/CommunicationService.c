#include "interfaces/ipcsockets.h"
#include "interfaces/websockets.h"
#include "utils/utils.h"

static websocketConnection wscClient;
static IPCSocketConnection* protectionService;

static void sigint_handler(int sig)
{
	wscClient.interrupted = 1;
    pthread_join(wscClient.thread, NULL);
    if(protectionService && protectionService->open)
        closeIPCConnection(protectionService);
    exit(0);
}

static int handleWebsocketMessageClient(struct lws* wsi, char* message)
{
    printf("WEBSOCKET CLIENT: %s\n", message);
    cJSON* msgJSON = cJSON_Parse(message);
    cJSON* msgType = cJSON_GetObjectItemCaseSensitive(msgJSON, "MessageType");
    if (cJSON_IsString(msgType) && (msgType->valuestring != NULL))
    {
        if (!strncmp(msgType->valuestring, "SPICommand", strlen("SPICommand")))
        {
            printf("STARTING SPICOMMAND!\n");
            cJSON* spiCommandsJSON = cJSON_GetObjectItemCaseSensitive(msgJSON, "Data");
            char* spiCommands = malloc(1);
            int count = 0;
            cJSON* spiCommand = NULL;
            printf("HALFWAY SPICOMMAND!\n");
            cJSON_ArrayForEach(spiCommand, spiCommandsJSON)
            {
                count++;
                if(count > 1)
                    spiCommands = realloc(spiCommands, count);
                spiCommands[count-1] = spiCommand->valueint;
            }
            sendMessageIPC(protectionService, IPCMSGTYPE_SPICOMMAND, spiCommands, count);
            free(message);
            cJSON_Delete(msgJSON);
        }
    }
    else
    {
        free(message);
        cJSON_Delete(msgJSON);
        return -1;   
    }
    return 0;
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
                printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
                cJSON* SPIanswer;
                cJSON* SPIanswerBytes;
                cJSON* SPIcommand;
                char* SPIcommandBytes;
                switch (msg.type)
                {
                case IPCMSGTYPE_INTERRUPTED:
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    free(msg.content);
                    return -1;
                    break;

                case IPCMSGTYPE_CLOSEDCONNECTION:
                    ipcsc->open = 0;
                    closeIPCConnection(ipcsc);
                    free(msg.content);
                    return 0;
                    break;

                case IPCMSGTYPE_SPIANSWER:
                    SPIanswer = cJSON_CreateObject();
                    SPIanswerBytes = NULL;
                    cJSON_AddStringToObject(SPIanswer, "MessageType", "SPIAnswer");
                    cJSON_AddStringToObject(SPIanswer, "SenderID", wscClient.ID);
                    SPIanswerBytes = cJSON_AddArrayToObject(SPIanswer, "Data");
                    for (int i = 0; i < msg.length; i++)
                    {
                        cJSON_AddNumberToObject(SPIanswerBytes, "", (unsigned char)msg.content[i]);
                    }
                    char* msgJSON = cJSON_Print(SPIanswer);
                    sendMessageWebsocket(wscClient.wsi, msgJSON);
                    free(msgJSON);
                    cJSON_Delete(SPIanswer);
                    break;

                default:
                    break;
                }   
            }
        }
        else
        {
            break;
        }
    }
}

static char* spiCommandMsgWS(char* bytes, int length)
{
    cJSON* SPICommand = cJSON_CreateObject();
    cJSON* SPICommandBytes = NULL;
    cJSON_AddStringToObject(SPICommand, "MessageType", "SPICommand");
    cJSON_AddStringToObject(SPICommand, "SenderID", wscClient.ID);
    SPICommandBytes = cJSON_AddArrayToObject(SPICommand, "Data");
    for (int i = 0; i < length; i++)
    {
        cJSON_AddNumberToObject(SPICommandBytes, "", (unsigned char)bytes[i]);
    }
    char* msg = cJSON_Print(SPICommand);
    sendMessageWebsocket(wscClient.wsi, msg);
    free(msg);
    cJSON_Delete(SPICommand);
}

int main(int argc, char const *argv[])
{

    protectionService = connectToIPCSocket(PROTECTION_SERVICE, messageHandlerIPC);
    if (protectionService == NULL)
        return -1;

    int rc;
    char buf[4096];

    while( (rc=read(STDIN_FILENO, buf, sizeof(buf))) > 0)
    {
        char msgContent[rc+1];
        for (int i = 0; i < rc; i++)
        {
            msgContent[i] = buf[i];
        }
        msgContent[rc] = '\0';
        if (rc < sizeof(buf))
            msgContent[rc-1] = '\0';
        if (!strcmp(msgContent, "close"))
            break;
        sendMessageIPC(protectionService, IPCMSGTYPE_USERBASEDFAULT, msgContent, strlen(msgContent));
    }  

    closeIPCConnection(protectionService);

    pthread_join(protectionService->thread, NULL);

    //closeIPCConnection(webcamService);
    return 0;
}