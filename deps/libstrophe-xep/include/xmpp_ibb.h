#include "strophe.h"

#define XMLNS_IBB "http://jabber.org/protocol/ibb"
/* xmpp_ibb_data_t and xmpp_ibb_session_t is not used in version 1.0 now.
  It is used for multi IBB session handle 
*/
       
typedef struct _xmpp_ibb_data_t {
	char* seq_num;
	char* recv_data;	
	struct _xmpp_ibb_data* next;

} xmpp_ibb_data_t;

typedef struct _xmpp_ibb_session_t {

	xmpp_conn_t* conn;
	char* sid;
	char* szblock_size;
	xmpp_ibb_data_t* ibb_data_queue;
	struct _xmpp_ibb_session_t* next;		

} xmpp_ibb_session_t;  


/* Get IBB Sesseion handle. It is not verified now. */
xmpp_ibb_session_t* XMPP_Get_IBB_Session_Handle(char* szSid);

/* Main callback function to dispatch XEP-0047 open/data/close stanzas    */
int XMPP_IBB_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

/* Process XEP-0047 data,and decode base64 data*/
int XMPP_IBB_data_process(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

/* Send XEP-0047 Data */
void XMPP_IBB_SendPayload(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza,
void * const userdata, char* resp);
 
 /* Not used now */
void XMPP_IBB_Data_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

/* Send XEP-0047 result stanza */
void XMPP_IBB_Ack_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

/* Send XEP-0047 close stanza to close session */
void XMPP_IBB_Close_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata);

/* Get recently IBB received data buffer. This is temporality function and departed when multi session function is supported*/
char* XMPP_IBB_Get_Recv();
/* Clean IBB received data buffer. This is temporality function and departed when multi session function is supported. */
void XMPP_IBB_Reset_Recv();
/* Get recently IBB received stanza handle. This is temporality function and departed when multi session function is supported. */
xmpp_stanza_t* XMPP_IBB_Get_gStanza();
/* Clear received Stanza. This is temporality function and departed when multi session function is supported.*/
void XMPP_IBB_Reset_gStanza();


