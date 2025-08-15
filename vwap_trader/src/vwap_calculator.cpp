#include "vwap_calculator.h"
#include <iostream>
#include <cstring>

VwapCalculator::VwapCalculator(uint32_t windowSeconds)
    : hotData{0, 0, 0.0, false},
      windowDurationNanos(static_cast<uint64_t>(windowSeconds) * 1000000000ULL),
      windowStartTime(0),
      firstWindowComplete(false),
      lastTradeTime(0),
      tradesInWindow(0),
      totalTradesProcessed(0) {
    std::memset(expiredTrades, 0, sizeof(expiredTrades));
}

void VwapCalculator::addTrade(const TradeMessage& trade) {
    if (trade.quantity == 0 || trade.price <= 0) {
        return;
    }
    
    const uint64_t timestamp = trade.timestamp;
    const uint32_t quantity = trade.quantity;
    const int32_t price = trade.price;
    
    const uint64_t priceVolume = multiplyWithOverflowCheck(
        static_cast<uint64_t>(price), 
        static_cast<uint64_t>(quantity)
    );
    
    if (windowStartTime == 0) {
        windowStartTime = timestamp;
    }
    
    VwapTradeRecord record(timestamp, quantity, price);
    tradeWindow.push_back(std::move(record));
    
    hotData.sumPriceVolume = addWithOverflowCheck(hotData.sumPriceVolume, priceVolume);
    hotData.sumVolume = addWithOverflowCheck(hotData.sumVolume, quantity);
    
    hotData.vwapCacheValid = false;
    
    ++tradesInWindow;
    ++totalTradesProcessed;
    lastTradeTime = timestamp;
    
    removeExpiredTrades(timestamp);
    
    if (!firstWindowComplete && 
        (timestamp - windowStartTime) >= windowDurationNanos) {
        firstWindowComplete = true;
    }
}

void VwapCalculator::removeExpiredTrades(uint64_t currentTime) {
    const uint64_t cutoffTime = (currentTime > windowDurationNanos) 
        ? (currentTime - windowDurationNanos) 
        : 0;
    
    // Batch removal for better performance
    size_t expiredCount = 0;
    
    while (!tradeWindow.empty() && tradeWindow.front().timestamp < cutoffTime) {
        const VwapTradeRecord& oldTrade = tradeWindow.front();
        
        if (expiredCount < MAX_EXPIRED_BATCH) {
            expiredTrades[expiredCount++] = oldTrade;
        }
        
        tradeWindow.pop_front();
        --tradesInWindow;
    }
    
    for (size_t i = 0; i < expiredCount; ++i) {
        hotData.sumPriceVolume -= expiredTrades[i].priceVolume;
        hotData.sumVolume -= expiredTrades[i].quantity;
    }
    
    if (expiredCount > 0) {
        hotData.vwapCacheValid = false;
    }
    
    if (tradeWindow.empty()) {
        windowStartTime = 0;
        hotData.sumPriceVolume = 0;
        hotData.sumVolume = 0;
    } else {
        windowStartTime = tradeWindow.front().timestamp;
    }
}

void VwapCalculator::printStatistics() const {
    std::cout << "\n=== VWAP Calculator Statistics ===" << std::endl;
    std::cout << "Trades in Window: " << tradesInWindow << std::endl;
    std::cout << "Total Trades Processed: " << totalTradesProcessed << std::endl;
    std::cout << "Current VWAP: $" << (getCurrentVwap() / 100.0) << std::endl;
    std::cout << "Window Complete: " << (hasCompleteWindow() ? "Yes" : "No") << std::endl;
    std::cout << "Cache Valid: " << (hotData.vwapCacheValid ? "Yes" : "No") << std::endl;
    std::cout << "==================================" << std::endl;
}