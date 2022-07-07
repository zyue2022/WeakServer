#ifndef COND_H
#define COND_H

#include <pthread.h>

#include <exception>

// 条件变量类
class cond {
private:
    pthread_cond_t m_cond;

public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t*);
    bool timedwait(pthread_mutex_t*, timespec);
    bool signal();
    bool broadcast();
};

inline cond::cond() {
    if (pthread_cond_init(&m_cond, nullptr) != 0) {
        throw std::exception();
    }
}

inline cond::~cond() { pthread_cond_destroy(&m_cond); }

inline bool cond::wait(pthread_mutex_t* mutex) {
    return pthread_cond_wait(&m_cond, mutex) == 0;
}

inline bool cond::timedwait(pthread_mutex_t* mutex, timespec t) {
    return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
}

inline bool cond::signal() { return pthread_cond_signal(&m_cond) == 0; }

inline bool cond::broadcast() { return pthread_cond_broadcast(&m_cond) == 0; }

#endif
/*
    inline的用处之一是：非 template 函数，
    把成员或非成员定义放在头文件中，定义前不加inline ，
    如果头文件被多个cpp文件引用，编译器会报错multiple definition
*/