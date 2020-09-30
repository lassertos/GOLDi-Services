#ifndef WEBSOCKETS_H
#define WEBSOCKETS_H

#include <libwebsockets.h>
#include <cjson/cJSON.h>
#include <signal.h>

#define GOLDi_SERVERADRESS "192.168.179.24"
#define GOLDi_SERVERPORT 8081

#define COMMUNICATION_PROTOCOL (struct lws_protocols){ "GOLDi-Communication-Protocol", callback_communication, 0, 0 }
#define WEBCAM_PROTOCOL (struct lws_protocols){ "GOLDi-Webcam-Protocol", callback_webcam, 0, 0 }

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
    int                                 (*messageHandler)(char*);
} websocketConnection;

void websocketConnectClient(lws_sorted_usec_list_t *sul);
int websocketPrepareContext(websocketConnection* wsc, struct lws_protocols protocol, char* serveraddress, int port, int (*messageHandler)(char*));
int callback_communication(struct lws *wsi, enum lws_callback_reasons reason,
		                    void *user, void *in, size_t len);
int callback_webcam(struct lws *wsi, enum lws_callback_reasons reason,
		                    void *user, void *in, size_t len);

#endif