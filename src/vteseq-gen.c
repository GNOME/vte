#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char * handler_index_names[] = {
  "0",
#define STRINGIZE(name) #name
#define VTE_SEQUENCE_HANDLER(name) STRINGIZE(name##_index),
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER
  "_dummy_handler_index"
};

enum {
  NULL_handler,
#define VTE_SEQUENCE_HANDLER(name) name,
#include "vteseq-list.h"
#undef VTE_SEQUENCE_HANDLER
  _dummy_handler
};

struct entry {
	const char *code;
	int handler;
};

#undef NULL
#define NULL 0

static struct entry vte_sequence_handlers[] = {
	{"!1", vte_sequence_handler_complain_key},
	{"!2", vte_sequence_handler_complain_key},
	{"!3", vte_sequence_handler_complain_key},

	{"#1", vte_sequence_handler_complain_key},
	{"#2", vte_sequence_handler_complain_key},
	{"#3", vte_sequence_handler_complain_key},
	{"#4", vte_sequence_handler_complain_key},

	{"%1", vte_sequence_handler_complain_key},
	{"%2", vte_sequence_handler_complain_key},
	{"%3", vte_sequence_handler_complain_key},
	{"%4", vte_sequence_handler_complain_key},
	{"%5", vte_sequence_handler_complain_key},
	{"%6", vte_sequence_handler_complain_key},
	{"%7", vte_sequence_handler_complain_key},
	{"%8", vte_sequence_handler_complain_key},
	{"%9", vte_sequence_handler_complain_key},
	{"%a", vte_sequence_handler_complain_key},
	{"%b", vte_sequence_handler_complain_key},
	{"%c", vte_sequence_handler_complain_key},
	{"%d", vte_sequence_handler_complain_key},
	{"%e", vte_sequence_handler_complain_key},
	{"%f", vte_sequence_handler_complain_key},
	{"%g", vte_sequence_handler_complain_key},
	{"%h", vte_sequence_handler_complain_key},
	{"%i", vte_sequence_handler_complain_key},
	{"%j", vte_sequence_handler_complain_key},

	{"&0", vte_sequence_handler_complain_key},
	{"&1", vte_sequence_handler_complain_key},
	{"&2", vte_sequence_handler_complain_key},
	{"&3", vte_sequence_handler_complain_key},
	{"&4", vte_sequence_handler_complain_key},
	{"&5", vte_sequence_handler_complain_key},
	{"&6", vte_sequence_handler_complain_key},
	{"&7", vte_sequence_handler_complain_key},
	{"&8", vte_sequence_handler_complain_key},
	{"&9", vte_sequence_handler_complain_key},

	{"*0", vte_sequence_handler_complain_key},
	{"*1", vte_sequence_handler_complain_key},
	{"*2", vte_sequence_handler_complain_key},
	{"*3", vte_sequence_handler_complain_key},
	{"*4", vte_sequence_handler_complain_key},
	{"*5", vte_sequence_handler_complain_key},
	{"*6", vte_sequence_handler_complain_key},
	{"*7", vte_sequence_handler_complain_key},
	{"*8", vte_sequence_handler_complain_key},
	{"*9", vte_sequence_handler_complain_key},

	{"@0", vte_sequence_handler_complain_key},
	{"@1", vte_sequence_handler_complain_key},
	{"@2", vte_sequence_handler_complain_key},
	{"@3", vte_sequence_handler_complain_key},
	{"@4", vte_sequence_handler_complain_key},
	{"@5", vte_sequence_handler_complain_key},
	{"@6", vte_sequence_handler_complain_key},
	{"@7", vte_sequence_handler_complain_key},
	{"@8", vte_sequence_handler_complain_key},
	{"@9", vte_sequence_handler_complain_key},

	{"al", vte_sequence_handler_al},
	{"AL", vte_sequence_handler_AL},
	{"ae", vte_sequence_handler_ae},
	{"as", vte_sequence_handler_as},

	{"bc", vte_sequence_handler_le},
	{"bl", vte_sequence_handler_bl},
	{"bt", vte_sequence_handler_bt},

	{"cb", vte_sequence_handler_cb},
	{"cc", vte_sequence_handler_noop},
	{"cd", vte_sequence_handler_cd},
	{"ce", vte_sequence_handler_ce},
	{"ch", vte_sequence_handler_ch},
	{"cl", vte_sequence_handler_cl},
	{"cm", vte_sequence_handler_cm},
	{"cr", vte_sequence_handler_cr},
	{"cs", vte_sequence_handler_cs},
	{"cS", vte_sequence_handler_cS},
	{"ct", vte_sequence_handler_ct},
	{"cv", vte_sequence_handler_cv},

	{"dc", vte_sequence_handler_dc},
	{"DC", vte_sequence_handler_DC},
	{"dl", vte_sequence_handler_dl},
	{"DL", vte_sequence_handler_DL},
	{"dm", vte_sequence_handler_noop},
	{"do", vte_sequence_handler_do},
	{"DO", vte_sequence_handler_DO},
	{"ds", NULL},

	{"eA", vte_sequence_handler_eA},
	{"ec", vte_sequence_handler_ec},
	{"ed", vte_sequence_handler_noop},
	{"ei", vte_sequence_handler_ei},

	{"ff", vte_sequence_handler_noop},
	{"fs", vte_sequence_handler_fs},
	{"F1", vte_sequence_handler_complain_key},
	{"F2", vte_sequence_handler_complain_key},
	{"F3", vte_sequence_handler_complain_key},
	{"F4", vte_sequence_handler_complain_key},
	{"F5", vte_sequence_handler_complain_key},
	{"F6", vte_sequence_handler_complain_key},
	{"F7", vte_sequence_handler_complain_key},
	{"F8", vte_sequence_handler_complain_key},
	{"F9", vte_sequence_handler_complain_key},
	{"FA", vte_sequence_handler_complain_key},
	{"FB", vte_sequence_handler_complain_key},
	{"FC", vte_sequence_handler_complain_key},
	{"FD", vte_sequence_handler_complain_key},
	{"FE", vte_sequence_handler_complain_key},
	{"FF", vte_sequence_handler_complain_key},
	{"FG", vte_sequence_handler_complain_key},
	{"FH", vte_sequence_handler_complain_key},
	{"FI", vte_sequence_handler_complain_key},
	{"FJ", vte_sequence_handler_complain_key},
	{"FK", vte_sequence_handler_complain_key},
	{"FL", vte_sequence_handler_complain_key},
	{"FM", vte_sequence_handler_complain_key},
	{"FN", vte_sequence_handler_complain_key},
	{"FO", vte_sequence_handler_complain_key},
	{"FP", vte_sequence_handler_complain_key},
	{"FQ", vte_sequence_handler_complain_key},
	{"FR", vte_sequence_handler_complain_key},
	{"FS", vte_sequence_handler_complain_key},
	{"FT", vte_sequence_handler_complain_key},
	{"FU", vte_sequence_handler_complain_key},
	{"FV", vte_sequence_handler_complain_key},
	{"FW", vte_sequence_handler_complain_key},
	{"FX", vte_sequence_handler_complain_key},
	{"FY", vte_sequence_handler_complain_key},
	{"FZ", vte_sequence_handler_complain_key},

	{"Fa", vte_sequence_handler_complain_key},
	{"Fb", vte_sequence_handler_complain_key},
	{"Fc", vte_sequence_handler_complain_key},
	{"Fd", vte_sequence_handler_complain_key},
	{"Fe", vte_sequence_handler_complain_key},
	{"Ff", vte_sequence_handler_complain_key},
	{"Fg", vte_sequence_handler_complain_key},
	{"Fh", vte_sequence_handler_complain_key},
	{"Fi", vte_sequence_handler_complain_key},
	{"Fj", vte_sequence_handler_complain_key},
	{"Fk", vte_sequence_handler_complain_key},
	{"Fl", vte_sequence_handler_complain_key},
	{"Fm", vte_sequence_handler_complain_key},
	{"Fn", vte_sequence_handler_complain_key},
	{"Fo", vte_sequence_handler_complain_key},
	{"Fp", vte_sequence_handler_complain_key},
	{"Fq", vte_sequence_handler_complain_key},
	{"Fr", vte_sequence_handler_complain_key},

	{"hd", NULL},
	{"ho", vte_sequence_handler_ho},
	{"hu", NULL},

	{"i1", NULL},
	{"i3", NULL},

	{"ic", vte_sequence_handler_ic},
	{"IC", vte_sequence_handler_IC},
	{"if", NULL},
	{"im", vte_sequence_handler_im},
	{"ip", NULL},
	{"iP", NULL},
	{"is", NULL},

	{"K1", vte_sequence_handler_complain_key},
	{"K2", vte_sequence_handler_complain_key},
	{"K3", vte_sequence_handler_complain_key},
	{"K4", vte_sequence_handler_complain_key},
	{"K5", vte_sequence_handler_complain_key},

	{"k0", vte_sequence_handler_complain_key},
	{"k1", vte_sequence_handler_complain_key},
	{"k2", vte_sequence_handler_complain_key},
	{"k3", vte_sequence_handler_complain_key},
	{"k4", vte_sequence_handler_complain_key},
	{"k5", vte_sequence_handler_complain_key},
	{"k6", vte_sequence_handler_complain_key},
	{"k7", vte_sequence_handler_complain_key},
	{"k8", vte_sequence_handler_complain_key},
	{"k9", vte_sequence_handler_complain_key},
	{"k;", vte_sequence_handler_complain_key},
	{"ka", vte_sequence_handler_complain_key},
	{"kA", vte_sequence_handler_complain_key},
	{"kb", vte_sequence_handler_kb},
	{"kB", vte_sequence_handler_complain_key},
	{"kC", vte_sequence_handler_complain_key},
	{"kd", vte_sequence_handler_complain_key},
	{"kD", vte_sequence_handler_complain_key},
	{"ke", vte_sequence_handler_ke},
	{"kE", vte_sequence_handler_complain_key},
	{"kF", vte_sequence_handler_complain_key},
	{"kh", vte_sequence_handler_complain_key},
	{"kH", vte_sequence_handler_complain_key},
	{"kI", vte_sequence_handler_complain_key},
	{"kl", vte_sequence_handler_complain_key},
	{"kL", vte_sequence_handler_complain_key},
	{"kM", vte_sequence_handler_complain_key},
	{"kN", vte_sequence_handler_complain_key},
	{"kP", vte_sequence_handler_complain_key},
	{"kr", vte_sequence_handler_complain_key},
	{"kR", vte_sequence_handler_complain_key},
	{"ks", vte_sequence_handler_ks},
	{"kS", vte_sequence_handler_complain_key},
	{"kt", vte_sequence_handler_complain_key},
	{"kT", vte_sequence_handler_complain_key},
	{"ku", vte_sequence_handler_complain_key},

	{"l0", NULL},
	{"l1", NULL},
	{"l2", NULL},
	{"l3", NULL},
	{"l4", NULL},
	{"l5", NULL},
	{"l6", NULL},
	{"l7", NULL},
	{"l8", NULL},
	{"l9", NULL},
	{"la", NULL},

	{"le", vte_sequence_handler_le},
	{"LE", vte_sequence_handler_LE},
	{"LF", NULL},
	{"ll", vte_sequence_handler_ll},
	{"LO", NULL},

	{"mb", vte_sequence_handler_mb},
	{"MC", NULL},
	{"md", vte_sequence_handler_md},
	{"me", vte_sequence_handler_me},
	{"mh", vte_sequence_handler_mh},
	{"mk", vte_sequence_handler_mk},
	{"ML", NULL},
	{"mm", NULL},
	{"mo", NULL},
	{"mp", vte_sequence_handler_mp},
	{"mr", vte_sequence_handler_mr},
	{"MR", NULL},

	{"nd", vte_sequence_handler_nd},
	{"nw", vte_sequence_handler_nw},

	{"pc", NULL},
	{"pf", NULL},
	{"pk", NULL},
	{"pl", NULL},
	{"pn", NULL},
	{"po", NULL},
	{"pO", NULL},
	{"ps", NULL},
	{"px", NULL},

	{"r1", NULL},
	{"r2", NULL},
	{"r3", NULL},

	{"..rp", NULL},
	{"RA", NULL},
	{"rc", vte_sequence_handler_rc},
	{"rf", NULL},
	{"RF", NULL},
	{"RI", vte_sequence_handler_RI},
	{"rp", NULL},
	{"rP", NULL},
	{"rs", NULL},
	{"RX", NULL},

	{"..sa", NULL},
	{"sa", NULL},
	{"SA", NULL},
	{"sc", vte_sequence_handler_sc},
	{"se", vte_sequence_handler_se},
	{"sf", vte_sequence_handler_sf},
	{"SF", vte_sequence_handler_SF},
	{"so", vte_sequence_handler_so},
	{"sr", vte_sequence_handler_sr},
	{"SR", vte_sequence_handler_SR},
	{"st", vte_sequence_handler_st},
	{"SX", NULL},

	{"ta", vte_sequence_handler_ta},
	{"te", vte_sequence_handler_noop},
	{"ti", vte_sequence_handler_noop},
	{"ts", vte_sequence_handler_ts},

	{"uc", vte_sequence_handler_uc},
	{"ue", vte_sequence_handler_ue},
	{"up", vte_sequence_handler_up},
	{"UP", vte_sequence_handler_UP},
	{"us", vte_sequence_handler_us},

	{"vb", vte_sequence_handler_vb},
	{"ve", vte_sequence_handler_ve},
	{"vi", vte_sequence_handler_vi},
	{"vs", vte_sequence_handler_vs},

	{"wi", NULL},

	{"XF", NULL},

	{"7-bit-controls", NULL},
	{"8-bit-controls", NULL},
	{"ansi-conformance-level-1", NULL},
	{"ansi-conformance-level-2", NULL},
	{"ansi-conformance-level-3", NULL},
	{"application-keypad", vte_sequence_handler_application_keypad},
	{"change-background-colors", NULL},
	{"change-color", NULL},
	{"change-cursor-colors", NULL},
	{"change-font-name", NULL},
	{"change-font-number", NULL},
	{"change-foreground-colors", NULL},
	{"change-highlight-colors", NULL},
	{"change-logfile", NULL},
	{"change-mouse-cursor-background-colors", NULL},
	{"change-mouse-cursor-foreground-colors", NULL},
	{"change-tek-background-colors", NULL},
	{"change-tek-foreground-colors", NULL},
	{"character-attributes", vte_sequence_handler_character_attributes},
	{"character-position-absolute", vte_sequence_handler_character_position_absolute},
	{"cursor-back-tab", vte_sequence_handler_bt},
	{"cursor-backward", vte_sequence_handler_LE},
	{"cursor-character-absolute", vte_sequence_handler_cursor_character_absolute},
	{"cursor-down", vte_sequence_handler_DO},
	{"cursor-forward-tabulation", vte_sequence_handler_ta},
	{"cursor-forward", vte_sequence_handler_RI},
	{"cursor-lower-left", vte_sequence_handler_cursor_lower_left},
	{"cursor-next-line", vte_sequence_handler_cursor_next_line},
	{"cursor-position", vte_sequence_handler_cursor_position},
	{"cursor-preceding-line", vte_sequence_handler_cursor_preceding_line},
	{"cursor-up", vte_sequence_handler_UP},
	{"dec-device-status-report", vte_sequence_handler_dec_device_status_report},
	{"dec-media-copy", NULL},
	{"decreset", vte_sequence_handler_decreset},
	{"decset", vte_sequence_handler_decset},
	{"delete-characters", vte_sequence_handler_DC},
	{"delete-lines", vte_sequence_handler_delete_lines},
	{"device-control-string", NULL},
	{"device-status-report", vte_sequence_handler_device_status_report},
	{"double-height-bottom-half", NULL},
	{"double-height-top-half", NULL},
	{"double-width", NULL},
	{"enable-filter-rectangle", NULL},
	{"enable-locator-reporting", NULL},
	{"end-of-guarded-area", NULL},
	{"erase-characters", vte_sequence_handler_erase_characters},
	{"erase-in-display", vte_sequence_handler_erase_in_display},
	{"erase-in-line", vte_sequence_handler_erase_in_line},
	{"form-feed", vte_sequence_handler_form_feed},
	{"full-reset", vte_sequence_handler_full_reset},
	{"horizontal-and-vertical-position", vte_sequence_handler_horizontal_and_vertical_position},
	{"index", vte_sequence_handler_index},
	{"initiate-hilite-mouse-tracking", NULL},
	{"insert-blank-characters", vte_sequence_handler_insert_blank_characters},
	{"insert-lines", vte_sequence_handler_insert_lines},
	{"invoke-g1-character-set-as-gr", NULL},
	{"invoke-g2-character-set-as-gr", NULL},
	{"invoke-g2-character-set", NULL},
	{"invoke-g3-character-set-as-gr", NULL},
	{"invoke-g3-character-set", NULL},
	{"iso8859-1-character-set", vte_sequence_handler_local_charset},
	{"linux-console-cursor-attributes", vte_sequence_handler_noop},
	{"line-position-absolute", vte_sequence_handler_line_position_absolute},
	{"media-copy", NULL},
	{"memory-lock", NULL},
	{"memory-unlock", NULL},
	{"next-line", vte_sequence_handler_next_line},
	{"normal-keypad", vte_sequence_handler_normal_keypad},
	{"repeat", NULL},
	{"request-locator-position", NULL},
	{"request-terminal-parameters", vte_sequence_handler_request_terminal_parameters},
	{"reset-mode", vte_sequence_handler_reset_mode},
	{"restore-cursor", vte_sequence_handler_rc},
	{"restore-mode", vte_sequence_handler_restore_mode},
	{"return-terminal-status", vte_sequence_handler_return_terminal_status},
	{"return-terminal-id", vte_sequence_handler_return_terminal_id},
	{"reverse-index", vte_sequence_handler_reverse_index},
	{"save-cursor", vte_sequence_handler_sc},
	{"save-mode", vte_sequence_handler_save_mode},
	{"screen-alignment-test", vte_sequence_handler_screen_alignment_test},
	{"scroll-down", vte_sequence_handler_scroll_down},
	{"scroll-up", vte_sequence_handler_scroll_up},
	{"select-character-protection", NULL},
	{"selective-erase-in-display", NULL},
	{"selective-erase-in-line", NULL},
	{"select-locator-events", NULL},
	{"send-primary-device-attributes", vte_sequence_handler_send_primary_device_attributes},
	{"send-secondary-device-attributes", vte_sequence_handler_send_secondary_device_attributes},
	{"set-conformance-level", NULL},
	{"set-icon-and-window-title", vte_sequence_handler_set_icon_and_window_title},
	{"set-icon-title", vte_sequence_handler_set_icon_title},
	{"set-mode", vte_sequence_handler_set_mode},
	{"set-scrolling-region", vte_sequence_handler_set_scrolling_region},
	{"set-text-property-21", NULL},
	{"set-text-property-2L", NULL},
	{"set-window-title", vte_sequence_handler_set_window_title},
	{"single-shift-g2", NULL},
	{"single-shift-g3", NULL},
	{"single-width", NULL},
	{"soft-reset", vte_sequence_handler_soft_reset},
	{"start-of-guarded-area", NULL},
	{"tab-clear", vte_sequence_handler_tab_clear},
	{"tab-set", vte_sequence_handler_st},
	{"utf-8-character-set", vte_sequence_handler_utf_8_charset},
	{"vertical-tab", vte_sequence_handler_vertical_tab},
	{"window-manipulation", vte_sequence_handler_window_manipulation},
};

static int
comp (const void *pa, const void *pb)
{
  const struct entry *a = pa, *b = pb;
  int alen, blen;

  alen = strlen (a->code);
  blen = strlen (b->code);

  /* sort all two-char codes at the beginning */
  alen = alen == 2 ? -1 : alen;
  blen = blen == 2 ? -1 : blen;

  if (alen < blen)
    return -1;
  if (alen > blen)
    return +1;
  return strcmp (a->code, b->code);
}

int
main (void)
{
  int i, twos, maxlen;
  const int n = sizeof (vte_sequence_handlers) / sizeof (vte_sequence_handlers[0]);

  qsort (vte_sequence_handlers, n, sizeof (vte_sequence_handlers[0]), comp);
  

  /* Write out the two-byte codes */

  for (i = 0; i < n; i++)
    if (strlen (vte_sequence_handlers[i].code) != 2)
      break;

  twos = i;

  printf ("const struct { gchar code[2]; guchar handler; }\n"
	  "vte_sequence_handlers_two[] = {\n");
  for (i = 0; i < twos; i++)
    printf ("  {\"%s\", %s},\n",
	    vte_sequence_handlers[i].code,
	    handler_index_names[vte_sequence_handlers[i].handler]);
  printf ("};\n\n");


  /* And the rest */

  maxlen = 0;
  for (i = twos; i < n; i++)
    if (maxlen < strlen (vte_sequence_handlers[i].code))
      maxlen = strlen (vte_sequence_handlers[i].code);

  printf ("const struct { guchar len; guchar handler; gchar code[%d+1]; }\n"
	  "vte_sequence_handlers_others[] = {\n", maxlen);
  for (i = twos; i < n; i++)
    printf ("  {%d, %s, \"%s\"},\n",
	    strlen (vte_sequence_handlers[i].code),
	    handler_index_names[vte_sequence_handlers[i].handler],
	    vte_sequence_handlers[i].code);
  printf ("};\n\n");

  return 0;
}
