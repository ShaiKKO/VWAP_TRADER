#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include "vwap_calculator.h"
#include "message.h"

class VwapTest {
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
    
    static bool compareDouble(double actual, double expected, double epsilon = 0.01) {
        return std::fabs(actual - expected) < epsilon;
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
    
public:
    static bool testBasicVwapCalculation() {
        VwapCalculator calc(30);
        
        calc.addTrade(createTrade("IBM", 1000000000, 100, 13900));
        calc.addTrade(createTrade("IBM", 2000000000, 200, 13950));
        calc.addTrade(createTrade("IBM", 3000000000, 150, 13925));
        
        
        double vwap = calc.getCurrentVwap();
        double expected = 13930.56;
        
        bool passed = compareDouble(vwap, expected);
        if (!passed) {
            std::cerr << "  Expected: " << expected << ", Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testSlidingWindow() {
        VwapCalculator calc(2); // 2-second window
        
        uint64_t baseTime = 1000000000000;
        
        calc.addTrade(createTrade("IBM", baseTime, 100, 14000));
        calc.addTrade(createTrade("IBM", baseTime + 1000000000, 200, 14100));
        calc.addTrade(createTrade("IBM", baseTime + 2500000000, 300, 14200));
        
        
        double vwap = calc.getCurrentVwap();
        double expected = 14160.0;
        
        bool passed = compareDouble(vwap, expected);
        if (!passed) {
            std::cerr << "  Expected: " << expected << ", Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testEmptyWindow() {
        VwapCalculator calc(1);
        
        double vwap = calc.getCurrentVwap();
        
        bool passed = (vwap == 0.0);
        if (!passed) {
            std::cerr << "  Expected: 0.0, Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testSingleTrade() {
        VwapCalculator calc(30);
        
        calc.addTrade(createTrade("IBM", 1000000000, 100, 14500));
        
        double vwap = calc.getCurrentVwap();
        double expected = 14500.0;
        
        bool passed = compareDouble(vwap, expected);
        if (!passed) {
            std::cerr << "  Expected: " << expected << ", Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testWindowCompletion() {
        VwapCalculator calc(1); // 1-second window
        
        uint64_t baseTime = 1000000000000;
        
        calc.addTrade(createTrade("IBM", baseTime, 100, 14000));
        
        if (calc.hasCompleteWindow()) {
            std::cerr << "  Window marked complete too early" << std::endl;
            return false;
        }
        
        calc.addTrade(createTrade("IBM", baseTime + 1000000000, 100, 14000));
        
        if (!calc.hasCompleteWindow()) {
            std::cerr << "  Window not marked complete after duration" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static bool testInvalidTrades() {
        VwapCalculator calc(30);
        
        calc.addTrade(createTrade("IBM", 1000000000, 100, 14000));
        
        calc.addTrade(createTrade("IBM", 2000000000, 0, 14000));    // Zero quantity
        calc.addTrade(createTrade("IBM", 3000000000, 100, 0));      // Zero price
        calc.addTrade(createTrade("IBM", 4000000000, 100, -100));   // Negative price
        
        if (calc.getTradeCount() != 1) {
            std::cerr << "  Invalid trades were accepted" << std::endl;
            return false;
        }
        
        double vwap = calc.getCurrentVwap();
        return compareDouble(vwap, 14000.0);
    }
    
    static bool testOverflowProtection() {
        VwapCalculator calc(30);
        
        calc.addTrade(createTrade("IBM", 1000000000, 1000000, 100000));
        
        double vwap = calc.getCurrentVwap();
        
        bool passed = (vwap == 100000.0);
        if (!passed) {
            std::cerr << "  Failed to handle large values: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testPrecision() {
        VwapCalculator calc(30);
        
        calc.addTrade(createTrade("IBM", 1000000000, 100, 13901));
        calc.addTrade(createTrade("IBM", 2000000000, 100, 13902));
        calc.addTrade(createTrade("IBM", 3000000000, 100, 13903));
        
        double vwap = calc.getCurrentVwap();
        double expected = 13902.0;
        
        bool passed = compareDouble(vwap, expected);
        if (!passed) {
            std::cerr << "  Expected: " << expected << ", Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testContinuousWindow() {
        VwapCalculator calc(2); // 2-second window
        
        uint64_t baseTime = 1000000000000;
        
        for (int i = 0; i < 5; i++) {
            calc.addTrade(createTrade("IBM", 
                                     baseTime + i * 1000000000ULL,
                                     100,
                                     14000 + i * 10));
        }
        
        
        double vwap = calc.getCurrentVwap();
        double expected = 14030.0;
        
        bool passed = compareDouble(vwap, expected);
        if (!passed) {
            std::cerr << "  Expected: " << expected << ", Got: " << vwap << std::endl;
        }
        return passed;
    }
    
    static bool testPerformance() {
        VwapCalculator calc(30);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < 10000; i++) {
            calc.addTrade(createTrade("IBM",
                                     static_cast<uint64_t>(i * 1000000),
                                     100,
                                     14000 + (i % 100)));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        
        double avgTimePerTrade = duration / 10000.0;
        
        bool passed = (avgTimePerTrade < 5.0);
        if (!passed) {
            std::cerr << "  Performance too slow: " << avgTimePerTrade 
                     << " μs/trade" << std::endl;
        }
        return passed;
    }
    
    static void runAllTests() {
        std::cout << "\n=== VWAP Calculator Test Suite ===" << std::endl;
        
        testsRun = 0;
        testsPassed = 0;
        
        printTestResult("Basic VWAP Calculation", testBasicVwapCalculation());
        printTestResult("Sliding Window", testSlidingWindow());
        printTestResult("Empty Window", testEmptyWindow());
        printTestResult("Single Trade", testSingleTrade());
        printTestResult("Window Completion", testWindowCompletion());
        printTestResult("Invalid Trades", testInvalidTrades());
        printTestResult("Overflow Protection", testOverflowProtection());
        printTestResult("Precision Handling", testPrecision());
        printTestResult("Continuous Window", testContinuousWindow());
        printTestResult("Performance", testPerformance());
        
        std::cout << "\nResults: " << testsPassed << "/" << testsRun 
                  << " tests passed" << std::endl;
        
        if (testsPassed == testsRun) {
            std::cout << "ALL TESTS PASSED ✓" << std::endl;
        } else {
            std::cout << "SOME TESTS FAILED ✗" << std::endl;
        }
    }
};

int VwapTest::testsRun = 0;
int VwapTest::testsPassed = 0;

