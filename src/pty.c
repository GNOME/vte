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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <glib.h>
#include "debug.h"
#include "pty.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#endif

/* Open the named PTY slave, fork off a child (storing its PID in child),
 * and exec the named command in its own session as a process group leader */
static int
vte_pty_fork_on_pty(const char *path, char **env_add,
		    const char *command, char **argv, pid_t *child)
{
	int fd, i;
	pid_t pid;
	char **args, *arg;

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
vte_pty_ptsname(int master)
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
vte_pty_getpt(void)
{
#ifdef HAVE_GETPT
	return getpt();
#else
	return open("/dev/ptmx", O_RDWR | O_NOCTTY);
#endif
}

static int
vte_pty_grantpt(int master)
{
#ifdef HAVE_GRANTPT
	return grantpt(master);
#else
	return 0;
#endif
}

static int
vte_pty_unlockpt(int fd)
{
#ifdef HAVE_UNLOCKPT
	return unlockpt(fd);
#elif defined(TIOCSPTLCK)
	int zero = 0;
	return ioctl(fd, TIOCSPTLCK, &zero);
#endif
	return -1;
}

static int
vte_pty_open_unix98(pid_t *child, char **env_add,
		    const char *command, char **argv,
		    int columns, int rows)
{
	int fd;
	char *buf;

	/* Attempt to open the master. */
	fd = vte_pty_getpt();
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		fprintf(stderr, "Allocated pty on fd %d.\n", fd);
	}
#endif
	if (fd != -1) {
		/* Read the slave number and unlock it. */
		if (((buf = vte_pty_ptsname(fd)) == NULL) ||
		    (vte_pty_grantpt(fd) != 0) ||
		    (vte_pty_unlockpt(fd) != 0)) {
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
			if (vte_pty_fork_on_pty(buf, env_add, command, argv,
						child) != 0) {
				close(fd);
				fd = -1;
			}
			g_free(buf);
		}
	}
	return fd;
}

static int
vte_pty_open_old_school(pid_t *child, char **env_add,
			const char *command, char **argv,
			int columns, int rows)
{
	/* FIXME */
	return -1;
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
 * environment.
 *
 * Returns: an open file descriptor for the pty master, -1 on failure
 */
int
vte_pty_open(pid_t *child, char **env_add,
	     const char *command, char **argv,
	     int columns, int rows)
{
	int ret = -1;
	if (ret == -1) {
		ret = vte_pty_open_unix98(child, env_add, command, argv,
					  columns, rows);
	}
	if (ret == -1) {
		ret = vte_pty_open_old_school(child, env_add, command, argv,
					      columns, rows);
	}
#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_PTY)) {
		fprintf(stderr, "Returning ptyfd = %d.\n", ret);
	}
#endif
	return ret;
}

#ifdef PTY_MAIN
int
main(int argc, char **argv)
{
	pid_t child;
	int fd;
	char c;
	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	fd = vte_pty_open(&child, NULL, "/usr/bin/tty", NULL, 0, 0);
	g_print("Child pid is %d.\n", (int)child);
	while(read(fd, &c, 1) == 1) {
		write(STDOUT_FILENO, &c, 1);
	}
	return 0;
}
#endif
