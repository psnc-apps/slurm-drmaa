/* $Id: hpc_bash.c 13 2011-04-20 15:41:43Z mmamonski $ */
/*
 * HPC-BASH - part of the DRMAA utilities library
 * Poznan Supercomputing and Networking Center Copyright (C) 2010
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_CONFIG_H
#       include <config.h>
#endif

#define HPC_BASH_EXIT_OK (0)
#define HPC_BASH_EXIT_ERROR (5)
#define HPC_BASH_SCRIPT_NAME_TEMPLATE "/tmp/hpc-bash.XXXXXX"

typedef struct hpc_batch_job_s {
	char *walltime;
	char *native;
} hpc_batch_job_t;

static char *translate_hpc_bash_script(const char *orginal_script_name);

int main(int argc, char **argv)
{
	char *tmp_script_file_name = NULL;
	int child_pid = -1;

	if (argc < 2) {
		fprintf(stderr, "Missing script file\n");
		exit(HPC_BASH_EXIT_ERROR);
	}

	if ((tmp_script_file_name = translate_hpc_bash_script(argv[1])) == NULL) {
		exit(HPC_BASH_EXIT_ERROR);
	}


	if ((child_pid = fork()) > 0) {
		int status = -1;

		/* parent process*/
		if (waitpid(child_pid, &status, 0) == -1) {
			perror("waitpid() failed");
			exit(HPC_BASH_EXIT_ERROR);
		}

		/* remove temporary script file */
		/*unlink(tmp_script_file_name);*/

		if (WIFEXITED(status)) {
			exit(WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			exit(128 + WTERMSIG(status));
		} else {
			fprintf(stderr, "Ilegall wait status = %d\n", status);
			exit(HPC_BASH_EXIT_ERROR);
		}

	} else if (child_pid == 0) {
		/* child process */

		/*shift arguments by one */
		argv++;
		argc--;
		execv(tmp_script_file_name, argv);
		perror("execv() failed");
		exit(127);
	} else {
		perror("fork() failed");
	}

	return 1;
}

#define WRITE_STR2(str,length) \
	do { \
		if (write(out_fd, str, length) != length) { \
			perror("write() failed"); \
			goto out; \
		} \
	} while (0)

#define WRITE_STR(str) \
	do { \
		WRITE_STR2(str, strlen(str)); \
	} while (0)

char *
translate_hpc_bash_script(const char *orginal_script_name)
{
	char script_template[32] = HPC_BASH_SCRIPT_NAME_TEMPLATE;
	char *tmp_script_file = NULL;
	char line_buf[1024] = "";
	int out_fd = -1;
	int line_counter = 1;
	bool in_for_loop = false;
	bool in_batch_job = false;
	bool after_done = false;
	FILE *in_file = NULL;
	hpc_batch_job_t batch_job;

	memset(&batch_job, 0, sizeof(batch_job));

	/** TODO use RAGEL !!! **/
	if ((out_fd = mkstemp(script_template)) == -1) {
		perror("mkstemp() failed");
		goto out;
	}

	if ((fchmod(out_fd, S_IXUSR | S_IRUSR | S_IWUSR)) == -1 ) {
		perror("fchmod() failed");
		goto out;
	}

	if ((in_file = fopen(orginal_script_name, "r")) == NULL) {
		perror("fopen() failed");
		goto out;
	}

	while (fgets(line_buf, sizeof(line_buf), in_file) != NULL) {
		int line_length = strlen(line_buf);

		if (line_length > 0 && line_buf[line_length - 1] != '\n') {
			fprintf(stderr, "line %d: line to long or no NEW line at the end of file", line_counter);
			goto out;
		}

		if (line_counter == 1) {
			if (strstr(line_buf, "hpc-bash"))
				WRITE_STR("#!/bin/bash\n");
			else {
				fprintf(stderr,"WARNING: no hpc-bash shebang found!");
				WRITE_STR2(line_buf, line_length);
			}
		} else if (strstr(line_buf, "#pragma hpc-bash parallel for")) {
			in_for_loop = true;
		} else if (strstr(line_buf, "#pragma hpc-bash batch-job")){
			in_batch_job = true;
		} else {
			if (in_batch_job) {
				WRITE_STR("\t" DRMAA_DIR_BIN "/drmaa-run ");
				in_batch_job = false;
			}

			if (in_for_loop && strstr(line_buf,"done")) {
				WRITE_STR("\t}&\n");
				in_for_loop = false;
				after_done = true;
			}

			WRITE_STR2(line_buf, line_length);

			if (after_done) {
				WRITE_STR("wait\n");
				after_done = false;
			}

			if (in_for_loop && strstr(line_buf,"do")) {
				WRITE_STR("\t{\n");
			}
		}

		line_counter++;
	}

	tmp_script_file = strdup(script_template);
out:
	if (out_fd != -1) {
		close(out_fd);
		out_fd = -1;
	}

	if (in_file) {
		fclose(in_file);
		in_file = NULL;
	}

	if (!tmp_script_file) /* on fault delete partially generated file */
		unlink(script_template);

	return tmp_script_file;
}
