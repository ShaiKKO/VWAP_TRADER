#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>

class TcpClient {
public:
    enum class ConnectionState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR_STATE
    };
    
    enum class ErrorType {
        NONE,
        CONNECTION_REFUSED,
        CONNECTION_TIMEOUT,
        CONNECTION_LOST,
        SEND_FAILED,
        RECEIVE_FAILED,
        INVALID_ADDRESS
    };
    
protected:
    int socketFd;
    ConnectionState state;
    ErrorType lastError;
    
    std::string host;
    uint16_t port;
    struct sockaddr_in serverAddr;
    
    uint32_t reconnectAttempts;
    uint64_t lastConnectAttempt;
    uint32_t currentBackoffMs;
    
    uint64_t bytesReceived;
    uint64_t bytesSent;
    uint64_t messagesReceived;
    uint64_t messagesSent;
    
    static constexpr uint32_t INITIAL_BACKOFF_MS = 1000;
    static constexpr uint32_t MAX_BACKOFF_MS = 30000;
    static constexpr uint32_t MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int CONNECT_TIMEOUT_SEC = 5;
    
public:
    TcpClient(const std::string& host, uint16_t port);
    virtual ~TcpClient();
    
    bool connect();
    bool connectWithTimeout(int timeoutSec);
    void disconnect();
    bool reconnect();
    bool isConnected() const noexcept { return state == ConnectionState::CONNECTED; }
    
    bool setNonBlocking();
    bool setSocketOptions();
    
    ssize_t send(const uint8_t* data, size_t len);
    ssize_t receive(uint8_t* buffer, size_t len);
    
    ErrorType getLastError() const noexcept { return lastError; }
    std::string getErrorString() const;
    
    int getSocketFd() const noexcept { return socketFd; }
    ConnectionState getState() const noexcept { return state; }
    void printStatistics() const;
    
protected:
    bool createSocket();
    bool resolveAddress();
    [[gnu::cold]] void handleConnectError();
    uint32_t calculateBackoff() noexcept;
};

#endif // TCP_CLIENT_H