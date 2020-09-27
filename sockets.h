#ifndef SOCKETS_H
#define SOCKETS_H

#include <unistd.h>

#define COMMUNICATION_SERVICE "GOLDiCommunicationService"
#define PROGRAMMING_SERVICE "GOLDiProgrammingService"
#define PROTECTION_SERVICE "GOLDiProtectionService"
#define INITIALIZATION_SERVICE "GOLDiInitializationService"

typedef struct 
{
    int fd;
    char* socketname;
    int id;
    char* buffer;
} IPCSocketConnection;

typedef struct
{
    int type;
    int length;
    char* content;
} Message;

int createIPCSocket();
IPCSocketConnection* connectToIPCSocket(char* socketname);
IPCSocketConnection* acceptIPCConnection(int fd, char* socketname);
int sendMessageIPC(IPCSocketConnection* ipcsc, int messageType, char* msg);
Message receiveMessageIPC(IPCSocketConnection* ipcsc);
void closeIPCConnection(IPCSocketConnection* ipcsc);

#endif