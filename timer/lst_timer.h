/***********************
 * 定时器处理非活动连接
    由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现一个服务器定时器，处理这种非活跃连接，释放连接资源。利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务.

    - 统一事件源
    - 基于升序链表的定时器
    - 处理非活动连接
 *************/

#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../log/log.h"

// 前置声明
class util_timer;

// 客户端数据结构体：绑定socket、地址、定时器
// 连接资源
struct client_data{
    sockaddr_in address;        //客户端socket地址
    int sockfd;                 //socket文件描述符
    util_timer* timer;          //定时器
};

//定时器节点类
class util_timer{
public:
    util_timer() : prev(nullptr), next(nullptr){}
public:
    time_t expire;           // 超时时间（时间戳）
    void* cb_func(client_data*);      // 超时回调函数（关闭连接）
    client_data* user_data;             // 指向客户端数据

    util_timer* prev;                   // 前驱节点
    util_timer* next;                   // 后继节点
};

// 升序双向链表定时器容器
class sort_timer_lst{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    //添加定时器，内部调用私有成员add_timer
    void add_timer(util_timer* timer);

    // 调整定时器（超时时间更新）
    void adjust_timer(util_timer* timer);

    // 删除定时器
    void del_timer(util_timer* timer);

    // 定时处理：检查并处理超时定时器
    void tick();

private:
    //私有成员，被公有成员add_timer和adjust_time调用
    //主要用于调整链表内部结点
    void add_timer(util_timer* timer, util_timer* lst_head);

private:
    util_timer* head;   // 链表头
    util_timer* tail;   // 链表尾
};

// 定时器回调函数：超时关闭客户端连接
void cb_func(client_data* user_data);

class Utils{
public:
    Utils();
    ~Utils();

    // 初始化时间槽
    void init(int timeslot);

    // 设置文件描述符非阻塞
    int setnonblocking(int fd);

    // 向epoll添加文件描述符
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务（触发tick）
    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    // 发送错误信息并关闭连接
    void show_error(int connfd, const char* info);

public:
    static int* u_pipefd;           // 管道fd：信号传递
    sort_timer_lst m_timer_list;    // 定时器链表
    static int u_epollfd;           // epoll文件描述符
    int m_TIMESLOT;                 // 定时时间槽
};

#endif