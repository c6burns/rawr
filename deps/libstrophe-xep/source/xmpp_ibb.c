#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wksxep0047.h"
#include "xmpp_ibb.h"

#include <strophe.h>
#include "common.h"

xmpp_ibb_session_t* gXMPP_IBB_handle;
char* gRecv=NULL;
xmpp_stanza_t* gStanza=NULL;

/*
* Main callback function to dispatch XEP-0047 open/data/close stanzas 
*   @param conn   a Strophe connection object  
*   @param stanza  a Strophe stanza object received from sender.
*   @param userdata  a callback interface to pass xmpp_ctx. 
*/
int XMPP_IBB_handler(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
    char*  szBlock_size;
    char*  szSid;

	
    printf("XMPP IBB Handler\n");

    if( xmpp_stanza_get_child_by_name(stanza, "open") != NULL)
    {

        printf(" =====XEP0047 Open Handle\n");
        szBlock_size = xmpp_stanza_get_attribute(xmpp_stanza_get_child_by_name(stanza, "open"), "block-size");
        szSid =  \
            xmpp_stanza_get_attribute(xmpp_stanza_get_child_by_name(stanza, "open"), "sid");
    printf("XEP0047 IQ blocksize=%s sid=%s \n",szBlock_size, szSid );

#if 0   // The is used to support multi sesssion. not verified. 
	xmpp_ibb_session_t* ibb_ssn_p;	
	ibb_ssn_p = malloc(sizeof(xmpp_ibb_session_t));
        ibb_ssn_p->sid = malloc(strlen(szSid)+1);		
	ibb_ssn_p->szblock_size = malloc(strlen(szBlock_size)+1);
	ibb_ssn_p->ibb_data_queue = NULL;
        ibb_ssn_p->next = NULL;
	
	strcpy(ibb_ssn_p->sid, szBlock_size);
	strcpy(ibb_ssn_p->szblock_size, szSid);

	//=================> gXMPP_IBB_handle-> next = ibb_ssn_p;
	XMPP_IBB_Add_Session_Queue(ibb_ssn_p);
#endif

        XMPP_IBB_Ack_Send(conn, stanza, userdata);

    }
    else  if( xmpp_stanza_get_child_by_name(stanza, "data") != NULL)
    {
        XMPP_IBB_Ack_Send(conn, stanza, userdata);
        printf("========XEP0047 Data process\n");
        XMPP_IBB_Data_Process(conn, stanza  , userdata);   
       
    }
    else if( xmpp_stanza_get_child_by_name(stanza, "close")  )
    {
        XMPP_IBB_Ack_Send(conn, stanza, userdata);
    }
    return 1;
}

/* Process XEP-0047 data,and decode base64 data*/
int XMPP_IBB_Data_Process(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{

    char* intext;
    unsigned char *result;
    xmpp_ctx_t *ctx = (xmpp_ctx_t*)userdata;
    
 
    intext = xmpp_stanza_get_text(xmpp_stanza_get_child_by_name(stanza, "data"));

    printf("[Raw Data=%s]\n",  intext);
    result = xmpp_base64_decode_str(ctx, intext, strlen(intext));
    printf("Decode result=%s\n", result);

    gRecv = malloc(strlen(result)+1);
    strcpy(gRecv, result);
    
    if(gStanza == NULL)
        gStanza = xmpp_stanza_copy(stanza);

#if 0  //data queue function has not been verified.  
	            
    ibb_ssn = XMPP_Get_IBB_Session_Handle(szSid);

    if( ibb_ssn == NULL)		
    {
        printf("Opened Session ID not found\n"); 
	goto error;
    }

    xmpp_ibb_data_t* ibb_data_new = malloc(sizeof(xmpp_ibb_data_t));
    ibb_data_new->seq_num = malloc(strlen(szSeq)+1);
    ibb_data_new->recv_data =  malloc(strlen(result)+1);
    
    strcpy(ibb_data_new->seq_num, szSeq);   
    strcpy(ibb_data_new->recv_data, result);
   
    XMPP_IBB_Add_Session_Data_Queue(ibb_ssn, ibb_data_new);
#endif

error:
    
    xmpp_free(ctx, intext);
    xmpp_free(ctx, result);

    return 1;
}

/*  Send XEP-0047 Data 
*   @param conn   a Strophe connection object 
*   @param stanza  received stanza from sender. It implies sid and sender "from"
*   @param userdata  pass xmpp_ctx
*   @param resp Data in string to be sent.
*/

void XMPP_IBB_SendPayload(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, 
void * const userdata, char* resp )
{

    static int seq = 0;
    int data_seq = 0;
    char Data_Seq_Buf[32];
    char ID_Buf[32];
    char* encoded_data;
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    xmpp_stanza_t *iq, *data,  *text;

    iq  = xmpp_stanza_new(ctx);
    data = xmpp_stanza_new(ctx);
    text = xmpp_stanza_new(ctx);

    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "set");

    sprintf(ID_Buf, "ID-seq-%d", seq);
    seq++;
    xmpp_stanza_set_id(iq, ID_Buf);
    xmpp_stanza_set_attribute(iq, "to", xmpp_stanza_get_attribute(stanza, "from"));
    xmpp_stanza_set_attribute(iq, "from", xmpp_stanza_get_attribute(stanza, "to"));

    xmpp_stanza_set_name(data, "data");

    xmpp_stanza_set_ns(data, XMLNS_IBB);

    xmpp_stanza_set_attribute(data, "sid",   \
    xmpp_stanza_get_attribute(xmpp_stanza_get_child_by_name(stanza, "data"), "sid"));
  
    sprintf(Data_Seq_Buf , "%d", data_seq);
    xmpp_stanza_set_attribute(data, "seq", Data_Seq_Buf);
       
    printf("\n[Response =%s]\n", resp);
    encoded_data = xmpp_base64_encode(ctx, (unsigned char*)resp, strlen(resp));

    xmpp_stanza_set_text_with_size(text, encoded_data, strlen(encoded_data));
    xmpp_stanza_add_child(data, text);
    xmpp_stanza_add_child(iq, data);
    xmpp_send(conn, iq);
    seq++;

    free(resp);
    xmpp_free(ctx, encoded_data);

    xmpp_stanza_release(data);
    xmpp_stanza_release(iq);
    xmpp_stanza_release(text);

    xmpp_stanza_release(stanza);  //copied by IBB IQ receiver handler

}


/* Send XEP-0047 result stanza */
void XMPP_IBB_Ack_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    xmpp_stanza_t *iq;

    iq  = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "result");

    xmpp_stanza_set_id(iq, xmpp_stanza_get_attribute(stanza, "id"));
    xmpp_stanza_set_attribute(iq, "to", xmpp_stanza_get_attribute(stanza, "from"));
    xmpp_stanza_set_attribute(iq, "from", xmpp_stanza_get_attribute(stanza, "to"));
    xmpp_send(conn, iq);
    xmpp_stanza_release(iq);
}

/*  XMPP_IBB_Close_Send() has not been verified. */
void XMPP_IBB_Close_Send(xmpp_conn_t * const conn, xmpp_stanza_t * const stanza, void * const userdata)
{
   
    	
#if 0
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    xmpp_stanza_t *iq;

    iq  = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(iq, "iq");
    xmpp_stanza_set_type(iq, "set");

    xmpp_stanza_set_id(iq, xmpp_stanza_get_attribute(stanza, "id"));
    xmpp_stanza_set_attribute(iq, "to", xmpp_stanza_get_attribute(stanza, "from"));
    xmpp_stanza_set_attribute(iq, "from", xmpp_stanza_get_attribute(stanza, "to"));

    xmpp_send(conn, iq);
    xmpp_stanza_release(iq);

#endif

}

/* Initialize IBB handle. */
xmpp_ibb_session_t* XMPP_IBB_Init()
{
  
    gXMPP_IBB_handle = malloc(sizeof(xmpp_ibb_session_t));

    gXMPP_IBB_handle->conn = NULL;
    gXMPP_IBB_handle->sid = NULL;
    gXMPP_IBB_handle->szblock_size = NULL;
    gXMPP_IBB_handle->ibb_data_queue = NULL;
    gXMPP_IBB_handle->next = NULL;
	
    return gXMPP_IBB_handle;	
}
/* Get IBB handle in order to get Session ID */
xmpp_ibb_session_t* XMPP_Get_IBB_Handle()
{
    return gXMPP_IBB_handle;
}

/* Get IBB Session Handle by Sid in order to get blocksize, data payload */
xmpp_ibb_session_t* XMPP_Get_IBB_Session_Handle(char* szSid)
{
    xmpp_ibb_session_t* ibb_ssn_p  = XMPP_Get_IBB_Handle();
    
    while(ibb_ssn_p!=NULL)
    {	
        if(strcmp(ibb_ssn_p->sid, szSid) == 0 )
	    return ibb_ssn_p;	
	else
 	    ibb_ssn_p= ibb_ssn_p->next; 
    }
   	    
    return NULL;

}

/*Add a session to the Queue */
void  XMPP_IBB_Add_Session_Queue(xmpp_ibb_session_t* ibb_ssn_new)
{
    xmpp_ibb_session_t* ibb_ssn_p  = XMPP_Get_IBB_Handle();

    while(ibb_ssn_p!=NULL)
    {
        if( ibb_ssn_p->next == NULL )
            ibb_ssn_p->next = ibb_ssn_new;
        else
            ibb_ssn_p = ibb_ssn_p->next;
    }
   
}

/* Add a datapayload to a session queue, wait for process  */
void XMPP_IBB_Add_Session_Data_Queue(xmpp_ibb_session_t* ibb_ssn, \
xmpp_ibb_data_t*  ibb_data_new)
{
 
    xmpp_ibb_data_t* ibb_data_p;
    ibb_data_p = ibb_ssn->ibb_data_queue;

    while( ibb_data_p!=NULL )
    {
        if( ibb_data_p->next == NULL )
            ibb_data_p->next = ibb_data_new;
        else
             ibb_data_p = ibb_data_p->next;
    }

}

/*Get the data from session queue*/
void XMPP_IBB_Del_Session_Data_Queue(xmpp_ibb_session_t* ibb_ssn)
{

    xmpp_ibb_data_t* tmp;
    tmp =  ibb_ssn->ibb_data_queue;
    ibb_ssn->ibb_data_queue = tmp->next;

    free(tmp->seq_num); 
    free(tmp->recv_data);
    free(tmp);

}

/* Get recently IBB received data buffer. This is temporality function and departed when multi session function is supported*/
char* XMPP_IBB_Get_Recv()
{   
    return gRecv;
}

/* Clean IBB received data buffer. This is temporality function and departed when multi session function is supported. */
void XMPP_IBB_Reset_Recv()
{
     if(gRecv !=NULL)
	free(gRecv);

     gRecv = NULL;
}


/* Get recently IBB received stanza handle. This is temporality function and departed when multi session function is supported. */
xmpp_stanza_t* XMPP_IBB_Get_gStanza()
{
    return gStanza;	
}

/* Clear received Stanza. This is temporality function and departed when multi session function is supported.*/
void XMPP_IBB_Reset_gStanza()
{

    if(gStanza != NULL)
        xmpp_stanza_release(gStanza);
     gStanza = NULL;

}




