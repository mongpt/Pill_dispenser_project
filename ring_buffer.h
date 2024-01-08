#ifndef UART_IRQ_RING_BUFFER_H
#define UART_IRQ_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct  {
    int head;
    int tail;
    int size;
    uint8_t *buffer;
} ring_buffer;

void rb_init(ring_buffer *rb, uint8_t *buffer, int size);
bool rb_empty(ring_buffer *rb);
bool rb_full(ring_buffer *rb);
bool rb_put(ring_buffer *rb, uint8_t data);
uint8_t rb_get(ring_buffer *rb);

void rb_alloc(ring_buffer *rb, int size);
void rb_free(ring_buffer *rb);

#endif //UART_IRQ_RING_BUFFER_H
