#include "ipcsockets.h"
#include "websockets.h"

static websocketConnection wscClient;
static websocketConnection wscServer;

static void sigint_handler(int sig)
{
	wscClient.interrupted = 1;
    pthread_join(wscClient.thread, NULL);
    wscServer.interrupted = 1;
    pthread_join(wscServer.thread, NULL);
    exit(0);
}

static int handleWebsocketMessageClient(struct lws* wsi, char* message)
{
    printf("WEBSOCKET CLIENT: %s\n", message);
    free(message);
    return 0;
}

static int handleWebsocketMessageServer(struct lws* wsi, char* message)
{
    sendMessageWebsocket(wsi, message);
    printf("WEBSOCKET SERVER: %s\n", message);
    free(message);
    return 0;
}

static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        sleep(1000);
        printf("STILL WORKING HARD!\n");
    }
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

    /*int fd = createIPCSocket(COMMUNICATION_SERVICE);
    IPCSocketConnection* webcamService = connectToIPCSocket(WEBCAM_SERVICE, messageHandlerIPC);
    if (webcamService == NULL)
        return -1;*/
    
    /* Websocket server creation */
    if(websocketPrepareContext(&wscServer, COMMUNICATION_PROTOCOL, GOLDi_SERVERADRESS, 8082, handleWebsocketMessageServer, 1))
    {
        return -1;
    }

    /* Websocket client creation */
    if(websocketPrepareContext(&wscClient, COMMUNICATION_PROTOCOL, GOLDi_SERVERADRESS, GOLDi_SERVERPORT, handleWebsocketMessageClient, 0))
    {
        return -1;
    }

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
        //sendMessageIPC(webcamService, 1, msgContent);
        sendMessageWebsocket(wscClient.wsi, msgContent);
    }

    //closeIPCConnection(webcamService);
    return 0;
}