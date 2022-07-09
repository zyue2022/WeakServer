#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <stdio.h>

#include <exception>
#include <queue>

#include "locker.h"
#include "sem.h"

/*
    int             num_of_thread;         :    线程数量
    pthread_t*      threads;               :    线程池数组
    int             max_num_of_task;       :    请求队列大小
    std::queue<T*>  workqueue;             :    请求队列
    locker          queuelocker;           :    互斥锁
    sem             queuestat;             :    信号量，用于判断是否有任务需要处理
    bool            is_need_stop;          :    是否结束线程
*/

// 线程池模板类
template <typename T>
class threadpool {
private:
    int               num_of_thread;
    pthread_t*        threads;
    long unsigned int max_num_of_task;
    std::queue<T*>    workqueue;
    locker            queuelocker;
    sem               queuestat;
    bool              is_need_stop;

public:
    threadpool(int num = 8, int max = 10000);
    ~threadpool();

    static void* worker(void*);

    bool append(T*);
    void run();
};

template <typename T>
threadpool<T>::threadpool(int num, int max)
    : num_of_thread(num), threads(nullptr), max_num_of_task(max), is_need_stop(false) {
    if (num <= 0 || max <= 0) {
        throw std::exception();
    }

    threads = new pthread_t[num_of_thread];
    if (!threads) {
        throw std::exception();
    }

    // 创建线程，并设置线程分离方便回收
    for (int i = 0; i < num_of_thread; ++i) {
        printf("正在创建第 %d 个线程...\n", i);
        // worker必须是静态函数,传入this参数
        int ret = pthread_create(i + threads, nullptr, worker, this);
        if (ret != 0) {
            delete[] threads;
            throw std::exception();
        }

        ret = pthread_detach(threads[i]);
        if (ret != 0) {
            delete[] threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool() {
    delete[] threads;
    is_need_stop = true;
}

template <typename T>
bool threadpool<T>::append(T* task) {
    queuelocker.lock();
    if (workqueue.size() >= max_num_of_task) {
        queuelocker.unlock();
        return false;
    }

    workqueue.push(task);
    queuelocker.unlock();
    queuestat.post();  // 信号量的V操作
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    // 线程池一旦对象析构，stop设置为true，所有子线程执行结束
    while (!is_need_stop) {
        // 先加锁后wait，如果wait阻塞了，那锁也不释放，造成死锁
        queuestat.wait();  // 信号量的P操作
        queuelocker.lock();

        if (workqueue.empty()) {
            queuelocker.unlock();
            continue;
        }

        T* task = workqueue.front();
        workqueue.pop();
        queuelocker.unlock();

        // 如果传进的连接task本身就是个null的话，必须要判断
        if (!task) {
            continue;
        }

        // process函数在taskection类
        task->process();
    }
}

#endif