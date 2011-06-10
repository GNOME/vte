/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#ifndef VTE_ENUMS_H
#define VTE_ENUMS_H

G_BEGIN_DECLS

/**
 * VteEraseBinding:
 * @VTE_ERASE_AUTO: For backspace, attempt to determine the right value from the terminal's IO settings.  For delete, use the control sequence.
 * @VTE_ERASE_ASCII_BACKSPACE: Send an ASCII backspace character (0x08).
 * @VTE_ERASE_ASCII_DELETE: Send an ASCII delete character (0x7F).
 * @VTE_ERASE_DELETE_SEQUENCE: Send the "@@7" control sequence.
 * @VTE_ERASE_TTY: Send terminal's "erase" setting.
 *
 * An enumerated type which can be used to indicate which string the terminal
 * should send to an application when the user presses the Delete or Backspace
 * keys.
 */
typedef enum {
	VTE_ERASE_AUTO,
	VTE_ERASE_ASCII_BACKSPACE,
	VTE_ERASE_ASCII_DELETE,
	VTE_ERASE_DELETE_SEQUENCE,
	VTE_ERASE_TTY
} VteEraseBinding;

/**
 * VteTerminalCursorBlinkMode:
 * @VTE_CURSOR_BLINK_SYSTEM: Follow GTK+ settings for cursor blinking.
 * @VTE_CURSOR_BLINK_ON: Cursor blinks.
 * @VTE_CURSOR_BLINK_OFF: Cursor does not blink.
 *
 * An enumerated type which can be used to indicate the cursor blink mode
 * for the terminal.
 */
typedef enum {
        VTE_CURSOR_BLINK_SYSTEM,
        VTE_CURSOR_BLINK_ON,
        VTE_CURSOR_BLINK_OFF
} VteTerminalCursorBlinkMode;

/**
 * VteTerminalCursorShape:
 * @VTE_CURSOR_SHAPE_BLOCK: Draw a block cursor.  This is the default.
 * @VTE_CURSOR_SHAPE_IBEAM: Draw a vertical bar on the left side of character.
 * This is similar to the default cursor for other GTK+ widgets.
 * @VTE_CURSOR_SHAPE_UNDERLINE: Draw a horizontal bar below the character.
 *
 * An enumerated type which can be used to indicate what should the terminal
 * draw at the cursor position.
 */
typedef enum {
        VTE_CURSOR_SHAPE_BLOCK,
        VTE_CURSOR_SHAPE_IBEAM,
        VTE_CURSOR_SHAPE_UNDERLINE
} VteTerminalCursorShape;

/**
 * VteTerminalEffect:
 * @VTE_TERMINAL_EFFECT_REVERSE: Text is draw with foreground and background color reversed.
 * @VTE_TERMINAL_EFFECT_COLOR: Text is drawn with the background color from the corresponding style property.
 */
typedef enum {
        VTE_TERMINAL_EFFECT_REVERSE,
        VTE_TERMINAL_EFFECT_COLOR
} VteTerminalEffect;


/**
 * VteWriteFlags:
 * @VTE_TERMINAL_WRITE_DEFAULT: Write contents as UTF-8 text.  This is the default.
 *
 * A flag type to determine how terminal contents should be written
 * to an output stream.
 */
typedef enum {
        VTE_WRITE_FLAG_DEFAULT = 0
} VteWriteFlags;

G_END_DECLS

#endif /* VTE_ENUMS_H */
