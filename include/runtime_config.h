// runtime_config.h - lightweight runtime flag/config parsing
#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdio>

struct RuntimeConfig {
    // Backpressure / buffering
    uint32_t recvSoftWatermarkPct = 75;   // percent of buffer
    uint32_t recvHardWatermarkPct = 95;   // drop / pause threshold
    bool     dropOnHardWatermark   = true; // else pause ingest (skip recv loop iteration)
    uint32_t recvHardResumeDeltaPct = 5;   // hysteresis: resume when below (hard - delta)
    enum class HardAction : uint8_t { DROP=0, PAUSE=1 };
    HardAction hardAction = HardAction::DROP; // canonical replacement for dropOnHardWatermark future-proof
    // Symbol interning
    bool     enableSymbolInterning = true; // can disable at runtime for A/B
    // Delta timestamp eviction verification mode (forces deterministic base)
    bool     forceDeltaTsBaseZero  = false;
    // Logging suppression override
    bool     suppressLogs          = true; // override compile-time default

    void loadFromEnv() {
        if (const char* v = std::getenv("VWAP_RECV_SOFT_WM_PCT")) recvSoftWatermarkPct = static_cast<uint32_t>(std::strtoul(v,nullptr,10));
        if (const char* v = std::getenv("VWAP_RECV_HARD_WM_PCT")) recvHardWatermarkPct = static_cast<uint32_t>(std::strtoul(v,nullptr,10));
    if (const char* v = std::getenv("VWAP_DROP_ON_HARD"))    dropOnHardWatermark   = (std::strcmp(v,"0")!=0);
    if (const char* v = std::getenv("VWAP_HARD_RESUME_DELTA")) recvHardResumeDeltaPct = static_cast<uint32_t>(std::strtoul(v,nullptr,10));
    if (const char* v = std::getenv("VWAP_HARD_ACTION")) { if (std::strcmp(v,"PAUSE") == 0) hardAction = HardAction::PAUSE; else hardAction = HardAction::DROP; }
        if (const char* v = std::getenv("VWAP_ENABLE_INTERN"))    enableSymbolInterning = (std::strcmp(v,"0")!=0);
        if (const char* v = std::getenv("VWAP_FORCE_DELTA_BASE_ZERO")) forceDeltaTsBaseZero = (std::strcmp(v,"0")!=0);
        if (const char* v = std::getenv("VWAP_SUPPRESS_LOGS"))    suppressLogs = (std::strcmp(v,"0")!=0);
        clamp();
    }
    void clamp() {
        if (recvSoftWatermarkPct < 10) recvSoftWatermarkPct = 10;
        if (recvSoftWatermarkPct > 90) recvSoftWatermarkPct = 90;
        if (recvHardWatermarkPct <= recvSoftWatermarkPct) recvHardWatermarkPct = recvSoftWatermarkPct + 5;
        if (recvHardWatermarkPct > 99) recvHardWatermarkPct = 99;
    if (recvHardResumeDeltaPct < 1) recvHardResumeDeltaPct = 1;
    if (recvHardResumeDeltaPct > 20) recvHardResumeDeltaPct = 20; // prevent pathological hysteresis
    }
};

RuntimeConfig& runtimeConfig();

#endif // RUNTIME_CONFIG_H
