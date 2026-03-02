/**
 * config_manager.h - Gestión de configuración persistente con LittleFS + JSON
 */
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "TZinfo.h"

class ConfigManager {
public:
    AppConfig config;
    
    ConfigManager() {
        setDefaults();
    }
    
    void setDefaults() {
        strlcpy(config.wifiSSID, "", sizeof(config.wifiSSID));
        strlcpy(config.wifiPassword, "", sizeof(config.wifiPassword));
        strlcpy(config.owmApiKey, "", sizeof(config.owmApiKey));
        strlcpy(config.owmLocationId, "2514824", sizeof(config.owmLocationId));
        strlcpy(config.locationName, "Los Barrios", sizeof(config.locationName));
        config.owmLanguage = "es";
        strlcpy(config.timezone, "Europe/Madrid", sizeof(config.timezone));
        config.isMetric = true;
        config.is12hStyle = false;
        config.isHHMM = false;
        config.screenOnHour = 7;
        config.screenOnMinute = 0;
        config.screenOffHour = 22;
        config.screenOffMinute = 0;
        config.screenSleepEnabled = true;
        config.brightness = 100;
        config.updateIntervalMin = 10;
    }
    
    bool begin() {
        if (!LittleFS.begin()) {
            Serial.println(F("[Config] LittleFS mount failed, formatting..."));
            LittleFS.format();
            if (!LittleFS.begin()) {
                Serial.println(F("[Config] LittleFS format failed!"));
                return false;
            }
        }
        Serial.println(F("[Config] LittleFS mounted OK"));
        return true;
    }
    
    bool load() {
        File f = LittleFS.open(CONFIG_FILE, "r");
        if (!f) {
            Serial.println(F("[Config] No config file found, using defaults"));
            return false;
        }
        
        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        
        if (err) {
            Serial.printf("[Config] JSON parse error: %s\n", err.c_str());
            return false;
        }
        
        // WiFi
        strlcpy(config.wifiSSID, doc["wifi_ssid"] | "", sizeof(config.wifiSSID));
        strlcpy(config.wifiPassword, doc["wifi_pass"] | "", sizeof(config.wifiPassword));
        
        // OWM
        strlcpy(config.owmApiKey, doc["owm_key"] | "", sizeof(config.owmApiKey));
        strlcpy(config.owmLocationId, doc["owm_loc_id"] | "2514824", sizeof(config.owmLocationId));
        strlcpy(config.locationName, doc["loc_name"] | "Los Barrios", sizeof(config.locationName));
        config.owmLanguage = doc["owm_lang"] | "es";
        
        // Timezone
        strlcpy(config.timezone, doc["timezone"] | "Europe/Madrid", sizeof(config.timezone));
        
        // Format
        config.isMetric = doc["metric"] | true;
        config.is12hStyle = doc["12h"] | false;
        config.isHHMM = doc["hhmm"] | false;
        
        // Screen schedule
        config.screenOnHour = doc["scr_on_h"] | 7;
        config.screenOnMinute = doc["scr_on_m"] | 0;
        config.screenOffHour = doc["scr_off_h"] | 22;
        config.screenOffMinute = doc["scr_off_m"] | 0;
        config.screenSleepEnabled = doc["scr_sleep"] | true;
        config.brightness = doc["brightness"] | 100;
        config.updateIntervalMin = doc["update_min"] | 10;
        
        Serial.println(F("[Config] Configuration loaded successfully"));
        printConfig();
        return true;
    }
    
    bool save() {
        StaticJsonDocument<1024> doc;
        
        doc["wifi_ssid"] = config.wifiSSID;
        doc["wifi_pass"] = config.wifiPassword;
        doc["owm_key"] = config.owmApiKey;
        doc["owm_loc_id"] = config.owmLocationId;
        doc["loc_name"] = config.locationName;
        doc["owm_lang"] = config.owmLanguage;
        doc["timezone"] = config.timezone;
        doc["metric"] = config.isMetric;
        doc["12h"] = config.is12hStyle;
        doc["hhmm"] = config.isHHMM;
        doc["scr_on_h"] = config.screenOnHour;
        doc["scr_on_m"] = config.screenOnMinute;
        doc["scr_off_h"] = config.screenOffHour;
        doc["scr_off_m"] = config.screenOffMinute;
        doc["scr_sleep"] = config.screenSleepEnabled;
        doc["brightness"] = config.brightness;
        doc["update_min"] = config.updateIntervalMin;
        
        File f = LittleFS.open(CONFIG_FILE, "w");
        if (!f) {
            Serial.println(F("[Config] Failed to open config file for writing"));
            return false;
        }
        
        serializeJson(doc, f);
        f.close();
        Serial.println(F("[Config] Configuration saved"));
        return true;
    }
    
    String getTimezoneString() {
        return getTzInfo(String(config.timezone));
    }
    
    void printConfig() {
        Serial.println(F("--- Current Configuration ---"));
        Serial.printf("  WiFi SSID: %s\n", config.wifiSSID);
        Serial.printf("  OWM Key: %s...\n", String(config.owmApiKey).substring(0, 8).c_str());
        Serial.printf("  Location: %s (ID: %s)\n", config.locationName, config.owmLocationId);
        Serial.printf("  Timezone: %s\n", config.timezone);
        Serial.printf("  Metric: %s, 12h: %s\n", config.isMetric ? "yes" : "no", config.is12hStyle ? "yes" : "no");
        Serial.printf("  Screen ON: %02d:%02d, OFF: %02d:%02d\n", 
            config.screenOnHour, config.screenOnMinute,
            config.screenOffHour, config.screenOffMinute);
        Serial.printf("  Update interval: %d min\n", config.updateIntervalMin);
        Serial.println(F("---"));
    }
};

#endif
