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

/**********************************************************************************************************************
 * \brief: Initialises the uart and sets up lorawan communication
 *
 * \param:
 *
 * \return: true: if connection established, false: if connection not established
 *
 * \remarks: Programmer should use this to initialize uart and lorawan communication.
 **********************************************************************************************************************/
bool loraInit();

/**********************************************************************************************************************
 * \brief: Communicates with uart.
 *
 * \param: 3 parameters. Takes the command to be sent, the sleep time to wait before read and the string to read the
 *         returned message to.
 *
 * \return: true: if uart responses, false: if uart does not response
 *
 * \remarks: Called by loraInit(), loraMsg(), retvalChecker(). Can be used directly from main().
 **********************************************************************************************************************/
bool loraCommunication(const char* command, const uint sleep_time, char* str);

/**********************************************************************************************************************
 * \brief: Formats message and sends it to uart.
 *
 * \param: 3 parameters. Takes the message to be sent, message length and string where the returned message is read to.
 *
 * \return: true: if uart responses, false: if uart does not response
 *
 * \remarks: Programmer should use this to send message.
 **********************************************************************************************************************/
bool loraMsg(const char* message, size_t msg_size, char* return_message);

/**********************************************************************************************************************
 * \brief: Calls loraCommunication and compares the returned value with the value in case of success.
 *
 * \param: 1 parameter. Takes the index of the struct element.
 *
 * \return: true: if compared strings are the same, false: if compared strings are not the same
 *
 * \remarks: Called by loraInit(). Programmer should not use this function.
 **********************************************************************************************************************/
bool retvalChecker(const int index);

#endif
