/******************************************************************************
 * Copyright (c) 1998-2010,2011 Free Software Foundation, Inc.                *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the 'Software'), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, distribute    *
 * with modifications, sublicense, and/or sell copies of the Software, and to *
 * permit persons to whom the Software is furnished to do so, subject to the  *
 * following conditions:                                                      *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
 *                                                                            *
 * Except as contained in this notice, the name(s) of the above copyright     *
 * holders shall not be used in advertising or otherwise to promote the sale, *
 * use or other dealings in this Software without prior written               *
 * authorization.                                                             *
 ******************************************************************************/

/* Generated from ncurses/include/Caps from the ncurses sources; you can get it here for example:
 * http://anonscm.debian.org/gitweb/?p=collab-maint/ncurses.git;a=blob_plain;f=include/Caps;h=cb650a6be900c9d460498aa46d7843a11da57446;hb=refs/heads/upstream
 */

#ifndef __VTE_TERMINFO_VARS_H__
#define __VTE_TERMINFO_VARS_H__

#define VTE_TERMINFO_CAP_AUTO_LEFT_MARGIN               "bw"
#define VTE_TERMINFO_VAR_AUTO_LEFT_MARGIN               (  0 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_AUTO_RIGHT_MARGIN              "am"
#define VTE_TERMINFO_VAR_AUTO_RIGHT_MARGIN              (  1 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_NO_ESC_CTLC                    "xsb"
#define VTE_TERMINFO_VAR_NO_ESC_CTLC                    (  2 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_CEOL_STANDOUT_GLITCH           "xhp"
#define VTE_TERMINFO_VAR_CEOL_STANDOUT_GLITCH           (  3 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_EAT_NEWLINE_GLITCH             "xenl"
#define VTE_TERMINFO_VAR_EAT_NEWLINE_GLITCH             (  4 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_ERASE_OVERSTRIKE               "eo"
#define VTE_TERMINFO_VAR_ERASE_OVERSTRIKE               (  5 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_GENERIC_TYPE                   "gn"
#define VTE_TERMINFO_VAR_GENERIC_TYPE                   (  6 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HARD_COPY                      "hc"
#define VTE_TERMINFO_VAR_HARD_COPY                      (  7 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HAS_META_KEY                   "km"
#define VTE_TERMINFO_VAR_HAS_META_KEY                   (  8 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HAS_STATUS_LINE                "hs"
#define VTE_TERMINFO_VAR_HAS_STATUS_LINE                (  9 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_INSERT_NULL_GLITCH             "in"
#define VTE_TERMINFO_VAR_INSERT_NULL_GLITCH             ( 10 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_MEMORY_ABOVE                   "da"
#define VTE_TERMINFO_VAR_MEMORY_ABOVE                   ( 11 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_MEMORY_BELOW                   "db"
#define VTE_TERMINFO_VAR_MEMORY_BELOW                   ( 12 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_MOVE_INSERT_MODE               "mir"
#define VTE_TERMINFO_VAR_MOVE_INSERT_MODE               ( 13 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_MOVE_STANDOUT_MODE             "msgr"
#define VTE_TERMINFO_VAR_MOVE_STANDOUT_MODE             ( 14 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_OVER_STRIKE                    "os"
#define VTE_TERMINFO_VAR_OVER_STRIKE                    ( 15 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_STATUS_LINE_ESC_OK             "eslok"
#define VTE_TERMINFO_VAR_STATUS_LINE_ESC_OK             ( 16 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_DEST_TABS_MAGIC_SMSO           "xt"
#define VTE_TERMINFO_VAR_DEST_TABS_MAGIC_SMSO           ( 17 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_TILDE_GLITCH                   "hz"
#define VTE_TERMINFO_VAR_TILDE_GLITCH                   ( 18 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_TRANSPARENT_UNDERLINE          "ul"
#define VTE_TERMINFO_VAR_TRANSPARENT_UNDERLINE          ( 19 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_XON_XOFF                       "xon"
#define VTE_TERMINFO_VAR_XON_XOFF                       ( 20 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_NEEDS_XON_XOFF                 "nxon"
#define VTE_TERMINFO_VAR_NEEDS_XON_XOFF                 ( 21 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_PRTR_SILENT                    "mc5i"
#define VTE_TERMINFO_VAR_PRTR_SILENT                    ( 22 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HARD_CURSOR                    "chts"
#define VTE_TERMINFO_VAR_HARD_CURSOR                    ( 23 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_NON_REV_RMCUP                  "nrrmc"
#define VTE_TERMINFO_VAR_NON_REV_RMCUP                  ( 24 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_NO_PAD_CHAR                    "npc"
#define VTE_TERMINFO_VAR_NO_PAD_CHAR                    ( 25 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_NON_DEST_SCROLL_REGION         "ndscr"
#define VTE_TERMINFO_VAR_NON_DEST_SCROLL_REGION         ( 26 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_CAN_CHANGE                     "ccc"
#define VTE_TERMINFO_VAR_CAN_CHANGE                     ( 27 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_BACK_COLOR_ERASE               "bce"
#define VTE_TERMINFO_VAR_BACK_COLOR_ERASE               ( 28 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HUE_LIGHTNESS_SATURATION       "hls"
#define VTE_TERMINFO_VAR_HUE_LIGHTNESS_SATURATION       ( 29 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_COL_ADDR_GLITCH                "xhpa"
#define VTE_TERMINFO_VAR_COL_ADDR_GLITCH                ( 30 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_CR_CANCELS_MICRO_MODE          "crxm"
#define VTE_TERMINFO_VAR_CR_CANCELS_MICRO_MODE          ( 31 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_HAS_PRINT_WHEEL                "daisy"
#define VTE_TERMINFO_VAR_HAS_PRINT_WHEEL                ( 32 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_ROW_ADDR_GLITCH                "xvpa"
#define VTE_TERMINFO_VAR_ROW_ADDR_GLITCH                ( 33 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_SEMI_AUTO_RIGHT_MARGIN         "sam"
#define VTE_TERMINFO_VAR_SEMI_AUTO_RIGHT_MARGIN         ( 34 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_CPI_CHANGES_RES                "cpix"
#define VTE_TERMINFO_VAR_CPI_CHANGES_RES                ( 35 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_LPI_CHANGES_RES                "lpix"
#define VTE_TERMINFO_VAR_LPI_CHANGES_RES                ( 36 | VTE_TERMINFO_VARTYPE_BOOLEAN)
#define VTE_TERMINFO_CAP_COLUMNS                        "cols"
#define VTE_TERMINFO_VAR_COLUMNS                        (  0 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_INIT_TABS                      "it"
#define VTE_TERMINFO_VAR_INIT_TABS                      (  1 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_LINES                          "lines"
#define VTE_TERMINFO_VAR_LINES                          (  2 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_LINES_OF_MEMORY                "lm"
#define VTE_TERMINFO_VAR_LINES_OF_MEMORY                (  3 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAGIC_COOKIE_GLITCH            "xmc"
#define VTE_TERMINFO_VAR_MAGIC_COOKIE_GLITCH            (  4 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_PADDING_BAUD_RATE              "pb"
#define VTE_TERMINFO_VAR_PADDING_BAUD_RATE              (  5 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_VIRTUAL_TERMINAL               "vt"
#define VTE_TERMINFO_VAR_VIRTUAL_TERMINAL               (  6 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_WIDTH_STATUS_LINE              "wsl"
#define VTE_TERMINFO_VAR_WIDTH_STATUS_LINE              (  7 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_NUM_LABELS                     "nlab"
#define VTE_TERMINFO_VAR_NUM_LABELS                     (  8 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_LABEL_HEIGHT                   "lh"
#define VTE_TERMINFO_VAR_LABEL_HEIGHT                   (  9 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_LABEL_WIDTH                    "lw"
#define VTE_TERMINFO_VAR_LABEL_WIDTH                    ( 10 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAX_ATTRIBUTES                 "ma"
#define VTE_TERMINFO_VAR_MAX_ATTRIBUTES                 ( 11 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAXIMUM_WINDOWS                "wnum"
#define VTE_TERMINFO_VAR_MAXIMUM_WINDOWS                ( 12 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAX_COLORS                     "colors"
#define VTE_TERMINFO_VAR_MAX_COLORS                     ( 13 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAX_PAIRS                      "pairs"
#define VTE_TERMINFO_VAR_MAX_PAIRS                      ( 14 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_NO_COLOR_VIDEO                 "ncv"
#define VTE_TERMINFO_VAR_NO_COLOR_VIDEO                 ( 15 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_BUFFER_CAPACITY                "bufsz"
#define VTE_TERMINFO_VAR_BUFFER_CAPACITY                ( 16 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_DOT_VERT_SPACING               "spinv"
#define VTE_TERMINFO_VAR_DOT_VERT_SPACING               ( 17 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_DOT_HORZ_SPACING               "spinh"
#define VTE_TERMINFO_VAR_DOT_HORZ_SPACING               ( 18 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAX_MICRO_ADDRESS              "maddr"
#define VTE_TERMINFO_VAR_MAX_MICRO_ADDRESS              ( 19 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MAX_MICRO_JUMP                 "mjump"
#define VTE_TERMINFO_VAR_MAX_MICRO_JUMP                 ( 20 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MICRO_COL_SIZE                 "mcs"
#define VTE_TERMINFO_VAR_MICRO_COL_SIZE                 ( 21 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_MICRO_LINE_SIZE                "mls"
#define VTE_TERMINFO_VAR_MICRO_LINE_SIZE                ( 22 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_NUMBER_OF_PINS                 "npins"
#define VTE_TERMINFO_VAR_NUMBER_OF_PINS                 ( 23 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_OUTPUT_RES_CHAR                "orc"
#define VTE_TERMINFO_VAR_OUTPUT_RES_CHAR                ( 24 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_OUTPUT_RES_LINE                "orl"
#define VTE_TERMINFO_VAR_OUTPUT_RES_LINE                ( 25 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_OUTPUT_RES_HORZ_INCH           "orhi"
#define VTE_TERMINFO_VAR_OUTPUT_RES_HORZ_INCH           ( 26 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_OUTPUT_RES_VERT_INCH           "orvi"
#define VTE_TERMINFO_VAR_OUTPUT_RES_VERT_INCH           ( 27 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_PRINT_RATE                     "cps"
#define VTE_TERMINFO_VAR_PRINT_RATE                     ( 28 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_WIDE_CHAR_SIZE                 "widcs"
#define VTE_TERMINFO_VAR_WIDE_CHAR_SIZE                 ( 29 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_BUTTONS                        "btns"
#define VTE_TERMINFO_VAR_BUTTONS                        ( 30 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_BIT_IMAGE_ENTWINING            "bitwin"
#define VTE_TERMINFO_VAR_BIT_IMAGE_ENTWINING            ( 31 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_BIT_IMAGE_TYPE                 "bitype"
#define VTE_TERMINFO_VAR_BIT_IMAGE_TYPE                 ( 32 | VTE_TERMINFO_VARTYPE_NUMERIC)
#define VTE_TERMINFO_CAP_BACK_TAB                       "cbt"
#define VTE_TERMINFO_VAR_BACK_TAB                       (  0 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_BELL                           "bel"
#define VTE_TERMINFO_VAR_BELL                           (  1 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CARRIAGE_RETURN                "cr"
#define VTE_TERMINFO_VAR_CARRIAGE_RETURN                (  2 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHANGE_SCROLL_REGION           "csr"
#define VTE_TERMINFO_VAR_CHANGE_SCROLL_REGION           (  3 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLEAR_ALL_TABS                 "tbc"
#define VTE_TERMINFO_VAR_CLEAR_ALL_TABS                 (  4 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLEAR_SCREEN                   "clear"
#define VTE_TERMINFO_VAR_CLEAR_SCREEN                   (  5 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLR_EOL                        "el"
#define VTE_TERMINFO_VAR_CLR_EOL                        (  6 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLR_EOS                        "ed"
#define VTE_TERMINFO_VAR_CLR_EOS                        (  7 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_COLUMN_ADDRESS                 "hpa"
#define VTE_TERMINFO_VAR_COLUMN_ADDRESS                 (  8 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_COMMAND_CHARACTER              "cmdch"
#define VTE_TERMINFO_VAR_COMMAND_CHARACTER              (  9 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_ADDRESS                 "cup"
#define VTE_TERMINFO_VAR_CURSOR_ADDRESS                 ( 10 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_DOWN                    "cud1"
#define VTE_TERMINFO_VAR_CURSOR_DOWN                    ( 11 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_HOME                    "home"
#define VTE_TERMINFO_VAR_CURSOR_HOME                    ( 12 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_INVISIBLE               "civis"
#define VTE_TERMINFO_VAR_CURSOR_INVISIBLE               ( 13 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_LEFT                    "cub1"
#define VTE_TERMINFO_VAR_CURSOR_LEFT                    ( 14 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_MEM_ADDRESS             "mrcup"
#define VTE_TERMINFO_VAR_CURSOR_MEM_ADDRESS             ( 15 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_NORMAL                  "cnorm"
#define VTE_TERMINFO_VAR_CURSOR_NORMAL                  ( 16 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_RIGHT                   "cuf1"
#define VTE_TERMINFO_VAR_CURSOR_RIGHT                   ( 17 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_TO_LL                   "ll"
#define VTE_TERMINFO_VAR_CURSOR_TO_LL                   ( 18 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_UP                      "cuu1"
#define VTE_TERMINFO_VAR_CURSOR_UP                      ( 19 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CURSOR_VISIBLE                 "cvvis"
#define VTE_TERMINFO_VAR_CURSOR_VISIBLE                 ( 20 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DELETE_CHARACTER               "dch1"
#define VTE_TERMINFO_VAR_DELETE_CHARACTER               ( 21 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DELETE_LINE                    "dl1"
#define VTE_TERMINFO_VAR_DELETE_LINE                    ( 22 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DIS_STATUS_LINE                "dsl"
#define VTE_TERMINFO_VAR_DIS_STATUS_LINE                ( 23 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DOWN_HALF_LINE                 "hd"
#define VTE_TERMINFO_VAR_DOWN_HALF_LINE                 ( 24 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_ALT_CHARSET_MODE         "smacs"
#define VTE_TERMINFO_VAR_ENTER_ALT_CHARSET_MODE         ( 25 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_BLINK_MODE               "blink"
#define VTE_TERMINFO_VAR_ENTER_BLINK_MODE               ( 26 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_BOLD_MODE                "bold"
#define VTE_TERMINFO_VAR_ENTER_BOLD_MODE                ( 27 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_CA_MODE                  "smcup"
#define VTE_TERMINFO_VAR_ENTER_CA_MODE                  ( 28 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_DELETE_MODE              "smdc"
#define VTE_TERMINFO_VAR_ENTER_DELETE_MODE              ( 29 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_DIM_MODE                 "dim"
#define VTE_TERMINFO_VAR_ENTER_DIM_MODE                 ( 30 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_INSERT_MODE              "smir"
#define VTE_TERMINFO_VAR_ENTER_INSERT_MODE              ( 31 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_SECURE_MODE              "invis"
#define VTE_TERMINFO_VAR_ENTER_SECURE_MODE              ( 32 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_PROTECTED_MODE           "prot"
#define VTE_TERMINFO_VAR_ENTER_PROTECTED_MODE           ( 33 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_REVERSE_MODE             "rev"
#define VTE_TERMINFO_VAR_ENTER_REVERSE_MODE             ( 34 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_STANDOUT_MODE            "smso"
#define VTE_TERMINFO_VAR_ENTER_STANDOUT_MODE            ( 35 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_UNDERLINE_MODE           "smul"
#define VTE_TERMINFO_VAR_ENTER_UNDERLINE_MODE           ( 36 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ERASE_CHARS                    "ech"
#define VTE_TERMINFO_VAR_ERASE_CHARS                    ( 37 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_ALT_CHARSET_MODE          "rmacs"
#define VTE_TERMINFO_VAR_EXIT_ALT_CHARSET_MODE          ( 38 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_ATTRIBUTE_MODE            "sgr0"
#define VTE_TERMINFO_VAR_EXIT_ATTRIBUTE_MODE            ( 39 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_CA_MODE                   "rmcup"
#define VTE_TERMINFO_VAR_EXIT_CA_MODE                   ( 40 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_DELETE_MODE               "rmdc"
#define VTE_TERMINFO_VAR_EXIT_DELETE_MODE               ( 41 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_INSERT_MODE               "rmir"
#define VTE_TERMINFO_VAR_EXIT_INSERT_MODE               ( 42 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_STANDOUT_MODE             "rmso"
#define VTE_TERMINFO_VAR_EXIT_STANDOUT_MODE             ( 43 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_UNDERLINE_MODE            "rmul"
#define VTE_TERMINFO_VAR_EXIT_UNDERLINE_MODE            ( 44 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_FLASH_SCREEN                   "flash"
#define VTE_TERMINFO_VAR_FLASH_SCREEN                   ( 45 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_FORM_FEED                      "ff"
#define VTE_TERMINFO_VAR_FORM_FEED                      ( 46 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_FROM_STATUS_LINE               "fsl"
#define VTE_TERMINFO_VAR_FROM_STATUS_LINE               ( 47 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INIT_1STRING                   "is1"
#define VTE_TERMINFO_VAR_INIT_1STRING                   ( 48 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INIT_2STRING                   "is2"
#define VTE_TERMINFO_VAR_INIT_2STRING                   ( 49 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INIT_3STRING                   "is3"
#define VTE_TERMINFO_VAR_INIT_3STRING                   ( 50 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INIT_FILE                      "if"
#define VTE_TERMINFO_VAR_INIT_FILE                      ( 51 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INSERT_CHARACTER               "ich1"
#define VTE_TERMINFO_VAR_INSERT_CHARACTER               ( 52 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INSERT_LINE                    "il1"
#define VTE_TERMINFO_VAR_INSERT_LINE                    ( 53 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INSERT_PADDING                 "ip"
#define VTE_TERMINFO_VAR_INSERT_PADDING                 ( 54 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_BACKSPACE                  "kbs"
#define VTE_TERMINFO_VAR_KEY_BACKSPACE                  ( 55 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CATAB                      "ktbc"
#define VTE_TERMINFO_VAR_KEY_CATAB                      ( 56 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CLEAR                      "kclr"
#define VTE_TERMINFO_VAR_KEY_CLEAR                      ( 57 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CTAB                       "kctab"
#define VTE_TERMINFO_VAR_KEY_CTAB                       ( 58 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_DC                         "kdch1"
#define VTE_TERMINFO_VAR_KEY_DC                         ( 59 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_DL                         "kdl1"
#define VTE_TERMINFO_VAR_KEY_DL                         ( 60 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_DOWN                       "kcud1"
#define VTE_TERMINFO_VAR_KEY_DOWN                       ( 61 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_EIC                        "krmir"
#define VTE_TERMINFO_VAR_KEY_EIC                        ( 62 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_EOL                        "kel"
#define VTE_TERMINFO_VAR_KEY_EOL                        ( 63 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_EOS                        "ked"
#define VTE_TERMINFO_VAR_KEY_EOS                        ( 64 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F0                         "kf0"
#define VTE_TERMINFO_VAR_KEY_F0                         ( 65 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F1                         "kf1"
#define VTE_TERMINFO_VAR_KEY_F1                         ( 66 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F10                        "kf10"
#define VTE_TERMINFO_VAR_KEY_F10                        ( 67 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F2                         "kf2"
#define VTE_TERMINFO_VAR_KEY_F2                         ( 68 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F3                         "kf3"
#define VTE_TERMINFO_VAR_KEY_F3                         ( 69 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F4                         "kf4"
#define VTE_TERMINFO_VAR_KEY_F4                         ( 70 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F5                         "kf5"
#define VTE_TERMINFO_VAR_KEY_F5                         ( 71 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F6                         "kf6"
#define VTE_TERMINFO_VAR_KEY_F6                         ( 72 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F7                         "kf7"
#define VTE_TERMINFO_VAR_KEY_F7                         ( 73 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F8                         "kf8"
#define VTE_TERMINFO_VAR_KEY_F8                         ( 74 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F9                         "kf9"
#define VTE_TERMINFO_VAR_KEY_F9                         ( 75 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_HOME                       "khome"
#define VTE_TERMINFO_VAR_KEY_HOME                       ( 76 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_IC                         "kich1"
#define VTE_TERMINFO_VAR_KEY_IC                         ( 77 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_IL                         "kil1"
#define VTE_TERMINFO_VAR_KEY_IL                         ( 78 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_LEFT                       "kcub1"
#define VTE_TERMINFO_VAR_KEY_LEFT                       ( 79 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_LL                         "kll"
#define VTE_TERMINFO_VAR_KEY_LL                         ( 80 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_NPAGE                      "knp"
#define VTE_TERMINFO_VAR_KEY_NPAGE                      ( 81 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_PPAGE                      "kpp"
#define VTE_TERMINFO_VAR_KEY_PPAGE                      ( 82 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_RIGHT                      "kcuf1"
#define VTE_TERMINFO_VAR_KEY_RIGHT                      ( 83 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SF                         "kind"
#define VTE_TERMINFO_VAR_KEY_SF                         ( 84 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SR                         "kri"
#define VTE_TERMINFO_VAR_KEY_SR                         ( 85 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_STAB                       "khts"
#define VTE_TERMINFO_VAR_KEY_STAB                       ( 86 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_UP                         "kcuu1"
#define VTE_TERMINFO_VAR_KEY_UP                         ( 87 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEYPAD_LOCAL                   "rmkx"
#define VTE_TERMINFO_VAR_KEYPAD_LOCAL                   ( 88 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEYPAD_XMIT                    "smkx"
#define VTE_TERMINFO_VAR_KEYPAD_XMIT                    ( 89 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F0                         "lf0"
#define VTE_TERMINFO_VAR_LAB_F0                         ( 90 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F1                         "lf1"
#define VTE_TERMINFO_VAR_LAB_F1                         ( 91 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F10                        "lf10"
#define VTE_TERMINFO_VAR_LAB_F10                        ( 92 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F2                         "lf2"
#define VTE_TERMINFO_VAR_LAB_F2                         ( 93 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F3                         "lf3"
#define VTE_TERMINFO_VAR_LAB_F3                         ( 94 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F4                         "lf4"
#define VTE_TERMINFO_VAR_LAB_F4                         ( 95 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F5                         "lf5"
#define VTE_TERMINFO_VAR_LAB_F5                         ( 96 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F6                         "lf6"
#define VTE_TERMINFO_VAR_LAB_F6                         ( 97 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F7                         "lf7"
#define VTE_TERMINFO_VAR_LAB_F7                         ( 98 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F8                         "lf8"
#define VTE_TERMINFO_VAR_LAB_F8                         ( 99 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LAB_F9                         "lf9"
#define VTE_TERMINFO_VAR_LAB_F9                         (100 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_META_OFF                       "rmm"
#define VTE_TERMINFO_VAR_META_OFF                       (101 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_META_ON                        "smm"
#define VTE_TERMINFO_VAR_META_ON                        (102 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_NEWLINE                        "nel"
#define VTE_TERMINFO_VAR_NEWLINE                        (103 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PAD_CHAR                       "pad"
#define VTE_TERMINFO_VAR_PAD_CHAR                       (104 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_DCH                       "dch"
#define VTE_TERMINFO_VAR_PARM_DCH                       (105 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_DELETE_LINE               "dl"
#define VTE_TERMINFO_VAR_PARM_DELETE_LINE               (106 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_DOWN_CURSOR               "cud"
#define VTE_TERMINFO_VAR_PARM_DOWN_CURSOR               (107 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_ICH                       "ich"
#define VTE_TERMINFO_VAR_PARM_ICH                       (108 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_INDEX                     "indn"
#define VTE_TERMINFO_VAR_PARM_INDEX                     (109 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_INSERT_LINE               "il"
#define VTE_TERMINFO_VAR_PARM_INSERT_LINE               (110 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_LEFT_CURSOR               "cub"
#define VTE_TERMINFO_VAR_PARM_LEFT_CURSOR               (111 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_RIGHT_CURSOR              "cuf"
#define VTE_TERMINFO_VAR_PARM_RIGHT_CURSOR              (112 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_RINDEX                    "rin"
#define VTE_TERMINFO_VAR_PARM_RINDEX                    (113 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_UP_CURSOR                 "cuu"
#define VTE_TERMINFO_VAR_PARM_UP_CURSOR                 (114 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PKEY_KEY                       "pfkey"
#define VTE_TERMINFO_VAR_PKEY_KEY                       (115 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PKEY_LOCAL                     "pfloc"
#define VTE_TERMINFO_VAR_PKEY_LOCAL                     (116 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PKEY_XMIT                      "pfx"
#define VTE_TERMINFO_VAR_PKEY_XMIT                      (117 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PRINT_SCREEN                   "mc0"
#define VTE_TERMINFO_VAR_PRINT_SCREEN                   (118 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PRTR_OFF                       "mc4"
#define VTE_TERMINFO_VAR_PRTR_OFF                       (119 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PRTR_ON                        "mc5"
#define VTE_TERMINFO_VAR_PRTR_ON                        (120 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_REPEAT_CHAR                    "rep"
#define VTE_TERMINFO_VAR_REPEAT_CHAR                    (121 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_RESET_1STRING                  "rs1"
#define VTE_TERMINFO_VAR_RESET_1STRING                  (122 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_RESET_2STRING                  "rs2"
#define VTE_TERMINFO_VAR_RESET_2STRING                  (123 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_RESET_3STRING                  "rs3"
#define VTE_TERMINFO_VAR_RESET_3STRING                  (124 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_RESET_FILE                     "rf"
#define VTE_TERMINFO_VAR_RESET_FILE                     (125 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_RESTORE_CURSOR                 "rc"
#define VTE_TERMINFO_VAR_RESTORE_CURSOR                 (126 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ROW_ADDRESS                    "vpa"
#define VTE_TERMINFO_VAR_ROW_ADDRESS                    (127 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SAVE_CURSOR                    "sc"
#define VTE_TERMINFO_VAR_SAVE_CURSOR                    (128 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SCROLL_FORWARD                 "ind"
#define VTE_TERMINFO_VAR_SCROLL_FORWARD                 (129 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SCROLL_REVERSE                 "ri"
#define VTE_TERMINFO_VAR_SCROLL_REVERSE                 (130 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_ATTRIBUTES                 "sgr"
#define VTE_TERMINFO_VAR_SET_ATTRIBUTES                 (131 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_TAB                        "hts"
#define VTE_TERMINFO_VAR_SET_TAB                        (132 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_WINDOW                     "wind"
#define VTE_TERMINFO_VAR_SET_WINDOW                     (133 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_TAB                            "ht"
#define VTE_TERMINFO_VAR_TAB                            (134 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_TO_STATUS_LINE                 "tsl"
#define VTE_TERMINFO_VAR_TO_STATUS_LINE                 (135 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_UNDERLINE_CHAR                 "uc"
#define VTE_TERMINFO_VAR_UNDERLINE_CHAR                 (136 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_UP_HALF_LINE                   "hu"
#define VTE_TERMINFO_VAR_UP_HALF_LINE                   (137 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INIT_PROG                      "iprog"
#define VTE_TERMINFO_VAR_INIT_PROG                      (138 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_A1                         "ka1"
#define VTE_TERMINFO_VAR_KEY_A1                         (139 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_A3                         "ka3"
#define VTE_TERMINFO_VAR_KEY_A3                         (140 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_B2                         "kb2"
#define VTE_TERMINFO_VAR_KEY_B2                         (141 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_C1                         "kc1"
#define VTE_TERMINFO_VAR_KEY_C1                         (142 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_C3                         "kc3"
#define VTE_TERMINFO_VAR_KEY_C3                         (143 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PRTR_NON                       "mc5p"
#define VTE_TERMINFO_VAR_PRTR_NON                       (144 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHAR_PADDING                   "rmp"
#define VTE_TERMINFO_VAR_CHAR_PADDING                   (145 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ACS_CHARS                      "acsc"
#define VTE_TERMINFO_VAR_ACS_CHARS                      (146 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PLAB_NORM                      "pln"
#define VTE_TERMINFO_VAR_PLAB_NORM                      (147 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_BTAB                       "kcbt"
#define VTE_TERMINFO_VAR_KEY_BTAB                       (148 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_XON_MODE                 "smxon"
#define VTE_TERMINFO_VAR_ENTER_XON_MODE                 (149 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_XON_MODE                  "rmxon"
#define VTE_TERMINFO_VAR_EXIT_XON_MODE                  (150 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_AM_MODE                  "smam"
#define VTE_TERMINFO_VAR_ENTER_AM_MODE                  (151 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_AM_MODE                   "rmam"
#define VTE_TERMINFO_VAR_EXIT_AM_MODE                   (152 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_XON_CHARACTER                  "xonc"
#define VTE_TERMINFO_VAR_XON_CHARACTER                  (153 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_XOFF_CHARACTER                 "xoffc"
#define VTE_TERMINFO_VAR_XOFF_CHARACTER                 (154 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENA_ACS                        "enacs"
#define VTE_TERMINFO_VAR_ENA_ACS                        (155 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LABEL_ON                       "smln"
#define VTE_TERMINFO_VAR_LABEL_ON                       (156 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LABEL_OFF                      "rmln"
#define VTE_TERMINFO_VAR_LABEL_OFF                      (157 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_BEG                        "kbeg"
#define VTE_TERMINFO_VAR_KEY_BEG                        (158 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CANCEL                     "kcan"
#define VTE_TERMINFO_VAR_KEY_CANCEL                     (159 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CLOSE                      "kclo"
#define VTE_TERMINFO_VAR_KEY_CLOSE                      (160 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_COMMAND                    "kcmd"
#define VTE_TERMINFO_VAR_KEY_COMMAND                    (161 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_COPY                       "kcpy"
#define VTE_TERMINFO_VAR_KEY_COPY                       (162 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_CREATE                     "kcrt"
#define VTE_TERMINFO_VAR_KEY_CREATE                     (163 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_END                        "kend"
#define VTE_TERMINFO_VAR_KEY_END                        (164 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_ENTER                      "kent"
#define VTE_TERMINFO_VAR_KEY_ENTER                      (165 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_EXIT                       "kext"
#define VTE_TERMINFO_VAR_KEY_EXIT                       (166 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_FIND                       "kfnd"
#define VTE_TERMINFO_VAR_KEY_FIND                       (167 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_HELP                       "khlp"
#define VTE_TERMINFO_VAR_KEY_HELP                       (168 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_MARK                       "kmrk"
#define VTE_TERMINFO_VAR_KEY_MARK                       (169 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_MESSAGE                    "kmsg"
#define VTE_TERMINFO_VAR_KEY_MESSAGE                    (170 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_MOVE                       "kmov"
#define VTE_TERMINFO_VAR_KEY_MOVE                       (171 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_NEXT                       "knxt"
#define VTE_TERMINFO_VAR_KEY_NEXT                       (172 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_OPEN                       "kopn"
#define VTE_TERMINFO_VAR_KEY_OPEN                       (173 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_OPTIONS                    "kopt"
#define VTE_TERMINFO_VAR_KEY_OPTIONS                    (174 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_PREVIOUS                   "kprv"
#define VTE_TERMINFO_VAR_KEY_PREVIOUS                   (175 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_PRINT                      "kprt"
#define VTE_TERMINFO_VAR_KEY_PRINT                      (176 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_REDO                       "krdo"
#define VTE_TERMINFO_VAR_KEY_REDO                       (177 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_REFERENCE                  "kref"
#define VTE_TERMINFO_VAR_KEY_REFERENCE                  (178 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_REFRESH                    "krfr"
#define VTE_TERMINFO_VAR_KEY_REFRESH                    (179 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_REPLACE                    "krpl"
#define VTE_TERMINFO_VAR_KEY_REPLACE                    (180 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_RESTART                    "krst"
#define VTE_TERMINFO_VAR_KEY_RESTART                    (181 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_RESUME                     "kres"
#define VTE_TERMINFO_VAR_KEY_RESUME                     (182 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SAVE                       "ksav"
#define VTE_TERMINFO_VAR_KEY_SAVE                       (183 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SUSPEND                    "kspd"
#define VTE_TERMINFO_VAR_KEY_SUSPEND                    (184 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_UNDO                       "kund"
#define VTE_TERMINFO_VAR_KEY_UNDO                       (185 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SBEG                       "kBEG"
#define VTE_TERMINFO_VAR_KEY_SBEG                       (186 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SCANCEL                    "kCAN"
#define VTE_TERMINFO_VAR_KEY_SCANCEL                    (187 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SCOMMAND                   "kCMD"
#define VTE_TERMINFO_VAR_KEY_SCOMMAND                   (188 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SCOPY                      "kCPY"
#define VTE_TERMINFO_VAR_KEY_SCOPY                      (189 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SCREATE                    "kCRT"
#define VTE_TERMINFO_VAR_KEY_SCREATE                    (190 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SDC                        "kDC"
#define VTE_TERMINFO_VAR_KEY_SDC                        (191 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SDL                        "kDL"
#define VTE_TERMINFO_VAR_KEY_SDL                        (192 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SELECT                     "kslt"
#define VTE_TERMINFO_VAR_KEY_SELECT                     (193 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SEND                       "kEND"
#define VTE_TERMINFO_VAR_KEY_SEND                       (194 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SEOL                       "kEOL"
#define VTE_TERMINFO_VAR_KEY_SEOL                       (195 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SEXIT                      "kEXT"
#define VTE_TERMINFO_VAR_KEY_SEXIT                      (196 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SFIND                      "kFND"
#define VTE_TERMINFO_VAR_KEY_SFIND                      (197 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SHELP                      "kHLP"
#define VTE_TERMINFO_VAR_KEY_SHELP                      (198 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SHOME                      "kHOM"
#define VTE_TERMINFO_VAR_KEY_SHOME                      (199 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SIC                        "kIC"
#define VTE_TERMINFO_VAR_KEY_SIC                        (200 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SLEFT                      "kLFT"
#define VTE_TERMINFO_VAR_KEY_SLEFT                      (201 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SMESSAGE                   "kMSG"
#define VTE_TERMINFO_VAR_KEY_SMESSAGE                   (202 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SMOVE                      "kMOV"
#define VTE_TERMINFO_VAR_KEY_SMOVE                      (203 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SNEXT                      "kNXT"
#define VTE_TERMINFO_VAR_KEY_SNEXT                      (204 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SOPTIONS                   "kOPT"
#define VTE_TERMINFO_VAR_KEY_SOPTIONS                   (205 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SPREVIOUS                  "kPRV"
#define VTE_TERMINFO_VAR_KEY_SPREVIOUS                  (206 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SPRINT                     "kPRT"
#define VTE_TERMINFO_VAR_KEY_SPRINT                     (207 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SREDO                      "kRDO"
#define VTE_TERMINFO_VAR_KEY_SREDO                      (208 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SREPLACE                   "kRPL"
#define VTE_TERMINFO_VAR_KEY_SREPLACE                   (209 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SRIGHT                     "kRIT"
#define VTE_TERMINFO_VAR_KEY_SRIGHT                     (210 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SRSUME                     "kRES"
#define VTE_TERMINFO_VAR_KEY_SRSUME                     (211 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SSAVE                      "kSAV"
#define VTE_TERMINFO_VAR_KEY_SSAVE                      (212 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SSUSPEND                   "kSPD"
#define VTE_TERMINFO_VAR_KEY_SSUSPEND                   (213 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_SUNDO                      "kUND"
#define VTE_TERMINFO_VAR_KEY_SUNDO                      (214 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_REQ_FOR_INPUT                  "rfi"
#define VTE_TERMINFO_VAR_REQ_FOR_INPUT                  (215 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F11                        "kf11"
#define VTE_TERMINFO_VAR_KEY_F11                        (216 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F12                        "kf12"
#define VTE_TERMINFO_VAR_KEY_F12                        (217 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F13                        "kf13"
#define VTE_TERMINFO_VAR_KEY_F13                        (218 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F14                        "kf14"
#define VTE_TERMINFO_VAR_KEY_F14                        (219 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F15                        "kf15"
#define VTE_TERMINFO_VAR_KEY_F15                        (220 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F16                        "kf16"
#define VTE_TERMINFO_VAR_KEY_F16                        (221 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F17                        "kf17"
#define VTE_TERMINFO_VAR_KEY_F17                        (222 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F18                        "kf18"
#define VTE_TERMINFO_VAR_KEY_F18                        (223 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F19                        "kf19"
#define VTE_TERMINFO_VAR_KEY_F19                        (224 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F20                        "kf20"
#define VTE_TERMINFO_VAR_KEY_F20                        (225 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F21                        "kf21"
#define VTE_TERMINFO_VAR_KEY_F21                        (226 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F22                        "kf22"
#define VTE_TERMINFO_VAR_KEY_F22                        (227 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F23                        "kf23"
#define VTE_TERMINFO_VAR_KEY_F23                        (228 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F24                        "kf24"
#define VTE_TERMINFO_VAR_KEY_F24                        (229 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F25                        "kf25"
#define VTE_TERMINFO_VAR_KEY_F25                        (230 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F26                        "kf26"
#define VTE_TERMINFO_VAR_KEY_F26                        (231 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F27                        "kf27"
#define VTE_TERMINFO_VAR_KEY_F27                        (232 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F28                        "kf28"
#define VTE_TERMINFO_VAR_KEY_F28                        (233 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F29                        "kf29"
#define VTE_TERMINFO_VAR_KEY_F29                        (234 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F30                        "kf30"
#define VTE_TERMINFO_VAR_KEY_F30                        (235 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F31                        "kf31"
#define VTE_TERMINFO_VAR_KEY_F31                        (236 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F32                        "kf32"
#define VTE_TERMINFO_VAR_KEY_F32                        (237 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F33                        "kf33"
#define VTE_TERMINFO_VAR_KEY_F33                        (238 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F34                        "kf34"
#define VTE_TERMINFO_VAR_KEY_F34                        (239 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F35                        "kf35"
#define VTE_TERMINFO_VAR_KEY_F35                        (240 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F36                        "kf36"
#define VTE_TERMINFO_VAR_KEY_F36                        (241 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F37                        "kf37"
#define VTE_TERMINFO_VAR_KEY_F37                        (242 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F38                        "kf38"
#define VTE_TERMINFO_VAR_KEY_F38                        (243 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F39                        "kf39"
#define VTE_TERMINFO_VAR_KEY_F39                        (244 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F40                        "kf40"
#define VTE_TERMINFO_VAR_KEY_F40                        (245 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F41                        "kf41"
#define VTE_TERMINFO_VAR_KEY_F41                        (246 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F42                        "kf42"
#define VTE_TERMINFO_VAR_KEY_F42                        (247 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F43                        "kf43"
#define VTE_TERMINFO_VAR_KEY_F43                        (248 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F44                        "kf44"
#define VTE_TERMINFO_VAR_KEY_F44                        (249 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F45                        "kf45"
#define VTE_TERMINFO_VAR_KEY_F45                        (250 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F46                        "kf46"
#define VTE_TERMINFO_VAR_KEY_F46                        (251 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F47                        "kf47"
#define VTE_TERMINFO_VAR_KEY_F47                        (252 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F48                        "kf48"
#define VTE_TERMINFO_VAR_KEY_F48                        (253 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F49                        "kf49"
#define VTE_TERMINFO_VAR_KEY_F49                        (254 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F50                        "kf50"
#define VTE_TERMINFO_VAR_KEY_F50                        (255 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F51                        "kf51"
#define VTE_TERMINFO_VAR_KEY_F51                        (256 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F52                        "kf52"
#define VTE_TERMINFO_VAR_KEY_F52                        (257 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F53                        "kf53"
#define VTE_TERMINFO_VAR_KEY_F53                        (258 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F54                        "kf54"
#define VTE_TERMINFO_VAR_KEY_F54                        (259 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F55                        "kf55"
#define VTE_TERMINFO_VAR_KEY_F55                        (260 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F56                        "kf56"
#define VTE_TERMINFO_VAR_KEY_F56                        (261 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F57                        "kf57"
#define VTE_TERMINFO_VAR_KEY_F57                        (262 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F58                        "kf58"
#define VTE_TERMINFO_VAR_KEY_F58                        (263 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F59                        "kf59"
#define VTE_TERMINFO_VAR_KEY_F59                        (264 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F60                        "kf60"
#define VTE_TERMINFO_VAR_KEY_F60                        (265 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F61                        "kf61"
#define VTE_TERMINFO_VAR_KEY_F61                        (266 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F62                        "kf62"
#define VTE_TERMINFO_VAR_KEY_F62                        (267 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_F63                        "kf63"
#define VTE_TERMINFO_VAR_KEY_F63                        (268 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLR_BOL                        "el1"
#define VTE_TERMINFO_VAR_CLR_BOL                        (269 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CLEAR_MARGINS                  "mgc"
#define VTE_TERMINFO_VAR_CLEAR_MARGINS                  (270 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_LEFT_MARGIN                "smgl"
#define VTE_TERMINFO_VAR_SET_LEFT_MARGIN                (271 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_RIGHT_MARGIN               "smgr"
#define VTE_TERMINFO_VAR_SET_RIGHT_MARGIN               (272 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_LABEL_FORMAT                   "fln"
#define VTE_TERMINFO_VAR_LABEL_FORMAT                   (273 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_CLOCK                      "sclk"
#define VTE_TERMINFO_VAR_SET_CLOCK                      (274 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DISPLAY_CLOCK                  "dclk"
#define VTE_TERMINFO_VAR_DISPLAY_CLOCK                  (275 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_REMOVE_CLOCK                   "rmclk"
#define VTE_TERMINFO_VAR_REMOVE_CLOCK                   (276 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CREATE_WINDOW                  "cwin"
#define VTE_TERMINFO_VAR_CREATE_WINDOW                  (277 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_GOTO_WINDOW                    "wingo"
#define VTE_TERMINFO_VAR_GOTO_WINDOW                    (278 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_HANGUP                         "hup"
#define VTE_TERMINFO_VAR_HANGUP                         (279 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DIAL_PHONE                     "dial"
#define VTE_TERMINFO_VAR_DIAL_PHONE                     (280 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_QUICK_DIAL                     "qdial"
#define VTE_TERMINFO_VAR_QUICK_DIAL                     (281 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_TONE                           "tone"
#define VTE_TERMINFO_VAR_TONE                           (282 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PULSE                          "pulse"
#define VTE_TERMINFO_VAR_PULSE                          (283 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_FLASH_HOOK                     "hook"
#define VTE_TERMINFO_VAR_FLASH_HOOK                     (284 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_FIXED_PAUSE                    "pause"
#define VTE_TERMINFO_VAR_FIXED_PAUSE                    (285 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_WAIT_TONE                      "wait"
#define VTE_TERMINFO_VAR_WAIT_TONE                      (286 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER0                          "u0"
#define VTE_TERMINFO_VAR_USER0                          (287 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER1                          "u1"
#define VTE_TERMINFO_VAR_USER1                          (288 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER2                          "u2"
#define VTE_TERMINFO_VAR_USER2                          (289 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER3                          "u3"
#define VTE_TERMINFO_VAR_USER3                          (290 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER4                          "u4"
#define VTE_TERMINFO_VAR_USER4                          (291 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER5                          "u5"
#define VTE_TERMINFO_VAR_USER5                          (292 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER6                          "u6"
#define VTE_TERMINFO_VAR_USER6                          (293 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER7                          "u7"
#define VTE_TERMINFO_VAR_USER7                          (294 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER8                          "u8"
#define VTE_TERMINFO_VAR_USER8                          (295 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_USER9                          "u9"
#define VTE_TERMINFO_VAR_USER9                          (296 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ORIG_PAIR                      "op"
#define VTE_TERMINFO_VAR_ORIG_PAIR                      (297 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ORIG_COLORS                    "oc"
#define VTE_TERMINFO_VAR_ORIG_COLORS                    (298 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INITIALIZE_COLOR               "initc"
#define VTE_TERMINFO_VAR_INITIALIZE_COLOR               (299 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_INITIALIZE_PAIR                "initp"
#define VTE_TERMINFO_VAR_INITIALIZE_PAIR                (300 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_COLOR_PAIR                 "scp"
#define VTE_TERMINFO_VAR_SET_COLOR_PAIR                 (301 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_FOREGROUND                 "setf"
#define VTE_TERMINFO_VAR_SET_FOREGROUND                 (302 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_BACKGROUND                 "setb"
#define VTE_TERMINFO_VAR_SET_BACKGROUND                 (303 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHANGE_CHAR_PITCH              "cpi"
#define VTE_TERMINFO_VAR_CHANGE_CHAR_PITCH              (304 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHANGE_LINE_PITCH              "lpi"
#define VTE_TERMINFO_VAR_CHANGE_LINE_PITCH              (305 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHANGE_RES_HORZ                "chr"
#define VTE_TERMINFO_VAR_CHANGE_RES_HORZ                (306 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHANGE_RES_VERT                "cvr"
#define VTE_TERMINFO_VAR_CHANGE_RES_VERT                (307 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DEFINE_CHAR                    "defc"
#define VTE_TERMINFO_VAR_DEFINE_CHAR                    (308 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_DOUBLEWIDE_MODE          "swidm"
#define VTE_TERMINFO_VAR_ENTER_DOUBLEWIDE_MODE          (309 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_DRAFT_QUALITY            "sdrfq"
#define VTE_TERMINFO_VAR_ENTER_DRAFT_QUALITY            (310 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_ITALICS_MODE             "sitm"
#define VTE_TERMINFO_VAR_ENTER_ITALICS_MODE             (311 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_LEFTWARD_MODE            "slm"
#define VTE_TERMINFO_VAR_ENTER_LEFTWARD_MODE            (312 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_MICRO_MODE               "smicm"
#define VTE_TERMINFO_VAR_ENTER_MICRO_MODE               (313 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_NEAR_LETTER_QUALITY      "snlq"
#define VTE_TERMINFO_VAR_ENTER_NEAR_LETTER_QUALITY      (314 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_NORMAL_QUALITY           "snrmq"
#define VTE_TERMINFO_VAR_ENTER_NORMAL_QUALITY           (315 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_SHADOW_MODE              "sshm"
#define VTE_TERMINFO_VAR_ENTER_SHADOW_MODE              (316 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_SUBSCRIPT_MODE           "ssubm"
#define VTE_TERMINFO_VAR_ENTER_SUBSCRIPT_MODE           (317 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_SUPERSCRIPT_MODE         "ssupm"
#define VTE_TERMINFO_VAR_ENTER_SUPERSCRIPT_MODE         (318 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_UPWARD_MODE              "sum"
#define VTE_TERMINFO_VAR_ENTER_UPWARD_MODE              (319 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_DOUBLEWIDE_MODE           "rwidm"
#define VTE_TERMINFO_VAR_EXIT_DOUBLEWIDE_MODE           (320 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_ITALICS_MODE              "ritm"
#define VTE_TERMINFO_VAR_EXIT_ITALICS_MODE              (321 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_LEFTWARD_MODE             "rlm"
#define VTE_TERMINFO_VAR_EXIT_LEFTWARD_MODE             (322 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_MICRO_MODE                "rmicm"
#define VTE_TERMINFO_VAR_EXIT_MICRO_MODE                (323 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_SHADOW_MODE               "rshm"
#define VTE_TERMINFO_VAR_EXIT_SHADOW_MODE               (324 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_SUBSCRIPT_MODE            "rsubm"
#define VTE_TERMINFO_VAR_EXIT_SUBSCRIPT_MODE            (325 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_SUPERSCRIPT_MODE          "rsupm"
#define VTE_TERMINFO_VAR_EXIT_SUPERSCRIPT_MODE          (326 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_UPWARD_MODE               "rum"
#define VTE_TERMINFO_VAR_EXIT_UPWARD_MODE               (327 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_COLUMN_ADDRESS           "mhpa"
#define VTE_TERMINFO_VAR_MICRO_COLUMN_ADDRESS           (328 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_DOWN                     "mcud1"
#define VTE_TERMINFO_VAR_MICRO_DOWN                     (329 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_LEFT                     "mcub1"
#define VTE_TERMINFO_VAR_MICRO_LEFT                     (330 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_RIGHT                    "mcuf1"
#define VTE_TERMINFO_VAR_MICRO_RIGHT                    (331 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_ROW_ADDRESS              "mvpa"
#define VTE_TERMINFO_VAR_MICRO_ROW_ADDRESS              (332 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MICRO_UP                       "mcuu1"
#define VTE_TERMINFO_VAR_MICRO_UP                       (333 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ORDER_OF_PINS                  "porder"
#define VTE_TERMINFO_VAR_ORDER_OF_PINS                  (334 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_DOWN_MICRO                "mcud"
#define VTE_TERMINFO_VAR_PARM_DOWN_MICRO                (335 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_LEFT_MICRO                "mcub"
#define VTE_TERMINFO_VAR_PARM_LEFT_MICRO                (336 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_RIGHT_MICRO               "mcuf"
#define VTE_TERMINFO_VAR_PARM_RIGHT_MICRO               (337 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PARM_UP_MICRO                  "mcuu"
#define VTE_TERMINFO_VAR_PARM_UP_MICRO                  (338 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SELECT_CHAR_SET                "scs"
#define VTE_TERMINFO_VAR_SELECT_CHAR_SET                (339 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_BOTTOM_MARGIN              "smgb"
#define VTE_TERMINFO_VAR_SET_BOTTOM_MARGIN              (340 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_BOTTOM_MARGIN_PARM         "smgbp"
#define VTE_TERMINFO_VAR_SET_BOTTOM_MARGIN_PARM         (341 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_LEFT_MARGIN_PARM           "smglp"
#define VTE_TERMINFO_VAR_SET_LEFT_MARGIN_PARM           (342 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_RIGHT_MARGIN_PARM          "smgrp"
#define VTE_TERMINFO_VAR_SET_RIGHT_MARGIN_PARM          (343 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_TOP_MARGIN                 "smgt"
#define VTE_TERMINFO_VAR_SET_TOP_MARGIN                 (344 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_TOP_MARGIN_PARM            "smgtp"
#define VTE_TERMINFO_VAR_SET_TOP_MARGIN_PARM            (345 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_START_BIT_IMAGE                "sbim"
#define VTE_TERMINFO_VAR_START_BIT_IMAGE                (346 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_START_CHAR_SET_DEF             "scsd"
#define VTE_TERMINFO_VAR_START_CHAR_SET_DEF             (347 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_STOP_BIT_IMAGE                 "rbim"
#define VTE_TERMINFO_VAR_STOP_BIT_IMAGE                 (348 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_STOP_CHAR_SET_DEF              "rcsd"
#define VTE_TERMINFO_VAR_STOP_CHAR_SET_DEF              (349 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SUBSCRIPT_CHARACTERS           "subcs"
#define VTE_TERMINFO_VAR_SUBSCRIPT_CHARACTERS           (350 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SUPERSCRIPT_CHARACTERS         "supcs"
#define VTE_TERMINFO_VAR_SUPERSCRIPT_CHARACTERS         (351 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_THESE_CAUSE_CR                 "docr"
#define VTE_TERMINFO_VAR_THESE_CAUSE_CR                 (352 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ZERO_MOTION                    "zerom"
#define VTE_TERMINFO_VAR_ZERO_MOTION                    (353 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CHAR_SET_NAMES                 "csnm"
#define VTE_TERMINFO_VAR_CHAR_SET_NAMES                 (354 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_KEY_MOUSE                      "kmous"
#define VTE_TERMINFO_VAR_KEY_MOUSE                      (355 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_MOUSE_INFO                     "minfo"
#define VTE_TERMINFO_VAR_MOUSE_INFO                     (356 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_REQ_MOUSE_POS                  "reqmp"
#define VTE_TERMINFO_VAR_REQ_MOUSE_POS                  (357 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_GET_MOUSE                      "getm"
#define VTE_TERMINFO_VAR_GET_MOUSE                      (358 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_A_FOREGROUND               "setaf"
#define VTE_TERMINFO_VAR_SET_A_FOREGROUND               (359 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_A_BACKGROUND               "setab"
#define VTE_TERMINFO_VAR_SET_A_BACKGROUND               (360 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PKEY_PLAB                      "pfxl"
#define VTE_TERMINFO_VAR_PKEY_PLAB                      (361 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DEVICE_TYPE                    "devt"
#define VTE_TERMINFO_VAR_DEVICE_TYPE                    (362 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_CODE_SET_INIT                  "csin"
#define VTE_TERMINFO_VAR_CODE_SET_INIT                  (363 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET0_DES_SEQ                   "s0ds"
#define VTE_TERMINFO_VAR_SET0_DES_SEQ                   (364 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET1_DES_SEQ                   "s1ds"
#define VTE_TERMINFO_VAR_SET1_DES_SEQ                   (365 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET2_DES_SEQ                   "s2ds"
#define VTE_TERMINFO_VAR_SET2_DES_SEQ                   (366 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET3_DES_SEQ                   "s3ds"
#define VTE_TERMINFO_VAR_SET3_DES_SEQ                   (367 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_LR_MARGIN                  "smglr"
#define VTE_TERMINFO_VAR_SET_LR_MARGIN                  (368 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_TB_MARGIN                  "smgtb"
#define VTE_TERMINFO_VAR_SET_TB_MARGIN                  (369 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_BIT_IMAGE_REPEAT               "birep"
#define VTE_TERMINFO_VAR_BIT_IMAGE_REPEAT               (370 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_BIT_IMAGE_NEWLINE              "binel"
#define VTE_TERMINFO_VAR_BIT_IMAGE_NEWLINE              (371 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_BIT_IMAGE_CARRIAGE_RETURN      "bicr"
#define VTE_TERMINFO_VAR_BIT_IMAGE_CARRIAGE_RETURN      (372 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_COLOR_NAMES                    "colornm"
#define VTE_TERMINFO_VAR_COLOR_NAMES                    (373 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DEFINE_BIT_IMAGE_REGION        "defbi"
#define VTE_TERMINFO_VAR_DEFINE_BIT_IMAGE_REGION        (374 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_END_BIT_IMAGE_REGION           "endbi"
#define VTE_TERMINFO_VAR_END_BIT_IMAGE_REGION           (375 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_COLOR_BAND                 "setcolor"
#define VTE_TERMINFO_VAR_SET_COLOR_BAND                 (376 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_PAGE_LENGTH                "slines"
#define VTE_TERMINFO_VAR_SET_PAGE_LENGTH                (377 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_DISPLAY_PC_CHAR                "dispc"
#define VTE_TERMINFO_VAR_DISPLAY_PC_CHAR                (378 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_PC_CHARSET_MODE          "smpch"
#define VTE_TERMINFO_VAR_ENTER_PC_CHARSET_MODE          (379 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_PC_CHARSET_MODE           "rmpch"
#define VTE_TERMINFO_VAR_EXIT_PC_CHARSET_MODE           (380 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_SCANCODE_MODE            "smsc"
#define VTE_TERMINFO_VAR_ENTER_SCANCODE_MODE            (381 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_EXIT_SCANCODE_MODE             "rmsc"
#define VTE_TERMINFO_VAR_EXIT_SCANCODE_MODE             (382 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_PC_TERM_OPTIONS                "pctrm"
#define VTE_TERMINFO_VAR_PC_TERM_OPTIONS                (383 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SCANCODE_ESCAPE                "scesc"
#define VTE_TERMINFO_VAR_SCANCODE_ESCAPE                (384 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ALT_SCANCODE_ESC               "scesa"
#define VTE_TERMINFO_VAR_ALT_SCANCODE_ESC               (385 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_HORIZONTAL_HL_MODE       "ehhlm"
#define VTE_TERMINFO_VAR_ENTER_HORIZONTAL_HL_MODE       (386 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_LEFT_HL_MODE             "elhlm"
#define VTE_TERMINFO_VAR_ENTER_LEFT_HL_MODE             (387 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_LOW_HL_MODE              "elohlm"
#define VTE_TERMINFO_VAR_ENTER_LOW_HL_MODE              (388 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_RIGHT_HL_MODE            "erhlm"
#define VTE_TERMINFO_VAR_ENTER_RIGHT_HL_MODE            (389 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_TOP_HL_MODE              "ethlm"
#define VTE_TERMINFO_VAR_ENTER_TOP_HL_MODE              (390 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_ENTER_VERTICAL_HL_MODE         "evhlm"
#define VTE_TERMINFO_VAR_ENTER_VERTICAL_HL_MODE         (391 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_A_ATTRIBUTES               "sgr1"
#define VTE_TERMINFO_VAR_SET_A_ATTRIBUTES               (392 | VTE_TERMINFO_VARTYPE_STRING)
#define VTE_TERMINFO_CAP_SET_PGLEN_INCH                 "slength"
#define VTE_TERMINFO_VAR_SET_PGLEN_INCH                 (393 | VTE_TERMINFO_VARTYPE_STRING)


#endif /* __VTE_TERMINFO_VARS_H__ */
