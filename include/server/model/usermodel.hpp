#ifndef _USERMODEL_H_
#define _USERMODEL_H_

#include "user.hpp"
#include "db.hpp"

// 用户的操作类
class UserModel{
public:
    // User表的增加方法
    bool insert(User &user);

    // 根据id获取对应用户的信息
    User query(int id);

    // 更新用户的状态信息
    bool updateState(User user);

    // 重置用户的状态信息
    void resetState();
};

#endif