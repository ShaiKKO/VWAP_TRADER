#include "market_data_client.h"
#include "message_parser.h"
#include <iostream>
#include "metrics.h"
#include "wire_format.h"
#include "runtime_config.h"

MarketDataClient::MarketDataClient(const std::string& host, uint16_t port)
    : TcpClient(host, port) {
}

void MarketDataClient::setMessageCallback(
    std::function<void(const MessageHeader&, const void*)> cb) {
    messageCallback = cb;
}

bool MarketDataClient::processIncomingData() {

    uint64_t localBytes = 0;
    auto& cfg = runtimeConfig();
    const uint32_t softPct = cfg.recvSoftWatermarkPct;
    const uint32_t hardPct = cfg.recvHardWatermarkPct;
    const uint32_t resumePct = (hardPct > cfg.recvHardResumeDeltaPct) ? (hardPct - cfg.recvHardResumeDeltaPct) : hardPct;
    bool& inHardPause = backpressure.inHardPause; // per-instance state

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
        // Backpressure check (dynamic thresholds + hysteresis)
        size_t used = receiveBuffer.availableBytes();
        constexpr size_t cap = 65536; // buffer size constant
        uint32_t usedPct = static_cast<uint32_t>((used * 100) / cap);

        if (usedPct >= hardPct) {
            // Hard watermark hit
            if (!inHardPause) g_systemMetrics.cold.hardWatermarkEvents.fetch_add(1, std::memory_order_relaxed);
            if (cfg.hardAction == RuntimeConfig::HardAction::DROP) {
                g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
                receiveBuffer.clear();
                inHardPause = false; // drop resets state
            } else { // PAUSE
                inHardPause = true;
                break; // stop reading more
            }
        } else if (inHardPause && usedPct <= resumePct) {
            // Exit pause when below resume threshold
            inHardPause = false;
        } else if (usedPct >= softPct && receiveBuffer.getWatermarkState() == MessageBuffer::SoftWatermarkState::NORMAL) {
            // soft watermark events already tracked internally; nothing to do beyond maybe future signaling
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
            if (pr == MessageBuffer::ExtractResult::INVALID_HEADER) {
                g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
                receiveBuffer.resync();
            }
            break;
        }
        ++localMsgs;
        // Handler table
        bool ok = false; bool isQuote = (header.type == MessageHeader::QUOTE_TYPE);
        if (isQuote || header.type == MessageHeader::TRADE_TYPE) {
            if (contiguous != header.length) {
                // Stitch into stack temp for wrap
                uint8_t temp[256]; // sufficient for both sizes (<256)
                std::memcpy(temp, bodyPtr, contiguous);
                std::memcpy(temp + contiguous, receiveBuffer.dataPtr(), header.length - contiguous);
                if (isQuote) { QuoteMessage q; MessageParser::parseQuoteFast(temp, q); ok = MessageParser::validateQuote(q); if (ok){ ++localQuotes; if (messageCallback) messageCallback(header, &q);} }
                else { TradeMessage t; MessageParser::parseTradeFast(temp, t); ok = MessageParser::validateTrade(t); if (ok){ ++localTrades; if (messageCallback) messageCallback(header, &t);} }
            } else {
                if (isQuote) { QuoteMessage q; MessageParser::parseQuoteFast(bodyPtr, q); ok = MessageParser::validateQuote(q); if (ok){ ++localQuotes; if (messageCallback) messageCallback(header, &q);} }
                else { TradeMessage t; MessageParser::parseTradeFast(bodyPtr, t); ok = MessageParser::validateTrade(t); if (ok){ ++localTrades; if (messageCallback) messageCallback(header, &t);} }
            }
            if (!ok) g_systemMetrics.cold.messagesDropped.fetch_add(1, std::memory_order_relaxed);
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
    if (localBytes) g_metricsView.addBytesReceived(localBytes);
    if (localMsgs) {
        g_metricsView.incMessagesReceived();
        if (localQuotes) for (uint64_t i=0;i<localQuotes;++i) g_metricsView.incQuotesProcessed();
        if (localTrades) for (uint64_t i=0;i<localTrades;++i) g_metricsView.incTradesProcessed();
    }
    return true;
}
