#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "string.h"

#define EE_ADDR 0x50
#define WRITE_CYCLE_TIME 5
#define sw0 9
#define sw1 8
#define sw2 7
#define led1 22
#define led2 21
#define led3 20
#define CMD_LEN 20

typedef struct ledState {
    uint8_t state;
    uint8_t not_state;
} ledState;

void configHardware();
void eepromWrite(const uint8_t *data, size_t len);  //write len data bytes to eeprom including 2 bytes word address
void eepromRead(const uint8_t *addr, uint8_t *dest, size_t len);    // read len data bytes back from eeprom at specified address
void readEELog();   //read all log from eeprom
int eraseEELog(); // erase eeprom for logging
void findNewLogEntry(uint8_t *buf);  // find a new valid entry for new logging
bool ledStateIsValid(ledState *ls);  // validate the eeprom data for leds states
void swCB(uint gpio, uint32_t events);  //callback function for buttons pressed
uint16_t crc16(const uint8_t *data_p, size_t length); //CRC calculation
void writeEELog(const char *logStr);    //write log to eeprom
void uartHandler();    //uart handler for read/erase commands
void handleSwPressed(uint sw, uint led, volatile bool *swPressed, uint64_t timeSwPressed, int *LS);

volatile bool sw0Pressed = false;
volatile bool sw1Pressed = false;
volatile bool sw2Pressed = false;
volatile uint64_t timeSw0Pressed;
volatile uint64_t timeSw1Pressed;
volatile uint64_t timeSw2Pressed;
volatile int pos = 0;
volatile char getCmd = 'N';
int led1State = 0;
int led2State = 0;
int led3State = 0;

int main()
{
    //config hardware
    configHardware();
    uint8_t bufLedState[4]; // 2 bytes address + 2 bytes data
    char logStr[61] = "\0";
    // Convert the integer to a string
    char timestampStr[20];  // Adjust the size based on your needs
    sprintf(timestampStr, "%llus; ", time_us_64() / 1000000);
    strcat(logStr, "Elapsed time: ");
    strcat(logStr, timestampStr);
    strcat(logStr, "Boot; "); // store log string
    // write the "Boot" log to eeprom when the device starts
    writeEELog(logStr);
    // prepare data to recall leds states from eeprom
    bufLedState[0] = 0x7F; //msb
    bufLedState[1] = 0xFE;  //lsb
    //buf[2] = 0xFF; //data
    uint8_t ledStateData[2] = {0,0};
    //recall last states of LEDs stored in eeprom
    eepromRead(bufLedState, ledStateData, 2);
    ledState ls = {.state = ledStateData[0], .not_state = ledStateData[1]};
    //validate led state
    if (ledStateIsValid(&ls)){
        // change led state according to the data
        // define state of each led
        led1State = ls.state & 0x1;
        led2State = (ls.state >> 1) & 0x1;
        led3State = (ls.state >> 2) & 0x1;
        gpio_put(led1, led1State);
        gpio_put(led2, led2State);
        gpio_put(led3, led3State);
    }
    else{
        led2State = 1;
        gpio_put(led2, led2State);
        ls.state = 0x02; //010b
        bufLedState[2] = ls.state;
        bufLedState[3] = ~ls.state;
        eepromWrite(bufLedState, 4);
    }
    printf("Elapsed time: %llus; ", time_us_64() / 1000000);
    printf("%s", led1State == 1 ? "1 ON; " : "1 OFF; ");
    printf("%s", led2State == 1 ? "2 ON; " : "2 OFF; ");
    printf("%s\n", led3State == 1 ? "3 ON; " : "3 OFF; ");
    printf("------------------------------\n");
    // Enable interrupts with callback for switches
    gpio_set_irq_enabled_with_callback(sw0, GPIO_IRQ_EDGE_FALL, true, &swCB);
    gpio_set_irq_enabled_with_callback(sw1, GPIO_IRQ_EDGE_FALL, true, &swCB);
    gpio_set_irq_enabled_with_callback(sw2, GPIO_IRQ_EDGE_FALL, true, &swCB);
    // enable uart interrupt
    irq_set_exclusive_handler(UART0_IRQ, uartHandler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);

    while (1){
        if (sw0Pressed)
            handleSwPressed(sw0, led1, &sw0Pressed, timeSw0Pressed, &led1State);
        if (sw1Pressed)
            handleSwPressed(sw1, led2, &sw1Pressed, timeSw1Pressed, &led2State);
        if (sw2Pressed)
            handleSwPressed(sw2, led3, &sw2Pressed, timeSw2Pressed, &led3State);
        if (getCmd == 'R'){
            readEELog();
            getCmd = 'N';
            uart_puts(uart0 ,"------------------------------\n");
        }
        if (getCmd == 'E'){
            // erase eeprom
            while(eraseEELog());    // if this function return 1 (means erasing did not succeed) then do again
            uart_puts(uart0,"*** EEPROM erased successfully ***\n");
            uart_puts(uart0 ,"------------------------------\n");
            getCmd = 'N';
        }
    }
    return 0;
}

//config hardware
void configHardware(){
    stdio_init_all();
    // Config switches and LEDs
    gpio_set_function(sw0, GPIO_FUNC_SIO);
    gpio_set_dir(sw0, false);
    gpio_pull_up(sw0);
    gpio_set_function(sw1, GPIO_FUNC_SIO);
    gpio_set_dir(sw1, false);
    gpio_pull_up(sw1);
    gpio_set_function(sw2, GPIO_FUNC_SIO);
    gpio_set_dir(sw2, false);
    gpio_pull_up(sw2);
    gpio_set_function(led1, GPIO_FUNC_SIO);
    gpio_set_dir(led1, true);
    gpio_set_function(led2, GPIO_FUNC_SIO);
    gpio_set_dir(led2, true);
    gpio_set_function(led3, GPIO_FUNC_SIO);
    gpio_set_dir(led3, true);
    // config i2c
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(16, GPIO_FUNC_I2C);
    gpio_set_function(17, GPIO_FUNC_I2C);
}

//write len data bytes to eeprom including 2 bytes word address
void eepromWrite(const uint8_t *data, size_t len){
    i2c_write_blocking(i2c0, EE_ADDR, data, len,false);
    sleep_ms(WRITE_CYCLE_TIME);
}

// read len data bytes back from eeprom at specified address
void eepromRead(const uint8_t *addr, uint8_t *dest, size_t len){
    i2c_write_blocking(i2c0, EE_ADDR, addr,2,true);
    sleep_ms(WRITE_CYCLE_TIME);
    i2c_read_blocking(i2c0,EE_ADDR, dest, len, false);
}

void readEELog(){
    printf("\n");
    // read log from eeprom
    uint8_t logData[64] = {0};
    uint8_t buf[3];
    bool foundLog = false;
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &buf[2], 1);
        if (buf[2]){ // found a valid log entry
            foundLog = true;
            eepromRead(buf, logData, 64);
            int zero_idx;
            for (zero_idx = 0; zero_idx < 62; zero_idx++){   // find index of 0 before CRC value
                if (logData[zero_idx] == 0){    //found 0 at zero_idx position
                    if (crc16(logData, zero_idx+3) == 0)
                        printf("%s\n", logData);
                    break;
                }
            }
        }
    }
    if (!foundLog)
        uart_puts(uart0, "No log found\n");
}

int eraseEELog(){
    uint8_t buf[3];
    buf[2] = 0x00;  //data
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromWrite(buf, 3);
    }

    // validate
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &buf[2], 1);
        if (buf[2])
            return 1;
    }
    return 0;
}

// find a new valid entry for new logging
void findNewLogEntry(uint8_t *buf){
    uint8_t data;
    for (int i = 0; i < 2048; i += 64){
        buf[0] = (uint8_t) (i >> 8);  //msb
        buf[1] = (uint8_t) i;  //lsb
        eepromRead(buf, &data, 1);
        if (!data) // found a valid address if its data is 0
            break;
    }
    if (data){  //all log entries are not available, need to erase them
        while(eraseEELog());    // if this function return 1 (means erasing did not succeed) then do again
        buf[0] = 0x00;
        buf[1] = 0x00;
    }
}

bool ledStateIsValid(ledState *ls){
    return ls->state == (uint8_t) ~ls->not_state;
}

// callback function when rotating encoder
void swCB(uint gpio, uint32_t events) {
    if (gpio == sw0){
        sw0Pressed = true;
        timeSw0Pressed = time_us_64();
    }
    else if (gpio == sw1){
        sw1Pressed = true;
        timeSw1Pressed = time_us_64();
    }
    else if (gpio == sw2){
        sw2Pressed = true;
        timeSw2Pressed = time_us_64();
    }
}

//CRC calculation
uint16_t crc16(const uint8_t *data_p, size_t length) {
    uint8_t x;
    uint16_t crc = 0xFFFF;
    while (length--) {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t) (x << 12)) ^ ((uint16_t) (x << 5)) ^ ((uint16_t) x);
    }
    return crc;
}

//write log to eeprom
void writeEELog(const char *logStr){
    uint8_t length = strlen(logStr);
    //trim log string if it exceeds 61 chars
    if (length > 61)
        length = 61;
    uint8_t bufLogging[length+3]; // 2 bytes address + log string + \0 + 2 bytes CRC
    uint8_t tempData[length+1]; //log string + \0
    // find the next log address for new log
    findNewLogEntry(bufLogging);    // get data for bufLogging[0] and bufLogging[1]
    for (int i = 0; i < length; i++)
        bufLogging[i+2] = logStr[i];
    bufLogging[length+2] = '\0';
    memcpy(tempData, logStr, strlen(logStr));
    tempData[length] = '\0';
    // calculate CRC then store it at idx+3 and idx+4 location
    uint16_t crc = crc16(tempData, sizeof(tempData));
    bufLogging[length+3] = (uint8_t) (crc >> 8);
    bufLogging[length+4] = (uint8_t) crc;
    // write log to eeprom
    eepromWrite(bufLogging, length+5);
}

void uartHandler(){
    char uartData[CMD_LEN];
    while (uart_is_readable_within_us(uart0, 1000)){
        char c = uart_getc(uart0);
        if (c == '\r' || c == '\n') {
            uartData[pos] = '\0';  // Null-terminate the string
            pos = 0;
            // after getting '\r' or '\n' then clear the rest in rev buff
            while (uart_is_readable_within_us(uart0, 500))
                uart_getc(uart0);
            if (strcmp(uartData, "read") == 0)
                getCmd = 'R';
            else if (strcmp(uartData, "erase") == 0)
                getCmd = 'E';
            else {
                printf("unknown command\n");
                printf("------------------------------\n");
            }
        } else {
            if (pos < CMD_LEN-1) {
                uartData[pos++] = c;
            }
        }
    }
}

void handleSwPressed(uint sw, uint led, volatile bool *swPressed, uint64_t timeSwPressed, int *LS){
    uint8_t filter = 0x00;
    uint8_t shiftBit = 0;
    switch (sw) {
        case sw0:
            filter = 0xFE;
            shiftBit = 0;
            break;
        case sw1:
            filter = 0xFD;
            shiftBit = 1;
            break;
        case sw2:
            filter = 0xFB;
            shiftBit = 2;
            break;
        default:
            break;
    }
    uint8_t bufLedState[4];
    bufLedState[0] = 0x7F; //msb
    bufLedState[1] = 0xFE;  //lsb
    if ((time_us_64() - timeSwPressed) >= 30000){
        if (!gpio_get(sw)){
            *swPressed = false;
            *LS = !*LS;
            gpio_put(led, *LS);
            ledState ls = {.state = 0x00, .not_state = 0xFF};
            // write led state to eeprom
            ls.state |= led3State << 2 | led2State << 1 | led1State;
            ls.state = (ls.state & filter) | (*LS << shiftBit);
            bufLedState[2] = ls.state;
            bufLedState[3] = ~ls.state;
            eepromWrite(bufLedState, 4);
            // write new log to eeprom
            char newStr[61] = "\0";
            // Convert the integer to a string
            char timestampStr[20];  // Adjust the size based on your needs
            sprintf(timestampStr, "%llus; ", time_us_64() / 1000000);
            strcat(newStr, "Elapsed time: ");
            strcat(newStr, timestampStr);
            strcat(newStr, led1State == 1 ? "1 ON; " : "1 OFF; ");
            strcat(newStr, led2State == 1 ? "2 ON; " : "2 OFF; ");
            strcat(newStr, led3State == 1 ? "2 ON; " : "2 OFF; ");
            printf("%s\n", newStr);
            writeEELog(newStr);
        }
    }
}
