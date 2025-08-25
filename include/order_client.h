#ifndef ORDER_CLIENT_H
#define ORDER_CLIENT_H

#include "tcp_client.h"
#include "message.h"
#include "wire_format.h"
#include "metrics.h"
#include <array>
#include <mutex>

class OrderClient : public TcpClient {
private:
    static constexpr size_t MAX_QUEUE_DEPTH = 1000;

    struct PendingSend {
        std::array<uint8_t, WireFormat::ORDER_SIZE> data;
        size_t length;
        size_t offset;
    };

    static constexpr size_t QUEUE_CAPACITY = MAX_QUEUE_DEPTH;
    PendingSend queue[QUEUE_CAPACITY];
    size_t qHead{0};
    size_t qTail{0};
    size_t qSize{0};
    std::mutex queueMutex;

    bool enqueue(const PendingSend& ps) noexcept {
        if (qSize == QUEUE_CAPACITY) return false;
        queue[qTail] = ps;
        qTail = (qTail + 1) % QUEUE_CAPACITY;
        ++qSize;
        if (qSize > g_systemMetrics.cold.queueHighWater.load(std::memory_order_relaxed)) {
            g_systemMetrics.cold.queueHighWater.store(qSize, std::memory_order_relaxed);
        }
        return true;
    }
    PendingSend& front() noexcept { return queue[qHead]; }
    void popFront() noexcept { if (qSize) { qHead = (qHead + 1) % QUEUE_CAPACITY; --qSize; } }
    bool empty() const noexcept { return qSize == 0; }

public:
    OrderClient(const std::string& host, uint16_t port);

    bool sendOrder(const OrderMessage& order) noexcept;
    void processSendQueue() noexcept;
};

#endif
