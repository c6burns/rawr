#ifndef RAWR_TERM_H
#define RAWR_TERM_H

typedef void (*rawr_term_callback_option)(uint8_t);
typedef void (*rawr_term_callback_cancel)();
typedef void (*rawr_term_callback_submit)(char *submitted);

int rawr_term_setup(void);
void rawr_term_cleanup(void);
int rawr_term_running(void);
void rawr_term_announce(const char *msg);

void rawr_term_set_input_name(const char *msg);
void rawr_term_set_input_level(double pct);
void rawr_term_set_output_name(const char *msg);
void rawr_term_set_output_level(double pct);

#endif
