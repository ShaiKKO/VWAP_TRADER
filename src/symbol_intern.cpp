#include "symbol_intern.h"

SymbolInternPool& symbolPool() {
    static SymbolInternPool pool;
    return pool;
}
