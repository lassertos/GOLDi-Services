#ifndef IPCSOCKETS_H
#define IPCSOCKETS_H

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
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
    int             fd;
    char*           socketname;
    char*           buffer;
    int             open;
    pthread_t       thread;
} IPCSocketConnection;

typedef enum 
{
    IPCMSGTYPE_INTERRUPTED              = -1,
    IPCMSGTYPE_CLOSEDCONNECTION         = 0,
    IPCMSGTYPE_SPICOMMAND               = 1,
    IPCMSGTYPE_SPIANSWER                = 2,
    IPCMSGTYPE_INITEXPERIMENT           = 3,
    IPCMSGTYPE_STARTEXPERIMENT          = 4,
    IPCMSGTYPE_STOPEXPERIMENT           = 5,
    IPCMSGTYPE_ENDEXPERIMENT            = 6,
    IPCMSGTYPE_SENSORDATA               = 7,
    IPCMSGTYPE_ACTUATORDATA             = 8,
    IPCMSGTYPE_DELAYBASEDFAULT          = 9,
    IPCMSGTYPE_DELAYBASEDFAULTACK       = 10,
    IPCMSGTYPE_USERBASEDFAULT           = 11,
    IPCMSGTYPE_INFRASTRUCTUREBASEDFAULT = 12
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
 *  for messages that are too long for a single transmission. TODO: move to c-file
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
unsigned int hasMessages(IPCSocketConnection* ipcsc);

#endif