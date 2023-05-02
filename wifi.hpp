#ifndef WIFI_HPP
#define WIFI_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include "SD.h"
#include "FS.h"

#define CREDENTIALS_FILE "/.config"

//AsyncWebServer on port 80
AsyncWebServer server(80);
bool wifi = false;

struct NetworkCredentials{
    char ssid[256];
    char password[256];
} creds;

void getSDWifiCredentials(){
    File file = SD.open(CREDENTIALS_FILE);
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, file);
    if (error)
        Serial.println(F("Failed to read file"));

    strncpy(creds.ssid, doc["ssid"], 256);
    strncpy(creds.password, doc["password"], 256);

    file.close();
}

bool initWiFi(int timeout) {
    getSDWifiCredentials();

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*) creds.ssid, (const char*) creds.password);
    Serial.print("Connecting to WiFi ..");
    auto to = millis();
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
        if(millis() - to > timeout) 
            return false;
    }
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
    Serial.print(" IP:  ");
    Serial.println(WiFi.localIP());
    return true;
}

#endif //WIFI_HPP