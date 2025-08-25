#include "decision_engine.h"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include "metrics.h"

bool DecisionEngine::QuoteIdentifier::operator==(const QuoteIdentifier& other) const noexcept {
    return timestamp == other.timestamp &&
           price == other.price &&
           quantity == other.quantity;
}

DecisionEngine::DecisionEngine(const std::string& symbol, char side, uint32_t maxOrderSize, uint64_t cooldownNanos)
        : symbol(symbol),
            side(side),
            maxOrderSize(maxOrderSize),
            currentState(TradingState::WAITING_FOR_FIRST_WINDOW),
            lastProcessedQuote{0, 0, 0},
            lastOrderTimestamp(0),
            cooldownNanos(cooldownNanos),
            quotesProcessed(0),
            ordersTriggered(0),
            ordersRejected(0),
            rejWaitingWindow(0),
            rejPriceUnfavorable(0),
            rejCooldown(0),
            rejDuplicate(0) {}

void DecisionEngine::onVwapWindowComplete() {
    if (currentState == TradingState::WAITING_FOR_FIRST_WINDOW) {
        currentState = TradingState::READY_TO_TRADE;
        std::cout << "Decision Engine: First VWAP window complete, ready to trade" << std::endl;
    }
}

Optional<OrderMessage> DecisionEngine::evaluateQuote(const QuoteMessage& quote, double vwap) {
    quotesProcessed++;
    extern SystemMetrics g_systemMetrics;
    g_systemMetrics.hot.quotesProcessed.fetch_add(1, std::memory_order_relaxed);
    auto wallStart = std::chrono::steady_clock::now();
    uint64_t currentTime = quote.timestamp;
    struct LatencyScope {
        std::chrono::steady_clock::time_point start;
        ~LatencyScope() {
            auto end = std::chrono::steady_clock::now();
            uint64_t nanos = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
            if (nanos > 0) g_metricsView.updateLatency(nanos);
        }
    } scope{wallStart};

    if (currentState == TradingState::WAITING_FOR_FIRST_WINDOW) {
    recordDecision({
            Decision::REJECTED_WAITING_WINDOW,
            currentTime,
            0.0,
            vwap,
            0,
            0,
            "Waiting for first VWAP window"
        });
    ++ordersRejected; ++rejWaitingWindow;
    return Optional<OrderMessage>();
    }

    if (isDuplicateQuote(quote)) {
    recordDecision({
            Decision::REJECTED_DUPLICATE,
            currentTime,
            0.0,
            vwap,
            0,
            0,
            "Duplicate quote"
        });
    ++ordersRejected; ++rejDuplicate;
    return Optional<OrderMessage>();
    }

    if (isInCooldown(currentTime)) {
    recordDecision({
            Decision::REJECTED_COOLDOWN,
            currentTime,
            0.0,
            vwap,
            0,
            0,
            "In cooldown period"
        });
    ++ordersRejected; ++rejCooldown;
    return Optional<OrderMessage>();
    }

    double relevantPrice;
    uint32_t relevantQuantity;

    if (side == 'B') {
        relevantPrice = quote.askPrice;
        relevantQuantity = quote.askQuantity;
    } else {
        relevantPrice = quote.bidPrice;
        relevantQuantity = quote.bidQuantity;
    }

    if (!shouldTriggerOrder(quote, vwap)) {
    recordDecision({
            Decision::REJECTED_PRICE_UNFAVORABLE,
            currentTime,
            relevantPrice,
            vwap,
            relevantQuantity,
            0,
            side == 'B' ? "Ask >= VWAP" : "Bid <= VWAP"
        });
    ++ordersRejected; ++rejPriceUnfavorable;
    return Optional<OrderMessage>();
    }

    uint32_t orderSize = calculateOrderSize(relevantQuantity);

    OrderMessage order = buildOrder(quote, orderSize);

    currentState = TradingState::ORDER_SENT;
    lastOrderTimestamp = currentTime;
    lastProcessedQuote = {quote.timestamp, static_cast<int32_t>(relevantPrice), relevantQuantity};

    recordDecision({
        Decision::ORDER_TRIGGERED,
        currentTime,
        relevantPrice,
        vwap,
        relevantQuantity,
        orderSize,
        side == 'B' ? "Buy: Ask < VWAP" : "Sell: Bid > VWAP"
    });

    ordersTriggered++;

    currentState = TradingState::READY_TO_TRADE;

    return Optional<OrderMessage>(order);
}

bool DecisionEngine::shouldTriggerOrder(const QuoteMessage& quote, double vwap) const noexcept {
    if (vwap <= 0) {
        return false;
    }

    if (side == 'B') {
        return static_cast<double>(quote.askPrice) < vwap;
    } else {
        return static_cast<double>(quote.bidPrice) > vwap;
    }
}

uint32_t DecisionEngine::calculateOrderSize(uint32_t quoteSize) const noexcept {
    return std::min(quoteSize, maxOrderSize);
}

bool DecisionEngine::isDuplicateQuote(const QuoteMessage& quote) const noexcept {
    QuoteIdentifier current;

    if (side == 'B') {
        current = {quote.timestamp, static_cast<int32_t>(quote.askPrice), quote.askQuantity};
    } else {
        current = {quote.timestamp, static_cast<int32_t>(quote.bidPrice), quote.bidQuantity};
    }

    return current == lastProcessedQuote;
}

bool DecisionEngine::isInCooldown(uint64_t currentTime) const noexcept {
    if (lastOrderTimestamp == 0) {
        return false;
    }
    return (currentTime - lastOrderTimestamp) < cooldownNanos;
}

void DecisionEngine::recordDecision(const Decision& decision) {
    decisionHistory.push_back(decision);

    while (decisionHistory.size() > MAX_HISTORY_SIZE) {
        decisionHistory.pop_front();
    }

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
    std::cout << "=================================" << std::endl;
}
