#include "TCRT.h"

TCRT *TCRT::sensors[SENSOR_AMOUNT] = {NULL}; // static member declaration 
int TCRT::sensorCount = 0;                   // static member declaration 

TCRT::TCRT(PinName Pin, PinName DarlingPin, double v) : sensorPin(Pin), DarlingtonPin(DarlingPin), VDD(v), rIndex(0), senseNorm(0)
{
    senseNormRolled[0] = 
    senseNormRolled[1] = 
    senseNormRolled[2] = 
    senseNormRolled[3] = 
    senseNormRolled[4] = 0;

    if (sensorCount < SENSOR_AMOUNT)
    {
        sensors[sensorCount++] = this;
    };
};

void TCRT::rollingPollAverage()
{
    senseNormRolled[rIndex % 5] = sensorPin.read();
    rIndex++;

    senseNorm = senseNormRolled[0] + 
                senseNormRolled[1] + 
                senseNormRolled[2] + 
                senseNormRolled[3] + 
                senseNormRolled[4]; 

    senseNorm /= 5;
}; 

// runs through all the polling once called. This is for synchronous polling between sensors since static is shared between all objects derived from TCRT
void TCRT::pollSensors(void)
{
    for (int i = 0; i < sensorCount; i++)
    {
        sensors[i]->rollingPollAverage();
    };
};

double TCRT::getSensorVoltage(bool Volt)
{
    return (Volt ? (senseNorm) * VDD : sensorPin.read());
};

void TCRT::turnSensorOn(void)
{
    DarlingtonPin = 1;
};

void TCRT::turnSensorOff(void)
{
    DarlingtonPin = 0;
};