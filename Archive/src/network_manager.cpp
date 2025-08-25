#include "network_manager.h"
#include "market_data_client.h"
#include "order_client.h"
#include <iostream>
#include <chrono>
#include "time_source.h"
#include <random>
#include <algorithm>
#include <thread>
#include "metrics.h"

NetworkManager::NetworkManager()
                : marketClient(nullptr), orderClient(nullptr), running(false),
                    marketReconnectDelay(1000), orderReconnectDelay(1000),
                    lastMarketReconnect(std::chrono::steady_clock::now()),
                    lastOrderReconnect(std::chrono::steady_clock::now()) {}

NetworkManager::~NetworkManager() {
    stop();

}

bool NetworkManager::initialize(const Config& config) {
    marketClient = std::make_unique<MarketDataClient>(config.marketDataHost,
                                                      config.marketDataPort);
    orderClient = std::make_unique<OrderClient>(config.orderHost,
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
    if (!running) return;

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
        tryReconnectMarket();
    }

    if (orderClient->isConnected()) {
        int fd = orderClient->getSocketFd();
        FD_SET(fd, &writeSet);
        maxFd = std::max(maxFd, fd);
    } else {
        tryReconnectOrder();
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    if (maxFd == -1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        handlePeriodicTasks();
        return;
    }

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
    if (!running) return;
    running = false;
    if (marketClient) marketClient->disconnect();
    if (orderClient)  orderClient->disconnect();
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
            if (quoteCallback) quoteCallback(*static_cast<const QuoteMessage*>(data));
            break;
        case MessageHeader::TRADE_TYPE:
            if (tradeCallback) tradeCallback(*static_cast<const TradeMessage*>(data));
            break;
    }
}

void NetworkManager::handlePeriodicTasks() {
    static uint64_t lastCheckNs = Time::instance().nowNanos();
    static uint64_t lastFlushNs = lastCheckNs;
    uint64_t nowNs = Time::instance().nowNanos();
    if (nowNs - lastCheckNs > 1000000000ULL) {
        lastCheckNs = nowNs;

    }
    if (nowNs - lastFlushNs > 1000000000ULL) {
        lastFlushNs = nowNs;
        flushAllMetrics();
    }
}

namespace {
inline uint64_t applyJitter(uint64_t baseMs, uint32_t pctLow = 85, uint32_t pctHigh = 115) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(pctLow, pctHigh);
    return (baseMs * dist(gen)) / 100;
}
}

void NetworkManager::tryReconnectMarket() {
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMarketReconnect).count();
    if (elapsedMs < (long long)marketReconnectDelay) return;
    lastMarketReconnect = now;
    if (marketClient->reconnect()) {
        marketReconnectDelay = 1000;
        std::cout << "Market data reconnected" << std::endl;
        g_systemMetrics.cold.connectionsAccepted.fetch_add(1, std::memory_order_relaxed);
    } else {
        marketReconnectDelay = std::min(marketReconnectDelay * 2, uint64_t(60000));
        marketReconnectDelay = applyJitter(marketReconnectDelay);
        std::cerr << "Market data reconnect failed, next attempt in " << marketReconnectDelay << "ms" << std::endl;
        g_systemMetrics.cold.connectionErrors.fetch_add(1, std::memory_order_relaxed);
    }
}

void NetworkManager::tryReconnectOrder() {
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastOrderReconnect).count();
    if (elapsedMs < (long long)orderReconnectDelay) return;
    lastOrderReconnect = now;
    if (orderClient->reconnect()) {
        orderReconnectDelay = 1000;
        std::cout << "Order client reconnected" << std::endl;
        g_systemMetrics.cold.connectionsAccepted.fetch_add(1, std::memory_order_relaxed);
    } else {
        orderReconnectDelay = std::min(orderReconnectDelay * 2, uint64_t(60000));
        orderReconnectDelay = applyJitter(orderReconnectDelay);
        std::cerr << "Order client reconnect failed, next attempt in " << orderReconnectDelay << "ms" << std::endl;
        g_systemMetrics.cold.connectionErrors.fetch_add(1, std::memory_order_relaxed);
    }
}
