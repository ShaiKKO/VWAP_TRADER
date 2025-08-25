#include <iostream>
#include <cstring>
#include <iomanip>
#include "message.h"
#include "message_parser.h"
#include "message_builder.h"
#include "message_buffer.h"
#include "endian_converter.h"

class ProtocolTest {
public:
    static int testsRun;
    static int testsPassed;
    
private:
    
    static void printTestResult(const char* testName, bool passed) {
        testsRun++;
        if (passed) {
            testsPassed++;
            std::cout << "[PASS] " << testName << std::endl;
        } else {
            std::cout << "[FAIL] " << testName << std::endl;
        }
    }
    
    static bool compareBytes(const uint8_t* actual, const uint8_t* expected, size_t size) {
        for (size_t i = 0; i < size; i++) {
            if (actual[i] != expected[i]) {
                std::cerr << "  Byte mismatch at position " << i 
                         << ": got 0x" << std::hex << (int)actual[i]
                         << ", expected 0x" << (int)expected[i] << std::dec << std::endl;
                return false;
            }
        }
        return true;
    }
    
public:
    static bool testQuoteParsing() {
        uint8_t quoteBytes[] = {
            0x49, 0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00,  // Symbol: "IBM"
            0x00, 0x7E, 0x95, 0xAA, 0x78, 0xD4, 0xED, 0x15,  // Timestamp
            0x64, 0x00, 0x00, 0x00,  // Bid Qty: 100
            0x49, 0x36, 0x00, 0x00,  // Bid Price: 13897
            0xC8, 0x00, 0x00, 0x00,  // Ask Qty: 200
            0x4A, 0x36, 0x00, 0x00   // Ask Price: 13898
        };
        
        QuoteMessage quote;
        bool result = MessageParser::parseQuote(quoteBytes, sizeof(quoteBytes), quote);
        
        if (!result) {
            std::cerr << "  Failed to parse quote" << std::endl;
            return false;
        }
        
        bool passed = true;
        if (std::strncmp(quote.symbol, "IBM", 3) != 0) {
            std::cerr << "  Symbol mismatch" << std::endl;
            passed = false;
        }
        if (quote.timestamp != 1580152659000000000ULL) {
            std::cerr << "  Timestamp mismatch: " << quote.timestamp << std::endl;
            passed = false;
        }
        if (quote.bidQuantity != 100) {
            std::cerr << "  Bid quantity mismatch: " << quote.bidQuantity << std::endl;
            passed = false;
        }
        if (quote.bidPrice != 13897) {
            std::cerr << "  Bid price mismatch: " << quote.bidPrice << std::endl;
            passed = false;
        }
        if (quote.askQuantity != 200) {
            std::cerr << "  Ask quantity mismatch: " << quote.askQuantity << std::endl;
            passed = false;
        }
        if (quote.askPrice != 13898) {
            std::cerr << "  Ask price mismatch: " << quote.askPrice << std::endl;
            passed = false;
        }
        
        return passed;
    }
    
    static bool testTradeParsing() {
        uint8_t tradeBytes[] = {
            0x49, 0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00,  // Symbol
            0x01, 0x7E, 0x95, 0xAA, 0x78, 0xD4, 0xED, 0x15,  // Timestamp
            0x32, 0x00, 0x00, 0x00,  // Qty: 50
            0x4B, 0x36, 0x00, 0x00   // Price: 13899
        };
        
        TradeMessage trade;
        bool result = MessageParser::parseTrade(tradeBytes, sizeof(tradeBytes), trade);
        
        if (!result) {
            std::cerr << "  Failed to parse trade" << std::endl;
            return false;
        }
        
        bool passed = true;
        if (std::strncmp(trade.symbol, "IBM", 3) != 0) {
            std::cerr << "  Symbol mismatch" << std::endl;
            passed = false;
        }
        if (trade.timestamp != 1580152659000000001ULL) {
            std::cerr << "  Timestamp mismatch: " << trade.timestamp << std::endl;
            passed = false;
        }
        if (trade.quantity != 50) {
            std::cerr << "  Quantity mismatch: " << trade.quantity << std::endl;
            passed = false;
        }
        if (trade.price != 13899) {
            std::cerr << "  Price mismatch: " << trade.price << std::endl;
            passed = false;
        }
        
        return passed;
    }
    
    static bool testOrderBuilding() {
        OrderMessage order;
        std::memset(order.symbol, 0, sizeof(order.symbol));
        std::memcpy(order.symbol, "IBM", 3);
        order.timestamp = 1580152659000000002ULL;
        order.side = 'B';
        order.quantity = 75;
        order.price = 13896;
        
        uint8_t buffer[OrderMessage::SIZE];
        size_t size = MessageBuilder::buildOrder(buffer, sizeof(buffer), order);
        
        if (size != OrderMessage::SIZE) {
            std::cerr << "  Build returned wrong size: " << size << std::endl;
            return false;
        }
        
        uint8_t expectedBytes[] = {
            0x49, 0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00,  // Symbol
            0x02, 0x7E, 0x95, 0xAA, 0x78, 0xD4, 0xED, 0x15,  // Timestamp
            0x42,  // Side: 'B'
            0x4B, 0x00, 0x00, 0x00,  // Qty: 75
            0x48, 0x36, 0x00, 0x00   // Price: 13896
        };
        
        return compareBytes(buffer, expectedBytes, OrderMessage::SIZE);
    }
    
    static bool testHeaderParsing() {
        uint8_t headerBytes[] = {0x20, 0x01, 0x00};  // 3 bytes: length, type, reserved
        
        MessageHeader header;
        bool result = MessageParser::parseHeader(headerBytes, sizeof(headerBytes), header);
        
        if (!result) {
            std::cerr << "  Failed to parse header" << std::endl;
            return false;
        }
        
        bool passed = true;
        if (header.length != 32) {
            std::cerr << "  Length mismatch: " << (int)header.length << std::endl;
            passed = false;
        }
        if (header.type != MessageHeader::QUOTE_TYPE) {
            std::cerr << "  Type mismatch: " << (int)header.type << std::endl;
            passed = false;
        }
        
        return passed;
    }
    
    static bool testMessageBuffer() {
        MessageBuffer buffer;
        
        uint8_t completeMessage[] = {
            0x20, 0x01, 0x00,  // Length=32, Type=1, Reserved=0 (3 bytes)
            0x49, 0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, // Symbol
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
            0x64, 0x00, 0x00, 0x00, // Bid Qty
            0x00, 0x00, 0x00, 0x00, // Bid Price
            0xC8, 0x00, 0x00, 0x00, // Ask Qty
            0x00, 0x00, 0x00, 0x00  // Ask Price
        };
        
        buffer.append(completeMessage, sizeof(completeMessage));
        
        MessageHeader header;
        uint8_t messageBody[64];
        auto result = buffer.extractMessage(header, messageBody);
        
        if (result != MessageBuffer::ExtractResult::SUCCESS) {
            std::cerr << "  Failed to extract complete message" << std::endl;
            return false;
        }
        
        buffer.clear();
        buffer.append(completeMessage, 10);
        
        result = buffer.extractMessage(header, messageBody);
        if (result != MessageBuffer::ExtractResult::NEED_MORE_DATA) {
            std::cerr << "  Should have returned NEED_MORE_DATA for partial message" << std::endl;
            return false;
        }
        
        buffer.append(completeMessage + 10, sizeof(completeMessage) - 10);
        result = buffer.extractMessage(header, messageBody);
        
        if (result != MessageBuffer::ExtractResult::SUCCESS) {
            std::cerr << "  Failed to extract message after completing partial" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testEndianConversion() {
        uint16_t host16 = 0x1234;
        uint16_t le16 = EndianConverter::htol16(host16);
        uint16_t back16 = EndianConverter::ltoh16(le16);
        if (back16 != host16) {
            std::cerr << "  16-bit conversion failed" << std::endl;
            return false;
        }
        
        uint32_t host32 = 0x12345678;
        uint32_t le32 = EndianConverter::htol32(host32);
        uint32_t back32 = EndianConverter::ltoh32(le32);
        if (back32 != host32) {
            std::cerr << "  32-bit conversion failed" << std::endl;
            return false;
        }
        
        uint64_t host64 = 0x123456789ABCDEF0ULL;
        uint64_t le64 = EndianConverter::htol64(host64);
        uint64_t back64 = EndianConverter::ltoh64(le64);
        if (back64 != host64) {
            std::cerr << "  64-bit conversion failed" << std::endl;
            return false;
        }
        
        int32_t hostSigned = -12345;
        int32_t leSigned = EndianConverter::htol32_signed(hostSigned);
        int32_t backSigned = EndianConverter::ltoh32_signed(leSigned);
        if (backSigned != hostSigned) {
            std::cerr << "  Signed conversion failed" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testInvalidMessages() {
        MessageHeader header;
        header.type = 99;  // Invalid type
        header.length = 32;
        
        if (MessageParser::validateHeader(header)) {
            std::cerr << "  Should have rejected invalid header type" << std::endl;
            return false;
        }
        
        QuoteMessage quote;
        quote.bidQuantity = 0;  // Invalid
        quote.askQuantity = 100;
        quote.bidPrice = 100;
        quote.askPrice = 101;
        
        if (MessageParser::validateQuote(quote)) {
            std::cerr << "  Should have rejected quote with zero quantity" << std::endl;
            return false;
        }
        
        OrderMessage order;
        order.side = 'X';  // Invalid side
        order.quantity = 100;
        order.price = 100;
        
        if (MessageBuilder::validateOrder(order)) {
            std::cerr << "  Should have rejected order with invalid side" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static void runAllTests() {
        std::cout << "\n=== Protocol Test Suite ===" << std::endl;
        
        testsRun = 0;
        testsPassed = 0;
        
        printTestResult("Quote Parsing", testQuoteParsing());
        printTestResult("Trade Parsing", testTradeParsing());
        printTestResult("Order Building", testOrderBuilding());
        printTestResult("Header Parsing", testHeaderParsing());
        printTestResult("Message Buffer", testMessageBuffer());
        printTestResult("Endian Conversion", testEndianConversion());
        printTestResult("Invalid Messages", testInvalidMessages());
        
        std::cout << "\nResults: " << testsPassed << "/" << testsRun << " tests passed" << std::endl;
        
        if (testsPassed == testsRun) {
            std::cout << "ALL TESTS PASSED ✓" << std::endl;
        } else {
            std::cout << "SOME TESTS FAILED ✗" << std::endl;
        }
    }
};

int ProtocolTest::testsRun = 0;
int ProtocolTest::testsPassed = 0;

