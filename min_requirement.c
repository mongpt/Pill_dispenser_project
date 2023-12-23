#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>
#include "hardware/uart.h"
#include "hardware/i2c.h"

#define in1 13
#define in2 6
#define in3 3
#define in4 2
#define TX_PIN 4
#define RX_PIN 5
#define sw_calib 7  //sw2
#define sw_start 9  //sw0
#define led 20
#define piezo 27
#define opto 28
#define SPEED 2
#define CMD_LEN 20
#define uart_name uart1
#define BAUDRATE 9600
#define STRLEN 80
#define EUI_LEN 17
#define CALIB_ROUNDS 1
#define DEBOUNCE 30 // debounce timer 30ms
#define DISPENSING_TIMEOUT 5000000 //timeout for dispensing 1 pill 30s
#define PILL_DROP_TIME 100  //duration of pill drops from wheel to piezo 100ms

volatile bool pillDetected = false; //true if piezo detects a dropped pill

void configHardware();
uint16_t runAndCountSteps(bool optoOutput);
void uartHandler();    //uart handler for read/erase commands
void doCalib(uint16_t *steps);
void startDispensation(uint16_t stepPerRev);
void gpioIrq(uint gpio, uint32_t events);  //callback function for buttons pressed
void blinkMs(uint32_t ms);  //blink the led in ms

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
static uint8_t nextPos = 0;    //keep track the next step position of motor

int main() {
    uint16_t stepPerRev = 0;    //count number of steps for a revolution after calibration
    bool startDispensing = false;
    configHardware();
    // Enable interrupts with callback for piezo sensor
    gpio_set_irq_enabled_with_callback(piezo, GPIO_IRQ_EDGE_FALL, true, &gpioIrq);

    while (1){
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
                            // run 7 times, each 1/8th rev
                            for (int i = 0; i < 7; i++){
                                uint64_t startTimeNewDispensation = time_us_64();
                                startDispensation(stepPerRev);
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

void doCalib(uint16_t *steps){
    int stepPerOptoChange;  //number of steps while a state of the opto sensor keeps unchanged
    int adjustSteps;  //number of steps to align the wheel with the hole
    nextPos = 0;
    uint16_t stepCalibCount = 0;  //count total of steps during calibration
    //if the opto is blocked, then run motor until the opto is activated
    printf("Calibrating device...\n");
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
    printf("Adjusting device...\n");
    uint8_t revPos = nextPos - 2;   //position of step that used in reverse direction
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
    printf("*** Calibration done ***\n");
    printf("------------------------------\n");
}

void startDispensation(uint16_t stepPerRev){
    int i;
    int stepsToRun;
    stepsToRun = stepPerRev / 8;
    printf("Pill dispensing...\n");
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
                if (pillDetected){
                    pillDetected = false;
                    printf("Pill detected\n");
                } else{
                    printf("No pill detected\n");
                    for (int x = 0; x < 5; x++)
                        blinkMs(300);
                }
                break;
            }
        }
        if (i == 8)
            nextPos = 0;
    }
}
