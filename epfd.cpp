#include "epfd.h"

// 设置文件描述符非阻塞
void set_fd_nonblock(int fd) {
    if (fd == -1) return;
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

/*
    添加文件描述符到epoll对象实例中
    oneshot指的某socket对应的fd事件最多只能被检测一次
    针对文件描述符，如果设置了oneshot，那么只会触发一次;
    防止一个线程在处理业务呢，然后来数据了，又从线程池里拿一个线程来处理新的业务;
*/
void add_fd_to_epoll(int epollfd, int fd, bool one_shot, bool is_ET) {
    if (fd == -1) return;
    epoll_event event;
    event.data.fd = fd;

    if (!is_ET) {
        event.events = EPOLLIN | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;  //边沿触发
    }

    if (one_shot) {
        // 防止同一个通信被不同线程处理
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置文件描述符非阻塞
    set_fd_nonblock(fd);
}

// 从epoll对象中删除文件描述符
void remove_fd_from_epoll(int epollfd, int fd) {
    if (fd == -1) return;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/*
    在epoll对象中修改文件描述符
    重置socket上EPOLLONESHOT事件,确保下一次可读时 EPOLLIN 可触发
*/
void modify_fd_from_epoll(int epollfd, int fd, int ev) {
    if (fd == -1) return;
    epoll_event event;
    event.data.fd = fd;
    // 默认边缘触发
    event.events  = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 设置端口复用
void reuse_addr(int sockfd) {
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
}

// 打印新连接的客户端信息
void print_client_info(sockaddr_in client_address) {
    char clientIp[16] = {0};
    inet_ntop(AF_INET, &client_address.sin_addr.s_addr, clientIp, sizeof(clientIp));
    unsigned short clientPort = ntohs(client_address.sin_port);
    printf("client ip is %s, port is %d\n", clientIp, clientPort);
}

// 信号处理函数
void sig_handler(int sig) {
    int save_errno = errno;
    int msg        = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

// 信号捕捉
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}
