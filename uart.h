#ifndef UART_IRQ_UART_H
#define UART_IRQ_UART_H

#include "ring_buffer.h"

void uart_setup(int uart_nr, int tx_pin, int rx_pin, int speed);
int uart_read(int uart_nr, uint8_t *buffer, int size);
int uart_write(int uart_nr, const uint8_t *buffer, int size);
int uart_send(int uart_nr, const char *str);

typedef struct {
    ring_buffer tx;
    ring_buffer rx;
    uart_inst_t *uart;
    int irqn;
    irq_handler_t handler;
} uart_t;
uart_t *uart_get_handle(int uart_nr);

#endif
