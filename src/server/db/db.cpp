#include "db.hpp"

static string host = "127.0.0.1";
static string user = "root";
static string passwd = "123456";
static string dbname = "chat";

// 初始化数据库
MySQL::MySQL()
{
    _mysql = mysql_init(nullptr);
}

// 关闭数据库
MySQL::~MySQL()
{
    if (_mysql != nullptr)
    {
        mysql_close(_mysql);
    }
}

// 连接数据库
bool MySQL::connect()
{
    MYSQL *ret = mysql_real_connect(_mysql, host.c_str(), user.c_str(), passwd.c_str(),
                                    dbname.c_str(), 3306, nullptr, 0);
    if (ret != nullptr)
    {
        // C和C++都是使用的ASCII，这里设置使用中文显示
        mysql_query(_mysql, "set names gbk");
        LOG_INFO << "connect sql sucess!";
        return true;
    }
    LOG_INFO << "connect sql fail!";
    return false;
}

// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_mysql, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败";
        return false;
    }
    return true;
}

// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_mysql, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败";
        return nullptr;
    }
    return mysql_use_result(_mysql);
}

// 获取连接
MYSQL *MySQL::getMysql()
{
    return _mysql;
}