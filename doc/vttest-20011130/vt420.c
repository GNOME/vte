/* $Id$ */

/*
 * Reference:  Installing and Using the VT420 Video Terminal (North American
 *             Model (EK-VT420-UG.002)
 */
#include <vttest.h>
#include <esc.h>
#include <ttymodes.h>

typedef struct {
  int mode;
  char *name;
  } MODES;

static void show_DECCIR(char *report);
static void show_DECTABSR(char *report);

/******************************************************************************/

static int
any_decrqpsr(MENU_ARGS, int Ps)
{
  char *report;

  vt_move(1,1);
  printf("Testing DECRQPSR: %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("%d$w", Ps);
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_dcs(report)) != 0) {
    if (strip_terminator(report)
     && *report == Ps + '0'
     && !strncmp(report+1, "$u", 2)) {
      show_result("%s (valid request)", SHOW_SUCCESS);
      switch (Ps) {
      case 1:
        show_DECCIR(report);
        break;
      case 2:
        show_DECTABSR(report);
        break;
      }
    } else {
      show_result(SHOW_FAILURE);
    }
  } else {
    show_result(SHOW_FAILURE);
  }

  restore_ttymodes();
  vt_move(max_lines-1, 1);
  return MENU_HOLD;
}

/*
 * FIXME: The VT420 manual says that a valid response begins "DCS 0 $ r",
 * however I see "DCS 1 $ r" on a real VT420, consistently.
 */
static int
any_decrqss(char *msg, char *func)
{
  char *report;
  char *show;

  vt_move(1,1);
  printf("Testing DECRQSS: %s\n", msg);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  decrqss(func);
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  switch (parse_decrqss(report, func)) {
    case 1:
      show = "ok (valid request)";
      break;
    case 0:
      show = "invalid request";
      break;
    default:
      show = SHOW_FAILURE;
      break;
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1, 1);
  return MENU_HOLD;
}

/******************************************************************************/

static int
rpt_DECSASD(MENU_ARGS)
{
  return any_decrqss(the_title, "$}");
}

static int
rpt_DECSACE(MENU_ARGS)
{
  return any_decrqss(the_title, "*x");
}

static int
rpt_DECSCA(MENU_ARGS)
{
  return any_decrqss(the_title, "\"q");
}

static int
rpt_DECSCL(MENU_ARGS)
{
  return any_decrqss(the_title, "\"p");
}

static int
rpt_DECSCPP(MENU_ARGS)
{
  return any_decrqss(the_title, "$|");
}

static int
rpt_DECSLPP(MENU_ARGS)
{
  return any_decrqss(the_title, "t");
}

static int
rpt_DECSNLS(MENU_ARGS)
{
  return any_decrqss(the_title, "*|");
}

static int
rpt_DECSLRM(MENU_ARGS)
{
  return any_decrqss(the_title, "s");
}

static int
rpt_DECSSDT(MENU_ARGS)
{
  return any_decrqss(the_title, "$~");
}

static int
rpt_DECSTBM(MENU_ARGS)
{
  return any_decrqss(the_title, "r");
}

static int
rpt_SGR(MENU_ARGS)
{
  return any_decrqss(the_title, "m");
}

static int
rpt_DECELF(MENU_ARGS)
{
  return any_decrqss(the_title, "+q");
}

/*
 * VT420 manual shows "=}", but the terminal returns an error.  VT510 sequences
 * show "*}".
 */
static int
rpt_DECLFKC(MENU_ARGS)
{
  return any_decrqss(the_title, "*}");
}

static int
rpt_DECSMKR(MENU_ARGS)
{
  return any_decrqss(the_title, "+r");
}

/******************************************************************************/

/*
 * DECCIR returns single characters separated by semicolons.  It's not clear
 * (unless you test on a DEC terminal) from the documentation, which only cites
 * their values.  This function returns an equivalent-value, recovering from
 * the bogus implementations that return a decimal number.
 */
static int
scan_chr(char *str, int *pos, int toc)
{
  int value = 0;
  while (str[*pos] != '\0' && str[*pos] != toc) {
    value = (value * 256) + (unsigned char)str[*pos];
    *pos += 1;
  }
  if (str[*pos] == toc)
    *pos += 1;
  return value;
}

static void
show_DataIntegrity(char *report)
{
  int pos = 0;
  int code = scanto(report, &pos, 'n');
  char *show;

  switch(code) {
  case 70: show = "No communication errors"; break;
  case 71: show = "Communication errors"; break;
  case 73: show = "Not reported since last power-up or RIS"; break;
  default: show = SHOW_FAILURE;
  }
  show_result(show);
}

/*
 * From Kermit 3.13 & VT220 pocket guide
 *
 * Request  CSI 1 $ w             cursor information report
 * Response DCS 1 $ u Pr; Pc; Pp; Srend; Satt; Sflag; Pgl; Pgr; Scss; Sdesig ST
 *        where   Pr is cursor row (counted from origin as 1,1)
 *                Pc is cursor column
 *                Pp is 1, video page, a constant for VT320s
 *                Srend = 40h + 8 (rev video on) + 4 (blinking on)
 *                                 + 2 (underline on) + 1 (bold on)
 *                Satt = 40h + 1  (selective erase on)
 *                Sflag = 40h + 8 (autowrap pending) + 4 (SS3 pending)
 *                                + 2 (SS2 pending) + 1 (Origin mode on)
 *                Pgl = char set in GL (0 = G0, 1 = G1, 2 = G2, 3 = G3)
 *                Pgr = char set in GR (same as for Pgl)
 *                Scss = 40h + 8 (G3 is 96 char) + 4 (G2 is 96 char)
 *                                + 2 (G1 is 96 char) + 1 (G0 is 96 char)
 *                Sdesig is string of character idents for sets G0...G3, with
 *                                no separators between set idents.
 *                If NRCs are active the set idents (all 94 byte types) are:
 *                British         A       Italian         Y
 *                Dutch           4       Norwegian/Danish ' (hex 60) or E or 6
 *                Finnish         5 or C  Portuguese      %6 or g or L
 *                French          R or f  Spanish         Z
 *                French Canadian 9 or Q  Swedish         7 or H
 *                German          K       Swiss           =
 *                Hebrew          %=
 *                (MS Kermit uses any choice when there are multiple)
 */

#define show_DECCIR_flag(value, mask, string) \
  if (value & mask) { value &= ~mask; show_result(string); }

static void
show_DECCIR(char *report)
{
  int Pr, Pc, Pp, Srend, Satt, Sflag, Pgl, Pgr, Scss, Sdesig;
  int pos = 3;  /* skip "1$u" */
  int n;

  Pr    = scanto(report, &pos, ';');
  Pc    = scanto(report, &pos, ';');
  Pp    = scanto(report, &pos, ';');
  vt_move(5,10); show_result("Cursor (%d,%d), page %d", Pr, Pc, Pp);

  Srend = scan_chr(report, &pos, ';');
  vt_move(6,10);
  if (Srend & 0x40) {
    show_DECCIR_flag(Srend, 0x40, "Rendition:");
    if (Srend == 0) show_result(" normal");
    show_DECCIR_flag(Srend, 0x08, " reverse");
    show_DECCIR_flag(Srend, 0x04, " blinking");
    show_DECCIR_flag(Srend, 0x02, " underline");
    show_DECCIR_flag(Srend, 0x01, " bold");
  }
  if (Srend) show_result(" -> unknown rendition (0x%x)", Srend);

  Satt  = scan_chr(report, &pos, ';');
  vt_move(7,10);
  switch(Satt) {
  case 0x40: show_result("Selective erase: off"); break;
  case 0x41: show_result("Selective erase: ON"); break;
  default:   show_result("Selective erase: unknown (0x%x)", Satt);
  }

  Sflag = scan_chr(report, &pos, ';');
  vt_move(8,10);
  if (Sflag & 0x40) {
    show_DECCIR_flag(Sflag, 0x40, "Flags:");
    show_DECCIR_flag(Sflag, 0x08, " autowrap pending");
    show_DECCIR_flag(Sflag, 0x04, " SS3 pending");
    show_DECCIR_flag(Sflag, 0x02, " SS2 pending");
    show_DECCIR_flag(Sflag, 0x01, " origin-mode on");
  } else {
    show_result(" -> unknown flag (0x%x)", Sflag);
  }

  Pgl   = scanto(report, &pos, ';');
  Pgr   = scanto(report, &pos, ';');
  vt_move(9,10);
  show_result("Char set in GL: G%d, Char set in GR: G%d", Pgl, Pgr);

  Scss  = scan_chr(report, &pos, ';');
  vt_move(10,10);
  if (Scss & 0x40) {
    show_DECCIR_flag(Scss, 0x40, "Char set sizes:");
    show_DECCIR_flag(Scss, 0x08, " G3 is 96 char");
    show_DECCIR_flag(Scss, 0x04, " G2 is 96 char");
    show_DECCIR_flag(Scss, 0x02, " G1 is 96 char");
    show_DECCIR_flag(Scss, 0x01, " G0 is 96 char"); /* VT420 manual says this cannot happen */
  } else {
    show_result(" -> unknown char set size (0x%x)", Scss);
  }

  n = 11;
  vt_move(n, 10);
  show_result("Character set idents for G0...G3: ");
  while ((Sdesig = report[pos]) != '\0') {
    vt_move(++n, 12);
    ++pos;
    switch (Sdesig) {
    case 'B':
      show_result("ASCII");
      break;
    case '<':
      show_result("DEC supplemental");
      break;
    case '0':
      show_result("DEC special graphics");
      break;
    case 'A':
      show_result("British");
      break;
    case 'Y':
      show_result("Italian");
      break;
    case '4':
      show_result("Dutch");
      break;
    case '\'':
    case 'E':
    case '6':
      show_result("Norwegian/Danish");
      break;
    case '5':
    case 'C':
      show_result("Finnish");
      break;
    case 'g':
    case 'L':
      show_result("Portuguese");
      break;
    case 'R':
    case 'f':
      show_result("French");
      break;
    case 'Z':
      show_result("Spanish");
      break;
    case '9':
    case 'Q':
      show_result("French Canadian");
      break;
    case '7':
    case 'H':
      show_result("Swedish");
      break;
    case 'K':
      show_result("German");
      break;
    case '=':
      show_result("Swiss");
      break;
    case '%':
      if ((Sdesig = report[pos]) != '\0') {
        ++pos;
        switch(Sdesig) {
        case '=':
          show_result("Hebrew");
          break;
        case '6':
          show_result("Portuguese");
          break;
        default:  show_result(" unknown (0x%x)", Sdesig);
        }
      }
      break;
    default:  show_result(" unknown (0x%x)", Sdesig);
    }
  }
}

/*
 * Request  CSI 2 $ w             tab stop report
 * Response DCS 2 $ u Pc/Pc/...Pc ST
 *        Pc are column numbers (from 1) where tab stops occur. Note the
 *        separator "/" occurs in a real VT320 but should have been ";".
 */
static void
show_DECTABSR(char *report)
{
  int pos = 3;  /* skip "2$u" */
  int stop;
  char *buffer = malloc(strlen(report));

  *buffer = '\0';
  strcat(report, "/"); /* simplify scanning */
  while ((stop = scanto(report, &pos, '/')) != 0) {
    sprintf(buffer + strlen(buffer), " %d", stop);
  }
  println("");
  show_result("Tab stops:%s", buffer);
  free(buffer);
}

static void
show_ExtendedCursorPosition(char *report)
{
  int pos = 0;
  int Pl = scan_any(report, &pos, 'R');
  int Pc = scan_any(report, &pos, 'R');
  int Pp = scan_any(report, &pos, 'R');

  if (Pl != 0 && Pc != 0) {
    if (Pp != 0)
      show_result("Line %d, Column %d, Page %d", Pl, Pc, Pp);
    else
      show_result("Line %d, Column %d (Page?)", Pl, Pc);
  } else
    show_result(SHOW_FAILURE);
}

static void
show_keypress(int row, int col)
{
  char *report;
  char last[BUFSIZ];

  last[0] = '\0';
  vt_move(row++,1);
  println("When you are done, press any key twice to quit.");
  vt_move(row,col);
  fflush(stdout);
  while (strcmp(report = instr(), last)) {
    vt_move(row,col);
    vt_clear(0);
    chrprint(report);
    strcpy(last, report);
  }
}

static void
show_MultisessionStatus(char *report)
{
  int pos = 0;
  int Ps1 = scan_any(report, &pos, 'n');
  int Ps2 = scanto(report, &pos, 'n');
  char *show;

  switch (Ps1) {
  case 80: show = "SSU sessions enabled (%d max)";               break;
  case 81: show = "SSU sessions available but pending (%d max)"; break;
  case 83: show = "SSU sessions not ready";                      break;
  case 87: show = "Sessions on separate lines";                  break;
  default: show = SHOW_FAILURE;
  }
  show_result(show, Ps2);
}

/******************************************************************************/

/*
 * VT400 & up.
 * DECBI - Back Index
 * This control function moves the cursor backward one column.  If the cursor
 * is at the left margin, then all screen data within the margin moves one
 * column to the right.  The column that shifted past the right margin is lost.
 * 
 * Format:  ESC 6
 * Description:
 * DECBI adds a new column at the left margin with no visual attributes.  DECBI
 * is not affected by the margins.  If the cursor is at the left border of the
 * page when the terminal received DECBI, then the terminal ignores DECBI.
 */
static int
tst_DECBI(MENU_ARGS)
{
  int n, m;
  int last = max_lines - 4;
  int final = min_cols / 4;

  for (n = final; n > 0; n--) {
    cup(1,1);
    if (n != final) {
      for (m = 0; m < 4; m++)
        decbi();
    }
    printf("%3d", n);
  }

  vt_move(last,1);
  vt_clear(0);

  println(the_title);
  println("If your terminal supports DECBI (backward index), then the top row");
  printf("should be numbered 1 through %d.\n", final);
  return MENU_HOLD;
}

static int
tst_DECBKM(MENU_ARGS)
{
  char *report;

  vt_move(1,1);
  println(the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  reset_inchar();
  decbkm(TRUE);
  println("Press the backspace key");
  vt_move(3,10);
  report = instr();
  chrprint(report);
  show_result(!strcmp(report, "\010") ? SHOW_SUCCESS : SHOW_FAILURE);

  reset_inchar();
  vt_move(5,1);
  decbkm(FALSE);
  println("Press the backspace key again");
  vt_move(6,10);
  report = instr();
  chrprint(report);
  show_result(!strcmp(report, "\177") ? SHOW_SUCCESS : SHOW_FAILURE);

  vt_move(max_lines-1,1);
  restore_ttymodes();
  return MENU_HOLD;
}

/*
 * VT400 & up
 * Change Attributes in Rectangular Area
 */
static int
tst_DECCARA(MENU_ARGS)
{
  int last = max_lines - 4;
  int top = 5;
  int left = 5;
  int right = 45;
  int bottom = max_lines-10;

  decsace(TRUE);
  decaln(); /* fill the screen */
  deccara(top, left, bottom, right, 7); /* invert a rectangle) */
  deccara(top+1, left+1, bottom-1, right-1, 0); /* invert a rectangle) */

  vt_move(last, 1);
  vt_clear(0);

  println(the_title);
  println("There should be an open rectangle formed by reverse-video E's");
  holdit();

  decsace(FALSE);
  decaln(); /* fill the screen */
  deccara(top, left, bottom, right, 7); /* invert a rectangle) */
  deccara(top+1, left+1, bottom-1, right-1, 0); /* invert a rectangle) */

  vt_move(last, 1);
  vt_clear(0);

  println(the_title);
  println("There should be an open rectangle formed by reverse-video E's");
  println("combined with wrapping at the margins.");
  return MENU_HOLD;
}

static int
tst_DECCIR(MENU_ARGS)
{
  return any_decrqpsr(PASS_ARGS, 1);
}

static int
tst_DECCKSR(MENU_ARGS, int Pid, char *the_csi)
{
  char *report;
  int pos = 0;

  vt_move(1,1);
  printf("Testing DECCKSR: %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("%s", the_csi);
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_dcs(report)) != 0
   && strip_terminator(report)
   && strlen(report) > 1
   && scanto(report, &pos, '!') == Pid
   && report[pos++] == '~'
   && (report = skip_digits(report+pos+1)) != 0
   && *report == '\0') {
    show_result(SHOW_SUCCESS);
  } else {
    show_result(SHOW_FAILURE);
  }

  restore_ttymodes();
  vt_move(max_lines-1, 1);
  return MENU_HOLD;
}

/*
 * VT400 & up.
 * Copy Rectangular area
 */
static int
tst_DECCRA(MENU_ARGS)
{
  int j;
  int top = 5;
  int left = 5;
  int right = 45;
  int bottom = max_lines-10;

  for (j = top; j < bottom; j++) {
    cup(j, left);  printf("*");
    cup(j, right); printf("*");
  }
  cup(top,left);
  for (j = left; j <= right; j++)
    printf("*");
  cup(bottom,left);
  for (j = left; j <= right; j++)
    printf("*");

  vt_move(max_lines-3,1);
  println(the_title);
  println("The box of *'s will be copied");
  holdit();

  deccra(top, left, bottom, right,  1, top+3, left + 4, 1);

  vt_move(max_lines-2, 1);
  vt_clear(0);

  println("The box should be copied, overlapping");
  return MENU_HOLD;
}

/*
 * VT400 & up.
 * Delete column.
 */
static int
tst_DECDC(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(n, last - n + 22); printf("*");
    cup(1,1); decdc(1);
  }
  cup(1,1); decdc(20);

  vt_move(last+1,1);
  println("If your terminal supports DECDC, there will be a column of *'s on the left");
  return MENU_HOLD;
}

/*
 * VT400 & up
 * Erase Rectangular area
 */
static int
tst_DECERA(MENU_ARGS)
{
  decaln();
  decera(5,5, max_lines-10, min_cols-5);

  vt_move(max_lines-3,1);
  vt_clear(0);

  println(the_title);
  println("There should be a rectangle cleared in the middle of the screen.");
  return MENU_HOLD;
}

/*
 * VT400 & up.
 *
 * DECFI - Forward Index
 * This control function moves the column forward one column.  If the cursor is
 * at the right margin, then all screen data within the margins moves one
 * column to the left.  The column shifted past the left margin is lost.
 * 
 * Format: ESC 9
 * Description:
 * DECFI adds a new column at the right margin with no visual attributes. 
 * DECFI is not affected by the margins.  If the cursor is at the right border
 * of the page when the terminal receives DECFI, then the terminal ignores
 * DECFI.
 */
static int
tst_DECFI(MENU_ARGS)
{
  int n, m;
  int last = max_lines - 4;
  int final = min_cols / 4;

  for (n = 1; n <= final; n++) {
    cup(1,min_cols-3);
    printf("%3d", n); /* leaves cursor in rightmost column */
    if (n != final) {
      for (m = 0; m < 4; m++)
        decfi();
    }
  }

  vt_move(last,1);
  vt_clear(0);

  println(the_title);
  println("If your terminal supports DECFI (forward index), then the top row");
  printf("should be numbered 1 through %d.\n", final);
  return MENU_HOLD;
}

/*
 * VT400 & up
 * Fill Rectangular area
 */
static int
tst_DECFRA(MENU_ARGS)
{
  decfra('*', 5,5, max_lines-10, min_cols-5);

  vt_move(max_lines-3,1);
  vt_clear(0);

  println(the_title);
  println("There should be a rectangle filled in the middle of the screen.");
  return MENU_HOLD;
}

/*
 * VT400 & up.
 * Insert column.
 */
static int
tst_DECIC(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(n, min_cols - 22 - last + n); printf("*");
    cup(1,1); decic(1);
  }
  decic(20);

  vt_move(last+1,1);
  println("If your terminal supports DECIC, there will be a column of *'s on the right");
  return MENU_HOLD;
}

static int
tst_DECKBUM(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  deckbum(TRUE);
  println("The keyboard is set for data processing.");
  show_keypress(3,10);

  vt_move(10,1);
  deckbum(FALSE);
  println("The keyboard is set for normal (typewriter) processing.");
  show_keypress(11,10);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

static int
tst_DECKPM(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  deckpm(TRUE);
  println("The keyboard is set for position reports.");
  show_keypress(3,10);

  vt_move(10,1);
  deckpm(FALSE);
  println("The keyboard is set for character codes.");
  show_keypress(11,10);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

static int
tst_DECNKM(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  decnkm(FALSE);
  println("Press one or more keys on the keypad.  They should generate numeric codes.");
  show_keypress(3,10);

  vt_move(10,1);
  decnkm(TRUE);
  println("Press one or more keys on the keypad.  They should generate control codes.");
  show_keypress(11,10);

  decnkm(FALSE);
  vt_move(max_lines-1,1);
  restore_ttymodes();
  return MENU_HOLD;
}

/*
 * VT400 & up
 * Reverse Attributes in Rectangular Area
 */
static int
tst_DECRARA(MENU_ARGS)
{
  int last = max_lines - 4;
  int top = 5;
  int left = 5;
  int right = 45;
  int bottom = max_lines-10;

  decsace(TRUE);
  decaln(); /* fill the screen */
  decrara(top, left, bottom, right, 7); /* invert a rectangle) */
  decrara(top+1, left+1, bottom-1, right-1, 7); /* invert a rectangle) */

  vt_move(last, 1);
  vt_clear(0);

  println(the_title);
  println("There should be an open rectangle formed by reverse-video E's");
  holdit();

  decsace(FALSE);
  decaln(); /* fill the screen */
  decrara(top, left, bottom, right, 7); /* invert a rectangle) */
  decrara(top+1, left+1, bottom-1, right-1, 7); /* invert a rectangle) */

  vt_move(last, 1);
  vt_clear(0);

  println(the_title);
  println("There should be an open rectangle formed by reverse-video E's");
  println("combined with wrapping at the margins.");
  return MENU_HOLD;
}

static int
tst_ISO_DECRPM(MENU_ARGS)
{
  int mode, Pa, Ps;
  char chr;
  char *report;

  static struct {
    int mode;
    char *name;
  } ansi_modes[] = {
    { GATM, "GATM" },
    { AM,   "AM"   },
    { CRM,  "CRM"  },
    { IRM,  "IRM"  },
    { SRTM, "SRTM" },
    { VEM,  "VEM"  },
    { HEM,  "HEM"  },
    { PUM,  "PUM"  },
    { SRM,  "SRM"  },
    { FEAM, "FEAM" },
    { FETM, "FETM" },
    { MATM, "MATM" },
    { TTM,  "TTM"  },
    { SATM, "SATM" },
    { TSM,  "TSM"  },
    { EBM,  "EBM"  },
    { LNM,  "LNM"  } };

  vt_move(1,1);
  printf("Testing %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for (mode = 0; mode < TABLESIZE(ansi_modes); mode++) {
    do_csi("%d$p", ansi_modes[mode].mode);
    report = instr();
    vt_move(mode+2,10);
    printf("%8s", ansi_modes[mode].name);
    if (LOG_ENABLED)
      fprintf(log_fp, "Testing %8s\n", ansi_modes[mode].name);
    chrprint(report);
    if ((report = skip_csi(report)) != 0
     && sscanf(report, "%d;%d$%c", &Pa, &Ps, &chr) == 3
     && Pa == ansi_modes[mode].mode
     && chr == 'y') {
      switch(Ps) {
      case 0:  show_result(" unknown");           break;
      case 1:  show_result(" set");               break;
      case 2:  show_result(" reset");             break;
      case 3:  show_result(" permanently set");   break;
      case 4:  show_result(" permanently reset"); break;
      default: show_result(" ?");                 break;
      }
    } else {
      show_result(SHOW_FAILURE);
    }
  }

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

static int
tst_DEC_DECRPM(MENU_ARGS)
{
  int mode, Pa, Ps;
  char chr;
  char *report;
  char *show;

  static struct {
    int mode;
    char *name;
  } dec_modes[] = {
    { DECCKM,  "DECCKM"  },
    { DECANM,  "DECANM"  },
    { DECCOLM, "DECCOLM" },
    { DECSCLM, "DECSCLM" },
    { DECSCNM, "DECSCNM" },
    { DECOM,   "DECOM"   },
    { DECAWM,  "DECAWM"  },
    { DECARM,  "DECARM"  },
    { DECPFF,  "DECPFF"  },
    { DECPEX,  "DECPEX"  },
    { DECTCEM, "DECTCEM" },
    { DECNRCM, "DECNRCM" },
    { DECHCCM, "DECHCCM" },
    { DECVCCM, "DECVCCM" },
    { DECPCCM, "DECPCCM" },
    { DECNKM,  "DECNKM"  },
    { DECBKM,  "DECBKM"  },
    { DECKBUM, "DECKBUM" },
    { DECVSSM, "DECVSSM" },
    { DECXRLM, "DECXRLM" },
    { DECKPM,  "DECKPM"  } };

  vt_move(1,1);
  printf("Testing %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for (mode = 0; mode < TABLESIZE(dec_modes); mode++) {
    do_csi("?%d$p", dec_modes[mode].mode);
    report = instr();
    vt_move(mode+2,10);
    printf("%8s", dec_modes[mode].name);
    if (LOG_ENABLED)
      fprintf(log_fp, "Testing %8s\n", dec_modes[mode].name);
    chrprint(report);
    if ((report = skip_csi(report)) != 0
     && sscanf(report, "?%d;%d$%c", &Pa, &Ps, &chr) == 3
     && Pa == dec_modes[mode].mode
     && chr == 'y') {
      switch(Ps) {
      case 0:  show = "unknown";           break;
      case 1:  show = "set";               break;
      case 2:  show = "reset";             break;
      case 3:  show = "permanently set";   break;
      case 4:  show = "permanently reset"; break;
      default: show = "?";                 break;
      }
    } else {
      show = SHOW_FAILURE;
    }
    show_result(show);
  }

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/* Test Window Report - VT400 */
static int
tst_DECRQDE(MENU_ARGS)
{
  char *report;
  char chr;
  int Ph, Pw, Pml, Pmt, Pmp;

  vt_move(1,1);
  println("Testing DECRQDE/DECRPDE Window Report");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("\"v");
  report = get_reply();
  vt_move(3,10);
  chrprint(report);

  if ((report = skip_csi(report)) != 0
   && sscanf(report, "%d;%d;%d;%d;%d\"%c",
      &Ph, &Pw, &Pml, &Pmt, &Pmp, &chr) == 6
   && chr == 'w') {
    vt_move(5, 10);
    show_result("lines:%d, cols:%d, left col:%d, top line:%d, page %d",
      Ph, Pw, Pml, Pmt, Pmp);
  } else {
    show_result(SHOW_FAILURE);
  }

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

static int
tst_DECRQSS(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Select active status display (DECSASD)",            rpt_DECSASD },
      { "Select attribute change extent (DECSACE)",          rpt_DECSACE },
      { "Set character attribute (DECSCA)",                  rpt_DECSCA },
      { "Set conformance level (DECSCL)",                    rpt_DECSCL },
      { "Set columns per page (DECSCPP)",                    rpt_DECSCPP },
      { "Set lines per page (DECSLPP)",                      rpt_DECSLPP },
      { "Set number of lines per screen (DECSNLS)",          rpt_DECSNLS },
      { "Set status line type (DECSSDT)",                    rpt_DECSSDT },
      { "Set left and right margins (DECSLRM)",              rpt_DECSLRM },
      { "Set top and bottom margins (DECSTBM)",              rpt_DECSTBM },
      { "Select graphic rendition (SGR)",                    rpt_SGR },
      { "Enable local functions (DECELF)",                   rpt_DECELF },
      { "Local function key control (DECLFKC)",              rpt_DECLFKC },
      { "Select modifier key reporting (DECSMKR)",           rpt_DECSMKR },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Status-Strings Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/* Request Terminal State Report */
static int
tst_DECRQTSR(MENU_ARGS)
{
  char *report;
  char *show;

  vt_move(1,1); println("Testing Terminal State Reports (DECRQTSR/DECTSR)");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("1$u");
  report = get_reply();

  vt_move(3,10);
  chrprint(report);

  if ((report = skip_dcs(report)) != 0
   && strip_terminator(report)
   && !strncmp(report, "1$s", 3)) {
    show = SHOW_SUCCESS;
  } else {
    show = SHOW_FAILURE;
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/* Test User-Preferred Supplemental Set - VT400 */
static int
tst_DECRQUPSS(MENU_ARGS)
{
  char *report;
  char *show;

  vt_move(1,1);
  println("Testing DECRQUPSS/DECAUPSS Window Report");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("&u");
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_dcs(report)) != 0
   && strip_terminator(report)) {
    if (!strcmp(report, "0!u%5"))
      show = "DEC Supplemental Graphic";
    else if (!strcmp(report, "1!uA"))
      show = "ISO Latin-1 supplemental";
    else
      show = "unknown";
  } else {
    show = SHOW_FAILURE;
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/*
 * Selective-Erase Rectangular area
 */
static int
tst_DECSERA(MENU_ARGS)
{
  int top = 5;
  int left = 5;
  int right = 45;
  int bottom = max_lines-10;
  int last = max_lines - 3;

  decaln();
  decsca(1);
  decfra('E', top+1, left+1, bottom-1, right-1);
  decsca(1);
  decsera(top, left, bottom, right); /* erase the inside */

  vt_move(last, 1);
  vt_clear(0);

  println(the_title);
  println("There should be an open rectangle formed by blanks on a background of E's");

  holdit();
  decaln();
  return MENU_NOHOLD;
}

/* FIXME: use DECRQSS to get reports */
static int
tst_DECSNLS(MENU_ARGS)
{
  int rows;

  vt_move(1,1); println("Testing Select Number of Lines per Screen (DECSNLS)");

  for (rows = 48; rows >= 24; rows -= 12) {
    set_tty_raw(TRUE);
    set_tty_echo(FALSE);

    printf("%d Lines/Screen: ", rows);
    decsnls(rows);
    decrqss("*|");
    chrprint(instr());
    println("");

    restore_ttymodes();
    holdit();
  }

  return MENU_NOHOLD;
}

static int
tst_DECTABSR(MENU_ARGS)
{
  return any_decrqpsr(PASS_ARGS, 2);
}

static int
tst_DSR_area_sum(MENU_ARGS)
{
  return tst_DECCKSR(PASS_ARGS, 1, "1;1;10;10;20;20*y");
}

static int
tst_DSR_cursor(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?6n", show_ExtendedCursorPosition);
}

static int
tst_DSR_data_ok(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?75n", show_DataIntegrity);
}

static int
tst_DSR_macrospace(MENU_ARGS)
{
  char *report;
  char *show;

  vt_move(1,1);
  printf("Testing DECMSR: %s\n", the_title);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do_csi("?62n");
  report = instr();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_csi(report)) != 0
   && (report = skip_digits(report)) != 0
   && !strcmp(report, "*{")) {
    show = SHOW_SUCCESS;
  } else {
    show = SHOW_FAILURE;
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1, 1);
  return MENU_HOLD;
}

static int
tst_DSR_memory_sum(MENU_ARGS)
{
  return tst_DECCKSR(PASS_ARGS, 1, "?63;1n");
}

static int
tst_DSR_multisession(MENU_ARGS)
{
  return any_DSR(PASS_ARGS, "?85n", show_MultisessionStatus);
}

int
tst_SRM(MENU_ARGS)
{
  int oldc, newc;

  vt_move(1,1);
  println(the_title);

  set_tty_raw(TRUE);

  set_tty_echo(FALSE);
  srm(FALSE);

  println("Local echo is enabled, remote echo disabled.  Press any keys, repeat to quit.");
  vt_move(3,10);

  oldc = -1;
  while ((newc = inchar()) != oldc)
    oldc = newc;

  set_tty_echo(TRUE);
  srm(TRUE);

  vt_move(10,1);
  println("Local echo is disabled, remote echo enabled.  Press any keys, repeat to quit.");
  vt_move(11,10);

  oldc = -1;
  while ((newc = inchar()) != oldc)
    oldc = newc;

  vt_move(max_lines-1,1);
  restore_ttymodes();
  return MENU_HOLD;
}

/******************************************************************************/

static int
tst_PageFormat(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test set columns per page (DECSCPP)",               not_impl },
      { "Test columns mode (DECCOLM)",                       not_impl },
      { "Test set lines per page (DECSLPP)",                 not_impl },
      { "Test set left and right margins (DECSLRM)",         not_impl },
      { "Test set vertical split-screen (DECVSSM)",          not_impl },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("Page Format Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

static int
tst_PageMovement(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Next Page (NP)",                               not_impl },
      { "Test Preceding Page (PP)",                          not_impl },
      { "Test Page Position Absolute (PPA)",                 not_impl },
      { "Test Page Position Backward (PPB)",                 not_impl },
      { "Test Page Position Relative (PPR)",                 not_impl },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("Page Format Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/*
 * The main vt100 module tests CUP, HVP, CUF, CUB, CUU, CUD
 */
static int
tst_VT420_cursor(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Back Index (DECBI)",                           tst_DECBI },
      { "Test Forward Index (DECFI)",                        tst_DECFI },
      { "Test Pan down (SU)",                                tst_SU },
      { "Test Pan up (SD)",                                  tst_SD},
      { "Test Vertical Cursor Coupling (DECVCCM)",           not_impl },
      { "Test Page Cursor Coupling (DECPCCM)",               not_impl },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Cursor-Movement Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/*
 * The main vt100 module tests IRM, DL, IL, DCH, ICH, ED, EL
 */
static int
tst_VT420_editing(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Delete Column (DECDC)",                        tst_DECDC },
      { "Erase Character",                                   tst_ECH },
      { "Test Insert Column (DECIC)",                        tst_DECIC },
      { "Test Protected-Areas (DECSCA)",                     tst_DECSCA },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Editing Sequence Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/*
 * The main vt100 module tests AM, LNM, DECKPAM, DECARM, DECAWM
 */
static int
tst_VT420_keyboard_ctl(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Backarrow key (DECBKM)",                       tst_DECBKM },
      { "Test Numeric keypad (DECNKM)",                      tst_DECNKM },
      { "Test Keyboard usage (DECKBUM)",                     tst_DECKBUM },
      { "Test Key position (DECKPM)",                        tst_DECKPM },
      { "Test Enable Local Functions (DECELF)",              not_impl },
      { "Test Local Function-Key Control (DECLFKC)",         not_impl },
      { "Test Select Modifier-Key Reporting (DECSMKR)",      not_impl }, /* DECEKBD */
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Keyboard-Control Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/*
 * These apply only to VT400's & above
 */
static int
tst_VT420_rectangle(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Change-Attributes in Rectangular Area (DECCARA)", tst_DECCARA },
      { "Test Copy Rectangular area (DECCRA)",               tst_DECCRA },
      { "Test Erase Rectangular area (DECERA)",              tst_DECERA },
      { "Test Fill Rectangular area (DECFRA)",               tst_DECFRA },
      { "Test Reverse-Attributes in Rectangular Area (DECRARA)", tst_DECRARA },
      { "Test Selective-Erase Rectangular area (DECSERA)",   tst_DECSERA },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Rectangular Area Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/* UDK and rectangle-checksum status are available only on VT400 */

static int
tst_VT420_report_device(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Extended Cursor-Position",                     tst_DSR_cursor },
      { "Test Printer Status",                               tst_DSR_printer },
      { "Test UDK Status",                                   tst_DSR_userkeys },
      { "Test Keyboard Status",                              tst_DSR_keyboard },
      { "Test Locator Status",                               tst_DSR_locator },
      { "Test Macro Space",                                  tst_DSR_macrospace },
      { "Test Memory Checksum",                              tst_DSR_memory_sum },
      { "Test Data Integrity",                               tst_DSR_data_ok },
      { "Test Multiple Session Status",                      tst_DSR_multisession },
      { "Test Checksum of Rectangular Area",                 tst_DSR_area_sum },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Device Status Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

static int
tst_VT420_report_presentation(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Cursor Information Report (DECCIR)",                tst_DECCIR },
      { "Tab Stop Report (DECTABSR)",                        tst_DECTABSR },
      { "ANSI Mode Report (DECRPM)",                         tst_ISO_DECRPM },
      { "DEC Mode Report (DECRPM)",                          tst_DEC_DECRPM },
      { "Restore Presentation State (DECRSPS)",              not_impl },
      { "Status-String Report (DECRQSS)",                    tst_DECRQSS },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Device Status Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

static int
tst_VT420_report_terminal(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Restore Terminal State (DECRSTS)",                  not_impl },
      { "Terminal State Report (DECRQTS/DECTSR)",            tst_DECRQTSR },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Terminal State Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

static int
tst_VT420_reports(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Device Status Reports",                        tst_VT420_report_device },
      { "Test Presentation State Reports",                   tst_VT420_report_presentation },
      { "Test Terminal State Reports",                       tst_VT420_report_terminal },
      { "Test User-Preferred Supplemental Set (DECAUPSS)",   tst_DECRQUPSS },
      { "Test Window Report (DECRPDE)",                      tst_DECRQDE },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Reports");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

/* DECSASD and DECSSDT are for VT400's only */
static int
tst_VT420_screen(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Send/Receive mode (SRM)",                      tst_SRM },
      { "Test Select Number of Lines per Screen (DECSNLS)",  tst_DECSNLS },
      { "Test Status line (DECSASD/DECSSDT)",                tst_statusline },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Screen-Display Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/

int
tst_vt420(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test cursor-movement",                              tst_VT420_cursor },
      { "Test downloading soft-chars (DECDLD)",              tst_softchars },
      { "Test editing sequences",                            tst_VT420_editing },
      { "Test keyboard-control",                             tst_VT420_keyboard_ctl },
      { "Test macro-definition (DECDMAC)",                   not_impl },
      { "Test page-format controls",                         tst_PageFormat },
      { "Test page-movement controls",                       tst_PageMovement },
      { "Test printing functions",                           tst_printing },
      { "Test rectangular area functions",                   tst_VT420_rectangle },
      { "Test reporting functions",                          tst_VT420_reports },
      { "Test screen-display functions",                     tst_VT420_screen },
      { "Test soft terminal-reset",                          tst_DECSTR },
      { "Test user-defined keys (DECUDK)",                   tst_DECUDK },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("VT420 Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
