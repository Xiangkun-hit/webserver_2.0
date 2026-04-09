#include "sql_connection_pool.h"
#include <mysql/mysql.h>

connection_pool::connection_pool(){
    m_CurConn = 0;
    m_FreeConn = 0;
}

// 获取单例
connection_pool* connection_pool::GetInstance(){
    static connection_pool connPool;
    return &connPool;
}

// 初始化连接池 主机地址、用户名、密码、数据库名、端口、最大连接数、日志开关
void connection_pool::init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int Port, int MaxConn, int close_log)
{
    //初始化数据库信息
    m_url = url;       
    m_Port = Port;        
    m_User = User;     
    m_PassWord = PassWord;     
    m_DatabaseName = DataBaseName;

    //创建MaxConn条数据库连接
    for(int i = 0; i < MaxConn; ++i){
        MYSQL* con = nullptr;
        con = mysql_init(con);

        if(!con){
            LOG_ERROR("MySQL Error: mysql_init failed");
            exit(1);
        }

        // 连接数据库
        con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, nullptr, 0);
        
        if(!con){
            LOG_ERROR("MySQL Error: mysql_real_connect failed");
			exit(1);
        }

        // 加入连接池
        connList.push_back(con);
        m_FreeConn++;
    }

    // 信号量初始值 = 空闲连接数
    reserve = sem(m_FreeConn);

    m_MaxConn = m_FreeConn;
}



// 获取连接
//当有请求时，从数据库连接池中返回一个可用连接，更新使用和空闲连接数
MYSQL* connection_pool::GetConnection()
{
    MYSQL* con = nullptr;

    if(connList.empty())    return nullptr;

    // 信号量等待
    // 取出连接，信号量原子减1，为0则等待
    reserve.wait();

    lock.lock();

    con = connList.front();
    connList.pop_front();

    m_FreeConn--;
    m_CurConn++;

    lock.unlock();
    return con;   
}

// 释放连接
bool connection_pool::ReleaseConnection(MYSQL* conn)
{
    if(!conn) return false;

    lock.lock();

    connList.push_back(conn);
    m_FreeConn++;
    m_CurConn--;

    lock.unlock();

    // 信号量post 释放连接原子加1
    reserve.post();
    return true;
}

// 销毁所有连接
// 遍历连接池链表，关闭对应数据库连接;清空链表并重置空闲连接和现有连接数量。
void connection_pool::DestroyPool()
{
    lock.lock();

    if(!connList.empty()){
        for(MYSQL* conn : connList){
            mysql_close(conn);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

// 获取空闲连接数
int connection_pool::GetFreeConn(){
    return m_FreeConn;
}

// 析构：销毁所有连接
connection_pool::~connection_pool(){
    DestroyPool();
}


// 在获取连接时，通过有参构造对传入的参数进行修改
connectionRAII::connectionRAII(MYSQL** SQL, connection_pool* connPool)
{
    *SQL = connPool->GetConnection();

    conRAII = *SQL;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII()
{
    poolRAII->ReleaseConnection(conRAII);
}