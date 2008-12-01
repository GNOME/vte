/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf -m 100 vteseq-n.gperf  */
/* Computed positions: -k'1,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 16 "vteseq-n.gperf"
struct vteseq_n_struct {
	int seq;
	VteTerminalSequenceHandler handler;
};
#include <string.h>
/* maximum key range = 64, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
vteseq_n_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 47, 72,
      14,  0,  1,  7, 42, 43, 72,  0, 24, 72,
      24, 26,  2, 11,  8, 13,  0, 29,  0, 16,
      23,  0, 11, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
      72, 72, 72, 72, 72, 72, 72, 72, 72
    };
  return len + asso_values[(unsigned char)str[len - 1]] + asso_values[(unsigned char)str[0]+3];
}

struct vteseq_n_pool_t
  {
    char vteseq_n_pool_str8[sizeof("set-mode")];
    char vteseq_n_pool_str9[sizeof("save-mode")];
    char vteseq_n_pool_str10[sizeof("soft-reset")];
    char vteseq_n_pool_str11[sizeof("scroll-up")];
    char vteseq_n_pool_str12[sizeof("cursor-up")];
    char vteseq_n_pool_str13[sizeof("decset")];
    char vteseq_n_pool_str14[sizeof("set-icon-title")];
    char vteseq_n_pool_str15[sizeof("decreset")];
    char vteseq_n_pool_str16[sizeof("set-window-title")];
    char vteseq_n_pool_str17[sizeof("cursor-next-line")];
    char vteseq_n_pool_str18[sizeof("cursor-lower-left")];
    char vteseq_n_pool_str19[sizeof("save-cursor")];
    char vteseq_n_pool_str20[sizeof("next-line")];
    char vteseq_n_pool_str21[sizeof("screen-alignment-test")];
    char vteseq_n_pool_str22[sizeof("cursor-preceding-line")];
    char vteseq_n_pool_str23[sizeof("tab-set")];
    char vteseq_n_pool_str25[sizeof("set-icon-and-window-title")];
    char vteseq_n_pool_str26[sizeof("cursor-character-absolute")];
    char vteseq_n_pool_str27[sizeof("device-status-report")];
    char vteseq_n_pool_str28[sizeof("character-position-absolute")];
    char vteseq_n_pool_str29[sizeof("cursor-forward")];
    char vteseq_n_pool_str30[sizeof("cursor-backward")];
    char vteseq_n_pool_str31[sizeof("dec-device-status-report")];
    char vteseq_n_pool_str32[sizeof("delete-lines")];
    char vteseq_n_pool_str33[sizeof("tab-clear")];
    char vteseq_n_pool_str34[sizeof("character-attributes")];
    char vteseq_n_pool_str35[sizeof("scroll-down")];
    char vteseq_n_pool_str36[sizeof("cursor-down")];
    char vteseq_n_pool_str37[sizeof("delete-characters")];
    char vteseq_n_pool_str38[sizeof("normal-keypad")];
    char vteseq_n_pool_str39[sizeof("reset-mode")];
    char vteseq_n_pool_str40[sizeof("cursor-position")];
    char vteseq_n_pool_str41[sizeof("restore-mode")];
    char vteseq_n_pool_str42[sizeof("utf-8-character-set")];
    char vteseq_n_pool_str43[sizeof("send-primary-device-attributes")];
    char vteseq_n_pool_str44[sizeof("set-scrolling-region")];
    char vteseq_n_pool_str45[sizeof("send-secondary-device-attributes")];
    char vteseq_n_pool_str46[sizeof("application-keypad")];
    char vteseq_n_pool_str47[sizeof("iso8859-1-character-set")];
    char vteseq_n_pool_str48[sizeof("line-position-absolute")];
    char vteseq_n_pool_str49[sizeof("insert-lines")];
    char vteseq_n_pool_str50[sizeof("cursor-forward-tabulation")];
    char vteseq_n_pool_str51[sizeof("restore-cursor")];
    char vteseq_n_pool_str52[sizeof("index")];
    char vteseq_n_pool_str53[sizeof("full-reset")];
    char vteseq_n_pool_str54[sizeof("window-manipulation")];
    char vteseq_n_pool_str55[sizeof("erase-in-line")];
    char vteseq_n_pool_str56[sizeof("horizontal-and-vertical-position")];
    char vteseq_n_pool_str58[sizeof("erase-in-display")];
    char vteseq_n_pool_str59[sizeof("vertical-tab")];
    char vteseq_n_pool_str60[sizeof("insert-blank-characters")];
    char vteseq_n_pool_str61[sizeof("return-terminal-id")];
    char vteseq_n_pool_str63[sizeof("cursor-back-tab")];
    char vteseq_n_pool_str64[sizeof("return-terminal-status")];
    char vteseq_n_pool_str65[sizeof("reverse-index")];
    char vteseq_n_pool_str66[sizeof("form-feed")];
    char vteseq_n_pool_str69[sizeof("request-terminal-parameters")];
    char vteseq_n_pool_str70[sizeof("linux-console-cursor-attributes")];
    char vteseq_n_pool_str71[sizeof("erase-characters")];
  };
static const struct vteseq_n_pool_t vteseq_n_pool_contents =
  {
    "set-mode",
    "save-mode",
    "soft-reset",
    "scroll-up",
    "cursor-up",
    "decset",
    "set-icon-title",
    "decreset",
    "set-window-title",
    "cursor-next-line",
    "cursor-lower-left",
    "save-cursor",
    "next-line",
    "screen-alignment-test",
    "cursor-preceding-line",
    "tab-set",
    "set-icon-and-window-title",
    "cursor-character-absolute",
    "device-status-report",
    "character-position-absolute",
    "cursor-forward",
    "cursor-backward",
    "dec-device-status-report",
    "delete-lines",
    "tab-clear",
    "character-attributes",
    "scroll-down",
    "cursor-down",
    "delete-characters",
    "normal-keypad",
    "reset-mode",
    "cursor-position",
    "restore-mode",
    "utf-8-character-set",
    "send-primary-device-attributes",
    "set-scrolling-region",
    "send-secondary-device-attributes",
    "application-keypad",
    "iso8859-1-character-set",
    "line-position-absolute",
    "insert-lines",
    "cursor-forward-tabulation",
    "restore-cursor",
    "index",
    "full-reset",
    "window-manipulation",
    "erase-in-line",
    "horizontal-and-vertical-position",
    "erase-in-display",
    "vertical-tab",
    "insert-blank-characters",
    "return-terminal-id",
    "cursor-back-tab",
    "return-terminal-status",
    "reverse-index",
    "form-feed",
    "request-terminal-parameters",
    "linux-console-cursor-attributes",
    "erase-characters"
  };
#define vteseq_n_pool ((const char *) &vteseq_n_pool_contents)
#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct vteseq_n_struct *
vteseq_n_lookup (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 59,
      MIN_WORD_LENGTH = 5,
      MAX_WORD_LENGTH = 32,
      MIN_HASH_VALUE = 8,
      MAX_HASH_VALUE = 71
    };

  static const unsigned char lengthtable[] =
    {
       0,  0,  0,  0,  0,  0,  0,  0,  8,  9, 10,  9,  9,  6,
      14,  8, 16, 16, 17, 11,  9, 21, 21,  7,  0, 25, 25, 20,
      27, 14, 15, 24, 12,  9, 20, 11, 11, 17, 13, 10, 15, 12,
      19, 30, 20, 32, 18, 23, 22, 12, 25, 14,  5, 10, 19, 13,
      32,  0, 16, 12, 23, 18,  0, 15, 22, 13,  9,  0,  0, 27,
      31, 16
    };
  static const struct vteseq_n_struct wordlist[] =
    {
      {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1},
#line 29 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str8, VTE_SEQUENCE_HANDLER(vte_sequence_handler_set_mode)},
#line 33 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str9, VTE_SEQUENCE_HANDLER(vte_sequence_handler_save_mode)},
#line 39 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str10, VTE_SEQUENCE_HANDLER(vte_sequence_handler_soft_reset)},
#line 34 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str11, VTE_SEQUENCE_HANDLER(vte_sequence_handler_scroll_up)},
#line 30 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str12, VTE_SEQUENCE_HANDLER(vte_sequence_handler_UP)},
#line 25 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str13, VTE_SEQUENCE_HANDLER(vte_sequence_handler_decset)},
#line 61 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str14, VTE_SEQUENCE_HANDLER(vte_sequence_handler_set_icon_title)},
#line 28 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str15, VTE_SEQUENCE_HANDLER(vte_sequence_handler_decreset)},
#line 71 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str16, VTE_SEQUENCE_HANDLER(vte_sequence_handler_set_window_title)},
#line 68 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str17, VTE_SEQUENCE_HANDLER(vte_sequence_handler_cursor_next_line)},
#line 72 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str18, VTE_SEQUENCE_HANDLER(vte_sequence_handler_cursor_lower_left)},
#line 42 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str19, VTE_SEQUENCE_HANDLER(vte_sequence_handler_sc)},
#line 32 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str20, VTE_SEQUENCE_HANDLER(vte_sequence_handler_next_line)},
#line 88 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str21, VTE_SEQUENCE_HANDLER(vte_sequence_handler_screen_alignment_test)},
#line 86 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str22, VTE_SEQUENCE_HANDLER(vte_sequence_handler_cursor_preceding_line)},
#line 27 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str23, VTE_SEQUENCE_HANDLER(vte_sequence_handler_st)},
      {-1},
#line 113 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str25, VTE_SEQUENCE_HANDLER(vte_sequence_handler_set_icon_and_window_title)},
#line 110 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str26, VTE_SEQUENCE_HANDLER(vte_sequence_handler_cursor_character_absolute)},
#line 82 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str27, VTE_SEQUENCE_HANDLER(vte_sequence_handler_device_status_report)},
#line 115 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str28, VTE_SEQUENCE_HANDLER(vte_sequence_handler_character_position_absolute)},
#line 58 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str29, VTE_SEQUENCE_HANDLER(vte_sequence_handler_RI)},
#line 63 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str30, VTE_SEQUENCE_HANDLER(vte_sequence_handler_LE)},
#line 107 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str31, VTE_SEQUENCE_HANDLER(vte_sequence_handler_dec_device_status_report)},
#line 45 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str32, VTE_SEQUENCE_HANDLER(vte_sequence_handler_delete_lines)},
#line 35 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str33, VTE_SEQUENCE_HANDLER(vte_sequence_handler_tab_clear)},
#line 81 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str34, VTE_SEQUENCE_HANDLER(vte_sequence_handler_character_attributes)},
#line 43 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str35, VTE_SEQUENCE_HANDLER(vte_sequence_handler_scroll_down)},
#line 40 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str36, VTE_SEQUENCE_HANDLER(vte_sequence_handler_DO)},
#line 73 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str37, VTE_SEQUENCE_HANDLER(vte_sequence_handler_DC)},
#line 53 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str38, VTE_SEQUENCE_HANDLER(vte_sequence_handler_normal_keypad)},
#line 38 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str39, VTE_SEQUENCE_HANDLER(vte_sequence_handler_reset_mode)},
#line 64 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str40, VTE_SEQUENCE_HANDLER(vte_sequence_handler_cursor_position)},
#line 48 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str41, VTE_SEQUENCE_HANDLER(vte_sequence_handler_restore_mode)},
#line 78 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str42, VTE_SEQUENCE_HANDLER(vte_sequence_handler_utf_8_charset)},
#line 124 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str43, VTE_SEQUENCE_HANDLER(vte_sequence_handler_send_primary_device_attributes)},
#line 83 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str44, VTE_SEQUENCE_HANDLER(vte_sequence_handler_set_scrolling_region)},
#line 127 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str45, VTE_SEQUENCE_HANDLER(vte_sequence_handler_send_secondary_device_attributes)},
#line 74 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str46, VTE_SEQUENCE_HANDLER(vte_sequence_handler_application_keypad)},
#line 100 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str47, VTE_SEQUENCE_HANDLER(vte_sequence_handler_local_charset)},
#line 93 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str48, VTE_SEQUENCE_HANDLER(vte_sequence_handler_line_position_absolute)},
#line 47 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str49, VTE_SEQUENCE_HANDLER(vte_sequence_handler_insert_lines)},
#line 111 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str50, VTE_SEQUENCE_HANDLER(vte_sequence_handler_ta)},
#line 60 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str51, VTE_SEQUENCE_HANDLER(vte_sequence_handler_rc)},
#line 24 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str52, VTE_SEQUENCE_HANDLER(vte_sequence_handler_index)},
#line 36 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str53, VTE_SEQUENCE_HANDLER(vte_sequence_handler_full_reset)},
#line 79 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str54, VTE_SEQUENCE_HANDLER(vte_sequence_handler_window_manipulation)},
#line 51 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str55, VTE_SEQUENCE_HANDLER(vte_sequence_handler_erase_in_line)},
#line 126 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str56, VTE_SEQUENCE_HANDLER(vte_sequence_handler_horizontal_and_vertical_position)},
      {-1},
#line 70 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str58, VTE_SEQUENCE_HANDLER(vte_sequence_handler_erase_in_display)},
#line 50 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str59, VTE_SEQUENCE_HANDLER(vte_sequence_handler_vertical_tab)},
#line 97 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str60, VTE_SEQUENCE_HANDLER(vte_sequence_handler_insert_blank_characters)},
#line 76 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str61, VTE_SEQUENCE_HANDLER(vte_sequence_handler_return_terminal_id)},
      {-1},
#line 62 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str63, VTE_SEQUENCE_HANDLER(vte_sequence_handler_bt)},
#line 94 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str64, VTE_SEQUENCE_HANDLER(vte_sequence_handler_return_terminal_status)},
#line 54 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str65, VTE_SEQUENCE_HANDLER(vte_sequence_handler_reverse_index)},
#line 31 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str66, VTE_SEQUENCE_HANDLER(vte_sequence_handler_form_feed)},
      {-1}, {-1},
#line 116 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str69, VTE_SEQUENCE_HANDLER(vte_sequence_handler_request_terminal_parameters)},
#line 125 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str70, VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 69 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_pool_t *)0)->vteseq_n_pool_str71, VTE_SEQUENCE_HANDLER(vte_sequence_handler_erase_characters)}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = vteseq_n_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        if (len == lengthtable[key])
          {
            register const char *s = wordlist[key].seq + vteseq_n_pool;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return &wordlist[key];
          }
    }
  return 0;
}
