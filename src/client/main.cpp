#include "public.pb.h"
#include <unordered_map>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <assert.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <thread>
#include <semaphore.h>
#include <atomic>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

// 记录当前系统登录的用户信息
fixbuf::User m_currentUser;
// 记录当前登录用户的好友消息
std::vector<fixbuf::User> m_currentUserFriendList;
// 记录当前登录用户的群组消息
std::vector<fixbuf::Group> m_currentGroupList;
// 是否显示登录界面
bool isMainMenuing = false;
// 读写之间的通信
sem_t m_sem;
// 登录的状态
std::atomic_bool isLoginSuccess(false);

// 显示当前登录成功用户的基本信息
void ShowCurrentUserData();
// 接收线程
void ReadTaskHandler(int fd);
// 获取系统时间（聊天信息需添加时间信息）
std::string GetCurrentTime();
// 聊天页面程序
void MainMenu(int fd);
// 注册的用户名
char name[50] = {0};    
// 注册的用户密码
char pwd[50] = {0};
// 文件已接收的长度和上次接收的长度
off_t recvsize = 0, lastsize = 0;
// 关于一次性接收n个字节的接收函数和发送函数
int SendN(int fd, void *p, off_t bufLen);
int RecvN(int fd, void *p, off_t bufLen);

void help(int fd = -1, std::string str = "");
void chat(int fd = -1, std::string str = "");
void addfriend(int fd = -1, std::string str = "");
void creategroup(int fd = -1, std::string str = "");
void addgroup(int fd = -1, std::string str = "");
void groupchat(int fd = -1, std::string str = "");
void friendlist(int fd = -1, std::string str = "");
void grouplist(int fd = -1, std::string str = "");
void loginout(int fd = -1, std::string str = "");
void ls(int fd, std::string str = "");
void del(int fd, std::string str = "");
void download(int fd, std::string str = "");
void upload(int fd, std::string str = "");
void login(std::string str = "");
void reg(std::string str = "");

// 系统支持的客户端命令列表
std::unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式:addfriend:friendid"},
    {"creategroup", "创建群组，格式:creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式:addgroup:groupid"},
    {"groupchat", "群聊，格式:groupchat:groupid:message"},
    {"loginout", "注销，格式:loginout"},
    {"friendlist", "查看好友列表，格式:friendlist"},
    {"grouplist", "查看群组列表，格式:grouplist"},
    {"ls", "查看当前网盘的文件目录，格式:ls"},
    {"del", "删除指定的网盘的文件，格式:del:filename"},
    {"download", "下载网盘中指定的文件，格式:download:filename:newfilename"},
    {"upload", "将本地文件上传到网盘中，格式:upload:filename"}
};

// 注册系统支持的客户端命令处理
std::unordered_map<std::string, std::function<void(int, std::string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout},
    {"friendlist", friendlist},
    {"grouplist", grouplist},
    {"ls", ls},
    {"del", del},
    {"download", download},
    {"upload", upload}};

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "format:./ChatClient IP SERVERPORT" << std::endl;
        exit(-1);
    }

    std::string ip = argv[1];
    int port = atoi(argv[2]);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd != -1);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    int ret = connect(fd, (struct sockaddr*)&address, sizeof(address));
    if (ret == -1)
    {
        std::cerr << "connect error" << std::endl;
        close(fd);
        exit(-1);
    }

    sem_init(&m_sem, 0, 0);
    std::thread readTask(ReadTaskHandler, fd);
    readTask.detach();

    while (1)
    {
        // 显示首页菜单 登录 注册 退出
        std::cout << "===============" << std::endl;
        std::cout << "1.  login" << std::endl;
        std::cout << "2.  register" << std::endl;
        std::cout << "3.  quit" << std::endl;
        std::cout << "===============" << std::endl;
        std::cout << "choice:";
        int choice;
        std::cin >> choice;
        std::cin.get(); // 读取缓冲区残留的回车

        switch (choice)
        {
        case 1:
        {
            int id;
            char pwd[20] = {0};
            std::cout << "userid:";
            std::cin >> id;
            std::cin.get();
            std::cout << "userpassword:";
            std::cin.getline(pwd, 20);

            fixbuf::Info info;
            info.set_type(fixbuf::LOGIN_MSG);
            fixbuf::User user;
            user.set_id(id);
            user.set_password(pwd);
            std::string user_buf, info_buf;
            if (!user.SerializeToString(&user_buf))
            {       
                std::cerr << "serialize failed" << std::endl;
                break;
            }
            info.set_info(user_buf);
            if (!info.SerializeToString(&info_buf))
            {
                std::cerr << "serialize failed" << std::endl;
                break;
            }
            int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
            if (len == -1)
            {
                std::cerr << "send login msg errnor, connect:" << info_buf << std::endl; 
            }

            // 等待子进程处理完登录的响应消息
            sem_wait(&m_sem);

            if (isLoginSuccess)
            {
                isMainMenuing = true;
                MainMenu(fd);
            }
        }
        break;
        case 2:
        {

            std::cout << "please input name:";
            std::cin.getline(name, 50);
            std::cout << "please input password:";
            std::cin.getline(pwd, 50);

            fixbuf::Info info;
            fixbuf::User user;
            user.set_name(name);
            user.set_password(pwd);
            std::string user_buf, info_buf;
            if (!user.SerializeToString(&user_buf))
            {
                std::cerr << "serialize failed" << std::endl;
                break;
            }
            info.set_type(fixbuf::REGISTER_MSG);
            info.set_info(user_buf);
            if (!info.SerializeToString(&info_buf))
            {
                std::cerr << "serialize failed" << std::endl;
                break;
            }
            int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
            if (len == -1)
            {
                std::cerr << "send register msg error, context:" << info_buf << std::endl;
            }

            sem_wait(&m_sem);
        }
        break;
        case 3:
            close(fd);
            exit(0);
        default:
            std::cerr << "invalid input!" << std::endl;
            break;
        }
    }

    close(fd);
    return 0;
}


void help(int fd, std::string str)
{
    std::cout << std::endl << "show command list" << std::endl;
    for (auto it : commandMap)
    {
        std::cout << it.first << "  :  " << it.second << std::endl;
    }
    std::cout << std::endl;
}

void chat(int fd, std::string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        std::cerr << "chat command invalid!" << std::endl;
        return;
    }

    int id = atoi(str.substr(0, idx).c_str());
    std::string msg = str.substr(idx + 1);
    fixbuf::ToUserMsg send_msg;
    send_msg.set_userid(m_currentUser.id());
    send_msg.set_touserid(id);
    send_msg.set_sendtime(GetCurrentTime());
    send_msg.set_username(m_currentUser.name());
    send_msg.set_msg(msg);
    std::string msg_buf, info_buf;
    if (!send_msg.SerializeToString(&msg_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::ONE_CHAT);
    info.set_info(msg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void addfriend(int fd, std::string str)
{
    int id = atoi(str.c_str());
    fixbuf::Info info;
    info.set_type(fixbuf::ADD_FRIEND_MSG);
    fixbuf::Friend addfriend;
    addfriend.set_friendid(id);
    addfriend.set_userid(m_currentUser.id());
    std::string addfriend_buf, info_buf;
    if (!addfriend.SerializeToString(&addfriend_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }
    info.set_info(addfriend_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }
    
    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf.c_str() << std::endl;
    }
}

void creategroup(int fd, std::string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        std::cerr << "command creategroup invaild!" << std::endl;
        return;
    }
    std::string groupname = str.substr(0, idx);
    std::string groupdesc = str.substr(idx + 1);
    std::string group_buf, info_buf;

    fixbuf::CreateGroup crtgroup;
    crtgroup.set_userid(m_currentUser.id());
    auto *group = crtgroup.mutable_group();
    group->set_name(groupname);
    group->set_desc(groupdesc);
    if (!crtgroup.SerializeToString(&group_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::CREATE_GROUP_MSG);
    info.set_info(group_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void addgroup(int fd, std::string str)
{
    int id = atoi(str.c_str());
    fixbuf::AddGroup addgroupmsg;
    addgroupmsg.set_userid(m_currentUser.id());
    addgroupmsg.set_groupid(id);
    std::string addgroup_buf, info_buf;
    if (!addgroupmsg.SerializeToString(&addgroup_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::ADD_GROUP_MSG);
    info.set_info(addgroup_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void groupchat(int fd, std::string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        std::cerr << "command groupchat invaild!" << std::endl;
        return;
    }
    int id = atoi(str.substr(0, idx).c_str());
    std::string msg = str.substr(idx + 1);

    fixbuf::GroupMsg groupmsg;
    groupmsg.set_userid(m_currentUser.id());
    groupmsg.set_groupid(id);
    groupmsg.set_msg(msg);
    groupmsg.set_sendtime(GetCurrentTime());
    groupmsg.set_username(m_currentUser.name());
    std::string groupmsg_buf, info_buf;
    if (!groupmsg.SerializeToString(&groupmsg_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::GROUP_CHAT);
    info.set_info(groupmsg_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void friendlist(int fd, std::string str)
{
    std::cout << "friendlist" << std::endl;
    for (auto &user : m_currentUserFriendList)
    {
        std::cout << user.id() << " " << user.name() << " " << (user.state()?"online":"offline") << std::endl;
    }
    std::cout << std::endl;
}

void grouplist(int fd, std::string str)
{
    std::cout << "grouplist" << std::endl;
    for (auto &group : m_currentGroupList)
    {
        std::cout << group.id() << " " << group.name() << " " << group.desc() << std::endl;
        auto *users = group.mutable_groupusers();
        for (int i = 0; i < users->size(); ++ i)
        {
            std::cout << "\r" << users->at(i).user().id() << " " << users->at(i).user().name() << " " << (users->at(i).user().state()?"online":"offline") << " " << users->at(i).role() << std::endl; 
        }
        std::cout << std::endl;
    }
}

void loginout(int fd, std::string str)
{
    fixbuf::Info info;
    info.set_type(fixbuf::LOGOUT_MSG);
    info.set_info(std::to_string(m_currentUser.id()));
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
    else 
    {
        isMainMenuing = false;
    }
}

void ls(int fd, std::string str)
{
    fixbuf::Info info;
    info.set_type(fixbuf::LIST_MSG);
    info.set_info(std::to_string(m_currentUser.id()));
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void del(int fd, std::string str)
{
    fixbuf::FileList filelist;
    filelist.set_userid(m_currentUser.id());
    filelist.add_filelist(str);
    std::string filelist_buf, info_buf;
    if (!filelist.SerializeToString(&filelist_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::DELETEFILE_MSG);
    info.set_info(filelist_buf);
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error!" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void download(int fd, std::string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        std::cerr << "command groupchat invaild!" << std::endl;
        return;
    }
    fixbuf::Info info;
    info.set_type(fixbuf::DOWNLOAD_MSG);
    info.set_info(str);
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void upload(int fd, std::string str)
{
    fixbuf::Info info;
    info.set_type(fixbuf::UPLOAD_MSG);
    info.set_info(str);
    std::string info_buf;
    if (!info.SerializeToString(&info_buf))
    {
        std::cerr << "serialize error" << std::endl;
        return;
    }

    int len = send(fd, info_buf.c_str(), info_buf.size(), 0);
    if (len == -1)
    {
        std::cerr << "send msg error, msg : " << info_buf << std::endl;
    }
}

void login(std::string str)
{
    fixbuf::ErrorMsg errmsg;
    if (!errmsg.ParseFromString(str))
    {
        std::cerr << "parse error, context:" << str << std::endl;
        return;
    }
    if (errmsg.errcode())
    {
        // 登录失败
        std::cerr << errmsg.errmsg() << std::endl;
        isLoginSuccess = false;
    }
    else 
    {
        // 登录成功
        fixbuf::LoginMsg loginmsg;
        if (!loginmsg.ParseFromString(errmsg.errmsg()))
        {
            std::cerr << "parse error, context:" << errmsg.errmsg();
            return;
        }

        m_currentUser = loginmsg.user();
        
        // 用户的好友列表
        auto *users =  loginmsg.mutable_friendlist();
        for (int i = 0; i < users->size(); ++ i)
        {
            m_currentUserFriendList.push_back(users->at(i));
        }

        // 用户的群组列表
        auto *groups = loginmsg.mutable_grouplist();
        for (int i = 0; i < groups->size(); ++ i)
        {
            m_currentGroupList.push_back(groups->at(i));
        }

        // 显示当前登录成功用户的基本信息
        ShowCurrentUserData();

        // 显示当前登录成功用户的离线消息
        auto *recvmsg = loginmsg.mutable_recvmsg();
        for (int i = 0; i < recvmsg->size(); ++ i)
        {
            std::cout << recvmsg->at(i) << std::endl;
        }

        isLoginSuccess = true;
    }
}

void reg(std::string str)
{
    fixbuf::ErrorMsg recv_msg;
    if (!recv_msg.ParseFromString(str))
    {
        std::cerr << "parse failed, context:" << str << std::endl;
        return;
    }
    if (recv_msg.errcode())
    {
        std::cout << name << " is already exist, register error!" << std::endl;
    }
    else 
    {
        fixbuf::User user;
        if (!user.ParseFromString(recv_msg.errmsg()))
        {
            std::cerr << "parse failed, context:" << recv_msg.errmsg() << std::endl;
        }
        else 
        {
            std::cout << name << " register success, userid is " << user.id() << ", do not forget it!" << std::endl;
        }
    }
}

// 显示当前登录成功用户的基本信息
void ShowCurrentUserData()
{
    std::cout << "===========================login user===========================" << std::endl;
    std::cout << "id : " << m_currentUser.id() << std::endl;
    std::cout << "name : " << m_currentUser.name() << std::endl;
    std::cout << std::endl;
    std::cout << "---------------------------friend list--------------------------" << std::endl;
    for (auto user : m_currentUserFriendList)
    {
        std::cout << user.id() << "         " << user.name() << std::endl;
    }
    std::cout << std::endl;
    std::cout << "---------------------------group list---------------------------" << std::endl;
    for (auto &group : m_currentGroupList)
    {
        std::cout << group.id() << "    " << group.name() << "      " << group.desc() << std::endl;
        auto *users = group.mutable_groupusers();
        for (int i = 0; i < users->size(); ++ i)
        {
            std::cout << "      " << users->at(i).user().id() << "      " << users->at(i).user().name() << "      " << (users->at(i).user().state()?"online":"offline") << "      " << users->at(i).role() << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::endl;
}

// 接收线程
void ReadTaskHandler(int fd)
{
    while (1)
    {
        char buf[1024] = {0};
        int len = recv(fd, buf, 1024, 0);
        if (len == -1 || len == 0)
        {
            close(fd);
            exit(-1);
        }
        fixbuf::Info info;
        if (!info.ParseFromString(buf))
        {
            std::cerr << "parse error, context:" << buf << std::endl;
            exit(-1);
        }
        if (info.type() == fixbuf::REGISTER_MSG_ACK)
        {
            // 注册的响应消息
            reg(info.info());
            sem_post(&m_sem);
        }
        else if (info.type() == fixbuf::ADD_FRIEND_MSG_ACK) 
        {
            // 添加好友的响应消息
            fixbuf::ErrorMsg errmsg;
            if (!errmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            if (errmsg.errcode())
            {
                // 添加好友失败
                std::cerr << errmsg.errmsg() << std::endl;
            }
            else 
            {
                std::cout << "Added friend successfully, come and chat with him" << std::endl;
            }
        }
        else if (info.type() == fixbuf::ADD_GROUP_MSG_ACK)
        {
            // 加入群组的响应消息
            fixbuf::ErrorMsg errmsg;
            if (!errmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            if (errmsg.errcode())
            {
                // 添加好友失败
                std::cerr << errmsg.errmsg() << std::endl;
            }
            else 
            {
                std::cout << errmsg.errmsg() << std::endl;
            }
        }
        else if (info.type() == fixbuf::CREATE_GROUP_MSG_ACK)
        {
            // 创建群组的响应消息
            fixbuf::ErrorMsg errmsg;
            if (!errmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            if (errmsg.errcode())
            {
                // 创建群组失败
                std::cerr << "The group name you want to create already exists, please create a new group" << std::endl;
            }
            else 
            {
                int id = atoi(errmsg.errmsg().c_str());
                std::cout << "group create success, groupid is " << id << std::endl;
            }
        }
        else if (info.type() == fixbuf::LOGIN_MSG_ACK)
        {
            // 登录成功的响应消息
            login(info.info());
            sem_post(&m_sem);
        }
        else if (info.type() == fixbuf::ONE_CHAT)
        {
            // 一对一聊天的响应消息
            fixbuf::FromMsg recvmsg;
            if (!recvmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            std::cout << recvmsg.sendtime() << " [" << recvmsg.fromid() << "]" << recvmsg.fromname() << " said: " << recvmsg.msg() << std::endl;
        }
        else if (info.type() == fixbuf::GROUP_CHAT)
        {
            // 群聊的响应消息
            fixbuf::GroupMsg groupmsg;
            if (!groupmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            std::cout << "群消息[" << groupmsg.groupid() << "] " << groupmsg.sendtime() << " [" << groupmsg.userid() << "] " << groupmsg.username() << " said: " << groupmsg.msg() << std::endl;
        }
        else if (info.type() == fixbuf::LIST_MSG_ACK)
        {
            // 查看网盘文件的响应消息
            fixbuf::FileList filelist;
            if (!filelist.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            std::cout << "file list : " << std::endl;
            for (int i = 0; i < filelist.mutable_filelist()->size(); ++ i)
            {
                std::cout << filelist.mutable_filelist()->at(i) << " ";
            }
            std::cout << std::endl;
        }
        else if (info.type() == fixbuf::DELETEFILE_MSG_ACK)
        {
            // 删除网盘中文件的响应消息
            fixbuf::ErrorMsg errmsg;
            if (!errmsg.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }
            if (errmsg.errcode())
            {
                std::cerr << errmsg.errmsg() << std::endl;
            }
            else 
            {
                std::cout << errmsg.errmsg() << std::endl;
            }
        }
        else if (info.type() == fixbuf::UPLOAD_MSG_ACK)
        {
            // 上传网盘文件的响应消息
            fixbuf::FileTrans filetrans;
            if (!filetrans.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }

            // 获取文件传输服务器的IP和端口
            std::string ip = filetrans.ip();
            int port = filetrans.port();
            std::string filename = filetrans.filename();

            // 和文件传输服务器建立连接
            int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            assert(sock_fd != -1);
            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = inet_addr(ip.c_str());
            int ret = connect(sock_fd, (struct sockaddr*)&address, sizeof(address));
            assert(ret != -1);

            int newfd = open(filename.c_str(), O_RDONLY);
            if (newfd == -1)
            {
                std::cerr << "The uploaded file does not exist, please re-upload the existing file" << std::endl;
                close(sock_fd);
                continue;
            }

            // 发送消息表示发送文件
            char flag = 1;
            ret = send(sock_fd, &flag, sizeof(flag), 0);
            assert(ret != -1);

            // 发送用户名
            int userNameLen = m_currentUser.name().size();
            int len = send(sock_fd, &userNameLen, sizeof(userNameLen), 0);
            assert(len != -1);
            len = send(sock_fd, m_currentUser.name().c_str(), userNameLen, 0);
            assert(len != -1);

            // 发送文件名
            int idx = filename.find_last_of('/');
            std::string sendFileName;
            if (idx == -1)
            {
                sendFileName = filename;
            }
            else 
            {
                sendFileName = filename.substr(idx + 1);
            }
            int fileNameLen = sendFileName.size();
            len = send(sock_fd, &fileNameLen, sizeof(fileNameLen), 0);
            assert(len != -1);
            len = send(sock_fd, sendFileName.c_str(), sendFileName.size(), 0);
            assert(len != -1);

            // 发送文件长度
            struct stat fileStat;
            stat(filename.c_str(), &fileStat);
            len = send(sock_fd, &fileStat.st_size, sizeof(fileStat.st_size), 0);
            assert(len != -1);

            // 发送文件内容
            void *p = mmap(nullptr, fileStat.st_size, PROT_READ, MAP_SHARED, newfd, 0);
            assert(p != (void*)-1);
            ret = SendN(sock_fd, p, fileStat.st_size);
            assert(ret != -1);
            munmap(p, fileStat.st_size);
            printf("上传完成\n");
            close(sock_fd);
            close(newfd);
        }
        else if (info.type() == fixbuf::DOWNLOAD_MSG_ACK) 
        {
            // 下载网盘文件响应消息
            fixbuf::FileTrans filetrans;
            if (!filetrans.ParseFromString(info.info()))
            {
                std::cerr << "parse error, context:" << info.info() << std::endl;
                exit(-1);
            }

            // 获取文件传输服务器的IP和端口
            std::string ip = filetrans.ip();
            int port = filetrans.port();
            std::string filename = filetrans.filename();
            std::string newfilename = filetrans.newfilename();

            // 和文件传输服务器建立连接
            int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            assert(sock_fd != -1);
            struct sockaddr_in address;
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = inet_addr(ip.c_str());
            int ret = connect(sock_fd, (struct sockaddr*)&address, sizeof(address));
            assert(ret != -1);

            char cwd[64] = {0};
            getcwd(cwd, 64);
            std::string filecwd = std::string(cwd) + "/" + newfilename;
            int newfd = open(filecwd.c_str(), O_CREAT | O_RDWR, 0666);
            assert(newfd != -1);

            // 发送消息表示接收文件
            char flag = 2;
            ret = send(sock_fd, &flag, sizeof(flag), 0);
            assert(ret != -1);

            // 发送用户名
            int userNameLen = m_currentUser.name().size();
            int len = send(sock_fd, &userNameLen, sizeof(userNameLen), 0);
            assert(len != -1);
            len = send(sock_fd, m_currentUser.name().c_str(), userNameLen, 0);
            assert(len != -1);

            // 发送请求下载的文件名
            int fileLen = filename.size();
            len = send(sock_fd, &fileLen, sizeof(fileLen), 0);
            assert(len != -1);
            len = send(sock_fd, filename.c_str(), fileLen, 0);
            assert(len != -1);

            // 接收文件的长度
            off_t fileTotalLen;
            len = recv(sock_fd, &fileTotalLen, sizeof(fileTotalLen), 0);
            assert(len != -1);

            ret = ftruncate(newfd, fileTotalLen);
            assert(ret != -1);
            void *p = mmap(nullptr, fileTotalLen, PROT_READ | PROT_WRITE, MAP_SHARED, newfd, 0);
            assert(p != (void*)-1);
            ret = RecvN(sock_fd, p, fileTotalLen);
            assert(ret != -1);

            printf("下载完成\n");
            close(sock_fd);
            close(newfd);
        }
    }
}

// 获取系统时间（聊天信息需添加时间信息）
std::string GetCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char data[60] = {0};
    sprintf(data, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(data);
}

// 聊天页面程序
void MainMenu(int fd)
{
    help();

    char buf[1024] = {0};
    while (isMainMenuing)
    {
        std::cin.getline(buf, 1024);
        std::string command_buf(buf);
        std::string command;
        int idx = command_buf.find(':');
        if (idx == -1)
        {
            command = command_buf;
        }
        else 
        {
            command = command_buf.substr(0, idx);
            command_buf = command_buf.substr(idx + 1);
        }

        auto it = commandMap.find(command);
        if (it == commandMap.end())
        {
            std::cerr << "invaild input command!" << std::endl;
            continue;
        }
        commandHandlerMap[command](fd, command_buf);
    }
}

int SendN(int fd, void *buf, off_t bufLen)
{
    char *p = (char*)buf;
    off_t total = 0, sendLen = 0, lastLen = 0;
    int ret;
    while (total < bufLen)
    {
        ret = send(fd, p + total, bufLen - total, 0);
        if (ret == -1)  return -1;
        total += ret;
        sendLen += ret;
        if (sendLen - lastLen > bufLen / 10000)
        {
            lastLen = sendLen;
            printf("%6.2f%%\r", (double)sendLen * 100 / bufLen);
        }
    }
    return 0;
}

int RecvN(int fd, void *buf, off_t bufLen)
{
    char *p = (char*)buf;
    off_t total = 0, recvLen = 0, lastLen = 0;
    int ret;
    while (total < bufLen)
    {
        ret = recv(fd, p + total, bufLen - total, 0);
        if (ret == -1) return -1;
        total += ret;
        recvLen += ret;
        if (recvLen - lastLen > bufLen / 10000)
        {
            lastLen = recvLen;
            printf("%6.2f%%\r", (double)recvLen * 100 / bufLen);
        }
    }
    return 0;
}
