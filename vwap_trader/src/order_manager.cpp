#include "order_manager.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <algorithm>

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
    
    decisionEngine = std::unique_ptr<DecisionEngine>(
        new DecisionEngine(symbol, side, this->maxOrderSize)
    );
    
    vwapCalculator = std::unique_ptr<VwapCalculator>(
        new VwapCalculator(this->vwapWindowSeconds)
    );
    
    std::cout << "OrderManager initialized:" << std::endl;
    std::cout << "  Symbol: " << symbol << std::endl;
    std::cout << "  Side: " << side << " (" << (side == 'B' ? "BUY" : "SELL") << ")" << std::endl;
    std::cout << "  Max Order Size: " << maxOrderSize << std::endl;
    std::cout << "  VWAP Window: " << vwapWindowSeconds << " seconds" << std::endl;
}

OrderManager::~OrderManager() {
}

Optional<OrderMessage> OrderManager::processQuote(const QuoteMessage& quote) {
    totalQuotesProcessed++;
    
    checkVwapWindowComplete();
    
    double currentVwap = vwapCalculator->getCurrentVwap();
    
    if (currentVwap <= 0 && currentState == State::READY_TO_TRADE) {
        std::cerr << "Warning: VWAP is zero but state is READY_TO_TRADE" << std::endl;
        return Optional<OrderMessage>();
    }
    
    Optional<OrderMessage> orderOpt = decisionEngine->evaluateQuote(quote, currentVwap);
    
    if (orderOpt.has_value()) {
        OrderMessage order = orderOpt.value();
        
        std::string reason;
        if (side == 'B') {
            reason = "Buy Order: Ask (" + std::to_string(quote.askPrice) + 
                     ") < VWAP (" + std::to_string(static_cast<int32_t>(currentVwap)) + ")";
        } else {
            reason = "Sell Order: Bid (" + std::to_string(quote.bidPrice) + 
                     ") > VWAP (" + std::to_string(static_cast<int32_t>(currentVwap)) + ")";
        }
        
        recordOrder(order, reason);
        totalOrdersSent++;
        
        currentState = State::ORDER_SENT;
        currentState = State::READY_TO_TRADE;
    }
    
    return orderOpt;
}

void OrderManager::processTrade(const TradeMessage& trade) {
    totalTradesProcessed++;
    vwapCalculator->addTrade(trade);
    checkVwapWindowComplete();
    
    if (totalTradesProcessed % 10 == 0) {
        double vwap = vwapCalculator->getCurrentVwap();
        std::cout << "[VWAP UPDATE] Current VWAP: $" << (vwap / 100.0) 
                  << " (after " << totalTradesProcessed << " trades)" << std::endl;
    }
}

void OrderManager::checkVwapWindowComplete() {
    if (currentState == State::WAITING_FOR_FIRST_WINDOW &&
        vwapCalculator->hasCompleteWindow()) {
        currentState = State::READY_TO_TRADE;
        decisionEngine->onVwapWindowComplete();  // Notify decision engine
        
        if (!vwapWindowCompleteNotified) {
            std::cout << "VWAP window complete - ready to trade" << std::endl;
            vwapWindowCompleteNotified = true;
        }
    }
}

void OrderManager::recordOrder(const OrderMessage& order, const std::string& reason) {
    OrderRecord record;
    record.timestamp = order.timestamp;
    
    // Extract symbol string safely
    char symbolStr[9];
    std::memcpy(symbolStr, order.symbol, 8);
    symbolStr[8] = '\0';
    record.symbol = std::string(symbolStr);
    
    record.side = order.side;
    record.quantity = order.quantity;
    record.price = order.price;
    record.reason = reason;
    
    orderHistory.push_back(std::move(record));
    
    std::cout << "[ORDER SENT] " << record.symbol 
              << " " << (record.side == 'B' ? "BUY" : "SELL")
              << " " << record.quantity 
              << " @ $" << (record.price / 100.0)
              << " | " << reason << std::endl;
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
            const auto& record = orderHistory[i];
            std::cout << std::setw(10) << record.symbol
                      << std::setw(8) << (record.side == 'B' ? "BUY" : "SELL")
                      << std::setw(10) << record.quantity
                      << std::setw(12) << std::fixed << std::setprecision(2) 
                      << (record.price / 100.0)
                      << "  " << record.reason << std::endl;
        }
    }
    std::cout << "=====================" << std::endl;
}