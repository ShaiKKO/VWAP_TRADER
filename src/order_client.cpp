#include "order_client.h"
#include "message_serializer.h"
#include "wire_format.h"
#include <iostream>
#include <iomanip>
#include <cerrno>
#include <cstring>
#include "metrics.h"

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
