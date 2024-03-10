#include "threadpool.h"
#include <stdlib.h>

// Initialize the thread pool
SimpleThreadPool* SimpleThreadPool_Init() {
    SimpleThreadPool* tp = (SimpleThreadPool*)malloc(sizeof(SimpleThreadPool));
    if (!tp) return NULL;

    tp->pool = CreateThreadpool(NULL);
    if (!tp->pool) {
        free(tp);
        return NULL;
    }

    InitializeThreadpoolEnvironment(&tp->callBackEnviron);
    SetThreadpoolCallbackPool(&tp->callBackEnviron, tp->pool);

    return tp;
}

// Submit work to the thread pool
BOOL SimpleThreadPool_Submit(SimpleThreadPool* tp, PTP_WORK_CALLBACK workCallback, PVOID context) {
    if (!tp || !tp->pool) return FALSE;

    PTP_WORK work = CreateThreadpoolWork(workCallback, context, &tp->callBackEnviron);
    if (!work) return FALSE;

    SubmitThreadpoolWork(work);
    return TRUE;
}

// Cleanup the thread pool
void SimpleThreadPool_Destroy(SimpleThreadPool* tp) {
    if (!tp) return;

    CloseThreadpool(tp->pool);
    DestroyThreadpoolEnvironment(&tp->callBackEnviron);
    free(tp);
}
