#include "vwap_calculator.h"
#include "message.h"
#include "metrics.h"
#include "features.h"
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
            timestamps(), quantities(), priceVolumes(), head(0), count(0), prefixGeneration(0),
            windowStartTime(0), firstWindowComplete(false), lastTradeTime(0),
            totalTradesProcessed(0), rejectedTrades(0) {}

void VwapCalculator::addTrade(const TradeMessage& trade) noexcept {

    // Reject zero or negative quantity or non-positive price explicitly; tests expect ignoring these
    if (trade.price <= 0 || trade.quantity == 0) {
        ++rejectedTrades;
        return;
    }

    if (lastTradeTime != 0 && trade.timestamp < lastTradeTime) {
        ++rejectedTrades;
        g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    uint64_t ts = trade.timestamp;
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
    if (Features::ENABLE_VWAP_DELTA_TS) {
        if (baseTimestamp == 0) baseTimestamp = ts; // establish base
        // store delta in timestamps array (microsecond granularity) to extend effective range
        uint64_t delta = ts - baseTimestamp;
        uint64_t micro = delta / 1000ULL; // truncate to microseconds
        ts = baseTimestamp + micro * 1000ULL; // normalized back to nearest micro
    }

    // (Removed oversized single-trade heuristic; rely on true overflow checks only.)

    // Insert into SoA ring
    size_t cap = MAX_TRADES;
    if (count < cap) {
        size_t pos = (head + count) % cap;
    timestamps[pos] = ts;
        quantities[pos] = qty;
        priceVolumes[pos] = priceVolume;
        ++count;
    } else {
        // overwrite oldest, adjust aggregates by removing oldest first
        size_t oldestPos = head;
        hotData.sumVolume      -= quantities[oldestPos];
        hotData.sumPriceVolume -= priceVolumes[oldestPos];
        // overwrite
    timestamps[oldestPos] = ts;
        quantities[oldestPos] = qty;
        priceVolumes[oldestPos] = priceVolume;
        head = (head + 1) % cap; // logical head moves forward
        ++prefixGeneration; // signal structural overwrite
    }

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
    if (count == 0) return;
    uint64_t cutoff = (currentTime > windowDurationNanos) ? (currentTime - windowDurationNanos) : 0;
    size_t cap = MAX_TRADES;
    if (timestamps[head] >= cutoff) return; // nothing to evict
    bool evicted = false;
    // Heuristic: start from lastEvictPos (virtual index) if still in range
    size_t startVirtual = lastEvictPos < count ? lastEvictPos : 0;
    // Advance while timestamp < cutoff
    size_t v = startVirtual;
    while (v < count) {
        size_t pos = (head + v) % cap;
        if (timestamps[pos] >= cutoff) break;
        ++v;
    }
    if (v == 0) return; // heuristic found none
    // Evict first v entries
    for (size_t i = 0; i < v; ++i) {
        size_t pos = (head + i) % cap;
        hotData.sumVolume      -= quantities[pos];
        hotData.sumPriceVolume -= priceVolumes[pos];
    }
    head = (head + v) % cap;
    count -= v;
    evicted = (v != 0);
    if (v > 0) {
        // retain fraction of evicted span as next heuristic start (temporal locality)
        size_t retained = v / 4; // 25%
        lastEvictPos = retained < count ? retained : 0;
    } else {
        lastEvictPos = 0;
    }
    if (evicted) {
        if (count == 0) {
            windowStartTime = 0;
            hotData.sumVolume = 0;
            hotData.sumPriceVolume = 0;
        } else {
            windowStartTime = timestamps[head];
        }
        hotData.vwapCacheValid = false;
        ++prefixGeneration;
    }
}

uint32_t VwapCalculator::lowerBoundTime(uint64_t cutoff) const noexcept {
    // Binary search over ring via virtual indexing
    size_t n = count;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        size_t pos = (head + mid) % MAX_TRADES;
        uint64_t ts = timestamps[pos];
        if (ts < cutoff) lo = mid + 1; else hi = mid;
    }
    return static_cast<uint32_t>(lo);
}

void VwapCalculator::printStatistics() const noexcept {
    std::cout << "\n=== VWAP Stats ===\n"
              << "Window Trades: " << count << "\n"
              << "Total Trades:  " << totalTradesProcessed << "\n"
              << "Rejected:      " << rejectedTrades << "\n"
              << "VWAP ($):      " << (getCurrentVwap() / 100.0) << "\n"
              << "Window Done:   " << (hasCompleteWindow() ? "Yes" : "No") << "\n"
              << "==================\n";

    assert((hotData.sumVolume != 0) || (hotData.sumPriceVolume == 0));
}
