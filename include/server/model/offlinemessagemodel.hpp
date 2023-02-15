#ifndef _OFFLINEMESSAGEMODEL_H_
#define _OFLLINEMESSAGEMODEL_H_

#include <string>
#include <vector>
using namespace std;

// 离线消息操作类
class OfflineMsgModel
{
public:
    // 新增用户的离线消息
    void insert(int userid, string msg);
    // 删除用户的离线消息
    void del(int userid);
    // 查询用户的离线消息
    vector<string> query(int userid);
};

#endif