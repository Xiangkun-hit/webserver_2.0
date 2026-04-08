#include <iostream>
#include <mysql/mysql.h>

using namespace std;

int main(){
    MYSQL* conn = mysql_init(nullptr);

    if(mysql_real_connect(conn, "localhost", "root", "123456", nullptr, 3306, nullptr, 0)){
        cout << "mysql 连接成功" << endl;
    }else{
        cout << "mysql 连接失败" << endl;
    }
    mysql_close(conn);
    return 0;
}