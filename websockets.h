#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <signal.h>
#include <assert.h>

#define GOLDi_SERVERADRESS "192.168.179.24"
#define GOLDi_SERVERPORT 8081

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
    pthread_t                           thread;
} websocketConnection;

int websocketPrepareContext(websocketConnection* wsc, struct lws_protocols protocol, char* serveraddress, int port, msgHandler messageHandler, int isServer);
int callback_communication(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int callback_webcam(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
int sendMessageWebsocket(struct lws *wsi, char* msg);

#endif