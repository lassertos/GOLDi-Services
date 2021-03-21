#include "websockets.h"
#include "../logging/log.h"

/*
 *  The retry and backoff policy we want to use for our client connections
 */
static const uint32_t backoff_ms[] = { 5000 };

static const lws_retry_bo_t retry = {
	.retry_ms_table			    = backoff_ms,
	.retry_ms_table_count	    = LWS_ARRAY_SIZE(backoff_ms),
	.conceal_count			    = LWS_RETRY_CONCEAL_ALWAYS,

	.secs_since_valid_ping		= 3,  /* force PINGs after secs idle */
	.secs_since_valid_hangup	= 10, /* hangup after secs idle */

	.jitter_percent			    = 20,
};

/*
 *	The function for the thread of a websocket connection 
 */
static void handleWebsocket(websocketConnection* wsc)
{
    while(!wsc->interrupted && (lws_service(wsc->context, 0) >= 0));
    wsc->interrupted = 1;
    lws_context_destroy(wsc->context);
	pthread_exit(NULL);
	lwsl_user("Completed\n");
    return;
}

/*
 *  Connects to the specified address and port
 */ 
static void websocketConnectClient(lws_sorted_usec_list_t *sul)
{
	websocketConnection *wsc = lws_container_of(sul, websocketConnection, sul);
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof(i));

	i.context = wsc->context;
	i.port = wsc->port;
	i.address = wsc->serveraddress;
	i.path = "/";
	i.host = i.address;
	i.origin = i.address;
	i.pwsi = &wsc->wsi;
	i.retry_and_idle_policy = &retry;
	i.userdata = wsc;

	if (!lws_client_connect_via_info(&i))
    {
		/*
		 * Failed... schedule a retry... we can't use the _retry_wsi()
		 * convenience wrapper api here because no valid wsi at this
		 * point.
		 */
		if (lws_retry_sul_schedule(wsc->context, 0, sul, &retry,
					   websocketConnectClient, &wsc->retry_count)) {
			lwsl_err("%s: connection attempts exhausted\n", __func__);
			wsc->interrupted = 1;
		}
    }
}

int websocketPrepareContext(websocketConnection* wsc, struct lws_protocols protocol, char* serveraddress, int port, websocketMsgHandler messageHandler, int isServer)
{
    struct lws_context_creation_info info;
    struct lws_protocols protocols[] = {
	    protocol,
	    { NULL, NULL, 0, 0 }
    };
	memset(&info, 0, sizeof info);
	int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;
    lws_set_log_level(logs, NULL);
	
	if(isServer)
	{
		info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
		info.port = port;
		info.user = wsc;
	}
	else
	{
		info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
	}
		
	info.protocols = protocols;

	wsc->context = lws_create_context(&info);
	if (!wsc->context) {
		lwsl_err("lws init failed\n");
		return -1;
	}
    wsc->info = info;
    wsc->serveraddress = serveraddress;
    wsc->port = port;
    wsc->messageHandler = messageHandler;

	if (!isServer)
	{
		lws_sul_schedule(wsc->context, 0, &wsc->sul, websocketConnectClient, 1);
	}

	if(pthread_create(&wsc->thread, NULL, &handleWebsocket, wsc)) {
        fprintf(stderr, "Error creating Websocket thread\n");
        return -1;
    }

    return 0;
}

int sendMessageWebsocket(struct lws *wsi, char* msg)
{
	int length = strlen(msg);
	unsigned char* message = malloc(LWS_PRE + length);
	for (int i = 0; i < length; i++)
	{
		message[LWS_PRE + i] = msg[i];
	}
	int m = lws_write(wsi, message+LWS_PRE, length, LWS_WRITE_TEXT);
	if (m < length) {
		free(message);
		lwsl_err("ERROR %d writing to ws\n", m);
		return -1;
	}
	free(message);
	return 0;
}

static char* connectMessage(char* ID)
{
	cJSON* connectMsg = cJSON_CreateObject();
	cJSON_AddStringToObject(connectMsg, "message", "Connect");
	cJSON_AddStringToObject(connectMsg, "senderID", ID);
	char* msg = cJSON_Print(connectMsg);
	cJSON_Delete(connectMsg);
	return msg; 
}

int callback(struct lws *wsi, enum lws_callback_reasons reason,
		                    void *user, void *in, size_t len)
{
	websocketConnection *wsc;
	if (user != NULL)
		wsc = (websocketConnection*)user;
	else
		wsc = (websocketConnection*)lws_context_user(lws_get_context(wsi));

	char* message;

	switch (reason) 
	{
		case LWS_CALLBACK_PROTOCOL_INIT:
			break;

		case LWS_CALLBACK_ESTABLISHED:
			break;

		case LWS_CALLBACK_CLOSED:
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			break;

		case LWS_CALLBACK_RECEIVE:
			log_debug("received websocket message");
			message = malloc(len + 1);
			memcpy(message, (char*)in, len);
			message[len] = '\0';
			wsc->messageHandler(wsi, message);
			break;

		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
				in ? (char *)in : "(null)");
			goto do_retry;
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
			log_debug("received websocket message");
			message = malloc(len + 1);
			memcpy(message, (char*)in, len);
			message[len] = '\0';
			wsc->messageHandler(wsi, message);
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			wsc->connectionEstablished = 1;
			lwsl_user("%s: established\n", __func__);
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			goto do_retry;

		default:
			break;
	}

	//return lws_callback_http_dummy(wsi, reason, user, in, len);
	return 0;

do_retry:
	/*
	 * retry the connection to keep it nailed up
	 *
	 * For this example, we try to conceal any problem for one set of
	 * backoff retries and then exit the app.
	 *
	 * If you set retry.conceal_count to be larger than the number of
	 * elements in the backoff table, it will never give up and keep
	 * retrying at the last backoff delay plus the random jitter amount.
	 */
	if (lws_retry_sul_schedule_retry_wsi(wsi, &wsc->sul, websocketConnectClient,
					     &wsc->retry_count)) {
		lwsl_err("%s: connection attempts exhausted\n", __func__);
		wsc->interrupted = 1;
	}

	return 0;
}