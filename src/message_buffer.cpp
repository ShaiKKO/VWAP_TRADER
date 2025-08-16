#include "message_buffer.h"
#include "message_parser.h"
#include "wire_format.h"
#include <cstring>
#include <algorithm>
#include "metrics.h"

MessageBuffer::MessageBuffer() noexcept : head(0), tail(0), used(0) {}

bool MessageBuffer::append(const uint8_t* data, size_t len) noexcept {
    if (!len) return true;
    if (len > BUFFER_SIZE - used) return false;
    size_t first = std::min(len, BUFFER_SIZE - tail);
    std::memcpy(buffer + tail, data, first);
    size_t second = len - first;
    if (second) std::memcpy(buffer, data + first, second);
    tail = (tail + len) & (BUFFER_SIZE - 1);
    used += len;
    return true;
}

MessageBuffer::ExtractResult MessageBuffer::peekMessage(MessageHeader& header, const uint8_t*& bodyPtr, size_t& contiguousBody) const noexcept {
    if (used < WireFormat::HEADER_SIZE) return ExtractResult::NEED_MORE_DATA;
    uint8_t len = buffer[head];
    uint8_t type = buffer[(head + 1) & (BUFFER_SIZE - 1)];
    header.length = len;
    header.type = type;
    if (!MessageParser::validateHeader(header)) return ExtractResult::INVALID_HEADER;
    size_t total = WireFormat::HEADER_SIZE + header.length;
    if (used < total) return ExtractResult::NEED_MORE_DATA;
    size_t bodyStart = (head + WireFormat::HEADER_SIZE) & (BUFFER_SIZE - 1);
    bodyPtr = buffer + bodyStart;
    contiguousBody = std::min(static_cast<size_t>(header.length), BUFFER_SIZE - bodyStart);
    return ExtractResult::SUCCESS;
}

void MessageBuffer::consume(const MessageHeader& header) noexcept {
    size_t total = WireFormat::HEADER_SIZE + header.length;
    head = (head + total) & (BUFFER_SIZE - 1);
    used -= total;
}

MessageBuffer::ExtractResult MessageBuffer::extractMessage(MessageHeader& header, uint8_t* messageBuffer) noexcept {
    const uint8_t* body; size_t contiguous; auto r = peekMessage(header, body, contiguous);
    if (r != ExtractResult::SUCCESS) return r;
    if (contiguous == header.length) {
        std::memcpy(messageBuffer, body, header.length);
    } else {
        size_t first = contiguous;
        std::memcpy(messageBuffer, body, first);
        std::memcpy(messageBuffer + first, buffer, header.length - first);
    }
    consume(header);
    return ExtractResult::SUCCESS;
}

size_t MessageBuffer::availableBytes() const noexcept { return used; }

size_t MessageBuffer::availableSpace() const noexcept { return BUFFER_SIZE - used; }

void MessageBuffer::clear() noexcept { head = tail = used = 0; }

size_t MessageBuffer::resync() noexcept {
    size_t available = used;
    if (available < WireFormat::HEADER_SIZE + 1) return available;

    const size_t MAX_SCAN = 256;
    size_t limit = (available < MAX_SCAN) ? available : MAX_SCAN;
    for (size_t i = 1; i + 1 < limit; ++i) {
        uint8_t length = buffer[(head + i) & (BUFFER_SIZE - 1)];
        uint8_t type   = buffer[(head + i + 1) & (BUFFER_SIZE - 1)];
        bool plausible = false;
        if (type == MessageHeader::QUOTE_TYPE && length == WireFormat::QUOTE_SIZE) plausible = true;
        else if (type == MessageHeader::TRADE_TYPE && length == WireFormat::TRADE_SIZE) plausible = true;
        if (plausible) return i;
    }

    extern SystemMetrics g_systemMetrics;
    if (limit >= 8) {
        g_systemMetrics.perf.resyncEvents.fetch_add(1, std::memory_order_relaxed);
    }
    head = (head + limit) & (BUFFER_SIZE - 1);
    used -= std::min(used, limit);
    return limit;
}
