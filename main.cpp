#include "connection.h"

// 最大的文件描述符个数
const int MAX_FD = 65535;

// epoll实例最大监听数量
const int MAX_EVENT_NUMBER = 10000;

// 定时时间
const int TIMESLOT = 5;

// 信号捕捉
void addsig(int sig, void (*handler)(int)) {
    // handler是一个函数指针
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

int main(int argc, char* argv[]) {
    // 判断传入参数
    if (argc <= 1) {
        printf("请按照如下格式运行：%s port\n", basename(argv[0]));
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对SIGPIE进行信号处理
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池并初始化
    threadpool<connection>* thread_pool = nullptr;
    try {
        thread_pool = new threadpool<connection>;
    } catch (...) {
        exit(-1);
    }

    // 创建一个数组用于保存所有客户端信息
    connection* users_httpconn = new connection[MAX_FD];

    // 网络编程逻辑

    // 创建监听套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("socket");
        exit(-1);
    }

    // 设置端口复用, 绑定前设置
    reuse_addr(listenfd);

    // 绑定
    sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(port);

    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    if (ret == -1) {
        perror("bind");
        exit(-1);
    }

    // 监听
    ret = listen(listenfd, 5);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }

    // 创建一个epoll对象实例
    int epollfd = epoll_create(1);
    // 事件数组
    epoll_event events[MAX_EVENT_NUMBER];

    connection::m_epollfd = epollfd;

    // 将监听文件描述符信息添加到epoll实例
    add_fd_to_epoll(connection::m_epollfd, listenfd, false);

    while (true) {
        // 返回检测到几个事件
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);  // -1是阻塞

        if (num < 0 && errno != EINTR) {
            printf("epoll failed...\n");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                // 新客户端连接
                sockaddr_in client_address;
                socklen_t   client_addr_size = sizeof(client_address);
                // 接受新连接
                int cfd = accept(listenfd, (sockaddr*)&client_address, &client_addr_size);

                if (cfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }

                // if (cfd == -1) {
                //     perror("accept");
                //     exit(-1);
                // }

                if (connection::m_user_count >= MAX_FD) {
                    // 目前最大连接数满，可以给客户端发送服务器内部正忙
                    close(cfd);
                    continue;
                }

                // 将新连接文件描述符加入epoll实例
                // 用文件描述符来充当索引
                users_httpconn[cfd].init_conn(cfd, client_address);

            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开或错误等事件
                users_httpconn[sockfd].close_conn();

            } else if (events[i].events & EPOLLIN) {
                // 一次性读出所有数据
                if (users_httpconn[sockfd].read()) {
                    thread_pool->append(users_httpconn + sockfd);
                } else {
                    users_httpconn[sockfd].close_conn();
                }

            } else if (events[i].events & EPOLLOUT) {
                // 写数据，并判断是否成功
                if (!users_httpconn[sockfd].write()) {
                    users_httpconn[sockfd].close_conn();
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);

    delete[] users_httpconn;
    delete thread_pool;

    return 0;
}