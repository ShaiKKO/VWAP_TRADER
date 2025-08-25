#include "test_comprehensive.h"
#include "../include/order_manager.h"
#include "../include/message.h"
#include <cstring>

class OrderTriggeringTests : public TestBase {
private:
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
    
    TradeMessage createTrade(const char* symbol, uint64_t timestamp,
                            uint32_t quantity, int32_t price) {
        TradeMessage trade;
        std::strcpy(trade.symbol, symbol);
        trade.timestamp = timestamp;
        trade.quantity = quantity;
        trade.price = price;
        return trade;
    }
    
    void setupVwap(OrderManager& manager, uint64_t baseTime, int32_t vwapPrice) {
        for (int i = 0; i < 5; ++i) {
            manager.processTrade(createTrade("TEST", baseTime + i * 200000000ULL, 
                                            100, vwapPrice));
        }
    }
    
public:
    OrderTriggeringTests() : TestBase("Order Triggering") {}
    
    void runAll() override {
        runTest("Buy Order - Ask < VWAP", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000, 
                                   9900, 100, 9950, 200);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Buy order should trigger when ask < VWAP";
                return false;
            }
            
            if (order.value().side != 'B') {
                details = "Order side should be 'B'";
                return false;
            }
            
            if (order.value().price != 9950) {
                details = "Order price should match ask price";
                return false;
            }
            
            return true;
        });
        
        runTest("Buy Order - Ask == VWAP (No Trigger)", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 10000, 200);
            
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Buy order should NOT trigger when ask == VWAP";
                return false;
            }
            
            return true;
        });
        
        runTest("Buy Order - Ask > VWAP (No Trigger)", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 10050, 200);
            
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Buy order should NOT trigger when ask > VWAP";
                return false;
            }
            
            return true;
        });
        
        runTest("Sell Order - Bid > VWAP", [this](std::string& details) {
            OrderManager manager("TEST", 'S', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   10050, 150, 10100, 200);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Sell order should trigger when bid > VWAP";
                return false;
            }
            
            if (order.value().side != 'S') {
                details = "Order side should be 'S'";
                return false;
            }
            
            if (order.value().price != 10050) {
                details = "Order price should match bid price";
                return false;
            }
            
            return true;
        });
        
        runTest("Sell Order - Bid == VWAP (No Trigger)", [this](std::string& details) {
            OrderManager manager("TEST", 'S', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   10000, 150, 10100, 200);
            
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Sell order should NOT trigger when bid == VWAP";
                return false;
            }
            
            return true;
        });
        
        runTest("Sell Order - Bid < VWAP (No Trigger)", [this](std::string& details) {
            OrderManager manager("TEST", 'S', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9950, 150, 10100, 200);
            
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Sell order should NOT trigger when bid < VWAP";
                return false;
            }
            
            return true;
        });
        
        runTest("Price Point Precision", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote1 = createQuote("TEST", baseTime + 1500000000,
                                    9900, 100, 9999, 200);
            auto order1 = manager.processQuote(quote1);
            
            if (!order1.has_value()) {
                details = "Should trigger at $99.99 when VWAP is $100.00";
                return false;
            }
            
            auto quote2 = createQuote("TEST", baseTime + 2000000000,
                                    9900, 100, 10001, 200);
            auto order2 = manager.processQuote(quote2);
            
            if (order2.has_value()) {
                details = "Should NOT trigger at $100.01 when VWAP is $100.00";
                return false;
            }
            
            return true;
        });
        
        runTest("Multiple Orders in Sequence", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            int ordersTriggered = 0;
            
            for (int i = 0; i < 5; ++i) {
                auto quote = createQuote("TEST", baseTime + (i + 2) * 1000000000ULL,
                                       9900, 100, 9950 - i * 10, 200);
                auto order = manager.processQuote(quote);
                if (order.has_value()) {
                    ordersTriggered++;
                    
                    if (order.value().price != 9950 - i * 10) {
                        details = "Order " + std::to_string(i) + " has wrong price";
                        return false;
                    }
                }
            }
            
            if (ordersTriggered != 5) {
                details = "Expected 5 orders, got " + std::to_string(ordersTriggered);
                return false;
            }
            
            return true;
        });
        
        runTest("Order Quantity Matches Quote", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000); // VWAP = $100.00
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 375);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 375) {
                details = "Order quantity should match ask quantity. Got: " +
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("VWAP Update Affects Triggering", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 2); // 2 second window
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime, 10000);
            
            auto quote1 = createQuote("TEST", baseTime + 1500000000,
                                    9900, 100, 9950, 200);
            auto order1 = manager.processQuote(quote1);
            
            if (!order1.has_value()) {
                details = "First order should trigger";
                return false;
            }
            
            for (int i = 0; i < 10; ++i) {
                manager.processTrade(createTrade("TEST", 
                                                baseTime + 2000000000 + i * 100000000ULL,
                                                200, 9900));
            }
            
            auto quote2 = createQuote("TEST", baseTime + 3000000000,
                                    9900, 100, 9950, 200);
            auto order2 = manager.processQuote(quote2);
            
            if (order2.has_value()) {
                details = "Order should NOT trigger after VWAP moved down";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runOrderTriggeringTests() {
    OrderTriggeringTests tests;
    tests.runAll();
    tests.printSummary();
}