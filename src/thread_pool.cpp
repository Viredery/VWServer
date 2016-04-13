#include "thread_pool.h"
#include <algorithm>
using namespace std;

Task::Task() {
}

Task::~Task() {
}

void Task::run() {
}

Thread_pool *Thread_pool::singleton = NULL;

list<pthread_t> Thread_pool::busy_threads;
list<pthread_t> Thread_pool::idle_threads;
queue<Task *> Thread_pool::task_queue;

pthread_mutex_t Thread_pool::task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Thread_pool::thread_move_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Thread_pool::task_cond = PTHREAD_COND_INITIALIZER;

Thread_pool *Thread_pool::get_thread_pool(int n) {
    if(singleton == NULL && n != 0)
        singleton = new Thread_pool(n);
    return singleton;
}

Thread_pool::Thread_pool(int n) {
    thread_num = n;
    create_pool();
}

Thread_pool::~Thread_pool() {
    delete singleton;
}

void Thread_pool::create_pool() {
    int i;
    for(i = 0; i != thread_num; i++) {
        pthread_t tid;
	if(pthread_create(&tid, NULL, thread_call, NULL) != 0)
	    throw -1;
	idle_threads.push_back(tid);
    }
}

void *Thread_pool::thread_call(void *data) {
    pthread_t sef_pid = pthread_self();
    while(true) {
        pthread_mutex_lock(&task_mutex);
        while(task_queue.empty())
            pthread_cond_wait(&task_cond, &task_mutex);
        Task *task = task_queue.front();
	task_queue.pop();
	pthread_mutex_unlock(&task_mutex);
	
	pthread_mutex_lock(&thread_move_mutex);
	idle2busy(sef_pid);
	pthread_mutex_unlock(&thread_move_mutex);
        //run the task
        task->run();
	pthread_mutex_lock(&thread_move_mutex);
	busy2idle(sef_pid);
	pthread_mutex_unlock(&thread_move_mutex);
    }
}

void Thread_pool::busy2idle(pthread_t pid) {
    list<pthread_t>::iterator aim_p = find(busy_threads.begin(), busy_threads.end(), pid);
    busy_threads.erase(aim_p);
    idle_threads.push_back(pid);
}

void Thread_pool::idle2busy(pthread_t pid) {
    list<pthread_t>::iterator aim_p = find(idle_threads.begin(), idle_threads.end(), pid);
    idle_threads.erase(aim_p);
    busy_threads.push_back(pid);
}

void Thread_pool::add_task(Task *t) {
    pthread_mutex_lock(&task_mutex);
    task_queue.push(t);
    pthread_mutex_unlock(&task_mutex);
    pthread_cond_signal(&task_cond);
}
