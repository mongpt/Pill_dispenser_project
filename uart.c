#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "ring_buffer.h"

#include "uart.h"
#if 0
typedef struct {
    ring_buffer tx;
    ring_buffer rx;
    uart_inst_t *uart;
    int irqn;
    irq_handler_t handler;
} uart_t;
#endif
void uart_irq_rx(uart_t *u);
void uart_irq_tx(uart_t *u);
void uart0_handler(void);
void uart1_handler(void);
#if 0
static uart_t *uart_get_handle(int uart_nr);
#endif
static uart_t u0 = { .uart = uart0, .irqn = UART0_IRQ, .handler = uart0_handler };
static uart_t u1 = { .uart = uart1, .irqn = UART1_IRQ, .handler = uart1_handler };
#if 0
static uart_t *uart_get_handle(int uart_nr) {
#else
uart_t *uart_get_handle(int uart_nr) {
#endif
    return uart_nr ? &u1 : &u0;
}


void uart_setup(int uart_nr, int tx_pin, int rx_pin, int speed)
{
    uart_t *uart = uart_get_handle(uart_nr);

    // ensure that we don't get any interrupts from the uart during configuration
    irq_set_enabled(uart->irqn, false);

    // allocate space for ring buffers
    rb_alloc(&uart->rx, 256);
    rb_alloc(&uart->tx, 256);

    // Set up our UART with the required speed.
    uart_init(uart->uart, speed);

    // Set the TX and RX pins by using the function select on the GPIO
    // See datasheet for more information on function select
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    irq_set_exclusive_handler(uart->irqn, uart->handler);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(uart->uart, true, false);
    // enable UART0 interrupts on NVIC
    irq_set_enabled(uart->irqn, true);
}

int uart_read(int uart_nr, uint8_t *buffer, int size)
{
    int count = 0;
    uart_t *u = uart_get_handle(uart_nr);
    while(count < size && !rb_empty(&u->rx)) {
        *buffer++ = rb_get(&u->rx);
        ++count;
    }
    return count;
}

int uart_write(int uart_nr, const uint8_t *buffer, int size)
{
    int count = 0;
    uart_t *u = uart_get_handle(uart_nr);
    // write data to ring buffer
    while(count < size && !rb_full(&u->tx)) {
        rb_put(&u->tx, *buffer++);
        ++count;
    }
    // disable interrupts on NVIC while managing transmit interrupts
    irq_set_enabled(u->irqn, false);

    // if transmit interrupt is not enabled we need to enable it and give fifo an initial filling
    if(!(uart_get_hw(u->uart)->imsc & (1 << UART_UARTIMSC_TXIM_LSB))) {
        // enable transmit interrupt
        uart_set_irq_enables(u->uart, true, true);
        // fifo requires initial filling
        uart_irq_tx(u);
    }

    // enable interrupts on NVIC
    irq_set_enabled(u->irqn, true);

    return count;
}

int uart_send(int uart_nr, const char *str)
{
    return uart_write(uart_nr, (const uint8_t *)str, strlen(str));
}


void uart_irq_rx(uart_t *u)
{
    while(uart_is_readable(u->uart)) {
        uint8_t c = uart_getc(u->uart);
        // ignoring return value for now
        rb_put(&u->rx, c);
    }
}

void uart_irq_tx(uart_t *u)
{
    while(!rb_empty(&u->tx) && uart_is_writable(u->uart)) {
        uart_get_hw(u->uart)->dr = rb_get((&u->tx));
    }

    if (rb_empty(&u->tx)) {
        // disable tx interrupt if transmit buffer is empty
        uart_set_irq_enables(u->uart, true, false);
    }
}

void uart0_handler(void)
{
    uart_irq_rx(&u0);
    uart_irq_tx(&u0);
}

void uart1_handler(void)
{
    uart_irq_rx(&u1);
    uart_irq_tx(&u1);
}
