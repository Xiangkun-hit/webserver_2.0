#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>

// ========== 静态成员变量初始化 ==========
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

locker m_lock;
std::map<std::string, std::string> users;

// 设置文件描述符非阻塞
int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
//-----------全局工具函数---------------
// 向epoll添加文件描述符
// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(TRIGMode == 1)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从epoll移除文件描述符
// 从内核时间表删除描述符
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改epoll文件描述符
// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
//---------------------------------------



//初始化套接字地址，函数内部会调用私有方法init
void http_conn::init(int sockfd, const sockaddr_in &addr, char* root, int TRIGMode,
                     int close_log, std::string user, std::string passwd, std::string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();
}

// 关闭http连接
// 关闭连接，关闭一个连接，客户总量减一
void http_conn::clost_conn(bool real_close){
    if(real_close && (m_sockfd != -1)){
        // 从epoll移除
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 处理客户请求
void http_conn::process(){

}

// 非阻塞读操作,读取浏览器端发来的全部数据
bool http_conn::read_once(){

}

// 非阻塞写操作,响应报文写入函数
bool http_conn::write(){

}



// 初始化数据库,同步线程初始化数据库读取表 （从连接池获取连接）
void http_conn::initmysql_result(connection_pool* connPool){
    // 从连接池获取一个MYSQL连接
    MYSQL* mysql = nullptr;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if(mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while(MYSQL_ROW row = mysql_fetch_row(result)){
        std::string temp1(row[0]);
        std::string temp2(row[1]);
        users[temp1] = temp2;
    }
}


// 初始化连接(私有)
// check_state默认为分析请求行状态
void http_conn::init(){
    m_timer = nullptr;          // 定时器指针置空
    mysql = nullptr;            // 数据库连接置空
    bytes_have_send = 0;
    bytes_to_send = 0;

    m_check_state = CHECK_STATE::CHECK_STATE_REQUESTLINE;       // 初始状态：解析请求行
    m_method = METHOD::GET;
    m_url = nullptr;
    m_version = nullptr;
    m_host = nullptr;
    m_content_length = 0;
    m_linger = false;           // 默认不保持连接

    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_line = 0;
    m_write_idx = 0;
    
    // 清空缓冲区
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// 解析http请求,从m_read_buf读取，并处理请求报文
HTTP_CODE http_conn::process_read(){

}      
// 填充http响应,向m_write_buf写入响应报文数据
bool http_conn::process_write(HTTP_CODE ret){
    
}     

//---------------------------------------------------
// 下面这组函数被process_read调用以分析http请求
HTTP_CODE http_conn::parse_request_line(char* text){               //主状态机解析报文中的 请求行 数据

}
HTTP_CODE http_conn::parse_headers(char* text){                    //主状态机解析报文中的 请求头 数据

}
HTTP_CODE http_conn::parse_content(char* text){                    //主状态机解析报文中的 请求体 数据

}
HTTP_CODE http_conn::do_request(){                                 //生成响应报文

}  

//从状态机读取一行，分析是请求报文的哪一部分
LINE_STATUS http_conn::parse_line(){

}

// ---------------------------------------------------
// 这组函数被process_write调用以填充http响应
// 根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
void http_conn::unmap(){

}
bool http_conn::add_response(const char* format, ...){

}
bool http_conn::add_content(const char* content){

}
bool http_conn::add_status_line(int status, const char* title){

}
bool http_conn::add_headers(int content_length){

}
bool http_conn::add_content_type(){

}
bool http_conn::add_content_length(int content_length){

}
bool http_conn::add_linger(){

}
bool http_conn::add_blank_line(){

}