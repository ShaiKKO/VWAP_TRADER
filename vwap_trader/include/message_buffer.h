#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <cstddef>
#include <cstdint>
#include "message.h"

class MessageBuffer final {
public:
    enum class ExtractResult {
        SUCCESS,
        NEED_MORE_DATA,
        INVALID_MESSAGE
    };
    
private:
    static constexpr size_t BUFFER_SIZE = 65536;
    uint8_t buffer[BUFFER_SIZE];
    size_t writePos;
    size_t readPos;
    
public:
    MessageBuffer() noexcept;
    bool append(const uint8_t* data, size_t len) noexcept;
    ExtractResult extractMessage(MessageHeader& header, uint8_t* messageBuffer) noexcept;
    size_t availableBytes() const noexcept;
    size_t availableSpace() const noexcept;
    void clear() noexcept;
    size_t resync() noexcept;
    
private:
    void compact() noexcept;
};

#endif // MESSAGE_BUFFER_H