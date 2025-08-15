#include "simulator.h"
#include "endian_converter.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <algorithm>
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
    
    // Set non-blocking mode
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
            clientSockets.push_back(clientSocket);
            if (config.verbose) {
                std::cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr) 
                         << ":" << ntohs(clientAddr.sin_port) << std::endl;
            }
        }
        
        // Clean up disconnected clients
        clientSockets.erase(
            std::remove_if(clientSockets.begin(), clientSockets.end(),
                [](int sock) {
                    char buf;
                    int result = recv(sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
                    return result == 0;
                }),
            clientSockets.end()
        );
        
        usleep(100000); // 100ms
    }
}

void MarketDataSimulator::generateMarketData() {
    auto startTime = std::chrono::steady_clock::now();
    auto lastMessageTime = startTime;
    int messageInterval = 1000000 / config.messagesPerSecond; // microseconds
    
    while (!shouldStop.load()) {
        auto now = std::chrono::steady_clock::now();
        
        // Check duration limit
        if (config.duration > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
            if (elapsed >= config.duration) {
                shouldStop.store(true);
                break;
            }
        }
        
        // Rate control
        auto timeSinceLastMessage = std::chrono::duration_cast<std::chrono::microseconds>(
            now - lastMessageTime).count();
        
        if (timeSinceLastMessage >= messageInterval) {
            // Generate market data based on scenario
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
            
            // Alternate between quotes and trades
            if (sequenceNumber % 3 == 0) {
                // Send trade
                TradeMessage trade = createTrade();
                std::vector<uint8_t> data = serializeTrade(trade);
                broadcastMessage(data.data(), data.size());
            } else {
                // Send quote
                QuoteMessage quote = createQuote();
                std::vector<uint8_t> data = serializeQuote(quote);
                broadcastMessage(data.data(), data.size());
            }
            
            sequenceNumber++;
            lastMessageTime = now;
        }
        
        usleep(1000); // 1ms sleep to prevent busy waiting
    }
}

void MarketDataSimulator::generateSteadyPrices() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<> dis(0.0, config.volatility);
    
    double change = dis(gen) * 0.01;
    double midPrice = (currentBid + currentAsk) / 2.0;
    midPrice += change;
    
    double spread = 0.01 + (std::abs(dis(gen)) * 0.005);
    currentBid = midPrice - spread / 2.0;
    currentAsk = midPrice + spread / 2.0;
}

void MarketDataSimulator::generateTrendingPrices(bool up) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<> dis(0.0, config.volatility);
    
    double trend = up ? 0.001 : -0.001;
    double noise = dis(gen) * 0.01;
    double midPrice = (currentBid + currentAsk) / 2.0;
    midPrice *= (1.0 + trend + noise);
    
    double spread = 0.01 + (std::abs(dis(gen)) * 0.005);
    currentBid = midPrice - spread / 2.0;
    currentAsk = midPrice + spread / 2.0;
}

void MarketDataSimulator::generateVolatilePrices() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::normal_distribution<> dis(0.0, config.volatility * 3.0);
    
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
    strncpy(quote.symbol, config.symbol.c_str(), 8);
    quote.timestamp = getCurrentTimestamp();
    quote.bidQuantity = 100 + (rand() % 900);
    quote.bidPrice = static_cast<int32_t>(currentBid * 100);
    quote.askQuantity = 100 + (rand() % 900);
    quote.askPrice = static_cast<int32_t>(currentAsk * 100);
    return quote;
}

TradeMessage MarketDataSimulator::createTrade() {
    TradeMessage trade;
    memset(&trade, 0, sizeof(trade));
    strncpy(trade.symbol, config.symbol.c_str(), 8);
    trade.timestamp = getCurrentTimestamp();
    trade.quantity = 100 + (rand() % 500);
    // Trade at mid price
    trade.price = static_cast<int32_t>((currentBid + currentAsk) * 50);
    return trade;
}

std::vector<uint8_t> MarketDataSimulator::serializeQuote(const QuoteMessage& quote) {
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + sizeof(QuoteMessage));
    
    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
    header->length = EndianConverter::htol32(sizeof(QuoteMessage));
    header->type = MessageHeader::QUOTE_TYPE;
    
    QuoteMessage* networkQuote = reinterpret_cast<QuoteMessage*>(buffer.data() + sizeof(MessageHeader));
    memcpy(networkQuote->symbol, quote.symbol, 8);
    networkQuote->timestamp = EndianConverter::htol64(quote.timestamp);
    networkQuote->bidQuantity = EndianConverter::htol32(quote.bidQuantity);
    networkQuote->bidPrice = EndianConverter::htol32_signed(quote.bidPrice);
    networkQuote->askQuantity = EndianConverter::htol32(quote.askQuantity);
    networkQuote->askPrice = EndianConverter::htol32_signed(quote.askPrice);
    
    return buffer;
}

std::vector<uint8_t> MarketDataSimulator::serializeTrade(const TradeMessage& trade) {
    std::vector<uint8_t> buffer(sizeof(MessageHeader) + sizeof(TradeMessage));
    
    MessageHeader* header = reinterpret_cast<MessageHeader*>(buffer.data());
    header->length = EndianConverter::htol32(sizeof(TradeMessage));
    header->type = MessageHeader::TRADE_TYPE;
    
    TradeMessage* networkTrade = reinterpret_cast<TradeMessage*>(buffer.data() + sizeof(MessageHeader));
    memcpy(networkTrade->symbol, trade.symbol, 8);
    networkTrade->timestamp = EndianConverter::htol64(trade.timestamp);
    networkTrade->quantity = EndianConverter::htol32(trade.quantity);
    networkTrade->price = EndianConverter::htol32_signed(trade.price);
    
    return buffer;
}

void MarketDataSimulator::broadcastMessage(const uint8_t* data, size_t size) {
    for (auto it = clientSockets.begin(); it != clientSockets.end(); ) {
        ssize_t sent = send(*it, data, size, MSG_NOSIGNAL);
        if (sent < 0) {
            close(*it);
            it = clientSockets.erase(it);
        } else {
            ++it;
        }
    }
    
    if (config.verbose && sequenceNumber % 10 == 0) {
        std::cout << "Sent message #" << sequenceNumber 
                 << " (Bid: " << currentBid 
                 << ", Ask: " << currentAsk << ")" << std::endl;
    }
}

uint64_t MarketDataSimulator::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

void MarketDataSimulator::cleanup() {
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