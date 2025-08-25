#include "message_buffer.h"
#include "message_parser.h"
#include "wire_format.h"
#include <random>
#include <cassert>
#include <iostream>

int main() {
    MessageBuffer buf;
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<uint32_t> dist(0, 255);

    const int ITER = 1000;
    int validInjected = 0;
    for (int i = 0; i < ITER; ++i) {
        uint8_t chunk[32];
        for (size_t j = 0; j < sizeof(chunk); ++j) {
            chunk[j] = static_cast<uint8_t>(dist(rng));
        }
        if (i % 50 == 0) {
            uint8_t len = (i % 100 == 0) ? WireFormat::TRADE_SIZE : WireFormat::QUOTE_SIZE;
            uint8_t typ = (len == WireFormat::QUOTE_SIZE) ? MessageHeader::QUOTE_TYPE : MessageHeader::TRADE_TYPE;
            chunk[0] = len; // length
            chunk[1] = typ; // type
            size_t bodyBytes = len;
            for (size_t k = 2; k < bodyBytes + 2 && k < sizeof(chunk); ++k) {
                chunk[k] = 0; // zero body if it fits
            }
            validInjected++;
        }
        buf.append(chunk, sizeof(chunk));
        MessageHeader hdr; uint8_t body[64];
        auto r = buf.extractMessage(hdr, body);
        if (r == MessageBuffer::ExtractResult::SUCCESS) {
            assert(hdr.type == MessageHeader::QUOTE_TYPE || hdr.type == MessageHeader::TRADE_TYPE);
        } else if (r == MessageBuffer::ExtractResult::INVALID_MESSAGE) {
        }
        if (buf.availableBytes() > 2048) {
            buf.clear();
        }
    }
    std::cout << "Fuzz stub executed; injected valid headers: " << validInjected << "\n";
    return 0;
}
