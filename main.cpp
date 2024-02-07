#include "mbed.h"
#include "C12832.h"
#include "ds2781.h"
#include "QEI.h"
#include "BLE.h"
#include "mbed2/299/TARGET_NUCLEO_F401RE/TARGET_STM/TARGET_STM32F4/TARGET_NUCLEO_F401RE/PinNames.h"
#include "mbed2/299/drivers/AnalogIn.h"
#include "mbed2/299/drivers/Ticker.h"

#define SENSOR_AMOUNT 5
#define SENSOR_BUFFER 5 //Buffer Size
#define SENSOR_POLL_FREQ 1000 //Hz

#define SUBSYS_POLL_FREQ 200 //Hz
#define SUBSYS_BUFFER 5 //Buffer Size

#define GAIN_PROPORTIONAL 0.1
#define GAIN_INTEGRAL 0.1
#define GAIN_DERIVATIVE 0.1

#define SCREEN_REFRESH_RATE 15 //Buffer Size


typedef enum {starting,straightline,curve,stop,turnaround} pstate;
pstate ProgramState;

//TCRT class. Creates an object for individual sensors on the sensor array with rolling average (size 5) built in. 
//Provide the Pin (ADC) for the sensor and the voltage levels expected (if you want to scale to voltage instead of from 0.0 - 1.0)
//Use getSensorNorm() to get value of voltage (rolled average). 
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
            for(int i = 0;i<SENSOR_BUFFER;i++){senseNormRolled[i] = 0;};
            if(sensorCount < SENSOR_AMOUNT)
            {
                sensors[sensorCount++] = this; //appending individual sensor object pointers into the TCRT* pointer array to be used in static function pollSensors() later
            };
        };
        void rollingPollAverage()
        {
            senseNormRolled[rIndex % SENSOR_BUFFER] = ampNorm();
            rIndex++;
            senseNorm = 0;
            for(int i = 0;i < SENSOR_BUFFER;i++){senseNorm += senseNormRolled[i];};
            senseNorm /= SENSOR_BUFFER;
        };
        static void pollSensors(void) //runs through all the polling once called. This is for synchronous polling between sensors since static is shared between all objects derived from TCRT
        {
            for(int i = 0; i < sensorCount; i++) {
                sensors[i]->rollingPollAverage();
            };
        };
        float ampNorm(void){return (sensorPin.read());};
        float getSensorNorm(bool Volt){return (Volt? senseNorm*VDD : senseNorm);}; 
};

TCRT* TCRT::sensors[SENSOR_AMOUNT]; //static member declaration (must be outside class)
int TCRT::sensorCount = 0;  //static member declaration (must be outside class)

//THIS CLASS INCLUDES PID AND PWM OUTPUT. Output rate will be tied to another Ticker. DO NOT MODIFY THIS YET.
class PWMGen {
    private:
        float PWM_Internal_1, PWM_Internal_2;
        DigitalOut PWM_OUT_CHANNEL_1, PWM_OUT_CHANNEL_2;
        float IntegralBuffer[2], DerivativeBuffer[2];
    public:
        PWMGen(PinName P1, PinName P2): PWM_OUT_CHANNEL_1(P1),PWM_OUT_CHANNEL_2(P2){};
};

//use the one-wire-pin PC_12
class BatteryMonitor {
    private:
        DigitalInOut BatteryPin;
        int VoltageReading, CurrentReading;
        float Voltage, Current;
    public:
        BatteryMonitor(PinName P1): BatteryPin(P1) {
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

char* screenLine2Buffer(BatteryMonitor* Batt){
    static char dspBuffer[20];
    sprintf(dspBuffer, "%02f           ", Batt->getBatteryVoltage());
    return dspBuffer;
};

int main (void)
{
    C12832 lcd(D11, D13, D12, D7, D10);
    float sensorPollRate = 1.0/SENSOR_POLL_FREQ;
    Ticker sensorPollTicker;
    sensorPollTicker.attach(callback(&TCRT::pollSensors),sensorPollRate);
    BatteryMonitor Battery(PC_12);

    Timer screenUpdateTimer;
    screenUpdateTimer.start();
    int refreshrate = 15; //Hz
    int timedelay = (static_cast<int>(1000/refreshrate)); //in ms
    while(1)
    {
        switch (ProgramState){
            case (starting):{
                if(screenUpdateTimer.read_ms() >= timedelay){
                    screenUpdateTimer.reset();
                    toScreen("Battery Voltage:       ", screenLine2Buffer(&Battery), "                       ", &lcd);
                };
            };
            case (straightline):{
                
            };
            case (curve):{
                
            };
            case (stop):{
                
            };
            case (turnaround):{
                
            };


        
        };
    };
};