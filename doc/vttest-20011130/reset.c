/* $Id$ */

#include <vttest.h>
#include <esc.h>

static int did_reset = FALSE;

int
tst_DECSTR(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);
  println("(VT220 & up)");
  println("");
  println("The terminal will now soft-reset");
  holdit();
  decstr();
  return MENU_HOLD;
}

static int
tst_DECTST(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);
  println("");

  if (did_reset)
    println("The terminal is now RESET.  Next, the built-in confidence test");
  else
    printf("The built-in confidence test ");
  printf("will be invoked. ");
  holdit();

  vt_clear(2);
  dectst(1);
  zleep(5000);          /* Wait 5.0 seconds */
  vt_move(10,1);
  println("If the built-in confidence test found any errors, a code");
  printf("%s", "is visible above. ");

  did_reset = FALSE;
  return MENU_HOLD;
}

static int
tst_RIS(MENU_ARGS)
{
  vt_move(1,1);
  println(the_title);
  println("(VT100 & up, not recommended)");
  println("");
  printf ("The terminal will now be RESET. ");
  holdit();
  ris();
  zleep(5000);          /* Wait 5.0 seconds */

  did_reset = TRUE;
  reset_level();
  input_8bits = FALSE;
  output_8bits = FALSE;
  return MENU_HOLD;
}

int
tst_rst(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Reset to Initial State (RIS)",                      tst_RIS },
      { "Invoke Terminal Test (DECTST)",                     tst_DECTST },
      { "Soft Terminal Reset (DECSTR)",                      tst_DECSTR },
      { "",                                                  0 }
    };

  did_reset = FALSE;

  do {
    vt_clear(2);
    title(0); printf(the_title);
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
