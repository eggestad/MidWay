/* Copyright (C) 1991, 92, 95, 96, 97, 98, 99 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* Copyright (C) 2002 Terje Eggestad  on the diff from glibc, LGPL as
   described above is still in effect */
 
/* well this was a part og glibc, 2.2.4 posix/execvp.c to be exact.
   since glibc is missing an execvpe, thus an exec thaht take both
   argv vector as well as the envv vector AND perform a PATH search we
   make one as close to glibc behavior as possible.  

   Terje */

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <paths.h>
#include <string.h>

#include <stdio.h>
#define DEBUG(m...) fprintf(stderr, m)

/* The file is accessible but it is not an executable file.  Invoke
   the shell to interpret it as a script.  */
static void
script_execute (const char *file, char *const argv[], char *const envp[])
{
  /* Count the arguments.  */
  int argc = 0;
  while (argv[argc++])
    ;

  /* Construct an argument list for the shell.  */
  {
    char *new_argv[argc + 1];
    new_argv[0] = (char *) _PATH_BSHELL;
    new_argv[1] = (char *) file;
    while (argc > 1)
      {
	new_argv[argc] = argv[argc - 1];
	--argc;
      }

    /* Execute the shell.  */
    execve (new_argv[0], new_argv, envp);
  }
}

char * getpath(char * const envp[])
{
  int i;
  for (i = 0; envp[i] != NULL; i++) {
    if (strncmp("PATH=", envp[i], 5) == 0) return &envp[i][5];
  };

  return NULL;
};

/* Execute FILE, searching in the `PATH' environment variable if it contains
   no slashes, with arguments ARGV and environment from `environ'.  */
int execvpe (const char *file, char *const argv[], char *const envp[])
{
  if (*file == '\0')
    {
      /* We check the simple case first. */
      errno  = ENOENT;
      return -1;
    }

  if (strchr (file, '/') != NULL)
    {
      /* Don't search when it contains a slash.  */
      execve (file, argv, envp);

      if (errno == ENOEXEC)
	script_execute (file, argv, envp);
    }
  else
    {
      int got_eacces = 0;
      char *path, *p, *name;
      size_t len;
      size_t pathlen;

      path = getpath (envp);
      if (path == NULL)
	{
	  /* There is no `PATH' in the environment.
	     The default search path is the current directory
	     followed by the path `confstr' returns for `_CS_PATH'.  */
	  len = confstr (_CS_PATH, (char *) NULL, 0);
	  path = (char *) alloca (1 + len);
	  path[0] = ':';
	  (void) confstr (_CS_PATH, path + 1, len);
	}

      DEBUG("path = %s\n", path);

      len = strlen (file) + 1;
      pathlen = strlen (path);
      name = alloca (pathlen + len + 1);
      /* Copy the file name at the top.  */
      name = (char *) memcpy (name + pathlen + 1, file, len);
      /* And add the slash.  */
      *--name = '/';

      p = path;
      do
	{
	  char *startp;

	  path = p;
	  p = strchr (path, ':');

	  /*in glibc we use __strchrnul which return a pointer to teh
            trailing NUL if no occurence of ':' */
	  if (p == NULL) {
	    p = path + strlen(path);
	  };

	  if (p == path)
	    /* Two adjacent colons, or a colon at the beginning or the end
	       of `PATH' means to search the current directory.  */
	    startp = name + 1;
	  else
	    startp = (char *) memcpy (name - (p - path), path, p - path);

	  DEBUG( "exec: %s\n", startp);
	  /* Try to execute this name.  If it works, execv will not return.  */
	  execve (startp, argv, envp);

	  if (errno == ENOEXEC)
	    script_execute (startp, argv, envp);

	  switch (errno)
	    {
	    case EACCES:
	      /* Record the we got a `Permission denied' error.  If we end
		 up finding no executable we can use, we want to diagnose
		 that we did find one but were denied access.  */
	      got_eacces = 1;
	    case ENOENT:
	    case ESTALE:
	    case ENOTDIR:
	      /* Those errors indicate the file is missing or not executable
		 by us, in which case we want to just try the next path
		 directory.  */
	      break;

	    default:
	      /* Some other error means we found an executable file, but
		 something went wrong executing it; return the error to our
		 caller.  */
	      return -1;
	    }
	}
      while (*p++ != '\0');

      /* We tried every element and none of them worked.  */
      if (got_eacces)
	/* At least one failure was due to permissions, so report that
           error.  */
	errno = EACCES;
    }

  /* Return the error from the last attempt (probably ENOENT).  */
  return -1;
}
