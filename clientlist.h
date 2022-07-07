#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#include "timer.h"

// 定时器链表，它是一个升序、双向链表，且带有头节点和尾节点。
class client_timer_list {
private:
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

client_timer_list::client_timer_list() : head(nullptr), tail(nullptr) {}

client_timer_list::~client_timer_list() {
    client_timer* tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void client_timer_list::tick() {
    if (!head) {
        return;
    }

    time_t cur = time(nullptr);  // 获取当前系统时间

    client_timer* tmp = head;
    while (tmp) {
        // 比较定时器的超时值和系统当前时间，以判断定时器是否到期
        if (cur < tmp->expire) {
            break;
        }

        // 调用定时器的回调函数，以执行定时任务
        tmp->cb_func(tmp->user_data);
        
        head = tmp->next;
        if (head) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

void client_timer_list::del_timer_from_list(client_timer* timer) {
    if (!timer) {
        return;
    }
    // 下面这个条件成立表示链表中只有一个定时器，即目标定时器
    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    /* 
        如果链表中至少有两个定时器，且目标定时器是链表的头节点，
        则将链表的头节点重置为原头节点的下一个节点，然后删除目标定时器。 
    */
    if (timer == head) {
        head       = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    /* 
        如果链表中至少有两个定时器，且目标定时器是链表的尾节点，
        则将链表的尾节点重置为原尾节点的前一个节点，然后删除目标定时器。
    */
    if (timer == tail) {
        tail       = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    // 如果目标定时器位于链表的中间，则把它前后的定时器串联起来，然后删除目标定时器
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void client_timer_list::adjust_timer_on_list(client_timer* timer) {
    if (!timer) {
        return;
    }

    client_timer* tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire)) {
        return;
    }

    if (timer == head) {
        // 如果目标定时器是链表的头节点，则将该定时器从链表中取出并重新插入链表
        head        = head->next;
        head->prev  = nullptr;
        timer->next = nullptr;
        add_timer_to_list(timer, head);
    } else {
        // 如果目标定时器不是链表的头节点，则将该定时器从链表中取出，然后插入其原来所在位置后的部分链表中
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer_to_list(timer, timer->next);
    }
}

void client_timer_list::add_timer_to_list(client_timer* timer) {
    if (!timer) {
        return;
    }
    if (!head) {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev  = timer;
        head        = timer;
        return;
    }
    add_timer_to_list(timer, head);
}

void client_timer_list::add_timer_to_list(client_timer* timer, client_timer* list_head) {
    client_timer* pre = list_head;
    client_timer* tmp = list_head->next;
    /* 
        遍历 list_head 节点之后的部分链表，直到找到一个超时时间
        大于目标定时器的超时时间节点并将目标定时器插入该节点之前 
    */
    while (tmp) {
        if (timer->expire < tmp->expire) {
            pre->next   = timer;
            timer->next = tmp;
            tmp->prev   = timer;
            timer->prev = pre;
            break;
        }
        pre = tmp;
        tmp = tmp->next;
    }
    /*  
        如果遍历完 lst_head 节点之后的部分链表，仍未找到超时时间大于目标定时器
        的超时时间的节点，则将目标定时器插入链表尾部，并把它设置为链表新的尾节点
    */
    if (!tmp) {
        pre->next   = timer;
        timer->prev = pre;
        timer->next = nullptr;
        tail        = timer;
    }
}

#endif