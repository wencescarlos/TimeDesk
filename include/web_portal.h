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
#include <functional>

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

    // Vistas previas de /clock: render(estilo, paleta) dibuja el estilo en el
    // framebuffer y rellena su paleta; pixel(x, y) lee el índice de color.
    typedef std::function<void(uint8_t style, uint16_t *palette)> PreviewRenderFn;
    typedef std::function<uint8_t(uint16_t x, uint16_t y)> PreviewPixelFn;
    void setPreviewSource(PreviewRenderFn render, PreviewPixelFn pixel) {
        previewRender = render;
        previewPixel = pixel;
    }
    String getLocalIP();
    
private:
    ESP8266WebServer server;
    DNSServer dnsServer;
    ConfigManager *configMgr;
    bool apMode;
    bool restartRequested;
    bool uploadAuthorized = false;   // la subida de firmware en curso tiene permiso
    bool showClockRequested;
    PreviewRenderFn previewRender;
    PreviewPixelFn previewPixel;
    
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
    void handleClockPreview();
    void handleStatusPage();
    // Acceso con contraseña (HTTP Basic). Sin contraseña o en modo AP, abierto.
    bool isAuthorized();
    bool checkAuth();   // si no hay permiso, pide credenciales y devuelve false

    void streamSchedulePage();
    void streamUpdatePage();
    String getTimezoneOptions();
    String getWifiNetworks();
};

#endif
