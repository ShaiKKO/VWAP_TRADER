#ifndef OPTIMIZED_TYPES_H
#define OPTIMIZED_TYPES_H

#include <cstdint>
#include <cstring>
#include <utility>


struct OptimizedQuoteMessage {
    char symbol[9];  // 8 chars + null terminator
    uint64_t timestamp;
    uint32_t bidQuantity;
    int32_t bidPrice;
    uint32_t askQuantity;
    int32_t askPrice;

    OptimizedQuoteMessage() = default;

    OptimizedQuoteMessage(OptimizedQuoteMessage&& other) noexcept {
        std::memcpy(this, &other, sizeof(OptimizedQuoteMessage));
    }

    OptimizedQuoteMessage& operator=(OptimizedQuoteMessage&& other) noexcept {
        if (this != &other) {
            std::memcpy(this, &other, sizeof(OptimizedQuoteMessage));
        }
        return *this;
    }

    OptimizedQuoteMessage(const OptimizedQuoteMessage&) = delete;
    OptimizedQuoteMessage& operator=(const OptimizedQuoteMessage&) = delete;

    // Fast reset
    void reset() {
        std::memset(this, 0, sizeof(OptimizedQuoteMessage));
    }
};

struct OptimizedTradeMessage {
    char symbol[9];
    uint64_t timestamp;
    uint32_t quantity;
    int32_t price;

    OptimizedTradeMessage() = default;

    OptimizedTradeMessage(OptimizedTradeMessage&& other) noexcept {
        std::memcpy(this, &other, sizeof(OptimizedTradeMessage));
    }

    OptimizedTradeMessage& operator=(OptimizedTradeMessage&& other) noexcept {
        if (this != &other) {
            std::memcpy(this, &other, sizeof(OptimizedTradeMessage));
        }
        return *this;
    }

    OptimizedTradeMessage(const OptimizedTradeMessage&) = delete;
    OptimizedTradeMessage& operator=(const OptimizedTradeMessage&) = delete;

    void reset() {
        std::memset(this, 0, sizeof(OptimizedTradeMessage));
    }
};

struct OptimizedOrderMessage {
    char symbol[9];
    uint64_t timestamp;
    uint8_t side;  // 'B' or 'S'
    uint32_t quantity;
    int32_t price;

    OptimizedOrderMessage() = default;

    OptimizedOrderMessage(OptimizedOrderMessage&& other) noexcept {
        std::memcpy(this, &other, sizeof(OptimizedOrderMessage));
    }

    OptimizedOrderMessage& operator=(OptimizedOrderMessage&& other) noexcept {
        if (this != &other) {
            std::memcpy(this, &other, sizeof(OptimizedOrderMessage));
        }
        return *this;
    }

    OptimizedOrderMessage(const OptimizedOrderMessage&) = delete;
    OptimizedOrderMessage& operator=(const OptimizedOrderMessage&) = delete;

    void reset() {
        std::memset(this, 0, sizeof(OptimizedOrderMessage));
    }
};

template<size_t SIZE>
class StackBuffer {
private:
    alignas(64) uint8_t data[SIZE];  // Cache-line aligned
    size_t used;

public:
    StackBuffer() : used(0) {}

    uint8_t* get() { return data; }
    const uint8_t* get() const { return data; }

    size_t size() const { return used; }
    size_t capacity() const { return SIZE; }

    void setUsed(size_t n) {
        used = (n <= SIZE) ? n : SIZE;
    }

    void reset() {
        used = 0;
    }

    bool append(const void* src, size_t len) {
        if (used + len > SIZE) return false;
        std::memcpy(data + used, src, len);
        used += len;
        return true;
    }
};

using MessageBuffer256 = StackBuffer<256>;
using NetworkBuffer8K = StackBuffer<8192>;
using NetworkBuffer64K = StackBuffer<65536>;

#endif // OPTIMIZED_TYPES_H
