#include "order_manager.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include "features.h"

OrderManager::OrderManager(const std::string& symbol, char side, uint32_t maxOrderSize, uint32_t vwapWindowSeconds)
    : symbol(symbol),
      side(side),
      maxOrderSize(maxOrderSize),
      vwapWindowSeconds(vwapWindowSeconds),
      currentState(State::WAITING_FOR_FIRST_WINDOW),
      totalQuotesProcessed(0),
      totalTradesProcessed(0),
      totalOrdersSent(0),
      vwapWindowCompleteNotified(false) {

    if (side != 'B' && side != 'S') {
        throw std::invalid_argument("Side must be 'B' or 'S'");
    }

    if (this->maxOrderSize == 0) {
        throw std::invalid_argument("Max order size must be positive");
    }

    if (this->vwapWindowSeconds == 0) {
        throw std::invalid_argument("VWAP window must be positive");
    }

    decisionEngine = std::make_unique<DecisionEngine>(symbol, side, this->maxOrderSize);
    vwapCalculator = std::make_unique<VwapCalculator>(this->vwapWindowSeconds);

    if (!Features::ENABLE_BENCHMARK_SUPPRESS_LOG) {
        std::cout << "OrderManager initialized:" << std::endl;
        std::cout << "  Symbol: " << symbol << std::endl;
        std::cout << "  Side: " << side << " (" << (side == 'B' ? "BUY" : "SELL") << ")" << std::endl;
        std::cout << "  Max Order Size: " << maxOrderSize << std::endl;
        std::cout << "  VWAP Window: " << vwapWindowSeconds << " seconds" << std::endl;
    }
}

OrderManager::~OrderManager() {
}

Optional<OrderMessage> OrderManager::processQuote(const QuoteMessage& quote) {
    totalQuotesProcessed++;

    checkVwapWindowComplete();

    double currentVwap = vwapCalculator->getCurrentVwap();

    OrderMessage order;
    bool triggered = decisionEngine->evaluateQuote(quote, currentVwap, order);
    if (triggered) {

    std::string reason = buildReason(quote, static_cast<int32_t>(currentVwap));
    recordOrder(order, reason);
        totalOrdersSent++;
    if (orderCallback) orderCallback(order);
    }
    if (triggered) return Optional<OrderMessage>(order); else return Optional<OrderMessage>();
}

void OrderManager::processTrade(const TradeMessage& trade) {
    totalTradesProcessed++;
    vwapCalculator->addTrade(trade);
    checkVwapWindowComplete();

    if (!Features::ENABLE_BENCHMARK_SUPPRESS_LOG) {
        if (totalTradesProcessed % 10 == 0) {
            double vwap = vwapCalculator->getCurrentVwap();
            std::cout << "[VWAP UPDATE] Current VWAP: $" << (vwap / 100.0)
                      << " (after " << totalTradesProcessed << " trades)" << std::endl;
        }
    }
}

void OrderManager::checkVwapWindowComplete() {
    if (currentState == State::WAITING_FOR_FIRST_WINDOW &&
        vwapCalculator->hasCompleteWindow()) {
        currentState = State::READY_TO_TRADE;
        decisionEngine->onVwapWindowComplete();

        if (!vwapWindowCompleteNotified && !Features::ENABLE_BENCHMARK_SUPPRESS_LOG) {
            std::cout << "VWAP window complete - ready to trade" << std::endl;
            vwapWindowCompleteNotified = true;
        } else if (!vwapWindowCompleteNotified) {
            vwapWindowCompleteNotified = true;
        }
    }
}

void OrderManager::recordOrder(const OrderMessage& order, const std::string& reason) {
    OrderRecord rec; rec.timestamp = order.timestamp; std::memcpy(rec.symbol, order.symbol, 8);
    rec.side = static_cast<uint8_t>(order.side); rec.quantity = order.quantity; rec.price = order.price;
    std::snprintf(rec.reason, OrderRecord::REASON_MAX, "%s", reason.c_str());
    orderHistory.push_back(rec);

    if (!Features::ENABLE_BENCHMARK_SUPPRESS_LOG) {
    char symBuf[9]; std::memcpy(symBuf, rec.symbol, 8); symBuf[8]='\0';
    std::cout << "[ORDER SENT] " << symBuf
          << " " << (rec.side == 'B' ? "BUY" : "SELL")
          << " " << rec.quantity
          << " @ $" << (rec.price / 100.0)
          << " | " << rec.reason << std::endl;
    }
}

std::string OrderManager::buildReason(const QuoteMessage& q, int32_t vwapCents) const {
    if (side == 'B') {
        return std::string("Buy Order: Ask (") + std::to_string(q.askPrice) + ") < VWAP (" + std::to_string(vwapCents) + ")";
    }
    return std::string("Sell Order: Bid (") + std::to_string(q.bidPrice) + ") > VWAP (" + std::to_string(vwapCents) + ")";
}

void OrderManager::printStatistics() const {
    std::cout << "\n=== Order Manager Statistics ===" << std::endl;
    std::cout << "State: ";
    switch (currentState) {
        case State::WAITING_FOR_FIRST_WINDOW:
            std::cout << "WAITING_FOR_FIRST_WINDOW";
            break;
        case State::READY_TO_TRADE:
            std::cout << "READY_TO_TRADE";
            break;
        case State::ORDER_SENT:
            std::cout << "ORDER_SENT";
            break;
    }
    std::cout << std::endl;
    std::cout << "Quotes Processed: " << totalQuotesProcessed << std::endl;
    std::cout << "Trades Processed: " << totalTradesProcessed << std::endl;
    std::cout << "Orders Sent: " << totalOrdersSent << std::endl;
    std::cout << "================================" << std::endl;

    vwapCalculator->printStatistics();
    if (decisionEngine) {
        std::cout << "Decision Engine Eval Latency:" << std::endl;
        std::cout << "  Max(ns):  " << decisionEngine->getEvalLatencyMax()
                  << " Last(ns): " << decisionEngine->getEvalLatencyLast()
                  << " SpikeThresh(ns): " << decisionEngine->getSpikeThreshold() << std::endl;
        const uint64_t* buckets = decisionEngine->getLatencyBuckets();
        std::cout << "  Buckets (<=ns count): ";
        const uint64_t* thresholds = DecisionEngine::latencyBucketThresholds();
        size_t bucketCount = DecisionEngine::latencyBucketCount();
        for (size_t i=0;i<bucketCount; ++i) {
            std::cout << thresholds[i] << ":" << buckets[i] << " ";
        }
        std::cout << ">" << thresholds[bucketCount-1]
                  << ":" << buckets[bucketCount] << std::endl;
    }
}

void OrderManager::printOrderHistory() const {
    printOrderHistory(orderHistory.size());
}

void OrderManager::printOrderHistory(size_t count) const {
    std::cout << "\n=== Order History ===" << std::endl;
    if (orderHistory.empty()) {
        std::cout << "No orders sent yet" << std::endl;
    } else {
        std::cout << std::setw(10) << "Symbol"
                  << std::setw(8) << "Side"
                  << std::setw(10) << "Quantity"
                  << std::setw(12) << "Price"
                  << "  Reason" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        size_t toShow = std::min(count, orderHistory.size());
        size_t start = orderHistory.size() > toShow ? orderHistory.size() - toShow : 0;

        for (size_t i = start; i < orderHistory.size(); ++i) {
            const auto& r = orderHistory[i]; char sym[9]; std::memcpy(sym,r.symbol,8); sym[8]='\0';
            std::cout << std::setw(10) << sym
                      << std::setw(8) << (r.side == 'B' ? "BUY" : "SELL")
                      << std::setw(10) << r.quantity
                      << std::setw(12) << std::fixed << std::setprecision(2)
                      << (r.price / 100.0)
                      << "  " << r.reason << std::endl;
        }
    }
    std::cout << "=====================" << std::endl;
}
