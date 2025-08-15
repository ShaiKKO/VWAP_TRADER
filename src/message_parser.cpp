#include "message_parser.h"
#include "endian_converter.h"
#include <cstring>
#include <algorithm>

bool MessageParser::parseHeader(const uint8_t* buffer, size_t bufferSize, MessageHeader& header) {
    if (bufferSize < MessageHeader::SIZE) {
        return false;
    }
    
    header.length = buffer[0];
    header.type = buffer[1];
    header.reserved = buffer[2];  // Added reserved field
    
    return true;
}

bool MessageParser::parseQuote(const uint8_t* buffer, size_t bufferSize, QuoteMessage& quote) {
    if (bufferSize < QuoteMessage::SIZE) {
        return false;
    }
    
    std::memcpy(&quote, buffer, QuoteMessage::SIZE);
    
    quote.timestamp = EndianConverter::ltoh64(quote.timestamp);
    quote.bidQuantity = EndianConverter::ltoh32(quote.bidQuantity);
    quote.bidPrice = EndianConverter::ltoh32(quote.bidPrice);
    quote.askQuantity = EndianConverter::ltoh32(quote.askQuantity);
    quote.askPrice = EndianConverter::ltoh32_signed(quote.askPrice);
    
    return true;
}

bool MessageParser::parseTrade(const uint8_t* buffer, size_t bufferSize, TradeMessage& trade) {
    if (bufferSize < TradeMessage::SIZE) {
        return false;
    }
    
    std::memcpy(&trade, buffer, TradeMessage::SIZE);
    
    trade.timestamp = EndianConverter::ltoh64(trade.timestamp);
    trade.quantity = EndianConverter::ltoh32(trade.quantity);
    trade.price = EndianConverter::ltoh32_signed(trade.price);
    
    return true;
}

bool MessageParser::validateHeader(const MessageHeader& header) {
    if (header.type != MessageHeader::QUOTE_TYPE && 
        header.type != MessageHeader::TRADE_TYPE) {
        return false;
    }
    
    if (header.type == MessageHeader::QUOTE_TYPE && 
        header.length != QuoteMessage::SIZE) {
        return false;
    }
    
    if (header.type == MessageHeader::TRADE_TYPE && 
        header.length != TradeMessage::SIZE) {
        return false;
    }
    
    return true;
}

bool MessageParser::validateQuote(const QuoteMessage& quote) {
    if (quote.bidQuantity == 0 || quote.askQuantity == 0) {
        return false;
    }
    
    if (quote.bidPrice <= 0 || quote.askPrice <= 0) {
        return false;
    }
    
    return true;
}

bool MessageParser::validateTrade(const TradeMessage& trade) {
    if (trade.quantity == 0 || trade.price <= 0) {
        return false;
    }
    
    return true;
}

bool MessageParser::validateSymbol(const char* symbol, const char* expectedSymbol) {
    // Compare up to 4 bytes for Trade/Order, 8 for Quote
    size_t len = std::strlen(expectedSymbol);
    return std::strncmp(symbol, expectedSymbol, std::min(len, size_t(4))) == 0;
}