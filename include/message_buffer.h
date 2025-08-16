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
        INVALID_HEADER,
        INVALID_LENGTH,
        UNKNOWN_TYPE,
        PARTIAL_BODY
    };

private:
    static constexpr size_t BUFFER_SIZE = 65536;
    uint8_t buffer[BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t used;

public:
    MessageBuffer() noexcept;
    bool append(const uint8_t* data, size_t len) noexcept;

    ExtractResult peekMessage(MessageHeader& header, const uint8_t*& bodyPtr, size_t& contiguousBody) const noexcept;
    void consume(const MessageHeader& header) noexcept;
    ExtractResult extractMessage(MessageHeader& header, uint8_t* messageBuffer) noexcept;
    size_t availableBytes() const noexcept;
    size_t availableSpace() const noexcept;
    void clear() noexcept;
    size_t resync() noexcept;
    const uint8_t* dataPtr() const noexcept { return buffer; }
    size_t headIndex() const noexcept { return head; }
};

#endif
