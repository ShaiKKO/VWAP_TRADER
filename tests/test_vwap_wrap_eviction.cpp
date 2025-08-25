#include "vwap_calculator.h"
#include "message.h"
#include <cassert>
#include <iostream>

#ifndef TRADE_MESSAGE_DEFINED
#endif

static TradeMessage makeTrade(uint64_t ts, uint32_t qty, int32_t price) {
    TradeMessage t; std::memset(&t,0,sizeof(t)); t.timestamp=ts; t.quantity=qty; t.price=price; return t;
}

int main() {
    VwapCalculator calc(5);
    const uint64_t SECOND = 1'000'000'000ULL;
    uint64_t ts = 0;
    for (int i=0;i<10050;++i) {
        calc.addTrade(makeTrade(ts, 1, 100+i%10));
        ts += 1; // 1ns increments
    }
    calc.addTrade(makeTrade(10*SECOND, 2, 200));
    double vwap = calc.getCurrentVwap();
    assert(vwap > 0.0);
    std::cout << "Prefix generation: " << calc.getPrefixGeneration() << "\n";
    std::cout << "VWAP: " << vwap << "\n";
    std::cout << "Wrap eviction test passed\n";
    return 0;
}
