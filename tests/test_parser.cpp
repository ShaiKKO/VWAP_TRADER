#include <iostream>
#include <cstring>
#include <cassert>
#include "message_serializer.h"
#include "message_parser.h"
#include "wire_format.h"
#include "endian_converter.h"

struct ParserTest {
    static int testsRun;
    static int testsPassed;

    static void assertTrue(bool cond, const char* name) {
        ++testsRun;
        if (cond) { ++testsPassed; }
        else { std::cerr << "[FAIL] " << name << std::endl; }
    }

    static void testQuoteRoundTrip() {
        uint8_t buf[WireFormat::HEADER_SIZE + WireFormat::QUOTE_SIZE];
        QuoteMessage q{}; std::memcpy(q.symbol, "IBM\0\0\0\0\0", 8);
        q.timestamp = 123456789ULL; q.bidQuantity=100; q.bidPrice=14050; q.askQuantity=120; q.askPrice=14060;
        size_t n = MessageSerializer::serializeQuoteMessage(buf, sizeof(buf), q);
        assertTrue(n == WireFormat::HEADER_SIZE + WireFormat::QUOTE_SIZE, "quote serialize size");
        MessageHeader h; QuoteMessage out{};
        assertTrue(MessageParser::parseHeader(buf, sizeof(buf), h), "parse header");
        assertTrue(MessageParser::validateHeader(h), "validate header");
        assertTrue(MessageParser::parseQuote(buf+WireFormat::HEADER_SIZE, h.length, out), "parse quote body");
        assertTrue(out.timestamp==q.timestamp && out.bidPrice==q.bidPrice && out.askPrice==q.askPrice, "quote fields round-trip");
    }

    static void testTradeRoundTrip() {
        uint8_t buf[WireFormat::HEADER_SIZE + WireFormat::TRADE_SIZE];
        TradeMessage t{}; std::memcpy(t.symbol, "IBM\0\0\0\0\0", 8);
        t.timestamp=999999; t.quantity=250; t.price=13990;
        size_t n = MessageSerializer::serializeTradeMessage(buf, sizeof(buf), t);
        assertTrue(n == WireFormat::HEADER_SIZE + WireFormat::TRADE_SIZE, "trade serialize size");
        MessageHeader h; TradeMessage out{};
        assertTrue(MessageParser::parseHeader(buf, sizeof(buf), h), "parse trade header");
        assertTrue(MessageParser::validateHeader(h), "validate trade header");
        assertTrue(MessageParser::parseTrade(buf+WireFormat::HEADER_SIZE, h.length, out), "parse trade body");
        assertTrue(out.price==t.price && out.quantity==t.quantity, "trade fields round-trip");
    }

    static void testInvalidHeaderType() {
        uint8_t buf[2] = { static_cast<uint8_t>(WireFormat::QUOTE_SIZE), 99 }; // invalid type
        MessageHeader h; bool ok = MessageParser::parseHeader(buf, sizeof(buf), h);
        assertTrue(ok, "parse raw header bytes");
        assertTrue(!MessageParser::validateHeader(h), "reject invalid type");
    }

    static void runAllTests() {
        testsRun=testsPassed=0;
        testQuoteRoundTrip();
        testTradeRoundTrip();
        testInvalidHeaderType();
        std::cout << "Parser Tests: " << testsPassed << "/" << testsRun << " passed" << std::endl;
    }
};

int ParserTest::testsRun=0; int ParserTest::testsPassed=0;
