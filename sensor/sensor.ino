#include "LoRaWan_APP.h"
#include "GPS_Air530Z.h"
#include "Arduino.h"

#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <TimeLib.h>
#include "DHT.h"
#include "mics.hpp"
#include "pms.hpp"

Air530ZClass GPS;
SSD1306Wire tft(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10); // addr , freq , SDA, SCL, resolution , rst
const uint8_t * font = ArialMT_Plain_10;

#define DHTPIN GPIO4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define RF_FREQUENCY 868000000
#define TX_OUTPUT_POWER 17

#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE 2
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define SYNC_WORD 0x1424

#define RX_TIMEOUT_VALUE 2000

#define SERVER_ID 0xAA;

const uint32_t COLORS[] = { 0x005005,0x407000,0x601000,0x500000,0x700010 };

const uint8_t ID = 0xFF;

struct NodePacket {
    uint8_t dest;
    uint8_t emitter;
    uint8_t lvl;
    time_t unix;
    float lat, lng;
    float hum, temp;
    float co, no2, nh3;
    uint16_t pm25, pm10;
};

struct GatewayPacket {
    uint8_t dest;
    uint8_t emitter;
    time_t unix;
};

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum {
    LOWPOWER,
    RX,
    TX
} 
States_t;

int16_t txNumber;
volatile States_t state;
volatile bool sleepMode = false;
int16_t Rssi, rxSize;

volatile uint32_t color;

volatile bool calibrated = false;
volatile bool gpsNeeded = true;

void setup() {
    Serial.begin(115200);
    pmsSerial.begin(9600);

    tft.init();
    tft.setFont(font);

    VextON();
    delay(10);
    GPS.begin();
    dht.begin();

    txNumber = 0;
    Rssi = 0;

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnRxTimeout;

    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetSyncWord(SYNC_WORD);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

    Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
    state = TX;

    pinMode(GPIO5, INPUT);
    attachInterrupt(digitalPinToInterrupt(GPIO5), calibrateTrigger, RISING);
}

void calibrateTrigger(){
    gpsNeeded = !gpsNeeded;
}

void loop() {
    if(!calibrated){
        println("Calibrating...");
        calibrateSensors();
        calibrated = true;
    }

    switch (state) {
    case TX:
        delay(10000);
        txNumber++;

        NodePacket p;
        p.dest = SERVER_ID;
        p.emitter = ID;
        readGPS(&p);
        readDHT(&p);
        readMICS(&p);
        readPMS(&p);
        changeColor(&p);

        showPacketInfo(p);
        turnOnRGB(color, 0);

        byte mem[sizeof(p)];
        memcpy(mem, &p, sizeof(p));
        Radio.Send(mem, sizeof(p));
        state = LOWPOWER;
        break;
    case RX:
		Serial.println("into RX mode");
		Radio.Rx(RX_TIMEOUT_VALUE);
		state=LOWPOWER;
		break;
    case LOWPOWER:
        lowPowerHandler();
        break;
    default:
        break;
    }
    Radio.IrqProcess();
}

void println(String msg) {
    tft.clear();
    Serial.println(msg);
    tft.drawString(0, 0, msg);
    tft.display();
}

void print(String msg, int l=0) {
    Serial.print(msg);
    tft.drawString(0, l, msg);
    tft.display();
}

void calibrateSensors(){
    // Circular buffer for the measurements
    uint16_t bufferNH3[MICS_CALIBRATION_SECONDS];
    uint16_t bufferRED[MICS_CALIBRATION_SECONDS];
    uint16_t bufferOX[MICS_CALIBRATION_SECONDS];

    // Pointers for the next element in the buffer
    uint8_t pntrNH3 = 0, pntrRED = 0, pntrOX = 0;

    // Current floating sum in the buffer
    uint16_t fltSumNH3 = 0, fltSumRED = 0, fltSumOX = 0;

    // Current measurements;
    uint16_t curNH3, curRED, curOX;

    // Flag to see if the channels are stable
    bool NH3Stable = false;
    bool REDStable = false;
    bool OXStable = false;
    bool GPSLocStable = false;
    bool GPSTimeStable = false;
    bool GPSDateStable = false;

    bool gpsOK = false;
    bool micsOK = false;

    // Initialize buffer
    for (int i = 0; i < MICS_CALIBRATION_SECONDS; ++i) {
        bufferNH3[i] = 0;
        bufferRED[i] = 0;
        bufferOX[i] = 0;
    }

    do {
        // Wait a second
        delay(1000);

        //Read GPS
        while (GPS.available() > 0) {
            GPS.encode(GPS.read());
        }
        
        // Read new resistance for NH3
        unsigned long rs = 0;
        delay(50);
        for (int i = 0; i < 3; i++) {
            delay(1);
            rs += analogRead(NH3PIN);
        }
        curNH3 = rs/3;

        // Read new resistance for CO
        rs = 0;
        delay(50);
        for (int i = 0; i < 3; i++) {
            delay(1);
            rs += analogRead(COPIN);
        }
        curRED = rs/3;

        // Read new resistance for NO2
        rs = 0;
        delay(50);
        for (int i = 0; i < 3; i++) {
            delay(1);
            rs += analogRead(OXPIN);
        }
        curOX = rs/3;

        // Update floating sum by subtracting value about to be overwritten and adding the new value.
        fltSumNH3 = fltSumNH3 + curNH3 - bufferNH3[pntrNH3];
        fltSumRED = fltSumRED + curRED - bufferRED[pntrRED];
        fltSumOX = fltSumOX + curOX - bufferOX[pntrOX];

        // Store new measurement in buffer
        bufferNH3[pntrNH3] = curNH3;
        bufferRED[pntrRED] = curRED;
        bufferOX[pntrOX] = curOX;

        // Determine new state of flags
        NH3Stable = abs(fltSumNH3 / MICS_CALIBRATION_SECONDS - curNH3) < MICS_CALIBRATION_DELTA;
        REDStable = abs(fltSumRED / MICS_CALIBRATION_SECONDS - curRED) < MICS_CALIBRATION_DELTA;
        OXStable = abs(fltSumOX / MICS_CALIBRATION_SECONDS - curOX) < MICS_CALIBRATION_DELTA;
        GPSLocStable = GPS.location.isValid() && GPS.location.lat() > 10.0;
        GPSTimeStable = GPS.time.isValid();
        GPSDateStable = GPS.date.isValid();

        gpsOK = !gpsNeeded || GPSLocStable && GPSTimeStable && GPSDateStable;
        micsOK = NH3Stable && REDStable && OXStable;

        // Advance buffer pointer
        pntrNH3 = (pntrNH3 + 1) % MICS_CALIBRATION_SECONDS ;
        pntrRED = (pntrRED + 1) % MICS_CALIBRATION_SECONDS;
        pntrOX = (pntrOX + 1) % MICS_CALIBRATION_SECONDS;

        //SHOW INFO
        char txt[512];
        int i = sprintf(txt, "Loc:%s %d.%05d %d.%05d\n", GPSLocStable? "OK":"...", (int)GPS.location.lat(), fracPart(GPS.location.lat(), 5), (int)GPS.location.lng(), fracPart(GPS.location.lng(), 5));
        i += sprintf(txt+i, "Date: %02d-%02d-%02d %s\n", GPS.date.year(), GPS.date.month(), GPS.date.day(), GPSDateStable? "DONE":"...");
        i += sprintf(txt+i, "Time: %02d:%02d:%02d %s\n", GPS.time.hour(), GPS.time.minute(), GPS.time.second(), GPSTimeStable? "DONE":"...");
        i += sprintf(txt+i, "NO2:%s ", OXStable? "DONE":"..");
        i += sprintf(txt+i, "NH3:%s\n", NH3Stable? "DONE":"...");
        i += sprintf(txt+i, "CO:%s\n", REDStable? "DONE":"...");
        println(txt);

        Serial.print(String{NH3Stable});
        Serial.print(String{REDStable});
        Serial.print(String{OXStable});
        Serial.print(String{GPSLocStable});
        Serial.print(String{GPSTimeStable});
        Serial.print(String{GPSDateStable});
    }
    while (!gpsOK || !micsOK);

    println("DONE!");

    NH3baseR = fltSumNH3 / MICS_CALIBRATION_SECONDS;
    REDbaseR = fltSumRED / MICS_CALIBRATION_SECONDS;
    OXbaseR = fltSumOX / MICS_CALIBRATION_SECONDS;

    setTime(GPS.time.hour(), GPS.time.minute(), GPS.time.second(), GPS.date.day(), GPS.date.month(), GPS.date.year());
}

void OnTxDone(){
	state=RX;
}

void OnTxTimeout(){
    Radio.Sleep( );
    state=RX;
}

void OnRxTimeout(){
    Radio.Sleep();
    state = TX;
    Serial.println("Into TX Mode...");
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr){
    Rssi=rssi;
    rxSize=size;
    GatewayPacket p;
    memcpy(&p, payload, size );
    if(p.dest != ID){
        state = TX;
        return;
    }    
    turnOnRGB(0x0000FF, 0);

    Radio.Sleep( );
    state=TX;
    delay(50);

    turnOnRGB(color, 0);
}

void VextON(){
    pinMode(Vext,OUTPUT);
    digitalWrite(Vext, LOW);
}

void VextOFF(){
    pinMode(Vext,OUTPUT);
    digitalWrite(Vext, HIGH);
}

int fracPart(double val, int n){
    return abs((int)((val - (int)(val))*pow(10,n)));
}

void readGPS(NodePacket* p){
    while (GPS.available() > 0) {
        GPS.encode(GPS.read());
    }

    p->lat = GPS.location.lat();
    p->lng = GPS.location.lng();

    p->unix = now();
}

void readDHT(NodePacket* p){
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    p->hum = h;
    p->temp = t;

    if (isnan(h) || isnan(t)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
    }
}

void readMICS(NodePacket* p){
    float co = measureMICS(CO);
    float nh3 = measureMICS(NH3);
    float no2 = measureMICS(NO2);

    p->co = co;
    p->nh3 = nh3;
    p->no2 = no2;
}

void readPMS(NodePacket* p){
    while(!pms.read(pmsData));

    p->pm25 = pmsData.PM_AE_UG_2_5;
    p->pm10 = pmsData.PM_AE_UG_10_0;
}

void showPacketInfo(const NodePacket& p){
    char str[512];
    int i = 0;

    i += sprintf(str+i, "%02d-%02d-%02d %02d:%02d:%02d\n", year(p.unix), month(p.unix), day(p.unix), hour(p.unix), minute(p.unix), second(p.unix));
    i += sprintf(str+i, "LAT:%d.%04d LON:%d.%04d\n", (int)p.lat, fracPart(p.lat,4), (int)p.lng, fracPart(p.lng, 4));
    i += sprintf(str+i, "Temp:%d.%02d Hum:%d.%02d\n", (int)p.temp, fracPart(p.temp, 2), (int)p.hum, fracPart(p.hum, 2));
    i += sprintf(str+i, "A:%d.%02d C:%d.%02d N:%d.%02d\n", (int)p.nh3, fracPart(p.nh3, 2), (int)p.co, fracPart(p.co, 2), (int)p.no2, fracPart(p.no2, 2));
    i += sprintf(str+i, "PM2.5: %u   PM10: %u\n", p.pm25, p.pm10);

    println(str);
}

void changeColor(NodePacket* p){
    int ans = INT_MIN;
    ans = max(no2Level(p->no2), ans);
    ans = max(nh3Level(p->nh3), ans);
    ans = max(coLevel(p->co), ans);
    ans = max(pm25Level(p->pm25), ans);
    ans = max(pm10Level(p->pm10), ans);

    p->lvl = ans+1;

    color = COLORS[ans];
}