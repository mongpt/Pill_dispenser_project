#include <stdio.h>
#include <string.h>
#include "pico/time.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "uart.h"
#include "lorawan.h"

#ifndef DEBUG_PRINT
#define DBG_PRINT(f_, ...)  printf((f_), ##__VA_ARGS__)
#else
#define DBG_PRINT(f_, ...)
#endif

static const int uart_nr = UART_NR;
static volatile int lorawanState = 0;
static lorawan_item lorawan[] = {{"AT\r\n", "+AT: OK\r\n", STD_WAITING_TIME},
                                 {"AT+MODE=LWOTAA\r\n", "+MODE: LWOTAA\r\n", STD_WAITING_TIME},
                                 {"AT+KEY=APPKEY,\"511F30D4D81E7B806536733DE7155FDE\"\r\n", "+KEY: APPKEY 511F30D4D81E7B806536733DE7155FDE\r\n", STD_WAITING_TIME},  // Gemma
                                 //{"AT+KEY=APPKEY,\"83A228D811E594812D8735EDDCCE28D0\"\r\n", "+KEY: APPKEY 83A228D811E594812D8735EDDCCE28D0\r\n", STD_WAITING_TIME},  // Mong
                                 //{"AT+KEY=APPKEY,\"3D036E4388F937105A649BA6B0AD6366\"\r\n", "+KEY: APPKEY 3D036E4388F937105A649BA6B0AD6366\r\n", STD_WAITING_TIME},  // Xuan
                                 {"AT+CLASS=A\r\n", "+CLASS: A\r\n", STD_WAITING_TIME},
                                 {"AT+PORT=8\r\n", "+PORT: 8\r\n", STD_WAITING_TIME},
                                 {"AT+JOIN\r\n", "", MSG_WAITING_TIME}};

bool loraInit() {
    uint count = 0;
    char return_message[STRLEN];
    const char joined[] = "Network joined";

    uart_setup(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

    while (true) {
        if (0 == lorawanState) {                         /*    Connecting to Lora module    */
            while (MAX_COUNT > count++) {
                if(true == retvalChecker(lorawanState)) {
                    lorawanState++;
                }
            }
            if (MAX_COUNT == count) {
                DBG_PRINT("Attempted to connect 5 times, Lorawan module not responding.\n");
                return false;
            }
        }
        for (lorawanState; lorawanState < (sizeof(lorawan)/sizeof(lorawan[0]) - 1); lorawanState++) {
            if (false == retvalChecker(lorawanState)) {
                lorawanState = 0;
                return false;
            }
        }
        if (true == loraCommunication(lorawan[lorawanState].command, lorawan[lorawanState].sleep_time, return_message)) {
            if (strstr(return_message, joined) != NULL) {
                DBG_PRINT("Comparison->same for: %s\n", return_message);
                lorawanState = 0;
                return true;
            }
        }
        lorawanState = 0;
        return false;
    }
}

bool loraCommunication(const char* command, const uint sleep_time, char* str) {
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

bool loraMsg(const char* message, size_t msg_size, char* return_message) {

    const char start_tag[] = "AT+MSG=\"";
    const char end_tag[] = "\"\r\n";
    char lorawan_message[STRLEN];

    if (msg_size > STRLEN-strlen(start_tag)-strlen(end_tag)-1) {
        return false;
    }

    strcpy(lorawan_message, start_tag);
    strncpy(&lorawan_message[strlen(start_tag)], message, STRLEN-strlen(start_tag)-strlen(end_tag)-1);
    strcat(lorawan_message, end_tag);
    lorawan_message[STRLEN-1] = '\0';
    DBG_PRINT("%s", lorawan_message);
    if(true == loraCommunication(lorawan_message, MSG_WAITING_TIME, return_message)) {
        return true;
    } else {
        return false;
    }
}

bool retvalChecker(const int index) {
    char return_message[STRLEN];

    if (true == loraCommunication(lorawan[index].command, lorawan[index].sleep_time, return_message)) {
        if (strcmp(lorawan[index].retval, return_message) == 0) {
            DBG_PRINT("Comparison->same for: %s\n", return_message);
            return true;
        } else {
            DBG_PRINT("Comparison->no match, return_message: %s lorawan[%d].retval: %s\n", return_message, index, lorawan[index].retval);
            DBG_PRINT("Exiting lora communication.\n");
            return false;
        }
    } else {
        DBG_PRINT("[%d] command failed, exiting lora communication.\n", index);
        return false;
    }
}
