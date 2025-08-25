#include <iostream>

#include "test_protocol.cpp"
#include "test_vwap.cpp"
#include "test_order_manager.cpp"
#include "test_parser.cpp"
#include "test_vwap_window.cpp"

int main() {
    std::cout << "=== VWAP Trading System Test Suite ===" << std::endl;
    
    int totalTests = 0;
    int totalPassed = 0;
    
    ProtocolTest::runAllTests();
    totalTests += ProtocolTest::testsRun;
    totalPassed += ProtocolTest::testsPassed;
    
    VwapTest::runAllTests();
    totalTests += VwapTest::testsRun;
    totalPassed += VwapTest::testsPassed;
    
    OrderManagerTest::runAllTests();
    ParserTest::runAllTests();
    totalTests += ParserTest::testsRun;
    totalPassed += ParserTest::testsPassed;

    VwapWindowEdgeTest::runAllTests();
    totalTests += VwapWindowEdgeTest::testsRun;
    totalPassed += VwapWindowEdgeTest::testsPassed;
    totalTests += OrderManagerTest::testsRun;
    totalPassed += OrderManagerTest::testsPassed;
    
    std::cout << "\n=== OVERALL TEST SUMMARY ===" << std::endl;
    std::cout << "Total: " << totalPassed << "/" << totalTests 
              << " tests passed" << std::endl;
    
    if (totalPassed == totalTests) {
        std::cout << "ALL TESTS PASSED ✓✓✓" << std::endl;
        return 0;
    } else {
        std::cout << "SOME TESTS FAILED ✗✗✗" << std::endl;
        return 1;
    }
}