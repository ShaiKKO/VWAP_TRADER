#include "simulator.h"
#include "endian_converter.h"
#include "message_serializer.h"
#include "wire_format.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include "time_source.h"
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

MarketDataSimulator::MarketDataSimulator(const SimulatorConfig& cfg)
    : config(cfg)
    , running(false)
    , shouldStop(false)
    , serverSocket(-1)
    , rngSeed(Time::instance().nowNanos())
    , currentBid(config.basePrice - 0.01)
    , currentAsk(config.basePrice + 0.01)
    , sequenceNumber(0) {

    if (config.scenario == MarketScenario::CSV_REPLAY && !config.csvPath.empty()) {
        auto csvReader = std::make_unique<CSVReader>(config.csvPath);
        if (csvReader->loadFile()) {
            csvReplayEngine = std::make_unique<CSVReplayEngine>(
                std::move(csvReader), config.replaySpeed);
            csvReplayEngine->start();
            std::cout << "CSV replay engine initialized with "
                      << csvReplayEngine->getTotalRecords() << " records" << std::endl;
        } else {
            std::cerr << "Failed to load CSV file: " << config.csvPath << std::endl;
        }
    }
}

MarketDataSimulator::~MarketDataSimulator() {
    stop();
}

bool MarketDataSimulator::start() {
    if (running.load()) {
        return false;
    }

    shouldStop.store(false);

    if (!setupSocket()) {
        return false;
    }

    running.store(true);
    serverThread = std::thread(&MarketDataSimulator::runServer, this);

    return true;
}

void MarketDataSimulator::stop() {
    shouldStop.store(true);

    if (serverThread.joinable()) {
        serverThread.join();
    }

    cleanup();
    running.store(false);
}

bool MarketDataSimulator::setupSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(serverSocket);
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(config.port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << config.port << std::endl;
        close(serverSocket);
        return false;
    }

    if (listen(serverSocket, 5) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(serverSocket);
        return false;
    }

    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);

    if (config.verbose) {
        std::cout << "Simulator listening on port " << config.port << std::endl;
    }

    return true;
}

void MarketDataSimulator::runServer() {
    std::thread acceptThread(&MarketDataSimulator::acceptClients, this);
    std::thread dataThread(&MarketDataSimulator::generateMarketData, this);

    acceptThread.join();
    dataThread.join();
}

void MarketDataSimulator::acceptClients() {
    while (!shouldStop.load()) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket >= 0) {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            clientSockets.push_back(clientSocket);
            if (config.verbose) {
                std::cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr)
                         << ":" << ntohs(clientAddr.sin_port) << std::endl;
            }
        }

        {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            clientSockets.erase(
                std::remove_if(clientSockets.begin(), clientSockets.end(),
                    [](int sock) {
                        char buf;
                        int result = recv(sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
                        return result == 0;
                    }),
                clientSockets.end()
            );
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void MarketDataSimulator::generateMarketData() {
    uint64_t startTime = Time::instance().nowNanos();
    uint64_t lastMessageTime = startTime;

    int messagesPerSec = std::max(1, std::min(10000, config.messagesPerSecond));
    int messageInterval = 1000000 / messagesPerSec;

    while (!shouldStop.load()) {
    uint64_t now = Time::instance().nowNanos();

        if (config.duration > 0) {
        auto elapsed = (now - startTime) / 1000000000ULL;
            if (elapsed >= config.duration) {
                shouldStop.store(true);
                break;
            }
        }

    auto timeSinceLastMessage = (now - lastMessageTime) / 1000ULL;

        if (timeSinceLastMessage >= messageInterval) {

            switch (config.scenario) {
                case MarketScenario::STEADY:
                    generateSteadyPrices();
                    break;
                case MarketScenario::TRENDING_UP:
                    generateTrendingPrices(true);
                    break;
                case MarketScenario::TRENDING_DOWN:
                    generateTrendingPrices(false);
                    break;
                case MarketScenario::VOLATILE:
                    generateVolatilePrices();
                    break;
                case MarketScenario::CSV_REPLAY:
                    replayFromCSV();
                    break;
            }

            if (sequenceNumber % 3 == 0) {

                TradeMessage trade = createTrade();
                std::vector<uint8_t> data = serializeTrade(trade);
                broadcastMessage(data.data(), data.size());
            } else {

                QuoteMessage quote = createQuote();
                std::vector<uint8_t> data = serializeQuote(quote);
                broadcastMessage(data.data(), data.size());
            }

            sequenceNumber++;
            lastMessageTime = now;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
}

void MarketDataSimulator::generateSteadyPrices() {

    thread_local std::mt19937 gen(rngSeed.fetch_add(1));
    thread_local std::normal_distribution<> dis(0.0, config.volatility);

    double change = dis(gen) * 0.01;
    double midPrice = (currentBid + currentAsk) / 2.0;
    midPrice += change;

    double spread = 0.01 + (std::abs(dis(gen)) * 0.005);
    currentBid = midPrice - spread / 2.0;
    currentAsk = midPrice + spread / 2.0;
}

void MarketDataSimulator::generateTrendingPrices(bool up) {

    thread_local std::mt19937 gen(rngSeed.fetch_add(1));
    thread_local std::normal_distribution<> dis(0.0, config.volatility);

    double trend = up ? 0.001 : -0.001;
    double noise = dis(gen) * 0.01;
    double midPrice = (currentBid + currentAsk) / 2.0;
    midPrice *= (1.0 + trend + noise);

    double spread = 0.01 + (std::abs(dis(gen)) * 0.005);
    currentBid = midPrice - spread / 2.0;
    currentAsk = midPrice + spread / 2.0;
}

void MarketDataSimulator::generateVolatilePrices() {

    thread_local std::mt19937 gen(rngSeed.fetch_add(1));
    thread_local std::normal_distribution<> dis(0.0, config.volatility * 3.0);

    double change = dis(gen) * 0.02;
    double midPrice = (currentBid + currentAsk) / 2.0;
    midPrice *= (1.0 + change);

    double spread = 0.01 + (std::abs(dis(gen)) * 0.02);
    currentBid = midPrice - spread / 2.0;
    currentAsk = midPrice + spread / 2.0;
}

void MarketDataSimulator::replayFromCSV() {
    if (!csvReplayEngine) {
        if (config.verbose) {
            std::cerr << "CSV replay engine not initialized, falling back to steady prices" << std::endl;
        }
        generateSteadyPrices();
        return;
    }

    MarketDataRecord record;
    if (csvReplayEngine->getNextMessage(record)) {
        if (record.type == MarketDataRecord::QUOTE) {
            currentBid = record.quote.bidPrice;
            currentAsk = record.quote.askPrice;

            QuoteMessage quote = createQuote();
            quote.bidQuantity = record.quote.bidQuantity;
            quote.askQuantity = record.quote.askQuantity;

            auto data = serializeQuote(quote);
            broadcastMessage(data.data(), data.size());

            if (config.verbose) {
                std::cout << "[CSV] Quote: " << config.symbol
                          << " Bid: $" << currentBid
                          << " Ask: $" << currentAsk
                          << " Progress: " << csvReplayEngine->getProgress() << "%"
                          << std::endl;
            }
        } else if (record.type == MarketDataRecord::TRADE) {
            TradeMessage trade = createTrade();
            trade.price = static_cast<int32_t>(record.trade.price * 100);
            trade.quantity = record.trade.quantity;

            auto data = serializeTrade(trade);
            broadcastMessage(data.data(), data.size());

            if (config.verbose) {
                std::cout << "[CSV] Trade: " << config.symbol
                          << " Price: $" << record.trade.price
                          << " Qty: " << record.trade.quantity
                          << std::endl;
            }
        }
    } else {
        if (csvReplayEngine->getPosition() >= csvReplayEngine->getTotalRecords()) {
            if (config.verbose) {
                std::cout << "CSV replay completed, restarting..." << std::endl;
            }
            csvReplayEngine->start();
        }
    }
}

QuoteMessage MarketDataSimulator::createQuote() {
    QuoteMessage quote;
    memset(&quote, 0, sizeof(quote));

    memset(quote.symbol, 0, sizeof(quote.symbol));
    memcpy(quote.symbol, config.symbol.c_str(),
           std::min(sizeof(quote.symbol), config.symbol.length()));
    quote.timestamp = getCurrentTimestamp();

    thread_local std::mt19937 gen(rngSeed.fetch_add(1));
    thread_local std::uniform_int_distribution<> qty_dis(100, 999);
    quote.bidQuantity = qty_dis(gen);
    quote.bidPrice = static_cast<int32_t>(currentBid * 100);
    quote.askQuantity = qty_dis(gen);
    quote.askPrice = static_cast<int32_t>(currentAsk * 100);
    return quote;
}

TradeMessage MarketDataSimulator::createTrade() {
    TradeMessage trade;
    memset(&trade, 0, sizeof(trade));

    memset(trade.symbol, 0, sizeof(trade.symbol));
    memcpy(trade.symbol, config.symbol.c_str(),
           std::min(sizeof(trade.symbol), config.symbol.length()));
    trade.timestamp = getCurrentTimestamp();

    thread_local std::mt19937 gen(rngSeed.fetch_add(1));
    thread_local std::uniform_int_distribution<> qty_dis(100, 599);
    trade.quantity = qty_dis(gen);

    trade.price = static_cast<int32_t>((currentBid + currentAsk) * 50);
    return trade;
}

std::vector<uint8_t> MarketDataSimulator::serializeQuote(const QuoteMessage& quote) {
    std::vector<uint8_t> buffer(WireFormat::HEADER_SIZE + WireFormat::QUOTE_SIZE);

    size_t size = MessageSerializer::serializeQuoteMessage(
        buffer.data(), buffer.size(), quote);

    if (size == 0) {
        buffer.clear();
    } else {
        buffer.resize(size);
    }

    return buffer;
}

std::vector<uint8_t> MarketDataSimulator::serializeTrade(const TradeMessage& trade) {
    std::vector<uint8_t> buffer(WireFormat::HEADER_SIZE + WireFormat::TRADE_SIZE);

    size_t size = MessageSerializer::serializeTradeMessage(
        buffer.data(), buffer.size(), trade);

    if (size == 0) {
        buffer.clear();
    } else {
        buffer.resize(size);
    }

    return buffer;
}

void MarketDataSimulator::broadcastMessage(const uint8_t* data, size_t size) {

    std::vector<int> socketsCopy;
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        socketsCopy = clientSockets;
    }

    std::vector<int> failedSockets;
    for (int sock : socketsCopy) {
        ssize_t sent = send(sock, data, size, MSG_NOSIGNAL);
        if (sent < 0) {
            failedSockets.push_back(sock);
        }
    }

    if (!failedSockets.empty()) {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        for (int failedSock : failedSockets) {
            close(failedSock);
            clientSockets.erase(
                std::remove(clientSockets.begin(), clientSockets.end(), failedSock),
                clientSockets.end()
            );
        }
    }

    if (config.verbose && sequenceNumber % 10 == 0) {
        std::cout << "Sent message #" << sequenceNumber
                 << " (Bid: " << currentBid
                 << ", Ask: " << currentAsk << ")" << std::endl;
    }
}

uint64_t MarketDataSimulator::getCurrentTimestamp() {
    return Time::instance().nowNanos();
}

void MarketDataSimulator::cleanup() {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);
    for (int sock : clientSockets) {
        close(sock);
    }
    clientSockets.clear();

    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
}

SimulatorConfig parseCommandLine(int argc, char* argv[]) {
    SimulatorConfig config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-p" || arg == "--port") {
            if (++i < argc) {
                config.port = std::stoi(argv[i]);
            }
        } else if (arg == "-s" || arg == "--symbol") {
            if (++i < argc) {
                config.symbol = argv[i];
            }
        } else if (arg == "--scenario") {
            if (++i < argc) {
                std::string scenario = argv[i];
                if (scenario == "steady") config.scenario = MarketScenario::STEADY;
                else if (scenario == "up") config.scenario = MarketScenario::TRENDING_UP;
                else if (scenario == "down") config.scenario = MarketScenario::TRENDING_DOWN;
                else if (scenario == "volatile") config.scenario = MarketScenario::VOLATILE;
                else if (scenario == "csv") config.scenario = MarketScenario::CSV_REPLAY;
            }
        } else if (arg == "--price") {
            if (++i < argc) {
                config.basePrice = std::stod(argv[i]);
            }
        } else if (arg == "--volatility") {
            if (++i < argc) {
                config.volatility = std::stod(argv[i]);
            }
        } else if (arg == "--rate") {
            if (++i < argc) {
                config.messagesPerSecond = std::stoi(argv[i]);
            }
        } else if (arg == "--duration") {
            if (++i < argc) {
                config.duration = std::stoi(argv[i]);
            }
        } else if (arg == "--csv") {
            if (++i < argc) {
                config.csvPath = argv[i];
                config.scenario = MarketScenario::CSV_REPLAY;
            }
        } else if (arg == "--replay-speed") {
            if (++i < argc) {
                config.replaySpeed = std::stod(argv[i]);
            }
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        }
    }

    return config;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -p, --port PORT          Server port (default: 9090)\n"
              << "  -s, --symbol SYMBOL      Stock symbol (default: IBM)\n"
              << "  --scenario TYPE          Market scenario: steady, up, down, volatile, csv\n"
              << "  --price PRICE           Base price (default: 140.00)\n"
              << "  --volatility VOL        Price volatility (default: 0.02)\n"
              << "  --rate MSGS_PER_SEC     Message rate (default: 10)\n"
              << "  --duration SECONDS      Run duration, 0 for infinite (default: 60)\n"
              << "  --csv FILE              CSV file for replay\n"
              << "  --replay-speed SPEED    CSV replay speed multiplier (default: 1.0)\n"
              << "  -v, --verbose           Verbose output\n"
              << "  -h, --help              Show this help\n";
}
