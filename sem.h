#ifndef SEM_H
#define SEM_H

#include <semaphore.h>

#include <exception>

// 信号量类
class sem {
private:
    sem_t m_sem;

public:
    sem();
    ~sem();
    bool wait();  // 等待信号量
    bool post();  // 增加信号量
};

inline sem::sem() {
    if (sem_init(&m_sem, 0, 0) != 0) {
        throw std::exception();
    }
}

inline sem::~sem() { sem_destroy(&m_sem); }

inline bool sem::wait() { return sem_wait(&m_sem) == 0; }

inline bool sem::post() { return sem_post(&m_sem) == 0; }

#endif