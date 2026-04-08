/*************************************************************
*循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size;  
*线程安全，每个操作前都要先加互斥锁，操作完后，再解锁
**************************************************************/

#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "lock/lock.h"

template<class T>
class block_queue{
public:
    block_queue(int max_size = 1000){       // 构造函数：初始化队列大小
        if(max_size <= 0){
            exit(-1);
        }

        m_max_size = max_size;
        m_array = new T[max_size];

        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    void clear(){       // 清空队列
        m_mutex.lock();

        m_size = 0;
        m_front = -1;
        m_back = -1;

        m_mutex.unlock();
    }

    bool full(){        // 判断队列是否满
        m_mutex.lock();

        if(m_size >= m_max_size){

            m_mutex.unlock();

            return true;
        }

        m_mutex.unlock();

        return false;
    }

    bool empty(){       // 判断队列是否空
        m_mutex.lock();

        if(0 == m_size){
            m_mutex.unlock();

            return true;
        }

        m_mutex.unlock();

        return false;
    }

    bool front(T &val){    // 获取队首元素
        m_mutex.lock();

        if(0 == m_size){
            m_mutex.unlock();

            return false;
        }

        val = m_array[m_front];

        m_mutex.unlock();

        return true;
    }

    bool back(T &val){      // 获取队尾元素
        m_mutex.lock();

        if(0 == m_size){
            m_mutex.unlock();

            return false;
        }

        val = m_array[m_back];

        m_mutex.unlock();

        return true;
    }

    int size(){     // 获取队列大小
        int tmp = 0;
        m_mutex.lock();

        tmp = m_size;

        m_mutex.unlock();
        return tmp;
    }

    int max_size(){     // 获取最大容量
        int tmp = 0;
        m_mutex.lock();

        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    //---------------------------核心功能---------------------------//
    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列，相当于生产者生产了一个元素
    //若当前没有线程等待条件变量,则唤醒无意义
    bool push(const T &item){           // 向队列添加元素（生产者）
        m_mutex.lock();

        // 如果队列满，唤醒消费者，返回false
        if(m_size >= m_max_size){    
            m_cond.broadcast();

            m_mutex.unlock();

            return false;
        }

        // 循环数组：更新队尾
        m_back = (m_back + 1) % m_max_size;
        //将新增数据放在循环数组的对应位置
        m_array[m_back] = item;
        m_size++;

        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item){                  // 从队列取出元素（消费者，阻塞）
        m_mutex.lock();

        //pop时，如果当前队列没有元素,将会等待条件变量
        while(m_size <= 0){
            if(!m_cond.wait(m_mutex.get())){
                m_mutex.unlock();
                return false;
            }
        }

        //取出队列首的元素，这里需要理解一下，使用循环数组模拟的队列
        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;

        m_mutex.unlock();
        return true;
    }

    //增加了超时处理，在项目中没有使用到
    //在pthread_cond_wait基础上增加了等待的时间，只指定时间内能抢到互斥锁即可
    //其他逻辑不变
    bool pop(T &item, int ms_timeout){  // 超时取出元素
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, nullptr);

        m_mutex.lock();

        if(m_size <= 0){
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t)){
                m_mutex.unlock();
                return false;
            }
        }

        if(m_size <= 0){
            m_mutex.unlock();

            return false;
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }










    ~block_queue(){     //析构函数：释放内存
        m_mutex.lock();

        if(m_array != nullptr){
            delete []m_array;
        }

        m_mutex.unlock();
    }

private:
    locker m_mutex;     // 互斥锁：保证队列线程安全
    cond m_cond;        // 条件变量：阻塞等待/唤醒

    T* m_array;         // 循环数组
    int m_size;         // 当前元素个数
    int m_max_size;     // 最大容量
    int m_front;        // 队首指针
    int m_back;         // 队尾指针
};

#endif