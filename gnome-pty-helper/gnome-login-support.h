#ifndef _GNOME_LOGIN_SUPPORT_H
#define _GNOME_LOGIN_SUPPORT_H

#ifdef HAVE_OPENPTY
#if defined(HAVE_PTY_H)
#    include <pty.h>
#elif defined(HAVE_UTIL_H) /* OpenBSD */
#    include <util.h>
#elif defined(HAVE_LIBUTIL_H) /* FreeBSD */
#    include <libutil.h>
#elif defined(HAVE_LIBUTIL) /* BSDI has libutil, but no libutil.h */
/* Avoid pulling in all the include files for no need */
struct termios;
struct winsize;
struct utmp;
	    
void login (struct utmp *ut);
int  login_tty (int fd);
int  logout (char *line);
void logwtmp (const char *line, const char *name, const char *host);
int  openpty (int *amaster, int *aslave, char *name, struct termios *termp, struct winsize *winp);
int  forkpty (int *amaster, char *name, struct termios *termp, struct winsize *winp);
#endif
#else
int openpty (int *master_fd, int *slavefd, char *name, struct termios *termp, struct winsize *winp);
pid_t forkpty (int *master_fd, char *name, struct termios *termp, struct winsize *winp);
#endif

#ifndef HAVE_LOGIN_TTY
int login_tty (int fd);
#elif defined(HAVE_UTMP_H)
/* Get the prototype from utmp.h */
#include <utmp.h>
#endif

int n_read (int fd, void *buffer, int size);
int n_write (int fd, const void *buffer, int size);

#endif /* _GNOME_LOGIN_SUPPORT_H */
