/* $Id$ */

#include <vttest.h>
#include <esc.h>

#define MAX_COLORS    8

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

static char     *colors[MAX_COLORS] =
{
  "black",       /* 30, 40 */
  "red",         /* 31, 41 */
  "green",       /* 32, 42 */
  "yellow",      /* 33, 43 */
  "blue",        /* 34, 44 */
  "magenta",     /* 35, 45 */
  "cyan",        /* 36, 46 */
  "white"        /* 37, 47 */
};

static int do_colors = TRUE;

static int next_word(char *s);
static void draw_box_caption(int x0, int y0, int x1, int y1, char **c);
static void draw_box_outline(int x0, int y0, int x1, int y1, int c);
static void draw_hline(int x0, int y0, int x1, int c);
static void draw_vline(int x0, int y0, int y1, int c);

/*
 * Pick an unusual color combination for testing, just in case the user's
 * got the background set to something different.
 */
static void
c_sgr(char *s)
{
  char temp[80];
  char *t = strchr(strcpy(temp, s), '0');
  int reset = FALSE;

  if (*temp == ';' || *temp == 0) {
    reset = TRUE;
  } else {
    for (t = temp; *t != 0; t++) {
      if (((t[0] == '0')
        && (t == temp || t[-1] == ';')
    && (t[1] == 0 || t[1] == ';'))
       || ((t[0] == ';')
        && (t[1] == ';'))) {
    reset = TRUE;
    break;
      }
    }
  }

  if (reset && do_colors) {
    sprintf(temp + strlen(temp), ";%d;%d", COLOR_YELLOW + 30, COLOR_BLUE + 40);
  }
  sgr(temp);
}

static void
draw_box_caption(int x0, int y0, int x1, int y1, char ** c)
{
  int x = x0, y = y0;
  int t;
  char *s;

  while ((s = *c++) != 0) {
    while ((t = *s++) != 0) {
      if (x == x0) {
        if (t == ' ')
          continue;
        cup(y, x);
        putchar(' '); x++;
      }
      putchar(t); x++;
      if ((t == ' ') && (next_word(s) > (x1-x-2))) {
    while (x < x1) {
      putchar(' '); x++;
    }
      }
      if (x >= x1) {
        putchar(' ');
        x = x0;
        y++;
      }
    }
  }
  while (y <= y1) {
    if (x == x0) {
      cup(y, x);
    }
    putchar(' ');
    if (++x >= x1) {
      putchar(' ');
      x = x0;
      y++;
    }
  }
}

static void
draw_box_outline(int x0, int y0, int x1, int y1, int c)
{
  draw_hline(x0, y0, x1, c);
  draw_hline(x0, y1, x1, c);
  draw_vline(x0, y0, y1, c);
  draw_vline(x1, y0, y1, c);
}

static void
draw_hline(int x0, int y0, int x1, int c)
{
  int n;

  cup(y0, x0);
  for (n = x0; n <= x1; n++)
    putchar(c);
}

static void
draw_vline(int x0, int y0, int y1, int c)
{
  int n;

  for (n = y0; n <= y1; n++) {
    cup(n, x0);
    putchar(c);
  }
}

static int
next_word(char *s)
{
  char *base;
  while (*s == ' ')
    s++;
  base = s;
  while (*s && *s != ' ')
    s++;
  return (s - base);
}

/*
 * Some terminals will reset colors with SGR-0; I've added the 39, 49 codes for
 * those that are ISO compliant.  (The black/white codes are for emulators
 * written by people who don't bother reading standards).
 */
static void
reset_colors(void)
{
  sgr("0;40;37;39;49");
  sgr("0");
}

static void
set_background(int bg)
{
  if (do_colors) {
    char temp[80];
    (void)sprintf(temp, "4%d", bg);
    sgr(temp);
  }
}

static void
set_color_pair(int fg, int bg)
{
  if (do_colors) {
    char temp[80];
    (void)sprintf(temp, "3%d;4%d", fg, bg);
    sgr(temp);
  }
}

static void
set_foreground(int fg)
{
  if (do_colors) {
    char temp[80];
    (void)sprintf(temp, "3%d", fg);
    sgr(temp);
  }
}

static void
set_test_colors(void)
{
  c_sgr("0");
}

/* Graphic rendition requires special handling with color, since SGR-0
 * is supposed to reset the colors as well.
 */
static void
show_graphic_rendition(void)
{
  ed(2);
  cup( 1,20); printf("Color/Graphic rendition test pattern:");
  cup( 4, 1); c_sgr("0");            printf("vanilla");
  cup( 4,40); c_sgr("0;1");          printf("bold");
  cup( 6, 6); c_sgr(";4");           printf("underline");
  cup( 6,45); c_sgr(";1");c_sgr("4");printf("bold underline");
  cup( 8, 1); c_sgr("0;5");          printf("blink");
  cup( 8,40); c_sgr("0;5;1");        printf("bold blink");
  cup(10, 6); c_sgr("0;4;5");        printf("underline blink");
  cup(10,45); c_sgr("0;1;4;5");      printf("bold underline blink");
  cup(12, 1); c_sgr("1;4;5;0;7");    printf("negative");
  cup(12,40); c_sgr("0;1;7");        printf("bold negative");
  cup(14, 6); c_sgr("0;4;7");        printf("underline negative");
  cup(14,45); c_sgr("0;1;4;7");      printf("bold underline negative");
  cup(16, 1); c_sgr("1;4;;5;7");     printf("blink negative");
  cup(16,40); c_sgr("0;1;5;7");      printf("bold blink negative");
  cup(18, 6); c_sgr("0;4;5;7");      printf("underline blink negative");
  cup(18,45); c_sgr("0;1;4;5;7");    printf("bold underline blink negative");
  cup(20, 6); c_sgr(""); set_foreground(9); printf("original foreground");
  cup(20,45); c_sgr(""); set_background(9); printf("original background");
  c_sgr(""); /* same as c_sgr("0") */

  decscnm(FALSE); /* Inverse video off */
  cup(max_lines-1,1); el(0); printf("Dark background. "); holdit();

  decscnm(TRUE); /* Inverse video */
  cup(max_lines-1,1); el(0); printf("Light background. "); holdit();
  decscnm(FALSE);
}

static void
show_line_deletions(void)
{
  int row;

  ed(2);
  cup(1,1);
  printf("This test deletes every third line from a list, marking cursor with '*'.\n");
  printf("The foreground and background should be yellow(orange) and blue, respectively.\n");

  for (row = 5; row <= max_lines; row++) {
    cup(row,1);
    printf("   row %3d: this is some text", row);
  }
  for (row = 7; row <= max_lines; row += 2 /* 3 - deletion */) {
    cup(row,1);
    dl(1);
    putchar('*');  /* cursor should be in column 1 */
  }
  cup(3,1);
  holdit();
}

static void
show_line_insertions(void)
{
  int row;

  ed(2);
  cup(1,1);
  printf("This test inserts after every second line in a list, marking cursor with '*'.\n");
  printf("The foreground and background should be yellow(orange) and blue, respectively.\n");

  for (row = 5; row <= max_lines; row++) {
    cup(row,1);
    printf("   row %3d: this is some text", row);
  }
  for (row = 7; row <= max_lines; row += 3 /* 2 + insertion */) {
    cup(row,1);
    il(1);
    putchar('*');  /* cursor should be in column 1 */
  }
  cup(3,1);
  holdit();
}

static int
show_test_pattern(MENU_ARGS)
/* generate a color test pattern */
{
  int i, j, k;

  reset_colors();
  ed(2);
  cup(1,1);
  printf("There are %d color combinations", MAX_COLORS * MAX_COLORS);

  for (k = 0; k <= 11; k += 11) {
    cup(k + 2, 1);
    printf("%dx%d matrix of foreground/background colors, bright *",
      MAX_COLORS, MAX_COLORS);

    if (k) {
      sgr("1");
      printf("on");
      sgr("0");
    } else {
      printf("off");
    }
    printf("*");

    for (i = 0; i < MAX_COLORS; i++) {
      cup(k + 3, (i+1) * 8 + 1);
      printf("%s", colors[i]);
    }

    for (i = 0; i < MAX_COLORS; i++) {
      cup(k + i + 4, 1);
      printf("%s", colors[i]);
    }

    for (i = 0; i < MAX_COLORS; i++) {
      for (j = 0; j < MAX_COLORS; j++) {
        if (k)
          sgr("1");
        set_color_pair(j, i);
        cup(k + 4 + i, (j+1) * 8 + 1);
        printf("Hello");
        reset_colors();
      }
    }
  }
  reset_colors();
  cup(max_lines-1, 1);
  return MENU_HOLD;
}

/*
 * "Real" color terminals support bce (background color erase).
 *
 * Set the foreground and background colors to something that's unusual.
 * Then clear the screen (the background should stick) and draw some nested
 * boxes (because that's simple). Use the ED, EL controls to clear away the
 * outer box, so we can exercise the various parameter combinations of each
 * of these.
 */
static int
simple_bce_test(MENU_ARGS)
{
  int i, j;
  static int top  = 3,    top2  = 7;    /* box margins */
  static int left = 10,   left2 = 18;
  static char *text1[] = {
    "The screen background should be blue, with a box made of asterisks",
    " and this caption, in orange (non-bold yellow). ",
    " There should be no cells with the default foreground or background.",
    0
  };
  static char *text2[] = {
    "The screen background should be black, with a box made of asterisks",
    " and this caption, in white (actually gray - it is not bold). ",
    " Only the asterisk box should be in color.",
    0
  };

  set_test_colors();
  ed(2);

  for (i = top; i < max_lines-top; i++) {
    cup(i, left);
    for (j = left; j < min_cols-left; j++) {
      putchar('X');
    }
  }

  draw_box_outline(left2, top2, min_cols-left2, max_lines-top2, '*');

  cup(top2-1, min_cols/2);
  ed(1); /* clear from home to cursor */
  cuf(1);
  el(0); /* clear from cursor to end of line */

  cup(max_lines - (top2-1), min_cols/2);
  ed(0); /* clear from cursor to end */
  cub(1);
  el(1); /* clear to beginning of line */

  for (i = top2; i <= max_lines-top2; i++) {
    cup(i, left2-1);
    el(1);
    cup(i, min_cols - (left2-1));
    el(0);
  }

  draw_box_caption(left2+1, top2+1, min_cols-left2-1, max_lines-top2-1, text1);

  cup(max_lines-1, 1);
  holdit();

  /* Now, set the background again just in case there's a glitch */
  set_foreground(COLOR_WHITE);
  set_background(COLOR_BLACK);

  cup(top2-1, min_cols/2);
  ed(1); /* clear from home to cursor */
  cuf(1);
  el(0); /* clear from cursor to end of line */

  cup(max_lines - (top2-1), min_cols/2);
  ed(0); /* clear from cursor to end */
  cub(1);
  el(1); /* clear to beginning of line */

  for (i = top2; i <= max_lines-top2; i++) {
    cup(i, left2-1);
    el(1);
    cup(i, min_cols - (left2-1));
    el(0);
  }

  draw_box_caption(left2+1, top2+1, min_cols-left2-1, max_lines-top2-1, text2);

  cup(max_lines-1, 1);
  holdit();

  reset_colors();
  return MENU_NOHOLD;
}

/*
 * Test the insert/delete line/character operations for color (bce) terminals
 * We'll test insert/delete line operations specially, because it is very hard
 * to see what is happening with the accordion test when it does not work.
 */
static int
test_color_insdel(MENU_ARGS)
{
  set_test_colors();

  show_line_insertions();
  show_line_deletions();

  /* The rest of the test can be done nicely with the standard vt100 test
   * for insert/delete, since it doesn't modify SGR.
   */
  tst_insdel(PASS_ARGS);
  reset_colors();
  return MENU_NOHOLD;
}

static int
test_color_screen(MENU_ARGS)
{
  set_test_colors();

  do_scrolling();
  show_graphic_rendition();
  reset_colors();
  return MENU_NOHOLD;
}

/*
 * VT220 and higher implement the 22, 24, 25 and 27 codes.
 * VT510 implements concealed text.
 *
 * ISO 6429 specifies additional SGR codes so that one needn't use SGR 0
 * to reset everything before switching, e.g., set/clear pairs are
 * bold      1/22
 * faint     2/22
 * italics   3/23
 * underline 4/24
 * blink     5/25
 * inverse   7/27
 * concealed 8/28
 */
static int
test_iso_6429_sgr(MENU_ARGS)
{
  set_test_colors();
  ed(2);
  cup( 1,20); printf("Extended/Graphic rendition test pattern:");
  cup( 4, 1); c_sgr("0");            printf("vanilla");
  cup( 4,40); c_sgr("0;1");          printf("bold");
  cup( 6, 6); c_sgr("22;4");         printf("underline");
  cup( 6,45); c_sgr("24;1;4");       printf("bold underline");
  cup( 8, 1); c_sgr("22;24;5");      printf("blink");
  cup( 8,40); c_sgr("25;5;1");       printf("bold blink");
  cup(10, 6); c_sgr("22;4;5");       printf("underline blink");
  cup(10,45); c_sgr("24;25;1;4;5");  printf("bold underline blink");
  cup(12, 1); c_sgr("22;24;25;7");   printf("negative");
  cup(12,40); c_sgr("1");            printf("bold negative");
  cup(14, 6); c_sgr("22;4;7");       printf("underline negative");
  cup(14,45); c_sgr("1;4;7");        printf("bold underline negative");
  cup(16, 1); c_sgr("22;24;5;7");    printf("blink negative");
  cup(16,40); c_sgr("1");            printf("bold blink negative");
  cup(18, 6); c_sgr("22;4");         printf("underline blink negative");
  cup(18,45); c_sgr("1");            printf("bold underline blink negative");
  cup(20, 6); c_sgr(""); set_foreground(9); printf("original foreground");
  cup(20,45); c_sgr(""); set_background(9); printf("original background");
  cup(22, 1); c_sgr(";8");           printf("concealed");
  cup(22,40); c_sgr("8;7");          printf("concealed negative");
  c_sgr(""); /* same as c_sgr("0") */
  printf(" <- concealed text");

  decscnm(FALSE); /* Inverse video off */
  cup(max_lines-1,1); el(0); printf("Dark background. "); holdit();

  decscnm(TRUE); /* Inverse video */
  cup(max_lines-1,1); el(0); printf("Light background. "); holdit();

  decscnm(FALSE);
  cup(max_lines-1,1); el(0); printf("Dark background. "); holdit();

  reset_colors();
  return MENU_NOHOLD;
}

/*
 */
static int
test_SGR_0(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);
  println("");
  println("ECMA-48 states that SGR 0 \"cancels the effect of any preceding occurrence");
  println("of SGR in the data stream regardless of the setting of the graphic rendition");
  println("combination mode (GRCM)\".");
  println("");
  println("");

  reset_colors();
  printf("You should see only black:");
  sgr("30;40");
  printf("SGR 30 and SGR 40 don't work");
  reset_colors();
  println(":up to here");

  reset_colors();
  printf("You should see only white:");
  sgr("37;47");
  printf("SGR 37 and SGR 47 don't work");
  reset_colors();
  println(":up to here");

  reset_colors();
  printf("You should see text here: ");
  sgr("30;40");
  sgr("0");
  printf("SGR 0 reset works (explicit 0)");
  println("");

  reset_colors();
  printf("................and here: ");
  sgr("37;47");
  sgr("");
  printf("SGR 0 reset works (default param)");
  println("");

  reset_colors();
  holdit();
  return MENU_NOHOLD;
}

/*
 * Allow user to test the same screens w/o colors.
 */
static int
toggle_color_mode(MENU_ARGS)
{
  do_colors = !do_colors;
  return MENU_NOHOLD;
}

/*
 * For terminals that support ANSI/ISO colors, work through a graduated
 * set of tests that first display colors (if the terminal does indeed
 * support them), then exercise the associated reset, clear operations.
 */
int
tst_colors(MENU_ARGS)
{
  static char txt_override_color[80];

  static MENU colormenu[] = {
    { "Return to main menu",                                 0 },
    { txt_override_color,                                    toggle_color_mode, },
    { "Display color test-pattern",                          show_test_pattern, },
    { "Test SGR-0 color reset",                              test_SGR_0, },
    { "Test BCE-style clear line/display",                   simple_bce_test, },
    { "Test of VT102-style features with BCE (Insert/Delete Char/Line)", test_color_insdel, },
    { "Test of screen features with BCE",                    test_color_screen, },
    { "Test of screen features with ISO 6429 SGR 22-27 codes", test_iso_6429_sgr, },
    { "", 0 }
  };

  do {
    vt_clear(2);
    sprintf(txt_override_color, "%sable color-switching",
        do_colors ? "Dis" : "En");
    title(0); println("ISO 6429 colors");
    title(2); println("Choose test type:");
  } while (menu(colormenu));
  return MENU_NOHOLD;
}
