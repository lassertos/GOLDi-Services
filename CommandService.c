#include "ipcsockets.h"
#include "spi.h"

static IPCSocketConnection* communicationService;

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
    }
}

int main(int argc, char const *argv[])
{
    if(setupSPIInterface() < 0)
    {
        printf("Setup of SPI-interface failed!\n");
        return -1;
    }

    int fd = createIPCSocket(PROTECTION_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerCommunicationService);
    if (communicationService == NULL)
    {
        printf("Connection to Communication Service could not be established!\n");
        return -1;
    }

    pthread_join(communicationService->thread, NULL);

    closeSPIInterface();

    return 0;
}
