#pragma once

#include "ConnectionPool.hpp"
#include "public.pb.h"
#include <vector>

class GroupModel
{
public:
    // 创建群组
    bool CreateGroup(fixbuf::Group &group);
    // 加入群组
    bool AddGroup(int userid, int groupid, const std::string &role);
    // 查询用户所在的群组信息
    std::vector<fixbuf::Group> QueryGroup(int userid);
    // 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
    std::vector<int> QueryGroupUsers(int userid, int groupid);
};