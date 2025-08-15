#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include "message.h"
#include "csv_reader.h"

enum class MarketScenario {
    STEADY,        // Stable prices with small variations
    TRENDING_UP,   // Gradually increasing prices
    TRENDING_DOWN, // Gradually decreasing prices
    VOLATILE,      // Large price swings
    CSV_REPLAY     // Replay from CSV file
};

struct SimulatorConfig {
    uint16_t port = 9090;
    std::string symbol = "IBM";
    MarketScenario scenario = MarketScenario::STEADY;
    double basePrice = 140.00;
    double volatility = 0.02;  // 2% default volatility
    int messagesPerSecond = 10;
    int duration = 60;  // seconds, 0 for infinite
    std::string csvPath = "";
    double replaySpeed = 1.0;
    bool verbose = false;
};

class MarketDataSimulator {
private:
    std::unique_ptr<CSVReplayEngine> csvReplayEngine;
    
public:
    MarketDataSimulator(const SimulatorConfig& config);
    ~MarketDataSimulator();
    
    // Start the simulator
    bool start();
    
    // Stop the simulator
    void stop();
    
    // Check if running
    bool isRunning() const { return running.load(); }
    
private:
    SimulatorConfig config;
    std::atomic<bool> running;
    std::atomic<bool> shouldStop;
    std::thread serverThread;
    int serverSocket;
    std::vector<int> clientSockets;
    
    // Price generation
    double currentBid;
    double currentAsk;
    uint64_t sequenceNumber;
    
    // Server methods
    void runServer();
    bool setupSocket();
    void acceptClients();
    void broadcastMessage(const uint8_t* data, size_t size);
    void handleClient(int clientSocket);
    
    // Message generation
    void generateMarketData();
    void generateSteadyPrices();
    void generateTrendingPrices(bool up);
    void generateVolatilePrices();
    void replayFromCSV();
    
    // Helper methods
    QuoteMessage createQuote();
    TradeMessage createTrade();
    std::vector<uint8_t> serializeQuote(const QuoteMessage& quote);
    std::vector<uint8_t> serializeTrade(const TradeMessage& trade);
    double generatePrice(double base, double volatility);
    uint64_t getCurrentTimestamp();
    
    // Cleanup
    void cleanup();
};

// Command line parser for standalone executable
SimulatorConfig parseCommandLine(int argc, char* argv[]);
void printUsage(const char* programName);

#endif // SIMULATOR_H