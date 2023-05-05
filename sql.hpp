#ifndef SQL_HPP
#define SQL_HPP

#include <Arduino.h>
#include <sqlite3.h>
#include <ArduinoJson.h>
#include "SD.h"
#include "FS.h"

const char* HEADER = "{\"type\": \"FeatureCollection\",\"crs\": {\"type\": \"name\",\"properties\": {\"name\": \"urn:ogc:def:crs:OGC:1.3:CRS84\"}},\"features\": [";
const char* END = "]}";

const char* JSON_TEMP_FILE = "/temp.json";
const char* TXT_TEMP_FILE = "/temp.txt";
const char* DB_FILE = "/sd/data.db";
const char* CREATE_SQL = "CREATE TABLE IF NOT EXISTS 'Data' ("
            "'ID'	INTEGER,"
            "'Time'	INTEGER NOT NULL,"
            "'Lat'	REAL NOT NULL,"
            "'Lon'	REAL NOT NULL,"
            "'Temp'	REAL,"
            "'Hum'	REAL,"
            "'NO2'  REAL,"
            "'CO'   REAL,"
            "'NH3'  REAL,"
            "'PM25' INTEGER,"
            "'PM10' INTEGER,"
            "'LVL' INTEGER,"
            "PRIMARY KEY('ID' AUTOINCREMENT)"
            ");";

static int simpleCallback(void *data, int argc, char **argv, char **azColName){
    return 0;
}

class SQLite {
    sqlite3 *db;
    bool open;
public:
    int errco;
    const char* errmsg;

    SQLite(const char* file){
        sqlite3_initialize();

        if(sqlite3_open(file, &db) != SQLITE_OK){
            errmsg = sqlite3_errmsg(db);
        }
        open = true;
    }

    ~SQLite(){
        this->close();
    }

    int close(){
        errco = SQLITE_OK;
        if(open){
            errco = sqlite3_close(db);
            if(errco == SQLITE_OK){
                open = false;              
            }
        }
        return errco;    
    }

    //Ejecuta el statement de la string sin retornar nada
    int exec(const char* sql){
        errco = sqlite3_exec(db, sql, simpleCallback, nullptr, (char**)&errmsg);
        if (errco != SQLITE_OK) {
            sqlite3_free((void*)errmsg);
        }
        return errco;
    }

    //Ejecuta el statement de la string y guarda la respuesta en un archivo /temp.json de la SD
    int jsonQuery(const char* sql);
    int jsonQuery(const char* sql, int limit, int step=128);
    int txtQuery(const char* sql);
    int rows();
};

struct FileN {
    File* f;
    int n;
};

static int jsonCallback(void *data, int argc, char **argv, char **azColName){
    FileN* fn = (FileN*)data;
    File* file = fn->f;
    DynamicJsonDocument doc(1024);

    doc["type"] = "Feature";
    JsonObject props = doc.createNestedObject("properties");
    JsonObject geom = doc.createNestedObject("geometry");
    geom["type"] = "Point";
    JsonArray coords = geom.createNestedArray("coordinates");
    
    for (int i = 0; i<argc; i++){
        if(strcmp(azColName[i], "Lat") == 0 || strcmp(azColName[i], "Lon") == 0){
            coords.add(atof(argv[i]));
        }
        else{
            props[azColName[i]] = argv[i]? atof(argv[i]) : 0;
        }
    }

    coords.add(0.0);

    if(fn->n != 0)
        file->print(",");
    fn->n++;
    serializeJson(doc, *file);

    return 0;
}

static int txtCallback(void *data, int argc, char **argv, char **azColName){
    FileN* fn = (FileN*)data;
    File* file = fn->f;
    
    for (int i = 0; i<argc; i++){
        file->println(argv[i]);
    }

    return 0;
}

int SQLite::jsonQuery(const char* s, int limit, int step){
    File file = SD.open(JSON_TEMP_FILE, FILE_WRITE);
    file.print(HEADER);
    FileN fn {&file, 0};

    int n = limit/step +1;
    int offset = 0;
    for(int i=0; i<n; i++){
        char sql[256];
        sprintf(sql, "%s LIMIT %d OFFSET %d;", s, step, offset);
        errco = sqlite3_exec(db, sql, jsonCallback, (void*)&fn, (char**)&errmsg);
        if (errco != SQLITE_OK) {
            Serial.println("Error");
            sqlite3_free((void*)errmsg);
        }
        offset += step;
    }

    file.print(END);
    file.close();
    return errco;
}

int SQLite::jsonQuery(const char* sql){
    File file = SD.open(JSON_TEMP_FILE, FILE_WRITE);
    file.print(HEADER);
    FileN fn {&file, 0};

    errco = sqlite3_exec(db, sql, jsonCallback, (void*)&fn, (char**)&errmsg);
    if (errco != SQLITE_OK) {
        sqlite3_free((void*)errmsg);
    }

    file.print(END);
    file.close();
    return errco;
}

int SQLite::rows(){
    int n = 0;
    errco = sqlite3_exec(db, "SELECT COUNT(*) FROM Data", 
    [](void* data, int argc, char** argv, char** azColName) -> int {
        int* n = (int*)data;
        String s = argv[0];
        *n = s.toInt();
        return 0;
    }, 
    &n, (char**)&errmsg);
    if (errco != SQLITE_OK) {
        sqlite3_free((void*)errmsg);
    }
    return n;
}

int SQLite::txtQuery(const char* sql){
    File file = SD.open(TXT_TEMP_FILE, FILE_WRITE);
    FileN fn {&file, 0};
    errco = sqlite3_exec(db, sql, txtCallback, (void*)&fn, (char**)&errmsg);
    if (errco != SQLITE_OK) {
        sqlite3_free((void*)errmsg);
    }
    file.close();
    return errco;
}

#endif //SQL_HPP