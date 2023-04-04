#include "LoRaWan_APP.h"
#include "GPS_Air530Z.h"
#include "Arduino.h"

#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <TimeLib.h>

Air530ZClass GPS;
SSD1306Wire tft(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10); // addr , freq , SDA, SCL, resolution , rst
const uint8_t * font = ArialMT_Plain_10;

//WORKS DONT TOUCH!

#define RF_FREQUENCY 868000000

#define TX_OUTPUT_POWER 20

#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 10
#define LORA_CODINGRATE 2
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define SYNC_WORD 0x1424

#define RX_TIMEOUT_VALUE 2000

const int ID = 0xFF;

struct Packet{
    int dest;
};

struct NodePacket : Packet {
    uint16_t year;
    uint8_t month, day;
    uint8_t hour, min, sec;

    float lat, lng;
    //float spd;

    uint32_t t;
};

struct GatewayPacket : Packet {
    int rssi;
    int snr;
    uint32_t dist;
    uint32_t t;
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

void setup() {
    VextON();
    delay(10);
    Serial.begin(115200);
    GPS.begin();

    tft.init();
    tft.setFont(font);

    txNumber = 0;
    Rssi = 0;

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnUserButton;

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

    waitUntilGPSReady();
}



void loop() {
    switch (state) {
    case TX:
        delay(3000);
        txNumber++;
        NodePacket p;
        p = generateGPSPacket();

        tft.clear();
        showGPSInfo(p);

        turnOnRGB(COLOR_SEND, 0);

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

void waitUntilGPSReady(){
    unsigned s = millis();
    while(!GPS.location.isValid()){
        delay(100);
        tft.clear();        
        showGPSInfo(generateGPSPacket());
        //print("Waiting for GPS position...", 39);
    }
    char txt[256];
    sprintf(txt, "GPS ready!\n Elapsed: %d s", (millis()-s)/1000);
    println(txt);     
}

void showGPSInfo(NodePacket p){
    char str[512];
    int i;
    i = 0;
    i += sprintf(str +i, "%02d-%02d-%02d %02d:%02d:%02d\n", p.year, p.month, p.day, p.hour, p.min, p.sec);

    i += sprintf(str +i, "LAT:%d.%04d LON:%d.%04d\n", (int)p.lat, fracPart(p.lat,4), (int)p.lng, fracPart(p.lng, 4));

    //i += sprintf(str +i, "VEL : %d.%02d\n", (int)p.spd, fracPart(p.spd,2));
    print(str);
}

void OnTxDone(){
	//Serial.print("TX done......");
	turnOnRGB(0,0);
	state=RX;
}

void OnTxTimeout(){
    Radio.Sleep( );
    //Serial.print("TX Timeout......");
    state=RX;
}

void OnUserButton(){
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
    turnOnRGB(getColor(p), 0);

    uint32_t curr = getMillis();
    uint32_t ping = curr - p.t;

    char str[256];
    sprintf(str, "Dist: %um\n RSSI: %d, Ping: %dms\nrssi: %d, snr: %d", p.dist, p.rssi, ping, rssi, snr);
    print(str, 26);

    Radio.Sleep( );

    state=TX;
}

uint32_t getColor(GatewayPacket p){
    uint32_t ans = COLOR_RECEIVED;
    int snr = p.snr;
    int rssi = p.rssi;

    if(snr < -20 || rssi < -120) //Rojo
        ans = COLOR_SEND;
    else if(rssi < -80) //Amarillo
        ans = COLOR_RXWINDOW2;

    return ans;
}

uint32_t getMillis(){
    return millis();
    /*setTime(GPS.time.hour(), GPS.time.minute(), GPS.time.second(), GPS.date.day(), GPS.date.month(), GPS.date.year());

    uint64_t t = now();
    t *= 1000LL;
    t += (uint64_t) GPS.time.centisecond() * 10LL;
    return t;*/
}

NodePacket generateGPSPacket(){
    NodePacket p;
    p.dest = 0xAA;

    while (GPS.available() > 0) {
        GPS.encode(GPS.read());
    }

    p.year = GPS.date.year();
    p.month = GPS.date.month();
    p.day = GPS.date.day();

    p.hour = GPS.time.hour();
    p.min = GPS.time.minute();
    p.sec = GPS.time.second();

    p.lat = GPS.location.lat();
    p.lng = GPS.location.lng();
    //p.spd = GPS.speed.kmph();

    p.t = getMillis();

    return p;
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}

int fracPart(double val, int n){
    return abs((int)((val - (int)(val))*pow(10,n)));
}
