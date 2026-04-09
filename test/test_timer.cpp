#include "../timer/lst_timer.h"
#include <iostream>
using namespace std;

int main(){
    // 初始化工具类
    Utils utils;
    utils.init(5);

    // 创建定时器链表
    sort_timer_lst timer_lst;

    // 创建客户端数据
    client_data user_data;
    user_data.sockfd = 100;

    // 创建定时器s
    util_timer* timer = new util_timer;
    timer->user_data = &user_data;
    timer->expire = time(nullptr) + 10;
    timer->cb_func = cb_func;

    // 添加定时器
    timer_lst.add_timer(timer);
    cout << "✅ 定时器添加成功！" << endl;

    // 触发定时处理
    timer_lst.tick();

    // 删除定时器
    timer_lst.del_timer(timer);
    cout << "✅ 定时器删除成功！" << endl;

    return 0;
}