#ifndef MESSAGE_TYPES_H
#define MESSAGE_TYPES_H

#include "message_queue_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
bool message_queue_push(MessageQueue_T* queue, const Message_T* message, uint32_t timeout_ms);
bool message_queue_pop(MessageQueue_T* queue, Message_T* message, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_TYPES_H
