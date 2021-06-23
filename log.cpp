//
// Created by zjd on 2021/5/28.
//

#include "log.h"

Log::Log()
{
    m_count = 0;
    std::cout << "log()" << std::endl;
}


Log::~Log()
{
    m_stop = true;
    std::cout << "~log()" << std::endl;
    m_output.close();
}

bool Log::init(std::string file_name)
{
    m_log_name = file_name;
    m_stop = false;
    m_dir_name = "/home/zjd/webserver/";
    m_open_file = m_dir_name + m_log_name;

    m_output.open(m_open_file.c_str(), std::ios::out | std::ios::app);

    if(!m_output.is_open())
    {
        std::cout << "open log file failed..." << std::endl;
        return false;
    }

    time_t cur_time;
    struct tm* sys_tm;
    time(&cur_time);
    sys_tm = localtime(&cur_time);
    char time_string [128];
    ctime_r(&cur_time, time_string);
    m_output << time_string;
    // m_output << sys_tm->tm_year << "." << sys_tm->tm_mon << "." << sys_tm->tm_mday << "." << sys_tm->tm_hour
    //          << "." << sys_tm->tm_min << "." << sys_tm->tm_sec << std::endl;

    pthread_t tid;
    pthread_create(&tid, NULL, flush_log_thread, NULL);
    return true;

}

void Log::async_write_log()
{
    while(!m_stop)
    {
        m_dequestat.wait();
        m_mutex.lock();
        // std::cout << "m_log_deque:" << m_log_deque.size() << std::endl;
        if(m_log_deque.size() == 0)
        {
            m_mutex.unlock();
            continue;
        }
        // std::string log = m_log_deque.front();
        // m_log_deque.pop_front();
        m_output << m_log_deque.front() << std::endl;
        m_log_deque.pop_front();
        m_mutex.unlock();
    }
}

void Log::append_log(Level log_level, std::string log_event)
{

    std::string output_info = "";
    switch(log_level)
    {
        case DEBUG:
        {
            output_info += "[DEBUG]:";
            break;
        }
        case INFO:
        {
            output_info += "[INFO]:";
            break;
        }
        case WARN:
        {
            output_info += "[WARN]:";
            break;
        }
        case ERROR:
        {
            output_info += "[ERROR]:";
            break;
        }
        default:
        {
            output_info += "[]";
            break;
        }
    };
    output_info += get_current_time();
    output_info += log_event;
    // output_info += "\n";
    // std::cout << "output_info : " << output_info << std::endl;
    push(output_info);

}

void Log::push(std::string log)
{
    m_mutex.lock();
    m_log_deque.push_back(log);
    ++m_count;
    m_mutex.unlock();
    m_dequestat.post();
}

std::string Log::get_current_time()
{
    time_t cur_time;
    // struct tm* sys_tm;
    time(&cur_time);
    // sys_tm = localtime(&cur_time);
    char time_string [128];
    ctime_r(&cur_time, time_string);
    time_string[24] = '\t';
    return time_string;
}