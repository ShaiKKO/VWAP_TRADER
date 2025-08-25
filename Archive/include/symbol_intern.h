#ifndef SYMBOL_INTERN_H
#define SYMBOL_INTERN_H

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "features.h"

// Simple fixed-capacity symbol interning for 8-char symbols -> 32-bit IDs.
// Not thread-safe for concurrent mutation; intended to be populated at startup.
class SymbolInternPool {
public:
    struct Entry { char symbol[8]; uint32_t id; uint64_t packed; };
private:
    static const size_t MAX_SYMBOLS = 256; // adjust if needed
    Entry entries[MAX_SYMBOLS];
    uint32_t count;
public:
    SymbolInternPool() : count(0) { for(size_t i=0;i<MAX_SYMBOLS;++i){ entries[i].id=UINT32_MAX; entries[i].packed=0; std::memset(entries[i].symbol,0,8);} }
    uint32_t intern(const char* sym8) {
        // Linear scan (small n). Could upgrade to open-addressed hash if needed.
        uint64_t key; std::memcpy(&key, sym8, 8);
        for(uint32_t i=0;i<count;++i){ if(entries[i].packed==key && std::memcmp(entries[i].symbol,sym8,8)==0) return entries[i].id; }
        if(count>=MAX_SYMBOLS) return UINT32_MAX; // pool full
        std::memcpy(entries[count].symbol, sym8, 8);
        entries[count].id = count;
        entries[count].packed = key;
        return count++;
    }
    const char* resolve(uint32_t id) const { return (id<count)? entries[id].symbol : (const char*)0; }
    static inline uint64_t pack8(const char* sym) noexcept { uint64_t v; std::memcpy(&v, sym, 8); return v; }
};

// Global accessor (simple singleton)
SymbolInternPool& symbolPool();

#endif // SYMBOL_INTERN_H
