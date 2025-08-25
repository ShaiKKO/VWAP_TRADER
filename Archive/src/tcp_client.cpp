#include "tcp_client.h"
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <chrono>
#include "time_source.h"
#include <iostream>
#include <netinet/tcp.h>
#include <random>
#include "metrics.h"

TcpClient::SocketHandle::~SocketHandle() { close(); }
void TcpClient::SocketHandle::close() noexcept { if (fd>=0) { ::shutdown(fd, SHUT_RDWR); ::close(fd); fd=-1; } }

TcpClient::TcpClient(const std::string& host, uint16_t port)
    : socketFd(),
      state(ConnectionState::DISCONNECTED),
      lastError(ErrorType::NONE),
      host(host),
      port(port),
      reconnectAttempts(0),
      lastConnectAttempt(0),
      currentBackoffMs(INITIAL_BACKOFF_MS),
      bytesReceived(0),
      bytesSent(0),
      messagesReceived(0),
      messagesSent(0) {

    std::memset(&serverAddr, 0, sizeof(serverAddr));
}

TcpClient::~TcpClient() { disconnect(); }

bool TcpClient::createSocket() noexcept {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { lastError = ErrorType::INVALID_ADDRESS; return false; }
    socketFd.reset(fd);
    return setSocketOptions();
}

bool TcpClient::setSocketOptions() noexcept {
    int optval = 1;
    if (setsockopt(socketFd.fd, SOL_SOCKET, SO_REUSEADDR,
                   &optval, sizeof(optval)) < 0) {
        return false;
    }

    int rcvBufSize = 65536;
    if (setsockopt(socketFd.fd, SOL_SOCKET, SO_RCVBUF,
                   &rcvBufSize, sizeof(rcvBufSize)) < 0) {
        return false;
    }

    int sndBufSize = 4096;
    if (setsockopt(socketFd.fd, SOL_SOCKET, SO_SNDBUF,
                   &sndBufSize, sizeof(sndBufSize)) < 0) {
        return false;
    }

    int noDelay = 1;
    if (setsockopt(socketFd.fd, IPPROTO_TCP, TCP_NODELAY,
                   &noDelay, sizeof(noDelay)) < 0) {
        return false;
    }
    int keepAlive = 1;
    setsockopt(socketFd.fd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));

    return true;
}

bool TcpClient::setNonBlocking() noexcept {
    int flags = fcntl(socketFd.fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }

    flags |= O_NONBLOCK;
    return fcntl(socketFd.fd, F_SETFL, flags) >= 0;
}

bool TcpClient::connect() noexcept {
    if (state == ConnectionState::CONNECTED) {
        return true;
    }
    if (!socketFd && !createSocket()) {
        return false;
    }

    if (!resolveAddress()) {
        return false;
    }

    if (!setNonBlocking()) {
        return false;
    }

    state = ConnectionState::CONNECTING;
    int result;
    do { result = ::connect(socketFd.fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    } while (result < 0 && errno == EINTR);

    if (result == 0) {
    state = ConnectionState::CONNECTED;
    reconnectAttempts = 0;
    currentBackoffMs = INITIAL_BACKOFF_MS;
    lastError = ErrorType::NONE;
        return true;
    }

    if (errno == EINPROGRESS) { return connectWithTimeout(CONNECT_TIMEOUT_SEC); }
    handleConnectError();
    return false;
}

bool TcpClient::connectWithTimeout(int timeoutSec) noexcept {
    fd_set writeSet;
    struct timeval timeout;

    FD_ZERO(&writeSet);
    FD_SET(socketFd.fd, &writeSet);

    timeout.tv_sec = timeoutSec;
    timeout.tv_usec = 0;

        int result;
        do {
            result = select(socketFd.fd + 1, nullptr, &writeSet, nullptr, &timeout);
        } while (result < 0 && errno == EINTR);

    if (result > 0) {
        int error = 0;
        socklen_t errorLen = sizeof(error);
    if (getsockopt(socketFd.fd, SOL_SOCKET, SO_ERROR,
                      &error, &errorLen) < 0 || error != 0) {
            handleConnectError();
            return false;
        }

    state = ConnectionState::CONNECTED;
    reconnectAttempts = 0;
    currentBackoffMs = INITIAL_BACKOFF_MS;
    lastError = ErrorType::NONE;
        std::cout << "Connected to " << host << ":" << port << std::endl;
        return true;
    }

    lastError = ErrorType::CONNECTION_TIMEOUT;
    state = ConnectionState::ERROR_STATE;
    return false;
}

bool TcpClient::resolveAddress() noexcept {
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr) <= 0) {
        lastError = ErrorType::INVALID_ADDRESS;
        return false;
    }

    return true;
}

bool TcpClient::reconnect() noexcept {
    if (state == ConnectionState::CONNECTED) {
        return true;
    }

    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        std::cerr << "Max reconnection attempts reached" << std::endl;
        return false;
    }

    uint64_t nowMs = Time::instance().nowNanos() / 1000000ULL;

    if (lastConnectAttempt > 0 &&
        (nowMs - lastConnectAttempt) < currentBackoffMs) {
        return false;
    }

    std::cout << "Reconnection attempt " << (reconnectAttempts + 1)
              << " after " << currentBackoffMs << "ms backoff" << std::endl;

    if (socketFd) { socketFd.reset(); }

    lastConnectAttempt = nowMs;
    reconnectAttempts++;
    g_systemMetrics.cold.partialSends.fetch_add(0, std::memory_order_relaxed);
    g_systemMetrics.cold.connectionErrors.fetch_add(0, std::memory_order_relaxed);

    if (connect()) {
        return true;
    }

    currentBackoffMs = calculateBackoff();
    return false;
}

uint32_t TcpClient::calculateBackoff() noexcept {

    thread_local std::mt19937 gen(static_cast<uint32_t>(Time::instance().nowNanos()));

    uint32_t backoff = currentBackoffMs * 2;

    std::uniform_int_distribution<uint32_t> jitterDist(0, backoff / 5);
    uint32_t jitter = jitterDist(gen);
    backoff += jitter;

    if (backoff > MAX_BACKOFF_MS) {
        backoff = MAX_BACKOFF_MS;
    }

    currentBackoffMs = backoff;
    return backoff;
}

void TcpClient::handleConnectError() noexcept {
    if (errno == ECONNREFUSED) {
        lastError = ErrorType::CONNECTION_REFUSED;
    } else if (errno == ETIMEDOUT) {
        lastError = ErrorType::CONNECTION_TIMEOUT;
    } else {
        lastError = ErrorType::CONNECTION_LOST;
    }

    state = ConnectionState::ERROR_STATE;
}

ssize_t TcpClient::send(const uint8_t* data, size_t len) noexcept {
    if (state != ConnectionState::CONNECTED) {
        return -1;
    }
    ssize_t sent = ::send(socketFd.fd, data, len, MSG_NOSIGNAL);

    if (sent > 0) {
        bytesSent += sent;
        messagesSent++;
        g_systemMetrics.hot.bytesSent.fetch_add(static_cast<uint64_t>(sent), std::memory_order_relaxed);
        g_systemMetrics.hot.messagesSent.fetch_add(1, std::memory_order_relaxed);
    } else if (sent < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            lastError = ErrorType::SEND_FAILED;
            state = ConnectionState::ERROR_STATE;
        }
    }

    return sent;
}

ssize_t TcpClient::receive(uint8_t* buffer, size_t len) noexcept {
    if (state != ConnectionState::CONNECTED) {
        return -1;
    }

    ssize_t received = ::recv(socketFd.fd, buffer, len, 0);

    if (received > 0) {
        bytesReceived += received;
        g_systemMetrics.hot.bytesReceived.fetch_add(static_cast<uint64_t>(received), std::memory_order_relaxed);
    } else if (received == 0) {
        lastError = ErrorType::CONNECTION_LOST;
        state = ConnectionState::ERROR_STATE;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        lastError = ErrorType::RECEIVE_FAILED;
        state = ConnectionState::ERROR_STATE;
    }

    return received;
}

void TcpClient::disconnect() noexcept { socketFd.reset(); state = ConnectionState::DISCONNECTED; }

std::string TcpClient::getErrorString() const {
    switch (lastError) {
        case ErrorType::NONE:
            return "No error";
        case ErrorType::CONNECTION_REFUSED:
            return "Connection refused";
        case ErrorType::CONNECTION_TIMEOUT:
            return "Connection timeout";
        case ErrorType::CONNECTION_LOST:
            return "Connection lost";
        case ErrorType::SEND_FAILED:
            return "Send failed";
        case ErrorType::RECEIVE_FAILED:
            return "Receive failed";
        case ErrorType::INVALID_ADDRESS:
            return "Invalid address";
        default:
            return "Unknown error";
    }
}

void TcpClient::printStatistics() const {
    std::cout << "\n=== TCP Client Statistics [" << host << ":" << port << "] ===" << std::endl;
    std::cout << "State: ";
    switch (state) {
        case ConnectionState::DISCONNECTED:
            std::cout << "DISCONNECTED";
            break;
        case ConnectionState::CONNECTING:
            std::cout << "CONNECTING";
            break;
        case ConnectionState::CONNECTED:
            std::cout << "CONNECTED";
            break;
        case ConnectionState::ERROR_STATE:
            std::cout << "ERROR";
            break;
    }
    std::cout << std::endl;
    std::cout << "Bytes Received: " << bytesReceived << std::endl;
    std::cout << "Bytes Sent: " << bytesSent << std::endl;
    std::cout << "Messages Received: " << messagesReceived << std::endl;
    std::cout << "Messages Sent: " << messagesSent << std::endl;
    std::cout << "Reconnect Attempts: " << reconnectAttempts << std::endl;
    std::cout << "Current Backoff: " << currentBackoffMs << "ms" << std::endl;
    if (lastError != ErrorType::NONE) {
        std::cout << "Last Error: " << getErrorString() << std::endl;
    }
    std::cout << "=================================" << std::endl;
}

TcpClient::ErrorType TcpClient::mapErrno(int e, ErrorType def) noexcept {
    switch(e) {
        case ECONNREFUSED: return ErrorType::CONNECTION_REFUSED;
        case ETIMEDOUT: return ErrorType::CONNECTION_TIMEOUT;
        case EPIPE: case ECONNRESET: return ErrorType::CONNECTION_LOST;
        default: return def;
    }
}
