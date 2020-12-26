#include "rawr/term.h"
#include "rawr/buf.h"

#include "mn/error.h"
#include "mn/log.h"
#include "mn/term.h"
#include "mn/thread.h"


typedef enum rawr_term_keystate_e {
	RAWR_TERM_KEYSTATE_MENU,
	RAWR_TERM_KEYSTATE_INPUT,
} rawr_term_keystate_t;

typedef struct rawr_term_s {
    uint16_t w;
    uint16_t h;
	uint16_t prompt_len;
	rawr_term_keystate_t keystate;
	rawr_buf_t buf;
} rawr_term_t;

mn_term_t mn_term = MN_TERM_INIT;
rawr_term_t rawr_term = { 0 };

uint32_t rawr_term_prompt();
void on_rawr_term_resize_cb(uint16_t x, uint16_t y);
void on_rawr_term_key_cb(enum mn_term_key key);
void on_rawr_term_char_cb(char c);

// --------------------------------------------------------------------------------------------------------------
int rawr_term_setup(void)
{
    mn_term_state_t term_state;

    MN_GUARD(rawr_buf_setup(&rawr_term.buf, MN_TERM_MAX_LINE));

    MN_GUARD(mn_term_setup(&mn_term));

    mn_term_callback_char(&mn_term, on_rawr_term_char_cb);
    mn_term_callback_key(&mn_term, on_rawr_term_key_cb);
    mn_term_callback_resize(&mn_term, on_rawr_term_resize_cb);

    MN_GUARD(mn_term_start(&mn_term));

    return MN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_cleanup(void)
{
    mn_term_state_t term_state;

    rawr_buf_cleanup(&rawr_term.buf);

    mn_term_stop(&mn_term);

    mn_term_cleanup(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
int rawr_term_running(void)
{
    mn_term_state_t state = mn_term_state(&mn_term);
    return (state == MN_TERM_STATE_STARTING || state == MN_TERM_STATE_STARTED);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_announce(const char *msg)
{
    mn_term_pos_store(&mn_term);
    mn_term_pos_set(&mn_term, 1, rawr_term.h - 1);
    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
    mn_term_write(&mn_term, "%s", msg);
    mn_term_clear_line_end(&mn_term);
    mn_term_pos_restore(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_set_input_name(const char *msg)
{
    mn_term_pos_store(&mn_term);
    mn_term_pos_set(&mn_term, 6, rawr_term.h - 4);
    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
    mn_term_write(&mn_term, "%s", msg);
    mn_term_clear_line_end(&mn_term);
    mn_term_pos_restore(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_set_input_level(double pct)
{
    mn_term_pos_store(&mn_term);
    mn_term_pos_set(&mn_term, 39, rawr_term.h - 4);
    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));

    char *levelstr = "[          ]";
    for (int i = 1; i <= 10; i++) {
        double thresh = (double)i / 10.0;
        if (pct > thresh) {
            levelstr[i] = '|';
        } else {
            levelstr[i] = ' ';
        }
    }
    mn_term_write(&mn_term, "%s", levelstr);
    mn_term_clear_line_end(&mn_term);
    mn_term_pos_restore(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_set_output_name(const char *msg)
{
    mn_term_pos_store(&mn_term);
    mn_term_pos_set(&mn_term, 6, rawr_term.h - 3);
    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
    mn_term_write(&mn_term, "%s", msg);
    mn_term_clear_line_end(&mn_term);
    mn_term_pos_restore(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
void rawr_term_set_output_level(double pct)
{
    mn_term_pos_store(&mn_term);
    mn_term_pos_set(&mn_term, 39, rawr_term.h - 3);
    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));

    char *level = "[          ]";
    for (int i = 1; i <= 10; i++) {
        double thresh = 10.0 / (double)i;
        if (pct > thresh) level[i] = '|';
    }
    mn_term_write(&mn_term, "%s", level);
    mn_term_clear_line_end(&mn_term);
    mn_term_pos_restore(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
uint32_t rawr_term_prompt()
{
    uint32_t len = 0;

    for (int i = 30; i >= 0; i--) {
        if ((rawr_term.h - i) <= 0) continue;
        if (rawr_term.w <= 50 && i > 0) continue;

        mn_term_pos_set(&mn_term, 1, rawr_term.h - i);
        switch (i) {
        case 28:
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
            mn_term_write(&mn_term, "[ ");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_DARK));
            mn_term_write(&mn_term, "---------------");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_PURPLE_BRIGHT));
            mn_term_write(&mn_term, " RAWR! Softphone ");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_DARK));
            mn_term_write(&mn_term, "---------------");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
            mn_term_write(&mn_term, " ]");
            mn_term_clear_line_end(&mn_term);
            break;
        case 29:
        case 2:
            mn_term_clear_line_end(&mn_term);
            break;
        case 4:
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_PURPLE_BRIGHT));
            mn_term_write(&mn_term, " in");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
            mn_term_write(&mn_term, "> ");
            mn_term_clear_line_end(&mn_term);
            break;
        case 3:
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_PURPLE_BRIGHT));
            mn_term_write(&mn_term, "out");
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
            mn_term_write(&mn_term, "> ");
            mn_term_clear_line_end(&mn_term);
            break;
        case 1:
            break;
        case 0:
            mn_term_pos_set(&mn_term, 1, rawr_term.h);
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREEN_BRIGHT));
            if (rawr_term.w < 50 || rawr_term.h < 30) {
                mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_RED_BRIGHT));
            }
            mn_term_write(&mn_term, "rawr");
            len += 4;

            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
            mn_term_write(&mn_term, "> ");
            len += 2;
            break;
        default:
            mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_DARK));
            mn_term_write(&mn_term, "> ");
            mn_term_clear_line_end(&mn_term);
            break;
        }
    }

    mn_term_color_set(&mn_term, mn_term_color16(&mn_term, MN_TERM_COLOR_GREY_BRIGHT));
    mn_term_pos_get(&mn_term);
    mn_term_write(&mn_term, "%s", rawr_term.buf.bb.buffer);

    mn_term_flush(&mn_term);

    return len;
}

// --------------------------------------------------------------------------------------------------------------
void on_rawr_term_char_cb(char c)
{
	if (rawr_term.keystate == RAWR_TERM_KEYSTATE_INPUT) {
		rawr_buf_insert(&rawr_term.buf, c);
		mn_term_write(&mn_term, "%c", c);
		mn_term_pos_store(&mn_term);
		mn_term_write(&mn_term, "%s", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
		mn_term_pos_restore(&mn_term);
    } else if (rawr_term.keystate == RAWR_TERM_KEYSTATE_MENU) {
        switch (c) {
        case '1':
            rawr_term_announce("Pressed: 1");
            break;
        case '2':
            rawr_term_announce("Pressed: 2");
            break;
        case '3':
            rawr_term_announce("Pressed: 3");
            break;
        case '4':
            rawr_term_announce("Pressed: 4");
            break;
        }
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_rawr_term_key_cb(enum mn_term_key key)
{
	uint16_t pos;

	switch (key) {
	case MN_TERM_KEY_F1:
		rawr_term_announce("Pressed: F1");
		break;
	case MN_TERM_KEY_F2:
		rawr_term_announce("Pressed: F2");
		break;
	case MN_TERM_KEY_F3:
		rawr_term_announce("Pressed: F3");
		break;
	case MN_TERM_KEY_F4:
		rawr_term_announce("Pressed: F4");
		break;
	case MN_TERM_KEY_F5:
		rawr_term_announce("Pressed: F5");
		break;
	case MN_TERM_KEY_F6:
		rawr_term_announce("Pressed: F6");
		break;
	case MN_TERM_KEY_F7:
		rawr_term_announce("Pressed: F7");
		break;
	case MN_TERM_KEY_F8:
		rawr_term_announce("Pressed: F8");
		break;
	case MN_TERM_KEY_F9:
		rawr_term_announce("Pressed: F9");
		break;
	case MN_TERM_KEY_F10:
		rawr_term_announce("Pressed: F10");
		break;
	case MN_TERM_KEY_F11:
		rawr_term_announce("Pressed: F11");
		break;
	case MN_TERM_KEY_F12:
		rawr_term_announce("Pressed: F12");
		break;
	case MN_TERM_KEY_HOME:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (pos) {
			rawr_buf_cursor_set(&rawr_term.buf, 0);
			mn_term_pos_set(&mn_term, rawr_term.prompt_len + 1, rawr_term.h);
		}
		break;
	case MN_TERM_KEY_END:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		while (rawr_buf_cursor_set(&rawr_term.buf, ++pos) == MN_SUCCESS) {
			mn_term_pos_right(&mn_term, 1);
		}
		break;
	case MN_TERM_KEY_INSERT:
		rawr_term_announce("Pressed: INS");
		break;
	case MN_TERM_KEY_DELETE:
		rawr_buf_delete(&rawr_term.buf);
		mn_term_pos_store(&mn_term);
		mn_term_write(&mn_term, "%s ", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
		mn_term_pos_restore(&mn_term);
		break;
	case MN_TERM_KEY_BACKSPACE:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (pos) {
			rawr_buf_cursor_set(&rawr_term.buf, pos - 1);
			rawr_buf_delete(&rawr_term.buf);
			mn_term_pos_left(&mn_term, 1);
			mn_term_pos_store(&mn_term);
			mn_term_write(&mn_term, "%s ", rawr_buf_slice(&rawr_term.buf, rawr_buf_cursor_get(&rawr_term.buf)));
			mn_term_pos_restore(&mn_term);
		}
		break;
	case MN_TERM_KEY_UP:
		rawr_term_announce("Pressed: UP");
		break;
	case MN_TERM_KEY_DOWN:
		rawr_term_announce("Pressed: DOWN");
		break;
	case MN_TERM_KEY_RIGHT:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (rawr_buf_cursor_set(&rawr_term.buf, pos + 1) == MN_SUCCESS) {
			mn_term_pos_right(&mn_term, 1);
		}
		break;
	case MN_TERM_KEY_LEFT:
		pos = rawr_buf_cursor_get(&rawr_term.buf);
		if (pos) {
			rawr_buf_cursor_set(&rawr_term.buf, pos - 1);
			mn_term_pos_left(&mn_term, 1);
		}
		break;
	case MN_TERM_KEY_TAB:
		rawr_term_announce("Pressed: TAB");
		break;
	case MN_TERM_KEY_ESC:
		rawr_term_announce("Pressed: ESC");
		break;
	case MN_TERM_KEY_BREAK:
		break;
	case MN_TERM_KEY_ENTER:
		// TODO: send command somewhere :D
		rawr_buf_clear(&rawr_term.buf);
		mn_term_pos_set(&mn_term, 1 + rawr_term.prompt_len, rawr_term.h);
		mn_term_clear_line_end(&mn_term);
		rawr_term.prompt_len = rawr_term_prompt();
		break;
	case MN_TERM_KEY_NONE:
	case MN_TERM_KEY_INVALID:
	default:
		break;
	}

	mn_term_flush(&mn_term);
}

// --------------------------------------------------------------------------------------------------------------
void on_rawr_term_resize_cb(uint16_t x, uint16_t y)
{
	rawr_term.w = x;
	rawr_term.h = y;
	rawr_term.prompt_len = rawr_term_prompt();
}
