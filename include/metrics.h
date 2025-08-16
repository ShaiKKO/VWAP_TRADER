#ifndef METRICS_H
#define METRICS_H

#include <cstdint>
#include <atomic>
#include <cstring>
#include <cstdio>

constexpr size_t CACHE_LINE_SIZE = 64;

struct alignas(CACHE_LINE_SIZE) HotMetrics {
    std::atomic<uint64_t> messagesSent;
    std::atomic<uint64_t> messagesReceived;
    std::atomic<uint64_t> bytesReceived;
    std::atomic<uint64_t> bytesSent;
    
    std::atomic<uint64_t> ordersPlaced;
    std::atomic<uint64_t> tradesProcessed;
    std::atomic<uint64_t> quotesProcessed;
    
    static constexpr size_t PAD_BYTES_HOT = (CACHE_LINE_SIZE - 7 * sizeof(std::atomic<uint64_t>));
    unsigned char _padding[PAD_BYTES_HOT ? PAD_BYTES_HOT : 1];
    
    HotMetrics() noexcept {
        messagesSent = 0;
        messagesReceived = 0;
        bytesReceived = 0;
        bytesSent = 0;
        ordersPlaced = 0;
        tradesProcessed = 0;
        quotesProcessed = 0;
        std::memset(_padding, 0, sizeof(_padding));
    }
    
    void reset() noexcept {
        messagesSent = 0;
        messagesReceived = 0;
        bytesReceived = 0;
        bytesSent = 0;
        ordersPlaced = 0;
        tradesProcessed = 0;
        quotesProcessed = 0;
    }
};

struct alignas(CACHE_LINE_SIZE) ColdMetrics {
    std::atomic<uint64_t> connectionsAccepted;
    std::atomic<uint64_t> connectionsClosed;
    std::atomic<uint64_t> connectionErrors;
    
    std::atomic<uint64_t> queueHighWater;
    std::atomic<uint64_t> messagesDropped;
    
    std::atomic<uint64_t> completedSends;
    std::atomic<uint64_t> partialSends;
    
    static constexpr size_t PAD_BYTES_COLD = (CACHE_LINE_SIZE - 7 * sizeof(std::atomic<uint64_t>));
    unsigned char _padding[PAD_BYTES_COLD];
    
    ColdMetrics() noexcept {
        connectionsAccepted = 0;
        connectionsClosed = 0;
        connectionErrors = 0;
        queueHighWater = 0;
        messagesDropped = 0;
        completedSends = 0;
        partialSends = 0;
        std::memset(_padding, 0, sizeof(_padding));
    }
    
    void reset() noexcept {
        connectionsAccepted = 0;
        connectionsClosed = 0;
        connectionErrors = 0;
        queueHighWater = 0;
        messagesDropped = 0;
        completedSends = 0;
        partialSends = 0;
    }
};

struct alignas(CACHE_LINE_SIZE) PerformanceMetrics {
    std::atomic<uint64_t> minLatency;
    std::atomic<uint64_t> maxLatency;
    std::atomic<uint64_t> totalLatency;
    std::atomic<uint64_t> latencyCount;
    
    std::atomic<uint64_t> lastResetTime;
    std::atomic<uint64_t> peakMessagesPerSecond;
    std::atomic<uint64_t> resyncEvents;
    std::atomic<uint64_t> failedSends;
    
    static constexpr size_t PAD_BYTES_PERF = (CACHE_LINE_SIZE - 8 * sizeof(std::atomic<uint64_t>));
    unsigned char _padding[PAD_BYTES_PERF ? PAD_BYTES_PERF : 1];
    
    PerformanceMetrics() noexcept {
        minLatency = UINT64_MAX;
        maxLatency = 0;
        totalLatency = 0;
        latencyCount = 0;
        lastResetTime = 0;
        peakMessagesPerSecond = 0;
        resyncEvents = 0;
        failedSends = 0;
        std::memset(_padding, 0, sizeof(_padding));
    }
    
    void reset() noexcept {
        minLatency = UINT64_MAX;
        maxLatency = 0;
        totalLatency = 0;
        latencyCount = 0;
        lastResetTime = 0;
        peakMessagesPerSecond = 0;
        resyncEvents = 0;
        failedSends = 0;
    }
    
    double getAverageLatency() const noexcept {
        uint64_t count = latencyCount.load(std::memory_order_relaxed);
        if (count == 0) return 0.0;
        return static_cast<double>(totalLatency.load(std::memory_order_relaxed)) / count;
    }
};

struct SystemMetrics {
    HotMetrics hot;
    ColdMetrics cold;
    PerformanceMetrics perf;
    
    void reset() noexcept {
        hot.reset();
        cold.reset();
        perf.reset();
    }
};

struct LatencyHistogram {
    static constexpr size_t BUCKETS = 22;
    std::atomic<uint64_t> buckets[BUCKETS];
    LatencyHistogram() noexcept { for (size_t i=0;i<BUCKETS;++i) buckets[i]=0; }
    void record(uint64_t nanos) noexcept {
        uint64_t v = nanos;
        size_t idx = 21;
        for (size_t b=0;b<21;++b) {
            if (v < (1ull<<b)) { idx = b; break; }
        }
        buckets[idx].fetch_add(1, std::memory_order_relaxed);
    }
    void reset() noexcept { for (size_t i=0;i<BUCKETS;++i) buckets[i]=0; }
};

struct MetricsSnapshot {
    uint64_t messagesSent;
    uint64_t messagesReceived;
    uint64_t bytesReceived;
    uint64_t bytesSent;
    uint64_t ordersPlaced;
    uint64_t tradesProcessed;
    uint64_t quotesProcessed;
    uint64_t connectionsAccepted;
    uint64_t connectionsClosed;
    uint64_t connectionErrors;
    uint64_t queueHighWater;
    uint64_t messagesDropped;
    uint64_t completedSends;
    uint64_t partialSends;
    uint64_t failedSends;
    uint64_t minLatency;
    uint64_t maxLatency;
    uint64_t totalLatency;
    uint64_t latencyCount;
    uint64_t resyncEvents;
    uint64_t peakMessagesPerSecond;

    static MetricsSnapshot capture(const SystemMetrics& m) noexcept {
        MetricsSnapshot s{};
        s.messagesSent     = m.hot.messagesSent.load(std::memory_order_relaxed);
        s.messagesReceived = m.hot.messagesReceived.load(std::memory_order_relaxed);
        s.bytesReceived    = m.hot.bytesReceived.load(std::memory_order_relaxed);
        s.bytesSent        = m.hot.bytesSent.load(std::memory_order_relaxed);
        s.ordersPlaced     = m.hot.ordersPlaced.load(std::memory_order_relaxed);
        s.tradesProcessed  = m.hot.tradesProcessed.load(std::memory_order_relaxed);
        s.quotesProcessed  = m.hot.quotesProcessed.load(std::memory_order_relaxed);
        s.connectionsAccepted = m.cold.connectionsAccepted.load(std::memory_order_relaxed);
        s.connectionsClosed   = m.cold.connectionsClosed.load(std::memory_order_relaxed);
        s.connectionErrors    = m.cold.connectionErrors.load(std::memory_order_relaxed);
        s.queueHighWater      = m.cold.queueHighWater.load(std::memory_order_relaxed);
        s.messagesDropped     = m.cold.messagesDropped.load(std::memory_order_relaxed);
        s.completedSends      = m.cold.completedSends.load(std::memory_order_relaxed);
        s.partialSends        = m.cold.partialSends.load(std::memory_order_relaxed);
        s.failedSends         = m.perf.failedSends.load(std::memory_order_relaxed);
        s.minLatency          = m.perf.minLatency.load(std::memory_order_relaxed);
        s.maxLatency          = m.perf.maxLatency.load(std::memory_order_relaxed);
        s.totalLatency        = m.perf.totalLatency.load(std::memory_order_relaxed);
        s.latencyCount        = m.perf.latencyCount.load(std::memory_order_relaxed);
        s.resyncEvents        = m.perf.resyncEvents.load(std::memory_order_relaxed);
        s.peakMessagesPerSecond = m.perf.peakMessagesPerSecond.load(std::memory_order_relaxed);
        return s;
    }

    void print() const {
        std::printf("Msgs Rcvd/Sent: %llu/%llu  Bytes Rcvd/Sent: %llu/%llu  Orders: %llu  Trades: %llu  Quotes: %llu\n",
            (unsigned long long)messagesReceived, (unsigned long long)messagesSent,
            (unsigned long long)bytesReceived, (unsigned long long)bytesSent,
            (unsigned long long)ordersPlaced, (unsigned long long)tradesProcessed, (unsigned long long)quotesProcessed);
        if (latencyCount) {
            double avg = (double)totalLatency / (double)latencyCount;
            std::printf("Latency ns min/avg/max: %llu/%.0f/%llu  samples=%llu\n",
                (unsigned long long)minLatency, avg, (unsigned long long)maxLatency, (unsigned long long)latencyCount);
        }
        std::printf("Drops=%llu Resync=%llu ConnErr=%llu QHighWater=%llu\n",
            (unsigned long long)messagesDropped, (unsigned long long)resyncEvents,
            (unsigned long long)connectionErrors, (unsigned long long)queueHighWater);
    }
};

static_assert(sizeof(HotMetrics) == CACHE_LINE_SIZE, 
              "HotMetrics must be exactly one cache line");
static_assert(sizeof(ColdMetrics) == CACHE_LINE_SIZE, "ColdMetrics must be exactly one cache line");
static_assert(alignof(HotMetrics) == CACHE_LINE_SIZE, 
              "HotMetrics must be cache-line aligned");
static_assert(alignof(ColdMetrics) == CACHE_LINE_SIZE, 
              "ColdMetrics must be cache-line aligned");
static_assert(alignof(PerformanceMetrics) == CACHE_LINE_SIZE, "PerformanceMetrics must be cache-line aligned");

struct MetricsView {
    SystemMetrics* sys;
    explicit MetricsView(SystemMetrics* s) : sys(s) {}
    inline void incMessagesReceived() noexcept { sys->hot.messagesReceived.fetch_add(1, std::memory_order_relaxed); }
    inline void incMessagesSent() noexcept { sys->hot.messagesSent.fetch_add(1, std::memory_order_relaxed); }
    inline void addBytesReceived(uint64_t n) noexcept { sys->hot.bytesReceived.fetch_add(n, std::memory_order_relaxed); }
    inline void addBytesSent(uint64_t n) noexcept { sys->hot.bytesSent.fetch_add(n, std::memory_order_relaxed); }
    inline void incOrdersPlaced() noexcept { sys->hot.ordersPlaced.fetch_add(1, std::memory_order_relaxed); }
    inline void incTradesProcessed() noexcept { sys->hot.tradesProcessed.fetch_add(1, std::memory_order_relaxed); }
    inline void incQuotesProcessed() noexcept { sys->hot.quotesProcessed.fetch_add(1, std::memory_order_relaxed); }
    inline void incResyncEvents() noexcept { sys->perf.resyncEvents.fetch_add(1, std::memory_order_relaxed); }
    inline void updateLatency(uint64_t nanos) noexcept {
        auto& perf = sys->perf;
        uint64_t curMin = perf.minLatency.load(std::memory_order_relaxed);
        while (nanos < curMin && !perf.minLatency.compare_exchange_weak(curMin, nanos, std::memory_order_relaxed)) {}
        uint64_t curMax = perf.maxLatency.load(std::memory_order_relaxed);
        while (nanos > curMax && !perf.maxLatency.compare_exchange_weak(curMax, nanos, std::memory_order_relaxed)) {}
        perf.totalLatency.fetch_add(nanos, std::memory_order_relaxed);
        perf.latencyCount.fetch_add(1, std::memory_order_relaxed);
    }
};

extern SystemMetrics g_systemMetrics;
extern MetricsView g_metricsView;

#endif // METRICS_H