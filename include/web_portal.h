/**
 * web_portal.h - Portal web de configuración con AP mode y servidor HTTP
 */
#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include "config.h"
#include "config_manager.h"
#include "github_updater.h"

class WebPortal {
public:
    WebPortal(ConfigManager *cfgMgr);
    
    void beginAP();
    void beginSTA();
    void stop();
    void handleClient();
    bool isAPMode();
    bool shouldRestart();
    bool consumeShowClockRequest();   // true una vez si la web pidio mostrar el reloj
    bool consumeUpdateCheckRequest(); // true una vez si la web pidio buscar actualizaciones
    void setUpdater(GithubUpdater *u) { updater = u; }
    String getLocalIP();
    
private:
    ESP8266WebServer server;
    DNSServer dnsServer;
    ConfigManager *configMgr;
    bool apMode;
    bool restartRequested;
    bool showClockRequested;
    bool updateCheckRequested = false;
    GithubUpdater *updater = nullptr;
    
    void setupRoutes();
    void handleRoot();
    void handleSave();
    void handleStatus();
    void handleSchedule();
    void handleScheduleSave();
    void handleScanWifi();
    void handleReset();
    void handleUpdate();
    void handleUpdateUpload();
    void handleNotFound();
    void handleClock();
    void handleClockSave();
    void handleAutoUpdateSave();
    void handleUpdateCheck();
    void handleUpdateInfo();

    String generateScheduleHTML();
    String generateUpdateHTML();
    String getTimezoneOptions();
    String getWifiNetworks();
};

#endif
