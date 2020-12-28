#ifndef RAWR_PLAYGROUND_SRTP_H
#define RAWR_PLAYGROUND_SRTP_H

#include <stdint.h>

typedef enum { sender,
               receiver,
               unknown } program_type;

int get_frame_size_enum(int frame_size, int sampling_rate);
int rtp_session_get_done();
int rtp_session_get_exiting();
void rtp_session_set_exiting(int val);
void rtp_session_run(program_type prog_type, const char *address, uint16_t port);
void rtp_session_run_recv(void *arg);
void rtp_session_run_send(void *arg);

#endif
