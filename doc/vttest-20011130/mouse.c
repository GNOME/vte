/* $Id$ */

#include <vttest.h>
#include <esc.h>
#include <ttymodes.h>

#define MCHR(c) (((c) - ' ') & 0xff)

#define isQuit(c) (((c) == 'q') || ((c) == 'Q'))

static void
cat_button(char *dst, char *src)
{
  if (*dst != 0) strcat(dst, ", ");
  strcat(dst, src);
}

static char *
locator_button(int b)
{
  static char result[80];

  if (b) {
    result[0] = 0;
    if (b & 1) cat_button(result, "right");
    if (b & 2) cat_button(result, "middle");
    if (b & 4) cat_button(result, "left");
    if (b & 8) cat_button(result, "M4");
  } else {
    strcpy(result, "no buttons down");
  }
  return result;
}

static char *
locator_event(int e)
{
  char *result;
  switch (e) {
  case 0:  result = "locator unavailable"; break;
  case 1:  result = "request - received a DECRQLP"; break;
  case 2:  result = "left button down"; break;
  case 3:  result = "left button up"; break;
  case 4:  result = "middle button down"; break;
  case 5:  result = "middle button up"; break;
  case 6:  result = "right button down"; break;
  case 7:  result = "right button up"; break;
  case 8:  result = "M4 button down"; break;
  case 9:  result = "M4 button up"; break;
  case 10: result = "locator outside filter rectangle"; break;
  default: result = "unknown event"; break;
  }
  return result;
}

static void
show_click(int y, int x, int c)
{
  cup(y,x);
  putchar(c);
  vt_move(y,x);
  fflush(stdout);
}

/* Print the corners of the highlight-region.  Note that xterm doesn't use
 * the last row.
 */
static void show_hilite(int first, int last)
{
  vt_move(first, 1);          printf("+");
  vt_move(last-1,  1);        printf("+");
  vt_move(first, min_cols);   printf("+");
  vt_move(last-1,  min_cols); printf("+");
  fflush(stdout);
}

static void show_locator_rectangle(void)
{
  const int first = 10;
  const int last  = 20;

  decefr(first, 1, last, min_cols);
  show_hilite(first, last);
}

static int show_locator_report(char *report, int row, int pixels)
{
  int Pe, Pb, Pr, Pc, Pp;
  int now = row;

  vt_move(3,10); vt_el(2); chrprint(report);
  while ((report = skip_csi(report)) != 0
   && (sscanf(report, "%d;%d;%d;%d&w", &Pe, &Pb, &Pr, &Pc) == 4
    || sscanf(report, "%d;%d;%d;%d;%d&w", &Pe, &Pb, &Pr, &Pc, &Pp) == 5)) {
    vt_move(row,10); vt_el(2);
    show_result("%s - %s (%d,%d)", locator_event(Pe), locator_button(Pb), Pr, Pc);
    vt_el(0);
    if (!pixels)  /* FIXME: we should be able to scale from the screen size */
      show_click(Pr, Pc, '*');
    report = strchr(report, '&') + 2;
    now = row++;
  }
  return now;
}

static void
show_dec_locator_events(MENU_ARGS, int mode, int pixels)
{
  int row, now;

  vt_move(1,1);
  println(the_title);
  println("Press 'q' to quit.  Mouse events will be marked with the button number.");

  decelr((mode > 0) ? mode : ((mode == 0) ? 2 : -mode), pixels ? 1 : 2);

  if (mode < 0)
    show_locator_rectangle();
  else if (mode == 0)
    do_csi("'w");      /* see decefr() */

  decsle(1);           /* report button-down events */
  decsle(3);           /* report button-up events */
  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  now = 4;
  for(;;) {
    char *report = instr();
    if (isQuit(*report)) {
      decrqlp(1);
      report = instr();
      show_locator_report(report, now+1, pixels);
      break;
    }
    row = 4;
    while (now > row) {
      vt_move(now,1); vt_el(2);
      now--;
    }
    now = show_locator_report(report, row, pixels);
    if (mode == 0) {
      decelr(2, pixels ? 1 : 2);
      do_csi("'w");    /* see decefr() */
    }
  }

  decelr(0,0);
  restore_ttymodes();

  vt_move(max_lines-2,1);
}

/* Normal Mouse Tracking */
static void
show_mouse_tracking(MENU_ARGS, char *the_mode)
{
  int y = 0, x = 0;

  vt_move(1,1);
  println(the_title);
  println("Press 'q' to quit.  Mouse events will be marked with the button number.");

  sm(the_mode);
  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for(;;) {
    char *report = instr();
    if (isQuit(*report))
      break;
    vt_move(3,10); vt_el(2); chrprint(report);
    while ((report = skip_csi(report)) != 0
     && *report == 'M'
     && strlen(report) >= 4) {
      int b = MCHR(report[1]);
      int adj = 1;
      vt_move(4,10); vt_el(2);
      show_result("code 0x%x (%d,%d)", b, MCHR(report[3]), MCHR(report[2]));
      if (b & ~3) {
        if (b & 4)
          printf(" shift");
        if (b & 8)
          printf(" meta");
        if (b & 16)
          printf(" control");
        if (b & 32)
          printf(" motion");
        if (b & 64)
          adj += 3;
      }
      b &= 3;
      if (b != 3) {
        b += adj;
        printf(" button %d", b);
        show_click(MCHR(report[3]), MCHR(report[2]), b + '0');
      } else if (MCHR(report[2]) != x || MCHR(report[3]) != y) {
        printf(" release");
        show_click(MCHR(report[3]), MCHR(report[2]), '*');
      }
      x = MCHR(report[2]);
      y = MCHR(report[3]);
      report += 4;
    }
  }

  rm(the_mode);
  restore_ttymodes();

  vt_move(max_lines-2,1);
}

static int
test_dec_locator_event(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, 2, FALSE);
  return MENU_HOLD;
}

static int
test_dec_locator_events(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, 1, FALSE);
  return MENU_HOLD;
}

static int
test_dec_locator_event_p(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, 2, TRUE);
  return MENU_HOLD;
}

static int
test_dec_locator_events_p(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, 1, TRUE);
  return MENU_HOLD;
}

static int
test_dec_locator_rectangle(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, -2, FALSE);
  return MENU_HOLD;
}

static int
test_dec_locator_unfiltered(MENU_ARGS)
{
  show_dec_locator_events(PASS_ARGS, 0, FALSE);
  return MENU_HOLD;
}

/* Any-Event Mouse Tracking */
static int
test_mouse_any_event(MENU_ARGS)
{
  show_mouse_tracking(PASS_ARGS, "?1003");
  return MENU_HOLD;
}

/* Button-Event Mouse Tracking */
static int
test_mouse_button_event(MENU_ARGS)
{
  show_mouse_tracking(PASS_ARGS, "?1002");
  return MENU_HOLD;
}

/* Mouse Highlight Tracking */
static int
test_mouse_hilite(MENU_ARGS)
{
  const int first = 10;
  const int last  = 20;
  int y = 0, x = 0;

  vt_move(1,1);
  println(the_title);
  println("Press 'q' to quit.  Mouse events will be marked with the button number.");
  printf("Highlighting range is [%d..%d)\n", first, last);
  show_hilite(first,last);

  sm("?1001");
  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for(;;) {
    char *report = instr();
    if (isQuit(*report))
      break;
    show_hilite(first,last);
    vt_move(4,10); vt_el(2); chrprint(report);
    if ((report = skip_csi(report)) != 0) {
      if (*report == 'M'
       && strlen(report) == 4) {
        int b = MCHR(report[1]);
        b &= 7;
        x = MCHR(report[2]);
        y = MCHR(report[3]);
        if (b != 3) {
          /* send the xterm the highlighting range (it MUST be done first) */
          do_csi("1;%d;%d;%d;%d;T", x, y, 10, 20);
          /* now, show the mouse-click */
          if (b < 3) b++;
          show_click(y, x, b + '0');
        }
        /* interpret the event */
        vt_move(5,10); vt_el(2);
        show_result("tracking: code 0x%x (%d,%d)", MCHR(report[1]), y, x);
        fflush(stdout);
      } else if (*report == 'T' && strlen(report) == 7) {
        /* interpret the event */
        vt_move(5,10); vt_el(2);
        show_result("done: start(%d,%d), end(%d,%d), mouse(%d,%d)",
          MCHR(report[2]), MCHR(report[1]),
          MCHR(report[4]), MCHR(report[3]),
          MCHR(report[6]), MCHR(report[5]));
        if (MCHR(report[2]) != y
         || MCHR(report[1]) != x)
          show_click(MCHR(report[2]), MCHR(report[1]), 's');
        if (MCHR(report[4]) != y
         || MCHR(report[3]) != x)
          show_click(MCHR(report[4]), MCHR(report[3]), 'e');
        if (MCHR(report[6]) != y
         || MCHR(report[5]) != x)
          show_click(MCHR(report[6]), MCHR(report[5]), 'm');
      } else if (*report == 't' && strlen(report) == 3) {
        /* interpret the event */
        vt_move(5,10); vt_el(2);
        show_result("done: end(%d,%d)",
          MCHR(report[2]), MCHR(report[1]));
        if (MCHR(report[2]) != y
         || MCHR(report[1]) != x)
          show_click(MCHR(report[2]), MCHR(report[1]), 'e');
      }
    }
  }

  rm("?1001");
  restore_ttymodes();

  vt_move(max_lines-2,1);
  return MENU_HOLD;
}

/* Normal Mouse Tracking */
static int
test_mouse_normal(MENU_ARGS)
{
  show_mouse_tracking(PASS_ARGS, "?1000");
  return MENU_HOLD;
}

/* X10 Mouse Compatibility */
static int
test_X10_mouse(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);
  println("Press 'q' to quit.  Mouse events will be marked with the button number.");

  sm("?9");
  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  for(;;) {
    char *report = instr();
    if (isQuit(*report))
      break;
    vt_move(3,10); vt_el(2); chrprint(report);
    if ((report = skip_csi(report)) != 0
     && *report == 'M'
     && strlen(report) == 4) {
      int x = report[2] - ' ';
      int y = report[3] - ' ';
      cup(y,x);
      printf("%d", report[1] - ' ' + 1);
      vt_move(y,x);
      fflush(stdout);
    }
  }

  rm("?9");
  restore_ttymodes();

  vt_move(max_lines-2,1);
  return MENU_HOLD;
}

/*
 * DEC locator events are implemented on DECterm, to emulate VT220.
 */
static int
tst_dec_locator_events (MENU_ARGS)
{
  static MENU my_menu[] = {
    { "Return to main menu",                                 0 },
    { "One-Shot",                                            test_dec_locator_event },
    { "Repeated",                                            test_dec_locator_events },
    { "One-Shot (pixels)",                                   test_dec_locator_event_p },
    { "Repeated (pixels)",                                   test_dec_locator_events_p },
    { "Filter Rectangle",                                    test_dec_locator_rectangle },
    { "Filter Rectangle (unfiltered)",                       test_dec_locator_unfiltered },
    { "",                                                    0 }
  };

  do {
    vt_clear(2);
    title(0); println("DEC Locator Events");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}

/*
 * xterm generally implements mouse escape sequences (except for dtterm and
 * DECterm).  XFree86 xterm implements some additional controls.
 */
int
tst_mouse(MENU_ARGS)
{
  static MENU my_menu[] = {
    { "Return to main menu",                                 0 },
    { "X10 Mouse Compatibility",                             test_X10_mouse },
    { "Normal Mouse Tracking",                               test_mouse_normal },
    { "Mouse Highlight Tracking",                            test_mouse_hilite },
    { "Mouse Any-Event Tracking (XFree86 xterm)",            test_mouse_any_event },
    { "Mouse Button-Event Tracking (XFree86 xterm)",         test_mouse_button_event },
    { "DEC Locator Events (DECterm)",                        tst_dec_locator_events },
    { "",                                                    0 }
  };

  do {
    vt_clear(2);
    title(0); println("XTERM mouse features");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
