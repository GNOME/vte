/*
 * gnome-login-support.c:
 *    Replacement for systems that lack login_tty, open_pty and forkpty
 *
 * Author:
 *    Miguel de Icaza (miguel@gnu.org)
 *
 *
 */
#include <config.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <grp.h>
#include <sys/types.h>
#include "gnome-login-support.h"

/*
 * HAVE_OPENPTY => HAVE_FORKPTY
 */

#ifndef HAVE_LOGIN_TTY
int
login_tty (int fd)
{
	pid_t pid = getpid ();

	/* Create the session */
	setsid ();

#ifdef TIOCSCTTY
	if (ioctl (fd, TIOCSCTTY, 0) == -1)
		return -1;
#else /* !TIOCSTTY */
        /* Hackery to set controlling tty on SVR4 -
           on SVR4 the first terminal we open after sesid()
           becomes our controlling terminal, thus we must
           find the name of, open, and re-close the tty
           since we already have it open at this point. */
        {
                char *ctty;
                int ct_fdes;

                ctty = ttyname(fd);
                ct_fdes = open(ctty, O_RDWR);
                close(ct_fdes);
        }
#endif /* !TIOCSTTY */

#if defined (_POSIX_VERSION) || defined (__svr4__)
	tcsetpgrp (0, pid);
#elif defined (TIOCSPGRP)
	ioctl (0, TIOCSPGRP, &pid);
#endif

	dup2 (fd, 0);
	dup2 (fd, 1);
	dup2 (fd, 2);
	if (fd > 2)
		close (fd);

	return 0;
}
#endif

#ifndef HAVE_OPENPTY
static int
pty_open_master_bsd (char *pty_name, int *used_bsd)
{
	int pty_master;
	char *ptr1, *ptr2;

	*used_bsd = 1;

	strcpy (pty_name, "/dev/ptyXX");
	for (ptr1 = "pqrstuvwxyzPQRST"; *ptr1; ++ptr1)
	{
		pty_name [8] = *ptr1;
		for (ptr2 = "0123456789abcdef"; *ptr2; ++ptr2)
		{
			pty_name [9] = *ptr2;

			/* Try to open master */
			if ((pty_master = open (pty_name, O_RDWR)) == -1) {
				if (errno == ENOENT)  /* Different from EIO */
					return -1;    /* Out of pty devices */
				else
					continue;      /* Try next pty device */
			}
			pty_name [5] = 't';	       /* Change "pty" to "tty" */
			if (access (pty_name, (R_OK | W_OK))){
				close (pty_master);
				pty_name [5] = 'p';
				continue;
			}
			return pty_master;
		}
	}
	return -1;  /* Ran out of pty devices */
}

static int
pty_open_slave_bsd (const char *pty_name)
{
	int pty_slave;
	struct group *group_info = getgrnam ("tty");

	if (group_info != NULL)
	{
		/* The following two calls will only succeed if we are root */

		chown (pty_name, getuid (), group_info->gr_gid);
		chmod (pty_name, S_IRUSR | S_IWUSR | S_IWGRP);
	}
	else
	{
		chown (pty_name, getuid (), -1);
		chmod (pty_name, S_IRUSR | S_IWUSR | S_IWGRP);
	}

#ifdef HAVE_REVOKE
	revoke (pty_name);
#endif

	if ((pty_slave = open (pty_name, O_RDWR)) == -1){
		return -1;
	}

	return pty_slave;
}

/* SystemVish pty opening */
#if defined (HAVE_GRANTPT)

#ifdef HAVE_STROPTS_H
#    include <stropts.h>
#endif

static int
pty_open_slave (const char *pty_name)
{
	int pty_slave = open (pty_name, O_RDWR);

	if (pty_slave == -1)
		return -1;

#ifdef HAVE_STROPTS_H
#if !defined(__osf__)
	if (!ioctl (pty_slave, I_FIND, "ptem"))
		if (ioctl (pty_slave, I_PUSH, "ptem") == -1){
			close (pty_slave);
			return -1;
		}

    if (!ioctl (pty_slave, I_FIND, "ldterm"))
	    if (ioctl (pty_slave, I_PUSH, "ldterm") == -1){
		    close (pty_slave);
		    return -1;
	    }

#if !defined(sgi) && !defined(__sgi)
    if (!ioctl (pty_slave, I_FIND, "ttcompat"))
	    if (ioctl (pty_slave, I_PUSH, "ttcompat") == -1)
	    {
		    perror ("ioctl (pty_slave, I_PUSH, \"ttcompat\")");
		    close (pty_slave);
		    return -1;
	    }
#endif /* sgi || __sgi */
#endif /* __osf__ */
#endif /* HAVE_STROPTS_H */

    return pty_slave;
}

static int
pty_open_master (char *pty_name, int *used_bsd)
{
	int pty_master;
	char *slave_name;

	strcpy (pty_name, "/dev/ptmx");

	pty_master = open (pty_name, O_RDWR);

	if ((pty_master == -1) && (errno == ENOENT)) {
		strcpy (pty_name, "/dev/ptc"); /* AIX */
		pty_master = open (pty_name, O_RDWR);
	}

	/*
	 * Try BSD open, this is needed for Linux which
	 * might have Unix98 devices but no kernel support
	 * for those.
	 */
	if (pty_master == -1) {
		*used_bsd = 1;
		return pty_open_master_bsd (pty_name, used_bsd);
	}
	*used_bsd = 0;

	if (grantpt (pty_master) == -1 || unlockpt (pty_master) == -1) {
		close (pty_master);
		return -1;
	}
	if ((slave_name = ptsname (pty_master)) == NULL){
		close (pty_master);
		return -1;
	}
	strcpy (pty_name, slave_name);
	return pty_master;
}
#else
#    define pty_open_master pty_open_master_bsd
#    define pty_open_slave  pty_open_slave_bsd
#endif

int
openpty (int *master_fd, int *slave_fd, char *name,
	 struct termios *termp, struct winsize *winp)
{
	int pty_master, pty_slave, used_bsd = 0;
	struct group *group_info;
	char line [256];

	pty_master = pty_open_master (line, &used_bsd);
	fcntl (pty_master, F_SETFD, FD_CLOEXEC);

	if (pty_master == -1)
		return -1;

	group_info = getgrnam ("tty");

	if (group_info != NULL){
		chown (line, getuid (), group_info->gr_gid);
		chmod (line, S_IRUSR | S_IWUSR | S_IWGRP);
	} else {
		chown (line, getuid (), -1);
		chmod (line, S_IRUSR | S_IWUSR | S_IWGRP);
	}

#ifdef HAVE_REVOKE
	revoke (line);
#endif

	/* Open slave side */
	if (used_bsd)
		pty_slave = pty_open_slave_bsd (line);
	else
		pty_slave = pty_open_slave (line);

	if (pty_slave == -1){
		close (pty_master);

		errno = ENOENT;
		return -1;
	}
	fcntl (pty_slave, F_SETFD, FD_CLOEXEC);

	*master_fd = pty_master;
	*slave_fd  = pty_slave;

	if (termp)
		tcsetattr (pty_slave, TCSAFLUSH, termp);

	if (winp)
		ioctl (pty_slave, TIOCSWINSZ, winp);

	if (name)
		strcpy (name, line);

	return 0;
}

pid_t
forkpty (int *master_fd, char *name, struct termios *termp, struct winsize *winp)
{
	int master, slave;
	pid_t pid;

	if (openpty (&master, &slave, name, termp, winp) == -1)
		return -1;

	pid = fork ();

	if (pid == -1)
		return -1;

	/* Child */
	if (pid == 0){
		close (master);
		login_tty (slave);
	} else {
		*master_fd = master;
		close (slave);
	}

	return pid;
}
#endif /* HAVE_OPENPTY */

int
n_read (int fd, void *buf, int count)
{
	int n = 0, i;
	char *buffer;

	buffer = (char*) buf;
	while (n < count) {
		i = read (fd, buffer + n, count - n);
		switch (i) {
		case -1:
			switch (errno) {
			case EINTR:
			case EAGAIN:
#ifdef ERESTART
			case ERESTART:
#endif
				/* suppress these errors */
				break;
			default:
				return -1;
				break;
			}
			break;
		case 0:
			return n;
			break;
		default:
			n += i;
			break;
		}
	}

	return n;
}

int
n_write (int fd, const void *buf, int count)
{
	int n = 0, i;
	const char *buffer;

	buffer = (char*) buf;
	while (n < count) {
		i = write (fd, buffer + n, count - n);
		switch (i) {
		case -1:
			switch (errno) {
			case EINTR:
			case EAGAIN:
#ifdef ERESTART
			case ERESTART:
#endif
				/* suppress these errors */
				break;
			default:
				return -1;
				break;
			}
			break;
		case 0:
			return n;
			break;
		default:
			n += i;
			break;
		}
	}

	return n;
}
