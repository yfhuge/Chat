#ifndef _DB_H_
#define _DB_H_

#include <mysql/mysql.h>
#include <string>
#include <muduo/base/Logging.h>
using namespace std;

class MySQL
{
public:
    // 初始化数据库
    MySQL();
    // 关闭数据库
    ~MySQL();
    // 连接数据库
    bool connect();
    // 更新操作
    bool update(string sql);
    // 查询操作
    MYSQL_RES *query(string sql);
    // 获取连接
    MYSQL *getMysql();

private:
    MYSQL *_mysql;
};

#endif