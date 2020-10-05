#include "ipcsockets.h"

IPCSocketConnection* communicationService;

static int messageHandlerIPC(IPCSocketConnection* ipcsc)
{
    return 0;
}

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(PROGRAMMING_SERVICE);
    communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE, messageHandlerIPC);
    if (communicationService == NULL)
        return -1;

    while(1)
    {
        Message msg = receiveMessageIPC(communicationService);
        printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
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