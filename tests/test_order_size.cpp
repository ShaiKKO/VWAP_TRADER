#include "test_comprehensive.h"
#include "../include/order_manager.h"
#include "../include/decision_engine.h"
#include "../include/message.h"
#include <cstring>

class OrderSizeTests : public TestBase {
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
    
    void setupVwap(OrderManager& manager, uint64_t baseTime) {

        for (int i = 0; i < 5; ++i) {
            manager.processTrade(createTrade("TEST", baseTime + i * 200000000ULL, 
                                            100, 10000));
        }
    }
    
public:
    OrderSizeTests() : TestBase("Max Order Size Enforcement") {}
    
    void runAll() override {
        runTest("Order Size Equals Max", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 500, 1); // Max size = 500
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
           

            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 500);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 500) {
                details = "Order quantity should be 500, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Order Size Limited by Max", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 300, 1); // Max size = 300
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 1000);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 300) {
                details = "Order quantity should be limited to 300, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Order Size Less Than Max", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            

            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 250);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 250) {
                details = "Order quantity should be 250, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Max Size 1 Share", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 10000);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 1) {
                details = "Order quantity should be 1, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Large Max Size", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 1000000, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   9900, 100, 9950, 5000);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 5000) {
                details = "Order quantity should be 5000, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Sell Side Max Size", [this](std::string& details) {
            OrderManager manager("TEST", 'S', 750, 1);
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            
            
            auto quote = createQuote("TEST", baseTime + 1500000000,
                                   10050, 2000, 10100, 100);
            
            auto order = manager.processQuote(quote);
            
            if (!order.has_value()) {
                details = "Order should trigger";
                return false;
            }
            
            if (order.value().quantity != 750) {
                details = "Order quantity should be limited to 750, got " + 
                         std::to_string(order.value().quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("DecisionEngine Direct Test", [this](std::string& details) {
            DecisionEngine engine("TEST", 'B', 100);
            
            engine.onVwapWindowComplete();
            
            auto quote = createQuote("TEST", 1000000000000ULL,
                                   9900, 100, 9950, 500);
            
            OrderMessage out; bool triggered = engine.evaluateQuote(quote, 10000.0, out);
            if (!triggered) {
                details = "Order should be generated";
                return false;
            }
            
            if (out.quantity != 100) {
                details = "DecisionEngine should limit to max size 100, got " +
                         std::to_string(out.quantity);
                return false;
            }
            
            return true;
        });
        
        runTest("Multiple Orders Respect Max", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 200, 1); // Max size = 200
            uint64_t baseTime = 1000000000000ULL;
            
            setupVwap(manager, baseTime);
            
            std::vector<uint32_t> quoteQuantities = {100, 300, 200, 500, 150};
            std::vector<uint32_t> expectedOrders = {100, 200, 200, 200, 150};
            
            for (size_t i = 0; i < quoteQuantities.size(); ++i) {
                auto quote = createQuote("TEST", baseTime + (i + 2) * 1000000000ULL,
                                       9900, 100, 9950 - i * 10, quoteQuantities[i]);
                
                auto order = manager.processQuote(quote);
                
                if (!order.has_value()) {
                    details = "Order " + std::to_string(i) + " should trigger";
                    return false;
                }
                
                if (order.value().quantity != expectedOrders[i]) {
                    details = "Order " + std::to_string(i) + 
                             " quantity should be " + std::to_string(expectedOrders[i]) +
                             ", got " + std::to_string(order.value().quantity);
                    return false;
                }
            }
            
            return true;
        });
        
        runTest("Zero Max Size Validation", [this](std::string& details) {
            try {
                OrderManager manager("TEST", 'B', 0, 1);
                details = "Should throw exception for zero max size";
                return false;
            } catch (const std::invalid_argument& e) {
                return true;
            } catch (...) {
                details = "Wrong exception type thrown";
                return false;
            }
        });
        
        runTest("Max Size with Different Symbols", [this](std::string& details) {
            OrderManager manager1("IBM", 'B', 100, 1);
            OrderManager manager2("AAPL", 'B', 500, 1);
            
            uint64_t baseTime = 1000000000000ULL;
            
            for (int i = 0; i < 5; ++i) {
                manager1.processTrade(createTrade("IBM", baseTime + i * 200000000ULL, 
                                                 100, 10000));
                manager2.processTrade(createTrade("AAPL", baseTime + i * 200000000ULL, 
                                                 100, 15000));
            }
            
            auto quoteIBM = createQuote("IBM", baseTime + 1500000000,
                                       9900, 100, 9950, 300);
            auto orderIBM = manager1.processQuote(quoteIBM);
            
            if (!orderIBM.has_value() || orderIBM.value().quantity != 100) {
                details = "IBM order should be limited to 100";
                return false;
            }
            
            auto quoteAAPL = createQuote("AAPL", baseTime + 1500000000,
                                        14900, 100, 14950, 600);
            auto orderAAPL = manager2.processQuote(quoteAAPL);
            
            if (!orderAAPL.has_value() || orderAAPL.value().quantity != 500) {
                details = "AAPL order should be limited to 500";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runOrderSizeTests() {
    OrderSizeTests tests;
    tests.runAll();
    tests.printSummary();
}