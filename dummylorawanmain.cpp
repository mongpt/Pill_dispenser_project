#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "uart.h"
#include "lorawan.h"

#define SW_0 9
#define BUTTON_PERIOD 10
#define BUTTON_FILTER 5
#define RELEASED 1

void buttonInit();
bool repeatingTimerCallback(struct repeating_timer *t);

static volatile bool buttonEvent = false;
static volatile bool lora_send = false;
static volatile bool lora_connected = false;

int main(void) {

    stdio_init_all();
    buttonInit();
    while (!lora_connected) {
        lora_connected = loraInit();
    }

    char retval_str[STRLEN];
    char dummy_msg1[STRLEN];
    strcpy(dummy_msg1, "Hello World!");
    char dummy_msg2[STRLEN];
    strcpy(dummy_msg2, "I am messaging, yay!");
#if 0
    /* to set the uart TIMEOUT value: */
    if (true == loraCommunication("AT+UART=TIMEOUT,300\r\n", STD_WAITING_TIME, retval_str)) {
        printf("%s\n",retval_str);
    }
#endif
    struct repeating_timer timer;
    add_repeating_timer_ms(BUTTON_PERIOD, repeatingTimerCallback, NULL, &timer);

    while (true) {

        if (buttonEvent) {
            buttonEvent = false;
            if (true == lora_send) {
                lora_send = false;
            } else {
                lora_send = true;
            }
        }

        if (true == lora_send && true == lora_connected) {
            if (loraMsg(dummy_msg1, strlen(dummy_msg1), retval_str)) {
                printf("%s\n", retval_str);
            } else {
                printf("Message not sent successfully.\n");
            }
            lora_send = false;
        }
    }
}

void buttonInit() {
    gpio_init(SW_0);
    gpio_set_dir(SW_0, GPIO_IN);
    gpio_pull_up(SW_0);
}

bool repeatingTimerCallback(struct repeating_timer *t) {
    // For SW_1: ON-OFF
    static uint button_state = 0, filter_counter = 0;
    uint new_state = 1;

    new_state = gpio_get(SW_0);
    if (button_state != new_state) {
        if (++filter_counter >= BUTTON_FILTER) {
            button_state = new_state;
            filter_counter = 0;
            if (new_state != RELEASED) {
                buttonEvent = true;
            }
        }
    } else {
        filter_counter = 0;
    }
    return true;
}
