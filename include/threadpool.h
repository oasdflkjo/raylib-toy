#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <windows.h>

typedef struct SimpleThreadPool {
    PTP_POOL pool;
    TP_CALLBACK_ENVIRON callBackEnviron;
} SimpleThreadPool;

SimpleThreadPool* SimpleThreadPool_Init();
BOOL SimpleThreadPool_Submit(SimpleThreadPool* tp, PTP_WORK_CALLBACK workCallback, PVOID context);
void SimpleThreadPool_Destroy(SimpleThreadPool* tp);

#endif // THREADPOOL_H
