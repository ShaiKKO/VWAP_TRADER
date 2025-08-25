#ifndef TIME_SOURCE_H
#define TIME_SOURCE_H

#include <cstdint>
#include <chrono>
#include "features.h"

// Abstract time source to enable deterministic replay and optional rdtsc calibration later.
class ITimeSource {
public:
    virtual ~ITimeSource() {}
    virtual uint64_t nowNanos() const = 0; // monotonic nanoseconds
};

// Steady clock implementation (default)
class SteadyTimeSource : public ITimeSource {
public:
    virtual uint64_t nowNanos() const {
        using clock = std::chrono::steady_clock;
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count());
    }
};

// Lightweight singleton accessor (no thread-safety required for initialization in this context).
class Time {
public:
    static inline const ITimeSource& instance() {
#if defined(__clang__) || defined(__GNUC__)
        // function-local static for thread-safe init (since C++11)
#endif
        static SteadyTimeSource ts; // can be swapped to a mock in tests by providing setCustom()
        return custom() ? *custom() : ts;
    }

    static inline void setCustom(ITimeSource* src) { custom() = src; }
    static inline void clearCustom() { custom() = 0; }
private:
    static inline ITimeSource*& custom() { static ITimeSource* ptr = 0; return ptr; }
};

#endif // TIME_SOURCE_H
