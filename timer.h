#ifndef TIMER_H
#define TIMER_H

#include <time.h>

extern int TIMESLOT;

class connection;  // 前向声明

// 定时器类
class client_timer {
public:
    connection*   http_conn;
    time_t        expire;  // 任务超时时间，使用绝对时间
    client_timer* prev;    // 指向前一个定时器
    client_timer* next;    // 指向后一个定时器

    client_timer(connection& conn);

    void renew_expire_time();  // 更新定时器超时时间
};

inline client_timer::client_timer(connection& conn) : http_conn(&conn), expire(0), prev(nullptr), next(nullptr) {}

inline void client_timer::renew_expire_time() {
    time_t curtime = time(nullptr);
    expire         = curtime + 3 * TIMESLOT;
}

#endif