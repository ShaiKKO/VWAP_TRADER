#include "message_buffer.h"
#include "message_parser.h"
#include <cstring>
#include <algorithm>

MessageBuffer::MessageBuffer() noexcept : writePos(0), readPos(0) {
}

bool MessageBuffer::append(const uint8_t* data, size_t len) noexcept {
    if (len == 0) {
        return true;
    }
    
    if (writePos + len > BUFFER_SIZE) {
        compact();
        
        if (writePos + len > BUFFER_SIZE) {
            return false;
        }
    }
    
    std::memcpy(buffer + writePos, data, len);
    writePos += len;
    
    return true;
}

MessageBuffer::ExtractResult MessageBuffer::extractMessage(MessageHeader& header, uint8_t* messageBuffer) noexcept {
    size_t available = writePos - readPos;
    
    if (available < MessageHeader::SIZE) {
        return ExtractResult::NEED_MORE_DATA;
    }
    
    if (!MessageParser::parseHeader(buffer + readPos, available, header)) {
        return ExtractResult::INVALID_MESSAGE;
    }
    
    if (!MessageParser::validateHeader(header)) {
        readPos += resync();
        return ExtractResult::INVALID_MESSAGE;
    }
    
    size_t totalSize = MessageHeader::SIZE + header.length;
    if (available < totalSize) {
        return ExtractResult::NEED_MORE_DATA;
    }
    
    std::memcpy(messageBuffer, buffer + readPos + MessageHeader::SIZE, header.length);
    
    readPos += totalSize;
    
    if (readPos == writePos) {
        readPos = 0;
        writePos = 0;
    }
    
    return ExtractResult::SUCCESS;
}

size_t MessageBuffer::availableBytes() const noexcept {
    return writePos - readPos;
}

size_t MessageBuffer::availableSpace() const noexcept {
    return BUFFER_SIZE - writePos;
}

void MessageBuffer::clear() noexcept {
    readPos = 0;
    writePos = 0;
}

void MessageBuffer::compact() noexcept {
    if (readPos == 0) {
        return;
    }
    
    size_t remaining = writePos - readPos;
    if (remaining > 0) {
        std::memmove(buffer, buffer + readPos, remaining);
    }
    
    writePos = remaining;
    readPos = 0;
}

size_t MessageBuffer::resync() noexcept {
    size_t available = writePos - readPos;
    
    for (size_t i = 1; i < available - 1; i++) {
        uint8_t type = buffer[readPos + i + 1];
        
        if (type == MessageHeader::QUOTE_TYPE || type == MessageHeader::TRADE_TYPE) {
            uint8_t length = buffer[readPos + i];
            
            if ((type == MessageHeader::QUOTE_TYPE && length == QuoteMessage::SIZE) ||
                (type == MessageHeader::TRADE_TYPE && length == TradeMessage::SIZE)) {
                return i;
            }
        }
    }
    
    return available;
}