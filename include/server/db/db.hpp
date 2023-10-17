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
    bool Connect(string host, string user, string pwd, string dbname, unsigned short port);
    // 更新操作
    bool Update(string sql);
    // 查询操作
    MYSQL_RES *Query(string sql);
    // 获取连接
    MYSQL *GetMysql();
    // 重置_alivetime;
    void ResetAliveTime();
    // 返回_alivetime
    clock_t GetAliveTime();
private:
    MYSQL *_mysql;
    clock_t _alivetime; // 记录空闲状态后的存活时间
};

#endif