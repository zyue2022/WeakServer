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
    pthread_t*      m_threads;             :    线程池数组
    int             max_num_of_requests;   :    请求队列大小
    std::queue<T*>  m_workqueue;           :    请求队列
    locker          m_queuelocker;         :    互斥锁
    sem             m_queuestat;           :    信号量，用于判断是否有任务需要处理
    bool            is_need_stop;          :    是否结束线程
*/

// 线程池模板类
template <typename T>
class threadpool {
private:
    int            num_of_thread;
    pthread_t*     m_threads;
    int            max_num_of_requests;
    std::queue<T*> m_workqueue;
    locker         m_queuelocker;
    sem            m_queuestat;
    bool           is_need_stop;

private:
    static void* worker(void*);

public:
    threadpool(int num = 8, int max = 10000);
    ~threadpool();
    bool append(T*);
    void run();
};

template <typename T>
threadpool<T>::threadpool(int num, int max)
    : num_of_thread(num),
      max_num_of_requests(max),
      is_need_stop(false),
      m_threads(nullptr) {
    if (num <= 0 || max <= 0) {
        throw std::exception();
    }

    m_threads = new pthread_t[num_of_thread];
    if (!m_threads) {
        throw std::exception();
    }

    // 创建线程，并设置线程分离方便回收
    for (int i = 0; i < num_of_thread; ++i) {
        printf("正在创建第 %d 个线程...\n", i);
        // worker必须是静态函数,传入this参数
        int ret = pthread_create(i + m_threads, nullptr, worker, this);
        if (ret != 0) {
            delete[] m_threads;
            throw std::exception();
        }

        ret = pthread_detach(m_threads[i]);
        if (ret != 0) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    is_need_stop = true;
}

template <typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() >= max_num_of_requests) {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push(request);
    m_queuelocker.unlock();
    m_queuestat.post();  // 信号量的V操作
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
        m_queuestat.wait();  // 信号量的P操作
        m_queuelocker.lock();

        // if (m_workqueue.empty()) {
        //     m_queuelocker.unlock();
        //     continue;
        // }

        T* request = m_workqueue.front();
        m_workqueue.pop();
        m_queuelocker.unlock();

        // 如果传进的request本身就是个null的话，必须要判断
        if (!request) {
            continue;
        }

        // process函数在httpconnection类
        request->process();
    }
}

#endif