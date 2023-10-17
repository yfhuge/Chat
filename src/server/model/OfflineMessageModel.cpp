#include "OfflineMessageModel.hpp"
#include <memory>

// 新增用户的离线消息
void OfflineMsgModel::Insert(int userid, std::string msg)
{
    char sql[128] = {0};
    sprintf(sql, "insert into offlinemessage(userid, message) values(%d, '%s');", userid, msg.c_str());

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    conn->Update(sql);
}

// 删除用户的离线消息
void OfflineMsgModel::Del(int userid)
{
    char sql[128] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d;", userid);

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    conn->Update(sql);
}

// 查询用户的离线消息
std::vector<std::string> OfflineMsgModel::Query(int userid)
{
    char sql[128]  = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d;", userid);

    std::vector<std::string> vec;
    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    if (res != nullptr)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            vec.push_back(row[0]);
        }
        mysql_free_result(res);
    }
    return vec;
}