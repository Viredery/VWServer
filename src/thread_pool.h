#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <pthread.h>
#include <list>
#include <queue>

class Task {
public:
    Task();
    ~Task();
    void run();
};

class Thread_pool
{
public:
    static Thread_pool *get_thread_pool(int n = 0);
    ~Thread_pool();
    void add_task(Task *t);
private:
    void create_pool();
    static Thread_pool *singleton;
    Thread_pool(int n = 0);
    static void *thread_call(void *data);
    static void busy2idle(pthread_t pid);
    static void idle2busy(pthread_t pid);
    static pthread_mutex_t task_mutex;
    static pthread_cond_t task_cond;
    static pthread_mutex_t thread_move_mutex;
    int thread_num;
    static std::list<pthread_t> busy_threads;
    static std::list<pthread_t> idle_threads;
    static std::queue<Task *> task_queue;
};
#endif
