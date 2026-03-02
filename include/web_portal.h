/**
 * web_portal.h - Portal web de configuración con AP mode y servidor HTTP
 */
#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "config.h"
#include "config_manager.h"

class WebPortal {
public:
    WebPortal(ConfigManager *cfgMgr);
    
    void beginAP();
    void beginSTA();
    void stop();
    void handleClient();
    bool isAPMode();
    bool shouldRestart();
    String getLocalIP();
    
private:
    ESP8266WebServer server;
    DNSServer dnsServer;
    ConfigManager *configMgr;
    bool apMode;
    bool restartRequested;
    
    void setupRoutes();
    void handleRoot();
    void handleSave();
    void handleStatus();
    void handleSchedule();
    void handleScheduleSave();
    void handleScanWifi();
    void handleReset();
    void handleNotFound();
    
    String generateHTML();
    String generateScheduleHTML();
    String getTimezoneOptions();
    String getWifiNetworks();
};

#endif
