#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

struct Config {
    std::string symbol;
    char side;
    uint32_t maxOrderSize;
    uint32_t vwapWindowSeconds;
    std::string marketDataHost;
    uint16_t marketDataPort;
    std::string orderHost;
    uint16_t orderPort;
    
    // Default constructor
    Config() noexcept
        : side('B'), maxOrderSize(0), vwapWindowSeconds(0),
          marketDataPort(0), orderPort(0) {}
};

#endif // CONFIG_H