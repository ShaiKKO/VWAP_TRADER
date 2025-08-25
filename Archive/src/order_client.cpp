#include "order_client.h"
#include "message_serializer.h"
#include "wire_format.h"
#include <iostream>
#include <iomanip>
#include <cerrno>
#include <cstring>
#include "metrics.h"
#include "features.h"

OrderClient::OrderClient(const std::string& host, uint16_t port)
    : TcpClient(host, port) {
}

bool OrderClient::sendOrder(const OrderMessage& order) noexcept {
    if (state != ConnectionState::CONNECTED) {
    std::cerr << "Cannot send order: not connected" << std::endl;
        return false;
    }

    if (order.side != 'B' && order.side != 'S') return false;
    if (order.quantity == 0) return false;
    if (order.price <= 0) return false;

    uint8_t buffer[WireFormat::ORDER_SIZE];
    size_t size = MessageSerializer::serializeOrder(buffer, sizeof(buffer), order);

    if (size == 0) {
        return false;
    }

    ssize_t sent = this->send(buffer, size);
    if (sent == static_cast<ssize_t>(size)) {
        std::cout << "Order sent: "
                  << (order.side == 'B' ? "BUY" : "SELL")
                  << " " << order.quantity
                  << " @ $" << std::fixed << std::setprecision(2)
                  << (order.price / 100.0) << std::endl;
        g_systemMetrics.hot.ordersPlaced.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        PendingSend ps{}; std::memcpy(ps.data.data(), buffer, size); ps.length = size; ps.offset = 0;
        std::lock_guard<std::mutex> lock(queueMutex);
        if (enqueue(ps)) { g_systemMetrics.cold.partialSends.fetch_add(1, std::memory_order_relaxed); return true; }
        g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed); return false;
    }
    if (sent >= 0 && sent < static_cast<ssize_t>(size)) {
        PendingSend ps{}; std::memcpy(ps.data.data(), buffer + sent, size - sent); ps.length = size - sent; ps.offset = 0;
        std::lock_guard<std::mutex> lock(queueMutex);
        if (enqueue(ps)) { g_systemMetrics.cold.partialSends.fetch_add(1, std::memory_order_relaxed); return true; }
        g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed); return false;
    }
    if (sent < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            state = ConnectionState::DISCONNECTED;
            std::cerr << "Connection lost: " << std::strerror(errno) << std::endl;
        } else {
            std::cerr << "Send error: " << std::strerror(errno) << std::endl;
        }
        return false;
    }
    return true;
}

void OrderClient::processSendQueue() noexcept {
    std::lock_guard<std::mutex> lock(queueMutex);

    if (Features::ENABLE_WRITEV_BATCHING) {
        if (qSize > 1) { flushBatch(); return; }
        // fall through to single-send loop for 0/1 pending
    }

    while (!empty() && state == ConnectionState::CONNECTED) {
        auto& pending = front();
        size_t remaining = pending.length - pending.offset;
        ssize_t sent = this->send(pending.data.data() + pending.offset, remaining);

        if (sent > 0) {
            pending.offset += sent;
            if (pending.offset >= pending.length) { popFront(); }
        } else if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {

                break;
            } else if (errno == EINTR) {

                continue;
            } else if (errno == EPIPE || errno == ECONNRESET) {

                state = ConnectionState::DISCONNECTED;

                while (!empty()) popFront();
                break;
            } else {

                while (!empty()) popFront();
                break;
            }
        }
    }
}

void OrderClient::flushBatch() noexcept {
    if (empty() || state != ConnectionState::CONNECTED) return;
    static const size_t MAX_IOV = 16; // tune
    struct iovec iov[MAX_IOV];
    size_t n = 0; size_t idx = qHead;
    while (n < MAX_IOV && n < qSize) {
        PendingSend& ps = queue[idx];
        size_t rem = ps.length - ps.offset;
        if (rem == 0) { idx = (idx + 1) % QUEUE_CAPACITY; continue; }
        iov[n].iov_base = ps.data.data() + ps.offset;
        iov[n].iov_len = rem;
        ++n; idx = (idx + 1) % QUEUE_CAPACITY;
    }
    if (n == 0) return;
    int sock = getSocketFd();
    if (sock < 0) return;
    ssize_t sent = ::writev(sock, iov, static_cast<int>(n));
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return; // try later
        if (errno == EPIPE || errno == ECONNRESET) { state = ConnectionState::DISCONNECTED; while (!empty()) popFront(); }
        return;
    }
    size_t remainingSent = static_cast<size_t>(sent);
    while (remainingSent && !empty()) {
        PendingSend& ps = front();
        size_t rem = ps.length - ps.offset;
        if (remainingSent >= rem) { remainingSent -= rem; popFront(); }
        else { ps.offset += remainingSent; remainingSent = 0; }
    }
}
