#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#include "timer.h"

class client_timer;

// 定时器链表，它是一个升序、双向链表，且带有头节点和尾节点。
class client_timer_list {
public:
    client_timer* head;  // 头结点
    client_timer* tail;  // 尾结点

public:
    client_timer_list();
    ~client_timer_list();

    // 将目标定时器timer添加到链表中
    void add_timer_to_list(client_timer* timer);

    // 当某个定时任务发生变化时，调整对应的定时器在链表中的位置
    void adjust_timer_on_list(client_timer* timer);

    // 将目标定时器 timer 从链表中删除
    void del_timer_from_list(client_timer* timer);

    // SIGALARM 信号每次被触发就在其信号处理函数中执行一次 tick() 函数，以处理链表上到期任务
    void tick();

private:
    // 将目标定时器 timer 添加到节点 list_head 之后的部分链表中
    void add_timer_to_list(client_timer* timer, client_timer* list_head);
};

#endif