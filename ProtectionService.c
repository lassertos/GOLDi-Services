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
            printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
            char* spiAnswer;
            switch (msg.type)
            {
            case MSGTYPE_INTERRUPTED:
                ipcsc->open = 0;
                closeIPCConnection(ipcsc);
                free(msg.content);
                return -1;
                break;

            case MSGTYPE_CLOSEDCONNECTION:
                ipcsc->open = 0;
                closeIPCConnection(ipcsc);
                free(msg.content);
                return 0;
                break;

            case MSGTYPE_SPICOMMAND:
                spiAnswer = malloc(msg.length);
                memcpy(spiAnswer, msg.content, msg.length);
                executeSPICommand(spiAnswer, msg.length);
                sendMessageIPC(ipcsc, MSGTYPE_SPIANSWER, spiAnswer, msg.length);
                free(msg.content);
                free(spiAnswer);
                break;
            
            default:
                break;
            }
        }
        else
        {
            break;
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