// Copyright © 2023, 2024, 2025 Christian Persch
//
// This library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#if !defined (__VTE_VTE_H_INSIDE__) && !defined (VTE_COMPILATION)
#error "Only <vte/vte.h> can be included directly."
#endif

#include <glib.h>

#include "vtemacros.h"
#include "vteenums.h"

G_BEGIN_DECLS

/**
 * VTE_SYSTEMD_PROPERTY_BOOT_ID:
 *
 * A %VTE_PROPERTY_UUID property to specify the boot ID
 * (i.e. `/proc/sys/kernel/random/boot_id`) of the system
 * the process issuing the sequence runs on.

 * This property is only available for
 * %VTE_SYSTEMD_CONTEXT_OPERATION_START contexts.
 *
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
 */
#define VTE_SYSTEMD_PROPERTY_CONTAINER "container"

/**
 * VTE_SYSTEMD_PROPERTY_CONTEXT_ID:
 *
 * A %VTE_PROPERTY_UUID property to specify the context ID.
 *
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
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
 * Since: 0.86
 */
#define VTE_SYSTEMD_PROPERTY_VM "vm"

G_END_DECLS
