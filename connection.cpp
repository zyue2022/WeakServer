#include "connection.h"

#include "clientlist.h"
#include "timer.h"

// 定义HTTP响应的一些状态信息
const char* ok_200_title    = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form  = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form  = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form  = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form  = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const char* doc_root = "/home/zyue/lesson/http_server/resources";

int connection::epollfd    = -1;
int connection::user_count = 0;

client_timer_list* connection::timer_list = nullptr;

connection::connection() : sockfd(-1), timer(nullptr) {}
connection::~connection() {}

void connection::init_conn() {
    init_timer();
    print_client_info(client_address);
    reuse_addr(sockfd);
    add_fd_to_epoll(epollfd, sockfd, true, true);
    init_parse();
    ++user_count;
    printf("after init, we have %d conn in all now...\n", user_count);
}

void connection::init_timer() {
    timer->renew_expire_time();
    timer_list->add_timer_to_list(timer);
}

void connection::update_timer() {
    timer->renew_expire_time();
    timer_list->adjust_timer_on_list(timer);
}

void connection::init_parse() {
    bzero(read_buf, READ_BUF_SIZE);
    bzero(write_buf, WRITE_BUF_SIZE);
    bzero(file_path, FILENAME_LEN);

    //struct stat  file_stat;  // 目标文件的状态。
    //struct iovec iv[2];      // 采用writev来执行写操作

    read_idx       = 0;
    parse_idx      = 0;
    parse_line     = 0;
    content_len    = 0;
    write_idx      = 0;
    bytes_to_send  = 0;
    bytes_had_send = 0;
    iv_count       = 0;

    url          = nullptr;
    version      = nullptr;
    host         = nullptr;
    file_address = nullptr;

    check_state   = CHECK_STATE_REQUESTLINE;
    method        = GET;
    is_keep_alive = false;
}

void connection::close_sock() {
    if (sockfd == -1) return;
    remove_fd_from_epoll(epollfd, sockfd);
    sockfd = -1;
    --user_count;
    printf("after close, there have %d conn in all...\n", user_count);
}

void connection::close_conn() {
    if (sockfd == -1) return;
    close_sock();
    timer_list->del_timer_from_list(timer);
}

bool connection::read() {
    if (read_idx >= READ_BUF_SIZE) {
        return false;
    }
    int bytes_of_read = 0;
    while (1) {
        bytes_of_read = recv(sockfd, read_buf + read_idx, READ_BUF_SIZE - read_idx, 0);
        if (bytes_of_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 读完全部数据
                break;
            }
            return false;
        } else if (bytes_of_read == 0) {
            // 对方关闭连接
            return false;
        }
        read_idx += bytes_of_read;
    }
    //printf("接收到数据：\n%s\n", read_buf);
    return true;
}

char* connection::get_one_line() { return read_buf + parse_line; }

// 解析一行，根据\r\n判断
LINE_STATUS connection::parse_http_one_line() {
    char temp;
    for (; parse_idx < read_idx; ++parse_idx) {
        temp = read_buf[parse_idx];
        if (temp == '\r') {
            if ((parse_idx + 1) == read_idx) {
                return LINE_OPEN;
            } else if (read_buf[parse_idx + 1] == '\n') {
                read_buf[parse_idx++] = '\0';
                read_buf[parse_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if ((parse_idx > 1) && (read_buf[parse_idx - 1] == '\r')) {
                read_buf[parse_idx - 1] = '\0';
                read_buf[parse_idx++]   = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CODE connection::parse_http_request_line(char* text) {
    // GET /index.html HTTP/1.1
    url = strpbrk(text, " \t");  // 判断第二个参数中的字符哪个在text中最先出现
    if (!url) {
        return BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *url++        = '\0';  // 置位空字符，字符串结束符
    char* _method = text;
    if (strcasecmp(_method, "GET") == 0) {  // 忽略大小写比较
        method = GET;
    } else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    // 检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。
    version = strpbrk(url, " \t");
    if (!version) {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    if (strcasecmp(version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    /**
     * http://192.168.110.129:10000/index.html
    */
    if (strncasecmp(url, "http://", 7) == 0) {
        url += 7;
        // 在参数 str 所指向的字符串中搜索第一次出现字符 c（一个无符号字符）的位置。
        url = strchr(url, '/');
    }
    if (!url || url[0] != '/') {
        return BAD_REQUEST;
    }
    check_state = CHECK_STATE_HEADER;  // 检查状态变成检查头
    return NO_REQUEST;
}

HTTP_CODE connection::parse_http_header(char* text) {
    // 遇到空行，表示头部字段解析完毕
    if (text[0] == '\0') {
        // 如果HTTP请求有消息体，则还需要读取content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if (content_len != 0) {
            check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            is_keep_alive = true;
        }
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn(text, " \t");
        content_len = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        // 处理Host头部字段
        text += 5;
        text += strspn(text, " \t");
        host = text;
    } else {
        //printf("unknow header %s\n", text);
    }
    return NO_REQUEST;
}

HTTP_CODE connection::parse_http_content(char* text) {
    if (read_idx >= (content_len + parse_idx)) {
        text[content_len] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

// 主状态机，解析请求
HTTP_CODE connection::parse_http_request() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE   ret_code    = NO_REQUEST;
    char*       text        = 0;

    while ((check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) ||
           (line_status = parse_http_one_line()) == LINE_OK) {
        // 解析到了请求体或一行完整的数据

        // 获取一行数据
        text       = get_one_line();
        parse_line = parse_idx;
        // printf("got one http line: \n%s\n\n", text);

        switch (check_state) {
            case CHECK_STATE_REQUESTLINE: {
                ret_code = parse_http_request_line(text);
                if (ret_code == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER: {
                ret_code = parse_http_header(text);
                if (ret_code == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ret_code == GET_REQUEST) {
                    return do_request();
                }
                break;
            }

            case CHECK_STATE_CONTENT: {
                ret_code = parse_http_content(text);
                if (ret_code == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if (ret_code == GET_REQUEST) {
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }

            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

/* 
    当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
    如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
    映射到内存地址file_address处，并告诉调用者获取文件成功
*/
HTTP_CODE connection::do_request() {
    // "/home/nowcoder/webserver/resources"
    strcpy(file_path, doc_root);
    int len = strlen(doc_root);
    strncpy(file_path + len, url, FILENAME_LEN - len - 1);
    // 获取real_file文件的相关的状态信息，-1失败，0成功
    if (stat(file_path, &file_stat) < 0) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if (!(file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if (S_ISDIR(file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open(file_path, O_RDONLY);
    // 创建内存映射
    file_address = (char*)mmap(0, file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

// 对内存映射区执行munmap操作
void connection::unmap() {
    if (file_address) {
        munmap(file_address, file_stat.st_size);
        file_address = 0;
    }
}

// 写HTTP响应
bool connection::write() {
    int temp = 0;

    if (bytes_to_send == 0) {
        // 将要发送的字节为0，这一次响应结束。
        modify_fd_from_epoll(epollfd, sockfd, EPOLLIN);
        init_parse();
        return true;
    }

    while (1) {
        // 分散写
        temp = writev(sockfd, iv, iv_count);
        if (temp <= -1) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if (errno == EAGAIN) {
                modify_fd_from_epoll(epollfd, sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_had_send += temp;
        bytes_to_send -= temp;

        if (bytes_had_send >= iv[0].iov_len) {
            iv[0].iov_len  = 0;
            iv[1].iov_base = file_address + (bytes_had_send - write_idx);
            iv[1].iov_len  = bytes_to_send;
        } else {
            iv[0].iov_base = write_buf + bytes_had_send;
            iv[0].iov_len  = iv[0].iov_len - temp;
        }

        if (bytes_to_send <= 0) {
            // 没有数据要发送了
            unmap();
            modify_fd_from_epoll(epollfd, sockfd, EPOLLIN);

            if (is_keep_alive) {
                init_parse();
                return true;
            } else {
                return false;
            }
        }
    }
}

// 往写缓冲中写入待发送的数据
bool connection::add_response(const char* format, ...) {
    if (write_idx >= WRITE_BUF_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(write_buf + write_idx, WRITE_BUF_SIZE - 1 - write_idx, format, arg_list);
    if (len >= (WRITE_BUF_SIZE - 1 - write_idx)) {
        return false;
    }
    write_idx += len;
    va_end(arg_list);
    return true;
}

bool connection::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool connection::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
    return true;
}

bool connection::add_content_length(int content_len) { return add_response("Content-Length: %d\r\n", content_len); }

bool connection::add_linger() {
    return add_response("Connection: %s\r\n", (is_keep_alive == true) ? "keep-alive" : "close");
}

bool connection::add_blank_line() { return add_response("%s", "\r\n"); }

bool connection::add_content(const char* content) { return add_response("%s", content); }

bool connection::add_content_type() { return add_response("Content-Type:%s\r\n", "text/html"); }

// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool connection::make_http_response(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if (!add_content(error_400_form)) {
                return false;
            }
            break;
        }
        case NO_RESOURCE: {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form)) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            add_headers(file_stat.st_size);
            iv[0].iov_base = write_buf;
            iv[0].iov_len  = write_idx;
            iv[1].iov_base = file_address;
            iv[1].iov_len  = file_stat.st_size;
            iv_count       = 2;

            bytes_to_send = write_idx + file_stat.st_size;

            return true;
        }
        default: {
            return false;
        }
    }

    iv[0].iov_base = write_buf;
    iv[0].iov_len  = write_idx;
    iv_count       = 1;
    bytes_to_send  = write_idx;
    return true;
}

void connection::process() {
    // 解析HTTP请求
    HTTP_CODE read_ret = parse_http_request();
    if (read_ret == NO_REQUEST) {
        modify_fd_from_epoll(epollfd, sockfd, EPOLLIN);
        return;
    }

    // 生成响应
    bool write_ret = make_http_response(read_ret);
    if (!write_ret) {
        close_conn();
    }
    modify_fd_from_epoll(epollfd, sockfd, EPOLLOUT);
}