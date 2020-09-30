#include <pthread.h>
#include "websockets.h"
#include "ipcsockets.h"

static websocketConnection wsc;

static void sigint_handler(int sig)
{
	wsc.interrupted = 1;
}

static void handleIPCSocket(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            Message msg = receiveMessageIPC(ipcsc);
            printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %ld\nMESSAGE CONTENT: %s\n", msg.type, strlen(msg.content), msg.content);
            free(msg.content);
            if (msg.type == -1)
            {
                ipcsc->open = 0;
                closeIPCConnection(ipcsc);
                return -1;
            }
            else if (msg.type == 0)
            {
                ipcsc->open = 0;
                closeIPCConnection(ipcsc);
                return 0;
            }
        }
    }
}

static void handleWebsocket()
{
    while(!wsc.interrupted && lws_service(wsc.context, 0) >= 0);
    wsc.interrupted = 1;
    lws_context_destroy(wsc.context);
	lwsl_user("Completed\n");
}

static int handleWebsocketMessage(char* message)
{
    printf("WEBSOCKET: %s\n", message);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

    /* IPC socket creation */
    int fd = createIPCSocket(WEBCAM_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE);
    if (communicationService == NULL)
        return -1;

    /* Websocket creation */
    if(websocketPrepareContext(&wsc, WEBCAM_PROTOCOL, GOLDi_SERVERADRESS, GOLDi_SERVERPORT, handleWebsocketMessage))
    {
        return -1;
    }
    lws_cmdline_option_handle_builtin(argc, argv, &wsc.info);

	/* schedule the first client connection attempt to happen immediately */
	lws_sul_schedule(wsc.context, 0, &wsc.sul, websocketConnectClient, 1);

    /* Thread creation */
    pthread_t ipcThread;

    if(pthread_create(&ipcThread, NULL, handleIPCSocket, communicationService)) {
        fprintf(stderr, "Error creating IPC thread\n");
        return 1;
    }

    pthread_t websocketThread;

    if(pthread_create(&websocketThread, NULL, handleWebsocket, NULL)) {
        fprintf(stderr, "Error creating Websocket thread\n");
        return 1;
    }

    pthread_join(ipcThread, NULL);
    pthread_join(websocketThread, NULL);

	return 0;
}