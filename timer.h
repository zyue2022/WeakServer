#ifndef TIMER_H
#define TIMER_H

#include <time.h>

class connection; // 前向声明

// 定时器类
class client_timer {
public:
    connection&   http_conn;
    time_t        expire;  // 任务超时时间，使用绝对时间
    client_timer* prev;    // 指向前一个定时器
    client_timer* next;    // 指向后一个定时器

    client_timer(connection& conn) : http_conn(conn), expire(0), prev(nullptr), next(nullptr) {}
};

#endif