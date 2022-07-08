#ifndef SEM_H
#define SEM_H

#include <semaphore.h>

#include <exception>

// 信号量类
class sem {
private:
    sem_t sema;

public:
    sem();
    ~sem();
    bool wait();  // 等待信号量
    bool post();  // 增加信号量
};

inline sem::sem() {
    if (sem_init(&sema, 0, 0) != 0) {
        throw std::exception();
    }
}

inline sem::~sem() { sem_destroy(&sema); }

inline bool sem::wait() { return sem_wait(&sema) == 0; }

inline bool sem::post() { return sem_post(&sema) == 0; }

#endif