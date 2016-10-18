/* $Id: exec.c 1 2009-10-05 13:30:30Z mamonski $ */

/*
 * DRMAA Utils Library
 * Copyright (C) 2008-2012 Poznan Supercomputing and Networking Center
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 * This program is released under the GPL with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <drmaa_utils/exec.h>
#include <drmaa_utils/xmalloc.h>
#include <drmaa_utils/compat.h>
#include <drmaa_utils/exception.h>
#include <drmaa_utils/logging.h>
#include <drmaa_utils/thread.h>
#include <drmaa_utils/xmalloc.h>
#include <drmaa_utils/iter.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>

static void *stream_ripper(void *fdp);

static void fsd_pipe(int *fds)
{
	int ret, i, count = 0;
	int tmp[3];

	fsd_log_enter(("(%p)", (void*)fds));

	for (i = 0; i < 3; i++)
		tmp[i] = -1;

	for (i = 0; i < 2; i++)
		fds[i] = -1;

	ret = pipe(fds);
	if (ret == -1)
		fsd_exc_raise_sys(0);

	for (i = 0; i < 2; i++)
	  {
		while (fds[i] < 3)
		  {
			assert(count < 3);
			tmp[count++] = fds[i];

			if ((fds[i] = dup(fds[i])) == -1)
				ret = -1;
		  }
	  }

	for (i = 0; i < count; i++)
		close(tmp[i]);

	if (ret == -1)
	  {
		for (i = 0; i < 2; i++)
		  {
			if (fds[i] != -1)
				close(fds[i]);
		  }

		fsd_exc_raise_sys(0);
	  }
}


void
fsd_exec_async(const char *command, char **args, int *stdin_desc, int *stdout_desc, int *stderr_desc, pid_t *child_pid)
{
	pid_t pid;
	int stdin_pipe[2] = { -1, -1 };
	int stderr_pipe[2] = { -1, -1 };
	int stdout_pipe[2] = { -1, -1 };

	fsd_log_enter(("(%s)",command));

	assert(command);
	assert(args);
	assert(child_pid);

	if (stdin_desc)
		*stdin_desc = -1;
	if (stderr_desc)
		*stderr_desc = -1;
	if (stdout_desc)
		*stdout_desc = -1;
	*child_pid = -1;

	TRY
	  {
		fsd_pipe(stdin_pipe);
		fsd_pipe(stdout_pipe);
		fsd_pipe(stderr_pipe);

		if ((pid = fork()) == -1)
		  {
			fsd_exc_raise_sys(0);
		  }
		else if (pid > 0)
		  {
			if (stdin_desc != NULL)
			  {
				*stdin_desc = stdin_pipe[1];
			  }
			else
			  {
				close(stdin_pipe[1]);
			  }

			if (stdout_desc)
			  {
				*stdout_desc = stdout_pipe[0];
			  }
			else
			  {
				close(stdin_pipe[0]);
			  }

			if (stderr_desc)
			  {
				*stderr_desc = stderr_pipe[0];
			  }
			else
			  {
				close(stderr_pipe[0]);
			  }

			close(stdin_pipe[0]);
			close(stdout_pipe[1]);
			close(stderr_pipe[1]);

			*child_pid = pid;
		  }
		else
		  {
			/* child process */
			dup2(stdin_pipe[0], 0);
			close(stdin_pipe[1]);

			dup2(stdout_pipe[1], 1);
			close(stdout_pipe[0]);

			dup2(stderr_pipe[1], 2);
			close(stderr_pipe[0]);

			if (execvp(command, args) == -1)
			  {
				fsd_log_error(("Could not execute command: %s ", command));
				_exit(127);
			  }
		  }
	  }
	EXCEPT_DEFAULT
	  {
		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		close(stderr_pipe[0]);
		close(stderr_pipe[1]);

		fsd_exc_reraise();
	  }
	END_TRY
}

int
fsd_exec_wait(pid_t child_pid)
{
	int status = -1;
	int exit_code = -1;

	fsd_log_enter(("(%d)", (int)child_pid));

	if (waitpid(child_pid, &status, 0) != -1)
	  {
		if (WIFEXITED(status))
		  {
			exit_code = WEXITSTATUS(status);
			fsd_log_debug(("exit code = %d", exit_code));
		  }
		else
		  {
			fsd_exc_raise_fmt(FSD_ERRNO_INTERNAL_ERROR, "Spawned process pid = %d was aborted or signaled", (int) child_pid);
		  }
	  }
	else
	  {
		fsd_exc_raise_sys(0);
	  }

	return exit_code;
}

int
fsd_exec_sync(const char *command, char **args, const char *stdinb, char **stdoutb, char **stderrb)
{
	pid_t child_pid = -1;
	int stdin_d = -1;
	int stdout_d = -1;
	int stderr_d = -1;
	int *arg = NULL;
	size_t len = -1;
	int exit_code = -1;
	int ret = -1;
	fsd_thread_t err_t = 0;
	fsd_thread_t out_t = 0;

	assert(command);
	assert(args);
	assert(stdoutb);
	assert(stderrb);

	fsd_log_enter(("(%s)",stdinb));

	*stdoutb = NULL;
	*stderrb = NULL;

	TRY
	  {
		fsd_exec_async(command, args, &stdin_d, &stdout_d, &stderr_d, &child_pid);

		fsd_malloc(arg, sizeof(int));
		*arg = stdout_d;

		fsd_thread_create(&out_t, stream_ripper, arg);
		arg = NULL;

		fsd_malloc(arg, sizeof(int));
		*arg = stderr_d;

		fsd_thread_create(&err_t, stream_ripper, arg);
		arg = NULL;


		if (stdinb)
		  {
			len = strlen(stdinb);

			while (1) {
				ret = write(stdin_d, stdinb, len);

				if (ret == -1) {
					fsd_exc_raise_sys(0);
				}

				len -= ret;
				stdinb += ret;

				if (len == 0)
					break;
			}
		  }

		close(stdin_d);
		stdin_d = -1;

		exit_code = fsd_exec_wait(child_pid);

		fsd_thread_join(out_t, (void **) stdoutb);
		fsd_thread_join(err_t, (void **) stderrb);

	  }
	EXCEPT_DEFAULT
	  {
		if (arg)
			fsd_free(arg);

		if (stdin_d != -1)
			close(stdin_d);

		if (*stdoutb)
			fsd_free(*stdoutb);

		close(stdout_d);
		close(stderr_d);

		fsd_exc_reraise();
	  }
	END_TRY

	return exit_code;
}


void *
stream_ripper(void *fdp)
{
	int fd;
	fsd_iter_t *chunks = NULL;
	char *content = NULL;
	ssize_t total_bread = 0;
	char *p = NULL;

	fd = *((int *) fdp);
	fsd_free(fdp);

	fsd_log_enter(("(%d)", fd));

	TRY
	  {
		content = fsd_calloc(content, FSD_MAX_STREAM_BUFFER + 1, char);
		content[0] = '\0';

		while (1)
		  {
			ssize_t bread = -1;

			if (total_bread >= FSD_MAX_STREAM_BUFFER)
			  {
				fsd_log_error(("Stream buffer exceeded: %d", FSD_MAX_STREAM_BUFFER));
				fsd_exc_raise_fmt(FSD_ERRNO_INTERNAL_ERROR, "Stream buffer exceeded: %d",  FSD_MAX_STREAM_BUFFER);
			  }


			bread =  read(fd, content + total_bread , FSD_MAX_STREAM_BUFFER - total_bread);

			if (bread == -1)
			  {
				fsd_exc_raise_sys(0);
			  }
			else if (bread == 0)
			  {
				break;
			  }
			else 
			  {
				total_bread += bread;
			  }
		  }

		close(fd);

		content[total_bread] = '\0';

		fsd_thread_exit(content);
		return NULL;
	  }
	EXCEPT_DEFAULT
	  {
		close(fd);
		/* TODO sm_list_free(chunks, SM_TRUE); */
		fsd_thread_exit(NULL);
	  }
	END_TRY

	return NULL;
}
