#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <string>
#include <netinet/in.h>
#include <iostream>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <thread>


void EpollCtl(int fd, int epollfd, int opt = 0);

class FileServer
{
public:
    static const int MAX_EPOLL_SIZE = 1024;
public:
    // 单例模式
    static FileServer *GetInstance();
    // 设置IP和端口
    void Init(const std::string &ip, int port);
    // 启动
    void Start();
    // 接收文件的线程
    static void RecvFileTask(int fd);
    // 获取IP
    std::string GetIp();
    // 获取port
    int GetPort();
private:
    FileServer();

    int m_fd;   // 通信的socket文件描述符
    std::string m_ip;   // IP
    int m_port; // 端口
    int m_epollfd;   // epoll IO多路复用的文件描述符
    struct epoll_event m_events[MAX_EPOLL_SIZE];    // epoll中就绪链表
};