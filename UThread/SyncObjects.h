///////////////////////////////////////////////////////////
//
// CCISEL 
// 2007-2011
//
// UThread library:
//     User threads supporting cooperative multithreading.
//     The current version of the library provides:
//        - Threads
//        - Mutexes
//        - Semaphores
//
// Authors: Carlos Martins, Joao Trindade, Duarte Nunes
// 
// 

#pragma once

#include "UThread.h"

//
// Wait block used to queue requests on synchronizers.
//

typedef struct _WAIT_BLOCK {
    LIST_ENTRY WaitListEntry;
    HANDLE Thread;
} WAIT_BLOCK, *PWAIT_BLOCK;

//
// Initializes the specified wait block.
//

FORCEINLINE
VOID
InitializeWaitBlock (
    __out PWAIT_BLOCK WaitBlock
    )
{
    WaitBlock->Thread = UtSelf();
}

//
// A mutex, containing the handle of the user thread that acquired RecursionCounter 
// times the Mutex. If Owner is NULL, then the Mutex is free.
//

typedef struct _UTHREAD_MUTEX {
    LIST_ENTRY WaitListHead;
    ULONG RecursionCounter;
    HANDLE Owner;
} UTHREAD_MUTEX, *PUTHREAD_MUTEX;

//
// Initializes a mutex instance. If Owned is TRUE, then the current thread becomes
// the owner.
//

VOID
UtInitializeMutex (
    __out PUTHREAD_MUTEX Mutex,
    __in BOOL Owned
    );

//
// Acquires the specified mutex, blocking the current thread if the mutex is not free.
//

VOID
UtAcquireMutex (
    __inout PUTHREAD_MUTEX Mutex
    );

//
// Releases the specified mutex, eventually unblocking a waiting thread to which the
// ownership of the mutex is transfered.
//

VOID
UtReleaseMutex (
    __inout PUTHREAD_MUTEX Mutex
    );

//
// A semaphore, containing the current number of permits, upper bounded by Limit.
//

typedef struct _UTHREAD_SEMAPHORE {
    LIST_ENTRY WaitListHead;
    ULONG Permits;
    ULONG Limit;
} UTHREAD_SEMAPHORE, *PUTHREAD_SEMAPHORE;

//
// Wait block used to queue requests on semaphores.
//

typedef struct _SEMAPHORE_WAIT_BLOCK {
    WAIT_BLOCK Header;
    ULONG RequestedPermits;
} SEMAPHORE_WAIT_BLOCK, *PSEMAPHORE_WAIT_BLOCK;

//
// Initializes the specified semaphore wait block.
//

FORCEINLINE
VOID
InitializeSemaphoreWaitBlock (
    __out PSEMAPHORE_WAIT_BLOCK SemaphoreWaitBlock,
    __in ULONG RequestedPermits
    )
{
    InitializeWaitBlock(&SemaphoreWaitBlock->Header);
    SemaphoreWaitBlock->RequestedPermits = RequestedPermits;
}

//
// Initializes a semaphore instance. Permits is the starting number of available 
// permits and Limit is the maximum number of permits allowed for the specified 
// semaphore instance.
//

VOID
UtInitializeSemaphore (
    __out PUTHREAD_SEMAPHORE Semaphore,
    __in ULONG Permits,
    __in ULONG Limit
    );

//
// Gets the specified number of permits from the semaphore. If there aren't enough 
// permits available, the calling thread is blocked until they are added by a call 
// to UtReleaseSemaphore().
//

VOID
UtAcquireSemaphore (
    __inout PUTHREAD_SEMAPHORE Semaphore,
    __in ULONG Permits
    );

//
// Adds the specified number of permits to the semaphore, eventually unblocking 
// waiting threads.
//

VOID
UtReleaseSemaphore (
    __inout PUTHREAD_SEMAPHORE Semaphore,
    __in ULONG Permits
    );
