#include "strophe.h"

#define MAX_COAP_URI_LEN 100
#define MAX_METHOD_LEN 10
#define MAX_JSON_PAYLOAD_LEN 200
#define MAX_HEAD_LEN	128
#define MAX_LEN 50

/* Create a libstrophe context, create a connection, initiate connection */
xmpp_conn_t*  XMPP_Init(char* jid, char* pass, char* host, xmpp_ctx_t  **pctx);

void XMPP_Presence(xmpp_conn_t* conn);

//void XMPP_IBB_SendPayload(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata, char *);
/* Send a presence to XMPP Server*/
void XMPP_Echo_Test(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);
/* Send XEP-0047 result stanza */
void XMPP_IBB_Ack_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

void XMPP_XEP0047_Init(xmpp_conn_t* conn, xmpp_ctx_t* ctx);

int XMPP_xep0047_data_process(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);


