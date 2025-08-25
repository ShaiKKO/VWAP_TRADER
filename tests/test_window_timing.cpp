#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/order_manager.h"
#include "../include/message.h"
#include <cstring>

class WindowTimingTests : public TestBase {
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
    WindowTimingTests() : TestBase("Window Timing Accuracy") {}
    
    void runAll() override {
        runTest("First Trade Starts Window", [this](std::string& details) {
            VwapCalculator calc(5); // 5 second window
            
            if (calc.hasCompleteWindow()) {
                details = "Should not have complete window initially";
                return false;
            }
            
            uint64_t firstTradeTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", firstTradeTime, 100, 10000));
            
            if (calc.hasCompleteWindow()) {
                details = "Window should not be complete after first trade";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", firstTradeTime + 4999999999ULL, 100, 10100));
            
            if (calc.hasCompleteWindow()) {
                details = "Window should not be complete at 4.999 seconds";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", firstTradeTime + 5000000000ULL, 100, 10200));
            
            if (!calc.hasCompleteWindow()) {
                details = "Window should be complete at 5 seconds";
                return false;
            }
            
            return true;
        });
        
        runTest("Window Slides Correctly", [this](std::string& details) {
            VwapCalculator calc(3); // 3 second window
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            calc.addTrade(createTrade("TEST", baseTime + 1000000000, 100, 10100));
            calc.addTrade(createTrade("TEST", baseTime + 2000000000, 100, 10200));
            
            if (calc.getTradeCount() != 3) {
                details = "Should have 3 trades in window";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", baseTime + 3500000000ULL, 100, 10300));
            
            if (calc.getTradeCount() != 3) {
                details = "Should still have 3 trades after expiring first";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", baseTime + 5000000000ULL, 100, 10400));
            
            if (calc.getTradeCount() != 3) {
                details = "Should have 3 trades after sliding window";
                return false;
            }
            
            return true;
        });
        
        runTest("Exact Window Boundary", [this](std::string& details) {
            VwapCalculator calc(2); // 2 second window
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            
            calc.addTrade(createTrade("TEST", baseTime + 2000000000ULL, 100, 10100));
            
            if (calc.getTradeCount() != 2) {
                details = "Both trades should be in 2-second window";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", baseTime + 2000000001ULL, 100, 10200));
            
            if (calc.getTradeCount() != 2) {
                details = "First trade should expire at exact boundary";
                return false;
            }
            
            return true;
        });
        
        runTest("No Trades Before Window", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 5); // 5 second window
            uint64_t baseTime = 1000000000000ULL;
            
            auto quote = createQuote("TEST", baseTime, 9900, 100, 9950, 200);
            auto order = manager.processQuote(quote);
            
            if (order.has_value()) {
                details = "Should not trigger order before window complete";
                return false;
            }
            
            if (manager.getState() != OrderManager::State::WAITING_FOR_FIRST_WINDOW) {
                details = "Should be in WAITING_FOR_FIRST_WINDOW state";
                return false;
            }
            
            return true;
        });
        
        runTest("Window Completion Transition", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 2); // 2 second window
            uint64_t baseTime = 1000000000000ULL;
            
            manager.processTrade(createTrade("TEST", baseTime, 100, 10000));
            
            if (manager.getState() != OrderManager::State::WAITING_FOR_FIRST_WINDOW) {
                details = "Should still be waiting after first trade";
                return false;
            }
            
            manager.processTrade(createTrade("TEST", baseTime + 1900000000ULL, 100, 10000));
            
            if (manager.getState() != OrderManager::State::WAITING_FOR_FIRST_WINDOW) {
                details = "Should still be waiting at 1.9 seconds";
                return false;
            }
            
            manager.processTrade(createTrade("TEST", baseTime + 2000000000ULL, 100, 10000));
            
            if (manager.getState() != OrderManager::State::READY_TO_TRADE) {
                details = "Should transition to READY_TO_TRADE after window completes";
                return false;
            }
            
            return true;
        });
        
        runTest("Multiple Window Periods", [this](std::string& details) {
            VwapCalculator calc(1); // 1 second window
            uint64_t baseTime = 1000000000000ULL;
            
            for (int window = 0; window < 5; window++) {
                uint64_t windowStart = baseTime + window * 2000000000ULL;
                
                calc.addTrade(createTrade("TEST", windowStart, 100, 10000 + window * 100));
                calc.addTrade(createTrade("TEST", windowStart + 500000000, 100, 10000 + window * 100));
                
                int expectedTrades = std::min(2 * (window + 1), 2); // Max 2 trades in 1 sec window
                
                if (calc.getTradeCount() > 2) {
                    details = "Should never have more than 2 trades in 1-second window";
                    return false;
                }
            }
            
            return true;
        });
        
        runTest("Timestamp Ordering", [this](std::string& details) {
            VwapCalculator calc(5);
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            calc.addTrade(createTrade("TEST", baseTime + 1000000000, 100, 10100));
            calc.addTrade(createTrade("TEST", baseTime + 2000000000, 100, 10200));
            
            double vwap1 = calc.getCurrentVwap();
            
            calc.addTrade(createTrade("TEST", baseTime + 1500000000, 100, 10150));
            
            double expectedVwap = (100.0 * 10000 + 100.0 * 10100 + 100.0 * 10200 + 100.0 * 10150) / 400.0;
            
            if (!TestUtils::compareDouble(calc.getCurrentVwap(), expectedVwap)) {
                details = "VWAP incorrect after out-of-order trade";
                return false;
            }
            
            return true;
        });
        
        runTest("Window Duration Accuracy", [this](std::string& details) {
            std::vector<uint32_t> windowSeconds = {1, 5, 10, 30, 60};
            
            for (uint32_t windowSec : windowSeconds) {
                VwapCalculator calc(windowSec);
                uint64_t baseTime = 1000000000000ULL;
                uint64_t windowNanos = windowSec * 1000000000ULL;
                
                calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
                
                calc.addTrade(createTrade("TEST", baseTime + windowNanos - 1, 100, 10100));
                
                if (calc.getTradeCount() != 2) {
                    details = "Should have 2 trades before window expiry for " + 
                             std::to_string(windowSec) + " second window";
                    return false;
                }
                
                calc.addTrade(createTrade("TEST", baseTime + windowNanos + 1, 100, 10200));
                
                if (calc.getTradeCount() != 2) {
                    details = "Should have 2 trades after first expires for " + 
                             std::to_string(windowSec) + " second window";
                    return false;
                }
            }
            
            return true;
        });
        
        runTest("Gap in Trading Activity", [this](std::string& details) {
            VwapCalculator calc(2); // 2 second window
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            calc.addTrade(createTrade("TEST", baseTime + 1000000000, 100, 10100));
            
            calc.addTrade(createTrade("TEST", baseTime + 10000000000ULL, 100, 10500));
            
            if (calc.getTradeCount() != 1) {
                details = "Should only have latest trade after gap";
                return false;
            }
            
            if (!TestUtils::compareDouble(calc.getCurrentVwap(), 10500.0)) {
                details = "VWAP should be price of only trade in window";
                return false;
            }
            
            return true;
        });
        
        runTest("Continuous Trading", [this](std::string& details) {
            VwapCalculator calc(3); // 3 second window
            uint64_t baseTime = 1000000000000ULL;
            
            int totalTrades = 0;
            for (int i = 0; i < 100; i++) {
                calc.addTrade(createTrade("TEST", 
                                         baseTime + i * 100000000ULL, // 100ms intervals
                                         10, 10000));
                totalTrades++;
            }
            
            int expectedTrades = 30; // 3 seconds / 100ms = 30 trades
            
            int diff = calc.getTradeCount() - expectedTrades;
            if (diff < -1 || diff > 1) {
                details = "Expected approximately " + std::to_string(expectedTrades) + 
                         " trades in window, got " + std::to_string(calc.getTradeCount());
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runWindowTimingTests() {
    WindowTimingTests tests;
    tests.runAll();
    tests.printSummary();
}