#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include "optional.h"
#include <string>
#include <array>
#include "message.h"

class DecisionEngine final {
public:
    enum class TradingState { WAITING_FOR_FIRST_WINDOW, READY_TO_TRADE, ORDER_SENT };
    struct Decision { enum Type { NO_ACTION, ORDER_TRIGGERED, REJECTED_WAITING_WINDOW, REJECTED_PRICE_UNFAVORABLE, REJECTED_COOLDOWN, REJECTED_DUPLICATE }; Type type; uint64_t timestamp; double quotePrice; double vwap; uint32_t quoteSize; uint32_t orderSize; static constexpr size_t REASON_MAX = 64; char reason[REASON_MAX]; };
private:
    std::string symbol; char side; uint32_t maxOrderSize; TradingState currentState;
    struct QuoteIdentifier { uint64_t timestamp; int32_t price; uint32_t quantity; bool operator==(const QuoteIdentifier& other) const noexcept; };
    QuoteIdentifier lastProcessedQuote; uint64_t lastOrderTimestamp; uint64_t cooldownNanos;
    static constexpr size_t MAX_HISTORY_SIZE = 1024; std::array<Decision, MAX_HISTORY_SIZE> decisionHistory; size_t historyHead = 0; size_t historyCount = 0;
    uint64_t quotesProcessed; uint64_t ordersTriggered; uint64_t ordersRejected; uint64_t rejWaitingWindow; uint64_t rejPriceUnfavorable; uint64_t rejCooldown; uint64_t rejDuplicate;
    uint64_t lastQuoteFingerprint = 0; using Predicate = bool(*)(double,double); Predicate triggerPredicate = nullptr; static uint64_t fingerprint(uint64_t ts, int32_t price, uint32_t qty) noexcept { return ts ^ (uint64_t(uint32_t(price)) << 32) ^ qty; }
    // Latency diagnostics
    uint64_t evalLatencyMaxNanos = 0;
    uint64_t evalLatencyLastNanos = 0;
    uint64_t spikeThresholdNanos = 5'000;
    static constexpr size_t LAT_BUCKETS = 5;
    static constexpr uint64_t LAT_THRESHOLDS[LAT_BUCKETS] = { 5'000, 10'000, 20'000, 50'000, 100'000 };
    uint64_t latencyBucketCounts[LAT_BUCKETS + 1] = {0};
public:
    DecisionEngine(const std::string& symbol, char side, uint32_t maxOrderSize, uint64_t cooldownNanos = 100'000'000ULL);
    void onVwapWindowComplete();
    bool evaluateQuote(const QuoteMessage& quote, double vwap, OrderMessage& outOrder) noexcept;
    bool isReady() const noexcept { return currentState != TradingState::WAITING_FOR_FIRST_WINDOW; }
    void printStatistics() const;
    uint64_t getRejWaitingWindow() const noexcept { return rejWaitingWindow; }
    uint64_t getRejPrice() const noexcept { return rejPriceUnfavorable; }
    uint64_t getRejCooldown() const noexcept { return rejCooldown; }
    uint64_t getRejDuplicate() const noexcept { return rejDuplicate; }
    uint64_t getEvalLatencyMax() const noexcept { return evalLatencyMaxNanos; }
    uint64_t getEvalLatencyLast() const noexcept { return evalLatencyLastNanos; }
    uint64_t getSpikeThreshold() const noexcept { return spikeThresholdNanos; }
    const uint64_t* getLatencyBuckets() const noexcept { return latencyBucketCounts; }
    static constexpr size_t latencyBucketCount() noexcept { return LAT_BUCKETS; }
    static constexpr const uint64_t* latencyBucketThresholds() noexcept { return LAT_THRESHOLDS; }
    size_t getRecentDecisions(Decision* out, size_t max) const noexcept { size_t n = historyCount; if (n > max) n = max; for (size_t i=0;i<n;++i){ size_t logicalIndex = historyCount - n + i; size_t pos = (historyHead + logicalIndex) % MAX_HISTORY_SIZE; out[i] = decisionHistory[pos]; } return n; }
private:
    bool shouldTriggerOrder(const QuoteMessage& quote, double vwap) const noexcept;
    uint32_t calculateOrderSize(uint32_t quoteSize) const noexcept;
    bool isDuplicateQuote(const QuoteMessage& quote) const noexcept;
    bool isInCooldown(uint64_t currentTime) const noexcept;
    void recordDecision(const Decision& decision) noexcept;
    OrderMessage buildOrder(const QuoteMessage& quote, uint32_t orderSize) const;
};

#endif
