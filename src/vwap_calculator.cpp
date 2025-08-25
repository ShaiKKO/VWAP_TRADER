#include "vwap_calculator.h"
#include "message.h"
#include "metrics.h"
#include <iostream>
#include <limits>
#include <cassert>

namespace {
    #ifndef __SIZEOF_INT128__
    inline bool wouldMultiplyOverflow(uint64_t a, uint64_t b) noexcept {
        return (a != 0 && b > std::numeric_limits<uint64_t>::max() / a);
    }
    #endif
    inline bool wouldAddOverflow(uint64_t a, uint64_t b) noexcept {
        return (b > std::numeric_limits<uint64_t>::max() - a);
    }
}

VwapCalculator::VwapCalculator(uint32_t windowSeconds) noexcept
    : hotData{0, 0, 0.0, false},
      windowDurationNanos(static_cast<uint64_t>(windowSeconds) * 1'000'000'000ULL),
      tradeWindow(),
    prefixVolume(),
    prefixPriceVolume(),
    #ifndef VWAP_DISABLE_TIME_INDEX
    timeIndex(),
    #endif
    oldestIndex(0),
    prefixGeneration(0),
    pendingRebuild(false),
      windowStartTime(0),
      firstWindowComplete(false),
      lastTradeTime(0),
      totalTradesProcessed(0),
      rejectedTrades(0) {}

void VwapCalculator::addTrade(const TradeMessage& trade) noexcept {

    if (trade.price < 0 || trade.quantity == 0) {
        ++rejectedTrades;
        return;
    }

    if (lastTradeTime != 0 && trade.timestamp < lastTradeTime) {
        ++rejectedTrades;
        g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t ts = trade.timestamp;
    const uint32_t qty = trade.quantity;
    const int32_t  price = trade.price;

#ifdef __SIZEOF_INT128__
    unsigned __int128 pv128 =
        static_cast<unsigned __int128>(static_cast<uint64_t>(price)) *
        static_cast<unsigned __int128>(qty);
    if (pv128 > std::numeric_limits<uint64_t>::max()) {
        ++rejectedTrades;
        return;
    }
    uint64_t priceVolume = static_cast<uint64_t>(pv128);
#else
    if (wouldMultiplyOverflow(static_cast<uint64_t>(price), static_cast<uint64_t>(qty))) {
        ++rejectedTrades;
        return;
    }
    uint64_t priceVolume = static_cast<uint64_t>(price) * static_cast<uint64_t>(qty);
#endif

#ifdef __SIZEOF_INT128__

    if (wouldAddOverflow(hotData.sumPriceVolume, priceVolume) ||
        wouldAddOverflow(hotData.sumVolume, qty)) {
        ++rejectedTrades;
        return;
    }
    uint64_t newPV  = hotData.sumPriceVolume + priceVolume;
    uint64_t newVol = hotData.sumVolume + qty;
#else
    if (wouldAddOverflow(hotData.sumPriceVolume, priceVolume) ||
        wouldAddOverflow(hotData.sumVolume, qty)) {
        ++rejectedTrades;
        return;
    }
    uint64_t newPV  = hotData.sumPriceVolume + priceVolume;
    uint64_t newVol = hotData.sumVolume + qty;
#endif

    if (windowStartTime == 0) windowStartTime = ts;

    tradeWindow.push_back(VwapTradeRecord(ts, qty, price));

    if (tradeWindow.full()) {
        pendingRebuild = true;
    }
    appendPrefix(qty, priceVolume);
    size_t n = tradeWindow.size();
    #ifndef VWAP_DISABLE_TIME_INDEX
    if (n) timeIndex[n-1] = ts;
    #endif

    hotData.sumPriceVolume = newPV;
    hotData.sumVolume      = newVol;
    hotData.vwapCacheValid = false;

    ++totalTradesProcessed;

    g_systemMetrics.hot.tradesProcessed.fetch_add(1, std::memory_order_relaxed);
    lastTradeTime = ts;

    removeExpiredTrades(ts);

    if (!firstWindowComplete &&
        (ts - windowStartTime) >= windowDurationNanos) {
        firstWindowComplete = true;
    }
}

void VwapCalculator::removeExpiredTrades(uint64_t currentTime) noexcept {
    const uint64_t cutoff = (currentTime > windowDurationNanos)
        ? (currentTime - windowDurationNanos)
        : 0;
    if (tradeWindow.empty()) return;

    if (tradeWindow.front().timestamp >= cutoff) return;

    size_t n = tradeWindow.size();

    if (pendingRebuild) rebuildPrefixes();

    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (tradeWindow[mid].timestamp < cutoff) lo = mid + 1; else hi = mid;
    }
    size_t removeCount = lo;
    if (removeCount == 0) return;
    uint64_t volRemoved = prefixVolume[removeCount-1];
    uint64_t pvRemoved  = prefixPriceVolume[removeCount-1];

    for (size_t i=0;i<removeCount;++i) tradeWindow.pop_front();
    hotData.sumVolume      -= volRemoved;
    hotData.sumPriceVolume -= pvRemoved;

    size_t newN = tradeWindow.size();
    if (newN) {

        std::memmove(prefixVolume.data(), prefixVolume.data() + removeCount, newN * sizeof(uint64_t));
        std::memmove(prefixPriceVolume.data(), prefixPriceVolume.data() + removeCount, newN * sizeof(uint64_t));
        #ifndef VWAP_DISABLE_TIME_INDEX
        std::memmove(timeIndex.data(), timeIndex.data() + removeCount, newN * sizeof(uint64_t));
        #endif
        for (size_t i=0;i<newN; ++i) {
            prefixVolume[i]      -= volRemoved;
            prefixPriceVolume[i] -= pvRemoved;
        }
    }
    oldestIndex += removeCount;
    if (oldestIndex > 10u * MAX_TRADES) oldestIndex = MAX_TRADES;
    if (tradeWindow.empty()) {
        hotData.sumVolume = 0; hotData.sumPriceVolume = 0; windowStartTime = 0;
    } else {
        windowStartTime = tradeWindow.front().timestamp;
    }
    hotData.vwapCacheValid = false;
}

void VwapCalculator::rebuildPrefixes() noexcept {
    size_t n = tradeWindow.size();
    uint64_t v=0, pv=0;
    for (size_t i=0;i<n;++i) {
        const auto& tr = tradeWindow[i];
        v += tr.quantity; pv += tr.priceVolume;
        prefixVolume[i] = v; prefixPriceVolume[i] = pv;
        #ifndef VWAP_DISABLE_TIME_INDEX
        timeIndex[i] = tr.timestamp;
        #endif
    }
    pendingRebuild = false;
    ++prefixGeneration;
}

void VwapCalculator::appendPrefix(uint32_t qty, uint64_t pv) noexcept {
    size_t n = tradeWindow.size();
    if (n==0) return;
    uint64_t prevV = (n>1)?prefixVolume[n-2]:0;
    uint64_t prevPV = (n>1)?prefixPriceVolume[n-2]:0;
    prefixVolume[n-1] = prevV + qty;
    prefixPriceVolume[n-1] = prevPV + pv;
}

uint32_t VwapCalculator::lowerBoundTime(uint64_t cutoff) const noexcept {
    size_t n = tradeWindow.size();
    size_t lo=0, hi=n;
    while (lo<hi) { size_t mid=(lo+hi)/2; if (tradeWindow[mid].timestamp < cutoff) lo=mid+1; else hi=mid; }
    return static_cast<uint32_t>(lo);
}

void VwapCalculator::printStatistics() const noexcept {
    std::cout << "\n=== VWAP Stats ===\n"
              << "Window Trades: " << tradeWindow.size() << "\n"
              << "Total Trades:  " << totalTradesProcessed << "\n"
              << "Rejected:      " << rejectedTrades << "\n"
              << "VWAP ($):      " << (getCurrentVwap() / 100.0) << "\n"
              << "Window Done:   " << (hasCompleteWindow() ? "Yes" : "No") << "\n"
              << "==================\n";

    assert((hotData.sumVolume != 0) || (hotData.sumPriceVolume == 0));
}
