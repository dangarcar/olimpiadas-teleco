#ifndef MICS_HPP
#define MICS_HPP

#include <math.h>
#include <float.h>
#include <Arduino.h>

#define NH3PIN ADC1
#define COPIN ADC2
#define OXPIN ADC3

#define MICS_CALIBRATION_SECONDS 15 //0-64
#define MICS_CALIBRATION_DELTA 3 //1-10

#define NO2_PPM_TO_UGM3 188.1

enum Channel {
  CH_NH3, CH_RED, CH_OX
};

enum Gas {
    CO, NO2, NH3, C3H8, C4H10, CH4, H2, C2H5OH
};

uint16_t NH3baseR;
uint16_t REDbaseR;
uint16_t OXbaseR;

/**
   Requests the current resistance for a given channel
   from the sensor. The value is an ADC value between
   0 and 4096.

   @param channel
          The channel to read the base resistance from.
   @return The unsigned 16-bit base resistance
           of the selected channel.
*/
uint16_t getResistance(Channel channel) {
    unsigned long rs = 0;
    int counter = 0;

    switch (channel) {
        case CH_NH3:
        for(int i = 0; i < 100; i++) {
            rs += analogRead(NH3PIN);
            counter++;
            delay(2);
        }
        return rs/counter;
        case CH_RED:
        for(int i = 0; i < 100; i++) {
            rs += analogRead(COPIN);
            counter++;
            delay(2);
        }
        return rs/counter;
        case CH_OX:      
        for(int i = 0; i < 100; i++) {
            rs += analogRead(OXPIN);
            counter++;
            delay(2);
        }
        return rs/counter;

    }

    return 0;
}

uint16_t getBaseResistance(Channel channel) {
     switch (channel) {
       case CH_NH3:
         return NH3baseR;
       case CH_RED:
         return REDbaseR;
       case CH_OX:
         return OXbaseR;
     }
  return 0;
}

/**
   Calculates the current resistance ratio for the given channel.

   @param channel
          The channel to request resistance values from.
   @return The floating-point resistance ratio for the given channel.
*/
float getCurrentRatio(Channel channel) {
  float baseResistance = (float) getBaseResistance(channel);
  float resistance = (float) getResistance(channel);

  return resistance / baseResistance * (4095.0 - baseResistance) / (4095.0 - resistance);
  

  return -1.0;
}

/**
   Measures the gas concentration in ppm for the specified gas.

   @param gas
          The gas to calculate the concentration for.
   @return The current concentration of the gas
           in parts per million (ppm).
*/
float measureMICS(Gas gas) {
  float ratio;
  float c = 0;

  switch (gas) {
    case CO:
      ratio = getCurrentRatio(CH_RED);
      c = pow(ratio, -1.179) * 4.385;
      break;
    case NO2:
      ratio = getCurrentRatio(CH_OX);
      //c = pow(ratio, 1.007) / 6.855;
      c = pow(ratio, 1.007) / 6.855;
      break;
    case NH3:
      ratio = getCurrentRatio(CH_NH3);
      c = pow(ratio, -1.67) / 1.47;
      break;
    case C3H8:
      ratio = getCurrentRatio(CH_NH3);
      c = pow(ratio, -2.518) * 570.164;
      break;
    case C4H10:
      ratio = getCurrentRatio(CH_NH3);
      c = pow(ratio, -2.138) * 398.107;
      break;
    case CH4:
      ratio = getCurrentRatio(CH_RED);
      c = pow(ratio, -4.363) * 630.957;
      break;
    case H2:
      ratio = getCurrentRatio(CH_RED);
      c = pow(ratio, -1.8) * 0.73;
      break;
    case C2H5OH:
      ratio = getCurrentRatio(CH_RED);
      c = pow(ratio, -1.552) * 1.622;
      break;
  }

  return isnan(c) ? -1 : c;
}

//Salubridad del aire en CO 0-4
int coLevel(float co){
    if(co < 5) return 0;
    if(co < 9) return 1;
    if(co < 20) return 2;
    if(co < 50) return 3;
    return 4;
}

//Salubridad del aire en NH3 0-4
int nh3Level(float nh3){
    if(nh3 < 0.8) return 0;
    if(nh3 < 1.2) return 1;
    if(nh3 < 1.8) return 2;
    if(nh3 < 2.6) return 3;
    return 4;
}

//Salubridad del aire en NO2 0-4
int no2Level(float no2){
    no2 *= NO2_PPM_TO_UGM3;
    if(no2 < 40) return 0;
    if(no2 < 120) return 1;
    if(no2 < 230) return 2;
    if(no2 < 340) return 3;
    return 4;
}

#endif //MICS_HPP