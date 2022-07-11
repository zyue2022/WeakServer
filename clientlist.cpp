#include "clientlist.h"
#include "log.h"
#include "connection.h"

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
    time_t cur = time(nullptr);  // 获取当前系统时间

    if (!head) {
        LOG_INFO("empty timer list, curtime: %ld", cur);
        return;
    }

    client_timer* tmp = head;
    while (tmp) {
        // 比较定时器的超时值和系统当前时间，以判断定时器是否到期
        if (cur < tmp->expire) {
            break;
        }

        head = tmp->next;//更新头节点位置

        // 超时就先关闭连接，后删除相应定时器；
        LOG_INFO("find a timeout connection, which sockfd is %d", tmp->http_conn->sockfd);
        tmp->http_conn->close_sock();
        
        // 执行完定时器中的定时任务之后，就将它从链表中删除，并重置链表头节点
        head = tmp->next;
        if( head ) {
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