#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool {
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_request = 10000);
    ~threadpool();

    bool append(T *request, int state);
    bool append_p(T *request);

private:
    static void *worker(void *arg);  // function for worker threads
    void run();

private:
    int thread_number;          // # threads in pool
    int max_requests;           // max # requests in queue
    pthread_t *threads;         // array of threads
    std::list<T*> work_queue;   // the work queue
    locker queue_locker;        // mutex for queue
    sem queue_stat;             // semaphore for incoming tasks
    connection_pool *conn_poll  // the database
    int actor_model;            // 0 for proactor, 1 for reactor
};

template <typename T>
threadpool<T>::threadpool(int am, connection_pool *cp, int tn, int mr) : 
        actor_model(am), thread_number(tn), max_requests(mr), threads(NULL), conn_pool(cp) {
    if (thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }

    threads = new pthread_T[thread_number];
    if (!threads) {
        throw std::exception();
    }

    for (int i = 0; i < thread_number; i++) {
        if (pthread_create(threads + i, nullptr, worker, this) != 0) {
            delete[] threads;
            throw std::exception();
        }
        if (pthread_detach(threads[i])) {
            delete[] threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool() {
    delete[] threads;
}

template <typename T>
bool threadpool<T>::append(T *request, int state) {
    queue_locker.lock();
    if (work_queue.size() >= max_requests) {
        queue_locker.unlock();
        return false;
    }
    request->state = state;
    work_queue.push_back(request)
    queue_locker.unlock();
    queue_stat.post();
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T *request) {
    queue_locker.lock();
    if (work_queue.size() >= max_requests) {
        queue_locker.unlock();
        return false;
    }
    work_queue.push_back(request)
    queue_locker.unlock();
    queue_stat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg) {
    threadpool *pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    while (1) {
        queue_stat.wait();
        queue_locker.lock();
        if (work_queue.empty()) {
            queue_locker.unlock();
            continue;
        }
        T *request = work_queue.front();
        work_queue.pop_front();
        queue_locker.unlock();

        if (actor_model == 1) {
            if (request->state == 0) {
                // read request
                if (request->read_once()) {
                    request->improve = 1;
                    connection_RAII mysqlcon(&request->mysql, conn_pool);
                    request->process();
                } else {
                    request->improve = 1;
                    request->timer_flag = 1;
                }
            } else {
                // write request
                if (request->write()) {
                    request->improve = 1;
                } else {
                    request->improve = 1;
                    request->timer_flag = 1;
                }
            }
        } else {
            connection_RAII mysqlcon(&request->mysql, conn_poll);
            request->process();
        }
    }
}

#endif
