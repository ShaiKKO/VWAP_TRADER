#include "decision_engine.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include "time_source.h"
#include "metrics.h"
#include "features.h"
#include <cstdlib>

// Out-of-line definition for static constexpr array (needed for some linkers/tests referencing symbol)
constexpr uint64_t DecisionEngine::LAT_THRESHOLDS[DecisionEngine::LAT_BUCKETS];

bool DecisionEngine::QuoteIdentifier::operator==(const QuoteIdentifier& other) const noexcept {
    return timestamp == other.timestamp &&
           price == other.price &&
           quantity == other.quantity;
}

DecisionEngine::DecisionEngine(const std::string& symbol, char side, uint32_t maxOrderSize, uint64_t cooldownNanos)
                : symbol(symbol), side(side), maxOrderSize(maxOrderSize), currentState(TradingState::WAITING_FOR_FIRST_WINDOW),
                    lastProcessedQuote{0,0,0}, lastOrderTimestamp(0), cooldownNanos(cooldownNanos), quotesProcessed(0), ordersTriggered(0),
                    ordersRejected(0), rejWaitingWindow(0), rejPriceUnfavorable(0), rejCooldown(0), rejDuplicate(0) {
        triggerPredicate = (side=='B') ? +[](double price,double v){ return price < v; } : +[](double price,double v){ return price > v; };
        const char* env = std::getenv("VWAP_SPIKE_THRESHOLD_NS");
        if (env) {
            uint64_t v = std::strtoull(env, nullptr, 10);
            if (v > 0) spikeThresholdNanos = v;
        }
}

void DecisionEngine::onVwapWindowComplete() {
    if (currentState == TradingState::WAITING_FOR_FIRST_WINDOW) {
        currentState = TradingState::READY_TO_TRADE;
        std::cout << "Decision Engine: First VWAP window complete, ready to trade" << std::endl;
    }
}

bool DecisionEngine::evaluateQuote(const QuoteMessage& quote, double vwap, OrderMessage& outOrder) noexcept {
    quotesProcessed++;
    extern SystemMetrics g_systemMetrics;
    g_systemMetrics.hot.quotesProcessed.fetch_add(1, std::memory_order_relaxed);
    uint64_t start = Time::instance().nowNanos();
    uint64_t currentTime = quote.timestamp;

    // Combined early rejection bitmask: bit0 waiting window, bit1 duplicate, bit2 cooldown
    uint32_t rejectMask = 0;
    if (currentState == TradingState::WAITING_FOR_FIRST_WINDOW) rejectMask |= 1u;
    if (!rejectMask && isDuplicateQuote(quote)) rejectMask |= 2u; // do not even test duplicate if already waiting
    if (!rejectMask && isInCooldown(currentTime)) rejectMask |= 4u;
    if (rejectMask) {
        Decision d{}; d.timestamp=currentTime; d.vwap=vwap; d.quotePrice=0.0; d.quoteSize=0; d.orderSize=0;
        if (rejectMask & 1u) { d.type=Decision::REJECTED_WAITING_WINDOW; ++ordersRejected; ++rejWaitingWindow; std::snprintf(d.reason, Decision::REASON_MAX, "%s", "Waiting for first VWAP window"); }
        else if (rejectMask & 2u) { d.type=Decision::REJECTED_DUPLICATE; ++ordersRejected; ++rejDuplicate; std::snprintf(d.reason, Decision::REASON_MAX, "%s", "Duplicate quote"); }
        else { d.type=Decision::REJECTED_COOLDOWN; ++ordersRejected; ++rejCooldown; std::snprintf(d.reason, Decision::REASON_MAX, "%s", "In cooldown period"); }
        recordDecision(d);
        uint64_t end = Time::instance().nowNanos();
        uint64_t nanos = end - start; evalLatencyLastNanos = nanos; if (nanos>evalLatencyMaxNanos) evalLatencyMaxNanos=nanos; g_metricsView.updateLatency(nanos);
        // bucket
        size_t idx=0; while (idx < LAT_BUCKETS && nanos <= LAT_THRESHOLDS[idx]) { latencyBucketCounts[idx]++; goto latency_done; } for (; idx < LAT_BUCKETS; ++idx){} latencyBucketCounts[LAT_BUCKETS]++; latency_done:;
        if (nanos > spikeThresholdNanos && !Features::ENABLE_BENCHMARK_SUPPRESS_LOG) std::cerr << "[EVAL SPIKE] earlyReject " << nanos << " ns" << std::endl;
        return false;
    }

    double relevantPrice = (side == 'B') ? quote.askPrice : quote.bidPrice;
    uint32_t relevantQuantity = (side == 'B') ? quote.askQuantity : quote.bidQuantity;

    if (!shouldTriggerOrder(quote, vwap)) {
        Decision d{}; d.type=Decision::REJECTED_PRICE_UNFAVORABLE; d.timestamp=currentTime; d.quotePrice=relevantPrice; d.vwap=vwap; d.quoteSize=relevantQuantity; d.orderSize=0;
        // snprintf only for this branch; cheap but still skip for earlier returns
        std::snprintf(d.reason, Decision::REASON_MAX, "%s", (side=='B'?"Ask >= VWAP":"Bid <= VWAP"));
        recordDecision(d); ++ordersRejected; ++rejPriceUnfavorable;
        uint64_t end = Time::instance().nowNanos(); uint64_t nanos = end - start; evalLatencyLastNanos = nanos; if (nanos>evalLatencyMaxNanos) evalLatencyMaxNanos=nanos; g_metricsView.updateLatency(nanos);
        size_t idx=0; while (idx < LAT_BUCKETS && nanos <= LAT_THRESHOLDS[idx]) { latencyBucketCounts[idx]++; goto latency_done2; } for (; idx < LAT_BUCKETS; ++idx){} latencyBucketCounts[LAT_BUCKETS]++; latency_done2:;
        if (nanos > spikeThresholdNanos && !Features::ENABLE_BENCHMARK_SUPPRESS_LOG) std::cerr << "[EVAL SPIKE] priceReject " << nanos << " ns" << std::endl;
        return false;
    }

    uint32_t orderSize = calculateOrderSize(relevantQuantity);
    OrderMessage order = buildOrder(quote, orderSize);
    currentState = TradingState::ORDER_SENT; lastOrderTimestamp = currentTime;
    lastProcessedQuote = {quote.timestamp, static_cast<int32_t>(relevantPrice), relevantQuantity};

    Decision d{}; d.type=Decision::ORDER_TRIGGERED; d.timestamp=currentTime; d.quotePrice=relevantPrice; d.vwap=vwap; d.quoteSize=relevantQuantity; d.orderSize=orderSize;
    std::snprintf(d.reason, Decision::REASON_MAX, "%s", (side=='B'?"Buy: Ask < VWAP":"Sell: Bid > VWAP"));
    recordDecision(d); ordersTriggered++; currentState = TradingState::READY_TO_TRADE; outOrder = order; lastQuoteFingerprint = fingerprint(quote.timestamp, static_cast<int32_t>(relevantPrice), relevantQuantity);

    uint64_t end = Time::instance().nowNanos(); uint64_t nanos = end - start; evalLatencyLastNanos = nanos; if (nanos>evalLatencyMaxNanos) evalLatencyMaxNanos=nanos; g_metricsView.updateLatency(nanos);
    size_t idx=0; while (idx < LAT_BUCKETS && nanos <= LAT_THRESHOLDS[idx]) { latencyBucketCounts[idx]++; goto latency_done3; } for (; idx < LAT_BUCKETS; ++idx){} latencyBucketCounts[LAT_BUCKETS]++; latency_done3:;
    if (nanos > spikeThresholdNanos && !Features::ENABLE_BENCHMARK_SUPPRESS_LOG) std::cerr << "[EVAL SPIKE] trigger " << nanos << " ns" << std::endl;
    return true;
}

bool DecisionEngine::shouldTriggerOrder(const QuoteMessage& quote, double vwap) const noexcept {
    if (vwap <= 0) return false; double price = (side=='B') ? quote.askPrice : quote.bidPrice; return triggerPredicate(price, vwap);
}

uint32_t DecisionEngine::calculateOrderSize(uint32_t quoteSize) const noexcept {
    return std::min(quoteSize, maxOrderSize);
}

bool DecisionEngine::isDuplicateQuote(const QuoteMessage& quote) const noexcept {
    int32_t p = (side=='B') ? static_cast<int32_t>(quote.askPrice) : static_cast<int32_t>(quote.bidPrice);
    uint32_t q = (side=='B') ? quote.askQuantity : quote.bidQuantity;
    uint64_t fp = fingerprint(quote.timestamp, p, q);
    return fp == lastQuoteFingerprint;
}

bool DecisionEngine::isInCooldown(uint64_t currentTime) const noexcept {
    if (lastOrderTimestamp == 0) {
        return false;
    }
    return (currentTime - lastOrderTimestamp) < cooldownNanos;
}

void DecisionEngine::recordDecision(const Decision& decision) noexcept {
    size_t pos;
    if (historyCount < MAX_HISTORY_SIZE) {
        pos = (historyHead + historyCount) % MAX_HISTORY_SIZE;
        ++historyCount;
    } else {
        // overwrite oldest
        pos = historyHead;
        historyHead = (historyHead + 1) % MAX_HISTORY_SIZE;
    }
    decisionHistory[pos] = decision;
    if (decision.type == Decision::ORDER_TRIGGERED) {
        std::cout << "[ORDER] "
                  << (side == 'B' ? "BUY" : "SELL")
                  << " " << decision.orderSize
                  << " @ $" << std::fixed << std::setprecision(2)
                  << (decision.quotePrice / 100.0)
                  << " (VWAP: $" << (decision.vwap / 100.0) << ")"
                  << " Reason: " << decision.reason
                  << std::endl;
    }
}

OrderMessage DecisionEngine::buildOrder(const QuoteMessage& quote, uint32_t orderSize) const {
    OrderMessage order;

    std::memcpy(order.symbol, symbol.c_str(),
                std::min(symbol.length(), sizeof(order.symbol)));

    for (size_t i = symbol.length(); i < sizeof(order.symbol); i++) {
        order.symbol[i] = '\0';
    }

    order.timestamp = quote.timestamp;

    order.side = side;

    order.quantity = orderSize;

    if (side == 'B') {
        order.price = static_cast<int32_t>(quote.askPrice);
    } else {
        order.price = static_cast<int32_t>(quote.bidPrice);
    }

    return order;
}

void DecisionEngine::printStatistics() const {
    std::cout << "\n=== Decision Engine Statistics ===" << std::endl;
    std::cout << "State: ";
    switch (currentState) {
        case TradingState::WAITING_FOR_FIRST_WINDOW:
            std::cout << "WAITING_FOR_FIRST_WINDOW";
            break;
        case TradingState::READY_TO_TRADE:
            std::cout << "READY_TO_TRADE";
            break;
        case TradingState::ORDER_SENT:
            std::cout << "ORDER_SENT";
            break;
    }
    std::cout << std::endl;
    std::cout << "Quotes Processed: " << quotesProcessed << std::endl;
    std::cout << "Orders Triggered: " << ordersTriggered << std::endl;
    std::cout << "Orders Rejected: " << ordersRejected << std::endl;
    if (ordersRejected) {
        std::cout << "  - Waiting Window:    " << rejWaitingWindow << std::endl;
        std::cout << "  - Price Unfavorable: " << rejPriceUnfavorable << std::endl;
        std::cout << "  - Cooldown:          " << rejCooldown << std::endl;
        std::cout << "  - Duplicate:         " << rejDuplicate << std::endl;
    }

    if (quotesProcessed > 0) {
        double triggerRate = (100.0 * ordersTriggered) / quotesProcessed;
        std::cout << "Trigger Rate: " << std::fixed << std::setprecision(2)
                  << triggerRate << "%" << std::endl;
    }
    std::cout << "Eval Latency Max(ns): " << evalLatencyMaxNanos << " Last(ns): " << evalLatencyLastNanos << " SpikeThresh(ns): " << spikeThresholdNanos << std::endl;
    std::cout << "Buckets(ns <=): ";
    for (size_t i=0;i<LAT_BUCKETS;++i) {
        std::cout << LAT_THRESHOLDS[i] << ":" << latencyBucketCounts[i] << " ";
    }
    std::cout << ">" << LAT_THRESHOLDS[LAT_BUCKETS-1] << ":" << latencyBucketCounts[LAT_BUCKETS] << std::endl;
    std::cout << "=================================" << std::endl;
}
