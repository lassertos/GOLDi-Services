#include <pthread.h>
#include "websockets.h"
#include "ipcsockets.h"

static websocketConnection wsc;

static void sigint_handler(int sig)
{
	wsc.interrupted = 1;
}

static int messageHandlerCommunicationService(IPCSocketConnection* ipcsc)
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
        else
        {
            break;
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

static int handleWebsocketMessage(char* message, int length)
{
    printf("WEBSOCKET: %.*s\n", length, message);
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, sigint_handler);

    /* IPC socket creation */
    int fd = createIPCSocket(WEBCAM_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    /* Websocket creation */
    /*if(websocketPrepareContext(&wsc, WEBCAM_PROTOCOL, GOLDi_SERVERADRESS, GOLDi_SERVERPORT, handleWebsocketMessage, 0))
    {
        return -1;
    }

	/* schedule the first client connection attempt to happen immediately */
    /*
	lws_sul_schedule(wsc.context, 0, &wsc.sul, websocketConnectClient, 1);

    pthread_t websocketThread;

    if(pthread_create(&websocketThread, NULL, handleWebsocket, NULL)) {
        fprintf(stderr, "Error creating Websocket thread\n");
        return 1;
    }
    */

    pthread_join(communicationService->thread, NULL);
    //pthread_join(websocketThread, NULL);

	return 0;
}