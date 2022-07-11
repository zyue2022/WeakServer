#ifndef COND_H
#define COND_H

#include <pthread.h>
#include <semaphore.h>

#include <exception>

// 条件变量类
class cond {
public:
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }

    ~cond() { pthread_cond_destroy(&m_cond); }

    bool wait(pthread_mutex_t *m_mutex) { return pthread_cond_wait(&m_cond, m_mutex) == 0; }

    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }

    bool signal() { return pthread_cond_signal(&m_cond) == 0; }

    bool broadcast() { return pthread_cond_broadcast(&m_cond) == 0; }

private:
    pthread_cond_t m_cond;
};

#endif