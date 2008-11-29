/*
 * utmp/wtmp file updating
 *
 * Authors:
 *    Miguel de Icaza (miguel@gnu.org).
 *    Timur I. Bakeyev (timur@gnu.org).
 *
 * FIXME: Do we want to register the PID of the process running *under* the
 * subshell or the PID of the parent process? (we are doing the latter now).
 *
 * FIXME: Solaris (utmpx) stuff need to be checked.
 */

#include <config.h>
#include <sys/types.h>
#include <sys/file.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>

#if defined(TIME_WITH_SYS_TIME)
#    include <sys/time.h>
#include <time.h>
#else
#  if defined(HAVE_SYS_TIME_H)
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
#endif

#if defined(HAVE_LASTLOG_H)
#    include <lastlog.h>
#endif

#if defined(HAVE_PATHS_H)
#    include <paths.h>
#endif

#if defined(HAVE_UTMP_H)
#    include <utmp.h>
#endif

#if defined(HAVE_UTMPX_H)
#    include <utmpx.h>
#endif

#if defined(HAVE_TTYENT_H)
#    include <ttyent.h>
#endif

#include "gnome-pty.h"
#include "gnome-login-support.h"



#if !defined(UTMP_OUTPUT_FILENAME)
#    if defined(UTMP_FILE)
#        define UTMP_OUTPUT_FILENAME UTMP_FILE
#    elif defined(_PATH_UTMP) /* BSD systems */
#        define UTMP_OUTPUT_FILENAME _PATH_UTMP
#    else
#        define UTMP_OUTPUT_FILENAME "/etc/utmp"
#    endif
#endif

#if !defined(WTMP_OUTPUT_FILENAME)
#    if defined(WTMPX_FILE)
#        define WTMP_OUTPUT_FILENAME WTMPX_FILE
#    elif defined(_PATH_WTMPX)
#        define WTMP_OUTPUT_FILENAME _PATH_WTMPX
#    elif defined(WTMPX_FILENAME)
#        define WTMP_OUTPUT_FILENAME WTMPX_FILENAME
#    elif defined(WTMP_FILE)
#        define WTMP_OUTPUT_FILENAME WTMP_FILE
#    elif defined(_PATH_WTMP) /* BSD systems */
#        define WTMP_OUTPUT_FILENAME _PATH_WTMP
#    else
#        define WTMP_OUTPUT_FILENAME "/etc/wtmp"
#    endif
#endif

#if defined(_PATH_LASTLOG) /* BSD systems */
#    define LASTLOG_OUTPUT_FILE	_PATH_LASTLOG
#else
#    define LASTLOG_OUTPUT_FILE	"/var/log/lastlog"
#endif

#if defined(HAVE_UPDWTMPX)
#include <utmpx.h>
#define update_wtmp updwtmpx
#elif defined(HAVE_UPDWTMP)
#define update_wtmp updwtmp
#else /* !HAVE_UPDWTMPX && !HAVE_UPDWTMP */
static void
update_wtmp (char *file, UTMP *putmp)
{
	int fd, times = 3;
#if defined(HAVE_FCNTL)
	struct flock lck;

	lck.l_whence = SEEK_END;
	lck.l_len    = 0;
	lck.l_start  = 0;
	lck.l_type   = F_WRLCK;
#endif

	if ((fd = open (file, O_WRONLY|O_APPEND, 0)) < 0)
		return;

#if defined (HAVE_FCNTL)
	while (times--)
	  if ((fcntl (fd, F_SETLK, &lck) < 0))
	    {
	      if (errno != EAGAIN && errno != EACCES) {
		close (fd);
		return;
	      }
	      sleep (1); /*?!*/
	    } else
	      break;
#elif defined(HAVE_FLOCK)
	while (times--)
	  if (flock (fd, LOCK_EX | LOCK_NB) < 0)
	    {
	      if (errno != EWOULDBLOCK)
		{
		  close (fd);
		  return;
		}
	      sleep (1); /*?!*/
	    } else
	      break;
#endif /* HAVE_FLOCK */

	lseek (fd, 0, SEEK_END);
	write (fd, putmp, sizeof(UTMP));

	/* unlock the file */
#if defined(HAVE_FCNTL)
	lck.l_type = F_UNLCK;
	fcntl (fd, F_SETLK, &lck);
#elif defined(HAVE_FLOCK)
	flock (fd, LOCK_UN);
#endif
	close (fd);
}
#endif /* !HAVE_GETUTMPX */


#if defined(HAVE_GETUTMPX)
static void
update_utmp (UTMP *ut)
{
	setutxent();
	pututxline (ut);
	endutxent();
}
#elif defined(HAVE_GETUTENT)
static void
update_utmp (UTMP *ut)
{
	setutent();
	pututline (ut);
	endutent();
}
#elif defined(HAVE_GETTTYENT)
/* This variant is sutable for most BSD */
static void
update_utmp (UTMP *ut)
{
	struct ttyent *ty;
	int fd, pos = 0;

	if ((fd = open (UTMP_OUTPUT_FILENAME, O_RDWR|O_CREAT, 0644)) < 0)
		return;

	setttyent ();
	while ((ty = getttyent ()) != NULL)
	{
		++pos;
		if (strncmp (ty->ty_name, ut->ut_line, sizeof (ut->ut_line)) == 0)
		{
			lseek (fd, (off_t)(pos * sizeof(UTMP)), SEEK_SET);
			write(fd, ut, sizeof(UTMP));
		}
	}
	endttyent ();

	close(fd);
}
#else
#define update_utmp(ut)
#endif

#if !defined(HAVE_LASTLOG)
#define update_lastlog(login_name, ut)
#else
static void
update_lastlog(char* login_name, UTMP *ut)
{
    struct passwd *pwd;
    struct lastlog ll;
    int fd;

    if ((fd = open(LASTLOG_OUTPUT_FILE, O_WRONLY, 0)) < 0)
	return;

    if ((pwd=getpwnam(login_name)) == NULL)
	return;

    memset (&ll, 0, sizeof(ll));

    lseek (fd, (off_t)pwd->pw_uid * sizeof (ll), SEEK_SET);

    time (&ll.ll_time);

    strncpy (ll.ll_line, ut->ut_line, sizeof (ll.ll_line));

#if defined(HAVE_UT_UT_HOST)
    if (ut->ut_host)
	strncpy (ll.ll_host, ut->ut_host, sizeof (ll.ll_host));
#endif

    write (fd, (void *)&ll, sizeof (ll));
    close (fd);
}
#endif /* HAVE_LASTLOG */

void
write_logout_record (char *login_name, void *data, int utmp, int wtmp)
{
	UTMP put, *ut = data;
	struct timeval tv;

	memset (&put, 0, sizeof(UTMP));

#if defined(HAVE_UT_UT_TYPE)
	put.ut_type = DEAD_PROCESS;
#endif
#if defined(HAVE_UT_UT_ID)
	strncpy (put.ut_id, ut->ut_id, sizeof (put.ut_id));
#endif

	strncpy (put.ut_line, ut->ut_line, sizeof (put.ut_line));

#if defined(HAVE_UT_UT_TV)
	gettimeofday(&tv, NULL);
	put.ut_tv.tv_sec = tv.tv_sec;
	put.ut_tv.tv_usec = tv.tv_usec;
#elif defined(HAVE_UT_UT_TIME)
	time (&put.ut_time);
#endif

#if defined(HAVE_UT_UT_NAME)
	strncpy (put.ut_name, login_name, sizeof (put.ut_name));
#elif defined(HAVE_UT_UT_USER)
	strncpy (put.ut_user, login_name, sizeof (put.ut_user));
#endif

	if (utmp)
		update_utmp (&put);

	if (wtmp)
		update_wtmp (WTMP_OUTPUT_FILENAME, &put);

	free (ut);
}

void *
write_login_record (char *login_name, char *display_name,
		    char *term_name, int utmp, int wtmp, int lastlog)
{
	UTMP *ut;
	char *pty = term_name;
	struct timeval tv;

	if ((ut=(UTMP *) malloc (sizeof (UTMP))) == NULL)
		return NULL;

	memset (ut, 0, sizeof (UTMP));

#if defined(HAVE_UT_UT_NAME)
	strncpy (ut->ut_name, login_name, sizeof (ut->ut_name));
#elif defined(HAVE_UT_UT_USER)
	strncpy (ut->ut_user, login_name, sizeof (ut->ut_user));
#endif

	/* This shouldn't happen */
	if (strncmp (pty, "/dev/", 5) == 0)
	    pty += 5;

#if defined(HAVE_STRRCHR)
	{
		char *p;

		if (strncmp (pty, "pts", 3) &&
		    (p = strrchr (pty, '/')) != NULL)
			pty = p + 1;
	}
#endif

#if defined(HAVE_UT_UT_ID)
	/* Just a safe-guard */
	ut->ut_id [0] = '\0';

	/* BSD-like terminal name */
	if (strncmp (pty, "pts", 3) == 0 ||
	    strncmp (pty, "pty", 3) == 0 ||
	    strncmp (pty, "tty", 3) == 0) {
		strncpy (ut->ut_id, pty+3, sizeof (ut->ut_id));
	} else {
		unsigned int num;
		char buf[10];
		/* Try to get device number and convert it to gnome-terminal # */
		if (sscanf (pty, "%*[^0-9a-f]%x", &num) == 1) {
			sprintf (buf, "gt%2.2x", num);
			strncpy (ut->ut_id, buf, sizeof (ut->ut_id));
		}
	}
#endif

	/* For utmpx ut_line should be null terminated */
	/* We do that for both cases to be sure */
	strncpy (ut->ut_line, pty, sizeof (ut->ut_line));
	ut->ut_line[sizeof (ut->ut_line)-1] = '\0';

	/* We want parent's pid, not our own */
#if defined(HAVE_UT_UT_PID)
	ut->ut_pid  = getppid ();
#endif

#if defined(HAVE_UT_UT_TYPE)
	ut->ut_type = USER_PROCESS;
#endif
	/* If structure has ut_tv it doesn't need ut_time */
#if defined(HAVE_UT_UT_TV)
	gettimeofday(&tv, NULL);
	ut->ut_tv.tv_sec = tv.tv_sec;
	ut->ut_tv.tv_usec = tv.tv_usec;
#elif defined(HAVE_UT_UT_TIME)
	time (&ut->ut_time);
#endif
	/* ut_ host supposed to be null terminated or len should */
	/* be specifid in additional field. We do both :)  */
#if defined(HAVE_UT_UT_HOST)
	strncpy (ut->ut_host, display_name, sizeof (ut->ut_host));
	ut->ut_host [sizeof (ut->ut_host)-1] = '\0';
#    if defined(HAVE_UT_UT_SYSLEN)
	ut->ut_syslen = strlen (ut->ut_host);
#    endif
#endif
	if (utmp)
		update_utmp (ut);

	if (wtmp)
		update_wtmp (WTMP_OUTPUT_FILENAME, ut);

	if (lastlog)
		update_lastlog(login_name, ut);

	return ut;
}

void *
update_dbs (int utmp, int wtmp, int lastlog, char *login_name, char *display_name, char *term_name)
{
	return write_login_record (login_name, display_name,
				   term_name, utmp, wtmp, lastlog);
}
