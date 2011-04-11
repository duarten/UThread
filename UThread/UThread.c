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
#include "UThread.h"
#include "List.h"

//
// The data structure representing the layout of a thread's execution 
// context when saved in the thread's stack.
//

typedef struct _UTHREAD_CONTEXT {
    ULONG EDI;
    ULONG ESI;
    ULONG EBX;
    ULONG EBP;
    VOID (*RetAddr)();
} UTHREAD_CONTEXT, *PUTHREAD_CONTEXT;

//
// The descriptor of a user thread, containing an intrusive link through which 
// the thread is linked in the ready queue, the thread's starting function and 
// argument, the memory block used as the thread's stack and a pointer to the  
// saved execution context.
//

typedef struct _UTHREAD {
    LIST_ENTRY Link;
    UT_FUNCTION Function;   
    UT_ARGUMENT Argument;   
    PUCHAR Stack;
    PUTHREAD_CONTEXT ThreadContext;
} UTHREAD, *PUTHREAD;

//
// The fixed stack size of a user thread.
//

#define STACK_SIZE (16 * 4096)

//
// The number of existing user threads.
//

static ULONG NumberOfThreads;

//
// The sentinel of the circular list linking the user threads that are schedulable. 
// The next thread to run is retrieved from the head of the list.
//

static LIST_ENTRY ReadyQueue = { &ReadyQueue, &ReadyQueue };

//
// The currently executing thread.
//

PUTHREAD RunningThread;

//
// The user thread proxy of the main operating system thread. This thread 
// is switched back in when there are no more runnable user threads and the 
// scheduler will exit.
//

PUTHREAD MainThread;

//
// Forward declaration of helper functions.
//

//
// The trampoline function that a user thread begins by executing, through  
// which the associated function is called.
//

static
VOID
InternalStart (
    );

//
// Performs a context switch from CurrentThread (switch out) to NextThread (switch in).
// __fastcall sets the calling convention such that CurrentThread is in ECX and 
// NextThread in EDX.
//

static
VOID
__fastcall
ContextSwitch (
    __inout PUTHREAD CurrentThread,
    __in PUTHREAD NextThread
    );

//
// Frees the resources associated with CurrentThread and switches to NextThread.
// __fastcall sets the calling convention such that CurrentThread is in ECX and 
// NextThread in EDX.
//

static
VOID
__fastcall
InternalExit (
    __inout PUTHREAD Thread,
    __in PUTHREAD NextThread
    );

//
// Returns and removes the first user thread in the ready queue. If the ready queue is empty, 
// the main thread is returned.
//

FORCEINLINE
PUTHREAD
SelectNextReadyThread (
    )
{
    return IsListEmpty(&ReadyQueue) 
         ? MainThread 
         : CONTAINING_RECORD(RemoveHeadList(&ReadyQueue), UTHREAD, Link);
}

//
// Definition of the public interface.
//

//
// Runs the scheduler. The operating system thread that calls the function 
// switches to a user thread and resumes execution only when all user threads 
// have exited.
//

VOID
UtRun (
    )
{
    UTHREAD Thread;

    //
    // There can be only one scheduler instance running.
    //

    _ASSERTE(RunningThread == NULL);

    if (IsListEmpty(&ReadyQueue)) {
        return;
    }

    //
    // Switch to a user thread.
    //
    
#if DEBUG
    Thread.Function = (UT_FUNCTION) UtRun;
#endif
    MainThread = &Thread;
    ContextSwitch(&Thread, SelectNextReadyThread());

    //
    // When we get here, there are no more runnable user threads.
    //

    _ASSERTE(IsListEmpty(&ReadyQueue));
    _ASSERTE(NumberOfThreads == 0);

    //
    // Allow another call to Uth_Run().
    //

    RunningThread = NULL;
}

//
// Creates a user thread to run the specified function. 
// The new thread is placed at the end of the ready queue.
//

HANDLE
UtCreate (
    __in UT_FUNCTION Function,
    __in UT_ARGUMENT Argument
    )
{
    PUTHREAD Thread;

    //
    // Dynamically allocate an instance of UTHREAD and the associated stack.
    //

    Thread = (PUTHREAD) malloc(sizeof(*Thread));
    Thread->Stack = (PUCHAR) malloc(STACK_SIZE);
    _ASSERTE(Thread != NULL && Thread->Stack != NULL);

    //
    // Zero the stack for emotional confort.
    //
    
    RtlZeroMemory(Thread->Stack, STACK_SIZE);

    Thread->Function = Function;
    Thread->Argument = Argument;

    //
    // Map an UTHREAD_CONTEXT instance on the thread's stack.
    // We'll use it to save the initial context of the thread.
    //
    // +------------+
    // | 0x00000000 |    <- Highest word of a thread's stack space
    // +============+       (needs to be set to 0 for Visual Studio to
    // |  RetAddr   | \     correctly present a thread's call stack).
    // +------------+  |
    // |    EBP     |  |
    // +------------+  |
    // |    EBX     |   >   Thread->ThreadContext mapped on the stack.
    // +------------+  |
    // |    ESI     |  |
    // +------------+  |
    // |    EDI     | /  <- The stack pointer will be set to this address
    // +============+       at the next context switch to this thread.
    // |            | \
    // +------------+  |
    // |     :      |  |
    //       :          >   Remaining stack space.
    // |     :      |  |
    // +------------+  |
    // |            | /  <- Lowest word of a thread's stack space
    // +------------+       (Thread->Stack always points to this location).
    //

    Thread->ThreadContext = (PUTHREAD_CONTEXT) (Thread->Stack 
                                                + STACK_SIZE 
                                                - sizeof(ULONG)
                                                - sizeof *Thread->ThreadContext);

    //
    // Set the thread's initial context by initializing the values of EDI, EBX, ESI 
    // and EBP (must be zero for Visual Studio to correctly present a thread's call stack)
    // and by hooking the return address. Upon the first context switch to this thread, 
    // after popping the dummy values of the "saved" registers, a ret instruction will 
    // place InternalStart's address on the processor's IP.
    //
    
    Thread->ThreadContext->EBX = 0x11111111;
    Thread->ThreadContext->ESI = 0x22222222;
    Thread->ThreadContext->EDI = 0x33333333;
    Thread->ThreadContext->EBP = 0x00000000;									  
    Thread->ThreadContext->RetAddr = InternalStart;

    //
    // Ready the thread and return a handle to it.
    //
    
    NumberOfThreads += 1;
    UtUnpark(Thread);
    
    return (HANDLE) Thread;
}

//
// Terminates the execution of the currently running thread. All associated resources
// will be released after the context switch to the next ready thread.
//

__declspec(noreturn)
VOID
UtExit (
    )
{
    NumberOfThreads -= 1;	
    InternalExit(RunningThread, SelectNextReadyThread());
    _ASSERTE(!"supposed to be here!");
}

//
// Relinquishes the processor to the first user thread in the ready queue. 
// If there are no ready threads, the function returns immediately.
//

VOID
UtYield (
    ) 
{
    if (!IsListEmpty(&ReadyQueue)) {

        //
        // Insert the running thread at the tail of the ready queue
        // and switch to the thread that is at front of the ready list.
        //

        InsertTailList(&ReadyQueue, &RunningThread->Link);
        ContextSwitch(RunningThread, CONTAINING_RECORD(RemoveHeadList(&ReadyQueue), UTHREAD, Link));
    }
}

//
// Returns a HANDLE to the executing user thread.
//

HANDLE
UtSelf (
    )
{
    return (HANDLE) RunningThread;
}

//
// Halts the execution of the current user thread.
//

VOID
UtPark (
    )
{
    ContextSwitch(RunningThread, SelectNextReadyThread());
}

//
// Places the specified user thread in the ready queue, where it becomes eligible to run.
//

VOID
UtUnpark (
    __in HANDLE ThreadHandle
    )
{
    InsertTailList(&ReadyQueue, &((PUTHREAD) ThreadHandle)->Link);
}

//
// Definition of the helper functions.
//

//
// The trampoline function that a user thread begins by executing,
// through which the associated function is called.
//

VOID
InternalStart (
    )
{
    RunningThread->Function(RunningThread->Argument);
    UtExit();
}

//
// Perform a context switch from CurrentThread (switch out) to NextThread (switch in).
// __fastcall sets the calling convention such that CurrentThread is in ECX and NextThread
// in EDX.
// __declspec(naked) directs the compiler to omit any prologue or epilogue code.
//

__declspec(naked)
VOID
__fastcall
ContextSwitch (
    __inout PUTHREAD CurrentThread,
    __in PUTHREAD NextThread
    )
{
    __asm {

        //
        // Switch out the running CurrentThread, saving the execution context
        // on the thread's own stack. The return address is atop the stack,
        // having been placed there by the call to this function.
        //

        push	ebp
        push	ebx
        push	esi
        push	edi

        //
        // Save ESP in CurrentThread->ThreadContext.
        //

        mov		dword ptr [ecx].ThreadContext, esp

        //
        // Set NextThread as the running thread.
        //

        mov     RunningThread, edx

        //
        // Load NextThread's context, starting by switching to its stack,
        // where the registers are saved.
        //

        mov		esp, dword ptr [edx].ThreadContext
        
        pop		edi
        pop		esi
        pop		ebx
        pop		ebp

        //
        // Jump to the return address saved on NextThread's stack when 
        // the function was called.
        //

        ret
    }
}

//
// Frees the resources associated with Thread.
// __fastcall sets the calling convention such that Thread is in ECX.
//

static
VOID
__fastcall
CleanupThread (
    __inout PUTHREAD Thread
    )
{
    free(Thread->Stack);
    free(Thread);
}

//
// Frees the resources associated with CurrentThread and switches to NextThread.
// __fastcall sets the calling convention such that CurrentThread is in ECX and 
// NextThread in EDX.
// __declspec(naked) directs the compiler to omit any prologue or epilogue code.
//

__declspec(naked)
VOID
__fastcall
InternalExit (
    __inout PUTHREAD CurrentThread,
    __in PUTHREAD NextThread
    )
{
    __asm {

        //
        // Set NextThread as the running thread.
        //

        mov     RunningThread, edx
        
        //
        // Load NextThread's stack pointer before calling CleanupThread(): making 
        // the call while using CurrentThread's stack would mean using the same 
        // memory being freed -- the stack.
        //

        mov		esp, dword ptr [edx].ThreadContext

        call    CleanupThread

        //
        // Finish switching in NextThread.
        //

        pop		edi
        pop		esi
        pop		ebx
        pop		ebp

        ret
    }
}
