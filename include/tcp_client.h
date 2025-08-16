#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>

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
    struct SocketHandle {
        int fd;
        SocketHandle() noexcept : fd(-1) {}
        explicit SocketHandle(int f) noexcept : fd(f) {}
        ~SocketHandle();
        SocketHandle(const SocketHandle&) = delete;
        SocketHandle& operator=(const SocketHandle&) = delete;
        SocketHandle(SocketHandle&& other) noexcept : fd(other.fd) { other.fd = -1; }
        SocketHandle& operator=(SocketHandle&& other) noexcept { if (this!=&other){ close(); fd=other.fd; other.fd=-1;} return *this; }
        void reset(int f=-1) noexcept { close(); fd=f; }
        void close() noexcept;
        explicit operator bool() const noexcept { return fd>=0; }
    };
    SocketHandle socketFd;
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
    explicit TcpClient(const std::string& host, uint16_t port);
    ~TcpClient();

    bool connect() noexcept;
    bool connectWithTimeout(int timeoutSec) noexcept;
    void disconnect() noexcept;
    bool reconnect() noexcept;
    bool isConnected() const noexcept { return state == ConnectionState::CONNECTED; }

    bool setNonBlocking() noexcept;
    bool setSocketOptions() noexcept;

    ssize_t send(const uint8_t* data, size_t len) noexcept;
    ssize_t receive(uint8_t* buffer, size_t len) noexcept;

    ErrorType getLastError() const noexcept { return lastError; }
    std::string getErrorString() const;

    int getSocketFd() const noexcept { return socketFd.fd; }
    ConnectionState getState() const noexcept { return state; }
    void printStatistics() const;

protected:
    bool createSocket() noexcept;
    bool resolveAddress() noexcept;
    [[gnu::cold]] void handleConnectError() noexcept;
    uint32_t calculateBackoff() noexcept;
    static ErrorType mapErrno(int e, ErrorType def) noexcept;
};

#endif
