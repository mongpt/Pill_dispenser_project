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

#define MAX_COMMANDS 6 // 6 commands that needs to set up communication and network communication with lorawan, not including message
#define STRLEN 80

#define STD_WAITING_TIME 500
#define MSG_WAITING_TIME 10000

typedef struct lorawan_item_ {
    char command[STRLEN];
    char retval[STRLEN];
    uint sleep_time;
} lorawan_item;

void buttonInit();
bool repeatingTimerCallback(struct repeating_timer *t);

static volatile bool buttonEvent = false;
static volatile uint lorawanState = 0;
static volatile bool lora_init = false;
static volatile bool lora_comm = false;

int main(void) {

    stdio_init_all();

    buttonInit();

    uart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

    lorawan_item lorawan[MAX_COMMANDS] = {{"AT\r\n", "+AT: OK\r\n", STD_WAITING_TIME},
                                          {"AT+MODE=LWOTAA\r\n", "+MODE: LWOTAA\r\n", STD_WAITING_TIME},
                                          {"AT+KEY=APPKEY,\"511F30D4D81E7B806536733DE7155FDE\"\r\n", "+KEY: APPKEY 511F30D4D81E7B806536733DE7155FDE\r\n", STD_WAITING_TIME},  // Gemma
                                           //{"AT+KEY=APPKEY,\"83A228D811E594812D8735EDDCCE28D0\"\r\n", "+KEY: APPKEY 83A228D811E594812D8735EDDCCE28D0\r\n", STD_WAITING_TIME},  // Mong
                                           //{"AT+KEY=APPKEY,\"3D036E4388F937105A649BA6B0AD6366\"\r\n", "+KEY: APPKEY 3D036E4388F937105A649BA6B0AD6366\r\n", STD_WAITING_TIME},  // Xuan
                                          {"AT+CLASS=A\r\n", "+CLASS: A\r\n", STD_WAITING_TIME},
                                          {"AT+PORT=8\r\n", "+PORT: 8\r\n", STD_WAITING_TIME},
                                          {"AT+JOIN\r\n", "+JOIN: Starting\r\n", MSG_WAITING_TIME}};  // +JOIN: Starting       sleep_time: testing needed
                                                                                                                       // +JOIN: NORMAL
                                                                                                                       // +JOIN: NetID 000000 DevAddr
                                                                                                                       // +JOIN: Done

    char retval_str[STRLEN];
    char dummy_msg1[STRLEN];
    strcpy(dummy_msg1, "Hello World!");
    char dummy_msg2[STRLEN];
    strcpy(dummy_msg2, "I am messaging, yay!");
    char lorawan_message[STRLEN];

    struct repeating_timer timer;
    add_repeating_timer_ms(BUTTON_PERIOD, repeatingTimerCallback, NULL, &timer);

    while (true) {

        uint count = 0;
        bool retval_bool = false;

        if (buttonEvent) {
            buttonEvent = false;
            if (true == lora_init) {
                retval_bool = false;
                lorawanState = 0;
                lora_init = false;
            } else {
                lora_init = true;
            }
        }

        if (true == lora_init) {
            switch (lorawanState) {
                case 0:                         /*    Connecting to Lora module    */
                    while (MAX_COUNT > count++) {
                        retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                        retval_str);
                        if (true == retval_bool) {
                            if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                                printf("Comparison->same for: %s\n", retval_str);
                                lorawanState = 1;
                                retval_bool = false;
                                break;
                            } else {
                                printf("Comparison->no match, retval_str: %s lorawan[%d].retval: %s\n", retval_str, lorawanState, lorawan[lorawanState].retval);
                                printf("Exiting lora communication.\n");
                                lora_init = false;
                                break;
                            }
                        }
                    }
                    if (MAX_COUNT == count) {
                        printf("Lorawan module not responding.\n");
                        lora_init = false;
                    }
                case 1:                         /*            MODE             */
                    retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                    retval_str);
                    if (true == retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 2;
                            retval_bool = false;
                            break;
                        } else {
                            printf("Comparison->no match, retval_str: %s lorawan[%d].retval: %s\n", retval_str, lorawanState, lorawan[lorawanState].retval);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_init = false;
                            break;
                        }
                    } else {
                        printf("MODE phase failed, exiting lora communication.\n");
                        lorawanState = 0;
                        lora_init = false;
                        break;
                    }
                case 2:                         /*           APIKEY            */
                    retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                    retval_str);
                    if (true == retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 3;
                            retval_bool = false;
                            break;
                        } else {
                            printf("Comparison->no match, retval_str: %s lorawan[%d].retval: %s\n", retval_str, lorawanState, lorawan[lorawanState].retval);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_init = false;
                            break;
                        }
                    } else {
                        printf("APIKEY failed, exiting lora communication.\n");
                        lorawanState = 0;
                        lora_init = false;
                        break;
                    }
                case 3:                         /*           CLASS             */
                    retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                    retval_str);
                    if (true == retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 4;
                            retval_bool = false;
                            break;
                        } else {
                            printf("Comparison->no match, retval_str: %s lorawan[%d].retval: %s\n", retval_str, lorawanState, lorawan[lorawanState].retval);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_init = false;
                            break;
                        }
                    } else {
                        printf("CLASS phase failed, exiting lora communication.\n");
                        lorawanState = 0;
                        lora_init = false;
                        break;
                    }
                case 4:                         /*            PORT             */
                    retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                    retval_str);
                    if (true == retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lorawanState = 5;
                            retval_bool = false;
                            break;
                        } else {
                            printf("Comparison->no match, retval_str: %s lorawan[%d].retval: %s\n", retval_str, lorawanState, lorawan[lorawanState].retval);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_init = false;
                            break;
                        }
                    } else {
                        printf("PORT phase failed, exiting lora communication.\n");
                        lorawanState = 0;
                        lora_init = false;
                        break;
                    }
               case 5:                         /*            JOIN             */
                   retval_bool = loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time,
                                                   retval_str);
                    if (true == retval_bool) {
                        if (strcmp(lorawan[lorawanState].retval, retval_str) == 0) {
                            printf("Comparison->same for: %s\n", retval_str);
                            lora_init = false;
                            lorawanState = 0;
                            //lora_comm = true;
                            break;
                        } else {
                            printf("Comparison->no match, retval_str: %s \n", retval_str);
                            printf("Exiting lora communication.\n");
                            lorawanState = 0;
                            lora_init = false;
                            lora_comm = true; // this needs to commented out
                            break;
                        }
                    } else {
                        printf("JOIN phase failed, exiting communication with LoRa.\n");
                        lorawanState = 0;
                        lora_init = false;
                        break;
                    }
            }
        }

        if (true == lora_comm) {  // "AT+MSG=\"\"\r\n": +11 chars
            strcpy(lorawan_message, "AT+MSG=\"");
            strcat(lorawan_message, dummy_msg1);
            strcat(lorawan_message, "\"\r\n");
            retval_bool = loraCommunication(lorawan_message, MSG_WAITING_TIME, retval_str);
            if (true == retval_bool) {
                printf("%s\n", retval_str);
            }
            lora_comm = false;
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
