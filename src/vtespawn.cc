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

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */

#include <glib-unix.h>
#include <gio/gio.h>

#if defined(__linux__) || defined(__DragonFly__)
#include <sys/syscall.h>  /* for syscall and SYS_getdents64 */
#endif

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
set_cloexec (void *data, int fd)
{
  if (fd >= GPOINTER_TO_INT (data))
    fd_set_cloexec (fd);

  return 0;
}

G_GNUC_UNUSED static int
close_func (void *data, int fd)
{
  if (fd >= GPOINTER_TO_INT (data))
    (void) close (fd);

  return 0;
}

#ifdef __linux__
struct linux_dirent64
{
  guint64        d_ino;    /* 64-bit inode number */
  guint64        d_off;    /* 64-bit offset to next structure */
  unsigned short d_reclen; /* Size of this dirent */
  unsigned char  d_type;   /* File type */
  char           d_name[]; /* Filename (null-terminated) */
};

static int
filename_to_fd (const char *p)
{
  char c;
  int fd = 0;
  const int cutoff = G_MAXINT / 10;
  const int cutlim = G_MAXINT % 10;

  if (*p == '\0')
    return -1;

  while ((c = *p++) != '\0')
    {
      if (!g_ascii_isdigit (c))
        return -1;
      c -= '0';

      /* Check for overflow. */
      if (fd > cutoff || (fd == cutoff && c > cutlim))
        return -1;

      fd = fd * 10 + c;
    }

  return fd;
}
#endif

#ifndef HAVE_FDWALK
int
fdwalk (int (*cb)(void *data, int fd), void *data);

int
fdwalk (int (*cb)(void *data, int fd), void *data)
{
  /* Fallback implementation of fdwalk. It should be async-signal safe, but it
   * may be slow on non-Linux operating systems, especially on systems allowing
   * very high number of open file descriptors.
   */
  int open_max;
  int fd;
  int res = 0;

#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rl;
#endif

#ifdef __linux__
  /* Avoid use of opendir/closedir since these are not async-signal-safe. */
  int dir_fd = open ("/proc/self/fd", O_RDONLY | O_DIRECTORY);
  if (dir_fd >= 0)
    {
      char buf[4096];
      int pos, nread;
      struct linux_dirent64 *de;

      while ((nread = syscall (SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0)
        {
          for (pos = 0; pos < nread; pos += de->d_reclen)
            {
              de = (struct linux_dirent64 *)(buf + pos);

              fd = filename_to_fd (de->d_name);
              if (fd < 0 || fd == dir_fd)
                  continue;

              if ((res = cb (data, fd)) != 0)
                  break;
            }
        }

      close (dir_fd);
      return res;
    }

  /* If /proc is not mounted or not accessible we fall back to the old
   * rlimit trick */

#endif

#ifdef HAVE_SYS_RESOURCE_H

  if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_max != RLIM_INFINITY)
      open_max = rl.rlim_max;
  else
#endif
      open_max = sysconf (_SC_OPEN_MAX);

  for (fd = 0; fd < open_max; fd++)
      if ((res = cb (data, fd)) != 0)
          break;

  return res;
}
#endif /* HAVE_FDWALK */

void
_vte_cloexec_from(int fd)
{
        fdwalk(set_cloexec, GINT_TO_POINTER(fd));
}

bool
_vte_read_ints(int      fd,
               int*    buf,
               int     n_ints_in_buf,
               int    *n_ints_read,
               int     timeout,
               GPollFD *cancellable_pollfd,
               GError **error)
{
  gsize bytes = 0;
  GPollFD pollfds[2];
  guint n_pollfds;
  int64_t start_time = 0;

  if (timeout >= 0 || cancellable_pollfd != nullptr)
    {
      if (fd_set_nonblocking(fd) < 0)
        {
          int errsv = errno;
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       _("Failed to set pipe nonblocking: %s"), g_strerror (errsv));
          return false;
        }

      pollfds[0].fd = fd;
      pollfds[0].events = G_IO_IN | G_IO_HUP | G_IO_ERR;
      n_pollfds = 1;

      if (cancellable_pollfd != NULL)
        {
          pollfds[1] = *cancellable_pollfd;
          n_pollfds = 2;
        }
    }
  else
    n_pollfds = 0;

  if (timeout >= 0)
    start_time = g_get_monotonic_time ();

  while (true)
    {
      gssize chunk;

      if (bytes >= sizeof(int)*2)
        break; /* give up, who knows what happened, should not be
                * possible.
                */

    again:
      if (n_pollfds != 0)
        {
          int r;

          pollfds[0].revents = pollfds[1].revents = 0;

          r = g_poll (pollfds, n_pollfds, timeout);

          /* Update timeout */
          if (timeout >= 0)
            {
              timeout -= (g_get_monotonic_time () - start_time) / 1000;
              if (timeout < 0)
                timeout = 0;
            }

          if (r < 0 && errno == EINTR)
            goto again;
          if (r < 0)
            {
              int errsv = errno;
              g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                           _("poll error: %s"), g_strerror (errsv));
              return false;
            }
          if (r == 0)
            {
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                                   _("Operation timed out"));
              return false;
            }

          /* If the passed-in poll FD becomes readable, that's the signal
           * to cancel the operation. We do NOT actually read from its FD!
           */
          if (n_pollfds == 2 && pollfds[1].revents)
            {
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                   _("Operation was cancelled"));
              return false;
            }

          /* Now we know we can try to read from the child */
        }

      chunk = read (fd,
                    ((char*)buf) + bytes,
                    sizeof(int) * n_ints_in_buf - bytes);
      if (chunk < 0 && errno == EINTR)
        goto again;

      if (chunk < 0)
        {
          int errsv = errno;

          /* Some weird shit happened, bail out */
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       _("Failed to read from child pipe (%s)"),
                       g_strerror (errsv));

          return false;
        }
      else if (chunk == 0)
        break; /* EOF */
      else /* chunk > 0 */
	bytes += chunk;
    }

  *n_ints_read = int(bytes / sizeof(int));

  return true;
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
