#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

struct alignas(1) MessageHeader {
    uint8_t length;
    uint8_t type;
    uint8_t reserved;
    
    static constexpr uint8_t QUOTE_TYPE = 1;
    static constexpr uint8_t TRADE_TYPE = 2;
    static constexpr uint8_t ORDER_TYPE = 3;
    static constexpr size_t SIZE = 3;
};

struct alignas(1) QuoteMessage {
    char symbol[8];
    uint64_t timestamp;
    uint32_t bidQuantity;
    uint32_t bidPrice;
    uint32_t askQuantity;
    int32_t askPrice;
    
    static constexpr size_t SIZE = 32;
    
    constexpr QuoteMessage() noexcept
        : symbol{}, timestamp(0), bidQuantity(0), bidPrice(0), askQuantity(0), askPrice(0) {}
};

struct alignas(1) TradeMessage {
    uint64_t timestamp;
    char symbol[4];
    uint32_t quantity;
    int32_t price;
    
    static constexpr size_t SIZE = 20;
    
    constexpr TradeMessage() noexcept
        : timestamp(0), symbol{}, quantity(0), price(0) {}
};

struct alignas(1) OrderMessage {
    uint64_t timestamp;
    char symbol[4];
    char side;
    uint32_t quantity;
    int32_t price;
    
    static constexpr size_t SIZE = 21;
    
    constexpr OrderMessage() noexcept
        : timestamp(0), symbol{}, side(0), quantity(0), price(0) {}
};

#pragma pack(pop)

static_assert(sizeof(MessageHeader) == MessageHeader::SIZE, "MessageHeader size mismatch");
static_assert(sizeof(QuoteMessage) == QuoteMessage::SIZE, "QuoteMessage size mismatch");
static_assert(sizeof(TradeMessage) == TradeMessage::SIZE, "TradeMessage size mismatch");
static_assert(sizeof(OrderMessage) == OrderMessage::SIZE, "OrderMessage size mismatch");

#endif // MESSAGE_H