#include <iostream>
#include <cmath>
#include <cstring>
#include "order_manager.h"
#include "message.h"

class OrderManagerTest {
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
    
    static TradeMessage createTrade(const char* symbol, uint64_t timestamp, 
                                   uint32_t quantity, int32_t price) {
        TradeMessage trade;
        std::memset(trade.symbol, 0, sizeof(trade.symbol));
        std::strncpy(trade.symbol, symbol, sizeof(trade.symbol) - 1);
        trade.timestamp = timestamp;
        trade.quantity = quantity;
        trade.price = price;
        return trade;
    }
    
    static QuoteMessage createQuote(const char* symbol, uint64_t timestamp,
                                   int32_t bidPrice, uint32_t bidQuantity,
                                   int32_t askPrice, uint32_t askQuantity) {
        QuoteMessage quote;
        std::memset(quote.symbol, 0, sizeof(quote.symbol));
        std::strncpy(quote.symbol, symbol, sizeof(quote.symbol) - 1);
        quote.timestamp = timestamp;
        quote.bidPrice = bidPrice;
        quote.bidQuantity = bidQuantity;
        quote.askPrice = askPrice;
        quote.askQuantity = askQuantity;
        return quote;
    }
    
public:
    static bool testInitialization() {
        OrderManager manager("IBM", 'B', 100, 2);
        
        bool passed = (manager.getState() == OrderManager::State::WAITING_FOR_FIRST_WINDOW);
        passed = passed && (manager.getCurrentVwap() == 0.0);
        passed = passed && (manager.getTotalOrdersSent() == 0);
        
        if (!passed) {
            std::cerr << "  Initial state not correct" << std::endl;
        }
        
        return passed;
    }
    
    static bool testWaitingForVwapWindow() {
        OrderManager manager("IBM", 'B', 100, 2);
        
        QuoteMessage quote = createQuote("IBM", 1000000000, 13900, 50, 13850, 75);
        
        Optional<OrderMessage> order = manager.processQuote(quote);
        
        bool passed = !order.has_value();
        passed = passed && (manager.getState() == OrderManager::State::WAITING_FOR_FIRST_WINDOW);
        passed = passed && (manager.getTotalOrdersSent() == 0);
        
        if (!passed) {
            std::cerr << "  Should not send order before VWAP window complete" << std::endl;
        }
        
        return passed;
    }
    
    static bool testVwapWindowCompletion() {
        OrderManager manager("IBM", 'B', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        
        if (manager.getState() != OrderManager::State::WAITING_FOR_FIRST_WINDOW) {
            std::cerr << "  State should still be WAITING after first trade" << std::endl;
            return false;
        }
        
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        if (manager.getState() != OrderManager::State::READY_TO_TRADE) {
            std::cerr << "  State should be READY_TO_TRADE after window complete" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testBuyOrderTrigger() {
        OrderManager manager("IBM", 'B', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 500000000ULL, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        double vwap = manager.getCurrentVwap();
        if (std::fabs(vwap - 14000.0) > 0.01) {
            std::cerr << "  VWAP should be 14000, got " << vwap << std::endl;
            return false;
        }
        
        QuoteMessage quote = createQuote("IBM", baseTime + 1500000000ULL, 
                                        13900, 50,
                                        13950, 75);
        
        Optional<OrderMessage> orderOpt = manager.processQuote(quote);
        
        if (!orderOpt.has_value()) {
            std::cerr << "  Should trigger buy order when ask < VWAP" << std::endl;
            return false;
        }
        
        OrderMessage order = orderOpt.value();
        
        bool passed = (order.side == 'B');
        passed = passed && (order.price == 13950);
        passed = passed && (order.quantity == 75);
        
        if (!passed) {
            std::cerr << "  Order details incorrect" << std::endl;
            std::cerr << "    Side: " << order.side << " (expected B)" << std::endl;
            std::cerr << "    Price: " << order.price << " (expected 13950)" << std::endl;
            std::cerr << "    Quantity: " << order.quantity << " (expected 75)" << std::endl;
        }
        
        return passed;
    }
    
    static bool testSellOrderTrigger() {
        OrderManager manager("IBM", 'S', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 500000000ULL, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        QuoteMessage quote = createQuote("IBM", baseTime + 1500000000ULL, 
                                        14050, 80,
                                        14100, 90);
        
        Optional<OrderMessage> orderOpt = manager.processQuote(quote);
        
        if (!orderOpt.has_value()) {
            std::cerr << "  Should trigger sell order when bid > VWAP" << std::endl;
            return false;
        }
        
        OrderMessage order = orderOpt.value();
        
        bool passed = (order.side == 'S');
        passed = passed && (order.price == 14050);
        passed = passed && (order.quantity == 80);
        
        if (!passed) {
            std::cerr << "  Order details incorrect" << std::endl;
        }
        
        return passed;
    }
    
    static bool testMaxOrderSizeLimit() {
        OrderManager manager("IBM", 'B', 50, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        QuoteMessage quote = createQuote("IBM", baseTime + 1500000000ULL, 
                                        13900, 50,
                                        13950, 200);
        
        Optional<OrderMessage> orderOpt = manager.processQuote(quote);
        
        if (!orderOpt.has_value()) {
            std::cerr << "  Should trigger order" << std::endl;
            return false;
        }
        
        OrderMessage order = orderOpt.value();
        
        if (order.quantity != 50) {
            std::cerr << "  Order quantity should be capped at 50, got " << order.quantity << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testUnfavorablePrice() {
        OrderManager manager("IBM", 'B', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        QuoteMessage quote = createQuote("IBM", baseTime + 1500000000ULL, 
                                        13900, 50,
                                        14050, 75);
        
        Optional<OrderMessage> orderOpt = manager.processQuote(quote);
        
        if (orderOpt.has_value()) {
            std::cerr << "  Should not trigger buy order when ask >= VWAP" << std::endl;
            return false;
        }
        
        return manager.getTotalOrdersSent() == 0;
    }
    
    static bool testOrderHistory() {
        OrderManager manager("IBM", 'B', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        QuoteMessage quote1 = createQuote("IBM", baseTime + 1500000000ULL, 13900, 50, 13950, 75);
        manager.processQuote(quote1);
        
        QuoteMessage quote2 = createQuote("IBM", baseTime + 2000000000ULL, 13850, 60, 13900, 80);
        manager.processQuote(quote2);
        
        const std::vector<OrderManager::OrderRecord>& history = manager.getOrderHistory();
        
        if (history.size() != 2) {
            std::cerr << "  Should have 2 orders in history, got " << history.size() << std::endl;
            return false;
        }
        
        bool passed = (history[0].side == 'B' && history[0].price == 13950);
        passed = passed && (history[1].side == 'B' && history[1].price == 13900);
        
        if (!passed) {
            std::cerr << "  Order history details incorrect" << std::endl;
        }
        
        return passed;
    }
    
    static bool testSlidingVwapWindow() {
        OrderManager manager("IBM", 'B', 100, 2);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14100));
        manager.processTrade(createTrade("IBM", baseTime + 2000000000ULL, 100, 14200));
        
        manager.processTrade(createTrade("IBM", baseTime + 3000000000ULL, 100, 14300));
        
        double vwap = manager.getCurrentVwap();
        double expected = 14200.0;  // Trades at t=1s (14100), t=2s (14200), t=3s (14300)
        
        if (std::fabs(vwap - expected) > 0.01) {
            std::cerr << "  VWAP should be " << expected << " (trades at 14100, 14200, 14300), got " << vwap << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testStateContinuity() {
        OrderManager manager("IBM", 'S', 100, 1);
        
        uint64_t baseTime = 1000000000000ULL;
        
        manager.processTrade(createTrade("IBM", baseTime, 100, 14000));
        manager.processTrade(createTrade("IBM", baseTime + 1000000000ULL, 100, 14000));
        
        if (manager.getState() != OrderManager::State::READY_TO_TRADE) {
            std::cerr << "  Should be READY_TO_TRADE after window complete" << std::endl;
            return false;
        }
        
        QuoteMessage quote = createQuote("IBM", baseTime + 1500000000ULL, 14050, 80, 14100, 90);
        manager.processQuote(quote);
        
        if (manager.getState() != OrderManager::State::READY_TO_TRADE) {
            std::cerr << "  Should remain READY_TO_TRADE after sending order" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static void runAllTests() {
        std::cout << "\n=== OrderManager Test Suite ===" << std::endl;
        
        testsRun = 0;
        testsPassed = 0;
        
        printTestResult("Initialization", testInitialization());
        printTestResult("Waiting for VWAP Window", testWaitingForVwapWindow());
        printTestResult("VWAP Window Completion", testVwapWindowCompletion());
        printTestResult("Buy Order Trigger", testBuyOrderTrigger());
        printTestResult("Sell Order Trigger", testSellOrderTrigger());
        printTestResult("Max Order Size Limit", testMaxOrderSizeLimit());
        printTestResult("Unfavorable Price", testUnfavorablePrice());
        printTestResult("Order History", testOrderHistory());
        printTestResult("Sliding VWAP Window", testSlidingVwapWindow());
        printTestResult("State Continuity", testStateContinuity());
        
        std::cout << "\nResults: " << testsPassed << "/" << testsRun 
                  << " tests passed" << std::endl;
        
        if (testsPassed == testsRun) {
            std::cout << "ALL TESTS PASSED ✓" << std::endl;
        } else {
            std::cout << "SOME TESTS FAILED ✗" << std::endl;
        }
    }
};

int OrderManagerTest::testsRun = 0;
int OrderManagerTest::testsPassed = 0;