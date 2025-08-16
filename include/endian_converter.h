#ifndef ENDIAN_CONVERTER_H
#define ENDIAN_CONVERTER_H

#include <cstdint>
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define IS_LITTLE_ENDIAN 1
#elif defined(__LITTLE_ENDIAN__)
    #define IS_LITTLE_ENDIAN 1
#elif defined(__x86_64__) || defined(__i386__) || defined(__aarch64__)
    #define IS_LITTLE_ENDIAN 1
#else
    #define IS_LITTLE_ENDIAN 0
#endif

static_assert(IS_LITTLE_ENDIAN, "This code requires a little-endian platform");

class EndianConverter {
public:
    static inline constexpr uint16_t ltoh16(uint16_t value) noexcept {
        #if IS_LITTLE_ENDIAN
            return value;
        #else
            return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
        #endif
    }

    static inline constexpr uint32_t ltoh32(uint32_t value) noexcept {
        #if IS_LITTLE_ENDIAN
            return value;
        #else
            return ((value & 0xFF000000) >> 24) |
                   ((value & 0x00FF0000) >> 8) |
                   ((value & 0x0000FF00) << 8) |
                   ((value & 0x000000FF) << 24);
        #endif
    }

    static inline constexpr uint64_t ltoh64(uint64_t value) noexcept {
        #if IS_LITTLE_ENDIAN
            return value;
        #else
            return ((value & 0xFF00000000000000ULL) >> 56) |
                   ((value & 0x00FF000000000000ULL) >> 40) |
                   ((value & 0x0000FF0000000000ULL) >> 24) |
                   ((value & 0x000000FF00000000ULL) >> 8) |
                   ((value & 0x00000000FF000000ULL) << 8) |
                   ((value & 0x0000000000FF0000ULL) << 24) |
                   ((value & 0x000000000000FF00ULL) << 40) |
                   ((value & 0x00000000000000FFULL) << 56);
        #endif
    }

    static inline constexpr int32_t ltoh32_signed(int32_t value) noexcept {
        return static_cast<int32_t>(ltoh32(static_cast<uint32_t>(value)));
    }

    static inline constexpr uint16_t htol16(uint16_t value) noexcept {
        return ltoh16(value);
    }

    static inline constexpr uint32_t htol32(uint32_t value) noexcept {
        return ltoh32(value);
    }

    static inline constexpr uint64_t htol64(uint64_t value) noexcept {
        return ltoh64(value);
    }

    static inline constexpr int32_t htol32_signed(int32_t value) noexcept {
        return static_cast<int32_t>(htol32(static_cast<uint32_t>(value)));
    }
};

#endif
