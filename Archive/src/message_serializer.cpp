#include "message_serializer.h"
#include "endian_converter.h"
#include "wire_format.h"
#include <cstring>

static_assert(WireFormat::QUOTE_SYMBOL_OFFSET == 0, "Quote symbol offset");
static_assert(WireFormat::QUOTE_ASK_PRICE_OFFSET + 4 == WireFormat::QUOTE_SIZE, "Quote size final field");
static_assert(WireFormat::TRADE_PRICE_OFFSET + 4 == WireFormat::TRADE_SIZE, "Trade size final field");
static_assert(WireFormat::ORDER_PRICE_OFFSET + 4 == WireFormat::ORDER_SIZE, "Order size final field");

size_t MessageSerializer::serializeHeader(uint8_t* buffer, size_t bufferSize, const MessageHeader& header) noexcept {
    if (bufferSize < WireFormat::HEADER_SIZE) {
        return 0;
    }

    buffer[0] = header.length;
    buffer[1] = header.type;

    return WireFormat::HEADER_SIZE;
}

size_t MessageSerializer::serializeQuote(uint8_t* buffer, size_t bufferSize, const QuoteMessage& quote) noexcept {
    if (bufferSize < WireFormat::QUOTE_SIZE) {
        return 0;
    }

    std::memcpy(buffer + WireFormat::QUOTE_SYMBOL_OFFSET, quote.symbol, 8);

    uint64_t timestamp = EndianConverter::htol64(quote.timestamp);
    std::memcpy(buffer + WireFormat::QUOTE_TIMESTAMP_OFFSET, &timestamp, sizeof(uint64_t));

    uint32_t bidQty = EndianConverter::htol32(quote.bidQuantity);
    std::memcpy(buffer + WireFormat::QUOTE_BID_QTY_OFFSET, &bidQty, sizeof(uint32_t));

    uint32_t bidPrice = EndianConverter::htol32(quote.bidPrice);
    std::memcpy(buffer + WireFormat::QUOTE_BID_PRICE_OFFSET, &bidPrice, sizeof(uint32_t));

    uint32_t askQty = EndianConverter::htol32(quote.askQuantity);
    std::memcpy(buffer + WireFormat::QUOTE_ASK_QTY_OFFSET, &askQty, sizeof(uint32_t));

    int32_t askPrice = EndianConverter::htol32_signed(quote.askPrice);
    std::memcpy(buffer + WireFormat::QUOTE_ASK_PRICE_OFFSET, &askPrice, sizeof(int32_t));

    return WireFormat::QUOTE_SIZE;
}

size_t MessageSerializer::serializeTrade(uint8_t* buffer, size_t bufferSize, const TradeMessage& trade) noexcept {
    if (bufferSize < WireFormat::TRADE_SIZE) {
        return 0;
    }

    std::memcpy(buffer + WireFormat::TRADE_SYMBOL_OFFSET, trade.symbol, 8);

    uint64_t timestamp = EndianConverter::htol64(trade.timestamp);
    std::memcpy(buffer + WireFormat::TRADE_TIMESTAMP_OFFSET, &timestamp, sizeof(uint64_t));

    uint32_t quantity = EndianConverter::htol32(trade.quantity);
    std::memcpy(buffer + WireFormat::TRADE_QUANTITY_OFFSET, &quantity, sizeof(uint32_t));

    int32_t price = EndianConverter::htol32_signed(trade.price);
    std::memcpy(buffer + WireFormat::TRADE_PRICE_OFFSET, &price, sizeof(int32_t));

    return WireFormat::TRADE_SIZE;
}

size_t MessageSerializer::serializeOrder(uint8_t* buffer, size_t bufferSize, const OrderMessage& order) noexcept {
    if (bufferSize < WireFormat::ORDER_SIZE) {
        return 0;
    }

    std::memcpy(buffer + WireFormat::ORDER_SYMBOL_OFFSET, order.symbol, 8);

    uint64_t timestamp = EndianConverter::htol64(order.timestamp);
    std::memcpy(buffer + WireFormat::ORDER_TIMESTAMP_OFFSET, &timestamp, sizeof(uint64_t));

    buffer[WireFormat::ORDER_SIDE_OFFSET] = order.side;

    uint32_t quantity = EndianConverter::htol32(order.quantity);
    std::memcpy(buffer + WireFormat::ORDER_QUANTITY_OFFSET, &quantity, sizeof(uint32_t));

    int32_t price = EndianConverter::htol32_signed(order.price);
    std::memcpy(buffer + WireFormat::ORDER_PRICE_OFFSET, &price, sizeof(int32_t));

    return WireFormat::ORDER_SIZE;
}

size_t MessageSerializer::serializeQuoteMessage(uint8_t* buffer, size_t bufferSize, const QuoteMessage& quote) noexcept {
    if (bufferSize < WireFormat::HEADER_SIZE + WireFormat::QUOTE_SIZE) {
        return 0;
    }

    MessageHeader header;
    header.length = WireFormat::QUOTE_SIZE;
    header.type = MessageHeader::QUOTE_TYPE;

    size_t headerSize = serializeHeader(buffer, bufferSize, header);
    if (headerSize == 0) {
        return 0;
    }

    size_t bodySize = serializeQuote(buffer + headerSize, bufferSize - headerSize, quote);
    if (bodySize == 0) {
        return 0;
    }

    return headerSize + bodySize;
}

size_t MessageSerializer::serializeTradeMessage(uint8_t* buffer, size_t bufferSize, const TradeMessage& trade) noexcept {
    if (bufferSize < WireFormat::HEADER_SIZE + WireFormat::TRADE_SIZE) {
        return 0;
    }

    MessageHeader header;
    header.length = WireFormat::TRADE_SIZE;
    header.type = MessageHeader::TRADE_TYPE;

    size_t headerSize = serializeHeader(buffer, bufferSize, header);
    if (headerSize == 0) {
        return 0;
    }

    size_t bodySize = serializeTrade(buffer + headerSize, bufferSize - headerSize, trade);
    if (bodySize == 0) {
        return 0;
    }

    return headerSize + bodySize;
}

