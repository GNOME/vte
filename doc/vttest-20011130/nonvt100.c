/* $Id$ */

/*
 * The list of non-VT320 codes was compiled using the list of non-VT320 codes
 * described in the Kermit 3.13 documentation, combined with the ISO-6429
 * (ECMA-48) spec.
 */
#include <vttest.h>
#include <ttymodes.h>
#include <esc.h>

int
not_impl(MENU_ARGS)
{
  vt_move(1,1);
  printf("Sorry, test not implemented:\r\n\r\n  %s", the_title);
  vt_move(max_lines-1,1);
  return MENU_HOLD;
}

/* VT420 doesn't do this, VT510 does */
static int
tst_CBT(MENU_ARGS)
{
  int n;
  int last = (min_cols + 7) / 8;

  for (n = 1; n <= last; n++) {
    cup(1,min_cols);
    cbt(n);
    printf("%d", last + 1 - n);
  }
  vt_move(max_lines-3,1);
  vt_clear(0);
  println(the_title);
  println("The tab-stops should be numbered consecutively starting at 1.");
  return MENU_HOLD;
}

/* Note: CHA and HPA have identical descriptions in ECMA-48 */
/* dtterm implements this (VT400 doesn't, VT510 does) */
static int
tst_CHA(MENU_ARGS)
{
  int n;

  for (n = 1; n < max_lines-3; n++) {
    cup(n, min_cols - n);
    cha(n);
    printf("+");
  }
  vt_move(max_lines-3, 1);
  for (n = 1; n <= min_cols; n++)
    printf("%c", n == max_lines-3 ? '+' : '*');
  vt_move(max_lines-2, 1);
  println(the_title);
  println("There should be a diagonal of +'s down to the row of *'s above this message");
  return MENU_HOLD;
}

/*
 * Kermit's documentation refers to this as CHI, ECMA-48 as CHT.
 *
 * VT420 doesn't do this, VT510 does
 */
static int
tst_CHT(MENU_ARGS)
{
  int n;
  int last = (min_cols * 2 + 7) / 8;

  vt_move(1,1);
  println("CHT with param == 1:");
  for (n = 0; n < last; n++) {
    cht(1);
    printf("*");
  }

  vt_move(4,1);
  println("CHT with param != 1:");
  for (n = 0; n < last; n++) {
    cup(5,1);
    cht(n);
    printf("+");
  }

  vt_move(7,1);
  println("Normal tabs:");
  for (n = 0; n < last; n++) {
    printf("\t*");
  }

  vt_move(max_lines-3, 1);
  println(the_title);
  println("The lines with *'s above should look the same (they're designed to");
  println("wrap-around once).");
  return MENU_HOLD;
}

/* VT420 doesn't do this, VT510 does */
static int
tst_CNL(MENU_ARGS)
{
  int n;

  vt_move(1, 1);
  printf("1.");
  for (n = 1; n <= max_lines - 3; n++) {
    cup(1, min_cols);
    cnl(n-1);
    printf("%d.", n);
  }

  vt_move(max_lines-3, 1);
  vt_clear(0);
  println(the_title);
  println("The lines above this should be numbered in sequence, from 1.");
  return MENU_HOLD;
}

/*
 * VT510 & up
 *
 * There's a comment in the MS-DOS Kermit 3.13 documentation that implies CPL
 * is used to replace RI (reverse-index).  ECMA-48 doesn't specify scrolling
 * regions, DEC terminals do apparently, so for CPL and CNL we'll test this.
 */
static int
tst_CPL(MENU_ARGS)
{
  int i;

  vt_move(max_lines, 1);
  for (i = max_lines-1; i > 0; i--) {
    cpl(1);
    printf("%d.", i);
  }

  vt_move(max_lines-3, 1);
  vt_clear(0);
  println(the_title);
  println("The lines above this should be numbered in sequence, from 1.");
  return MENU_HOLD;
}

/* VT420 doesn't do this, VT510 does */
static int
tst_HPA(MENU_ARGS)
{
  int n;
  int last = max_lines-4;

  for (n = 1; n < last; n++) {
    cup(n, min_cols - n);
    hpa(n);
    printf("+");
  }

  vt_move(last, 1);
  for (n = 1; n <= min_cols; n++)
    printf("%c", n == last ? '+' : '*');
  vt_move(last+1, 1);
  println(the_title);
  println("There should be a diagonal of +'s down to the row of *'s above this message.");
  println("(The + in the row of *'s is the target)");
  return MENU_HOLD;
}

/*
 * Neither VT420 nor VT510.
 */
static int
tst_REP(MENU_ARGS)
{
  int n;
  int last = max_lines-4;

  vt_move(1,1);
  for (n = 1; n < last; n++) {
    if (n > 1) {
      printf(" ");
      if (n > 2)
        rep(n-2);
    }
    printf("+");
    rep(1);  /* make that 2 +'s */
    rep(10); /* this should be ignored, since a control sequence precedes */
    println("");
  }

  vt_move(last, 1);
  for (n = 1; n <= min_cols; n++)
    printf("%c", (n == last || n == last+1) ? '+' : '*');
  vt_move(last+1, 1);
  println(the_title);
  println("There should be a diagonal of 2 +'s down to the row of *'s above this message.");
  println("(The ++ in the row of *'s is the target)");
  return MENU_HOLD;
}

/*
 * Test the SD (scroll-down) by forcing characters written in a diagonal into
 * a horizontal row.
 *
 * VT400 and dtterm use the (incorrect?) escape sequence (ending with 'T'
 * instead of '^'), apparently someone misread 05/14 as 05/04 or vice versa.
 */
int
tst_SD(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(n, n);
    printf("*");
    sd(1);
  }
  vt_move(last+1,1);
  vt_clear(0);
  println(the_title);
  println("There should be a horizontal row of *'s above, just above the message.");
  return MENU_HOLD;
}

/*
 * not in VT510
 *
 * Test the SL (scroll-left) by forcing characters written in a diagonal into
 * a vertical line.
 */
static int
tst_SL(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(n, min_cols/2 + last - n);
    printf("*");
    sl(1);
  }
  vt_move(last,1);
  vt_clear(0);
  println(the_title);
  println("There should be a vertical column of *'s centered above.");
  return MENU_HOLD;
}

/*
 * not in VT510
 *
 * Test the SR (scroll-right) by forcing characters written in a diagonal into
 * a vertical line.
 */
static int
tst_SR(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(n, min_cols/2 - last + n);
    printf("*");
    sr(1);
  }
  vt_move(last,1);
  vt_clear(0);
  println(the_title);
  println("There should be a vertical column of *'s centered above.");
  return MENU_HOLD;
}

/*
 * Test the SU (scroll-up) by forcing characters written in a diagonal into
 * a horizontal row.
 */
int
tst_SU(MENU_ARGS)
{
  int n;
  int last = max_lines - 3;

  for (n = 1; n < last; n++) {
    cup(last + 1 - n, n);
    printf("*");
    su(1);
  }
  vt_move(last+1,1);
  vt_clear(0);
  println(the_title);
  println("There should be a horizontal row of *'s above, on the top row.");
  return MENU_HOLD;
}

/*
 * Test SPA (set-protected area)
 */
static int
tst_SPA(MENU_ARGS)
{
  int i, j, pass;

  for (pass = 0; pass < 2; pass++) {
    if (pass == 0) {
      esc("V"); /* SPA */
    }
    /* make two passes so we can paint over the protected-chars in the second */
    for (i = 5; i <= max_lines - 6; i++) {
      cup(i, 20);
      for (j = 20; j < min_cols - 20; j++) {
        printf("*");
      }
    }
    if (pass == 0) {
      esc("W"); /* EPA */

      cup(max_lines/2, min_cols/2);
      ed(0); /* after the cursor */
      ed(1); /* before the cursor */
      ed(2); /* the whole display */

      el(0); /* after the cursor */
      el(1); /* before the cursor */
      el(2); /* the whole line */

      ech(min_cols);

      cup(max_lines-4, 1);
      println(the_title);
      println("There should be an solid box made of *'s in the middle of the screen.");
      holdit();
    }
  }
  return MENU_NOHOLD;
}

/*
 * Kermit's documentation refers to this as CVA, ECMA-48 as VPA.
 * Move the cursor in the current column to the specified line.
 *
 * VT420 doesn't do this, VT510 does
 */
static int
tst_VPA(MENU_ARGS)
{
  int n;

  vt_move(5, 20);
  for (n = 20; n <= min_cols - 20; n++)
    printf("*");
  for (n = 5; n < max_lines - 6; n++) {
    vpa(n);
    printf("*\b");
  }
  for (n = min_cols - 20; n >= 20; n--)
    printf("\b*\b");
  for (n = 5; n < max_lines - 6; n++) {
    vpa(n);
    printf("*\b");
  }

  vt_move(max_lines-3, 1);
  println(the_title);
  println("There should be a box-outline made of *'s in the middle of the screen.");
  return MENU_HOLD;
}

/******************************************************************************/

static int
tst_ecma48_curs(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Character-Position-Absolute (HPA)",            tst_HPA },
      { "Test Cursor-Back-Tab (CBT)",                        tst_CBT },
      { "Test Cursor-Character-Absolute (CHA)",              tst_CHA },
      { "Test Cursor-Horizontal-Index (CHT)",                tst_CHT },
      { "Test Line-Position-Absolute (VPA)",                 tst_VPA },
      { "Test Next-Line (CNL)",                              tst_CNL },
      { "Test Previous-Line (CPL)",                          tst_CPL },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("ISO-6429 (ECMA-48) Cursor-Movement");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

static int
tst_ecma48_misc(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test Protected-Areas (SPA)",                        tst_SPA },
      { "Test Repeat (REP)",                                 tst_REP },
      { "Test Scroll-Down (SD)",                             tst_SD },
      { "Test Scroll-Left (SL)",                             tst_SL },
      { "Test Scroll-Right (SR)",                            tst_SR },
      { "Test Scroll-Up (SU)",                               tst_SU },
      { "",                                                  0 },
    };

  do {
    vt_clear(2);
    title(0); printf("Miscellaneous ISO-6429 (ECMA-48) Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/******************************************************************************/
int
tst_nonvt100(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Test of VT220/VT320 features",                      tst_vt220 },
      { "Test of VT420 features",                            tst_vt420 },
      { "Test ISO-6429 cursor-movement",                     tst_ecma48_curs },
      { "Test ISO-6429 colors",                              tst_colors },
      { "Test other ISO-6429 features",                      tst_ecma48_misc },
      { "Test XTERM special features",                       tst_xterm },
      { "",                                                  0 }
    };

  do {
    vt_clear(2);
    title(0); printf("Non-VT100 Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
