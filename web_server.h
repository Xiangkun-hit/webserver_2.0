#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http_conn/http_conn.h"

const int MAX_FD = 65536;       //最大文件描述符
const int MAX_EVENT_NUMBER = 10000;     //最大事件数
const int TIMESLOT = 5;         //最小超时单位

class WebServer{
public:
    WebServer();
    ~WebServer();

    // 初始化服务器
    void init(int port, std::string user, std::string passWord, std::string databaseName,
            int log_write, int opt_linger, int trigmode, int sql_num,
            int thread_num, int close_log, int actor_model);
    
    // 线程池初始化
    void thread_pool();

    // 数据库连接池初始化
    void sql_pool();

    // 日志初始化
    void log_write();

    // 触发模式设置
    void trig_mode();

    // 监听socket初始化
    void event_listen();

    // 事件循环
    void event_loop();

    //定时器
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);
    void deal_timer(util_timer *timer, int sockfd);


    // 处理新客户端连接
    bool dealclientdata();

    // 处理信号
    bool dealwithsignal(bool& timeout, bool& stop_server);

    // 处理读事件
    void dealwithread(int sockfd);

    // 处理写事件
    void dealwithwrite(int sockfd);

public:
    // 基础网络参数
    int m_port;             // 端口
    char* m_root;           // 网页根目录
    int m_log_write;        // 日志写入方式
    int m_close_log;        // 日志开关
    int m_actormodel;       // 事件模式

    // socket相关
    int m_listenfd;         // 监听fd
    int m_epollfd;          // epollfd
    int m_opt_linger;       // 优雅关闭

    // 触发模式
    int m_TRIGMode;         // 整体模式
    int m_LISTENTrigmode;   // 监听模式
    int m_CONNTrigmode;     // 连接模式

    // 数据库相关
    connection_pool* m_connPool;
    std::string m_user;     //登陆数据库用户名
    std::string m_passWord; //登陆数据库密码
    std::string m_databaseName; //使用数据库名
    int m_sql_num;

    // 线程池相关
    threadpool<http_conn>* m_pool;
    int m_thread_num;

    // 客户端数组
    http_conn* users;

    // 管道：信号通知
    int m_pipefd[2];

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    // 定时器相关
    sort_timer_lst m_timer_lst;
    client_data* users_timer;

    Utils utils;
};

#endif