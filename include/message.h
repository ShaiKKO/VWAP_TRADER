#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <cstring>
#include <cstddef>
#include <type_traits>

struct MessageHeader {
    uint8_t length;
    uint8_t type;
    
    static constexpr uint8_t QUOTE_TYPE = 1;
    static constexpr uint8_t TRADE_TYPE = 2;
    // NO ORDER_TYPE - Orders are sent as 25 bytes without any header per spec
};

struct QuoteMessage {
    char symbol[8];
    uint64_t timestamp;
    uint32_t bidQuantity;
    uint32_t bidPrice;
    uint32_t askQuantity;
    int32_t askPrice;
};

struct TradeMessage {
    char symbol[8];
    uint64_t timestamp;
    uint32_t quantity;
    int32_t price;
};

struct OrderMessage {
    char symbol[8];
    uint64_t timestamp;
    uint32_t quantity;
    int32_t price;
    char side;
    char _padding[3];
    
    OrderMessage() noexcept {
        std::memset(this, 0, sizeof(*this));
    }
};

static_assert(sizeof(MessageHeader) == 2, "MessageHeader must be 2 bytes");
static_assert(sizeof(QuoteMessage) == 32, "QuoteMessage must be 32 bytes");
static_assert(sizeof(TradeMessage) == 24, "TradeMessage must be 24 bytes");
static_assert(sizeof(OrderMessage) == 32, "OrderMessage must be 32 bytes (28 bytes + 4 alignment)");

static_assert(offsetof(MessageHeader, type) == 1, "Header type at offset 1");
static_assert(offsetof(QuoteMessage, timestamp) == 8, "Quote timestamp at offset 8");
static_assert(offsetof(TradeMessage, timestamp) == 8, "Trade timestamp at offset 8");
static_assert(offsetof(TradeMessage, quantity) == 16, "Trade quantity at offset 16");
static_assert(offsetof(TradeMessage, price) == 20, "Trade price at offset 20");
static_assert(offsetof(OrderMessage, timestamp) == 8, "Order timestamp at offset 8");
static_assert(offsetof(OrderMessage, quantity) == 16, "Order quantity at offset 16");
static_assert(offsetof(OrderMessage, price) == 20, "Order price at offset 20");
static_assert(offsetof(OrderMessage, side) == 24, "Order side at offset 24");

static_assert(std::is_trivially_copyable<MessageHeader>::value, "MessageHeader must be trivially copyable");
static_assert(std::is_trivially_copyable<QuoteMessage>::value, "QuoteMessage must be trivially copyable");
static_assert(std::is_trivially_copyable<TradeMessage>::value, "TradeMessage must be trivially copyable");
static_assert(std::is_trivially_copyable<OrderMessage>::value, "OrderMessage must be trivially copyable");

#endif // MESSAGE_H