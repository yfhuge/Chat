#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

// 服务器ctrl c异常结束后，重置user的状态信息
void sigHandle(int)
{
    Chatservice::instance()->reset();
    exit(0);
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        cerr << "input invaild, example ./CHatServer 127.0.0.1 6000" << endl;
        exit(0);
    }

    string ip = argv[1];
    int port = atoi(argv[2]);

    signal(SIGINT, sigHandle);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServe");

    server.start();
    loop.loop();

    return 0;
}