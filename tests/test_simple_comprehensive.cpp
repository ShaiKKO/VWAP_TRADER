#include <iostream>
#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/message.h"
#include <cstring>

class SimpleTest : public TestBase {
private:
    TradeMessage createTrade(uint64_t timestamp, uint32_t quantity, int32_t price) {
        TradeMessage trade;
        std::strcpy(trade.symbol, "TEST");
        trade.timestamp = timestamp;
        trade.quantity = quantity;
        trade.price = price;
        return trade;
    }
    
public:
    SimpleTest() : TestBase("Simple VWAP Test") {}
    
    void runAll() override {
        runTest("Basic VWAP", [this](std::string& details) {
            VwapCalculator calc(10);
            
            calc.addTrade(createTrade(1000000000, 100, 10000));
            calc.addTrade(createTrade(2000000000, 100, 10200));
            
            double vwap = calc.getCurrentVwap();
            double expected = 10100.0;
            
            if (!TestUtils::compareDouble(vwap, expected)) {
                details = "Expected " + std::to_string(expected) + ", got " + std::to_string(vwap);
                return false;
            }
            
            return true;
        });
    }
};

int main() {
    std::cout << "Testing Comprehensive Test Framework\n";
    std::cout << "====================================\n\n";
    
    SimpleTest test;
    test.runAll();
    test.printSummary();
    
    return 0;
}