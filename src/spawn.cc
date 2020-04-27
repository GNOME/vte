/*
 * Copyright © 2001,2002 Red Hat, Inc.
 * Copyright © 2009, 2010, 2019, 2020 Christian Persch
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

#include "config.h"

#include "spawn.hh"

#include "libc-glue.hh"

#include <vte/vte.h>
#include "vteptyinternal.hh"
#include "vtespawn.hh"
#include "debug.h"
#include "reaper.hh"

#include <assert.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

#include <glib/gi18n-lib.h>
#include <glib-unix.h>

#include "glib-glue.hh"

#ifdef WITH_SYSTEMD
#include "systemd.hh"
#endif

#include "vtedefines.hh"

namespace vte::base {

static bool
make_pipe(int flags,
          vte::libc::FD& read_fd,
          vte::libc::FD& write_fd,
          vte::glib::Error& error)
{
        int fds[2] = { -1, -1 };
        if (!g_unix_open_pipe(fds, flags, error))
                return false;

        read_fd = fds[0];
        write_fd = fds[1];
        return true;
}

static char**
merge_environ(char** envp /* consumed */,
              char const* cwd,
              bool inherit)
{
        auto table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

        if (inherit) {
                auto parent_environ = g_listenv();
                if (parent_environ) {
                        for (auto i = unsigned{0}; parent_environ[i] != NULL; ++i) {
                                g_hash_table_replace(table,
                                                     g_strdup(parent_environ[i]),
                                                     g_strdup(g_getenv(parent_environ[i])));
                        }
                        g_strfreev(parent_environ);
                }
        }

        /* Make sure the one in envp overrides the default. */
        g_hash_table_replace(table, g_strdup("TERM"), g_strdup(VTE_TERMINFO_NAME));

        if (envp) {
                for (auto i = unsigned{0}; envp[i] != nullptr; ++i) {
                        auto name = g_strdup(envp[i]);
                        auto value = strchr(name, '=');
                        if (value) {
                                *value = '\0';
                                value = g_strdup(value + 1);
                        }
                        g_hash_table_replace(table, name, value); /* takes ownership of name and value */
                }

                g_strfreev(envp);
        }

        /* Always set this ourself, not allowing replacing from envp */
        g_hash_table_replace(table, g_strdup("VTE_VERSION"), g_strdup_printf("%u", VTE_VERSION_NUMERIC));
        g_hash_table_replace(table, g_strdup("COLORTERM"), g_strdup("truecolor"));

        /* We need to put the working directory also in PWD, so that
         * e.g. bash starts in the right directory if @directory is a symlink.
         * See bug #502146 and #758452.
         *
         * If chdir to cwd fails, and we fall back to the fallback cwd, PWD will
         * be set to a directory != the actual working directory, but that's not
         * a problem since PWD is only used when it's equal to the actual working
         * directory.
         */
        if (cwd)
                g_hash_table_replace(table, g_strdup("PWD"), g_strdup(cwd));
        else
                g_hash_table_remove(table, "PWD");

        auto array = g_ptr_array_sized_new(g_hash_table_size(table) + 1);

        auto iter = GHashTableIter{};
        g_hash_table_iter_init(&iter, table);
        char *name, *value;
        while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value)) {
                if (value)
                        g_ptr_array_add(array, g_strconcat(name, "=", value, nullptr));
        }
        g_hash_table_destroy(table);
        g_ptr_array_add(array, nullptr);

        return reinterpret_cast<char**>(g_ptr_array_free(array, false));
}

void
SpawnContext::prepare_environ()
{
        m_envv = vte::glib::take_strv(merge_environ(m_envv.release(), m_cwd.get(), inherit_environ()));
}

int
SpawnContext::exec() const noexcept
{
        /* NOTE! This function must not rely on smart pointers to
         * release their object, since the destructors are NOT run
         * when the exec succeeds!
         */

        _VTE_DEBUG_IF(VTE_DEBUG_MISC) {
                g_printerr ("Spawning command:\n");
                auto argv = m_argv.get();
                for (auto i = 0; argv[i] != NULL; i++) {
                        g_printerr("    argv[%d] = %s\n", i, argv[i]);
                }
                if (m_envv) {
                        auto envv = m_envv.get();
                        for (auto i = 0; envv[i] != NULL; i++) {
                                g_printerr("    env[%d] = %s\n", i, envv[i]);
                        }
                }
                g_printerr("    directory: %s\n",
                           m_cwd ? m_cwd.get() : "(none)");
        }

        /* Unblock all signals */
        sigset_t set;
        sigemptyset(&set);
        if (pthread_sigmask(SIG_SETMASK, &set, nullptr) == -1) {
                _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "pthread_sigmask");
                return -1;
        }

        /* Reset the handlers for all signals to their defaults.  The parent
         * (or one of the libraries it links to) may have changed one to be ignored.
         * Esp. SIGPIPE since it ensures this process terminates when we write
         * to child_err_report_pipe after the parent has exited.
         */
        for (auto n = int{1}; n < NSIG; ++n) {
                if (n == SIGSTOP || n == SIGKILL)
                        continue;

                signal(n, SIG_DFL);
        }

        /* Close all file descriptors on exec. Note that this includes
         * child_error_report_pipe_write, which keeps the parent from blocking
         * forever on the other end of that pipe.
         * (Note that stdin, stdout and stderr will be set by the child setup afterwards.)
         */
        // FIXMEchpe make sure child_error_report_pipe_write is != 0, 1, 2 !!! before setting up PTY below!!!
        _vte_cloexec_from(3);

        if (!(pty()->flags() & VTE_PTY_NO_SESSION)) {
                /* This starts a new session; we become its process-group leader,
                 * and lose our controlling TTY.
                 */
                _vte_debug_print(VTE_DEBUG_PTY, "Starting new session\n");
                if (setsid() == -1) {
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "setsid");
                        return -1;
                }
        }

        /* Note: *not* FD_CLOEXEC! */
        auto peer_fd = pty()->get_peer();
        if (peer_fd == -1)
                return -1;

#ifdef TIOCSCTTY
        /* On linux, opening the PTY peer above already made it our controlling TTY (since
         * previously there was none, after the setsid() call). However, it appears that e.g.
         * on *BSD, that doesn't happen, so we need this explicit ioctl here.
         */
        if (!(pty()->flags() & VTE_PTY_NO_CTTY)) {
                if (ioctl(peer_fd, TIOCSCTTY, peer_fd) != 0) {
                        _vte_debug_print(VTE_DEBUG_PTY, "%s failed: %m\n", "ioctl(TIOCSCTTY)");
                        return -1;
                }
        }
#endif

        /* now setup child I/O through the tty */
        if (peer_fd != STDIN_FILENO) {
                if (dup2(peer_fd, STDIN_FILENO) != STDIN_FILENO)
                        return -1;
        }
        if (peer_fd != STDOUT_FILENO) {
                if (dup2(peer_fd, STDOUT_FILENO) != STDOUT_FILENO)
                        return -1;
        }
        if (peer_fd != STDERR_FILENO) {
                if (dup2(peer_fd, STDERR_FILENO) != STDERR_FILENO)
                        return -1;
        }

        if (peer_fd != STDIN_FILENO  &&
            peer_fd != STDOUT_FILENO &&
            peer_fd != STDERR_FILENO) {
                close(peer_fd);
        }

        if (m_cwd && chdir(m_cwd.get()) < 0) {
                /* If the fallback fails too, make sure to return the errno
                 * from the original cwd, not the fallback cwd.
                 */
                auto errsv = vte::libc::ErrnoSaver{};
                if (m_fallback_cwd && chdir(m_fallback_cwd.get()) < 0)
                        return G_SPAWN_ERROR_CHDIR;

                errsv.reset();
        }

        /* Finally call an extra child setup */
        if (m_child_setup)
                m_child_setup(m_child_setup_data);

        /* exec */
        _vte_execute(arg0(),
                     argv(),
                     environ(),
                     search_path(),
                     search_path_from_envp());

        /* If we get here, exec failed */
        return G_SPAWN_ERROR_FAILED;
}

SpawnOperation::~SpawnOperation()
{
        if (m_cancellable && m_cancellable_pollfd.fd != -1)
                g_cancellable_release_fd(m_cancellable.get());

        if (m_pid != -1) {
                /* Since we're not passing the PID back to the caller,
                 * we need to kill and reap it ourself.
                 */
                if (m_kill_pid) {
                        auto const pgrp = getpgid(m_pid);
                        /* Make sure not to kill ourself, if the child died before
                         * it could call setsid()!
                         */
                        if (pgrp != -1 && pgrp != getpgid(getpid()))
                                kill(-pgrp, SIGHUP);

                        kill(m_pid, SIGHUP);
                }

                vte_reaper_add_child(m_pid);
        }
}

bool
SpawnOperation::prepare(vte::glib::Error& error) noexcept
{
#ifndef WITH_SYSTEMD
        if (context().systemd_scope()) {
                error.set_literal(G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                  "systemd not available");
                return false;
        }
#endif

        if (m_cancellable &&
            !g_cancellable_make_pollfd(m_cancellable.get(), &m_cancellable_pollfd)) {
                auto errsv = vte::libc::ErrnoSaver{};
                error.set(G_IO_ERROR,
                          g_io_error_from_errno(errsv),
                          "Failed to make cancellable pollfd: %s",
                          g_strerror(errsv));
                return false;
        }

        auto child_report_error_pipe_read = vte::libc::FD{};
        auto child_report_error_pipe_write = vte::libc::FD{};
        assert(!child_report_error_pipe_read);
        assert(!child_report_error_pipe_write);
        if (!make_pipe(FD_CLOEXEC,
                       child_report_error_pipe_read,
                       child_report_error_pipe_write,
                       error))
                return false;

        assert(child_report_error_pipe_read);
        assert(child_report_error_pipe_write);
        auto const pid = fork();
        if (pid < 0) {
                auto errsv = vte::libc::ErrnoSaver{};
                error.set(G_IO_ERROR,
                          g_io_error_from_errno(errsv),
                          "Failed to fork: %s",
                          g_strerror(errsv));
                return false;
        }

        if (pid == 0) {
                /* Child */

                child_report_error_pipe_read.reset();
                assert(!child_report_error_pipe_read);

                auto const err = context().exec();

                /* If we get here, exec failed. Write the error to the pipe and exit */
                assert(!child_report_error_pipe_read);
                _vte_write_err(child_report_error_pipe_write.get(), err);
                _exit(127);
                return true;
        }

        /* Parent */
        m_pid = pid;
        m_child_report_error_pipe_read = std::move(child_report_error_pipe_read);
        assert(m_child_report_error_pipe_read);

        return true;
}

bool
SpawnOperation::run(vte::glib::Error& error) noexcept
{
        int buf[2] = {G_SPAWN_ERROR_FAILED, ENOSYS};
        auto n_read = int{0};

        g_assert_cmpint(m_child_report_error_pipe_read.get(), !=, -1);
        assert(m_child_report_error_pipe_read);

        if (!_vte_read_ints(m_child_report_error_pipe_read.get(),
                            buf, 2, &n_read,
                            m_timeout,
                            &m_cancellable_pollfd,
                            error))
                return false;

        if (n_read >= 2) {
                /* The process will have called _exit(127) already, no need to kill it */
                m_kill_pid = false;

                switch (buf[0]) {
                case G_SPAWN_ERROR_CHDIR: {
                        auto cwd = vte::glib::take_string(context().cwd() ? g_utf8_make_valid(context().cwd(), -1) : nullptr);
                        error.set(G_IO_ERROR,
                                  g_io_error_from_errno(buf[1]),
                                  _("Failed to change to directory “%s”: %s"),
                                  cwd.get(),
                                  g_strerror(buf[1]));
                        break;
                }

                case G_SPAWN_ERROR_FAILED: {
                        auto arg = vte::glib::take_string(g_utf8_make_valid(context().argv()[0], -1));
                        error.set(G_IO_ERROR,
                                  g_io_error_from_errno(buf[1]),
                                  _("Failed to execute child process “%s”: %s"),
                                  arg.get(),
                                  g_strerror(buf[1]));
                        break;
                }

                default: {
                        auto arg = vte::glib::take_string(g_utf8_make_valid(context().argv()[0], -1));
                        error.set(G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  _("Unknown error executing child process “%s”"),
                                  arg.get());
                        break;
                }
                }

                return false;
        }

        /* Spawn succeeded */

#ifdef WITH_SYSTEMD
        if (context().systemd_scope() &&
            !vte::systemd::create_scope_for_pid_sync(m_pid,
                                                     m_timeout, // FIXME: recalc timeout
                                                     m_cancellable.get(),
                                                     error)) {
                if (context().require_systemd_scope())
                        return false;

                _vte_debug_print(VTE_DEBUG_PTY,
                                 "Failed to create systemd scope: %s",
                                 error.message());
                error.reset();
        }
#endif // WITH_SYSTEMD

        return true;
}

void
SpawnOperation::run_in_thread(GTask* task) noexcept
{
        auto error = vte::glib::Error{};
        if (run(error))
                g_task_return_int(task, ssize_t{release_pid()});
        else
                g_task_return_error(task, error.release());
}

/* Note: this function takes ownership of @this */
void
SpawnOperation::run_async(void* source_tag,
                          GAsyncReadyCallback callback,
                          void* user_data)
{
        /* Create a GTask to run the user-provided callback, and transfers
         * ownership of @this to the task, meaning that @this will be deleted after
         * the task is completed.
         */
        auto task = vte::glib::take_ref(g_task_new(context().pty_wrapper(),
                                                   m_cancellable.get(),
                                                   callback,
                                                   user_data));
        g_task_set_source_tag(task.get(), source_tag);
        g_task_set_task_data(task.get(), this, delete_cb);
        // g_task_set_name(task.get(), "vte-spawn-async");

        /* Spawning is split into the fork() phase, and waiting for the child to
         * exec or report an error. This is done so that the fork is happening on
         * the main thread; see issue vte#118.
         */
        auto error = vte::glib::Error{};
        if (!prepare(error))
                return g_task_return_error(task.get(), error.release());

        /* Async read from the child */
        g_task_run_in_thread(task.get(), run_in_thread_cb);
}

bool
SpawnOperation::run_sync(GPid* pid,
                         vte::glib::Error& error)
{
        auto rv = prepare(error) && run(error);
        if (rv)
                *pid = release_pid();
        else
                *pid = -1;

        return rv;
}

} // namespace vte::base
