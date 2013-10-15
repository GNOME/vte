/*
 * Copyright © 2013 Christian Persch
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

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for O_TMPFILE */
#endif

#include "vteutils.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>

/* Temporary define until glibc release catches up */
#ifdef __linux__
#ifndef O_TMPFILE
#ifndef __O_TMPFILE
#define __O_TMPFILE     020000000
#endif
#define O_TMPFILE (__O_TMPFILE | O_DIRECTORY)
#endif
#endif

int
_vte_mkstemp (void)
{
        int fd;
        gchar *file_name;

#ifdef O_TMPFILE
        fd = open (g_get_tmp_dir (),
                   O_TMPFILE | O_EXCL | O_RDWR | O_NOATIME,
                   0600);
        if (fd != -1)
                goto done;

        /* Try again with g_file_open_tmp */
#endif

        fd = g_file_open_tmp ("vteXXXXXX", &file_name, NULL);
        if (fd == -1)
                return -1;

        unlink (file_name);
        g_free (file_name);

#ifdef O_NOATIME
        do { } while (fcntl (fd, F_SETFL, O_NOATIME) == -1 && errno == EINTR);
#endif

#ifdef O_TMPFILE
 done:
#endif

        return fd;
}
