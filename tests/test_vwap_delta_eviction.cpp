#include "vwap_calculator.h"
#include "message.h"
#include <cassert>
#include <iostream>
#include <cstring>

static TradeMessage mk(uint64_t ts, uint32_t q, int32_t p){ TradeMessage t; std::memset(&t,0,sizeof(t)); t.timestamp=ts; t.quantity=q; t.price=p; return t; }

extern "C" void runVwapDeltaEvictionTests(){
    VwapCalculator calc(2); // 2 second window
    const uint64_t NS = 1'000'000'000ULL;
    calc.addTrade(mk(10*NS + 100, 100, 10000)); // t0
    calc.addTrade(mk(10*NS + 500'000'000ULL, 200, 10100)); // within window
    calc.addTrade(mk(11*NS + 100, 300, 10200)); // pushes first out
    double v = calc.getCurrentVwap();
    if (static_cast<int>(v+0.5) != 10160){
        std::cerr << "Delta eviction VWAP mismatch: got " << v << " expected 10160\n";
    return;
    }
    std::cout << "Delta timestamp eviction test passed (VWAP=" << v << ")\n";
}
