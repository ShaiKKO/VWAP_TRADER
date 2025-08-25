#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <sstream>

extern "C" {
    void runVwapAccuracyTests();
    void runOrderTriggeringTests();
    void runOrderSizeTests();
    void runWindowTimingTests();
    void runNetworkResilienceTests();
    void runBinaryProtocolTests();
    void runEdgeCaseTests();
    void runVwapDeltaEvictionTests();
    void runSymbolInternTests();
    void runPerformanceTests();
    void runStressTests();
    void runBackpressureTests();
}

struct TestCategory {
    std::string name;
    std::string description;
    void (*runFunc)();
    bool enabled;
};

class ComprehensiveTestRunner {
private:
    std::vector<TestCategory> categories;
    bool runPerformance;
    bool runStress;
    bool verbose;
    
    void printHeader() {
        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "           VWAP TRADING SYSTEM - COMPREHENSIVE TEST SUITE\n";
        std::cout << "================================================================\n";
        std::cout << "Testing all requirements and edge cases\n";
        std::cout << "Target: <1ms end-to-end latency\n";
        std::cout << "================================================================\n\n";
    }
    
    void printCategoryHeader(const std::string& name, const std::string& description) {
        std::cout << "\n";
        std::cout << "----------------------------------------------------------------\n";
        std::cout << "  " << name << "\n";
        std::cout << "  " << description << "\n";
        std::cout << "----------------------------------------------------------------\n";
    }
    
    void printSummary(int totalRun, int totalPassed, double totalTime) {
        std::cout << "\n";
        std::cout << "================================================================\n";
        std::cout << "                         FINAL SUMMARY\n";
        std::cout << "================================================================\n";
        
        double passRate = (totalRun > 0) ? (totalPassed * 100.0 / totalRun) : 0;
        
        std::cout << "Total Test Categories: " << totalRun << "\n";
        std::cout << "Categories Passed: " << totalPassed << "\n";
        std::cout << "Pass Rate: " << std::fixed << std::setprecision(1) << passRate << "%\n";
        std::cout << "Total Time: " << std::fixed << std::setprecision(2) << totalTime << " seconds\n";
        
        if (passRate == 100.0) {
            std::cout << "\n✓ ALL TESTS PASSED - SYSTEM MEETS ALL REQUIREMENTS\n";
        } else {
            std::cout << "\n✗ SOME TESTS FAILED - REVIEW OUTPUT ABOVE\n";
        }
        
        std::cout << "================================================================\n\n";
    }
    
    void initializeCategories() {
        categories = {
            {"VWAP Accuracy", 
             "Testing VWAP calculation with known inputs/outputs", 
             runVwapAccuracyTests, true},
             
            {"Order Triggering", 
             "Testing buy/sell order triggers at correct price points", 
             runOrderTriggeringTests, true},
             
            {"Order Size Enforcement", 
             "Testing maximum order size limits", 
             runOrderSizeTests, true},
             
            {"Window Timing", 
             "Testing window timing accuracy and sliding behavior", 
             runWindowTimingTests, true},
             
            {"Network Resilience", 
             "Testing connection loss, reconnection, and buffering", 
             runNetworkResilienceTests, true},
             
            {"Binary Protocol", 
             "Testing byte-level protocol correctness and endianness", 
             runBinaryProtocolTests, true},
             
            {"Edge Cases", 
             "Testing empty window, single trade, zero quantity, etc.", 
             runEdgeCaseTests, true},

            {"VWAP Delta Eviction", 
             "Testing eviction correctness with delta timestamp compression", 
             runVwapDeltaEvictionTests, true},

            {"Symbol Interning", 
             "Testing symbol interning id stability and packing", 
             runSymbolInternTests, true},

            {"Backpressure Hysteresis",
             "Testing multi-connection hard watermark pause/resume hysteresis",
             runBackpressureTests, true},
             
            {"Performance Benchmarks", 
             "Testing latency and throughput requirements", 
             runPerformanceTests, runPerformance},
             
            {"Stress Tests", 
             "Testing system under high load and extreme conditions", 
             runStressTests, runStress}
        };
    }
    
public:
    ComprehensiveTestRunner(bool perf = true, bool stress = true, bool verb = false) 
        : runPerformance(perf), runStress(stress), verbose(verb) {
        initializeCategories();
    }
    
    void run() {
        printHeader();
        
        auto suiteStart = std::chrono::high_resolution_clock::now();
        
        int categoriesRun = 0;
        int categoriesPassed = 0;
        
        for (const auto& category : categories) {
            if (!category.enabled) {
                continue;
            }
            
            printCategoryHeader(category.name, category.description);
            
            categoriesRun++;
            
            std::stringstream buffer;
            std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
            
            auto categoryStart = std::chrono::high_resolution_clock::now();
            
            try {
                category.runFunc();
            } catch (const std::exception& e) {
                buffer << "EXCEPTION: " << e.what() << std::endl;
            }
            
            auto categoryEnd = std::chrono::high_resolution_clock::now();
            double categoryTime = std::chrono::duration<double>(categoryEnd - categoryStart).count();
            
            std::cout.rdbuf(old);
            
            std::string output = buffer.str();
            std::cout << output;
            
            bool passed = (output.find("FAIL") == std::string::npos) &&
                         (output.find("EXCEPTION") == std::string::npos);
            
            if (passed) {
                categoriesPassed++;
                std::cout << "✓ Category PASSED (" << std::fixed << std::setprecision(2) 
                         << categoryTime << "s)\n";
            } else {
                std::cout << "✗ Category FAILED (" << std::fixed << std::setprecision(2) 
                         << categoryTime << "s)\n";
            }
        }
        
        auto suiteEnd = std::chrono::high_resolution_clock::now();
        double totalTime = std::chrono::duration<double>(suiteEnd - suiteStart).count();
        
        printSummary(categoriesRun, categoriesPassed, totalTime);
    }
    
    void runSpecific(const std::string& categoryName) {
        for (auto& category : categories) {
            if (category.name == categoryName) {
                printHeader();
                printCategoryHeader(category.name, category.description);
                
                auto start = std::chrono::high_resolution_clock::now();
                category.runFunc();
                auto end = std::chrono::high_resolution_clock::now();
                
                double time = std::chrono::duration<double>(end - start).count();
                std::cout << "\nCompleted in " << std::fixed << std::setprecision(2) 
                         << time << " seconds\n";
                return;
            }
        }
        
        std::cerr << "Category '" << categoryName << "' not found\n";
        std::cerr << "Available categories:\n";
        for (const auto& cat : categories) {
            std::cerr << "  - " << cat.name << "\n";
        }
    }
};

int main(int argc, char* argv[]) {
    bool runPerformance = true;
    bool runStress = true;
    bool verbose = false;
    std::string specificCategory;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--no-performance") {
            runPerformance = false;
        } else if (arg == "--no-stress") {
            runStress = false;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--category" && i + 1 < argc) {
            specificCategory = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "VWAP Trading System - Comprehensive Test Suite\n\n";
            std::cout << "Usage: " << argv[0] << " [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --no-performance    Skip performance benchmark tests\n";
            std::cout << "  --no-stress        Skip stress tests\n";
            std::cout << "  --verbose, -v      Enable verbose output\n";
            std::cout << "  --category <name>  Run only specific test category\n";
            std::cout << "  --help, -h         Show this help message\n\n";
            std::cout << "Test Categories:\n";
            std::cout << "  - VWAP Accuracy\n";
            std::cout << "  - Order Triggering\n";
            std::cout << "  - Order Size Enforcement\n";
            std::cout << "  - Window Timing\n";
            std::cout << "  - Network Resilience\n";
            std::cout << "  - Binary Protocol\n";
            std::cout << "  - Edge Cases\n";
            std::cout << "  - Performance Benchmarks\n";
            std::cout << "  - Stress Tests\n";
            return 0;
        }
    }
    
    ComprehensiveTestRunner runner(runPerformance, runStress, verbose);
    
    if (!specificCategory.empty()) {
        runner.runSpecific(specificCategory);
    } else {
        runner.run();
    }
    
    return 0;
}