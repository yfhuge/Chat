#pragma once

#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
#include "ChatService.hpp"

class ChatServer
{
public:
    ChatServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listenAddr,
            const std::string& nameArg);
    void start();
private:

    void OnConnect(const muduo::net::TcpConnectionPtr&);
    void OnMessage(const muduo::net::TcpConnectionPtr&, 
            muduo::net::Buffer *buffer, 
            muduo::Timestamp time);

    muduo::net::TcpServer m_server;
    muduo::net::EventLoop *m_loop;
};