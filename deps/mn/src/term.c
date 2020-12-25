#include "mn/term.h"

#include <ctype.h>
#include <stdarg.h>

#include "uv.h"

#include "mn/buffer.h"

const char *const mn_term_key_string[] = {
    "",
    "[F1]",
    "[F2]",
    "[F3]",
    "[F4]",
    "[F5]",
    "[F6]",
    "[F7]",
    "[F8]",
    "[F9]",
    "[F10]",
    "[F11]",
    "[F12]",
    "[HOME]",
    "[END]",
    "[INS]",
    "[DEL]",
    "[BKSP]",
    "[UP]",
    "[DOWN]",
    "[RIGHT]",
    "[LEFT]",
    "[TAB]",
    "[ESC]",
};

typedef struct mn_term_priv_s {
    uv_loop_t uv_loop;
    uv_signal_t uv_signal;
    int ttyin_fd, ttyout_fd;
    uv_tty_t uv_tty_in, uv_tty_out;
    uv_timer_t uv_timer_tty;
    struct aws_byte_buf bbuf;
    struct aws_byte_cursor bpos;
} mn_term_priv_t;

void on_term_close_cb(uv_handle_t *handle);
void on_term_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);
void on_term_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void on_term_timer_cb(uv_timer_t *handle);
void on_term_signal_cb(uv_signal_t *handle, int signum);

// private ------------------------------------------------------------------------------------------------------
void mn_term_state_set(mn_term_t *term, mn_term_state_t state)
{
    MN_ASSERT(term);
    mn_atomic_store(&term->state, state);
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_write_direct(mn_term_t *term, const char *fmt, ...)
{
    uv_buf_t buf;
    int len;
    char str[MN_TERM_MAX_LINE];
    mn_term_priv_t *priv = term->priv;

    va_list args;
    va_start(args, fmt);
    len = vsnprintf(str, MN_TERM_MAX_LINE, fmt, args);
    va_end(args);

    buf.base = str;
    buf.len = len;
    MN_GUARD(uv_try_write((uv_stream_t *)&priv->uv_tty_out, &buf, 1) <= 0);

    return MN_SUCCESS;
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_buf_write_u8(mn_term_t *term, char in_char)
{
    MN_ASSERT(term && term->priv);

    int ret = MN_ERROR;
    mn_term_priv_t *priv = term->priv;

    MN_GUARD_CLEANUP(!aws_byte_buf_write_u8(&priv->bbuf, in_char));

    ret = MN_SUCCESS;

cleanup:
    return ret;
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_key_print(mn_term_t *term, enum mn_term_key key)
{
    MN_GUARD(key <= MN_TERM_KEY_NONE);
    MN_GUARD(key >= MN_TERM_KEY_INVALID);
    return mn_term_write_direct(term, mn_term_key_string[key]);
}

// private ------------------------------------------------------------------------------------------------------
void mn_term_key_handle(mn_term_t *term, enum mn_term_key key)
{
    if (term->debug_print) mn_term_key_print(term, key);

    if (term->cb_key) term->cb_key(key);
    ;
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_csi_print(mn_term_t *term, const char *buf, int len)
{
    MN_ASSERT(term && buf);

    char str[128];
    int istr = 0;

    str[istr++] = '[';
    for (int i = 0; i < len; i++) {
        const char c = buf[i];
        if (c == 27) {
            str[istr++] = '^';
            continue;
        }
        str[istr++] = c;
    }
    str[istr++] = ']';
    str[istr++] = '\0';

    return mn_term_write_direct(term, "%s", str);
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_csi_parse(mn_term_t *term, const char *input, int maxlen, struct mn_term_csi *csi)
{
    MN_ASSERT(term && input && csi);

    char str[128];
    int istr = 0;
    int is_shift = 0;
    memset(csi, 0, sizeof(*csi));

    MN_GUARD(maxlen < 3);
    MN_GUARD(input[0] != '\033');

    for (int i = 1; i < maxlen; i++) {
        char c = input[i];

        if (isdigit(c)) {
            str[istr++] = c;
            continue;
        } else if (c == '[') {
            csi->open_count++;
            continue;
        } else if (c == 'O') {
            is_shift = 1;
            continue;
        }

        if (istr) {
            str[istr] = '\0';
            csi->param[csi->param_count++] = atoi(str);
            istr = 0;
        } else {
            csi->param[csi->param_count++] = 0;
        }

        if (is_shift) {
            csi->seq_end = 'O';
            csi->seq_len = i + 1;
            csi->shift_char = c;
            break;
        } else if (isalpha(c) || c == '~') {
            csi->seq_end = c;
            csi->seq_len = i + 1;
            break;
        }
    }

    MN_GUARD(!csi->seq_end);

    return MN_SUCCESS;
}

// private ------------------------------------------------------------------------------------------------------
int mn_term_csi_handle(mn_term_t *term, const struct mn_term_csi *csi)
{
    int unhandled = 0;

    switch (csi->seq_end) {
    case 'A':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_UP);
        } else if (csi->open_count == 2) {
            mn_term_key_handle(term, MN_TERM_KEY_F1);
        }
        break;
    case 'B':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_DOWN);
        } else if (csi->open_count == 2) {
            mn_term_key_handle(term, MN_TERM_KEY_F2);
        }
        break;
    case 'C':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_RIGHT);
        } else if (csi->open_count == 2) {
            mn_term_key_handle(term, MN_TERM_KEY_F3);
        }
        break;
    case 'D':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_LEFT);
        } else if (csi->open_count == 2) {
            mn_term_key_handle(term, MN_TERM_KEY_F4);
        }
        break;
    case 'E':
        if (csi->open_count == 2) {
            mn_term_key_handle(term, MN_TERM_KEY_F5);
        }
        break;
    case 'F':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_END);
        }
        break;
    case 'H':
        if (csi->open_count == 1) {
            mn_term_key_handle(term, MN_TERM_KEY_HOME);
        }
        break;
    case 'O': // single shift
        switch (csi->shift_char) {
        case 'P':
            mn_term_key_handle(term, MN_TERM_KEY_F1);
            break;
        case 'Q':
            mn_term_key_handle(term, MN_TERM_KEY_F2);
            break;
        case 'R':
            mn_term_key_handle(term, MN_TERM_KEY_F3);
            break;
        case 'S':
            mn_term_key_handle(term, MN_TERM_KEY_F4);
            break;
        }
        break;
    case 'R':
        term->pos_last.y = csi->param[0];
        term->pos_last.x = csi->param[1];
        if (term->debug_print) mn_term_write_direct(term, "[%d,%d]", term->pos_last.x, term->pos_last.y);
        break;
    case '~':
        switch (csi->param[0]) {
        case 1:
            mn_term_key_handle(term, MN_TERM_KEY_HOME);
            break;
        case 2:
            mn_term_key_handle(term, MN_TERM_KEY_INSERT);
            break;
        case 3:
            mn_term_key_handle(term, MN_TERM_KEY_DELETE);
            break;
        case 4:
            mn_term_key_handle(term, MN_TERM_KEY_END);
            break;
        case 15:
            mn_term_key_handle(term, MN_TERM_KEY_F5);
            break;
        case 17:
            mn_term_key_handle(term, MN_TERM_KEY_F6);
            break;
        case 18:
            mn_term_key_handle(term, MN_TERM_KEY_F7);
            break;
        case 19:
            mn_term_key_handle(term, MN_TERM_KEY_F8);
            break;
        case 20:
            mn_term_key_handle(term, MN_TERM_KEY_F9);
            break;
        case 21:
            mn_term_key_handle(term, MN_TERM_KEY_F10);
            break;
        case 23:
            mn_term_key_handle(term, MN_TERM_KEY_F11);
            break;
        case 24:
            mn_term_key_handle(term, MN_TERM_KEY_F12);
            break;
        default:
            return MN_ERROR;
        }
        break;
    default:
        return MN_ERROR;
    }

    return MN_SUCCESS;
}

// private ------------------------------------------------------------------------------------------------------
void mn_term_input_handle(mn_term_t *term, const char *buf, int len)
{
    char in_char;
    struct mn_term_csi csi;
    mn_term_priv_t *priv = term->priv;

    for (int i = 0; i < len; i++) {
        in_char = buf[i];

        switch (in_char) {
        case 3: // ctrl-C
        case 4: // ctrl-D
            mn_term_key_handle(term, MN_TERM_KEY_BREAK);
            mn_term_state_set(term, MN_TERM_STATE_STOPPING);
            return;
        case 8:
        case 127: // backspace
            mn_term_key_handle(term, MN_TERM_KEY_BACKSPACE);
            break;
        case 9: // tab
            mn_term_key_handle(term, MN_TERM_KEY_TAB);
            break;
        case 12:
        case 13:
        case 15: // line ending / enter key
            mn_term_key_handle(term, MN_TERM_KEY_ENTER);
            //if (priv->bbuf.len > 0) {
            //	MN_GUARD_CLEANUP(mn_term_buf_write_u8(term, '\0'));
            //	if (term->cb_cmd) term->cb_cmd((const char *)priv->bbuf.buffer);
            //	aws_byte_buf_reset(&priv->bbuf, true);
            //}
            break;
        case 27: // escape seq (or possible just esc keypress)
            if (mn_term_csi_parse(term, &buf[i], len - i, &csi) == MN_SUCCESS) {
                if (mn_term_csi_handle(term, &csi)) {
                    if (term->debug_print) mn_term_csi_print(term, &buf[i], csi.seq_len);
                }
                i += csi.seq_len - 1;
            } else {
                mn_term_key_handle(term, MN_TERM_KEY_ESC);
            }
            break;
        default:
            if (!isprint(in_char)) {
                mn_term_write_direct(term, "[%d]", (int)in_char);
            } else {
                if (term->cb_char) term->cb_char(in_char);
            }
            //printf("%c", in_char);
            MN_GUARD_CLEANUP(mn_term_buf_write_u8(term, in_char));
            break;
        }
    }

    return;

cleanup:
    mn_term_state_set(term, MN_TERM_STATE_ERROR);
}

// private ------------------------------------------------------------------------------------------------------
void mn_term_uv_cleanup(mn_term_t *term)
{
    mn_term_priv_t *priv = term->priv;
    // uv_tty_set_mode(&priv->uv_tty_in, UV_TTY_MODE_NORMAL);
    // uv_tty_reset_mode();

    if (uv_is_active((uv_handle_t *)&priv->uv_signal)) {
        uv_close((uv_handle_t *)&priv->uv_signal, on_term_close_cb);
    }

    if (uv_is_active((uv_handle_t *)&priv->uv_timer_tty)) {
        uv_close((uv_handle_t *)&priv->uv_timer_tty, on_term_close_cb);
    }

    if (uv_is_active((uv_handle_t *)&priv->uv_tty_in)) {
        uv_close((uv_handle_t *)&priv->uv_tty_in, on_term_close_cb);
    }

    if (uv_is_active((uv_handle_t *)&priv->uv_tty_out)) {
        uv_close((uv_handle_t *)&priv->uv_tty_in, on_term_close_cb);
    }
}

// private ------------------------------------------------------------------------------------------------------
void mn_term_queue_process(mn_term_t *term)
{
    const uint64_t batch_id = mn_atomic_load(&term->batch_id);
    //if (batch_id == term->batch_process_cache) return;

    uv_buf_t buf;
    struct mn_term_buf *term_buf;
    mn_term_priv_t *priv = term->priv;

    while (mn_queue_spsc_peek(&term->buffer_queue, (void **)&term_buf) == MN_SUCCESS) {
        if (batch_id == term->batch_process_cache) return;
        mn_queue_spsc_pop_cached(&term->buffer_queue);

        buf.base = term_buf->buf;
        buf.len = MN_BUFLEN_CAST(term_buf->len);
        MN_GUARD_CLEANUP(uv_try_write((uv_stream_t *)&priv->uv_tty_out, &buf, 1) <= 0);

        MN_GUARD_CLEANUP(mn_list_ptr_push_back(&term->buffer_stack, term_buf));
    }

    term->batch_process_cache++;

    return;

cleanup:
    mn_term_state_set(term, MN_TERM_STATE_ERROR);
}

// --------------------------------------------------------------------------------------------------------------
void on_term_close_cb(uv_handle_t *handle)
{
}

// --------------------------------------------------------------------------------------------------------------
void on_term_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = malloc(size);
    buf->len = MN_BUFLEN_CAST(size);
}

// --------------------------------------------------------------------------------------------------------------
void on_term_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    mn_term_t *term = stream->data;
    mn_term_priv_t *priv = term->priv;
    if (nread <= 0) {
        mn_term_state_set(term, MN_TERM_STATE_STOPPING);
        return;
    }

    mn_term_input_handle(term, buf->base, (int)nread);
}

// --------------------------------------------------------------------------------------------------------------
void on_term_timer_cb(uv_timer_t *handle)
{
    mn_term_t *term = handle->data;
    mn_term_priv_t *priv = term->priv;

    if (handle == &priv->uv_timer_tty) {
        if (mn_term_state(term) == MN_TERM_STATE_STOPPING || mn_term_state(term) == MN_TERM_STATE_ERROR) {
            mn_term_uv_cleanup(term);
        } else {
            mn_term_queue_process(term);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------
void on_term_signal_cb(uv_signal_t *handle, int signum)
{
    if (signum == SIGWINCH) {
        mn_term_t *term = handle->data;
        mn_term_priv_t *priv = term->priv;
        uv_tty_get_winsize(&priv->uv_tty_out, &term->size.x, &term->size.y);
        if (term->cb_resize) term->cb_resize(term->size.x, term->size.y);
    }
}

// --------------------------------------------------------------------------------------------------------------
void run_term_thread_io(void *data)
{
    MN_ASSERT(data);
    mn_term_t *term = (mn_term_t *)data;

    //int ret;
    mn_term_priv_t priv_scoped;
    mn_term_priv_t *priv = &priv_scoped;
    mn_term_state_t state = mn_term_state(term);

    MN_GUARD_CLEANUP(state != MN_TERM_STATE_STARTING);

    memset(priv, 0, sizeof(*priv));
    term->priv = priv;

    MN_GUARD_CLEANUP(aws_byte_buf_init(&priv->bbuf, aws_default_allocator(), MN_TERM_MAX_LINE));
    aws_byte_buf_reset(&priv->bbuf, true);
    priv->bpos = aws_byte_cursor_from_buf(&priv->bbuf);

    MN_GUARD_CLEANUP(uv_loop_init(&priv->uv_loop));

    MN_GUARD_CLEANUP(uv_signal_init(&priv->uv_loop, &priv->uv_signal));
    MN_GUARD_CLEANUP(uv_signal_start(&priv->uv_signal, on_term_signal_cb, SIGWINCH));
    priv->uv_signal.data = term;

    uv_timer_init(&priv->uv_loop, &priv->uv_timer_tty);
    uv_timer_start(&priv->uv_timer_tty, on_term_timer_cb, 10, 10);
    priv->uv_timer_tty.data = term;

    priv->ttyin_fd = 0;
    priv->ttyout_fd = 1;

    MN_GUARD_CLEANUP(uv_guess_handle(priv->ttyin_fd) != UV_TTY);
    MN_GUARD_CLEANUP(uv_guess_handle(priv->ttyout_fd) != UV_TTY);

    MN_GUARD_CLEANUP(uv_tty_init(&priv->uv_loop, &priv->uv_tty_in, priv->ttyin_fd, 1));
    MN_GUARD_CLEANUP(!uv_is_readable((uv_stream_t *)&priv->uv_tty_in));
    priv->uv_tty_in.data = term;

    MN_GUARD_CLEANUP(uv_tty_init(&priv->uv_loop, &priv->uv_tty_out, priv->ttyout_fd, 0));
    MN_GUARD_CLEANUP(!uv_is_writable((uv_stream_t *)&priv->uv_tty_out));
    priv->uv_tty_out.data = term;

    MN_GUARD_CLEANUP(uv_tty_get_winsize(&priv->uv_tty_out, &term->size.x, &term->size.y));
    if (term->cb_resize) term->cb_resize(term->size.x, term->size.y);

    MN_GUARD_CLEANUP(uv_tty_set_mode(&priv->uv_tty_in, UV_TTY_MODE_RAW));
    MN_GUARD_CLEANUP(uv_read_start((uv_stream_t *)&priv->uv_tty_in, on_term_alloc_cb, on_term_read_cb));

    state = mn_term_state(term);
    if (state == MN_TERM_STATE_STARTING) {
        mn_term_state_set(term, MN_TERM_STATE_STARTED);
        MN_GUARD_CLEANUP(uv_run(&priv->uv_loop, UV_RUN_DEFAULT));
    }

    mn_term_state_set(term, MN_TERM_STATE_STOPPED);
    return;

cleanup:
    mn_term_state_set(term, MN_TERM_STATE_ERROR);
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_debug_print(mn_term_t *term, bool enabled)
{
    MN_ASSERT(term);
    term->debug_print = enabled ? 1 : 0;
}

// --------------------------------------------------------------------------------------------------------------
mn_term_state_t mn_term_state(mn_term_t *term)
{
    MN_ASSERT(term);
    return mn_atomic_load(&term->state);
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_callback_char(mn_term_t *term, mn_term_callback_char_func cb_char)
{
    MN_ASSERT(term);
    term->cb_char = cb_char;
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_callback_key(mn_term_t *term, mn_term_callback_key_func cb_key)
{
    MN_ASSERT(term);
    term->cb_key = cb_key;
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_callback_resize(mn_term_t *term, mn_term_callback_resize_func cb_resize)
{
    MN_ASSERT(term);
    term->cb_resize = cb_resize;
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_setup(mn_term_t *term)
{
    MN_ASSERT(term);

    MN_GUARD_CLEANUP(term->priv != NULL);

    mn_mutex_setup(&term->mtx);

    memset(&term->buffer_stack, 0, sizeof(term->buffer_stack));
    MN_GUARD_NULL_CLEANUP(term->buffer_pool = MN_MEM_ACQUIRE(sizeof(*term->buffer_pool) * MN_TERM_BUFFER_STACK_SIZE));

    MN_GUARD_CLEANUP(mn_list_ptr_setup(&term->buffer_stack, MN_TERM_BUFFER_STACK_SIZE));

    struct mn_term_buf *tb;
    for (int i = 1; i <= MN_TERM_BUFFER_STACK_SIZE; i++) {
        tb = term->buffer_pool + MN_TERM_BUFFER_STACK_SIZE - i;
        MN_GUARD_CLEANUP(mn_list_ptr_push_back(&term->buffer_stack, tb));
    }

    MN_GUARD_CLEANUP(mn_queue_spsc_setup(&term->buffer_queue, MN_TERM_MAX_LINE));

    mn_atomic_store(&term->batch_id, 0);
    term->batch_request_cache = 0;
    term->batch_process_cache = 0;

    term->cb_char = NULL;
    term->cb_key = NULL;
    term->cb_resize = NULL;

    return MN_SUCCESS;

cleanup:
    mn_term_state_set(term, MN_TERM_STATE_ERROR);
    return MN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_cleanup(mn_term_t *term)
{
    MN_ASSERT(term);

    mn_queue_spsc_cleanup(&term->buffer_queue);
    mn_list_ptr_cleanup(&term->buffer_stack);
    MN_MEM_RELEASE(term->buffer_pool);
    mn_mutex_cleanup(&term->mtx);

    uv_tty_reset_mode();
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_start(mn_term_t *term)
{
    MN_ASSERT(term);

    mn_term_state_t state = mn_term_state(term);
    MN_GUARD(state != MN_TERM_STATE_STOPPED && state != MN_TERM_STATE_NEW);

    mn_term_state_set(term, MN_TERM_STATE_STARTING);
    MN_GUARD_CLEANUP(mn_thread_launch(&term->thread_io, run_term_thread_io, term));

    return MN_SUCCESS;

cleanup:
    mn_term_state_set(term, MN_TERM_STATE_ERROR);
    return MN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_stop(mn_term_t *term)
{
    MN_ASSERT(term);

    mn_term_state_t state = mn_term_state(term);
    MN_GUARD(state == MN_TERM_STATE_STOPPING || state == MN_TERM_STATE_STOPPED);

    mn_term_state_set(term, MN_TERM_STATE_STOPPING);

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear(mn_term_t *term)
{
    return mn_term_write(term, "\033[2J");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear_down(mn_term_t *term)
{
    return mn_term_write(term, "\033[J");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear_up(mn_term_t *term)
{
    return mn_term_write(term, "\033[1J");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear_line(mn_term_t *term)
{
    return mn_term_write(term, "\033[2K");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear_line_home(mn_term_t *term)
{
    return mn_term_write(term, "\033[1K");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_clear_line_end(mn_term_t *term)
{
    return mn_term_write(term, "\033[K");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_get(mn_term_t *term)
{
    return mn_term_write(term, "\033[6n");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_set(mn_term_t *term, uint16_t x, uint16_t y)
{
    x = x ? x : 1;
    y = y ? y : 1;
    return mn_term_write(term, "\033[%u;%uH", y, x);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_store(mn_term_t *term)
{
    return mn_term_write(term, "\033[s");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_restore(mn_term_t *term)
{
    return mn_term_write(term, "\033[u");
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_up(mn_term_t *term, uint16_t count)
{
    return mn_term_write(term, "\033[%dA", count ? count : 1);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_down(mn_term_t *term, uint16_t count)
{
    return mn_term_write(term, "\033[%dB", count ? count : 1);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_right(mn_term_t *term, uint16_t count)
{
    return mn_term_write(term, "\033[%dC", count ? count : 1);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_pos_left(mn_term_t *term, uint16_t count)
{
    return mn_term_write(term, "\033[%dD", count ? count : 1);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_write(mn_term_t *term, const char *fmt, ...)
{
    MN_ASSERT(term);

    struct mn_term_buf *term_buf;
    MN_GUARD(mn_list_ptr_pop_back(&term->buffer_stack, (void **)&term_buf));

    va_list args;
    va_start(args, fmt);
    term_buf->len = vsnprintf(term_buf->buf, MN_TERM_MAX_LINE, fmt, args);
    va_end(args);

    if (!term_buf->len) return MN_SUCCESS;

    term_buf->batch_id = term->batch_request_cache;

    MN_GUARD(term_buf->len < 0);
    return mn_queue_spsc_push(&term->buffer_queue, term_buf);
}

// --------------------------------------------------------------------------------------------------------------
void mn_term_flush(mn_term_t *term)
{
    MN_ASSERT(term);
    term->batch_request_cache++;
    mn_atomic_store(&term->batch_id, term->batch_request_cache);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_color_set(mn_term_t *term, uint8_t color)
{
    return mn_term_write(term, "\033[38;5;%dm", color);
}

// --------------------------------------------------------------------------------------------------------------
int mn_term_bgcolor_set(mn_term_t *term, uint8_t color)
{
    return mn_term_write(term, "\033[48;5;%dm", color);
}

// --------------------------------------------------------------------------------------------------------------
uint8_t mn_term_color16(mn_term_t *term, enum mn_term_color color)
{
    return (color > 0xff) ? 0xff : color;
}

// --------------------------------------------------------------------------------------------------------------
uint8_t mn_term_color256(mn_term_t *term, uint8_t r, uint8_t g, uint8_t b)
{
    r = (r > 5) ? 5 : r;
    g = (g > 5) ? 5 : g;
    b = (b > 5) ? 5 : b;
    return (r * 36) + (g * 6) + b + 16;
}

// --------------------------------------------------------------------------------------------------------------
uint8_t mn_term_grey24(mn_term_t *term, uint8_t grey)
{
    return (grey > 23) ? 23 : grey;
}
