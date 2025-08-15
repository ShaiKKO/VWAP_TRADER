#ifndef VWAP_CALCULATOR_H
#define VWAP_CALCULATOR_H

#include <cstdint>
#include <atomic>
#include "circular_buffer.h"
#include "message.h"

class VwapCalculator final {
private:
    alignas(64) struct {
        uint64_t sumPriceVolume;
        uint64_t sumVolume;
        mutable double cachedVwap;
        mutable bool vwapCacheValid;
    } hotData;
    
    const uint64_t windowDurationNanos;
    VwapWindowBuffer tradeWindow;
    
    uint64_t windowStartTime;
    bool firstWindowComplete;
    uint64_t lastTradeTime;
    
    uint32_t tradesInWindow;
    uint64_t totalTradesProcessed;
    
    static constexpr size_t MAX_EXPIRED_BATCH = 100;
    VwapTradeRecord expiredTrades[MAX_EXPIRED_BATCH];
    
public:
    explicit VwapCalculator(uint32_t windowSeconds);
    
    void addTrade(const TradeMessage& trade);
    double getCurrentVwap() const;
    bool hasCompleteWindow() const noexcept;
    
    uint32_t getTradeCount() const noexcept { return tradesInWindow; }
    uint64_t getTotalTradesProcessed() const noexcept { return totalTradesProcessed; }
    
    void printStatistics() const;
    
private:
    void removeExpiredTrades(uint64_t currentTime);
    
    static uint64_t multiplyFast(uint64_t a, uint64_t b) {
        return a * b;
    }
    
    static uint64_t addFast(uint64_t a, uint64_t b) {
        return a + b;
    }
    static uint64_t multiplyWithOverflowCheck(uint64_t a, uint64_t b) {
        if (a > 0 && b > UINT64_MAX / a) {
            return UINT64_MAX;
        }
        return a * b;
    }
    
    static uint64_t addWithOverflowCheck(uint64_t a, uint64_t b) {
        if (a > UINT64_MAX - b) {
            return UINT64_MAX;
        }
        return a + b;
    }
};

inline double VwapCalculator::getCurrentVwap() const {
    if (__builtin_expect(hotData.vwapCacheValid, 1)) {
        return hotData.cachedVwap;
    }
    
    if (__builtin_expect(hotData.sumVolume == 0, 0)) {
        hotData.cachedVwap = 0.0;
    } else {
        hotData.cachedVwap = static_cast<double>(hotData.sumPriceVolume) / 
                             static_cast<double>(hotData.sumVolume);
    }
    
    hotData.vwapCacheValid = true;
    return hotData.cachedVwap;
}

inline bool VwapCalculator::hasCompleteWindow() const noexcept {
    return firstWindowComplete && !tradeWindow.empty();
}

#endif // VWAP_CALCULATOR_H