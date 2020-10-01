#include "ipcsockets.h"

static IPCSocketConnection communicationService;

static int messageHandlerCommunicationService(IPCSocketConnection* ipcsc)
{
    while(1)
    {
        if(ipcsc->open)
        {
            Message msg = receiveMessageIPC(ipcsc);
            printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %ld\nMESSAGE CONTENT: %s\n", msg.type, strlen(msg.content), msg.content);
            free(msg.content);
            if (msg.type < 0)
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

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(PROTECTION_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
        return -1;

    pthread_join(communicationService->thread, NULL);

    return 0;
}
