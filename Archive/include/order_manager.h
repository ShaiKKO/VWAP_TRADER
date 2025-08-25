#ifndef ORDER_MANAGER_H
#define ORDER_MANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <functional>
#include "message.h"
#include "optional.h"
#include "vwap_calculator.h"
#include "decision_engine.h"
#include "circular_buffer.h"

class OrderManager final {
public:
    enum class State {
        WAITING_FOR_FIRST_WINDOW,
        READY_TO_TRADE,
        ORDER_SENT
    };

    struct OrderRecord {
        uint64_t timestamp;
        char symbol[8];
        uint8_t side;
        uint32_t quantity;
        int32_t price;
        static constexpr size_t REASON_MAX = 64;
        char reason[REASON_MAX];
    }; // POD, no dynamic allocations

private:
    std::string symbol;
    char side;
    uint32_t maxOrderSize;
    uint32_t vwapWindowSeconds;

    State currentState;

    std::unique_ptr<VwapCalculator> vwapCalculator;
    std::unique_ptr<DecisionEngine> decisionEngine;
    std::function<void(const OrderMessage&)> orderCallback;

    uint64_t totalQuotesProcessed;
    uint64_t totalTradesProcessed;
    uint64_t totalOrdersSent;
    bool vwapWindowCompleteNotified;

    static constexpr size_t MAX_ORDER_HISTORY = 1000;
    CircularBuffer<OrderRecord, MAX_ORDER_HISTORY> orderHistory;

public:
    OrderManager(const std::string& symbol, char side,
                uint32_t maxOrderSize, uint32_t vwapWindowSeconds);
    ~OrderManager();

    OrderManager(const OrderManager&) = delete;
    OrderManager& operator=(const OrderManager&) = delete;

    OrderManager(OrderManager&&) = default;
    OrderManager& operator=(OrderManager&&) = default;

    Optional<OrderMessage> processQuote(const QuoteMessage& quote);
    void processTrade(const TradeMessage& trade);
    void setOrderCallback(std::function<void(const OrderMessage&)> cb) { orderCallback = std::move(cb); }

    void printStatistics() const;
    void printOrderHistory() const;
    void printOrderHistory(size_t count) const;
    State getState() const noexcept { return currentState; }
    bool isReadyToTrade() const noexcept { return currentState == State::READY_TO_TRADE; }
    double getCurrentVwap() const { return vwapCalculator->getCurrentVwap(); }
    uint64_t getQuoteCount() const noexcept { return totalQuotesProcessed; }
    uint64_t getTradeCount() const noexcept { return totalTradesProcessed; }
    uint64_t getOrderCount() const noexcept { return totalOrdersSent; }
    // Legacy test compatibility alias
    uint64_t getTotalOrdersSent() const noexcept { return totalOrdersSent; }

    std::vector<OrderRecord> getOrderHistory() const {
        std::vector<OrderRecord> v;
        v.reserve(orderHistory.size());
        for (const auto& r : orderHistory) v.push_back(r);
        return v;
    }

private:
    void checkVwapWindowComplete();
    void recordOrder(const OrderMessage& order, const std::string& reason);
    std::string buildReason(const QuoteMessage& q, int32_t vwapCents) const;
};

#endif
