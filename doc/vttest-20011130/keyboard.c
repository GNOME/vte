/* $Id$ */

#include <vttest.h>
#include <ttymodes.h>
#include <esc.h>

/* Test of:
     - DECLL   (Load LEDs)
     - Keyboard return messages
     - SM RM   (Set/Reset Mode) - Cursor Keys
                                - Auto repeat
     - DECKPAM (Keypad Application Mode)
     - DECKPNM (Keypad Numeric Mode)

The standard VT100 keyboard layout:

                                                        UP   DN   LE  RI

ESC   1!   2@   3#   4$   5%   6^   7&   8*   9(   0)   -_   =+   `~  BS

TAB*    qQ   wW   eE   rR   tT   yY   uU   iI   oO   pP   [{   ]}      DEL

**   **   aA   sS   dD   fF   gG   hH   jJ   kK   lL   ;:   ,"   RETN  \|

**   ****   zZ   xX   cC   vV   bB   nN   mM   ,<   .>   /?   ****   LF

             ****************SPACE BAR****************

                                                           PF1 PF2 PF3 PF4

                                                           *7* *8* *9* *-*

                                                           *4* *5* *6* *,*

                                                           *1* *2* *3*

                                                           ***0*** *.* ENT

The standard LK401 (VT420) keyboard layout:

F1 F2 F3 F4 F5   F6 F7 F8 F9 F10   F11 F12 F13 F14   Help Do   F17 F18 F19 F20

  `~  1!   2@   3#   4$   5%   6^   7&   8*   9(   0)   -_   =+   DEL

TAB*    qQ   wW   eE   rR   tT   yY   uU   iI   oO   pP   [{   ]}   Return

**   **   aA   sS   dD   fF   gG   hH   jJ   kK   lL   ;:   ,"   \| 

*****   <>  zZ   xX   cC   vV   bB   nN   mM   ,<   .>   /?    ******

***** *****  ****************SPACE BAR****************  ****** ******

                       Find  Insert Remove                 PF1 PF2 PF3 PF4

                      Select  Prev   Next                  *7* *8* *9* *-*

                               Up                          *4* *5* *6* *,*

                       Left   Down   Right                 *1* *2* *3*

                                                           ***0*** *.* ENT
*/

static struct key {
    char c;
    int  row;
    int  col;
    char *symbol;
} VT100_keytab [] = {
    { ESC, 1,  0, "ESC" },
    { '1', 1,  6, "1" },    { '!', 1,  7, "!" },
    { '2', 1, 11, "2" },    { '@', 1, 12, "@" },
    { '3', 1, 16, "3" },    { '#', 1, 17, "#" },
    { '4', 1, 21, "4" },    { '$', 1, 22, "$" },
    { '5', 1, 26, "5" },    { '%', 1, 27, "%" },
    { '6', 1, 31, "6" },    { '^', 1, 32, "^" },
    { '7', 1, 36, "7" },    { '&', 1, 37, "&" },
    { '8', 1, 41, "8" },    { '*', 1, 42, "*" },
    { '9', 1, 46, "9" },    { '(', 1, 47, "(" },
    { '0', 1, 51, "0" },    { ')', 1, 52, ")" },
    { '-', 1, 56, "-" },    { '_', 1, 57, "_" },
    { '=', 1, 61, "=" },    { '+', 1, 62, "+" },
    { '`', 1, 66, "`" },    { '~', 1, 67, "~" },
    {   8, 1, 70, "BS" },
    {   9, 2,  0, " TAB " },
    { 'q', 2,  8, "q" },    { 'Q', 2,  9, "Q" },
    { 'w', 2, 13, "w" },    { 'W', 2, 14, "W" },
    { 'e', 2, 18, "e" },    { 'E', 2, 19, "E" },
    { 'r', 2, 23, "r" },    { 'R', 2, 24, "R" },
    { 't', 2, 28, "t" },    { 'T', 2, 29, "T" },
    { 'y', 2, 33, "y" },    { 'Y', 2, 34, "Y" },
    { 'u', 2, 38, "u" },    { 'U', 2, 39, "U" },
    { 'i', 2, 43, "i" },    { 'I', 2, 44, "I" },
    { 'o', 2, 48, "o" },    { 'O', 2, 49, "O" },
    { 'p', 2, 53, "p" },    { 'P', 2, 54, "P" },
    { '[', 2, 58, "[" },    { '{', 2, 59, "{" },
    { ']', 2, 63, "]" },    { '}', 2, 64, "}" },
    { 127, 2, 71, "DEL" },
    { 'a', 3, 10, "a" },    { 'A', 3, 11, "A" },
    { 's', 3, 15, "s" },    { 'S', 3, 16, "S" },
    { 'd', 3, 20, "d" },    { 'D', 3, 21, "D" },
    { 'f', 3, 25, "f" },    { 'F', 3, 26, "F" },
    { 'g', 3, 30, "g" },    { 'G', 3, 31, "G" },
    { 'h', 3, 35, "h" },    { 'H', 3, 36, "H" },
    { 'j', 3, 40, "j" },    { 'J', 3, 41, "J" },
    { 'k', 3, 45, "k" },    { 'K', 3, 46, "K" },
    { 'l', 3, 50, "l" },    { 'L', 3, 51, "L" },
    { ';', 3, 55, ";" },    { ':', 3, 56, ":" },
    {'\'', 3, 60, "'" },    { '"', 3, 61,"\"" },
    {  13, 3, 65, "RETN"},
    {'\\', 3, 71,"\\" },    { '|', 3, 72, "|" },
    { 'z', 4, 12, "z" },    { 'Z', 4, 13, "Z" },
    { 'x', 4, 17, "x" },    { 'X', 4, 18, "X" },
    { 'c', 4, 22, "c" },    { 'C', 4, 23, "C" },
    { 'v', 4, 27, "v" },    { 'V', 4, 28, "V" },
    { 'b', 4, 32, "b" },    { 'B', 4, 33, "B" },
    { 'n', 4, 37, "n" },    { 'N', 4, 38, "N" },
    { 'm', 4, 42, "m" },    { 'M', 4, 43, "M" },
    { ',', 4, 47, "," },    { '<', 4, 48, "<" },
    { '.', 4, 52, "." },    { '>', 4, 53, ">" },
    { '/', 4, 57, "/" },    { '?', 4, 58, "?" },
    {  10, 4, 69, "LF" },
    { ' ', 5, 13, "                SPACE BAR                "},
    {'\0', 0,  0, ""  }
  },
  LK401_keytab [] = {
    { '`', 1,  3, "`" },    { '~', 1,  4, "~" },
    { '1', 1,  7, "1" },    { '!', 1,  8, "!" },
    { '2', 1, 12, "2" },    { '@', 1, 13, "@" },
    { '3', 1, 17, "3" },    { '#', 1, 18, "#" },
    { '4', 1, 22, "4" },    { '$', 1, 23, "$" },
    { '5', 1, 27, "5" },    { '%', 1, 28, "%" },
    { '6', 1, 32, "6" },    { '^', 1, 33, "^" },
    { '7', 1, 37, "7" },    { '&', 1, 38, "&" },
    { '8', 1, 42, "8" },    { '*', 1, 43, "*" },
    { '9', 1, 47, "9" },    { '(', 1, 48, "(" },
    { '0', 1, 52, "0" },    { ')', 1, 53, ")" },
    { '-', 1, 57, "-" },    { '_', 1, 58, "_" },
    { '=', 1, 62, "=" },    { '+', 1, 63, "+" },
    { 127, 1, 67, "DEL" },
    {   9, 2,  0, "TAB " },
    { 'q', 2,  9, "q" },    { 'Q', 2, 10, "Q" },
    { 'w', 2, 14, "w" },    { 'W', 2, 15, "W" },
    { 'e', 2, 19, "e" },    { 'E', 2, 20, "E" },
    { 'r', 2, 24, "r" },    { 'R', 2, 25, "R" },
    { 't', 2, 29, "t" },    { 'T', 2, 30, "T" },
    { 'y', 2, 34, "y" },    { 'Y', 2, 35, "Y" },
    { 'u', 2, 39, "u" },    { 'U', 2, 40, "U" },
    { 'i', 2, 44, "i" },    { 'I', 2, 45, "I" },
    { 'o', 2, 49, "o" },    { 'O', 2, 50, "O" },
    { 'p', 2, 54, "p" },    { 'P', 2, 55, "P" },
    { '[', 2, 59, "[" },    { '{', 2, 60, "{" },
    { ']', 2, 64, "]" },    { '}', 2, 65, "}" },
    { 13,  2, 69, "Return" },
    { 'a', 3, 11, "a" },    { 'A', 3, 12, "A" },
    { 's', 3, 16, "s" },    { 'S', 3, 17, "S" },
    { 'd', 3, 21, "d" },    { 'D', 3, 22, "D" },
    { 'f', 3, 26, "f" },    { 'F', 3, 27, "F" },
    { 'g', 3, 31, "g" },    { 'G', 3, 32, "G" },
    { 'h', 3, 36, "h" },    { 'H', 3, 37, "H" },
    { 'j', 3, 41, "j" },    { 'J', 3, 42, "J" },
    { 'k', 3, 46, "k" },    { 'K', 3, 47, "K" },
    { 'l', 3, 51, "l" },    { 'L', 3, 52, "L" },
    { ';', 3, 56, ";" },    { ':', 3, 57, ":" },
    {'\'', 3, 61, "'" },    { '"', 3, 62,"\"" },
    {'\\', 3, 66,"\\" },    { '|', 3, 67, "|" },
    { '<', 4,  9, "<" },    { '>', 4, 10, ">" },
    { 'z', 4, 13, "z" },    { 'Z', 4, 14, "Z" },
    { 'x', 4, 18, "x" },    { 'X', 4, 19, "X" },
    { 'c', 4, 23, "c" },    { 'C', 4, 24, "C" },
    { 'v', 4, 28, "v" },    { 'V', 4, 29, "V" },
    { 'b', 4, 33, "b" },    { 'B', 4, 34, "B" },
    { 'n', 4, 38, "n" },    { 'N', 4, 39, "N" },
    { 'm', 4, 43, "m" },    { 'M', 4, 44, "M" },
    { ',', 4, 48, "," },    { '<', 4, 49, "<" },
    { '.', 4, 53, "." },    { '>', 4, 54, ">" },
    { '/', 4, 58, "/" },    { '?', 4, 59, "?" },
    { ' ', 5, 14, "                SPACE BAR                "},
    {'\0', 0,  0, ""  }
  },
  *keytab;

typedef struct {
    unsigned char prefix;
    char *msg;
} CTLKEY;

static struct curkey {
    CTLKEY curkeymsg[3];
    int  curkeyrow;
    int  curkeycol;
    char *curkeysymbol;
    char *curkeyname;
} VT100_curkeytab [] = {

    /* A Reset,   A Set,     VT52  */

    {{{CSI,"A"}, {SS3,"A"}, {ESC,"A"}}, 0, 56, "UP",  "Up arrow"   },
    {{{CSI,"B"}, {SS3,"B"}, {ESC,"B"}}, 0, 61, "DN",  "Down arrow" },
    {{{CSI,"D"}, {SS3,"D"}, {ESC,"D"}}, 0, 66, "LT",  "Left arrow" },
    {{{CSI,"C"}, {SS3,"C"}, {ESC,"C"}}, 0, 71, "RT",  "Right arrow"},
    {{{0,  ""},  {0,  ""},  {0,  "" }}, 0,  0, "",    "" }
  },
  LK401_curkeytab [] = {

    /* A Reset,   A Set,     VT52  */

    {{{CSI,"A"}, {SS3,"A"}, {ESC,"A"}}, 8, 32, "Up",    "Up arrow"   },
    {{{CSI,"B"}, {SS3,"B"}, {ESC,"B"}}, 9, 31, "Down",  "Down arrow" },
    {{{CSI,"D"}, {SS3,"D"}, {ESC,"D"}}, 9, 24, "Left",  "Left arrow" },
    {{{CSI,"C"}, {SS3,"C"}, {ESC,"C"}}, 9, 38, "Right", "Right arrow"},
    {{{0,  ""},  {0,  ""},  {0,  "" }}, 0,  0, "",      "" }
  },
  *curkeytab;

static struct fnckey {
    CTLKEY fnkeymsg[2];
    int  fnkeyrow;
    int  fnkeycol;
    char *fnkeysymbol;
    char *fnkeyname;
} fnkeytab [] = {

    /* Normal,     VT100/VT52  */
    {{{CSI,"11~"}, {0,""}},  0,  1, "F1",   "F1 (xterm)"   },
    {{{CSI,"12~"}, {0,""}},  0,  4, "F2",   "F2 (xterm)"   },
    {{{CSI,"13~"}, {0,""}},  0,  7, "F3",   "F3 (xterm)"   },
    {{{CSI,"14~"}, {0,""}},  0, 10, "F4",   "F4 (xterm)"   },
    {{{CSI,"15~"}, {0,""}},  0, 13, "F5",   "F5 (xterm)"   },

    {{{CSI,"17~"}, {0,""}},  0, 18, "F6",   "F6"   },
    {{{CSI,"18~"}, {0,""}},  0, 21, "F7",   "F7"   },
    {{{CSI,"19~"}, {0,""}},  0, 24, "F8",   "F8"   },
    {{{CSI,"20~"}, {0,""}},  0, 27, "F9",   "F9"   },
    {{{CSI,"21~"}, {0,""}},  0, 30, "F10",  "F10"   },
    {{{CSI,"23~"}, {0,""}},  0, 36, "F11",  "F11"   },
    {{{CSI,"24~"}, {0,""}},  0, 40, "F12",  "F12"   },
    {{{CSI,"25~"}, {0,""}},  0, 44, "F13",  "F13"   },
    {{{CSI,"26~"}, {0,""}},  0, 48, "F14",  "F14"   },
    {{{CSI,"28~"}, {0,""}},  0, 54, "Help", "Help (F15)"   },
    {{{CSI,"29~"}, {0,""}},  0, 59, "Do",   "Do (F16)"   },
    {{{CSI,"31~"}, {0,""}},  0, 64, "F17",  "F17"   },
    {{{CSI,"32~"}, {0,""}},  0, 68, "F18",  "F18"   },
    {{{CSI,"33~"}, {0,""}},  0, 72, "F19",  "F19"   },
    {{{CSI,"34~"}, {0,""}},  0, 76, "F20",  "F20"   },
    {{{0,  ""},    {0,"" }}, 0,  0, "",     ""   }
  },
  edt_keypadtab[] = {
    {{{CSI,"1~"}, {0,""}}, 6, 24, "Find" ,  "Find"  },
    {{{CSI,"2~"}, {0,""}}, 6, 30, "Insert", "Insert Here"   },
    {{{CSI,"3~"}, {0,""}}, 6, 37, "Remove", "Remove"   },
    {{{CSI,"4~"}, {0,""}}, 7, 23, "Select", "Select"   },
    {{{CSI,"5~"}, {0,""}}, 7, 31, "Prev",   "Prev"   },
    {{{CSI,"6~"}, {0,""}}, 7, 38, "Next",   "Next"   },
    {{{0,  ""},   {0,""}}, 0,  0, "",       ""   }
  };

static struct fnkey {
    CTLKEY fnkeymsg[4];
    int  fnkeyrow;
    int  fnkeycol;
    char *fnkeysymbol;
    char *fnkeyname;
} num_keypadtab [] = {

  /* ANSI-num, ANSI-app,  VT52-nu,   VT52-ap,     r,  c,  symb   name        */

  {{{SS3,"P"}, {SS3,"P"}, {ESC,"P"}, {ESC,"P" }}, 6, 59, "PF1", "PF1"        },
  {{{SS3,"Q"}, {SS3,"Q"}, {ESC,"Q"}, {ESC,"Q" }}, 6, 63, "PF2", "PF2"        },
  {{{SS3,"R"}, {SS3,"R"}, {ESC,"R"}, {ESC,"R" }}, 6, 67, "PF3", "PF3"        },
  {{{SS3,"S"}, {SS3,"S"}, {ESC,"S"}, {ESC,"S" }}, 6, 71, "PF4", "PF4"        },
  {{{0,  "7"}, {SS3,"w"}, {0,  "7"}, {ESC,"?w"}}, 7, 59, " 7 ", "Numeric 7"  },
  {{{0,  "8"}, {SS3,"x"}, {0,  "8"}, {ESC,"?x"}}, 7, 63, " 8 ", "Numeric 8"  },
  {{{0,  "9"}, {SS3,"y"}, {0,  "9"}, {ESC,"?y"}}, 7, 67, " 9 ", "Numeric 9"  },
  {{{0,  "-"}, {SS3,"m"}, {0,  "-"}, {ESC,"?m"}}, 7, 71, " - ", "Minus"      },
  {{{0,  "4"}, {SS3,"t"}, {0,  "4"}, {ESC,"?t"}}, 8, 59, " 4 ", "Numeric 4"  },
  {{{0,  "5"}, {SS3,"u"}, {0,  "5"}, {ESC,"?u"}}, 8, 63, " 5 ", "Numeric 5"  },
  {{{0,  "6"}, {SS3,"v"}, {0,  "6"}, {ESC,"?v"}}, 8, 67, " 6 ", "Numeric 6"  },
  {{{0,  ","}, {SS3,"l"}, {0,  ","}, {ESC,"?l"}}, 8, 71, " , ", "Comma"      },
  {{{0,  "1"}, {SS3,"q"}, {0,  "1"}, {ESC,"?q"}}, 9, 59, " 1 ", "Numeric 1"  },
  {{{0,  "2"}, {SS3,"r"}, {0,  "2"}, {ESC,"?r"}}, 9, 63, " 2 ", "Numeric 2"  },
  {{{0,  "3"}, {SS3,"s"}, {0,  "3"}, {ESC,"?s"}}, 9, 67, " 3 ", "Numeric 3"  },
  {{{0,  "0"}, {SS3,"p"}, {0,  "0"}, {ESC,"?p"}},10, 59, "   0   ","Numeric 0"},
  {{{0,  "."}, {SS3,"n"}, {0,  "."}, {ESC,"?n"}},10, 67, " . ", "Point"      },
  {{{0,"\015"},{SS3,"M"}, {0,"\015"},{ESC,"?M"}},10, 71, "ENT", "ENTER"      },
  {{{0,  ""},  {0,  ""},  {0,  ""},  {0,  ""}},   0,  0, "",    ""           }
};

struct natkey {
    char natc;
    int  natrow;
    int  natcol;
    char *natsymbol;
};

static int same_CTLKEY(char *response, CTLKEY *code);

static int
find_cursor_key(char *curkeystr, int ckeymode)
{
  int i;

  for (i = 0; curkeytab[i].curkeysymbol[0] != '\0'; i++) {
    if (same_CTLKEY(curkeystr, &curkeytab[i].curkeymsg[ckeymode])) {
      return i;
    }
  }
  return -1;
}

static int
find_editing_key(char *keypadstr, int fkeymode)
{
  int i;

  for (i = 0; edt_keypadtab[i].fnkeysymbol[0] != '\0'; i++) {
    if (same_CTLKEY(keypadstr, &edt_keypadtab[i].fnkeymsg[fkeymode])) {
      return i;
    }
  }
  return -1;
}

static int
find_function_key(char *keypadstr, int fkeymode)
{
  int i;

  for (i = 0; fnkeytab[i].fnkeysymbol[0] != '\0'; i++) {
    if (same_CTLKEY(keypadstr, &fnkeytab[i].fnkeymsg[fkeymode])) {
      return i;
    }
  }
  return -1;
}

static int
find_num_keypad_key(char *keypadstr, int fkeymode)
{
  int i;

  for (i = 0; num_keypadtab[i].fnkeysymbol[0] != '\0'; i++) {
    if (same_CTLKEY(keypadstr, &num_keypadtab[i].fnkeymsg[fkeymode])) {
      return i;
    }
  }
  return -1;
}

static void
set_keyboard_layout(struct natkey *table)
{
  int i, j;

  for (j = 0; table[j].natc != '\0'; j++) {
    for (i = 0; keytab[i].c != '\0'; i++) {
      if (keytab[i].row == table[j].natrow &&
          keytab[i].col == table[j].natcol) {
        keytab[i].c = table[j].natc;
        keytab[i].symbol = table[j].natsymbol;
        break;
      }
    }
  }
}

static int
default_layout(MENU_ARGS)
{
  /* FIXME: nothing resets the default keytab to original state */
  return MENU_NOHOLD;
}

static int
same_CTLKEY(char *response, CTLKEY *code)
{
  switch (code->prefix) {
  case CSI:
    if ((response = skip_csi(response)) == 0)
      return FALSE;
    break;
  case SS3:
    if ((response = skip_ss3(response)) == 0)
      return FALSE;
    break;
  case ESC:
    if (*response++ != ESC)
      return FALSE;
  default:
    break;
  }
  return !strcmp(response, code->msg);
}

static int
set_D47_layout(MENU_ARGS)
{
  static struct natkey table[] =
  {
    { '"', 1, 12, "\""},
    { '&', 1, 32, "&" },
    { '/', 1, 37, "/" },
    { '(', 1, 42, "(" },
    { ')', 1, 47, ")" },
    { '=', 1, 52, "=" },
    { '+', 1, 56, "+" },    { '?', 1, 57, "?" },
    { '`', 1, 61, "`" },    { '@', 1, 62, "@" },
    { '<', 1, 66, "<" },    { '>', 1, 67, ">" },
    { '}', 2, 58, "}" },    { ']', 2, 59, "]" },
    { '^', 2, 63, "^" },    { '~', 2, 64, "~" },
    { '|', 3, 55, "|" },    {'\\', 3, 56,"\\" },
    { '{', 3, 60, "{" },    { '[', 3, 61, "[" },
    {'\'', 3, 71, "'" },    { '*', 3, 72, "*" },
    { ',', 4, 47, "," },    { ';', 4, 48, ";" },
    { '.', 4, 52, "." },    { ':', 4, 53, ":" },
    { '-', 4, 57, "-" },    { '_', 4, 58, "_" },
    {'\0', 0,  0, ""  }
  };

  set_keyboard_layout(table);
  return MENU_NOHOLD;
}

static int
set_E47_layout(MENU_ARGS)
{
  static struct natkey table[] =
  {
    { '"', 1, 12, "\""},
    { '&', 1, 32, "&" },
    { '/', 1, 37, "/" },
    { '(', 1, 42, "(" },
    { ')', 1, 47, ")" },
    { '=', 1, 52, "=" },
    { '+', 1, 56, "+" },    { '?', 1, 57, "?" },
    { '`', 1, 61, "`" },    { '@', 1, 62, "@" },
    { '<', 1, 66, "<" },    { '>', 1, 67, ">" },
    { '}', 2, 58, "}" },    { ']', 2, 59, "]" },
    { '~', 2, 63, "~" },    { '^', 2, 64, "^" },
    { '|', 3, 55, "|" },    {'\\', 3, 56,"\\" },
    { '{', 3, 60, "{" },    { '[', 3, 61, "[" },
    {'\'', 3, 71, "'" },    { '*', 3, 72, "*" },
    { ',', 4, 47, "," },    { ';', 4, 48, ";" },
    { '.', 4, 52, "." },    { ':', 4, 53, ":" },
    { '-', 4, 57, "-" },    { '_', 4, 58, "_" },
    {'\0', 0,  0, ""  }
  };

  set_keyboard_layout(table);
  return MENU_NOHOLD;
}

static void
show_character(int i, char *scs_params, int hilite)
{
  int special = ((scs_params != 0) && (strlen(keytab[i].symbol) == 1));

  vt_move(1 + 2 * keytab[i].row, 1 + keytab[i].col);
  if (hilite)  vt_hilite(TRUE);
  if (special) esc(scs_params);
  printf("%s", keytab[i].symbol);
  if (special) scs(0, 'B');
  if (hilite)  vt_hilite(FALSE);
}

static void
show_cursor_keys(int flag)
{
  int i;

  curkeytab = (terminal_id() < 200) ? VT100_curkeytab : LK401_curkeytab;

  for (i = 0; curkeytab[i].curkeysymbol[0] != '\0'; i++) {
    vt_move(1 + 2 * curkeytab[i].curkeyrow, 1 + curkeytab[i].curkeycol);
    if (flag) vt_hilite(TRUE);
    printf("%s", curkeytab[i].curkeysymbol);
    if (flag) vt_hilite(FALSE);
  }
}

static void
show_editing_keypad(int flag)
{
  if (terminal_id() >= 200) {
    int i;

    for (i = 0; edt_keypadtab[i].fnkeysymbol[0] != '\0'; i++) {
      vt_move(1 + 2 * edt_keypadtab[i].fnkeyrow, 1 + edt_keypadtab[i].fnkeycol);
      if (flag) vt_hilite(TRUE);
      printf("%s", edt_keypadtab[i].fnkeysymbol);
      if (flag) vt_hilite(FALSE);
    }
  }
}

static void
show_function_keys(int flag)
{
  if (terminal_id() >= 200) {
    int i;

    for (i = 0; fnkeytab[i].fnkeysymbol[0] != '\0'; i++) {
      vt_move(1 + 2 * fnkeytab[i].fnkeyrow, 1 + fnkeytab[i].fnkeycol);
      if (flag) vt_hilite(TRUE);
      printf("%s", fnkeytab[i].fnkeysymbol);
      if (flag) vt_hilite(FALSE);
    }
  }
}

static void
show_keyboard(int flag, char *scs_params)
{
  int i;

  if (terminal_id() >= 200) /* LK201 _looks_ the same as LK401 (to me) */
    keytab = LK401_keytab;
  else
    keytab = VT100_keytab;

  for (i = 0; keytab[i].c != '\0'; i++) {
    show_character(i, scs_params, TRUE);
  }
}

static void
show_numeric_keypad(int flag)
{
  int i;

  for (i = 0; num_keypadtab[i].fnkeysymbol[0] != '\0'; i++) {
    vt_move(1 + 2 * num_keypadtab[i].fnkeyrow, 1 + num_keypadtab[i].fnkeycol);
    if (flag) vt_hilite(TRUE);
    printf("%s", num_keypadtab[i].fnkeysymbol);
    if (flag) vt_hilite(FALSE);
  }
}

/******************************************************************************/

static int
tst_AnswerBack(MENU_ARGS)
{
  char *abmstr;

  set_tty_crmod(TRUE);
  vt_clear(2);
  vt_move(5,1);
  println("Finally, a check of the ANSWERBACK MESSAGE, which can be sent");
  println("by pressing CTRL-BREAK. The answerback message can be loaded");
  println("in SET-UP B by pressing SHIFT-A and typing e.g.");
  println("");
  println("         \" H e l l o , w o r l d Return \"");
  println("");
  println("(the double-quote characters included).  Do that, and then try");
  println("to send an answerback message with CTRL-BREAK.  If it works,");
  println("the answerback message should be displayed in reverse mode.");
  println("Finish with a single RETURN.");

  set_tty_crmod(FALSE);
  do {
    vt_move(17,1);
    inflush();
    abmstr = get_reply();
    vt_move(17,1);
    vt_el(0);
    chrprint(abmstr);
  } while (strcmp(abmstr,"\r"));
  restore_ttymodes();
  return MENU_NOHOLD;
}

static int
tst_AutoRepeat(MENU_ARGS)
{
  char arptstring[BUFSIZ];

  vt_clear(2);
  vt_move(10,1);
  println("Test of the AUTO REPEAT feature");

  println("");
  println("Hold down an alphanumeric key for a while, then push RETURN.");
  printf("%s", "Auto Repeat OFF: ");
  rm("?8"); /* DECARM */
  inputline(arptstring);
  if (LOG_ENABLED)
    fprintf(log_fp, "Input: %s\n", arptstring);
  if (strlen(arptstring) == 0)      println("No characters read!??");
  else if (strlen(arptstring) == 1) println("OK.");
  else                              println("Too many characters read.");
  println("");

  println("Hold down an alphanumeric key for a while, then push RETURN.");
  printf("%s", "Auto Repeat ON: ");
  sm("?8"); /* DECARM */
  inputline(arptstring);
  if (LOG_ENABLED)
    fprintf(log_fp, "Input: %s\n", arptstring);
  if (strlen(arptstring) == 0)      println("No characters read!??");
  else if (strlen(arptstring) == 1) println("Not enough characters read.");
  else                              println("OK.");
  println("");

  return MENU_HOLD;
}

static int
tst_ControlKeys(MENU_ARGS)
{
  int  i, okflag;
  int  kbdc;
  char temp[80];
  char *kbds = strcpy(temp, " ");

  static struct ckey {
      int  ccount;
      char *csymbol;
  } ckeytab [] = {
      { 0, "NUL (CTRL-@ or CTRL-Space)" },
      { 0, "SOH (CTRL-A)" },
      { 0, "STX (CTRL-B)" },
      { 0, "ETX (CTRL-C)" },
      { 0, "EOT (CTRL-D)" },
      { 0, "ENQ (CTRL-E)" },
      { 0, "ACK (CTRL-F)" },
      { 0, "BEL (CTRL-G)" },
      { 0, "BS  (CTRL-H) (BACK SPACE)" },
      { 0, "HT  (CTRL-I) (TAB)" },
      { 0, "LF  (CTRL-J) (LINE FEED)" },
      { 0, "VT  (CTRL-K)" },
      { 0, "FF  (CTRL-L)" },
      { 0, "CR  (CTRL-M) (RETURN)" },
      { 0, "SO  (CTRL-N)" },
      { 0, "SI  (CTRL-O)" },
      { 0, "DLE (CTRL-P)" },
      { 0, "DC1 (CTRL-Q) (X-On)" },
      { 0, "DC2 (CTRL-R)" },
      { 0, "DC3 (CTRL-S) (X-Off)" },
      { 0, "DC4 (CTRL-T)" },
      { 0, "NAK (CTRL-U)" },
      { 0, "SYN (CTRL-V)" },
      { 0, "ETB (CTRL-W)" },
      { 0, "CAN (CTRL-X)" },
      { 0, "EM  (CTRL-Y)" },
      { 0, "SUB (CTRL-Z)" },
      { 0, "ESC (CTRL-[) (ESCAPE)" },
      { 0, "FS  (CTRL-\\ or CTRL-? or CTRL-_)" },
      { 0, "GS  (CTRL-])" },
      { 0, "RS  (CTRL-^ or CTRL-~ or CTRL-`)" },
      { 0, "US  (CTRL-_ or CTRL-?)" }
  };

  vt_clear(2);
  for (i = 0; i < 32; i++) {
    vt_move(1 + (i % 16), 1 + 40 * (i / 16));
    vt_hilite(TRUE);
    printf("%s", ckeytab[i].csymbol);
    vt_hilite(FALSE);
  }
  vt_move(19,1);
  set_tty_crmod(TRUE);
  println(
  "Push each CTRL-key TWICE. Note that you should be able to send *all*");
  println(
  "CTRL-codes twice, including CTRL-S (X-Off) and CTRL-Q (X-Off)!");
  println(
  "Finish with DEL (also called DELETE or RUB OUT), or wait 1 minute.");
  set_tty_raw(TRUE);
  do {
    vt_move(max_lines-1,1); kbdc = inchar();
    vt_move(max_lines-1,1); vt_el(0);
    if (kbdc < 32) {
      printf("  %s", ckeytab[kbdc].csymbol);
      if (LOG_ENABLED)
        fprintf(log_fp, "Key: %s\n", ckeytab[kbdc].csymbol);
    } else {
      sprintf(kbds, "%c", kbdc);
      chrprint(kbds);
      printf("%s", " -- not a CTRL key");
    }
    if (kbdc < 32) ckeytab[kbdc].ccount++;
    if (ckeytab[kbdc].ccount == 2) {
      vt_move(1 + (kbdc % 16), 1 + 40 * (kbdc / 16));
      printf("%s", ckeytab[kbdc].csymbol);
    }
  } while (kbdc != '\177');

  restore_ttymodes();
  vt_move(max_lines,1);
  okflag = 1;
  for (i = 0; i < 32; i++) if (ckeytab[i].ccount < 2) okflag = 0;
  if (okflag) printf("%s", "OK. ");
  else        printf("%s", "You have not been able to send all CTRL keys! ");
  return MENU_HOLD;
}

static int
tst_CursorKeys(MENU_ARGS)
{
  int  i;
  int  ckeymode;
  char *curkeystr;
  VTLEVEL save;

  static char *curkeymodes[3] = {
      "ANSI / Cursor key mode RESET",
      "ANSI / Cursor key mode SET",
      "VT52 Mode"
  };

  vt_clear(2);
  save_level(&save);
  show_keyboard(0, (char *)0);
  show_function_keys(0);
  show_editing_keypad(0);
  show_numeric_keypad(0);
  vt_move(max_lines-2,1);

  set_tty_crmod(FALSE);
  set_tty_echo(FALSE);

  for (ckeymode = 0; ckeymode <= 2; ckeymode++) {
    if (ckeymode) sm("?1"); /* DECCKM */
    else          rm("?1");

    show_cursor_keys(1);
    vt_move(21,1); printf("<%s>%20s", curkeymodes[ckeymode], "");
    vt_move(max_lines-2,1); vt_el(0);
    vt_move(max_lines-2,1); printf("%s", "Press each cursor key. Finish with TAB.");
    for(;;) {
      vt_move(max_lines-1,1);
      if (ckeymode == 2) set_level(0); /* VT52 mode */
      curkeystr = instr();
      set_level(1);                     /* ANSI mode */

      vt_move(max_lines-1,1); vt_el(0);
      vt_move(max_lines-1,1); chrprint(curkeystr);

      if (!strcmp(curkeystr,"\t")) break;
      if ((i = find_cursor_key(curkeystr, ckeymode)) >= 0) {
        vt_hilite(TRUE);
        show_result(" (%s key) ", curkeytab[i].curkeyname);
        vt_hilite(FALSE);
        vt_move(1 + 2 * curkeytab[i].curkeyrow, 1 + curkeytab[i].curkeycol);
        printf("%s", curkeytab[i].curkeysymbol);
      } else {
        vt_hilite(TRUE);
        show_result("%s", " (Unknown cursor key) ");
        vt_hilite(FALSE);
      }
    }
  }

  restore_level(&save);
  vt_move(max_lines-1,1); vt_el(0);
  restore_ttymodes();
  return MENU_MERGE;
}

static int
tst_EditingKeypad(MENU_ARGS)
{
  int  i;
  int  fkeymode;
  char *fnkeystr;
  VTLEVEL save;

  static char *fnkeymodes[] = {
      "Normal mode",
      "VT100/VT52 mode (none should be recognized)"
  };

  save_level(&save);
  show_keyboard(0, (char *)0);
  show_cursor_keys(0);
  show_function_keys(0);
  show_numeric_keypad(0);
  vt_move(max_lines-2,1);

  if (terminal_id() < 200) {
    printf("Sorry, a real VT%d terminal doesn't have an editing keypad\n", terminal_id());
    return MENU_HOLD;
  }

  set_tty_crmod(FALSE);
  set_tty_echo(FALSE);

  for (fkeymode = 0; fkeymode <= 1; fkeymode++) {
    show_editing_keypad(1);
    vt_move(21,1); printf("<%s>%20s", fnkeymodes[fkeymode], "");
    vt_move(max_lines-2,1); vt_el(0);
    vt_move(max_lines-2,1); printf("%s", "Press each function key. Finish with TAB.");

    for(;;) {
      vt_move(max_lines-1,1);
      if (fkeymode == 0)  default_level();
      if (fkeymode != 0)  set_level(1);    /* VT100 mode */

      fnkeystr = instr();

      vt_move(max_lines-1,1); vt_el(0);
      vt_move(max_lines-1,1); chrprint(fnkeystr);

      if (!strcmp(fnkeystr,"\t")) break;
      if ((i = find_editing_key(fnkeystr, fkeymode)) >= 0) {
        vt_hilite(TRUE);
        show_result(" (%s key) ", edt_keypadtab[i].fnkeyname);
        vt_hilite(FALSE);
        vt_move(1 + 2 * edt_keypadtab[i].fnkeyrow, 1 + edt_keypadtab[i].fnkeycol);
        printf("%s", edt_keypadtab[i].fnkeysymbol);
      } else {
        vt_hilite(TRUE);
        show_result("%s", " (Unknown function key) ");
        vt_hilite(FALSE);
      }
    }
  }

  vt_move(max_lines-1,1); vt_el(0);
  restore_level(&save);
  restore_ttymodes();
  return MENU_MERGE;
}

static int
tst_FunctionKeys(MENU_ARGS)
{
  int  i;
  int  fkeymode;
  char *fnkeystr;
  VTLEVEL save;

  static char *fnkeymodes[] = {
      "Normal mode (F6-F20, except xterm also F1-F5)",
      "VT100/VT52 mode (F11-F13 only)"
  };

  save_level(&save);
  show_keyboard(0, (char *)0);
  show_cursor_keys(0);
  show_editing_keypad(0);
  show_numeric_keypad(0);
  vt_move(max_lines-2,1);

  if (terminal_id() < 200) {
    printf("Sorry, a real VT%d terminal doesn't have function keys\n", terminal_id());
    return MENU_HOLD;
  }

  set_tty_crmod(FALSE);
  set_tty_echo(FALSE);

  for (fkeymode = 0; fkeymode <= 1; fkeymode++) {
    show_function_keys(1);
    vt_move(21,1); printf("<%s>%20s", fnkeymodes[fkeymode], "");
    vt_move(max_lines-2,1); vt_el(0);
    vt_move(max_lines-2,1); printf("%s", "Press each function key. Finish with TAB.");

    for(;;) {
      vt_move(max_lines-1,1);
      if (fkeymode == 0)  default_level();
      if (fkeymode != 0)  set_level(1);    /* VT100 mode */

      fnkeystr = instr();

      vt_move(max_lines-1,1); vt_el(0);
      vt_move(max_lines-1,1); chrprint(fnkeystr);

      if (!strcmp(fnkeystr,"\t")) break;
      if ((i = find_function_key(fnkeystr, fkeymode)) >= 0) {
        vt_hilite(TRUE);
        show_result(" (%s key) ", fnkeytab[i].fnkeyname);
        vt_hilite(FALSE);
        vt_move(1 + 2 * fnkeytab[i].fnkeyrow, 1 + fnkeytab[i].fnkeycol);
        printf("%s", fnkeytab[i].fnkeysymbol);
      } else {
        vt_hilite(TRUE);
        show_result("%s", " (Unknown function key) ");
        vt_hilite(FALSE);
      }
    }
  }

  vt_move(max_lines-1,1); vt_el(0);
  restore_level(&save);
  restore_ttymodes();
  return MENU_MERGE;
}

static int
tst_NumericKeypad(MENU_ARGS)
{
  int  i;
  int  fkeymode;
  char *fnkeystr;
  VTLEVEL save;

  static char *fnkeymodes[4] = {
      "ANSI Numeric mode",
      "ANSI Application mode",
      "VT52 Numeric mode",
      "VT52 Application mode"
  };

  vt_clear(2);
  save_level(&save);
  show_keyboard(0, (char *)0);
  show_cursor_keys(0);
  show_function_keys(0);
  show_editing_keypad(0);
  vt_move(max_lines-2,1);

  set_tty_crmod(FALSE);
  set_tty_echo(FALSE);

  for (fkeymode = 0; fkeymode <= 3; fkeymode++) {
    show_numeric_keypad(1);
    vt_move(21,1); printf("<%s>%20s", fnkeymodes[fkeymode], "");
    vt_move(max_lines-2,1); vt_el(0);
    vt_move(max_lines-2,1); printf("%s", "Press each function key. Finish with TAB.");

    for(;;) {
      vt_move(max_lines-1,1);
      if (fkeymode >= 2)  set_level(0);    /* VT52 mode */
      if (fkeymode % 2)   deckpam();   /* Application mode */
      else                deckpnm();   /* Numeric mode     */
      fnkeystr = instr();
      set_level(1);                    /* ANSI mode */

      vt_move(max_lines-1,1); vt_el(0);
      vt_move(max_lines-1,1); chrprint(fnkeystr);

      if (!strcmp(fnkeystr,"\t")) break;
      if ((i = find_num_keypad_key(fnkeystr, fkeymode)) >= 0) {
        vt_hilite(TRUE);
        show_result(" (%s key) ", num_keypadtab[i].fnkeyname);
        vt_hilite(FALSE);
        vt_move(1 + 2 * num_keypadtab[i].fnkeyrow, 1 + num_keypadtab[i].fnkeycol);
        printf("%s", num_keypadtab[i].fnkeysymbol);
      } else {
        vt_hilite(TRUE);
        show_result("%s", " (Unknown function key) ");
        vt_hilite(FALSE);
      }
    }
  }

  vt_move(max_lines-1,1); vt_el(0);
  restore_level(&save);
  restore_ttymodes();
  return MENU_MERGE;
}

static int
tst_KeyboardLayout(MENU_ARGS)
{
  static MENU keyboardmenu[] = {
      { "Standard American ASCII layout",                    default_layout },
      { "Swedish national layout D47",                       set_D47_layout },
      { "Swedish national layout E47",                       set_E47_layout },
        /* add new keyboard layouts here */
      { "",                                                  0 }
    };

  if (terminal_id() < 200) {
    vt_clear(2);
    keytab = VT100_keytab;
    title(0); println("Choose keyboard layout:");
    (void) menu(keyboardmenu);
  }

  tst_keyboard_layout((char *)0);

  return MENU_MERGE;
}

static int
tst_LED_Lights(MENU_ARGS)
{
  int  i;
  char *ledmsg[6], *ledseq[6];

  ledmsg[0] = "L1 L2 L3 L4"; ledseq[0] = "1;2;3;4";
  ledmsg[1] = "   L2 L3 L4"; ledseq[1] = "1;0;4;3;2";
  ledmsg[2] = "   L2 L3";    ledseq[2] = "1;4;;2;3";
  ledmsg[3] = "L1 L2";       ledseq[3] = ";;2;1";
  ledmsg[4] = "L1";          ledseq[4] = "1";
  ledmsg[5] = "";            ledseq[5] = "";

#ifdef UNIX
  fflush(stdout);
#endif
  vt_clear(2);
  vt_move(10,1);
  println("These LEDs (\"lamps\") on the keyboard should be on:");
  for (i = 0; i <= 5; i++) {
    vt_move(10,52); vt_el(0); printf("%s", ledmsg[i]);
    decll("0");
    decll(ledseq[i]);
    vt_move(12,1); holdit();
  }
  return MENU_NOHOLD;
}

/******************************************************************************/
int
tst_keyboard_layout(char *scs_params)
{
  int  i;
  int  kbdc;
  char temp[80];
  char *kbds = strcpy(temp, " ");

  vt_clear(2);
  show_keyboard(1, scs_params);
  show_cursor_keys(0);
  show_function_keys(0);
  show_editing_keypad(0);
  show_numeric_keypad(0);
  vt_move(max_lines-2,1);

  set_tty_crmod(FALSE);
  set_tty_echo(FALSE);

  inflush();
  printf("Press each key, both shifted and unshifted. Finish with RETURN:");
  do { /* while (kbdc != 13) */
    vt_move(max_lines-1,1); kbdc = inchar();
    vt_move(max_lines-1,1); vt_el(0);
    if (scs_params != 0 && kbdc > ' ' && kbdc < '\177') {
      vt_hilite(TRUE);
      esc(scs_params);
      printf(" %c ", kbdc);
      scs(0, 'B');
      printf("= %d ", kbdc);
      scs(0, 'B');
      vt_hilite(FALSE);
    } else {
      sprintf(kbds, "%c", kbdc);
      chrprint(kbds);
    }
    for (i = 0; keytab[i].c != '\0'; i++) {
      if (keytab[i].c == kbdc) {
        show_character(i, scs_params, FALSE);
        /* LK401 keyboard will have more than one hit for '<' and '>' */
      }
    }
  } while (kbdc != 13);

  vt_move(max_lines-1,1); vt_el(0);
  restore_ttymodes();
  return MENU_MERGE;
}

/******************************************************************************/
int
tst_keyboard(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "LED Lights",                                        tst_LED_Lights },
      { "Auto Repeat",                                       tst_AutoRepeat },
      { "KeyBoard Layout",                                   tst_KeyboardLayout },
      { "Cursor Keys",                                       tst_CursorKeys },
      { "Numeric Keypad",                                    tst_NumericKeypad },
      { "Editing Keypad",                                    tst_EditingKeypad },
      { "Function Keys",                                     tst_FunctionKeys },
      { "AnswerBack",                                        tst_AnswerBack },
      { "Control Keys",                                      tst_ControlKeys },
      { "", 0 }
    };

  do {
    vt_clear(2);
    title(0); printf("Keyboard Tests");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
