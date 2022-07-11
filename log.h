#ifndef LOG_H
#define LOG_H

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <string>

#include "blockqueue.h"

class Log {
public:
    //C++11以后,使用局部变量懒汉不用加锁
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
        return nullptr;
    }

    //可选择的参数有日志文件、日志缓冲区大小、最大行数以及最长日志条队列
    bool init(const char *file_name, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char *format, ...);

    void flush(void);

private:
    Log();
    virtual ~Log();
    void async_write_log();

private:
    char dir_name[128];  //路径名
    char log_name[128];  //log文件名

    FILE *m_fp;  //打开log的文件指针

    bool      m_is_async;      //是否同步标志位
    int       m_split_lines;   //日志最大行数
    char     *m_buf;           //日志缓冲区
    int       m_log_buf_size;  //日志缓冲区大小
    long long m_count;         //日志行数记录
    int       m_today;         //因为按天分类,记录当前时间是那一天

    locker m_mutex;  //互斥锁

    blockqueue<std::string> *m_log_queue;  //阻塞队列
};

#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif
