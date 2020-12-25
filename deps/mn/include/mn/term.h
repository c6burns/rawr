#ifndef MN_TERM_H
#define MN_TERM_H

#ifdef _WIN32
#    include <io.h>
#endif

#include "mn/allocator.h"
#include "mn/atomic.h"
#include "mn/error.h"
#include "mn/list_ptr.h"
#include "mn/mutex.h"
#include "mn/queue_spsc.h"
#include "mn/thread.h"

#define MN_TERM_MAX_LINE 1024
#define MN_TERM_BUFFER_STACK_SIZE 1024
#define MN_TERM_INIT                                              \
    {                                                             \
        .priv = NULL, .state = MN_ATOMIC_INIT(MN_TERM_STATE_NEW), \
    }

enum mn_term_key {
    MN_TERM_KEY_NONE,
    MN_TERM_KEY_F1,
    MN_TERM_KEY_F2,
    MN_TERM_KEY_F3,
    MN_TERM_KEY_F4,
    MN_TERM_KEY_F5,
    MN_TERM_KEY_F6,
    MN_TERM_KEY_F7,
    MN_TERM_KEY_F8,
    MN_TERM_KEY_F9,
    MN_TERM_KEY_F10,
    MN_TERM_KEY_F11,
    MN_TERM_KEY_F12,
    MN_TERM_KEY_HOME,
    MN_TERM_KEY_END,
    MN_TERM_KEY_INSERT,
    MN_TERM_KEY_DELETE,
    MN_TERM_KEY_BACKSPACE,
    MN_TERM_KEY_UP,
    MN_TERM_KEY_DOWN,
    MN_TERM_KEY_RIGHT,
    MN_TERM_KEY_LEFT,
    MN_TERM_KEY_TAB,
    MN_TERM_KEY_ESC,
    MN_TERM_KEY_BREAK,
    MN_TERM_KEY_ENTER,
    MN_TERM_KEY_INVALID,
};

enum mn_term_color {
    MN_TERM_COLOR_BLACK,
    MN_TERM_COLOR_RED_DARK,
    MN_TERM_COLOR_GREEN_DARK,
    MN_TERM_COLOR_YELLOW_DARK,
    MN_TERM_COLOR_BLUE_DARK,
    MN_TERM_COLOR_PURPLE_DARK,
    MN_TERM_COLOR_AQUA_DARK,
    MN_TERM_COLOR_GREY_BRIGHT,
    MN_TERM_COLOR_GREY_DARK,
    MN_TERM_COLOR_RED_BRIGHT,
    MN_TERM_COLOR_GREEN_BRIGHT,
    MN_TERM_COLOR_YELLOW_BRIGHT,
    MN_TERM_COLOR_BLUE_BRIGHT,
    MN_TERM_COLOR_PURPLE_BRIGHT,
    MN_TERM_COLOR_AQUA_BRIGHT,
    MN_TERM_COLOR_WHITE,
};

typedef enum mn_term_state_e {
    MN_TERM_STATE_NEW,
    MN_TERM_STATE_STARTING,
    MN_TERM_STATE_STARTED,
    MN_TERM_STATE_STOPPING,
    MN_TERM_STATE_STOPPED,
    MN_TERM_STATE_ERROR,
    MN_TERM_STATE_INVALID,
} mn_term_state_t;

struct mn_term_pos {
    int x;
    int y;
};

struct mn_term_csi {
    char seq_end;
    int seq_len;
    int param_count;
    int param[8];
    int open_count;
    char shift_char;
};

struct mn_term_buf {
    char buf[MN_TERM_MAX_LINE];
    uint32_t len;
    uint64_t batch_id;
};

typedef void (*mn_term_callback_char_func)(char in_char);
typedef void (*mn_term_callback_key_func)(enum mn_term_key key);
typedef void (*mn_term_callback_resize_func)(uint16_t x, uint16_t y);

typedef struct mn_term_s {
    void *priv;
    mn_queue_spsc_t buffer_queue;
    mn_list_ptr_t buffer_stack;
    struct mn_term_buf *buffer_pool;
    mn_term_callback_char_func cb_char;
    mn_term_callback_key_func cb_key;
    mn_term_callback_resize_func cb_resize;
    mn_thread_t thread_io;
    mn_mutex_t mtx;
    mn_atomic_t state;
    mn_atomic_t batch_id;
    uint64_t batch_request_cache;
    uint64_t batch_process_cache;
    struct mn_term_pos size;
    struct mn_term_pos pos_last;
    int debug_print;
} mn_term_t;

int mn_term_setup(mn_term_t *term);
void mn_term_cleanup(mn_term_t *term);

int mn_term_start(mn_term_t *term);
int mn_term_stop(mn_term_t *term);

void mn_term_debug_print(mn_term_t *term, bool enabled);
mn_term_state_t mn_term_state(mn_term_t *term);
void mn_term_callback_char(mn_term_t *term, mn_term_callback_char_func cb_char);
void mn_term_callback_key(mn_term_t *term, mn_term_callback_key_func cb_char);
void mn_term_callback_resize(mn_term_t *term, mn_term_callback_resize_func cb_cmd);

int mn_term_clear(mn_term_t *term);
int mn_term_clear_down(mn_term_t *term);
int mn_term_clear_up(mn_term_t *term);
int mn_term_clear_line(mn_term_t *term);
int mn_term_clear_line_home(mn_term_t *term);
int mn_term_clear_line_end(mn_term_t *term);

int mn_term_pos_get(mn_term_t *term);
int mn_term_pos_set(mn_term_t *term, uint16_t x, uint16_t y);
int mn_term_pos_store(mn_term_t *term);
int mn_term_pos_restore(mn_term_t *term);
int mn_term_pos_up(mn_term_t *term, uint16_t count);
int mn_term_pos_down(mn_term_t *term, uint16_t count);
int mn_term_pos_right(mn_term_t *term, uint16_t count);
int mn_term_pos_left(mn_term_t *term, uint16_t count);

int mn_term_write(mn_term_t *term, const char *fmt, ...);
void mn_term_flush(mn_term_t *term);

void mn_term_flush(mn_term_t *term);
int mn_term_color_set(mn_term_t *term, uint8_t color);
int mn_term_bgcolor_set(mn_term_t *term, uint8_t color);
uint8_t mn_term_color16(mn_term_t *term, enum mn_term_color color);
uint8_t mn_term_color256(mn_term_t *term, uint8_t r, uint8_t g, uint8_t b);
uint8_t mn_term_grey24(mn_term_t *term, uint8_t grey);

#endif
