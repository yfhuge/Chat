#include "redis.hpp"

Redis::Redis() : m_publishContext(nullptr), m_subscribeContext(nullptr)
{ }

Redis::~Redis()
{
    if (m_publishContext != nullptr)
    {
        redisFree(m_publishContext);
    }
    if (m_subscribeContext != nullptr)
    {
        redisFree(m_subscribeContext);
    }
}

// 连接redis服务器
bool Redis::Connect()
{
    // 负责publish发布上下文消息的连接
    m_publishContext = redisConnect("127.0.0.1", 6379);
    if (m_publishContext == nullptr)
    {
        std::cerr << "connect redis error!" << std::endl;
        return false;
    }

    // 负责subscribe订阅上下文消息的连接
    m_subscribeContext = redisConnect("127.0.0.1", 6379);
    if (m_subscribeContext == nullptr)
    {
        std::cerr << "connect redis error!" << std::endl;
        return false;
    }

    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    std::thread t([&](){
        ObserverChannelMsg();
    });
    t.detach();
    std::cout << "connect redis_server success!" << std::endl;
    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::Publish(int channel, std::string message)
{
    redisReply *reply = (redisReply*)redisCommand(m_publishContext, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        std::cerr << "publish command failed" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道channel订阅消息
bool Redis::Subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅，不接受通道消息
    // 通道消息的接收专门在ObserverMsgHandler函数的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则会和notifyMsg线程抢占资源
    if (redisAppendCommand(m_subscribeContext, "SUBSCRIBE %d", channel) == REDIS_ERR)
    {
        std::cerr << "subscribe command failed!" << std::endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被设置为1）
    int done = 0;
    while (!done)
    {
        if (redisBufferWrite(m_subscribeContext, &done) == REDIS_ERR)
        {
            std::cerr << "subscribe command failed!" << std::endl;
            return false;
        }
    }

    return true;
}

// 向redis指定的通道unsubscribe消息
bool Redis::UnSubscribe(int channel)
{
    if (redisAppendCommand(m_subscribeContext, "UNSUBSCRIBE %d", channel) == REDIS_ERR)
    {
        std::cerr << "unsubscribe command failed" << std::endl;
        return false;
    }

    int done = 0;
    while (!done)
    {
        if (redisBufferWrite(m_subscribeContext, &done) == REDIS_ERR)
        {
            std::cerr << "unsubscribe command failed" << std::endl;
            return false;
        }
    }

    return true;
}

// 在独立线程中接收订阅通道的消息
void Redis::ObserverChannelMsg()
{
    redisReply *reply = nullptr;
    while (redisGetReply(m_subscribeContext, (void**)&reply) == REDIS_OK)
    {
        // 订阅收到的消息是一个带三元素的数组 即"message"、"订阅的通道号"、"真正的消息"
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发送消息
            m_notifyMsgHandler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }

    std::cerr << ">>>>>>>>>>>>> observer_channel_message quit <<<<<<<<<<<<<" << std::endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::InitNotifyHandler(std::function<void(int, std::string)> fn)
{
    m_notifyMsgHandler = fn;
}