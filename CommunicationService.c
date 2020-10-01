#include "ipcsockets.h"
#include "websockets.h"

static websocketConnection wscClient;
static websocketConnection wscServer;
static int interrupted = 0;

static void sigint_handler(int sig)
{
	wscClient.interrupted = 1;
    wscServer.interrupted = 1;
    interrupted = 1;
}

static int handleWebsocketMessageClient(struct lws* wsi, char* message, int length)
{
    printf("WEBSOCKET CLIENT: %.*s\n", length, message);
    return length;
}

static int handleWebsocketMessageServer(struct lws* wsi, char* message, int length)
{
    sendMessageWebsocket(wsi, message, length+1);
    printf("WEBSOCKET SERVER: %.*s\n", length, message);
    return length;
}

static void handleWebsocket(websocketConnection* wsc)
{
    while(!wsc->interrupted && lws_service(wsc->context, 0) >= 0);
    wsc->interrupted = 1;
    lws_context_destroy(wsc->context);
	lwsl_user("Completed\n");
    return;
}

int main(int argc, char const *argv[])
{
    /*
    int fd = createIPCSocket(COMMUNICATION_SERVICE);
    IPCSocketConnection* webcamService = connectToIPCSocket(WEBCAM_SERVICE);
    if (webcamService == NULL)
        return -1;
    */
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

    signal(SIGINT, sigint_handler);

	/* schedule the first client connection attempt to happen immediately */
	lws_sul_schedule(wscClient.context, 0, &wscClient.sul, websocketConnectClient, 1);

    /* Thread creation */
    pthread_t websocketServerThread;

    if(pthread_create(&websocketServerThread, NULL, handleWebsocket, &wscServer)) {
        fprintf(stderr, "Error creating IPC thread\n");
        return 1;
    }

    pthread_t websocketClientThread;

    if(pthread_create(&websocketClientThread, NULL, handleWebsocket, &wscClient)) {
        fprintf(stderr, "Error creating Websocket thread\n");
        return 1;
    }

    int rc;
    char buf[4096];

    while( (rc=read(STDIN_FILENO, buf, sizeof(buf))) > 0 && !interrupted)
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
        sendMessageWebsocket(wscClient.wsi, msgContent, strlen(msgContent) + 1);
    }

    pthread_join(websocketServerThread, NULL);
    pthread_join(websocketClientThread, NULL);

    //closeIPCConnection(webcamService);
    exit(0);
}