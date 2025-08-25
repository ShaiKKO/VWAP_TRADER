#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/message.h"
#include <cstring>

class VwapAccuracyTests : public TestBase {
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
    
public:
    VwapAccuracyTests() : TestBase("VWAP Calculation Accuracy") {}
    
    void runAll() override {
        runTest("Known Input/Output Test 1", [this](std::string& details) {
            
            VwapCalculator calc(10);
            calc.addTrade(createTrade("TEST", 1000000000, 100, 10000));
            calc.addTrade(createTrade("TEST", 2000000000, 100, 10200));
            calc.addTrade(createTrade("TEST", 3000000000, 100, 10400));
            
            double vwap = calc.getCurrentVwap();
            double expected = 10200.0;
            
            if (!TestUtils::compareDouble(vwap, expected)) {
                details = "Expected VWAP: " + std::to_string(expected) + 
                         ", Got: " + std::to_string(vwap);
                return false;
            }
            return true;
        });
        
        runTest("Known Input/Output Test 2", [this](std::string& details) {
            
            VwapCalculator calc(10);
            calc.addTrade(createTrade("TEST", 1000000000, 200, 10000));
            calc.addTrade(createTrade("TEST", 2000000000, 100, 10500));
            calc.addTrade(createTrade("TEST", 3000000000, 300, 10200));
            
            double vwap = calc.getCurrentVwap();
            double expected = 10175.0;
            
            if (!TestUtils::compareDouble(vwap, expected)) {
                details = "Expected VWAP: " + std::to_string(expected) + 
                         ", Got: " + std::to_string(vwap);
                return false;
            }
            return true;
        });
        
        runTest("Fractional Penny Precision", [this](std::string& details) {
            
            VwapCalculator calc(10);
            calc.addTrade(createTrade("TEST", 1000000000, 333, 10001));
            calc.addTrade(createTrade("TEST", 2000000000, 667, 10002));
            
            double vwap = calc.getCurrentVwap();
            double expected = 10001.667;
            
            if (!TestUtils::compareDouble(vwap, expected, 0.001)) {
                details = "Expected VWAP: " + std::to_string(expected) + 
                         ", Got: " + std::to_string(vwap);
                return false;
            }
            return true;
        });
        
        runTest("Large Volume Test", [this](std::string& details) {
            VwapCalculator calc(10);
            
            calc.addTrade(createTrade("TEST", 1000000000, 1000000, 10000));
            calc.addTrade(createTrade("TEST", 2000000000, 2000000, 10100));
            calc.addTrade(createTrade("TEST", 3000000000, 3000000, 10200));
            
            double vwap = calc.getCurrentVwap();
            double expected = 10133.33;
            
            if (!TestUtils::compareDouble(vwap, expected, 1.0)) {
                details = "Expected VWAP: " + std::to_string(expected) + 
                         ", Got: " + std::to_string(vwap);
                return false;
            }
            return true;
        });
        
        runTest("Window Sliding Accuracy", [this](std::string& details) {
            VwapCalculator calc(2); // 2 second window
            
            uint64_t baseTime = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", baseTime, 100, 10000));
            calc.addTrade(createTrade("TEST", baseTime + 1000000000, 100, 10200));
            
            double vwap1 = calc.getCurrentVwap();
            double expected1 = 10100.0;
            
            if (!TestUtils::compareDouble(vwap1, expected1)) {
                details = "Initial VWAP incorrect";
                return false;
            }
            
            calc.addTrade(createTrade("TEST", baseTime + 3000000000, 100, 10400));
            
            double vwap2 = calc.getCurrentVwap();
            double expected2 = 10300.0;
            
            if (!TestUtils::compareDouble(vwap2, expected2)) {
                details = "VWAP after window slide incorrect. Expected: " + 
                         std::to_string(expected2) + ", Got: " + std::to_string(vwap2);
                return false;
            }
            
            return true;
        });
        
        runTest("Continuous Updates", [this](std::string& details) {
            VwapCalculator calc(5);
            uint64_t timestamp = 1000000000000ULL;
            
            std::vector<double> expectedVwaps;
            std::vector<double> actualVwaps;
            
            for (int i = 1; i <= 10; ++i) {
                calc.addTrade(createTrade("TEST", timestamp + i * 500000000ULL, 
                                         100 * i, 10000 + i * 10));
                
                double vwap = calc.getCurrentVwap();
                actualVwaps.push_back(vwap);
                
                double sumPV = 0, sumV = 0;
                int start = std::max(1, i - 9); // Last 10 trades or window
                for (int j = start; j <= i; ++j) {
                    if (timestamp + j * 500000000ULL > timestamp + i * 500000000ULL - 5000000000ULL) {
                        sumPV += (100 * j) * (10000 + j * 10);
                        sumV += (100 * j);
                    }
                }
                expectedVwaps.push_back(sumV > 0 ? sumPV / sumV : 0);
            }
            
            for (size_t i = 0; i < expectedVwaps.size(); ++i) {
                if (!TestUtils::compareDouble(actualVwaps[i], expectedVwaps[i], 1.0)) {
                    details = "Mismatch at update " + std::to_string(i + 1) +
                             ": Expected=" + std::to_string(expectedVwaps[i]) +
                             ", Got=" + std::to_string(actualVwaps[i]);
                    return false;
                }
            }
            
            return true;
        });
        
        runTest("Price Volatility Handling", [this](std::string& details) {
            VwapCalculator calc(10);
            uint64_t timestamp = 1000000000000ULL;
            
            calc.addTrade(createTrade("TEST", timestamp, 100, 10000));       // $100.00
            calc.addTrade(createTrade("TEST", timestamp + 1000000000, 100, 15000));  // $150.00
            calc.addTrade(createTrade("TEST", timestamp + 2000000000, 100, 8000));   // $80.00
            calc.addTrade(createTrade("TEST", timestamp + 3000000000, 100, 12000));  // $120.00
            
            double vwap = calc.getCurrentVwap();
            double expected = 11250.0; // Average: (100+150+80+120)/4 = $112.50
            
            if (!TestUtils::compareDouble(vwap, expected)) {
                details = "VWAP with volatile prices incorrect";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runVwapAccuracyTests() {
    VwapAccuracyTests tests;
    tests.runAll();
    tests.printSummary();
}