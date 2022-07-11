/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;  
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include "cond.h"
#include "locker.h"

template <class T>
class blockqueue {
private:
    locker m_mutex;
    cond   m_cond;

    T *m_array;

    int m_size;
    int m_max_size;
    int m_front;
    int m_back;

public:
    blockqueue(int max_size = 1000);
    ~blockqueue();

    bool empty();
    bool full();
    bool front(T &value);
    bool back(T &value);
    void clear();

    int size();
    int max_size();

    bool push(const T &item);
    bool pop(T &item);
    bool pop(T &item, int ms_timeout);
};

template <class T>
blockqueue<T>::blockqueue(int max_size) {
    if (max_size <= 0) {
        exit(-1);
    }

    m_max_size = max_size;
    m_array    = new T[m_max_size];
    m_size     = 0;
    m_front    = -1;
    m_back     = -1;
}

template <class T>
blockqueue<T>::~blockqueue() {
    m_mutex.lock();
    if (m_array != NULL) delete[] m_array;
    m_mutex.unlock();
}

template <class T>
void blockqueue<T>::clear() {
    m_mutex.lock();
    m_size  = 0;
    m_front = -1;
    m_back  = -1;
    m_mutex.unlock();
}

//判断队列是否满了
template <class T>
bool blockqueue<T>::full() {
    m_mutex.lock();
    if (m_size >= m_max_size) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

//判断队列是否为空
template <class T>
bool blockqueue<T>::empty() {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

//返回队首元素
template <class T>
bool blockqueue<T>::front(T &value) {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

//返回队尾元素
template <class T>
bool blockqueue<T>::back(T &value) {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <class T>
int blockqueue<T>::size() {
    int tmp = 0;

    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();

    return tmp;
}

template <class T>
int blockqueue<T>::max_size() {
    int tmp = 0;

    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();

    return tmp;
}

//往队列添加元素，需要将所有使用队列的线程先唤醒
//当有元素push进队列,相当于生产者生产了一个元素
//若当前没有线程等待条件变量,则唤醒无意义
template <class T>
bool blockqueue<T>::push(const T &item) {
    m_mutex.lock();
    if (m_size >= m_max_size) {
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    m_back = (m_back + 1) % m_max_size;

    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

//pop时,如果当前队列没有元素,将会等待条件变量
template <class T>
bool blockqueue<T>::pop(T &item) {
    m_mutex.lock();
    while (m_size <= 0) {
        if (!m_cond.wait(m_mutex.get())) {
            m_mutex.unlock();
            return false;
        }
    }
    m_front = (m_front + 1) % m_max_size;

    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

//增加了超时处理
template <class T>
bool blockqueue<T>::pop(T &item, int ms_timeout) {
    struct timespec t   = {0, 0};
    struct timeval  now = {0, 0};
    gettimeofday(&now, nullptr);
    m_mutex.lock();
    if (m_size <= 0) {
        t.tv_sec  = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!m_cond.timewait(m_mutex.get(), t)) {
            m_mutex.unlock();
            return false;
        }
    }

    if (m_size <= 0) {
        m_mutex.unlock();
        return false;
    }

    m_front = (m_front + 1) % m_max_size;
    item    = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

#endif
