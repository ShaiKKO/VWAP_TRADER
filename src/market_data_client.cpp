#include "market_data_client.h"
#include "message_parser.h"
#include <iostream>
#include "metrics.h"
#include "wire_format.h"

MarketDataClient::MarketDataClient(const std::string& host, uint16_t port)
    : TcpClient(host, port) {
}

void MarketDataClient::setMessageCallback(
    std::function<void(const MessageHeader&, const void*)> cb) {
    messageCallback = cb;
}

bool MarketDataClient::processIncomingData() {

    uint64_t localBytes = 0;
    for (int iter=0; iter<4; ++iter) {
        uint8_t tempBuffer[4096];
        ssize_t bytesRead = receive(tempBuffer, sizeof(tempBuffer));
        if (bytesRead <= 0) {
            if (bytesRead == 0) return false;
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            return false;
        }
        localBytes += static_cast<uint64_t>(bytesRead);
        if (!receiveBuffer.append(tempBuffer, static_cast<size_t>(bytesRead))) {
            g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
            receiveBuffer.clear();
            break;
        }
        if (bytesRead < static_cast<ssize_t>(sizeof(tempBuffer))) break;
    }

    uint64_t localMsgs = 0;
    uint64_t localQuotes = 0;
    uint64_t localTrades = 0;
    while (true) {
        MessageHeader header; const uint8_t* bodyPtr; size_t contiguous;
        auto pr = receiveBuffer.peekMessage(header, bodyPtr, contiguous);
        if (pr != MessageBuffer::ExtractResult::SUCCESS) {
            if (pr != MessageBuffer::ExtractResult::NEED_MORE_DATA) {

                if (pr == MessageBuffer::ExtractResult::INVALID_HEADER) {
                    g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
                    receiveBuffer.resync();
                }
            }
            break;
        }

    messagesReceived++;
    ++localMsgs;
        bool ok = false;
        if (header.type == MessageHeader::QUOTE_TYPE) {
            QuoteMessage quote;
            uint8_t temp[WireFormat::QUOTE_SIZE];
            if (contiguous == header.length) {
                ok = MessageParser::parseQuote(bodyPtr, header.length, quote) && MessageParser::validateQuote(quote);
            } else {
                std::memcpy(temp, bodyPtr, contiguous);
                std::memcpy(temp + contiguous, receiveBuffer.dataPtr(), header.length - contiguous);
                ok = MessageParser::parseQuote(temp, header.length, quote) && MessageParser::validateQuote(quote);
            }
            if (ok) {
        ++localQuotes;
                if (messageCallback) messageCallback(header, &quote);
            } else {
                g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
            }
        } else if (header.type == MessageHeader::TRADE_TYPE) {
            TradeMessage trade;
            uint8_t temp[WireFormat::TRADE_SIZE];
            if (contiguous == header.length) {
                ok = MessageParser::parseTrade(bodyPtr, header.length, trade) && MessageParser::validateTrade(trade);
            } else {
                std::memcpy(temp, bodyPtr, contiguous);
                std::memcpy(temp + contiguous, receiveBuffer.dataPtr(), header.length - contiguous);
                ok = MessageParser::parseTrade(temp, header.length, trade) && MessageParser::validateTrade(trade);
            }
            if (ok) {
        ++localTrades;
                if (messageCallback) messageCallback(header, &trade);
            } else {
                g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
        }
        receiveBuffer.consume(header);

#if defined(__GNUC__)
        if (receiveBuffer.availableBytes() >= WireFormat::HEADER_SIZE) {
            size_t hi = receiveBuffer.headIndex();
            size_t prefetchOffset = (hi + WireFormat::HEADER_SIZE) & (65536 - 1);
            __builtin_prefetch(receiveBuffer.dataPtr() + prefetchOffset, 0, 1);
        }
#endif
    }
    if (localBytes) g_systemMetrics.hot.bytesReceived.fetch_add(localBytes, std::memory_order_relaxed);
    if (localMsgs) {
    g_systemMetrics.hot.messagesReceived.fetch_add(localMsgs, std::memory_order_relaxed);
    if (localQuotes) g_systemMetrics.hot.quotesProcessed.fetch_add(localQuotes, std::memory_order_relaxed);
    if (localTrades) g_systemMetrics.hot.tradesProcessed.fetch_add(localTrades, std::memory_order_relaxed);
    }
    return true;
}
