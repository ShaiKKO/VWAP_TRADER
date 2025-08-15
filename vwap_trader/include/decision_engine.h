#ifndef DECISION_ENGINE_H
#define DECISION_ENGINE_H

#include "optional.h"
#include <string>
#include <deque>
#include "message.h"

class DecisionEngine final {
public:
    enum class TradingState {
        WAITING_FOR_FIRST_WINDOW,
        READY_TO_TRADE,
        ORDER_SENT
    };
    
    struct Decision {
        enum Type {
            NO_ACTION,
            ORDER_TRIGGERED,
            REJECTED_WAITING_WINDOW,
            REJECTED_PRICE_UNFAVORABLE,
            REJECTED_COOLDOWN,
            REJECTED_DUPLICATE
        };
        
        Type type;
        uint64_t timestamp;
        double quotePrice;
        double vwap;
        uint32_t quoteSize;
        uint32_t orderSize;
        std::string reason;
    };
    
private:
    std::string symbol;
    char side;
    uint32_t maxOrderSize;
    TradingState currentState;
    
    struct QuoteIdentifier {
        uint64_t timestamp;
        int32_t price;
        uint32_t quantity;
        
        bool operator==(const QuoteIdentifier& other) const noexcept;
    };
    
    QuoteIdentifier lastProcessedQuote;
    uint64_t lastOrderTimestamp;
    static constexpr uint64_t COOLDOWN_NANOS = 100'000'000;
    
    std::deque<Decision> decisionHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    uint64_t quotesProcessed;
    uint64_t ordersTriggered;
    uint64_t ordersRejected;
    
public:
    DecisionEngine(const std::string& symbol, char side, uint32_t maxOrderSize);
    void onVwapWindowComplete();
    Optional<OrderMessage> evaluateQuote(const QuoteMessage& quote, double vwap);
    bool isReady() const noexcept { return currentState != TradingState::WAITING_FOR_FIRST_WINDOW; }
    void printStatistics() const;
    
private:
    bool shouldTriggerOrder(const QuoteMessage& quote, double vwap) const noexcept;
    uint32_t calculateOrderSize(uint32_t quoteSize) const noexcept;
    bool isDuplicateQuote(const QuoteMessage& quote) const noexcept;
    bool isInCooldown(uint64_t currentTime) const noexcept;
    void recordDecision(const Decision& decision);
    OrderMessage buildOrder(const QuoteMessage& quote, uint32_t orderSize) const;
};

#endif // DECISION_ENGINE_H