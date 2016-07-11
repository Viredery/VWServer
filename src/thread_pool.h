#pragma once

#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include "task.h"
#include <pthread.h>
#include <list>
#include <queue>

namespace VWServer
{

class ThreadPool
{
public:
    /**
     * Singleton pattern
     * 使用者需要保证第一次调用时处于单线程环境下
     */
    static ThreadPool *getThreadPool(int n = 0);
    /**
     *  添加任务到线程池
     *  Task类中有一个run()方法
     */
    void addTask(Task *t);
private:
    ThreadPool(int n = 0);
    ~ThreadPool();
    void createPool();

    static void *threadCall(void *data);
    void runThread();
    void busy2Idle(pthread_t pid);
    void idle2Busy(pthread_t pid);

    static ThreadPool *singleton;

    pthread_mutex_t taskMutex;
    pthread_cond_t taskCond;
    pthread_mutex_t threadMoveMutex;
    //线程池中的线程数量
    int threadNum;
    //维护工作线程的链表
    std::list<pthread_t> busyThreads;
    //维护空闲线程的链表
    std::list<pthread_t> idleThreads;
    //任务队列
    std::queue<Task *> taskQueue;
};

}

#endif
