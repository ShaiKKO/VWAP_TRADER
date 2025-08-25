#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>

namespace Const {
    constexpr uint64_t NANOS_PER_SEC = 1'000'000'000ULL;
    constexpr uint64_t MICROS_PER_SEC = 1'000'000ULL;
    constexpr uint32_t DEFAULT_BACKOFF_MS = 1000;
    constexpr uint32_t MAX_BACKOFF_MS = 60'000;
    constexpr uint32_t MAX_QUEUE_DEPTH = 1000;
    constexpr uint32_t MAX_DECISION_HISTORY = 1000;
}

inline constexpr uint64_t nanosPerSec() noexcept { return Const::NANOS_PER_SEC; }
inline constexpr uint32_t priceTicksPerDollar() noexcept { return 100; }

#endif
