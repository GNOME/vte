/* $Id$ */

#ifndef VTTEST_H
#define VTTEST_H 1

#define VERSION "1.7b 1985-04-19"
#include <patchlev.h>

/* Choose one of these */

#ifdef HAVE_CONFIG_H
#include <config.h>
#define UNIX
#else

/* Assume ANSI and (minimal) Posix */
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define RETSIGTYPE void

#endif

#define SIG_ARGS int sig	/* FIXME: configure-test */

#include <stdio.h>
#include <stdarg.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif

#include <ctype.h>

#if HAVE_TERMIOS_H && HAVE_TCGETATTR
#  define USE_POSIX_TERMIOS 1
#else
#  if HAVE_TERMIO_H
#    define USE_TERMIO 1
#  else
#    if HAVE_SGTTY_H
#      define USE_SGTTY 1
#      define USE_FIONREAD 1
#    else
#      ifdef VMS
         /* FIXME */
#      else
         please fix me
#      endif
#    endif
#  endif
#endif

#include <signal.h>
#include <setjmp.h>

#if USE_POSIX_TERMIOS
#  include <termios.h>
#  define TTY struct termios
#else
#  if USE_TERMIO
#    include <termio.h>
/*#    define TCSANOW TCSETA */
/*#    define TCSADRAIN TCSETAW */
/*#    define TCSAFLUSH TCSETAF */
#    define TTY struct termio
#    define tcsetattr(fd, cmd, arg) ioctl(fd, cmd, arg)
#    define tcgetattr(fd, arg) ioctl(fd, TCGETA, arg)
#    define VDISABLE (unsigned char)(-1)
#  else
#    if USE_SGTTY
#      include <sgtty.h>
#      define TTY struct sgttyb 
#    endif
#  endif
#endif

#if HAVE_SYS_FILIO_H
#  include <sys/filio.h>	/* FIONREAD */
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if !defined(sun) || !defined(NL0)
#  if HAVE_IOCTL_H
#   include <ioctl.h>
#  else
#   if HAVE_SYS_IOCTL_H
#    include <sys/ioctl.h>
#   endif
#  endif
#endif

/*FIXME: really want 'extern' for errno*/
#include <errno.h>

#define LOG_ENABLED ((log_fp != 0) && !log_disabled)

extern FILE *log_fp;
extern int brkrd;
extern int input_8bits;
extern int log_disabled;
extern int max_cols;
extern int max_lines;
extern int min_cols;
extern int output_8bits;
extern int reading;
extern int tty_speed;
extern int use_padding;
extern jmp_buf intrenv;

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#define SHOW_SUCCESS "ok"
#define SHOW_FAILURE "failed"

#define TABLESIZE(table) (sizeof(table)/sizeof(table[0]))

#define DEFAULT_SPEED 9600

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(p) /* nothing */
#endif

#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))

/* my SunOS 4.1.x doesn't have prototyped headers */
#if defined(__GNUC__) && defined(sun) && !defined(__SVR4)
extern void perror(const char *s);
extern int _flsbuf(int c, FILE *s);
extern int fclose(FILE *s);
extern int fflush(FILE *s);
extern int fprintf(FILE *s, const char *fmt, ...);
extern int fgetc(FILE *s);
extern int fputc(int c, FILE *s);
extern int fputs(char *p, FILE *s);
extern int ioctl(int fd, unsigned long mask, void *p);
extern int printf(const char *fmt, ...);
extern int scanf(const char *fmt, ...);
extern int sscanf(const char *src, const char *fmt, ...);
extern long strtol(const char *src, char **dst, int base);
#endif

#define MENU_ARGS    char *   the_title
#define PASS_ARGS /* char * */the_title

typedef struct {
  char *description;
  int (*dispatch)(MENU_ARGS);
} MENU;

typedef struct {
  int cur_level;
  int input_8bits;
  int output_8bits;
} VTLEVEL;

#define MENU_NOHOLD 0
#define MENU_HOLD   1
#define MENU_MERGE  2

#define TITLE_LINE  3

/* main.c */
extern RETSIGTYPE onbrk(SIG_ARGS);
extern RETSIGTYPE onterm(SIG_ARGS);
extern char *skip_csi(char *input);
extern char *skip_dcs(char *input);
extern char *skip_digits(char *src);
extern char *skip_prefix(char *prefix, char *input);
extern char *skip_ss3(char *input);
extern int any_DSR(MENU_ARGS, char *text, void (*explain)(char *report));
extern int bug_a(MENU_ARGS);
extern int bug_b(MENU_ARGS);
extern int bug_c(MENU_ARGS);
extern int bug_d(MENU_ARGS);
extern int bug_e(MENU_ARGS);
extern int bug_f(MENU_ARGS);
extern int bug_l(MENU_ARGS);
extern int bug_s(MENU_ARGS);
extern int bug_w(MENU_ARGS);
extern int get_level(void);
extern int main(int argc, char *argv[]);
extern int menu(MENU *table);
extern int not_impl(MENU_ARGS);
extern int parse_decrqss(char *report, char *func);
extern int set_level(int level);
extern int scan_any(char *str, int *pos, int toc);
extern int scanto(char *str, int *pos, int toc);
extern int setup_terminal(MENU_ARGS);
extern int strip_suffix(char *src, char *suffix);
extern int strip_terminator(char *src);
extern int terminal_id(void);
extern int tst_DECSCA(MENU_ARGS);
extern int tst_DECSTR(MENU_ARGS);
extern int tst_DECTCEM(MENU_ARGS);
extern int tst_DECUDK(MENU_ARGS);
extern int tst_DSR_keyboard(MENU_ARGS);
extern int tst_DSR_locator(MENU_ARGS);
extern int tst_DSR_printer(MENU_ARGS);
extern int tst_DSR_userkeys(MENU_ARGS);
extern int tst_ECH(MENU_ARGS);
extern int tst_S8C1T(MENU_ARGS);
extern int tst_SD(MENU_ARGS);
extern int tst_SRM(MENU_ARGS);
extern int tst_SU(MENU_ARGS);
extern int tst_bugs(MENU_ARGS);
extern int tst_characters(MENU_ARGS);
extern int tst_colors(MENU_ARGS);
extern int tst_doublesize(MENU_ARGS);
extern int tst_insdel(MENU_ARGS);
extern int tst_keyboard(MENU_ARGS);
extern int tst_keyboard_layout(char *scs_params);
extern int tst_mouse(MENU_ARGS);
extern int tst_movements(MENU_ARGS);
extern int tst_nonvt100(MENU_ARGS);
extern int tst_printing(MENU_ARGS);
extern int tst_reports(MENU_ARGS);
extern int tst_rst(MENU_ARGS);
extern int tst_screen(MENU_ARGS);
extern int tst_setup(MENU_ARGS);
extern int tst_softchars(MENU_ARGS);
extern int tst_statusline(MENU_ARGS);
extern int tst_vt220(MENU_ARGS);
extern int tst_vt420(MENU_ARGS);
extern int tst_vt52(MENU_ARGS);
extern int tst_xterm(MENU_ARGS);
extern void bye(void);
extern void chrprint(char *s);
extern void default_level(void);
extern void do_scrolling(void);
extern void enable_logging(void);
extern void initterminal(int pn);
extern void reset_level(void);
extern void restore_level(VTLEVEL *save);
extern void save_level(VTLEVEL *save);
extern void scs_graphics(void);
extern void scs_normal(void);
extern void setup_softchars(char *filename);
extern void show_result(const char *fmt, ...);
extern void title(int offset);
extern void vt_clear(int code);
extern void vt_el(int code);
extern void vt_hilite(int flag);
extern void vt_move(int row, int col);

#endif /* VTTEST_H */
