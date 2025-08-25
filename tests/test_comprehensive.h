#ifndef TEST_COMPREHENSIVE_H
#define TEST_COMPREHENSIVE_H

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <functional>
#include <thread>
#include <atomic>
#include <random>

struct TestResult {
    std::string category;
    std::string name;
    bool passed;
    std::string details;
    double executionTimeMs;
};

class TestBase {
protected:
    std::vector<TestResult> results;
    std::string categoryName;
    
    void recordResult(const std::string& testName, bool passed, 
                     const std::string& details = "", double timeMs = 0.0) {
        results.push_back({categoryName, testName, passed, details, timeMs});
    }
    
    template<typename Func>
    bool runTest(const std::string& name, Func testFunc) {
        auto start = std::chrono::high_resolution_clock::now();
        bool passed = false;
        std::string details;
        
        try {
            passed = testFunc(details);
        } catch (const std::exception& e) {
            passed = false;
            details = std::string("Exception: ") + e.what();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double timeMs = std::chrono::duration<double, std::milli>(end - start).count();
        
        recordResult(name, passed, details, timeMs);
        
        std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " 
                  << name << " (" << std::fixed << std::setprecision(2) 
                  << timeMs << "ms)";
        if (!passed && !details.empty()) {
            std::cout << " - " << details;
        }
        std::cout << std::endl;
        
        return passed;
    }
    
public:
    TestBase(const std::string& category) : categoryName(category) {}
    virtual ~TestBase() = default;
    
    virtual void runAll() = 0;
    
    const std::vector<TestResult>& getResults() const { return results; }
    
    void printSummary() const {
        int passed = 0;
        int failed = 0;
        double totalTime = 0.0;
        
        for (const auto& result : results) {
            if (result.passed) passed++;
            else failed++;
            totalTime += result.executionTimeMs;
        }
        
        std::cout << "\n=== " << categoryName << " Summary ===" << std::endl;
        std::cout << "Passed: " << passed << "/" << (passed + failed) << std::endl;
        std::cout << "Total time: " << std::fixed << std::setprecision(2) 
                  << totalTime << "ms" << std::endl;
        
        if (failed > 0) {
            std::cout << "\nFailed tests:" << std::endl;
            for (const auto& result : results) {
                if (!result.passed) {
                    std::cout << "  - " << result.name;
                    if (!result.details.empty()) {
                        std::cout << ": " << result.details;
                    }
                    std::cout << std::endl;
                }
            }
        }
    }
};

namespace TestUtils {
    inline bool compareDouble(double a, double b, double epsilon = 0.01) {
        return std::fabs(a - b) < epsilon;
    }
    
    inline std::string formatPrice(int32_t price) {
        return "$" + std::to_string(price / 100.0);
    }
    
    template<typename T>
    inline bool vectorEquals(const std::vector<T>& a, const std::vector<T>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }
}

#endif // TEST_COMPREHENSIVE_H
