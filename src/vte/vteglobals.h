/*
 * Copyright (C) 2001,2002,2003,2009,2010 Red Hat, Inc.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"

G_BEGIN_DECLS

_VTE_PUBLIC
char *vte_get_user_shell(void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
const char *vte_get_features (void) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
VteFeatureFlags vte_get_feature_flags(void) _VTE_CXX_NOEXCEPT;

#define VTE_TEST_FLAGS_NONE (G_GUINT64_CONSTANT(0))
#define VTE_TEST_FLAGS_ALL (~G_GUINT64_CONSTANT(0))

_VTE_PUBLIC
void vte_set_test_flags(guint64 flags) _VTE_CXX_NOEXCEPT;

_VTE_PUBLIC
guint64 vte_get_test_flags(void) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_NAME_PREFIX:
 *
 * The string prefix that any termprop's name must start with to be installed
 * by vte_install_termprop().
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_NAME_PREFIX "vte.ext."

_VTE_PUBLIC
int vte_install_termprop(char const* name,
                         VtePropertyType type,
                         VtePropertyFlags flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
int vte_install_termprop_alias(char const* name,
                               char const* target_name) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1, 2);

_VTE_PUBLIC
char const** vte_get_termprops(gsize* length) _VTE_CXX_NOEXCEPT G_GNUC_MALLOC;

_VTE_PUBLIC
gboolean vte_query_termprop(char const* name,
                            char const** resolved_name,
                            int* prop,
                            VtePropertyType* type,
                            VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT _VTE_GNUC_NONNULL(1);

_VTE_PUBLIC
gboolean vte_query_termprop_by_id(int prop,
                                  char const** name,
                                  VtePropertyType* type,
                                  VtePropertyFlags* flags) _VTE_CXX_NOEXCEPT;

/**
 * VTE_TERMPROP_CURRENT_DIRECTORY_URI:
 *
 * A %VTE_PROPERTY_URI termprop that stores the current directory
 * URI as set by OSC 7.
 * Use this with vte_terminal_ref_termprop_uri() instead of using
 * vte_terminal_get_current_directory_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_DIRECTORY_URI "vte.cwd"

/**
 * VTE_TERMPROP_CURRENT_FILE_URI:
 *
 * A %VTE_PROPERTY_URI termprop that stores the current file URI
 * as set by OSC 6.
 * Use this with vte_terminal_ref_termprop_uri() instead of using
 * vte_terminal_get_current_file_uri().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CURRENT_FILE_URI "vte.cwf"

/**
 * VTE_TERMPROP_XTERM_TITLE:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the xterm window title
 * as set by OSC 0 and OSC 2.
 * Use this with vte_terminal_get_termprop_string() instead of using
 * vte_terminal_get_window_title().
 *
 * Note that this termprop is not settable via the termprop OSC.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_XTERM_TITLE "xterm.title"

/**
 * VTE_TERMPROP_CONTAINER_NAME:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the name of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_NAME "vte.container.name"

/**
 * VTE_TERMPROP_CONTAINER_RUNTIME:
 *
 * A %VTE_PROPERTY_STRING termprop that stores the runtime of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_RUNTIME "vte.container.runtime"

/**
 * VTE_TERMPROP_CONTAINER_UID:
 *
 * A %VTE_PROPERTY_UINT termprop that stores the user ID of the
 * container.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_CONTAINER_UID "vte.container.uid"

/**
 * VTE_TERMPROP_SHELL_PRECMD:
 *
 * A %VTE_PROPERTY_VALUELESS termprop that signals that the shell
 * is going to prompt.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_PRECMD "vte.shell.precmd"

/**
 * VTE_TERMPROP_SHELL_PREEXEC:
 *
 * A %VTE_PROPERTY_VALUELESS termprop that signals that the shell
 * is preparing to execute the command entered at the prompt.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_PREEXEC "vte.shell.preexec"

/**
 * VTE_TERMPROP_SHELL_POSTEXEC:
 *
 * An ephemeral %VTE_PROPERTY_UINT termprop that signals that the shell
 * has executed the commands entered at the prompt and these commands
 * have returned. The termprop value is the exit code.
 *
 * Since: 0.78
 */
#define VTE_TERMPROP_SHELL_POSTEXEC "vte.shell.postexec"

/**
 * VTE_TERMPROP_PROGRESS_HINT:
 *
 * A %VTE_PROPERTY_INT termprop that stores a hint how to interpret
 * the %VTE_TERMPROP_PROGRESS_VALUE termprop value. If set, this
 * termprop's value will be from the #VteProgressHint enumeration.
 * An unset termprop should be treated as if it had value
 * %VTE_PROGRESS_HINT_ACTIVE if the %VTE_TERMPROP_PROGRESS_VALUE
 * termprop has a value
 *
 * Note that this termprop never will have the value
 * %VTE_PROGRESS_HINT_INACTIVE.
 *
 * The value of this termprop should be ignored unless the
 * %VTE_TERMPROP_PROGRESS_VALUE termprop has a value.
 *
 * Note that before version 0.82, this termprop could not be set by
 * the termprop OSC, but instead only by OSC 9 ; 4 (ConEmu progress).
 *
 * Since: 0.80
 */
#define VTE_TERMPROP_PROGRESS_HINT "vte.progress.hint"

/**
 * VTE_TERMPROP_PROGRESS_VALUE:
 *
 * A %VTE_PROPERTY_UINT termprop that stores the progress of the running
 * command as a value between 0 and 100.
 *
 * Note that before version 0.82, this termprop could not be set by
 * the termprop OSC, but instead only by OSC 9 ; 4 (ConEmu progress).
 *
 * Since: 0.80
 */
#define VTE_TERMPROP_PROGRESS_VALUE "vte.progress.value"

/*
 * VTE_TERMPROP_ICON_COLOR:
 *
 * A %VTE_PROPERTY_RGB termprop to specify a color for use
 * in a favicon or tab highlight.
 *
 * Aapplications should use this if the %VTE_TERMPROP_ICON_IMAGE
 * termprop is unset.
 *
 * Since: 0.80
 */
#define VTE_TERMPROP_ICON_COLOR "vte.icon.color"

/**
 * VTE_TERMPROP_ICON_IMAGE:
 *
 * A %VTE_PROPERTY_IMAGE termprop to specify an image for use
 * as a favicon.
 *
 * Applications should prefer to use this termprop, if set, over
 * the %VTE_TERMPROP_ICON_COLOR color.
 *
 * Note that this termprop is not settable via the termprop OSC.
 * Instead, if the #VteTerminal:enable-sixel property is %TRUE,
 * this termprop can be set from a SIXEL image sequence with the
 * fourth parameter (ID) set to 65535.
 *
 * Since: 0.80
 */
#define VTE_TERMPROP_ICON_IMAGE "vte.icon.image"

/**
 * VTE_SYSTEMD_PROPERTY_BOOT_ID:
 *
 * A %VTE_PROPERTY_UUID property to specify the boot ID
 * (i.e. `/proc/sys/kernel/random/boot_id`) of the system
 * the process issuing the sequence runs on.

 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_BOOT_ID "bootid"

/**
 * VTE_SYSTEMD_PROPERTY_COMM:
 *
 * A %VTE_PROPERTY_STRING property to specify The process name
 * (i.e. `/proc/$PID/comm`, `PR_GET_NAME`) of the process
 * issuing the sequence.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_COMM "comm"

/**
 * VTE_SYSTEMD_PROPERTY_COMMAND_LINE:
 *
 * A %VTE_PROPERTY_STRING property to specify the full command
 * line of the invoked command.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_COMMAND_LINE "cmdline"

/**
 * VTE_SYSTEMD_PROPERTY_CONTAINER:
 *
 * A %VTE_PROPERTY_STRING property to specify the name of
 * the container being invoked.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_CONTAINER "container"

/**
 * VTE_SYSTEMD_PROPERTY_CONTEXT_ID:
 *
 * A %VTE_PROPERTY_UUID property to specify the context ID.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_CONTEXT_ID "context-id"

/**
 * VTE_SYSTEMD_PROPERTY_CONTEXT_TYPE:
 *
 * A %VTE_PROPERTY_INT property to specify the context type
 * as a value from #VteSystemdContextType.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_CONTEXT_TYPE "type"

/**
 * VTE_SYSTEMD_PROPERTY_CURRENT_DIRECTORY:
 *
 * A %VTE_PROPERTY_URI property to specify the current working
 * directory.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_CURRENT_DIRECTORY "cwd"

/**
 * VTE_SYSTEMD_PROPERTY_EXIT_CONDITION:
 *
 * A %VTE_PROPERTY_INT property to specify the exit condition
 * of the context as a value from #VteSystemdContextExitCondition.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_END contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_EXIT_CONDITION "exit"

/**
 * VTE_SYSTEMD_PROPERTY_EXIT_SIGNAL:
 *
 * A %VTE_PROPERTY_STRING property to specify the symbolic
 * name of the termination signal, if the process died
 * abnormally. E.g. `SIGKILL`, etc.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_END contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_EXIT_SIGNAL "signal"

/**
 * VTE_SYSTEMD_PROPERTY_EXIT_STATUS:
 *
 * A %VTE_PROPERTY_UINT property to specify the command's
 * numeric exit status in the range 0…255.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_END contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_EXIT_STATUS "status"

/**
 * VTE_SYSTEMD_PROPERTY_HOSTNAME:
 *
 * A %VTE_PROPERTY_STRING property to specify the unix host name
 * of the system the process issuing the sequence runs on.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_HOSTNAME "hostname"

/**
 * VTE_SYSTEMD_PROPERTY_MACHINE_ID:
 *
 * A %VTE_PROPERTY_UUID property to specify the machine ID
 * (i.e. `/etc/machine-id`) of the system the process issuing
 * the sequence runs on.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_MACHINE_ID "machineid"

/**
 * VTE_SYSTEMD_PROPERTY_PID:
 *
 * A %VTE_PROPERTY_UINT property to specify The numeric PID
 * of the process issuing the sequence.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_PID "pid"

/**
 * VTE_SYSTEMD_PROPERTY_PIDFD_INODE:
 *
 * A %VTE_PROPERTY_UINT property to specify the inode
 * number of the PIDFD of the process issuing the sequence.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_PIDFD_INODE "pidfdid"

/**
 * VTE_SYSTEMD_PROPERTY_TARGET_HOST:
 *
 * A %VTE_PROPERTY_STRING property to specify the target
 * unix, DNS host name, or IP address.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_TARGET_HOST "targethost"

/**
 * VTE_SYSTEMD_PROPERTY_TARGET_USER:
 *
 * A %VTE_PROPERTY_STRING property to specify the target
 * unix user name of a context of type
 * %VTE_SYSTEMD_CONTEXT_TYPE_ELEVATE,
 * %VTE_SYSTEMD_CONTEXT_TYPE_CHPRIV,
 * %VTE_SYSTEMD_CONTEXT_TYPE_VM,
 * %VTE_SYSTEMD_CONTEXT_TYPE_CONTAINER, or
 * %VTE_SYSTEMD_CONTEXT_TYPE_REMOTE.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_TARGET_USER "targetuser"

/**
 * VTE_SYSTEMD_PROPERTY_USER:
 *
 * A %VTE_PROPERTY_STRING property to specify the unix user name
 * the process issuing the sequence runs as.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_USER "user"

/**
 * VTE_SYSTEMD_PROPERTY_VM:
 *
 * A %VTE_PROPERTY_STRING property to specify the name of
 * the VM being invoked.
 *
 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.82
 */
#define VTE_SYSTEMD_PROPERTY_VM "vm"

G_END_DECLS
