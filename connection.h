#ifndef CONNECTION_H
#define CONNECTION_H

#include "epfd.h"
#include "httpstate.h"

class connection {
public:
    static int m_user_count;  // 统计目前用户数量
    static int m_epollfd;     // 所有socket上的事件都被注册到同一个epoll对象中

private:
    static const int READ_BUF_SIZE  = 2048;  // 读缓冲区大小
    static const int WRITE_BUF_SIZE = 2048;  // 写缓冲区大小
    static const int FILENAME_LEN   = 200;   // 文件名的最大长度

    int         m_sockfd;          // 该HTTP连接的socket文件描述符
    sockaddr_in m_client_address;  // 该HTTP连接的地址信息

private:
    char        m_read_buf[READ_BUF_SIZE];  // 读缓冲区
    int         m_read_idx;                 // 在读缓冲区读取数据时的索引
    int         m_parse_idx;                // 当前正在解析的请求的字符在读缓冲区的位置
    int         m_parse_line;               // 当前正在解析的请求的所在行，即行的起始位置
    CHECK_STATE m_check_state;              // 主状态机当前所处的状态
    METHOD      m_method;                   // 请求方法
    char*       m_url;                      // 请求的目标文件的文件名
    char*       m_version;                  // HTTP协议版本号，我们仅支持HTTP1.1
    char*       m_host;                     // 主机名
    char        m_file_path[FILENAME_LEN];  // 请求的目标文件的完整路径，其内容等于 doc_root + m_url
    bool        is_keep_alive;              // 是否开启HTTP长连接
    int         m_content_len;              // HTTP请求的消息总长度

private:
    char         m_write_buf[WRITE_BUF_SIZE];  // 写缓冲区
    int          m_write_idx;                  // 写缓冲区中待发送的字节数
    int          bytes_to_send;                // 将要发送的数据的字节数
    int          bytes_had_send;               // 已经发送的字节数
    char*        m_file_address;               // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat  m_file_stat;                  // 目标文件的状态。
    struct iovec m_iv[2];                      // 采用writev来执行写操作
    int          m_iv_count;                   // m_iv_count表示被写内存块的数量

public:
    connection() {}
    ~connection() {}

    void init_conn(int, sockaddr_in&);  // 初始化新客户端http连接
    void close_conn();                  // 关闭连接
    bool read();                        // 非阻塞读数据，一次性读完
    bool write();                       // 非阻塞写数据，一次性写完
    void process();                     // 处理http请求，由线程池里面的线程调用

private:
    void      init_parse();          // 初始化http解析请求的状态
    HTTP_CODE parse_http_request();  // 解析http请求

    /* 下面这一组函数被parse_http_request调用来解析请求报文 */

    char*       get_one_line();                       // 获取一行报文
    LINE_STATUS parse_http_one_line();                // 解析http请求的某一行
    HTTP_CODE   parse_http_request_line(char* text);  // 解析HTTP请求方法、目标URL、版本号
    HTTP_CODE   parse_http_header(char* text);        // 解析http请求头部
    HTTP_CODE   parse_http_content(char* text);       // 解析http请求体
    HTTP_CODE   do_request();                         // 具体处理请求

private:
    bool make_http_response(HTTP_CODE ret_code);  // 填充http响应

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