/* $Id$ */

#include <vttest.h>
#include <esc.h>

static int pex_mode;
static int pff_mode;
static int started;
static int assigned;
static int margin_lo;
static int margin_hi;

static void
setup_printout(MENU_ARGS, int visible, char * whole)
{
  margin_lo = 7;
  margin_hi = max_lines - 5;

  vt_clear(2);
  cup(1,1);
  println(the_title);
  println("Test screen for printing.  We will set scrolling margins at");
  printf("lines %d and %d, and write a test pattern there.\n", margin_lo, margin_hi);
  printf("The test pattern should be %s.\n", visible
        ? "visible"
        : "invisible");
  printf("The %s should be in the printer's output.\n", whole);
  decstbm(margin_lo, margin_hi);
  cup(margin_lo, 1);
}

static void
test_printout(void)
{
  int row, col;
  vt_move(margin_hi,1);
  for (row = 0; row < max_lines; row++) {
    printf("%3d:", row);
    for (col = 0; col < min_cols - 5; col++) {
      printf("%c", ((row + col) % 26) + 'a');
    }
    printf("\n");
  }
}

static void
cleanup_printout(void)
{
  decstbm(0, 0);
  vt_move(max_lines-2,1);
}

static int
tst_Assign(MENU_ARGS)
{
  mc_printer_assign(assigned = !assigned);
  return MENU_HOLD;
}

static int
tst_DECPEX(MENU_ARGS)
{
  decpex(pex_mode = !pex_mode);
  return MENU_HOLD;
}

static int
tst_DECPFF(MENU_ARGS)
{
  decpff(pff_mode = !pff_mode);
  return MENU_HOLD;
}

static int
tst_Start(MENU_ARGS)
{
  mc_printer_start(started = !started);
  return MENU_HOLD;
}

static int
tst_autoprint(MENU_ARGS)
{
  setup_printout(PASS_ARGS, TRUE, "scrolling region");
  mc_autoprint(TRUE);
  test_printout();
  mc_autoprint(FALSE);
  cleanup_printout();
  return MENU_HOLD;
}

static int
tst_printer_controller(MENU_ARGS)
{
  setup_printout(PASS_ARGS, FALSE, "scrolling region");
  mc_printer_controller(TRUE);
  test_printout();
  mc_printer_controller(FALSE);
  cleanup_printout();
  return MENU_HOLD;
}

static int
tst_print_all_pages(MENU_ARGS)
{
  setup_printout(PASS_ARGS, TRUE, "contents of all pages");
  test_printout();
  mc_print_all_pages();
  cleanup_printout();
  return MENU_HOLD;
}

static int
tst_print_cursor(MENU_ARGS)
{
  int row;
  setup_printout(PASS_ARGS, TRUE, "reverse of the scrolling region");
  test_printout();
  for (row = margin_hi; row >= margin_lo; row--) {
    vt_move(row,1);
    mc_print_cursor_line();
  }
  cleanup_printout();
  return MENU_HOLD;
}

static int
tst_print_display(MENU_ARGS)
{
  setup_printout(PASS_ARGS, TRUE, "whole display");
  test_printout();
  mc_print_composed();
  cleanup_printout();
  return MENU_HOLD;
}

static int
tst_print_page(MENU_ARGS)
{
  setup_printout(PASS_ARGS, TRUE, pex_mode ? "whole page" : "scrolling region");
  test_printout();
  mc_print_page();
  cleanup_printout();
  return MENU_HOLD;
}

int
tst_printing(MENU_ARGS)
{
  static char pex_mesg[80];
  static char pff_mesg[80];
  static char assign_mesg[80];
  static char start_mesg[80];

  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { assign_mesg,                                         tst_Assign },
      { start_mesg,                                          tst_Start },
      { pex_mesg,                                            tst_DECPEX },
      { pff_mesg,                                            tst_DECPFF },
      { "Test Auto-print mode (MC - DEC private mode)",      tst_autoprint },
      { "Test Printer-controller mode (MC)",                 tst_printer_controller },
      { "Test Print-page (MC)",                              tst_print_page },
      { "Test Print composed main-display (MC)",             tst_print_display },
      { "Test Print all pages (MC)",                         tst_print_all_pages },
      { "Test Print cursor line (MC)",                       tst_print_cursor },
      { "",                                                  0 }
    };

  do {
    sprintf(pex_mesg, "%s Printer-Extent mode (DECPEX)",
      pex_mode ? "Disable" : "Enable");
    sprintf(pff_mesg, "%s Print Form Feed Mode (DECPFF)",
      pff_mode ? "Disable" : "Enable");
    strcpy(assign_mesg, assigned
      ? "Release printer (MC)"
      : "Assign printer to active session (MC)");
    sprintf(start_mesg, "%s printer-to-host session (MC)",
      started ? "Stop" : "Start");
    vt_clear(2);
    title(0); printf("Printing-Control Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));

  if (pex_mode)
    decpex(pex_mode = 0);

  if (pff_mode)
    decpex(pff_mode = 0);

  if (assigned)
    mc_printer_start(assigned = 0);

  if (started)
    mc_printer_start(started = 0);

  return MENU_NOHOLD;
}
