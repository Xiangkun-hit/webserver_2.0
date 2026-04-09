#include "log.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

Log::Log(){
    m_count = 0;
    m_is_async = false;
}

Log::~Log(){
    if(m_fp != nullptr){
        fclose(m_fp);
    }
}

// 初始化日志：路径、文件名、缓冲区大小、最大行数、队列大小
bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size){
    
    //如果设置了max_queue_size,则设置为异步
    if(max_queue_size >= 1){
        m_is_async = true;      //设置写入方式flag

        m_log_queue = new block_queue<std::string>(max_queue_size);     //创建并设置阻塞队列长度
        pthread_t tid;

        //flush_log_thread为回调函数,这里表示创建线程异步写日志
        pthread_create(&tid, nullptr, flush_log_thread, nullptr);
    }

    //输出内容的长度
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', sizeof(m_buf));

    //日志的最大行数
    m_split_lines = split_lines;

    // 获取当前时间
    time_t t = time(nullptr);
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    m_today = my_tm.tm_mday;

    // 解析文件名,从后往前找到第一个/的位置
    const char* p = strrchr(file_name, '/');
    char log_full_name[512]= {0};

    //相当于自定义日志名
    //若输入的文件名没有/，则直接将时间+文件名作为日志名
    if(p == NULL){
        snprintf(log_full_name, 511, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }else{
        //将/的位置向后移动一个位置，然后复制到logname中
        //p - file_name + 1是文件所在路径文件夹的长度
        //dirname相当于./
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 511, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }

    // 打开日志文件
    m_fp = fopen(log_full_name, "a");
    if(m_fp == nullptr){
        return false;
    } 
    return true;
}

// 写日志（外部调用接口）
void Log::write_log(int level, const char* format, ...){
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};

    // 日志分级
    switch(level){
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[error]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
    }

    //写入一个log，对m_count++, m_split_lines最大行数
    m_mutex.lock();
    m_count++;

    // 按天/按行数分割日志
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0){
        char new_log[512] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        // 情况1：跨天分割（日期变了）
        //如果是时间不是今天,则创建今天的日志，更新m_today和m_count
        if(m_today != my_tm.tm_mday){
            // 拼接新文件名：目录 + 日期后缀 + 基础日志名
            // 例：./log/2025_05_26_server.log
            snprintf(new_log, 511, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        // 情况2：按行数分割（文件写满了，日期没变）
        else{
            //超过了最大行，在之前的日志名基础上加后缀, m_count/m_split_lines
            // 拼接文件名：目录 + 日期 + 日志名.序号
            // 例：./log/2025_05_26_server.log.1  .2  .3
            snprintf(new_log, 511, "%s%s%s.%lld", dir_name, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    // 可变参数解析
    va_list valst;
    va_start(valst, format);

    std::string log_str;
    m_mutex.lock();

    //写入内容格式：时间 + 内容
    //时间格式化，snprintf成功返回写字符的总数，其中不包括结尾的null字符
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    // 写入日志内容
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);

    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;
    m_mutex.unlock();

    //若m_is_async为true表示异步，默认为同步
    //若异步,则将日志信息加入阻塞队列,同步则加锁向文件中写
    if(m_is_async && !m_log_queue->full()){
        m_log_queue->push(log_str);
    }else{
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);        
}

// 刷新缓冲区
void Log::flush(void){
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}