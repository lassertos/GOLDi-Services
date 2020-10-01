#include "ipcsockets.h"

static int messageHandlerIPC(IPCSocketConnection* ipc)
{
    return 0;
}

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(PROGRAMMING_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
        return -1;

    while(1)
    {
        Message msg = receiveMessageIPC(communicationService);
        printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %ld\nMESSAGE CONTENT: %s\n", msg.type, strlen(msg.content), msg.content);
        free(msg.content);
        if (msg.type == -1)
        {
            communicationService->open = 0;
            closeIPCConnection(communicationService);
            return -1;
        }
        else if (msg.type == 0)
        {
            communicationService->open = 0;
            closeIPCConnection(communicationService);
            return 0;
        }
    }

    return 0;
}