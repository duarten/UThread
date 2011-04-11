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

#include <crtdbg.h>
#include <stdio.h>
#include "UThread.h"
#include "SyncObjects.h"
#include "List.h"

///////////////////////////////////////////////////////////////
//															 //
// Test 1: 10 threads, each one printing its number 16 times //
//															 //
///////////////////////////////////////////////////////////////

ULONG Test1_Count;

VOID
Test1_Thread (
    __in UT_ARGUMENT Argument
    ) 
{
    UCHAR Char;
    ULONG Index;
    
    Char = (UCHAR) Argument;	

    for (Index = 0; Index < 1000; ++Index) {
        putchar(Char);

        if ((rand() % 4) == 0) {
            UtYield();
        }
    }

    ++Test1_Count;
    
    UtExit();
}

VOID
Test1 (
    ) 
{
    ULONG Index;

    Test1_Count = 0; 

    printf("\n :: Test 1 - BEGIN :: \n\n");

    for (Index = 0; Index < 10; ++Index) {
        UtCreate(Test1_Thread, (UT_ARGUMENT) ('0' + Index));
    }   

    UtRun();

    _ASSERTE(Test1_Count == 10);
    printf("\n\n :: Test 1 - END :: \n");
}

///////////////////////////////////////////////////////////////
//															 //
// Test 2: Testing mutexes									 //
//															 //
///////////////////////////////////////////////////////////////

ULONG Test2_Count;

VOID
Test2_Thread1 (
    __in UT_ARGUMENT Argument
    ) 
{
    PUTHREAD_MUTEX Mutex;
    
    Mutex = (PUTHREAD_MUTEX) Argument;

    printf("Thread1 running\n");

    printf("Thread1 acquiring the mutex...\n");
    UtAcquireMutex(Mutex);
    printf("Thread1 acquired the mutex...\n");
    
    UtYield();

    printf("Thread1 acquiring the mutex again...\n");
    UtAcquireMutex(Mutex);
    printf("Thread1 acquired the mutex again...\n");

    UtYield();

    printf("Thread1 releasing the mutex...\n");
    UtReleaseMutex(Mutex);
    printf("Thread1 released the mutex...\n");

    UtYield();

    printf("Thread1 releasing the mutex again...\n");
    UtReleaseMutex(Mutex);
    printf("Thread1 released the mutex again...\n");

    printf("Thread1 exiting\n");
    ++Test2_Count;
}

VOID
Test2_Thread2 (
    __in UT_ARGUMENT Argument
    ) 
{
    PUTHREAD_MUTEX Mutex;

    Mutex = (PUTHREAD_MUTEX) Argument;

    printf("Thread2 running\n");

    printf("Thread2 acquiring the mutex...\n");
    UtAcquireMutex(Mutex);
    printf("Thread2 acquired the mutex...\n");
    
    UtYield();

    printf("Thread2 releasing the mutex...\n");
    UtReleaseMutex(Mutex);
    printf("Thread2 released the mutex...\n");
    
    printf("Thread2 exiting\n");
    ++Test2_Count;
}

VOID
Test2_Thread3 (
    __in UT_ARGUMENT Argument
    ) 
{
    PUTHREAD_MUTEX Mutex;

    Mutex = (PUTHREAD_MUTEX) Argument;
    
    printf("Thread3 running\n");

    printf("Thread3 acquiring the mutex...\n");
    UtAcquireMutex(Mutex);
    printf("Thread3 acquired the mutex...\n");
    
    UtYield();

    printf("Thread3 releasing the mutex...\n");
    UtReleaseMutex(Mutex);
    printf("Thread3 released the mutex...\n");
    
    printf("Thread3 exiting\n");
    ++Test2_Count;
}

VOID
Test2 (
    ) 
{
    UTHREAD_MUTEX Mutex;
    
    UtInitializeMutex(&Mutex, FALSE);
    
    printf("\n-:: Test 2 - BEGIN ::-\n\n");

    Test2_Count = 0; // makes test2 non-reentrant
    
    UtCreate(Test2_Thread1, &Mutex);
    UtCreate(Test2_Thread2, &Mutex);
    UtCreate(Test2_Thread3, &Mutex);
    UtRun();

    _ASSERTE(Test2_Count == 3);
    
    printf("\n-:: Test 2 -  END  ::-\n");
}

///////////////////////////////////////////////////////////////
//															 //
// Test 3: building a mailbox with a mutex and a semaphore   //
//															 //
///////////////////////////////////////////////////////////////

//
// Mailbox containing message queue, a lock to ensure exclusive access 
// and a semaphore to control the message queue.
//

typedef struct _MAILBOX {
    UTHREAD_MUTEX Lock;      
    UTHREAD_SEMAPHORE Semaphore;   
    LIST_ENTRY MessageQueue;
} MAILBOX, *PMAILBOX;

//
// Mailbox message.
//

typedef struct _MAILBOX_MESSAGE {
    LIST_ENTRY QueueEntry;
    PVOID Data;
} MAILBOX_MESSAGE, *PMAILBOX_MESSAGE;

static 
VOID 
Mailbox_Initialize (
    PMAILBOX Mailbox
    )
{
    UtInitializeMutex(&Mailbox->Lock, FALSE);
    UtInitializeSemaphore(&Mailbox->Semaphore, 0, 20000);
    InitializeListHead(&Mailbox->MessageQueue);
}

static
VOID
Mailbox_Post (
    __inout PMAILBOX Mailbox,
    __in PVOID Data
    )
{
    PMAILBOX_MESSAGE Message;

    //
    // Create an envelope.
    //
    
    Message = (PMAILBOX_MESSAGE) malloc(sizeof *Message);

    _ASSERTE(Message != NULL);

    Message->Data = Data;;

    //
    // Insert the message in the mailbox queue.
    //

    UtAcquireMutex(&Mailbox->Lock);

    UtYield();
    InsertTailList(&Mailbox->MessageQueue, &Message->QueueEntry);

    //printf("** enqueued: 0x%08x **\n", Message);
    UtReleaseMutex(&Mailbox->Lock);
    
    //
    // Add one permit to indicate the availability of one more message.
    // 

    UtReleaseSemaphore(&Mailbox->Semaphore, 1);
}

static 
PVOID
Mailbox_Wait (
    __inout PMAILBOX Mailbox
    )
{
    PVOID Data;
    PMAILBOX_MESSAGE Message;
    
    //
    // Wait for a message to be available in the mailbox.
    //

    UtAcquireSemaphore(&Mailbox->Semaphore, 1);
    
    //
    // Get the envelope from the mailbox queue.
    //

    UtAcquireMutex(&Mailbox->Lock);
    
    UtYield();
    Message = CONTAINING_RECORD(RemoveHeadList(&Mailbox->MessageQueue), MAILBOX_MESSAGE, QueueEntry);
    
    _ASSERTE(Message != NULL);
    //printf("** dequeued: 0x%08x **\n", Message);
    
    UtReleaseMutex(&Mailbox->Lock);
    
    //
    // Extract the message from the envelope.
    //

    Data = Message->Data;

    //
    // Destroy the envelope and return the message.
    //

    free(Message);
    return Data;
}

#define TERMINATOR ((PCHAR)~0)

ULONG Test3_CountProducers;
ULONG Test3_CountConsumers;

VOID
Test3_ProducerThread (
    __in UT_ARGUMENT Argument
    ) 
{
    static ULONG CurrentId = 0;
    ULONG ProducerId;
    PMAILBOX Mailbox;
    PCHAR Message;
    ULONG MessageNumber;

    Mailbox = (PMAILBOX) Argument;
    ProducerId = ++CurrentId;
    
    for (MessageNumber = 0; MessageNumber < 5000; ++MessageNumber) {
        Message = (PCHAR) malloc(64);
        sprintf_s(Message, 64, "Message %04d from producer %d", MessageNumber, ProducerId);
        printf(" ** producer %d: sending message %04d [0x%08x]\n", ProducerId, MessageNumber, Message);
        
        Mailbox_Post(Mailbox, Message);
        
        if ((rand() % 2) == 0) { 
            UtYield();
        }
        // Sleep(1000); 
    }
    
    ++Test3_CountProducers;
}

VOID
Test3_ConsumerThread (
    __in UT_ARGUMENT Argument
    ) 
{
    ULONG ConsumerId;
    static ULONG CurrentId = 0;
    PMAILBOX Mailbox;
    PCHAR Message;
    ULONG MessageCount;
    
    ConsumerId = ++CurrentId;
    Mailbox = (PMAILBOX) Argument;
    MessageCount = 0;

    do {

        //
        // Get a message from the mailbox.
        //

        Message = (PCHAR) Mailbox_Wait(Mailbox);
        if (Message != TERMINATOR) {
            ++MessageCount;
            printf(" ++ consumer %d: got \"%s\" [0x%08x]\n", ConsumerId, Message, Message);
            
            //
            // Free the memory used by the message.
            //

            free(Message);
        } else {
            printf(" ++ consumer %d: exiting after %d messages\n", ConsumerId, MessageCount);
            break;
        }
    } while (TRUE);
    
    ++Test3_CountConsumers;
}

VOID
Test3_FirstThread (
    UT_ARGUMENT Argument
    )
{
    MAILBOX Mailbox;

    UNREFERENCED_PARAMETER(Argument);

    Mailbox_Initialize(&Mailbox);

    Test3_CountProducers = 0;
    Test3_CountConsumers = 0;

    UtCreate(Test3_ConsumerThread, &Mailbox);
    UtCreate(Test3_ConsumerThread, &Mailbox);
    UtCreate(Test3_ProducerThread, &Mailbox);
    UtCreate(Test3_ProducerThread, &Mailbox);
    UtCreate(Test3_ProducerThread, &Mailbox);
    UtCreate(Test3_ProducerThread, &Mailbox);

    do {
        UtYield();
    } while (Test3_CountProducers != 4);

    Mailbox_Post(&Mailbox, TERMINATOR);
    Mailbox_Post(&Mailbox, TERMINATOR);
    
    do {
        UtYield();
    } while (Test3_CountConsumers != 2);
}

VOID
Test3 ( 
    ) 
{
    printf("\n-:: Test 3 - BEGIN ::-\n\n");
    UtCreate(Test3_FirstThread, NULL);
    UtRun();
    printf("\n-:: Test 3 -  END  ::-\n");
}

VOID
__cdecl
main (
    __in ULONG argc,
    __in_ecount(argc) PCHAR argv[]
    )
{	
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    Test1();
    Test2();
    Test3();

    getchar();
}
