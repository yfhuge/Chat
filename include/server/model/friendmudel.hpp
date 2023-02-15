#ifndef _FRIENDMODEL_H_
#define _FRIENDMODEL_H_

#include "user.hpp"
#include <vector>
using namespace std;

// 用户好友表的操作
class FriendModel{
public:
    // 添加好友
    void insert(int userid, int friendid);

    // 查询好友列表
    vector<User> query(int userid);
};

#endif