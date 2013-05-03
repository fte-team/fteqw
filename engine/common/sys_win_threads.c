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
#include "quakedef.h"
#include <sys/types.h>
#include <sys/timeb.h>

#include <winsock.h>
#include <conio.h>

#ifdef MULTITHREAD
#include <process.h>
#endif

#ifdef MULTITHREAD
/* Thread creation calls */
typedef struct threadwrap_s
{
	void (*func)(void *);
	void *args;
} threadwrap_t;

// the thread call is wrapped so we don't need WINAPI everywhere
DWORD WINAPI threadwrapper(void *args)
{
	threadwrap_t tw;
	tw.func = ((threadwrap_t *)args)->func;
	tw.args = ((threadwrap_t *)args)->args;

	free(args);
	tw.func(tw.args);

#ifndef WIN32CRTDLL
	_endthreadex(0);
#endif
	return 0;
}

#ifdef _DEBUG
const DWORD MS_VC_EXCEPTION=0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; // Must be 0x1000.
   LPCSTR szName; // Pointer to name (in user addr space).
   DWORD dwThreadID; // Thread ID (-1=caller thread).
   DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
void Sys_SetThreadName(unsigned int dwThreadID, char *threadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}
#endif

void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	threadwrap_t *tw = (threadwrap_t *)malloc(sizeof(threadwrap_t));
	HANDLE handle;
	DWORD tid;

	if (!tw)
		return NULL;

	stacksize += 128; // wrapper overhead, also prevent default stack size
	tw->func = func;
	tw->args = args;
#ifdef WIN32CRTDLL
	handle = (HANDLE)CreateThread(NULL, stacksize, &threadwrapper, (void *)tw, 0, &tid);
#else
	handle = (HANDLE)_beginthreadex(NULL, stacksize, &threadwrapper, (void *)tw, 0, &tid);
#endif
	if (!handle)
	{
		free(tw);
		return NULL;
	}

#ifdef _DEBUG
	Sys_SetThreadName(tid, name);
#endif

	return (void *)handle;
}

void Sys_DetachThread(void *thread)
{
	CloseHandle((HANDLE)thread);
}

void Sys_WaitOnThread(void *thread)
{
	WaitForSingleObject((HANDLE)thread, INFINITE);
	CloseHandle((HANDLE)thread);
}

/* Mutex calls */
void *Sys_CreateMutex(void)
{
	return (void *)CreateMutex(NULL, 0, NULL);
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0;
}

qboolean Sys_LockMutex(void *mutex)
{
	return WaitForSingleObject(mutex, INFINITE) == WAIT_OBJECT_0;
}

qboolean Sys_UnlockMutex(void *mutex)
{
	return !!ReleaseMutex(mutex);
}

void Sys_DestroyMutex(void *mutex)
{
	CloseHandle(mutex);
}

/* Conditional wait calls */
/*
TODO: Windows Vista has condition variables as documented here:
http://msdn.microsoft.com/en-us/library/ms682052(VS.85).aspx
Note this uses Slim Reader/Writer locks (Vista+ exclusive)
or critical sections.

The condition variable implementation is based on the libSDL implementation.
This code could probably be made more efficient with the use of events or
different mechanisms but for now the main concern is a correct and
complete solution.
*/
typedef struct condvar_s
{
    int waiting;
    int signals;
    CRITICAL_SECTION countlock;
	CRITICAL_SECTION mainlock;
    HANDLE wait_sem;
    HANDLE wait_done;
} condvar_t;

void *Sys_CreateConditional(void)
{
	condvar_t *cv;

	cv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!cv)
		return NULL;

	cv->waiting = 0;
	cv->signals = 0;
	InitializeCriticalSection (&cv->mainlock);
	InitializeCriticalSection (&cv->countlock);
	cv->wait_sem = CreateSemaphore (NULL, 0, 0x7fffffff, NULL);
	cv->wait_done = CreateSemaphore (NULL, 0, 0x7fffffff, NULL);

	if (cv->wait_sem && cv->wait_done)
		return (void *)cv;

	// something failed so deallocate everything
	if (cv->wait_done)
		CloseHandle(cv->wait_done);
	if (cv->wait_sem)
		CloseHandle(cv->wait_sem);
	DeleteCriticalSection(&cv->countlock);
	DeleteCriticalSection(&cv->mainlock);
	free(cv);

	return NULL;
}

qboolean Sys_LockConditional(void *condv)
{
	EnterCriticalSection(&((condvar_t *)condv)->mainlock);
	return true;
}

qboolean Sys_UnlockConditional(void *condv)
{
	LeaveCriticalSection(&((condvar_t *)condv)->mainlock);
	return true;
}

qboolean Sys_ConditionWait(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;
	qboolean success;
	DWORD status;

	// increase count for non-signaled waiting threads
	EnterCriticalSection(&cv->countlock);
	cv->waiting++;
	LeaveCriticalSection(&cv->countlock);

	LeaveCriticalSection(&cv->mainlock); // unlock as per condition variable definition

	// wait on a signal
#if 0
	success = (WaitForSingleObject(cv->wait_sem, INFINITE) != WAIT_FAILED);
#else
	do
	{
		MSG msg;
		while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
      		DispatchMessage (&msg);
		status = MsgWaitForMultipleObjects(1, &cv->wait_sem, FALSE, INFINITE, QS_SENDMESSAGE|QS_POSTMESSAGE);
	} while (status == (WAIT_OBJECT_0+1));
	success = status != WAIT_FAILED;
#endif

	// update waiting count and alert signaling thread that we're done to avoid the deadlock condition
	EnterCriticalSection(&cv->countlock);
	if (cv->signals > 0)
	{
		ReleaseSemaphore(cv->wait_done, cv->signals, NULL);
		cv->signals = 0;
	}
	cv->waiting--;
	LeaveCriticalSection(&cv->countlock);

	EnterCriticalSection(&cv->mainlock); // lock as per condition variable definition

	return success;
}

qboolean Sys_ConditionSignal(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	// if there are non-signaled waiting threads, we signal one and wait on the response
	EnterCriticalSection(&cv->countlock);
	if (cv->waiting > cv->signals)
	{
		cv->signals++;
		ReleaseSemaphore(cv->wait_sem, 1, NULL);
		LeaveCriticalSection(&cv->countlock);
		WaitForSingleObject(cv->wait_done, INFINITE);
	}
	else
		LeaveCriticalSection(&cv->countlock);

    return true;
}

qboolean Sys_ConditionBroadcast(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	// if there are non-signaled waiting threads, we signal all of them and wait on all the responses back
	EnterCriticalSection(&cv->countlock);
	if (cv->waiting > cv->signals)
	{
		int i, num_waiting;

		num_waiting = (cv->waiting - cv->signals);
		cv->signals = cv->waiting;

		ReleaseSemaphore(cv->wait_sem, num_waiting, NULL);
		LeaveCriticalSection(&cv->countlock);
		// there's no call to wait for the same object multiple times so we need to loop through
		// and burn up the semaphore count
		for (i = 0; i < num_waiting; i++)
			WaitForSingleObject(cv->wait_done, INFINITE);
	}
	else
		LeaveCriticalSection(&cv->countlock);

	return true;
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	CloseHandle(cv->wait_done);
	CloseHandle(cv->wait_sem);
	DeleteCriticalSection(&cv->countlock);
	DeleteCriticalSection(&cv->mainlock);
	free(cv);
}
#endif
