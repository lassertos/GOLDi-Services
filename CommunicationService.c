#include "sockets.h"

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(COMMUNICATION_SERVICE);
    IPCSocketConnection* programmingService = connectToIPCSocket(PROGRAMMING_SERVICE);
    if (programmingService == NULL)
        return -1;
    char* message = malloc(64001);

    for (int i = 0; i < 64000; i++) 
    {
        message[i] = 'a';
        message[i+1] = '\0';
        sendMessageIPC(programmingService, 1, message);
    }

    free(message);
    closeIPCConnection(programmingService);
    return 0;
}