#include "FriendModel.hpp"

// 新增好友
bool FriendModel::Insert(int userid, int friendid)
{
    char sql[128] = {0};
    sprintf(sql, "select * from user where id = '%d';", friendid);

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    if (res == nullptr)
    {
        LOG_ERROR << "所添加的用户不存在";
        return false;
    }
    mysql_free_result(res);
    sprintf(sql, "insert into friend(userid, friendid) values(%d, %d);", userid, friendid);
    if (conn->Update(sql)) 
    {
        return true;
    }
    return false;
}

// 查询好友列表
std::vector<fixbuf::User> FriendModel::Query(int userid)
{
    char sql[128] = {0};
    sprintf(sql, "select * from user where id in (select friendid from friend where userid = %d);", userid);

    std::vector<fixbuf::User> vec;
    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    if (res != nullptr)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            fixbuf::User user;
            user.set_id(atoi(row[0]));
            user.set_name(row[1]);
            user.set_state(!strcmp(row[3], "online"));
            vec.push_back(user);
        }
        mysql_free_result(res);
    }
    return vec;
}