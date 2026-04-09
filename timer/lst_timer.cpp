#include "lst_timer.h"

// 初始化空链表
sort_timer_lst::sort_timer_lst(){
    head = nullptr;
    tail = nullptr;
}

// 析构：清空所有定时器
sort_timer_lst::~sort_timer_lst(){
    util_timer* tmp = head;
    while(tmp){
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}


//添加定时器，内部调用私有成员add_timer
void sort_timer_lst::add_timer(util_timer* timer){
    if(!timer) return;

    // 空链表，直接作为头尾
    if(!head){
        head = tail = timer;
        return;
    }

    // 比头节点早，插入头部
    if(timer->expire < head->expire){
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    //否则调用私有成员，调整内部结点
    add_timer(timer, head);
}

// 调整定时器（超时时间更新）任务发生变化时，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer* timer){
    if(!timer) return;

    //被调整的定时器在链表尾部
    //定时器超时值仍然小于下一个定时器超时值，不调整
    util_timer* tmp = timer->next;
    if(!tmp || timer->expire < tmp->expire){
        return;
    }

    //被调整定时器是链表"头结点"，将定时器取出，重新插入
    if(timer == head){
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }

    //被调整定时器是链表“中间节点"，将定时器取出，重新插入
    else{
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

// 删除定时器
void sort_timer_lst::del_timer(util_timer* timer){
    if(!timer) return;

    //链表中只有一个定时器，需要删除该定时器
    if((timer == head) && (head == tail)){
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }

    // 删除头节点（被删除的定时器为头结点）
    if(timer == head){
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }

    // 删除尾节点（被删除的定时器为尾结点）
    if(timer == tail){
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }

    // 删除中间节点(被删除的定时器在链表内部，常规链表结点删除)
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

/******************************************************************************************
 * 使用统一事件源，SIGALRM信号每次被触发，主循环中调用一次定时任务处理函数，处理链表容器中到期的定时器。
 * 具体的逻辑如下，

 * - 遍历定时器升序链表容器，从头结点开始依次处理每个定时器，直到遇到尚未到期的定时器
 * - 若当前时间小于定时器超时时间，跳出循环，即未找到到期的定时器
 * - 若当前时间大于定时器超时时间，即找到了到期的定时器，执行回调函数，然后将它从链表中删除，然后继续遍历
 ******************************************************************************************/
// 定时处理：检查并处理超时定时器
void sort_timer_lst::tick(){
    if(!head) return;

    //获取当前时间
    time_t cur = time(nullptr);
    util_timer* tmp = head;

    // 遍历表头，处理所有超时节点
    while(tmp){
        //链表容器为升序排列
        //当前时间小于定时器的超时时间，后面的定时器也没有到期
        if(cur < tmp->expire){
            break;
        }

        //当前定时器到期，则调用回调函数，执行定时事件
        tmp->cb_func(tmp->user_data);

        //将处理后的定时器从链表容器中删除，并重置头结点
        head = tmp->next;
        if(head) head->prev = nullptr;
        delete tmp;
        tmp = head;
    }
}

//私有成员，被公有成员add_timer和adjust_time调用
//主要用于调整链表内部结点
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head){
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;

    //遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置，常规双向链表插入操作
    while(tmp){
        if(timer->expire < tmp->expire){
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    //遍历完未找到，目标定时器需要放到尾结点处
    if(!tmp){
        prev->next = timer;
        timer->prev = prev;
        timer->next = nullptr;
        tail = timer;
    }
}


void cb_func(client_data* user_data){
    //删除非活动连接在socket上的注册事件
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);

    //关闭文件描述符
    close(user_data->sockfd);

    //减少连接数
    // http_conn::m_user_count--;

    LOG_INFO("close fd %d", user_data->sockfd);
}



// 初始化时间槽
void Utils::init(int timeslot){
    m_TIMESLOT = timeslot;
}

// 设置文件描述符非阻塞
int Utils::setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 向epoll添加文件描述符
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
    epoll_event event;
    event.data.fd = fd;
    if(1 == TRIGMode) event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else event.events = EPOLLIN | EPOLLRDHUP;

    if(one_shot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理函数：将信号写入管道
void Utils::sig_handler(int sig){
    //为保证函数的可重入性，保留原来的errno
    //可重入性表示中断后再次进入该函数，环境变量与之前相同，不会丢失数据
    int save_errno = errno;
    int msg = sig;

    //将信号值从管道写端写入，传输字符类型，而非整型
    send(u_pipefd[1], (char*) &msg, 1, 0);

    //将原来的errno赋值为当前的errno
    errno = save_errno;
}

// 自定义信号函数：创建sigaction结构体变量，设置信号函数
// 信号处理函数中仅仅通过管道发送信号值，不处理信号对应的逻辑，缩短异步执行时间，减少对主程序的影响。
void Utils::addsig(int sig, void(handler)(int), bool restart = true){
    //创建sigaction结构体变量
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    //信号处理函数中仅仅发送信号值，不做对应逻辑处理
    sa.sa_handler = handler;
    if(restart) sa.sa_flags |= SA_RESTART;

    //将所有信号添加到信号集中
    sigfillset(&sa.sa_mask);

    //执行sigaction函数
    assert(sigaction(sig, &sa, nullptr) != -1);
}

// 定时处理任务（触发tick）
// 定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler(){
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

// 发送错误信息并关闭连接
void Utils::show_error(int connfd, const char* info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

// 静态成员初始化
int* Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;