/*
 * Copyright (C) 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010, 2019 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION: vte-pty
 * @short_description: Functions for starting a new process on a new pseudo-terminal and for
 * manipulating pseudo-terminals
 *
 * The terminal widget uses these functions to start commands with new controlling
 * pseudo-terminals and to resize pseudo-terminals.
 */

#include <config.h>

#include "pty.hh"

#include <vte/vte.h>
#include "vteptyinternal.hh"
#include "vtetypes.hh"
#include "vtespawn.hh"

#include <assert.h>
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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <unistd.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#if defined(__sun) && defined(HAVE_STROPTS_H)
#include <stropts.h>
#endif
#include <glib.h>
#include <gio/gio.h>
#include "debug.h"

#include <glib/gi18n-lib.h>

#include "glib-glue.hh"

#ifdef WITH_SYSTEMD
#include "systemd.hh"
#endif

/* NSIG isn't in POSIX, so if it doesn't exist use this here. See bug #759196 */
#ifndef NSIG
#define NSIG (8 * sizeof(sigset_t))
#endif

#define VTE_VERSION_NUMERIC ((VTE_MAJOR_VERSION) * 10000 + (VTE_MINOR_VERSION) * 100 + (VTE_MICRO_VERSION))

#define VTE_TERMINFO_NAME "xterm-256color"

namespace vte::base {

Pty*
Pty::ref() noexcept
{
        g_atomic_int_inc(&m_refcount);
        return this;
}

void
Pty::unref() noexcept
{
        if (g_atomic_int_dec_and_test(&m_refcount))
                delete this;
}

void
Pty::child_setup() const noexcept
{
        /* Unblock all signals */
        sigset_t set;
        sigemptyset(&set);
        if (pthread_sigmask(SIG_SETMASK, &set, nullptr) == -1) {
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "pthread_sigmask");
                _exit(127);
        }

        /* Reset the handlers for all signals to their defaults.  The parent
         * (or one of the libraries it links to) may have changed one to be ignored. */
        for (int n = 1; n < NSIG; n++) {
                if (n == SIGSTOP || n == SIGKILL)
                        continue;

                signal(n, SIG_DFL);
        }

        auto masterfd = fd();
        if (masterfd == -1)
                _exit(127);

        if (grantpt(masterfd) != 0) {
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "grantpt");
                _exit(127);
        }

	if (unlockpt(masterfd) != 0) {
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "unlockpt");
                _exit(127);
        }

        if (!(m_flags & VTE_PTY_NO_SESSION)) {
                /* This starts a new session; we become its process-group leader,
                 * and lose our controlling TTY.
                 */
                _vte_debug_print (VTE_DEBUG_PTY, "Starting new session\n");
                if (setsid() == -1) {
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "setsid");
                        _exit(127);
                }
        }
        /* FIXME? else if (m_flags & VTE_PTY_NO_CTTTY)
         * No session and no controlling TTY wanted, do we need to lose our controlling TTY,
         * perhaps by open("/dev/tty") + ioctl(TIOCNOTTY) ?
         */

        /* Now open the PTY peer. Note that this also makes the PTY our controlling TTY. */
        /* Note: *not* O_CLOEXEC! */
        auto const fd_flags = int{O_RDWR | ((m_flags & VTE_PTY_NO_CTTY) ? O_NOCTTY : 0)};
        auto fd = int{-1};

#ifdef __linux__
        fd = ioctl(masterfd, TIOCGPTPEER, fd_flags);
        /* Note: According to the kernel's own tests (tools/testing/selftests/filesystems/devpts_pts.c),
         * the error returned when the running kernel does not support this ioctl should be EINVAL.
         * However it appears that the actual error returned is ENOTTY. So we check for both of them.
         * See issue#182.
         */
        if (fd == -1 &&
            errno != EINVAL &&
            errno != ENOTTY) {
		_vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "ioctl(TIOCGPTPEER)");
		_exit(127);
        }

        /* Fall back to ptsname + open */
#endif

        if (fd == -1) {
                auto const name = ptsname(masterfd);
                if (name == nullptr) {
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "ptsname");
                        _exit(127);
                }

                _vte_debug_print (VTE_DEBUG_PTY,
                                  "Setting up child pty: master FD = %d name = %s\n",
                                  masterfd, name);

                fd = ::open(name, fd_flags);
                if (fd == -1) {
                        _vte_debug_print (VTE_DEBUG_PTY, "Failed to open PTY: %m\n");
                        _exit(127);
                }
        }

        assert(fd != -1);

#ifdef TIOCSCTTY
        /* On linux, opening the PTY peer above already made it our controlling TTY (since
         * previously there was none, after the setsid() call). However, it appears that e.g.
         * on *BSD, that doesn't happen, so we need this explicit ioctl here.
         */
        if (!(m_flags & VTE_PTY_NO_CTTY)) {
                if (ioctl(fd, TIOCSCTTY, fd) != 0) {
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "ioctl(TIOCSCTTY)");
                        _exit(127);
                }
        }
#endif

#if defined(__sun) && defined(HAVE_STROPTS_H)
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

	/* Close the original FD, unless it's one of the stdio descriptors */
	if (fd != STDIN_FILENO &&
			fd != STDOUT_FILENO &&
			fd != STDERR_FILENO) {
		close(fd);
	}

        /* Now set the TERM environment variable */
        /* FIXME: Setting environment here seems to have no effect, the merged envp2 will override on exec.
         * By the way, we'd need to set the one from there, if any. */
        g_setenv("TERM", VTE_TERMINFO_NAME, TRUE);

        char version[7];
        g_snprintf (version, sizeof (version), "%u", VTE_VERSION_NUMERIC);
        g_setenv ("VTE_VERSION", version, TRUE);

	/* Finally call an extra child setup */
	if (m_extra_child_setup.func) {
		m_extra_child_setup.func(m_extra_child_setup.data);
	}
}

/* TODO: clean up the spawning
 * - replace current env rather than adding!
 */

/*
 * __vte_pty_merge_environ:
 * @envp: environment vector
 * @inherit: whether to use the parent environment
 *
 * Merges @envp to the parent environment, and returns a new environment vector.
 *
 * Returns: a newly allocated string array. Free using g_strfreev()
 */
static gchar **
__vte_pty_merge_environ (char **envp,
                         const char *directory,
                         gboolean inherit)
{
	GHashTable *table;
        GHashTableIter iter;
        char *name, *value;
	gchar **parent_environ;
	GPtrArray *array;
	gint i;

	table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	if (inherit) {
		parent_environ = g_listenv ();
		for (i = 0; parent_environ[i] != NULL; i++) {
			g_hash_table_replace (table,
				              g_strdup (parent_environ[i]),
					      g_strdup (g_getenv (parent_environ[i])));
		}
		g_strfreev (parent_environ);
	}

        /* Make sure the one in envp overrides the default. */
        g_hash_table_replace (table, g_strdup ("TERM"), g_strdup (VTE_TERMINFO_NAME));

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

        g_hash_table_replace (table, g_strdup ("VTE_VERSION"), g_strdup_printf ("%u", VTE_VERSION_NUMERIC));

	/* Always set this ourself, not allowing replacing from envp */
	g_hash_table_replace(table, g_strdup("COLORTERM"), g_strdup("truecolor"));

        /* We need to put the working directory also in PWD, so that
         * e.g. bash starts in the right directory if @directory is a symlink.
         * See bug #502146 and #758452.
         */
        if (directory)
                g_hash_table_replace(table, g_strdup("PWD"), g_strdup(directory));

	array = g_ptr_array_sized_new (g_hash_table_size (table) + 1);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, (void**) &name, (void**) &value)) {
                g_ptr_array_add (array, g_strconcat (name, "=", value, nullptr));
        }
        g_assert(g_hash_table_size(table) == array->len);
	g_hash_table_destroy (table);
	g_ptr_array_add (array, NULL);

	return (gchar **) g_ptr_array_free (array, FALSE);
}

static void
pty_child_setup_cb(void* data)
{
        auto pty = reinterpret_cast<vte::base::Pty*>(data);
        pty->child_setup();
}

/*
 * Pty::spawn:
 * @directory: the name of a directory the command should start in, or %nullptr
 *   to use the cwd
 * @argv: child's argument vector
 * @envv: a list of environment variables to be added to the environment before
 *   starting the process, or %nullptr
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: function to run in the child just before exec()
 * @child_setup_data: user data for @child_setup
 * @child_pid: a location to store the child PID, or %nullptr
 * @timeout: a timeout value in ms, or %nullptr
 * @cancellable: a #GCancellable, or %nullptr
 * @error: return location for a #GError, or %nullptr
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
bool
Pty::spawn(char const* directory,
           char** argv,
           char** envv,
           GSpawnFlags spawn_flags_,
           GSpawnChildSetupFunc child_setup_func,
           gpointer child_setup_data,
           GPid* child_pid /* out */,
           int timeout,
           GCancellable* cancellable,
           GError** error) noexcept
{
        guint spawn_flags = (guint) spawn_flags_;
        bool ret{true};
        bool inherit_envv;
        char** envp2;
        int i;
        GPollFD pollfd;

#ifndef WITH_SYSTEMD
        if (spawn_flags & VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE) {
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                            "systemd not available");
                return false;
        }
#endif

        if (cancellable && !g_cancellable_make_pollfd(cancellable, &pollfd)) {
                vte::util::restore_errno errsv;
                g_set_error(error,
                            G_IO_ERROR,
                            g_io_error_from_errno(errsv),
                            "Failed to make cancellable pollfd: %s",
                            g_strerror(errsv));
                return false;
        }

        spawn_flags |= G_SPAWN_DO_NOT_REAP_CHILD;

        /* We do NOT support this flag. If you want to have some FD open in the child
         * process, simply use a child setup function that unsets the CLOEXEC flag
         * on that FD.
         */
        spawn_flags &= ~G_SPAWN_LEAVE_DESCRIPTORS_OPEN;

        inherit_envv = (spawn_flags & VTE_SPAWN_NO_PARENT_ENVV) == 0;
        spawn_flags &= ~VTE_SPAWN_NO_PARENT_ENVV;

        /* add the given environment to the childs */
        envp2 = __vte_pty_merge_environ (envv, directory, inherit_envv);

        _VTE_DEBUG_IF (VTE_DEBUG_MISC) {
                g_printerr ("Spawning command:\n");
                for (i = 0; argv[i] != NULL; i++) {
                        g_printerr ("    argv[%d] = %s\n", i, argv[i]);
                }
                for (i = 0; envp2[i] != NULL; i++) {
                        g_printerr ("    env[%d] = %s\n", i, envp2[i]);
                }
                g_printerr ("    directory: %s\n",
                            directory ? directory : "(none)");
        }

	m_extra_child_setup.func = child_setup_func;
	m_extra_child_setup.data = child_setup_data;

        auto pid = pid_t{-1};
        auto err = vte::glib::Error{};
        ret = vte_spawn_async_with_pipes_cancellable(directory,
                                                     argv, envp2,
                                                     (GSpawnFlags)spawn_flags,
                                                     (GSpawnChildSetupFunc)pty_child_setup_cb,
                                                     this,
                                                     &pid,
                                                     nullptr, nullptr, nullptr,
                                                     timeout,
                                                     cancellable ? &pollfd : nullptr,
                                                     err);
        if (!ret &&
            directory != nullptr &&
            err.matches(G_SPAWN_ERROR, G_SPAWN_ERROR_CHDIR)) {
                /* try spawning in our working directory */
                err.reset();
                ret = vte_spawn_async_with_pipes_cancellable(nullptr,
                                                             argv, envp2,
                                                             (GSpawnFlags)spawn_flags,
                                                             (GSpawnChildSetupFunc)pty_child_setup_cb,
                                                             this,
                                                             &pid,
                                                             nullptr, nullptr, nullptr,
                                                             timeout,
                                                             cancellable ? &pollfd : nullptr,
                                                             err);
        }

        g_strfreev (envp2);

	m_extra_child_setup.func = nullptr;
	m_extra_child_setup.data = nullptr;

        if (cancellable)
                g_cancellable_release_fd(cancellable);

#ifdef WITH_SYSTEMD
        if (ret &&
            !(spawn_flags & VTE_SPAWN_NO_SYSTEMD_SCOPE) &&
            !vte::systemd::create_scope_for_pid_sync(pid,
                                                     timeout, // FIXME: recalc timeout
                                                     cancellable,
                                                     err)) {
                if (spawn_flags & VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE) {
                        auto pgrp = getpgid(pid);
                        if (pgrp != -1) {
                                kill(-pgrp, SIGHUP);
                        }

                        kill(pid, SIGHUP);

                        ret = false;
                } else {
                        _vte_debug_print(VTE_DEBUG_PTY,
                                         "Failed to create systemd scope: %s",
                                         err.message());
                        err.reset();
                }
        }
#endif // WITH_SYSTEMD

        if (!ret)
                return err.propagate(error);

        *child_pid = pid;
        return true;
}

/*
 * Pty::set_size:
 * @rows: the desired number of rows
 * @columns: the desired number of columns
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::set_size(int rows,
              int columns) const noexcept
{
        auto master = fd();

	struct winsize size;
	memset(&size, 0, sizeof(size));
	size.ws_row = rows > 0 ? rows : 24;
	size.ws_col = columns > 0 ? columns : 80;
	_vte_debug_print(VTE_DEBUG_PTY,
			"Setting size on fd %d to (%d,%d).\n",
			master, columns, rows);
        auto ret = ioctl(master, TIOCSWINSZ, &size);

        if (ret != 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "Failed to set size on %d: %m\n", master);
        }

        return ret == 0;
}

/*
 * Pty::get_size:
 * @rows: (out) (allow-none): a location to store the number of rows, or %NULL
 * @columns: (out) (allow-none): a location to store the number of columns, or %NULL
 *
 * Reads the pseudo terminal's window size.
 *
 * If getting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::get_size(int* rows,
              int* columns) const noexcept
{
        auto master = fd();

	struct winsize size;
	memset(&size, 0, sizeof(size));
        auto ret = ioctl(master, TIOCGWINSZ, &size);
        if (ret == 0) {
		if (columns != nullptr) {
			*columns = size.ws_col;
		}
		if (rows != nullptr) {
			*rows = size.ws_row;
		}
		_vte_debug_print(VTE_DEBUG_PTY,
				"Size on fd %d is (%d,%d).\n",
				master, size.ws_col, size.ws_row);
                return true;
	}

        vte::util::restore_errno errsv;
        _vte_debug_print(VTE_DEBUG_PTY,
                         "Failed to read size from fd %d: %m\n", master);

        return false;
}

static int
fd_set_cloexec(int fd)
{
        int flags = fcntl(fd, F_GETFD, 0);
        if (flags < 0)
                return flags;

        return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static int
fd_set_nonblocking(int fd)
{
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
                return -1;
        if ((flags & O_NONBLOCK) != 0)
                return 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int
fd_set_cpkt(int fd)
{
        /* tty_ioctl(4) -> every read() gives an extra byte at the beginning
         * notifying us of stop/start (^S/^Q) events. */
        int one = 1;
        return ioctl(fd, TIOCPKT, &one);
}

static int
fd_setup(int fd)
{
        if (fd_set_cloexec(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "Setting CLOEXEC flag", g_strerror(errsv));
                return -1;
        }

        if (fd_set_nonblocking(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "Setting O_NONBLOCK flag", g_strerror(errsv));
                return -1;
        }

        if (fd_set_cpkt(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "ioctl(TIOCPKT)", g_strerror(errsv));
                return -1;
        }

        return 0;
}

/*
 * _vte_pty_open_posix:
 * @pty: a #VtePty
 * @error: a location to store a #GError, or %NULL
 *
 * Opens a new file descriptor to a new PTY master.
 *
 * Returns: the new PTY's master FD, or -1
 */
static int
_vte_pty_open_posix(void)
{
	/* Attempt to open the master. */
        vte::util::smart_fd fd;
        fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
#ifndef __linux__
        /* Other kernels may not support CLOEXEC or NONBLOCK above, so try to fall back */
        bool need_cloexec = false, need_nonblocking = false;
        if (fd == -1 && errno == EINVAL) {
                /* Try without NONBLOCK and apply the flag afterward */
                need_nonblocking = true;
                fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
                if (fd == -1 && errno == EINVAL) {
                        /* Try without CLOEXEC and apply the flag afterwards */
                        need_cloexec = true;
                        fd = posix_openpt(O_RDWR | O_NOCTTY);
                }
        }
#endif /* !linux */

        if (fd == -1) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "posix_openpt", g_strerror(errsv));
                return -1;
        }

#ifndef __linux__
        if (need_cloexec && fd_set_cloexec(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "Setting CLOEXEC flag", g_strerror(errsv));
                return -1;
        }

        if (need_nonblocking && fd_set_nonblocking(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "Setting NONBLOCK flag", g_strerror(errsv));
                return -1;
        }
#endif /* !linux */

        if (fd_set_cpkt(fd) < 0) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY,
                                 "%s failed: %s", "ioctl(TIOCPKT)", g_strerror(errsv));
                return -1;
        }

	_vte_debug_print(VTE_DEBUG_PTY, "Allocated pty on fd %d.\n", (int)fd);

        return fd.steal();
}

static int
_vte_pty_open_foreign(int masterfd /* consumed */)
{
        vte::util::smart_fd fd(masterfd);
        if (fd == -1) {
                errno = EBADF;
                return -1;
        }

        if (fd_setup(fd) < 0)
                return -1;

        return fd.steal();
}

/*
 * Pty::set_utf8:
 * @utf8: whether or not the pty is in UTF-8 mode
 *
 * Tells the kernel whether the terminal is UTF-8 or not, in case it can make
 * use of the info.  Linux 2.6.5 or so defines IUTF8 to make the line
 * discipline do multibyte backspace correctly.
 *
 * Returns: %true on success, or %false on error with errno set
 */
bool
Pty::set_utf8(bool utf8) const noexcept
{
#ifdef IUTF8
	struct termios tio;
        if (tcgetattr(fd(), &tio) == -1) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m", "tcgetattr");
                return false;
        }

        auto saved_cflag = tio.c_iflag;
        if (utf8) {
                tio.c_iflag |= IUTF8;
        } else {
                tio.c_iflag &= ~IUTF8;
        }

        /* Only set the flag if it changes */
        if (saved_cflag != tio.c_iflag &&
            tcsetattr(fd(), TCSANOW, &tio) == -1) {
                vte::util::restore_errno errsv;
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m", "tcsetattr");
                return false;
	}
#endif

        return true;
}

Pty*
Pty::create(VtePtyFlags flags)
{
        auto fd = _vte_pty_open_posix();
        if (fd == -1)
                return nullptr;

        return new Pty{fd, flags};
}

Pty*
Pty::create_foreign(int fd,
                    VtePtyFlags flags)
{
        fd = _vte_pty_open_foreign(fd);
        if (fd == -1)
                return nullptr;

        return new Pty{fd, flags};
}

} // namespace vte::base
