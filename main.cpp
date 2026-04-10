#include "web_server.h"

int main(int argc, char* argv[])
{
    // 端口
    int port = 9006;

    // 数据库信息
    std::string user = "root";
    std::string passWord = "123456";
    std::string databaseName = "qgydb";

    // 日志写入方式
    int log_write = 1;
    // 优雅关闭
    int opt_linger = 0;
    // 触发模式
    int trigmode = 3;
    // 数据库连接数
    int sql_num = 8;
    // 线程数
    int thread_num = 8;
    // 日志开关
    int close_log = 0;
    // 事件模式
    int actor_model = 0;

    // 初始化服务器
    WebServer server;
    server.init(port, user, passWord, databaseName, log_write, opt_linger, trigmode, sql_num, thread_num, close_log, actor_model);
    server.trig_mode();
    server.log_write();
    server.sql_pool();
    server.thread_pool();
    server.event_listen();
    server.event_loop();

    return 0;
}