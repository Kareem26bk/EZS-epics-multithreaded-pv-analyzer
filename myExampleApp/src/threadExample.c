#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <iocsh.h>

#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <dbCommon.h>

// Shared Data Structure
typedef struct {
    epicsMutexId mutex;
    epicsEventId event;
    double *buffer;
    long num_elements;
    epicsTimeStamp timestamp;
    int is_active;
} SharedData;

static SharedData sharedData = {0};

// Private Worker Arguments
typedef struct {
    double *buffer;
    long num_elements;
    epicsTimeStamp timestamp;
} WorkerArgs;

// Prime Check Function
static int is_prime(long n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (long i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }
    return 1;
}

// --------------------------------------------------------
// Tier 3: Worker Thread (Priority: Low, Short-lived)
// --------------------------------------------------------
static void workerThread(void *arg) {
    WorkerArgs *workerData = (WorkerArgs *)arg;
    epicsThreadId myId = epicsThreadGetIdSelf();

    double sum = 0;
    for (long i = 0; i < workerData->num_elements; i++) {
        sum += workerData->buffer[i];
    }

    long integer_sum = (long)sum;
    printf("New worker threadId: %p\n", (void*)myId);
    printf("sum = %ld\n", integer_sum);

    if (is_prime(integer_sum)) {
        char timeStr[64];
        epicsTimeToStrftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S.%09fZ", &workerData->timestamp);
        printf("worker data is prim %ld at %s\n", integer_sum, timeStr);
    }

    // SYNCHRONISATION POINT: Free private heap copy
    free(workerData->buffer);
    free(workerData);
}

// --------------------------------------------------------
// Tier 2: Dispatcher Thread (Priority: Medium, Persistent)
// --------------------------------------------------------
static void dispatcherThread(void *arg) {
    while (sharedData.is_active) {
        // SYNCHRONISATION POINT: Block until event signal received (No Busy-Wait)
        epicsEventWait(sharedData.event);

        if (!sharedData.is_active) break;

        WorkerArgs *workerData = (WorkerArgs *)malloc(sizeof(WorkerArgs));
        if (!workerData) continue;

        // SYNCHRONISATION POINT: Lock Mutex to safely copy from Shared Buffer
        epicsMutexLock(sharedData.mutex);
        workerData->num_elements = sharedData.num_elements;
        workerData->timestamp = sharedData.timestamp;
        workerData->buffer = (double *)malloc(workerData->num_elements * sizeof(double));
        memcpy(workerData->buffer, sharedData.buffer, workerData->num_elements * sizeof(double));
        // SYNCHRONISATION POINT: Unlock Mutex immediately after copy
        epicsMutexUnlock(sharedData.mutex);

        // SYNCHRONISATION POINT: Create Worker Thread with LOW priority
        epicsThreadCreate("workerThread", epicsThreadPriorityLow, epicsThreadGetStackSize(epicsThreadStackSmall), (EPICSTHREADFUNC)workerThread, workerData);
    }
}

// --------------------------------------------------------
// Tier 1: Event Callback (Priority: High, Non-blocking)
// --------------------------------------------------------
static void eventCallback(void *user_arg, struct dbChannel *chan, int eventsRemaining, struct db_field_log *pfl) {
    long options = 0;
    long nRequest = sharedData.num_elements;

    // SYNCHRONISATION POINT: Lock Mutex before writing to Shared Buffer
    epicsMutexLock(sharedData.mutex);

    dbChannelGet(chan, DBF_DOUBLE, sharedData.buffer, &options, &nRequest, NULL);
    struct dbCommon *precord = (struct dbCommon *)dbChannelRecord(chan);
    sharedData.timestamp = precord->time;

    // SYNCHRONISATION POINT: Unlock Mutex
    epicsMutexUnlock(sharedData.mutex);

    // SYNCHRONISATION POINT: Signal Dispatcher Thread to wake up
    epicsEventSignal(sharedData.event);
}

// --------------------------------------------------------
// Main Entry: threadExample
// --------------------------------------------------------
static void threadExampleCall(const iocshArgBuf *args) {
    const char *pvName = args[0].sval;
    if (!pvName) {
        printf("Error: PV name is required.\n");
        return;
    }

    struct dbChannel *chan = dbChannelCreate(pvName);
    if (!chan) {
        printf("Error: Could not create dbChannel for PV '%s'.\n", pvName);
        return;
    }

    if (dbChannelOpen(chan) != 0) {
        printf("Error: Could not open dbChannel for PV '%s'.\n", pvName);
        dbChannelDelete(chan);
        return;
    }

    sharedData.num_elements = dbChannelFinalElements(chan);
    sharedData.buffer = (double *)malloc(sharedData.num_elements * sizeof(double));

    // SYNCHRONISATION POINT: Initialize Mutex and Event (epicsEventEmpty)
    if (!sharedData.mutex) sharedData.mutex = epicsMutexCreate();
    if (!sharedData.event) sharedData.event = epicsEventCreate(epicsEventEmpty);
    sharedData.is_active = 1;

    printf("threadExample: '%s' NSAM = %ld\n", pvName, sharedData.num_elements);

    // Start Dispatcher Thread
    epicsThreadCreate("dispatcherThread", epicsThreadPriorityMedium, epicsThreadGetStackSize(epicsThreadStackMedium), (EPICSTHREADFUNC)dispatcherThread, NULL);

    // Setup Subscription
    dbEventCtx ctx = db_init_events();
    if (!ctx) {
        printf("Error: Could not initialize dbEvent context.\n");
        return;
    }

    if (db_add_event(ctx, chan, eventCallback, NULL, DBE_VALUE)) {
        db_start_events(ctx, "eventTask", NULL, NULL, epicsThreadPriorityHigh);
        printf("threadExample: subscription active for '%s'\n", pvName);
    } else {
        printf("Error: Could not add event subscription.\n");
    }
}

// --------------------------------------------------------
// IOC Shell Registration
// --------------------------------------------------------
static const iocshArg threadExampleArg0 = {"pv-name", iocshArgString};
static const iocshArg * const threadExampleArgs[] = {&threadExampleArg0};
static const iocshFuncDef threadExampleFuncDef = {
    "threadExample", 1, threadExampleArgs,
    "Check on a given compress record that the sum of val-array is prime\n"
    "Output of the timestamp if prim. The calculation must not influence the PV process.\n"
    "Example: threadExample $(user):compressExample"
};

static void threadExampleRegister(void) {
    iocshRegister(&threadExampleFuncDef, threadExampleCall);
}
epicsExportRegistrar(threadExampleRegister);
