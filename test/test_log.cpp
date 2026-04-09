#include "../log/log.h"
#include "../lock/lock.h"
#include <unistd.h>

int main(){
    Log::get_instance()->init("server.log", 0, 8192, 5000000, 1024);

    LOG_DEBUG("这是Debug日志，测试成功！");
    LOG_INFO("这是Info日志，项目：%s", "webserver_2.0");
    LOG_WARN("这是Warn日志");
    LOG_ERROR("这是Error日志");

    sleep(1);
    return 0;
}