#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include <cstddef>
#include <cstdint>
#include "message.h"

class MessageBuilder {
public:
    static size_t buildOrder(uint8_t* buffer, size_t bufferSize, const OrderMessage& order);
    static bool validateOrder(const OrderMessage& order);
};

#endif // MESSAGE_BUILDER_H