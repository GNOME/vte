/* $Id$ */

/*
 * Reference:  VT220 Programmer Pocket Guide (EK-VT220-HR-002)
 */
#include <vttest.h>
#include <ttymodes.h>
#include <esc.h>

int
any_DSR(MENU_ARGS, char *text, void (*explain)(char *report))
{
  char *report;

  vt_move(1,1);
  printf("Testing DSR: %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("%s", text);
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_csi(report)) != 0
   && strlen(report) > 2
   && *report++ == '?') {
    if (explain != 0)
      (*explain)(report);
    else
      show_result(SHOW_SUCCESS);
  } else {
    show_result(SHOW_FAILURE);
  }

  restore_ttymodes();
  vt_move(max_lines-1, 1);
  return MENU_HOLD;
}

static void
report_ok(char *ref, char *tst)
{
  if ((tst = skip_csi(tst)) == 0)
    tst = "?";
  show_result(!strcmp(ref, tst) ? SHOW_SUCCESS : SHOW_FAILURE);
}

/*
 * Request  CSI ? 26 n            keyboard dialect
 * Response CSI ? 27; Ps n
 */
static void
show_KeyboardStatus(char *report)
{
  int pos = 0;
  int code;
  int save;
  char *show = SHOW_FAILURE;

  if ((code = scanto(report, &pos, ';')) == 27
   && (code = scan_any(report, &pos, 'n')) != 0) {
    switch(code) {
    case  1:  show = "North American/ASCII"; break;
    case  2:  show = "British";              break;
    case  3:  show = "Flemish";              break;
    case  4:  show = "French Canadian";      break;
    case  5:  show = "Danish";               break;
    case  6:  show = "Finnish";              break;
    case  7:  show = "German";               break;
    case  8:  show = "Dutch";                break;
    case  9:  show = "Italian";              break;
    case 10:  show = "Swiss (French)";       break;
    case 11:  show = "Swiss (German)";       break;
    case 12:  show = "Swedish";              break;
    case 13:  show = "Norwegian/Danish";     break;
    case 14:  show = "French";               break;
    case 15:  show = "Spanish";              break;
    case 16:  show = "Portugese";            break;
    case 17:  show = "Hebrew";               break; /* FIXME: kermit says 14 */
    default:  show = "unknown";
    }
  }
  show_result(show);

  /* VT420 implements additional parameters past those reported by the VT220 */
  save = pos;
  code = scan_any(report, &pos, 'n');
  if (save != pos) {
    vt_move(5,10);
    switch(code) {
    case 0: show = "keyboard ready"; break;
    case 3: show = "no keyboard"; break;
    case 8: show = "keyboard busy"; break;
    default: show = "unknown keyboard status";
    }
    show_result(show);

    vt_move(6,10);
    switch (code = scan_any(report, &pos, 'n')) {
    case 0:  show = "LK201"; break;
    case 1:  show = "LK401"; break;
    default: show = "unknown keyboard type";
    }
    show_result(show);
  }
}

static void
show_Locator_Status(char *report)
{
  int pos = 0;
  int code = scanto(report, &pos, 'n');
  char *show;

  switch(code) {
  case 53: show = "No locator"; break;
  case 50: show = "Locator ready"; break;
  case 58: show = "Locator busy"; break;
  default: show = SHOW_FAILURE;
  }
  show_result(show);
}

static void
show_PrinterStatus(char *report)
{
  int pos = 0;
  int code = scanto(report, &pos, 'n');
  char *show;

  switch (code) {
  case 13: show = "No printer"; break;
  case 10: show = "Printer ready"; break;
  case 11: show = "Printer not ready"; break;
  case 18: show = "Printer busy"; break;
  case 19: show = "Printer assigned to other session"; break;
  default: show = SHOW_FAILURE;
  }
  show_result(show);
}

static void
show_UDK_Status(char *report)
{
  int pos = 0;
  int code = scanto(report, &pos, 'n');
  char *show;

  switch(code) {
  case 20: show = "UDKs unlocked"; break;
  case 21: show = "UDKs locked";   break;
  default: show = SHOW_FAILURE;
  }
  show_result(show);
}

/* VT220 & up.
 */
int
tst_S8C1T(MENU_ARGS)
{
  char *report;
  int flag = input_8bits;
  int pass;

  vt_move(1,1);
  println(the_title);

  vt_move(5,1);
  println("This tests the VT200+ control sequence to direct the terminal to emit 8-bit");
  println("control-sequences instead of <esc> sequences.");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for (pass = 0; pass < 2; pass++) {
    flag = !flag;
    s8c1t(flag);
    cup(1,1); dsr(6);
    report = instr();
    vt_move(10 + pass * 3, 1);
    printf("8-bit controls %s: ", flag ? "enabled" : "disabled");
    chrprint(report);
    report_ok("1;1R", report);
  }

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/*
 * Test DEC's selective-erase (set-protected area) by drawing a box of
 * *'s that will remain, and a big X of *'s that gets cleared..
 */
int
tst_DECSCA(MENU_ARGS)
{
  int i, j, pass;
  int tmar = 5;
  int bmar = max_lines - 8;
  int lmar = 20;
  int rmar = min_cols - lmar;

  for (pass = 0; pass < 2; pass++) {
    if (pass == 0)
      decsca(1);
    for (i = tmar; i <= bmar; i++) {
      cup(i, lmar);
      for (j = lmar; j <= rmar; j++) {
        printf("*");
      }
    }
    if (pass == 0) {
      decsca(0);

      for (j = 0; j <= 2; j++) {
        for (i = 1; i < tmar; i++) {
          cup(i, lmar - tmar + (i+j)); printf("*");
          cup(i, rmar + tmar - (i+j)); printf("*");
        }
        for (i = bmar + 1; i < max_lines; i++) {
          cup(i, lmar + bmar - i + j); printf("*");
          cup(i, rmar - bmar + i - j); printf("*");
        }
        cup(max_lines/2, min_cols/2);
        decsed(j);
      }

      for (i = rmar+1; i <= min_cols; i++) {
        cup(tmar, i);        printf("*");
        cup(max_lines/2, i); printf("*");
      }
      cup(max_lines/2, min_cols/2);
      decsel(0); /* after the cursor */

      for (i = 1; i < lmar; i++) {
        cup(tmar, i);        printf("*");
        cup(max_lines/2, i); printf("*");
      }
      cup(max_lines/2, min_cols/2);
      decsel(1); /* before the cursor */

      cup(tmar, min_cols/2);
      decsel(2); /* the whole line */

      vt_move(max_lines-3, 1);
      vt_clear(0);
      println("If your terminal supports DEC protected areas (DECSCA, DECSED, DECSEL),");
      println("there will be an solid box made of *'s in the middle of the screen.");
      holdit();
    }
  }
  return MENU_NOHOLD;
}

/*
 * VT220 & up
 *
 * Test if the terminal can make the cursor invisible
 */
int
tst_DECTCEM(MENU_ARGS)
{
  vt_move(1,1);
  rm("?25");
  println("The cursor should be invisible");
  holdit();
  sm("?25");
  println("The cursor should be visible again");
  return MENU_HOLD;
}

int
tst_DECUDK(MENU_ARGS)
{
  int key;

  static struct {
    int code;
    char *name;
  } keytable[] = {
    /* xterm programs these: */
    { 11, "F1" },
    { 12, "F2" },
    { 13, "F3" },
    { 14, "F4" },
    { 15, "F5" },
    /* vt420 programs these: */
    { 17, "F6" },
    { 18, "F7" },
    { 19, "F8" },
    { 20, "F9" },
    { 21, "F10" },
    { 23, "F11" },
    { 24, "F12" },
    { 25, "F13" },
    { 26, "F14" },
    { 28, "F15" },
    { 29, "F16" },
    { 31, "F17" },
    { 32, "F18" },
    { 33, "F19" },
    { 34, "F20" } };

  for (key = 0; key < TABLESIZE(keytable); key++) {
    char temp[80];
    char *s;
    temp[0] = '\0';
    for (s = keytable[key].name; *s; s++)
      sprintf(temp + strlen(temp), "%02x", *s & 0xff);
    do_dcs("1;1|%d/%s", keytable[key].code, temp);
  }

  vt_move(1,1);
  println(the_title);
  println("Press 'q' to quit.  Function keys should echo their labels.");
  println("(On a DEC terminal you must press SHIFT as well).");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for (;;) {
    char *report = instr();
    if (*report == 'q')
      break;
    vt_move(5,10);
    vt_clear(0);
    chrprint(report);
  }

  do_dcs("0"); /* clear all keys */

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

int
tst_DSR_keyboard(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?26n", show_KeyboardStatus);
}

int
tst_DSR_locator(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?53n", show_Locator_Status);
}

int
tst_DSR_printer(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?15n", show_PrinterStatus);
}

int
tst_DSR_userkeys(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?25n", show_UDK_Status);
}

/*
 * VT200 and up
 *
 * Test to ensure that 'ech' (erase character) is honored, with no parameter,
 * explicit parameter, and longer than the screen width (to ensure that the
 * terminal doesn't try to wrap-around the erasure).
 */
int
tst_ECH(MENU_ARGS)
{
  int i;
  int last = max_lines - 4;

  decaln();
  for (i = 1; i <= max_lines; i++) {
    cup(i, min_cols - i - 2);
    do_csi("X"); /* make sure default-parameter works */
    cup(i, min_cols - i - 1);
    printf("*");
    ech(min_cols);
    printf("*"); /* this should be adjacent, in the upper-right corner */
  }

  vt_move(last, 1);
  vt_clear(0);

  vt_move(last, min_cols - (last + 10));
  println("diagonal: ^^ (clear)");
  println("ECH test: there should be E's with a gap before diagonal of **'s");
  println("The lower-right diagonal region should be cleared.  Nothing else.");
  return MENU_HOLD;
}

/******************************************************************************/

static int
tst_device_status(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Keyboard Status",                              tst_DSR_keyboard },
      { "Test Printer Status",                               tst_DSR_printer },
      { "Test UDK Status",                                   tst_DSR_userkeys },
      { "Test Locator Status",                               tst_DSR_locator },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT220 Device Status Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

static int
tst_terminal_modes(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Send/Receive mode (SRM)",                      tst_SRM },
      { "Test Visible/Invisible Cursor (DECTCEM)",           tst_DECTCEM },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT220 Terminal Mode Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

int
tst_vt220(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test 8-bit controls (S7C1T/S8C1T)",                 tst_S8C1T },
      { "Test Device Status Report (DSR)",                   tst_device_status },
      { "Test Erase Char (ECH)",                             tst_ECH },
      { "Test Printer (MC)",                                 tst_printing },
      { "Test Protected-Areas (DECSCA)",                     tst_DECSCA },
      { "Test Soft Character Sets (DECDLD)",                 tst_softchars },
      { "Test Terminal Modes",                               tst_terminal_modes },
      { "Test user-defined keys (DECUDK)",                   tst_DECUDK },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT220/VT320 Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
