#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <array>

template<typename T, size_t CAPACITY>
class CircularBuffer final {
private:
    std::array<T, CAPACITY> buffer;
    size_t head;
    size_t tail;
    size_t count;

public:
    constexpr CircularBuffer() noexcept : head(0), tail(0), count(0) {}

    bool push_back(const T& item) {
        if (count == CAPACITY) {
            buffer[tail] = item;
            tail = (tail + 1) % CAPACITY;
            head = (head + 1) % CAPACITY;
        } else {
            buffer[tail] = item;
            tail = (tail + 1) % CAPACITY;
            count++;
        }
        return true;
    }

    bool push_back(T&& item) noexcept(std::is_nothrow_move_assignable<T>::value) {
        if (count == CAPACITY) {
            buffer[tail] = std::move(item);
            tail = (tail + 1) % CAPACITY;
            head = (head + 1) % CAPACITY;
        } else {
            buffer[tail] = std::move(item);
            tail = (tail + 1) % CAPACITY;
            count++;
        }
        return true;
    }

    template<typename... Args>
    bool emplace_back(Args&&... args) noexcept(
        noexcept(T(std::forward<Args>(args)...)) &&
        std::is_nothrow_move_assignable<T>::value) {
        if (count == CAPACITY) {
            buffer[tail] = T(std::forward<Args>(args)...);
            tail = (tail + 1) % CAPACITY;
            head = (head + 1) % CAPACITY;
        } else {
            buffer[tail] = T(std::forward<Args>(args)...);
            tail = (tail + 1) % CAPACITY;
            count++;
        }
        return true;
    }

    void pop_front() noexcept {
        if (count > 0) {
            head = (head + 1) % CAPACITY;
            count--;
        }
    }

    T& front() noexcept {
        return buffer[head];
    }

    const T& front() const noexcept {
        return buffer[head];
    }

    T& back() noexcept {
        size_t idx = (tail + CAPACITY - 1) % CAPACITY;
        return buffer[idx];
    }

    const T& back() const noexcept {
        size_t idx = (tail + CAPACITY - 1) % CAPACITY;
        return buffer[idx];
    }

    T& operator[](size_t idx) noexcept {
        return buffer[(head + idx) % CAPACITY];
    }

    const T& operator[](size_t idx) const noexcept {
        return buffer[(head + idx) % CAPACITY];
    }

    size_t size() const noexcept {
        return count;
    }

    bool empty() const noexcept {
        return count == 0;
    }

    bool full() const noexcept {
        return count == CAPACITY;
    }

    void clear() noexcept {
        head = 0;
        tail = 0;
        count = 0;
    }

    class iterator {
    private:
        CircularBuffer* cb;
        size_t index;

    public:
        iterator(CircularBuffer* buffer, size_t idx) : cb(buffer), index(idx) {}

        T& operator*() { return (*cb)[index]; }
        T* operator->() { return &(*cb)[index]; }

    iterator& operator++() noexcept {
            ++index;
            return *this;
        }

    bool operator!=(const iterator& other) const noexcept {
            return index != other.index;
        }
    };

    class const_iterator {
    private:
        const CircularBuffer* cb;
        size_t index;

    public:
        const_iterator(const CircularBuffer* buffer, size_t idx) : cb(buffer), index(idx) {}

        const T& operator*() const { return (*cb)[index]; }
        const T* operator->() const { return &(*cb)[index]; }

    const_iterator& operator++() noexcept {
            ++index;
            return *this;
        }

    bool operator!=(const const_iterator& other) const noexcept {
            return index != other.index;
        }
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, count); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, count); }
};

struct VwapTradeRecord {
    uint64_t timestamp;
    uint32_t quantity;
    int32_t price;
    uint64_t priceVolume;

    VwapTradeRecord() : timestamp(0), quantity(0), price(0), priceVolume(0) {}

    VwapTradeRecord(uint64_t ts, uint32_t qty, int32_t p)
        : timestamp(ts), quantity(qty), price(p),
          priceVolume(static_cast<uint64_t>(p) * static_cast<uint64_t>(qty)) {}
};

using VwapWindowBuffer = CircularBuffer<VwapTradeRecord, 10000>;

#endif
