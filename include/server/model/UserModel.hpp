#pragma once

#include "ConnectionPool.hpp"
#include "public.pb.h"

class UserModel
{
public:
    // 增加用户信息
    bool Insert(fixbuf::User &user);

    // 根据id获取对应用户的信息
    fixbuf::User Query(int id);

    // 更新用户的状态信息
    bool UpdateState(fixbuf::User user);

    // 重置用户的状态信息
    void ResetState();
};