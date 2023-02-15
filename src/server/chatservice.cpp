#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
using namespace std;
using namespace muduo;
// 获取单例对象
Chatservice *Chatservice::instance()
{
    static Chatservice servcie;
    return &servcie;
}

// 注册消息以及对应的Handler回调对象
Chatservice::Chatservice()
{
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&Chatservice::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&Chatservice::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&Chatservice::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&Chatservice::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&Chatservice::addFriend, this, _1, _2, _3)});

    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&Chatservice::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&Chatservice::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&Chatservice::groupChat, this, _1, _2, _3)});
}

// 处理登录业务
void Chatservice::login(const TcpConnectionPtr &conn,
                        json &js,
                        Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _usermodel.query(id);
    if (user.getId() != -1 && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该账号已经登录，无法进行登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录，请重新输入新的账号";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // 登录成功，将该用户的状态改变为online
            user.setState("online");
            _usermodel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 用户登录成功，查询该用户的是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 发送该用户的离线消息后，删除该用户的离线消息
                _offlineMsgModel.del(id);
            }

            // 查询该用户的好友列表
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询该用户的群组信息
            vector<Group> groupVec = _groupModel.queryGroups(id);

            if (!groupVec.empty())
            {
                vector<string> vec3;
                for (Group &group : groupVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    vector<string> vec4;
                    for (GroupUser &user : group.getUser())
                    {
                        json userjs;
                        userjs["id"] = user.getId();
                        userjs["name"] = user.getName();
                        userjs["state"] = user.getState();
                        userjs["role"] = user.getRole();
                        vec4.push_back(userjs.dump());
                    }
                    groupjs["users"] = vec4;
                    vec3.push_back(groupjs.dump());
                }
                response["groups"] = vec3;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 用户登录失败，账号或密码输入错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "账号或密码输入错误，请重新输入";
        conn->send(response.dump());
    }
}

// 处理注册业务
void Chatservice::reg(const TcpConnectionPtr &conn,
                      json &js,
                      Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPwd(pwd);
    json response;
    if (_usermodel.insert(user))
    {
        // 注册成功
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        response["msgid"] = REG_MSG_ACK;
        response["erron"] = 1;
        conn->send(response.dump());
    }
}

// 新增好友业务
void Chatservice::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    _friendModel.insert(userid, friendid);
}

// 一对一聊天业务
void Chatservice::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // 用户在线，转发消息
            it->second->send(js.dump());
        }
    }
    // 用户不在线，发送离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 创建群组业务
void Chatservice::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);
    json response;
    if (_groupModel.createGroup(group))
    {
        // 创建成功
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 0;
        response["groupid"] = group.getId();
        _groupModel.addGroup(userid, group.getId(), "creator");
        conn->send(response.dump());
    }
    else
    {
        // 创建失败
        response["msgid"] = CREATE_GROUP_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 加入群组业务
void Chatservice::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["id"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群聊天业务
void Chatservice::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> vec = _groupModel.queryGrouUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : vec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}

// 处理注销业务
void Chatservice::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _usermodel.updateState(user);
}

// 异常结束，处理用户的状态信息
void Chatservice::reset()
{
    _usermodel.resetState();
}

// 获取消息对应的处理器
MsgHandler Chatservice::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }
    return _msgHandlerMap[msgid];
}

// 客户端的异常退出
void Chatservice::clientCloseExecption(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表中删除用户的连接信息
                _userConnMap.erase(it);
                user.setId(it->first);
                break;
            }
        }
    }

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _usermodel.updateState(user);
    }
}
