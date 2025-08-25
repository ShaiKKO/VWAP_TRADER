#include "message_parser.h"
#include "endian_converter.h"
#include "wire_format.h"
#include <cstring>
#include <algorithm>
#include "metrics.h"

static_assert(sizeof(QuoteMessage) == WireFormat::QUOTE_SIZE, "Native Quote size mismatch wire size");
static_assert(sizeof(TradeMessage) == WireFormat::TRADE_SIZE, "Native Trade size mismatch wire size");

bool MessageParser::parseHeader(const uint8_t* buffer, size_t bufferSize, MessageHeader& header) noexcept {
    if (bufferSize < WireFormat::HEADER_SIZE) {
        return false;
    }

    header.length = buffer[0];
    header.type = buffer[1];

    return true;
}

bool MessageParser::parseQuote(const uint8_t* buffer, size_t bufferSize, QuoteMessage& quote) noexcept {
    if (bufferSize < WireFormat::QUOTE_SIZE) {
        return false;
    }

    std::memset(&quote, 0, sizeof(quote));

    std::memcpy(quote.symbol, buffer + WireFormat::QUOTE_SYMBOL_OFFSET, 8);

    uint64_t timestamp;
    std::memcpy(&timestamp, buffer + WireFormat::QUOTE_TIMESTAMP_OFFSET, sizeof(uint64_t));
    quote.timestamp = EndianConverter::ltoh64(timestamp);

    uint32_t bidQty;
    std::memcpy(&bidQty, buffer + WireFormat::QUOTE_BID_QTY_OFFSET, sizeof(uint32_t));
    quote.bidQuantity = EndianConverter::ltoh32(bidQty);

    uint32_t bidPrice;
    std::memcpy(&bidPrice, buffer + WireFormat::QUOTE_BID_PRICE_OFFSET, sizeof(uint32_t));
    quote.bidPrice = EndianConverter::ltoh32(bidPrice);

    uint32_t askQty;
    std::memcpy(&askQty, buffer + WireFormat::QUOTE_ASK_QTY_OFFSET, sizeof(uint32_t));
    quote.askQuantity = EndianConverter::ltoh32(askQty);

    int32_t askPrice;
    std::memcpy(&askPrice, buffer + WireFormat::QUOTE_ASK_PRICE_OFFSET, sizeof(int32_t));
    quote.askPrice = EndianConverter::ltoh32_signed(askPrice);

    return true;
}

bool MessageParser::parseTrade(const uint8_t* buffer, size_t bufferSize, TradeMessage& trade) noexcept {
    if (bufferSize < WireFormat::TRADE_SIZE) {
        return false;
    }

    std::memset(&trade, 0, sizeof(trade));

    std::memcpy(trade.symbol, buffer + WireFormat::TRADE_SYMBOL_OFFSET, 8);

    uint64_t timestamp;
    std::memcpy(&timestamp, buffer + WireFormat::TRADE_TIMESTAMP_OFFSET, sizeof(uint64_t));
    trade.timestamp = EndianConverter::ltoh64(timestamp);

    uint32_t quantity;
    std::memcpy(&quantity, buffer + WireFormat::TRADE_QUANTITY_OFFSET, sizeof(uint32_t));
    trade.quantity = EndianConverter::ltoh32(quantity);

    int32_t price;
    std::memcpy(&price, buffer + WireFormat::TRADE_PRICE_OFFSET, sizeof(int32_t));
    trade.price = EndianConverter::ltoh32_signed(price);

    return true;
}

bool MessageParser::validateHeader(const MessageHeader& header) noexcept {

    if (header.type != MessageHeader::QUOTE_TYPE &&
        header.type != MessageHeader::TRADE_TYPE) {
        return false;
    }

    if (header.type == MessageHeader::QUOTE_TYPE &&
        header.length != WireFormat::QUOTE_SIZE) {
        return false;
    }

    if (header.type == MessageHeader::TRADE_TYPE &&
        header.length != WireFormat::TRADE_SIZE) {
        return false;
    }

    return true;
}

bool MessageParser::validateQuote(const QuoteMessage& quote) noexcept {
    if (quote.bidQuantity == 0 || quote.askQuantity == 0) return false;
    if (quote.askPrice < 0 || static_cast<int32_t>(quote.bidPrice) < 0) return false;
    if (static_cast<int32_t>(quote.bidPrice) > quote.askPrice) return false;
    return true;
}

bool MessageParser::validateTrade(const TradeMessage& trade) noexcept {
    if (trade.quantity == 0) return false;
    if (trade.price < 0) return false;
    return true;
}

bool MessageParser::parseOrder(const uint8_t* buffer, size_t bufferSize, OrderMessage& order) noexcept {
    if (bufferSize < WireFormat::ORDER_SIZE) {
        return false;
    }

    std::memcpy(order.symbol, buffer + WireFormat::ORDER_SYMBOL_OFFSET, 8);

    uint64_t timestamp;
    std::memcpy(&timestamp, buffer + WireFormat::ORDER_TIMESTAMP_OFFSET, sizeof(uint64_t));
    order.timestamp = EndianConverter::ltoh64(timestamp);

    order.side = buffer[WireFormat::ORDER_SIDE_OFFSET];

    uint32_t quantity;
    std::memcpy(&quantity, buffer + WireFormat::ORDER_QUANTITY_OFFSET, sizeof(uint32_t));
    order.quantity = EndianConverter::ltoh32(quantity);

    int32_t price;
    std::memcpy(&price, buffer + WireFormat::ORDER_PRICE_OFFSET, sizeof(int32_t));
    order.price = EndianConverter::ltoh32_signed(price);

    std::memset(order._padding, 0, sizeof(order._padding));

    return true;
}

bool MessageParser::validateOrder(const OrderMessage& order) noexcept {

    if (order.side != 'B' && order.side != 'S') {
        return false;
    }
    return true;
}

bool MessageParser::validateSymbol(const char* symbol, const char* expectedSymbol) noexcept {

    return std::memcmp(symbol, expectedSymbol, 8) == 0;
}
