#ifndef ORDER_CLIENT_H
#define ORDER_CLIENT_H

#include "tcp_client.h"
#include "message.h"
#include <queue>
#include <vector>
#include <mutex>

class OrderClient : public TcpClient {
private:
    std::queue<std::vector<uint8_t>> sendQueue;
    std::mutex queueMutex;
    
public:
    OrderClient(const std::string& host, uint16_t port);
    
    bool sendOrder(const OrderMessage& order);
    void processSendQueue();
};

#endif // ORDER_CLIENT_H