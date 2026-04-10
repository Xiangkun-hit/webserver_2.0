#include "http_conn.h"
#include <mysql/mysql.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

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
// read_once循环读取client数据，直到无数据可读或对方关闭连接，读取到m_read_buffer中，并更新m_read_idx。
bool http_conn::read_once(){
    if(m_read_idx >= READ_BUFFER_SIZE) return false;

    int bytes_read = 0;

    // LT模式读数据
    if(0 == m_TRIGMode){
        //从套接字接收数据，存储在m_read_buf缓冲区
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        //修改m_read_idx的读取字节数
        m_read_idx += bytes_read;

        if(bytes_read < 0) return false;

        return true;
    }
    // ET 模式读数据  //非阻塞ET模式下，需要一次性将数据读完
    else{
        while(true){
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
            if(bytes_read == -1){
                if(errno == EAGAIN || errno == EWOULDBLOCK){break;}
                return false;
            }else if(bytes_read == 0){
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
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
    //初始化从状态机状态、HTTP请求解析结果
    LINE_STATUS line_status = LINE_STATUS::LINE_OK;
    HTTP_CODE ret = HTTP_CODE::NO_REQUEST;
    char* text = 0;

    //这里为什么要写两个判断条件？第一个判断条件为什么这样写？
    //具体的在主状态机逻辑中会讲解。
    //parse_line为从状态机的具体实现
    while((m_check_state == CHECK_STATE::CHECK_STATE_CONTENT && line_status == LINE_STATUS::LINE_OK)
          || (line_status = parse_line()) == LINE_STATUS::LINE_OK){
        text = get_line();

        //m_start_line是每一个数据行在m_read_buf中的起始位置
        //m_checked_idx表示从状态机在m_read_buf中读取的位置
        m_start_line = m_checked_idx;
        LOG_INFO("%s", text);

        //主状态机的三种状态转移逻辑
        switch(m_check_state)
        {
            case CHECK_STATE::CHECK_STATE_REQUESTLINE:
            {
                //解析请求行
                ret = parse_request_line(text);
                if(ret == HTTP_CODE::BAD_REQUEST) return HTTP_CODE::BAD_REQUEST;
                break;
            }
            case CHECK_STATE::CHECK_STATE_HEADER:
            {
                //解析请求头
                ret = parse_headers(text);
                if(ret == HTTP_CODE::BAD_REQUEST) return HTTP_CODE::BAD_REQUEST;
                //完整解析GET请求后，跳转到报文响应函数
                else if(ret == HTTP_CODE::GET_REQUEST) return do_request();
                break;
            }
            case CHECK_STATE::CHECK_STATE_CONTENT:
            {
                //解析请求体
                ret = parse_content(text);

                //完整解析POST请求后，跳转到报文响应函数
                if(ret == HTTP_CODE::GET_REQUEST) return do_request();

                //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                line_status = LINE_STATUS::LINE_OPEN;
                break;
            }
            default:
                return HTTP_CODE::INTERNAL_ERROR;
        }
    }
    return HTTP_CODE::NO_REQUEST;
}      

// 填充http响应,向m_write_buf写入响应报文数据
bool http_conn::process_write(HTTP_CODE ret){
    
}     

//---------------------------------------------------
// 下面这组函数被process_read调用以分析http请求

// 解析http请求行，获得请求方法，目标url及http版本号
HTTP_CODE http_conn::parse_request_line(char* text){               //主状态机解析报文中的 请求行 数据
    //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
    //请求行中最先含有空格和\t任一字符的位置并返回
    m_url = strpbrk(text, "\t");

    //如果没有空格或\t，则报文格式有误
    if(!m_url) return HTTP_CODE::BAD_REQUEST;

    //将该位置改为\0，用于将前面数据取出
    *m_url = '\0';
    *m_url++;

    //取出数据，并通过与GET和POST比较，以确定请求方式
    char* method = text;
    if(strcasecmp(method, "GET") == 0) m_method = METHOD::GET;
    else if(strcasecmp(method, "POST") == 0) {m_method = METHOD::POST; cgi = 1;}
    else return HTTP_CODE::BAD_REQUEST;

    //m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
    //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符
    m_url += strspn(m_url, "\t");

    //使用与判断请求方式的相同逻辑，判断HTTP版本号
    m_version = strpbrk(m_url, "\t");
    if(!m_version) return HTTP_CODE::BAD_REQUEST;

    *m_version++ = '\0';
    m_version += strspn(m_version, "\t");

    //仅支持HTTP/1.1
    if(strcasecmp(m_version, "HTTP/1.1") != 0) return HTTP_CODE::BAD_REQUEST;

    //对请求资源前7个字符进行判断
    //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理
    if(strncasecmp(m_url, "http://", 7)){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    //同样增加https情况
    if(strncasecmp(m_url, "https://", 8)==0)
    {
        m_url+=8;
        m_url=strchr(m_url, '/');
    }

    //一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
    if(!m_url || m_url[0] != '/'){
        return HTTP_CODE::BAD_REQUEST;
    }

    //当url为/时，显示欢迎界面
    if(strlen(m_url) == 1)  strcat(m_url, "judge.html");

    //请求行处理完毕，将主状态机转移处理请求头
    m_check_state = CHECK_STATE::CHECK_STATE_HEADER;
    return HTTP_CODE::NO_REQUEST;
}

//解析http请求的一个头部信息
HTTP_CODE http_conn::parse_headers(char* text){                    //主状态机解析报文中的 请求头 数据
    //判断是空行还是请求头
    if(text[0] == '\0'){
        //判断是GET还是POST请求
        if(m_content_length != 0){
            //POST需要跳转到消息体处理状态
            m_check_state = CHECK_STATE::CHECK_STATE_CONTENT;
            return HTTP_CODE::NO_REQUEST;
        }
        return HTTP_CODE::GET_REQUEST;
    }
    //解析请求头部连接字段 Connection:
    else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;

        //跳过空格和\t字符
        text += strspn(text, "\t");
        if(strcasecmp(text, "keep-alive") == 0) m_linger = true;    //如果是长连接，则将linger标志设置为true
    }
    //解析请求头部内容长度字段 Content-length:
    else if(strncasecmp(text, "Content-length", 15) == 0){
        text += 15;
        text += strspn(text, "\t");
        m_content_length = atol(text);
    }
    //解析请求头部HOST字段 Host:
    else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, "\t");
        m_host = text;
    }
    else{
        LOG_INFO("oop!unknow header: %s", text);
    }
    return HTTP_CODE::NO_REQUEST;
}

// 解析请求体（POST 登录/注册数据）
HTTP_CODE http_conn::parse_content(char* text){                    //主状态机解析报文中的 请求体 数据
    //判断buffer中是否读取了消息体
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return HTTP_CODE::GET_REQUEST;
    }
    return HTTP_CODE::NO_REQUEST;
}

// 处理HTTP请求（核心：登录/注册/访问网页）
HTTP_CODE http_conn::do_request(){                                 //生成响应报文
    // 拼接网页根目录 + 请求路径
    // 将初始化的m_real_file赋值为网站根目录
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    //找到m_url中/的位置
    const char* p = strrchr(m_url, '/');

    // 处理登录/注册逻辑（POST）
    if(cgi == 1 && (*(p+1) == '2' || *(p +1) == '3')){
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';

        //如果是注册，先检测数据库中是否有重名的
        //没有重名的，进行增加数据
        if(*(p+1) == '3'){
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())  //没有重名
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(std::pair<std::string, std::string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_url, "/log.html");
                else
                    strcpy(m_url, "/registerError.html");
            }
            else
                strcpy(m_url, "/registerError.html");
        }
        
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if(*(p+1) == '2'){
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }

    }

    //如果请求资源为/0，表示跳转注册界面
    if(*(p + 1) == '0'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");

        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果请求资源为/1，表示跳转登录界面
    else if(*(p + 1) == '1'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");

        //将网站目录和/log.html进行拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果请求资源为/5，表示POST请求，跳转到picture.html，即图片请求页面
    else if(*(p + 1) == '5'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果请求资源为/6，表示POST请求，跳转到video.html，即视频请求页面
    else if(*(p + 1) == '6'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果请求资源为/7，表示POST请求，跳转到fans.html，即关注页面
    else if(*(p + 1) == '7'){
        char* m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");

        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    //如果以上均不符合，即不是登录和注册，直接将url与网站目录拼接
    //这里的情况是welcome界面，请求服务器上的一个图片
    else{
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    }
    
    // 获取文件状态
    // 通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
    // 失败返回NO_RESOURCE状态，表示资源不存在
    if(stat(m_real_file, &m_file_stat) < 0) return HTTP_CODE::NO_RESOURCE;

    //判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if(!(m_file_stat.st_mode & S_IROTH)) return HTTP_CODE::FORBIDDEN_RESOURCE;

    //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if(S_ISDIR(m_file_stat.st_mode)) return HTTP_CODE::BAD_REQUEST;

    //以只读方式获取文件描述符，通过mmap将该文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return HTTP_CODE::FILE_REQUEST;
}  
//----------------------------------------------------------


// 从状态机读取一行，用于分析出一行内容,分析是请求报文的哪一部分
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
LINE_STATUS http_conn::parse_line(){
    //m_read_idx指向缓冲区m_read_buf的数据末尾的下一个字节
    //m_checked_idx指向从状态机当前正在分析的字节
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        //temp为将要分析的字节
        temp = m_read_buf[m_checked_idx];
        
        //如果当前是\r字符，则有可能会读取到完整行
        if(temp == '\r'){

            //下一个字符达到了buffer结尾，则接收不完整，需要继续接收
            if((m_checked_idx + 1) == m_read_idx){
                return LINE_STATUS::LINE_OPEN;
            }

            //下一个字符是\n，将\r\n改为\0\0
            else if(m_read_buf[m_checked_idx+1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_STATUS::LINE_OK;
            }

            //如果都不符合，则返回语法错误
            return LINE_STATUS::LINE_BAD;
        }

        //如果当前字符是\n，也有可能读取到完整行
        //一般是上次读取到\r就到buffer末尾了，没有接收完整，再次接收时会出现这种情况
        else if(temp == '\n'){
            //前一个字符是\r，则接收完整
            if(m_checked_idx > 1 && m_read_buf[m_checked_idx-1] == '\r'){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_STATUS::LINE_OK;
            }
            return LINE_STATUS::LINE_BAD;
        }
    }
    //并没有找到\r\n，需要继续接收
    return LINE_STATUS::LINE_OPEN;
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