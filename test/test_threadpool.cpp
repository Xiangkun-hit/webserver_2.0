#include "../threadpool/threadpool.h"
#include "../mysql/sql_connection_pool.h"
#include <iostream>
#include <unistd.h>

using namespace std;

// 模拟任务类
struct mock_task {
    void process() {
        cout << "✅ 线程池处理任务成功！" << endl;
    }
    int m_state;
};

int main(){
    // 初始化数据库连接池（仅用于构造线程池）
    connection_pool* conn = connection_pool::GetInstance();
    conn->init("localhost", "root", "123456", "qgydb", 3306, 8, 0);

    threadpool<mock_task>* pool = new threadpool<mock_task>(0, conn, 4, 100);

    mock_task task;
    task.m_state = 0;

    pool->append(&task);

    sleep(1);

    delete pool;

    cout << "✅ 线程池测试完成！" << endl;
    return 0;
}