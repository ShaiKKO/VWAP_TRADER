#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include "message.h"
#include "csv_reader.h"

enum class MarketScenario {
    STEADY,
    TRENDING_UP,
    TRENDING_DOWN,
    VOLATILE,
    CSV_REPLAY
};

struct SimulatorConfig {
    uint16_t port = 9090;
    std::string symbol = "IBM";
    MarketScenario scenario = MarketScenario::STEADY;
    double basePrice = 140.00;
    double volatility = 0.02;
    int messagesPerSecond = 10;
    int duration = 60;
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

    bool start();

    void stop();

    bool isRunning() const { return running.load(); }

private:
    SimulatorConfig config;
    std::atomic<bool> running;
    std::atomic<bool> shouldStop;
    std::thread serverThread;
    int serverSocket;
    std::vector<int> clientSockets;
    std::mutex clientSocketsMutex;
    std::atomic<uint64_t> rngSeed;

    double currentBid;
    double currentAsk;
    uint64_t sequenceNumber;

    void runServer();
    bool setupSocket();
    void acceptClients();
    void broadcastMessage(const uint8_t* data, size_t size);
    void handleClient(int clientSocket);

    void generateMarketData();
    void generateSteadyPrices();
    void generateTrendingPrices(bool up);
    void generateVolatilePrices();
    void replayFromCSV();

    QuoteMessage createQuote();
    TradeMessage createTrade();
    std::vector<uint8_t> serializeQuote(const QuoteMessage& quote);
    std::vector<uint8_t> serializeTrade(const TradeMessage& trade);
    double generatePrice(double base, double volatility);
    uint64_t getCurrentTimestamp();

    void cleanup();
};

SimulatorConfig parseCommandLine(int argc, char* argv[]);
void printUsage(const char* programName);

#endif
