/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Multithreading.
 */

#include <notte/thread.h>

#ifdef NOTTE_WINDOWS

#include <windows.h>

/* === TYPES === */

struct Thread
{
  HANDLE handle;
  DWORD id;
  void *ud;
  Thread_Fn fn;
};

struct Mutex 
{
  CRITICAL_SECTION cr;
};

/* === PROTOTYPES === */

static DWORD WINAPI ThreadRun(LPVOID *ud);

/* === PUBLIC FUNCTIONS === */

Err_Code 
ThreadCreate(Allocator alloc, void *ud, Thread_Fn fn, Thread **threadOut)
{
  Thread *thread = NEW(alloc, Thread, MEMORY_TAG_THREAD);

  thread->fn = fn;
  thread->ud = ud;

  thread->handle = CreateThread(NULL, 0, ThreadRun, thread, 0, &thread->id);

  *threadOut = thread;
  return ERR_OK;
}

void 
ThreadDestroy(Allocator alloc, Thread *thread)
{
  FREE(alloc, thread, Thread, MEMORY_TAG_THREAD);
}

Err_Code 
MutexCreate(Allocator alloc, Mutex **mutexOut)
{
  Mutex *mutex = NEW(alloc, Mutex, MEMORY_TAG_THREAD);

  InitializeCriticalSection(&mutex->cr);

  *mutexOut = mutex;
  return ERR_OK;
}

void 
MutexDestroy(Allocator alloc, Mutex *mutex)
{
  DeleteCriticalSection(&mutex->cr);
  FREE(alloc, mutex, Mutex, MEMORY_TAG_THREAD);
}

void 
MutexAcquire(Mutex *mutex)
{
  EnterCriticalSection(&mutex->cr);
}

void 
MutexRelease(Mutex *mutex)
{
  LeaveCriticalSection(&mutex->cr);
}

/* === PRIVATE FUNCTIONS === */

static DWORD WINAPI
ThreadRun(LPVOID *ud)
{
  Thread *thread = (Thread *) ud;

  thread->fn(thread->ud);

  return 0;
}

#endif
