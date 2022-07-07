#ifndef TIMER_H
#define TIMER_H

#include <arpa/inet.h>
#include <time.h>

class client_timer;  // 前向声明

// 客户端数据
struct client_data {
    sockaddr_in   client_address;  // 客户端地址
    int           sockfd;          // socket文件描述符
    client_timer* timer;           // 定时器
};

// 定时器类
class client_timer {
public:
    client_data*  user_data;
    time_t        expire;  // 任务超时时间，使用绝对时间
    client_timer* prev;    // 指向前一个定时器
    client_timer* next;    // 指向后一个定时器

    client_timer() : user_data(nullptr), expire(0), prev(nullptr), next(nullptr) {}

    void (*cb_func)(client_data*);  // 任务回调函数，回调函数处理的客户数据，由定时器的执行者传递给回调函数
};

#endif