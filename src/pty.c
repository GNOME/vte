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
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include "pty.h"

/* Open the named PTY slave, fork off a child (storing its PID in child),
 * and exec the named command in its own session as a process group leader */
static int
vte_pty_fork_on_fd(const char *path, const char **env_add,
		   const char *command, const char **argv, pid_t *child)
{
	int fd, i;
	pid_t pid;
	char **args, *arg;

	/* Start up a child. */
	pid = fork();
	if (pid == -1) {
		/* Error fork()ing.  Bail. */
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

	/* Close all descriptors except for the slave. */
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

	/* Set any environment variables. */
	for (i = 0; (env_add != NULL) && (env_add[i] != NULL); i++) {
		if (putenv(g_strdup(env_add[i])) == -1) {
			g_warning("Error adding `%s' to environment, "
				  "continuing.", env_add[i]);
		}
	}

	/* Outta here. */
	if (argv != NULL) {
		for (i = 0; (argv[i] != NULL); i++) ;
		args = g_malloc0(sizeof(char*) * (i + 1));
		for (i = 0; (argv[i] != NULL); i++) {
			args[i] = g_strdup(argv[i]);
		}
		execv(command, args);
	} else {
		if (strchr(command, '/')) {
			arg = g_strdup(strrchr(command, '/') + 1);
		} else {
			arg = g_strdup_printf("%s", command);
		}
		execl(command, arg, NULL);
	}

	/* Avoid calling any atexit() code. */
	_exit(0);
}

static char *
vte_pty_ptsname(int master)
{
#if defined(HAVE_PTSNAME_R)
	char buf[PATH_MAX];
	memset(buf, 0, sizeof(buf));
	if (ptsname_r(master, buf, sizeof(buf) - 1) == 0) {
		return g_strdup(buf);
	}
#elif defined(HAVE_PTSNAME)
	char *p;
	if ((p = ptsname(master)) == NULL) {
		return g_strdup(p);
	}
#elif defined(TIOCGPTN)
	int pty = 0;
	if (ioctl(master, TIOCGPTN, &pty) == 0) {
		return g_strdup_printf("/dev/pts/%d", pty);
	}
#endif
	return NULL;
}

static int
vte_pty_getpt()
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
vte_pty_open_unix98(pid_t *child, const char **env_add,
		    const char *command, const char **argv)
{
	int fd;
	char *buf;

	/* Attempt to open the master. */
	fd = vte_pty_getpt();
	if (fd != -1) {
		/* Read the slave number and unlock it. */
		if (((buf = vte_pty_ptsname(fd)) == NULL) ||
		    (vte_pty_grantpt(fd) != 0) ||
		    (vte_pty_unlockpt(fd) != 0)) {
			close(fd);
			fd = -1;
		} else {
			/* Start up a child process with the given command. */
			if (vte_pty_fork_on_fd(buf, env_add, command, argv,
					       child) != 0) {
				close(fd);
				fd = -1;
			}
		}
	}
	return fd;
}

static int
vte_pty_open_old_school(pid_t *child, const char **env_add,
			const char *command, const char **argv)
{
	/* FIXME */
	return -1;
}

int
vte_pty_open(pid_t *child, const char **env_add,
	     const char *command, const char **argv)
{
	int ret = -1;
	if (ret == -1) {
		ret = vte_pty_open_unix98(child, env_add, command, argv);
	}
	if (ret == -1) {
		ret = vte_pty_open_old_school(child, env_add, command, argv);
	}
	return ret;
}

#ifdef PTY_MAIN
int
main(int argc, char **argv)
{
	pid_t child;
	int fd;
	char c;
	fd = vte_pty_open(&child, "/usr/bin/tty");
	g_print("Child pid is %d.\n", (int)child);
	while(read(fd, &c, 1) == 1) {
		write(STDOUT_FILENO, &c, 1);
	}
	return 0;
}
#endif
