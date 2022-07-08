#include "connection.h"

const int MAX_FD           = 65535;  // 最大的文件描述符个数
const int MAX_EVENT_NUMBER = 10000;  // epoll实例最大监听数量

extern int TIMESLOT;  //定时器触发时间

extern int pipefd[2];  // 传输信号的管道，[0]读，[1]写

int main(int argc, char* argv[]) {
    // 判断传入参数
    if (argc <= 1) {
        printf("请按照如下格式运行：%s port\n", basename(argv[0]));
        exit(-1);
    }

    // 服务器本地地址信息
    sockaddr_in local_address;
    local_address.sin_family      = AF_INET;
    local_address.sin_addr.s_addr = INADDR_ANY;
    int local_port                = atoi(argv[1]);
    local_address.sin_port        = htons(local_port);

    // 创建监听套接字
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    // 设置端口复用, 绑定前设置
    reuse_addr(listenfd);

    // 绑定
    int ret = bind(listenfd, (sockaddr*)&local_address, sizeof(local_address));
    assert(ret != -1);

    // 监听
    ret = listen(listenfd, 5);
    assert(ret != -1);

    // 创建一个epoll对象实例
    int epollfd = epoll_create(1);
    assert(epollfd != -1);

    // 将监听文件描述符信息添加到epoll实例
    add_fd_to_epoll(epollfd, listenfd, false);

    // 创建epoll事件数组
    epoll_event events[MAX_EVENT_NUMBER];

    // 创建线程池并初始化
    threadpool<connection>* thread_pool = nullptr;
    try {
        thread_pool = new threadpool<connection>;
    } catch (...) {
        exit(-1);
    }

    // 创建一个数组用于保存所有http客户端信息
    connection* connections = new connection[MAX_FD];
    connection::epollfd     = epollfd;

    // 创建一个定时器链表用于保存所有http客户端连接是否超时的信息
    connection::timer_list = new client_timer_list();

    // 创建用于信号传输的管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    set_fd_nonblock(pipefd[1]);
    add_fd_to_epoll(epollfd, pipefd[0], false);

    // 设置信号处理函数
    addsig(SIGALRM);

    bool timeout = false;

    // 定时,5秒后产生SIGALARM信号
    alarm(TIMESLOT);

    while (1) {
        // 返回检测到几个事件
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);  // -1是阻塞

        if (num < 0 && errno != EINTR) {
            printf("epoll failed...\n");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; ++i) {
            int sockfd = events[i].data.fd;

            if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开或错误等事件
                connections[sockfd].close_conn();
            }

            if (sockfd == pipefd[0]) {
                // 检测到定时信号
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0) {
                    continue;
                }
                for (int i = 0; i < ret; ++i) {
                    // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                    // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。
                    timeout = true;
                    break;
                }
            }

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

                if (connection::user_count >= MAX_FD) {
                    // 目前最大连接数满，可以给客户端发送服务器内部正忙
                    close(cfd);
                    continue;
                }

                // 初始化，用文件描述符来充当索引
                connections[cfd].client.sockfd         = cfd;
                connections[cfd].client.client_address = client_address;
                // 创建定时器，绑定定时器与用户连接数据
                client_timer* timer = new client_timer();
                timer->http_conn = &connections[cfd];
                connections[cfd].client.timer = timer;
                connections[cfd].init_conn();

            } else if (events[i].events & EPOLLIN) {
                // 一次性读出所有数据
                if (connections[sockfd].read()) {
                    thread_pool->append(connections + sockfd);
                } else {
                    connections[sockfd].close_conn();
                }

            } else if (events[i].events & EPOLLOUT) {
                // 写数据，并判断是否成功
                if (!connections[sockfd].write()) {
                    connections[sockfd].close_conn();
                }
            }
        }
        /* 
            最后处理定时事件，因为I/O事件有更高的优先级。
            但这样做将导致定时任务不能精准的按照预定的时间执行。
        */
        if (timeout) {
            timer_handler(connection::timer_list);
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);

    delete[] connections;
    delete thread_pool;

    return 0;
}