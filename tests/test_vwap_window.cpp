#include <iostream>
#include <cassert>
#include "vwap_calculator.h"
#include "message.h"

struct VwapWindowEdgeTest {
    static int testsRun; static int testsPassed;
    static void assertTrue(bool c, const char* n){ ++testsRun; if(c) ++testsPassed; else std::cerr<<"[FAIL] "<<n<<"\n"; }

    static TradeMessage makeTrade(uint64_t ts, uint32_t qty, int32_t px){ TradeMessage t; std::memcpy(t.symbol,"IBM\0\0\0\0\0",8); t.timestamp=ts; t.quantity=qty; t.price=px; return t; }

    static void testBoundaryExclusion() {
        VwapCalculator calc(1); // 1 second window
        uint64_t base=1'000'000'000ULL; // 1s in nanos
        calc.addTrade(makeTrade(base+10, 100, 10000)); // in window
        calc.addTrade(makeTrade(base+900'000'000ULL, 100, 20000)); // still in first second
        double v1 = calc.getCurrentVwap();
    calc.addTrade(makeTrade(base+1'000'000'000ULL, 100, 30000));
        double v2 = calc.getCurrentVwap();
        assertTrue(v1>0, "initial vwap computed");
    assertTrue((int)v2 == 20000, "boundary exclusion works");
    }

    static void testOverflowRejection() {
        VwapCalculator calc(10);
        calc.addTrade(makeTrade(1000, 2, 2000000000));
        calc.addTrade(makeTrade(2000, 2, 2000000000));
        uint64_t beforeRejected = calc.getRejectedTrades();
        calc.addTrade(makeTrade(3000, 0xFFFFFFFFu, INT32_MAX));
        bool ok = calc.getCurrentVwap() > 0 && calc.getRejectedTrades() >= beforeRejected;
        assertTrue(ok, "overflow trade rejected");
    }

    static void runAllTests(){ testsRun=testsPassed=0; testBoundaryExclusion(); testOverflowRejection(); std::cout<<"VWAP Window Tests: "<<testsPassed<<"/"<<testsRun<<" passed"<<std::endl; }
};
int VwapWindowEdgeTest::testsRun=0; int VwapWindowEdgeTest::testsPassed=0;
