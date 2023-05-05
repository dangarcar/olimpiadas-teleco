#include "SSD1306Wire.h"
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include "FS.h"
#include "SD.h"
#include <esp_task_wdt.h>

#include "sql.hpp"
#include "wifi.hpp"

//Pines
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RST 23
#define LORA_DIO0 26

#define HSPI_SCLK 14
#define HSPI_MISO 2
#define HSPI_MOSI 15
#define HSPI_CS 13

#define LORA_BAND 868E6
#define LORA_SPREADING_FACTOR 10

#define ID 0xAA;

struct Packet{
    uint8_t dest;
    uint8_t emmiter;
    time_t unix;
};

struct NodePacket : Packet {
    float lat, lng;
    float hum, temp;
    float co, no2, nh3;
    uint16_t pm25, pm10;
    uint8_t lvl;
};

struct GatewayPacket : Packet {

};

//TFT
SSD1306Wire tft(0x3c, SDA, SCL);
const uint8_t* font = ArialMT_Plain_10;

//SD Serial
SPIClass * hspi = nullptr;

//SQLite database
SQLite* sql = nullptr;

void setup()
{
    esp_task_wdt_init(30, false); //DANGER! Disable the watchdog timer of the background tasks

    hspi = new SPIClass(HSPI);
    hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS); //SCLK, MISO, MOSI, SS
    pinMode(HSPI_CS, OUTPUT); //HSPI SS

    Serial.begin(115200);

    tft.init();
    tft.setFont(font);

    if(!SD.begin(HSPI_CS, *hspi)){
        println("SD Card Error");
        return;
    }

    //CREATE DATABASE
    sql = new SQLite(DB_FILE);
    sql->exec(CREATE_SQL);

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_BAND)) {
        println("Starting LoRa failed!");
        while (1);
    }

    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setCodingRate4(6);

    wifi = initWiFi(10000);

    pinMode(25, OUTPUT);
    digitalWrite(25, LOW);

    if(wifi){
        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(SD, "/index.html", "text/html");
        });

        server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request){
            SD.remove(JSON_TEMP_FILE);
            sql->jsonQuery("SELECT ID,Time,Lon,Lat,Temp,Hum,NO2,CO,NH3,PM25,PM10,LVL FROM Data ORDER BY ID DESC", 2000);
            request->send(SD, JSON_TEMP_FILE, "application/json");
        });

        server.on("/last", HTTP_GET, [](AsyncWebServerRequest* request){
            SD.remove(JSON_TEMP_FILE);
            sql->jsonQuery("SELECT ID,Time,Lon,Lat,Temp,Hum,NO2,CO,NH3,PM25,PM10,LVL FROM Data ORDER BY Time DESC LIMIT 1;");
            request->send(SD, JSON_TEMP_FILE, "application/json");
        });

        server.serveStatic("/", SD, "/");

        server.begin();

        char str[128];
        sprintf(str, "Listening at %s\n", WiFi.localIP().toString().c_str());
        println(str);
    }
    else {
        println("Couldn't connect to WiFi");
    }
}

void loop()
{
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        digitalWrite(25, HIGH);

        NodePacket p;
        char mem[sizeof(p)];
        for(int i=0; i<sizeof(p); i++){
            mem[i] = LoRa.read();
        }
        memcpy(&p, mem, sizeof(p));
        
        storePacket(p);
        sendResponsePacket(p);
        showPacketInfo(p);

        digitalWrite(25, LOW);
    }
}

void storePacket(const NodePacket& p){
    char sqltxt[512];
    sprintf(sqltxt, "INSERT INTO 'Data'('Time','Lat','Lon','Temp','Hum','NO2','CO','NH3','PM25','PM10', 'LVL') "
    "VALUES (%lu, %f, %f, %f, %f, %f, %f, %f, %u, %u, %u);", 
    p.unix, p.lat, p.lng, p.temp, p.hum, isinf(p.no2)? 0.0:p.no2, isinf(p.co)? 0.0:p.co, isinf(p.nh3)? 0.0:p.nh3, p.pm25, p.pm10, p.lvl);
    int rc = sql->exec(sqltxt);
    if(rc != SQLITE_OK)
        Serial.printf("Error %d, %s\n", sql->errco, sql->errmsg);
    else
        Serial.println("Data inserted");
}

void showPacketInfo(const NodePacket& p){
    char str[512];
    int i = sprintf(str, "%s:   %02d:%02d:%02d\n", WiFi.localIP().toString().c_str(), hour(p.unix), minute(p.unix), second(p.unix));
    i += sprintf(str + i, "Hum:%d.%02d Temp:%d.%02d\n", (int)p.hum, fracPart(p.hum, 2), (int)p.temp, fracPart(p.hum, 2));
    //i += sprintf(str + i, "%02d-%02d-%02d %02d:%02d:%02d\n", year(p.unix), month(p.unix), day(p.unix), hour(p.unix), minute(p.unix), second(p.unix));
    i += sprintf(str+i, "A:%d.%02d C:%d.%02d N:%d.%02d\n", (int)p.nh3, fracPart(p.nh3, 2), (int)p.co, fracPart(p.co, 2), (int)p.no2, fracPart(p.no2, 2));
    i += sprintf(str+i, "PM2.5:%u PM10:%u\n", p.pm25, p.pm10);

    println(str);
}

void sendResponsePacket(const NodePacket& p){
    GatewayPacket g;
    g.dest = p.emmiter;
    g.emmiter = ID;
    g.unix = p.unix;

    LoRa.idle();
    LoRa.beginPacket();
    char mem[sizeof(g)];
    memcpy(mem, &g, sizeof(g));
    for(auto c : mem)
        LoRa.write(c);
    LoRa.endPacket(true);
}

void println(String msg){
    tft.clear();
    Serial.println(msg);
    tft.drawString(0, 0, msg);
    tft.display();
}

inline int fracPart(double val, int n){
    return abs((int)((val - (int)(val))*pow(10,n)));
}