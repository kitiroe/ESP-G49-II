#pragma once
#include "mbed.h"
#include "C12832.h"
#include "ds2781.h"
#include "OneWire_Methods.h"
#include "QEI.h"
#include <cstdint>


#define SENSOR_AMOUNT 5 //dont change this pls
#define SENSOR_BUFFER 5 //Buffer Size. Ideally should be SENSOR_POLL_FREQ / SYS_OUTPUT_RATE
#define SENSOR_POLL_FREQ 1000 //Hz

//need tuning
#define GAIN_PROPORTIONAL 0.1 
#define GAIN_INTEGRAL 0.1
#define GAIN_DERIVATIVE 0.1
#define GAIN_AGGRESSIVE 1.2

#define BASE_DUTY 0.8f

#define SYS_OUTPUT_RATE 50 //Hz

#define WHEEL_DIAMETER 0.08//m
#define GEAR_RATIO 1/12
#define PI 3.14
#define CPR 256
#define TCRT_MAX_VDD 3.3 //V

//seerial 6 RX TX PA_11/12
//ANALOG IN PC_2/3/4/5 PB_1
//PWM PC_8/6
//encoder PB_15/14


typedef enum {starting,straightline,stop,turnaround} pstate;
pstate ProgramState;

//TCRT class. Creates an object for individual sensors on the sensor array with rolling average (size 5) built in. 
//Provide the Pin (ADC) for the sensor and the voltage levels expected (if you want to scale to voltage instead of from 0.0 - 1.0)
//Use getSensorVoltage() to get value of voltage (rolled average). 
class TCRT {
    private:
        AnalogIn sensorPin;
        float VDD,senseNorm;
        float senseNormRolled[SENSOR_BUFFER];
        int rIndex;
        static TCRT* sensors[SENSOR_BUFFER]; //operating with one ticker, so we must have a pointer to all the TCRT objects (to be used for function callback)
        static int sensorCount; //to keep count of the sensors
    public:
        TCRT(PinName Pin, float v): sensorPin(Pin), VDD(v)
        {
            rIndex = 0;
            senseNorm = 0;
            for(int i = 0;i<SENSOR_BUFFER;i++){ senseNormRolled[i] = 0; };
            if(sensorCount < SENSOR_AMOUNT){ sensors[sensorCount++] = this; };
        };
        void rollingPollAverage()
        {
            senseNormRolled[rIndex % SENSOR_BUFFER] = ampNorm();
            rIndex++;
            senseNorm = 0;
            for(int i = 0;i < SENSOR_BUFFER;i++){senseNorm += senseNormRolled[i];};
            senseNorm /= SENSOR_BUFFER;
        };
        //runs through all the polling once called. This is for synchronous polling between sensors since static is shared between all objects derived from TCRT
        static void pollSensors(void) { for(int i = 0; i < sensorCount; i++) {sensors[i]->rollingPollAverage(); }; }; 
        float ampNorm(void){return (sensorPin.read());};
        float getSensorVoltage(bool Volt){return (Volt? senseNorm*VDD : senseNorm);}; 
};

TCRT* TCRT::sensors[SENSOR_AMOUNT]; //static member declaration (must be outside class)
int TCRT::sensorCount = 0;  //static member declaration (must be outside class)



/*
Records QEI inputs and returns speed and distance for wheel. Use getDist() and getSpeed().
*/
class Encoder {
    private:
        QEI encode;
        uint32_t count, countPrev;
        float distance, speed;
    public:
        Encoder(PinName channel1, PinName channel2): encode(channel1,channel2,NC,CPR){ resetAllValues(); };
        void updateValues(void)
        {
            countPrev = count;
            count += encode.getPulses();
            distance = encode.getRevolutions()*WHEEL_DIAMETER*PI;
            speed = (((count - countPrev)/CPR)*SYS_OUTPUT_RATE)*WHEEL_DIAMETER*PI*GEAR_RATIO;
        };
        void resetAllValues(void)
        {
            encode.reset();
            distance = 0;
            speed = 0;
            count = 0;
            countPrev = 0;
        };
        float getDist(void){return distance;};  //returns distance from last reset() call
        float getSpeed(void){return speed;};    //returns speed in m/s
};

class PWMGen {
    private:
        PwmOut PWM_LEFT, PWM_RIGHT;
    public:
        PWMGen(PinName P1, PinName P2): PWM_LEFT(P1),PWM_RIGHT(P2) { reset();};
        void reset()
        {
            PWM_LEFT.period(0.0f);
            PWM_RIGHT.period(0.0f);
            PWM_LEFT.write(0.5f);
            PWM_RIGHT.write(0.5f);
        };
        void begin()
        {
            PWM_LEFT.period(1.0/SYS_OUTPUT_RATE);
            PWM_RIGHT.period(1.0/SYS_OUTPUT_RATE);
        };
        void setPWMDuty(float leftPWM, float rightPWM)
        {
            PWM_LEFT.write(leftPWM);
            PWM_RIGHT.write(rightPWM);
        };
};

//use the one-wire-pin PC_12
class BatteryMonitor {
    private:
        DigitalInOut one_wire_pin;
        int VoltageReading, CurrentReading;
        float Voltage, Current;
    public:
        BatteryMonitor(PinName P1): one_wire_pin(P1) {
            VoltageReading = 0;
            CurrentReading = 0;
            Voltage = 0.0;
            Current = 0.0;
        };
        void pollBattery(void){
            VoltageReading = ReadVoltage();
            Voltage = VoltageReading*0.00967;
            CurrentReading = ReadCurrent();
            Current = CurrentReading/6400.0;
        };

        float getBatteryVoltage(void){return Voltage;};
        float getBatteryCurrent(void){return Current;};
};

//this class is for calculating PID error as two PWM outputs.
class PIDSys {
    private:
        float error[3];
        float errorOuter[3];
        float output;
        float leftPWM, rightPWM;
        float A0,A1,A2;
        TCRT* S1;
        TCRT* S2;
        TCRT* S4;
        TCRT* S5;
    public:
        PIDSys(TCRT* s1, TCRT* s2, TCRT* s4, TCRT* s5): S1(s1),S2(s2),S4(s4),S5(s5)
        {
            reset();
            COEF();
        };
        void reset()
        {
            error[2] = 0;
            error[1] = 0;
            error[0] = 0;
            output = 0;
            leftPWM = 0.5f;
            rightPWM = 0.5f;
            A0 = 0;
            A1 = 0;
            A2 = 0;
        };
        void COEF(void)
        {
            A0 = GAIN_PROPORTIONAL + GAIN_INTEGRAL/SYS_OUTPUT_RATE + GAIN_DERIVATIVE*SYS_OUTPUT_RATE;
            A1 = -GAIN_PROPORTIONAL - 2*GAIN_DERIVATIVE*SYS_OUTPUT_RATE;
            A2 = GAIN_DERIVATIVE*SYS_OUTPUT_RATE;
        };
        void calculatePID(bool toggleAggressive)
        {
            if(toggleAggressive){
                errorOuter[2] = errorOuter[1];
                errorOuter[1] = errorOuter[0];
                errorOuter[0] = (S5->getSensorVoltage(false) - S1->getSensorVoltage(false));
                output = (output + A0*errorOuter[0] + A1*errorOuter[1] + A2*errorOuter[2])*GAIN_AGGRESSIVE;
            } else {
                error[2] = error[1];
                error[1] = error[0];
                error[0] = (S4->getSensorVoltage(false) - S2->getSensorVoltage(false));
                output = output + A0*error[0] + A1*error[1] + A2*error[2];
            };
            outputPWM();
        };
        void outputPWM(void)
        {
            if(output > 0)
            {
                leftPWM = BASE_DUTY + output; 
                rightPWM = BASE_DUTY - output;
            } else if (output < 0) {
                leftPWM = BASE_DUTY - output;
                rightPWM = BASE_DUTY + output;
            } else {
                leftPWM = ceil(BASE_DUTY);
                rightPWM = ceil(BASE_DUTY);
            };
        };

        float getLeftPWM(void){return leftPWM;};
        float getRightPWM(void){return rightPWM;};
};

//LCD display buffer. Pass string pointers to display in lines 1-3 on the LCD screen. Keep the strings under 23 bytes if possible
// call toScreen with the appropriate arguments to push anything to the LCD display - refresh rate sensitive (dont go above 15 Hz)
void toScreen(char* line1, char*  line2, char* line3,C12832* lcd){
    static char lastLine1[21] = "";
    static char lastLine2[21] = "";
    static char lastLine3[21] = "";
    if(strncmp(lastLine1,line1,20) != 0){
        strncpy(lastLine1,line1,20);
        lastLine1[20] = '\0';
        lcd->locate(0,0);
        lcd->printf(line1);
    };
    if(strncmp(lastLine2,line2,20) != 0){
        strncpy(lastLine2,line2,20);
        lastLine2[20] = '\0';
        lcd->locate(0,10);
        lcd->printf(line2);
    };
    if(strncmp(lastLine3,line3,20) != 0){
        strncpy(lastLine3,line3,20);
        lastLine3[20] = '\0';
        lcd->locate(0,20);
        lcd->printf(line3);
    };
};

char* batteryMonitorBuffer(BatteryMonitor* Batt){
    static char dspBuffer[20];
    sprintf(dspBuffer, "%.02f V          ", Batt->getBatteryVoltage());
    return dspBuffer;
};

char* sensorVoltageBuffer(TCRT* S1,TCRT* S2){
    static char dspBuffer[20];
    sprintf(dspBuffer, "%.02f      %.02f     ", S1->getSensorVoltage(true),S2->getSensorVoltage(true));
};

class JoystickISR {
    private:
        InterruptIn centreJoy;
        Timeout joyDebounce;
        bool toggleState;
    public:
    JoystickISR(PinName centre) : centreJoy(centre){
        toggleState = false;
        centreJoy.rise(callback(this,&JoystickISR::centreISR));
    };
    void toggleInput(){
        toggleState = !toggleState;
        if(toggleState){
            centreJoy.disable_irq();
        } else {
            centreJoy.enable_irq();
        };
    };
    void timerDebounce(){joyDebounce.attach(callback(this,&JoystickISR::toggleInput),0.3);};
    void centreISR(){
        toggleInput();
        timerDebounce();
        switch(ProgramState){
            case(starting)      : ProgramState = straightline;  break;
            case(straightline)  : ProgramState = stop;         break;
            case(stop)          : ProgramState = turnaround;    break;
            case(turnaround)    : ProgramState = starting;      break;
            default:break;
        };
    };
};

int main (void)
{
    JoystickISR joy(D4);
    PWMGen toMDB(PC_8,PC_6);
    BatteryMonitor Battery(PC_12);
    Encoder leftWheel(PB_14,PB_15);
    C12832 lcd(D11, D13, D12, D7, D10);
    TCRT S1(PA_0,TCRT_MAX_VDD) , S2(PA_1,TCRT_MAX_VDD) , S3(PA_4,TCRT_MAX_VDD) , S4(PB_0,TCRT_MAX_VDD), S5(PC_1,TCRT_MAX_VDD);
    PIDSys PID(&S1,&S2,&S4,&S5);

    Ticker sensorPollTicker;
    float sensorPollRate = 1.0/SENSOR_POLL_FREQ;
    sensorPollTicker.attach(callback(&TCRT::pollSensors),sensorPollRate);
    
    Timer outputUpdateTimer;
    outputUpdateTimer.start();
    int timedelay = (static_cast<int>(1000/SYS_OUTPUT_RATE)); //in ms

    bool straightLineStart = true;

    while(1)
    {
        switch (ProgramState){
            case (starting):{
                if(outputUpdateTimer.read_ms() >= timedelay){
                    outputUpdateTimer.reset();



                    toScreen("START STATE        ", batteryMonitorBuffer(&Battery), sensorVoltageBuffer(&S2, &S4), &lcd);
                };
            };
            case (straightline):{
                if(outputUpdateTimer.read_ms() >= timedelay){
                    outputUpdateTimer.reset();
                    toMDB.setPWMDuty(1.0f, 1.0f); //test

                    toScreen("STRAIGHT LINE       ", sensorVoltageBuffer(&S3, &S4), sensorVoltageBuffer(&S1, &S5), &lcd);
                    if(straightLineStart)
                    {
                        toMDB.begin();
                        straightLineStart = false;
                    };
                };
            };
            case (stop):{
                if(outputUpdateTimer.read_ms() >= timedelay){
                    outputUpdateTimer.reset();
                    toMDB.setPWMDuty(0.0f, 0.0f); //test

                    toScreen("STRAIGHT LINE       ", batteryMonitorBuffer(&Battery), "                       ", &lcd);
                };
            };
            case (turnaround):{
                
            };


        
        };
    };
};
