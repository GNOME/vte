/* $Id$ */

#include <vttest.h>
#include <ttymodes.h>
#include <esc.h>

#define is_inter(c) ((c) >= 0x20 && ((c) <= 0x2f))
#define is_final(c) ((c) >= 0x30 && ((c) <= 0x7e))

#define L_CURL '{'
#define MAX_WIDTH 10

static char *EraseCtl = "";
static char *FontName = "";
static char *StartingCharPtr = "";
static char *TextCell = "";
static char *WidthAttr = "";
static char *font_string = "";
static int FontNumber;
static int MatrixHigh;
static int MatrixWide;
static int StartingCharNum;

/*
 * Lookup the given character 'chr' in the font-string and write a readable
 * display of the glyph
 */
static void
decode_header(void)
{
  int Pe, Pcms, Pw, Pt;
  char *s;

  switch (sscanf(font_string+2, "%d;%d;%d;%d;%d;%d", &FontNumber, &StartingCharNum, &Pe, &Pcms, &Pw, &Pt)) {
  case 0: FontNumber  = 0;
  case 1: StartingCharNum = 0;
  case 2: Pe   = 0;
  case 3: Pcms = 0;
  case 4: Pw   = 0;
  case 5: Pt   = 0;
  case 6:
    break;
  }

  switch (Pcms) {
  case 1: MatrixWide =  0;  MatrixHigh =  0; break; /* illegal */
  case 2: MatrixWide =  5;  MatrixHigh = 10; break;
  case 3: MatrixWide =  6;  MatrixHigh = 10; break;
  case 0:
  case 4: MatrixWide =  7;  MatrixHigh = 10; break;
  default: MatrixWide = Pcms; MatrixHigh = 10; break; /* 5 thru 10 */
  }

  switch (Pe) {
  case 0: EraseCtl = "this DRCS set"; break;
  case 1: EraseCtl = "only reloaded chars"; break;
  case 2: EraseCtl = "all chars in all DRCS sets"; break;
  default: EraseCtl = "?"; break;
  }

  switch (Pw) {
  case  0: /* FALLTHRU */
  case  1:  WidthAttr =  "80 cols, 24 lines"; break;
  case  2:  WidthAttr = "132 cols, 24 lines"; break;
  case 11:  WidthAttr =  "80 cols, 36 lines"; break;
  case 12:  WidthAttr = "132 cols, 36 lines"; break;
  case 21:  WidthAttr =  "80 cols, 24 lines"; break;
  case 22:  WidthAttr = "132 cols, 48 lines"; break;
  default:  WidthAttr = "?"; break;
  }

  if (Pt == 2)
    TextCell = "Full Cell";
  else
    TextCell = "Text";
  for (s = font_string; *s; s++) {
    if (*s == L_CURL) {
      char *t;
      char tmp[BUFSIZ];
      size_t use = 0;
      for (t = s + 1; *t; t++) {
        if (is_inter(*t)) {
          tmp[use++] = *t;
        }
        if (is_final(*t)) {
          tmp[use++] = *t++;
          tmp[use] = '\0';
          FontName = strcpy(malloc(use+1), tmp);
          StartingCharPtr = t;
          break;
        }
      }
      break;
    }
  }
}

static char *
find_char(int chr)
{
  char *s = StartingCharPtr;

  chr -= (' ' + StartingCharNum);
  if (chr < 0)
    return 0;
  while (chr > 0) {
    do {
      if (*s == '\0')
        return 0;
    } while (*s++ != ';');
    chr--;
  }
  return s;
}

static void
display_head(FILE *fp)
{
  fprintf(fp, "Font %d:%s, Matrix %dx%d (%s, %s)\n",
    FontNumber, FontName, MatrixWide, MatrixHigh, WidthAttr, TextCell);
  fprintf(fp, "Start %d, Erase %s\n",
    StartingCharNum, EraseCtl);
}

static int
display_char(FILE *fp, int chr)
{
  char *s;
  int bit, n, high;
  char bits[6][MAX_WIDTH];

  s = find_char(chr);
  if (s != 0) {
    fprintf(fp, "Glyph '%c'\n", chr);
    bit = 0;
    high = 0;
    do {
      if (*s >= '?' && *s <= '~') {
        for (n = 0; n < 6; n++)
          bits[n][bit] = ((*s - '?') & 1 << n) ? 'O' : '.';
        bit++;
      } else if ((*s == ';' || *s == '/') && bit) {
        for (n = 0; (n < 6) && (high++ < MatrixHigh); n++) {
          bits[n][bit] = '\0';
          fprintf(fp, "%s\n", bits[n]);
        }
        bit = 0;
      }
    } while (*s++ != ';');
    return TRUE;
  }
  return FALSE;
}

static int
tst_DECDLD(MENU_ARGS)
{
  char *s;

  vt_move(1,1);
  printf("Working...\n");
  for (s = font_string; *s; s++) {
    putchar(*s);
    if (*s == '\n') {
      fflush(stdout);
      padding(20);
    }
  }
  fflush(stdout);
  padding(20);
  printf("...done ");

  printf("%c*%s", ESC, FontName); /* designate G2 as the DRCS font */
  padding(4);
  fflush(stdout);

  return MENU_HOLD;
}

static int
tst_display(MENU_ARGS)
{
  int d, c = -1;

  vt_move(1,1);
  display_head(stdout);
  println("");
  println("Press any key to display its soft-character.  Repeat a key to quit.");

  set_tty_raw(TRUE);
  set_tty_echo(FALSE);

  do {
    d = c;
    c = inchar();
    vt_move(6,1);
    vt_clear(0);
    if (display_char(stdout, c)) {
      println("");
      printf("Render: %cN%c", ESC, c); /* use SS2 to invoke G2 into GL */
    }
  } while (c != d);

  restore_ttymodes();
  return MENU_NOHOLD;
}

/*
 * Remove all characters in all DRCS sets (the " @" is a dummy name)
 */
static int
tst_cleanup(MENU_ARGS)
{
  do_dcs("1;1;2%c @", L_CURL);
  padding(20);
  return MENU_NOHOLD;
}

/*
 * Read a soft-character definition string from a file.  Strip off garbage
 * at the beginning (to accommodate the "font2xx" output format).
 */
void
setup_softchars(char *filename)
{
  FILE *fp;
  int c;
  size_t len = 1024;
  size_t use = 0;
  char *buffer = malloc(len);
  char *s;
  char *first = 0;
  char *last = 0;
  int save_8bits = input_8bits;
  input_8bits = FALSE; /* use the 7-bit input-parsing */

  /* read the file into memory */
  if ((fp = fopen(filename, "r")) == 0) {
    perror(filename);
    exit(EXIT_FAILURE);
  }
  while ((c = fgetc(fp)) != EOF) {
    if (use+1 >= len) {
      buffer = realloc(buffer, len *= 2);
    }
    buffer[use++] = c;
  }
  buffer[use] = '\0';
  fclose(fp);

  /* find the DCS that begins the control string */
  /* and the ST that ends the control string */
  for (s = buffer; *s; s++) {
    if (first == 0) {
      if (skip_dcs(s) != 0)
        first = s;
    } else {
      if (!strncmp(s, st_input(), 2)) {
        last = s + 2;
        *last = '\0';
        break;
      }
    }
  }
  input_8bits = save_8bits;

  if (first == 0 || last == 0) {
    fprintf(stderr, "Not a vtXXX font description: %s\n", filename);
    exit(EXIT_FAILURE);
  }
  for (s = buffer; (*s++ = *first++) != '\0'; )
    ;
  if (LOG_ENABLED && first != 0)
    fprintf(log_fp, "Font String:\n%s\n", buffer);

  font_string = buffer;

  decode_header();
}

int
tst_softchars(MENU_ARGS)
{
  static MENU my_menu[] = {
      { "Exit",                                              0 },
      { "Download the soft characters",                      tst_DECDLD },
      { "Examine the soft characters",                       tst_display },
      { "Clear the soft characters",                         tst_cleanup },
      { "",                                                  0 }
    };

  vt_move(1,1);
  if (font_string == 0 || *font_string == 0) {
    printf("You did not specify a font-file with the -f option\n");
    return MENU_HOLD;
  }
  do {
    vt_clear(2);
    title(0); printf("Soft Character Sets");
    title(2); println("Choose test type:");
  } while (menu(my_menu));
  return MENU_NOHOLD;
}
