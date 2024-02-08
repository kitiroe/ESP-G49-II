#include "TCRT.h"

TCRT* TCRT::sensors[SENSOR_AMOUNT] = {NULL}; //static member declaration (must be outside class)
int TCRT::sensorCount = 0;  //static member declaration (must be outside class)

TCRT::TCRT(PinName Pin, float v): sensorPin(Pin), VDD(v), rIndex(0), senseNorm(0)
{
    for(int i = 0;i<SENSOR_BUFFER;i++){ senseNormRolled[i] = 0; };
    if(sensorCount < SENSOR_AMOUNT){ sensors[sensorCount++] = this; };
};
        
void TCRT::rollingPollAverage()
{
    senseNormRolled[rIndex % SENSOR_BUFFER] = ampNorm();
    rIndex++;
    senseNorm = 0;
    for(int i = 0;i < SENSOR_BUFFER;i++){senseNorm += senseNormRolled[i];};
    senseNorm /= SENSOR_BUFFER;
};

//runs through all the polling once called. This is for synchronous polling between sensors since static is shared between all objects derived from TCRT
void TCRT::pollSensors(void) 
{ 
    for(int i = 0; i < sensorCount; i++) 
    {
        sensors[i]->rollingPollAverage(); 
    };
}; 

float TCRT::ampNorm(void)
{
    return (sensorPin.read());
};

float TCRT::getSensorVoltage(bool Volt)
{
    return (Volt? senseNorm*VDD : senseNorm);
}; 