/*
 * Copyright (C) 2001-2004 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#pragma once

#define VTE_TAB_WIDTH			8
#define VTE_LINE_WIDTH			1
#define VTE_ROWS			24
#define VTE_COLUMNS			80

/*
 * Colors are encoded in 25 bits as follows:
 *
 * 0 .. 255:
 *   Colors set by SGR 256-color extension (38/48;5;index).
 *   These are direct indices into the color palette.
 *
 * 256 .. VTE_PALETTE_SIZE - 1 (261):
 *   Special values, such as default colors.
 *   These are direct indices into the color palette.
 *
 * VTE_LEGACY_COLORS_OFFSET (512) .. VTE_LEGACY_COLORS_OFFSET + VTE_LEGACY_FULL_COLOR_SET_SIZE - 1 (527):
 *   Colors set by legacy escapes (30..37/40..47, 90..97/100..107).
 *   These are translated to 0 .. 15 before looking up in the palette, taking bold into account.
 *
 * VTE_DIM_COLORS (2^10) .. :
 *   Dimmed version of the above, for foreground colors.
 *   Cell attributes can't have these colors.
 *
 * VTE_RGB_COLOR (2^24) .. VTE_RGB_COLOR + 16Mi - 1 (2^25 - 1):
 *   Colors set by SGR truecolor extension (38/48;2;red;green;blue)
 *   These are direct RGB values.
 */
#define VTE_LEGACY_COLORS_OFFSET	512
#define VTE_LEGACY_COLOR_SET_SIZE	8
#define VTE_LEGACY_FULL_COLOR_SET_SIZE	16
#define VTE_COLOR_PLAIN_OFFSET		0
#define VTE_COLOR_BRIGHT_OFFSET		8
#define VTE_DIM_COLOR			(1 << 10)
#define VTE_RGB_COLOR			(1 << 24)

#define VTE_DEFAULT_FG			256
#define VTE_DEFAULT_BG			257
#define VTE_BOLD_FG			258
#define VTE_HIGHLIGHT_FG		259
#define VTE_HIGHLIGHT_BG		260
#define VTE_CURSOR_BG			261
#define VTE_CURSOR_FG                   262
#define VTE_PALETTE_SIZE		263

#define VTE_SCROLLBACK_INIT		512
#define VTE_DEFAULT_CURSOR		GDK_XTERM
#define VTE_MOUSING_CURSOR		GDK_LEFT_PTR
#define VTE_TAB_MAX			999
#define VTE_ADJUSTMENT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_INPUT_RETRY_PRIORITY	G_PRIORITY_HIGH
#define VTE_INPUT_PRIORITY		G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_INPUT_PRIORITY	G_PRIORITY_DEFAULT_IDLE
#define VTE_CHILD_OUTPUT_PRIORITY	G_PRIORITY_HIGH
#define VTE_FX_PRIORITY			G_PRIORITY_DEFAULT_IDLE
#define VTE_REGCOMP_FLAGS		REG_EXTENDED
#define VTE_REGEXEC_FLAGS		0
#define VTE_INPUT_CHUNK_SIZE		0x2000
#define VTE_MAX_INPUT_READ		0x1000
#define VTE_INVALID_BYTE		'?'
#define VTE_DISPLAY_TIMEOUT		10
#define VTE_UPDATE_TIMEOUT		15
#define VTE_UPDATE_REPEAT_TIMEOUT	30
#define VTE_MAX_PROCESS_TIME		100
#define VTE_CELL_BBOX_SLACK		1
#define VTE_DEFAULT_UTF8_AMBIGUOUS_WIDTH 1

#define VTE_UTF8_BPC                    (6) /* Maximum number of bytes used per UTF-8 character */

/* Keep in decreasing order of precedence. */
#define VTE_COLOR_SOURCE_ESCAPE 0
#define VTE_COLOR_SOURCE_API 1

#define VTE_FONT_SCALE_MIN (.25)
#define VTE_FONT_SCALE_MAX (4.)
