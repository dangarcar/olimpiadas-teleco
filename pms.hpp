#ifndef PMS_HPP
#define PMS_HPP

#include <Arduino.h>

//Salubridad del aire en PM2.5 0-4
int pm25Level(uint16_t pm25){
    if(pm25 < 10) return 0;
    if(pm25 < 25) return 1;
    if(pm25 < 50) return 2;
    if(pm25 < 75) return 3;
    return 4;
}

//Salubridad del aire en PM10 0-4
int pm10Level(uint16_t pm10){
    if(pm10 < 20) return 0;
    if(pm10 < 50) return 1;
    if(pm10 < 100) return 2;
    if(pm10 < 150) return 3;
    return 4;
}

#endif //PMS_HPP