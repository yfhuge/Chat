#include "ChatServer.hpp"
#include "FileServer.hpp"
#include <signal.h>
#include <thread>

void SigHandle(int)
{
    ChatService::GetInstance()->Reset();
    exit(0);
}

void fileServer(std::string ip, int port)
{
    FileServer::GetInstance()->Init(ip, port);
    FileServer::GetInstance()->Start();
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "format:./ChatServer IP SERVERPORT FILEPORT" << std::endl;
        exit(-1);
    }

    std::string ip = argv[1];
    int serverPort = atoi(argv[2]);
    int filePort = atoi(argv[3]);

    signal(SIGINT, SigHandle);

    std::thread m_fileServer(fileServer, ip, filePort);
    muduo::net::EventLoop loop;
    muduo::net::InetAddress address(ip, serverPort);
    ChatServer server(&loop, address, "ChatServer");
    server.start();
    loop.loop();

    return 0;
}