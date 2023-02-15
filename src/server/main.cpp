#include "chatserver.hpp"
#include "chatservice.hpp"
#include <iostream>
#include <signal.h>

// 服务器ctrl c异常结束后，重置user的状态信息
void sigHandle(int) {
    Chatservice::instance()->reset();
    exit(0);
}

int main()
{
    signal(SIGINT, sigHandle);

    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServe");

    server.start();
    loop.loop();

    return 0;
}