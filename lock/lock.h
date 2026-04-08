#ifndef LOCK_H
#define LOCK_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

//1.信号量类
class sem{
public:
    sem(){ //构造函数 初始化信号量 初始值0
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();     // 初始化失败抛出异常，保证安全
        }
    }

    sem(int num){   //重载，指定初始值
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }

    ~sem(){     //析构函数，销毁信号量
        sem_destroy(&m_sem);
    }

    bool wait(){    // P操作：等待信号量（减1）
        return sem_wait(&m_sem) == 0;
    }

    bool post(){    // V操作：发送信号量（加1）
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};


//2.互斥锁类
class locker{
public:
    locker(){       // 构造：初始化互斥锁
        if(pthread_mutex_init(&m_mutex, nullptr) != 0){
            throw std::exception();
        }
    }

    ~locker(){      // 析构：销毁互斥锁
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock(){    //加锁
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock(){  //解锁
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* get(){     // 获取锁指针（给条件变量使用）
        return &m_mutex;
    }


private:
    pthread_mutex_t m_mutex;

};


//3.条件变量类
class cond{
public:
    cond(){     // 构造：初始化条件变量
        if(pthread_cond_init(&m_cond, nullptr) != 0){
            throw std::exception();
        }
    }

    ~cond(){    // 析构：销毁条件变量
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t* m_mutex){        // 等待条件变量
        return pthread_cond_wait(&m_cond, m_mutex) == 0;
    }

    bool timewait(pthread_mutex_t* m_mutex, struct timespec t){        // 超时等待
        return pthread_cond_timedwait(&m_cond, m_mutex, &t) == 0;
    }

    bool signal(){      // 唤醒一个线程
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool broadcast(){   // 唤醒所有线程
        return pthread_cond_broadcast(&m_cond) == 0;
    }


private:
    pthread_cond_t m_cond;
};

#endif