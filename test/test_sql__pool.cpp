#include "../mysql/sql_connection_pool.h"
#include <iostream>

using namespace std;

int main(){
    connection_pool* pool = connection_pool::GetInstance();

    pool->init("localhost", "root", "123456", "qgydb", 3306, 8, 0);

    MYSQL* mysql = nullptr;
    connectionRAII(&mysql, pool);

    if(mysql){
        cout << "✅ 数据库连接池获取连接成功！" << endl;

        int ret = mysql_query(mysql, "INSERT INTO user(username, passwd) VALUES('test','123456')");

        if(!ret) cout << "✅ 插入用户成功！" << endl;
        else cout << "❌ 插入失败！" << endl;
    }
    else{
        cout << "❌ 连接池获取连接失败！" << endl;
    }

    return 0;
}