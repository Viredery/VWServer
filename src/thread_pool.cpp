#include "thread_pool.h"
#include <algorithm>

namespace VWServer
{

ThreadPool *ThreadPool::singleton = NULL;

ThreadPool *ThreadPool::getThreadPool(int n) {
    if(singleton == NULL && n != 0)
        singleton = new ThreadPool(n);
    return singleton;
}

ThreadPool::ThreadPool(int n) {
    pthread_mutex_init(&taskMutex, NULL);
    pthread_mutex_init(&threadMoveMutex, NULL);
    pthread_cond_init(&taskCond, NULL);
    threadNum = n;
    createPool();
}

ThreadPool::~ThreadPool() {
    pthread_mutex_destroy(&taskMutex);
    pthread_mutex_destroy(&threadMoveMutex);
    pthread_cond_destroy(&taskCond);
    if (NULL != singleton)
        delete singleton;
}

void ThreadPool::createPool() {
    int i;
    for(i = 0; i != threadNum; i++) {
        pthread_t tid;
        if(pthread_create(&tid, NULL, threadCall, this) != 0)
            throw -1;
	    idleThreads.push_back(tid);
    }
}

void *ThreadPool::threadCall(void *data) {
    ThreadPool *ptrThreadPool = static_cast<ThreadPool *>(data);
    if (NULL != ptrThreadPool) {
        ptrThreadPool->runThread();
    }
    return NULL;
}

void ThreadPool::runThread() {
    pthread_t sefPid = pthread_self();
    while(true) {
        pthread_mutex_lock(&taskMutex);
        while(taskQueue.empty())
            pthread_cond_wait(&taskCond, &taskMutex);
        Task *task = taskQueue.front();
        taskQueue.pop();
        pthread_mutex_unlock(&taskMutex);
    
        pthread_mutex_lock(&threadMoveMutex);
        idle2Busy(sefPid);
        pthread_mutex_unlock(&threadMoveMutex);
    
        //run the task
        task->run();
    
        pthread_mutex_lock(&threadMoveMutex);
        busy2Idle(sefPid);
        pthread_mutex_unlock(&threadMoveMutex);
    }
}

void ThreadPool::busy2Idle(pthread_t pid) {
    std::list<pthread_t>::iterator iterAim = std::find(busyThreads.begin(), busyThreads.end(), pid);
    busyThreads.erase(iterAim);
    idleThreads.push_back(pid);
}

void ThreadPool::idle2Busy(pthread_t pid) {
    std::list<pthread_t>::iterator iterAim = std::find(idleThreads.begin(), idleThreads.end(), pid);
    idleThreads.erase(iterAim);
    busyThreads.push_back(pid);
}

void ThreadPool::addTask(Task *t) {
    pthread_mutex_lock(&taskMutex);
    taskQueue.push(t);
    pthread_mutex_unlock(&taskMutex);
    pthread_cond_signal(&taskCond);
}

}