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

#include "../config.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#include <glib.h>
#include "debug.h"
#include "pty.h"

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

#ifdef VTE_USE_GNOME_PTY_HELPER
#include <sys/socket.h>
#include <sys/uio.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include "../gnome-pty-helper/gnome-pty.h"
static gboolean _vte_pty_helper_started = FALSE;
static pid_t _vte_pty_helper_pid = -1;
static int _vte_pty_helper_tunnel = -1;
static GTree *_vte_pty_helper_map = NULL;
#endif

/* Reset the handlers for all known signals to their defaults.  The parent
 * (or one of the libraries it links to) may have changed one to be ignored. */
static void
_vte_pty_reset_signal_handlers(void)
{
	signal(SIGHUP,  SIG_DFL);
	signal(SIGINT,  SIG_DFL);
	signal(SIGILL,  SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGFPE,  SIG_DFL);
	signal(SIGKILL, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGALRM, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
	signal(SIGCONT, SIG_DFL);
	signal(SIGSTOP, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
	signal(SIGTTIN, SIG_DFL);
	signal(SIGTTOU, SIG_DFL);
#ifdef SIGBUS
	signal(SIGBUS,  SIG_DFL);
#endif
#ifdef SIGPOLL
	signal(SIGPOLL, SIG_DFL);
#endif
#ifdef SIGPROF
	signal(SIGPROF, SIG_DFL);
#endif
#ifdef SIGSYS
	signal(SIGSYS,  SIG_DFL);
#endif
#ifdef SIGTRAP
	signal(SIGTRAP, SIG_DFL);
#endif
#ifdef SIGURG
	signal(SIGURG,  SIG_DFL);
#endif
#ifdef SIGVTALARM
	signal(SIGVTALARM, SIG_DFL);
#endif
#ifdef SIGXCPU
	signal(SIGXCPU, SIG_DFL);
#endif
#ifdef SIGXFSZ
	signal(SIGXFSZ, SIG_DFL);
#endif
#ifdef SIGIOT
	signal(SIGIOT,  SIG_DFL);
#endif
#ifdef SIGEMT
	signal(SIGEMT,  SIG_DFL);
#endif
#ifdef SIGSTKFLT
	signal(SIGSTKFLT, SIG_DFL);
#endif
#ifdef SIGIO
	signal(SIGIO,   SIG_DFL);
#endif
#ifdef SIGCLD
	signal(SIGCLD,  SIG_DFL);
#endif
#ifdef SIGPWR
	signal(SIGPWR,  SIG_DFL);
#endif
#ifdef SIGINFO
	signal(SIGINFO, SIG_DFL);
#endif
#ifdef SIGLOST
	signal(SIGLOST, SIG_DFL);
#endif
#ifdef SIGWINCH
	signal(SIGWINCH, SIG_DFL);
#endif
#ifdef SIGUNUSED
	signal(SIGUNUSED, SIG_DFL);
#endif
}

struct vte_pty_child_setup_data {
	enum {
		TTY_OPEN_BY_NAME,
		TTY_OPEN_BY_FD
	} mode;
	union {
		const char *name;
		int fd;
	} tty;
};
static void
vte_pty_child_setup (gpointer arg)
{
	struct vte_pty_child_setup_data *data = arg;
	int fd = -1;
	const char *tty = NULL;


	/* Save the name of the pty -- we'll need it later to acquire
	 * it as our controlling terminal. */
	switch (data->mode) {
		case TTY_OPEN_BY_NAME:
			tty = data->tty.name;
			break;
		case TTY_OPEN_BY_FD:
			fd = data->tty.fd;
			tty = ttyname(fd);
			break;
	}

	_vte_debug_print (VTE_DEBUG_PTY,
			"Setting up child pty: name = %s, fd = %d\n",
				tty ? tty : "(none)", fd);


	/* Start a new session and become process-group leader. */
#if defined(HAVE_SETSID) && defined(HAVE_SETPGID)
	_vte_debug_print (VTE_DEBUG_PTY, "Starting new session\n");
	setsid();
	setpgid(0, 0);
#endif

	/* Try to reopen the pty to acquire it as our controlling terminal. */
	if (tty != NULL) {
		int i = open(tty, O_RDWR);
		if (i != -1) {
			if (fd != -1){
				close(fd);
			}
			fd = i;
		}
	}

	if (fd == -1)
		_exit (127);

#ifdef TIOCSCTTY
	/* TIOCSCTTY is defined?  Let's try that, too. */
	ioctl(fd, TIOCSCTTY, fd);
#endif

#ifdef HAVE_STROPTS_H
	if ((ioctl(fd, I_FIND, "ptem") == 0) &&
			(ioctl(fd, I_PUSH, "ptem") == -1)) {
		_exit (127);
	}
	if ((ioctl(fd, I_FIND, "ldterm") == 0) &&
			(ioctl(fd, I_PUSH, "ldterm") == -1)) {
		_exit (127);
	}
	if ((ioctl(fd, I_FIND, "ttcompat") == 0) &&
			(ioctl(fd, I_PUSH, "ttcompat") == -1)) {
		perror ("ioctl (fd, I_PUSH, \"ttcompat\")");
		_exit (127);
	}
#endif

	/* now setup child I/O through the tty */
	if (fd != STDIN_FILENO) {
		if (dup2(fd, STDIN_FILENO) != STDIN_FILENO){
			_exit (127);
		}
	}
	if (fd != STDOUT_FILENO) {
		if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO){
			_exit (127);
		}
	}
	if (fd != STDERR_FILENO) {
		if (dup2(fd, STDERR_FILENO) != STDERR_FILENO){
			_exit (127);
		}
	}

	/* Close the original slave descriptor, unless it's one of the stdio
	 * descriptors. */
	if (fd != STDIN_FILENO &&
			fd != STDOUT_FILENO &&
			fd != STDERR_FILENO) {
		close(fd);
	}


	/* Reset our signals -- our parent may have done any number of
	 * weird things to them. */
	_vte_pty_reset_signal_handlers();
}

/* TODO: clean up the spawning
 * - replace current env rather than adding!
 * - allow user control over flags (eg DO_NOT_CLOSE)
 * - additional user callback for child setup
 */

/* Run the given command (if specified) */
static gboolean
_vte_pty_run_on_pty (struct vte_pty_child_setup_data *data,
		     const char *command, char **argv, char **envp,
		     const char *directory,
		     GPid *pid, GError **error)
{
	gboolean ret = TRUE;
	GError *local_error = NULL;

	if (command != NULL) {
		gchar **arg2, **envp2;
		gint i, k, argc;

		/* push the command into argv[0] */
		argc = argv ? g_strv_length (argv) : 0;
		arg2 = g_new (char *, argc + 2);
		arg2[0] = g_strdup (command);
		for (i = 0; i < argc; i++) {
			arg2[i+1] = g_strdup (argv[i]);
		}
		arg2[i+1] = NULL;

		/* add the given environment to the childs */
		i = g_strv_length (environ) + (envp ? g_strv_length (envp) : 0);
		envp2 = g_new (char *, i + 1);
		for (i = 0; environ[i] != NULL; i++) {
			envp2[i] = g_strdup (environ[i]);
		}
		if (envp != NULL) {
			for (k = 0; envp[k] != NULL; k++) {
				envp2[i++] = g_strdup (envp[k]);
			}
		}
		envp2[i] = NULL;

		_VTE_DEBUG_IF (VTE_DEBUG_MISC) {
			g_printerr ("Spawing command '%s'\n", command);
			for (i = 0; arg2[i] != NULL; i++) {
				g_printerr ("    argv[%d] = %s\n", i, arg2[i]);
			}
			for (i = 0; envp2[i] != NULL; i++) {
				g_printerr ("    env[%d] = %s\n", i, envp2[i]);
			}
			g_printerr ("    directory: %s\n",
					directory ? directory : "(none)");
		}

		ret = g_spawn_async_with_pipes (directory,
				arg2, envp2,
				G_SPAWN_CHILD_INHERITS_STDIN |
				G_SPAWN_SEARCH_PATH |
				G_SPAWN_DO_NOT_REAP_CHILD |
				(argv ? G_SPAWN_FILE_AND_ARGV_ZERO : 0),
				vte_pty_child_setup, data,
				pid,
				NULL, NULL, NULL,
				&local_error);
		if (ret == FALSE) {
			if (g_error_matches (local_error,
						G_SPAWN_ERROR,
						G_SPAWN_ERROR_CHDIR)) {
				/* try spawning in our working directory */
				g_clear_error (&local_error);
				ret = g_spawn_async_with_pipes (NULL,
						arg2, envp2,
						G_SPAWN_CHILD_INHERITS_STDIN |
						G_SPAWN_SEARCH_PATH |
						G_SPAWN_DO_NOT_REAP_CHILD |
						(argv ? G_SPAWN_FILE_AND_ARGV_ZERO : 0),
						vte_pty_child_setup, data,
						pid,
						NULL, NULL, NULL,
						&local_error);
			}
		}
		g_strfreev (arg2);
		g_strfreev (envp2);

		_vte_debug_print (VTE_DEBUG_MISC,
				"Spawn result: %s%s\n",
				ret?"Success":"Failure - ",
				ret?"":local_error->message);
		if (local_error)
			g_propagate_error (error, local_error);
	}
#ifdef HAVE_FORK
	else {
		*pid = fork();
		switch (*pid) {
			case -1:
				g_set_error (error,
					       	G_SPAWN_ERROR,
					       	G_SPAWN_ERROR_FAILED,
						"Unable to fork: %s",
						g_strerror (errno));
				ret = FALSE;
			case 0: /* child */
				vte_pty_child_setup (data);
				break;
			default: /* parent */
				break;
		}
	}
#endif

	return ret;
}

/* Open the named PTY slave, fork off a child (storing its PID in child),
 * and exec the named command in its own session as a process group leader */
static gboolean
_vte_pty_fork_on_pty_name (const char *path, int parent_fd, char **envp,
			   const char *command, char **argv,
			   const char *directory,
			   int columns, int rows, GPid *child)
{
	struct vte_pty_child_setup_data data;

	data.mode = TTY_OPEN_BY_NAME;
	data.tty.name = path;

	if (!_vte_pty_run_on_pty(&data,
			command, argv, envp, directory,
			child, NULL)) {
		/* XXX propagate the error */
		return FALSE;
	}

	_vte_pty_set_size(parent_fd, columns, rows);
	return TRUE;
}

/* Fork off a child (storing its PID in child), and exec the named command
 * in its own session as a process group leader using the given terminal. */
static gboolean
_vte_pty_fork_on_pty_fd (int fd, char **envp,
			 const char *command, char **argv,
			 const char *directory,
			 int columns, int rows, GPid *child)
{
	struct vte_pty_child_setup_data data;

	data.mode = TTY_OPEN_BY_FD;
	data.tty.fd = fd;

	if (!_vte_pty_run_on_pty(&data,
				command, argv, envp, directory,
				child, NULL)) {
		/* XXX propagate the error */
		return FALSE;
	}

	_vte_pty_set_size(fd, columns, rows);
	return TRUE;
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
_vte_pty_set_size(int master, int columns, int rows)
{
	struct winsize size;
	int ret;
	memset(&size, 0, sizeof(size));
	size.ws_row = rows ? rows : 24;
	size.ws_col = columns ? columns : 80;
	_vte_debug_print(VTE_DEBUG_PTY,
			"Setting size on fd %d to (%d,%d).\n",
			master, columns, rows);
	ret = ioctl(master, TIOCSWINSZ, &size);
	if (ret != 0) {
		_vte_debug_print(VTE_DEBUG_PTY,
				"Failed to set size on %d: %s.\n",
				master, strerror(errno));
	}
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
_vte_pty_get_size(int master, int *columns, int *rows)
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
		_vte_debug_print(VTE_DEBUG_PTY,
				"Size on fd %d is (%d,%d).\n",
				master, size.ws_col, size.ws_row);
	} else {
		_vte_debug_print(VTE_DEBUG_PTY,
				"Failed to read size from fd %d.\n",
				master);
	}
	return ret;
}

static char *
_vte_pty_ptsname(int master)
{
#if defined(HAVE_PTSNAME_R)
	gsize len = 1024;
	char *buf = NULL;
	int i;
	do {
		buf = g_malloc0(len);
		i = ptsname_r(master, buf, len - 1);
		switch (i) {
		case 0:
			/* Return the allocated buffer with the name in it. */
			_vte_debug_print(VTE_DEBUG_PTY,
					"PTY slave is `%s'.\n", buf);
			return buf;
			break;
		default:
			g_free(buf);
			buf = NULL;
			break;
		}
		len *= 2;
	} while ((i != 0) && (errno == ERANGE));
#elif defined(HAVE_PTSNAME)
	char *p;
	if ((p = ptsname(master)) != NULL) {
		_vte_debug_print(VTE_DEBUG_PTY, "PTY slave is `%s'.\n", p);
		return g_strdup(p);
	}
#elif defined(TIOCGPTN)
	int pty = 0;
	if (ioctl(master, TIOCGPTN, &pty) == 0) {
		_vte_debug_print(VTE_DEBUG_PTY,
				"PTY slave is `/dev/pts/%d'.\n", pty);
		return g_strdup_printf("/dev/pts/%d", pty);
	}
#endif
	return NULL;
}

static int
_vte_pty_getpt(void)
{
	int fd, flags;
#ifdef HAVE_GETPT
	/* Call the system's function for allocating a pty. */
	fd = getpt();
#else
	/* Try to allocate a pty by accessing the pty master multiplex. */
	fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	if ((fd == -1) && (errno == ENOENT)) {
		fd = open("/dev/ptc", O_RDWR | O_NOCTTY); /* AIX */
	}
#endif
	/* Set it to blocking. */
	flags = fcntl(fd, F_GETFL);
	flags &= ~(O_NONBLOCK);
	fcntl(fd, F_SETFL, flags);
	return fd;
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
_vte_pty_open_unix98(GPid *child, char **env_add,
		     const char *command, char **argv,
		     const char *directory, int columns, int rows)
{
	int fd;
	char *buf;

	/* Attempt to open the master. */
	fd = _vte_pty_getpt();
	_vte_debug_print(VTE_DEBUG_PTY, "Allocated pty on fd %d.\n", fd);
	if (fd != -1) {
		/* Read the slave number and unlock it. */
		if (((buf = _vte_pty_ptsname(fd)) == NULL) ||
		    (_vte_pty_grantpt(fd) != 0) ||
		    (_vte_pty_unlockpt(fd) != 0)) {
			_vte_debug_print(VTE_DEBUG_PTY,
					"PTY setup failed, bailing.\n");
			close(fd);
			fd = -1;
		} else {
			/* Start up a child process with the given command. */
			if (!_vte_pty_fork_on_pty_name(buf, fd,
						      env_add, command,
						      argv, directory,
						      columns, rows,
						      child)) {
				close(fd);
				fd = -1;
			}
			g_free(buf);
		}
	}
	return fd;
}

#ifdef HAVE_RECVMSG
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
#elif defined (I_RECVFD)
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

#ifdef VTE_USE_GNOME_PTY_HELPER
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

/* Like read, but hide EINTR and EAGAIN. */
static ssize_t
n_read(int fd, void *buffer, size_t count)
{
	size_t n = 0;
	char *buf = buffer;
	int i;
	while (n < count) {
		i = read(fd, buf + n, count - n);
		switch (i) {
		case 0:
			return n;
		case -1:
			switch (errno) {
			case EINTR:
			case EAGAIN:
#ifdef ERESTART
			case ERESTART:
#endif
				break;
			default:
				return -1;
			}
			break;
		default:
			n += i;
			break;
		}
	}
	return n;
}

/* Like write, but hide EINTR and EAGAIN. */
static ssize_t
n_write(int fd, const void *buffer, size_t count)
{
	size_t n = 0;
	const char *buf = buffer;
	int i;
	while (n < count) {
		i = write(fd, buf + n, count - n);
		switch (i) {
		case 0:
			return n;
		case -1:
			switch (errno) {
			case EINTR:
			case EAGAIN:
#ifdef ERESTART
			case ERESTART:
#endif
				break;
			default:
				return -1;
			}
			break;
		default:
			n += i;
			break;
		}
	}
	return n;
}



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
	/* Sanity check. */
	if (access(LIBEXECDIR "/gnome-pty-helper", X_OK) != 0) {
		/* Give the user some clue as to why session logging is not
		 * going to work (assuming we can open a pty using some other
		 * method). */
		g_warning(_("can not run %s"), LIBEXECDIR "/gnome-pty-helper");
		return FALSE;
	}
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
	if (_vte_pty_pipe_open(&_vte_pty_helper_tunnel, &tunnel) != 0) {
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
_vte_pty_open_with_helper(GPid *child, char **env_add,
			  const char *command, char **argv,
			  const char *directory,
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
		if (n_write(_vte_pty_helper_tunnel,
			    &ops, sizeof(ops)) != sizeof(ops)) {
			return -1;
		}
		_vte_debug_print(VTE_DEBUG_PTY, "Sent request to helper.\n");
		/* Read back the response. */
		if (n_read(_vte_pty_helper_tunnel,
			   &ret, sizeof(ret)) != sizeof(ret)) {
			return -1;
		}
		_vte_debug_print(VTE_DEBUG_PTY,
				"Received response from helper.\n");
		if (ret == 0) {
			return -1;
		}
		_vte_debug_print(VTE_DEBUG_PTY, "Helper returns success.\n");
		/* Read back a tag. */
		if (n_read(_vte_pty_helper_tunnel,
			   &tag, sizeof(tag)) != sizeof(tag)) {
			return -1;
		}
		_vte_debug_print(VTE_DEBUG_PTY, "Tag = %p.\n", tag);
		/* Receive the master and slave ptys. */
		_vte_pty_read_ptypair(_vte_pty_helper_tunnel,
				      &parentfd, &childfd);

		if ((parentfd == -1) || (childfd == -1)) {
			close(parentfd);
			close(childfd);
			return -1;
		}
		_vte_debug_print(VTE_DEBUG_PTY,
				"Got master pty %d and slave pty %d.\n",
				parentfd, childfd);

		/* Add the parent and the tag to our map. */
		g_tree_insert(_vte_pty_helper_map,
			      GINT_TO_POINTER(parentfd),
			      tag);
		/* Start up a child process with the given command. */
		if (!_vte_pty_fork_on_pty_fd(childfd, env_add, command,
					    argv, directory,
					    columns, rows, child)) {
			close(parentfd);
			close(childfd);
			return -1;
		}
		close(childfd);
		return parentfd;
	}
	return -1;
}
#endif

/**
 * _vte_pty_open:
 * @child: location to store the new process's ID
 * @env_add: a list of environment variables to add to the child's environment
 * @command: name of the binary to run
 * @argv: arguments to pass to @command
 * @directory: directory to start the new command in, or %NULL
 * @columns: desired window columns
 * @rows: desired window rows
 * @lastlog: %TRUE if the lastlog should be updated
 * @utmp: %TRUE if the utmp or utmpx log should be updated
 * @wtmp: %TRUE if the wtmp or wtmpx log should be updated
 *
 * Starts a new copy of @command running under a psuedo-terminal, optionally in
 * the supplied @directory, with window size set to @rows x @columns
 * and variables in @env_add added to its environment.  If any combination of
 * @lastlog, @utmp, and @wtmp is set, then the session is logged in the
 * corresponding system files.
 *
 * Returns: an open file descriptor for the pty master, -1 on failure
 */
int
_vte_pty_open(pid_t *child_pid, char **env_add,
	      const char *command, char **argv, const char *directory,
	      int columns, int rows,
	      gboolean lastlog, gboolean utmp, gboolean wtmp)
{
	GPid child;
	int ret = -1;
#ifdef VTE_USE_GNOME_PTY_HELPER
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
		ret = _vte_pty_open_with_helper(&child, env_add, command, argv,
						directory,
						columns, rows, opmap[op]);
	}
#endif
	if (ret == -1) {
		ret = _vte_pty_open_unix98(&child, env_add, command, argv,
					   directory, columns, rows);
	}
	if (ret != -1) {
		*child_pid = (pid_t) child;
	}
	_vte_debug_print(VTE_DEBUG_PTY,
			"Returning ptyfd = %d, child = %ld.\n",
			ret, (long) child);
	return ret;
}

/**
 * _vte_pty_set_utf8:
 * @pty: The pty master descriptor.
 * @utf8: Whether or not the pty is in UTF-8 mode.
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 */
void
_vte_pty_set_utf8(int pty, gboolean utf8)
{
#if defined(HAVE_TCSETATTR) && defined(IUTF8)
	struct termios tio;
	tcflag_t saved_cflag;
	if (pty != -1) {
		if (tcgetattr(pty, &tio) != -1) {
			saved_cflag = tio.c_iflag;
			tio.c_iflag &= ~IUTF8;
			if (utf8) {
				tio.c_iflag |= IUTF8;
			}
			if (saved_cflag != tio.c_iflag) {
				tcsetattr(pty, TCSANOW, &tio);
			}
		}
	}
#endif
}

/**
 * _vte_pty_close:
 * @pty: the pty master descriptor.
 *
 * Cleans up the PTY associated with the descriptor, specifically any logging
 * performed for the session.  The descriptor itself remains open.
 */
void
_vte_pty_close(int pty)
{
#ifdef VTE_USE_GNOME_PTY_HELPER
	gpointer tag;
	GnomePtyOps ops;
	if (_vte_pty_helper_map != NULL) {
		if (g_tree_lookup(_vte_pty_helper_map, GINT_TO_POINTER(pty))) {
			/* Signal the helper that it needs to close its
			 * connection. */
			ops = GNOME_PTY_CLOSE_PTY;
			tag = g_tree_lookup(_vte_pty_helper_map,
					    GINT_TO_POINTER(pty));
			if (n_write(_vte_pty_helper_tunnel,
				    &ops, sizeof(ops)) != sizeof(ops)) {
				return;
			}
			if (n_write(_vte_pty_helper_tunnel,
				    &tag, sizeof(tag)) != sizeof(tag)) {
				return;
			}
			/* Remove the item from the map. */
			g_tree_remove(_vte_pty_helper_map,
				      GINT_TO_POINTER(pty));
		}
	}
#endif
}

#ifdef PTY_MAIN
int fd;

static void
sigchld_handler(int signum)
{
	/* This is very unsafe.  Never do it in production code. */
	_vte_pty_close(fd);
}

int
main(int argc, char **argv)
{
	GPid child = 0;
	char c;
	int ret;
	signal(SIGCHLD, sigchld_handler);
	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	fd = _vte_pty_open(&child, NULL,
			   (argc > 1) ? argv[1] : NULL,
			   (argc > 1) ? argv + 1 : NULL,
			   NULL,
			   0, 0,
			   TRUE, TRUE, TRUE);
	if (child == 0) {
		int i;
		for (i = 0; ; i++) {
			switch (i % 3) {
			case 0:
			case 1:
				g_print("%d\n", i);
				break;
			case 2:
				g_printerr("%d\n", i);
				break;
			default:
				g_assert_not_reached();
				break;
			}
			sleep(1);
		}
	}
	g_print("Child pid is %d.\n", (int)child);
	do {
		ret = n_read(fd, &c, 1);
		if (ret == 0) {
			break;
		}
		if ((ret == -1) && (errno != EAGAIN) && (errno != EINTR)) {
			break;
		}
		if (argc < 2) {
			n_write(STDOUT_FILENO, "[", 1);
		}
		n_write(STDOUT_FILENO, &c, 1);
		if (argc < 2) {
			n_write(STDOUT_FILENO, "]", 1);
		}
	} while (TRUE);
	return 0;
}
#endif
