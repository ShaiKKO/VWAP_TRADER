#include "market_data_client.h"
#include "message_parser.h"
#include <iostream>

MarketDataClient::MarketDataClient(const std::string& host, uint16_t port)
    : TcpClient(host, port) {
}

void MarketDataClient::setMessageCallback(
    std::function<void(const MessageHeader&, const void*)> cb) {
    messageCallback = cb;
}

bool MarketDataClient::processIncomingData() {
    uint8_t tempBuffer[4096];
    ssize_t bytesRead = receive(tempBuffer, sizeof(tempBuffer));
    
    if (bytesRead <= 0) {
        return bytesRead == 0 ? false : (errno == EAGAIN);
    }
    
    if (!receiveBuffer.append(tempBuffer, bytesRead)) {
        std::cerr << "Receive buffer overflow" << std::endl;
        receiveBuffer.clear();
        return false;
    }
    
    MessageHeader header;
    uint8_t messageBody[64];
    
    while (receiveBuffer.extractMessage(header, messageBody) == 
           MessageBuffer::ExtractResult::SUCCESS) {
        
        if (!MessageParser::validateHeader(header)) {
            continue;
        }
        
        messagesReceived++;
        
        switch (header.type) {
            case MessageHeader::QUOTE_TYPE: {
                QuoteMessage quote;
                if (MessageParser::parseQuote(messageBody, header.length, quote)) {
                    if (messageCallback) {
                        messageCallback(header, &quote);
                    }
                }
                break;
            }
            
            case MessageHeader::TRADE_TYPE: {
                TradeMessage trade;
                if (MessageParser::parseTrade(messageBody, header.length, trade)) {
                    if (messageCallback) {
                        messageCallback(header, &trade);
                    }
                }
                break;
            }
        }
    }
    
    return true;
}