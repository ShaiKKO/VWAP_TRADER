#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/order_manager.h"
#include "../include/decision_engine.h"
#include "../include/message.h"
#include <cstring>
#include <limits>

class EdgeCaseTests : public TestBase {
private:
    TradeMessage createTrade(const char* symbol, uint64_t timestamp,
                            uint32_t quantity, int32_t price) {
        TradeMessage trade;
        std::strcpy(trade.symbol, symbol);
        trade.timestamp = timestamp;
        trade.quantity = quantity;
        trade.price = price;
        return trade;
    }
    
    QuoteMessage createQuote(const char* symbol, uint64_t timestamp,
                            int32_t bidPrice, uint32_t bidQty,
                            int32_t askPrice, uint32_t askQty) {
        QuoteMessage quote;
        std::strcpy(quote.symbol, symbol);
        quote.timestamp = timestamp;
        quote.bidPrice = bidPrice;
        quote.bidQuantity = bidQty;
        quote.askPrice = askPrice;
        quote.askQuantity = askQty;
        return quote;
    }
    
public:
    EdgeCaseTests() : TestBase("Edge Cases") {}
    
    void runAll() override {
        runTest("Empty Window", [this](std::string& details) {
            VwapCalculator calc(5);
            
            double vwap = calc.getCurrentVwap();
            
            if (vwap != 0.0) {
                details = "VWAP should be 0 for empty window, got " + std::to_string(vwap);
                return false;
            }
            
            if (calc.hasCompleteWindow()) {
                details = "Should not have complete window with no trades";
                return false;
            }
            
            if (calc.getTradeCount() != 0) {
                details = "Trade count should be 0";
                return false;
            }
            
            return true;
        });
        
        runTest("Single Trade", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 1000000000000ULL, 100, 12345));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 12345.0)) {
                details = "VWAP should equal single trade price";
                return false;
            }
            
            if (calc.getTradeCount() != 1) {
                details = "Should have exactly 1 trade";
                return false;
            }
            
            return true;
        });
        
        runTest("Zero Quantity Trade", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 1000000000000ULL, 100, 10000));
            
            calc.addTrade(createTrade("TEST", 2000000000000ULL, 0, 20000));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 10000.0)) {
                details = "Zero quantity trade should be ignored";
                return false;
            }
            
            if (calc.getTradeCount() != 1) {
                details = "Zero quantity trade should not be counted";
                return false;
            }
            
            return true;
        });
        
        runTest("Zero Price Trade", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 1000000000000ULL, 100, 10000));
            
            calc.addTrade(createTrade("TEST", 2000000000000ULL, 100, 0));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 10000.0)) {
                details = "Zero price trade should be ignored";
                return false;
            }
            
            return true;
        });
        
        runTest("Negative Price Trade", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 1000000000000ULL, 100, 10000));
            
            calc.addTrade(createTrade("TEST", 2000000000000ULL, 100, -5000));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 10000.0)) {
                details = "Negative price trade should be ignored";
                return false;
            }
            
            return true;
        });
        
        runTest("Maximum Values", [this](std::string& details) {
            VwapCalculator calc(5);
            
            uint32_t maxQty = std::numeric_limits<uint32_t>::max();
            calc.addTrade(createTrade("TEST", 1000000000000ULL, maxQty, 10000));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 10000.0, 1.0)) {
                details = "Should handle maximum quantity";
                return false;
            }
            
            int32_t maxPrice = std::numeric_limits<int32_t>::max();
            calc.addTrade(createTrade("TEST", 2000000000000ULL, 100, maxPrice));
            
            vwap = calc.getCurrentVwap();
            if (vwap <= 0) {
                details = "Should handle maximum price values";
                return false;
            }
            
            return true;
        });
        
        runTest("Minimum Positive Values", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 1000000000000ULL, 1, 1));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 1.0)) {
                details = "Should handle minimum positive values";
                return false;
            }
            
            return true;
        });
        
        runTest("Empty Symbol", [this](std::string& details) {
            OrderManager manager("", 'B', 100, 5); // Empty symbol
            
            uint64_t baseTime = 1000000000000ULL;
            
            for (int i = 0; i < 10; ++i) {
                manager.processTrade(createTrade("", baseTime + i * 600000000ULL, 100, 10000));
            }
            
            auto quote = createQuote("", baseTime + 6000000000ULL, 9900, 100, 9950, 200);
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Should still trigger orders with empty symbol";
                return false;
            }
            
            return true;
        });
        
        runTest("Maximum Symbol Length", [this](std::string& details) {
            OrderManager manager("ABCDEFGH", 'B', 100, 1);
            
            uint64_t baseTime = 1000000000000ULL;
            
            for (int i = 0; i < 5; ++i) {
                manager.processTrade(createTrade("ABCDEFGH", baseTime + i * 200000000ULL, 100, 10000));
            }
            
            auto quote = createQuote("ABCDEFGH", baseTime + 1500000000ULL, 9900, 100, 9950, 200);
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Should handle 8-character symbols";
                return false;
            }
            
            if (std::strlen(order.value().symbol) != 8) {
                details = "Order symbol should be 8 characters";
                return false;
            }
            
            return true;
        });
        
        runTest("Zero Bid/Ask Quantities", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            for (int i = 0; i < 5; ++i) {
                manager.processTrade(createTrade("TEST", baseTime + i * 200000000ULL, 100, 10000));
            }
            
            auto quote = createQuote("TEST", baseTime + 1500000000ULL, 9900, 100, 9950, 0);
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Should not trigger order with zero ask quantity";
                return false;
            }
            
            return true;
        });
        
        runTest("All Trades Expired", [this](std::string& details) {
            VwapCalculator calc(1); // 1 second window
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            calc.addTrade(createTrade("TEST", baseTime + 500000000, 100, 10100));
            
            calc.addTrade(createTrade("TEST", baseTime + 5000000000ULL, 100, 15000));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 15000.0)) {
                details = "VWAP should be price of only remaining trade";
                return false;
            }
            
            if (calc.getTradeCount() != 1) {
                details = "Should have only 1 trade after expiration";
                return false;
            }
            
            return true;
        });
        
        runTest("Timestamp Zero", [this](std::string& details) {
            VwapCalculator calc(5);
            
            calc.addTrade(createTrade("TEST", 0, 100, 10000));
            
            double vwap = calc.getCurrentVwap();
            
            if (!TestUtils::compareDouble(vwap, 10000.0)) {
                details = "Should handle timestamp 0";
                return false;
            }
            
            return true;
        });
        
        runTest("Duplicate Quotes", [this](std::string& details) {
            DecisionEngine engine("TEST", 'B', 100);
            engine.onVwapWindowComplete();
            
            auto quote = createQuote("TEST", 1000000000000ULL, 9900, 100, 9950, 200);
            
            OrderMessage o1, o2; bool t1 = engine.evaluateQuote(quote, 10000.0, o1); bool t2 = engine.evaluateQuote(quote, 10000.0, o2);
            if (!t1) {
                details = "First quote should trigger order";
                return false;
            }
            
            if (t2) {
                details = "Duplicate quote should not trigger order";
                return false;
            }
            
            return true;
        });
        
        runTest("Invalid Order Side", [this](std::string& details) {
            try {
                OrderManager manager("TEST", 'X', 100, 5); // Invalid side 'X'
                details = "Should reject invalid order side";
                return false;
            } catch (const std::invalid_argument& e) {
                return true;
            } catch (...) {
                details = "Wrong exception type for invalid side";
                return false;
            }
        });
        
        runTest("Overflow Protection", [this](std::string& details) {
            VwapCalculator calc(10);
            
            for (int i = 0; i < 1000; ++i) {
                calc.addTrade(createTrade("TEST", 
                                         1000000000000ULL + i * 1000000,
                                         1000000, // Large quantity
                                         2000000)); // Large price
            }
            
            double vwap = calc.getCurrentVwap();
            
            if (vwap <= 0 || std::isnan(vwap) || std::isinf(vwap)) {
                details = "VWAP calculation should handle overflow";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runEdgeCaseTests() {
    EdgeCaseTests tests;
    tests.runAll();
    tests.printSummary();
}