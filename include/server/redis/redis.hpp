#pragma once

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <iostream>

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool Connect();

    // 向redis指定的通道channel发布消息
    bool Publish(int channel, std::string message);

    // 向redis指定的通道channel订阅消息
    bool Subscribe(int channel);

    // 向redis指定的通道unsubscribe消息
    bool UnSubscribe(int channel);

    // 在独立线程中接收订阅通道的消息
    void ObserverChannelMsg();

    // 初始化向业务层上报通道消息的回调对象
    void InitNotifyHandler(std::function<void(int, std::string)> fn);

private:
    // hiredis同步上下文对象，负责publish消息
    redisContext *m_publishContext;
    
    // hiredis同步上下文对象，负责subscribe消息
    redisContext *m_subscribeContext;

    // 回调操作，收到订阅的消息，给service成上报
    std::function<void(int, std::string)> m_notifyMsgHandler;
};