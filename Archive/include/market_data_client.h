#ifndef MARKET_DATA_CLIENT_H
#define MARKET_DATA_CLIENT_H

#include "tcp_client.h"
#include "message_buffer.h"
#include "message.h"
#include <functional>

class MarketDataClient : public TcpClient {
private:
    MessageBuffer receiveBuffer;
    std::function<void(const MessageHeader&, const void*)> messageCallback;
    struct BackpressureState {
        bool inHardPause = false; // per-connection hard watermark pause flag
    } backpressure;

public:
    MarketDataClient(const std::string& host, uint16_t port);

    void setMessageCallback(std::function<void(const MessageHeader&, const void*)> cb);
    bool processIncomingData();
};

#endif
