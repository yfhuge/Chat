#pragma once

#include <functional>
#include <muduo/net/TcpConnection.h>
#include <string>
#include <unordered_map>
#include "public.pb.h"
#include "UserModel.hpp"
#include "FriendModel.hpp"
#include "GroupModel.hpp"
#include "OfflineMessageModel.hpp"
#include "FileModel.hpp"
#include <muduo/base/Logging.h>
#include "FileServer.hpp"
#include "redis.hpp"
#include "LRU.hpp"

// 表示处理消息的事件回调函数
using MsgHandler = std::function<void(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp)>;

class ChatService
{
public:
    static ChatService *GetInstance();

    // 返回指定的业务处理函数
    MsgHandler GetHandler(int type);

    // 处理登录业务
    void Login(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 处理注册业务
    void Register(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 添加好友业务
    void AddFriend(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 一对一聊天业务
    void OneChat(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 创建群组业务
    void CreateGroup(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 加入群组业务
    void AddGroup(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 群聊天业务
    void GroupChat(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 注销业务
    void Logout(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 查看网盘文件业务
    void List(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 删除网盘指定文件
    void Del(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 上传网盘文件业务
    void UpLoad(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);
    // 下载网盘文件业务
    void DownLoad(const muduo::net::TcpConnectionPtr&, const std::string&, muduo::Timestamp);

    // 异常结束，重置用户的状态信息
    void Reset();
    // 客户端的异常退出
    void ClientCloseExecpt(const muduo::net::TcpConnectionPtr&);

    // 从redis消息队列中获取订阅消息
    void HandleRedisSubscribeMsg(int, std::string);

private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> m_msgHandlerMap;

    // 存储在线用户的通信连接
    std::unordered_map<int, muduo::net::TcpConnectionPtr> m_userConnMap;

    // 互斥锁，保证m_userConnMap的线程安全
    std::mutex m_mutex;

    // 用户操作类
    UserModel m_usermodel;

    // 好友操作类
    FriendModel m_friendmodel;

    // 群组操作类
    GroupModel m_groupModel;

    // 消息操作类
    OfflineMsgModel m_offlineMsgModel;

    // 文件操作类
    FileModel m_fileModel;

    // redis操作对象
    Redis m_redis;

    // LRU对象
    LRU m_lru;
};