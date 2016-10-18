/* $Id: drmaa_run.c 40 2012-09-10 09:23:08Z mmamonski $ */
/*
 * HPC-BASH - part of the DRMAA utilities library
 * Poznan Supercomputing and Networking Center Copyright (C) 2011
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


#include <drmaa_utils/drmaa.h>
#include <drmaa_utils/logging.h>
#include <drmaa_utils/exception.h>
#include <drmaa_utils/xmalloc.h>
#include <drmaa_utils/iter.h>

#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



#define DRMAA_LIBRARY_PATH "DRMAA_LIBRARY_PATH"

typedef int (*drmaa_init_function_t)(const char *, char *, size_t );
typedef int (*drmaa_exit_function_t)(char *, size_t );
typedef int (*drmaa_allocate_job_template_function_t)(drmaa_job_template_t **, char *, size_t);
typedef int (*drmaa_delete_job_template_function_t)(drmaa_job_template_t *, char *, size_t);
typedef int (*drmaa_set_attribute_function_t)(drmaa_job_template_t *, const char *, const char *, char *, size_t);
typedef int (*drmaa_get_attribute_function_t)(drmaa_job_template_t *, const char *, char *, size_t , char *, size_t);
typedef int (*drmaa_set_vector_attribute_function_t)(drmaa_job_template_t *, const char *, const char *[], char *, size_t);
typedef int (*drmaa_get_vector_attribute_function_t)(drmaa_job_template_t *, const char *, drmaa_attr_values_t **, char *, size_t);
typedef int (*drmaa_run_job_function_t)(char *, size_t, const drmaa_job_template_t *, char *, size_t);
typedef int (*drmaa_run_bulk_jobs_function_t)(drmaa_job_ids_t**, const drmaa_job_template_t*, int, int, int, char*, size_t);
typedef int (*drmaa_control_function_t)(const char *, int, char *, size_t);
typedef int (*drmaa_job_ps_function_t)(const char *, int *, char *, size_t);
typedef int (*drmaa_wait_function_t)(const char *, char *, size_t, int *, signed long, drmaa_attr_values_t **, char *, size_t);
typedef int (*drmaa_wifexited_function_t)(int *, int, char *, size_t);
typedef int (*drmaa_wexitstatus_function_t)(int *exit_status, int, char *, size_t);
typedef int (*drmaa_wifsignaled_function_t)(int *signaled, int, char *, size_t);
typedef int (*drmaa_wtermsig_function_t)(char *signal, size_t signal_len, int, char *, size_t);
typedef int (*drmaa_wcoredump_function_t)(int *core_dumped, int, char *, size_t);
typedef int (*drmaa_wifaborted_function_t)(int *aborted, int, char *, size_t);
typedef char (*drmaa_strerror_function_t)(int);
typedef int (*drmaa_get_contact_function_t)(char *, size_t , char *, size_t);
typedef int (*drmaa_version_function_t)(unsigned int *, unsigned int *, char *, size_t);
typedef int (*drmaa_get_DRM_system_function_t)(char *, size_t, char *, size_t);
typedef int (*drmaa_get_DRMAA_implementation_function_t)(char *, size_t, char *, size_t);

typedef struct
{
	drmaa_init_function_t init;
	drmaa_exit_function_t exit;
	drmaa_allocate_job_template_function_t allocate_job_template;
	drmaa_delete_job_template_function_t delete_job_template;
	drmaa_set_attribute_function_t set_attribute;
	drmaa_get_attribute_function_t get_attribute;
	drmaa_set_vector_attribute_function_t set_vector_attribute;
	drmaa_get_vector_attribute_function_t get_vector_attribute;
	drmaa_run_job_function_t run_job;
	drmaa_run_bulk_jobs_function_t run_bulk_jobs;
	drmaa_control_function_t control;
	drmaa_job_ps_function_t job_ps;
	drmaa_wait_function_t wait;
	drmaa_wifexited_function_t wifexited;
	drmaa_wexitstatus_function_t wexitstatus;
	drmaa_wifsignaled_function_t wifsignaled;
	drmaa_wtermsig_function_t wtermsig;
	drmaa_wcoredump_function_t wcoredump;
	drmaa_wifaborted_function_t wifaborted;
	drmaa_strerror_function_t strerror;
	drmaa_get_contact_function_t get_contact;
	drmaa_version_function_t version;
	drmaa_get_DRM_system_function_t get_DRM_system;
	drmaa_get_DRMAA_implementation_function_t get_DRMAA_implementation;
	void *handle;
} fsd_drmaa_api_t;

typedef struct
{
	char *native_specification;
	char *walltime;
	char *rusage_file;
	bool interactive;
	bool print_rusage;
	char *command;
	char **command_args;
	int command_argc;
	int start, end, step;
} fsd_drmaa_run_bulk_opt_t;


static fsd_drmaa_api_t load_drmaa();
static void unload_drmaa(fsd_drmaa_api_t *drmaa_api);

static fsd_drmaa_run_bulk_opt_t parse_args(int argc, char **argv);

static int run_and_wait(fsd_drmaa_api_t drmaa_api, fsd_drmaa_run_bulk_opt_t run_opt);

int main(int argc, char **argv)
{
	fsd_drmaa_api_t drmaa_api = { .handle = NULL };
	fsd_drmaa_run_bulk_opt_t run_opt;
	int status = -1;

	fsd_log_enter(("(argc=%d)", argc));

	TRY
	 {
		drmaa_api = load_drmaa();
		run_opt = parse_args(argc,argv);
		status = run_and_wait(drmaa_api, run_opt);
	 }
	EXCEPT_DEFAULT
	 {
		fsd_log_fatal(("Error"));
	 }
	FINALLY
	 {
		unload_drmaa(&drmaa_api);
	 }
	END_TRY

	exit(status);
}


fsd_drmaa_api_t load_drmaa()
{
	fsd_drmaa_api_t api;
	const char *path_to_drmaa = getenv(DRMAA_LIBRARY_PATH);

	fsd_log_enter(("(path=%s)", path_to_drmaa));

	memset(&api, 0, sizeof(api));

	if (!path_to_drmaa) {
#ifdef __APPLE__
		path_to_drmaa = DRMAA_DIR_PREFIX"/lib/libdrmaa.dylib";
#else
		path_to_drmaa = DRMAA_DIR_PREFIX"/lib/libdrmaa.so";
#endif
	}

	fsd_log_enter(("(final path=%s)", path_to_drmaa));

	api.handle = dlopen(path_to_drmaa, RTLD_LAZY | RTLD_GLOBAL);

	if (!api.handle) {
		const char *msg = dlerror();

		if (!msg)
			fsd_log_fatal(("Could not load DRMAA library: %s (DRMAA_LIBRARY_PATH=%s)\n", msg, path_to_drmaa));
		else
			fsd_log_fatal(("Could not load DRMAA library (DRMAA_LIBRARY_PATH=%s)\n", path_to_drmaa));

		fsd_exc_raise_code(FSD_ERRNO_INVALID_VALUE);
	}

	if ((api.init = (drmaa_init_function_t)dlsym(api.handle, "drmaa_init")) == 0)
		goto fault;
	if ((api.exit = (drmaa_exit_function_t)dlsym(api.handle, "drmaa_exit")) == 0)
		goto fault;
	if ((api.allocate_job_template = (drmaa_allocate_job_template_function_t)dlsym(api.handle, "drmaa_allocate_job_template")) == 0)
		goto fault;
	if ((api.delete_job_template = (drmaa_delete_job_template_function_t)dlsym(api.handle, "drmaa_delete_job_template")) == 0)
		goto fault;
	if ((api.set_attribute = (drmaa_set_attribute_function_t)dlsym(api.handle, "drmaa_set_attribute")) == 0)
		goto fault;
	if ((api.get_attribute = (drmaa_get_attribute_function_t)dlsym(api.handle, "drmaa_get_attribute")) == 0)
		goto fault;
	if ((api.set_vector_attribute = (drmaa_set_vector_attribute_function_t)dlsym(api.handle, "drmaa_set_vector_attribute")) == 0)
		goto fault;
	if ((api.get_vector_attribute = (drmaa_get_vector_attribute_function_t)dlsym(api.handle, "drmaa_get_vector_attribute")) == 0)
		goto fault;
	if ((api.run_job = (drmaa_run_job_function_t)dlsym(api.handle, "drmaa_run_job")) == 0)
		goto fault;
	if ((api.run_bulk_jobs = (drmaa_run_job_function_t)dlsym(api.handle, "drmaa_run_bulk_jobs")) == 0)
		goto fault;
	if ((api.control = (drmaa_control_function_t)dlsym(api.handle, "drmaa_control")) == 0)
		goto fault;
	if ((api.job_ps = (drmaa_job_ps_function_t)dlsym(api.handle, "drmaa_job_ps")) == 0)
		goto fault;
	if ((api.wait = (drmaa_wait_function_t)dlsym(api.handle, "drmaa_wait")) == 0)
		goto fault;
	if ((api.wifexited = (drmaa_wifexited_function_t)dlsym(api.handle, "drmaa_wifexited")) == 0)
		goto fault;
	if ((api.wexitstatus = (drmaa_wexitstatus_function_t)dlsym(api.handle, "drmaa_wexitstatus")) == 0)
		goto fault;
	if ((api.wifsignaled = (drmaa_wifsignaled_function_t)dlsym(api.handle, "drmaa_wifsignaled")) == 0)
		goto fault;
	if ((api.wtermsig = (drmaa_wtermsig_function_t)dlsym(api.handle, "drmaa_wtermsig")) == 0)
		goto fault;
	if ((api.wcoredump = (drmaa_wcoredump_function_t)dlsym(api.handle, "drmaa_wcoredump")) == 0)
		goto fault;
	if ((api.wifaborted = (drmaa_wifaborted_function_t)dlsym(api.handle, "drmaa_wifaborted")) == 0)
		goto fault;
	if ((api.strerror = (drmaa_strerror_function_t)dlsym(api.handle, "drmaa_strerror")) == 0)
		goto fault;
	if ((api.get_contact = (drmaa_get_contact_function_t)dlsym(api.handle, "drmaa_get_contact")) == 0)
		goto fault;
	if ((api.version = (drmaa_version_function_t)dlsym(api.handle, "drmaa_version")) == 0)
		goto fault;
	if ((api.get_DRM_system = (drmaa_get_DRM_system_function_t)dlsym(api.handle, "drmaa_get_DRM_system")) == 0)
		goto fault;
	if ((api.get_DRMAA_implementation = (drmaa_get_DRMAA_implementation_function_t)dlsym(api.handle, "drmaa_get_DRMAA_implementation")) == 0)
		goto fault;

	fsd_log_enter(("(library successfully loaded)"));

	return api;

fault:
	fsd_log_fatal(("Failed to dlsym DRMAA function"));

	if (api.handle)
		dlclose(api.handle);

	/*make invalid */
	memset(&api, 0, sizeof(api));

	return api;
}

void unload_drmaa(fsd_drmaa_api_t *drmaa_api_handle)
{
	fsd_log_enter(("()"));
	
	if (drmaa_api_handle->handle)
		dlclose(drmaa_api_handle->handle);
}

static fsd_drmaa_run_bulk_opt_t parse_args(int argc, char **argv)
{
	fsd_drmaa_run_bulk_opt_t options;

	memset(&options, 0, sizeof(options));

	argv++;
	argc--;

	while (argc >= 0 && argv[0][0] == '-')
	{

		if (strncmp(argv[0],"-native=", 8) == 0) {
			options.native_specification = argv[0] + 8;
			fsd_log_info(("native specification = '%s'", options.native_specification));
		} else {
			fsd_log_fatal(("unknown option: %s", argv[0]));
			exit(1); /* TODO exception */
		}

		argv++;
		argc--;
	}

	if (argc < 4) {
		fsd_log_fatal(("syntax error\ndrmaa-run-bulk {start} {end} {step} {command}"));
		exit(1);
	}

	options.start = atoi(argv[0]); argc--; argv++;
	options.end   = atoi(argv[0]); argc--; argv++;
	options.step  = atoi(argv[0]); argc--; argv++;
	
	/* TODO arg count check */
	options.command = argv[0];
	argv++;
	argc--;

	options.command_args = argv;
	options.command_argc = argc;

	return options;
}

int run_and_wait(fsd_drmaa_api_t api, fsd_drmaa_run_bulk_opt_t run_opt)
{
	char working_directory[1024] = ".";
	drmaa_job_template_t *jt = NULL;
	char errbuf[DRMAA_ERROR_STRING_BUFFER] = "";
	char stdin_name[1048] = "";
	char stdout_name[1048] = "";
	char stderr_name[1048] = "";
	/*char jobid[DRMAA_JOBNAME_BUFFER] = "";*/
	fsd_iter_t *jobids;
	/*char **jobids = NULL;*/
	int status;

	extern char **environ;


	if ((api.init(NULL, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS))
		goto fault;
	if ((api.allocate_job_template(&jt, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS))
		goto fault;


	if ((api.set_attribute(jt, DRMAA_REMOTE_COMMAND, run_opt.command, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)) goto fault;

	/*  args */
	if (run_opt.command_argc > 0) {
		char **args_vector = NULL;
		int i;


		fsd_calloc(args_vector, run_opt.command_argc + 1, char *);

		for (i = 0; i < run_opt.command_argc; i++) {
			args_vector[i] = run_opt.command_args[i];
		}

		if ((api.set_vector_attribute(jt, DRMAA_V_ARGV, (const char **) args_vector, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)) goto fault;
	}
	
	unsetenv("module");

	/*  environment */
	if ((api.set_vector_attribute(jt, DRMAA_V_ENV, (const char **) environ, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)) goto fault;

	/*  working directory */
	getcwd(working_directory, sizeof(working_directory));

	if ((api.set_attribute(jt, DRMAA_WD, working_directory, errbuf,	sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)) goto fault;

	if (run_opt.native_specification && (api.set_attribute(jt, DRMAA_NATIVE_SPECIFICATION, run_opt.native_specification, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)) goto fault;


	/* stdout.PID stderr.PID */
	sprintf(stdin_name, ":%s/.stdin.%u", working_directory, (unsigned int) getpid());

	/*
	sprintf(stdout_name, ":%s/.stdout.%u", working_directory, (unsigned int) getpid());
	sprintf(stderr_name, ":%s/.stderr.%u", working_directory, (unsigned int) getpid());
	*/

	sprintf(stdout_name, ":%s/.stdout.%u.%s", working_directory, (unsigned int) getpid(), "$drmaa_incr_ph$");
	sprintf(stderr_name, ":%s/.stderr.%u.%s", working_directory, (unsigned int) getpid(), "$drmaa_incr_ph$");

	/* read stdin */
	if (! isatty(0)) {
		int fd = -1;
		char buf[1024] = "";
		int bread = -1;

		if ((fd = open(stdin_name + 1, O_WRONLY | O_EXCL | O_CREAT, 0600)) < 0) {
			perror("open failed:");
			exit(3);
		}

		while ((bread = read(0, buf, sizeof(buf))) > 0 ) {
			write(fd, buf, bread);
		}

		close(fd);

		if (api.set_attribute(jt, DRMAA_INPUT_PATH, stdin_name, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS)
			goto fault;
	}

	if (api.set_attribute(jt, DRMAA_OUTPUT_PATH, stdout_name, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) goto fault;
	if (api.set_attribute(jt, DRMAA_ERROR_PATH, stderr_name, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) goto fault;

/*
	{
		int n_jobs, i;
		n_jobs = (run_opt.end - run_opt.start) / run_opt.step + 1;
		fsd_calloc(jobids, n_jobs + 1, char *);
		for (i = 0; i < n_jobs; i++) {
			fsd_calloc(jobids[i], 128 + 1, sizeof(char));
		}
	}
*/

	/* run */
	if (api.run_bulk_jobs(((drmaa_job_ids_t**)&jobids), jt, run_opt.start, run_opt.end, run_opt.step, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) {
		fsd_log_fatal(("Failed to submit a bulk job: %s ", errbuf));
		exit(2); /* TODO exception */
	}

	/* wait */

	{
		const char *jobid;
		int i;

		fsd_log_info(("waiting for %d jobs ...", jobids->len(jobids)));

		for (i = 0; i < jobids->len(jobids); ++i) {
			jobid = jobids->next(jobids);

			fsd_log_info(("waiting for job %s ...", jobid));

			if (api.wait(jobid, NULL, 0, &status, DRMAA_TIMEOUT_WAIT_FOREVER, NULL, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) {
				fsd_log_fatal(("Failed to wait for a job %s: %s ", jobid, errbuf));
				exit(132); /* TODO Exception */
			}

			fsd_log_info(("job %s finished...", jobid));
		}
	}

	/*  print stdout and stderr */
/*
	{
		char buf[1024] = "";
		struct stat stat_buf;
		int breads;
		int tries_count = 0;


		fsd_log_info(("opening stdout file: %s", stdout_name));
retry1:
		if (stat(stdout_name + 1, &stat_buf) == -1) {
			if (tries_count > 3)
				fsd_log_fatal(("Failed to get stdout (%s) of job %s", stdout_name + 1, jobid));
			else {
				sleep(3);
				tries_count++;
				goto retry1;
			}
		} else {
			int fd = open(stdout_name + 1, O_RDONLY);

			if (fd < 0) { perror("open failed"); exit(3); }

			fsd_log_info(("opened stdout file:%s", stdout_name));

			while ((breads = read(fd, buf, sizeof(buf))) > 0) {
				write(1, buf, breads);
			}

			close(fd);

			unlink(stdout_name + 1);
		}
retry2:
		if (stat(stderr_name + 1, & stat_buf) == -1) {
			if (tries_count > 3)
				fsd_log_fatal(("Failed to get stderr (%s) of job %s\n", stderr_name + 1, jobid));
			else {
				sleep(3);
				tries_count++;
				goto retry2;
			}
		} else {
			int fd = open(stderr_name + 1, O_RDONLY);

			if (fd < 0) { perror("open failed"); exit(3); }

			while ((breads = read(fd, buf, sizeof(buf))) > 0) {
				write(2, buf, breads);
			}

			close(fd);

			unlink(stderr_name + 1);
		}

	}
*/

	if (strlen(stdin_name) != 0) {
		unlink(stdin_name + 1);
	}

	/* exit with appropriate code */
	{
		int exited = 0;
		int signaled = 0;
		int exit_status = 1;


		if (api.wifexited(&exited, status, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) {
			goto fault;
		}

		if (api.wifsignaled(&signaled, status, errbuf, sizeof(errbuf) - 1) != DRMAA_ERRNO_SUCCESS) {
			goto fault;
		}

		if (exited) {
			(void) api.wexitstatus(&exit_status, status, errbuf, sizeof(errbuf) - 1);
		} else {
			if (signaled) {
				exit_status = 128;
			} else {
				exit_status = 1;
			}
		}

		api.exit(errbuf, sizeof(errbuf) - 1);
		fsd_log_info(("exit_status = %d", exit_status));
		return exit_status;
	}
fault:
	fsd_log_fatal(("Error"));
	return 1;
}

