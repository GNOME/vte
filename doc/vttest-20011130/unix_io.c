/* $Id$ */

#include <stdarg.h>
#include <vttest.h>
#include <esc.h>

static void
give_up(int sig)
{
  if (LOG_ENABLED) {
    fprintf(log_fp, "** killing program due to timeout\n");
    fflush(log_fp);
  }
  kill(getpid(), (int) SIGTERM);
}

static int last_char;

void
reset_inchar(void)
{
  last_char = -1;
}

/*
 * Wait until a character is typed on the terminal then read it, without
 * waiting for CR.
 */
char
inchar(void)
{
  int lval; char ch;

  fflush(stdout);
  lval = last_char;
  brkrd = FALSE;
  reading = TRUE;
#if HAVE_ALARM
  signal(SIGALRM, give_up);
  alarm(60);	/* timeout after 1 minute, in case user's keyboard is hung */
#endif
  read(0,&ch,1);
#if HAVE_ALARM
  alarm(0);
#endif
  reading = FALSE;
#ifdef DEBUG
  {
    FILE *fp = fopen("ttymodes.log","a");
    if (fp != 0) {
      fprintf(fp, "%d>%#x\n", brkrd, ch);
      fclose(fp);
    }
  }
#endif
  if (brkrd)
    last_char = 0177;
  else
    last_char = ch;
  if ((last_char==0177) && (last_char==lval))
    give_up(SIGTERM);
  return(last_char);
}

/*
 * Get an unfinished string from the terminal:  wait until a character is typed
 * on the terminal, then read it, and all other available characters.  Return a
 * pointer to that string.
 */
char *
instr(void)
{
#if USE_FIONREAD
  long l1;
#endif
  int i;
  static char result[1024];

  i = 0;
  result[i++] = inchar();
/* Wait 0.1 seconds (1 second in vanilla UNIX) */
  zleep(100);
  fflush(stdout);
#if HAVE_RDCHK
  while(rdchk(0)) {
    read(0,result+i,1);
    if (i++ == sizeof(result)-2) break;
  }
#else
#if USE_FIONREAD
  while(ioctl(0,FIONREAD,&l1), l1 > 0L) {
    while(l1-- > 0L) {
      read(0,result+i,1);
      if (i++ == sizeof(result)-2) goto out1;
    }
  }
out1:
#else
  while(read(2,result+i,1) == 1)
    if (i++ == sizeof(result)-2) break;
#endif
#endif
  result[i] = '\0';

  if (LOG_ENABLED) {
    fputs("Reply: ", log_fp);
    put_string(log_fp, result);
    fputs("\n", log_fp);
  }

  return(result);
}

char *
get_reply(void)
{
  return instr(); /* cf: vms_io.c */
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
  int val;

#if HAVE_RDCHK
  while(rdchk(0))
    read(0,&val,1);
#else
#if USE_FIONREAD
  long l1;
  ioctl (0, FIONREAD, &l1);
  while(l1-- > 0L)
    read(0,&val,1);
#else
  while(read(2,&val,1) > 0)
    ;
#endif
#endif
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
  char ch;
  fflush(stdout);
  brkrd = FALSE;
  reading = TRUE;
  do { read(0,&ch,1); } while(ch != '\n' && !brkrd);
  if (brkrd)
    give_up(SIGTERM);
  reading = FALSE;
}

/*
 * Sleep and do nothing (don't waste CPU) for t milliseconds
 */
void
zleep(int t)
{
#if HAVE_USLEEP
  unsigned msecs = t * 1000;
  usleep(msecs);
#else
  unsigned secs = t / 1000;
  if (secs == 0) secs = 1;
  sleep(secs);	/* UNIX can only sleep whole seconds */
#endif
}
