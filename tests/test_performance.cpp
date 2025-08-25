#include "test_comprehensive.h"
#include "../include/vwap_calculator.h"
#include "../include/order_manager.h"
#include "../include/message_parser.h"
#include "../include/message_serializer.h"
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>

class PerformanceTests : public TestBase {
private:
    std::mt19937 rng{42}; // Fixed seed for reproducibility
    
    TradeMessage createRandomTrade(uint64_t timestamp) {
        TradeMessage trade;
        std::strcpy(trade.symbol, "TEST");
        trade.timestamp = timestamp;
        
        std::uniform_int_distribution<uint32_t> qtyDist(1, 10000);
        std::uniform_int_distribution<int32_t> priceDist(9000, 11000);
        
        trade.quantity = qtyDist(rng);
        trade.price = priceDist(rng);
        
        return trade;
    }
    
    QuoteMessage createRandomQuote(uint64_t timestamp) {
        QuoteMessage quote;
        std::strcpy(quote.symbol, "TEST");
        quote.timestamp = timestamp;
        
        std::uniform_int_distribution<uint32_t> qtyDist(1, 1000);
        std::uniform_int_distribution<int32_t> priceDist(9000, 11000);
        
        int32_t midPrice = priceDist(rng);
        quote.bidPrice = midPrice - 10;
        quote.askPrice = midPrice + 10;
        quote.bidQuantity = qtyDist(rng);
        quote.askQuantity = qtyDist(rng);
        
        return quote;
    }
    
    template<typename Func>
    double measureLatency(Func func, int iterations = 1000) {
        using namespace std::chrono;
        
        for (int i = 0; i < 100; ++i) {
            func();
        }
        
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = high_resolution_clock::now();
        
        double totalUs = duration<double, std::micro>(end - start).count();
        return totalUs / iterations;
    }
    
    std::vector<double> measureLatencyDistribution(std::function<void()> func, int iterations = 10000) {
        using namespace std::chrono;
        std::vector<double> latencies;
        latencies.reserve(iterations);
        
        for (int i = 0; i < 100; ++i) {
            func();
        }
        
        for (int i = 0; i < iterations; ++i) {
            auto start = high_resolution_clock::now();
            func();
            auto end = high_resolution_clock::now();
            
            double latencyUs = duration<double, std::micro>(end - start).count();
            latencies.push_back(latencyUs);
        }
        
        std::sort(latencies.begin(), latencies.end());
        return latencies;
    }
    
    double getPercentile(const std::vector<double>& sortedData, double percentile) {
        if (sortedData.empty()) return 0.0;
        
        size_t index = static_cast<size_t>(sortedData.size() * percentile / 100.0);
        if (index >= sortedData.size()) index = sortedData.size() - 1;
        
        return sortedData[index];
    }
    
public:
    PerformanceTests() : TestBase("Performance Benchmarks") {}
    
    void runAll() override {
        runTest("VWAP Calculation Latency", [this](std::string& details) {
            VwapCalculator calc(30);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 1000; ++i) {
                calc.addTrade(createRandomTrade(timestamp + i * 1000000));
            }
            
            double avgLatency = measureLatency([&]() {
                static uint64_t ts = timestamp + 2000000000ULL;
                calc.addTrade(createRandomTrade(ts++));
            });
            
            if (avgLatency > 1.0) { // Target: < 1 microsecond
                details = "Average latency " + std::to_string(avgLatency) + 
                         "µs exceeds target of 1µs";
                return false;
            }
            
            double vwapLatency = measureLatency([&]() {
                volatile double vwap = calc.getCurrentVwap();
                (void)vwap;
            });
            
            if (vwapLatency > 0.1) { // Target: < 0.1 microsecond
                details = "VWAP calculation latency " + std::to_string(vwapLatency) + 
                         "µs exceeds target of 0.1µs";
                return false;
            }
            
            return true;
        });
        
        runTest("Order Processing Latency", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 5);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 100; ++i) {
                manager.processTrade(createRandomTrade(timestamp + i * 50000000));
            }
            
            auto latencies = measureLatencyDistribution([&]() {
                static uint64_t ts = timestamp + 6000000000ULL;
                auto quote = createRandomQuote(ts++);
                volatile auto order = manager.processQuote(quote);
                (void)order;
            });
            
            double p50 = getPercentile(latencies, 50);
            double p99 = getPercentile(latencies, 99);
            
            if (p50 > 1.0) { // Target: P50 < 1µs
                details = "P50 latency " + std::to_string(p50) + "µs exceeds target";
                return false;
            }
            
            if (p99 > 10.0) { // Target: P99 < 10µs
                details = "P99 latency " + std::to_string(p99) + "µs exceeds target";
                return false;
            }
            
            return true;
        });
        
        runTest("Message Parsing Throughput", [this](std::string& details) {
            MessageParser parser;
            
            std::vector<std::vector<uint8_t>> messages;
            for (int i = 0; i < 1000; ++i) {
                std::vector<uint8_t> buffer(256);
                MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
                header->length = sizeof(QuoteMessage);
                header->type = MessageHeader::QUOTE_TYPE;
                header->reserved = 0;
                
                QuoteMessage* quote = reinterpret_cast<QuoteMessage*>(buffer.data() + sizeof(MessageHeader));
                *quote = createRandomQuote(1000000000000ULL + i);
                
                size_t size = sizeof(MessageHeader) + sizeof(QuoteMessage);
                buffer.resize(size);
                messages.push_back(buffer);
            }
            
            using namespace std::chrono;
            auto start = high_resolution_clock::now();
            
            int iterations = 10000;
            for (int i = 0; i < iterations; ++i) {
                QuoteMessage quote;
                parser.parseQuote(messages[i % messages.size()].data() + sizeof(MessageHeader), sizeof(QuoteMessage), quote);
            }
            
            auto end = high_resolution_clock::now();
            double totalMs = duration<double, std::milli>(end - start).count();
            double throughput = iterations / (totalMs / 1000.0); // messages per second
            
            if (throughput < 1000000) { // Target: > 1M messages/sec
                details = "Throughput " + std::to_string(throughput) + 
                         " msg/s below target of 1M msg/s";
                return false;
            }
            
            return true;
        });
        
        runTest("End-to-End Latency", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 2);
            MessageParser parser;
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 50; ++i) {
                manager.processTrade(createRandomTrade(timestamp + i * 40000000));
            }
            
            auto latencies = measureLatencyDistribution([&]() {
                static uint64_t ts = timestamp + 3000000000ULL;
                
                uint8_t buffer[256];
                MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer);
                header->length = sizeof(QuoteMessage);
                header->type = MessageHeader::QUOTE_TYPE;
                header->reserved = 0;
                
                QuoteMessage* quote = reinterpret_cast<QuoteMessage*>(buffer + sizeof(MessageHeader));
                *quote = createRandomQuote(ts++);
                quote->askPrice = 9000; // Ensure it triggers
                
                QuoteMessage parsedQuote;
                parser.parseQuote(buffer + sizeof(MessageHeader), sizeof(QuoteMessage), parsedQuote);
                
                volatile auto order = manager.processQuote(parsedQuote);
                (void)order;
            }, 10000);
            
            double p50 = getPercentile(latencies, 50);
            double p95 = getPercentile(latencies, 95);
            double p99 = getPercentile(latencies, 99);
            
            if (p99 > 1000.0) {
                details = "P99 end-to-end latency " + std::to_string(p99) + 
                         "µs exceeds 1ms target";
                return false;
            }
            
            details = "P50=" + std::to_string(p50) + "µs, P95=" + 
                     std::to_string(p95) + "µs, P99=" + std::to_string(p99) + "µs";
            
            return true;
        });
        
        runTest("Memory Allocation Performance", [this](std::string& details) {
            
            VwapCalculator calc(30);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 100; ++i) {
                calc.addTrade(createRandomTrade(timestamp + i * 10000000));
            }
            
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 100000; ++i) {
                calc.addTrade(createRandomTrade(timestamp + i * 1000));
                volatile double vwap = calc.getCurrentVwap();
                (void)vwap;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            
            if (totalMs > 100.0) { // Should complete 100k operations in <100ms
                details = "Operations too slow, likely allocating memory";
                return false;
            }
            
            return true;
        });
        
        runTest("Sliding Window Performance", [this](std::string& details) {
            VwapCalculator calc(1); // 1 second window
            uint64_t timestamp = 1000000000000ULL;
            
            auto latencies = measureLatencyDistribution([&]() {
                static uint64_t ts = timestamp;
                ts += 10000000; // 10ms between trades
                calc.addTrade(createRandomTrade(ts));
            }, 10000);
            
            double p99 = getPercentile(latencies, 99);
            
            if (p99 > 5.0) { // Target: P99 < 5µs even with window maintenance
                details = "Window sliding overhead too high: " + std::to_string(p99) + "µs";
                return false;
            }
            
            return true;
        });
        
        runTest("Large Window Performance", [this](std::string& details) {
            VwapCalculator calc(300); // 5 minute window
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 10000; ++i) {
                calc.addTrade(createRandomTrade(timestamp + i * 30000000)); // 30ms apart
            }
            
            double addLatency = measureLatency([&]() {
                static uint64_t ts = timestamp + 400000000000ULL;
                calc.addTrade(createRandomTrade(ts++));
            });
            
            if (addLatency > 10.0) { // Should still be fast with large window
                details = "Add trade latency with large window: " + std::to_string(addLatency) + "µs";
                return false;
            }
            
            double vwapLatency = measureLatency([&]() {
                volatile double vwap = calc.getCurrentVwap();
                (void)vwap;
            });
            
            if (vwapLatency > 0.1) { // VWAP should be cached
                details = "VWAP calculation not properly cached";
                return false;
            }
            
            return true;
        });
        
        runTest("Concurrent Quote Processing", [this](std::string& details) {
            OrderManager manager("TEST", 'B', 100, 5);
            uint64_t timestamp = 1000000000000ULL;
            
            for (int i = 0; i < 100; ++i) {
                manager.processTrade(createRandomTrade(timestamp + i * 50000000));
            }
            
            auto start = std::chrono::high_resolution_clock::now();
            
            for (int i = 0; i < 100000; ++i) {
                auto quote = createRandomQuote(timestamp + 6000000000ULL + i);
                manager.processQuote(quote);
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            double throughput = 100000 / (totalMs / 1000.0);
            
            if (throughput < 500000) { // Target: > 500k quotes/sec
                details = "Quote processing throughput only " + 
                         std::to_string(throughput) + " quotes/sec";
                return false;
            }
            
            return true;
        });
    }
};

extern "C" void runPerformanceTests() {
    PerformanceTests tests;
    tests.runAll();
    tests.printSummary();
}