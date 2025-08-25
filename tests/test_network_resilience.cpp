#include "test_comprehensive.h"
#include "../include/tcp_client.h"
#include "../include/network_manager.h"
#include "../include/message_buffer.h"
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

class NetworkResilienceTests : public TestBase {
private:
    class TestServer {
    private:
        int serverFd;
        int port;
        std::thread serverThread;
        std::atomic<bool> running;
        std::atomic<int> connectionsAccepted;
        
    public:
        TestServer(int p) : port(p), serverFd(-1), running(false), connectionsAccepted(0) {}
        
        bool start() {
            serverFd = socket(AF_INET, SOCK_STREAM, 0);
            if (serverFd < 0) return false;
            
            int opt = 1;
            setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);
            
            if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
                close(serverFd);
                return false;
            }
            
            if (listen(serverFd, 5) < 0) {
                close(serverFd);
                return false;
            }
            
            running = true;
            serverThread = std::thread([this]() { acceptLoop(); });
            
            return true;
        }
        
        void stop() {
            running = false;
            if (serverFd >= 0) {
                shutdown(serverFd, SHUT_RDWR);
                close(serverFd);
            }
            if (serverThread.joinable()) {
                serverThread.join();
            }
        }
        
        void acceptLoop() {
            while (running) {
                sockaddr_in clientAddr{};
                socklen_t clientLen = sizeof(clientAddr);
                
                int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
                if (clientFd >= 0) {
                    connectionsAccepted++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    close(clientFd);
                }
            }
        }
        
        int getConnectionCount() const { return connectionsAccepted.load(); }
        
        ~TestServer() {
            stop();
        }
    };
    
    bool waitForConnection(TcpClient& client, int timeoutMs = 5000) {
        auto start = std::chrono::steady_clock::now();
        while (!client.isConnected()) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }
    
public:
    NetworkResilienceTests() : TestBase("Network Resilience") {}
    
    void runAll() override {
        signal(SIGPIPE, SIG_IGN);
        
        runTest("Connection Establishment", [this](std::string& details) {
            TestServer server(19001);
            if (!server.start()) {
                details = "Failed to start test server";
                return false;
            }
            
            TcpClient client("127.0.0.1", 19001);
            if (!client.connect()) {
                details = "Failed to connect to test server";
                return false;
            }
            
            if (!waitForConnection(client, 1000)) {
                details = "Connection not established within timeout";
                return false;
            }
            
            client.disconnect();
            return true;
        });
        
        runTest("Connection Loss Detection", [this](std::string& details) {
            TestServer server(19002);
            if (!server.start()) {
                details = "Failed to start test server";
                return false;
            }
            
            TcpClient client("127.0.0.1", 19002);
            client.connect();
            
            if (!waitForConnection(client)) {
                details = "Initial connection failed";
                return false;
            }
            
            server.stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            uint8_t testData[] = "test";
            ssize_t result = client.send(testData, sizeof(testData));
            
            if (result > 0) {
                details = "Should detect connection loss on send";
                return false;
            }
            
            if (client.isConnected()) {
                details = "Should report disconnected after connection loss";
                return false;
            }
            
            return true;
        });
        
        
        
        
        runTest("Partial Message Handling", [this](std::string& details) {
            MessageBuffer buffer;
            
            MessageHeader header;
            header.type = MessageHeader::QUOTE_TYPE;
            header.length = sizeof(QuoteMessage);
            header.reserved = 0;
            
            QuoteMessage quote;
            std::strcpy(quote.symbol, "TEST");
            quote.timestamp = 1000000000;
            quote.bidPrice = 10000;
            quote.bidQuantity = 100;
            quote.askPrice = 10050;
            quote.askQuantity = 200;
            
            buffer.append(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
            
            buffer.append(reinterpret_cast<const uint8_t*>(&quote), sizeof(quote) - 5);
            
            MessageHeader extractedHeader;
            uint8_t messageData[256];
            
            if (buffer.extractMessage(extractedHeader, messageData) == MessageBuffer::ExtractResult::SUCCESS) {
                details = "Should not extract partial message";
                return false;
            }
            
            buffer.append(reinterpret_cast<const uint8_t*>(&quote) + sizeof(quote) - 5, 5);
            
            if (buffer.extractMessage(extractedHeader, messageData) != MessageBuffer::ExtractResult::SUCCESS) {
                details = "Should extract complete message after receiving all bytes";
                return false;
            }
            
            return true;
        });
        
        runTest("Multiple Connection Attempts", [this](std::string& details) {
            TestServer server(19004);
            
            TcpClient client("127.0.0.1", 19004);
            
            if (client.connect()) {
                details = "Should fail to connect when server is down";
                return false;
            }
            
            server.start();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (!client.connect()) {
                details = "Should connect after server starts";
                return false;
            }
            
            client.disconnect();
            return true;
        });
        
        runTest("Send Buffer Full Handling", [this](std::string& details) {
            TestServer server(19005);
            server.start();
            
            TcpClient client("127.0.0.1", 19005);
            client.connect();
            
            if (!waitForConnection(client)) {
                details = "Connection failed";
                return false;
            }
            
            std::vector<char> largeData(1024 * 1024, 'X'); // 1MB of data
            
            int totalSent = 0;
            int attempts = 0;
            
            while (totalSent < 100000 && attempts < 100) {
                ssize_t sent = client.send(reinterpret_cast<uint8_t*>(largeData.data()) + totalSent, 
                                     std::min(1024, (int)largeData.size() - totalSent));
                if (sent > 0) {
                    totalSent += sent;
                } else if (sent == 0) {
                    break;
                } else {
                    break;
                }
                attempts++;
            }
            
            if (totalSent == 0) {
                details = "Should be able to send some data";
                return false;
            }
            
            return true;
        });
        
        runTest("Receive Buffer Management", [this](std::string& details) {
            MessageBuffer buffer;
            
            for (int i = 0; i < 10; i++) {
                MessageHeader header;
                header.type = MessageHeader::TRADE_TYPE;
                header.length = sizeof(TradeMessage);
                header.reserved = 0;
                
                TradeMessage trade;
                std::strcpy(trade.symbol, "TEST");
                trade.timestamp = 1000000000 + i;
                trade.quantity = 100;
                trade.price = 10000 + i;
                
                buffer.append(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
                buffer.append(reinterpret_cast<const uint8_t*>(&trade), sizeof(trade));
            }
            
            int extractedCount = 0;
            MessageHeader header;
            uint8_t data[256];
            
            while (buffer.extractMessage(header, data) == MessageBuffer::ExtractResult::SUCCESS) {
                extractedCount++;
            }
            
            if (extractedCount != 10) {
                details = "Should extract all 10 messages, got " + std::to_string(extractedCount);
                return false;
            }
            
            return true;
        });
        
    }
};

extern "C" void runNetworkResilienceTests() {
    NetworkResilienceTests tests;
    tests.runAll();
    tests.printSummary();
}