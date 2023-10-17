#include "db.hpp"

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
bool MySQL::Connect(string host, string user, string pwd, string dbname, unsigned short port)
{
    MYSQL *ret = mysql_real_connect(_mysql, host.c_str(), user.c_str(), pwd.c_str(),
                                    dbname.c_str(), port, nullptr, 0);
    if (ret != nullptr)
    {
        // C和C++都是使用的ASCII，这里设置使用中文显示
        mysql_query(_mysql, "set names gbk");
        LOG_INFO << "connect sql sucess!";
        return true;
    }
    LOG_ERROR << "connect sql fail!";
    return false;
}

// 更新操作
bool MySQL::Update(string sql)
{
    if (mysql_query(_mysql, sql.c_str()))
    {
        LOG_ERROR << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败, error : " << mysql_error(_mysql);
        return false;
    }
    return true;
}

// 查询操作
MYSQL_RES *MySQL::Query(string sql)
{
    if (mysql_query(_mysql, sql.c_str()))
    {
        LOG_ERROR << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败, error : " << mysql_error(_mysql);
        return nullptr;
    }
    return mysql_use_result(_mysql);
}

// 获取连接
MYSQL *MySQL::GetMysql()
{
    return _mysql;
}

// 重置_alivetime;
void MySQL::ResetAliveTime()
{
    _alivetime = clock();
}

// 返回_alivetime
clock_t MySQL::GetAliveTime()
{
    return _alivetime;
}