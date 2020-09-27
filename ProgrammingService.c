#include <stdio.h>
#include "sockets.h"

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(PROGRAMMING_SERVICE);
    IPCSocketConnection* communicationService = acceptIPCConnection(fd, COMMUNICATION_SERVICE);
    if (communicationService == NULL)
        return -1;

    int done = 0;

    while(!done)
    {
        Message msg = receiveMessageIPC(communicationService);
        printf("MESSAGE TYPE:    %d\nMESSAGE LENGTH:  %d\nMESSAGE CONTENT: %s\n", msg.type, msg.length, msg.content);
    }

    closeIPCConnection(communicationService);
    return 0;
}