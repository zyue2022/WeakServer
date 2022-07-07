#ifndef EPFD_H
#define EPFD_H

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "cond.h"
#include "locker.h"
#include "sem.h"
#include "threadpool.h"

int  set_fd_nonblock(int fd);                              // 设置文件描述符非阻塞
void add_fd_to_epoll(int epollfd, int fd, bool one_shot);  // 添加文件描述符到epoll对象实例中
void remove_fd_from_epoll(int epollfd, int fd);            // 从epoll对象中删除文件描述符
void modify_fd_from_epoll(int epollfd, int fd, int ev);    // 从epoll对象中删除文件描述符
void reuse_addr(int sockfd);                               // 设置端口复用
void print_client_info(sockaddr_in client_address);        // 打印新连接的客户端信息

#endif