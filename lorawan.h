#ifndef LORAWAN
#define LORAWAN

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

#define STD_WAITING_TIME 500
#define MSG_WAITING_TIME 10000

#define STRLEN 128

typedef struct lorawan_item_ {
    char command[STRLEN];
    char retval[STRLEN];
    uint sleep_time;
} lorawan_item;

bool loraInit();
bool loraCommunication(const char* command, const uint sleep_time, char* str);
bool loraMsg(const char *message, size_t msg_size, char *return_message);
bool retvalChecker(const int index);

#endif
