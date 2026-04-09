/*****************************************************************************************************
 * http连接处理类
 * 根据状态转移,通过主从状态机封装了http连接类。其中,主状态机在内部调用从状态机,从状态机将处理状态和数据传给主状态机

 * 客户端发出http连接请求
 * 从状态机读取数据,更新自身状态和接收数据,传给主状态机
 * 主状态机根据从状态机状态,更新自身状态,决定响应请求还是继续读取
 ****************************************************************************************************/

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string>
#include "../lock/lock.h"
#include "../mysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

// 前置声明（解决互相依赖）
class util_timer;

// HTTP请求方法
enum class METHOD{
    GET = 0,
    POST
};

// 主状态机状态（解析HTTP请求用）
enum class CHECK_STATE{
    CHECK_STATE_REQUESTLINE = 0,    // 解析请求行
    CHECK_STATE_HEADER,             // 解析请求头
    CHECK_STATE_CONTENT             // 解析请求体
};

// 解析结果
enum class HTTP_CODE{
    NO_REQUEST = 0,   // 数据不完整，继续读
    GET_REQUEST,      // 获得完整请求
    BAD_REQUEST,      // 请求语法错误
    NO_RESOURCE,      // 无资源
    FORBIDDEN_RESOURCE,// 禁止访问
    FILE_REQUEST,     // 请求文件
    INTERNAL_ERROR,   // 服务器内部错误
    CLOSED_CONNECTION // 关闭连接
};

// 报文读取状态
enum LINE_STATUS {
    LINE_OK = 0,  // 读取到完整行
    LINE_BAD,     // 行出错
    LINE_OPEN     // 行数据不完整
};

class http_conn{
public:
    static const int FILENAME_LEN = 200;            //文件名最大长度
    static const int READ_BUFFER_SIZE = 2048;       //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 10240;     //写缓冲区大小

public:
    http_conn(){}
    ~http_conn(){}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char*, int, int, std::string user, std::string passwd, std::string sqlname);

    // 关闭http连接
    void clost_conn(bool real_close = true);

    // 处理客户请求
    void process();

    // 非阻塞读操作,读取浏览器端发来的全部数据
    bool read_once();

    // 非阻塞写操作,响应报文写入函数
    bool write();

    // 获取客户端地址
    sockaddr_in* get_address(){
        return &m_address;
    }

    // 初始化数据库,同步线程初始化数据库读取表
    void initmysql_result(connection_pool* connPool);

    // 绑定定时器
    void timer(util_timer* timer){
        m_timer = timer;
    }


public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL* mysql;   //数据库相关
    int m_state;    //读为0, 写为1


private:
    void init();        // 初始化连接

    HTTP_CODE process_read();       // 解析http请求,从m_read_buf读取，并处理请求报文

    bool process_write(HTTP_CODE ret);      // 填充http响应,向m_write_buf写入响应报文数据

    //---------------------------------------------------
    // 下面这组函数被process_read调用以分析http请求
    HTTP_CODE parse_request_line(char* text);               //主状态机解析报文中的 请求行 数据
    HTTP_CODE parse_headers(char* text);                    //主状态机解析报文中的 请求头 数据
    HTTP_CODE parse_content(char* text);                    //主状态机解析报文中的 请求体 数据
    HTTP_CODE do_request();                                 //生成响应报文

    //m_start_line是已经解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char* get_line(){return m_read_buf + m_start_line; }      
    
    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    // ---------------------------------------------------
    // 这组函数被process_write调用以填充http响应
    // 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    void unmap();
    bool add_response(const char* format, ...);
    bool add_content(const char* content);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    util_timer* m_timer;

private:
    int m_sockfd;                   // 该http连接的socket
    sockaddr_in m_address;          // 客户端的socket地址

    char m_read_buf[READ_BUFFER_SIZE];      //存储读取的请求报文数据
    int m_read_idx;                 //缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int m_checked_idx;              //m_read_buf读取的位置m_checked_idx(当前正在分析的字符在读缓冲区的位置)
    int m_start_line;               //m_read_buf中已经解析的字符个数(当前正在解析的行的起始位置)
    
    char m_write_buf[WRITE_BUFFER_SIZE];    //存储发出的响应报文数据
    int m_write_idx;                // 写缓冲区待发送的字节数

    CHECK_STATE m_check_state;           // 主状态机当前所处状态
    METHOD m_method;                    // 请求方法

    //以下为解析请求报文中对应的6个变量--存储读取文件的名称
    char m_real_file[FILENAME_LEN];     // 客户请求的目标文件的文件名
    char* m_url;                        // 客户请求目标文件的路径
    char* m_version;                    // http协议版本号
    char* m_host;                       // 主机名
    int m_content_length;               // http请求的消息体长度
    bool m_linger;                      // http请求是否保持连接

    char* m_file_address;               // 客户请求的目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;            // 目标文件的状态，通过它可以判断文件是否存在/是否为目录/是否可读，并获取文件大小
    struct iovec m_iv[2];               // io向量机制iovec;采用writev执行写操作，iovec结构体指针
    int m_iv_count;                     // 被写的内存块数量

    int cgi;            // 是否启用的cgi
    char* m_string;                     // 存储请求头数据
    int bytes_to_send;                  // 剩余发送字节数
    int bytes_have_send;                // 已发送字节数
    char* doc_root;
    int improv;
    int timer_flag;

    std::map<std::string, std::string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif