#pragma once 

#include <string>
#include <vector>
#include "ConnectionPool.hpp"

class OfflineMsgModel
{
public:
    // 新增用户的离线消息
    void Insert(int userid, std::string msg);
    // 删除用户的离线消息
    void Del(int userid);
    // 查询用户的离线消息
    std::vector<std::string> Query(int userid);
};