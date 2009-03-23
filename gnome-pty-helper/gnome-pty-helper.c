/*
 * gnome-pty.c:  Helper setuid application used to open a pseudo-
 * terminal, set the permissions, ownership and record user login
 * information
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 *
 * Parent application talks to us via a couple of sockets that are strategically
 * placed on file descriptors 0 and 1 (STDIN_FILENO and STDOUT_FILENO).
 *
 * We use the STDIN_FILENO to read and write the protocol information and we use
 * the STDOUT_FILENO to pass the file descriptors (we need two different file
 * descriptors as using a socket for both data transfers and file descriptor
 * passing crashes some BSD kernels according to Theo de Raadt)
 *
 * A sample protocol is used:
 *
 * OPEN_PTY             => 1 <tag> <master-pty-fd> <slave-pty-fd>
 *                      => 0
 *
 * CLOSE_PTY  <tag>     => void
 *
 * <tag> is a pointer.  If tag is NULL, then the ptys were not allocated.
 * ptys are passed using file descriptor passing on the stdin file descriptor
 *
 * We use as little as possible external libraries.
 */
#include <config.h>

/* Use this to pull SCM_RIGHTS definition on IRIX */
#if defined(irix) || defined (__irix__) || defined(sgi) || defined (__sgi__)
#    define _XOPEN_SOURCE 1
extern char *strdup(const char *);
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <termios.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <utmp.h>
#include <grp.h>
#include "gnome-pty.h"
#include "gnome-login-support.h"

/* For PATH_MAX on BSD-like systems. */
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif

static struct passwd *pwent;
static char login_name_buffer [48];
static char *login_name, *display_name;

struct pty_info {
	char   *login_name;
	struct pty_info *next;
	char   *line;
	void   *data;
	char   utmp, wtmp, lastlog;
};

typedef struct pty_info pty_info;

static pty_info *pty_list;

#ifdef HAVE_SENDMSG
#include <sys/socket.h>
#include <sys/uio.h>

#ifdef HAVE_SYS_UN_H /* Linux libc5 */
#include <sys/un.h>
#endif

#ifndef CMSG_DATA /* Linux libc5 */
/* Ancillary data object manipulation macros.  */
#if !defined __STRICT_ANSI__ && defined __GNUC__ && __GNUC__ >= 2
# define CMSG_DATA(cmsg) ((cmsg)->cmsg_data)
#else
# define CMSG_DATA(cmsg) ((unsigned char *) ((struct cmsghdr *) (cmsg) + 1))
#endif
#endif /* CMSG_DATA */

/* Solaris doesn't define these */
#ifndef CMSG_ALIGN
#define CMSG_ALIGN(len) (((len) + sizeof (size_t) - 1) & (size_t) ~(sizeof (size_t) - 1))
#endif
#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN (len) + CMSG_ALIGN (sizeof (struct cmsghdr)))
#endif
#ifndef CMSG_LEN
#define CMSG_LEN(len) (CMSG_ALIGN (sizeof (struct cmsghdr)) + (len))
#endif

static int
pass_fd (int client_fd, int fd)
{
        struct iovec  iov[1];
        struct msghdr msg;
        char          buf [1];
        char    cmsgbuf[CMSG_SPACE(sizeof(int))];
        struct  cmsghdr *cmptr;

	iov [0].iov_base = buf;
	iov [0].iov_len  = 1;

	msg.msg_iov        = iov;
	msg.msg_iovlen     = 1;
	msg.msg_name       = NULL;
	msg.msg_namelen    = 0;
	msg.msg_control    = (caddr_t) cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);

        cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type  = SCM_RIGHTS;
	cmptr->cmsg_len   = CMSG_LEN(sizeof(int));
	*(int *)CMSG_DATA (cmptr) = fd;

	if (sendmsg (client_fd, &msg, 0) != 1)
		return -1;

	return 0;
}

#elif defined(__sgi) && !defined(HAVE_SENDMSG)

/*
 * IRIX 6.2 is like 4.3BSD; it will not have HAVE_SENDMSG set,
 * because msghdr used msg_accrights and msg_accrightslen rather
 * than the newer msg_control and msg_controllen fields configure
 * checks.  The SVR4 code below doesn't work because pipe()
 * semantics are controlled by the svr3pipe systune variable,
 * which defaults to uni-directional pipes.  Also sending
 * file descriptors through pipes isn't implemented.
 */

#include <sys/socket.h>
#include <sys/uio.h>

static int
pass_fd (int client_fd, int fd)
{
  struct iovec  iov[1];
  struct msghdr msg;
  char          buf [1];

  iov [0].iov_base = buf;
  iov [0].iov_len  = 1;

  msg.msg_iov        = iov;
  msg.msg_iovlen     = 1;
  msg.msg_name       = NULL;
  msg.msg_namelen    = 0;
  msg.msg_accrights    = (caddr_t) &fd;
  msg.msg_accrightslen = sizeof(fd);

  if (sendmsg (client_fd, &msg, 0) != 1)
    return -1;

  return 0;
}

#else
#include <stropts.h>
#ifdef I_SENDFD

int
pass_fd (int client_fd, int fd)
{
	if (ioctl (client_fd, I_SENDFD, fd) < 0)
		return -1;
	return 0;
}
#endif
#endif

static void
pty_free (pty_info *pi)
{
	free (pi);
}

static void
pty_remove (pty_info *pi)
{
	pty_info *l, *last;

	last = (void *) 0;

	for (l = pty_list; l; l = l->next) {
		if (l == pi) {
			if (last == (void *) 0)
				pty_list = pi->next;
			else
				last->next = pi->next;
			free (pi->line);
			free (pi->login_name);
			pty_free (pi);
			return;
		}
		last = l;
	}

	exit (1);
}

static void
shutdown_pty (pty_info *pi)
{
	if (pi->utmp || pi->wtmp || pi->lastlog)
		if (pi->data)
			write_logout_record (pi->login_name, pi->data, pi->utmp, pi->wtmp);

	pty_remove (pi);
}

static void
shutdown_helper (void)
{
	pty_info *pi;

	for (pi = pty_list; pi; pi = pty_list)
		shutdown_pty (pi);
}

static pty_info *
pty_add (int utmp, int wtmp, int lastlog, char *line, char *login_name)
{
	pty_info *pi = malloc (sizeof (pty_info));

	if (pi == NULL) {
		shutdown_helper ();
		exit (1);
	}

	memset (pi, 0, sizeof (pty_info));

	if (strncmp (line, "/dev/", 5))
		pi->line = strdup (line);
	else
		pi->line = strdup (line+5);

	if (pi->line == NULL) {
		shutdown_helper ();
		exit (1);
	}

	pi->next = pty_list;
	pi->utmp = utmp;
	pi->wtmp = wtmp;
	pi->lastlog = lastlog;
	pi->login_name = strdup (login_name);

	pty_list = pi;

	return pi;
}

static struct termios*
init_term_with_defaults(struct termios* term)
{
	/*
	 *	openpty assumes POSIX termios so this should be portable.
	 *	Don't change this to a structure init - POSIX doesn't say
	 *	anything about field order.
	 */
	memset(term, 0, sizeof(struct termios));

	term->c_iflag = 0
#ifdef BRKINT
	  | BRKINT
#endif
#ifdef ICRNL
	  | ICRNL
#endif
#ifdef IMAXBEL
	  | IMAXBEL
#endif
#ifdef IXON
	  | IXON
#endif
#ifdef IXANY
	  | IXANY
#endif
	  ;
	term->c_oflag = 0
#ifdef OPOST
	  | OPOST
#endif
#ifdef ONLCR
	  | ONLCR
#endif
#ifdef NL0
	  | NL0
#endif
#ifdef CR0
	  | CR0
#endif
#ifdef TAB0
	  | TAB0
#endif
#ifdef BS0
	  | BS0
#endif
#ifdef VT0
	  | VT0
#endif
#ifdef FF0
	  | FF0
#endif
	  ;
	term->c_cflag = 0
#ifdef CREAD
	  | CREAD
#endif
#ifdef CS8
	  | CS8
#endif
#ifdef HUPCL
	  | HUPCL
#endif
	  ;
#ifdef EXTB
	cfsetispeed(term, EXTB);
	cfsetospeed(term, EXTB);
#else
#   ifdef B38400
        cfsetispeed(term, B38400);
        cfsetospeed(term, B38400);
#   else
#       ifdef B9600
        cfsetispeed(term, B9600);
        cfsetospeed(term, B9600);
#       endif
#   endif
#endif /* EXTB */

	term->c_lflag = 0
#ifdef ECHO
	  | ECHO
#endif
#ifdef ICANON
	  | ICANON
#endif
#ifdef ISIG
	  | ISIG
#endif
#ifdef IEXTEN
	  | IEXTEN
#endif
#ifdef ECHOE
	  | ECHOE
#endif
#ifdef ECHOKE
	  | ECHOKE
#endif
#ifdef ECHOK
	  | ECHOK
#endif
#ifdef ECHOCTL
	  | ECHOCTL
#endif
	  ;

#ifdef N_TTY
	/* should really be a check for c_line, but maybe this is good enough */
	term->c_line = N_TTY;
#endif

	/* These two may overlap so set them first */
	/* That setup means, that read() will be blocked until  */
	/* at least 1 symbol will be read.                      */
	term->c_cc[VMIN]  = 1;
	term->c_cc[VTIME] = 0;

	/*
	 * Now set the characters. This is of course a religious matter
	 * but we use the defaults, with erase bound to the key gnome-terminal
	 * maps.
	 *
	 * These are the ones set by "stty sane".
	 */

	term->c_cc[VINTR]  = 'C'-64;
	term->c_cc[VQUIT]  = '\\'-64;
	term->c_cc[VERASE] = 127;
	term->c_cc[VKILL]  = 'U'-64;
	term->c_cc[VEOF]   = 'D'-64;
#ifdef VSWTC
	term->c_cc[VSWTC]  = 255;
#endif
	term->c_cc[VSTART] = 'Q'-64;
	term->c_cc[VSTOP]  = 'S'-64;
	term->c_cc[VSUSP]  = 'Z'-64;
	term->c_cc[VEOL]   = 255;

	/*
	 *	Extended stuff.
	 */

#ifdef VREPRINT
	term->c_cc[VREPRINT] = 'R'-64;
#endif
#ifdef VSTATUS
	term->c_cc[VSTATUS]  = 'T'-64;
#endif
#ifdef VDISCARD
	term->c_cc[VDISCARD] = 'O'-64;
#endif
#ifdef VWERASE
	term->c_cc[VWERASE]  = 'W'-64;
#endif
#ifdef VLNEXT
	term->c_cc[VLNEXT]   = 'V'-64;
#endif
#ifdef VDSUSP
	term->c_cc[VDSUSP]   = 'Y'-64;
#endif
#ifdef VEOL2
	term->c_cc[VEOL2]    = 255;
#endif
    return term;
}

static int
open_ptys (int utmp, int wtmp, int lastlog)
{
	const char *term_name;
	int status, master_pty, slave_pty;
	pty_info *p;
	int result;
	uid_t savedUid;
	gid_t savedGid;
	struct group *group_info;
	struct termios term;

	/* Initialize term */
	init_term_with_defaults(&term);

	/* root privileges */
	savedUid = geteuid();
	savedGid = getegid();

	/* drop privileges to the user level */
#if defined(HAVE_SETEUID)
	seteuid (pwent->pw_uid);
	setegid (pwent->pw_gid);
#elif defined(HAVE_SETREUID)
	setreuid (savedUid, pwent->pw_uid);
	setregid (savedGid, pwent->pw_gid);
#else
#error "No means to drop privileges! Huge security risk! Won't compile."
#endif
	/* Open pty with privileges of the user */
	status = openpty (&master_pty, &slave_pty, NULL, &term, NULL);

	/* Restore saved privileges to root */
#ifdef HAVE_SETEUID
	seteuid (savedUid);
	setegid (savedGid);
#elif defined(HAVE_SETREUID)
	setreuid (pwent->pw_uid, savedUid);
	setregid (pwent->pw_gid, savedGid);
#else
#error "No means to raise privileges! Huge security risk! Won't compile."
#endif
	/* openpty() failed, reject request */
	if (status == -1 || (term_name = ttyname(slave_pty)) == NULL) {
		result = 0;
		n_write (STDIN_FILENO, &result, sizeof (result));
		return 0;
	}

	/* a bit tricky, we re-do the part of the openpty()  */
	/* that required root privileges, and, hence, failed */
	group_info = getgrnam ("tty");
	fchown (slave_pty, getuid (), group_info ? group_info->gr_gid : -1);
	fchmod (slave_pty, S_IRUSR | S_IWUSR | S_IWGRP);
	/* It's too late to call revoke at this time... */
	/* revoke(term_name); */

	/* add pty to the list of allocated by us */
	p = pty_add (utmp, wtmp, lastlog, term_name, login_name);
	result = 1;

	if (n_write (STDIN_FILENO, &result, sizeof (result)) != sizeof (result) ||
	    n_write (STDIN_FILENO, &p, sizeof (p)) != sizeof (p) ||
	    pass_fd (STDOUT_FILENO, master_pty)  == -1 ||
	    pass_fd (STDOUT_FILENO, slave_pty)   == -1) {
		exit (0);
	}

	if (utmp || wtmp || lastlog) {
		p->data = write_login_record (login_name, display_name,
					      term_name, utmp, wtmp, lastlog);
	}

	close (master_pty);
	close (slave_pty);

	return 1;
}

static void
close_pty_pair (void *tag)
{
	pty_info *pi;

	for (pi = pty_list; pi; pi = pi->next) {
		if (tag == pi) {
			shutdown_pty (pi);
			break;
		}
	}
}

#define MB (1024*1024)

struct {
	int limit;
	int value;
} sensible_limits [] = {
	{ RLIMIT_CPU,    120 },
	{ RLIMIT_FSIZE,  1 * MB },
	{ RLIMIT_DATA,   1 * MB },
	{ RLIMIT_STACK,  1 * MB },
#ifdef RLIMIT_AS
	{ RLIMIT_AS,     1 * MB },
#endif
	{ RLIMIT_NOFILE, 10 },
#ifdef RLIMIT_NPROC
	{ RLIMIT_NPROC,  5 },
#endif
	{ -1, -1 }
};

static void
sanity_checks (void)
{
	int stderr_fd;
	int i, open_max;
	int flag;

	/*
	 * Make sure stdin/stdout are open.  This is a requirement
	 * for our program to work and closes potential security holes.
	 */
	if ((fcntl (0, F_GETFL, &flag) == -1 && errno == EBADF) ||
	    (fcntl (1, F_GETFL, &flag) == -1 && errno == EBADF)) {
		exit (1);
	}

	/*
	 * File descriptors 0 and 1 have been setup by the parent process
	 * to be used for the protocol exchange and for transfering
	 * file descriptors.
	 *
	 * Make stderr point to a terminal.
	 */
	if (fcntl (2, F_GETFL, &flag) == -1 && errno == EBADF) {
		stderr_fd = open ("/dev/tty", O_RDWR);
		if (stderr_fd == -1) {
			stderr_fd = open ("/dev/null", O_RDWR);
			if (stderr_fd == -1)
				exit (1);
		}

		if (stderr_fd != 2)
			while (dup2 (stderr_fd, 2) == -1 && errno == EINTR)
				;
	}

	/* Close any file descriptor we do not use */
	open_max = sysconf (_SC_OPEN_MAX);
	for (i = 3; i < open_max; i++) {
		close (i);
	}

	/* Check sensible resource limits */
	for (i = 0; sensible_limits [i].value != -1; i++) {
		struct rlimit rlim;

		if (getrlimit (sensible_limits [i].limit, &rlim) != 0)
			continue;

		if (rlim.rlim_cur != RLIM_INFINITY &&
		    rlim.rlim_cur < sensible_limits [i].value) {
			if (setrlimit (sensible_limits [i].limit, &rlim) != 0) {
				fprintf (stderr, "Living environment not ok\n");
				exit (1);
			}
		}
	}

	/* Make sure SIGIO/SIGINT is SIG_IGN */
	{
		struct sigaction sa;
		sigset_t sigset;

		sa.sa_handler = SIG_IGN;
		sigemptyset (&sa.sa_mask);
		sa.sa_flags = 0;

		sigemptyset(&sigset);
		sigaddset(&sigset, SIGIO);
		sigaddset(&sigset, SIGINT);
		sigprocmask(SIG_UNBLOCK, &sigset, NULL);

		sigaction (SIGIO, &sa, NULL);
		sigaction (SIGINT, &sa, NULL);
	}
}

static volatile int done;

static void
exit_handler (int signum)
{
	done = 1;
}


int
main (int argc, char *argv [])
{
	int res, n;
	void *tag;
	GnomePtyOps op;
	const char *logname;

	sanity_checks ();

	pwent = NULL;

	logname = getenv ("LOGNAME");
	if (logname != NULL) {
		pwent = getpwnam (logname);
		if (pwent != NULL && pwent->pw_uid != getuid ()) {
			/* LOGNAME is lying, fall back to looking up the uid */
			pwent = NULL;
		}
	}

	if (pwent == NULL)
		pwent = getpwuid (getuid ());

	if (pwent)
		login_name = pwent->pw_name;
	else {
		sprintf (login_name_buffer, "#%u", (unsigned int)getuid ());
		login_name = login_name_buffer;
	}

        /* Change directory so we don't prevent unmounting in case the initial cwd
         * is on an external device (see bug #574491). 
         */
	if (chdir ("/") < 0)
                fprintf (stderr, "Failed to chdir to /: %s\n", strerror (errno));
 
	display_name = getenv ("DISPLAY");
	if (!display_name)
		display_name = "localhost";

	done = 0;

	/* Make sure we clean up utmp/wtmp even under vncserver */
	signal (SIGHUP, exit_handler);
	signal (SIGTERM, exit_handler);

	while (!done) {
		res = n_read (STDIN_FILENO, &op, sizeof (op));

		if (res != sizeof (op)) {
			done = 1;
			continue;
		}

		switch (op) {
		case GNOME_PTY_OPEN_PTY_UTMP:
			open_ptys (1, 0, 0);
			break;

		case GNOME_PTY_OPEN_PTY_UWTMP:
			open_ptys (1, 1, 0);
			break;

		case GNOME_PTY_OPEN_PTY_WTMP:
			open_ptys (0, 1, 0);
			break;

		case GNOME_PTY_OPEN_PTY_LASTLOG:
			open_ptys (0, 0, 1);
			break;

		case GNOME_PTY_OPEN_PTY_LASTLOGUTMP:
			open_ptys (1, 0, 1);
			break;

		case GNOME_PTY_OPEN_PTY_LASTLOGUWTMP:
			open_ptys (1, 1, 1);
			break;

		case GNOME_PTY_OPEN_PTY_LASTLOGWTMP:
			open_ptys (0, 1, 1);
			break;

		case GNOME_PTY_OPEN_NO_DB_UPDATE:
			open_ptys (0, 0, 0);
			break;

		case GNOME_PTY_RESET_TO_DEFAULTS:
			break;

		case GNOME_PTY_CLOSE_PTY:
			n = n_read (STDIN_FILENO, &tag, sizeof (tag));
			if (n != sizeof (tag)) {
				shutdown_helper ();
				exit (1);
			}
			close_pty_pair (tag);
			break;

		case GNOME_PTY_SYNCH:
			{
				int result = 0;
				n_write (STDIN_FILENO, &result, 1);
			}
			break;
		}

	}

	shutdown_helper ();
	return 0;
}

