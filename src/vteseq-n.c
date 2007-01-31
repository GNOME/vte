/* C code produced by gperf version 3.0.2 */
/* Command-line: gperf -g -a -S 4 -t -m 100 -C -P -l -H vteseq_n_hash -N vteseq_n_in_word_set -Q vteseq_n_stringpool vteseq-n.gperf  */
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
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "vteseq-n.gperf"
struct vteseq_n_lookup { const gint name; VteTerminalSequenceHandler handler; };

#undef TOTAL_KEYWORDS
#define TOTAL_KEYWORDS 60
#undef MIN_WORD_LENGTH
#define MIN_WORD_LENGTH 5
#undef MAX_WORD_LENGTH
#define MAX_WORD_LENGTH 34
#undef MIN_HASH_VALUE
#define MIN_HASH_VALUE 8
#undef MAX_HASH_VALUE
#define MAX_HASH_VALUE 71
/* maximum key range = 64, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static guint
vteseq_n_hash (register const guchar *str, register guint len)
{
  static const guint asso_values[] =
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
      14,  0,  1,  7, 42, 43, 72,  1, 24, 72,
      24, 26,  2, 11,  8, 13,  0, 29,  0, 16,
      23,  0, 13, 20, 72, 72, 72, 72, 72, 72,
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
  return len + asso_values[(guchar)str[len - 1]] + asso_values[(guchar)str[0]+3];
}

struct vteseq_n_stringpool_t
  {
    char vteseq_n_stringpool_str0[sizeof("set-mode")];
    char vteseq_n_stringpool_str1[sizeof("save-mode")];
    char vteseq_n_stringpool_str2[sizeof("soft-reset")];
    char vteseq_n_stringpool_str3[sizeof("scroll-up")];
    char vteseq_n_stringpool_str4[sizeof("cursor-up")];
    char vteseq_n_stringpool_str5[sizeof("decset")];
    char vteseq_n_stringpool_str6[sizeof("set-icon-title")];
    char vteseq_n_stringpool_str7[sizeof("decreset")];
    char vteseq_n_stringpool_str8[sizeof("set-window-title")];
    char vteseq_n_stringpool_str9[sizeof("cursor-next-line")];
    char vteseq_n_stringpool_str10[sizeof("cursor-lower-left")];
    char vteseq_n_stringpool_str11[sizeof("save-cursor")];
    char vteseq_n_stringpool_str12[sizeof("next-line")];
    char vteseq_n_stringpool_str13[sizeof("screen-alignment-test")];
    char vteseq_n_stringpool_str14[sizeof("cursor-preceding-line")];
    char vteseq_n_stringpool_str15[sizeof("tab-set")];
    char vteseq_n_stringpool_str16[sizeof("set-icon-and-window-title")];
    char vteseq_n_stringpool_str17[sizeof("cursor-character-absolute")];
    char vteseq_n_stringpool_str18[sizeof("device-status-report")];
    char vteseq_n_stringpool_str19[sizeof("character-position-absolute")];
    char vteseq_n_stringpool_str20[sizeof("cursor-forward")];
    char vteseq_n_stringpool_str21[sizeof("cursor-backward")];
    char vteseq_n_stringpool_str22[sizeof("dec-device-status-report")];
    char vteseq_n_stringpool_str23[sizeof("delete-lines")];
    char vteseq_n_stringpool_str24[sizeof("tab-clear")];
    char vteseq_n_stringpool_str25[sizeof("character-attributes")];
    char vteseq_n_stringpool_str26[sizeof("scroll-down")];
    char vteseq_n_stringpool_str27[sizeof("cursor-down")];
    char vteseq_n_stringpool_str28[sizeof("delete-characters")];
    char vteseq_n_stringpool_str29[sizeof("normal-keypad")];
    char vteseq_n_stringpool_str30[sizeof("reset-mode")];
    char vteseq_n_stringpool_str31[sizeof("cursor-position")];
    char vteseq_n_stringpool_str32[sizeof("restore-mode")];
    char vteseq_n_stringpool_str33[sizeof("utf-8-character-set")];
    char vteseq_n_stringpool_str34[sizeof("send-primary-device-attributes")];
    char vteseq_n_stringpool_str35[sizeof("set-scrolling-region")];
    char vteseq_n_stringpool_str36[sizeof("send-secondary-device-attributes")];
    char vteseq_n_stringpool_str37[sizeof("application-keypad")];
    char vteseq_n_stringpool_str38[sizeof("iso8859-1-character-set")];
    char vteseq_n_stringpool_str39[sizeof("line-position-absolute")];
    char vteseq_n_stringpool_str40[sizeof("insert-lines")];
    char vteseq_n_stringpool_str41[sizeof("cursor-forward-tabulation")];
    char vteseq_n_stringpool_str42[sizeof("restore-cursor")];
    char vteseq_n_stringpool_str43[sizeof("index")];
    char vteseq_n_stringpool_str44[sizeof("full-reset")];
    char vteseq_n_stringpool_str45[sizeof("vte_sequence_handlers_others[] = {")];
    char vteseq_n_stringpool_str46[sizeof("erase-in-line")];
    char vteseq_n_stringpool_str47[sizeof("window-manipulation")];
    char vteseq_n_stringpool_str48[sizeof("horizontal-and-vertical-position")];
    char vteseq_n_stringpool_str49[sizeof("erase-in-display")];
    char vteseq_n_stringpool_str50[sizeof("vertical-tab")];
    char vteseq_n_stringpool_str51[sizeof("insert-blank-characters")];
    char vteseq_n_stringpool_str52[sizeof("return-terminal-id")];
    char vteseq_n_stringpool_str53[sizeof("cursor-back-tab")];
    char vteseq_n_stringpool_str54[sizeof("return-terminal-status")];
    char vteseq_n_stringpool_str55[sizeof("reverse-index")];
    char vteseq_n_stringpool_str56[sizeof("form-feed")];
    char vteseq_n_stringpool_str57[sizeof("request-terminal-parameters")];
    char vteseq_n_stringpool_str58[sizeof("linux-console-cursor-attributes")];
    char vteseq_n_stringpool_str59[sizeof("erase-characters")];
  };
static const struct vteseq_n_stringpool_t vteseq_n_stringpool_contents =
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
    "vte_sequence_handlers_others[] = {",
    "erase-in-line",
    "window-manipulation",
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
#define vteseq_n_stringpool ((const char *) &vteseq_n_stringpool_contents)
#ifdef __GNUC__
__inline
#endif
static VteTerminalSequenceHandler
vteseq_n_lookup (register const guchar *str, register guint len)
{
  static const struct vteseq_n_lookup wordlist[] =
    {
#line 11 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str0, vte_sequence_handler_set_mode},
#line 15 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str1, vte_sequence_handler_save_mode},
#line 21 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str2, vte_sequence_handler_soft_reset},
#line 16 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str3, vte_sequence_handler_scroll_up},
#line 12 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str4, vte_sequence_handler_UP},
#line 7 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str5, vte_sequence_handler_decset},
#line 43 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str6, vte_sequence_handler_set_icon_title},
#line 10 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str7, vte_sequence_handler_decreset},
#line 53 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str8, vte_sequence_handler_set_window_title},
#line 50 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str9, vte_sequence_handler_cursor_next_line},
#line 54 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str10, vte_sequence_handler_cursor_lower_left},
#line 24 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str11, vte_sequence_handler_sc},
#line 14 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str12, vte_sequence_handler_next_line},
#line 70 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str13, vte_sequence_handler_screen_alignment_test},
#line 68 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str14, vte_sequence_handler_cursor_preceding_line},
#line 9 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str15, vte_sequence_handler_st},
#line 95 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str16, vte_sequence_handler_set_icon_and_window_title},
#line 92 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str17, vte_sequence_handler_cursor_character_absolute},
#line 64 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str18, vte_sequence_handler_device_status_report},
#line 97 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str19, vte_sequence_handler_character_position_absolute},
#line 40 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str20, vte_sequence_handler_RI},
#line 45 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str21, vte_sequence_handler_LE},
#line 89 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str22, vte_sequence_handler_dec_device_status_report},
#line 27 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str23, vte_sequence_handler_delete_lines},
#line 17 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str24, vte_sequence_handler_tab_clear},
#line 63 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str25, vte_sequence_handler_character_attributes},
#line 25 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str26, vte_sequence_handler_scroll_down},
#line 22 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str27, vte_sequence_handler_DO},
#line 55 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str28, vte_sequence_handler_DC},
#line 35 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str29, vte_sequence_handler_normal_keypad},
#line 20 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str30, vte_sequence_handler_reset_mode},
#line 46 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str31, vte_sequence_handler_cursor_position},
#line 30 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str32, vte_sequence_handler_restore_mode},
#line 60 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str33, vte_sequence_handler_utf_8_charset},
#line 106 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str34, vte_sequence_handler_send_primary_device_attributes},
#line 65 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str35, vte_sequence_handler_set_scrolling_region},
#line 109 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str36, vte_sequence_handler_send_secondary_device_attributes},
#line 56 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str37, vte_sequence_handler_application_keypad},
#line 82 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str38, vte_sequence_handler_local_charset},
#line 75 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str39, vte_sequence_handler_line_position_absolute},
#line 29 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str40, vte_sequence_handler_insert_lines},
#line 93 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str41, vte_sequence_handler_ta},
#line 42 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str42, vte_sequence_handler_rc},
#line 6 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str43, vte_sequence_handler_index},
#line 18 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str44, vte_sequence_handler_full_reset},
#line 3 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str45},
#line 33 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str46, vte_sequence_handler_erase_in_line},
#line 61 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str47, vte_sequence_handler_window_manipulation},
#line 108 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str48, vte_sequence_handler_horizontal_and_vertical_position},
#line 52 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str49, vte_sequence_handler_erase_in_display},
#line 32 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str50, vte_sequence_handler_vertical_tab},
#line 79 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str51, vte_sequence_handler_insert_blank_characters},
#line 58 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str52, vte_sequence_handler_return_terminal_id},
#line 44 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str53, vte_sequence_handler_bt},
#line 76 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str54, vte_sequence_handler_return_terminal_status},
#line 36 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str55, vte_sequence_handler_reverse_index},
#line 13 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str56, vte_sequence_handler_form_feed},
#line 98 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str57, vte_sequence_handler_request_terminal_parameters},
#line 107 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str58, vte_sequence_handler_noop},
#line 51 "vteseq-n.gperf"
      {(int)(long)&((struct vteseq_n_stringpool_t *)0)->vteseq_n_stringpool_str59, vte_sequence_handler_erase_characters}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = vteseq_n_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register const struct vteseq_n_lookup *resword;

          if (key < 39)
            {
              if (key < 23)
                {
                  switch (key - 8)
                    {
                      case 0:
                        if (len == 8)
                          {
                            resword = &wordlist[0];
                            goto compare;
                          }
                        break;
                      case 1:
                        if (len == 9)
                          {
                            resword = &wordlist[1];
                            goto compare;
                          }
                        break;
                      case 2:
                        if (len == 10)
                          {
                            resword = &wordlist[2];
                            goto compare;
                          }
                        break;
                      case 3:
                        if (len == 9)
                          {
                            resword = &wordlist[3];
                            goto compare;
                          }
                        break;
                      case 4:
                        if (len == 9)
                          {
                            resword = &wordlist[4];
                            goto compare;
                          }
                        break;
                      case 5:
                        if (len == 6)
                          {
                            resword = &wordlist[5];
                            goto compare;
                          }
                        break;
                      case 6:
                        if (len == 14)
                          {
                            resword = &wordlist[6];
                            goto compare;
                          }
                        break;
                      case 7:
                        if (len == 8)
                          {
                            resword = &wordlist[7];
                            goto compare;
                          }
                        break;
                      case 8:
                        if (len == 16)
                          {
                            resword = &wordlist[8];
                            goto compare;
                          }
                        break;
                      case 9:
                        if (len == 16)
                          {
                            resword = &wordlist[9];
                            goto compare;
                          }
                        break;
                      case 10:
                        if (len == 17)
                          {
                            resword = &wordlist[10];
                            goto compare;
                          }
                        break;
                      case 11:
                        if (len == 11)
                          {
                            resword = &wordlist[11];
                            goto compare;
                          }
                        break;
                      case 12:
                        if (len == 9)
                          {
                            resword = &wordlist[12];
                            goto compare;
                          }
                        break;
                      case 13:
                        if (len == 21)
                          {
                            resword = &wordlist[13];
                            goto compare;
                          }
                        break;
                      case 14:
                        if (len == 21)
                          {
                            resword = &wordlist[14];
                            goto compare;
                          }
                        break;
                    }
                }
              else
                {
                  switch (key - 23)
                    {
                      case 0:
                        if (len == 7)
                          {
                            resword = &wordlist[15];
                            goto compare;
                          }
                        break;
                      case 2:
                        if (len == 25)
                          {
                            resword = &wordlist[16];
                            goto compare;
                          }
                        break;
                      case 3:
                        if (len == 25)
                          {
                            resword = &wordlist[17];
                            goto compare;
                          }
                        break;
                      case 4:
                        if (len == 20)
                          {
                            resword = &wordlist[18];
                            goto compare;
                          }
                        break;
                      case 5:
                        if (len == 27)
                          {
                            resword = &wordlist[19];
                            goto compare;
                          }
                        break;
                      case 6:
                        if (len == 14)
                          {
                            resword = &wordlist[20];
                            goto compare;
                          }
                        break;
                      case 7:
                        if (len == 15)
                          {
                            resword = &wordlist[21];
                            goto compare;
                          }
                        break;
                      case 8:
                        if (len == 24)
                          {
                            resword = &wordlist[22];
                            goto compare;
                          }
                        break;
                      case 9:
                        if (len == 12)
                          {
                            resword = &wordlist[23];
                            goto compare;
                          }
                        break;
                      case 10:
                        if (len == 9)
                          {
                            resword = &wordlist[24];
                            goto compare;
                          }
                        break;
                      case 11:
                        if (len == 20)
                          {
                            resword = &wordlist[25];
                            goto compare;
                          }
                        break;
                      case 12:
                        if (len == 11)
                          {
                            resword = &wordlist[26];
                            goto compare;
                          }
                        break;
                      case 13:
                        if (len == 11)
                          {
                            resword = &wordlist[27];
                            goto compare;
                          }
                        break;
                      case 14:
                        if (len == 17)
                          {
                            resword = &wordlist[28];
                            goto compare;
                          }
                        break;
                      case 15:
                        if (len == 13)
                          {
                            resword = &wordlist[29];
                            goto compare;
                          }
                        break;
                    }
                }
            }
          else
            {
              if (key < 54)
                {
                  switch (key - 39)
                    {
                      case 0:
                        if (len == 10)
                          {
                            resword = &wordlist[30];
                            goto compare;
                          }
                        break;
                      case 1:
                        if (len == 15)
                          {
                            resword = &wordlist[31];
                            goto compare;
                          }
                        break;
                      case 2:
                        if (len == 12)
                          {
                            resword = &wordlist[32];
                            goto compare;
                          }
                        break;
                      case 3:
                        if (len == 19)
                          {
                            resword = &wordlist[33];
                            goto compare;
                          }
                        break;
                      case 4:
                        if (len == 30)
                          {
                            resword = &wordlist[34];
                            goto compare;
                          }
                        break;
                      case 5:
                        if (len == 20)
                          {
                            resword = &wordlist[35];
                            goto compare;
                          }
                        break;
                      case 6:
                        if (len == 32)
                          {
                            resword = &wordlist[36];
                            goto compare;
                          }
                        break;
                      case 7:
                        if (len == 18)
                          {
                            resword = &wordlist[37];
                            goto compare;
                          }
                        break;
                      case 8:
                        if (len == 23)
                          {
                            resword = &wordlist[38];
                            goto compare;
                          }
                        break;
                      case 9:
                        if (len == 22)
                          {
                            resword = &wordlist[39];
                            goto compare;
                          }
                        break;
                      case 10:
                        if (len == 12)
                          {
                            resword = &wordlist[40];
                            goto compare;
                          }
                        break;
                      case 11:
                        if (len == 25)
                          {
                            resword = &wordlist[41];
                            goto compare;
                          }
                        break;
                      case 12:
                        if (len == 14)
                          {
                            resword = &wordlist[42];
                            goto compare;
                          }
                        break;
                      case 13:
                        if (len == 5)
                          {
                            resword = &wordlist[43];
                            goto compare;
                          }
                        break;
                      case 14:
                        if (len == 10)
                          {
                            resword = &wordlist[44];
                            goto compare;
                          }
                        break;
                    }
                }
              else
                {
                  switch (key - 54)
                    {
                      case 0:
                        if (len == 34)
                          {
                            resword = &wordlist[45];
                            goto compare;
                          }
                        break;
                      case 1:
                        if (len == 13)
                          {
                            resword = &wordlist[46];
                            goto compare;
                          }
                        break;
                      case 2:
                        if (len == 19)
                          {
                            resword = &wordlist[47];
                            goto compare;
                          }
                        break;
                      case 3:
                        if (len == 32)
                          {
                            resword = &wordlist[48];
                            goto compare;
                          }
                        break;
                      case 4:
                        if (len == 16)
                          {
                            resword = &wordlist[49];
                            goto compare;
                          }
                        break;
                      case 5:
                        if (len == 12)
                          {
                            resword = &wordlist[50];
                            goto compare;
                          }
                        break;
                      case 6:
                        if (len == 23)
                          {
                            resword = &wordlist[51];
                            goto compare;
                          }
                        break;
                      case 7:
                        if (len == 18)
                          {
                            resword = &wordlist[52];
                            goto compare;
                          }
                        break;
                      case 9:
                        if (len == 15)
                          {
                            resword = &wordlist[53];
                            goto compare;
                          }
                        break;
                      case 10:
                        if (len == 22)
                          {
                            resword = &wordlist[54];
                            goto compare;
                          }
                        break;
                      case 11:
                        if (len == 13)
                          {
                            resword = &wordlist[55];
                            goto compare;
                          }
                        break;
                      case 12:
                        if (len == 9)
                          {
                            resword = &wordlist[56];
                            goto compare;
                          }
                        break;
                      case 15:
                        if (len == 27)
                          {
                            resword = &wordlist[57];
                            goto compare;
                          }
                        break;
                      case 16:
                        if (len == 31)
                          {
                            resword = &wordlist[58];
                            goto compare;
                          }
                        break;
                      case 17:
                        if (len == 16)
                          {
                            resword = &wordlist[59];
                            goto compare;
                          }
                        break;
                    }
                }
            }
          return 0;
        compare:
          {
            register const char *s = resword->name + vteseq_n_stringpool;

            if (*str == *s && !memcmp (str + 1, s + 1, len - 1))
              return resword->handler;
          }
        }
    }
  return 0;
}
