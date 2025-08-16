#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>
#include <cstdint>
#include <array>
#include <atomic>

template<typename T, size_t POOL_SIZE>
class MemoryPool {
private:
    struct alignas(64) PoolNode {
        T object;
        std::atomic<PoolNode*> next;

        PoolNode() : next(nullptr) {}
    };

    std::array<PoolNode, POOL_SIZE> pool;
    std::atomic<PoolNode*> freeList;
    std::atomic<size_t> allocatedCount;

public:
    MemoryPool() : freeList(nullptr), allocatedCount(0) {

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            pool[i].next = freeList.load();
            freeList = &pool[i];
        }
    }

    T* allocate() {
        PoolNode* node = freeList.load(std::memory_order_acquire);

        while (node) {
            PoolNode* next = node->next.load(std::memory_order_relaxed);
            if (freeList.compare_exchange_weak(node, next,
                                              std::memory_order_release,
                                              std::memory_order_relaxed)) {
                allocatedCount.fetch_add(1, std::memory_order_relaxed);
                return &node->object;
            }
            node = freeList.load(std::memory_order_acquire);
        }

        return nullptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;

        PoolNode* node = reinterpret_cast<PoolNode*>(
            reinterpret_cast<uint8_t*>(ptr) - offsetof(PoolNode, object)
        );

        if (node < &pool[0] || node >= &pool[POOL_SIZE]) {
            return;
        }

        PoolNode* oldHead;
        do {
            oldHead = freeList.load(std::memory_order_relaxed);
            node->next = oldHead;
        } while (!freeList.compare_exchange_weak(oldHead, node,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));

        allocatedCount.fetch_sub(1, std::memory_order_relaxed);
    }

    size_t getAllocatedCount() const {
        return allocatedCount.load(std::memory_order_relaxed);
    }

    size_t getAvailableCount() const {
        return POOL_SIZE - getAllocatedCount();
    }

    void reset() {

        freeList = nullptr;
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            pool[i].next = freeList.load();
            freeList = &pool[i];
        }
        allocatedCount = 0;
    }
};

template<typename T, size_t POOL_SIZE>
class SimplePool {
private:
    struct PoolEntry {
        alignas(alignof(T)) uint8_t storage[sizeof(T)];
        bool inUse;

        PoolEntry() : inUse(false) {}

        T* getObject() {
            return reinterpret_cast<T*>(storage);
        }
    };

    std::array<PoolEntry, POOL_SIZE> pool;
    size_t searchStart;

public:
    SimplePool() : searchStart(0) {}

    T* allocate() {

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            size_t idx = (searchStart + i) % POOL_SIZE;
            if (!pool[idx].inUse) {
                pool[idx].inUse = true;
                searchStart = (idx + 1) % POOL_SIZE;
                return pool[idx].getObject();
            }
        }
        return nullptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;

        for (auto& entry : pool) {
            if (entry.getObject() == ptr) {
                entry.inUse = false;
                return;
            }
        }
    }

    void reset() {
        for (auto& entry : pool) {
            entry.inUse = false;
        }
        searchStart = 0;
    }
};

#endif
