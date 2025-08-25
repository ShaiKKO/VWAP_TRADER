#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/order_manager.h"
#include "../include/message_buffer.h"
#include "../include/network_manager.h"
#include "../include/message_parser.h"
#include <random>
#include <thread>
#include <atomic>

class StressTests : public TestBase {
private:
    std::mt19937 rng{42};
    
    TradeMessage createRandomTrade(const std::string& symbol, uint64_t timestamp) {
        TradeMessage trade;
        std::strncpy(trade.symbol, symbol.c_str(), 8);
        trade.timestamp = timestamp;
        
        std::uniform_int_distribution<uint32_t> qtyDist(1, 100000);
        std::uniform_int_distribution<int32_t> priceDist(1000, 200000);
        
        trade.quantity = qtyDist(rng);
        trade.price = priceDist(rng);
        
        return trade;
    }
    
    QuoteMessage createRandomQuote(const std::string& symbol, uint64_t timestamp) {
        QuoteMessage quote;
        std::strncpy(quote.symbol, symbol.c_str(), 8);
        quote.timestamp = timestamp;
        
        std::uniform_int_distribution<uint32_t> qtyDist(1, 10000);
        std::uniform_int_distribution<int32_t> priceDist(5000, 15000);
        
        int32_t midPrice = priceDist(rng);
        quote.bidPrice = midPrice - std::uniform_int_distribution<int32_t>(1, 100)(rng);
        quote.askPrice = midPrice + std::uniform_int_distribution<int32_t>(1, 100)(rng);
        quote.bidQuantity = qtyDist(rng);
        quote.askQuantity = qtyDist(rng);
        
        return quote;
    }
    
public:
    StressTests() : TestBase("Stress Tests") {}
    
    void runAll() override {
        runTest("High Volume Trading", [this](std::string& details) {
            VwapCalculator calc(60); // 1 minute window
            uint64_t timestamp = 1000000000000ULL;
            
            const int numTrades = 1000000;
            
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < numTrades; ++i) {
                calc.addTrade(createRandomTrade("STRESS", timestamp + i * 1000));
                
                if (i % 10000 == 0) {
                    volatile double vwap = calc.getCurrentVwap();
                    (void)vwap;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double totalSec = std::chrono::duration<double>(end - start).count();
            
            double tradesPerSec = numTrades / totalSec;
            
            if (tradesPerSec < 100000) {
                details = "Processing rate " + std::to_string(tradesPerSec) + 
                         " trades/sec below target";
                return false;
            }
            
            double finalVwap = calc.getCurrentVwap();
            if (finalVwap <= 0 || std::isnan(finalVwap) || std::isinf(finalVwap)) {
                details = "VWAP calculation failed after stress";
                return false;
            }
            
            return true;
        });
        
        runTest("Rapid Quote Bursts", [this](std::string& details) {
            OrderManager manager("BURST", 'B', 1000, 5);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 100; ++i) {
                manager.processTrade(createRandomTrade("BURST", timestamp + i * 50000000));
            }
            
            int ordersGenerated = 0;
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 10000; ++i) {
                auto quote = createRandomQuote("BURST", timestamp + 6000000000ULL + i * 1000);
                auto order = manager.processQuote(quote);
                if (order.has_value()) {
                    ordersGenerated++;
                }
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (totalMs > 100.0) { // Should process 10k quotes in <100ms
                details = "Quote burst processing too slow: " + std::to_string(totalMs) + "ms";
                return false;
            }
            
            return true;
        });
        
        runTest("Memory Stability", [this](std::string& details) {
            VwapCalculator calc(10); // 10 second window
            uint64_t timestamp = 1000000000000ULL;
            
            for (int batch = 0; batch < 100; ++batch) {
                for (int i = 0; i < 10000; ++i) {
                    calc.addTrade(createRandomTrade("MEM", timestamp));
                    timestamp += 1000000; // 1ms between trades
                }
                
                if (calc.getTradeCount() > 20000) {
                    details = "Trade count growing unbounded: " + 
                             std::to_string(calc.getTradeCount());
                    return false;
                }
            }
            
            int tradeCount = calc.getTradeCount();
            if (tradeCount < 9000 || tradeCount > 11000) {
                details = "Unexpected trade count after sliding: " + 
                         std::to_string(tradeCount);
                return false;
            }
            
            return true;
        });
        
        runTest("Message Buffer Overflow", [this](std::string& details) {
            MessageBuffer buffer;
            
            for (int i = 0; i < 10000; ++i) {
                MessageHeader header;
                header.type = MessageHeader::TRADE_TYPE;
                header.length = sizeof(TradeMessage);
                header.reserved = 0;
                
                TradeMessage trade = createRandomTrade("BUF", 1000000000000ULL + i);
                
                buffer.append(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
                buffer.append(reinterpret_cast<const uint8_t*>(&trade), sizeof(trade));
            }
            
            int extracted = 0;
            MessageHeader header;
            uint8_t data[256];
            
            while (buffer.extractMessage(header, data) == MessageBuffer::ExtractResult::SUCCESS && extracted < 10000) {
                extracted++;
            }
            
            if (extracted != 10000) {
                details = "Could not extract all messages: " + std::to_string(extracted);
                return false;
            }
            
            return true;
        });
        
        runTest("Maximum Window Duration", [this](std::string& details) {
            VwapCalculator calc(3600);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 7200; ++i) { // 2 trades per second for 1 hour
                calc.addTrade(createRandomTrade("MAX", timestamp + i * 500000000ULL));
            }
            
            if (calc.getTradeCount() != 7200) {
                details = "Should hold all trades in 1-hour window";
                return false;
            }
            
            calc.addTrade(createRandomTrade("MAX", timestamp + 3700000000000ULL));
            
            if (calc.getTradeCount() >= 7200) {
                details = "Old trades not expiring properly";
                return false;
            }
            
            return true;
        });
        
        runTest("Continuous Operation", [this](std::string& details) {
            OrderManager manager("CONT", 'B', 500, 10);
            uint64_t timestamp = 1000000000000ULL;
            
            const int hoursToSimulate = 24;
            const int tradesPerHour = 10000;
            const int quotesPerHour = 50000;
            
            std::uniform_int_distribution<int> actionDist(0, 9);
            
            for (int hour = 0; hour < hoursToSimulate; ++hour) {
                uint64_t hourStart = timestamp + hour * 3600000000000ULL;
                
                for (int i = 0; i < tradesPerHour + quotesPerHour; ++i) {
                    if (actionDist(rng) < 2) { // 20% trades
                        manager.processTrade(createRandomTrade("CONT", 
                                           hourStart + i * 720000000ULL));
                    } else { // 80% quotes
                        manager.processQuote(createRandomQuote("CONT", 
                                           hourStart + i * 720000000ULL));
                    }
                }
            }
            
            manager.printStatistics();
            
            if (manager.getTradeCount() == 0) {
                details = "No trades processed during continuous operation";
                return false;
            }
            
            return true;
        });
        
        runTest("Extreme Price Movements", [this](std::string& details) {
            VwapCalculator calc(30);
            uint64_t timestamp = 1000000000000ULL;
            
            calc.addTrade(createTrade("EXTREME", timestamp, 1000, 10000));
            
            calc.addTrade(createTrade("EXTREME", timestamp + 1000000000, 1000, 1000000));
            
            calc.addTrade(createTrade("EXTREME", timestamp + 2000000000, 1000, 100));
            
            calc.addTrade(createTrade("EXTREME", timestamp + 3000000000, 1000, 10000));
            
            double vwap = calc.getCurrentVwap();
            
            if (vwap <= 0 || std::isnan(vwap) || std::isinf(vwap)) {
                details = "VWAP calculation failed with extreme prices";
                return false;
            }
            
            return true;
        });
        
        runTest("Network Message Storm", [this](std::string& details) {
            MessageBuffer buffer;
            
            std::vector<char> partialData;
            
            for (int i = 0; i < 100000; ++i) {
                std::uniform_int_distribution<int> sizeDist(1, 50);
                int fragmentSize = sizeDist(rng);
                
                for (int j = 0; j < fragmentSize; ++j) {
                    partialData.push_back(static_cast<char>(rng() % 256));
                }
                
                buffer.append(reinterpret_cast<const uint8_t*>(partialData.data()), partialData.size());
                partialData.clear();
                
                MessageHeader header;
                uint8_t data[256];
                while (buffer.extractMessage(header, data) == MessageBuffer::ExtractResult::SUCCESS) {
                }
            }
            
            return true;
        });
        
        runTest("Order History Overflow", [this](std::string& details) {
            OrderManager manager("HIST", 'B', 100, 1);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 10; ++i) {
                manager.processTrade(createRandomTrade("HIST", timestamp + i * 100000000));
            }
            
            int ordersGenerated = 0;
            for (int i = 0; i < 2000; ++i) {
                auto quote = createQuote("HIST", timestamp + 1500000000 + i * 10000000,
                                       10100, 100, 9900, 100); // Will trigger buy
                auto order = manager.processQuote(quote);
                if (order.has_value()) {
                    ordersGenerated++;
                }
            }
            
            auto history = manager.getOrderHistory();
            if (history.size() > 1000) { // MAX_ORDER_HISTORY from order_manager.h
                details = "Order history not properly bounded: " + 
                         std::to_string(history.size());
                return false;
            }
            
            return true;
        });
        
        runTest("Concurrent Component Stress", [this](std::string& details) {
            OrderManager manager("ALL", 'B', 1000, 5);
            MessageParser parser;
            MessageBuffer buffer;
            
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 10000; ++i) {
                uint8_t tradeBuf[256];
                MessageHeader* header_t = reinterpret_cast<MessageHeader*>(tradeBuf);
                header_t->length = sizeof(TradeMessage);
                header_t->type = MessageHeader::TRADE_TYPE;
                header_t->reserved = 0;
                
                TradeMessage* trade = reinterpret_cast<TradeMessage*>(tradeBuf + sizeof(MessageHeader));
                *trade = createRandomTrade("ALL", timestamp + i * 1000000);
                
                size_t tradeSize = sizeof(MessageHeader) + sizeof(TradeMessage);
                
                buffer.append(tradeBuf, tradeSize);
                
                MessageHeader header;
                uint8_t data[256];
                if (buffer.extractMessage(header, data) == MessageBuffer::ExtractResult::SUCCESS) {
                    if (header.type == MessageHeader::TRADE_TYPE) {
                        TradeMessage parsedTrade;
                        parser.parseTrade(data, sizeof(TradeMessage), parsedTrade);
                        manager.processTrade(parsedTrade);
                    }
                }
                
                uint8_t quoteBuf[256];
                MessageHeader* header_q = reinterpret_cast<MessageHeader*>(quoteBuf);
                header_q->length = sizeof(QuoteMessage);
                header_q->type = MessageHeader::QUOTE_TYPE;
                header_q->reserved = 0;
                
                QuoteMessage* quote = reinterpret_cast<QuoteMessage*>(quoteBuf + sizeof(MessageHeader));
                *quote = createRandomQuote("ALL", timestamp + i * 1000000 + 500000);
                
                size_t quoteSize = sizeof(MessageHeader) + sizeof(QuoteMessage);
                
                buffer.append(quoteBuf, quoteSize);
                
                if (buffer.extractMessage(header, data) == MessageBuffer::ExtractResult::SUCCESS) {
                    if (header.type == MessageHeader::QUOTE_TYPE) {
                        QuoteMessage parsedQuote;
                        parser.parseQuote(data, sizeof(QuoteMessage), parsedQuote);
                        manager.processQuote(parsedQuote);
                    }
                }
            }
            
            if (manager.getTradeCount() == 0) {
                details = "No trades processed in concurrent stress test";
                return false;
            }
            
            return true;
        });
    }
    
private:
    TradeMessage createTrade(const std::string& symbol, uint64_t timestamp,
                            uint32_t quantity, int32_t price) {
        TradeMessage trade;
        std::strncpy(trade.symbol, symbol.c_str(), 8);
        trade.timestamp = timestamp;
        trade.quantity = quantity;
        trade.price = price;
        return trade;
    }
    
    QuoteMessage createQuote(const std::string& symbol, uint64_t timestamp,
                            int32_t bidPrice, uint32_t bidQty,
                            int32_t askPrice, uint32_t askQty) {
        QuoteMessage quote;
        std::strncpy(quote.symbol, symbol.c_str(), 8);
        quote.timestamp = timestamp;
        quote.bidPrice = bidPrice;
        quote.bidQuantity = bidQty;
        quote.askPrice = askPrice;
        quote.askQuantity = askQty;
        return quote;
    }
};

extern "C" void runStressTests() {
    StressTests tests;
    tests.runAll();
    tests.printSummary();
}