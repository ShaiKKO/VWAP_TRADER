// features.h - centralized compile-time feature toggles (C++11/14 only)
#ifndef FEATURES_H
#define FEATURES_H

// All flags are constexpr so the compiler can dead-strip unused branches.
// Adjust here rather than scattering #ifdefs.
struct Features {
    static constexpr bool ENABLE_SYMBOL_INTERNING      = true;  // enabled: 64-bit symbol compares
    static constexpr bool ENABLE_BRANCH_FUNNELING      = true;  // parser dispatch table
    static constexpr bool ENABLE_ZERO_COPY_VIEW        = true;  // expose WireSpan view
    static constexpr bool ENABLE_BATCH_METRICS         = true;  // hot-path metrics batching
    static constexpr bool ENABLE_TIME_ABSTRACTION      = true;  // ITimeSource indirection
    static constexpr bool ENABLE_SLIDING_WINDOW_MODE   = true;  // (future) alternate VWAP modes
    static constexpr bool ENABLE_PREFIX_COMPRESSION    = false; // (future) timestamp delta compression
    static constexpr bool ENABLE_VWAP_DELTA_TS         = false; // compress timestamps to base + micro-delta
    static constexpr bool ENABLE_WRITEV_BATCHING       = true;  // coalesce order sends via writev
    static constexpr bool ENABLE_BENCHMARK_SUPPRESS_LOG= true;  // suppress std::cout spam in perf benchmark
    static constexpr bool ENABLE_LATENCY_HISTOGRAM     = true;  // collect end-to-end latency histogram in benchmark
};

#endif // FEATURES_H
