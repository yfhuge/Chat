#include "ChatService.hpp"

ChatService::ChatService() : m_lru(100)
{
    m_msgHandlerMap.insert({0, std::bind(&ChatService::Login, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({2, std::bind(&ChatService::Register, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({4, std::bind(&ChatService::AddFriend, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({6, std::bind(&ChatService::CreateGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({8, std::bind(&ChatService::AddGroup, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({10, std::bind(&ChatService::OneChat, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({11, std::bind(&ChatService::GroupChat, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({12, std::bind(&ChatService::Logout, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({13, std::bind(&ChatService::List, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({15, std::bind(&ChatService::Del, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({17, std::bind(&ChatService::DownLoad, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});
    m_msgHandlerMap.insert({19, std::bind(&ChatService::UpLoad, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)});

    // 连接redis服务器
    if (m_redis.Connect())
    {
        // 设置上报消息的回调函数
        m_redis.InitNotifyHandler(std::bind(&ChatService::HandleRedisSubscribeMsg, this, std::placeholders::_1, std::placeholders::_2));
    }
}

ChatService* ChatService::GetInstance()
{
    static ChatService instance;
    return &instance;
}

// 返回指定的业务处理函数
MsgHandler ChatService::GetHandler(int type)
{
    // 查找执行的业务处理器
    auto it = m_msgHandlerMap.find(type);
    if (it == m_msgHandlerMap.end())
    {
        //  没有找到，返回一个默认的处理器，写入日志操作
        return [=](const muduo::net::TcpConnectionPtr& conn, const std::string& buf, muduo::Timestamp time){
            std::cout << "conn't find " << std::endl;
        };
    }
    return it->second;
}

// 处理登录业务
void ChatService::Login(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::User user, recv_user;
    if (!recv_user.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    user = m_usermodel.Query(recv_user.id());
    fixbuf::Info info;
    info.set_type(fixbuf::LOGIN_MSG_ACK);
    fixbuf::ErrorMsg errmsg;
    if (user.id() != -1 && user.password() == recv_user.password())
    {
        if (user.state())
        {
            // 此时这个用户是在线状态
            errmsg.set_errcode(2);
            errmsg.set_errmsg("该账号已经登录，请重新登录其他的账号");
        }
        else 
        {
            // 此时这个用户是离线状态，用户登录成功，记录用户连接信息
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_userConnMap.insert({user.id(), conn});
                int ans = m_lru.put(user.id(), user.id());
                if (ans != -1)
                {
                    // 丢弃之前的连接
                    fixbuf::User m_lastUser;
                    m_lastUser = m_usermodel.Query(ans);
                    m_redis.UnSubscribe(m_lastUser.id());
                    m_lastUser.set_state(false);
                    m_usermodel.UpdateState(m_lastUser);
                    m_userConnMap[ans]->shutdown();
                    m_userConnMap.erase(ans);
                }
            }

            // 用户登录成功后，向redis订阅channel
            m_redis.Subscribe(user.id());

            // 更新用户的信息
            user.set_state(true);
            m_usermodel.UpdateState(user);

            errmsg.set_errcode(0);
            fixbuf::LoginMsg loginmsg;
            loginmsg.mutable_user()->CopyFrom(user);

            // 查询是否有离线消息
            std::vector<std::string> msgVec = m_offlineMsgModel.Query(user.id());
            if (msgVec.size())
            {
                for (std::string &msg : msgVec)
                {
                    loginmsg.add_recvmsg(msg);
                }
                m_offlineMsgModel.Del(user.id());
            }

            // 查询用户的好友列表
            std::vector<fixbuf::User> userVec = m_friendmodel.Query(user.id());
            if (userVec.size())
            {
                for (fixbuf::User &m_user : userVec)
                {
                    loginmsg.add_friendlist()->CopyFrom(m_user);
                }
            }

            // 查询用户的群组信息
            std::vector<fixbuf::Group> groupVec = m_groupModel.QueryGroup(user.id());
            if (groupVec.size())
            {
                for (fixbuf::Group &group : groupVec)
                {
                    loginmsg.add_grouplist()->CopyFrom(group);
                }
            }

            std::string loginmsg_buf;
            if (!loginmsg.SerializeToString(&loginmsg_buf))
            {
                LOG_ERROR << "serialize error";
                return;
            }
            errmsg.set_errmsg(loginmsg_buf);
        }
    }
    else 
    {
        errmsg.set_errcode(1);
        errmsg.set_errmsg("账号不存在或者密码输入错误，请重新输入");
    }
    std::string msg_buf;
    if (!errmsg.SerializeToString(&msg_buf))
    {
        LOG_ERROR << "serialize error, context:" << msg_buf;
        return;
    }
    info.set_info(msg_buf);
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    conn->send(info_buf);
}

// 处理注册业务
void ChatService::Register(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::User user;
    if (!user.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    m_lru.put(user.id(), user.id());
    fixbuf::ErrorMsg msg;
    if (m_usermodel.Insert(user))
    {
        msg.set_errcode(0);
        std::string user_buf;
        if (!user.SerializeToString(&user_buf))
        {
            LOG_ERROR << "serialize error";
            return;
        }
        msg.set_errmsg(user_buf);
    }
    else 
    {
        msg.set_errcode(1);
    }
    std::string msg_buf;
    if (!msg.SerializeToString(&msg_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::REGISTER_MSG_ACK);
    info.set_info(msg_buf);
    std::string buffer;
    if (!info.SerializeToString(&buffer))
    {
        LOG_ERROR << "serialize error";
    }
    conn->send(buffer);
}

// 新增好友业务
void ChatService::AddFriend(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::Friend friendmsg;
    if (!friendmsg.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    m_lru.put(friendmsg.userid(), friendmsg.userid());
    fixbuf::ErrorMsg errmsg;
    if (m_friendmodel.Insert(friendmsg.userid(), friendmsg.friendid()))
    {
        errmsg.set_errcode(0);
    }
    else 
    {
        errmsg.set_errcode(1);
        errmsg.set_errmsg("添加的好友不存在，请添加存在的用户为好友");
    }
    fixbuf::Info info;
    info.set_type(fixbuf::ADD_FRIEND_MSG_ACK);
    std::string msg_buf;
    if (!errmsg.SerializeToString(&msg_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    info.set_info(msg_buf);
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    conn->send(info_buf);
}

// 一对一聊天业务
void ChatService::OneChat(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::ToUserMsg onechat;
    if (!onechat.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return; 
    }
    m_lru.put(onechat.userid(), onechat.userid());
    int toid = onechat.touserid();
    fixbuf::FromMsg recvmsg;
    recvmsg.set_fromid(onechat.userid());
    recvmsg.set_sendtime(onechat.sendtime());
    recvmsg.set_msg(onechat.msg());
    std::string recvmsg_buf, info_buf;
    if (!recvmsg.SerializeToString(&recvmsg_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::ONE_CHAT);
    info.set_info(recvmsg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_userConnMap.find(toid);
        if (it != m_userConnMap.end())
        {
            // 用户在线，转发消息
            it->second->send(info_buf);
            return;
        }
    }

    // 查询用户是否在线，在线就通过redis发布
    fixbuf::User toUser = m_usermodel.Query(toid);
    if (toUser.state())
    {
        m_redis.Publish(toid, info_buf);
        return;
    }
    
    // 用户不在线，发送离线消息
    m_offlineMsgModel.Insert(toid, info_buf);
}

// 创建群组业务
void ChatService::CreateGroup(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::CreateGroup crtGroup;
    if (!crtGroup.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    int userid = crtGroup.userid();
    m_lru.put(userid, userid);
    fixbuf::Group group = crtGroup.group();
    fixbuf::ErrorMsg errmsg;
    if (m_groupModel.CreateGroup(group))
    {
        m_groupModel.AddGroup(userid, group.id(), "creator");
        errmsg.set_errcode(0);
        errmsg.set_errmsg(std::to_string(group.id()));
    }
    else 
    {
        errmsg.set_errcode(1);
    }
    fixbuf::Info info;
    std::string msg_buf, info_buf;
    if (!errmsg.SerializeToString(&msg_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    info.set_type(fixbuf::CREATE_GROUP_MSG_ACK);
    info.set_info(msg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    conn->send(info_buf);
}

// 加入群组业务
void ChatService::AddGroup(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::AddGroup addgroup;
    if (!addgroup.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    m_lru.put(addgroup.userid(), addgroup.userid());
    fixbuf::ErrorMsg errmsg;
    if (m_groupModel.AddGroup(addgroup.userid(), addgroup.groupid(), "normal"))
    {
        errmsg.set_errcode(0);
        errmsg.set_errmsg("加入群组"+std::to_string(addgroup.groupid()) +"成功，快来和群友打个招呼吧");
    }
    else 
    {
        errmsg.set_errcode(1);
        errmsg.set_errmsg("所加入的群组不存在，请重新操作加入一个存在的群组");
    }
    fixbuf::Info info;
    info.set_type(fixbuf::ADD_FRIEND_MSG_ACK);
    std::string msg_buf, info_buf;
    if (!errmsg.SerializeToString(&msg_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    info.set_info(msg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error";
        return;
    }
    conn->send(info_buf);
}

// 群聊天业务
void ChatService::GroupChat(const muduo::net::TcpConnectionPtr &conn, const std::string &buf, muduo::Timestamp time)
{
    fixbuf::GroupMsg groupmsg;
    if (!groupmsg.ParseFromString(buf))
    {
        LOG_ERROR << "parse error, context:" << buf;
        return;
    }
    m_lru.put(groupmsg.userid(), groupmsg.userid());
    std::vector<int> vec = m_groupModel.QueryGroupUsers(groupmsg.userid(), groupmsg.groupid());

    std::lock_guard<std::mutex> lock(m_mutex);
    for (int id : vec)
    {
        auto it = m_userConnMap.find(id);
        fixbuf::Info info;
        info.set_type(fixbuf::GROUP_CHAT);
        info.set_info(buf);
        std::string info_buf;
        if (!info.SerializeToString(&info_buf))
        {
            LOG_ERROR << "serialize error!";
            return;
        }
        if (it != m_userConnMap.end())
        {
            it->second->send(info_buf);
        }
        else 
        {
            fixbuf::User user = m_usermodel.Query(id);
            if (user.state())
            {
                m_redis.Publish(id, info_buf);
            }
            else 
            {
                m_offlineMsgModel.Insert(id, info_buf);
            }
        }
    }
}

// 注销
void ChatService::Logout(const muduo::net::TcpConnectionPtr &conn, const std::string &buffer, muduo::Timestamp time)
{
    int id = atoi(buffer.c_str());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_userConnMap.find(id);
        if (it != m_userConnMap.end())
        {
            m_userConnMap.erase(it);
        }
    }
    m_lru.del(id);
    // 用户注销，先当于下线，在redis中订阅
    m_redis.UnSubscribe(id);

    // 更新用户的状态信息
    fixbuf::User user;
    user.set_id(id);
    user.set_state(false);
    m_usermodel.UpdateState(user);
}

// 查看用户的网盘文件
void ChatService::List(const muduo::net::TcpConnectionPtr &conn, const std::string &buffer, muduo::Timestamp time)
{
    int id = atoi(buffer.c_str());
    fixbuf::User user;
    user = m_usermodel.Query(id);
    
    std::vector<std::string> vec = m_fileModel.List(user.name());
    fixbuf::FileList filelist;
    filelist.set_userid(id);
    for (auto filename : vec)
    {
        filelist.add_filelist(filename);
    }
    std::string filelist_buf, info_buf;
    if (!filelist.SerializeToString(&filelist_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::LIST_MSG_ACK);
    info.set_info(filelist_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }

    conn->send(info_buf);
}

// 删除用户的网盘文件
void ChatService::Del(const muduo::net::TcpConnectionPtr &conn, const std::string &buffer, muduo::Timestamp time)
{
    fixbuf::FileList filelist;
    if (!filelist.ParseFromString(buffer))
    {
        LOG_ERROR << "parse error, context:" << buffer;
        return;
    }
    if (filelist.mutable_filelist()->size() == 0) 
    {
        return;
    }
    std::string filename = filelist.mutable_filelist()->at(0);
    fixbuf::User user = m_usermodel.Query(filelist.userid());
    fixbuf::ErrorMsg errmsg;
    if (m_fileModel.DeleteFile(user.name(), filename))
    {
        errmsg.set_errcode(0);
        errmsg.set_errmsg(filename + " delete success");
    }
    else 
    {
        errmsg.set_errcode(1);
        errmsg.set_errmsg(filename + " isn't exists, cann't delete!");
    }
    std::string errmsg_buf, info_buf;
    if (!errmsg.SerializeToString(&errmsg_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::DELETEFILE_MSG_ACK);
    info.set_info(errmsg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    conn->send(info_buf);
}

// 上传网盘文件业务
void ChatService::UpLoad(const muduo::net::TcpConnectionPtr &conn, const std::string &buffer, muduo::Timestamp time)
{
    fixbuf::FileTrans filetrans;
    filetrans.set_ip(FileServer::GetInstance()->GetIp());
    filetrans.set_port(FileServer::GetInstance()->GetPort());
    filetrans.set_filename(buffer);    
    std::string info_buf, filetrans_buf;
    if (!filetrans.SerializeToString(&filetrans_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::UPLOAD_MSG_ACK);
    info.set_info(filetrans_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    conn->send(info_buf);
}

// 下载网盘文件业务
void ChatService::DownLoad(const muduo::net::TcpConnectionPtr &conn, const std::string &buffer, muduo::Timestamp time)
{
    fixbuf::FileTrans filetrans;
    filetrans.set_ip(FileServer::GetInstance()->GetIp());
    filetrans.set_port(FileServer::GetInstance()->GetPort());
    int idx = buffer.find(':');
    if (idx == -1) return;
    filetrans.set_filename(buffer.substr(0, idx));   
    filetrans.set_newfilename(buffer.substr(idx + 1)); 
    std::string info_buf, filetrans_buf;
    if (!filetrans.SerializeToString(&filetrans_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::DOWNLOAD_MSG_ACK);
    info.set_info(filetrans_buf);
    if (!info.SerializeToString(&info_buf))
    {
        LOG_ERROR << "serialize error!";
        return;
    }
    conn->send(info_buf);
}

// 异常结束，重置用户的状态信息
void ChatService::Reset()
{
    m_usermodel.ResetState();
}

// 客户端的异常退出
void ChatService::ClientCloseExecpt(const muduo::net::TcpConnectionPtr &conn)
{
    fixbuf::User user;
    user.set_id(-1);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_userConnMap.begin(); it != m_userConnMap.end(); ++ it)
        {
            if (it->second == conn)
            {
                // 从map表中删除用户的连接信息
                m_userConnMap.erase(it);
                user.set_id(it->first);
                break;
            }
        }
    }

    // 用户异常结束，此时用户下线，在redis中取消订阅
    m_redis.UnSubscribe(user.id());

    // 更新用户的状态信息
    if (user.id() != -1)
    {
        user.set_state(false);
        m_usermodel.UpdateState(user);
    }
}

// 从redis消息队列中获取订阅消息
void ChatService::HandleRedisSubscribeMsg(int userid, std::string msg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_userConnMap.find(userid);
    if (it != m_userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储用户离线消息
    m_offlineMsgModel.Insert(userid, msg);
}