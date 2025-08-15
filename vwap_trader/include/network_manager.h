#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <functional>
#include <algorithm>
#include <sys/select.h>
#include <errno.h>
#include <cstring>
#include "config.h"
#include "message.h"

class MarketDataClient;
class OrderClient;

class NetworkManager final {
private:
    MarketDataClient* marketClient;
    OrderClient* orderClient;
    bool running;

    std::function<void(const QuoteMessage&)> quoteCallback;
    std::function<void(const TradeMessage&)> tradeCallback;

public:
    NetworkManager();
    ~NetworkManager();

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    NetworkManager(NetworkManager&&) = default;
    NetworkManager& operator=(NetworkManager&&) = default;

    bool initialize(const Config& config);
    void processEvents();
    void stop();

    void setQuoteCallback(std::function<void(const QuoteMessage&)> cb);
    void setTradeCallback(std::function<void(const TradeMessage&)> cb);

    bool sendOrder(const OrderMessage& order);

private:
    void handleMarketData(const MessageHeader& header, const void* data);
    void handlePeriodicTasks();
};

#endif // NETWORK_MANAGER_H
