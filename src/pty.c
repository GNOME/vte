/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ident "$Id$"
#include "../config.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <glib.h>
#include "debug.h"
#include "pty.h"

#include "../gnome-pty-helper/gnome-pty.h"

#ifdef MSG_NOSIGNAL
#define PTY_RECVMSG_FLAGS MSG_NOSIGNAL
#else
#define PTY_RECVMSG_FLAGS 0
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#endif

static gboolean _vte_pty_helper_started = FALSE;
static pid_t _vte_pty_helper_pid = -1;
static int _vte_pty_helper_tunnel = -1;
static GTree *_vte_pty_helper_map = NULL;

/* Run the given command, using the given descriptor as the controlling
 * terminal. */
static int
_vte_pty_run_on_pty(int fd, char **env_add, const char *command, char **argv)
{
	int i;
	char **args, *arg;
	if (fd != STDIN_FILENO) {
		dup2(fd, STDIN_FILENO);
	}
	if (fd != STDOUT_FILENO) {
		dup2(fd, STDOUT_FILENO);
	}
	if (fd != STDERR_FILENO) {
		dup2(fd, STDERR_FILENO);
	}

	/* Close the original slave descriptor, unless it's one of the stdio
	 * descriptors. */
	if ((fd != STDIN_FILENO) &&
	    (fd != STDOUT_FILENO) &&
	    (fd != STDERR_FILENO)) {
		close(fd);
	}

#ifdef HAVE_STROPTS_H
	if (!ioctl (fd, I_FIND, "ptem") && ioctl (fd, I_PUSH, "ptem") == -1) {
		close (fd);
		return -1;
	}

	if (!ioctl (fd, I_FIND, "ldterm") && ioctl (fd, I_PUSH, "ldterm") == -1) {
		close (fd);
		return -1;
	}

	if (!ioctl (fd, I_FIND, "ttcompat") && ioctl (fd, I_PUSH, "ttcompat") == -1) {
		perror ("ioctl (fd, I_PUSH, \"ttcompat\")");
		close (fd);
		return -1;
	}
#endif /* HAVE_STROPTS_H */

	/* Set any environment variables. */
	for (i = 0; (env_add != NULL) && (env_add[i] != NULL); i++) {
		if (putenv(g_strdup(env_add[i])) != 0) {
			g_warning(_("Error adding `%s' to environment, "
				    "continuing."), env_add[i]);
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) ||
		    _vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "%ld: Set `%s'.\n", (long) getpid(),
				env_add[i]);
		}
#endif
	}

	/* Outta here. */
	if (argv != NULL) {
		for (i = 0; (argv[i] != NULL); i++) ;
		args = g_malloc0(sizeof(char*) * (i + 1));
		for (i = 0; (argv[i] != NULL); i++) {
			args[i] = g_strdup(argv[i]);
		}
		execvp(command, args);
	} else {
		arg = g_strdup(command);
		execlp(command, arg, NULL);
	}

	/* Avoid calling any atexit() code. */
	_exit(0);
	g_assert_not_reached();
	return 0;
}

/* Open the named PTY slave, fork off a child (storing its PID in child),
 * and exec the named command in its own session as a process group leader */
static int
_vte_pty_fork_on_pty_name(const char *path, char **env_add,
			  const char *command, char **argv, pid_t *child)
{
	int fd, i;
	pid_t pid;

	/* Start up a child. */
	pid = fork();
	if (pid == -1) {
		/* Error fork()ing.  Bail. */
		*child = -1;
		return -1;
	}
	if (pid != 0) {
		/* Parent.  Close our connection to the slave and return the
		 * new child's PID. */
		*child = pid;
		return 0;
	}

	/* Child.  Start a new session and become process-group leader. */
	setsid();
	setpgid(0, 0);

	/* Close all descriptors. */
	for (i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
		close(i);
	}

	/* Open the slave PTY, acquiring it as the controlling terminal for
	 * this process and its children. */
	fd = open(path, O_RDWR);
	if (fd == -1) {
		return -1;
	}
	return _vte_pty_run_on_pty(fd, env_add, command, argv);
}

/* Fork off a child (storing its PID in child), and exec the named command
 * in its own session as a process group leader using the given terminal. */
static int
_vte_pty_fork_on_pty_fd(int fd, char **env_add,
			const char *command, char **argv, pid_t *child)
{
	int i;
	char *tty;
	pid_t pid;

	/* Start up a child. */
	pid = fork();
	if (pid == -1) {
		/* Error fork()ing.  Bail. */
		*child = -1;
		return -1;
	}
	if (pid != 0) {
		/* Parent.  Close our connection to the slave and return the
		 * new child's PID. */
		*child = pid;
		return 0;
	}

	/* Save the name of the pty -- we'll need it later to acquire it as
	 * our controlling terminal. */
	tty = ttyname(fd);

	/* Child.  Start a new session and become process-group leader. */
	setsid();
	setpgid(0, 0);

	/* Close all other descriptors. */
	for (i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
		if (i != fd) {
			close(i);
		}
	}

	/* Try to reopen the pty to acquire it as our controlling terminal. */
	if (tty != NULL) {
		i = open(tty, O_RDWR);
		if (i != -1) {
			close(fd);
			fd = i;
		}
	}

	return _vte_pty_run_on_pty(fd, env_add, command, argv);
}

/**
 * vte_pty_set_size:
 * @master: the file descriptor of the pty master
 * @columns: the desired number of columns
 * @rows: the desired number of rows
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
vte_pty_set_size(int master, int columns, int rows)
{
	struct winsize size;
	int ret;
	memset(&size, 0, sizeof(size));
	size.ws_row = rows ? rows : 24;
	size.ws_col = columns ? columns : 80;
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		fprintf(stderr, "Setting size on fd %d to (%d,%d).\n",
			master, columns, rows);
	}
#endif
	ret = ioctl(master, TIOCSWINSZ, &size);
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		if (ret != 0) {
			fprintf(stderr, "Failed to set size on %d: %s.\n",
				master, strerror(errno));
		}
	}
#endif
	return ret;
}

/**
 * vte_pty_get_size:
 * @master: the file descriptor of the pty master
 * @columns: a place to store the number of columns
 * @rows: a place to store the number of rows
 *
 * Attempts to read the pseudo terminal's window size.
 *
 * Returns: 0 on success, -1 on failure.
 */
int
vte_pty_get_size(int master, int *columns, int *rows)
{
	struct winsize size;
	int ret;
	memset(&size, 0, sizeof(size));
	ret = ioctl(master, TIOCGWINSZ, &size);
	if (ret == 0) {
		if (columns != NULL) {
			*columns = size.ws_col;
		}
		if (rows != NULL) {
			*rows = size.ws_row;
		}
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "Size on fd %d is (%d,%d).\n",
				master, size.ws_col, size.ws_row);
		}
#endif
	} else {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "Failed to read size from fd %d.\n",
				master);
		}
#endif
	}
	return ret;
}

static char *
_vte_pty_ptsname(int master)
{
#if defined(HAVE_PTSNAME_R)
	char buf[PATH_MAX];
	memset(buf, 0, sizeof(buf));
	if (ptsname_r(master, buf, sizeof(buf) - 1) == 0) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "PTY slave is `%s'.\n", buf);
		}
#endif
		return g_strdup(buf);
	}
#elif defined(HAVE_PTSNAME)
	char *p;
	if ((p = ptsname(master)) != NULL) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "PTY slave is `%s'.\n", p);
		}
#endif
		return g_strdup(p);
	}
#elif defined(TIOCGPTN)
	int pty = 0;
	if (ioctl(master, TIOCGPTN, &pty) == 0) {
#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "PTY slave is `/dev/pts/%d'.\n", pty);
		}
#endif
		return g_strdup_printf("/dev/pts/%d", pty);
	}
#endif
	return NULL;
}

static int
_vte_pty_getpt(void)
{
#ifdef HAVE_GETPT
	return getpt();
#else
	return open("/dev/ptmx", O_RDWR | O_NOCTTY);
#endif
}

static int
_vte_pty_grantpt(int master)
{
#ifdef HAVE_GRANTPT
	return grantpt(master);
#else
	return 0;
#endif
}

static int
_vte_pty_unlockpt(int fd)
{
#ifdef HAVE_UNLOCKPT
	return unlockpt(fd);
#elif defined(TIOCSPTLCK)
	int zero = 0;
	return ioctl(fd, TIOCSPTLCK, &zero);
#else
	return -1;
#endif
}

static int
_vte_pty_open_unix98(pid_t *child, char **env_add,
		    const char *command, char **argv,
		    int columns, int rows)
{
	int fd;
	char *buf;

	/* Attempt to open the master. */
	fd = _vte_pty_getpt();
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		fprintf(stderr, "Allocated pty on fd %d.\n", fd);
	}
#endif
	if (fd != -1) {
		/* Read the slave number and unlock it. */
		if (((buf = _vte_pty_ptsname(fd)) == NULL) ||
		    (_vte_pty_grantpt(fd) != 0) ||
		    (_vte_pty_unlockpt(fd) != 0)) {
#ifdef VTE_DEBUG
			if (_vte_debug_on(VTE_DEBUG_PTY)) {
				fprintf(stderr, "PTY setup failed, bailing.\n");
			}
#endif
			close(fd);
			fd = -1;
		} else {
			/* Set the window size. */
			vte_pty_set_size(fd, columns, rows);
			/* Start up a child process with the given command. */
			if (_vte_pty_fork_on_pty_name(buf, env_add, command,
						      argv, child) != 0) {
				close(fd);
				fd = -1;
			}
			g_free(buf);
		}
	}
	return fd;
}

#ifdef HAVE_SENDMSG
static void
_vte_pty_read_ptypair(int tunnel, int *parentfd, int *childfd)
{
	int i, ret;
	char control[LINE_MAX], iobuf[LINE_MAX];
	struct cmsghdr *cmsg;
	struct msghdr msg;
	struct iovec vec;

	for (i = 0; i < 2; i++) {
		vec.iov_base = iobuf;
		vec.iov_len = sizeof(iobuf);
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &vec;
		msg.msg_iovlen = 1;
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);
		ret = recvmsg(tunnel, &msg, PTY_RECVMSG_FLAGS);
		if (ret == -1) {
			return;
		}
		for (cmsg = CMSG_FIRSTHDR(&msg);
		     cmsg != NULL;
		     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if (cmsg->cmsg_type == SCM_RIGHTS) {
				memcpy(&ret, CMSG_DATA(cmsg), sizeof(ret));
				switch (i) {
					case 0:
						*parentfd = ret;
						break;
					case 1:
						*childfd = ret;
						break;
					default:
						g_assert_not_reached();
						break;
				}
			}
		}
	}
}
#else
#ifdef I_RECVFD
static void
_vte_pty_read_ptypair(int tunnel, int *parentfd, int *childfd)
{
	int i;
	if (ioctl(tunnel, I_RECVFD, &i) == -1) {
		return;
	}
	*parentfd = i;
	if (ioctl(tunnel, I_RECVFD, &i) == -1) {
		return;
	}
	*childfd = i;
}
#endif
#endif

#ifdef HAVE_SOCKETPAIR
static int
_vte_pty_pipe_open(int *a, int *b)
{
	int p[2], ret = -1;
#ifdef PF_UNIX
#ifdef SOCK_STREAM
	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, p);
#else
#ifdef SOCK_DGRAM
	ret = socketpair(PF_UNIX, SOCK_DGRAM, 0, p);
#endif
#endif
	if (ret == 0) {
		*a = p[0];
		*b = p[1];
		return 0;
	}
#endif
	return ret;
}
#else
static int
_vte_pty_pipe_open(int *a, int *b)
{
	int p[2], ret = -1;

	ret = pipe(p);

	if (ret == 0) {
		*a = p[0];
		*b = p[1];
	}
	return ret;
}
#endif

static void
_vte_pty_stop_helper(void)
{
	if (_vte_pty_helper_started) {
		g_tree_destroy(_vte_pty_helper_map);
		_vte_pty_helper_map = NULL;
		close(_vte_pty_helper_tunnel);
		_vte_pty_helper_tunnel = -1;
		kill(_vte_pty_helper_pid, SIGTERM);
		_vte_pty_helper_pid = -1;
		_vte_pty_helper_started = FALSE;
	}
}

static gint
_vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

static gboolean
_vte_pty_start_helper(void)
{
	int i, tmp[2], tunnel;
	/* Create a communication link for use with the helper. */
	tmp[0] = open("/dev/null", O_RDONLY);
	if (tmp[0] == -1) {
		return FALSE;
	}
	tmp[1] = open("/dev/null", O_RDONLY);
	if (tmp[1] == -1) {
		close(tmp[0]);
		return FALSE;
	}
	if (_vte_pty_pipe_open(&_vte_pty_helper_tunnel, &tunnel) == -1) {
		return FALSE;
	}
	close(tmp[0]);
	close(tmp[1]);
	/* Now fork and start the helper. */
	_vte_pty_helper_pid = fork();
	if (_vte_pty_helper_pid == -1) {
		return FALSE;
	}
	if (_vte_pty_helper_pid == 0) {
		/* Child.  Close all descriptors. */
		for (i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
			if (i != tunnel) {
				close(i);
			}
		}
		/* Reassign the socket pair to stdio. */
		dup2(tunnel, STDIN_FILENO);
		dup2(tunnel, STDOUT_FILENO);
		close(tunnel);
		close(_vte_pty_helper_tunnel);
		/* Exec our helper. */
		execl(LIBEXECDIR "/gnome-pty-helper",
		      "gnome-pty-helper", NULL);
		/* Bail. */
		_exit(1);
	}
	close(tunnel);
	_vte_pty_helper_map = g_tree_new(_vte_direct_compare);
	atexit(_vte_pty_stop_helper);
	return TRUE;
}

static int
_vte_pty_open_old_school(pid_t *child, char **env_add,
			 const char *command, char **argv,
			 int columns, int rows, int op)
{
	GnomePtyOps ops;
	int ret;
	int parentfd = -1, childfd = -1;
	gpointer tag;
	/* We have to use the pty helper here. */
	if (!_vte_pty_helper_started) {
		_vte_pty_helper_started = _vte_pty_start_helper();
	}
	/* Try to open a new descriptor. */
	if (_vte_pty_helper_started) {
		ops = op;
		/* Send our request. */
		if (write(_vte_pty_helper_tunnel,
			  &ops, sizeof(ops)) != sizeof(ops)) {
			return -1;
		}
		/* Read back the response. */
		if (read(_vte_pty_helper_tunnel,
			 &ret, sizeof(ret)) != sizeof(ret)) {
			return -1;
		}
		if (ret == 0) {
			return -1;
		}
		/* Read back a tag. */
		if (read(_vte_pty_helper_tunnel,
			 &tag, sizeof(tag)) != sizeof(tag)) {
			return -1;
		}
		/* Receive the master and slave ptys. */
		_vte_pty_read_ptypair(_vte_pty_helper_tunnel,
				      &parentfd, &childfd);

		if ((parentfd == -1) || (childfd == -1)) {
			close(parentfd);
			close(childfd);
			return -1;
		}

#ifdef VTE_DEBUG
		if (_vte_debug_on(VTE_DEBUG_MISC) ||
		    _vte_debug_on(VTE_DEBUG_PTY)) {
			fprintf(stderr, "Got master pty %d and slave pty %d.\n",
				parentfd, childfd);
		}
#endif

		/* Add the parent and the tag to our map. */
		g_tree_insert(_vte_pty_helper_map,
			      GINT_TO_POINTER(parentfd),
			      tag);
		/* Set the window size. */
		vte_pty_set_size(parentfd, columns, rows);
		/* Start up a child process with the given command. */
		if (_vte_pty_fork_on_pty_fd(childfd, env_add, command,
					    argv, child) != 0) {
			close(parentfd);
			close(childfd);
			return -1;
		}
		close(childfd);
		return parentfd;
	}
	return -1;
}

/**
 * vte_pty_open_with_logging:
 * @child: location to store the new process's ID
 * @env_add: a list of environment variables to add to the child's environment
 * @command: name of the binary to run
 * @argv: arguments to pass to @command
 * @columns: desired window columns
 * @rows: desired window rows
 * @lastlog: TRUE if the lastlog should be updated
 * @utmp: TRUE if the utmp or utmpx log should be updated
 * @wtmp: TRUE if the wtmp or wtmpx log should be updated
 *
 * Starts a new copy of @command running under a psuedo-terminal, with window
 * size set to @rows x @columns and variables in @env_add added to its
 * environment.  If any combination of @lastlog, @utmp, and @wtmp is set,
 * then the session is logged in the appropriate system log.
 *
 * Returns: an open file descriptor for the pty master, -1 on failure
 */
int
vte_pty_open_with_logging(pid_t *child, char **env_add,
			  const char *command, char **argv,
			  int columns, int rows,
			  gboolean lastlog, gboolean utmp, gboolean wtmp)
{
	int ret = -1;
	int op = 0;
	int opmap[8] = {
		GNOME_PTY_OPEN_NO_DB_UPDATE,		/* 0 0 0 */
		GNOME_PTY_OPEN_PTY_LASTLOG,		/* 0 0 1 */
		GNOME_PTY_OPEN_PTY_UTMP,		/* 0 1 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGUTMP,		/* 0 1 1 */
		GNOME_PTY_OPEN_PTY_WTMP,		/* 1 0 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGWTMP,		/* 1 0 1 */
		GNOME_PTY_OPEN_PTY_UWTMP,		/* 1 1 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGUWTMP,	/* 1 1 1 */
	};
	if (lastlog) {
		op += 1;
	}
	if (utmp) {
		op += 2;
	}
	if (wtmp) {
		op += 4;
	}
	g_assert(op >= 0);
	g_assert(op < G_N_ELEMENTS(opmap));
	if (ret == -1) {
		ret = _vte_pty_open_old_school(child, env_add, command, argv,
					       columns, rows, opmap[op]);
	}
	if (ret == -1) {
		ret = _vte_pty_open_unix98(child, env_add, command, argv,
					   columns, rows);
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		fprintf(stderr, "Returning ptyfd = %d.\n", ret);
	}
#endif
	return ret;
}

/**
 * vte_pty_open:
 * @child: location to store the new process's ID
 * @env_add: a list of environment variables to add to the child's environment
 * @command: name of the binary to run
 * @argv: arguments to pass to @command
 * @columns: desired window columns
 * @rows: desired window rows
 *
 * Starts a new copy of @command running under a psuedo-terminal, with window
 * size set to @rows x @columns and variables in @env_add added to its
 * environment.  A convenience wrapper for vte_pty_open_with_logging().
 *
 * Returns: an open file descriptor for the pty master, -1 on failure
 */
int
vte_pty_open(pid_t *child, char **env_add,
	     const char *command, char **argv,
	     int columns, int rows)
{
	return vte_pty_open_with_logging(child, env_add, command, argv,
					 columns, rows,
					 FALSE, FALSE, FALSE);
}


/**
 * vte_pty_close:
 * @pty: the pty master descriptor.
 *
 * Cleans up the PTY associated with the descriptor, specifically any logging
 * performed for the session.  The descriptor itself remains open.
 */
void
vte_pty_close(int pty)
{
	gpointer tag;
	GnomePtyOps ops;
	if (g_tree_lookup(_vte_pty_helper_map, GINT_TO_POINTER(pty))) {
		/* Signal the helper that it needs to close its connection. */
		ops = GNOME_PTY_CLOSE_PTY;
		tag = g_tree_lookup(_vte_pty_helper_map, GINT_TO_POINTER(pty));
		if (write(_vte_pty_helper_tunnel,
			  &ops, sizeof(ops)) != sizeof(ops)) {
			return;
		}
		if (write(_vte_pty_helper_tunnel,
			  &tag, sizeof(tag)) != sizeof(tag)) {
			return;
		}
		/* Remove the item from the map. */
		g_tree_remove(_vte_pty_helper_map, GINT_TO_POINTER(pty));
	}
}

#ifdef PTY_MAIN
int fd;

static void
sigchld_handler(int signum)
{
	/* This is very unsafe.  Never do it in production code. */
	vte_pty_close(fd);
}

int
main(int argc, char **argv)
{
	pid_t child;
	char c;
	int ret;
	signal(SIGCHLD, sigchld_handler);
	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	fd = vte_pty_open(&child, NULL,
			  (argc > 1) ? argv[1] : "/usr/bin/tty",
			  (argc > 1) ? argv + 1 : NULL,
			  0, 0);
	g_print("Child pid is %d.\n", (int)child);
	do {
		ret = read(fd, &c, 1);
		if (ret == 0) {
			break;
		}
		if ((ret == -1) && (errno != EAGAIN) && (errno != EINTR)) {
			break;
		}
		write(STDOUT_FILENO, &c, 1);
	} while (TRUE);
	return 0;
}
#endif
