#ifndef TEST_HPP
#define TEST_HPP

#define NO2_PPM_TO_UGM3 188.1

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

#endif //TEST_HPP