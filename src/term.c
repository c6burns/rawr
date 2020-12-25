#include "rawr/term.h"
#include "rawr/buf.h"

#include "mn/error.h"
#include "mn/log.h"
#include "mn/term.h"
#include "mn/thread.h"

typedef struct rawr_term_s {
	uint16_t w, h;
	uint16_t prompt_len;
	rawr_buf_t buf;
} rawr_term_t;

mn_term_t term = MN_TERM_INIT;
rawr_term_t rawr_term = { 0 };

uint32_t rc_prompt();

void on_term_char_cb(char c)
{
	rawr_buf_insert(&rawr_term.buf, c);
	mn_term_write(&term, "%c", c);
	mn_term_pos_store(&term);
	mn_term_write(&term, "%s", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
	mn_term_pos_restore(&term);
}

void on_term_key_cb(enum mn_term_key key)
{
	uint16_t pos;

	switch (key) {
	case MN_TERM_KEY_F1:
		break;
	case MN_TERM_KEY_F2:
		break;
	case MN_TERM_KEY_F3:
		break;
	case MN_TERM_KEY_F4:
		break;
	case MN_TERM_KEY_F5:
		break;
	case MN_TERM_KEY_F6:
		break;
	case MN_TERM_KEY_F7:
		break;
	case MN_TERM_KEY_F8:
		break;
	case MN_TERM_KEY_F9:
		break;
	case MN_TERM_KEY_F10:
		break;
	case MN_TERM_KEY_F11:
		break;
	case MN_TERM_KEY_F12:
		break;
	case MN_TERM_KEY_HOME:
		break;
	case MN_TERM_KEY_END:
		break;
	case MN_TERM_KEY_INSERT:
		break;
	case MN_TERM_KEY_DELETE:
		rawr_buf_delete(&rawr_term.buf);
		mn_term_pos_store(&term);
		mn_term_write(&term, "%s ", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
		mn_term_pos_restore(&term);
		break;
	case MN_TERM_KEY_BACKSPACE:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (pos) {
			rawr_buf_cursor_set(&rawr_term.buf, pos - 1);
			rawr_buf_delete(&rawr_term.buf);
			mn_term_pos_left(&term, 1);
			mn_term_pos_store(&term);
			mn_term_write(&term, "%s ", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
			mn_term_pos_restore(&term);
		}
		break;
	case MN_TERM_KEY_UP:
		break;
	case MN_TERM_KEY_DOWN:
		break;
	case MN_TERM_KEY_RIGHT:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (rawr_buf_cursor_set(&rawr_term.buf, pos + 1) == MN_SUCCESS) {
			mn_term_pos_right(&term, 1);
		}
		break;
	case MN_TERM_KEY_LEFT:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (pos) {
			rawr_buf_cursor_set(&rawr_term.buf, pos - 1);
			mn_term_pos_left(&term, 1);
		}
		break;
	case MN_TERM_KEY_TAB:
		break;
	case MN_TERM_KEY_ESC:
		break;
	case MN_TERM_KEY_BREAK:
		break;
	case MN_TERM_KEY_ENTER:
		mn_term_write(&term, "\n\n");
		rawr_buf_clear(&rawr_term.buf);
		rawr_term.prompt_len = rc_prompt();
		break;
	case MN_TERM_KEY_NONE:
	case MN_TERM_KEY_INVALID:
	default:
		break;
	}

	mn_term_flush(&term);
}

uint32_t rc_prompt()
{
	uint32_t len = 0;

	mn_term_pos_set(&term, 1, rawr_term.h);

	//mn_term_clear_line(&term);

	mn_term_color_set(&term, mn_term_color16(&term, MN_TERM_COLOR_GREEN_BRIGHT));
	mn_term_write(&term, "rc");
	len += 2;

	mn_term_color_set(&term, mn_term_color16(&term, MN_TERM_COLOR_GREY_BRIGHT));
	mn_term_write(&term, "> ");
	len += 2;

	mn_term_pos_get(&term);
	mn_term_write(&term, "%s", rawr_term.buf.bb.buffer);

	mn_term_flush(&term);

	return len;
}

void on_term_resize_cb(uint16_t x, uint16_t y)
{
	rawr_term.w = x;
	rawr_term.h = y;
	rawr_term.prompt_len = rc_prompt();
	//mn_term_write(&term, "\nCommand: %s\n\n", cmd);
	//rc_prompt();
}

int main(int argc, char** argv)
{
	mn_term_state_t term_state;

	rawr_buf_setup(&rawr_term.buf, MN_TERM_MAX_LINE);

	mn_term_setup(&term);
	mn_term_callback_char(&term, on_term_char_cb);
	mn_term_callback_key(&term, on_term_key_cb);
	mn_term_callback_resize(&term, on_term_resize_cb);
	//mn_term_debug_print(&term, true);
	MN_GUARD(mn_term_start(&term));
	//mn_term_write(&term, "I love potatoes :D :D :D\n\n");
	//rc_prompt();
	//mn_term_flush(&term);
	while (1) {
		term_state = mn_term_state(&term);
		if (term_state == MN_TERM_STATE_STOPPING || term_state == MN_TERM_STATE_STOPPED || term_state == MN_TERM_STATE_ERROR) break;

		mn_thread_sleep_ms(50);
	}

	mn_term_cleanup(&term);

	return 0;
}