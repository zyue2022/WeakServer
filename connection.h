#ifndef CONNECTION_H
#define CONNECTION_H

#include "epfd.h"
#include "state.h"

class client_timer;
class client_timer_list;

class connection {
public:
    static int user_count;  // 统计目前用户数量
    static int epollfd;     // 所有socket上的事件都被注册到同一个epoll对象中

    static client_timer_list* timer_list;  // 每个HTTP连接的定时器的列表

    sockaddr_in   client_address;  // 客户端地址
    int           sockfd;          // socket文件描述符
    client_timer* timer;           // 定时器

private:
    static const int READ_BUF_SIZE  = 2048;  // 读缓冲区大小
    static const int WRITE_BUF_SIZE = 2048;  // 写缓冲区大小
    static const int FILENAME_LEN   = 200;   // 文件名的最大长度

private:
    char        read_buf[READ_BUF_SIZE];  // 读缓冲区
    int         read_idx;                 // 在读缓冲区读取数据时的索引
    int         parse_idx;                // 当前正在解析的请求的字符在读缓冲区的位置
    int         parse_line;               // 当前正在解析的请求的所在行，即行的起始位置
    CHECK_STATE check_state;              // 主状态机当前所处的状态
    METHOD      method;                   // 请求方法
    char*       url;                      // 请求的目标文件的文件名
    char*       version;                  // HTTP协议版本号，我们仅支持HTTP1.1
    char*       host;                     // 主机名
    char        file_path[FILENAME_LEN];  // 请求的目标文件的完整路径，其内容等于 doc_root + url
    bool        is_keep_alive;            // 是否开启HTTP长连接
    int         content_len;              // HTTP请求的消息总长度

private:
    char         write_buf[WRITE_BUF_SIZE];  // 写缓冲区
    int          write_idx;                  // 写缓冲区中待发送的字节数
    size_t       bytes_to_send;              // 将要发送的数据的字节数
    size_t       bytes_had_send;             // 已经发送的字节数
    char*        file_address;               // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat  file_stat;                  // 目标文件的状态。
    struct iovec iv[2];                      // 采用writev来执行写操作
    int          iv_count;                   // iv_count表示被写内存块的数量

public:
    connection();
    ~connection();

    void init_conn();     // 初始化新客户端http连接
    void init_timer();    // 初始化定时器
    void update_timer();  // 更新定时器
    void close_sock();    // 断开连接
    void close_conn();    // 删除连接
    bool read();          // 非阻塞读数据，一次性读完
    bool write();         // 非阻塞写数据，一次性写完
    void process();       // 处理http请求，由线程池里面的线程调用

private:
    void      init_parse();          // 初始化http解析请求的状态
    HTTP_CODE parse_http();  // 解析http请求

    /* 下面这一组函数被parse_http_request调用来解析请求报文 */

    char*       get_one_line();                       // 获取一行报文
    LINE_STATUS parse_http_one_line();                // 解析http请求的某一行
    HTTP_CODE   parse_http_request(char* text);  // 解析HTTP请求方法、目标URL、版本号
    HTTP_CODE   parse_http_header(char* text);        // 解析http请求头部
    HTTP_CODE   parse_http_content(char* text);       // 解析http请求体
    HTTP_CODE   fetch_file();                         // 具体处理请求

private:
    bool reply_http(HTTP_CODE ret_code);  // 填充http响应

    /* 下面这一组函数被make_http_response调用以填充http响应 */

    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_content_type();
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};

#endif