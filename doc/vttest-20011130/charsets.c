/* $Id$ */

/*
 * Test character-sets (e.g., SCS control, DECNRCM mode)
 */
#include <vttest.h>
#include <esc.h>

/* the values, where specified, correspond to the keyboard-language codes */
typedef enum {
  ASCII = 1,
  British = 2,
  Flemish = 3,
  French_Canadian = 4,
  Danish = 5,
  Finnish = 6,
  German = 7,
  Dutch = 8,
  Italian = 9,
  Swiss_French = 10,
  Swiss_German = 11,
  Swiss,
  Swedish = 12,
  Norwegian_Danish = 13,
  French = 14,
  Spanish = 15,
  Portugese = 16,
  Hebrew = 17,
  DEC_Spec_Graphic,
  DEC_Supp,
  DEC_Supp_Graphic,
  DEC_Tech,
  British_Latin_1,
  Unknown
} National;

static const struct {
  National code;  /* internal name (chosen to sort properly!) */
  int allow96;    /* flag for 96-character sets (e.g., GR mapping) */
  int order;      /* check-column so we can mechanically-sort this table */
  int model;      /* 0=base, 2=vt220, 3=vt320, etc. */
  char *final;    /* end of SCS string */
  char *name;     /* the string we'll show the user */
} KnownCharsets[] = {
  { ASCII,             0, 0, 0, "B",    "US ASCII" },
  { British,           0, 0, 0, "A",    "British" },
  { British_Latin_1,   1, 0, 3, "A",    "Latin-1" },
  { DEC_Spec_Graphic,  0, 0, 0, "0",    "DEC Special Graphics" },
  { DEC_Supp,          0, 0, 2, "<",    "DEC Supplemental" },
  { DEC_Supp_Graphic,  0, 0, 3, "%5",   "DEC Supplemental Graphic" },
  { DEC_Tech,          0, 0, 3, ">",    "DEC Technical" },
  { Danish,            0, 0, 0, "?",    "Danish" },
  { Dutch,             0, 0, 2, "4",    "Dutch" },
  { Finnish,           0, 0, 2, "5",    "Finnish" },
  { Finnish,           0, 1, 2, "C",    "Finnish" },
  { Flemish,           0, 0, 0, "?",    "Flemish" },
  { French,            0, 0, 2, "R",    "French" },
  { French_Canadian,   0, 0, 2, "Q",    "French Canadian" },
  { German,            0, 0, 2, "K",    "German" },
  { Hebrew,            0, 0, 3, "%=",   "Hebrew" },
  { Italian,           0, 0, 2, "Y",    "Italian" },
  { Norwegian_Danish,  0, 0, 3, "`",    "Norwegian/Danish" },
  { Norwegian_Danish,  0, 1, 2, "E",    "Norwegian/Danish" },
  { Norwegian_Danish,  0, 2, 2, "6",    "Norwegian/Danish" },
  { Portugese,         0, 0, 3, "%6",   "Portugese" },
  { Spanish,           0, 0, 2, "Z",    "Spanish" },
  { Swedish,           0, 0, 2, "7",    "Swedish" }, /* or "H" */
  { Swiss,             0, 0, 2, "=",    "Swiss" },
  { Swiss_French,      0, 0, 0, "?",    "Swiss (French)" },
  { Swiss_German,      0, 0, 0, "?",    "Swiss (German)" },
  { Unknown,           0, 0, 0, "?",    "Unknown" }
};

static int national;

static int current_Gx[4];

static char *
scs_params(char *dst, int g)
{
  int n = current_Gx[g];

  sprintf(dst, "%c%s",
    (KnownCharsets[n].allow96 && get_level() > 2)
      ? "?-./"[g]
      : "()*+"[g],
    KnownCharsets[n].final);
  return dst;
}

static void
do_scs(int g)
{
  char buffer[80];

  esc(scs_params(buffer, g));
}

static int
lookupCode(National code)
{
  int n;
  for (n = 0; n < TABLESIZE(KnownCharsets); n++) {
    if (KnownCharsets[n].code == code)
      return n;
  }
  return lookupCode(ASCII);
}

/* reset given Gg back to sane setting */
static int
sane_cs(int g)
{
  return lookupCode((g == 0)
    ? ASCII
    : (get_level() > 1)
      ? British_Latin_1       /* ...to get 8-bit codes 128-255 */
      : DEC_Supp_Graphic);
}

/* reset given Gg back to sane setting */
static int
reset_scs(int g)
{
  int n = sane_cs(g);
  do_scs(n);
  return n;
}

/* reset all of the Gn to sane settings */
static int
reset_charset(MENU_ARGS)
{
  int n, m;

  decnrcm(national = FALSE);
  for (n = 0; n < 4; n++) {
    m = sane_cs(n);
    if (m != current_Gx[n]) {
      current_Gx[n] = m;
      do_scs(n);
    }
  }
  return MENU_NOHOLD;
}

static int the_code;
static int the_list[TABLESIZE(KnownCharsets)+2];

static int
lookup_Gx(MENU_ARGS)
{
  int n;
  the_code = -1;
  for (n = 0; n < TABLESIZE(KnownCharsets); n++) {
    if (the_list[n]
     && !strcmp(the_title, KnownCharsets[n].name)) {
      the_code = n;
      break;
    }
  }
  return MENU_NOHOLD;
}

static void
specify_any_Gx(int g)
{
  MENU my_menu[TABLESIZE(KnownCharsets)+2];
  int n, m;

  /*
   * Build up a menu of the character sets we will allow the user to specify.
   * There are a couple of tentative table entries (the "?" ones), which we
   * won't show in any event.  Beyond that, we limit some of the character sets
   * based on the emulation level (vt220 implements national replacement
   * character sets, for example, but not the 96-character ISO Latin-1).
   */
  for (n = m = 0; n < TABLESIZE(KnownCharsets); n++) {
    the_list[n] = 0;
    if (!strcmp(KnownCharsets[n].final, "?"))
      continue;
    if (get_level() < KnownCharsets[n].model)
      continue;
    if ((g == 0) && KnownCharsets[n].allow96)
      continue;
    if (m && !strcmp(my_menu[m-1].description, KnownCharsets[n].name))
      continue;
    my_menu[m].description = KnownCharsets[n].name;
    my_menu[m].dispatch = lookup_Gx;
    the_list[n] = 1;
    m++;
  }
  my_menu[m].description = "";
  my_menu[m].dispatch = 0;

  do {
    vt_clear(2);
    title(0); println("Choose character-set:");
  } while (menu(my_menu) && the_code < 0);

  current_Gx[g] = the_code;
}

static int
toggle_nrc(MENU_ARGS)
{
  national = !national;
  decnrcm(national);
  return MENU_NOHOLD;
}

static int
specify_G0(MENU_ARGS)
{
  specify_any_Gx(0);
  return MENU_NOHOLD;
}

static int
specify_G1(MENU_ARGS)
{
  specify_any_Gx(1);
  return MENU_NOHOLD;
}

static int
specify_G2(MENU_ARGS)
{
  specify_any_Gx(2);
  return MENU_NOHOLD;
}

static int
specify_G3(MENU_ARGS)
{
  specify_any_Gx(3);
  return MENU_NOHOLD;
}

static int
tst_layout(MENU_ARGS)
{
  char buffer[80];
  return tst_keyboard_layout(scs_params(buffer, 0));
}

static int
tst_vt100_charsets(MENU_ARGS)
{
  /* Test of:
     SCS    (Select character Set)
  */
  static const struct { char code; char *msg; } table[] = {
    { 'A', "UK / national" },
    { 'B', "US ASCII" },
    { '0', "Special graphics and line drawing" },
    { '1', "Alternate character ROM standard characters" },
    { '2', "Alternate character ROM special graphics" },
  };

  int i, j, g, cset;

  cup(1,10); printf("Selected as G0 (with SI)");
  cup(1,48); printf("Selected as G1 (with SO)");
  for (cset = 0; cset < TABLESIZE(table); cset++) {
    int row = 3 + (4 * cset);

    scs(1,'B');
    cup(row, 1);
    sgr("1");
    printf("Character set %c (%s)", table[cset].code, table[cset].msg);
    sgr("0");
    for (g = 0; g <= 1; g++) {
      int set_nrc = (get_level() >= 2 && table[cset].code == 'A');
      if (set_nrc)
        decnrcm(TRUE);
      scs(g, table[cset].code);
      for (i = 1; i <= 3; i++) {
        cup(row + i, 10 + 38 * g);
        for (j = 0; j <= 31; j++) {
          printf("%c", i * 32 + j);
        }
      }
      if (set_nrc != national)
        decnrcm(national);
    }
  }
  scs(0,'B');
  scs(1,'B');
  cup(max_lines,1); printf("These are the installed character sets. ");
  return MENU_HOLD;
}

static int
tst_shift_in_out(MENU_ARGS)
{
  /* Test of:
     SCS    (Select character Set)
  */
  static char *label[] = {
    "Selected as G0 (with SI)",
    "Selected as G1 (with SO)"
  };
  int i, j, cset;
  char buffer[80];

  cup(1,10); printf("These are the G0 and G1 character sets.");
  for (cset = 0; cset < 2; cset++) {
    int row = 3 + (4 * cset);

    scs(cset,'B');
    cup(row, 1);
    sgr("1");
    printf("Character set %s (%s)",
        KnownCharsets[current_Gx[cset]].final,
        KnownCharsets[current_Gx[cset]].name);
    sgr("0");

    cup(row, 48);
    printf("%s", label[cset]);

    esc(scs_params(buffer, cset));
    for (i = 1; i <= 3; i++) {
      cup(row + i, 10);
      for (j = 0; j <= 31; j++) {
        printf("%c", i * 32 + j);
      }
    }
    scs(cset,'B');
  }
  cup(max_lines,1);
  return MENU_HOLD;
}

static int
tst_vt220_locking(MENU_ARGS)
{
  /* Test of:
     SCS    (Select character Set)
  */
  static const struct {
    int upper;
    int mapped;
    char *code;
    char *msg;
  } table[] = {
    { 1, 1, "~", "G1 into GR (LS1R)" },
    { 0, 2, "n", "G2 into GL (LS2)"  }, /* "{" vi */
    { 1, 2, "}", "G2 into GR (LS2R)" },
    { 0, 3, "o", "G3 into GL (LS3)"  },
    { 1, 3, "|", "G3 into GR (LS3R)" },
  };

  int i, j, cset;

  cup(1,10); printf("Locking shifts, with NRC %s:",
    national ? "enabled" : "disabled");
  for (cset = 0; cset < TABLESIZE(table); cset++) {
    int row = 3 + (4 * cset);
    int map = table[cset].mapped;

    scs(1,'B');
    cup(row, 1);
    sgr("1");
    printf("Character set %s (%s)",
        KnownCharsets[current_Gx[map]].final,
        KnownCharsets[current_Gx[map]].name);
    sgr("0");

    cup(row, 48);
    printf("Maps %s", table[cset].msg);

    do_scs(map);
    esc(table[cset].code);
    for (i = 1; i <= 3; i++) {
      cup(row + i, 10);
      for (j = 0; j <= 31; j++) {
        printf("%c", table[cset].upper * 128 + i * 32 + j);
      }
    }
    reset_scs(cset);
  }
  scs(1,'B');
  cup(max_lines,1);
  return MENU_HOLD;
}

static int
tst_vt220_single(MENU_ARGS)
{
  int pass, x, y;

  for (pass = 0; pass < 2; pass++) {
    int g = pass + 2;

    vt_clear(2);
    cup(1,1);
    printf("Testing single-shift G%d into GL (SS%d) with NRC %s\n",
      g, g, national ? "enabled" : "disabled");
    printf("G%d is %s", g, KnownCharsets[current_Gx[g]].name);

    do_scs(g);
    for (y = 0; y < 16; y++) {
      for (x = 0; x < 6; x++) {
        int ch = y + (x * 16) + 32;
        cup(y+5, (x * 12) + 5);
        printf("%3d: (", ch);
        esc(pass ? "O" : "N");  /* SS3 or SS2 */
        printf("%c", ch);
        printf(")");
      }
    }

    cup(max_lines,1);
    holdit();
  }

  return MENU_NOHOLD;
}

/******************************************************************************/

/* Reset G0 to ASCII */
void
scs_normal(void)
{
  scs(0,'B');
}

/* Set G0 to Line Graphics */
void
scs_graphics(void)
{
  scs(0,'0');
}

int
tst_characters(MENU_ARGS)
{
  static char whatis_Gx[4][80];
  static char nrc_mesg[80];

  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Reset (ASCII for G0, G1, no NRC mode)",             reset_charset },
      { nrc_mesg,                                            toggle_nrc },
      { whatis_Gx[0],                                        specify_G0 },
      { whatis_Gx[1],                                        specify_G1 },
      { whatis_Gx[2],                                        specify_G2 },
      { whatis_Gx[3],                                        specify_G3 },
      { "Test VT100 Character Sets",                         tst_vt100_charsets },
      { "Test Shift In/Shift Out (SI/SO)",                   tst_shift_in_out },
      { "Test VT220 Locking Shifts",                         tst_vt220_locking },
      { "Test VT220 Single Shifts",                          tst_vt220_single },
      { "Test Soft Character Sets",                          not_impl },
      { "Test Keyboard Layout with G0 Selection",            tst_layout },
      { "",                                                  0 }
  };
  int n;

  reset_charset(PASS_ARGS); /* make the menu consistent */
  if (get_level() > 1 || input_8bits || output_8bits) {
    do {
      vt_clear(2);
      title(0); printf("Character-Set Tests");
      title(2); println("Choose test type:");
      sprintf(nrc_mesg, "%s National Replacement Character (NRC) mode",
        national ? "Disable" : "Enable");
      for (n = 0; n < 4; n++) {
        sprintf(whatis_Gx[n], "Specify G%d (now %s)",
            n, KnownCharsets[current_Gx[n]].name);
      }
    } while (menu(my_menu));
    return reset_charset(PASS_ARGS);
  } else {
    return tst_vt100_charsets(PASS_ARGS);
  }
}
