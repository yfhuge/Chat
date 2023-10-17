#include "ChatServer.hpp"

ChatServer::ChatServer(muduo::net::EventLoop* loop,
            const muduo::net::InetAddress& listenAddr,
            const std::string& nameArg)
:m_server(loop, listenAddr, nameArg), m_loop(loop)
{
    m_server.setConnectionCallback(std::bind(&ChatServer::OnConnect, this, std::placeholders::_1));
    m_server.setMessageCallback(std::bind(&ChatServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    m_server.setThreadNum(4);
}

void ChatServer::start()
{
    m_server.start();
}

void ChatServer::OnConnect(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        ChatService::GetInstance()->ClientCloseExecpt(conn);
        conn->shutdown();
    }
}

void ChatServer::OnMessage(const muduo::net::TcpConnectionPtr &conn,
            muduo::net::Buffer *buffer,
            muduo::Timestamp time)
{
    std::string buf = buffer->retrieveAllAsString();
    fixbuf::Info buf_info;
    if (!buf_info.ParseFromString(buf))
    {
        // 序列化失败
        LOG_ERROR << "info parse error, context:" << buf;
        return;
    }
    // 获取消息的类型
    int type = buf_info.type();
    // 根据消息类型得到相应的业务处理函数
    auto msgHandler = ChatService::GetInstance()->GetHandler(type);
    // 进行业务处理
    msgHandler(conn, buf_info.info(), time);
}