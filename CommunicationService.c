#include "ipcsockets.h"

int main(int argc, char const *argv[])
{
    int fd = createIPCSocket(COMMUNICATION_SERVICE);
    IPCSocketConnection* webcamService = connectToIPCSocket(WEBCAM_SERVICE);
    if (webcamService == NULL)
        return -1;

    int rc;
    char buf[4096];

    while( (rc=read(STDIN_FILENO, buf, sizeof(buf))) > 0)
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
        sendMessageIPC(webcamService, 1, msgContent);
    }

    closeIPCConnection(webcamService);
    return 0;
}