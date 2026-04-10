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
#include "../mysql/sql_connection_pool.h"

// 模板线程池：适配任意任务类型（本项目为http_conn）
template<typename T>
class threadpool{
public:
    // 构造：初始化线程数、任务队列最大容量、数据库连接池
    //thread_number是线程池中线程的数量
    //max_requests是请求队列中最多允许的、等待处理的请求的数量
    //connPool是数据库连接池指针
    threadpool(int actor_model, connection_pool* connPool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();

    // 向任务队列添加任务(生产者)
    bool append(T* request, int state);

    // 重载添加任务（Proactor模式用）
    bool append(T* request);


private:
    // 线程工作函数（静态，pthread要求）(消费者)
    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void* worker(void* arg);

    // 线程真正运行逻辑
    void run();

private:
    int m_thread_number;       //线程池中的线程数
    pthread_t* m_threads;   //描述线程池的数组，其大小为m_thread_number
    int m_max_requests;     // 任务队列最大允许任务数
    std::list<T*> m_workqueue;      // 请求队列
    locker m_queuelocker;    //保护请求队列的互斥锁
    sem m_queuestat;         // 信号量：判断是否有任务待处理
    int m_actor_model;      // 事件处理模式（Reactor/Proactor）
    connection_pool* m_connPool;        // 数据库连接池
    bool m_stop;            // 是否结束线程
};

/*********************************************************
*   线程池创建与回收

* - 构造函数中创建线程池,pthread_create函数中将类的对象作为参数传递给静态函数(worker),在静态函数中引用这个对象,并调用其动态方法(run)。

* - 具体的，类对象传递时用this指针，传递给静态函数后，将其转换为线程池类，并调用私有成员函数run。
********************************************************/
// 构造函数实现
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool* connPool, int thread_number, int max_requests)
         : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests), m_threads(nullptr),m_connPool(connPool)
{
    // 线程数和连接池合法性检查
    if(thread_number <=0 || max_requests <= 0) throw std::exception();

    // 动态创建线程数组 线程id初始化
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) throw std::exception();

    // 创建线程，并设置为线程分离（自动释放资源，无需pthread_join）
    for(int i = 0; i < m_thread_number; ++i){
        if(pthread_create(m_threads + i, nullptr, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i] != 0)){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

// 析构函数
template<typename T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
    m_stop = true;
}


// 向任务队列添加任务(生产者)
// 添加任务（Reactor模式，带状态）
template<typename T>
bool threadpool<T>::append(T* request, int state){
    m_queuelocker.lock();

    // 任务队列已满
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    // request->m_state = state;
    // 添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 信号量V操作，唤醒工作线程
    m_queuestat.post();
    return true;
}

// 重载添加任务（Proactor模式用）
template<typename T>
bool threadpool<T>::append(T* request)
{
    m_queuelocker.lock();

    // 根据硬件，预先设置请求队列的最大值
    // 任务队列已满
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }

    // 添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();

    // 信号量V操作，唤醒工作线程
    m_queuestat.post();
    return true;
}
    
// 线程工作函数（静态，pthread要求）
// 工作线程运行的函数，它不断从工作队列中取出任务并执行之
// 内部访问私有成员函数run，完成线程处理要求。
template<typename T>
void* threadpool<T>::worker(void* arg){
    //将参数强转为线程池类，调用成员方法
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

// 线程真正运行逻辑
// 主要实现，工作线程从请求队列中取出某个任务进行处理，注意线程同步。
template<typename T>
void threadpool<T>::run(){


    // 循环运行，直到m_stop为true
    while(!m_stop){
        // 信号量P操作：无任务则阻塞等待
        m_queuestat.wait();

        //被唤醒后先加互斥锁
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }

        //从请求队列中取出第一个任务
        //将任务从请求队列删除
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request) continue;

        // 执行任务（处理HTTP请求）
        if(1 == m_actor_model){     // Proactor模式
            //Proactor模式
            if(0 == request->m_state){      // 读请求
                if(request->read_once()){
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }else{
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }else{                          // 写请求
                request->improv = 1;
                request->timer_flag = 1;
            }
        }
        else{       // Reactor模式
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }

}

    

#endif