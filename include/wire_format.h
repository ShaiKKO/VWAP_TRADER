#ifndef WIRE_FORMAT_H
#define WIRE_FORMAT_H

#include <cstddef>
#include <cstdint>

namespace WireFormat {
    constexpr size_t HEADER_SIZE = 2;
    constexpr size_t QUOTE_SIZE = 32;
    constexpr size_t TRADE_SIZE = 24;
    constexpr size_t ORDER_SIZE = 25;
    
    constexpr size_t QUOTE_SYMBOL_OFFSET = 0;
    constexpr size_t QUOTE_TIMESTAMP_OFFSET = 8;
    constexpr size_t QUOTE_BID_QTY_OFFSET = 16;
    constexpr size_t QUOTE_BID_PRICE_OFFSET = 20;
    constexpr size_t QUOTE_ASK_QTY_OFFSET = 24;
    constexpr size_t QUOTE_ASK_PRICE_OFFSET = 28;
    
    constexpr size_t TRADE_SYMBOL_OFFSET = 0;
    constexpr size_t TRADE_TIMESTAMP_OFFSET = 8;
    constexpr size_t TRADE_QUANTITY_OFFSET = 16;
    constexpr size_t TRADE_PRICE_OFFSET = 20;
    
    // Order wire offsets (NOTE: unaligned fields!)
    constexpr size_t ORDER_SYMBOL_OFFSET = 0;
    constexpr size_t ORDER_TIMESTAMP_OFFSET = 8;
    constexpr size_t ORDER_SIDE_OFFSET = 16;
    constexpr size_t ORDER_QUANTITY_OFFSET = 17; // UNALIGNED!
    constexpr size_t ORDER_PRICE_OFFSET = 21;
    
    constexpr size_t MAX_MESSAGE_SIZE = HEADER_SIZE + QUOTE_SIZE;
}

static_assert(WireFormat::QUOTE_ASK_PRICE_OFFSET + 4 == WireFormat::QUOTE_SIZE, 
              "Quote wire format size mismatch");
static_assert(WireFormat::TRADE_PRICE_OFFSET + 4 == WireFormat::TRADE_SIZE, 
              "Trade wire format size mismatch");
static_assert(WireFormat::ORDER_PRICE_OFFSET + 4 == WireFormat::ORDER_SIZE, 
              "Order wire format size mismatch");

static_assert(WireFormat::ORDER_QUANTITY_OFFSET == 17, 
              "Order quantity must be at unaligned offset 17");
static_assert(WireFormat::ORDER_QUANTITY_OFFSET % 4 != 0, 
              "Order quantity offset is intentionally unaligned");

#endif // WIRE_FORMAT_H