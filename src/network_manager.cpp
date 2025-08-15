#include "network_manager.h"
#include "market_data_client.h"
#include "order_client.h"
#include <iostream>
#include <chrono>

NetworkManager::NetworkManager() 
    : marketClient(nullptr), orderClient(nullptr), running(false) {
}

NetworkManager::~NetworkManager() {
    stop();
    delete marketClient;
    delete orderClient;
}

bool NetworkManager::initialize(const Config& config) {
    marketClient = new MarketDataClient(config.marketDataHost, 
                                       config.marketDataPort);
    orderClient = new OrderClient(config.orderHost, 
                                 config.orderPort);
    
    marketClient->setMessageCallback(
        [this](const MessageHeader& header, const void* data) {
            handleMarketData(header, data);
        });
    
    if (!marketClient->connect()) {
        std::cerr << "Failed to connect to market data" << std::endl;
        return false;
    }
    
    if (!orderClient->connect()) {
        std::cerr << "Failed to connect to order server" << std::endl;
        return false;
    }
    
    running = true;
    return true;
}

void NetworkManager::processEvents() {
    if (!running) {
        return;
    }
    
    fd_set readSet, writeSet;
    struct timeval timeout;
    
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    
    int maxFd = -1;
    
    if (marketClient->isConnected()) {
        int fd = marketClient->getSocketFd();
        FD_SET(fd, &readSet);
        maxFd = std::max(maxFd, fd);
    } else {
        marketClient->reconnect();
    }
    
    if (orderClient->isConnected()) {
        int fd = orderClient->getSocketFd();
        FD_SET(fd, &writeSet);
        maxFd = std::max(maxFd, fd);
    } else {
        orderClient->reconnect();
    }
    
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    
    int activity = select(maxFd + 1, &readSet, &writeSet, nullptr, &timeout);
    
    if (activity < 0) {
        if (errno != EINTR) {
            std::cerr << "Select error: " << strerror(errno) << std::endl;
            return;
        }
        return;
    }
    
    if (marketClient->isConnected() && 
        FD_ISSET(marketClient->getSocketFd(), &readSet)) {
        marketClient->processIncomingData();
    }
    
    if (orderClient->isConnected() && 
        FD_ISSET(orderClient->getSocketFd(), &writeSet)) {
        orderClient->processSendQueue();
    }
    
    handlePeriodicTasks();
}

void NetworkManager::stop() {
    running = false;
    
    if (marketClient) {
        marketClient->disconnect();
    }
    
    if (orderClient) {
        orderClient->disconnect();
    }
}

void NetworkManager::setQuoteCallback(std::function<void(const QuoteMessage&)> cb) {
    quoteCallback = cb;
}

void NetworkManager::setTradeCallback(std::function<void(const TradeMessage&)> cb) {
    tradeCallback = cb;
}

bool NetworkManager::sendOrder(const OrderMessage& order) {
    return orderClient ? orderClient->sendOrder(order) : false;
}

void NetworkManager::handleMarketData(const MessageHeader& header, const void* data) {
    switch (header.type) {
        case MessageHeader::QUOTE_TYPE:
            if (quoteCallback) {
                quoteCallback(*static_cast<const QuoteMessage*>(data));
            }
            break;
            
        case MessageHeader::TRADE_TYPE:
            if (tradeCallback) {
                tradeCallback(*static_cast<const TradeMessage*>(data));
            }
            break;
    }
}

void NetworkManager::handlePeriodicTasks() {
    static uint64_t lastCheck = 0;
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    
    if (now - lastCheck > 1'000'000'000) {
        lastCheck = now;
    }
}