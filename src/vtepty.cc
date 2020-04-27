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

#include <vte/vte.h>

#include <errno.h>
#include <glib.h>
#include <gio/gio.h>
#include "debug.h"

#include <glib/gi18n-lib.h>

#include "libc-glue.hh"
#include "pty.hh"
#include "vtespawn.hh"

#include "vteptyinternal.hh"

#if !GLIB_CHECK_VERSION(2, 42, 0)
#define G_PARAM_EXPLICIT_NOTIFY 0
#endif

#define I_(string) (g_intern_static_string(string))

typedef struct _VtePtyPrivate VtePtyPrivate;

typedef struct {
	GSpawnChildSetupFunc extra_child_setup;
	gpointer extra_child_setup_data;
} VtePtyChildSetupData;

/**
 * VtePty:
 */
struct _VtePty {
        GObject parent_instance;

        /* <private> */
        VtePtyPrivate *priv;
};

struct _VtePtyPrivate {
        vte::base::Pty* pty; /* owned */
        int foreign_fd; /* foreign FD if  != -1 */
        VtePtyFlags flags;
};

struct _VtePtyClass {
        GObjectClass parent_class;
};

vte::base::Pty*
_vte_pty_get_impl(VtePty* pty)
{
        return pty->priv->pty;
}

#define IMPL(wrapper) (_vte_pty_get_impl(wrapper))

/**
 * VTE_SPAWN_NO_PARENT_ENVV:
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * vte_pty_spawn_async().
 *
 * Normally, the spawned process inherits the environment from the parent
 * process; when this flag is used, only the environment variables passed
 * to vte_pty_spawn_async() etc. are passed to the child process.
 */

/**
 * VTE_SPAWN_NO_SYSTEMD_SCOPE:
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * vte_pty_spawn_async().
 *
 * Prevents vte_pty_spawn_async() etc. from moving the newly created child
 * process to a systemd user scope.
 *
 * Since: 0.60
 */

/**
 * VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE
 *
 * Use this as a spawn flag (together with flags from #GSpawnFlags) in
 * vte_pty_spawn_async().
 *
 * Requires vte_pty_spawn_async() etc. to move the newly created child
 * process to a systemd user scope; if that fails, the whole spawn fails.
 *
 * This is supported on Linux only.
 *
 * Since: 0.60
 */

/**
 * vte_pty_child_setup:
 * @pty: a #VtePty
 *
 * FIXMEchpe
 */
void
vte_pty_child_setup (VtePty *pty)
{
        g_return_if_fail(pty != nullptr);
        auto impl = IMPL(pty);
        g_return_if_fail(impl != nullptr);

        impl->child_setup();
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
 * @timeout: a timeout value in ms, or %NULL
 * @cancellable: a #GCancellable, or %NULL
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
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported, since stdin, stdout
 * and stderr of the child process will always be connected to the PTY.
 *
 * If spawning the command in @working_directory fails because the child
 * is unable to chdir() to it, falls back trying to spawn the command
 * in the parent's working directory.
 *
 * Returns: %TRUE on success, or %FALSE on failure with @error filled in
 */
gboolean
_vte_pty_spawn(VtePty *pty,
               const char *directory,
               char **argv,
               char **envv,
               GSpawnFlags spawn_flags_,
               GSpawnChildSetupFunc child_setup,
               gpointer child_setup_data,
               GPid *child_pid /* out */,
               int timeout,
               GCancellable *cancellable,
               GError **error)
{
        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);

        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        return impl->spawn(directory,
                           argv,
                           envv,
                           spawn_flags_,
                           child_setup,
                           child_setup_data,
                           child_pid,
                           timeout,
                           cancellable,
                           error);
}

/**
 * vte_pty_set_size:
 * @pty: a #VtePty
 * @rows: the desired number of rows
 * @columns: the desired number of columns
 * @error: (allow-none): return location to store a #GError, or %NULL
 *
 * Attempts to resize the pseudo terminal's window size.  If successful, the
 * OS kernel will send #SIGWINCH to the child process group.
 *
 * If setting the window size failed, @error will be set to a #GIOError.
 *
 * Returns: %TRUE on success, %FALSE on failure with @error filled in
 */
gboolean
vte_pty_set_size(VtePty *pty,
                 int rows,
                 int columns,
                 GError **error)
{
        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->set_size(rows, columns))
                return true;

        auto errsv = vte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR,
                    g_io_error_from_errno(errsv),
                    "Failed to set window size: %s",
                    g_strerror(errsv));

        return false;
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
 */
gboolean
vte_pty_get_size(VtePty *pty,
                 int *rows,
                 int *columns,
                 GError **error)
{
        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->get_size(rows, columns))
                return true;

        auto errsv = vte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR,
                    g_io_error_from_errno(errsv),
                    "Failed to get window size: %s",
                    g_strerror(errsv));
        return false;
}

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
 */
gboolean
vte_pty_set_utf8(VtePty *pty,
                 gboolean utf8,
                 GError **error)
{
        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        if (impl->set_utf8(utf8))
                return true;

        auto errsv = vte::libc::ErrnoSaver{};
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                    "%s failed: %s", "tc[sg]etattr", g_strerror(errsv));
        return false;
}

/**
 * vte_pty_close:
 * @pty: a #VtePty
 *
 * Since 0.42 this is a no-op.
 *
 * Deprecated: 0.42
 */
void
vte_pty_close (VtePty *pty)
{
        /* impl->close(); */
}

/* VTE PTY class */

enum {
        PROP_0,
        PROP_FLAGS,
        PROP_FD,
};

/* GInitable impl */

static gboolean
vte_pty_initable_init (GInitable *initable,
                       GCancellable *cancellable,
                       GError **error)
{
        VtePty *pty = VTE_PTY (initable);
        VtePtyPrivate *priv = pty->priv;

        if (priv->foreign_fd != -1) {
                priv->pty = vte::base::Pty::create_foreign(priv->foreign_fd, priv->flags);
                priv->foreign_fd = -1;
        } else {
                priv->pty = vte::base::Pty::create(priv->flags);
        }

        if (priv->pty == nullptr) {
                auto errsv = vte::libc::ErrnoSaver{};
                g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errsv),
                            "Failed to open PTY: %s", g_strerror(errsv));
                return FALSE;
        }

        return !g_cancellable_set_error_if_cancelled(cancellable, error);
}

static void
vte_pty_initable_iface_init (GInitableIface  *iface)
{
        iface->init = vte_pty_initable_init;
}

/* GObjectClass impl */

G_DEFINE_TYPE_WITH_CODE (VtePty, vte_pty, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (VtePty)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, vte_pty_initable_iface_init))

static void
vte_pty_init (VtePty *pty)
{
        VtePtyPrivate *priv;

        priv = pty->priv = (VtePtyPrivate *)vte_pty_get_instance_private (pty);

        priv->pty = nullptr;
        priv->foreign_fd = -1;
        priv->flags = VTE_PTY_DEFAULT;
}

static void
vte_pty_finalize (GObject *object)
{
        VtePty *pty = VTE_PTY (object);
        VtePtyPrivate *priv = pty->priv;

        if (priv->pty != nullptr)
                priv->pty->unref();

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
                priv->flags = (VtePtyFlags) g_value_get_flags(value);
                break;

        case PROP_FD:
                priv->foreign_fd = g_value_get_int(value);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
vte_pty_class_init (VtePtyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->set_property = vte_pty_set_property;
        object_class->get_property = vte_pty_get_property;
        object_class->finalize     = vte_pty_finalize;

        /**
         * VtePty:flags:
         *
         * Flags.
         */
        g_object_class_install_property
                (object_class,
                 PROP_FLAGS,
                 g_param_spec_flags ("flags", NULL, NULL,
                                     VTE_TYPE_PTY_FLAGS,
                                     VTE_PTY_DEFAULT,
                                     (GParamFlags) (G_PARAM_READWRITE |
                                                    G_PARAM_CONSTRUCT_ONLY |
                                                    G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY)));

        /**
         * VtePty:fd:
         *
         * The file descriptor of the PTY master.
         */
        g_object_class_install_property
                (object_class,
                 PROP_FD,
                 g_param_spec_int ("fd", NULL, NULL,
                                   -1, G_MAXINT, -1,
                                   (GParamFlags) (G_PARAM_READWRITE |
                                                  G_PARAM_CONSTRUCT_ONLY |
                                                  G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY)));
}

/* public API */

/**
 * vte_pty_error_quark:
 *
 * Error domain for VTE PTY errors. Errors in this domain will be from the #VtePtyError
 * enumeration. See #GError for more information on error domains.
 *
 * Returns: the error domain for VTE PTY errors
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
 * vte_pty_new_sync: (constructor)
 * @flags: flags from #VtePtyFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Allocates a new pseudo-terminal.
 *
 * You can later use fork() or the g_spawn_async() family of functions
 * to start a process on the PTY.
 *
 * If using fork(), you MUST call vte_pty_child_setup() in the child.
 *
 * If using g_spawn_async() and friends, you MUST either use
 * vte_pty_child_setup() directly as the child setup function, or call
 * vte_pty_child_setup() from your own child setup function supplied.
 *
 * When using vte_terminal_spawn_sync() with a custom child setup
 * function, vte_pty_child_setup() will be called before the supplied
 * function; you must not call it again.
 *
 * Also, you MUST pass the %G_SPAWN_DO_NOT_REAP_CHILD flag.
 *
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported, since stdin, stdout
 * and stderr of the child process will always be connected to the PTY.
 *
 * Note that you should set the PTY's size using vte_pty_set_size() before
 * spawning the child process, so that the child process has the correct
 * size from the start instead of starting with a default size and then
 * shortly afterwards receiving a SIGWINCH signal. You should prefer
 * using vte_terminal_pty_new_sync() which does this automatically.
 *
 * Returns: (transfer full): a new #VtePty, or %NULL on error with @error filled in
 */
VtePty *
vte_pty_new_sync (VtePtyFlags flags,
                  GCancellable *cancellable,
                  GError **error)
{
        return (VtePty *) g_initable_new (VTE_TYPE_PTY,
                                          cancellable,
                                          error,
                                          "flags", flags,
                                          NULL);
}

/**
 * vte_pty_new_foreign_sync: (constructor)
 * @fd: a file descriptor to the PTY
 * @cancellable: (allow-none): a #GCancellable, or %NULL
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
 */
VtePty *
vte_pty_new_foreign_sync (int fd,
                          GCancellable *cancellable,
                          GError **error)
{
        g_return_val_if_fail(fd != -1, nullptr);

        return (VtePty *) g_initable_new (VTE_TYPE_PTY,
                                          cancellable,
                                          error,
                                          "fd", fd,
                                          NULL);
}

/**
 * vte_pty_get_fd:
 * @pty: a #VtePty
 *
 * Returns: the file descriptor of the PTY master in @pty. The
 *   file descriptor belongs to @pty and must not be closed or have
 *   its flags changed
 */
int
vte_pty_get_fd (VtePty *pty)
{
        g_return_val_if_fail(VTE_IS_PTY(pty), FALSE);
        auto impl = IMPL(pty);
        g_return_val_if_fail(impl != nullptr, FALSE);

        return impl->fd();
}

typedef struct {
        VtePty* m_pty;
        char* m_working_directory;
        char** m_argv;
        char** m_envv;
        GSpawnFlags m_spawn_flags;
        GSpawnChildSetupFunc m_child_setup;
        gpointer m_child_setup_data;
        GDestroyNotify m_child_setup_data_destroy;
        int m_timeout;
} AsyncSpawnData;

static AsyncSpawnData*
async_spawn_data_new (VtePty* pty,
                      char const* working_directory,
                      char** argv,
                      char** envv,
                      GSpawnFlags spawn_flags,
                      GSpawnChildSetupFunc child_setup,
                      gpointer child_setup_data,
                      GDestroyNotify child_setup_data_destroy,
                      int timeout)
{
        auto data = g_new(AsyncSpawnData, 1);

        data->m_pty = (VtePty*)g_object_ref(pty);
        data->m_working_directory = g_strdup(working_directory);
        data->m_argv = g_strdupv(argv);
        data->m_envv = envv ? g_strdupv(envv) : nullptr;
        data->m_spawn_flags = spawn_flags;
        data->m_child_setup = child_setup;
        data->m_child_setup_data = child_setup_data;
        data->m_child_setup_data_destroy = child_setup_data_destroy;
        data->m_timeout = timeout;

        return data;
}

static void
async_spawn_data_free(gpointer data_)
{
        AsyncSpawnData *data = reinterpret_cast<AsyncSpawnData*>(data_);

        g_free(data->m_working_directory);
        g_strfreev(data->m_argv);
        g_strfreev(data->m_envv);
        if (data->m_child_setup_data && data->m_child_setup_data_destroy)
                data->m_child_setup_data_destroy(data->m_child_setup_data);
        g_object_unref(data->m_pty);

        g_free(data);
}

static void
async_spawn_run_in_thread(GTask *task,
                          gpointer object,
                          gpointer data_,
                          GCancellable *cancellable)
{
        AsyncSpawnData *data = reinterpret_cast<AsyncSpawnData*>(data_);

        GPid pid;
        GError *error = NULL;
        if (_vte_pty_spawn(data->m_pty,
                            data->m_working_directory,
                            data->m_argv,
                            data->m_envv,
                            (GSpawnFlags)data->m_spawn_flags,
                            data->m_child_setup, data->m_child_setup_data,
                            &pid,
                            data->m_timeout,
                            cancellable,
                            &error))
                g_task_return_pointer(task, g_memdup(&pid, sizeof(pid)), g_free);
        else
                g_task_return_error(task, error);
}

/**
 * vte_pty_spawn_async:
 * @pty: a #VtePty
 * @working_directory: (allow-none): the name of a directory the command should start
 *   in, or %NULL to use the current working directory
 * @argv: (array zero-terminated=1) (element-type filename): child's argument vector
 * @envv: (allow-none) (array zero-terminated=1) (element-type filename): a list of environment
 *   variables to be added to the environment before starting the process, or %NULL
 * @spawn_flags: flags from #GSpawnFlags
 * @child_setup: (allow-none) (scope async): an extra child setup function to run in the child just before exec(), or %NULL
 * @child_setup_data: (closure child_setup): user data for @child_setup, or %NULL
 * @child_setup_data_destroy: (destroy child_setup_data): a #GDestroyNotify for @child_setup_data, or %NULL
 * @timeout: a timeout value in ms, or -1 to wait indefinitely
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 *
 * Starts the specified command under the pseudo-terminal @pty.
 * The @argv and @envv lists should be %NULL-terminated.
 * The "TERM" environment variable is automatically set to a default value,
 * but can be overridden from @envv.
 * @pty_flags controls logging the session to the specified system log files.
 *
 * Note that %G_SPAWN_DO_NOT_REAP_CHILD will always be added to @spawn_flags.
 *
 * Note also that %G_SPAWN_STDOUT_TO_DEV_NULL, %G_SPAWN_STDERR_TO_DEV_NULL,
 * and %G_SPAWN_CHILD_INHERITS_STDIN are not supported in @spawn_flags, since
 * stdin, stdout and stderr of the child process will always be connected to
 * the PTY.
 *
 * Note that all open file descriptors will be closed in the child. If you want
 * to keep some file descriptor open for use in the child process, you need to
 * use a child setup function that unsets the FD_CLOEXEC flag on that file
 * descriptor.
 *
 * Beginning with 0.60, and on linux only, and unless %VTE_SPAWN_NO_SYSTEMD_SCOPE is
 * passed in @spawn_flags, the newly created child process will be moved to its own
 * systemd user scope; and if %VTE_SPAWN_REQUIRE_SYSTEMD_SCOPE is passed, and creation
 * of the systemd user scope fails, the whole spawn will fail.
 * You can override the options used for the systemd user scope by
 * providing a systemd override file for 'vte-spawn-.scope' unit. See man:systemd.unit(5)
 * for further information.
 *
 * See vte_pty_new(), g_spawn_async() and vte_terminal_watch_child() for more information.
 *
 * Since: 0.48
 */
void
vte_pty_spawn_async(VtePty *pty,
                    const char *working_directory,
                    char **argv,
                    char **envv,
                    GSpawnFlags spawn_flags,
                    GSpawnChildSetupFunc child_setup,
                    gpointer child_setup_data,
                    GDestroyNotify child_setup_data_destroy,
                    int timeout,
                    GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
        g_return_if_fail(argv != nullptr);
        g_return_if_fail(!child_setup_data || child_setup);
        g_return_if_fail(!child_setup_data_destroy || child_setup_data);
        g_return_if_fail(cancellable == nullptr || G_IS_CANCELLABLE (cancellable));
        g_return_if_fail(callback);

        auto data = async_spawn_data_new(pty,
                                         working_directory, argv, envv,
                                         spawn_flags,
                                         child_setup, child_setup_data, child_setup_data_destroy,
                                         timeout);

        auto task = g_task_new(pty, cancellable, callback, user_data);
        g_task_set_source_tag(task, (void*)vte_pty_spawn_async);
        g_task_set_task_data(task, data, async_spawn_data_free);
        g_task_run_in_thread(task, async_spawn_run_in_thread);
        g_object_unref(task);
}

/**
 * vte_pty_spawn_finish:
 * @pty: a #VtePty
 * @result: a #GAsyncResult
 * @child_pid: (out) (allow-none) (transfer full): a location to store the child PID, or %NULL
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 *
 * Since: 0.48
 */
gboolean
vte_pty_spawn_finish(VtePty *pty,
                     GAsyncResult *result,
                     GPid *child_pid /* out */,
                     GError **error)
{
        g_return_val_if_fail (VTE_IS_PTY (pty), FALSE);
        g_return_val_if_fail (G_IS_TASK (result), FALSE);
        g_return_val_if_fail(error == nullptr || *error == nullptr, FALSE);

        gpointer pidptr = g_task_propagate_pointer(G_TASK(result), error);
        if (pidptr == nullptr) {
                if (child_pid)
                        *child_pid = -1;
                return FALSE;
        }

        if (child_pid)
                *child_pid = *(GPid*)pidptr;
        if (error)
                *error = nullptr;

        g_free(pidptr);
        return TRUE;
}
