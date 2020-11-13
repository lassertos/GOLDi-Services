#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>

//TODO change to real values
#define GOLDi_SERVERADDRESS "192.168.179.24"
#define GOLDi_SERVERPORT 8082

#define COMMUNICATION_PROTOCOL (struct lws_protocols){ "GOLDi-Communication-Protocol", callback_communication, 0, 0 }
#define WEBCAM_PROTOCOL (struct lws_protocols){ "GOLDi-Webcam-Protocol", callback_webcam, 0, 0 }

typedef int(*msgHandler)(struct lws*, char*);
typedef struct 
{
    lws_sorted_usec_list_t              sul;
    struct lws                         *wsi;
    uint16_t                            retry_count;
    int                                 interrupted;
    struct lws_context                 *context;
    struct lws_context_creation_info    info;
    int                                 port;
    char*                               serveraddress;
    msgHandler                          messageHandler;
    int                                 isServer;
    char*                               ID;
    volatile int                        connectionEstablished;
    pthread_t                           thread;
} websocketConnection;

enum WebsocketCommands 
{
    WebsocketCommandNack                    = 0,
    WebsocketCommandAck                     = 1,

    WebsocketCommandDeviceData              = 2,
    WebsocketCommandDeviceRegistered        = 3,

    WebsocketCommandExperimentInit          = 4,
    WebsocketCommandExperimentInitAck       = 5,
    WebsocketCommandExperimentClose         = 6,
    WebsocketCommandExperimentCloseAck      = 7,

    WebsocketCommandExperimentData          = 8,

    WebsocketCommandDirectConnectionInit    = 15,
    WebsocketCommandDirectConnectionAck     = 16,
    WebsocketCommandDirectConnectionFail    = 17,

    WebsocketCommandRunPS                   = 20,
    WebsocketCommandStopPS                  = 21,
    WebsocketCommandSensorData              = 22,
    WebsocketCommandActuatorData            = 23,
    WebsocketCommandLight                   = 24,
    WebsocketCommandUserVariable            = 25,

    WebsocketCommandInitPS                  = 30,
    WebsocketCommandInitPSAck               = 31,
    WebsocketCommandProgramCU               = 32,
    WebsocketCommandProgramCUAck            = 33,
    WebsocketCommandResetCU                 = 34,

    WebsocketCommandDelayFault              = 40,
    WebsocketCommandDelayFaultAck           = 41,
    WebsocketCommandDelayError              = 42,
    WebsocketCommandUserError               = 43,
    WebsocketCommandInfrastructureError     = 44
};

int websocketPrepareContext(websocketConnection* wsc, struct lws_protocols protocol, char* serveraddress, int port, msgHandler messageHandler, int isServer);
int callback_communication(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int callback_webcam(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int sendMessageWebsocket(struct lws *wsi, char* msg);

#endif