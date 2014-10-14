/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//well, linux or cygwin (windows with posix emulation layer), anyway...

#include "quakedef.h"
	
#ifdef MULTITHREAD
#include <limits.h>
#include <pthread.h>
/* Thread creation calls */
typedef void *(*pfunction_t)(void *);

static pthread_t mainthread;

void Sys_ThreadsInit(void)
{
	mainthread = pthread_self();
}
static qboolean Sys_IsThread(void *thread)
{
	return pthread_equal(pthread_self(), *(pthread_t*)thread);
}
static qboolean Sys_IsMainThread(void)
{
	return Sys_IsThread(&mainthread);
}
void Sys_ThreadAbort(void)
{
	pthread_exit(NULL);
}

void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	pthread_t *thread;
	pthread_attr_t attr;

	thread = (pthread_t *)malloc(sizeof(pthread_t));
	if (!thread)
		return NULL;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (stacksize < PTHREAD_STACK_MIN*2)
		stacksize = PTHREAD_STACK_MIN*2;
	pthread_attr_setstacksize(&attr, stacksize);
	if (pthread_create(thread, &attr, (pfunction_t)func, args))
	{
		free(thread);
		thread = NULL;
	}
	pthread_attr_destroy(&attr);

	return (void *)thread;
}

void Sys_WaitOnThread(void *thread)
{
	int err;
	err = pthread_join(*(pthread_t *)thread, NULL);
	if (err)
		printf("pthread_join(%p) failed, error %s\n", thread, strerror(err));
		
	free(thread);
}

/* Mutex calls */
void *Sys_CreateMutex(void)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

	if (mutex && !pthread_mutex_init(mutex, NULL))
		return mutex;
	return NULL;
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return !pthread_mutex_trylock(mutex);
}

qboolean Sys_LockMutex(void *mutex)
{
	return !pthread_mutex_lock(mutex);
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !pthread_mutex_unlock(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	pthread_mutex_destroy(mutex);
	free(mutex);
}

/* Conditional wait calls */
typedef struct condvar_s
{
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
} condvar_t;

void *Sys_CreateConditional(void)
{
	condvar_t *condv;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;

	condv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!condv)
		return NULL;

	mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	if (!mutex)
		return NULL;

	cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	if (!cond)
		return NULL;

	if (!pthread_mutex_init(mutex, NULL))
	{
		if (!pthread_cond_init(cond, NULL))
		{
			condv->cond = cond;
			condv->mutex = mutex;

			return (void *)condv;
		}
		else
			pthread_mutex_destroy(mutex);
	}

	free(cond);
	free(mutex);
	free(condv);
	return NULL;
}

qboolean Sys_LockConditional(void *condv)
{
	return !pthread_mutex_lock(((condvar_t *)condv)->mutex);
}

qboolean Sys_UnlockConditional(void *condv)
{
	return !pthread_mutex_unlock(((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionWait(void *condv)
{
	return !pthread_cond_wait(((condvar_t *)condv)->cond, ((condvar_t *)condv)->mutex);
}

qboolean Sys_ConditionSignal(void *condv)
{
	return !pthread_cond_signal(((condvar_t *)condv)->cond);
}

qboolean Sys_ConditionBroadcast(void *condv)
{
	return !pthread_cond_broadcast(((condvar_t *)condv)->cond);
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	pthread_cond_destroy(cv->cond);
	pthread_mutex_destroy(cv->mutex);
	free(cv->cond);
	free(cv->mutex);
	free(cv);
}
#endif

void Sys_Sleep (double seconds)
{
	struct timespec ts;

	ts.tv_sec = (time_t)seconds;
	seconds -= ts.tv_sec;
	ts.tv_nsec = seconds * 1000000000.0;

	nanosleep(&ts, NULL);
}

