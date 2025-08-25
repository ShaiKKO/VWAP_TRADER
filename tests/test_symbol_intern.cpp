#include "symbol_intern.h"
#include <iostream>
#include <cassert>
#include <cstring>

extern "C" void runSymbolInternTests(){
    auto& pool = symbolPool();
    const char symA[8] = {'I','B','M',0,0,0,0,0};
    const char symB[8] = {'I','B','M',0,0,0,0,0};
    uint32_t idA = pool.intern(symA);
    uint32_t idB = pool.intern(symB); // should return same id
    if (idA != idB) { std::cerr << "Interning mismatch (ids differ)\n"; return; }
    uint64_t packedA = SymbolInternPool::pack8(pool.resolve(idA));
    uint64_t packedB = SymbolInternPool::pack8(pool.resolve(idB));
    assert(packedA == packedB);
    std::cout << "Symbol interning basic test passed (id="<<idA<<")\n";
}
