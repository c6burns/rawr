#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wksxep0047.h"
#include "xmpp_ibb.h"

#include <strophe.h>
#include "common.h"


/* Call back function to process received presence stanza */
int presence_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
    xmpp_stanza_t *pres;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*)userdata;

    printf("presence handler\n");

    if(strcmp(xmpp_stanza_get_attribute(stanza, "from"), \
             conn->jid) == 0)
    {
	printf("Get Presence of myself, return\n");
        return 1;
    }

    pres = xmpp_stanza_new(ctx);
 
    xmpp_stanza_set_name(pres, "presence");

    xmpp_stanza_set_attribute(pres , "to", xmpp_stanza_get_attribute(stanza, "from")); 
 
    xmpp_send(conn, pres);
    xmpp_stanza_release(pres);
    return 1;

}


/* define a handler for connection events */
/*This is the first callback function called by libstrophe and is used to
register another callback function. This function will be called only ance.
 */
void conn_handler(xmpp_conn_t * const conn, const xmpp_conn_event_t status,
                  const int error, xmpp_stream_error_t * const stream_error,
                  void * const userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    if (status == XMPP_CONN_CONNECT) {       
        fprintf(stderr, "connected Received : [%s]\n", (char *)userdata);    
        
        XMPP_Presence(conn);
        xmpp_handler_add(conn, XMPP_IBB_handler, XMLNS_IBB , \
	"iq", "set", ctx);	 
//        xmpp_handler_add(conn,message_handler, NULL, "message", NULL , ctx); 
       
    }
    else {
        fprintf(stderr, "DEBUG: disconnected\n");        
    }

}

/*Send  IBB "</close>" to XMPP server to terminate  IBB session */
//void XMPP_IBB_Close_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
//{
//
//
//}

/*Send XMPP Presence to XMPP server*/
void XMPP_Presence(xmpp_conn_t* conn)
{

    xmpp_stanza_t* pres;
    xmpp_ctx_t *ctx;

    ctx = xmpp_ctx_new(NULL, NULL);

    /* Send initial <presence/> so that we appear online to contacts */
    pres = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(pres, "presence");

   /* Do not free ctx before unused */    

    xmpp_send(conn, pres);       
    xmpp_stanza_release(pres);
    

}

/*XMPP Init, Get XMPP connection object by JID, Password, Host*/
xmpp_conn_t*  XMPP_Init(char* jid, char* pass, char* host, xmpp_ctx_t  **pctx)
{
    xmpp_conn_t *conn;
    xmpp_log_t *log;

    printf("jid=[%s] pass=[%s] host=[%s]", jid, pass, host);
    xmpp_initialize();

    /* create a context */
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG); /* pass NULL instead to silence output */
    *pctx = xmpp_ctx_new(NULL, log);

    /* create a connection */
    conn = xmpp_conn_new(*pctx);
    /* setup authentication information */
    xmpp_conn_set_jid(conn, jid);
    xmpp_conn_set_pass(conn, pass);

//    /* initiate connection */
//    xmpp_connect_client(conn, host, 0, iq_ibb_open_handler, *pctx);
    xmpp_connect_client(conn, host, 0, conn_handler, *pctx);

       
    return conn;
}


/*Register IBB CB function, not verified.*/
void Register_IBB_CB(xmpp_conn_t* conn, char* host, \
unsigned short port, xmpp_conn_handler callback )
{
    xmpp_ctx_t  *ctx;
    xmpp_log_t *log;	

    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG); /* pass NULL instead to silence output */

    ctx = xmpp_ctx_new(NULL, log);     

   //    /* initiate connection */
    xmpp_connect_client(conn, host, port, XMPP_IBB_handler, ctx);

}

/*Disconnect the connection between XMPP server and shutdown XMPP library*/
void XMPP_Close(xmpp_ctx_t *ctx,  xmpp_conn_t *conn)
{

    if(conn !=NULL)
        xmpp_disconnect(conn);

//    xmpp_handler_delete(conn, message_handler);
    xmpp_handler_delete(conn, XMPP_IBB_handler);  
 
    xmpp_conn_release(conn);

    fprintf(stderr, "Conn release!");
  
    /* final shutdown of the library */
  
   if(ctx!=NULL)
        xmpp_ctx_free(ctx);
   xmpp_shutdown();

}

