/**************************************************************************************************
 * 半同步/半反应堆线程池
 * 使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。

 * - 同步I/O模拟proactor模式
 * - 半同步/半反应堆
 * - 线程池
 **************************************************************************************************/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <pthread.h>
#include <exception>
#include <list>

#include "../lock/lock.h"
#include "mysql/sql_connection_pool.h"

// 模板线程池：适配任意任务类型（本项目为http_conn）
template<typename T>
class threadpool{
public:
    // 构造：初始化线程数、任务队列最大容量、数据库连接池
    //thread_number是线程池中线程的数量
    //max_requests是请求队列中最多允许的、等待处理的请求的数量
    //connPool是数据库连接池指针
    threadpool(connection_pool* connPool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();

    // 向任务队列添加任务(生产者)
    bool append(T* request, int state);

    // 重载添加任务（Proactor模式用）
    bool append(T* request);


private:
    // 线程工作函数（静态，pthread要求）
    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void* worker(void* arg);

    // 线程真正运行逻辑
    void run();

private:
    int m_thread_numer;       //线程池中的线程数
    pthread_t* m_threads;   //描述线程池的数组，其大小为m_thread_number
    int m_max_requests;     // 任务队列最大允许任务数
    std::list<T*> m_workqueue;      // 请求队列
    locker m_queuelocker;    //保护请求队列的互斥锁
    sem m_queuestat;         // 信号量：判断是否有任务待处理
    int m_actor_model;      // 事件处理模式（Reactor/Proactor）
    connection_pool* m_connPool;        // 数据库连接池
};


#endif