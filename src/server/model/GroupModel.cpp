#include "GroupModel.hpp"

// 创建群组
bool GroupModel::CreateGroup(fixbuf::Group &group)
{
    char sql[128] = {0};
    sprintf(sql, "insert into allgroup(groupname, groupdesc) values('%s', '%s');", group.name().c_str(), group.desc().c_str());

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    if (conn->Update(sql))
    {
        group.set_id(mysql_insert_id(conn->GetMysql()));
        return true;
    }
    return false;
}

// 加入群组
bool GroupModel::AddGroup(int userid, int groupid, const std::string &role)
{
    char sql[128] = {0};
    sprintf(sql, "insert into groupuser(groupid, userid, role) values(%d, %d, '%s');", groupid, userid, role.c_str());

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    return conn->Update(sql);
}

// 查询用户所在的群组信息
std::vector<fixbuf::Group> GroupModel::QueryGroup(int userid)
{
    char sql[128] = {0};
    std::vector<fixbuf::Group> vec;
    sprintf(sql, "select id, groupname, groupdesc from allgroup where id in (select groupid from groupuser where userid = %d);", userid);

    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    if (res != nullptr)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            fixbuf::Group group;
            group.set_id(atoi(row[0]));
            group.set_name(row[1]);
            group.set_desc(row[2]);
            vec.push_back(group);
        }
        mysql_free_result(res);
    }

    for (fixbuf::Group &group : vec)
    {
        sprintf(sql, "select a.id, a.name, a.state, b.role from user a inner join groupuser b on a.id = b.userid where b.groupid = %d;", group.id());
        MYSQL_RES *res = conn->Query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                fixbuf::GroupUser groupuser;
                fixbuf::User *user = groupuser.mutable_user();
                user->set_id(atoi(row[0]));
                user->set_name(row[1]);
                user->set_state(strcmp(row[2], "offline"));
                groupuser.set_role(row[3]);
                group.add_groupusers()->CopyFrom(groupuser);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
std::vector<int> GroupModel::QueryGroupUsers(int userid, int groupid)
{
    char sql[128] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d && userid != %d;", groupid, userid);

    std::vector<int> vec;
    std::shared_ptr<MySQL> conn = ConnectionPool::GetInstance()->GetConnection();
    MYSQL_RES *res = conn->Query(sql);
    if (res != nullptr)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            vec.push_back(atoi(row[0]));
        }
        mysql_free_result(res);
    }
    return vec;
}