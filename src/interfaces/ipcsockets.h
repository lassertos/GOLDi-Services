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
#include "../utils/utils.h"
#include <signal.h>

#define COMMUNICATION_SERVICE "GOLDiCommunicationService"
#define PROGRAMMING_SERVICE "GOLDiProgrammingService"
#define PROTECTION_SERVICE "GOLDiProtectionService"
#define COMMAND_SERVICE "GOLDiCommandService"
#define INITIALIZATION_SERVICE "GOLDiInitializationService"
#define WEBCAM_SERVICE "GOLDiWebcamService"

#define MAX_MESSAGE_SIZE 64
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
    volatile int    open;
    pthread_mutex_t mutex;
    pthread_t       thread;
} IPCSocketConnection;

//TODO maybe change some of the msgtypes / or merge them and cleanup
typedef enum 
{
    IPCMSGTYPE_INTERRUPTED                          = -1,
    IPCMSGTYPE_CLOSEDCONNECTION                     = 0,
    IPCMSGTYPE_SPICOMMAND                           = 1,
    IPCMSGTYPE_SPIANSWER                            = 2,
    IPCMSGTYPE_INITCOMMANDSERVICE                   = 3,
    IPCMSGTYPE_STARTEXPERIMENT                      = 4,
    IPCMSGTYPE_STOPEXPERIMENT                       = 5,
    IPCMSGTYPE_ENDEXPERIMENT                        = 6,
    IPCMSGTYPE_SENSORDATA                           = 7,
    IPCMSGTYPE_ACTUATORDATA                         = 8,
    IPCMSGTYPE_DELAYBASEDFAULT                      = 9,
    IPCMSGTYPE_DELAYBASEDFAULTACK                   = 10,
    IPCMSGTYPE_DELAYBASEDERROR                      = 11,
    IPCMSGTYPE_USERBASEDERROR                       = 12,
    IPCMSGTYPE_INFRASTRUCTUREBASEDERROR             = 13,
    IPCMSGTYPE_INITPROTECTIONSERVICE                = 16,
    IPCMSGTYPE_STARTINITIALIZATION                  = 18,
    IPCMSGTYPE_INITIALIZATIONFINISHED               = 19,
    IPCMSGTYPE_RUNPHYSICALSYSTEM                    = 20,
    IPCMSGTYPE_STOPPHYSICALSYSTEM                   = 21,
    IPCMSGTYPE_PROGRAMCONTROLUNIT                   = 22,
    IPCMSGTYPE_PROGRAMFPGA                          = 23,
    IPCMSGTYPE_PROGRAMMINGFINISHED                  = 24,
    IPCMSGTYPE_SETUSERVARIABLE                      = 25,
    IPCMSGTYPE_INITPROGRAMMINGSERVICE               = 26,
    IPCMSGTYPE_STOPINITIALIZATION                   = 27,
    IPCMSGTYPE_INITINITIALIZATION                   = 28,
    IPCMSGTYPE_INITPROTECTIONFINISHED               = 29,
    IPCMSGTYPE_INITINITALIZATIONSERVICEFINISHED     = 30,
    IPCMSGTYPE_INITPROGRAMMINGSERVICEFINISHED       = 31,
    IPCMSGTYPE_INITCOMMANDSERVICEFINISHED           = 32,
    IPCMSGTYPE_INITWEBCAMSERVICE                    = 33,
    IPCMSGTYPE_INITWEBCAMSERVICEFINISHED            = 34,
    IPCMSGTYPE_PROGRAMCONTROLUNITFINISHED           = 35,
    IPCMSGTYPE_EXPERIMENTINIT                       = 36,
    IPCMSGTYPE_STOPCOMMANDSERVICE                   = 37,
    IPCMSGTYPE_RETURNCOMMANDSERVICE                 = 38
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

typedef int(*IPCMsgHandler)(IPCSocketConnection* ipcsc);

int createIPCSocket(char* socketname);
IPCSocketConnection* connectToIPCSocket(char* socketname, IPCMsgHandler messageHandler);
IPCSocketConnection* acceptIPCConnection(int fd, IPCMsgHandler messageHandler);
int sendMessageIPC(IPCSocketConnection* ipcsc, MessageType messageType, char* msg, int length);
Message receiveMessageIPC(IPCSocketConnection* ipcsc);
void closeIPCConnection(IPCSocketConnection* ipcsc);
unsigned int hasMessages(IPCSocketConnection* ipcsc);

#endif