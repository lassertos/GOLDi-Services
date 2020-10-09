#ifndef IPCSOCKETS_H
#define IPCSOCKETS_H

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define COMMUNICATION_SERVICE "GOLDiCommunicationService"
#define PROGRAMMING_SERVICE "GOLDiProgrammingService"
#define PROTECTION_SERVICE "GOLDiProtectionService"
#define COMMAND_SERVICE "GOLDiCommandService"
#define INITIALIZATION_SERVICE "GOLDiInitializationService"
#define WEBCAM_SERVICE "GOLDiWebcamService"

#define MAX_MESSAGE_SIZE 1024
#define MESSAGE_HEADER_SIZE 16

/*
 *  Describes a connection to another Service via an IPC socket:
 *  fd - File Descriptor of the open socket
 *  socketname - a name for the socket
 *  buffer - used for sending data over the socket
 *  open - indicates if the connection is still active
 */
typedef struct 
{
    int         fd;
    char*       socketname;
    char*       buffer;
    int         open;
    pthread_t   thread;
} IPCSocketConnection;

typedef enum 
{
    MSGTYPE_INTERRUPTED = -1,
    MSGTYPE_CLOSEDCONNECTION = 0,
    MSGTYPE_SPICOMMAND = 1,
    MSGTYPE_SPIANSWER = 2
} MessageType;

/*
 *  A message with type and content.
 */
typedef struct
{
    MessageType     type;
    int             length;
    char*           content;
} Message;

/*
 *  A message for correct communication over the socket. The additional parameters are important 
 *  for messages that are too long for a single transmission.
 */
typedef struct
{
    MessageType type;
    int         length;
    int         fragmentNumber;
    int         isLastFragment;
    char*       content;
} SocketMessage;

typedef int(*IPCmsgHandler)(IPCSocketConnection* ipcsc);

int createIPCSocket();
IPCSocketConnection* connectToIPCSocket(char* socketname, IPCmsgHandler messageHandler);
IPCSocketConnection* acceptIPCConnection(int fd, char* socketname, IPCmsgHandler messageHandler);
int sendMessageIPC(IPCSocketConnection* ipcsc, MessageType messageType, char* msg, int length);
Message receiveMessageIPC(IPCSocketConnection* ipcsc);
void closeIPCConnection(IPCSocketConnection* ipcsc);

#endif