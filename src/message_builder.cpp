#include "message_builder.h"
#include "endian_converter.h"
#include <cstring>

size_t MessageBuilder::buildOrder(uint8_t* buffer, size_t bufferSize, const OrderMessage& order) {
    if (bufferSize < OrderMessage::SIZE) {
        return 0;
    }
    
    if (!validateOrder(order)) {
        return 0;
    }
    
    OrderMessage networkOrder;
    
    // New field order: timestamp first, then symbol (4 bytes)
    networkOrder.timestamp = EndianConverter::htol64(order.timestamp);
    std::memcpy(networkOrder.symbol, order.symbol, 4);
    networkOrder.side = order.side;
    networkOrder.quantity = EndianConverter::htol32(order.quantity);
    networkOrder.price = EndianConverter::htol32_signed(order.price);
    
    std::memcpy(buffer, &networkOrder, OrderMessage::SIZE);
    
    return OrderMessage::SIZE;
}

bool MessageBuilder::validateOrder(const OrderMessage& order) {
    if (order.side != 'B' && order.side != 'S') {
        return false;
    }
    
    if (order.quantity == 0 || order.price <= 0) {
        return false;
    }
    
    return true;
}