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

#define _GNU_SOURCE
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
qboolean Sys_IsThread(void *thread)
{
	return pthread_equal(pthread_self(), *(pthread_t*)thread);
}
qboolean Sys_IsMainThread(void)
{
	return Sys_IsThread(&mainthread);
}
void Sys_ThreadAbort(void)
{
	pthread_exit(NULL);
}

#ifdef ANDROID
#include <jni.h>
extern JavaVM *sys_jvm;
#endif

#if 1
typedef struct {
	int (*func)(void *);
	void *args;
} qthread_t;
static int Sys_CreatedThread(void *v)
{
	qthread_t *qthread = v;
	int r;

#ifdef ANDROID
	JNIEnv* env;
	(*sys_jvm)->AttachCurrentThread(sys_jvm, &env, NULL);
#endif

	r = qthread->func(qthread->args);

#ifdef ANDROID
	(*sys_jvm)->DetachCurrentThread(sys_jvm);
#endif

	return r;
}

void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	pthread_t *thread;
	qthread_t *qthread;
	pthread_attr_t attr;

	thread = (pthread_t *)malloc(sizeof(pthread_t)+sizeof(qthread_t));
	if (!thread)
		return NULL;

	qthread = (qthread_t*)(thread+1);
	qthread->func = func;
	qthread->args = args;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (stacksize < PTHREAD_STACK_MIN*2)
		stacksize = PTHREAD_STACK_MIN*2;
	if (stacksize < PTHREAD_STACK_MIN+65536*16)
		stacksize = PTHREAD_STACK_MIN+65536*16;
	pthread_attr_setstacksize(&attr, stacksize);
	if (pthread_create(thread, &attr, (pfunction_t)Sys_CreatedThread, qthread))
	{
		free(thread);
		thread = NULL;
	}
	pthread_attr_destroy(&attr);

#ifdef __USE_GNU
	pthread_setname_np(*thread, name);
#endif

	return (void *)thread;
}
#else
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

#ifdef __USE_GNU
	pthread_setname_np(*thread, name);
#endif

	return (void *)thread;
}
#endif

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




#ifdef SUBSERVERS
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#define STDIN 0
#define STDOUT 1
#define STDERR 2

typedef struct slaveserver_s
{
	pubsubserver_t pub;

	int inpipe;
	int outpipe;
	pid_t pid;	//so we don't end up with zombie processes
	
	qbyte inbuffer[2048];
	int inbufsize;
} linsubserver_t;

static void Sys_InstructSlave(pubsubserver_t *ps, sizebuf_t *cmd)
{
	//FIXME: this is blocking. this is bad if the target is also blocking while trying to write to us.
	//FIXME: merge buffering logic with SSV_InstructMaster, and allow for failure if full
	linsubserver_t *s = (linsubserver_t*)ps;
	if (s->outpipe == -1)
		return;	//it already died.
	cmd->data[0] = cmd->cursize & 0xff;
	cmd->data[1] = (cmd->cursize>>8) & 0xff;
	write(s->outpipe, cmd->data, cmd->cursize);
}

static int Sys_SubServerRead(pubsubserver_t *ps)
{
	linsubserver_t *s = (linsubserver_t*)ps;

	if (s->inbufsize < sizeof(s->inbuffer) && s->inpipe != -1)
	{
		ssize_t avail = read(s->inpipe, s->inbuffer+s->inbufsize, sizeof(s->inbuffer)-s->inbufsize);
		if (!avail)
		{	//eof
			close(s->inpipe);
			close(s->outpipe);
			Con_Printf("%i:%s has died\n", s->pub.id, s->pub.name);
			s->inpipe = -1;
			s->outpipe = -1;
			waitpid(s->pid, NULL, 0);
		}
		else if (avail < 0)
		{
			int e = errno;
			if (e == EAGAIN || e == EWOULDBLOCK)
				;
			else
				perror("subserver read");
		}
		else
			s->inbufsize += avail;
	}

	if(s->inbufsize >= 2)
	{
		unsigned short len = s->inbuffer[0] | (s->inbuffer[1]<<8);
		if (s->inbufsize >= len && len>=2)
		{
			memcpy(net_message.data, s->inbuffer+2, len-2);
			net_message.cursize = len-2;
			memmove(s->inbuffer, s->inbuffer+len, s->inbufsize - len);
			s->inbufsize -= len;
			MSG_BeginReading (msg_nullnetprim);

			return 1;
		}
	}
	else if (s->inpipe == -1)
		return -1;
	return 0;
}

#ifdef SQL
#include "sv_sql.h"
#endif

pubsubserver_t *Sys_ForkServer(void)
{
#ifdef SERVERONLY
//	extern  jmp_buf 	host_abort;
		
	int toslave[2];
	int tomaster[2];
	linsubserver_t *ctx;
	pid_t pid;

	//make sure we're fully synced, so that workers can't mess up
	Cvar_Set(Cvar_FindVar("worker_count"), "0");
	COM_WorkerFullSync();
	DL_DeThread();
#ifdef SQL
	SQL_KillServers();	//FIXME: this is bad...
#endif
	//FIXME: we should probably use posix_atfork for those.

	pipe(toslave);
	pipe(tomaster);

	//make the reads non-blocking.
	fcntl(toslave[1], F_SETFL, fcntl(toslave[1], F_GETFL, 0)|O_NONBLOCK);
	fcntl(tomaster[0], F_SETFL, fcntl(tomaster[0], F_GETFL, 0)|O_NONBLOCK);

	pid = fork();

	if (!pid)
	{	//this is the child
		dup2(toslave[0], STDIN);
		close(toslave[1]);
		close(toslave[0]);
		dup2(tomaster[1], STDOUT);

		isClusterSlave = true;

		FS_UnloadPackFiles();	//these handles got wiped. make sure they're all properly wiped before loading new handles.
		NET_Shutdown();

		FS_ReloadPackFiles();

		return NULL;	//lets hope the caller can cope.
		//jump out into the main work loop
//		longjmp(host_abort, 1);
//		exit(0);	//err...
	}
	else
	{	//this is the parent
		close(toslave[0]);
		close(tomaster[1]);
		if (pid == -1)
		{	//fork failed. make sure everything is destroyed.
			close(toslave[1]);
			close(tomaster[0]);
			return NULL;
		}

		Con_DPrintf("Forked new server node\n");
		ctx = Z_Malloc(sizeof(*ctx));
	}

	

#else
	int toslave[2];
	int tomaster[2];
	char exename[MAX_OSPATH];
	posix_spawn_file_actions_t action;
	linsubserver_t *ctx;
	char *argv[64];
	int argc = 0;

	argv[argc++] = exename;
	argv[argc++] = "-clusterslave";
	argc += FS_GetManifestArgv(argv+argc, countof(argv)-argc-1);
	argv[argc++] = NULL;

#if 0
	strcpy(exename, "/bin/ls");
	args[1] = NULL;
#elif 0
	strcpy(exename, "/tmp/ftedbg/fteqw.sv");
#else
	memset(exename, 0, sizeof(exename));	//having problems with valgrind being stupid.
	if (readlink("/proc/self/exe", exename, sizeof(exename)-1) <= 0)
		return NULL;
#endif
	Con_DPrintf("Execing %s\n", exename);

	ctx = Z_Malloc(sizeof(*ctx));

	pipe(toslave);
	pipe(tomaster);

	//make the reads non-blocking.
	fcntl(toslave[1], F_SETFL, fcntl(toslave[1], F_GETFL, 0)|O_NONBLOCK);
	fcntl(tomaster[0], F_SETFL, fcntl(tomaster[0], F_GETFL, 0)|O_NONBLOCK);

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, toslave[1]);
	posix_spawn_file_actions_addclose(&action, tomaster[0]);

	posix_spawn_file_actions_adddup2(&action, toslave[0],	STDIN);
	posix_spawn_file_actions_adddup2(&action, tomaster[1],	STDOUT);
//	posix_spawn_file_actions_adddup2(&action, tomaster[1],	STDERR);

	posix_spawn_file_actions_addclose(&action, toslave[0]);
	posix_spawn_file_actions_addclose(&action, tomaster[1]);

	posix_spawn(&ctx->pid, exename, &action, NULL, argv, NULL);
#endif

	ctx->inpipe = tomaster[0];
	close(tomaster[1]);
	close(toslave[0]);
	ctx->outpipe = toslave[1];

	ctx->pub.funcs.InstructSlave = Sys_InstructSlave;
	ctx->pub.funcs.SubServerRead = Sys_SubServerRead;
	return &ctx->pub;
}

void Sys_InstructMaster(sizebuf_t *cmd)
{
	write(STDOUT, cmd->data, cmd->cursize);

	//FIXME: handle partial writes.
}

void SSV_CheckFromMaster(void)
{
	static char inbuffer[1024];
	static int inbufsize;
	for(;;)
	{
		if(inbufsize >= 2)
		{
			unsigned short len = inbuffer[0] | (inbuffer[1]<<8);
			if (inbufsize >= len && len>=2)
			{
				memcpy(net_message.data, inbuffer+2, len-2);
				net_message.cursize = len-2;
				memmove(inbuffer, inbuffer+len, inbufsize - len);
				inbufsize -= len;
				MSG_BeginReading (msg_nullnetprim);

				SSV_ReadFromControlServer();

				continue;	//keep trying to handle it
			}
		}

		if (inbufsize == sizeof(inbuffer))
		{	//fatal: we can't easily recover from this.
			SV_FinalMessage("Cluster message too large\n");
			Cmd_ExecuteString("quit force", RESTRICT_LOCAL);
			break;
		}

		{
			ssize_t avail = read(STDIN, inbuffer+inbufsize, sizeof(inbuffer)-inbufsize);
			if (!avail)
			{	//eof
				SV_FinalMessage("Cluster shut down\n");
				Cmd_ExecuteString("quit force", RESTRICT_LOCAL);
				break;
			}
			else if (avail < 0)
			{
				int e = errno;
				if (e == EAGAIN || e == EWOULDBLOCK)
					;
				else
					perror("master read");
				break;
			}
			else
				inbufsize += avail;
		}
	}
}
#endif
