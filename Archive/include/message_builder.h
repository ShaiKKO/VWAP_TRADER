#ifndef MESSAGE_BUILDER_H
#define MESSAGE_BUILDER_H

#include <cstddef>
#include <cstdint>
#include "message.h"
#include "message_serializer.h"

// Backwards-compatible adapter for older tests expecting MessageBuilder.
class MessageBuilder {
public:
    static inline size_t buildOrder(uint8_t* buf, size_t cap, const OrderMessage& order) noexcept {
        return MessageSerializer::serializeOrder(buf, cap, order);
    }
    static inline bool validateOrder(const OrderMessage& o) noexcept {
        if ((o.side!='B' && o.side!='S') || o.quantity==0 || o.price<=0) return false; return true;
    }
};

#endif // MESSAGE_BUILDER_H
