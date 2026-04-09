/*****************************
 * 数据库连接池
    - 单例模式，保证唯一
    - list实现连接池
    - 连接池为静态大小
    - 互斥锁实现线程安全
 *******************************/


#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include <../lock/lock.h>
#include <../log/log.h>

class connection_pool{
public:
    // 获取单例
    static connection_pool* GetInstance();

    // 初始化连接池 主机地址、用户名、密码、数据库名、端口、最大连接数、日志开关
    void init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log);



    // 获取连接
    MYSQL* GetConnection();

    // 释放连接
    bool ReleaseConnection(MYSQL* conn);

    // 销毁所有连接
    void DestroyPool();

    // 获取空闲连接数
    int GetFreeConn();

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;      // 最大连接数
    int m_CurConn;      // 当前已使用的连接数
    int m_FreeConn;     // 当前空闲的连接数
    locker lock;        // 互斥锁
    std::list<MYSQL*> connList;  // 连接池链表
    sem reserve;        // 信号量：控制空闲连接



public:
    std::string m_url;      // 主机地址
    std::string m_Port;     // 数据库端口号
    std::string m_User;     // 登陆数据库用户名
    std::string m_PassWord; // 登陆数据库密码
    std::string m_DatabaseName;     // 使用数据库名
    int m_close_log;        // 日志开关
};

// RAII机制：自动获取/释放连接
class connectionRAII{

public:
    connectionRAII(MYSQL** con, connection_pool* connPool);
    ~connectionRAII();

private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};


#endif