/* $Id$ */

#include <vttest.h>
#include <esc.h>

/* FIXME: for Solaris 2.5, which is broken */
#define FLUSH fflush(stdout)

static int soft_scroll;

/******************************************************************************/

static char csi_7[] = { ESC, '[', 0 };
static unsigned char csi_8[] = { 0x9b, 0 };

char *
csi_input(void)
{
  return input_8bits ? (char *)csi_8 : csi_7;
}

char *
csi_output(void)
{
  return output_8bits ? (char *)csi_8 : csi_7;
}

/******************************************************************************/

static char dcs_7[] = { ESC, 'P', 0 };
static unsigned char dcs_8[] = { 0x90, 0 };

char *
dcs_input(void)
{
  return input_8bits ? (char *)dcs_8 : dcs_7;
}

char *
dcs_output(void)
{
  return output_8bits ? (char *)dcs_8 : dcs_7;
}

/******************************************************************************/

static char osc_7[] = { ESC, ']', 0 };
static unsigned char osc_8[] = { 0x9d, 0 };

char *
osc_input(void)
{
  return input_8bits ? (char *)osc_8 : osc_7;
}

char *
osc_output(void)
{
  return output_8bits ? (char *)osc_8 : osc_7;
}

/******************************************************************************/

static char ss3_7[] = { ESC, 'O', 0 };
static unsigned char ss3_8[] = { 0x8f, 0 };

char *
ss3_input(void)
{
  return input_8bits ? (char *)ss3_8 : ss3_7;
}

char *
ss3_output(void)
{
  return output_8bits ? (char *)ss3_8 : ss3_7;
}

/******************************************************************************/

static char st_7[] = { ESC, '\\', 0 };
static unsigned char st_8[] = { 0x9c, 0 };

char *
st_input(void)
{
  return input_8bits ? (char *)st_8 : st_7;
}

char *
st_output(void)
{
  return output_8bits ? (char *)st_8 : st_7;
}

/******************************************************************************/

/*
 * The actual number of nulls for padding is an estimate; it's working at
 * 9600bd.
 */
void
padding(int msecs)
{
  if (use_padding) {
    int count = (3 * msecs * tty_speed + DEFAULT_SPEED - 1) / DEFAULT_SPEED;
    while (count-- > 0)
      putchar(0);
  }
}

void
extra_padding(int msecs)
{
  if (use_padding)
    padding (soft_scroll ? (msecs * 4) : msecs);
}

void
println(char *s)
{
  printf("%s\r\n", s);
}

void
put_char(FILE *fp, int c)
{
  if (fp == stdout)
    putchar(c);
  else {
    c &= 0xff;
    if (c <= ' ' || c >= '\177')
      fprintf(fp, "<%d> ", c);
    else
      fprintf(fp, "%c ", c);
  }
}

void
put_string(FILE *fp, char *s)
{
  while (*s != '\0')
    put_char(fp, *s++);
}

static void
va_out(FILE *fp, va_list ap, char *fmt)
{
  char temp[10];

  while (*fmt != '\0') {
    if (*fmt == '%') {
      switch(*++fmt) {
      case 'c':
	put_char(fp, va_arg(ap, int));
	break;
      case 'd':
	sprintf(temp, "%d", va_arg(ap, int));
	put_string(fp, temp);
	break;
      case 's':
	put_string(fp, va_arg(ap, char *));
	break;
      }
    } else {
      put_char(fp, *fmt);
    }
    fmt++;
  }
}

/* CSI xxx */
void
do_csi(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs(csi_output(), stdout);
  va_out(stdout, ap, fmt);
  va_end(ap);
  FLUSH;

  if (LOG_ENABLED) {
    fputs("Send: ", log_fp);
    put_string(log_fp, csi_output());
    va_start(ap, fmt);
    va_out(log_fp, ap, fmt);
    va_end(ap);
    fputs("\n", log_fp);
  }
}

/* DCS xxx ST */
void
do_dcs(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs(dcs_output(), stdout);
  va_out(stdout, ap, fmt);
  va_end(ap);
  fputs(st_output(), stdout);
  FLUSH;

  if (LOG_ENABLED) {
    va_start(ap, fmt);
    fputs("Send: ", log_fp);
    put_string(log_fp, dcs_output());
    va_out(log_fp, ap, fmt);
    va_end(ap);
    put_string(log_fp, st_output());
    fputs("\n", log_fp);
  }
}

/* DCS xxx ST */
void
do_osc(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs(osc_output(), stdout);
  va_out(stdout, ap, fmt);
  va_end(ap);
  fputs(st_output(), stdout);
  FLUSH;

  if (LOG_ENABLED) {
    va_start(ap, fmt);
    fputs("Send: ", log_fp);
    put_string(log_fp, osc_output());
    va_out(log_fp, ap, fmt);
    va_end(ap);
    put_string(log_fp, st_output());
    fputs("\n", log_fp);
  }
}

void
esc(char *s)
{
  printf("%c%s", ESC, s);

  if (LOG_ENABLED) {
    fprintf(log_fp, "Send: ");
    put_char(log_fp, ESC);
    put_string(log_fp, s);
    fputs("\n", log_fp);
  }
}

void
brc(int pn, int c)
{
  do_csi("%d%c", pn, c);
}

void
brc2(int pn1, int pn2, int c)
{
  do_csi("%d;%d%c", pn1, pn2, c);
}

void
brc3(int pn1, int pn2, int pn3, int c)
{
  do_csi("%d;%d;%d%c", pn1, pn2, pn3, c);
}

/******************************************************************************/

void
cbt(int pn) /* Cursor Back Tab */
{
  brc(pn, 'Z');
}

void
cha(int pn) /* Cursor Character Absolute */
{
  brc(pn, 'G');
}

void
cht(int pn) /* Cursor Forward Tabulation */
{
  brc(pn, 'I');
}

void
cnl(int pn) /* Cursor Next Line */
{
  brc(pn,'E');
}

void
cpl(int pn) /* Cursor Previous Line */
{
  brc(pn,'F');
}

void
cub(int pn)  /* Cursor Backward */
{
  brc(pn,'D');
  padding(2);
}

void
cud(int pn)  /* Cursor Down */
{
  brc(pn,'B');
  extra_padding(2);
}

void
cuf(int pn)  /* Cursor Forward */
{
  brc(pn,'C');
  padding(2);
}

void
cup(int pn1, int pn2)  /* Cursor Position */
{
  brc2(pn1, pn2, 'H');
  padding(5); /* 10 for vt220 */
}

void
cuu(int pn)  /* Cursor Up */
{
  brc(pn,'A');
  extra_padding(2);
}

void
da(void)  /* Device Attributes */
{
  brc(0,'c');
}

void
decaln(void)  /* Screen Alignment Display */
{
  esc("#8");
}

void
decbi(void) /* VT400: Back Index */
{
  esc("6");
  padding(40);
}

void
decbkm(int flag) /* VT400: Backarrow key */
{
  if (flag)
    sm("?67"); /* backspace */
  else
    rm("?67"); /* delete */
}

void
deccara(int top, int left, int bottom, int right, int attr)
{
  do_csi("%d;%d;%d;%d;%d$r", top, left, bottom, right, attr);
}

void
deccolm(int flag) /* 80/132 Columns */
{
  if (flag)
    sm("?3"); /* 132 columns */
  else
    rm("?3"); /* 80 columns */
}

void
deccra(int Pts, int Pl, int Pbs, int Prs, int Pps, int Ptd, int Pld, int Ppd)
{
  do_csi("%d;%d;%d;%d;%d;%d;%d;%d;$v",
    Pts,  /* top-line border */
    Pl,   /* left-line border */
    Pbs,  /* bottom-line border */
    Prs,  /* right-line border */
    Pps,  /* source page number */
    Ptd,  /* destination top-line border */
    Pld,  /* destination left-line border */
    Ppd); /* destination page number */
}

void
decdc(int pn) /* VT400 Delete Column */
{
  do_csi("%d'~", pn);
  padding(10 * pn);
}

void
decefr(int top, int left, int bottom, int right) /* DECterm Enable filter rectangle */
{
  do_csi("%d;%d;%d;%d'w", top, left, bottom, right);
}

void
decelr(int all_or_one, int pixels_or_cells) /* DECterm Enable Locator Reporting */
{
  do_csi("%d;%d'z", all_or_one, pixels_or_cells);
}

void
decera(int top, int left, int bottom, int right) /* VT400 Erase Rectangular area */
{
  do_csi("%d;%d;%d;%d$z", top, left, bottom, right);
}

void
decdhl(int lower)  /* Double Height Line (also double width) */
{
  if (lower) esc("#4");
  else       esc("#3");
}

void
decdwl(void)  /* Double Wide Line */
{
  esc("#6");
}

void
decfi(void) /* VT400: Forward Index */
{
  esc("9");
  padding(40);
}

void
decfra(int c, int top, int left, int bottom, int right) /* VT400 Fill Rectangular area */
{
  do_csi("%d;%d;%d;%d;%d$x", c, top, left, bottom, right);
}

void
decid(void) /* required for VT52, not recommended above VT100 */
{
  esc("Z");     /* Identify     */
}

void
decic(int pn) /* VT400 Insert Column */
{
  do_csi("%d'}", pn);
  padding(10 * pn);
}

void
deckbum(int flag) /* VT400: Keyboard Usage */
{
  if (flag)
    sm("?68"); /* data processing */
  else
    rm("?68"); /* typewriter */
}

void
deckpam(void)  /* Keypad Application Mode */
{
  esc("=");
}

void
deckpm(int flag) /* VT400: Keyboard Position */
{
  if (flag)
    sm("?81"); /* position reports */
  else
    rm("?81"); /* character codes */
}

void
deckpnm(void)  /* Keypad Numeric Mode */
{
  esc(">");
}

void
decll(char *ps)  /* Load LEDs */
{
  do_csi("%sq", ps);
}

void
decnkm(int flag) /* VT400: Numeric Keypad */
{
  if (flag)
    sm("?66"); /* application */
  else
    rm("?66"); /* numeric */
}

void
decpex(int flag) /* VT220: printer extent mode */
{
  if (flag)
    sm("?19"); /* full screen (page) */
  else
    rm("?19"); /* scrolling region */
}

void
decpff(int flag) /* VT220: print form feed mode */
{
  if (flag)
    sm("?18"); /* form feed */
  else
    rm("?18"); /* no form feed */
}

void
decnrcm(int flag) /* VT220: National replacement character set */
{
  if (flag)
    sm("?42"); /* national */
  else
    rm("?42"); /* multinational */
}

void
decrara(int top, int left, int bottom, int right, int attr)
{
  do_csi("%d;%d;%d;%d;%d$t", top, left, bottom, right, attr);
}

void
decrc(void)  /* Restore Cursor */
{
  esc("8");
}

void
decreqtparm(int pn)  /* Request Terminal Parameters */
{
  brc(pn,'x');
}

void
decrqlp(int mode) /* DECterm Request Locator Position */
{
  do_csi("%d'|", mode);
}

void
decrqss(char *pn) /* VT200 Request Status-String */
{
  do_dcs("$q%s", pn);
}

void
decsace(int flag) /* VT400 Select attribute change extent */
{
  do_csi("%d*x", flag ? 2 : 0);
}

void
decsasd(int pn) /* VT200 Select active status display */
{
  do_csi("%d$}", pn);
}

void
decsc(void)  /* Save Cursor */
{
  esc("7");
}

void
decsca(int pn1) /* VT200 select character attribute (protect) */
{
  do_csi("%d\"q", pn1);
}

void
decsclm(int flag) /* Scrolling mode (smooth/jump) */
{
  if (flag)
    sm("?4"); /* smooth scrolling */
  else
    rm("?4"); /* jump-scrolling scrolling */
  soft_scroll = flag;
}

void
decscnm(int flag) /* Screen mode (inverse-video) */
{
  if (flag)
    sm("?5"); /* inverse video */
  else
    rm("?5"); /* normal video */
  padding(200);
}

void
decsed(int pn1) /* VT200 selective erase in display */
{
  do_csi("?%dJ", pn1);
}

void
decsel(int pn1) /* VT200 selective erase in line */
{
  do_csi("?%dK", pn1);
}

void
decsera(int top, int left, int bottom, int right) /* VT400 Selective erase rectangular area */
{
  do_csi("%d;%d;%d;%d${", top, left, bottom, right);
}

void
decsle(int mode) /* DECterm Select Locator Events */
{
  do_csi("%d'{", mode);
}

void
decsnls(int pn) /* VT400 Select number of lines per screen */
{
  do_csi("%d*|", pn);
}

void
decssdt(int pn) /* VT200 Select status line type */
{
  do_csi("%d$~", pn);
}

void
decstbm(int pn1, int pn2)  /* Set Top and Bottom Margins */
{
  if (pn1 || pn2) brc2(pn1, pn2, 'r');
  else            esc("[r");
  /* Good for >24-line terminals */
}

void
decstr(void)  /* VT200 Soft terminal reset */
{
  do_csi("!p");
}

void
decswl(void)  /* Single Width Line */
{
  esc("#5");
}

void
dectst(int pn)  /* Invoke Confidence Test */
{
  brc2(2, pn, 'y');
#ifdef UNIX
  fflush(stdout);
#endif
}

void
dsr(int pn)  /* Device Status Report */
{
  brc(pn, 'n');
}

void
ed(int pn)  /* Erase in Display */
{
  brc(pn, 'J');
  padding(50);
}

void
el(int pn)  /* Erase in Line */
{
  brc(pn,'K');
  padding(3); /* 4 for vt400 */
}

void
ech(int pn) /* Erase character(s) */
{
  brc(pn,'X');
}

void
hpa(int pn) /* Character Position Absolute */
{
  brc(pn, '`');
}

void
hts(void)  /* Horizontal Tabulation Set */
{
  esc("H");
}

void
hvp(int pn1, int pn2)  /* Horizontal and Vertical Position */
{
  brc2(pn1, pn2, 'f');
}

void
ind(void)  /* Index */
{
  esc("D");
  padding(20); /* vt220 */
}

/* The functions beginning "mc_" are variations of Media Copy (MC) */

void
mc_autoprint(int flag) /* VT220: auto print mode */
{
  do_csi("?%di", flag ? 5 : 4);
}

void
mc_printer_controller(int flag) /* VT220: printer controller mode */
{
  do_csi("%di", flag ? 5 : 4);
}

void
mc_print_page(void) /* VT220: print page */
{
  do_csi("i");
}

void
mc_print_composed(void) /* VT300: print composed main display */
{
  do_csi("?10i");
}

void
mc_print_all_pages(void) /* VT300: print composed all pages */
{
  do_csi("?11i");
}

void
mc_print_cursor_line(void) /* VT220: print cursor line */
{
  do_csi("?1i");
}

void
mc_printer_start(int flag) /* VT300: start/stop printer-to-host session */
{
  do_csi("?%di", flag ? 9 : 8);
}

void
mc_printer_assign(int flag) /* VT300: assign/release printer to active session */
{
  do_csi("?%di", flag ? 18 : 19);
}

void
nel(void)  /* Next Line */
{
  esc("E");
}

void
rep(int pn) /* Repeat */
{
  do_csi("%db", pn);
}

void
ri(void)  /* Reverse Index */
{
  esc("M");
  extra_padding(5); /* 14 on vt220 */
}

void
ris(void) /*  Reset to Initial State */
{
  esc("c");
#ifdef UNIX
  fflush(stdout);
#endif
}

void
rm(char *ps)  /* Reset Mode */
{
  do_csi("%sl", ps);
}

void s8c1t(int flag) /* Tell terminal to respond with 7-bit or 8-bit controls */
{
  if ((input_8bits = flag) != FALSE)
    esc(" G"); /* select 8-bit controls */
  else
    esc(" F"); /* select 7-bit controls */
  fflush(stdout);
  zleep(300);
}

void
scs(int g, int c)  /* Select character Set */
{
  printf("%c%c%c%c%c%c%c", ESC, g ? ')' : '(', c,
                           ESC, g ? '(' : ')', 'B',
			   g ? 14 : 15);
  padding(4);
}

void
sd(int pn)  /* Scroll Down */
{
  brc(pn, 'T');
}

void
sgr(char *ps)  /* Select Graphic Rendition */
{
  do_csi("%sm", ps);
  padding(2);
}

void
sl(int pn)  /* Scroll Left */
{
  do_csi("%d @", pn);
}

void
sm(char *ps)  /* Set Mode */
{
  do_csi("%sh", ps);
}

void
sr(int pn)  /* Scroll Right */
{
  do_csi("%d A", pn);
}

void
srm(int flag) /* VT400: Send/Receive mode */
{
  if (flag)
    sm("12"); /* local echo off */
  else
    rm("12"); /* local echo on */
}

void
su(int pn)  /* Scroll Up */
{
  brc(pn, 'S');
  extra_padding(5);
}

void
tbc(int pn)  /* Tabulation Clear */
{
  brc(pn, 'g');
}

void
dch(int pn) /* Delete character */
{
  brc(pn, 'P');
}

void
ich(int pn) /* Insert character -- not in VT102 */
{
  brc(pn, '@');
}

void
dl(int pn) /* Delete line */
{
  brc(pn, 'M');
}

void
il(int pn) /* Insert line */
{
  brc(pn, 'L');
}

void
vpa(int pn) /* Line Position Absolute */
{
  brc(pn, 'd');
}

void
vt52cub1(void) /* cursor left */
{
  esc("D");
  padding(5);
}

void
vt52cud1(void) /* cursor down */
{
  esc("B");
  padding(5);
}

void
vt52cuf1(void) /* cursor right */
{
  esc("C");
  padding(5);
}

void
vt52cup(int l, int c) /* direct cursor address */
{
  printf("%cY%c%c", ESC, l + 31, c + 31);
  padding(5);
}

void
vt52cuu1(void) /* cursor up */
{
  esc("A");
  padding(5);
}

void
vt52ed(void) /* erase to end of screen */
{
  esc("J");
  padding(5);
}

void
vt52el(void) /* erase to end of line */
{
  esc("K");
  padding(5);
}

void
vt52home(void) /* cursor to home */
{
  esc("H");
  padding(5);
}

void
vt52ri(void) /* reverse line feed */
{
  esc("I");
  padding(5);
}

