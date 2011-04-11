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
// Authors: Carlos Martins, João Trindade, Duarte Nunes
// 
// 

#pragma once

#include <Windows.h>

typedef VOID * UT_ARGUMENT;
typedef VOID (*UT_FUNCTION)(UT_ARGUMENT);

//
// Runs the scheduler. The operating system thread that calls the function 
// switches to a user thread and resumes execution only when all user threads 
// have exited.
//

VOID
UtRun (
    );

//
// Creates a user thread to run the specified function. 
// The new thread is placed at the end of the ready queue.
//

HANDLE
UtCreate (
    __in UT_FUNCTION Function,
    __in UT_ARGUMENT Argument
    );

//
// Terminates the execution of the currently running thread. All associated resources
// will be released after the context switch to the next ready thread.
//

__declspec(noreturn)
VOID
UtExit (
    );

//
// Relinquishes the processor to the first user thread in the ready queue. 
// If there are no ready threads, the function returns immediately.
//

VOID
UtYield (
    );

//
// Returns a HANDLE to the executing user thread.
//

HANDLE
UtSelf (
    );

//
// Halts the execution of the current user thread.
//

VOID
UtPark (
    );

//
// Places the specified user thread in the ready queue, where it becomes eligible to run.
//

VOID
UtUnpark (
    __in HANDLE ThreadHandle
    );
