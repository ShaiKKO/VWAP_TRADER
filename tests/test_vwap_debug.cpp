#include <iostream>
#include <cstring>
#include "vwap_calculator.h"
#include "message.h"

TradeMessage createTrade(const char* symbol, uint64_t timestamp, 
                         uint32_t quantity, int32_t price) {
    TradeMessage trade;
    std::memset(trade.symbol, 0, sizeof(trade.symbol));
    std::strncpy(trade.symbol, symbol, sizeof(trade.symbol) - 1);
    trade.timestamp = timestamp;
    trade.quantity = quantity;
    trade.price = price;
    return trade;
}

int main() {
    std::cout << "Debug: Continuous Window Test" << std::endl;
    
    VwapCalculator calc(2); // 2-second window
    
    uint64_t baseTime = 1000000000000; // 1000 seconds in nanos
    
    std::cout << "Window duration: 2 seconds (2000000000 nanos)" << std::endl;
    
    for (int i = 0; i < 5; i++) {
        uint64_t timestamp = baseTime + i * 1000000000ULL;
        int32_t price = 14000 + i * 10;
        
        std::cout << "\nAdding trade " << i << ": t=" << timestamp 
                  << " (+" << i << "s), price=" << price << std::endl;
        
        calc.addTrade(createTrade("IBM", timestamp, 100, price));
        
        std::cout << "  After: trades=" << calc.getTradeCount() 
                  << ", vwap=" << calc.getCurrentVwap() << std::endl;
    }
    
    std::cout << "\nFinal state:" << std::endl;
    std::cout << "  Trade count: " << calc.getTradeCount() << std::endl;
    std::cout << "  VWAP: " << calc.getCurrentVwap() << std::endl;
    
    
    return 0;
}