#ifndef MESSAGE_PARSER_H
#define MESSAGE_PARSER_H

#include <cstddef>
#include <cstdint>
#include "message.h"
#include "features.h"
#include "wire_format.h"
#include "endian_converter.h"

class MessageParser {
public:
    struct WireSpan {
        const uint8_t* ptr;
        uint8_t type;
        uint8_t length;
    };
    static bool parseHeader(const uint8_t* buffer, size_t bufferSize, MessageHeader& header) noexcept;
    static bool parseQuote(const uint8_t* buffer, size_t bufferSize, QuoteMessage& quote) noexcept;
    static bool parseTrade(const uint8_t* buffer, size_t bufferSize, TradeMessage& trade) noexcept;
    // Fast variants: assume bufferSize == exact wire size, no redundant length guard.
    static inline void parseQuoteFast(const uint8_t* b, QuoteMessage& q) noexcept {
        std::memcpy(q.symbol, b + WireFormat::QUOTE_SYMBOL_OFFSET, 8);
        uint64_t ts; std::memcpy(&ts, b + WireFormat::QUOTE_TIMESTAMP_OFFSET, 8); q.timestamp = EndianConverter::ltoh64(ts);
        uint32_t bidQty; std::memcpy(&bidQty, b + WireFormat::QUOTE_BID_QTY_OFFSET, 4); q.bidQuantity = EndianConverter::ltoh32(bidQty);
        uint32_t bidPx; std::memcpy(&bidPx, b + WireFormat::QUOTE_BID_PRICE_OFFSET, 4); q.bidPrice = EndianConverter::ltoh32(bidPx);
        uint32_t askQty; std::memcpy(&askQty, b + WireFormat::QUOTE_ASK_QTY_OFFSET, 4); q.askQuantity = EndianConverter::ltoh32(askQty);
        int32_t askPx; std::memcpy(&askPx, b + WireFormat::QUOTE_ASK_PRICE_OFFSET, 4); q.askPrice = EndianConverter::ltoh32_signed(askPx);
    }
    static inline void parseTradeFast(const uint8_t* b, TradeMessage& t) noexcept {
        std::memcpy(t.symbol, b + WireFormat::TRADE_SYMBOL_OFFSET, 8);
        uint64_t ts; std::memcpy(&ts, b + WireFormat::TRADE_TIMESTAMP_OFFSET, 8); t.timestamp = EndianConverter::ltoh64(ts);
        uint32_t qty; std::memcpy(&qty, b + WireFormat::TRADE_QUANTITY_OFFSET, 4); t.quantity = EndianConverter::ltoh32(qty);
        int32_t px; std::memcpy(&px, b + WireFormat::TRADE_PRICE_OFFSET, 4); t.price = EndianConverter::ltoh32_signed(px);
    }
    static bool parseOrder(const uint8_t* buffer, size_t bufferSize, OrderMessage& order) noexcept;

    static bool validateHeader(const MessageHeader& header) noexcept;
    static bool validateQuote(const QuoteMessage& quote) noexcept;
    static bool validateTrade(const TradeMessage& trade) noexcept;
    static bool validateOrder(const OrderMessage& order) noexcept;
    static bool validateSymbol(const char* symbol, const char* expectedSymbol) noexcept;
    static inline bool symbolsEqualFast(const char* a, const char* b) noexcept {
        if (Features::ENABLE_SYMBOL_INTERNING) { uint64_t x; std::memcpy(&x,a,8); uint64_t y; std::memcpy(&y,b,8); return x==y; }
        return std::memcmp(a,b,8)==0;
    }

    template<typename QCB, typename TCB>
    static bool dispatch(const MessageHeader& header, const uint8_t* body, size_t bodySize,
                         QCB&& onQuote, TCB&& onTrade) noexcept {
        if (!validateHeader(header) || bodySize < header.length) return false;
        if (Features::ENABLE_BRANCH_FUNNELING) {
            // Fast path: dispatch table of handlers.
            typedef bool (*Handler)(const uint8_t*, size_t, QCB&, TCB&);
            struct Entry { uint8_t type; Handler fn; };
            auto quoteFn = [](const uint8_t* b, size_t sz, QCB& qcb, TCB&) -> bool {
                QuoteMessage q; if (!parseQuote(b, sz, q) || !validateQuote(q)) return false; qcb(q); return true; };
            auto tradeFn = [](const uint8_t* b, size_t sz, QCB&, TCB& tcb) -> bool {
                TradeMessage t; if (!parseTrade(b, sz, t) || !validateTrade(t)) return false; tcb(t); return true; };
            static const Entry table[2] = {{MessageHeader::QUOTE_TYPE, quoteFn}, {MessageHeader::TRADE_TYPE, tradeFn}};
            // Linear scan (2 entries) is cheaper than building an indexed array for sparse types.
            if (header.type == table[0].type) return table[0].fn(body, bodySize, onQuote, onTrade);
            if (header.type == table[1].type) return table[1].fn(body, bodySize, onQuote, onTrade);
            return false;
        } else {
            if (header.type == MessageHeader::QUOTE_TYPE) {
                QuoteMessage q; if (!parseQuote(body, bodySize, q) || !validateQuote(q)) return false; onQuote(q); return true;
            } else if (header.type == MessageHeader::TRADE_TYPE) {
                TradeMessage t; if (!parseTrade(body, bodySize, t) || !validateTrade(t)) return false; onTrade(t); return true;
            }
            return false;
        }
    }
};

#endif
