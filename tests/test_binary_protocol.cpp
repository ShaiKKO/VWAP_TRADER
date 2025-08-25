#include "test_comprehensive.h"
#include "../include/message.h"
#include "../include/message_parser.h"
#include "../include/message_builder.h"
#include "../include/endian_converter.h"
#include <cstring>
#include <arpa/inet.h>

class BinaryProtocolTests : public TestBase {
private:
    void hexDump(const char* data, size_t len, std::string& output) {
        std::stringstream ss;
        for (size_t i = 0; i < len; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') 
               << (int)(unsigned char)data[i] << " ";
            if ((i + 1) % 16 == 0) ss << "\n";
        }
        output = ss.str();
    }
    
    bool verifyBytes(const char* actual, const char* expected, size_t len, std::string& details) {
        for (size_t i = 0; i < len; i++) {
            if (actual[i] != expected[i]) {
                details = "Byte mismatch at offset " + std::to_string(i) + 
                         ": expected 0x" + std::to_string((unsigned char)expected[i]) +
                         ", got 0x" + std::to_string((unsigned char)actual[i]);
                return false;
            }
        }
        return true;
    }
    
public:
    BinaryProtocolTests() : TestBase("Binary Protocol Correctness") {}
    
    void runAll() override {
        runTest("Header Structure Alignment", [this](std::string& details) {
            if (sizeof(MessageHeader) != 2) {
                details = "Header size should be 2 bytes, got " + std::to_string(sizeof(MessageHeader));
                return false;
            }
            MessageHeader h; h.length=0xAA; h.type=0x55;
            unsigned char* p = reinterpret_cast<unsigned char*>(&h);
            if (p[0]!=0xAA || p[1]!=0x55) { details = "Header layout mismatch"; return false; }
            return true; });
        
        runTest("Quote Message Binary Layout", [this](std::string& details) {
            QuoteMessage quote;
            std::memset(&quote, 0, sizeof(quote));
            
            std::strcpy(quote.symbol, "IBM");
            quote.timestamp = 0x123456789ABCDEF0ULL;
            quote.bidQuantity = 0x11223344;
            quote.bidPrice = 0x55667788;
            quote.askQuantity = 0x99AABBCC;
            quote.askPrice = 0xDDEEFF00;
            
            char* bytes = reinterpret_cast<char*>(&quote);
            
            if (std::memcmp(bytes, "IBM\0\0\0\0\0", 8) != 0) {
                details = "Symbol field incorrect";
                return false;
            }
            
            uint64_t* ts = reinterpret_cast<uint64_t*>(bytes + 8);
            if (*ts != quote.timestamp) {
                details = "Timestamp field incorrect";
                return false;
            }
            
            uint32_t* bidQty = reinterpret_cast<uint32_t*>(bytes + 16);
            if (*bidQty != quote.bidQuantity) {
                details = "Bid quantity field incorrect";
                return false;
            }
            
            int32_t* bidPrc = reinterpret_cast<int32_t*>(bytes + 20);
            if (*bidPrc != quote.bidPrice) {
                details = "Bid price field incorrect";
                return false;
            }
            
            if (sizeof(QuoteMessage) != 32) {
                details = "Quote message size should be 32 bytes, got " + 
                         std::to_string(sizeof(QuoteMessage));
                return false;
            }
            
            return true;
        });
        
    runTest("Trade Message Binary Layout", [this](std::string& details) {
            TradeMessage trade;
            std::memset(&trade, 0, sizeof(trade));
            
            std::strcpy(trade.symbol, "AAPL");
            trade.timestamp = 0xFEDCBA9876543210ULL;
            trade.quantity = 0xAABBCCDD;
            trade.price = 0x11223344;
            
            char* bytes = reinterpret_cast<char*>(&trade);
            
            if (std::memcmp(bytes, "AAPL\0\0\0\0", 8) != 0) {
                details = "Symbol field incorrect";
                return false;
            }
            
            if (sizeof(TradeMessage) != 24) {
                details = "Trade message size should be 24 bytes, got " + 
                         std::to_string(sizeof(TradeMessage));
                return false;
            }
            
            return true;
        });
        
    runTest("Order Message Binary Layout", [this](std::string& details) {
            OrderMessage order;
            std::memset(&order, 0, sizeof(order));
            
            std::strcpy(order.symbol, "MSFT");
            order.timestamp = 0x123456789ABCDEF0ULL;
            order.side = 'B';
            order.quantity = 0x12345678;
            order.price = 0x87654321;
            
            if (sizeof(OrderMessage) != 32) {
                details = "Order message size should be 32 bytes, got " + 
                         std::to_string(sizeof(OrderMessage));
                return false;
            }
            
            char* bytes = reinterpret_cast<char*>(&order);
            if (bytes[24] != 'B') {
                details = "Side field not at correct offset or incorrect value";
                return false;
            }
            
            return true;
        });
        
        runTest("Endianness Conversion", [this](std::string& details) {
            MessageParser parser;
            
            struct RawQuote {
                char symbol[8];
                uint64_t timestamp;
                uint32_t bidQuantity;
                int32_t bidPrice;
                uint32_t askQuantity;
                int32_t askPrice;
            } __attribute__((packed));
            
            RawQuote raw;
            std::strcpy(raw.symbol, "TEST");
            raw.timestamp = htonll(1000000000000ULL);
            raw.bidQuantity = htonl(100);
            raw.bidPrice = htonl(9950);
            raw.askQuantity = htonl(200);
            raw.askPrice = htonl(10050);
            
            QuoteMessage quote;
            if (!parser.parseQuote(reinterpret_cast<const uint8_t*>(&raw), sizeof(QuoteMessage), quote)) {
                details = "Failed to parse quote";
                return false;
            }
            
            if (quote.timestamp != 1000000000000ULL) {
                details = "Timestamp not converted correctly";
                return false;
            }
            
            if (quote.bidQuantity != 100 || quote.bidPrice != 9950) {
                details = "Bid values not converted correctly";
                return false;
            }
            
            if (quote.askQuantity != 200 || quote.askPrice != 10050) {
                details = "Ask values not converted correctly";
                return false;
            }
            
            return true;
        });
        
        runTest("Message Builder Output", [this](std::string& details) {
            MessageBuilder builder;
            
            OrderMessage order;
            std::strcpy(order.symbol, "IBM");
            order.timestamp = 0x123456789ABCDEF0ULL;
            order.side = 'S';
            order.quantity = 500;
            order.price = 10500;
            
            uint8_t buffer[256];
            MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
            header->length = sizeof(OrderMessage);
            header->type = MessageHeader::QUOTE_TYPE;
            
            size_t orderSize = builder.buildOrder(buffer + sizeof(MessageHeader), 256 - sizeof(MessageHeader), order);
            size_t msgSize = orderSize > 0 ? sizeof(MessageHeader) + orderSize : 0;
            
            if (msgSize != sizeof(MessageHeader) + OrderMessage::SIZE) {
                details = "Built message size mismatch, got " + std::to_string(msgSize);
                return false;
            }
            
            if (header->type != MessageHeader::QUOTE_TYPE) {
                details = "Header message type incorrect";
                return false;
            }
            
            if (header->length != sizeof(OrderMessage)) {
                details = "Header message length incorrect";
                return false;
            }
            
            
            return true;
        });
        
        runTest("Parser Round-Trip", [this](std::string& details) {
            MessageParser parser;
            
            QuoteMessage origQuote;
            std::strcpy(origQuote.symbol, "GOOG");
            origQuote.timestamp = 9876543210ULL;
            origQuote.bidQuantity = 1500;
            origQuote.bidPrice = 150000;
            origQuote.askQuantity = 2000;
            origQuote.askPrice = 150100;
            
            uint8_t buffer[256];
            MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
            header->length = sizeof(QuoteMessage);
            header->type = MessageHeader::QUOTE_TYPE;
            /* no reserved */
            
            QuoteMessage* netQuote = reinterpret_cast<QuoteMessage*>(buffer + sizeof(MessageHeader));
            std::memcpy(netQuote->symbol, origQuote.symbol, 8);
            netQuote->timestamp = EndianConverter::htol64(origQuote.timestamp);
            netQuote->bidQuantity = EndianConverter::htol32(origQuote.bidQuantity);
            netQuote->bidPrice = EndianConverter::htol32(origQuote.bidPrice);
            netQuote->askQuantity = EndianConverter::htol32(origQuote.askQuantity);
            netQuote->askPrice = EndianConverter::htol32_signed(origQuote.askPrice);
            
            QuoteMessage parsedQuote;
            if (!parser.parseQuote(buffer + sizeof(MessageHeader), sizeof(QuoteMessage), parsedQuote)) {
                details = "Failed to parse built quote";
                return false;
            }
            
            if (std::strcmp(parsedQuote.symbol, origQuote.symbol) != 0) {
                details = "Symbol mismatch after round-trip";
                return false;
            }
            
            if (parsedQuote.timestamp != origQuote.timestamp) {
                details = "Timestamp mismatch after round-trip";
                return false;
            }
            
            if (parsedQuote.bidPrice != origQuote.bidPrice || 
                parsedQuote.askPrice != origQuote.askPrice) {
                details = "Price mismatch after round-trip";
                return false;
            }
            
            return true;
        });
        
        runTest("Invalid Message Handling", [this](std::string& details) {
            MessageParser parser;
            
            char garbage[100];
            std::memset(garbage, 0xFF, sizeof(garbage));
            
            QuoteMessage quote;
            TradeMessage trade;
            OrderMessage order;
            
            bool parsed = parser.parseQuote(reinterpret_cast<const uint8_t*>(garbage), sizeof(garbage), quote);
            parsed = parsed || parser.parseTrade(reinterpret_cast<const uint8_t*>(garbage), sizeof(garbage), trade);
            
            char shortBuf[5];
            if (parser.parseQuote(reinterpret_cast<const uint8_t*>(shortBuf), 10, quote)) {
                details = "Should not parse from too-short buffer";
                return false;
            }
            
            return true;
        });
        
        runTest("Packed Structure Verification", [this](std::string& details) {
            
            #pragma pack(push, 1)
            struct TestStruct {
                uint8_t a;
                uint16_t b;
                uint32_t c;
                uint64_t d;
            };
            #pragma pack(pop)
            
            if (sizeof(TestStruct) != 15) {
                details = "Packed struct has unexpected padding";
                return false;
            }
            
            size_t expectedQuoteSize = 8 + 8 + 4 + 4 + 4 + 4; // 32 bytes
            if (sizeof(QuoteMessage) != expectedQuoteSize) {
                details = "QuoteMessage has padding, size is " + 
                         std::to_string(sizeof(QuoteMessage));
                return false;
            }
            
            return true;
        });
        
        runTest("Symbol Null Termination", [this](std::string& details) {
            QuoteMessage quote;
            std::memset(&quote, 0xFF, sizeof(quote)); // Fill with non-zero
            
            std::memcpy(quote.symbol, "ABCDEFGH", 8);
            
            for (int i = 0; i < 8; i++) {
                if (quote.symbol[i] == '\0') {
                    details = "8-char symbol should not have null terminator";
                    return false;
                }
            }
            
            std::memset(quote.symbol, 0, 8);
            std::strcpy(quote.symbol, "IBM");
            
            if (quote.symbol[3] != '\0') {
                details = "Short symbol should be null-terminated";
                return false;
            }
            
            return true;
        });
        
        runTest("Cross-Platform Compatibility", [this](std::string& details) {
            uint16_t test16 = 0x1234;
            uint32_t test32 = 0x12345678;
            uint64_t test64 = 0x123456789ABCDEF0ULL;
            
            uint16_t net16 = htons(test16);
            uint32_t net32 = htonl(test32);
            uint64_t net64 = htonll(test64);
            
            uint16_t host16 = ntohs(net16);
            uint32_t host32 = ntohl(net32);
            uint64_t host64 = ntohll(net64);
            
            if (host16 != test16 || host32 != test32 || host64 != test64) {
                details = "Byte order conversion round-trip failed";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runBinaryProtocolTests() {
    BinaryProtocolTests tests;
    tests.runAll();
    tests.printSummary();
}