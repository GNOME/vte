/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010 Christian Persch
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

/**
 * SECTION: vte-pty
 * @short_description: Functions for starting a new process on a new pseudo-terminal and for
 * manipulating pseudo-terminals
 *
 * The terminal widget uses these functions to start commands with new controlling
 * pseudo-terminals and to resize pseudo-terminals.
 *
 * Since: 0.26
 */

#include <config.h>

#include "vtepty.h"
#include "vtepty-private.h"
#include "vte.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
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
#include <gio/gio.h>
#include "debug.h"
#include "pty.h"

#ifdef MSG_NOSIGNAL
#define PTY_RECVMSG_FLAGS MSG_NOSIGNAL
#else
#define PTY_RECVMSG_FLAGS 0
#endif

#include <glib/gi18n-lib.h>

#ifdef VTE_USE_GNOME_PTY_HELPER
#include <sys/uio.h>
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#include "../gnome-pty-helper/gnome-pty.h"
static gboolean _vte_pty_helper_started = FALSE;
static pid_t _vte_pty_helper_pid = -1;
static int _vte_pty_helper_tunnel = -1;
#endif

/* Reset the handlers for all known signals to their defaults.  The parent
 * (or one of the libraries it links to) may have changed one to be ignored. */
static void
_vte_pty_reset_signal_handlers(void)
{
#ifdef SIGHUP
	signal(SIGHUP,  SIG_DFL);
#endif
	signal(SIGINT,  SIG_DFL);
	signal(SIGILL,  SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGFPE,  SIG_DFL);
#ifdef SIGKILL
	signal(SIGKILL, SIG_DFL);
#endif
	signal(SIGSEGV, SIG_DFL);
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_DFL);
#endif
#ifdef SIGALRM
	signal(SIGALRM, SIG_DFL);
#endif
	signal(SIGTERM, SIG_DFL);
#ifdef SIGCHLD
	signal(SIGCHLD, SIG_DFL);
#endif
#ifdef SIGCONT
	signal(SIGCONT, SIG_DFL);
#endif
#ifdef SIGSTOP
	signal(SIGSTOP, SIG_DFL);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_DFL);
#endif
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

typedef struct _VtePtyPrivate VtePtyPrivate;

typedef struct {
	enum {
		TTY_OPEN_BY_NAME,
		TTY_OPEN_BY_FD
	} mode;
	union {
		const char *name;
		int fd;
	} tty;
} VtePtyChildSetupData;

/**
 * VtePty:
 *
 * Since: 0.26
 */
struct _VtePty {
        GObject parent_instance;

        /* <private> */
        VtePtyPrivate *priv;
};

struct _VtePtyPrivate {
        VtePtyFlags flags;
        int pty_fd;

        const char *term;
        VtePtyChildSetupData child_setup_data;

        gpointer helper_tag; /* only use when using_helper is TRUE */

        guint utf8 : 1;
        guint foreign : 1;
        guint using_helper : 1;
};

struct _VtePtyClass {
        GObjectClass parent_class;
};

/**
 * vte_pty_child_setup:
 * @pty: a #VtePty
 *
 * FIXMEchpe
 *
 * Since: 0.26
 */
void
vte_pty_child_setup (VtePty *pty)
{
        VtePtyPrivate *priv = pty->priv;
	VtePtyChildSetupData *data = &priv->child_setup_data;
	int fd = -1;
	const char *tty = NULL;

        if (priv->foreign) {
                fd = priv->pty_fd;
        } else {
                /* Save the name of the pty -- we'll need it later to acquire
                * it as our controlling terminal.
                */
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


                /* Try to reopen the pty to acquire it as our controlling terminal. */
                /* FIXMEchpe: why not just use the passed fd in TTY_OPEN_BY_FD mode? */
                if (tty != NULL) {
                        int i = open(tty, O_RDWR);
                        if (i != -1) {
                                if (fd != -1){
                                        close(fd);
                                }
                                fd = i;
                        }
                }
        }

	if (fd == -1)
		_exit (127);

	/* Start a new session and become process-group leader. */
#if defined(HAVE_SETSID) && defined(HAVE_SETPGID)
	_vte_debug_print (VTE_DEBUG_PTY, "Starting new session\n");
	setsid();
	setpgid(0, 0);
#endif

#ifdef TIOCSCTTY
	/* TIOCSCTTY is defined?  Let's try that, too. */
	ioctl(fd, TIOCSCTTY, fd);
#endif

#ifdef HAVE_STROPTS_H
	if (isastream (fd) == 1) {
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

        /* Now set the TERM environment variable */
        if (priv->term != NULL) {
                g_setenv("TERM", priv->term, TRUE);
        }
}

/* TODO: clean up the spawning
 * - replace current env rather than adding!
 */

/*
 * __vte_pty_get_argv:
 * @command: the command to run
 * @argv: the argument vector
 * @flags: (inout) flags from #GSpawnFlags
 *
 * Creates an argument vector to pass to g_spawn_async() from @command and
 * @argv, modifying *@flags if necessary.
 *
 * Returns: a newly allocated array of strings. Free with g_strfreev()
 */
char **
__vte_pty_get_argv (const char *command,
                    char **argv,
                    GSpawnFlags *flags /* inout */)
{
	char **argv2;
	int i, argc;

        g_return_val_if_fail(command != NULL, NULL);

	/* push the command into argv[0] */
	argc = argv ? g_strv_length (argv) : 0;
	argv2 = g_new (char *, argc + 2);

	argv2[0] = g_strdup (command);

	for (i = 0; i < argc; i++) {
		argv2[i+1] = g_strdup (argv[i]);
	}
	argv2[i+1] = NULL;

        if (argv) {
                *flags |= G_SPAWN_FILE_AND_ARGV_ZERO;
        }

	return argv2;
}

/*
 * __vte_pty_merge_environ:
 * @envp: environment vector
 *
 * Merges @envp to the parent environment, and returns a new environment vector.
 *
 * Returns: a newly allocated string array. Free using g_strfreev()
 */
static gchar **
__vte_pty_merge_environ (char **envp)
{
	GHashTable *table;
        GHashTableIter iter;
        char *name, *value;
	gchar **parent_environ;
	GPtrArray *array;
	gint i;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	parent_environ = g_listenv ();
	for (i = 0; parent_environ[i] != NULL; i++) {
		g_hash_table_replace (table,
			              g_strdup (parent_environ[i]),
				      g_strdup (g_getenv (parent_environ[i])));
	}
	g_strfreev (parent_environ);

	if (envp != NULL) {
		for (i = 0; envp[i] != NULL; i++) {
			name = g_strdup (envp[i]);
			value = strchr (name, '=');
			if (value) {
				*value = '\0';
				value = g_strdup (value + 1);
			}
			g_hash_table_replace (table, name, value);
		}
	}

	array = g_ptr_array_sized_new (g_hash_table_size (table) + 1);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, (gpointer) &name, (gpointer) &value)) {
                g_ptr_array_add (array, g_strconcat (name, "=", value, NULL));
        }
        g_assert(g_hash_table_size(table) == array->len);
	g_hash_table_destroy (table);
	g_ptr_array_add (array, NULL);

	return (gchar **) g_ptr_array_free (array, FALSE);
}

/*
 * __vte_pty_get_pty_flags:
 * @lastlog: %TRUE if the session should be logged to the lastlog
 * @utmp: %TRUE if the session should be logged to the utmp/utmpx log
 * @wtmp: %TRUE if the session should be logged to the wtmp/wtmpx log
 *
 * Combines the @lastlog, @utmp, @wtmp arguments into the coresponding
 * #VtePtyFlags flags.
 *
 * Returns: flags from #VtePtyFlags
 */
VtePtyFlags
__vte_pty_get_pty_flags(gboolean lastlog,
                        gboolean utmp,
                        gboolean wtmp)
{
        VtePtyFlags flags = VTE_PTY_DEFAULT;

        if (!lastlog)
                flags |= VTE_PTY_NO_LASTLOG;
        if (!utmp)
                flags |= VTE_PTY_NO_UTMP;
        if (!wtmp)
                flags |= VTE_PTY_NO_WTMP;

        return flags;
}

/*
 * __vte_pty_spawn:
 * @pty: a #VtePty
 * @directory: the name of a directory the command should start in, or %NULL
 *   to use the cwd
 * @argv: child's argument vector
 * @envv: a list of environment variables to be added to the environment before
 *   starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before exec()
 * @child_setup_data: user data for @child_setup
 * @child_pid: a location to store the child PID, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Uses g_spawn_async() to spawn the command in @argv. The child's environment will
 * be the parent environment with the variables in @envv set afterwards.
 *
 * Enforces the vte_terminal_watch_child() requirements by adding
 * %G_SPAWN_DO_NOT_REAP_CHILD to @spawn_flags.
 *
 * Note that the %G_SPAWN_LEAVE_DESCRIPTORS_OPEN flag is not supported;
 * it will be cleared!
 *
 * If spawning the command in @working_directory fails because the child
 * is unable to chdir() to it, falls back trying to spawn the command
 * in the parent's working directory.
 *
 * Returns: %TRUE on success, or %FALSE on failure with @error filled in
 */
gboolean
__vte_pty_spawn (VtePty *pty,
                 const char *directory,
                 char **argv,
                 char **envv,
                 GSpawnFlags spawn_flags,
                 GSpawnChildSetupFunc child_setup,
                 gpointer child_setup_data,
                 GPid *child_pid /* out */,
                 GError **error)
{
	gboolean ret = TRUE;
        char **envp2;
        gint i;
        GError *err = NULL;

        spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;

        /* FIXMEchpe: Enforce this until I've checked our code to make sure
         * it doesn't leak out internal FDs into the child this way.
         */
        spawn_flags &= ~G_SPAWN_LEAVE_DESCRIPTORS_OPEN;

        /* add the given environment to the childs */
        envp2 = __vte_pty_merge_environ (envv);

        _VTE_DEBUG_IF (VTE_DEBUG_MISC) {
                g_printerr ("Spawing command:\n");
                for (i = 0; argv[i] != NULL; i++) {
                        g_printerr ("    argv[%d] = %s\n", i, argv[i]);
                }
                for (i = 0; envp2[i] != NULL; i++) {
                        g_printerr ("    env[%d] = %s\n", i, envp2[i]);
                }
                g_printerr ("    directory: %s\n",
                            directory ? directory : "(none)");
        }

        ret = g_spawn_async_with_pipes(directory,
                                       argv, envp2,
                                       spawn_flags,
                                       child_setup ? child_setup
                                                   : (GSpawnChildSetupFunc) vte_pty_child_setup,
                                       child_setup ? child_setup_data : pty,
                                       child_pid,
                                       NULL, NULL, NULL,
                                       &err);
        if (!ret &&
            directory != NULL &&
            g_error_matches(err, G_SPAWN_ERROR, G_SPAWN_ERROR_CHDIR)) {
                /* try spawning in our working directory */
                g_clear_error(&err);
                ret = g_spawn_async_with_pipes(NULL,
                                               argv, envp2,
                                               spawn_flags,
                                               child_setup ? child_setup
                                                           : (GSpawnChildSetupFunc) vte_pty_child_setup,
                                               child_setup ? child_setup_data : pty,
                                               child_pid,
                                               NULL, NULL, NULL,
                                               &err);
        }

        g_strfreev (envp2);

        if (ret)
                return TRUE;

        g_propagate_error (error, err);
        return FALSE;
}

/*
 * __vte_pty_fork:
 * @pty: a #VtePty
 * @pid: (out) a location to store a #GPid, or %NULL
 * @error: a location to store a #GError, or %NULL
 *
 * Forks and calls vte_pty_child_setup() in the child.
 *
 * Returns: %TRUE on success, or %FALSE on failure with @error filled in
 */
gboolean
__vte_pty_fork(VtePty *pty,
               GPid *pid,
               GError **error)
{
#ifdef HAVE_FORK
        gboolean ret = TRUE;

        *pid = fork();
        switch (*pid) {
                case -1:
                        g_set_error(error,
                                    G_SPAWN_ERROR,
                                    G_SPAWN_ERROR_FAILED,
                                    "Unable to fork: %s",
                                    g_strerror(errno));
                        ret = FALSE;
                case 0: /* child */
                        vte_pty_child_setup(pty);
                        break;
                default: /* parent */
                        break;
        }

	return ret;
#else /* !HAVE_FORK */
        g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                            "No fork implementation");
        return FALSE;
#endif /* HAVE_FORK */
}

/**
 * vte_pty_set_size:
 * @pty: a #VtePty
 * @rows: the desired number of rows
 * @columns: the desired number of columns
 * @error: (allow-none); return location to store a #GError, or %NULL
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * If setting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 *
 * Since: 0.26
 */
gboolean
vte_pty_set_size(VtePty *pty,
                 int rows,
                 int columns,
                 GError **error)
{
        VtePtyPrivate *priv;
	struct winsize size;
        int master;
	int ret;

        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);

        priv = pty->priv;

        master = vte_pty_get_fd(pty);

	memset(&size, 0, sizeof(size));
	size.ws_row = rows > 0 ? rows : 24;
	size.ws_col = columns > 0 ? columns : 80;
	_vte_debug_print(VTE_DEBUG_PTY,
			"Setting size on fd %d to (%d,%d).\n",
			master, columns, rows);
	ret = ioctl(master, TIOCSWINSZ, &size);
	if (ret != 0) {
                int errsv = errno;

                g_set_error(error, G_IO_ERROR,
                            g_io_error_from_errno(errsv),
                            "Failed to set window size: %s",
                            g_strerror(errsv));

		_vte_debug_print(VTE_DEBUG_PTY,
				"Failed to set size on %d: %s.\n",
				master, g_strerror(errsv));

                errno = errsv;

                return FALSE;
	}

        return TRUE;
}

/**
 * vte_pty_get_size:
 * @pty: a #VtePty
 * @rows: (out) (allow-none): a location to store the number of rows, or %NULL
 * @columns: (out) (allow-none): a location to store the number of columns, or %NULL
 * @error: return location to store a #GError, or %NULL
 *
 * Reads the pseudo terminal's window size.
 *
 * If getting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 *
 * Since: 0.26
 */
gboolean
vte_pty_get_size(VtePty *pty,
                 int *rows,
                 int *columns,
                 GError **error)
{
        VtePtyPrivate *priv;
	struct winsize size;
        int master;
	int ret;

        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);

        priv = pty->priv;

        master = vte_pty_get_fd(pty);

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
                return TRUE;
	} else {
                int errsv = errno;

                g_set_error(error, G_IO_ERROR,
                            g_io_error_from_errno(errsv),
                            "Failed to get window size: %s",
                            g_strerror(errsv));

		_vte_debug_print(VTE_DEBUG_PTY,
				"Failed to read size from fd %d: %s\n",
				master, g_strerror(errsv));

                errno = errsv;

                return FALSE;
	}
}

/*
 * _vte_pty_ptsname:
 * @master: file descriptor to the PTY master
 * @error: a location to store a #GError, or %NULL
 *
 * Returns: a newly allocated string containing the file name of the
 *   PTY slave device, or %NULL on failure with @error filled in
 */
static char *
_vte_pty_ptsname(int master,
                 GError **error)
{
#if defined(HAVE_PTSNAME_R)
	gsize len = 1024;
	char *buf = NULL;
	int i, errsv;
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
                        errsv = errno;
			g_free(buf);
                        errno = errsv;
			buf = NULL;
			break;
		}
		len *= 2;
	} while ((i != 0) && (errno == ERANGE));

        g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                    "%s failed: %s", "ptsname_r", g_strerror(errno));
        return NULL;
#elif defined(HAVE_PTSNAME)
	char *p;
	if ((p = ptsname(master)) != NULL) {
		_vte_debug_print(VTE_DEBUG_PTY, "PTY slave is `%s'.\n", p);
		return g_strdup(p);
	}

        g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                    "%s failed: %s", "ptsname", g_strerror(errno));
        return NULL;
#elif defined(TIOCGPTN)
	int pty = 0;
	if (ioctl(master, TIOCGPTN, &pty) == 0) {
		_vte_debug_print(VTE_DEBUG_PTY,
				"PTY slave is `/dev/pts/%d'.\n", pty);
		return g_strdup_printf("/dev/pts/%d", pty);
	}

        g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                    "%s failed: %s", "ioctl(TIOCGPTN)", g_strerror(errno));
        return NULL;
#else
#error no ptsname implementation for this platform
#endif
}

/*
 * _vte_pty_getpt:
 * @error: a location to store a #GError, or %NULL
 *
 * Opens a file descriptor for the next available PTY master.
 * Sets the descriptor to blocking mode!
 *
 * Returns: a new file descriptor, or %-1 on failure
 */
static int
_vte_pty_getpt(GError **error)
{
	int fd, flags, rv;
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
        if (fd == -1) {
                g_set_error (error, VTE_PTY_ERROR,
                             VTE_PTY_ERROR_PTY98_FAILED,
                             "%s failed: %s", "getpt", g_strerror(errno));
                return -1;
        }

        rv = fcntl(fd, F_GETFL, 0);
        if (rv < 0) {
                int errsv = errno;
                g_set_error(error, VTE_PTY_ERROR,
                            VTE_PTY_ERROR_PTY98_FAILED,
                            "%s failed: %s", "fcntl(F_GETFL)", g_strerror(errno));
                close(fd);
                errno = errsv;
                return -1;
        }

	/* Set it to blocking. */
        /* FIXMEchpe: why?? vte_terminal_set_pty does the inverse... */
        flags = rv & ~(O_NONBLOCK);
        rv = fcntl(fd, F_SETFL, flags);
        if (rv < 0) {
                int errsv = errno;
                g_set_error(error, VTE_PTY_ERROR,
                            VTE_PTY_ERROR_PTY98_FAILED,
                            "%s failed: %s", "fcntl(F_SETFL)", g_strerror(errno));
                close(fd);
                errno = errsv;
                return -1;
        }

	return fd;
}

static gboolean
_vte_pty_grantpt(int master,
                 GError **error)
{
#ifdef HAVE_GRANTPT
        int rv;

        rv = grantpt(master);
        if (rv != 0) {
                int errsv = errno;
                g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                            "%s failed: %s", "grantpt", g_strerror(errsv));
                errno = errsv;
                return FALSE;
        }
#endif
        return TRUE;
}

static gboolean
_vte_pty_unlockpt(int fd,
                  GError **error)
{
        int rv;
#ifdef HAVE_UNLOCKPT
	rv = unlockpt(fd);
        if (rv != 0) {
                int errsv = errno;
                g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                            "%s failed: %s", "unlockpt", g_strerror(errsv));
                errno = errsv;
                return FALSE;
        }
        return TRUE;
#elif defined(TIOCSPTLCK)
	int zero = 0;
	rv = ioctl(fd, TIOCSPTLCK, &zero);
        if (rv != 0) {
                int errsv = errno;
                g_set_error(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY98_FAILED,
                            "%s failed: %s", "ioctl(TIOCSPTLCK)", g_strerror(errsv));
                errno = errsv;
                return FALSE;
        }
        return TRUE;
#else
#error no unlockpt implementation for this platform
#endif
}

/*
 * _vte_pty_open_unix98:
 * @pty: a #VtePty
 * @error: a location to store a #GError, or %NULL
 *
 * Opens a new file descriptor to a new PTY master.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
static gboolean
_vte_pty_open_unix98(VtePty *pty,
                     GError **error)
{
        VtePtyPrivate *priv = pty->priv;
	int fd;
	char *buf;

	/* Attempt to open the master. */
	fd = _vte_pty_getpt(error);
	if (fd == -1)
                return FALSE;

	_vte_debug_print(VTE_DEBUG_PTY, "Allocated pty on fd %d.\n", fd);

        /* Read the slave number and unlock it. */
        if ((buf = _vte_pty_ptsname(fd, error)) == NULL ||
            !_vte_pty_grantpt(fd, error) ||
            !_vte_pty_unlockpt(fd, error)) {
                int errsv = errno;
                _vte_debug_print(VTE_DEBUG_PTY,
                                "PTY setup failed, bailing.\n");
                close(fd);
                errno = errsv;
                return FALSE;
        }

        priv->pty_fd = fd;
        priv->child_setup_data.mode = TTY_OPEN_BY_NAME;
        priv->child_setup_data.tty.name = buf;
        priv->using_helper = FALSE;

        return TRUE;
}

#ifdef VTE_USE_GNOME_PTY_HELPER
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

/*
 * _vte_pty_stop_helper:
 *
 * Terminates the running GNOME PTY helper.
 */
static void
_vte_pty_stop_helper(void)
{
	if (_vte_pty_helper_started) {
		close(_vte_pty_helper_tunnel);
		_vte_pty_helper_tunnel = -1;
		kill(_vte_pty_helper_pid, SIGTERM);
		_vte_pty_helper_pid = -1;
		_vte_pty_helper_started = FALSE;
	}
}

/*
 * _vte_pty_start_helper:
 * @error: a location to store a #GError, or %NULL
 *
 * Starts the GNOME PTY helper process, if it is not already running.
 *
 * Returns: %TRUE if the helper was already started, or starting it succeeded,
 *   %FALSE on failure with @error filled in
 */
static gboolean
_vte_pty_start_helper(GError **error)
{
	int i, errsv;
        int tunnel = -1;
        int tmp[2] = { -1, -1 };

        if (_vte_pty_helper_started)
                return TRUE;

	/* Create a communication link for use with the helper. */
	tmp[0] = open("/dev/null", O_RDONLY);
	if (tmp[0] == -1) {
		goto failure;
	}
	tmp[1] = open("/dev/null", O_RDONLY);
	if (tmp[1] == -1) {
		goto failure;
	}
	if (_vte_pty_pipe_open(&_vte_pty_helper_tunnel, &tunnel) != 0) {
		goto failure;
	}
	close(tmp[0]);
	close(tmp[1]);
        tmp[0] = tmp[1] = -1;

	/* Now fork and start the helper. */
	_vte_pty_helper_pid = fork();
	if (_vte_pty_helper_pid == -1) {
		goto failure;
	}
	if (_vte_pty_helper_pid == 0) {
		/* Child.  Close descriptors.  No need to close all,
		 * gnome-pty-helper does that anyway. */
		for (i = 0; i < 3; i++) {
			close(i);
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
	atexit(_vte_pty_stop_helper);

        _vte_pty_helper_started = TRUE;
	return TRUE;

failure:
        errsv = errno;

        g_set_error(error, VTE_PTY_ERROR,
                    VTE_PTY_ERROR_PTY_HELPER_FAILED,
                    "Failed to start gnome-pty-helper: %s",
                    g_strerror (errsv));

        if (tmp[0] != -1)
                close(tmp[0]);
        if (tmp[1] != -1)
                close(tmp[1]);
        if (tunnel != -1)
                close(tunnel);
        if (_vte_pty_helper_tunnel != -1)
                close(_vte_pty_helper_tunnel);

        _vte_pty_helper_pid = -1;
        _vte_pty_helper_tunnel = -1;

        errno = errsv;
        return FALSE;
}

/*
 * _vte_pty_helper_ops_from_flags:
 * @flags: flags from #VtePtyFlags
 *
 * Translates @flags into the corresponding op code for the
 * GNOME PTY helper.
 *
 * Returns: the #GnomePtyOps corresponding to @flags
 */
static int
_vte_pty_helper_ops_from_flags (VtePtyFlags flags)
{
	int op = 0;
	static const int opmap[8] = {
		GNOME_PTY_OPEN_NO_DB_UPDATE,		/* 0 0 0 */
		GNOME_PTY_OPEN_PTY_LASTLOG,		/* 0 0 1 */
		GNOME_PTY_OPEN_PTY_UTMP,		/* 0 1 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGUTMP,		/* 0 1 1 */
		GNOME_PTY_OPEN_PTY_WTMP,		/* 1 0 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGWTMP,		/* 1 0 1 */
		GNOME_PTY_OPEN_PTY_UWTMP,		/* 1 1 0 */
		GNOME_PTY_OPEN_PTY_LASTLOGUWTMP,	/* 1 1 1 */
	};
	if ((flags & VTE_PTY_NO_LASTLOG) == 0) {
		op += 1;
	}
	if ((flags & VTE_PTY_NO_UTMP) == 0) {
		op += 2;
	}
	if ((flags & VTE_PTY_NO_WTMP) == 0) {
		op += 4;
	}
	g_assert(op >= 0 && op < (int) G_N_ELEMENTS(opmap));

        return opmap[op];
}

/*
 * _vte_pty_open_with_helper:
 * @pty: a #VtePty
 * @error: a location to store a #GError, or %NULL
 *
 * Opens a new file descriptor to a new PTY master using the
 * GNOME PTY helper.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
static gboolean
_vte_pty_open_with_helper(VtePty *pty,
                          GError **error)
{
        VtePtyPrivate *priv = pty->priv;
	GnomePtyOps ops;
	int ret;
	int parentfd = -1, childfd = -1;
	gpointer tag;

	/* We have to use the pty helper here. */
	if (!_vte_pty_start_helper(error))
                return FALSE;

	/* Try to open a new descriptor. */

        ops = _vte_pty_helper_ops_from_flags(priv->flags);
        /* Send our request. */
        if (n_write(_vte_pty_helper_tunnel,
                    &ops, sizeof(ops)) != sizeof(ops)) {
                g_set_error (error, VTE_PTY_ERROR,
                              VTE_PTY_ERROR_PTY_HELPER_FAILED,
                              "Failed to send request to gnome-pty-helper: %s",
                              g_strerror(errno));
                return FALSE;
        }
        _vte_debug_print(VTE_DEBUG_PTY, "Sent request to helper.\n");
        /* Read back the response. */
        if (n_read(_vte_pty_helper_tunnel,
                    &ret, sizeof(ret)) != sizeof(ret)) {
                g_set_error (error, VTE_PTY_ERROR,
                              VTE_PTY_ERROR_PTY_HELPER_FAILED,
                              "Failed to read response from gnome-pty-helper: %s",
                              g_strerror(errno));
                return FALSE;
        }
        _vte_debug_print(VTE_DEBUG_PTY,
                        "Received response from helper.\n");
        if (ret == 0) {
                g_set_error_literal (error, VTE_PTY_ERROR,
                                      VTE_PTY_ERROR_PTY_HELPER_FAILED,
                                      "gnome-pty-helper failed to open pty");
                return FALSE;
        }
        _vte_debug_print(VTE_DEBUG_PTY, "Helper returns success.\n");
        /* Read back a tag. */
        if (n_read(_vte_pty_helper_tunnel,
                    &tag, sizeof(tag)) != sizeof(tag)) {
                g_set_error (error, VTE_PTY_ERROR,
                              VTE_PTY_ERROR_PTY_HELPER_FAILED,
                              "Failed to read tag from gnome-pty-helper: %s",
                              g_strerror(errno));
                return FALSE;
        }
        _vte_debug_print(VTE_DEBUG_PTY, "Tag = %p.\n", tag);
        /* Receive the master and slave ptys. */
        _vte_pty_read_ptypair(_vte_pty_helper_tunnel,
                              &parentfd, &childfd);

        if ((parentfd == -1) || (childfd == -1)) {
                int errsv = errno;

                close(parentfd);
                close(childfd);

                g_set_error (error, VTE_PTY_ERROR,
                              VTE_PTY_ERROR_PTY_HELPER_FAILED,
                              "Failed to read master or slave pty from gnome-pty-helper: %s",
                              g_strerror(errsv));
                errno = errsv;
                return FALSE;
        }

        _vte_debug_print(VTE_DEBUG_PTY,
                        "Got master pty %d and slave pty %d.\n",
                        parentfd, childfd);

        priv->using_helper = TRUE;
        priv->helper_tag = tag;
        priv->pty_fd = parentfd;

        priv->child_setup_data.mode = TTY_OPEN_BY_FD;
        priv->child_setup_data.tty.fd = childfd;

        return TRUE;
}

#endif /* VTE_USE_GNOME_PTY_HELPER */

/**
 * vte_pty_set_utf8:
 * @pty: a #VtePty
 * @utf8: whether or not the pty is in UTF-8 mode
 * @error: (allow-none): return location to store a #GError, or %NULL
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 *
 * Since: 0.26
 */
gboolean
vte_pty_set_utf8(VtePty *pty,
                 gboolean utf8,
                 GError **error)
{
#if defined(HAVE_TCSETATTR) && defined(IUTF8)
        VtePtyPrivate *priv;
	struct termios tio;
	tcflag_t saved_cflag;

        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);

        priv = pty->priv;
        g_return_val_if_fail (priv->pty_fd > 0, FALSE);

        if (tcgetattr(priv->pty_fd, &tio) == -1) {
                int errsv = errno;
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "%s failed: %s", "tcgetattr", g_strerror(errsv));
                errno = errsv;
                return FALSE;
        }

        saved_cflag = tio.c_iflag;
        if (utf8) {
                tio.c_iflag |= IUTF8;
        } else {
              tio.c_iflag &= ~IUTF8;
        }

        /* Only set the flag if it changes */
        if (saved_cflag != tio.c_iflag &&
            tcsetattr(priv->pty_fd, TCSANOW, &tio) == -1) {
                int errsv = errno;
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "%s failed: %s", "tcgetattr", g_strerror(errsv));
                errno = errsv;
                return FALSE;
	}
#endif

        return TRUE;
}

/**
 * vte_pty_close:
 * @pty: a #VtePty
 *
 * Cleans up the PTY, specifically any logging performed for the session.
 * The file descriptor to the PTY master remains open.
 *
 * Since: 0.26
 */
void
vte_pty_close (VtePty *pty)
{
#ifdef VTE_USE_GNOME_PTY_HELPER
        VtePtyPrivate *priv = pty->priv;
	gpointer tag;
	GnomePtyOps ops;

        if (!priv->using_helper)
                return;

        /* Signal the helper that it needs to close its connection. */
        tag = priv->helper_tag;

        ops = GNOME_PTY_CLOSE_PTY;
        if (n_write(_vte_pty_helper_tunnel,
                    &ops, sizeof(ops)) != sizeof(ops)) {
                return;
        }
        if (n_write(_vte_pty_helper_tunnel,
                    &tag, sizeof(tag)) != sizeof(tag)) {
                return;
        }

        ops = GNOME_PTY_SYNCH;
        if (n_write(_vte_pty_helper_tunnel,
                    &ops, sizeof(ops)) != sizeof(ops)) {
                return;
        }
        n_read(_vte_pty_helper_tunnel, &ops, 1);

        priv->helper_tag = NULL;
        priv->using_helper = FALSE;
#endif
}

/* VTE PTY class */

enum {
        PROP_0,
        PROP_FLAGS,
        PROP_FD,
        PROP_TERM
};

/* GInitable impl */

static gboolean
vte_pty_initable_init (GInitable *initable,
                       GCancellable *cancellable,
                       GError **error)
{
        VtePty *pty = VTE_PTY (initable);
        VtePtyPrivate *priv = pty->priv;
        gboolean ret = FALSE;

        if (cancellable != NULL) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                    "Cancellable initialisation not supported");
                return FALSE;
        }

        /* If we already have a (foreign) FD, we're done. */
        if (priv->foreign) {
                g_assert(priv->pty_fd != -1);
                return TRUE;
        }

#ifdef VTE_USE_GNOME_PTY_HELPER
	if ((priv->flags & VTE_PTY_NO_HELPER) == 0) {
                GError *err = NULL;

		ret = _vte_pty_open_with_helper(pty, &err);
                g_assert(ret || err != NULL);

                if (ret)
                        goto out;

                _vte_debug_print(VTE_DEBUG_PTY,
                                 "_vte_pty_open_with_helper failed: %s\n",
                                 err->message);

                /* Only do fallback if gnome-pty-helper failed! */
                if ((priv->flags & VTE_PTY_NO_FALLBACK) ||
                    !g_error_matches(err,
                                     VTE_PTY_ERROR,
                                     VTE_PTY_ERROR_PTY_HELPER_FAILED)) {
                        g_propagate_error (error, err);
                        goto out;
                }

                g_error_free(err);
                /* Fall back to unix98 PTY */
        }
#else
        if (priv->flags & VTE_PTY_NO_FALLBACK) {
                g_set_error_literal(error, VTE_PTY_ERROR, VTE_PTY_ERROR_PTY_HELPER_FAILED,
                                    "VTE compiled without GNOME PTY helper");
                goto out;
        }
#endif /* VTE_USE_GNOME_PTY_HELPER */

        ret = _vte_pty_open_unix98(pty, error);

  out:
	_vte_debug_print(VTE_DEBUG_PTY,
			"vte_pty_initable_init returning %s with ptyfd = %d\n",
			ret ? "TRUE" : "FALSE", priv->pty_fd);

	return ret;
}

static void
vte_pty_initable_iface_init (GInitableIface  *iface)
{
        iface->init = vte_pty_initable_init;
}

/* GObjectClass impl */

G_DEFINE_TYPE_WITH_CODE (VtePty, vte_pty, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, vte_pty_initable_iface_init))

static void
vte_pty_init (VtePty *pty)
{
        VtePtyPrivate *priv;

        priv = pty->priv = G_TYPE_INSTANCE_GET_PRIVATE (pty, VTE_TYPE_PTY, VtePtyPrivate);

        priv->flags = VTE_PTY_DEFAULT;
        priv->pty_fd = -1;
        priv->foreign = FALSE;
        priv->using_helper = FALSE;
        priv->helper_tag = NULL;
        priv->term = vte_terminal_get_default_emulation(NULL /* that's ok, this function is just retarded */); /* already interned */
}

static void
vte_pty_finalize (GObject *object)
{
        VtePty *pty = VTE_PTY (object);
        VtePtyPrivate *priv = pty->priv;

        if (priv->child_setup_data.mode == TTY_OPEN_BY_FD &&
            priv->child_setup_data.tty.fd != -1) {
                /* Close the child FD */
                close(priv->child_setup_data.tty.fd);
        }

        vte_pty_close(pty);

        /* Close the master FD */
        if (priv->pty_fd != -1) {
                close(priv->pty_fd);
        }

        G_OBJECT_CLASS (vte_pty_parent_class)->finalize (object);
}

static void
vte_pty_get_property (GObject    *object,
                       guint       property_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
        VtePty *pty = VTE_PTY (object);
        VtePtyPrivate *priv = pty->priv;

        switch (property_id) {
        case PROP_FLAGS:
                g_value_set_flags(value, priv->flags);
                break;

        case PROP_FD:
                g_value_set_int(value, vte_pty_get_fd(pty));
                break;

        case PROP_TERM:
                g_value_set_string(value, priv->term);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        }
}

static void
vte_pty_set_property (GObject      *object,
                       guint         property_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
        VtePty *pty = VTE_PTY (object);
        VtePtyPrivate *priv = pty->priv;

        switch (property_id) {
        case PROP_FLAGS:
                priv->flags = g_value_get_flags(value);
                break;

        case PROP_FD:
                priv->pty_fd = g_value_get_int(value);
                priv->foreign = (priv->pty_fd != -1);
                break;

        case PROP_TERM:
                vte_pty_set_term(pty, g_value_get_string(value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
vte_pty_class_init (VtePtyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        g_type_class_add_private(object_class, sizeof(VtePtyPrivate));

        object_class->set_property = vte_pty_set_property;
        object_class->get_property = vte_pty_get_property;
        object_class->finalize     = vte_pty_finalize;

        /**
         * VtePty:flags:
         *
         * Controls how the session is recorded in lastlog, utmp, and wtmp,
         * and whether to use the GNOME PTY helper.
         *
         * Since: 0.26
         */
        g_object_class_install_property
                (object_class,
                 PROP_FLAGS,
                 g_param_spec_flags ("flags", NULL, NULL,
                                     VTE_TYPE_PTY_FLAGS,
                                     VTE_PTY_DEFAULT,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS));

        /**
         * VtePty:fd:
         *
         * The file descriptor of the PTY master.
         *
         * Since: 0.26
         */
        g_object_class_install_property
                (object_class,
                 PROP_FD,
                 g_param_spec_int ("fd", NULL, NULL,
                                   -1, G_MAXINT, -1,
                                   G_PARAM_READWRITE |
                                   G_PARAM_CONSTRUCT_ONLY |
                                   G_PARAM_STATIC_STRINGS));

        /**
         * VtePty:term:
         *
         * The value to set for the TERM environment variable
         * in vte_pty_child_setup().
         *
         * Since: 0.26
         */
        g_object_class_install_property
                (object_class,
                 PROP_TERM,
                 g_param_spec_string ("term", NULL, NULL,
                                      "xterm",
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_STRINGS));
}

/* public API */

/**
 * vte_pty_error_quark:
 *
 * Error domain for VTE PTY errors. Errors in this domain will be from the #VtePtyError
 * enumeration. See #GError for more information on error domains.
 *
 * Returns: the error domain for VTE PTY errors
 *
 * Since: 0.26
 */
GQuark
vte_pty_error_quark(void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string("vte-pty-error");

  return quark;
}

/**
 * vte_pty_new:
 * @flags: flags from #VtePtyFlags
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Allocates a new pseudo-terminal.
 *
 * You can later use fork() or the g_spawn_async() familty of functions
 * to start a process on the PTY.
 *
 * If using fork(), you MUST call vte_pty_child_setup() in the child.
 *
 * If using g_spawn_async() and friends, you MUST either use
 * vte_pty_child_setup () directly as the child setup function, or call
 * vte_pty_child_setup() from your own child setup function supplied.
 *
 * Also, you MUST pass the %G_SPAWN_DO_NOT_REAP_CHILD flag.
 *
 * When using the GNOME PTY Helper,
 * unless some of the %VTE_PTY_NO_LASTLOG, %VTE_PTY_NO_UTMP or
 * %VTE_PTY_NO_WTMP flags are passed in @flags, the
 * session is logged in the corresponding lastlog, utmp or wtmp
 * system files.
 *
 * When passing %VTE_PTY_NO_HELPER in @flags, the
 * GNOME PTY Helper bypassed entirely.
 *
 * When passing %VTE_PTY_NO_FALLBACK in @flags,
 * and opening a PTY using the PTY helper fails, there will
 * be no fallback to allocate a PTY using Unix98 PTY functions.
 *
 * Returns: (transfer full): a new #VtePty, or %NULL on error with @error filled in
 *
 * Since: 0.26
 */
VtePty *
vte_pty_new (VtePtyFlags flags,
             GError **error)
{
        return g_initable_new (VTE_TYPE_PTY,
                               NULL /* cancellable */,
                               error,
                               "flags", flags,
                               NULL);
}

/**
 * vte_pty_new_foreign:
 * @fd: (transfer full): a file descriptor to the PTY
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Creates a new #VtePty for the PTY master @fd.
 *
 * No entry will be made in the lastlog, utmp or wtmp system files.
 *
 * Note that the newly created #VtePty will take ownership of @fd
 * and close it on finalize.
 *
 * Returns: (transfer full): a new #VtePty for @fd, or %NULL on error with @error filled in
 *
 * Since: 0.26
 */
VtePty *
vte_pty_new_foreign (int fd,
                     GError **error)
{
        g_return_val_if_fail(fd >= 0, NULL);

        return g_initable_new (VTE_TYPE_PTY,
                               NULL /* cancellable */,
                               error,
                               "fd", fd,
                               NULL);
}

/**
 * vte_pty_get_fd:
 * @pty: a #VtePty
 *
 * Returns: (transfer none): the file descriptor of the PTY master in @pty. The
 *   file descriptor belongs to @pty and must not be closed
 */
int
vte_pty_get_fd (VtePty *pty)
{
        VtePtyPrivate *priv;

        g_return_val_if_fail(VTE_IS_PTY(pty), -1);

        priv = pty->priv;
        g_return_val_if_fail(priv->pty_fd != -1, -1);

        return priv->pty_fd;
}

/**
 * vte_pty_set_term:
 * @pty: a #VtePty
 * @emulation: (allow-none): the name of a terminal description, or %NULL
 *
 * Sets what value of the TERM environment variable to set
 * when using vte_pty_child_setup().
 *
 * Note: When using fork() and execve(), or the g_spawn_async() family of
 * functions with vte_pty_child_setup(),
 * and the environment passed to them contains the <literal>TERM</literal>
 * environment variable, that value will override the one set here.
 *
 * Since: 0.26
 */
void
vte_pty_set_term (VtePty *pty,
                  const char *emulation)
{
        VtePtyPrivate *priv;

        g_return_if_fail(VTE_IS_PTY(pty));
        g_return_if_fail(emulation != NULL);

        priv = pty->priv;
        emulation = g_intern_string(emulation);
        if (emulation == priv->term)
                return;

        priv->term = emulation;
        g_object_notify(G_OBJECT(pty), "term");
}

/* Reimplementation of the ugly deprecated APIs _vte_pty_*() */

#ifndef VTE_DISABLE_DEPRECATED_SOURCE

static GHashTable *fd_to_pty_hash = NULL;

static VtePty *
get_vte_pty_for_fd (int fd)
{
        VtePty *pty;

        if (fd_to_pty_hash != NULL &&
            (pty = g_hash_table_lookup(fd_to_pty_hash, &fd)) != NULL)
                return pty;

        g_warning("No VtePty found for fd %d!\n", fd);
        return NULL;
}

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
 *
 * Deprecated: 0.26: Use #VtePty together with fork() or the g_spawn_async() family of functions instead
 */
int
_vte_pty_open(pid_t *child,
              char **env_add,
              const char *command,
              char **argv,
              const char *directory,
              int columns,
              int rows,
              gboolean lastlog,
              gboolean utmp,
              gboolean wtmp)
{
        VtePty *pty;
        GPid pid;
        gboolean ret;

        pty = vte_pty_new(__vte_pty_get_pty_flags (lastlog, utmp, wtmp), NULL);
        if (pty == NULL)
                return -1;

        if (command != NULL) {
                char **real_argv;
                GSpawnFlags spawn_flags;

                spawn_flags = G_SPAWN_CHILD_INHERITS_STDIN |
                              G_SPAWN_SEARCH_PATH;
                real_argv = __vte_pty_get_argv(command, argv, &spawn_flags);
                ret = __vte_pty_spawn(pty,
                                      directory,
                                      real_argv,
                                      env_add,
                                      spawn_flags,
                                      NULL, NULL,
                                      &pid,
                                      NULL);
                g_strfreev(real_argv);
        } else {
                ret = __vte_pty_fork(pty, &pid, NULL);
        }

        if (!ret) {
                g_object_unref(pty);
                return -1;
        }

        vte_pty_set_size(pty, rows, columns, NULL);

        /* Stash the pty in the hash so we can later retrieve it by FD */
        if (fd_to_pty_hash == NULL) {
                fd_to_pty_hash = g_hash_table_new_full(g_int_hash,
                                                       g_int_equal,
                                                       NULL,
                                                       (GDestroyNotify) g_object_unref);
        }

        g_hash_table_insert(fd_to_pty_hash, &pty->priv->pty_fd, pty /* adopt refcount */);

        if (child)
                *child = (pid_t) pid;

        return vte_pty_get_fd(pty);
}

/**
 * _vte_pty_get_size:
 * @master: the file descriptor of the PTY master
 * @columns: a place to store the number of columns
 * @rows: a place to store the number of rows
 *
 * Attempts to read the pseudo terminal's window size.
 *
 * Returns: 0 on success, -1 on failure.
 *
 * Deprecated: 0.26: Use #VtePty and vte_pty_get_size() instead
 */
int
_vte_pty_get_size(int master,
                  int *columns,
                  int *rows)
{
        VtePty *pty;

        if ((pty = get_vte_pty_for_fd(master)) == NULL)
                return -1;

        if (vte_pty_get_size(pty, rows, columns, NULL))
                return 0;

        return -1;
}

/**
 * _vte_pty_set_size:
 * @master: the file descriptor of the PTY master
 * @columns: the desired number of columns
 * @rows: the desired number of rows
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * Returns: 0 on success, -1 on failure.
 *
 * Deprecated: 0.26: Use #VtePty and vte_pty_set_size() instead
 */
int
_vte_pty_set_size(int master,
                  int columns,
                  int rows)
{
        VtePty *pty;

        if ((pty = get_vte_pty_for_fd(master)) == NULL)
                return -1;

        if (vte_pty_set_size(pty, rows, columns, NULL))
                return 0;

        return -1;
}

/**
 * _vte_pty_set_utf8:
 * @pty: The pty master descriptor.
 * @utf8: Whether or not the pty is in UTF-8 mode.
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 *
 * Deprecated: 0.26: Use #VtePty and vte_pty_set_utf8() instead
 */
void _vte_pty_set_utf8(int master,
                       gboolean utf8)
{
        VtePty *pty;

        if ((pty = get_vte_pty_for_fd(master)) == NULL)
                return;

        vte_pty_set_utf8(pty, utf8, NULL);
}

/**
 * _vte_pty_close:
 * @pty: the pty master descriptor.
 *
 * Cleans up the PTY associated with the descriptor, specifically any logging
 * performed for the session.  The descriptor itself remains open.
 *
 * Deprecated: 0.26: Use #VtePty and vte_pty_close() instead
 */
void _vte_pty_close(int master)
{
        VtePty *pty;

        if ((pty = get_vte_pty_for_fd(master)) == NULL)
                return;

        /* Prevent closing the FD */
        pty->priv->pty_fd = -1;

        g_hash_table_remove(fd_to_pty_hash, &master);

        if (g_hash_table_size(fd_to_pty_hash) == 0) {
                g_hash_table_destroy(fd_to_pty_hash);
                fd_to_pty_hash = NULL;
        }
}

#endif /* !VTE_DISABLE_DEPRECATED_SOURCE */
