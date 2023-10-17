#include "UserModel.hpp"

// 增加用户信息
bool UserModel::Insert(fixbuf::User &user)
{
    char sql[128] = {0};
    sprintf(sql, "insert into user(name, password, state) values('%s', '%s', '%s');", user.name().c_str(), user.password().c_str(), user.state()?"offline":"online");

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    if (conn->Update(sql))
    {
        user.set_id(mysql_insert_id(conn->GetMysql()));
        return true;
    }
    return false;
    
}

// 根据id获取对应用户的信息
fixbuf::User UserModel::Query(int id)
{
    char sql[128] = {0};
    sprintf(sql, "select * from user where id = %d;", id);

    // 获得连接
    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    fixbuf::User user;
    if (res != nullptr)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr)
        {
            user.set_id(atoi(row[0]));
            user.set_name(row[1]);
            user.set_password(row[2]);
            user.set_state(strcmp(row[3], "online") == 0);
            mysql_free_result(res);
        }
    }
    else 
    {
        user.set_id(-1);
    }
    return user;
}

// 更新用户的状态信息
bool UserModel::UpdateState(fixbuf::User user)
{
    char sql[128] = {0};
    std::string state = user.state()?"online":"offline";
    sprintf(sql, "update user set state = '%s' where id = %d;", user.state()?"online":"offline", user.id());

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    if (conn->Update(sql))
    {
        return true;
    }
    return false;
}

// 重置用户的状态信息
void UserModel::ResetState()
{
    char sql[128] = "update user set state = 'offline';";

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    conn->Update(sql);
}