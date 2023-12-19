#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "uart.h"
#include "hardware/pwm.h"
#include "lorawan.h"

#define SW_0 9
#define BUTTON_PERIOD 10
#define BUTTON_FILTER 5
#define RELEASED 1

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
#define WAITING_TIME 500
#define MAX_COUNT 5

#define MAX_COMMANDS 6 // 6 commands that needs to set up communication and network communication with lorawan, not including message
#define STRLEN 80

typedef struct lorawan_item_ {
    char command[STRLEN];
    char retval[STRLEN];
    uint sleep_time;
} lorawan_item;

void buttonInit();
bool repeatingTimerCallback(struct repeating_timer *t);
void removeColonsAndLowercase(const char *input, char *output);

volatile bool buttonEvent = false;
volatile uint lorawanState = 0;

int main(void) {

    stdio_init_all();

    buttonInit();

    uart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

    bool lora_comm = false;
    lorawan_item lorawan[MAX_COMMANDS] = {{"AT\r\n", "+AT: OK\r\n", 500},
                                          {"AT+MODE=LWOTAA\r\n", "+MODE: LWOTAA\r\n", 500},
                                          {"AT+KEY=APPKEY,\"511F30D4D81E7B806536733DE7155FDE\"\r\n", "+KEY: APPKEY 511F30D4D81E7B806536733DE7155FDE\r\n", 500},  // Gemma
                                              //{"AT+KEY=APPKEY,\"83A228D811E594812D8735EDDCCE28D0\"\r\n", "+KEY: APPKEY 83A228D811E594812D8735EDDCCE28D0\r\n", 500},  // Mong
                                              //{"AT+KEY=APPKEY,\"3D036E4388F937105A649BA6B0AD6366\"\r\n", "+KEY: APPKEY 3D036E4388F937105A649BA6B0AD6366\r\n", 500},  // Xuan
                                          {"AT+CLASS=A\r\n", "+CLASS: A\r\n", 500},
                                          {"AT+PORT=8\r\n", "+PORT: 8\r\n", 500},
                                          {"AT+JOIN\r\n", "+JOIN: Starting\r\n", 10000}}; // +JOIN: Starting
                                                                                                                   // +JOIN: NORMAL
                                                                                                                   // +JOIN: NetID 000000 DevAddr
                                                                                                                   // +JOIN: Done
    lorawan_item  lorawan_message;
    char retval_str[STRLEN];

    struct repeating_timer timer;
    add_repeating_timer_ms(BUTTON_PERIOD, repeatingTimerCallback, NULL, &timer);

    while (true) {

        if (buttonEvent) {
            buttonEvent = false;
            if (true == lora_comm) {
                lora_comm = false;
            } else {
                lora_comm = true;
            }
        }

        uint count = 0;
        bool retval_bool = 0;

        if (true == lora_comm) {
            switch (lorawanState) {
                case 0:                         /* Connecting to Lora module */
                    while (MAX_COUNT > count++) {
                        retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                        if (retval_bool) {
                            if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                                printf("Comparison->same for: %s\n", retval_str);
                                lorawanState = 1;
                                retval_bool = 0;
                                break;
                            } else {
                                printf("Comparison-> not same, retval_str: %s\n", retval_str);
                                printf("Exiting lora communication.\n");
                                lora_comm = false;
                            }
                        }
                    }
                    if (MAX_COUNT == count) {
                        printf("Lorawan module not responding.\n");
                        lora_comm = false;
                    }
                case 1:                         /*          MODE          */
                    retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                    if (retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 2;
                            break;
                        } else {
                            printf("Comparison-> not same, retval_str: %s\n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_comm = false;
                        }
                    } else {
                        printf("Mode setting failed, exiting communication with LoraWan.\n");
                        lorawanState = 0;
                        lora_comm = false;
                        break;
                    }
                case 2:                          /*     APIKEY      */
                    retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                    if (retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 3;
                            break;
                        } else {
                            printf("Comparison-> not same, retval_str: %s\n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_comm = false;
                        }
                    } else {
                        printf("Mode setting failed, exiting communication with LoRa.\n");
                        lorawanState = 0;
                        lora_comm = false;
                        break;
                    }
                case 3:                          /*       CLASS       */
                    retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                    if (retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 4;
                            break;
                        } else {
                            printf("Comparison-> not same, retval_str: %s\n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_comm = false;
                        }
                    } else {
                        printf("APIKEY phase failed, exiting communication with LoRa.\n");
                        lorawanState = 0;
                        lora_comm = false;
                        break;
                    }
                case 4:
                    retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                    if (retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 5;
                            break;
                        } else {
                            printf("Comparison-> not same, retval_str: %s\n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_comm = false;
                        }
                    } else {
                        printf("APIKEY phase failed, exiting communication with LoRa.\n");
                        lorawanState = 0;
                        lora_comm = false;
                        break;
                    }
               case 5:
                   retval_bool = generic_lora(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, retval_str);
                    if (retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lora_comm = false;
                            break;
                        } else {
                            printf("Comparison-> not same, retval_str: %s\n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_comm = false;
                        }
                    } else {
                        printf("APIKEY phase failed, exiting communication with LoRa.\n");
                        lorawanState = 0;
                        lora_comm = false;
                        break;
                    }
            }
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

void removeColonsAndLowercase(const char *input, char *output) {
    int inputLength = strlen(input);
    int outputIndex = 0;

    for (int i = 0; i < inputLength; i++) {
        if (isxdigit((unsigned char) input[i])) {
            output[outputIndex] = tolower(input[i]);
            outputIndex++;
        }
    }

    output[outputIndex] = '\0';
}
