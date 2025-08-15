#include "order_client.h"
#include "message_builder.h"
#include <iostream>
#include <iomanip>

OrderClient::OrderClient(const std::string& host, uint16_t port)
    : TcpClient(host, port) {
}

bool OrderClient::sendOrder(const OrderMessage& order) {
    if (state != ConnectionState::CONNECTED) {
        std::cerr << "Cannot send order: not connected" << std::endl;
        return false;
    }
    
    uint8_t buffer[OrderMessage::SIZE];
    size_t size = MessageBuilder::buildOrder(buffer, sizeof(buffer), order);
    
    if (size == 0) {
        return false;
    }
    
    ssize_t sent = send(buffer, size);
    
    if (sent == static_cast<ssize_t>(size)) {
        std::cout << "Order sent: " 
                  << (order.side == 'B' ? "BUY" : "SELL")
                  << " " << order.quantity
                  << " @ $" << std::fixed << std::setprecision(2)
                  << (order.price / 100.0) << std::endl;
        return true;
    }
    
    if (sent < 0 && errno == EAGAIN) {
        std::lock_guard<std::mutex> lock(queueMutex);
        sendQueue.push(std::vector<uint8_t>(buffer, buffer + size));
        return true;
    }
    
    return false;
}

void OrderClient::processSendQueue() {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    while (!sendQueue.empty() && state == ConnectionState::CONNECTED) {
        auto& data = sendQueue.front();
        ssize_t sent = send(data.data(), data.size());
        
        if (sent == static_cast<ssize_t>(data.size())) {
            sendQueue.pop();
        } else if (sent < 0 && errno != EAGAIN) {
            while (!sendQueue.empty()) {
                sendQueue.pop();
            }
            break;
        } else {
            break;
        }
    }
}