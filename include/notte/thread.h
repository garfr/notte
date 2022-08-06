/*
 * Copyright (c) 2022 Gavin Ratcliff
 *
 * Multithreading.
 */

#ifndef NOTTE_THREAD_H
#define NOTTE_THREAD_H

#include <notte/error.h>
#include <notte/memory.h>

typedef struct Thread Thread;
typedef struct Mutex Mutex;

typedef void (*Thread_Fn)(void *ud);

Err_Code ThreadCreate(Allocator alloc, void *ud, Thread_Fn fn, 
    Thread **threadOut);
void ThreadDestroy(Allocator alloc, Thread *thread);

Err_Code MutexCreate(Allocator alloc, Mutex **mutexOut);
void MutexDestroy(Allocator alloc, Mutex *mutex);
void MutexAcquire(Mutex *mutex);
bool MutexTryAcquire(Mutex *mutex);
void MutexRelease(Mutex *mutex);

#endif /* NOTTE_THREAD_H */
