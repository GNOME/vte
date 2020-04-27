/* gspawn.c - Process launching
 *
 *  Copyright 2000 Red Hat, Inc.
 *  g_execvpe implementation based on GNU libc execvp:
 *   Copyright 1991, 92, 95, 96, 97, 98, 99 Free Software Foundation, Inc.
 *
 * GLib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GLib; see the file COPYING.LIB.  If not, write
 * to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>   /* for fdwalk */
#include <dirent.h>

#include <glib-unix.h>
#include <gio/gio.h>

#include "vtespawn.hh"
#include "vteutils.h"  /* for strchrnul on non-GNU systems */
#include "reaper.hh"

#define _(s) g_dgettext("glib20", s)

static gssize
write_all (int fd, gconstpointer vbuf, gsize to_write)
{
  char *buf = (char *) vbuf;

  while (to_write > 0)
    {
      gssize count = write (fd, buf, to_write);
      if (count < 0)
        {
          if (errno != EINTR)
            return FALSE;
        }
      else
        {
          to_write -= count;
          buf += count;
        }
    }

  return TRUE;
}

void
_vte_write_err (int fd,
                int msg)
{
        int data[2] = {msg, errno};

        write_all(fd, data, sizeof(data));
}

G_GNUC_UNUSED static int
close_func (void *data, int fd)
{
  if (fd >= GPOINTER_TO_INT (data))
    (void) close (fd);

  return 0;
}

/* Based on execvp from GNU C Library */

static void
script_execute (const char *file,
                char      **argv,
                char      **envp)
{
  /* Count the arguments.  */
  int argc = 0;
  while (argv[argc])
    ++argc;

  /* Construct an argument list for the shell.  */
  {
    char **new_argv;

    new_argv = g_new0 (char*, argc + 2); /* /bin/sh and NULL */

    new_argv[0] = (char *) "/bin/sh";
    new_argv[1] = (char *) file;
    while (argc > 0)
      {
	new_argv[argc + 1] = argv[argc];
	--argc;
      }

    /* Execute the shell. */
    if (envp)
      execve (new_argv[0], new_argv, envp);
    else
      execv (new_argv[0], new_argv);

    g_free (new_argv);
  }
}

int
_vte_execute (const char *file,
              char      **argv,
              char      **envp,
              bool        search_path,
              bool        search_path_from_envp)
{
  if (*file == '\0')
    {
      /* We check the simple case first. */
      errno = ENOENT;
      return -1;
    }

  if (!(search_path || search_path_from_envp) || strchr (file, '/') != NULL)
    {
      /* Don't search when it contains a slash. */
      if (envp)
        execve (file, argv, envp);
      else
        execv (file, argv);

      if (errno == ENOEXEC)
	script_execute (file, argv, envp);
    }
  else
    {
      gboolean got_eacces = 0;
      const char *path, *p;
      char *name, *freeme;
      gsize len;
      gsize pathlen;

      path = NULL;
      if (search_path_from_envp)
        path = g_environ_getenv (envp, "PATH");
      if (search_path && path == NULL)
        path = g_getenv ("PATH");

      if (path == NULL)
	{
	  /* There is no 'PATH' in the environment.  The default
	   * search path in libc is the current directory followed by
	   * the path 'confstr' returns for '_CS_PATH'.
           */

          /* In GLib we put . last, for security, and don't use the
           * unportable confstr(); UNIX98 does not actually specify
           * what to search if PATH is unset. POSIX may, dunno.
           */

          path = "/bin:/usr/bin:.";
	}

      len = strlen (file) + 1;
      pathlen = strlen (path);
      freeme = name = (char*)g_malloc (pathlen + len + 1);

      /* Copy the file name at the top, including '\0'  */
      memcpy (name + pathlen + 1, file, len);
      name = name + pathlen;
      /* And add the slash before the filename  */
      *name = '/';

      p = path;
      do
	{
	  char *startp;

	  path = p;
	  p = strchrnul (path, ':');

	  if (p == path)
	    /* Two adjacent colons, or a colon at the beginning or the end
             * of 'PATH' means to search the current directory.
             */
	    startp = name + 1;
	  else
            startp = (char*)memcpy (name - (p - path), path, p - path);

	  /* Try to execute this name.  If it works, execv will not return.  */
          if (envp)
            execve (startp, argv, envp);
          else
            execv (startp, argv);

	  if (errno == ENOEXEC)
	    script_execute (startp, argv, envp);

	  switch (errno)
	    {
	    case EACCES:
	      /* Record the we got a 'Permission denied' error.  If we end
               * up finding no executable we can use, we want to diagnose
               * that we did find one but were denied access.
               */
	      got_eacces = TRUE;

              /* FALL THRU */

	    case ENOENT:
#ifdef ESTALE
	    case ESTALE:
#endif
#ifdef ENOTDIR
	    case ENOTDIR:
#endif
	      /* Those errors indicate the file is missing or not executable
               * by us, in which case we want to just try the next path
               * directory.
               */
	      break;

	    case ENODEV:
	    case ETIMEDOUT:
	      /* Some strange filesystems like AFS return even
	       * stranger error numbers.  They cannot reasonably mean anything
	       * else so ignore those, too.
	       */
	      break;

	    default:
	      /* Some other error means we found an executable file, but
               * something went wrong executing it; return the error to our
               * caller.
               */
              g_free (freeme);
	      return -1;
	    }
	}
      while (*p++ != '\0');

      /* We tried every element and none of them worked.  */
      if (got_eacces)
	/* At least one failure was due to permissions, so report that
         * error.
         */
        errno = EACCES;

      g_free (freeme);
    }

  /* Return the error from the last attempt (probably ENOENT).  */
  return -1;
}
