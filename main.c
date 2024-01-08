#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "pill_eeprom.h"
#include "uart.h"
#include "lorawan.h"
#include "hardware/rtc.h"
#include "string.h"

#define in1                 13
#define in2                 6
#define in3                 3
#define in4                 2
#define sw_calib            7  //sw2
#define sw_start            9  //sw0
#define led                 20
#define piezo               27
#define opto                28
#define SPEED               2
#define CALIB_ROUNDS        2
#define DEBOUNCE            30 // debounce timer 30ms
#define DISPENSING_TIMEOUT  20000000 //timeout for dispensing 1 pill 30s (deducted 10s of Lora)
#define PILL_DROP_TIME      100  //duration of pill drops from wheel to piezo 100ms
#define NUM_PILLS_DISPENSED 32767    //store number of pills actually dispensed
#define NUM_DISPENSED_TIMES 32766   //store number of dispensed times (days): 1-7 times
// address 32765 is reserved for adjustSteps
#define LAST_CALIB_STEPS    32763  //store number of steps per revolution from the last calibration (2 bytes)
#define IS_TURNING          32762   //store the state of compartment's turning

void configHardware();
uint16_t runAndCountSteps(bool optoOutput);
void adjustCompartment(int adjustSteps);
void doCalib(uint16_t *steps);
void startDispensation(uint16_t stepPerRev, uint8_t *numPillsDispensed, uint8_t *numDispensedTimes);
void gpioIrq(uint gpio, uint32_t events);  //callback function for buttons pressed
void blinkMs(uint32_t ms);  //blink the led in ms
void reCalib(uint8_t numDispensedTimes, uint16_t stepPerRev, int adjustSteps);
char* getDateTime();

const uint8_t in[4] = {in1, in2, in3, in4};
const uint8_t stepsData[8][4] = {
        {1,0,0,0},
        {1,1,0,0},
        {0,1,0,0},
        {0,1,1,0},
        {0,0,1,0},
        {0,0,1,1},
        {0,0,0,1},
        {1,0,0,1}
};
volatile bool pillDetected = false; //true if piezo detects a dropped pill
static bool lora_connected = false;
static uint8_t nextPos = 0;    //keep track the next step position of motor
static uint8_t isTurning = 0; //1=a compartment is still turning, 0=stopped
static datetime_t t;

int main() {
    configHardware();
    // Start the RTC
    rtc_init();
    rtc_get_datetime(&t);
    if (t.year < 2024){
        t.year  = 2024;
        t.month = 1;
        t.day   = 8;
        t.dotw  = 1; // 0 is Sunday, so 5 is Friday
        t.hour  = 13;
        t.min   = 42;
        t.sec   = 00;
        rtc_set_datetime(&t);
    }
    ////////////////////
    uint16_t stepPerRev = 0;    //count number of steps for a revolution after calibration
    bool startDispensing = false;
    uint8_t numPillsDispensed;  //check the number of pills were actually dispensed
    uint8_t numDispensedTimes;  //check the number of dispensed times (days), 1-7 times
    uint8_t buff[4];    //eeprom's addresses used to write/read data
    uint64_t dispensingTimeout = DISPENSING_TIMEOUT;
    while (!lora_connected)
        lora_connected = loraInit();
    //read log from eeprom
    readEELog();

    char msg[62] = "\0"; //store log stringgetDateTime();
    strcpy(msg, getDateTime());
    strcat(msg, " Boot");
    // write new log to eeprom
    writeEELog(msg);

    // Enable interrupts with callback for piezo sensor
    gpio_set_irq_enabled_with_callback(piezo, GPIO_IRQ_EDGE_FALL, true, &gpioIrq);

    //check number of pills were dispensed before boot
    buff[0] = (uint8_t) (NUM_PILLS_DISPENSED >> 8);
    buff[1] = (uint8_t) NUM_PILLS_DISPENSED;
    eepromRead(buff, &numPillsDispensed, 1);
    //check number of dispensed times before boot
    buff[0] = (uint8_t) (NUM_DISPENSED_TIMES >> 8);
    buff[1] = (uint8_t) NUM_DISPENSED_TIMES;
    eepromRead(buff, &numDispensedTimes, 1);
    //check the state of turning progress before boot
    buff[0] = (uint8_t) (IS_TURNING >> 8);
    buff[1] = (uint8_t) IS_TURNING;
    eepromRead(buff, &isTurning, 1);
    //recall last stepPerRev from eeprom
    uint8_t stepPerRevBuff[3];
    buff[0] = (uint8_t) (LAST_CALIB_STEPS >> 8);
    buff[1] = (uint8_t) LAST_CALIB_STEPS;
    eepromRead(buff, stepPerRevBuff, 3);
    stepPerRev = (stepPerRevBuff[0] << 8) | stepPerRevBuff[1];
    int adjustSteps = stepPerRevBuff[2];
    if (isTurning || (numDispensedTimes >= 1 && numDispensedTimes < 7)){
        strcpy(msg, "Device was turned off/reset during turning");
        loraMsg(msg, strlen(msg));
        // write new log to eeprom
        writeEELog(msg);
        if (isTurning){
            // do recalib device
            reCalib(numDispensedTimes, stepPerRev, adjustSteps);
        }
        sleep_ms(1000);
        // continue the rest dispensing times
        uint8_t leftTimes = 7 - numDispensedTimes;
        for (int x = 0; x < leftTimes; x++){
            uint64_t startTimeNewDispensation = time_us_64();
            startDispensation(stepPerRev, &numPillsDispensed, &numDispensedTimes);
            if (x == leftTimes - 1){
                strcpy(msg, "Dispenser is empty!");
                loraMsg(msg, strlen(msg));
                // write new log to eeprom
                writeEELog(msg);
                dispensingTimeout = DISPENSING_TIMEOUT - 10;
            }
            while (time_us_64() - startTimeNewDispensation < dispensingTimeout);    //wait until timeout 30s
        }
    }

    while (1){
        //reset data before starting a new turn
        numDispensedTimes = 0;
        numPillsDispensed = 0;
        dispensingTimeout = DISPENSING_TIMEOUT;
        pillDetected = false;
        startDispensing = false;
        while (gpio_get(sw_calib))
            blinkMs(100);
        if (!gpio_get(sw_calib)){
            sleep_ms(DEBOUNCE);
            if (!gpio_get(sw_calib)){
                gpio_put(led, 1);
                doCalib(&stepPerRev);
                while (!gpio_get(sw_calib));    //wait for sw_calib released
                while (!startDispensing) {
                    if (!gpio_get(sw_start)){
                        sleep_ms(DEBOUNCE);
                        if (!gpio_get(sw_start)) {
                            gpio_put(led, 0);
                            startDispensing = true;
                            //update numDispensedTimes & numPillsDispensed to eeprom
                            buff[0] = (uint8_t) (NUM_DISPENSED_TIMES >> 8);
                            buff[1] = (uint8_t) NUM_DISPENSED_TIMES;
                            buff[2] = numDispensedTimes;
                            buff[3] = numPillsDispensed;
                            eepromWrite(buff, 4);
                            // run 7 times, each 1/8th rev
                            for (int i = 0; i < 7; i++){
                                uint64_t startTimeNewDispensation = time_us_64();
                                startDispensation(stepPerRev, &numPillsDispensed, &numDispensedTimes);
                                if (i == 6){
                                    strcpy(msg, "Dispenser is empty!");
                                    loraMsg(msg, strlen(msg));
                                    // write new log to eeprom
                                    writeEELog(msg);
                                    dispensingTimeout = DISPENSING_TIMEOUT - 10;
                                }
                                while (time_us_64() - startTimeNewDispensation < dispensingTimeout);    //wait until timeout 30s

                                while (time_us_64() - startTimeNewDispensation < DISPENSING_TIMEOUT);    //wait until timeout 30s
                            }
                            while (!gpio_get(sw_start));    //wait for sw_start released
                        }
                    }
                }
            }
        }
    }
    return 0;
}

//config hardware
void configHardware(){
    stdio_init_all();
    // Config switches
    gpio_set_function(sw_calib, GPIO_FUNC_SIO);
    gpio_set_dir(sw_calib, false);
    gpio_pull_up(sw_calib);
    gpio_set_function(sw_start, GPIO_FUNC_SIO);
    gpio_set_dir(sw_start, false);
    gpio_pull_up(sw_start);
    // config led
    gpio_set_function(led, GPIO_FUNC_SIO);
    gpio_set_dir(led, true);
    gpio_set_function(led, GPIO_FUNC_SIO);
    // config i2c
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(16, GPIO_FUNC_I2C);
    gpio_set_function(17, GPIO_FUNC_I2C);
    // config stepper
    gpio_init(in1);
    gpio_set_dir(in1, GPIO_OUT);
    gpio_init(in2);
    gpio_set_dir(in2, GPIO_OUT);
    gpio_init(in3);
    gpio_set_dir(in3, GPIO_OUT);
    gpio_init(in4);
    gpio_set_dir(in4, GPIO_OUT);
    // config dispenser
    gpio_set_function(opto, GPIO_FUNC_SIO);
    gpio_set_dir(opto, false);
    gpio_pull_up(opto);
    gpio_set_function(piezo, GPIO_FUNC_SIO);
    gpio_set_dir(piezo, false);
    gpio_pull_up(piezo);
}

// interrupt handler for gpio
void gpioIrq(uint gpio, uint32_t events){
    if (gpio == piezo)
        pillDetected = true;
}

void blinkMs(uint32_t ms){
    gpio_put(led, 1);
    sleep_ms(ms);
    gpio_put(led, 0);
    sleep_ms(ms);
}

uint16_t runAndCountSteps(bool optoOutput){
    uint16_t stepCalibCount = 0;
    while (gpio_get(opto) == optoOutput){
        for (int i = nextPos; i < 8; i++){
            for (int j = 0; j < 4; j++)
                gpio_put(in[j], stepsData[i][j]);
            busy_wait_ms(SPEED);
            stepCalibCount++;
            if (gpio_get(opto) != optoOutput) {
                nextPos = i == 7 ? 0 : i + 1;
                break;
            }
        }
        if (gpio_get(opto) == optoOutput)
            nextPos = 0;
    }
    return stepCalibCount;
}

void adjustCompartment(int adjustSteps){
    char msg[62] = "\0";
    printf("Adjusting device...\n");
    uint8_t revPos;
    if (nextPos == 0)
        revPos = 6;
    else if (nextPos == 1)
        revPos = 7;
    else
        revPos = nextPos - 2;
    while (adjustSteps > 0){
        for (int i = revPos; i >= 0; i--){
            for (int j = 0; j < 4; j++)
                gpio_put(in[j], stepsData[i][j]);
            busy_wait_ms(SPEED);
            adjustSteps--;
            if (adjustSteps == 0){
                nextPos = i == 7 ? 0 : i + 1;
                break;
            }
            if (i == 0)
                revPos = 7;
        }
    }
    strcpy(msg, getDateTime());
    strcat(msg, " Device calibrated.");
    // write new log to eeprom
    writeEELog(msg);
}

void doCalib(uint16_t *steps){
    char msg[62] = "\0";
    uint8_t buff[5];    //store number of steps per revolution to eeprom
    int stepPerOptoChange;  //number of steps while a state of the opto sensor keeps unchanged
    int adjustSteps;  //number of steps to align the wheel with the hole
    nextPos = 0;
    uint16_t stepCalibCount = 0;  //count total of steps during calibration
    //if the opto is blocked, then run motor until the opto is activated
    strcpy(msg, getDateTime());
    strcat(msg, " Device calibrating...");
    // write new log to eeprom
    writeEELog(msg);
    bool optoOutput = gpio_get(opto);
    if (optoOutput){
        runAndCountSteps(optoOutput);
        optoOutput = gpio_get(opto);
    }
    //run motor to the position where it blocks the opto
    runAndCountSteps(optoOutput);
    stepCalibCount = 0; // reset this to start calib
    // start to run and count steps for n rounds
    for (int round = 0; round < CALIB_ROUNDS; round++){
        // run twice because opto reaches 2 states for 1 round
        for (int i = 0; i < 2; i++){
            optoOutput = gpio_get(opto);
            stepPerOptoChange = runAndCountSteps(optoOutput);
            stepCalibCount += stepPerOptoChange;
        }
    }
    adjustSteps = stepPerOptoChange / 2;
    *steps = stepCalibCount / CALIB_ROUNDS;
    buff[0] = (uint8_t) (LAST_CALIB_STEPS >> 8);
    buff[1] = (uint8_t) LAST_CALIB_STEPS;
    buff[2] = (uint8_t) (*steps >> 8);
    buff[3] = (uint8_t) *steps;
    buff[4] = adjustSteps;
    eepromWrite(buff, 5);
    //adjust the compartment's position
    adjustCompartment(adjustSteps);
}

void startDispensation(uint16_t stepPerRev, uint8_t *numPillsDispensed, uint8_t *numDispensedTimes){
    int i;
    char msg[62] = "\0";
    uint8_t buff[4];
    int stepsToRun = stepPerRev / 8;
    isTurning = 1;  //turning started
    //update data to eeprom
    buff[0] = (uint8_t) (IS_TURNING >> 8);
    buff[1] = (uint8_t) IS_TURNING;
    buff[2] = isTurning;
    eepromWrite(buff, 3);
    while (stepsToRun > 0){
        for (i = nextPos; i < 8; i++){
            for (int j = 0; j < 4; j++){
                gpio_put(in[j], stepsData[i][j]);
            }
            busy_wait_ms(SPEED);
            stepsToRun--;
            if (stepsToRun == 0) {
                nextPos = i == 7 ? 0 : i + 1;
                isTurning = 0;  //turning finished
                //update data to eeprom
                buff[0] = (uint8_t) (IS_TURNING >> 8);
                buff[1] = (uint8_t) IS_TURNING;
                buff[2] = isTurning;
                eepromWrite(buff, 3);
                (*numDispensedTimes)++;
                strcpy(msg, getDateTime());
                strcat(msg, " Day ");
                char num2Str[2];
                sprintf(num2Str, "%d:", *numDispensedTimes);
                strcat(msg, num2Str);
                busy_wait_ms(PILL_DROP_TIME);
                if (pillDetected)
                    (*numPillsDispensed)++;
                //update data to eeprom
                buff[0] = (uint8_t) (NUM_DISPENSED_TIMES >> 8);
                buff[1] = (uint8_t) NUM_DISPENSED_TIMES;
                buff[2] = *numDispensedTimes;
                buff[3] = *numPillsDispensed;
                eepromWrite(buff, 4);
                if (pillDetected){
                    pillDetected = false;
                    strcat(msg, " Pill detected.");
                } else{
                    strcat(msg, " No pill detected.");
                    for (int x = 0; x < 5; x++)
                        blinkMs(300);
                }
                strcat(msg, " Pills left: ");
                sprintf(num2Str, "%d", 7 - *numDispensedTimes);
                strcat(msg, num2Str);
                // write new log to eeprom
                writeEELog(msg);
                loraMsg(msg, strlen(msg));
                break;
            }
        }
        if (i == 8)
            nextPos = 0;
    }
}

void reCalib(uint8_t numDispensedTimes, uint16_t stepPerRev, int adjustSteps){

    //turn motor backward until opto changes to 1
    bool optoOutput = gpio_get(opto);
    if (optoOutput){
        //run backward until opto = 0
        while (gpio_get(opto) == optoOutput){
            for (int i = 7; i >= 0; i--){
                for (int j = 0; j < 4; j++)
                    gpio_put(in[j], stepsData[i][j]);
                busy_wait_ms(SPEED);
                if (gpio_get(opto) != optoOutput) {
                    nextPos = i == 7 ? 0 : i + 1;
                    break;
                }
            }
        }
        //adjust the compartment's position
        adjustCompartment(adjustSteps);
        //run forward to continue the dispensation
        int stepsToRun = (stepPerRev  / 8) * numDispensedTimes;
        int i;
        while (stepsToRun > 0){
            for (i = nextPos; i < 8; i++){
                for (int j = 0; j < 4; j++){
                    gpio_put(in[j], stepsData[i][j]);
                }
                busy_wait_ms(SPEED);
                stepsToRun--;
                if (stepsToRun == 0) {
                    nextPos = i == 7 ? 0 : i + 1;
                    busy_wait_ms(PILL_DROP_TIME);
                    break;
                }
            }
            if (i == 8)
                nextPos = 0;
        }
    }
}

char* getDateTime(){
    static char timeData[20] = "\0";
    rtc_get_datetime(&t);
    sprintf(timeData, "%d/%d/%d %d:%d:%d",t.day, t.month, t.year, t.hour, t.min, t.sec);
    //printf("%s\n", timeData);
    return timeData;
}
