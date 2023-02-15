#include "json.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <unordered_map>
#include <functional>
using namespace std;
using json = nlohmann::json;

#include "user.hpp"
#include "group.hpp"
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 接收线程
void readTaskHandler(int fd);
// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
// 聊天页面程序
void mainMenu(int fd);
// 是否显示登录页面
bool isMainMenuing = false;

void help(int fd = -1, string str = "");
void chat(int fd = -1, string str = "");
void addfriend(int fd = -1, string str = "");
void creategroup(int fd = -1, string str = "");
void addgroup(int fd = -1, string str = "");
void groupchat(int fd = -1, string str = "");
void loginout(int fd = -1, string str = "");
void friendlist(int fd = -1, string str = "");
void grouplist(int fd = -1, string str = "");

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式:addfriend:friendid"},
    {"creategroup", "创建群组，格式:creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式:addgroup:groupid"},
    {"groupchat", "群聊，格式:groupchat:groupid:message"},
    {"loginout", "注销，格式:loginout"},
    {"friendlist", "查看好友列表，格式:friendlist"},
    {"grouplist", "查看群组列表，格式:grouplist"}};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout},
    {"friendlist", friendlist},
    {"grouplist", grouplist}};

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和端口
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip + port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)) == -1)
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // main线程用于接收用户输入，负责发送数据
    while (1)
    {
        // 显示首页菜单 登录、注册、退出
        cout << "=================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1:
        { // login业务
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg errno:" << request << endl;
            }
            else
            {
                char buf[1024] = {0};
                len = recv(clientfd, buf, 1024, 0);
                if (len == -1)
                {
                    cerr << "recv login response errno" << endl;
                }
                else
                {
                    json responsejs = json::parse(buf);
                    if (responsejs["errno"].get<int>() != 0) // 登录失败
                    {
                        cout << responsejs["errmsg"].get<string>() << endl;
                    }
                    else //  登录成功
                    {
                        isMainMenuing = true;

                        g_currentUser.setId(id);
                        g_currentUser.setName(responsejs["name"].get<string>());
                        g_currentUser.setState("online");

                        // 记录该用户好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();

                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json friendjs = json::parse(str);
                                User user;
                                user.setId(friendjs["id"].get<int>());
                                user.setName(friendjs["name"]);
                                user.setState(friendjs["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }
                        // 记录该用户组群信息
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> vec = responsejs["groups"];
                            for (string &str : vec)
                            {
                                json groupjs = json::parse(str);

                                Group group;
                                group.setId(groupjs["id"].get<int>());
                                group.setName(groupjs["groupname"]);
                                group.setDesc(groupjs["groupdesc"]);

                                vector<string> vec2 = groupjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json groupuserjs = json::parse(userstr);
                                    user.setId(groupuserjs["id"].get<int>());
                                    user.setName(groupuserjs["name"]);
                                    user.setState(groupuserjs["state"]);
                                    user.setRole(groupuserjs["role"]);
                                    group.getUser().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        // 显示当前登录成功用户的基本信息
                        showCurrentUserData();

                        // 显示当前登录成功用户的离线消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> offmsgVec = responsejs["offlinemsg"];
                            for (string &msgstr : offmsgVec)
                            {
                                json offmsgjs = json::parse(msgstr);
                                if (offmsgjs["msgid"].get<int>() == ONE_CHAT_MSG)
                                {
                                    cout << offmsgjs["time"].get<string>() << " [" << offmsgjs["id"].get<int>() << "] " << offmsgjs["name"].get<string>() << " said: " << offmsgjs["msg"].get<string>() << endl;
                                }
                                else
                                {
                                    cout << "群消息[" << offmsgjs["groupid"].get<int>() << "] " << offmsgjs["time"].get<string>() << " [" << offmsgjs["id"].get<int>() << "] " << offmsgjs["name"].get<string>() << " said: " << offmsgjs["msg"].get<string>() << endl;
                                }
                            }
                        }
                        static int readTaskNum = 0;
                        // 登录成功，启动接收线程负责接收数据，只用启动一次
                        if (readTaskNum == 0)
                        {
                            std::thread readTask(readTaskHandler, clientfd);
                            readTask.detach();
                        }

                        // 进入聊天主菜单界面
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2:
        { // register业务
            char name[50], pwd[50];
            cout << "please input name:";
            cin.getline(name, 50);
            cout << "please input password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string respest = js.dump();

            int len = send(clientfd, respest.c_str(), strlen(respest.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send register msg errno" << endl;
            }
            else
            {
                char buf[1024] = {0};
                len = recv(clientfd, buf, 1024, 0);
                if (len == -1)
                {
                    cerr << "recv register response errno" << endl;
                }
                else
                {
                    json responsejs = json::parse(buf);

                    if (responsejs["errno"].get<int>())
                    {
                        cout << name << " is already exist, register errno!" << endl;
                    }
                    else
                    {
                        cout << name << " register success, userid is " << responsejs["id"] << ", do not forget it!" << endl;
                    }
                }
            }
        }
        break;
        case 3: // 退出
            close(clientfd);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user============================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "----------------------friend list---------------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << "       " << user.getName() << "        " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << "      " << group.getName() << "       " << group.getDesc() << endl;
            for (GroupUser &user : group.getUser())
            {
                cout << "   " << user.getId() << "  " << user.getName() << "    " << user.getState() << "   " << user.getRole() << endl;
            }
        }
    }
    cout << "============================================================" << endl;
}

// 接收线程
void readTaskHandler(int fd)
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

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buf);
        int msgtype = js["msgid"].get<int>();
        if (msgtype == ONE_CHAT_MSG)
        {
            cout << js["time"].get<string>() << " [" << js["id"].get<int>() << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        if (msgtype == GROUP_CHAT_MSG)
        {
            cout << "群消息[" << js["groupid"].get<int>() << "] " << js["time"].get<string>() << " [" << js["id"].get<int>() << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        if (msgtype == CREATE_GROUP_MSG_ACK)
        {
            if (js["errno"].get<int>() == 0)
            {
                cout << "创建的群组号为：" << js["groupid"].get<int>() << "，不要忘记它!" << endl;
            }
            else
            {
                cout << "群组创建失败!" << endl;
            }
            continue;
        }
    }
}

// 主聊天页面
void mainMenu(int fd)
{
    help();

    char buf[1024] = {0};
    while (isMainMenuing)
    {
        cin.getline(buf, 1024);
        string commandBuf(buf);
        string command;
        int idx = commandBuf.find(":");
        if (idx == -1)
        {
            command = commandBuf;
        }
        else
        {
            command = commandBuf.substr(0, idx);
            commandBuf = commandBuf.substr(idx + 1);
        }

        auto it = commandMap.find(command);
        if (it == commandMap.end())
        {
            cerr << "invaild input command!" << endl;
            continue;
        }
        commandHandlerMap[command](fd, commandBuf);
    }
}

void help(int fd, string str)
{
    cout << "show command list" << endl;
    for (auto it : commandMap)
    {
        cout << it.first << "  :  " << it.second << endl;
    }
    cout << endl;
}

// friendid:msg
void chat(int fd, string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int id = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["time"] = getCurrentTime();
    js["to"] = id;
    js["msg"] = msg;
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

// friendid
void addfriend(int fd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    cout << "addfriend  :   " << buffer << endl;

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send add friend error -> " << buffer << endl;
    }
}

// groupname:groupdesc
void creategroup(int fd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        cerr << "creategroup command invaild!" << endl;
        return;
    }
    string name = str.substr(0, idx);
    string desc = str.substr(idx + 1);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = name;
    js["groupdesc"] = desc;
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send create group error -> " << buffer << endl;
    }
}

// groupid
void addgroup(int fd, string str)
{
    int id = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = id;
    string buffer = js.dump();
    cout << "addgroup  :   " << buffer << endl;

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send add group error -> " << buffer << endl;
    }
}

// groupid:msg
void groupchat(int fd, string str)
{
    int idx = str.find(':');
    if (idx == -1)
    {
        cerr << "groupchat command invaild" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["time"] = getCurrentTime();
    js["groupid"] = groupid;
    js["msg"] = msg;
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send group chat error -> " << buffer << endl;
    }
}

void loginout(int fd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "send loginout error -> " << buffer << endl;
    }
    else
    {
        isMainMenuing = false;
    }
}

/*
不是实时更新的，只是登录时的状态，后面看看能不能可以实现实时更新
*/
void friendlist(int fd, string str)
{
    cout << "friendlist" << endl;
    for (auto &user : g_currentUserFriendList)
    {
        cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
    }
    cout << endl;
}

/*
不是实时更新的，只是登录时的状态，后面看看能不能可以实现实时更新
*/
void grouplist(int fd, string str)
{
    cout << "groulist" << endl;
    for (auto &group : g_currentUserGroupList)
    {
        cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
        for (auto &user : group.getUser())
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
        }
        cout << endl;
    }
    cout << endl;
}

// 获取系统时间
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char data[60] = {0};
    sprintf(data, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_yday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(data);
}
