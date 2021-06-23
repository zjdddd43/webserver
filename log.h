#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <deque>
#include "locker.h"
#include <fstream>
#include <ctime>
#include <pthread.h>
#include <string>

enum Level
{
    DEBUG = 0,
    INFO,
    WARN,
    ERROR
};

class Log
{
public:

    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    static std::string get_current_time();

    static void* flush_log_thread(void *arg)
    {
        Log::get_instance()->async_write_log();
    }

    bool init(std::string file_name);
    void append_log(Level log_level, std::string log_event);

private:
    Log();
    ~Log();

    void async_write_log();
    void push(std::string log);

private:
    // 目录名
    std::string m_dir_name;
    // log文件名
    std::string m_log_name;

    std::string m_open_file;
    
    // 日志行数
    long long m_count;
    // 最大阻塞队列 max deque size
    int m_mds;
    bool m_stop;

    std::deque<std::string> m_log_deque;
    // deque的互斥锁
    locker m_mutex;
    // 是否有任务要处理
    sem m_dequestat;
    //
    Level m_level;
    std::ofstream m_output;
};

#define LOG_DEBUG(log_info) Log::get_instance()->append_log(DEBUG, log_info)
#define LOG_INFO(log_info) Log::get_instance()->append_log(INFO, log_info)
#define LOG_WARN(log_info) Log::get_instance()->append_log(WARN, log_info)
#define LOG_ERROR(log_info) Log::get_instance()->append_log(ERROR, log_info)

#endif