/* $Id$ */

#include <vttest.h>
#include <ttymodes.h>
#include <esc.h>
#include <ctype.h>

static
struct table {
    int key;
    char *msg;
} paritytable[] = {
    { 1, "NONE" },
    { 4, "ODD"  },
    { 5, "EVEN" },
    { -1, "" }
},nbitstable[] = {
    { 1, "8" },
    { 2, "7" },
    { -1,"" }
},speedtable[] = {
    {   0,    "50" },
    {   8,    "75" },
    {  16,   "110" },
    {  24,   "134.5"},
    {  32,   "150" },
    {  40,   "200" },
    {  48,   "300" },
    {  56,   "600" },
    {  64,  "1200" },
    {  72,  "1800" },
    {  80,  "2000" },
    {  88,  "2400" },
    {  96,  "3600" },
    { 104,  "4800" },
    { 112,  "9600" },
    { 120, "19200" },
    { 128, "38400" },
    { -1, "" }
},operating_level[] = {
    {   6, "VT102" },
    {  12, "VT125" },
    {  61, "VT100 family" },
    {  62, "VT200 family" },
    {  63, "VT300 family" },
    {  64, "VT400 family" },
    {  65, "VT500 family" },
    {  -1, "" }
},extensions[] = {
    {   1, "132 columns" },                             /* vt400 */
    {   2, "printer port" },                            /* vt400 */
    {   3, "ReGIS Graphics" },                          /* kermit */
    {   4, "Sixel Graphics" },                          /* kermit */
    {   6, "selective erase" },                         /* vt400 */
    {   7, "soft character set (DRCS)" },               /* vt400 */
    {   8, "user-defined keys" },                       /* vt400 */
    {   9, "national replacement character-sets" },     /* kermit */
    {  10, "text ruling vector" },                      /* ? */
    {  11, "25th status line" },                        /* ? */
    {  12, "Serbo-Croation (SCS)" },                    /* vt500 */
    {  13, "local editing mode" },                      /* kermit */
    {  14, "8-bit architecture" },                      /* ? */
    {  15, "DEC technical set" },                       /* vt400 */
    {  16, "locator device port (ReGIS)" },             /* kermit */
    {  17, "terminal state reports" },                  /* ? */
    {  18, "user windows" },                            /* vt400 */
    {  19, "two sessions" },                            /* vt400 */
    {  21, "horizontal scrolling" },                    /* vt400 */
    {  22, "color" },                                   /* vt500 */
    {  23, "Greek" },                                   /* vt500 */
    {  24, "Turkish" },                                 /* vt500 */
    {  29, "ANSI text locator" },                       /* DXterm */
    {  39, "page memory extension" },                   /* ? */
    {  42, "ISO Latin-2" },                             /* vt500 */
    {  44, "PC Term" },                                 /* vt500 */
    {  45, "Soft key mapping" },                        /* vt500 */
    {  46, "ASCII Terminal emulation (WYSE,TVI,...)" }, /* vt500 */
    {  -1, "" }
};

static int
legend(int n, char *input, char *word, char *description)
{
  int i;
  unsigned len = strlen(word);
  char buf[BUFSIZ];

  for (i = 0; input[i] != 0; i++) {
    if ((i == 0 || !isalpha(input[i-1]))
     && !strncmp(word, input+i, len)) {
      sprintf(buf, "%-8s %-3s = %s", n ? "" : "Legend:", word, description);
      show_result("%s", buf);
      println("");
      return n+1;
    }
  }
  return n;
}

static char *
lookup(struct table t[], int k)
{
  int i;
  for (i = 0; t[i].key != -1; i++) {
    if (t[i].key == k) return(t[i].msg);
  }
  return("BAD VALUE");
}

static int
scan_DA(char *str, int *pos)
{
  int save = *pos;
  int value = scanto(str, pos, ';');
  if (value == 0) {
    *pos = save;
    value = scanto(str, pos, 'c');
    if (str[*pos] != '\0')
      value = 0;
  }
  return value;
}

/******************************************************************************/

static int
tst_DA(MENU_ARGS)
{
  int i, found;
  char *report, *cmp;

  static char *attributes[][2] = { /* after CSI */
    { "?1;0c",   "No options (vanilla VT100)" },
    { "?1;1c",   "VT100 with STP" },
    { "?1;2c",   "VT100 with AVO (could be a VT102)" },
    { "?1;3c",   "VT100 with STP and AVO" },
    { "?1;4c",   "VT100 with GPO" },
    { "?1;5c",   "VT100 with STP and GPO" },
    { "?1;6c",   "VT100 with AVO and GPO" },
    { "?1;7c",   "VT100 with STP, AVO and GPO" },
    { "?1;11c",  "VT100 with PP and AVO" },
    { "?1;15c",  "VT100 with PP, GPO and AVO" },
    { "?2c",     "VT102" },
    { "?4;2c",   "VT132 with AVO" },
    { "?4;3c",   "VT132 with AVO and STP" },
    { "?4;6c",   "VT132 with GPO and AVO" },
    { "?4;7c",   "VT132 with GPO, AVO, and STP" },
    { "?4;11c",  "VT132 with PP and AVO" },
    { "?4;15c",  "VT132 with PP, GPO and AVO" },
    { "?6c",     "VT102" },
    { "?7c",     "VT131" },
    { "?12;5c",  "VT125" },           /* VT125 also has ROM version */
    { "?12;7c",  "VT125 with AVO" },  /* number, so this won't work */
    { "?5;0c",   "VK100 (GIGI)" },
    { "?5c",     "VK100 (GIGI)" },
    { "?62;1;2;4;6;8;9;15c",       "VT220" },
    { "?63;1;2;8;9c",              "VT320" },
    { "?63;1;2;4;6;8;9;15c",       "VT320" },
    { "?63;1;3;4;6;8;9;15;16;29c", "DXterm" },
    { "", "" }
  };

  vt_move(1,1);
  println("Test of Device Attributes report (what are you)");

  set_tty_raw(TRUE);
  da();
  report = get_reply();
  vt_move(3,1);
  vt_el(0);
  printf("Report is: ");
  chrprint(report);

  found = FALSE;
  if ((cmp = skip_csi(report)) != 0) {
    for (i = 0; *attributes[i][0] != '\0'; i++) {
      if (!strcmp(cmp, attributes[i][0])) {
        int n = 0;
        show_result(" -- means %s", attributes[i][1]);
        println("");
        n = legend(n, attributes[i][1], "STP", "Processor Option");
        n = legend(n, attributes[i][1], "AVO", "Advanced Video Option");
        n = legend(n, attributes[i][1], "GPO", "Graphics Processor Option");
        n = legend(n, attributes[i][1], "PP",  "Printer Port");
        found = TRUE;
        break;
      }
    }
  }
  if (!found) { /* this could be a vt200+ with some options disabled */
    if (cmp != 0 && *cmp == '?') {
      int reportpos = 1;
      int value = scan_DA(cmp, &reportpos);
      show_result("%s\n", lookup(operating_level, value));
      println("");
      while ((value = scan_DA(cmp, &reportpos)) != 0) {
        printf("   ");
        show_result("%d = %s\n", value, lookup(extensions, value));
        println("");
      }
      found = TRUE;
    }
  }
  if (!found)
    show_result(" -- Unknown response, refer to the manual");

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/*
 * Applies to VT220 & up (probably no VT100's).
 * Expected reply (from VT420 manual):
 *      CSI > 4 1 ; Pv ; 0 c (Pv = firmware version)
 * From kermit:
 *      CSI > 2 4 ; Pv ; 0 c (Pv = "0 ; 0 ; 0", for "0.0")
 * I've seen also:
 *      CSI > 8 3 ; Pv ; 0 c  (e.g., Pv = "3 0 7 0 1")
 */
static int
tst_DA_2(MENU_ARGS)
{
  static const struct {
    int Pp;
    const char *name;
  } tbl[] = {
    {  1,  "VT220" },
    { 18,  "VT330" },
    { 19,  "VT340" },
    { 24,  "kermit" },
    { 28,  "DECterm" },
    { 41,  "VT420" },
  };

  char *report;
  int Pp, Pv, Pc;
  char ch;
  char *show = SHOW_FAILURE;
  size_t n;

  vt_move(1,1); println("Testing Secondary Device Attributes (Firmware version)");

  set_tty_raw(TRUE);
  do_csi(">c"); /* or "CSI > 0 c" */
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_csi(report)) != 0) {
    if (sscanf(report, ">%d;%d;%d%c", &Pp, &Pv, &Pc, &ch) == 4
     && ch == 'c') {
      const char *name = "unknown";
      show = SHOW_SUCCESS;
      for (n = 0; n < TABLESIZE(tbl); n++) {
        if (Pp == tbl[n].Pp) {
          name = tbl[n].name;
          break;
        }
      }
      vt_move(4,10); printf("Pp=%d (%s)", Pp, name);
      vt_move(5,10); printf("Pv=%d, firmware version %d.%d", Pv, Pv/10, Pv%10);
      vt_move(6,10); printf("Pc=%d, ROM cartridge registration number", Pc);
    }
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/*
 * VT400 (reply is a hexidecimal string)
 */
static int
tst_DA_3(MENU_ARGS)
{
  char *report;
  char *show;

  vt_move(1,1); println("Testing Tertiary Device Attributes (unit ID)");

  set_tty_raw(TRUE);
  do_csi("=c"); /* or "CSI = 0 c" */
  report = get_reply();
  vt_move(3,10);
  chrprint(report);
  if ((report = skip_dcs(report)) != 0
   && strip_terminator(report) != 0
   && *report++ == '!'
   && *report++ == '|'
   && strlen(report) != 0) {
    show = SHOW_SUCCESS;
  } else {
    show = SHOW_FAILURE;
  }
  show_result(show);

  restore_ttymodes();
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/* Not supported above VT320 */
static int
tst_DECREQTPARM(MENU_ARGS)
{
  int parity, nbits, xspeed, rspeed, clkmul, flags;
  int reportpos;
  char *report, *report2, *cmp;

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);
  vt_move(2,1);
  println("Test of the \"Request Terminal Parameters\" feature, argument 0.");
  vt_move(3,1);
  decreqtparm(0);
  report = get_reply();
  vt_move(5,1);
  vt_el(0);
  printf("Report is: ");
  chrprint(report);

  if ((cmp = skip_csi(report)) != 0)
    report = cmp;

  if (strlen(report) < 14
   || report[0] != '2'
   || report[1] != ';')
    println(" -- Bad format");
  else {
    reportpos = 2;
    parity = scanto(report, &reportpos, ';');
    nbits  = scanto(report, &reportpos, ';');
    xspeed = scanto(report, &reportpos, ';');
    rspeed = scanto(report, &reportpos, ';');
    clkmul = scanto(report, &reportpos, ';');
    flags  = scanto(report, &reportpos, 'x');

    if (parity == 0 || nbits == 0 || clkmul == 0) println(" -- Bad format");
    else                                          println(" -- OK");

    show_result(
      "This means: Parity %s, %s bits, xmitspeed %s, recvspeed %s.\n",
      lookup(paritytable, parity),
      lookup(nbitstable, nbits),
      lookup(speedtable, xspeed),
      lookup(speedtable, rspeed));
    show_result("(CLoCk MULtiplier = %d, STP option flags = %d)\n", clkmul, flags);
  }

  vt_move(10,1);
  println("Test of the \"Request Terminal Parameters\" feature, argument 1.");
  vt_move(11,1);
  decreqtparm(1);       /* Does the same as decreqtparm(0), reports "3" */
  report2 = get_reply();
  vt_move(13,1);
  vt_el(0);
  printf("Report is: ");
  chrprint(report2);

  if ((cmp = skip_csi(report2)) != 0)
    report2 = cmp;

  if (strlen(report2) < 1
   || report2[0] != '3')
    println(" -- Bad format");
  else {
    report2[0] = '2';
    if (!strcmp(report,report2)) println(" -- OK");
    else                         println(" -- Bad format");
  }
  vt_move(max_lines,1);

  restore_ttymodes();
  return MENU_HOLD;
}

static int
tst_DSR(MENU_ARGS)
{
  int found;
  char *report, *cmp;

  set_tty_raw(TRUE);
  vt_move(1,1);
  printf("Test of Device Status Report 5 (report terminal status).");
  vt_move(2,1);
  dsr(5);
  report = get_reply();
  vt_move(2,1);
  vt_el(0);
  printf("Report is: ");
  chrprint(report);

  if ((cmp = skip_csi(report)) != 0)
    found = !strcmp(cmp, "0n") || !strcmp(cmp, "3n");
  else
    found = 0;

  if (found)
    show_result(" -- means \"TERMINAL OK\"");
  else
    show_result(" -- Unknown response!");

  vt_move(4,1);
  println("Test of Device Status Report 6 (report cursor position).");
  vt_move(5,1);
  dsr(6);
  report = get_reply();
  vt_move(5,1);
  vt_el(0);
  printf("Report is: ");
  chrprint(report);

  if ((cmp = skip_csi(report)) != 0)
    found = !strcmp(cmp,"5;1R");
  else
    found = 0;

  if (found)
    show_result(" -- OK");
  else
    show_result(" -- Unknown response!");

  vt_move(max_lines-1,1);
  restore_ttymodes();
  return MENU_HOLD;
}

static int
tst_ENQ(MENU_ARGS)
{
  char *report;

  vt_move(5,1);
  println("This is a test of the ANSWERBACK MESSAGE. (To load the A.B.M.");
  println("see the TEST KEYBOARD part of this program). Below here, the");
  println("current answerback message in your terminal should be");
  println("displayed. Finish this test with RETURN.");
  vt_move(10,1);

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);
  inflush();
  printf("%c", 5); /* ENQ */
  report = get_reply();
  vt_move(10,1);
  chrprint(report);
  vt_move(12,1);

  restore_ttymodes();
  return MENU_HOLD;
}

static int
tst_NLM(MENU_ARGS)
{
  char *report;

  vt_move(1,1);
  println("Test of LineFeed/NewLine mode.");
  vt_move(3,1);
  sm("20");
  set_tty_crmod(FALSE);
  printf("NewLine mode set. Push the RETURN key: ");
  report = instr();
  vt_move(4,1);
  vt_el(0);
  chrprint(report);
  if (!strcmp(report, "\015\012")) show_result(" -- OK");
  else                             show_result(" -- Not expected");
  vt_move(6,1);
  rm("20");
  printf("NewLine mode reset. Push the RETURN key: ");
  report = instr();
  vt_move(7,1);
  vt_el(0);
  chrprint(report);
  if (!strcmp(report, "\015")) show_result(" -- OK");
  else                         show_result(" -- Not expected");
  vt_move(9,1);

  restore_ttymodes();
  return MENU_HOLD;
}

/******************************************************************************/
int
tst_reports(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                                   0 },
      { "<ENQ> (AnswerBack Message)",                             tst_ENQ },
      { "Set/Reset Mode - LineFeed / Newline",                    tst_NLM },
      { "Device Status Report (DSR)                 VT100 & up",  tst_DSR },
      { "Primary Device Attributes (DA)             VT100 & up",  tst_DA },
      { "Secondary Device Attributes (DA)           VT220 & up",  tst_DA_2 },
      { "Tertiary Device Attributes (DA)            VT420",       tst_DA_3 },
      { "Request Terminal Parameters (DECREQTPARM)  VT100",       tst_DECREQTPARM },
      { "",                                                       0 }
    };

  do {
    vt_clear(2);
    title(0); printf("Terminal Reports/Responses");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
