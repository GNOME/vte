/* $Id$ */

#define DEBUG

#include <stdarg.h>
#include <vttest.h>
#include <esc.h>
#include <ttymodes.h>

#include <starlet.h>
#include <lib$routines.h>
#include <stsdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <iodef.h>
#include <ttdef.h>
#include <tt2def.h>

typedef struct  {
        unsigned short int status;      /* I/O completion status */
        unsigned short int count;       /* byte transfer count   */
        int dev_dep_data;               /* device dependant data */
        } QIO_SB;                       /* This is a QIO I/O Status Block */

#define NIBUF   1024                    /* Input buffer size            */
#define NOBUF   1024                    /* MM says big buffers win!     */
#define EFN     0                       /* Event flag                   */

static char     obuf[NOBUF];            /* Output buffer                */
static int      nobuf;                  /* # of bytes in above          */
static char     ibuf[NIBUF];            /* Input buffer                 */
static int      nibuf;                  /* # of bytes in above          */
static int      oldmode[3];             /* Old TTY mode bits            */
static int      newmode[3];             /* New TTY mode bits            */
static short    iochan;                 /* TTY I/O channel              */

static int      in_flags;
static int      cr_flag = TRUE;

static void
give_up(int status)
{
  if (LOG_ENABLED)
    fprintf(log_fp, "status=%#x\n", status);
  close_tty();
  exit(status);
}

static int
lookup_speed(int code)
{
  static struct {
    int code;
    int speed;
  } table[] = {
     {TT$C_BAUD_50, 50 }
    ,{TT$C_BAUD_75, 75 }
    ,{TT$C_BAUD_110, 110 }
    ,{TT$C_BAUD_134, 134 }
    ,{TT$C_BAUD_150, 150 }
    ,{TT$C_BAUD_300, 300 }
    ,{TT$C_BAUD_600, 600 }
    ,{TT$C_BAUD_1200, 1200 }
    ,{TT$C_BAUD_1800, 1800 }
    ,{TT$C_BAUD_2000, 2000 }
    ,{TT$C_BAUD_2400, 2400 }
    ,{TT$C_BAUD_3600, 3600 }
    ,{TT$C_BAUD_4800, 4800 }
    ,{TT$C_BAUD_7200, 7200 }
    ,{TT$C_BAUD_9600, 9600 }
    ,{TT$C_BAUD_19200, 19200 }
    ,{TT$C_BAUD_38400, 38400 }
#ifdef TT$C_BAUD_57600
    ,{TT$C_BAUD_57600, 57600 }
    ,{TT$C_BAUD_76800, 76800 }
    ,{TT$C_BAUD_115200, 115200 }
#endif
  };
  int n;
  int speed = DEFAULT_SPEED;
  for (n = 0; n < TABLESIZE(table); n++) {
    if (table[n].code == code) {
      if (table[n].speed > DEFAULT_SPEED)
        speed = table[n].speed;
      break;
    }
  }
  return speed;
}

/*
 * Read the tty-in.  If we're looking for a single character, wait. Otherwise,
 * read whatever is available, assuming that it's queued and ready.
 *
 * The VMS terminal driver operates with a one-second timer, and (from the
 * manual) we must give at least two seconds for the timeout value.  If we
 * give a 0-second timeout, we'll only get (from testing) 16 bytes, which
 * is enough for a CPR, but not for DA, etc.
 */
static void
read_vms_tty(int length, int timed)
{
  int status;
  QIO_SB iosb;
  int term[2] = {0,0};
  int my_flags = IO$_READLBLK | in_flags;
  int timeout = 0;

  if (length < 1)
    return;
  if (length > 1) {
    my_flags |= IO$M_TIMED;
    timeout = 1; /* seconds */
  }
  if (timed)
    timeout = 2; /* seconds */

#ifdef DEBUG
  if (LOG_ENABLED) {
    fprintf(log_fp, "reading: len=%d, flags=%#x\n", length, my_flags);
    fflush(log_fp);
  }
#endif
  status = sys$qiow(EFN, iochan, my_flags,
    &iosb, 0, 0, ibuf, length, timeout, term, 0, 0);
#ifdef DEBUG
  if (LOG_ENABLED) {
    fprintf(log_fp,
          "read: st=%d, cnt=%#x, dev=%#x\n",
          iosb.status, iosb.count, iosb.dev_dep_data);
    fflush(log_fp);
  }
#endif

  if (status != SS$_NORMAL
   || iosb.status == SS$_ENDOFFILE)
    give_up(status);

  nibuf = iosb.count + (iosb.dev_dep_data >> 16);
}

/******************************************************************************/

void reset_inchar(void)
{
  /* FIXME */
}

/*
 * Wait until a character is typed on the terminal then read it, without
 * waiting for CR.
 */
char
inchar(void)
{
  int c;

  fflush(stdout);
  read_vms_tty(1, FALSE);
  c = (ibuf[0] & 0xff);
  if (c == '\r' && cr_flag && !(in_flags & IO$M_NOFILTR)) {
    c = '\n';
    if (in_flags & IO$M_NOECHO)
      putchar(c);
  }
  return c;
}

/*
 * Get an unfinished string from the terminal:  wait until a character is typed
 * on the terminal, then read it, and all other available characters.  Return a
 * pointer to that string.
 *
 * This function won't read long strings, but we use it for instances where
 * we only expect short (16 bytes or less) replies (i.e., a function-key).
 */
char *
instr(void)
{
  static char result[1024];

  result[0] = inchar();
  zleep(100); /* Wait 0.1 seconds */
  fflush(stdout);
  read_vms_tty (sizeof(result)-3, FALSE);
  memcpy(result+1, ibuf, nibuf);
  result[1 + nibuf] = '\0';

  if (LOG_ENABLED) {
    fputs("Reply: ", log_fp);
    put_string(log_fp, result);
    fputs("\n", log_fp);
  }

  return(result);
}

/*
 * Get an unfinished string from the terminal:  wait until a character is typed
 * on the terminal, then read it, and all other available characters.  Return a
 * pointer to that string.
 *
 * Unlike 'instr()', this will really read long strings.
 */
char *
get_reply(void)
{
  static char result[256];

  result[0] = inchar();
  zleep(100); /* Wait 0.1 seconds */
  fflush(stdout);
  read_vms_tty (sizeof(result)-3, TRUE);
  memcpy(result+1, ibuf, nibuf);
  result[1 + nibuf] = '\0';

  if (LOG_ENABLED) {
    fputs("Reply: ", log_fp);
    put_string(log_fp, result);
    fputs("\n", log_fp);
  }

  return(result);
}

/*
 * Read to the next newline, truncating the buffer at BUFSIZ-1 characters
 */
void
inputline(char *s)
{
  do {
    int ch;
    char *d = s;
    while ((ch = getchar()) != EOF && ch != '\n') {
      if ((d - s) < BUFSIZ-2)
        *d++ = ch;
    }
    *d = 0;
  } while (!*s);
}

/*
 * Flush input buffer, make sure no pending input character
 */
void
inflush(void)
{
  nibuf = 0;
}

void
outflush(void)
{
  fflush(stdout);
}

void
holdit(void)
{
  inflush();
  printf("Push <RETURN>");
  readnl();
}

void
readnl(void)
{
  fflush(stdout);
  while (inchar() != '\n')
    ;
}

/*
 * Sleep and do nothing (don't waste CPU) for t milliseconds
 */
void
zleep(int t)
{
  float seconds = t/1000.;
  lib$wait(&seconds);
}

/******************************************************************************/

void
init_ttymodes(int pn)
{
  struct dsc$descriptor idsc;
  struct dsc$descriptor odsc;
  char oname[40];
  QIO_SB iosb;
  int status;

  odsc.dsc$a_pointer = "TT";
  odsc.dsc$w_length  = strlen(odsc.dsc$a_pointer);
  odsc.dsc$b_dtype   = DSC$K_DTYPE_T;
  odsc.dsc$b_class   = DSC$K_CLASS_S;
  idsc.dsc$b_dtype   = DSC$K_DTYPE_T;
  idsc.dsc$b_class   = DSC$K_CLASS_S;

  do {
    idsc.dsc$a_pointer = odsc.dsc$a_pointer;
    idsc.dsc$w_length  = odsc.dsc$w_length;
    odsc.dsc$a_pointer = &oname[0];
    odsc.dsc$w_length  = sizeof(oname);

    status = lib$sys_trnlog(&idsc, &odsc.dsc$w_length, &odsc);
    if (status != SS$_NORMAL
     && status != SS$_NOTRAN)
      give_up(status);
    if (oname[0] == 0x1B) {
      odsc.dsc$a_pointer += 4;
      odsc.dsc$w_length  -= 4;
    }
  } while (status == SS$_NORMAL);

  status = sys$assign(&odsc, &iochan, 0, 0);
  if (status != SS$_NORMAL)
    give_up(status);

  status = sys$qiow(EFN, iochan, IO$_SENSEMODE, &iosb, 0, 0,
    oldmode, sizeof(oldmode), 0, 0, 0, 0);
  if (status != SS$_NORMAL
  || iosb.status != SS$_NORMAL)
    give_up(status);
#ifdef DEBUG
  if (LOG_ENABLED)
    fprintf(log_fp,
          "sense: st=%d, cnt=%#x, dev=%#x\n",
          iosb.status, iosb.count, iosb.dev_dep_data);
#endif

  newmode[0] = oldmode[0];
  newmode[1] = oldmode[1] | TT$M_EIGHTBIT;
  newmode[1] &= ~(TT$M_TTSYNC|TT$M_HOSTSYNC);

  /* FIXME: this assumes we're doing IO$_SETCHAR */
  newmode[2] = oldmode[2] | TT2$M_PASTHRU;

  status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
    newmode, sizeof(newmode), 0, 0, 0, 0);
  if (status != SS$_NORMAL
  || iosb.status != SS$_NORMAL)
    give_up(status);

  max_lines = (newmode[1]>>24);
  min_cols  = newmode[0]>>16;
  tty_speed = lookup_speed(iosb.count & 0xff);

  if (LOG_ENABLED) {
    fprintf(log_fp, "TTY modes %#x, %#x, %#x\n", oldmode[0], oldmode[1], oldmode[2]);
    fprintf(log_fp, "iosb.count = %#x\n", iosb.count);
    fprintf(log_fp, "iosb.dev_dep_data = %#x\n", iosb.dev_dep_data);
        fprintf(log_fp, "TTY speed = %d\n", tty_speed);
  }
}

/*
 * Restore state to "normal", for menu-prompting
 */
void
restore_ttymodes(void)
{
  outflush();
  in_flags = 0;
  cr_flag  = TRUE;
}

void
close_tty(void)
{
  int status;
  QIO_SB iosb;

  status = sys$qiow(EFN, iochan, IO$_SETMODE, &iosb, 0, 0,
            oldmode, sizeof(oldmode), 0, 0, 0, 0);
  if (status == SS$_IVCHAN)
    return; /* already closed it */
  (void) sys$dassgn(iochan);
}

void
set_tty_crmod(int enabled)
{
  cr_flag = enabled;
}

void
set_tty_echo(int enabled)
{
  if (enabled)
    in_flags &= ~IO$M_NOECHO;
  else
    in_flags |= IO$M_NOECHO;
}

void
set_tty_raw(int enabled)
{
  if (enabled)
    in_flags |= IO$M_NOFILTR;
  else
    in_flags &= ~IO$M_NOFILTR;
}
