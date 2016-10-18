/* $Id: fsd_session.c 36 2012-01-08 15:37:52Z mmamonski $ */
/*
 * FedStage DRMAA utilities library
 * Copyright (C) 2006-2008  FedStage Systems
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <drmaa_utils/conf.h>
#include <drmaa_utils/drmaa.h>
#include <drmaa_utils/iter.h>
#include <drmaa_utils/job.h>
#include <drmaa_utils/session.h>

#ifndef lint
static char rcsid[]
#	ifdef __GNUC__
		__attribute__ ((unused))
#	endif
	= "$Id: fsd_session.c 36 2012-01-08 15:37:52Z mmamonski $";
#endif


static void
fsd_drmaa_session_release( fsd_drmaa_session_t *self );

static void
fsd_drmaa_session_destroy(
		fsd_drmaa_session_t *self );

static void
fsd_drmaa_session_destroy_nowait( fsd_drmaa_session_t *self );

static char*
fsd_drmaa_session_run_job(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt
		);

static fsd_iter_t*
fsd_drmaa_session_run_bulk(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt,
		int start, int end, int incr
		);

static void
fsd_drmaa_session_control_job(
		fsd_drmaa_session_t *self,
		const char *job_id, int action
		);

static void
fsd_drmaa_session_job_ps(
		fsd_drmaa_session_t *self,
		const char *job_id, int *remote_ps
		);

static void
fsd_drmaa_session_synchronize(
		fsd_drmaa_session_t *self,
		const char **input_job_ids, const struct timespec *timeout,
		bool dispose
		);

static char*
fsd_drmaa_session_wait(
		fsd_drmaa_session_t *self,
		const char *job_id, const struct timespec *timeout,
		int *status, fsd_iter_t **rusage
		);

static fsd_job_t *
fsd_drmaa_session_new_job(
		fsd_drmaa_session_t *self,
		const char *job_id
		);

static char*
fsd_drmaa_session_run_impl(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt, int bulk_incr
		);

static void
fsd_drmaa_session_wait_for_single_job(
		fsd_drmaa_session_t *self,
		const char *job_id, const struct timespec *timeout,
		int *status, fsd_iter_t **rusage, bool dispose
		);

static char*
fsd_drmaa_session_wait_for_any_job(
		fsd_drmaa_session_t *self,
		const struct timespec *timeout,
		int *status, fsd_iter_t **rusage,
		bool dispose
		);

static void
fsd_drmaa_session_wait_for_job_status_change(
		fsd_drmaa_session_t *self,
		fsd_cond_t *wait_condition,
		fsd_mutex_t *mutex,
		const struct timespec *timeout
		);

static void*
fsd_drmaa_session_wait_thread( fsd_drmaa_session_t *self );

static void
fsd_drmaa_session_stop_wait_thread( fsd_drmaa_session_t *self );

static void
fsd_drmaa_session_update_all_jobs_status( fsd_drmaa_session_t *self );

static char**
fsd_drmaa_session_get_submited_job_ids(
		fsd_drmaa_session_t *self
		);

static fsd_job_t*
fsd_drmaa_session_get_job(
		fsd_drmaa_session_t *self, const char *job_id
		);

static void
fsd_drmaa_session_load_configuration(
		fsd_drmaa_session_t *self, const char *basename
		);

static void
fsd_drmaa_session_read_configuration(
		fsd_drmaa_session_t *self,
		const char *filename, bool must_exist,
		const char *configuration, size_t config_len
		);

static void
fsd_drmaa_session_apply_configuration(
		fsd_drmaa_session_t *self
		);



fsd_drmaa_session_t *
fsd_drmaa_session_new( const char *contact )
{
	fsd_drmaa_session_t *volatile self = NULL;

	fsd_log_enter(( "(%s)", contact ));
	TRY
	 {
		fsd_malloc( self, fsd_drmaa_session_t );

		self->release = fsd_drmaa_session_release;
		self->destroy = fsd_drmaa_session_destroy;
		self->destroy_nowait = fsd_drmaa_session_destroy_nowait;
		self->run_job = fsd_drmaa_session_run_job;
		self->run_bulk = fsd_drmaa_session_run_bulk;
		self->control_job = fsd_drmaa_session_control_job;
		self->job_ps = fsd_drmaa_session_job_ps;
		self->synchronize = fsd_drmaa_session_synchronize;
		self->wait = fsd_drmaa_session_wait;
		self->new_job = fsd_drmaa_session_new_job;
		self->run_impl = fsd_drmaa_session_run_impl;
		self->wait_for_single_job = fsd_drmaa_session_wait_for_single_job;
		self->wait_for_any_job = fsd_drmaa_session_wait_for_any_job;
		self->wait_for_job_status_change =
			fsd_drmaa_session_wait_for_job_status_change;
		self->wait_thread = fsd_drmaa_session_wait_thread;
		self->stop_wait_thread = fsd_drmaa_session_stop_wait_thread;
		self->update_all_jobs_status = fsd_drmaa_session_update_all_jobs_status;
		self->get_submited_job_ids = fsd_drmaa_session_get_submited_job_ids;
		self->get_job = fsd_drmaa_session_get_job;
		self->load_configuration = fsd_drmaa_session_load_configuration;
		self->read_configuration = fsd_drmaa_session_read_configuration;
		self->apply_configuration = fsd_drmaa_session_apply_configuration;

		self->ref_cnt = 1;
		self->destroy_requested = false;
		self->contact = NULL;
		self->jobs = NULL;
		self->configuration = NULL;
		self->pool_delay.tv_sec = 10;
		self->pool_delay.tv_nsec = 0;
		self->cache_job_state = 0;
		self->enable_wait_thread = false;
		self->job_categories = NULL;
		self->missing_jobs = FSD_REVEAL_MISSING_JOBS;
		self->wait_thread_started = false;
		self->wait_thread_run_flag = false;

		fsd_mutex_init( &self->mutex );
		fsd_cond_init( &self->wait_condition );
		fsd_cond_init( &self->destroy_condition );
		fsd_mutex_init( &self->drm_connection_mutex );
		self->jobs = fsd_job_set_new();
		self->contact = fsd_strdup( contact );
		
	 }
	EXCEPT_DEFAULT
	 {
		if( self != NULL )
			self->destroy( self );
		fsd_exc_reraise();
	 }
	END_TRY

	return self;
}


void
fsd_drmaa_session_release( fsd_drmaa_session_t *self )
{
	fsd_mutex_lock( &self->mutex );
	self->ref_cnt--;
	fsd_assert( self->ref_cnt > 0 );
	if( self->ref_cnt == 1 )
		fsd_cond_broadcast( &self->destroy_condition );
	fsd_mutex_unlock( &self->mutex );
}


void
fsd_drmaa_session_destroy( fsd_drmaa_session_t *self )
{
	bool already_destroying = false;

	fsd_log_enter(( "" ));
	fsd_mutex_lock( &self->mutex );
	TRY
	 {
		if( self->destroy_requested )
			already_destroying = true;
		else
		 {
			self->destroy_requested = true;
			fsd_cond_broadcast( &self->wait_condition );
		 }
	 }
	FINALLY
	 { fsd_mutex_unlock( &self->mutex ); }
	END_TRY

	if( already_destroying )
	 { /* XXX: actually it can not happen in current implementation
				when using DRMAA API */
		self->release( self );
		fsd_exc_raise_code( FSD_DRMAA_ERRNO_NO_ACTIVE_SESSION );
	 }

	self->jobs->signal_all( self->jobs );

	fsd_mutex_lock( &self->mutex );
	TRY
	 {
		while( self->ref_cnt > 1 )
			fsd_cond_wait( &self->destroy_condition, &self->mutex );
		fsd_log_debug(("started = %d  run_flag = %d", self->wait_thread_started, self->wait_thread_run_flag ));
		if( self->wait_thread_started )
			self->stop_wait_thread( self );
	 }
	FINALLY
	 { fsd_mutex_unlock( &self->mutex ); }
	END_TRY

	self->destroy_nowait( self );
	fsd_log_return(( "" ));
}


void
fsd_drmaa_session_destroy_nowait( fsd_drmaa_session_t *self )
{
	fsd_log_enter(( "" ));
	fsd_conf_dict_destroy( self->configuration );
	fsd_free( self->contact );

	if( self->jobs )
		self->jobs->destroy( self->jobs );

	fsd_mutex_destroy( &self->mutex );
	fsd_cond_destroy( &self->wait_condition );
	fsd_cond_destroy( &self->destroy_condition );
	fsd_mutex_destroy( &self->drm_connection_mutex );

	fsd_free( self );
	fsd_log_return(( "" ));
}


char *
fsd_drmaa_session_run_job(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt )
{
	return self->run_impl( self, jt, -1 );
}


fsd_iter_t *
fsd_drmaa_session_run_bulk(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt,
		int start, int end, int incr )
{
	volatile unsigned n_jobs;
	char **volatile result = NULL;

	if( incr > 0 )
		n_jobs = (end-start) / incr + 1;
	else
		n_jobs = (start-end) / -incr + 1;

	TRY
	 {
		unsigned i;
		int idx;
		fsd_calloc( result, n_jobs + 1, char* );
		for( i=0, idx=start;  i < n_jobs;  i++, idx+=incr )
			result[i] = self->run_impl( self, jt, idx );
	 }
	EXCEPT_DEFAULT
	 {
		if( result )
			fsd_free_vector( result );
		fsd_exc_reraise();
	 }
	END_TRY

	return fsd_iter_new( result, -1 );
}


void
fsd_drmaa_session_control_job(
		fsd_drmaa_session_t *self,
		const char *job_id, int action )
{
	char **job_ids = NULL;
	char **i;

	TRY
	 {
		if( !strcmp( job_id, DRMAA_JOB_IDS_SESSION_ALL ) )
			job_ids = self->get_submited_job_ids( self );
		else
		 {
			fsd_calloc( job_ids, 2, char* );
			job_ids[0] = fsd_strdup( job_id );
		 }

		for( i = job_ids;  *i != NULL;  i++ )
		 {
			fsd_job_t *job = NULL;
			TRY
			 {
				job = self->get_job( self, *i );
				if( job == NULL )
				 {
					if( !strcmp( job_id, DRMAA_JOB_IDS_SESSION_ALL ) )
					 { /* job was just removed from session */ }
					else
						job = self->new_job( self, *i );
				 }
				if( job )
					job->control( job, action );
			 }
			FINALLY
			 {
				if ( job )
				 job->release( job );
			 }
			END_TRY
		 }
	 }
	FINALLY
	 {
		fsd_free_vector( job_ids );
	 }
	END_TRY
}


void
fsd_drmaa_session_job_ps(
		fsd_drmaa_session_t *self, const char *job_id, int *remote_ps )
{
	fsd_job_t *volatile job = NULL;
	TRY
	 {
		job = self->get_job( self, job_id );
		if( job == NULL )
		 {
			fsd_log_info(( "job_ps: recreating job object: %s", job_id ));
			job = self->new_job( self, job_id );
		 }
		fsd_log_debug((" job->last_update_time = %u",  (unsigned int)job->last_update_time));
		if( time(NULL) - job->last_update_time >= self->cache_job_state
				|| job->state == DRMAA_PS_UNDETERMINED ) 
		  {
			fsd_log_debug(("updating status of job: %s ", job_id));
			job->update_status( job );
			job->last_update_time = time(NULL);
		  }
		*remote_ps = job->state;
	 }
	FINALLY
	 {
		if( job )
			job->release( job );
	 }
	END_TRY
}


void
fsd_drmaa_session_synchronize(
		fsd_drmaa_session_t *self,
		const char **input_job_ids, const struct timespec *timeout,
		bool dispose
		)
{
	volatile bool wait_for_all = false;
	char **volatile job_ids_buf = NULL;
	const char **job_ids = NULL;
	const char **i;

	fsd_log_enter(( "(job_ids={...}, timeout=..., dispose=%d)",
			(int)dispose ));

	if( input_job_ids == NULL )
		fsd_exc_raise_code( FSD_ERRNO_INVALID_ARGUMENT );

	TRY
	 {
		for( i = input_job_ids;  *i != NULL;  i++ )
			if( !strcmp(*i, DRMAA_JOB_IDS_SESSION_ALL) )
				wait_for_all = true;

		if( wait_for_all )
		 {
			job_ids_buf = self->get_submited_job_ids( self );
			job_ids = (const char**)job_ids_buf;
		 }
		else
			job_ids = input_job_ids;

		for( i = job_ids;  *i != NULL;  i++ )
			TRY
			 {
				self->wait_for_single_job( self, *i, timeout, NULL, NULL, dispose );
			 }
			EXCEPT( FSD_DRMAA_ERRNO_INVALID_JOB )
			 { /* job was ripped by another thread */ }
			END_TRY
	 }
	FINALLY
	 {
		fsd_free_vector( job_ids_buf );
	 }
	END_TRY
}


char *
fsd_drmaa_session_wait(
		fsd_drmaa_session_t *self,
		const char *job_id, const struct timespec *timeout,
		int *stat, fsd_iter_t **rusage
		)
{
	if( 0==strcmp(job_id, DRMAA_JOB_IDS_SESSION_ANY) )
		return self->wait_for_any_job( self, timeout, stat, rusage, true );
	else
	 {
		self->wait_for_single_job( self, job_id, timeout, stat, rusage, true );
		return fsd_strdup( job_id );
	 }
}


fsd_job_t *
fsd_drmaa_session_new_job( fsd_drmaa_session_t *self, const char *job_id )
{
	fsd_job_t *job;
	job = fsd_job_new( fsd_strdup(job_id) );
	job->session = self;
	return job;
}


char *
fsd_drmaa_session_run_impl(
		fsd_drmaa_session_t *self,
		const fsd_template_t *jt, int bulk_incr )
{
	fsd_exc_raise_code( FSD_ERRNO_NOT_IMPLEMENTED );
}


void
fsd_drmaa_session_wait_for_single_job(
		fsd_drmaa_session_t *self,
		const char *job_id, const struct timespec *timeout,
		int *status, fsd_iter_t **rusage,
		bool dispose
		)
{
	fsd_job_t *volatile job = NULL;
	volatile bool locked = false;

	fsd_log_enter(( "(%s)", job_id ));
	TRY
	 {
		job = self->get_job( self, job_id );
		if( job == NULL )
		 {
			fsd_log_info(("Job %s is not known to DRMAA. Creating job object.", job_id));
			job = self->new_job( self, job_id );
			self->jobs->add( self->jobs, job );
		 }
		job->update_status( job );
		while( !self->destroy_requested  &&  job->state < DRMAA_PS_DONE )
		 {
			bool signaled = true;
			fsd_log_debug(( "fsd_drmaa_session_wait_for_single_job: "
						"waiting for %s to terminate", job_id ));
			if( self->enable_wait_thread )
			 {
				if( timeout )
					signaled = fsd_cond_timedwait(
							&job->status_cond, &job->mutex, timeout );
				else
				 {
					fsd_cond_wait( &job->status_cond, &job->mutex );
				 }
				if( !signaled )
					fsd_exc_raise_code( FSD_DRMAA_ERRNO_EXIT_TIMEOUT );
			 }
			else
			 {
				self->wait_for_job_status_change(
						self, &job->status_cond, &job->mutex, timeout );
			 }

			fsd_log_debug(( "fsd_drmaa_session_wait_for_single_job: woken up" ));
			if( !self->enable_wait_thread )
				job->update_status( job );
		 }

		if( self->destroy_requested )
			fsd_exc_raise_code( FSD_DRMAA_ERRNO_EXIT_TIMEOUT );

		job->get_termination_status( job, status, rusage );
		if( dispose )
		 {
			job->release( job ); /*release mutex in order to ensure proper order of locking: first job_set mutex then job mutex */

			locked = fsd_mutex_lock( &self->mutex );

			job = self->get_job( self, job_id );
			if (job != NULL)
			 {
				self->jobs->remove( self->jobs, job );
				job->flags |= FSD_JOB_DISPOSED;
			 }
			else
			 {
				fsd_log_error(("Some other thread has already reaped job %s", job_id ));
			 }

			locked = fsd_mutex_unlock( &self->mutex );
		 }
	 }
	FINALLY
	 {
		if ( job )
			job->release( job );
		if ( locked )
			fsd_mutex_unlock( &self->mutex );
	 }
	END_TRY
	fsd_log_return((""));
}


char *
fsd_drmaa_session_wait_for_any_job(
		fsd_drmaa_session_t *self,
		const struct timespec *timeout,
		int *status, fsd_iter_t **rusage,
		bool dispose
		)
{
	fsd_job_set_t *set = self->jobs;
	fsd_job_t *volatile job = NULL;
	char *volatile job_id = NULL;
	volatile bool locked = false;

	fsd_log_enter(( "" ));

	TRY
	 {
		while( job == NULL )
		 {
			bool signaled = true;

			if( self->destroy_requested )
				fsd_exc_raise_code( FSD_DRMAA_ERRNO_NO_ACTIVE_SESSION );

			if( !self->enable_wait_thread )
				self->update_all_jobs_status( self );

			locked = fsd_mutex_lock( &self->mutex );
			if( set->empty( set ) )
				fsd_exc_raise_msg( FSD_DRMAA_ERRNO_INVALID_JOB,
						"No job found to be waited for" );

			if( (job = set->find_terminated( set )) != NULL )
				break;

			if( self->destroy_requested )
				fsd_exc_raise_code( FSD_DRMAA_ERRNO_NO_ACTIVE_SESSION );
			if( self->enable_wait_thread )
			 {
				fsd_log_debug(( "wait_for_any_job: waiting for wait thread" ));
				if( timeout )
					signaled = fsd_cond_timedwait(
							&self->wait_condition, &self->mutex, timeout );
				else
					fsd_cond_wait( &self->wait_condition, &self->mutex );
			 }
			else
			 {
				fsd_log_debug(( "wait_for_any_job: waiting for next check" ));
				self->wait_for_job_status_change( self,
						&self->wait_condition, &self->mutex, timeout );
			 }
			locked = fsd_mutex_unlock( &self->mutex );
			fsd_log_debug((
						"wait_for_any_job: woken up; signaled=%d", signaled ));

			if( !signaled )
				fsd_exc_raise_code( FSD_DRMAA_ERRNO_EXIT_TIMEOUT );

		 }
		fsd_log_debug(( "wait_for_any_job: waiting finished" ));

		job_id = fsd_strdup( job->job_id );
		job->get_termination_status( job, status, rusage );
	 }
	EXCEPT_DEFAULT
	 {
		if( job_id )
			fsd_free( job_id );
		fsd_exc_reraise();
	 }
	FINALLY
	 {
		if( job )
		 {
			if( fsd_exc_get() == NULL  &&  dispose )
			 {
				set->remove( set, job );
				job->flags |= FSD_JOB_DISPOSED;
			 }
			job->release( job );
		 }
		if( locked )
			fsd_mutex_unlock( &self->mutex );
	 }
	END_TRY

	fsd_log_return(( " =%s", job_id ));
	return job_id;
}


void
fsd_drmaa_session_wait_for_job_status_change(
		fsd_drmaa_session_t *self,
		fsd_cond_t *wait_condition,
		fsd_mutex_t *mutex,
		const struct timespec *timeout
		)
{
	struct timespec ts, *next_check = &ts;
	bool status_changed;

	if( timeout )
		fsd_log_enter((
					"(timeout=%ld.%09ld)",
					timeout->tv_sec, timeout->tv_nsec ));
	else
		fsd_log_enter(( "(timeout=(null))" ));
	fsd_get_time( next_check );
	fsd_ts_add( next_check, &self->pool_delay );
	if( timeout  &&  fsd_ts_cmp( timeout, next_check ) < 0 )
		next_check = (struct timespec*)timeout;
	fsd_log_debug(( "wait_for_job_status_change: waiting untill %ld.%09ld",
				next_check->tv_sec, next_check->tv_nsec ));
	status_changed = fsd_cond_timedwait(wait_condition, mutex,(const struct timespec *) next_check );
	if( !status_changed  &&  next_check == timeout )
		fsd_exc_raise_code( FSD_DRMAA_ERRNO_EXIT_TIMEOUT );

	fsd_log_return(( ": next_check=%ld.%09ld, status_changed=%d",
				next_check->tv_sec, next_check->tv_nsec,
				(int)status_changed
				));
}


void *
fsd_drmaa_session_wait_thread( fsd_drmaa_session_t *self )
{
	struct timespec ts, *next_check = &ts;
	bool volatile locked = false;

	fsd_log_enter(( "" ));
	locked = fsd_mutex_lock( &self->mutex );
	TRY
	 {
		while( self->wait_thread_run_flag )
			TRY
			 {
				fsd_log_debug(( "wait thread: next iteration" ));
				self->update_all_jobs_status( self );
				fsd_cond_broadcast( &self->wait_condition );
				
				fsd_get_time( next_check );
				fsd_ts_add( next_check, &self->pool_delay );
				fsd_cond_timedwait( &self->wait_condition, &self->mutex, (const struct timespec *) next_check );
				
			 }
			EXCEPT_DEFAULT
			 {
				const fsd_exc_t *e = fsd_exc_get();
				fsd_log_error(( "wait thread: <%d:%s>", e->code(e), e->message(e) ));
			 }
			END_TRY
	 }
	FINALLY
	 { 
		if (locked)
			fsd_mutex_unlock( &self->mutex ); 
	 }
	END_TRY

	fsd_log_return(( " =NULL" ));
	return NULL;
}


void
fsd_drmaa_session_stop_wait_thread( fsd_drmaa_session_t *self )
{
	volatile int lock_count = 0;
	fsd_log_enter(( "" ));
	fsd_mutex_lock( &self->mutex );
	TRY
	 {
		fsd_log_debug(("started = %d  run_flag = %d", self->wait_thread_started, self->wait_thread_run_flag ));
		if( self->wait_thread_started )
		 {
			self->wait_thread_run_flag = false;
			fsd_log_debug(("started = %d  run_flag = %d", self->wait_thread_started, self->wait_thread_run_flag ));
			fsd_cond_broadcast( &self->wait_condition );
			TRY
			 {
				lock_count = fsd_mutex_unlock_times( &self->mutex );
				fsd_thread_join( self->wait_thread_handle, NULL );
			 }
			FINALLY
			 {
				int i;
				for( i = 0;  i < lock_count;  i++ )
					fsd_mutex_lock( &self->mutex );
			 }
			END_TRY
			self->wait_thread_started = false;
		 }

	 }
	FINALLY
	 { fsd_mutex_unlock( &self->mutex ); }
	END_TRY
	fsd_log_return(( "" ));
}


void
fsd_drmaa_session_update_all_jobs_status(
		fsd_drmaa_session_t *self )
{
	char **volatile job_ids = NULL;
	fsd_log_enter(( "" ));
	TRY
	 {
		const char **i;
		fsd_job_t *volatile job = NULL;
		job_ids = self->get_submited_job_ids( self );
		for( i = (const char **)job_ids;  *i;  i++ )
			TRY
			 {
				job = self->get_job( self, *i );
				if( job )
					job->update_status( job );
			 }
			FINALLY
			 {
				if( job )
					job->release( job );
			 }
			END_TRY
	 }
	FINALLY
	 {
		fsd_free_vector( job_ids );
	 }
	END_TRY
	fsd_log_return(( "" ));
}


char **
fsd_drmaa_session_get_submited_job_ids( fsd_drmaa_session_t *self )
{
	return self->jobs->get_all_job_ids( self->jobs );
}


fsd_job_t *
fsd_drmaa_session_get_job( fsd_drmaa_session_t *self, const char *job_id )
{
	return self->jobs->get( self->jobs, job_id );
}


void
fsd_drmaa_session_load_configuration(
		fsd_drmaa_session_t *self, const char *basename
		)
{
	char *volatile system_conf = NULL;
	char *volatile user_conf = NULL;
	char *volatile varname = NULL;
	TRY
	 {
		const char *home;
		const char *envvalue;
		char *i;

		system_conf = fsd_asprintf( DRMAA_DIR_SYSCONF"/%s.conf", basename );

		home = getenv( "HOME" );
		if( home == NULL )
		 { home = "."; }
		user_conf = fsd_asprintf( "%s/.%s.conf", home, basename );

		varname = fsd_asprintf( "%s_CONF", basename );
		for( i = varname;  *i;  i++ )
			*i = toupper( *(unsigned char*)i );
		envvalue = getenv( varname );

		self->configuration = fsd_conf_read(
			 self->configuration, system_conf, false, NULL, 0 );
		self->configuration = fsd_conf_read(
			 self->configuration, user_conf, false, NULL, 0 );
		if( envvalue )
			self->configuration = fsd_conf_read(
				 self->configuration, envvalue, true, NULL, 0 );
		self->apply_configuration( self );
	 }
	FINALLY
	 {
		fsd_free( system_conf );
		fsd_free( user_conf );
		fsd_free( varname );
	 }
	END_TRY
}


void
fsd_drmaa_session_read_configuration(
		fsd_drmaa_session_t *self,
		const char *filename, bool must_exist,
		const char *configuration, size_t configuration_len
		)
{
	self->configuration = fsd_conf_read(
		 self->configuration,
		 filename, must_exist,
		 configuration, configuration_len );
	self->apply_configuration( self );
}


void
fsd_drmaa_session_apply_configuration( fsd_drmaa_session_t *self )
{
	fsd_conf_option_t *pool_delay = NULL;
	fsd_conf_option_t *cache_job_state = NULL;
	fsd_conf_option_t *wait_thread = NULL;
	fsd_conf_option_t *job_categories = NULL;
	fsd_conf_option_t *missing_jobs = NULL;

	fsd_log_enter((""));
	if( self->configuration  !=  NULL ) {

		pool_delay = fsd_conf_dict_get(
				self->configuration, "pool_delay" );
		cache_job_state = fsd_conf_dict_get(
				self->configuration, "cache_job_state" );
		wait_thread = fsd_conf_dict_get(
				self->configuration, "wait_thread" );
		job_categories = fsd_conf_dict_get(
				self->configuration, "job_categories" );
		missing_jobs = fsd_conf_dict_get(
				self->configuration, "missing_jobs" );
	}

	if( pool_delay )
	 {
		if( pool_delay->type == FSD_CONF_INTEGER
				&&  pool_delay->val.integer > 0 )
		 {
			fsd_log_debug(("pool_delay=%d", pool_delay->val.integer));
			self->pool_delay.tv_sec = pool_delay->val.integer;
		 }
		else
			fsd_exc_raise_msg(
					FSD_ERRNO_INTERNAL_ERROR,
					"configuration: 'pool_delay' must be positive integer"
					);
	 }
	if( cache_job_state )
	 {
		if( cache_job_state->type == FSD_CONF_INTEGER
				&&   cache_job_state->val.integer >= 0 )
		 {
			fsd_log_debug(("cache_job_state=%d", cache_job_state->val.integer));
			self->cache_job_state = cache_job_state->val.integer;
		 }
		else
			fsd_exc_raise_msg(
					FSD_ERRNO_INTERNAL_ERROR,
					"configuration: 'cache_job_state' must be nonnegative integer"
					);
	 }
	if( wait_thread )
	 {
		if( wait_thread->type == FSD_CONF_INTEGER )
		 {

			fsd_log_info(("wait_thread=%d", wait_thread->val.integer));
			self->enable_wait_thread = (wait_thread->val.integer != 0 );
		 }
		else
			fsd_exc_raise_msg(
					FSD_ERRNO_INTERNAL_ERROR,
					"configuration: 'wait_thread' should be 0 or 1"
					);
	 }
	if( job_categories )
	 {
		if( job_categories->type == FSD_CONF_DICT )
			self->job_categories = job_categories->val.dict;
		else
			fsd_exc_raise_msg(
					FSD_ERRNO_INTERNAL_ERROR,
					"configuration: 'job_categories' should be dictionary"
					);
	 }
	if( missing_jobs )
	 {
		bool ok = true;
		if( missing_jobs->type != FSD_CONF_STRING )
		 {
			const char *value = missing_jobs->val.string;
			if( !strcmp( value, "ignore" ) )
				self->missing_jobs = FSD_IGNORE_MISSING_JOBS;
			else if( !strcmp( value, "ignore-queued" ) )
				self->missing_jobs = FSD_IGNORE_QUEUED_MISSING_JOBS;
			else if( !strcmp( value, "reveal" ) )
				self->missing_jobs = FSD_REVEAL_MISSING_JOBS;
			else
				ok = false;
		 }
		else
			ok = false;

		if( !ok )
			fsd_exc_raise_msg(
					FSD_ERRNO_INTERNAL_ERROR,
					"configuration: 'missing_jobs' should be one of: "
					"'ignore', 'ignore-queued' or 'reveal'"
					);
	 }

	if( self->enable_wait_thread  &&  !self->wait_thread_started )
	 {
		fsd_log_debug(("Starting wait thread"));
		self->wait_thread_run_flag = true;
		fsd_thread_create( &self->wait_thread_handle,
				(void*(*)(void*))self->wait_thread, self );
		self->wait_thread_started = true;
		fsd_log_debug(( "wait thread started" ));
	 }
	else if( !self->enable_wait_thread  &&  self->wait_thread_started )
	 {
		fsd_log_debug(("Stopping wait thread"));
		self->stop_wait_thread( self );
	 }

	fsd_log_return((""));
}

