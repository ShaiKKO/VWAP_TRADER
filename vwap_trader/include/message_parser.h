#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <cstddef>
#include <cstdint>
#include "message.h"

class MessageParser {
public:
    static bool parseHeader(const uint8_t* buffer, size_t bufferSize, MessageHeader& header);
    static bool parseQuote(const uint8_t* buffer, size_t bufferSize, QuoteMessage& quote);
    static bool parseTrade(const uint8_t* buffer, size_t bufferSize, TradeMessage& trade);

    static bool validateHeader(const MessageHeader& header);
    static bool validateQuote(const QuoteMessage& quote);
    static bool validateTrade(const TradeMessage& trade);
    static bool validateSymbol(const char* symbol, const char* expectedSymbol);
};

#endif // MESSAGE_PARSER_H
