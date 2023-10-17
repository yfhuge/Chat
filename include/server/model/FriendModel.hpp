#pragma once

#include "ConnectionPool.hpp"
#include <vector>
#include "public.pb.h"

class FriendModel
{
public:
    // 新增好友
    bool Insert(int userid, int friendid);

    // 查询好友列表
    std::vector<fixbuf::User> Query(int userid);
};