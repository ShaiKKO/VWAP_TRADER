#ifndef MESSAGE_SERIALIZER_H
#define MESSAGE_SERIALIZER_H

#include <cstddef>
#include <cstdint>
#include "message.h"

class MessageSerializer {
public:

    static size_t serializeHeader(uint8_t* buffer, size_t bufferSize, const MessageHeader& header) noexcept;
    static size_t serializeQuote(uint8_t* buffer, size_t bufferSize, const QuoteMessage& quote) noexcept;
    static size_t serializeTrade(uint8_t* buffer, size_t bufferSize, const TradeMessage& trade) noexcept;
    static size_t serializeOrder(uint8_t* buffer, size_t bufferSize, const OrderMessage& order) noexcept;

    static size_t serializeQuoteMessage(uint8_t* buffer, size_t bufferSize, const QuoteMessage& quote) noexcept;
    static size_t serializeTradeMessage(uint8_t* buffer, size_t bufferSize, const TradeMessage& trade) noexcept;

};

#endif
