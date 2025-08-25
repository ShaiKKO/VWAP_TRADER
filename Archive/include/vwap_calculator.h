#ifndef VWAP_CALCULATOR_H
#define VWAP_CALCULATOR_H

#include <cstdint>
#include <array>

struct TradeMessage;

class VwapCalculator final {
private:
    alignas(64) struct HotData {
        uint64_t sumPriceVolume;
        uint64_t sumVolume;
        mutable double cachedVwap;
        mutable bool vwapCacheValid;
    } hotData;
    
    static_assert(sizeof(HotData) <= 64, "HotData must fit in cache line");
    
    const uint64_t windowDurationNanos;
    static constexpr size_t MAX_TRADES = 10000;
    // Structure-of-Arrays ring buffer (SoA)
    std::array<uint64_t, MAX_TRADES> timestamps;     // nanosecond timestamps
    std::array<uint32_t, MAX_TRADES> quantities;     // trade sizes
    std::array<uint64_t, MAX_TRADES> priceVolumes;   // price * quantity
    // Optional delta timestamp compression base (if enabled)
    uint64_t baseTimestamp = 0;
    mutable uint32_t lastEvictPos = 0; // heuristic starting point for eviction search
    size_t head = 0;      // logical start index
    size_t count = 0;     // number of valid entries
    uint32_t prefixGeneration = 0; // kept for compatibility / tests

    uint64_t windowStartTime; // timestamp of oldest trade
    bool firstWindowComplete;
    uint64_t lastTradeTime;

    uint64_t totalTradesProcessed;
    uint64_t rejectedTrades;

public:
    explicit VwapCalculator(uint32_t windowSeconds) noexcept;

    VwapCalculator(const VwapCalculator&) = delete;
    VwapCalculator& operator=(const VwapCalculator&) = delete;
    VwapCalculator(VwapCalculator&&) noexcept = default;
    VwapCalculator& operator=(VwapCalculator&&) noexcept = delete;
    ~VwapCalculator() = default;

    void addTrade(const TradeMessage& trade) noexcept;
    double getCurrentVwap() const noexcept;
    bool hasCompleteWindow() const noexcept { return firstWindowComplete && count != 0; }

    uint32_t getTradeCount() const noexcept { return static_cast<uint32_t>(count); }
    uint64_t getTotalTradesProcessed() const noexcept { return totalTradesProcessed; }
    uint64_t getRejectedTrades() const noexcept { return rejectedTrades; }
    uint64_t getWindowStartTime() const noexcept { return windowStartTime; }
    uint64_t getLastTradeTime() const noexcept { return lastTradeTime; }
    uint32_t getPrefixGeneration() const noexcept { return prefixGeneration; }

    void printStatistics() const noexcept;

private:
    void removeExpiredTrades(uint64_t currentTime) noexcept;
    uint32_t lowerBoundTime(uint64_t cutoff) const noexcept;
};

inline double VwapCalculator::getCurrentVwap() const noexcept {
    if (!hotData.vwapCacheValid) {
        hotData.cachedVwap = (hotData.sumVolume == 0)
            ? 0.0
            : static_cast<double>(hotData.sumPriceVolume) / static_cast<double>(hotData.sumVolume);
        hotData.vwapCacheValid = true;
    }
    return hotData.cachedVwap;
}

#endif // VWAP_CALCULATOR_H