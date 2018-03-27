/*
 * Copyright © 2001-2004 Red Hat, Inc.
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2008-2018 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

#include <glib.h>

#include <vte/vte.h>
#include "vteinternal.hh"
#include "vtegtk.hh"
#include "vteutils.h"  /* for strchrnul on non-GNU systems */
#include "caps.hh"
#include "debug.h"

#define BEL_C0 "\007"
#define ST_C0 _VTE_CAP_ST

#include <algorithm>

void
vte::parser::Sequence::print() const
{
#ifdef VTE_DEBUG
        auto c = m_seq != nullptr ? terminator() : 0;
        char c_buf[7];
        g_snprintf(c_buf, sizeof(c_buf), "%lc", c);
        g_printerr("%s:%s [%s]", type_string(), command_string(),
                   g_unichar_isprint(c) ? c_buf : _vte_debug_sequence_to_string(c_buf, -1));
        if (m_seq != nullptr && m_seq->n_args > 0) {
                g_printerr("[ ");
                for (unsigned int i = 0; i < m_seq->n_args; i++) {
                        if (i > 0)
                                g_print(", ");
                        g_printerr("%d", m_seq->args[i]);
                }
                g_printerr(" ]");
        }
        g_printerr("\n");
#endif
}

char const*
vte::parser::Sequence::type_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (type()) {
        case VTE_SEQ_NONE:    return "NONE";
        case VTE_SEQ_IGNORE:  return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE:  return "ESCAPE";
        case VTE_SEQ_CSI:     return "CSI";
        case VTE_SEQ_DCS:     return "DCS";
        case VTE_SEQ_OSC:     return "OSC";
        default:
                g_assert(false);
                return nullptr;
        }
}

char const*
vte::parser::Sequence::command_string() const
{
        if (G_UNLIKELY(m_seq == nullptr))
                return "(nil)";

        switch (command()) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#include "parser-cmd.hh"
#undef _VTE_CMD
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", command());
                return buf;
        }
}

/* A couple are duplicated from vte.c, to keep them static... */

/* Check how long a string of unichars is.  Slow version. */
static gsize
vte_unichar_strlen(gunichar const* c)
{
	gsize i;
	for (i = 0; c[i] != 0; i++) ;
	return i;
}

/* Convert a wide character string to a multibyte string */
/* Simplified from glib's g_ucs4_to_utf8() to simply allocate the maximum
 * length instead of walking the input twice.
 */
char*
vte::parser::Sequence::ucs4_to_utf8(gunichar const* str) const
{
        auto len = vte_unichar_strlen(str);
        auto outlen = (len * VTE_UTF8_BPC) + 1;

        auto result = (char*)g_try_malloc(outlen);
        if (result == nullptr)
                return nullptr;

        auto end = str + len;
        auto p = result;
        for (auto i = str; i < end; i++)
                p += g_unichar_to_utf8(*i, p);
        *p = '\0';

        return result;
}

/* Emit a "bell" signal. */
void
VteTerminalPrivate::emit_bell()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `bell'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_BELL], 0);
}


/* Emit a "deiconify-window" signal. */
void
VteTerminalPrivate::emit_deiconify_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `deiconify-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_DEICONIFY_WINDOW], 0);
}

/* Emit a "iconify-window" signal. */
void
VteTerminalPrivate::emit_iconify_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `iconify-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_ICONIFY_WINDOW], 0);
}

/* Emit a "raise-window" signal. */
void
VteTerminalPrivate::emit_raise_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `raise-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_RAISE_WINDOW], 0);
}

/* Emit a "lower-window" signal. */
void
VteTerminalPrivate::emit_lower_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `lower-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_LOWER_WINDOW], 0);
}

/* Emit a "maximize-window" signal. */
void
VteTerminalPrivate::emit_maximize_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `maximize-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_MAXIMIZE_WINDOW], 0);
}

/* Emit a "refresh-window" signal. */
void
VteTerminalPrivate::emit_refresh_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `refresh-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_REFRESH_WINDOW], 0);
}

/* Emit a "restore-window" signal. */
void
VteTerminalPrivate::emit_restore_window()
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `restore-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_RESTORE_WINDOW], 0);
}

/* Emit a "move-window" signal.  (Pixels.) */
void
VteTerminalPrivate::emit_move_window(guint x,
                                     guint y)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `move-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_MOVE_WINDOW], 0, x, y);
}

/* Emit a "resize-window" signal.  (Grid size.) */
void
VteTerminalPrivate::emit_resize_window(guint columns,
                                       guint rows)
{
        _vte_debug_print(VTE_DEBUG_SIGNALS, "Emitting `resize-window'.\n");
        g_signal_emit(m_terminal, signals[SIGNAL_RESIZE_WINDOW], 0, columns, rows);
}


/* Some common functions */

/* In Xterm, upon printing a character in the last column the cursor doesn't
 * advance.  It's special cased that printing the following letter will first
 * wrap to the next row.
 *
 * As a rule of thumb, escape sequences that move the cursor (e.g. cursor up)
 * or immediately update the visible contents (e.g. clear in line) disable
 * this special mode, whereas escape sequences with no immediate visible
 * effect (e.g. color change) leave this special mode on.  There are
 * exceptions of course (e.g. scroll up).
 *
 * In VTE, a different technical approach is used.  The cursor is advanced to
 * the invisible column on the right, but it's set back to the visible
 * rightmost column whenever necessary (that is, before handling any of the
 * sequences that disable the special cased mode in xterm).  (Bug 731155.)
 */
void
VteTerminalPrivate::ensure_cursor_is_onscreen()
{
        if (G_UNLIKELY (m_screen->cursor.col >= m_column_count))
                m_screen->cursor.col = m_column_count - 1;
}

void
VteTerminalPrivate::home_cursor()
{
        set_cursor_coords(0, 0);
}

void
VteTerminalPrivate::clear_screen()
{
        auto row = m_screen->cursor.row - m_screen->insert_delta;
        auto initial = _vte_ring_next(m_screen->row_data);
	/* Add a new screen's worth of rows. */
        for (auto i = 0; i < m_row_count; i++)
                ring_append(true);
	/* Move the cursor and insertion delta to the first line in the
	 * newly-cleared area and scroll if need be. */
        m_screen->insert_delta = initial;
        m_screen->cursor.row = row + m_screen->insert_delta;
        adjust_adjustments();
	/* Redraw everything. */
        invalidate_all();
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear the current line. */
void
VteTerminalPrivate::clear_current_line()
{
	VteRowData *rowdata;

	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		/* Get the data for the row which the cursor points to. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, m_screen->cursor.row);
		g_assert(rowdata != NULL);
		/* Remove it. */
		_vte_row_data_shrink (rowdata, 0);
		/* Add enough cells to the end of the line to fill out the row. */
                _vte_row_data_fill (rowdata, &m_fill_defaults, m_column_count);
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
		invalidate_cells(0, m_column_count,
                                 m_screen->cursor.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear above the current line. */
void
VteTerminalPrivate::clear_above_current()
{
	/* If the cursor is actually on the screen, clear data in the row
	 * which corresponds to the cursor. */
        for (auto i = m_screen->insert_delta; i < m_screen->cursor.row; i++) {
                if (_vte_ring_next(m_screen->row_data) > i) {
			/* Get the data for the row we're erasing. */
                        auto rowdata = _vte_ring_index_writable(m_screen->row_data, i);
			g_assert(rowdata != NULL);
			/* Remove it. */
			_vte_row_data_shrink (rowdata, 0);
			/* Add new cells until we fill the row. */
                        _vte_row_data_fill (rowdata, &m_fill_defaults, m_column_count);
			rowdata->attr.soft_wrapped = 0;
			/* Repaint the row. */
			invalidate_cells(0, m_column_count, i, 1);
		}
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Scroll the text, but don't move the cursor.  Negative = up, positive = down. */
void
VteTerminalPrivate::scroll_text(vte::grid::row_t scroll_amount)
{
        vte::grid::row_t start, end;
        if (m_scrolling_restricted) {
                start = m_screen->insert_delta + m_scrolling_region.start;
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                start = m_screen->insert_delta;
                end = start + m_row_count - 1;
	}

        while (_vte_ring_next(m_screen->row_data) <= end)
                ring_append(false);

	if (scroll_amount > 0) {
		for (auto i = 0; i < scroll_amount; i++) {
                        ring_remove(end);
                        ring_insert(start, true);
		}
	} else {
		for (auto i = 0; i < -scroll_amount; i++) {
                        ring_remove(start);
                        ring_insert(end, true);
		}
	}

	/* Update the display. */
        scroll_region(start, end - start + 1, scroll_amount);

	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();

	/* We've modified the display.  Make a note of it. */
        m_text_inserted_flag = TRUE;
        m_text_deleted_flag = TRUE;
}

/* Restore cursor. */
void
VteTerminalPrivate::seq_restore_cursor(vte::parser::Params const& params)
{
        restore_cursor();
}

void
VteTerminalPrivate::restore_cursor()
{
        restore_cursor(m_screen);
        ensure_cursor_is_onscreen();
}

/* Save cursor. */
void
VteTerminalPrivate::seq_save_cursor(vte::parser::Params const& params)
{
        save_cursor();
}

void
VteTerminalPrivate::save_cursor()
{
        save_cursor(m_screen);
}

/* Switch to normal screen. */
void
VteTerminalPrivate::switch_normal_screen()
{
        switch_screen(&m_normal_screen);
}

void
VteTerminalPrivate::switch_screen(VteScreen *new_screen)
{
        /* if (new_screen == m_screen) return; ? */

        /* The two screens use different hyperlink pools, so carrying on the idx
         * wouldn't make sense and could lead to crashes.
         * Ideally we'd carry the target URI itself, but I'm just lazy.
         * Also, run a GC before we switch away from that screen. */
        m_hyperlink_hover_idx = _vte_ring_get_hyperlink_at_position(m_screen->row_data, -1, -1, true, NULL);
        g_assert (m_hyperlink_hover_idx == 0);
        m_hyperlink_hover_uri = NULL;
        emit_hyperlink_hover_uri_changed(NULL);  /* FIXME only emit if really changed */
        m_defaults.attr.hyperlink_idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, NULL);
        g_assert (m_defaults.attr.hyperlink_idx == 0);

        /* cursor.row includes insert_delta, adjust accordingly */
        auto cr = m_screen->cursor.row - m_screen->insert_delta;
        m_screen = new_screen;
        m_screen->cursor.row = cr + m_screen->insert_delta;

        /* Make sure the ring is large enough */
        ensure_row();
}

/* Switch to alternate screen. */
void
VteTerminalPrivate::switch_alternate_screen()
{
        switch_screen(&m_alternate_screen);
}

/* Switch to normal screen and restore cursor (in this order). */
void
VteTerminalPrivate::switch_normal_screen_and_restore_cursor()
{
        switch_normal_screen();
        restore_cursor();
}

/* Save cursor and switch to alternate screen (in this order). */
void
VteTerminalPrivate::save_cursor_and_switch_alternate_screen()
{
        save_cursor();
        switch_alternate_screen();
}

/* Set icon/window titles. */
void
VteTerminalPrivate::set_title_internal(vte::parser::Params const& params,
                                       bool change_icon_title,
                                       bool change_window_title)
{
        if (change_icon_title == FALSE && change_window_title == FALSE)
		return;

	/* Get the string parameter's value. */
        char* title;
        if (!params.string_at(0, title))
                return;

			char *p, *validated;
			const char *end;

                        //FIXMEchpe why? it's guaranteed UTF-8 already
			/* Validate the text. */
			g_utf8_validate(title, strlen(title), &end);
			validated = g_strndup(title, end - title);

			/* No control characters allowed. */
			for (p = validated; *p != '\0'; p++) {
				if ((*p & 0x1f) == *p) {
					*p = ' ';
				}
			}

			/* Emit the signal */
                        if (change_window_title) {
                                g_free(m_window_title_changed);
                                m_window_title_changed = g_strdup(validated);
			}

                        if (change_icon_title) {
                                g_free(m_icon_title_changed);
                                m_icon_title_changed = g_strdup(validated);
			}

			g_free (validated);

        g_free(title);
}

/* Toggle a terminal mode. */
void
VteTerminalPrivate::set_mode(vte::parser::Params const& params,
                             bool value)
{
        auto n_params = params.size();
        if (n_params == 0)
                return;

	for (unsigned int i = 0; i < n_params; i++) {
                int setting;
                if (!params.number_at_unchecked(i, setting))
                        continue;

                switch (setting) {
                case 2:		/* keyboard action mode (?) */
                        break;
                case 4:		/* insert/overtype mode */
                        m_insert_mode = value;
                        break;
                case 12:	/* send/receive mode (local echo) */
                        m_sendrecv_mode = value;
                        break;
                case 20:	/* automatic newline / normal linefeed mode */
                        m_linefeed_mode = value;
                        break;
                default:
                        break;
                }
        }
}

void
VteTerminalPrivate::reset_mouse_smooth_scroll_delta()
{
	m_mouse_smooth_scroll_delta = 0.0;
}

typedef void (VteTerminalPrivate::* decset_handler_t)();

struct decset_t {
        gint16 setting;
        /* offset in VteTerminalPrivate (> 0) or VteScreen (< 0) */
        gint16 boffset;
        gint16 ioffset;
        gint16 poffset;
        gint16 fvalue;
        gint16 tvalue;
        decset_handler_t reset, set;
};

static int
decset_cmp(const void *va,
           const void *vb)
{
        const struct decset_t *a = (const struct decset_t *)va;
        const struct decset_t *b = (const struct decset_t *)vb;

        return a->setting < b->setting ? -1 : a->setting > b->setting;
}

/* Manipulate certain terminal attributes. */
void
VteTerminalPrivate::decset(vte::parser::Params const& params,
                           bool restore,
                           bool save,
                           bool set)
{

        auto n_params = params.size();
        for (unsigned int i = 0; i < n_params; i++) {
                int setting;

                if (!params.number_at(i, setting))
                        continue;

		decset(setting, restore, save, set);
	}
}

void
VteTerminalPrivate::decset(long setting,
                           bool restore,
                           bool save,
                           bool set)
{
	static const struct decset_t settings[] = {
#define PRIV_OFFSET(member) (G_STRUCT_OFFSET(VteTerminalPrivate, member))
#define SCREEN_OFFSET(member) (-G_STRUCT_OFFSET(VteScreen, member))
		/* 1: Application/normal cursor keys. */
		{1, 0, PRIV_OFFSET(m_cursor_mode), 0,
		 VTE_KEYMODE_NORMAL,
		 VTE_KEYMODE_APPLICATION,
		 nullptr, nullptr,},
		/* 2: disallowed, we don't do VT52. */
		{2, 0, 0, 0, 0, 0, nullptr, nullptr,},
                /* 3: DECCOLM set/reset to and from 132/80 columns */
                {3, 0, 0, 0,
                 FALSE,
                 TRUE,
                 nullptr, nullptr,},
		/* 5: Reverse video. */
                {5, PRIV_OFFSET(m_reverse_mode), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 6: Origin mode: when enabled, cursor positioning is
		 * relative to the scrolling region. */
                {6, PRIV_OFFSET(m_origin_mode), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 7: Wraparound mode. */
                {7, PRIV_OFFSET(m_autowrap), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 8: disallowed, keyboard repeat is set by user. */
		{8, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 9: Send-coords-on-click. */
		{9, 0, PRIV_OFFSET(m_mouse_tracking_mode), 0,
		 0,
		 MOUSE_TRACKING_SEND_XY_ON_CLICK,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,},
		/* 12: disallowed, cursor blinks is set by user. */
		{12, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 18: print form feed. */
		/* 19: set print extent to full screen. */
		/* 25: Cursor visible. */
		{25, PRIV_OFFSET(m_cursor_visible), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 30/rxvt: disallowed, scrollbar visibility is set by user. */
		{30, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 35/rxvt: disallowed, fonts set by user. */
		{35, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 38: enter Tektronix mode. */
                /* 40: Enable DECCOLM mode. */
                {40, PRIV_OFFSET(m_deccolm_mode), 0, 0,
                 FALSE,
                 TRUE,
                 nullptr, nullptr,},
		/* 41: more(1) fix. */
		/* 42: Enable NLS replacements. */
		/* 44: Margin bell. */
		/* 47: Alternate screen. */
                {47, 0, 0, 0,
                 0,
                 0,
                 &VteTerminalPrivate::switch_normal_screen,
                 &VteTerminalPrivate::switch_alternate_screen,},
		/* 66: Keypad mode. */
		{66, PRIV_OFFSET(m_keypad_mode), 0, 0,
		 VTE_KEYMODE_NORMAL,
		 VTE_KEYMODE_APPLICATION,
		 nullptr, nullptr,},
		/* 67: disallowed, backspace key policy is set by user. */
		{67, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 1000: Send-coords-on-button. */
		{1000, 0, PRIV_OFFSET(m_mouse_tracking_mode), 0,
		 0,
		 MOUSE_TRACKING_SEND_XY_ON_BUTTON,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,},
		/* 1001: Hilite tracking. */
		{1001, 0, PRIV_OFFSET(m_mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_HILITE_TRACKING),
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,},
		/* 1002: Cell motion tracking. */
		{1002, 0, PRIV_OFFSET(m_mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_CELL_MOTION_TRACKING),
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,},
		/* 1003: All motion tracking. */
		{1003, 0, PRIV_OFFSET(m_mouse_tracking_mode), 0,
		 (0),
		 (MOUSE_TRACKING_ALL_MOTION_TRACKING),
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,
                 &VteTerminalPrivate::reset_mouse_smooth_scroll_delta,},
		/* 1004: Focus tracking. */
		{1004, PRIV_OFFSET(m_focus_tracking_mode), 0, 0,
		 FALSE,
		 TRUE,
                 nullptr,
                 &VteTerminalPrivate::feed_focus_event_initial,},
		/* 1006: Extended mouse coordinates. */
		{1006, PRIV_OFFSET(m_mouse_xterm_extension), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 1007: Alternate screen scroll. */
		{1007, PRIV_OFFSET(m_alternate_screen_scroll), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 1010/rxvt: disallowed, scroll-on-output is set by user. */
		{1010, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 1011/rxvt: disallowed, scroll-on-keypress is set by user. */
		{1011, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 1015/urxvt: Extended mouse coordinates. */
		{1015, PRIV_OFFSET(m_mouse_urxvt_extension), 0, 0,
		 FALSE,
		 TRUE,
                 nullptr, nullptr,},
		/* 1035: disallowed, don't know what to do with it. */
		{1035, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 1036: Meta-sends-escape. */
		{1036, PRIV_OFFSET(m_meta_sends_escape), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
		/* 1037: disallowed, delete key policy is set by user. */
		{1037, 0, 0, 0, 0, 0, nullptr, nullptr,},
		/* 1047: Use alternate screen buffer. */
                {1047, 0, 0, 0,
                 0,
                 0,
                 &VteTerminalPrivate::switch_normal_screen,
                 &VteTerminalPrivate::switch_alternate_screen,},
		/* 1048: Save/restore cursor position. */
		{1048, 0, 0, 0,
		 0,
		 0,
                 &VteTerminalPrivate::restore_cursor,
                 &VteTerminalPrivate::save_cursor,},
		/* 1049: Use alternate screen buffer, saving the cursor
		 * position. */
                {1049, 0, 0, 0,
                 0,
                 0,
                 &VteTerminalPrivate::switch_normal_screen_and_restore_cursor,
                 &VteTerminalPrivate::save_cursor_and_switch_alternate_screen,},
		/* 2004: Bracketed paste mode. */
		{2004, PRIV_OFFSET(m_bracketed_paste_mode), 0, 0,
		 FALSE,
		 TRUE,
		 nullptr, nullptr,},
#undef PRIV_OFFSET
#undef SCREEN_OFFSET
	};
        struct decset_t key;
        struct decset_t *found;

	/* Handle the setting. */
        key.setting = setting;
        found = (struct decset_t *)bsearch(&key, settings, G_N_ELEMENTS(settings), sizeof(settings[0]), decset_cmp);
        if (!found) {
		_vte_debug_print (VTE_DEBUG_MISC,
				  "DECSET/DECRESET mode %ld not recognized, ignoring.\n",
				  setting);
                return;
	}

        key = *found;
        do {
                gboolean *bvalue = NULL;
                gint *ivalue = NULL;
                gpointer *pvalue = NULL, pfvalue = NULL, ptvalue = NULL;
                gpointer p;

		/* Handle settings we want to ignore. */
		if ((key.fvalue == key.tvalue) &&
		    (!key.set) &&
		    (!key.reset)) {
			break;
		}

#define STRUCT_MEMBER_P(type,total_offset) \
                (type) (total_offset >= 0 ? G_STRUCT_MEMBER_P(this, total_offset) : G_STRUCT_MEMBER_P(m_screen, -total_offset))

                if (key.boffset) {
                        bvalue = STRUCT_MEMBER_P(gboolean*, key.boffset);
                } else if (key.ioffset) {
                        ivalue = STRUCT_MEMBER_P(int*, key.ioffset);
                } else if (key.poffset) {
                        pvalue = STRUCT_MEMBER_P(gpointer*, key.poffset);
                        pfvalue = STRUCT_MEMBER_P(gpointer, key.fvalue);
                        ptvalue = STRUCT_MEMBER_P(gpointer, key.tvalue);
                }
#undef STRUCT_MEMBER_P

		/* Read the old setting. */
		if (restore) {
			p = g_hash_table_lookup(m_dec_saved,
						GINT_TO_POINTER(setting));
			set = (p != NULL);
			_vte_debug_print(VTE_DEBUG_PARSER,
					"Setting %ld was %s.\n",
					setting, set ? "set" : "unset");
		}
		/* Save the current setting. */
		if (save) {
			if (bvalue) {
				set = *(bvalue) != FALSE;
			} else
			if (ivalue) {
                                set = *(ivalue) == (int)key.tvalue;
			} else
			if (pvalue) {
				set = *(pvalue) == ptvalue;
			}
			_vte_debug_print(VTE_DEBUG_PARSER,
					"Setting %ld is %s, saving.\n",
					setting, set ? "set" : "unset");
			g_hash_table_insert(m_dec_saved,
					    GINT_TO_POINTER(setting),
					    GINT_TO_POINTER(set));
		}
		/* Change the current setting to match the new/saved value. */
		if (!save) {
			_vte_debug_print(VTE_DEBUG_PARSER,
					"Setting %ld to %s.\n",
					setting, set ? "set" : "unset");
			if (key.set && set) {
				(this->*key.set)();
			}
			if (bvalue) {
				*(bvalue) = set;
			} else
			if (ivalue) {
                                *(ivalue) = set ? (int)key.tvalue : (int)key.fvalue;
			} else
			if (pvalue) {
                                *(pvalue) = set ? ptvalue : pfvalue;
			}
			if (key.reset && !set) {
				(this->*key.reset)();
			}
		}
	} while (0);

	/* Do whatever's necessary when the setting changes. */
	switch (setting) {
	case 1:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering application cursor mode.\n" :
				"Leaving application cursor mode.\n");
		break;
	case 3:
                /* 3: DECCOLM set/reset to 132/80 columns mode, clear screen and cursor home */
                if (m_deccolm_mode) {
                        emit_resize_window(set ? 132 : 80,
                                           m_row_count);
                        clear_screen();
                        home_cursor();
                }
		break;
	case 5:
		/* Repaint everything in reverse mode. */
                invalidate_all();
		break;
	case 6:
		/* Reposition the cursor in its new home position. */
                home_cursor();
		break;
	case 47:
	case 1047:
	case 1049:
                /* Clear the alternate screen if we're switching to it */
		if (set) {
			clear_screen();
		}
		/* Reset scrollbars and repaint everything. */
		gtk_adjustment_set_value(m_vadjustment,
					 m_screen->scroll_delta);
		set_scrollback_lines(m_scrollback_lines);
                queue_contents_changed();
                invalidate_all();
		break;
	case 9:
	case 1000:
	case 1001:
	case 1002:
	case 1003:
                /* Mouse pointer might change. */
                apply_mouse_cursor();
		break;
	case 66:
		_vte_debug_print(VTE_DEBUG_KEYBOARD, set ?
				"Entering application keypad mode.\n" :
				"Leaving application keypad mode.\n");
		break;
	default:
		break;
	}
}

/* THE HANDLERS */

/* Do nothing. */
void
VteTerminalPrivate::seq_nop(vte::parser::Params const& params)
{
}

void
VteTerminalPrivate::set_character_replacements(unsigned slot,
                                               VteCharacterReplacement replacement)
{
        g_assert(slot < G_N_ELEMENTS(m_character_replacements));
        m_character_replacements[slot] = replacement;
}

/* G0 character set is a pass-thru (no mapping). */
void
VteTerminalPrivate::seq_designate_g0_plain(vte::parser::Params const& params)
{
        set_character_replacements(0, VTE_CHARACTER_REPLACEMENT_NONE);
}

/* G0 character set is DEC Special Character and Line Drawing Set. */
void
VteTerminalPrivate::seq_designate_g0_line_drawing(vte::parser::Params const& params)
{
        set_character_replacements(0, VTE_CHARACTER_REPLACEMENT_LINE_DRAWING);
}

/* G0 character set is British (# is converted to £). */
void
VteTerminalPrivate::seq_designate_g0_british(vte::parser::Params const& params)
{
        set_character_replacements(0, VTE_CHARACTER_REPLACEMENT_BRITISH);
}

/* G1 character set is a pass-thru (no mapping). */
void
VteTerminalPrivate::seq_designate_g1_plain(vte::parser::Params const& params)
{
        set_character_replacements(1, VTE_CHARACTER_REPLACEMENT_NONE);
}

/* G1 character set is DEC Special Character and Line Drawing Set. */
void
VteTerminalPrivate::seq_designate_g1_line_drawing(vte::parser::Params const& params)
{
        set_character_replacements(1, VTE_CHARACTER_REPLACEMENT_LINE_DRAWING);
}

/* G1 character set is British (# is converted to £). */
void
VteTerminalPrivate::seq_designate_g1_british(vte::parser::Params const& params)
{
        set_character_replacements(1, VTE_CHARACTER_REPLACEMENT_BRITISH);
}

void
VteTerminalPrivate::set_character_replacement(unsigned slot)
{
        g_assert(slot < G_N_ELEMENTS(m_character_replacements));
        m_character_replacement = &m_character_replacements[slot];
}

/* SI (shift in): switch to G0 character set. */
void
VteTerminalPrivate::seq_shift_in(vte::parser::Params const& params)
{
        set_character_replacement(0);
}

/* SO (shift out): switch to G1 character set. */
void
VteTerminalPrivate::seq_shift_out(vte::parser::Params const& params)
{
        set_character_replacement(1);
}

/* Beep. */
void
VteTerminalPrivate::seq_bell(vte::parser::Params const& params)
{
        m_bell_pending = true;
}

/* Backtab. */
void
VteTerminalPrivate::seq_cursor_back_tab(vte::parser::Params const& params)
{
	/* Calculate which column is the previous tab stop. */
        auto newcol = m_screen->cursor.col;

	if (m_tabstops) {
		/* Find the next tabstop. */
		while (newcol > 0) {
			newcol--;
                        if (get_tabstop(newcol % m_column_count)) {
				break;
			}
		}
	}

	/* Warp the cursor. */
	_vte_debug_print(VTE_DEBUG_PARSER,
			"Moving cursor to column %ld.\n", (long)newcol);
        set_cursor_column(newcol);
}

/* Clear from the cursor position (inclusive!) to the beginning of the line. */
void
VteTerminalPrivate::clear_to_bol()
{
        ensure_cursor_is_onscreen();

	/* Get the data for the row which the cursor points to. */
	auto rowdata = ensure_row();
        /* Clean up Tab/CJK fragments. */
        cleanup_fragments(0, m_screen->cursor.col + 1);
	/* Clear the data up to the current column with the default
	 * attributes.  If there is no such character cell, we need
	 * to add one. */
        vte::grid::column_t i;
        for (i = 0; i <= m_screen->cursor.col; i++) {
                if (i < (glong) _vte_row_data_length (rowdata)) {
			/* Muck with the cell in this location. */
                        auto pcell = _vte_row_data_get_writable(rowdata, i);
                        *pcell = m_color_defaults;
		} else {
			/* Add new cells until we have one here. */
                        _vte_row_data_append (rowdata, &m_color_defaults);
		}
	}
	/* Repaint this row. */
        invalidate_cells(0, m_screen->cursor.col+1,
                         m_screen->cursor.row, 1);

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Clear to the right of the cursor and below the current line. */
void
VteTerminalPrivate::clear_below_current()
{
        ensure_cursor_is_onscreen();

	/* If the cursor is actually on the screen, clear the rest of the
	 * row the cursor is on and all of the rows below the cursor. */
        VteRowData *rowdata;
        auto i = m_screen->cursor.row;
	if (i < _vte_ring_next(m_screen->row_data)) {
		/* Get the data for the row we're clipping. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, i);
                /* Clean up Tab/CJK fragments. */
                if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col)
                        cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
		/* Clear everything to the right of the cursor. */
		if (rowdata)
                        _vte_row_data_shrink(rowdata, m_screen->cursor.col);
	}
	/* Now for the rest of the lines. */
        for (i = m_screen->cursor.row + 1;
	     i < _vte_ring_next(m_screen->row_data);
	     i++) {
		/* Get the data for the row we're removing. */
		rowdata = _vte_ring_index_writable(m_screen->row_data, i);
		/* Remove it. */
		if (rowdata)
			_vte_row_data_shrink (rowdata, 0);
	}
	/* Now fill the cleared areas. */
        bool const not_default_bg = (m_fill_defaults.attr.back() != VTE_DEFAULT_BG);

        for (i = m_screen->cursor.row;
	     i < m_screen->insert_delta + m_row_count;
	     i++) {
		/* Retrieve the row's data, creating it if necessary. */
		if (_vte_ring_contains(m_screen->row_data, i)) {
			rowdata = _vte_ring_index_writable (m_screen->row_data, i);
			g_assert(rowdata != NULL);
		} else {
			rowdata = ring_append(false);
		}
		/* Pad out the row. */
                if (not_default_bg) {
                        _vte_row_data_fill(rowdata, &m_fill_defaults, m_column_count);
		}
		rowdata->attr.soft_wrapped = 0;
		/* Repaint this row. */
		invalidate_cells(0, m_column_count,
                                 i, 1);
	}

	/* We've modified the display.  Make a note of it. */
	m_text_deleted_flag = TRUE;
}

/* Clear from the cursor position to the end of the line. */
void
VteTerminalPrivate::clear_to_eol()
{
	/* If we were to strictly emulate xterm, we'd ensure the cursor is onscreen.
	 * But due to https://bugzilla.gnome.org/show_bug.cgi?id=740789 we intentionally
	 * deviate and do instead what konsole does. This way emitting a \e[K doesn't
	 * influence the text flow, and serves as a perfect workaround against a new line
	 * getting painted with the active background color (except for a possible flicker).
	 */
	/* ensure_cursor_is_onscreen(); */

	/* Get the data for the row which the cursor points to. */
        auto rowdata = ensure_row();
	g_assert(rowdata != NULL);
        if ((glong) _vte_row_data_length(rowdata) > m_screen->cursor.col) {
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, _vte_row_data_length(rowdata));
                /* Remove the data at the end of the array until the current column
                 * is the end of the array. */
                _vte_row_data_shrink(rowdata, m_screen->cursor.col);
		/* We've modified the display.  Make a note of it. */
		m_text_deleted_flag = TRUE;
	}
        bool const not_default_bg = (m_fill_defaults.attr.back() != VTE_DEFAULT_BG);

        if (not_default_bg) {
		/* Add enough cells to fill out the row. */
                _vte_row_data_fill(rowdata, &m_fill_defaults, m_column_count);
	}
	rowdata->attr.soft_wrapped = 0;
	/* Repaint this row. */
	invalidate_cells(m_screen->cursor.col, m_column_count - m_screen->cursor.col,
                         m_screen->cursor.row, 1);
}

/* Move the cursor to the given column (horizontal position), 1-based. */
void
VteTerminalPrivate::seq_cursor_character_absolute(vte::parser::Params const& params)
{
        auto value = params.number_or_default_at(0, 1) - 1;
        set_cursor_column(value);
}

/*
 * VteTerminalPrivate::set_cursor_column:
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor column to @col, clamped to the range 0..m_column_count-1.
 */
void
VteTerminalPrivate::set_cursor_column(vte::grid::column_t col)
{
        m_screen->cursor.col = CLAMP(col, 0, m_column_count - 1);
}

/*
 * VteTerminalPrivate::set_cursor_row:
 * @row: the row. 0-based and relative to the scrolling region
 *
 * Sets the cursor row to @row. @row is relative to the scrolling region
 * (0 if restricted scrolling is off).
 */
void
VteTerminalPrivate::set_cursor_row(vte::grid::row_t row)
{
        vte::grid::row_t start_row, end_row;
        if (m_origin_mode &&
            m_scrolling_restricted) {
                start_row = m_scrolling_region.start;
                end_row = m_scrolling_region.end;
        } else {
                start_row = 0;
                end_row = m_row_count - 1;
        }
        row += start_row;
        row = CLAMP(row, start_row, end_row);

        m_screen->cursor.row = row + m_screen->insert_delta;
}

/*
 * VteTerminalPrivate::get_cursor_row:
 *
 * Returns: the relative cursor row, 0-based and relative to the scrolling region
 * if set (regardless of origin mode).
 */
vte::grid::row_t
VteTerminalPrivate::get_cursor_row() const
{
        auto row = m_screen->cursor.row - m_screen->insert_delta;
        /* Note that we do NOT check m_origin_mode here! */
        if (m_scrolling_restricted) {
                row -= m_scrolling_region.start;
        }
        return row;
}

vte::grid::column_t
VteTerminalPrivate::get_cursor_column() const
{
        return m_screen->cursor.col;
}

/*
 * VteTerminalPrivate::set_cursor_coords:
 * @row: the row. 0-based and relative to the scrolling region
 * @col: the column. 0-based from 0 to m_column_count - 1
 *
 * Sets the cursor row to @row. @row is relative to the scrolling region
 * (0 if restricted scrolling is off).
 *
 * Sets the cursor column to @col, clamped to the range 0..m_column_count-1.
 */
void
VteTerminalPrivate::set_cursor_coords(vte::grid::row_t row,
                                      vte::grid::column_t column)
{
        set_cursor_column(column);
        set_cursor_row(row);
}

/* Move the cursor to the given position, 1-based. */
void
VteTerminalPrivate::seq_cursor_position(vte::parser::Params const& params)
{
        /* The first is the row, the second is the column. */
        auto rowval = params.number_or_default_at(0, 1) - 1;
        auto colval = params.number_or_default_at(1, 1) - 1;
        set_cursor_coords(rowval, colval);
}

/* Carriage return. */
void
VteTerminalPrivate::seq_carriage_return(vte::parser::Params const& params)
{
        set_cursor_column(0);
}

void
VteTerminalPrivate::reset_scrolling_region()
{
        m_scrolling_restricted = FALSE;
        home_cursor();
}

/* Restrict scrolling and updates to a subset of the visible lines. */
void
VteTerminalPrivate::seq_set_scrolling_region(vte::parser::Params const& params)
{
	/* We require two parameters.  Anything less is a reset. */
        //        if (params.size() < 2)
        //                return reset_scrolling_region();

        auto start = params.number_or_default_at_unchecked(0) - 1;
        auto end = params.number_or_default_at_unchecked(1) - 1;
        set_scrolling_region(start, end);
}

void
VteTerminalPrivate::set_scrolling_region(vte::grid::row_t start /* relative */,
                                         vte::grid::row_t end /* relative */)
{
        /* A (1-based) value of 0 means default. */
        if (start == -1) {
		start = 0;
	}
        if (end == -1) {
                end = m_row_count - 1;
        }
        /* Bail out on garbage, require at least 2 rows, as per xterm. */
        // FIXMEchpe
        if (start < 0 || start >= m_row_count - 1 || end < start + 1) {
                reset_scrolling_region();
                return;
        }
        if (end >= m_row_count) {
                end = m_row_count - 1;
	}

	/* Set the right values. */
        m_scrolling_region.start = start;
        m_scrolling_region.end = end;
        m_scrolling_restricted = TRUE;
        if (m_scrolling_region.start == 0 &&
            m_scrolling_region.end == m_row_count - 1) {
		/* Special case -- run wild, run free. */
                m_scrolling_restricted = FALSE;
	} else {
		/* Maybe extend the ring -- bug 710483 */
                while (_vte_ring_next(m_screen->row_data) < m_screen->insert_delta + m_row_count)
                        _vte_ring_insert(m_screen->row_data, _vte_ring_next(m_screen->row_data));
	}

        home_cursor();
}

/* Move the cursor to the beginning of the Nth next line, no scrolling. */
void
VteTerminalPrivate::seq_cursor_next_line(vte::parser::Params const& params)
{
        set_cursor_column(0);
        seq_cursor_down(params);
}

/* Move the cursor to the beginning of the Nth previous line, no scrolling. */
void
VteTerminalPrivate::seq_cursor_preceding_line(vte::parser::Params const& params)
{
        set_cursor_column(0);
        seq_cursor_up(params);
}

/* Move the cursor to the given row (vertical position), 1-based. */
void
VteTerminalPrivate::seq_line_position_absolute(vte::parser::Params const& params)
{
        // FIXMEchpe shouldn't we ensure_cursor_is_onscreen AFTER setting the new cursor row?
        ensure_cursor_is_onscreen();

        auto val = params.number_or_default_at(0, 1) - 1;
        set_cursor_row(val);
}

/* Delete a character at the current cursor position. */
void
VteTerminalPrivate::delete_character()
{
	VteRowData *rowdata;
	long col;

        ensure_cursor_is_onscreen();

        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		long len;
		/* Get the data for the row which the cursor points to. */
                rowdata = _vte_ring_index_writable(m_screen->row_data, m_screen->cursor.row);
		g_assert(rowdata != NULL);
                col = m_screen->cursor.col;
		len = _vte_row_data_length (rowdata);
		/* Remove the column. */
		if (col < len) {
                        /* Clean up Tab/CJK fragments. */
                        cleanup_fragments(col, col + 1);
			_vte_row_data_remove (rowdata, col);
                        bool const not_default_bg = (m_fill_defaults.attr.back() != VTE_DEFAULT_BG);

                        if (not_default_bg) {
                                _vte_row_data_fill(rowdata, &m_fill_defaults, m_column_count);
                                len = m_column_count;
			}
                        rowdata->attr.soft_wrapped = 0;
			/* Repaint this row. */
                        invalidate_cells(col, len - col,
                                         m_screen->cursor.row, 1);
		}
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Delete N characters at the current cursor position. */
void
VteTerminalPrivate::seq_delete_characters(vte::parser::Params const& params)
{
        auto val = std::max(std::min(params.number_or_default_at(0, 1),
                                     int(m_column_count - m_screen->cursor.col)),
                            int(1));
        for (auto i = 0; i < val; i++)
                delete_character();
}

/* Cursor down N lines, no scrolling. */
void
VteTerminalPrivate::seq_cursor_down(vte::parser::Params const& params)
{
        auto val = params.number_or_default_at(0, 1);
        move_cursor_down(val);
}

void
VteTerminalPrivate::move_cursor_down(vte::grid::row_t rows)
{
        rows = CLAMP(rows, 1, m_row_count);

        // FIXMEchpe why not do this afterwards?
        ensure_cursor_is_onscreen();

        vte::grid::row_t end;
        // FIXMEchpe why not check m_origin_mode here?
        if (m_scrolling_restricted) {
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                end = m_screen->insert_delta + m_row_count - 1;
	}

        m_screen->cursor.row = MIN(m_screen->cursor.row + rows, end);
}

/* Erase characters starting at the cursor position (overwriting N with
 * spaces, but not moving the cursor). */
void
VteTerminalPrivate::seq_erase_characters(vte::parser::Params const& params)
{
	/* If we got a parameter, use it. */
        auto count = std::min(params.number_or_default_at(0, 1), int(65535));
        erase_characters(count);
}

void
VteTerminalPrivate::erase_characters(long count)
{
	VteCell *cell;
	long col, i;

        ensure_cursor_is_onscreen();

	/* Clear out the given number of characters. */
	auto rowdata = ensure_row();
        if (_vte_ring_next(m_screen->row_data) > m_screen->cursor.row) {
		g_assert(rowdata != NULL);
                /* Clean up Tab/CJK fragments. */
                cleanup_fragments(m_screen->cursor.col, m_screen->cursor.col + count);
		/* Write over the characters.  (If there aren't enough, we'll
		 * need to create them.) */
		for (i = 0; i < count; i++) {
                        col = m_screen->cursor.col + i;
			if (col >= 0) {
				if (col < (glong) _vte_row_data_length (rowdata)) {
					/* Replace this cell with the current
					 * defaults. */
					cell = _vte_row_data_get_writable (rowdata, col);
                                        *cell = m_color_defaults;
				} else {
					/* Add new cells until we have one here. */
                                        _vte_row_data_fill (rowdata, &m_color_defaults, col + 1);
				}
			}
		}
		/* Repaint this row. */
                invalidate_cells(m_screen->cursor.col, count,
                                 m_screen->cursor.row, 1);
	}

	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Form-feed / next-page. */
void
VteTerminalPrivate::seq_form_feed(vte::parser::Params const& params)
{
        line_feed();
}

/* Insert a blank character. */
void
VteTerminalPrivate::insert_blank_character()
{
        ensure_cursor_is_onscreen();

        auto save = m_screen->cursor;
        insert_char(' ', true, true);
        m_screen->cursor = save;
}

/* Insert N blank characters. */
/* TODOegmont: Insert them in a single run, so that we call cleanup_fragments only once. */
void
VteTerminalPrivate::seq_insert_blank_characters(vte::parser::Params const& params)
{
        auto val = std::max(std::min(params.number_or_default_at(0, 1),
                                     int(m_column_count - m_screen->cursor.col)),
                            int(1));
        for (auto i = 0; i < val; i++)
                insert_blank_character();
}

/* REP: Repeat the last graphic character n times. */
void
VteTerminalPrivate::seq_repeat(vte::parser::Params const& params)
{
        auto val = std::min(params.number_or_default_at(0, 1),
                            int(65535)); // FIXMEchpe maybe limit more, to m_column_count - m_screen->cursor.col ?
        for (auto i = 0; i < val; i++) {
                // FIXMEchpe can't we move that check out of the loop?
                if (m_last_graphic_character == 0)
                        break;
                insert_char(m_last_graphic_character, false, true);
        }
}

/* Cursor down 1 line, with scrolling. */
void
VteTerminalPrivate::seq_index(vte::parser::Params const& params)
{
        line_feed();
}

/* Cursor left. */
void
VteTerminalPrivate::seq_backspace(vte::parser::Params const& params)
{
        ensure_cursor_is_onscreen();

        if (m_screen->cursor.col > 0) {
		/* There's room to move left, so do so. */
                m_screen->cursor.col--;
	}
}

/* Cursor left N columns. */
void
VteTerminalPrivate::seq_cursor_backward(vte::parser::Params const& params)
{
        auto val = params.number_or_default_at(0, 1);
        move_cursor_backward(val);
}

void
VteTerminalPrivate::move_cursor_backward(vte::grid::column_t columns)
{
        ensure_cursor_is_onscreen();

        auto col = get_cursor_column();
        columns = CLAMP(columns, 1, col);
        set_cursor_column(col - columns);
}

/* Cursor right N columns. */
void
VteTerminalPrivate::seq_cursor_forward(vte::parser::Params const& params)
{
        auto val = params.number_or_default_at(0, 1);
        move_cursor_forward(val);
}

void
VteTerminalPrivate::move_cursor_forward(vte::grid::column_t columns)
{
        columns = CLAMP(columns, 1, m_column_count);

        ensure_cursor_is_onscreen();

        /* The cursor can be further to the right, don't move in that case. */
        auto col = get_cursor_column();
        if (col < m_column_count) {
		/* There's room to move right. */
                set_cursor_column(col + columns);
	}
}

/* Move the cursor to the beginning of the next line, scrolling if necessary. */
void
VteTerminalPrivate::seq_next_line(vte::parser::Params const& params)
{
        set_cursor_column(0);
        cursor_down(true);
}

/* Scroll the text down N lines, but don't move the cursor. */
void
VteTerminalPrivate::seq_scroll_down(vte::parser::Params const& params)
{
        /* No ensure_cursor_is_onscreen() here as per xterm */
        auto val = std::max(params.number_or_default_at(0, 1), int(1));
        scroll_text(val);
}

/* Internal helper for changing color in the palette */
void
VteTerminalPrivate::change_color(vte::parser::Params const& params,
                                 const char *terminator)
{
        char **pairs;
        {
                char* str;
                if (!params.string_at(0, str))
                        return;

		pairs = g_strsplit (str, ";", 0);
                g_free(str);
        }

        if (!pairs)
                return;

        vte::color::rgb color;
        guint idx, i;

		for (i = 0; pairs[i] && pairs[i + 1]; i += 2) {
			idx = strtoul (pairs[i], (char **) NULL, 10);

			if (idx >= VTE_DEFAULT_FG)
				continue;

			if (color.parse(pairs[i + 1])) {
                                set_color(idx, VTE_COLOR_SOURCE_ESCAPE, color);
			} else if (strcmp (pairs[i + 1], "?") == 0) {
				gchar buf[128];
				auto c = get_color(idx);
				g_assert(c != NULL);
				g_snprintf (buf, sizeof (buf),
					    _VTE_CAP_OSC "4;%u;rgb:%04x/%04x/%04x%s",
					    idx, c->red, c->green, c->blue, terminator);
				feed_child(buf, -1);
			}
		}

		g_strfreev (pairs);

		/* emit the refresh as the palette has changed and previous
		 * renders need to be updated. */
		emit_refresh_window();
}

/* Change color in the palette, BEL terminated */
void
VteTerminalPrivate::seq_change_color_bel(vte::parser::Params const& params)
{
	change_color(params, BEL_C0);
}

/* Change color in the palette, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_color_st(vte::parser::Params const& params)
{
	change_color(params, ST_C0);
}

/* Reset color in the palette */
void
VteTerminalPrivate::seq_reset_color(vte::parser::Params const& params)
{
        auto n_params = params.size();
        if (n_params) {
                for (unsigned int i = 0; i < n_params; i++) {
                        int value;
                        if (!params.number_at_unchecked(i, value))
                                continue;

                        if (value < 0 || value >= VTE_DEFAULT_FG)
                                continue;

                        reset_color(value, VTE_COLOR_SOURCE_ESCAPE);
                }
	} else {
		for (unsigned int idx = 0; idx < VTE_DEFAULT_FG; idx++) {
			reset_color(idx, VTE_COLOR_SOURCE_ESCAPE);
		}
	}
}

/* Scroll the text up N lines, but don't move the cursor. */
void
VteTerminalPrivate::seq_scroll_up(vte::parser::Params const& params)
{
        /* No ensure_cursor_is_onscreen() here as per xterm */

        auto val = std::max(params.number_or_default_at(0, 1), int(1));
        scroll_text(-val);
}

/* Cursor down 1 line, with scrolling. */
void
VteTerminalPrivate::seq_line_feed(vte::parser::Params const& params)
{
        line_feed();
}

void
VteTerminalPrivate::line_feed()
{
        ensure_cursor_is_onscreen();
        cursor_down(true);
}

/* Cursor up 1 line, with scrolling. */
void
VteTerminalPrivate::seq_reverse_index(vte::parser::Params const& params)
{
        ensure_cursor_is_onscreen();

        vte::grid::row_t start, end;
        if (m_scrolling_restricted) {
                start = m_scrolling_region.start + m_screen->insert_delta;
                end = m_scrolling_region.end + m_screen->insert_delta;
	} else {
                start = m_screen->insert_delta;
                end = start + m_row_count - 1;
	}

        if (m_screen->cursor.row == start) {
		/* If we're at the top of the scrolling region, add a
		 * line at the top to scroll the bottom off. */
		ring_remove(end);
		ring_insert(start, true);
		/* Update the display. */
		scroll_region(start, end - start + 1, 1);
                invalidate_cells(0, m_column_count,
                                 start, 2);
	} else {
		/* Otherwise, just move the cursor up. */
                m_screen->cursor.row--;
	}
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We modified the display, so make a note of it. */
        m_text_modified_flag = TRUE;
}

/* Set tab stop in the current column. */
void
VteTerminalPrivate::seq_tab_set(vte::parser::Params const& params)
{
	if (m_tabstops == NULL) {
		m_tabstops = g_hash_table_new(NULL, NULL);
	}
	set_tabstop(m_screen->cursor.col);
}

/* Tab. */
void
VteTerminalPrivate::seq_tab(vte::parser::Params const& params)
{
        move_cursor_tab();
}

void
VteTerminalPrivate::move_cursor_tab()
{
        long old_len;
        vte::grid::column_t newcol, col;

	/* Calculate which column is the next tab stop. */
        newcol = col = m_screen->cursor.col;

	g_assert (col >= 0);

	if (m_tabstops != NULL) {
		/* Find the next tabstop. */
		for (newcol++; newcol < VTE_TAB_MAX; newcol++) {
			if (get_tabstop(newcol)) {
				break;
			}
		}
	}

	/* If we have no tab stops or went past the end of the line, stop
	 * at the right-most column. */
	if (newcol >= m_column_count) {
		newcol = m_column_count - 1;
	}

	/* but make sure we don't move cursor back (bug #340631) */
	if (col < newcol) {
		VteRowData *rowdata = ensure_row();

		/* Smart tab handling: bug 353610
		 *
		 * If we currently don't have any cells in the space this
		 * tab creates, we try to make the tab character copyable,
		 * by appending a single tab char with lots of fragment
		 * cells following it.
		 *
		 * Otherwise, just append empty cells that will show up
		 * as a space each.
		 */

		old_len = _vte_row_data_length (rowdata);
                _vte_row_data_fill (rowdata, &basic_cell, newcol);

		/* Insert smart tab if there's nothing in the line after
		 * us, not even empty cells (with non-default background
		 * color for example).
		 *
		 * Notable bugs here: 545924, 597242, 764330 */
		if (col >= old_len && newcol - col <= VTE_TAB_WIDTH_MAX) {
			glong i;
			VteCell *cell = _vte_row_data_get_writable (rowdata, col);
			VteCell tab = *cell;
			tab.attr.set_columns(newcol - col);
			tab.c = '\t';
			/* Save tab char */
			*cell = tab;
			/* And adjust the fragments */
			for (i = col + 1; i < newcol; i++) {
				cell = _vte_row_data_get_writable (rowdata, i);
				cell->c = '\t';
				cell->attr.set_columns(1);
				cell->attr.set_fragment(true);
			}
		}

		invalidate_cells(m_screen->cursor.col, newcol - m_screen->cursor.col,
                                 m_screen->cursor.row, 1);
                m_screen->cursor.col = newcol;
	}
}

void
VteTerminalPrivate::seq_cursor_forward_tabulation(vte::parser::Params const& params)
{
        auto val = std::max(std::min(params.number_or_default_at(0, 1),
                                     int(m_column_count - m_screen->cursor.col)),
                            int(1));
        for (auto i = 0; i < val; i++)
                move_cursor_tab();
}

/* Clear tabs selectively. */
void
VteTerminalPrivate::seq_tab_clear(vte::parser::Params const& params)
{
        auto param = params.number_or_default_at(0, 0);

	if (param == 0) {
		clear_tabstop(m_screen->cursor.col);
	} else if (param == 3) {
		if (m_tabstops != nullptr) {
			g_hash_table_destroy(m_tabstops);
			m_tabstops = nullptr;
		}
	}
}

/* Cursor up N lines, no scrolling. */
void
VteTerminalPrivate::seq_cursor_up(vte::parser::Params const& params)
{
        auto val = params.number_or_default_at(0, 1);
        move_cursor_up(val);
}

void
VteTerminalPrivate::move_cursor_up(vte::grid::row_t rows)
{
        rows = CLAMP(rows, 1, m_row_count);

        //FIXMEchpe why not do this afterward?
        ensure_cursor_is_onscreen();

        vte::grid::row_t start;
        //FIXMEchpe why not check m_origin_mode here?
        if (m_scrolling_restricted) {
                start = m_screen->insert_delta + m_scrolling_region.start;
	} else {
		start = m_screen->insert_delta;
	}

        m_screen->cursor.row = MAX(m_screen->cursor.row - rows, start);
}

/* Vertical tab. */
void
VteTerminalPrivate::seq_vertical_tab(vte::parser::Params const& params)
{
        line_feed();
}

/* Parse parameters of SGR 38, 48 or 58, starting at @index within @params.
 * If @might_contain_color_space_id, a true color sequence sequence is started, and after
 * its leading number "2" at least 4 more parameters are present, then there's an (ignored)
 * color_space_id before the three color components. See the comment below in
 * seq_character_attributes() to understand the different accepted formats.
 * Returns the color index, or -1 on error.
 * Increments @index to point to the last consumed parameter (not beyond). */

template<unsigned int redbits, unsigned int greenbits, unsigned int bluebits>
int32_t
VteTerminalPrivate::parse_sgr_38_48_parameters(vte::parser::Params const& params,
                                               unsigned int *index,
                                               bool might_contain_color_space_id)
{
        auto n_params = params.size();
        if (*index < n_params) {
                int param0;
                if (G_UNLIKELY(!params.number_at_unchecked(*index, param0)))
                        return -1;

		switch (param0) {
                case 2: {
                        if (G_UNLIKELY(*index + 3 >= n_params))
				return -1;
                        if (might_contain_color_space_id && *index + 5 <= n_params)
			        *index += 1;

                        int param1, param2, param3;
                        if (G_UNLIKELY(!params.number_at_unchecked(*index + 1, param1) ||
                                       !params.number_at_unchecked(*index + 2, param2) ||
                                       !params.number_at_unchecked(*index + 3, param3)))
                                return -1;

			if (G_UNLIKELY (param1 < 0 || param1 >= 256 || param2 < 0 || param2 >= 256 || param3 < 0 || param3 >= 256))
				return -1;
			*index += 3;

			return VTE_RGB_COLOR(redbits, greenbits, bluebits, param1, param2, param3);
                }
                case 5: {
                        int param1;
                        if (G_UNLIKELY(!params.number_at(*index + 1, param1)))
                                return -1;

                        if (G_UNLIKELY(param1 < 0 || param1 >= 256))
				return -1;
			*index += 1;
			return param1;
                }
		}
	}
	return -1;
}

/* Handle ANSI color setting and related stuffs (SGR).
 * @params contains the values split at semicolons, with sub arrays splitting at colons
 * wherever colons were encountered. */
void
VteTerminalPrivate::seq_character_attributes(vte::parser::Params const& params)
{
	/* Step through each numeric parameter. */
        auto n_params = params.size();
        unsigned int i;
	for (i = 0; i < n_params; i++) {
		/* If this parameter is an array, it can be a fully colon separated 38 or 48
		 * (see below for details). */
		if (G_UNLIKELY(params.has_subparams_at_unchecked(i))) {
                        auto subparams = params.subparams_at_unchecked(i);

                        int param0, param1;
                        if (G_UNLIKELY(!subparams.number_at(0, param0)))
                                continue;

                        switch (param0) {
                        case 4:
                                if (subparams.number_at(1, param1) && param1 >= 0 && param1 <= 3)
                                        m_defaults.attr.set_underline(param1);
                                break;
                        case 38: {
                                unsigned int index = 1;
                                auto color = parse_sgr_38_48_parameters<8, 8, 8>(subparams, &index, true);
                                if (G_LIKELY (color != -1))
                                        m_defaults.attr.set_fore(color);
                                break;
                        }
                        case 48: {
                                unsigned int index = 1;
                                auto color = parse_sgr_38_48_parameters<8, 8, 8>(subparams, &index, true);
                                if (G_LIKELY (color != -1))
                                        m_defaults.attr.set_back(color);
                                break;
                        }
                        case 58: {
                                unsigned int index = 1;
                                auto color = parse_sgr_38_48_parameters<4, 5, 4>(subparams, &index, true);
                                if (G_LIKELY (color != -1))
                                        m_defaults.attr.set_deco(color);
                                break;
                        }
                        }

			continue;
		}
		/* If this parameter is not a number either, skip it. */
                int param;
                if (!params.number_at_unchecked(i, param))
                        continue;

		switch (param) {
                case -1:
		case 0:
                        reset_default_attributes(false);
			break;
		case 1:
                        m_defaults.attr.set_bold(true);
			break;
		case 2:
                        m_defaults.attr.set_dim(true);
			break;
		case 3:
                        m_defaults.attr.set_italic(true);
			break;
		case 4:
                        m_defaults.attr.set_underline(1);
			break;
		case 5:
                        m_defaults.attr.set_blink(true);
			break;
		case 7:
                        m_defaults.attr.set_reverse(true);
			break;
		case 8:
                        m_defaults.attr.set_invisible(true);
			break;
		case 9:
                        m_defaults.attr.set_strikethrough(true);
			break;
                case 21:
                        m_defaults.attr.set_underline(2);
                        break;
		case 22: /* ECMA 48. */
                        m_defaults.attr.unset(VTE_ATTR_BOLD_MASK | VTE_ATTR_DIM_MASK);
			break;
		case 23:
                        m_defaults.attr.set_italic(false);
			break;
		case 24:
                        m_defaults.attr.set_underline(0);
			break;
		case 25:
                        m_defaults.attr.set_blink(false);
			break;
		case 27:
                        m_defaults.attr.set_reverse(false);
			break;
		case 28:
                        m_defaults.attr.set_invisible(false);
			break;
		case 29:
                        m_defaults.attr.set_strikethrough(false);
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
                        m_defaults.attr.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 30));
			break;
		case 38:
		case 48:
                case 58:
		{
			/* The format looks like:
			 * - 256 color indexed palette:
                         *   - ^[[38:5:INDEXm  (de jure standard: ITU-T T.416 / ISO/IEC 8613-6; we also allow and ignore further parameters)
                         *   - ^[[38;5;INDEXm  (de facto standard, understood by probably all terminal emulators that support 256 colors)
			 * - true colors:
                         *   - ^[[38:2:[id]:RED:GREEN:BLUE[:...]m  (de jure standard: ITU-T T.416 / ISO/IEC 8613-6)
                         *   - ^[[38:2:RED:GREEN:BLUEm             (common misinterpretation of the standard, FIXME: stop supporting it at some point)
                         *   - ^[[38;2;RED;GREEN;BLUEm             (de facto standard, understood by probably all terminal emulators that support true colors)
                         * See bugs 685759 and 791456 for details.
                         * The colon version was handled above separately.
                         * This branch here is reached when the separators are semicolons. */
			if ((i + 1) < n_params) {
                                ++i;
                                int32_t color;
                                switch (param) {
                                case 38:
                                        color = parse_sgr_38_48_parameters<8 ,8 ,8>(params, &i, false);
                                        if (G_LIKELY (color != -1))
                                                m_defaults.attr.set_fore(color);
                                        break;
                                case 48:
                                        color = parse_sgr_38_48_parameters<8, 8, 8>(params, &i, false);
                                        if (G_LIKELY (color != -1))
                                                m_defaults.attr.set_back(color);
                                        break;
                                case 58:
                                        color = parse_sgr_38_48_parameters<4, 5, 4>(params, &i, false);
                                        g_printerr("Parsed semicoloned deco colour: %x\n", color);
                                        if (G_LIKELY (color != -1))
                                                m_defaults.attr.set_deco(color);
                                        break;
				}
			}
			break;
		}
		case 39:
			/* default foreground */
                        m_defaults.attr.set_fore(VTE_DEFAULT_FG);
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
                        m_defaults.attr.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 40));
			break;
	     /* case 48: was handled above at 38 to avoid code duplication */
		case 49:
			/* default background */
                        m_defaults.attr.set_back(VTE_DEFAULT_BG);
			break;
                case 53:
                        m_defaults.attr.set_overline(true);
                        break;
                case 55:
                        m_defaults.attr.set_overline(false);
                        break;
             /* case 58: was handled above at 38 to avoid code duplication */
                case 59:
                        /* default decoration color, that is, same as the cell's foreground */
                        m_defaults.attr.set_deco(VTE_DEFAULT_FG);
                        break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
                        m_defaults.attr.set_fore(VTE_LEGACY_COLORS_OFFSET + (param - 90) +
                                                 VTE_COLOR_BRIGHT_OFFSET);
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
                        m_defaults.attr.set_back(VTE_LEGACY_COLORS_OFFSET + (param - 100) +
                                                 VTE_COLOR_BRIGHT_OFFSET);
			break;
		}
	}
	/* If we had no parameters, default to the defaults. */
	if (i == 0) {
                reset_default_attributes(false);
	}
	/* Save the new colors. */
        m_color_defaults.attr.copy_colors(m_defaults.attr);
        m_fill_defaults.attr.copy_colors(m_defaults.attr);
}

/* Move the cursor to the given column in the top row, 1-based. */
void
VteTerminalPrivate::seq_cursor_position_top_row(vte::parser::Params const& params)
{
        auto colval = params.number_or_default_at(0, 1) - 1;
        set_cursor_coords(0, colval);

}

/* Request terminal attributes. */
void
VteTerminalPrivate::seq_request_terminal_parameters(vte::parser::Params const& params)
{
	feed_child("\e[?x", -1);
}

/* Request terminal attributes. */
void
VteTerminalPrivate::seq_return_terminal_status(vte::parser::Params const& params)
{
	feed_child("", 0);
}

/* Send primary device attributes. */
void
VteTerminalPrivate::seq_send_primary_device_attributes(vte::parser::Params const& params)
{
        // FIXMEchpe only send anything when param==0 as per ECMA48
	/* Claim to be a VT220 with only national character set support. */
        feed_child("\e[?62;c", -1);
}

/* Send terminal ID. */
void
VteTerminalPrivate::seq_return_terminal_id(vte::parser::Params const& params)
{
	seq_send_primary_device_attributes(params);
}

/* Send secondary device attributes. */
void
VteTerminalPrivate::seq_send_secondary_device_attributes(vte::parser::Params const& params)
{
	char **version;
	char buf[128];
	long ver = 0, i;
	/* Claim to be a VT220, more or less.  The '>' in the response appears
	 * to be undocumented. */
	version = g_strsplit(VERSION, ".", 0);
	if (version != NULL) {
		for (i = 0; version[i] != NULL; i++) {
			ver = ver * 100;
			ver += atol(version[i]);
		}
		g_strfreev(version);
	}
	g_snprintf(buf, sizeof (buf), _VTE_CAP_ESC "[>1;%ld;0c", ver);
	feed_child(buf, -1);
}

/* Set one or the other. */
void
VteTerminalPrivate::seq_set_icon_title(vte::parser::Params const& params)
{
	set_title_internal(params, true, false);
}

void
VteTerminalPrivate::seq_set_window_title(vte::parser::Params const& params)
{
	set_title_internal(params, false, true);
}

/* Set both the window and icon titles to the same string. */
void
VteTerminalPrivate::seq_set_icon_and_window_title(vte::parser::Params const& params)
{
	set_title_internal(params, true, true);
}

void
VteTerminalPrivate::seq_set_current_directory_uri(vte::parser::Params const& params)
{
        char* uri = nullptr;
        if (params.string_at(0, uri)) {
                /* Validate URI */
                if (uri[0]) {
                        auto filename = g_filename_from_uri (uri, nullptr, nullptr);
                        if (filename == nullptr) {
                                /* invalid URI */
                                g_free (uri);
                                uri = nullptr;
                        } else {
                                g_free (filename);
                        }
                } else {
                        g_free(uri);
                        uri = nullptr;
                }
        }

        g_free(m_current_directory_uri_changed);
        m_current_directory_uri_changed = uri /* adopt */;
}

void
VteTerminalPrivate::seq_set_current_file_uri(vte::parser::Params const& params)
{
        char* uri = nullptr;
        if (params.string_at(0, uri)) {
                /* Validate URI */
                if (uri[0]) {
                        auto filename = g_filename_from_uri (uri, nullptr, nullptr);
                        if (filename == nullptr) {
                                /* invalid URI */
                                g_free (uri);
                                uri = nullptr;
                        } else {
                                g_free (filename);
                        }
                } else {
                        g_free(uri);
                        uri = nullptr;
                }
        }

        g_free(m_current_file_uri_changed);
        m_current_file_uri_changed = uri /* adopt */;
}

/* Handle OSC 8 hyperlinks.
 * See bug 779734 and https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda. */
void
VteTerminalPrivate::seq_set_current_hyperlink(vte::parser::Params const& params)
{

        char* hyperlink_params = nullptr;
        char* uri = nullptr;
        if (params.size() >= 2) {
                params.string_at_unchecked(0, hyperlink_params);
                params.string_at_unchecked(1, uri);
        }

        set_current_hyperlink(hyperlink_params, uri);
}

void
VteTerminalPrivate::set_current_hyperlink(char *hyperlink_params /* adopted */,
                                          char* uri /* adopted */)
{
        guint idx;
        char *id = NULL;
        char idbuf[24];

        if (!m_allow_hyperlink)
                return;

        /* Get the "id" parameter */
        if (hyperlink_params) {
                if (strncmp(hyperlink_params, "id=", 3) == 0) {
                        id = hyperlink_params + 3;
                } else {
                        id = strstr(hyperlink_params, ":id=");
                        if (id)
                                id += 4;
                }
        }
        if (id) {
                *strchrnul(id, ':') = '\0';
        }
        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                          "OSC 8: id=\"%s\" uri=\"%s\"\n",
                          id, uri);

        if (uri && strlen(uri) > VTE_HYPERLINK_URI_LENGTH_MAX) {
                _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                  "Overlong URI ignored: \"%s\"\n",
                                  uri);
                uri[0] = '\0';
        }

        if (id && strlen(id) > VTE_HYPERLINK_ID_LENGTH_MAX) {
                _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                  "Overlong \"id\" ignored: \"%s\"\n",
                                  id);
                id[0] = '\0';
        }

        if (uri && uri[0]) {
                /* The hyperlink, as we carry around and store in the streams, is "id;uri" */
                char *hyperlink;

                if (!id || !id[0]) {
                        /* Automatically generate a unique ID string. The colon makes sure
                         * it cannot conflict with an explicitly specified one. */
                        sprintf(idbuf, ":%ld", m_hyperlink_auto_id++);
                        id = idbuf;
                        _vte_debug_print (VTE_DEBUG_HYPERLINK,
                                          "Autogenerated id=\"%s\"\n",
                                          id);
                }
                hyperlink = g_strdup_printf("%s;%s", id, uri);
                idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, hyperlink);
                g_free (hyperlink);
        } else {
                /* idx = 0; also remove the previous current_idx so that it can be GC'd now. */
                idx = _vte_ring_get_hyperlink_idx(m_screen->row_data, NULL);
        }

        m_defaults.attr.hyperlink_idx = idx;

        g_free(hyperlink_params);
        g_free(uri);
}

/* Restrict the scrolling region. */
void
VteTerminalPrivate::seq_set_scrolling_region_from_start(vte::parser::Params const& params)
{
        /* We require a parameters.  Anything less is a reset. */
        if (params.size() < 1)
                return reset_scrolling_region();

        auto end = params.number_or_default_at(1) - 1;
        set_scrolling_region(-1, end);

}

void
VteTerminalPrivate::seq_set_scrolling_region_to_end(vte::parser::Params const& params)
{
        /* We require a parameters.  Anything less is a reset. */
        if (params.size() < 1)
                return reset_scrolling_region();

        auto start = params.number_or_default_at(0) - 1;
        set_scrolling_region(start, -1);

}

void
VteTerminalPrivate::set_keypad_mode(VteKeymode mode)
{
        m_keypad_mode = mode;
}

/* Same as cursor_character_absolute, not widely supported. */
void
VteTerminalPrivate::seq_character_position_absolute(vte::parser::Params const& params)
{
        seq_cursor_character_absolute (params);
}

/* Set certain terminal attributes. */
void
VteTerminalPrivate::seq_set_mode(vte::parser::Params const& params)
{
        set_mode(params, true);
}

/* Unset certain terminal attributes. */
void
VteTerminalPrivate::seq_reset_mode(vte::parser::Params const& params)
{
        set_mode(params, false);
}

/* Set certain terminal attributes. */
void
VteTerminalPrivate::seq_decset(vte::parser::Params const& params)
{
        decset(params, false, false, true);
}

/* Unset certain terminal attributes. */
void
VteTerminalPrivate::seq_decreset(vte::parser::Params const& params)
{
        decset(params, false, false, false);
}

/* Erase certain lines in the display. */
void
VteTerminalPrivate::seq_erase_in_display(vte::parser::Params const& params)
{
}

void
VteTerminalPrivate::erase_in_display(vte::parser::Sequence const& seq)
{
        /* We don't implement the protected attribute, so we can ignore selective:
         * bool selective = (seq.command() == VTE_CMD_DECSED);
         */

	switch (seq[0]) {
        case -1: /* default */
	case 0:
		/* Clear below the current line. */
                clear_below_current();
		break;
	case 1:
		/* Clear above the current line. */
                clear_above_current();
		/* Clear everything to the left of the cursor, too. */
		/* FIXME: vttest. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire screen. */
                clear_screen();
		break;
        case 3:
                /* Drop the scrollback. */
                drop_scrollback();
                break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

void
VteTerminalPrivate::seq_erase_in_line(vte::parser::Params const& params)
{
}

void
VteTerminalPrivate::erase_in_line(vte::parser::Sequence const& seq)
{
        /* We don't implement the protected attribute, so we can ignore selective:
         * bool selective = (seq.command() == VTE_CMD_DECSEL);
         */

	switch (seq[0]) {
        case -1: /* default */
	case 0:
		/* Clear to end of the line. */
                clear_to_eol();
		break;
	case 1:
		/* Clear to start of the line. */
                clear_to_bol();
		break;
	case 2:
		/* Clear the entire line. */
                clear_current_line();
		break;
	default:
		break;
	}
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Perform a full-bore reset. */
void
VteTerminalPrivate::seq_full_reset(vte::parser::Params const& params)
{
	reset(true, true);
}

/* Insert a certain number of lines below the current cursor. */
void
VteTerminalPrivate::seq_insert_lines(vte::parser::Params const& params)
{
	/* The default is one. */
        auto param = params.number_or_default_at(0, 1);
        insert_lines(param);
}

void
VteTerminalPrivate::insert_lines(vte::grid::row_t param)
{
        vte::grid::row_t end, i;

	/* Find the region we're messing with. */
        auto row = m_screen->cursor.row;
        if (m_scrolling_restricted) {
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                end = m_screen->insert_delta + m_row_count - 1;
	}

	/* Only allow to insert as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        auto limit = end - row + 1;
        param = MIN (param, limit);

	for (i = 0; i < param; i++) {
		/* Clear a line off the end of the region and add one to the
		 * top of the region. */
                ring_remove(end);
                ring_insert(row, true);
	}
        m_screen->cursor.col = 0;
	/* Update the display. */
        scroll_region(row, end - row + 1, param);
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We've modified the display.  Make a note of it. */
        m_text_inserted_flag = TRUE;
}

/* Delete certain lines from the scrolling region. */
void
VteTerminalPrivate::seq_delete_lines(vte::parser::Params const& params)
{
	/* The default is one. */
        auto param = params.number_or_default_at(0, 1);
        delete_lines(param);
}

void
VteTerminalPrivate::delete_lines(vte::grid::row_t param)
{
        vte::grid::row_t end, i;

	/* Find the region we're messing with. */
        auto row = m_screen->cursor.row;
        if (m_scrolling_restricted) {
                end = m_screen->insert_delta + m_scrolling_region.end;
	} else {
                end = m_screen->insert_delta + m_row_count - 1;
	}

        /* Only allow to delete as many lines as there are between this row
         * and the end of the scrolling region. See bug #676090.
         */
        auto limit = end - row + 1;
        param = MIN (param, limit);

	/* Clear them from below the current cursor. */
	for (i = 0; i < param; i++) {
		/* Insert a line at the end of the region and remove one from
		 * the top of the region. */
                ring_remove(row);
                ring_insert(end, true);
	}
        m_screen->cursor.col = 0;
	/* Update the display. */
        scroll_region(row, end - row + 1, -param);
	/* Adjust the scrollbars if necessary. */
        adjust_adjustments();
	/* We've modified the display.  Make a note of it. */
        m_text_deleted_flag = TRUE;
}

/* Device status reports. The possible reports are the cursor position and
 * whether or not we're okay. */
void
VteTerminalPrivate::seq_device_status_report(vte::parser::Params const& params)
{
        int param;
        if (!params.number_at(0, param))
                return;

        switch (param) {
			case 5:
				/* Send a thumbs-up sequence. */
				feed_child(_VTE_CAP_CSI "0n", -1);
				break;
			case 6:
				/* Send the cursor position. */
                                vte::grid::row_t rowval, origin, rowmax;
                                if (m_origin_mode &&
                                    m_scrolling_restricted) {
                                        origin = m_scrolling_region.start;
                                        rowmax = m_scrolling_region.end;
                                } else {
                                        origin = 0;
                                        rowmax = m_row_count - 1;
                                }
                                // FIXMEchpe this looks wrong. shouldn't this first clamp to origin,rowmax and *then* subtract origin?
                                rowval = m_screen->cursor.row - m_screen->insert_delta - origin;
                                rowval = CLAMP(rowval, 0, rowmax);
                                char buf[128];
                                g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "%ld;%ldR",
                                           rowval + 1,
                                           CLAMP(m_screen->cursor.col + 1, 1, m_column_count));
				feed_child(buf, -1);
				break;
			default:
				break;
        }
}

/* DEC-style device status reports. */
void
VteTerminalPrivate::seq_dec_device_status_report(vte::parser::Params const& params)
{
        int param;
        if (!params.number_at(0, param))
                return;

        switch (param) {
			case 6:
				/* Send the cursor position. */
                                vte::grid::row_t rowval, origin, rowmax;
                                if (m_origin_mode &&
                                    m_scrolling_restricted) {
                                        origin = m_scrolling_region.start;
                                        rowmax = m_scrolling_region.end;
                                } else {
                                        origin = 0;
                                        rowmax = m_row_count - 1;
                                }
                                // FIXMEchpe this looks wrong. shouldn't this first clamp to origin,rowmax and *then* subtract origin?
                                rowval = m_screen->cursor.row - m_screen->insert_delta - origin;
                                rowval = CLAMP(rowval, 0, rowmax);
                                char buf[128];
				g_snprintf(buf, sizeof(buf),
					   _VTE_CAP_CSI "?%ld;%ldR",
                                           rowval + 1,
                                           CLAMP(m_screen->cursor.col + 1, 1, m_column_count));
				feed_child(buf, -1);
				break;
			case 15:
				/* Send printer status -- 10 = ready,
				 * 11 = not ready.  We don't print. */
				feed_child(_VTE_CAP_CSI "?11n", -1);
				break;
			case 25:
				/* Send UDK status -- 20 = locked,
				 * 21 = not locked.  I don't even know what
				 * that means, but punt anyway. */
				feed_child(_VTE_CAP_CSI "?20n", -1);
				break;
			case 26:
				/* Send keyboard status.  50 = no locator. */
				feed_child(_VTE_CAP_CSI "?50n", -1);
				break;
			default:
				break;
        }
}

/* Restore a certain terminal attribute. */
void
VteTerminalPrivate::seq_restore_mode(vte::parser::Params const& params)
{
        decset(params, true, false, false);
}

/* Save a certain terminal attribute. */
void
VteTerminalPrivate::seq_save_mode(vte::parser::Params const& params)
{
        decset(params, false, true, false);
}

/* Perform a screen alignment test -- fill all visible cells with the
 * letter "E". */
void
VteTerminalPrivate::seq_screen_alignment_test(vte::parser::Params const& params)
{
	for (auto row = m_screen->insert_delta;
	     row < m_screen->insert_delta + m_row_count;
	     row++) {
		/* Find this row. */
                while (_vte_ring_next(m_screen->row_data) <= row)
                        ring_append(false);
                adjust_adjustments();
                auto rowdata = _vte_ring_index_writable (m_screen->row_data, row);
		g_assert(rowdata != NULL);
		/* Clear this row. */
		_vte_row_data_shrink (rowdata, 0);

                emit_text_deleted();
		/* Fill this row. */
                VteCell cell;
		cell.c = 'E';
		cell.attr = basic_cell.attr;
		cell.attr.set_columns(1);
                _vte_row_data_fill(rowdata, &cell, m_column_count);
                emit_text_inserted();
	}
        invalidate_all();

	/* We modified the display, so make a note of it for completeness. */
        m_text_modified_flag = TRUE;
}

/* DECSCUSR set cursor style */
void
VteTerminalPrivate::seq_set_cursor_style(vte::parser::Params const& params)
{
        auto n_params = params.size();
        if (n_params > 1)
                return;

        int style;
        if (n_params == 0) {
                /* no parameters means default (according to vt100.net) */
                style = VTE_CURSOR_STYLE_TERMINAL_DEFAULT;
        } else {
                if (!params.number_at(0, style))
                        return;
                if (style < 0 || style > 6) {
                        return;
                }
        }

        set_cursor_style(VteCursorStyle(style));
}

/* Perform a soft reset. */
void
VteTerminalPrivate::seq_soft_reset(vte::parser::Params const& params)
{
	reset(false, false);
}

/* Window manipulation control sequences.  Most of these are considered
 * bad ideas, but they're implemented as signals which the application
 * is free to ignore, so they're harmless.  Handle at most one action,
 * see bug 741402. */
void
VteTerminalPrivate::seq_window_manipulation(vte::parser::Params const& params)
{
        auto n_params = params.size();
        if (n_params < 1)
                return;

        int  param;
        if (!params.number_at_unchecked(0, param))
                return;

        int arg1 = -1;
        int arg2 = -1;
        if (n_params >= 2)
                params.number_at_unchecked(1, arg1);
        if (n_params >= 3)
                params.number_at_unchecked(2, arg2);

	GdkScreen *gscreen;
	char buf[128];
	int width, height;

        switch (param) {
        case 1:
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Deiconifying window.\n");
                emit_deiconify_window();
                break;
        case 2:
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Iconifying window.\n");
                emit_iconify_window();
                break;
        case 3:
                if ((arg1 != -1) && (arg2 != -1)) {
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Moving window to "
                                         "%d,%d.\n", arg1, arg2);
                        emit_move_window(arg1, arg2);
                }
                break;
        case 4:
                if ((arg1 != -1) && (arg2 != -1)) {
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Resizing window "
                                         "(to %dx%d pixels, grid size %ldx%ld).\n",
                                         arg2, arg1,
                                         arg2 / m_cell_width,
                                         arg1 / m_cell_height);
                        emit_resize_window(arg2 / m_cell_width,
                                           arg1 / m_cell_height);
                }
                break;
        case 5:
                _vte_debug_print(VTE_DEBUG_PARSER, "Raising window.\n");
                emit_raise_window();
                break;
        case 6:
                _vte_debug_print(VTE_DEBUG_PARSER, "Lowering window.\n");
                emit_lower_window();
                break;
        case 7:
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Refreshing window.\n");
                invalidate_all();
                emit_refresh_window();
                break;
        case 8:
                if ((arg1 != -1) && (arg2 != -1)) {
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Resizing window "
                                         "(to %d columns, %d rows).\n",
                                         arg2, arg1);
                        emit_resize_window(arg2, arg1);
                }
                break;
        case 9:
                switch (arg1) {
                case 0:
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Restoring window.\n");
                        emit_restore_window();
                        break;
                case 1:
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Maximizing window.\n");
                        emit_maximize_window();
                        break;
                default:
                        break;
                }
                break;
        case 11:
                /* If we're unmapped, then we're iconified. */
                g_snprintf(buf, sizeof(buf),
                           _VTE_CAP_CSI "%dt",
                           1 + !gtk_widget_get_mapped(m_widget));
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting window state %s.\n",
                                 gtk_widget_get_mapped(m_widget) ?
                                 "non-iconified" : "iconified");
                feed_child(buf, -1);
                break;
        case 13:
                /* Send window location, in pixels. */
                gdk_window_get_origin(gtk_widget_get_window(m_widget),
                                      &width, &height);
                g_snprintf(buf, sizeof(buf),
                           _VTE_CAP_CSI "3;%d;%dt",
                           width + m_padding.left,
                           height + m_padding.top);
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting window location"
                                 "(%d++,%d++).\n",
                                 width, height);
                feed_child(buf, -1);
                break;
        case 14:
                /* Send window size, in pixels. */
                g_snprintf(buf, sizeof(buf),
                           _VTE_CAP_CSI "4;%d;%dt",
                           (int)(m_row_count * m_cell_height),
                           (int)(m_column_count * m_cell_width));
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting window size "
                                 "(%dx%d)\n",
                                 (int)(m_row_count * m_cell_height),
                                 (int)(m_column_count * m_cell_width));

                feed_child(buf, -1);
                break;
        case 18:
                /* Send widget size, in cells. */
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting widget size.\n");
                g_snprintf(buf, sizeof(buf),
                           _VTE_CAP_CSI "8;%ld;%ldt",
                           m_row_count,
                           m_column_count);
                feed_child(buf, -1);
                break;
        case 19:
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting screen size.\n");
                gscreen = gtk_widget_get_screen(m_widget);
                height = gdk_screen_get_height(gscreen);
                width = gdk_screen_get_width(gscreen);
                g_snprintf(buf, sizeof(buf),
                           _VTE_CAP_CSI "9;%ld;%ldt",
                           height / m_cell_height,
                           width / m_cell_width);
                feed_child(buf, -1);
                break;
        case 20:
                /* Report a static icon title, since the real
                   icon title should NEVER be reported, as it
                   creates a security vulnerability.  See
                   http://marc.info/?l=bugtraq&m=104612710031920&w=2
                   and CVE-2003-0070. */
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting fake icon title.\n");
                /* never use m_icon_title here! */
                g_snprintf (buf, sizeof (buf),
                            _VTE_CAP_OSC "LTerminal" _VTE_CAP_ST);
                feed_child(buf, -1);
                break;
        case 21:
                /* Report a static window title, since the real
                   window title should NEVER be reported, as it
                   creates a security vulnerability.  See
                   http://marc.info/?l=bugtraq&m=104612710031920&w=2
                   and CVE-2003-0070. */
                _vte_debug_print(VTE_DEBUG_PARSER,
                                 "Reporting fake window title.\n");
                /* never use m_window_title here! */
                g_snprintf (buf, sizeof (buf),
                            _VTE_CAP_OSC "lTerminal" _VTE_CAP_ST);
                feed_child(buf, -1);
                break;
        default:
                if (param >= 24) {
                        _vte_debug_print(VTE_DEBUG_PARSER,
                                         "Resizing to %d rows.\n",
                                         param);
                        /* Resize to the specified number of
                         * rows. */
                        emit_resize_window(m_column_count,
                                           param);
                }
                break;
        }
}

/* Internal helper for setting/querying special colors */
void
VteTerminalPrivate::change_special_color(vte::parser::Params const& params,
                                         int index,
                                         int index_fallback,
                                         int osc,
                                         const char *terminator)
{
        char* name;
        if (!params.string_at(0, name))
                return;

        vte::color::rgb color;

		if (color.parse(name))
			set_color(index, VTE_COLOR_SOURCE_ESCAPE, color);
		else if (strcmp (name, "?") == 0) {
			gchar buf[128];
			auto c = get_color(index);
			if (c == NULL && index_fallback != -1)
				c = get_color(index_fallback);
			g_assert(c != NULL);
			g_snprintf (buf, sizeof (buf),
				    _VTE_CAP_OSC "%d;rgb:%04x/%04x/%04x%s",
				    osc, c->red, c->green, c->blue, terminator);
			feed_child(buf, -1);
		}
}

/* Change the default foreground cursor, BEL terminated */
void
VteTerminalPrivate::seq_change_foreground_color_bel(vte::parser::Params const& params)
{
        change_special_color(params, VTE_DEFAULT_FG, -1, 10, BEL_C0);
}

/* Change the default foreground cursor, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_foreground_color_st(vte::parser::Params const& params)
{
        change_special_color(params, VTE_DEFAULT_FG, -1, 10, ST_C0);
}

/* Reset the default foreground color */
void
VteTerminalPrivate::seq_reset_foreground_color(vte::parser::Params const& params)
{
        reset_color(VTE_DEFAULT_FG, VTE_COLOR_SOURCE_ESCAPE);
}

/* Change the default background cursor, BEL terminated */
void
VteTerminalPrivate::seq_change_background_color_bel(vte::parser::Params const& params)
{
        change_special_color(params, VTE_DEFAULT_BG, -1, 11, BEL_C0);
}

/* Change the default background cursor, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_background_color_st(vte::parser::Params const& params)
{
        change_special_color(params, VTE_DEFAULT_BG, -1, 11, ST_C0);
}

/* Reset the default background color */
void
VteTerminalPrivate::seq_reset_background_color(vte::parser::Params const& params)
{
        reset_color(VTE_DEFAULT_BG, VTE_COLOR_SOURCE_ESCAPE);
}

/* Change the color of the cursor background, BEL terminated */
void
VteTerminalPrivate::seq_change_cursor_background_color_bel(vte::parser::Params const& params)
{
        change_special_color(params, VTE_CURSOR_BG, VTE_DEFAULT_FG, 12, BEL_C0);
}

/* Change the color of the cursor background, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_cursor_background_color_st(vte::parser::Params const& params)
{
        change_special_color(params, VTE_CURSOR_BG, VTE_DEFAULT_FG, 12, ST_C0);
}

/* Reset the color of the cursor */
void
VteTerminalPrivate::seq_reset_cursor_background_color(vte::parser::Params const& params)
{
        reset_color(VTE_CURSOR_BG, VTE_COLOR_SOURCE_ESCAPE);
}

/* Change the highlight background color, BEL terminated */
void
VteTerminalPrivate::seq_change_highlight_background_color_bel(vte::parser::Params const& params)
{
        change_special_color(params, VTE_HIGHLIGHT_BG, VTE_DEFAULT_FG, 17, BEL_C0);
}

/* Change the highlight background color, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_highlight_background_color_st(vte::parser::Params const& params)
{
        change_special_color(params, VTE_HIGHLIGHT_BG, VTE_DEFAULT_FG, 17, ST_C0);
}

/* Reset the highlight background color */
void
VteTerminalPrivate::seq_reset_highlight_background_color(vte::parser::Params const& params)
{
        reset_color(VTE_HIGHLIGHT_BG, VTE_COLOR_SOURCE_ESCAPE);
}

/* Change the highlight foreground color, BEL terminated */
void
VteTerminalPrivate::seq_change_highlight_foreground_color_bel(vte::parser::Params const& params)
{
        change_special_color(params, VTE_HIGHLIGHT_FG, VTE_DEFAULT_BG, 19, BEL_C0);
}

/* Change the highlight foreground color, ST_C0 terminated */
void
VteTerminalPrivate::seq_change_highlight_foreground_color_st(vte::parser::Params const& params)
{
        change_special_color(params, VTE_HIGHLIGHT_FG, VTE_DEFAULT_BG, 19, ST_C0);
}

/* Reset the highlight foreground color */
void
VteTerminalPrivate::seq_reset_highlight_foreground_color(vte::parser::Params const& params)
{
        reset_color(VTE_HIGHLIGHT_FG, VTE_COLOR_SOURCE_ESCAPE);
}

/* URXVT generic OSC 777 */

void
VteTerminalPrivate::seq_urxvt_777(vte::parser::Params const& params)
{
        /* Accept but ignore this for compatibility with downstream-patched vte (bug #711059)*/
}

/* iterm2 OSC 133 & 1337 */

void
VteTerminalPrivate::seq_iterm2_133(vte::parser::Params const& params)
{
        /* Accept but ignore this for compatibility when sshing to an osx host
         * where the iterm2 integration is loaded even when not actually using
         * iterm2.
         */
}

void
VteTerminalPrivate::seq_iterm2_1337(vte::parser::Params const& params)
{
        /* Accept but ignore this for compatibility when sshing to an osx host
         * where the iterm2 integration is loaded even when not actually using
         * iterm2.
         */
}

#define UNIMPLEMENTED_SEQUENCE_HANDLER(name) \
        void \
        VteTerminalPrivate::seq_ ## name (vte::parser::Params const& params) \
        { \
                static bool warned = false; \
                if (!warned) { \
                        _vte_debug_print(VTE_DEBUG_PARSER, \
                                         "Unimplemented handler for control sequence `%s'.\n", \
                                         "name"); \
                        warned = true; \
                } \
        }

UNIMPLEMENTED_SEQUENCE_HANDLER(ansi_conformance_level_1)
UNIMPLEMENTED_SEQUENCE_HANDLER(ansi_conformance_level_2)
UNIMPLEMENTED_SEQUENCE_HANDLER(ansi_conformance_level_3)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_font_name)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_font_number)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_logfile)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_mouse_cursor_background_color_bel)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_mouse_cursor_background_color_st)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_mouse_cursor_foreground_color_bel)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_mouse_cursor_foreground_color_st)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_background_color_bel)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_background_color_st)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_cursor_color_bel)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_cursor_color_st)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_foreground_color_bel)
UNIMPLEMENTED_SEQUENCE_HANDLER(change_tek_foreground_color_st)
UNIMPLEMENTED_SEQUENCE_HANDLER(cursor_lower_left)
UNIMPLEMENTED_SEQUENCE_HANDLER(dec_media_copy)
UNIMPLEMENTED_SEQUENCE_HANDLER(default_character_set)
UNIMPLEMENTED_SEQUENCE_HANDLER(device_control_string)
UNIMPLEMENTED_SEQUENCE_HANDLER(double_height_bottom_half)
UNIMPLEMENTED_SEQUENCE_HANDLER(double_height_top_half)
UNIMPLEMENTED_SEQUENCE_HANDLER(double_width)
UNIMPLEMENTED_SEQUENCE_HANDLER(eight_bit_controls)
UNIMPLEMENTED_SEQUENCE_HANDLER(enable_filter_rectangle)
UNIMPLEMENTED_SEQUENCE_HANDLER(enable_locator_reporting)
UNIMPLEMENTED_SEQUENCE_HANDLER(end_of_guarded_area)
UNIMPLEMENTED_SEQUENCE_HANDLER(initiate_hilite_mouse_tracking)
UNIMPLEMENTED_SEQUENCE_HANDLER(invoke_g1_character_set_as_gr)
UNIMPLEMENTED_SEQUENCE_HANDLER(invoke_g2_character_set)
UNIMPLEMENTED_SEQUENCE_HANDLER(invoke_g2_character_set_as_gr)
UNIMPLEMENTED_SEQUENCE_HANDLER(invoke_g3_character_set)
UNIMPLEMENTED_SEQUENCE_HANDLER(invoke_g3_character_set_as_gr)
UNIMPLEMENTED_SEQUENCE_HANDLER(linux_console_cursor_attributes)
UNIMPLEMENTED_SEQUENCE_HANDLER(media_copy)
UNIMPLEMENTED_SEQUENCE_HANDLER(memory_lock)
UNIMPLEMENTED_SEQUENCE_HANDLER(memory_unlock)
UNIMPLEMENTED_SEQUENCE_HANDLER(request_locator_position)
UNIMPLEMENTED_SEQUENCE_HANDLER(reset_mouse_cursor_foreground_color)
UNIMPLEMENTED_SEQUENCE_HANDLER(reset_mouse_cursor_background_color)
UNIMPLEMENTED_SEQUENCE_HANDLER(reset_tek_background_color)
UNIMPLEMENTED_SEQUENCE_HANDLER(reset_tek_cursor_color)
UNIMPLEMENTED_SEQUENCE_HANDLER(reset_tek_foreground_color)
UNIMPLEMENTED_SEQUENCE_HANDLER(select_character_protection)
UNIMPLEMENTED_SEQUENCE_HANDLER(select_locator_events)
UNIMPLEMENTED_SEQUENCE_HANDLER(selective_erase_in_display)
UNIMPLEMENTED_SEQUENCE_HANDLER(selective_erase_in_line)
UNIMPLEMENTED_SEQUENCE_HANDLER(send_tertiary_device_attributes)
UNIMPLEMENTED_SEQUENCE_HANDLER(set_conformance_level)
UNIMPLEMENTED_SEQUENCE_HANDLER(set_text_property_21)
UNIMPLEMENTED_SEQUENCE_HANDLER(set_text_property_2L)
UNIMPLEMENTED_SEQUENCE_HANDLER(set_xproperty)
UNIMPLEMENTED_SEQUENCE_HANDLER(seven_bit_controls)
UNIMPLEMENTED_SEQUENCE_HANDLER(single_shift_g2)
UNIMPLEMENTED_SEQUENCE_HANDLER(single_shift_g3)
UNIMPLEMENTED_SEQUENCE_HANDLER(single_width)
UNIMPLEMENTED_SEQUENCE_HANDLER(start_of_guarded_area)
UNIMPLEMENTED_SEQUENCE_HANDLER(start_or_end_of_string)
UNIMPLEMENTED_SEQUENCE_HANDLER(utf_8_character_set)

#undef UNIMPLEMENTED_UNIMPLEMENTED_SEQUENCE_HANDLER

/// FIXME


/*
 * Copyright (C) 2015 David Herrmann <dh.herrmann@gmail.com>
 *
 * vte is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 */

enum {
        /* 7bit mode (default: on) */
        VTE_FLAG_7BIT_MODE                = (1U << 0),
        /* hide cursor caret (default: off) */
        VTE_FLAG_HIDE_CURSOR                = (1U << 1),
        /* do not send TPARM unrequested (default: off) */
        VTE_FLAG_INHIBIT_TPARM        = (1U << 2),
        /* perform carriage-return on line-feeds (default: off) */
        VTE_FLAG_NEWLINE_MODE        = (1U << 3),
        /* wrap-around is pending */
        VTE_FLAG_PENDING_WRAP        = (1U << 4),
        /* application-keypad mode (default: off) */
        VTE_FLAG_KEYPAD_MODE                = (1U << 5),
        /* enable application cursor-keys (default: off) */
        VTE_FLAG_CURSOR_KEYS                = (1U << 6),
};

enum {
        VTE_CONFORMANCE_LEVEL_VT52,
        VTE_CONFORMANCE_LEVEL_VT100,
        VTE_CONFORMANCE_LEVEL_VT400,
        VTE_CONFORMANCE_LEVEL_N,
};
/*
 * Command Handlers
 * This is the unofficial documentation of all the VTE_CMD_* definitions.
 * Each handled command has a separate function with an extensive comment on
 * the semantics of the command.
 * Note that many semantics are unknown and need to be verified. This is mostly
 * about error-handling, though. Applications rarely rely on those features.
 */

void
VteTerminalPrivate::NONE(vte::parser::Sequence const& seq)
{
}

void
VteTerminalPrivate::GRAPHIC(vte::parser::Sequence const& seq)
{
#if 0
        struct vte_char ch = VTE_CHAR_NULL;

        if (screen->state.cursor_x + 1 == screen->page->width
            && screen->flags & VTE_FLAG_PENDING_WRAP
            && screen->state.auto_wrap) {
                screen_cursor_down(screen, 1, true);
                screen_cursor_set(screen, 0, screen->state.cursor_y);
        }

        screen_cursor_clear_wrap(screen);

        ch = vte_char_merge(ch, screen_map(screen, seq->terminator));
        vte_page_write(screen->page,
                          screen->state.cursor_x,
                          screen->state.cursor_y,
                          ch,
                          1,
                          &screen->state.attr,
                          screen->age,
                          false);

        if (screen->state.cursor_x + 1 == screen->page->width)
                screen->flags |= VTE_FLAG_PENDING_WRAP;
        else
                screen_cursor_right(screen, 1);

        return 0;
#endif

        insert_char(seq.terminator(), false, false);
}

void
VteTerminalPrivate::BEL(vte::parser::Sequence const& seq)
{
        /*
         * BEL - sound bell tone
         * This command should trigger an acoustic bell. Usually, this is
         * forwarded directly to the pcspkr. However, bells have become quite
         * uncommon and annoying, so we're not implementing them here. Instead,
         * it's one of the commands we forward to the caller.
         */

#if 0
        screen_forward(screen, VTE_CMD_BEL, seq);
#endif

        seq_bell(seq);
}

void
VteTerminalPrivate::BS(vte::parser::Sequence const& seq)
{
        /*
         * BS - backspace
         * Move cursor one cell to the left. If already at the left margin,
         * nothing happens.
         */

#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, 1);
#endif

        seq_backspace(seq);
}

void
VteTerminalPrivate::CBT(vte::parser::Sequence const& seq)
{
        /*
         * CBT - cursor-backward-tabulation
         * Move the cursor @args[0] tabs backwards (to the left). The
         * current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond position 0 and
         * it will stop there.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0

        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_left_tab(screen, num);
#endif

        seq_cursor_back_tab(seq);
}

void
VteTerminalPrivate::CHA(vte::parser::Sequence const& seq)
{
        /*
         * CHA - cursor-horizontal-absolute
         * Move the cursor to position @args[0] in the current line. The
         * cursor cannot be moved beyond the rightmost cell and will stop
         * there.
         *
         * Defaults:
         *   args[0]: 1
         */

#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, pos - 1, screen->state.cursor_y);
#endif

        seq_cursor_character_absolute(seq);
}

void
VteTerminalPrivate::CHT(vte::parser::Sequence const& seq)
{
        /*
         * CHT - cursor-horizontal-forward-tabulation
         * Move the cursor @args[0] tabs forward (to the right). The
         * current cursor cell, in case it's a tab, is not counted.
         * Furthermore, the cursor cannot be moved beyond the rightmost cell
         * and will stop there.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right_tab(screen, num);
#endif

        seq_cursor_forward_tabulation(seq);
}

void
VteTerminalPrivate::CNL(vte::parser::Sequence const& seq)
{
        /*
         * CNL - cursor-next-line
         * Move the cursor @args[0] lines down.
         *
         * TODO: Does this stop at the bottom or cause a scroll-up?
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        seq_cursor_next_line(seq);
}

void
VteTerminalPrivate::CPL(vte::parser::Sequence const& seq)
{
        /*
         * CPL - cursor-preceding-line
         * Move the cursor @args[0] lines up.
         *
         * TODO: Does this stop at the top or cause a scroll-up?
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        seq_cursor_preceding_line(seq);
}

void
VteTerminalPrivate::CR(vte::parser::Sequence const& seq)
{
        /*
         * CR - carriage-return
         * Move the cursor to the left margin on the current line.
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        seq_carriage_return(seq);
}

void
VteTerminalPrivate::CUB(vte::parser::Sequence const& seq)
{
        /*
         * CUB - cursor-backward
         * Move the cursor @args[0] positions to the left. The cursor stops
         * at the left-most position.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_left(screen, num);
#endif

        seq_cursor_backward(seq);
}

void
VteTerminalPrivate::CUD(vte::parser::Sequence const& seq)
{
        /*
         * CUD - cursor-down
         * Move the cursor @args[0] positions down. The cursor stops at the
         * bottom margin. If it was already moved further, it stops at the
         * bottom line.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif

        seq_cursor_down(seq);
}

void
VteTerminalPrivate::CUF(vte::parser::Sequence const& seq)
{
        /*
         * CUF -cursor-forward
         * Move the cursor @args[0] positions to the right. The cursor stops
         * at the right-most position.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif

        seq_cursor_forward(seq);
}

void
VteTerminalPrivate::CUP(vte::parser::Sequence const& seq)
{
        /*
         * CUP - cursor-position
         * Moves the cursor to position @args[1] x @args[0]. If either is 0, it
         * is treated as 1. The positions are subject to the origin-mode and
         * clamped to the addressable with/height.
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         */
#if 0
        unsigned int x = 1, y = 1;

        if (seq->args[0] > 0)
                y = seq->args[0];
        if (seq->args[1] > 0)
                x = seq->args[1];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, x - 1, y - 1);
#endif

        seq_cursor_position(seq);
}

void
VteTerminalPrivate::CUU(vte::parser::Sequence const& seq)
{
        /*
         * CUU - cursor-up
         * Move the cursor @args[0] positions up. The cursor stops at the
         * top margin. If it was already moved further, it stops at the
         * top line.
         *
         * Defaults:
         *   args[0]: 1
         *
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_up(screen, num, false);
#endif

        seq_cursor_up(seq);
}

void
VteTerminalPrivate::DA1(vte::parser::Sequence const& seq)
{
        /*
         * DA1 - primary-device-attributes
         * The primary DA asks for basic terminal features. We simply return
         * a hard-coded list of features we implement.
         * Note that the primary DA asks for supported features, not currently
         * enabled features.
         *
         * The terminal's answer is:
         *   ^[ ? 64 ; ARGS c
         * The first argument, 64, is fixed and denotes a VT420, the last
         * DEC-term that extended this number.
         * All following arguments denote supported features. Note
         * that at most 15 features can be sent (max CSI args). It is safe to
         * send more, but clients might not be able to parse them. This is a
         * client's problem and we shouldn't care. There is no other way to
         * send those feature lists, so we have to extend them beyond 15 in
         * those cases.
         *
         * Known modes:
         *    1: 132 column mode
         *       The 132 column mode is supported by the terminal.
         *    2: printer port
         *       A priner-port is supported and can be addressed via
         *       control-codes.
         *    3: ReGIS graphics
         *       Support for ReGIS graphics is available. The ReGIS routines
         *       provide the "remote graphics instruction set" and allow basic
         *       vector-rendering.
         *    4: sixel
         *       Support of Sixel graphics is available. This provides access
         *       to the sixel bitmap routines.
         *    6: selective erase
         *       The terminal supports DECSCA and related selective-erase
         *       functions. This allows to protect specific cells from being
         *       erased, if specified.
         *    7: soft character set (DRCS)
         *       TODO: ?
         *    8: user-defined keys (UDKs)
         *       TODO: ?
         *    9: national-replacement character sets (NRCS)
         *       National-replacement character-sets are available.
         *   12: Yugoslavian (SCS)
         *       TODO: ?
         *   15: technical character set
         *       The DEC technical-character-set is available.
         *   18: windowing capability
         *       TODO: ?
         *   21: horizontal scrolling
         *       TODO: ?
         *   22: ANSII color
         *       TODO: ?
         *   23: Greek
         *       TODO: ?
         *   24: Turkish
         *       TODO: ?
         *   29: ANSI text locator
         *       TODO: ?
         *   42: ISO Latin-2 character set
         *       TODO: ?
         *   44: PCTerm
         *       TODO: ?
         *   45: soft keymap
         *       TODO: ?
         *   46: ASCII emulation
         *       TODO: ?
         */
#if 0
        SEQ_WRITE(screen, C0_CSI, C1_CSI, "?64;1;6;9;15c");
#endif

        seq_send_primary_device_attributes(seq);
}

void
VteTerminalPrivate::DA2(vte::parser::Sequence const& seq)
{
        /*
         * DA2 - secondary-device-attributes
         * The secondary DA asks for the terminal-ID, firmware versions and
         * other non-primary attributes. All these values are
         * informational-only and should not be used by the host to detect
         * terminal features.
         *
         * The terminal's response is:
         *   ^[ > 61 ; FIRMWARE ; KEYBOARD c
         * whereas 65 is fixed for VT525 terminals, the last terminal-line that
         * increased this number. FIRMWARE is the firmware
         * version encoded as major/minor (20 == 2.0) and KEYBOARD is 0 for STD
         * keyboard and 1 for PC keyboards.
         *
         * We replace the firmware-version with the systemd-version so clients
         * can decode it again.
         */
#if 0
        return SEQ_WRITE(screen, C0_CSI, C1_CSI,
                         ">65;" __stringify(LINUX_VERSION_CODE) ";1c");
#endif

        seq_send_secondary_device_attributes(seq);
}

void
VteTerminalPrivate::DA3(vte::parser::Sequence const& seq)
{
        /*
         * DA3 - tertiary-device-attributes
         * The tertiary DA is used to query the terminal-ID.
         *
         * The terminal's response is:
         *   ^P ! | XX AA BB CC ^\
         * whereas all four parameters are hexadecimal-encoded pairs. XX
         * denotes the manufacturing site, AA BB CC is the terminal's ID.
         */

        /* we do not support tertiary DAs */
#if 0
#endif
}

void
VteTerminalPrivate::DC1(vte::parser::Sequence const& seq)
{
        /*
         * DC1 - device-control-1 or XON
         * This clears any previous XOFF and resumes terminal-transmission.
         */

        /* we do not support XON */
}

void
VteTerminalPrivate::DC3(vte::parser::Sequence const& seq)
{
        /*
         * DC3 - device-control-3 or XOFF
         * Stops terminal transmission. No further characters are sent until
         * an XON is received.
         */

        /* we do not support XOFF */
}

void
VteTerminalPrivate::DCH(vte::parser::Sequence const& seq)
{
        /*
         * DCH - delete-character
         * This deletes @argv[0] characters at the current cursor position.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_delete_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        seq_delete_characters(seq);
}

void
VteTerminalPrivate::DECALN(vte::parser::Sequence const& seq)
{
        /*
         * DECALN - screen-alignment-pattern
         *
         * Probably not worth implementing.
         */

        seq_screen_alignment_test(seq);
}

void
VteTerminalPrivate::DECANM(vte::parser::Sequence const& seq)
{
        /*
         * DECANM - ansi-mode
         * Set the terminal into VT52 compatibility mode. Control sequences
         * overlap with regular sequences so we have to detect them early before
         * dispatching them.
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECBI(vte::parser::Sequence const& seq)
{
        /*
         * DECBI - back-index
         * This control function moves the cursor backward one column. If the
         * cursor is at the left margin, then all screen data within the margin
         * moves one column to the right. The column that shifted past the right
         * margin is lost.
         * DECBI adds a new column at the left margin with no visual attributes.
         * DECBI does not affect the margins. If the cursor is beyond the
         * left-margin at the left border, then the terminal ignores DECBI.
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECCARA(vte::parser::Sequence const& seq)
{
        /*
         * DECCARA - change-attributes-in-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECCRA - copy-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECDC(vte::parser::Sequence const& seq)
{
        /*
         * DECDC - delete-column
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECDHL_BH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_BH - double-width-double-height-line: bottom half
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECDHL_TH(vte::parser::Sequence const& seq)
{
        /*
         * DECDHL_TH - double-width-double-height-line: top half
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECDWL(vte::parser::Sequence const& seq)
{
        /*
         * DECDWL - double-width-single-height-line
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECEFR(vte::parser::Sequence const& seq)
{
        /*
         * DECEFR - enable-filter-rectangle
         * Defines the coordinates of a filter rectangle (top, left, bottom,
         * right as @args[0] to @args[3]) and activates it.
         * Anytime the locator is detected outside of the filter rectangle, an
         * outside rectangle event is generated and the rectangle is disabled.
         * Filter rectangles are always treated as "one-shot" events. Any
         * parameters that are omitted default to the current locator position.
         * If all parameters are omitted, any locator motion will be reported.
         * DECELR always cancels any prevous rectangle definition.
         *
         * The locator is usually associated with the mouse-cursor, but based
         * on cells instead of pixels. See DECELR how to initialize and enable
         * it. DECELR can also enable pixel-mode instead of cell-mode.
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECELF(vte::parser::Sequence const& seq)
{
        /*
         * DECELF - enable-local-functions
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECELR(vte::parser::Sequence const& seq)
{
        /*
         * DECELR - enable-locator-reporting
         * This changes the locator-reporting mode. @args[0] specifies the mode
         * to set, 0 disables locator-reporting, 1 enables it continuously, 2
         * enables it for a single report. @args[1] specifies the
         * precision-mode. 0 and 2 set the reporting to cell-precision, 1 sets
         * pixel-precision.
         *
         * Defaults:
         *   args[0]: 0
         *   args[1]: 0
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECERA(vte::parser::Sequence const& seq)
{
        /*
         * DECERA - erase-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECFI(vte::parser::Sequence const& seq)
{
        /*
         * DECFI - forward-index
         * This control function moves the cursor forward one column. If the
         * cursor is at the right margin, then all screen data within the
         * margins moves one column to the left. The column shifted past the
         * left margin is lost.
         * DECFI adds a new column at the right margin, with no visual
         * attributes. DECFI does not affect margins. If the cursor is beyond
         * the right margin at the border of the page when the terminal
         * receives DECFI, then the terminal ignores DECFI.
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECFRA(vte::parser::Sequence const& seq)
{
        /*
         * DECFRA - fill-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECIC(vte::parser::Sequence const& seq)
{
        /*
         * DECIC - insert-column
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECID(vte::parser::Sequence const& seq)
{
        /*
         * DECID - return-terminal-id
         * This is an obsolete form of VTE_CMD_DA1.
         */

        DA1(seq);
}

void
VteTerminalPrivate::DECINVM(vte::parser::Sequence const& seq)
{
        /*
         * DECINVM - invoke-macro
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECKBD(vte::parser::Sequence const& seq)
{
        /*
         * DECKBD - keyboard-language-selection
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECKPAM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPAM - keypad-application-mode
         * Enables the keypad-application mode. If enabled, the keypad sends
         * special characters instead of the printed characters. This way,
         * applications can detect whether a numeric key was pressed on the
         * top-row or on the keypad.
         * Default is keypad-numeric-mode.
         */
#if 0
        screen->flags |= VTE_FLAG_KEYPAD_MODE;
#endif

        set_keypad_mode(VTE_KEYMODE_APPLICATION);
}

void
VteTerminalPrivate::DECKPNM(vte::parser::Sequence const& seq)
{
        /*
         * DECKPNM - keypad-numeric-mode
         * This disables the keypad-application-mode (DECKPAM) and returns to
         * the keypad-numeric-mode. Keypresses on the keypad generate the same
         * sequences as corresponding keypresses on the main keyboard.
         * Default is keypad-numeric-mode.
         */
#if 0
        screen->flags &= ~VTE_FLAG_KEYPAD_MODE;
#endif

	set_keypad_mode(VTE_KEYMODE_NORMAL);
}

void
VteTerminalPrivate::DECLFKC(vte::parser::Sequence const& seq)
{
        /*
         * DECLFKC - local-function-key-control
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECLL(vte::parser::Sequence const& seq)
{
        /*
         * DECLL - load-leds
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECLTOD(vte::parser::Sequence const& seq)
{
        /*
         * DECLTOD - load-time-of-day
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECPCTERM(vte::parser::Sequence const& seq)
{
        /*
         * DECPCTERM - pcterm-mode
         * This enters/exits the PCTerm mode. Default mode is VT-mode. It can
         * also select parameters for scancode/keycode mappings in SCO mode.
         *
         * Definitely not worth implementing. Lets kill PCTerm/SCO modes!
         */
}

void
VteTerminalPrivate::DECPKA(vte::parser::Sequence const& seq)
{
        /*
         * DECPKA - program-key-action
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECPKFMR(vte::parser::Sequence const& seq)
{
        /*
         * DECPKFMR - program-key-free-memory-report
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRARA(vte::parser::Sequence const& seq)
{
        /*
         * DECRARA - reverse-attributes-in-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRC(vte::parser::Sequence const& seq)
{
        /*
         * DECRC - restore-cursor
         * Restores the terminal to the state saved by the save cursor (DECSC)
         * function. If there was not a previous DECSC, then this does:
         *   * Home the cursor
         *   * Resets DECOM
         *   * Resets the SGR attributes
         *   * Designates ASCII (IR #6) to GL, and DEC Supplemental Graphics to GR
         *
         * Note that the status line has its own DECSC buffer.
         */
#if 0
        screen_restore_state(screen, &screen->saved);
#endif

        seq_restore_cursor(seq);
}

void
VteTerminalPrivate::DECREQTPARM(vte::parser::Sequence const& seq)
{
        /*
         * DECREQTPARM - request-terminal-parameters
         * The sequence DECREPTPARM is sent by the terminal controller to notify
         * the host of the status of selected terminal parameters. The status
         * sequence may be sent when requested by the host or at the terminal's
         * discretion. DECREPTPARM is sent upon receipt of a DECREQTPARM.
         *
         * If @args[0] is 0, this marks a request and the terminal is allowed
         * to send DECREPTPARM messages without request. If it is 1, the same
         * applies but the terminal should no longer send DECREPTPARM
         * unrequested.
         * 2 and 3 mark a report, but 3 is only used if the terminal answers as
         * an explicit request with @args[0] == 1.
         *
         * The other arguments are ignored in requests, but have the following
         * meaning in responses:
         *   args[1]: 1=no-parity-set 4=parity-set-and-odd 5=parity-set-and-even
         *   args[2]: 1=8bits-per-char 2=7bits-per-char
         *   args[3]: transmission-speed
         *   args[4]: receive-speed
         *   args[5]: 1=bit-rate-multiplier-is-16
         *   args[6]: This value communicates the four switch values in block 5
         *            of SETUP B, which are only visible to the user when an STP
         *            option is installed. These bits may be assigned for an STP
         *            device. The four bits are a decimal-encoded binary number.
         *            Value between 0-15.
         *
         * The transmission/receive speeds have mappings for number => bits/s
         * which are quite weird. Examples are: 96->3600, 112->9600, 120->19200
         *
         * Defaults:
         *   args[0]: 0
         */
#if 0
        if (seq->n_args < 1 || seq->args[0] == 0) {
                screen->flags &= ~VTE_FLAG_INHIBIT_TPARM;
                return SEQ_WRITE(screen, C0_CSI, C1_CSI, "2;1;1;120;120;1;0x");
        } else if (seq->args[0] == 1) {
                screen->flags |= VTE_FLAG_INHIBIT_TPARM;
                return SEQ_WRITE(screen, C0_CSI, C1_CSI, "3;1;1;120;120;1;0x");
        } else {
                return 0;
        }
#endif

        seq_request_terminal_parameters(seq);
}

void
VteTerminalPrivate::DECRPKT(vte::parser::Sequence const& seq)
{
        /*
         * DECRPKT - report-key-type
         * Response to DECRQKT, we can safely ignore it as we're the one sending
         * it to the host.
         */
}

void
VteTerminalPrivate::DECRQCRA(vte::parser::Sequence const& seq)
{
        /*
         * DECRQCRA - request-checksum-of-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQDE(vte::parser::Sequence const& seq)
{
        /*
         * DECRQDE - request-display-extent
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQKT(vte::parser::Sequence const& seq)
{
        /*
         * DECRQKT - request-key-type
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQLP(vte::parser::Sequence const& seq)
{
        /*
         * DECRQLP - request-locator-position
         * See DECELR for locator-information.
         *
         * TODO: document and implement
         */
}

void
VteTerminalPrivate::DECRQM_ANSI(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_ANSI - request-mode-ansi
         * The host sends this control function to find out if a particular mode
         * is set or reset. The terminal responds with a report mode function.
         * @args[0] contains the mode to query.
         *
         * Response is DECRPM with the first argument set to the mode that was
         * queried, second argument is 0 if mode is invalid, 1 if mode is set,
         * 2 if mode is not set (reset), 3 if mode is permanently set and 4 if
         * mode is permanently not set (reset):
         *   ANSI: ^[ MODE ; VALUE $ y
         *   DEC:  ^[ ? MODE ; VALUE $ y
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECRQM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DECRQM_DEC - request-mode-dec
         * Same as DECRQM_ANSI but for DEC modes.
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECRQPKFM(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPKFM - request-program-key-free-memory
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQPSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQPSR - request-presentation-state-report
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQTSR(vte::parser::Sequence const& seq)
{
        /*
         * DECRQTSR - request-terminal-state-report
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECRQUPSS(vte::parser::Sequence const& seq)
{
        /*
         * DECRQUPSS - request-user-preferred-supplemental-set
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSACE(vte::parser::Sequence const& seq)
{
        /*
         * DECSACE - select-attribute-change-extent
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSASD(vte::parser::Sequence const& seq)
{
        /*
         * DECSASD - select-active-status-display
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSC(vte::parser::Sequence const& seq)
{
        /*
         * DECSC - save-cursor
         * Save cursor and terminal state so it can be restored later on.
         * This stores:
         *   * Cursor position
         *   * SGR attributes
         *   * Charset designations for GL and GR
         *   * Wrap flag
         *   * DECOM state
         *   * Selective erase attribute
         *   * Any SS2 or SS3 sent
         *
         */
#if 0
        screen_save_state(screen, &screen->saved);
#endif

        seq_save_cursor(seq);
}

void
VteTerminalPrivate::DECSCA(vte::parser::Sequence const& seq)
{
        /*
         * DECSCA - select-character-protection-attribute
         *
         * Defaults:
         *   args[0]: 0
         *
         * References: VT525
         */
#if 0
        unsigned int mode = 0;

        if (seq->args[0] > 0)
                mode = seq->args[0];

        switch (mode) {
        case 0:
        case 2:
                screen->state.attr.protect = 0;
                break;
        case 1:
                screen->state.attr.protect = 1;
                break;
        }
#endif
}

void
VteTerminalPrivate::DECSCL(vte::parser::Sequence const& seq)
{
        /*
         * DECSCL - select-conformance-level
         * Select the terminal's operating level. The factory default is
         * level 4 (VT Level 4 mode, 7-bit controls).
         * When you change the conformance level, the terminal performs a hard
         * reset (RIS).
         *
         * @args[0] defines the conformance-level, valid values are:
         *   61: Level 1 (VT100)
         *   62: Level 2 (VT200)
         *   63: Level 3 (VT300)
         *   64: Level 4 (VT400)
         * @args[1] defines the 8bit-mode, valid values are:
         *    0: 8-bit controls
         *    1: 7-bit controls
         *    2: 8-bit controls (same as 0)
         *
         * If @args[0] is 61, then @args[1] is ignored and 7bit controls are
         * enforced.
         *
         * Defaults:
         *   args[0]: 64
         *   args[1]: 0
         */
#if 0
        unsigned int level = 64, bit = 0;

        if (seq->n_args > 0) {
                level = seq->args[0];
                if (seq->n_args > 1)
                        bit = seq->args[1];
        }

        vte_screen_hard_reset(screen);

        switch (level) {
        case 61:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT100;
                screen->flags |= VTE_FLAG_7BIT_MODE;
                break;
        case 62 ... 69:
                screen->conformance_level = VTE_CONFORMANCE_LEVEL_VT400;
                if (bit == 1)
                        screen->flags |= VTE_FLAG_7BIT_MODE;
                else
                        screen->flags &= ~VTE_FLAG_7BIT_MODE;
                break;
        }
#endif
}

void
VteTerminalPrivate::DECSCP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCP - select-communication-port
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSCPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSCPP - select-columns-per-page
         * Select columns per page. The number of rows is unaffected by this.
         * @args[0] selectes the number of columns (width), DEC only defines 80
         * and 132, but we allow any integer here. 0 is equivalent to 80.
         * Page content is *not* cleared and the cursor is left untouched.
         * However, if the page is reduced in width and the cursor would be
         * outside the visible region, it's set to the right border. Newly added
         * cells are cleared. No data is retained outside the visible region.
         *
         * Defaults:
         *   args[0]: 0
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECSCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSCS - select-communication-speed
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSCUSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSCUSR - set-cursor-style
         * This changes the style of the cursor. @args[0] can be one of:
         *   0, 1: blinking block
         *      2: steady block
         *      3: blinking underline
         *      4: steady underline
         * Changing this setting does _not_ affect the cursor visibility itself.
         * Use DECTCEM for that.
         *
         * Defaults:
         *   args[0]: 0
         */

        seq_set_cursor_style(seq);
}

void
VteTerminalPrivate::DECSDDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDDT - select-disconnect-delay-time
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSDPT(vte::parser::Sequence const& seq)
{
        /*
         * DECSDPT - select-digital-printed-data-type
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSED(vte::parser::Sequence const& seq)
{
        /*
         * DECSED - selective-erase-in-display
         * This control function erases some or all of the erasable characters
         * in the display. DECSED can only erase characters defined as erasable
         * by the DECSCA control function. DECSED works inside or outside the
         * scrolling margins.
         *
         * @args[0] defines which regions are erased. If it is 0, all cells from
         * the cursor (inclusive) till the end of the display are erase. If it
         * is 1, all cells from the start of the display till the cursor
         * (inclusive) are erased. If it is 2, all cells are erased.
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_display(seq);
}

void
VteTerminalPrivate::DECSEL(vte::parser::Sequence const& seq)
{
        /*
         * DECSEL - selective-erase-in-line
         * This control function erases some or all of the erasable characters
         * in a single line of text. DECSEL erases only those characters defined
         * as erasable by the DECSCA control function. DECSEL works inside or
         * outside the scrolling margins.
         *
         * @args[0] defines the region to be erased. If it is 0, all cells from
         * the cursor (inclusive) till the end of the line are erase. If it is
         * 1, all cells from the start of the line till the cursor (inclusive)
         * are erased. If it is 2, the whole line of the cursor is erased.
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_line(seq);
}

void
VteTerminalPrivate::DECSERA(vte::parser::Sequence const& seq)
{
        /*
         * DECSERA - selective-erase-rectangular-area
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSFC(vte::parser::Sequence const& seq)
{
        /*
         * DECSFC - select-flow-control
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSKCV(vte::parser::Sequence const& seq)
{
        /*
         * DECSKCV - set-key-click-volume
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSLCK(vte::parser::Sequence const& seq)
{
        /*
         * DECSLCK - set-lock-key-style
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSLE(vte::parser::Sequence const& seq)
{
        /*
         * DECSLE - select-locator-events
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECSLPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSLPP - set-lines-per-page
         * Set the number of lines used for the page. @args[0] specifies the
         * number of lines to be used. DEC only allows a limited number of
         * choices, however, we allow all integers. 0 is equivalent to 24.
         *
         * Defaults:
         *   args[0]: 0
         *
         * TODO: implement
         */
}

void
VteTerminalPrivate::DECSLRM_OR_SC(vte::parser::Sequence const& seq)
{
        /*
         * DECSLRM_OR_SC - set-left-and-right-margins or save-cursor
         *
         * TODO: Detect save-cursor and run it. DECSLRM is not worth
         *       implementing.
         */

        //FIXMEchpe
        seq_save_cursor(seq);
}

void
VteTerminalPrivate::DECSMBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSMBV - set-margin-bell-volume
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSMKR(vte::parser::Sequence const& seq)
{
        /*
         * DECSMKR - select-modifier-key-reporting
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSNLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSNLS - set-lines-per-screen
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSPP(vte::parser::Sequence const& seq)
{
        /*
         * DECSPP - set-port-parameter
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSPPCS(vte::parser::Sequence const& seq)
{
        /*
         * DECSPPCS - select-pro-printer-character-set
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSPRTT(vte::parser::Sequence const& seq)
{
        /*
         * DECSPRTT - select-printer-type
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSR(vte::parser::Sequence const& seq)
{
        /*
         * DECSR - secure-reset
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSRFR(vte::parser::Sequence const& seq)
{
        /*
         * DECSRFR - select-refresh-rate
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSSCLS(vte::parser::Sequence const& seq)
{
        /*
         * DECSSCLS - set-scroll-speed
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSSDT(vte::parser::Sequence const& seq)
{
        /*
         * DECSSDT - select-status-display-line-type
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSSL(vte::parser::Sequence const& seq)
{
        /*
         * DECSSL - select-setup-language
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECST8C(vte::parser::Sequence const& seq)
{
        /*
         * DECST8C - set-tab-at-every-8-columns
         * Clear the tab-ruler and reset it to a tab at every 8th column,
         * starting at 9 (though, setting a tab at 1 is fine as it has no
         * effect).
         */
#if 0
        unsigned int i;

        for (i = 0; i < screen->page->width; i += 8)
                screen->tabs[i / 8] = 0x1;
#endif
}

void
VteTerminalPrivate::DECSTBM(vte::parser::Sequence const& seq)
{
        /*
         * DECSTBM - set-top-and-bottom-margins
         * This call resets the cursor position to (1,1).
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: last page-line
         */
#if 0
        unsigned int top, bottom;

        top = 1;
        bottom = screen->page->height;

        if (seq->args[0] > 0)
                top = seq->args[0];
        if (seq->args[1] > 0)
                bottom = seq->args[1];

        if (top > screen->page->height)
                top = screen->page->height;
        if (bottom > screen->page->height)
                bottom = screen->page->height;

        if (top >= bottom ||
            top > screen->page->height ||
            bottom > screen->page->height) {
                top = 1;
                bottom = screen->page->height;
        }

        vte_page_set_scroll_region(screen->page, top - 1, bottom - top + 1);
        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, 0, 0);
#endif

        seq_set_scrolling_region(seq);
}

void
VteTerminalPrivate::DECSTR(vte::parser::Sequence const& seq)
{
        /*
         * DECSTR - soft-terminal-reset
         * Perform a soft reset to the default values.
         */
#if 0
        vte_screen_soft_reset(screen);
#endif

        seq_soft_reset(seq);
}

void
VteTerminalPrivate::DECSTRL(vte::parser::Sequence const& seq)
{
        /*
         * DECSTRL - set-transmit-rate-limit
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSWBV(vte::parser::Sequence const& seq)
{
        /*
         * DECSWBV - set-warning-bell-volume
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECSWL(vte::parser::Sequence const& seq)
{
        /*
         * DECSWL - single-width-single-height-line
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECTID(vte::parser::Sequence const& seq)
{
        /*
         * DECTID - select-terminal-id
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECTME(vte::parser::Sequence const& seq)
{
        /*
         * DECTME - terminal-mode-emulation
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DECTST(vte::parser::Sequence const& seq)
{
        /*
         * DECTST - invoke-confidence-test
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::DL(vte::parser::Sequence const& seq)
{
        /*
         * DL - delete-line
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_delete_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        seq_delete_lines(seq);
}

void
VteTerminalPrivate::DSR_ANSI(vte::parser::Sequence const& seq)
{
        /*
         * DSR_ANSI - device-status-report-ansi
         *
         * TODO: implement
         */

        seq_device_status_report(seq);
}

void
VteTerminalPrivate::DSR_DEC(vte::parser::Sequence const& seq)
{
        /*
         * DSR_DEC - device-status-report-dec
         *
         * TODO: implement
         */

        seq_dec_device_status_report(seq);
}

void
VteTerminalPrivate::ECH(vte::parser::Sequence const& seq)
{
        /*
         * ECH - erase-character
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_erase(screen->page,
                          screen->state.cursor_x, screen->state.cursor_y,
                          screen->state.cursor_x + num, screen->state.cursor_y,
                          &screen->state.attr, screen->age, false);
#endif

        seq_erase_characters(seq);
}

void
VteTerminalPrivate::ED(vte::parser::Sequence const& seq)
{
        /*
         * ED - erase-in-display
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_display(seq);
}

void
VteTerminalPrivate::EL(vte::parser::Sequence const& seq)
{
        /*
         * EL - erase-in-line
         *
         * Defaults:
         *   args[0]: 0
         */

        erase_in_line(seq);
}

void
VteTerminalPrivate::ENQ(vte::parser::Sequence const& seq)
{
        /*
         * ENQ - enquiry
         * Transmit the answerback-string. If none is set, do nothing.
         */
#if 0
        if (screen->answerback)
                return screen_write(screen,
                                    screen->answerback,
                                    strlen(screen->answerback));
#endif

        seq_return_terminal_status(seq);
}

void
VteTerminalPrivate::EPA(vte::parser::Sequence const& seq)
{
        /*
         * EPA - end-of-guarded-area
         *
         * TODO: What is this?
         */
}

void
VteTerminalPrivate::FF(vte::parser::Sequence const& seq)
{
        /*
         * FF - form-feed
         * This causes the cursor to jump to the next line. It is treated the
         * same as LF.
         */

#if 0
        screen_LF(screen, seq);
#endif

        seq_form_feed(seq);
}

void
VteTerminalPrivate::HPA(vte::parser::Sequence const& seq)
{
        /*
         * HPA - horizontal-position-absolute
         * HPA causes the active position to be moved to the n-th horizontal
         * position of the active line. If an attempt is made to move the active
         * position past the last position on the line, then the active position
         * stops at the last position on the line.
         *
         * @args[0] defines the horizontal position. 0 is treated as 1.
         *
         * Defaults:
         *   args[0]: 1
         */

#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set(screen, num - 1, screen->state.cursor_y);
#endif

        seq_character_position_absolute(seq);
}

void
VteTerminalPrivate::HPR(
                      vte::parser::Sequence const& seq)
{
        /*
         * HPR - horizontal-position-relative
         * HPR causes the active position to be moved to the n-th following
         * horizontal position of the active line. If an attempt is made to move
         * the active position past the last position on the line, then the
         * active position stops at the last position on the line.
         *
         * @args[0] defines the horizontal position. 0 is treated as 1.
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_right(screen, num);
#endif
}

void
VteTerminalPrivate::HT(vte::parser::Sequence const& seq)
{
        /*
         * HT - horizontal-tab
         * Moves the cursor to the next tab stop. If there are no more tab
         * stops, the cursor moves to the right margin. HT does not cause text
         * to auto wrap.
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_right_tab(screen, 1);
#endif

        seq_tab(seq);
}

void
VteTerminalPrivate::HTS(vte::parser::Sequence const& seq)
{
        /*
         * HTS - horizontal-tab-set
         *
         * XXX
         */
#if 0
        unsigned int pos;

        pos = screen->state.cursor_x;
        if (screen->page->width > 0)
                screen->tabs[pos / 8] |= 1U << (pos % 8);
#endif

        seq_tab_set(seq);
}

void
VteTerminalPrivate::HVP(vte::parser::Sequence const& seq)
{
        /*
         * HVP - horizontal-and-vertical-position
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *   args[1]: 1
         *
         * References: ECMA-48 FIXME
         *             VT525
         */

        CUP(seq);
}

void
VteTerminalPrivate::ICH(vte::parser::Sequence const& seq)
{
        /*
         * ICH - insert-character
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_cells(screen->page,
                                 screen->state.cursor_x,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        seq_insert_blank_characters(seq);
}

void
VteTerminalPrivate::IL(vte::parser::Sequence const& seq)
{
        /*
         * IL - insert-line
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        vte_page_insert_lines(screen->page,
                                 screen->state.cursor_y,
                                 num,
                                 &screen->state.attr,
                                 screen->age);
#endif

        seq_insert_lines(seq);
}

void
VteTerminalPrivate::IND(vte::parser::Sequence const& seq)
{
        /*
         * IND - index - DEPRECATED
         */
#if 0
        screen_cursor_down(screen, 1, true);
#endif

        seq_index(seq);
}

void
VteTerminalPrivate::LF(vte::parser::Sequence const& seq)
{
        /*
         * LF - line-feed
         */

#if 0
        screen_cursor_down(screen, 1, true);
        if (screen->flags & VTE_FLAG_NEWLINE_MODE)
                screen_cursor_left(screen, screen->state.cursor_x);
#endif

        seq_line_feed(seq);
}

void
VteTerminalPrivate::LS1R(vte::parser::Sequence const& seq)
{
        /*
         * LS1R - locking-shift-1-right
         * Map G1 into GR.
         */
#if 0
        screen->state.gr = &screen->g1;
#endif
}

void
VteTerminalPrivate::LS2(vte::parser::Sequence const& seq)
{
        /*
         * LS2 - locking-shift-2
         * Map G2 into GL.
         */
#if 0
        screen->state.gl = &screen->g2;
#endif
}

void
VteTerminalPrivate::LS2R(vte::parser::Sequence const& seq)
{
        /*
         * LS2R - locking-shift-2-right
         * Map G2 into GR.
         */
#if 0
        screen->state.gr = &screen->g2;
#endif
}

void
VteTerminalPrivate::LS3(vte::parser::Sequence const& seq)
{
        /*
         * LS3 - locking-shift-3
         * Map G3 into GL.
         */

#if 0
        screen->state.gl = &screen->g3;
#endif
}

void
VteTerminalPrivate::LS3R(vte::parser::Sequence const& seq)
{
        /*
         * LS3R - locking-shift-3-right
         * Map G3 into GR.
         */
#if 0
        screen->state.gr = &screen->g3;
#endif
}

void
VteTerminalPrivate::MC_ANSI(vte::parser::Sequence const& seq)
{
        /*
         * MC_ANSI - media-copy-ansi
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::MC_DEC(vte::parser::Sequence const& seq)
{
        /*
         * MC_DEC - media-copy-dec
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::NEL(vte::parser::Sequence const& seq)
{
        /*
         * NEL - next-line
         * XXX
         */
#if 0
        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, 1, true);
        screen_cursor_set(screen, 0, screen->state.cursor_y);
#endif

        seq_next_line(seq);
}

void
VteTerminalPrivate::NP(vte::parser::Sequence const& seq)
{
        /*
         * NP - next-page
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *
         * Probably not worth implementing. We only support a single page.
         */
}

void
VteTerminalPrivate::NUL(vte::parser::Sequence const& seq)
{
        /*
         */
}

void
VteTerminalPrivate::PP(vte::parser::Sequence const& seq)
{
        /*
         * PP - preceding-page
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *
         * Probably not worth implementing. We only support a single page.
         */
}

void
VteTerminalPrivate::PPA(vte::parser::Sequence const& seq)
{
        /*
         * PPA - page-position-absolute
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *
         * Probably not worth implementing. We only support a single page.
         */
}

void
VteTerminalPrivate::PPB(vte::parser::Sequence const& seq)
{
        /*
         * PPB - page-position-backward
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *
         * Probably not worth implementing. We only support a single page.
         */
}

void
VteTerminalPrivate::PPR(vte::parser::Sequence const& seq)
{
        /*
         * PPR - page-position-relative
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         *
         * Probably not worth implementing. We only support a single page.
         */
}

void
VteTerminalPrivate::RC(vte::parser::Sequence const& seq)
{
        /*
         * RC - restore-cursor
         */

#if 0
        screen_DECRC(screen, seq);
#endif
}

void
VteTerminalPrivate::REP(vte::parser::Sequence const& seq)
{
        /*
         * REP - repeat
         * Repeat the preceding graphics-character the given number of times.
         * @args[0] specifies how often it shall be repeated. 0 is treated as 1.
         *
         * Defaults:
         *   args[0]: 1
         */

        seq_repeat(seq);
}

void
VteTerminalPrivate::RI(vte::parser::Sequence const& seq)
{
        /*
         * RI - reverse-index
         * Moves the cursor up one line in the same column. If the cursor is at
         * the top margin, the page scrolls down.
         */
#if 0
        screen_cursor_up(screen, 1, true);
#endif

        seq_reverse_index(seq);
}

void
VteTerminalPrivate::RIS(vte::parser::Sequence const& seq)
{
        /*
         * RIS - reset-to-initial-state
         * XXX
         */

#if 0
        vte_screen_hard_reset(screen);
#endif

        seq_full_reset(seq);
}

void
VteTerminalPrivate::RM_ANSI(vte::parser::Sequence const& seq)
{
        /*
         * RM_ANSI - reset-mode-ansi
         *
         * TODO: implement (see VT510rm manual)
         */
#if 0
        unsigned int i;

        for (i = 0; i < seq->n_args; ++i)
                screen_mode_change_ansi(screen, seq->args[i], false);
#endif

        seq_reset_mode(seq);
}

void
VteTerminalPrivate::RM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * RM_DEC - reset-mode-dec
         * This is the same as RM_ANSI but for DEC modes.
         */
#if 0
        unsigned int i;

        for (i = 0; i < seq->n_args; ++i)
                screen_mode_change_dec(screen, seq->args[i], false);
#endif

        seq_decreset(seq);
}

void
VteTerminalPrivate::S7C1T(vte::parser::Sequence const& seq)
{
        /*
         * S7C1T - set-7bit-c1-terminal
         * This causes the terminal to start sending C1 controls as 7bit
         * sequences instead of 8bit C1 controls.
         * This is ignored if the terminal is below level-2 emulation mode
         * (VT100 and below), the terminal already sends 7bit controls then.
         */

#if 0
        if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                screen->flags |= VTE_FLAG_7BIT_MODE;
#endif
}

void
VteTerminalPrivate::S8C1T(vte::parser::Sequence const& seq)
{
        /*
         * S8C1T - set-8bit-c1-terminal
         * This causes the terminal to start sending C1 controls as 8bit C1
         * control instead of 7bit sequences.
         * This is ignored if the terminal is below level-2 emulation mode
         * (VT100 and below). The terminal always sends 7bit controls in those
         * modes.
         */
#if 0
        if (screen->conformance_level > VTE_CONFORMANCE_LEVEL_VT100)
                screen->flags &= ~VTE_FLAG_7BIT_MODE;
#endif
}

void
VteTerminalPrivate::SCS(vte::parser::Sequence const& seq)
{
        /*
         * SCS - select-character-set
         * Designate character sets to G-sets. The mapping from intermediates
         * and terminal characters in the escape sequence to G-sets and
         * character-sets is non-trivial and implemented separately. See there
         * for more information.
         * This call simply sets the selected G-set to the desired
         * character-set.
         */
#if 0
        vte_charset *cs = NULL;

        /* TODO: support more of them? */
        switch (seq->charset) {
        case VTE_CHARSET_ISO_LATIN1_SUPPLEMENTAL:
        case VTE_CHARSET_ISO_LATIN2_SUPPLEMENTAL:
        case VTE_CHARSET_ISO_LATIN5_SUPPLEMENTAL:
        case VTE_CHARSET_ISO_GREEK_SUPPLEMENTAL:
        case VTE_CHARSET_ISO_HEBREW_SUPPLEMENTAL:
        case VTE_CHARSET_ISO_LATIN_CYRILLIC:
                break;

        case VTE_CHARSET_DEC_SPECIAL_GRAPHIC:
                cs = &vte_dec_special_graphics;
                break;
        case VTE_CHARSET_DEC_SUPPLEMENTAL:
                cs = &vte_dec_supplemental_graphics;
                break;
        case VTE_CHARSET_DEC_TECHNICAL:
        case VTE_CHARSET_CYRILLIC_DEC:
        case VTE_CHARSET_DUTCH_NRCS:
        case VTE_CHARSET_FINNISH_NRCS:
        case VTE_CHARSET_FRENCH_NRCS:
        case VTE_CHARSET_FRENCH_CANADIAN_NRCS:
        case VTE_CHARSET_GERMAN_NRCS:
        case VTE_CHARSET_GREEK_DEC:
        case VTE_CHARSET_GREEK_NRCS:
        case VTE_CHARSET_HEBREW_DEC:
        case VTE_CHARSET_HEBREW_NRCS:
        case VTE_CHARSET_ITALIAN_NRCS:
        case VTE_CHARSET_NORWEGIAN_DANISH_NRCS:
        case VTE_CHARSET_PORTUGUESE_NRCS:
        case VTE_CHARSET_RUSSIAN_NRCS:
        case VTE_CHARSET_SCS_NRCS:
        case VTE_CHARSET_SPANISH_NRCS:
        case VTE_CHARSET_SWEDISH_NRCS:
        case VTE_CHARSET_SWISS_NRCS:
        case VTE_CHARSET_TURKISH_DEC:
        case VTE_CHARSET_TURKISH_NRCS:
                break;

        case VTE_CHARSET_USERPREF_SUPPLEMENTAL:
                break;
        }

        if (seq->intermediates & VTE_SEQ_FLAG_POPEN)
                screen->g0 = cs ? : &vte_unicode_lower;
        else if (seq->intermediates & VTE_SEQ_FLAG_PCLOSE)
                screen->g1 = cs ? : &vte_unicode_upper;
        else if (seq->intermediates & VTE_SEQ_FLAG_MULT)
                screen->g2 = cs ? : &vte_unicode_lower;
        else if (seq->intermediates & VTE_SEQ_FLAG_PLUS)
                screen->g3 = cs ? : &vte_unicode_upper;
        else if (seq->intermediates & VTE_SEQ_FLAG_MINUS)
                screen->g1 = cs ? : &vte_unicode_upper;
        else if (seq->intermediates & VTE_SEQ_FLAG_DOT)
                screen->g2 = cs ? : &vte_unicode_lower;
        else if (seq->intermediates & VTE_SEQ_FLAG_SLASH)
                screen->g3 = cs ? : &vte_unicode_upper;
#endif

        // FIXMEchpe: seq_designate_*(seq);
        set_character_replacements(0, VTE_CHARACTER_REPLACEMENT_NONE);
}

void
VteTerminalPrivate::SD(vte::parser::Sequence const& seq)
{
        /*
         * SD - scroll-down
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_down(screen->page,
                                num,
                                &screen->state.attr,
                                screen->age,
                                NULL);
#endif

        seq_scroll_down(seq);
}

void
VteTerminalPrivate::SGR(vte::parser::Sequence const& seq)
{
        /*
         * SGR - select-graphics-rendition
         */
#if 0
        struct vte_color *dst;
        unsigned int i, code;
        int v;

        if (seq->n_args < 1) {
                memset(&screen->state.attr, 0, sizeof(screen->state.attr));
                return 0;
        }

        for (i = 0; i < seq->n_args; ++i) {
                v = seq->args[i];
                switch (v) {
                case 1:
                        screen->state.attr.bold = 1;
                        break;
                case 3:
                        screen->state.attr.italic = 1;
                        break;
                case 4:
                        screen->state.attr.underline = 1;
                        break;
                case 5:
                        screen->state.attr.blink = 1;
                        break;
                case 7:
                        screen->state.attr.inverse = 1;
                        break;
                case 8:
                        screen->state.attr.hidden = 1;
                        break;
                case 22:
                        screen->state.attr.bold = 0;
                        break;
                case 23:
                        screen->state.attr.italic = 0;
                        break;
                case 24:
                        screen->state.attr.underline = 0;
                        break;
                case 25:
                        screen->state.attr.blink = 0;
                        break;
                case 27:
                        screen->state.attr.inverse = 0;
                        break;
                case 28:
                        screen->state.attr.hidden = 0;
                        break;
                case 30 ... 37:
                        screen->state.attr.fg.ccode = v - 30 +
                                                      VTE_CCODE_BLACK;
                        break;
                case 39:
                        screen->state.attr.fg.ccode = 0;
                        break;
                case 40 ... 47:
                        screen->state.attr.bg.ccode = v - 40 +
                                                      VTE_CCODE_BLACK;
                        break;
                case 49:
                        screen->state.attr.bg.ccode = 0;
                        break;
                case 90 ... 97:
                        screen->state.attr.fg.ccode = v - 90 +
                                                      VTE_CCODE_LIGHT_BLACK;
                        break;
                case 100 ... 107:
                        screen->state.attr.bg.ccode = v - 100 +
                                                      VTE_CCODE_LIGHT_BLACK;
                        break;
                case 38:
                        /* fallthrough */
                case 48:

                        if (v == 38)
                                dst = &screen->state.attr.fg;
                        else
                                dst = &screen->state.attr.bg;

                        ++i;
                        if (i >= seq->n_args)
                                break;

                        switch (seq->args[i]) {
                        case 2:
                                /* 24bit-color support */

                                i += 3;
                                if (i >= seq->n_args)
                                        break;

                                dst->ccode = VTE_CCODE_RGB;
                                dst->red = (seq->args[i - 2] >= 0) ? seq->args[i - 2] : 0;
                                dst->green = (seq->args[i - 1] >= 0) ? seq->args[i - 1] : 0;
                                dst->blue = (seq->args[i] >= 0) ? seq->args[i] : 0;

                                break;
                        case 5:
                                /* 256-color support */

                                ++i;
                                if (i >= seq->n_args || seq->args[i] < 0)
                                        break;

                                dst->ccode = VTE_CCODE_256;
                                code = seq->args[i];
                                dst->c256 = code < 256 ? code : 0;

                                break;
                        }

                        break;
                case -1:
                        /* fallthrough */
                case 0:
                        memset(&screen->state.attr, 0,
                               sizeof(screen->state.attr));
                        break;
                }
        }
#endif

        seq_character_attributes(seq);
}

void
VteTerminalPrivate::SI(vte::parser::Sequence const& seq)
{
        /*
         * SI - shift-in
         * Map G0 into GL.
         */
#if 0
        screen->state.gl = &screen->g0;
#endif

        seq_shift_in(seq);
}

void
VteTerminalPrivate::SM_ANSI(vte::parser::Sequence const& seq)
{
        /*
         * SM_ANSI - set-mode-ansi
         *
         * TODO: implement
         */
#if 0
        unsigned int i;

        for (i = 0; i < seq->n_args; ++i)
                screen_mode_change_ansi(screen, seq->args[i], true);
#endif

        seq_set_mode(seq);
}

void
VteTerminalPrivate::SM_DEC(vte::parser::Sequence const& seq)
{
        /*
         * SM_DEC - set-mode-dec
         * This is the same as SM_ANSI but for DEC modes.
         */
#if 0
        unsigned int i;

        for (i = 0; i < seq->n_args; ++i)
                screen_mode_change_dec(screen, seq->args[i], true);
#endif

        seq_decset(seq);
}

void
VteTerminalPrivate::SO(vte::parser::Sequence const& seq)
{
        /*
         * SO - shift-out
         * Map G1 into GL.
         */
#if 0
        screen->state.gl = &screen->g1;
#endif

        seq_shift_out(seq);
}

void
VteTerminalPrivate::SPA(vte::parser::Sequence const& seq)
{
        /*
         * SPA - start-of-protected-area
         *
         * TODO: What is this?
         */
}

void
VteTerminalPrivate::SS2(vte::parser::Sequence const& seq)
{
        /*
         * SS2 - single-shift-2
         * Temporarily map G2 into GL for the next graphics character.
         */
#if 0
        screen->state.glt = &screen->g2;
#endif
}

void
VteTerminalPrivate::SS3(vte::parser::Sequence const& seq)
{
        /*
         * SS3 - single-shift-3
         * Temporarily map G3 into GL for the next graphics character
         */
#if 0
        screen->state.glt = &screen->g3;
#endif
}

void
VteTerminalPrivate::ST(vte::parser::Sequence const& seq)
{
        /*
         * ST - string-terminator
         * The string-terminator is usually part of control-sequences and
         * handled by the parser. In all other situations it is silently
         * ignored.
         */
}

void
VteTerminalPrivate::SU(vte::parser::Sequence const& seq)
{
        /*
         * SU - scroll-up
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        vte_page_scroll_up(screen->page,
                              num,
                              &screen->state.attr,
                              screen->age,
                              screen->history);
#endif

        seq_scroll_up(seq);
}

void
VteTerminalPrivate::SUB(vte::parser::Sequence const& seq)
{
        /*
         * SUB - substitute
         * Cancel the current control-sequence and print a replacement
         * character. Our parser already handles this so all we have to do is
         * print the replacement character.
         */
#if 0
        static const struct vte_seq rep = {
                .type = VTE_SEQ_GRAPHIC,
                .command = VTE_CMD_GRAPHIC,
                .terminator = 0xfffd,
        };

        return screen_GRAPHIC(screen, &rep);
#endif
}

void
VteTerminalPrivate::TBC(vte::parser::Sequence const& seq)
{
        /*
         * TBC - tab-clear
         * Clears tab stops.
         *
         * Arguments:
         *   args[0]: mode
         *
         * Defaults:
         *   args[0]: 0
         */
#if 0
        unsigned int mode = 0, pos;

        if (seq->args[0] > 0)
                mode = seq->args[0];

        switch (mode) {
        case 0:
                pos = screen->state.cursor_x;
                if (screen->page->width > 0)
                        screen->tabs[pos / 8] &= ~(1U << (pos % 8));
                break;
        case 3:
                if (screen->page->width > 0)
                        memset(screen->tabs, 0, (screen->page->width + 7) / 8);
                break;
        }
#endif

        seq_tab_clear(seq);
}

void
VteTerminalPrivate::VPA(vte::parser::Sequence const& seq)
{
        /*
         * VPA - vertical-line-position-absolute
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int pos = 1;

        if (seq->args[0] > 0)
                pos = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_set_rel(screen, screen->state.cursor_x, pos - 1);
#endif

        seq_line_position_absolute(seq);
}

void
VteTerminalPrivate::VPR(vte::parser::Sequence const& seq)
{
        /*
         * VPR - vertical-line-position-relative
         * XXX
         *
         * Defaults:
         *   args[0]: 1
         */
#if 0
        unsigned int num = 1;

        if (seq->args[0] > 0)
                num = seq->args[0];

        screen_cursor_clear_wrap(screen);
        screen_cursor_down(screen, num, false);
#endif
}

void
VteTerminalPrivate::VT(vte::parser::Sequence const& seq)
{
        /*
         * VT - vertical-tab
         * This causes a vertical jump by one line. Terminals treat it exactly
         * the same as LF.
         */

        LF(seq);
}

void
VteTerminalPrivate::XTERM_CLLHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_CLLHP - xterm-cursor-lower-left-hp-bugfix
         * Move the cursor to the lower-left corner of the page. This is an HP
         * bugfix by xterm.
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_IHMT(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_IHMT - xterm-initiate-highlight-mouse-tracking
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_MLHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MLHP - xterm-memory-lock-hp-bugfix
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_MUHP(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_MUHP - xterm-memory-unlock-hp-bugfix
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_RPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RPM - xterm-restore-private-mode
         */

        seq_restore_mode(seq);
}

void
VteTerminalPrivate::XTERM_RRV(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RRV - xterm-reset-resource-value
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_RTM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_RTM - xterm-reset-title-mode
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SACL1(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SACL1 - xterm-set-ansi-conformance-level-1
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SACL2(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SACL2 - xterm-set-ansi-conformance-level-2
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SACL3(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SACL3 - xterm-set-ansi-conformance-level-3
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SDCS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SDCS - xterm-set-default-character-set
         * Select the default character set. We treat this the same as UTF-8 as
         * this is our default character set. As we always use UTF-8, this
         * becomes as no-op.
         */
}

void
VteTerminalPrivate::XTERM_SGFX(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SGFX - xterm-sixel-graphics
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SPM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SPM - xterm-set-private-mode
         */

        seq_save_mode(seq);
}

void
VteTerminalPrivate::XTERM_SRV(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SRV - xterm-set-resource-value
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_STM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_STM - xterm-set-title-mode
         *
         * Probably not worth implementing.
         */
}

void
VteTerminalPrivate::XTERM_SUCS(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_SUCS - xterm-select-utf8-character-set
         * Select UTF-8 as character set. This is our default and only
         * character set. Hence, this is a no-op.
         */
}

void
VteTerminalPrivate::XTERM_WM(vte::parser::Sequence const& seq)
{
        /*
         * XTERM_WM - xterm-window-management
         *
         * Probably not worth implementing.
         */

        seq_window_manipulation(seq);
}
