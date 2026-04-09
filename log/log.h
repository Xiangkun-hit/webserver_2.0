#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "../blockqueue.h"

class Log{
public:
    // C++11以后,使用局部变量懒汉不用加锁
    // 公有静态方法：获取单例对象（全局唯一）
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }

    // 异步日志线程函数：从阻塞队列取日志，写入文件
    static void* flush_log_thread(void* args){
        Log::get_instance()->async_write_log();
        return nullptr;
    }

    // 初始化日志：路径、文件名、缓冲区大小、最大行数、队列大小
    bool init(const char* file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    // 写日志（外部调用接口）
    void write_log(int level, const char* format, ...);

    // 刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();

    // 异步写日志（核心：从阻塞队列取数据）
    void* async_write_log(){
        std::string single_log;
        while(m_log_queue->pop(single_log)){
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
        return nullptr;
    }

private:
    char dir_name[128];     //路径名
    char log_name[128];     //log文件名
    int m_split_lines;      //日志最大行数
    int m_log_buf_size;     //日志缓冲区大小
    long long m_count;      //日志行数记录
    int m_today;            //因为按天分类,记录当前时间是那一天
    FILE *m_fp;             //打开log的文件指针
    char* m_buf;            //日志缓冲区
    block_queue<std::string>* m_log_queue;   //阻塞队列（异步日志核心）
    locker m_mutex;         //互斥锁（线程安全）
    bool m_is_async;        //是否异步标志位

public:
    int m_close_log;       //关闭日志
};

// 宏定义：外部调用写日志,主要用于不同类型的日志输出
#define LOG_DEBUG(format, ...) if(0 == Log::get_instance()->m_close_log){\
    Log::get_instance()->write_log(0, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == Log::get_instance()->m_close_log){\
    Log::get_instance()->write_log(1, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == Log::get_instance()->m_close_log){\
    Log::get_instance()->write_log(2, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == Log::get_instance()->m_close_log){\
    Log::get_instance()->write_log(3, format, ##__VA_ARGS__);\
    Log::get_instance()->flush();}

#endif