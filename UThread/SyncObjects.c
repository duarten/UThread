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

#include <crtdbg.h>
#include "SyncObjects.h"
#include "List.h"

//
// Initializes a mutex instance. If Owned is TRUE, then the current thread becomes
// the owner.
//

VOID
UtInitializeMutex (
    __out PUTHREAD_MUTEX Mutex,
    __in BOOL Owned
    )
{
    InitializeListHead(&Mutex->WaitListHead);
    Mutex->Owner = Owned ? UtSelf() : NULL;
    Mutex->RecursionCounter = Owned ? 1 : 0;
}

//
// Acquires the specified mutex, blocking the current thread if the mutex is not free.
//

VOID
UtAcquireMutex (
    __inout PUTHREAD_MUTEX Mutex
    )
{
    HANDLE Self;
    WAIT_BLOCK WaitBlock;

    if (Mutex->Owner == (Self = UtSelf())) {

        //
        // Recursive aquisition. Increment the recursion counter.
        //

        Mutex->RecursionCounter += 1;
    } else if (Mutex->Owner == NULL) {

        //
        // Mutex is free. Acquire the mutex by setting its owner to the current thread.
        //

        Mutex->Owner = Self;
        Mutex->RecursionCounter = 1;
    } else {

        //
        // Insert the running thread in the wait list.
        //

        InitializeWaitBlock(&WaitBlock);
        InsertTailList(&Mutex->WaitListHead, &WaitBlock.WaitListEntry);

        //
        // Park the current thread. When the thread is unparked, it will have ownership of the mutex.
        //
        
        UtPark();
        _ASSERTE(Mutex->Owner == Self);
    }
}

//
// Releases the specified mutex, eventually unblocking a waiting thread to which the
// ownership of the mutex is transfered.
//

VOID
UtReleaseMutex (
    __inout PUTHREAD_MUTEX Mutex
    )
{
    PWAIT_BLOCK WaitBlock;

    _ASSERTE(Mutex->Owner == UtSelf());

    if ((Mutex->RecursionCounter -= 1) > 0) {
        
        //
        // The current thread is still the owner of the mutex.
        //

        return;
    }

    if (IsListEmpty(&Mutex->WaitListHead)) {

        //
        // No threads are blocked; the mutex becomes free.
        //

        Mutex->Owner = NULL;
        return;
    }

    //
    // Get the next blocked thread and transfer mutex ownership to it.
    //

    WaitBlock = CONTAINING_RECORD(RemoveHeadList(&Mutex->WaitListHead), WAIT_BLOCK, WaitListEntry);
    Mutex->Owner = WaitBlock->Thread;
    Mutex->RecursionCounter = 1;
        
    //
    // Unpark the thread.
    //

    UtUnpark(WaitBlock->Thread);
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
    )
{
    InitializeListHead(&Semaphore->WaitListHead);
    Semaphore->Permits = Permits;
    Semaphore->Limit = Limit;
}

//
// Gets the specified number of permits from the semaphore. If there aren't enough 
// permits available, the calling thread is blocked until they are added by a call 
// to UtReleaseSemaphore().
//

VOID
UtAcquireSemaphore (
    __inout PUTHREAD_SEMAPHORE Semaphore,
    __in ULONG Permits
    )
{
    SEMAPHORE_WAIT_BLOCK WaitBlock;

    //
    // If there are enough permits available, get them and keep running.
    //

    if (Semaphore->Permits >= Permits) {
        Semaphore->Permits -= Permits;
        return;
    }

    //
    // There are no permits available. Insert the running thread in the wait list.
    //

    InitializeSemaphoreWaitBlock(&WaitBlock, Permits);   
    InsertTailList(&Semaphore->WaitListHead, &WaitBlock.Header.WaitListEntry);

    //
    // Park the current thread.
    //

    UtPark();
}

//
// Adds the specified number of permits to the semaphore, eventually unblocking 
// waiting threads.
//

VOID
UtReleaseSemaphore (
    __inout PUTHREAD_SEMAPHORE Semaphore,
    __in ULONG Permits
    )
{
    PLIST_ENTRY ListHead;
    PSEMAPHORE_WAIT_BLOCK WaitBlock;
    PLIST_ENTRY WaitEntry;

    if ((Semaphore->Permits += Permits) > Semaphore->Limit) {
        Semaphore->Permits = Semaphore->Limit;
    }

    ListHead = &Semaphore->WaitListHead;

    //
    // Release all blocked threads whose request can be satisfied.
    //
    
    while (Semaphore->Permits > 0 && (WaitEntry = ListHead->Flink) != ListHead) {
        WaitBlock = CONTAINING_RECORD(WaitEntry, SEMAPHORE_WAIT_BLOCK, Header.WaitListEntry);

        if (Semaphore->Permits < WaitBlock->RequestedPermits) {
            
            //
            // We stop at the first request that cannot be satisfied to ensure FIFO ordering.
            //

            break;
        }

        Semaphore->Permits -= WaitBlock->RequestedPermits;
        RemoveHeadList(ListHead);
        UtUnpark(WaitBlock->Header.Thread);
    }
}
