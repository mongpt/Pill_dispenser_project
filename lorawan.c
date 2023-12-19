#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "uart.h"
#include "hardware/pwm.h"
#include "lorawan.h"

#if 0
#define UART_NR 0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define BAUD_RATE 9600
#define MAX_COUNT 5

#define STRLEN 80

static const int uart_nr = UART_NR;

bool generic_lora(const char* command, const uint sleep_time, char* str) {
    int pos = 0;
    uart_send(uart_nr, command);
    sleep_ms(sleep_time);
    pos = uart_read(UART_NR, (uint8_t *) str, STRLEN - 1);
    if (pos > 0) {
        str[pos] = '\0';
        return true;
    }
    return false;
}
