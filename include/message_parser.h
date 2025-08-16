#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <cstddef>
#include <cstdint>
#include "message.h"

class MessageParser {
public:
    static bool parseHeader(const uint8_t* buffer, size_t bufferSize, MessageHeader& header) noexcept;
    static bool parseQuote(const uint8_t* buffer, size_t bufferSize, QuoteMessage& quote) noexcept;
    static bool parseTrade(const uint8_t* buffer, size_t bufferSize, TradeMessage& trade) noexcept;
    static bool parseOrder(const uint8_t* buffer, size_t bufferSize, OrderMessage& order) noexcept;

    static bool validateHeader(const MessageHeader& header) noexcept;
    static bool validateQuote(const QuoteMessage& quote) noexcept;
    static bool validateTrade(const TradeMessage& trade) noexcept;
    static bool validateOrder(const OrderMessage& order) noexcept;
    static bool validateSymbol(const char* symbol, const char* expectedSymbol) noexcept;

    template<typename QCB, typename TCB>
    static bool dispatch(const MessageHeader& header, const uint8_t* body, size_t bodySize,
                         QCB&& onQuote, TCB&& onTrade) noexcept {
        if (!validateHeader(header) || bodySize < header.length) return false;
        if (header.type == MessageHeader::QUOTE_TYPE) {
            QuoteMessage q; if (!parseQuote(body, bodySize, q) || !validateQuote(q)) return false; onQuote(q); return true;
        } else if (header.type == MessageHeader::TRADE_TYPE) {
            TradeMessage t; if (!parseTrade(body, bodySize, t) || !validateTrade(t)) return false; onTrade(t); return true;
        }
        return false;
    }
};

#endif
