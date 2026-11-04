/**
 * TimeDesk - Reloj y estación meteorológica de escritorio
 * 
 * Características:
 * - 6 estilos de reloj cambiables en caliente
 * - Tiempo, pronóstico y fase lunar (OpenWeatherMap)
 * - Portal web de configuración (modo AP + modo estación)
 * - Horario de encendido/apagado por web y menú táctil
 * - Actualización automática diaria desde GitHub Releases
 * - Almacenamiento persistente con LittleFS + JSON
 * 
 * Hardware: ESP8266 + ILI9341 TFT 240x320 + XPT2046 Touch
 * 
 * Basado en el proyecto original de ThingPulse (MIT License)
 */

#include <Arduino.h>
#include "time.h"
#include <SPI.h>
#include <ESP8266WiFi.h>

#include <XPT2046_Touchscreen.h>
#include "TouchControllerWS.h"
#include "SunMoonCalc.h"

#include <JsonListener.h>
#include <OpenWeatherMapCurrent.h>
#include <OpenWeatherMapForecast.h>
#include <MiniGrafx.h>
#undef min              
#undef max 
#include <Carousel.h>
#include <ILI9341_SPI.h>

#include "version.h"
#include "config.h"
#include "config_manager.h"
#include "web_portal.h"
#include "ArialRounded.h"
#include "moonphases.h"
#include "weathericons.h"
#include "clock_styles.h"
#include "github_updater.h"


// =====================================================
// OBJETOS GLOBALES
// =====================================================
ADC_MODE(ADC_VCC);

uint16_t palette[] = {ILI9341_BLACK, ILI9341_WHITE, ILI9341_YELLOW, 0x7E3C};

ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette);
Carousel carousel(&gfx, 0, 0, 240, 100);

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TouchControllerWS touchController(&ts);

ConfigManager configManager;
WebPortal webPortal(&configManager);
GithubUpdater githubUpdater;

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
SunMoonCalc::Moon moonData;

// =====================================================
// VARIABLES DE ESTADO
// =====================================================
enum Screen : uint8_t {
    SCR_MAIN = 0,      // Tiempo actual, hora y fecha
    SCR_CLOCK,         // Reloj a pantalla completa (segun el estilo elegido)
    SCR_DETAIL,        // Datos extendidos del tiempo
    SCR_FORECAST1,     // Pronostico 1
    SCR_FORECAST2,     // Pronostico 2
    SCR_ABOUT,         // Informacion del dispositivo
    SCR_SETTINGS,      // Ajustes tactiles
    SCR_COUNT
};
uint8_t screen = SCR_MAIN;
int screenCount = SCR_COUNT;
long lastDownloadUpdate = 0;
long timerPress = 0;
bool canBtnPress = true;
uint16_t dividerTop, dividerBottom, dividerMiddle;

// Settings screen touch zones
enum SettingsItem {
    SETT_NONE = -1,
    SETT_12H = 0,
    SETT_METRIC = 1,
    SETT_SLEEP_TOGGLE = 2,
    SETT_ON_HOUR_UP = 3,
    SETT_ON_HOUR_DOWN = 4,
    SETT_OFF_HOUR_UP = 5,
    SETT_OFF_HOUR_DOWN = 6,
    SETT_SAVE = 7,
    SETT_BACK = 8
};

// =====================================================
// DECLARACIONES FORWARD
// =====================================================
void calibrationCallback(int16_t x, int16_t y);
CalibrationCallback calibration = &calibrationCallback;

void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawWifiQuality();
void drawCurrentWeather();
void drawForecast();
void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex);
void drawAstronomy();
char determineMoonIcon();
void drawCurrentWeatherDetail();
void drawLabelValue(uint8_t line, String label, String value);
void drawForecastTable(uint8_t start);
void drawAbout();
void drawSettingsScreen();
void drawClockScreen();
void setBacklight(bool on);
void runGithubUpdate(bool manual);
void handleSettingsTouch(TS_Point p);
void drawSeparator(uint16_t y);
String getTime(time_t *timestamp);
const char* getMeteoconIconFromProgmem(String iconText);
const char* getMiniMeteoconIconFromProgmem(String iconText);
void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
void drawForecast3(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y);
char* make12_24(int hour);
uint8_t changeScreen(TS_Point p, uint8_t screen);
bool sleep_mode();
bool shouldDisplayBeOff();
bool connectWifi();

FrameCallback frames[] = { drawForecast1, drawForecast2, drawForecast3 };
int frameCount = 3;

// =====================================================
// CONEXIÓN WIFI
// =====================================================
bool connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return true;
    
    if (strlen(configManager.config.wifiSSID) == 0) {
        Serial.println(F("[WiFi] No SSID configured"));
        return false;
    }
    
    Serial.printf("[WiFi] Connecting to: %s\n", configManager.config.wifiSSID);
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(configManager.config.wifiSSID, configManager.config.wifiPassword);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        drawProgress(min(attempts * 5, 90), "Conectando a '" + String(configManager.config.wifiSSID) + "'...");
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        drawProgress(100, "Conectado: " + WiFi.localIP().toString());
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    Serial.println(F("\n[WiFi] Connection failed!"));
    return false;
}

// =====================================================
// INIT TIME
// =====================================================
void initTime() {
    time_t now;
    String tzString = configManager.getTimezoneString();
    
    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    
    Serial.printf("[Time] Configuring timezone: %s -> %s\n", 
        configManager.config.timezone, tzString.c_str());
    configTime(tzString.c_str(), NTP_SERVERS);
    
    int i = 1;
    while ((now = time(nullptr)) < NTP_MIN_VALID_EPOCH) {
        drawProgress(i * 10, "Sincronizando hora...");
        Serial.print(".");
        delay(300);
        yield();
        i++;
        if (i > 30) break;
    }
    drawProgress(100, "Hora sincronizada");
    Serial.println();
    printf("[Time] Local: %s", asctime(localtime(&now)));
}

// =====================================================
// SETUP
// =====================================================
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n\n================================="));
    Serial.print(F(" TimeDesk v"));
    Serial.println(F(FIRMWARE_VERSION));
    Serial.println(F("=================================\n"));
    
    // Inicializar pantalla
    pinMode(TFT_LED, OUTPUT);
    digitalWrite(TFT_LED, HIGH);
    
    gfx.init();
    gfx.fillBuffer(MINI_BLACK);
    gfx.commit();
    
    // Inicializar LittleFS y cargar configuración
    configManager.begin();
    configManager.load();
    
    // Estilo de reloj: fuentes + paleta del tema elegido
    clockSetFonts(ArialRoundedMTBold_14, ArialRoundedMTBold_36);
    clockApplyPalette(configManager.config.clockStyle, palette);
    setBacklight(true);
    
    // Inicializar touch
    Serial.println(F("[Setup] Initializing touch..."));
    ts.begin();
    
    // Calibración táctil
    boolean isCalibrationAvailable = touchController.loadCalibration();
    
    gfx.fillBuffer(MINI_BLACK);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setFont(ArialRoundedMTBold_36);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 50, F("TimeDesk"));
    gfx.setFont(ArialMT_Plain_10);
    gfx.setColor(MINI_BLUE);
    gfx.drawString(120, 96, "v" FIRMWARE_VERSION);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 140, F("Mantén presionado"));
    gfx.drawString(120, 160, F("para calibrar pantalla"));
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 190, F("Suelta para continuar"));
    gfx.commit();
    delay(2500);
    yield();
    
    if (ts.touched()) {
        isCalibrationAvailable = false;
        gfx.fillBuffer(MINI_BLACK);
        gfx.setColor(MINI_YELLOW);
        gfx.setTextAlignment(TEXT_ALIGN_CENTER);
        gfx.drawString(120, 160, F("Calibración iniciada"));
        gfx.drawString(120, 180, F("Suelta la pantalla"));
        gfx.commit();
        while (ts.touched()) { delay(10); yield(); }
        delay(100);
        touchController.getPoint();
    }
    
    if (!isCalibrationAvailable) {
        Serial.println(F("[Setup] Starting touch calibration..."));
        touchController.startCalibration(&calibration);
        while (!touchController.isCalibrationFinished()) {
            gfx.fillBuffer(MINI_BLACK);
            gfx.setColor(MINI_YELLOW);
            gfx.setTextAlignment(TEXT_ALIGN_CENTER);
            gfx.drawString(120, 160, F("Toca el punto\npara calibrar"));
            touchController.continueCalibration();
            gfx.commit();
            yield();
        }
        touchController.saveCalibration();
    }
    
    dividerTop = 64;
    dividerBottom = gfx.getHeight() - dividerTop;
    dividerMiddle = gfx.getWidth() / 2;
    
    // Intentar conectar WiFi
    drawProgress(10, "Conectando WiFi...");
    
    if (!connectWifi()) {
        // No WiFi configurado o fallo -> modo AP
        Serial.println(F("[Setup] Starting AP mode for configuration..."));
        drawProgress(50, "Modo configuración AP");
        delay(500);
        
        webPortal.beginAP();
        
        // Pantalla de modo AP
        gfx.fillBuffer(MINI_BLACK);
        gfx.setFont(ArialRoundedMTBold_14);
        gfx.setTextAlignment(TEXT_ALIGN_CENTER);
        gfx.setColor(MINI_YELLOW);
        gfx.drawString(120, 40, F("MODO CONFIGURACIÓN"));
        gfx.setColor(MINI_WHITE);
        gfx.drawString(120, 80, F("Conecta al WiFi:"));
        gfx.setColor(MINI_BLUE);
        gfx.drawString(120, 105, AP_SSID);
        gfx.setColor(MINI_WHITE);
        gfx.drawString(120, 135, F("Contraseña:"));
        gfx.setColor(MINI_BLUE);
        gfx.drawString(120, 160, AP_PASSWORD);
        gfx.setColor(MINI_WHITE);
        gfx.drawString(120, 200, F("Abre el navegador:"));
        gfx.setColor(MINI_YELLOW);
        gfx.drawString(120, 225, "http://" + webPortal.getLocalIP());
        gfx.setColor(MINI_WHITE);
        gfx.drawString(120, 270, F("Configura tu WiFi,"));
        gfx.drawString(120, 290, F("API y zona horaria"));
        gfx.commit();
        
        // Loop del portal AP
        while (!webPortal.shouldRestart()) {
            webPortal.handleClient();
            yield();
        }
        
        Serial.println(F("[Setup] Configuration saved, restarting..."));
        delay(2000);
        ESP.restart();
    }
    
    // WiFi conectado -> iniciar servidor web en modo STA
    webPortal.setUpdater(&githubUpdater);
    webPortal.beginSTA();
    
    carousel.setFrames(frames, frameCount);
    carousel.disableAllIndicators();
    
    initTime();
    updateData();
    
    timerPress = millis();
    lastDownloadUpdate = millis();
    canBtnPress = true;
    screen = configManager.config.startOnClock ? SCR_CLOCK : SCR_MAIN;
    
    Serial.println(F("[Setup] Ready!"));
    Serial.printf("[Setup] Web config: http://%s/\n", WiFi.localIP().toString().c_str());
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
    static bool asleep = false;
    
    // Atender peticiones web (siempre)
    webPortal.handleClient();
    
    // Comprobar si se pidió reinicio desde la web
    if (webPortal.shouldRestart()) {
        drawProgress(50, "Reiniciando...");
        delay(1500);
        ESP.restart();
    }
    
    // Cambios en caliente desde el portal (estilo de reloj, brillo)
    clockApplyPalette(configManager.config.clockStyle, palette);
    if (webPortal.consumeShowClockRequest()) {
        screen = SCR_CLOCK;
        timerPress = millis();   // despierta la pantalla si estaba en reposo
    }
    
    // Actualización desde GitHub: diaria a la hora configurada, o a petición
    bool manualCheck = webPortal.consumeUpdateCheckRequest();
    if (manualCheck || githubUpdater.isDue(configManager.config)) {
        runGithubUpdate(manualCheck);
    }
    
    gfx.fillBuffer(MINI_BLACK);
    
    // Gestión táctil
    if (touchController.isTouched(500)) {
        TS_Point p = touchController.getPoint();
        timerPress = millis();
        
        if (!asleep) {
            if (screen == SCR_SETTINGS) {
                handleSettingsTouch(p);
            } else {
                screen = changeScreen(p, screen);
            }
        }
    }
    
    if (!(asleep = sleep_mode())) {
        setBacklight(true);   // aplica cambios de brillo hechos desde la web
        switch (screen) {
            case SCR_MAIN: {
                drawTime();
                drawWifiQuality();
                int remainingTimeBudget = carousel.update();
                if (remainingTimeBudget > 0) delay(remainingTimeBudget);
                drawCurrentWeather();
                drawAstronomy();
                break;
            }
            case SCR_CLOCK:
                drawClockScreen();
                break;
            case SCR_DETAIL:
                drawCurrentWeatherDetail();
                break;
            case SCR_FORECAST1:
                drawForecastTable(0);
                break;
            case SCR_FORECAST2:
                drawForecastTable(4);
                break;
            case SCR_ABOUT:
                drawAbout();
                break;
            case SCR_SETTINGS:
                drawSettingsScreen();
                break;
        }
        gfx.commit();
        
        // Actualizar datos meteorológicos
        unsigned long updateMs = (unsigned long)configManager.config.updateIntervalMin * 60UL * 1000UL;
        if (millis() - lastDownloadUpdate > updateMs) {
            updateData();
            lastDownloadUpdate = millis();
        }
    }
}

// =====================================================
// PANTALLA DE AJUSTES (TÁCTIL)
// =====================================================
void drawSettingsScreen() {
    char buf[8];
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 4, F("AJUSTES"));
    
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    
    // Formato 12h / 24h
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 30, F("Formato hora:"));
    gfx.setColor(configManager.config.is12hStyle ? MINI_YELLOW : MINI_BLUE);
    gfx.drawString(150, 30, configManager.config.is12hStyle ? "12h" : "24h");
    
    // Métrico / Imperial
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 52, F("Unidades:"));
    gfx.setColor(configManager.config.isMetric ? MINI_YELLOW : MINI_BLUE);
    gfx.drawString(150, 52, configManager.config.isMetric ? "Metrico" : "Imperial");
    
    // Sleep toggle
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 74, F("Horario auto:"));
    gfx.setColor(configManager.config.screenSleepEnabled ? MINI_YELLOW : MINI_BLUE);
    gfx.drawString(150, 74, configManager.config.screenSleepEnabled ? "ON" : "OFF");
    
    // Estilo de reloj (tocar para pasar al siguiente; se aplica al momento)
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 96, F("Estilo reloj:"));
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 96, String(clockStyleName(configManager.config.clockStyle)) + " >");
    
    // Línea separadora
    gfx.setColor(MINI_BLUE);
    gfx.drawLine(0, 118, 240, 118);
    
    // Hora encendido / apagado con botones - / +
    const char *labels[] = {"Encendido:", "Apagado:"};
    uint8_t hours[] = {configManager.config.screenOnHour, configManager.config.screenOffHour};
    uint8_t mins[] = {configManager.config.screenOnMinute, configManager.config.screenOffMinute};
    for (uint8_t i = 0; i < 2; i++) {
        uint16_t y = 126 + i * 28;
        gfx.setColor(MINI_WHITE);
        gfx.drawString(10, y, labels[i]);
        gfx.setColor(MINI_YELLOW);
        sprintf(buf, "%02d:%02d", hours[i], mins[i]);
        gfx.drawString(100, y, buf);
        gfx.setColor(MINI_BLUE);
        gfx.fillRect(160, y - 4, 30, 20);
        gfx.fillRect(200, y - 4, 30, 20);
        gfx.setColor(MINI_WHITE);
        gfx.drawString(170, y - 2, "-");
        gfx.drawString(208, y - 2, "+");
    }
    
    // Línea separadora
    gfx.setColor(MINI_BLUE);
    gfx.drawLine(0, 180, 240, 180);
    
    // Info de red y ubicación
    gfx.setFont(ArialMT_Plain_10);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 186, F("Config web:"));
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(80, 186, "http://" + WiFi.localIP().toString());
    gfx.setColor(MINI_WHITE);
    gfx.drawString(10, 200, "WiFi: " + String(configManager.config.wifiSSID) + " (" + String(WiFi.RSSI()) + " dBm)");
    gfx.drawString(10, 214, "Loc: " + String(configManager.config.locationName));
    gfx.drawString(10, 228, "TZ: " + String(configManager.config.timezone));
    
    // Botones guardar / volver
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_BLUE);
    gfx.fillRect(10, 252, 100, 26);
    gfx.fillRect(130, 252, 100, 26);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(60, 255, "GUARDAR");
    gfx.drawString(180, 255, "VOLVER");
    
    gfx.setFont(ArialMT_Plain_10);
    gfx.drawString(120, 290, "Actualiza cada " + String(configManager.config.updateIntervalMin) + " min");
    gfx.drawString(120, 304, "Heap: " + String(ESP.getFreeHeap() / 1024) + "KB");
}

void handleSettingsTouch(TS_Point p) {
    AppConfig &c = configManager.config;
    if (p.y >= 25 && p.y < 47) {
        c.is12hStyle = !c.is12hStyle;
    } else if (p.y >= 47 && p.y < 69) {
        c.isMetric = !c.isMetric;
    } else if (p.y >= 69 && p.y < 91) {
        c.screenSleepEnabled = !c.screenSleepEnabled;
    } else if (p.y >= 91 && p.y < 116) {
        c.clockStyle = (c.clockStyle + 1) % CLOCK_STYLE_COUNT;   // la paleta se aplica en loop()
    } else if (p.y >= 120 && p.y < 174 && p.x >= 160) {
        bool isOn = p.y < 148;
        bool up = p.x >= 200;
        uint8_t &h = isOn ? c.screenOnHour : c.screenOffHour;
        h = (h + (up ? 1 : 23)) % 24;
    } else if (p.y >= 248 && p.y < 282 && p.x < 120) {
        configManager.save();
        drawProgress(100, "Configuracion guardada!");
        delay(1000);
    } else if (p.y >= 248 && p.y < 282 && p.x >= 120) {
        screen = SCR_MAIN;
    }
}

// =====================================================
// PANTALLA DE RELOJ COMPLETA
// =====================================================
void runGithubUpdate(bool manual) {
    // Mensaje fijo: el panel ILI9341 conserva la imagen aunque se libere el buffer
    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 120, F("Buscando actualizaciones"));
    gfx.drawString(120, 140, F("en GitHub..."));
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 180, F("Si hay una nueva version"));
    gfx.drawString(120, 200, F("se instalara y reiniciara."));
    gfx.drawString(120, 230, F("No lo desconectes."));
    gfx.commit();
    
    // HTTPS necesita ~25KB de RAM: se libera el framebuffer (19KB) mientras dura
    gfx.freeBuffer();
    GhUpdateResult r = githubUpdater.run(configManager.config);
    gfx.init();   // si se instaló la nueva versión, el ESP ya se habrá reiniciado
    
    if (manual || (r != GH_UP_TO_DATE && r != GH_NO_RELEASE)) {
        String msg = String(GithubUpdater::resultText(r));
        if (githubUpdater.status().latestVersion.length()) msg += " (v" + githubUpdater.status().latestVersion + ")";
        drawProgress(100, msg);
        delay(2500);
    }
    timerPress = millis();
}

void drawClockScreen() {
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    ClockExtras extras;
    extras.temperature = String(currentWeather.temp, 1) + (configManager.config.isMetric ? "°C" : "°F");
    extras.description = currentWeather.description;
    extras.location = configManager.config.locationName;
    clockDrawFull(gfx, timeinfo, configManager.config, extras);
}

// =====================================================
// RETROILUMINACIÓN (PWM según el brillo configurado)
// =====================================================
void setBacklight(bool on) {
    static int16_t current = -1;
    int16_t level = on ? configManager.config.brightness : 0;
    if (level == current) return;
    current = level;
    analogWriteRange(100);
    if (level >= 100) digitalWrite(TFT_LED, HIGH);
    else if (level <= 0) setBacklight(false);
    else analogWrite(TFT_LED, level);
}

// =====================================================
// SLEEP MODE
// =====================================================
bool shouldDisplayBeOff() {
    if (!configManager.config.screenSleepEnabled) return false;
    
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    uint16_t nowMin = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    uint16_t offMin = configManager.config.screenOffHour * 60 + configManager.config.screenOffMinute;
    uint16_t onMin = configManager.config.screenOnHour * 60 + configManager.config.screenOnMinute;
    
    if (offMin > onMin) {
        return (nowMin >= offMin || nowMin < onMin);
    } else {
        return (nowMin >= offMin && nowMin < onMin);
    }
}

bool sleep_mode() {
    static bool sleeping = false;
    
    if (shouldDisplayBeOff()) {
        if (SLEEP_INTERVAL_SECS && millis() - timerPress >= SLEEP_INTERVAL_SECS * 1000UL) {
            if (sleeping) return true;
            
            int s = 0;
            do {
                drawProgress(s, "Entrando en modo reposo...");
                delay(10);
                yield();
            } while (s++ < 100 && !touchController.isTouched(0));
            
            if (s < 100) {
                timerPress = millis();
                touchController.getPoint();
                if (touchController.isTouched(0)) touchController.getPoint();
            } else {
                sleeping = true;
                setBacklight(false);
            }
        } else {
            if (sleeping) {
                setBacklight(true);
                sleeping = false;
            }
        }
    } else {
        if (sleeping) {
            setBacklight(true);
            sleeping = false;
        }
    }
    
    return sleeping;
}

// =====================================================
// ACTUALIZACIÓN DE DATOS
// =====================================================
void updateData() {
    time_t now = time(nullptr);
    
    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    
    drawProgress(30, "Actualizando condiciones...");
    OpenWeatherMapCurrent *currentWeatherClient = new OpenWeatherMapCurrent();
    currentWeatherClient->setMetric(configManager.config.isMetric);
    currentWeatherClient->setLanguage(configManager.config.owmLanguage);
    currentWeatherClient->updateCurrentById(&currentWeather, 
        configManager.config.owmApiKey, configManager.config.owmLocationId);
    delete currentWeatherClient;
    
    drawProgress(60, "Actualizando pronósticos...");
    OpenWeatherMapForecast *forecastClient = new OpenWeatherMapForecast();
    forecastClient->setMetric(configManager.config.isMetric);
    forecastClient->setLanguage(configManager.config.owmLanguage);
    uint8_t allowedHours[] = {12, 0};
    forecastClient->setAllowedHours(allowedHours, sizeof(allowedHours));
    forecastClient->updateForecastsById(forecasts, 
        configManager.config.owmApiKey, configManager.config.owmLocationId, MAX_FORECASTS);
    delete forecastClient;
    
    drawProgress(85, "Actualizando astronomía...");
    SunMoonCalc *smCalc = new SunMoonCalc(now, currentWeather.lat, currentWeather.lon);
    moonData = smCalc->calculateSunAndMoonData().moon;
    delete smCalc;
    
    drawProgress(100, "Actualización completa");
    Serial.printf("[Update] Done. Free heap: %d bytes\n", ESP.getFreeHeap());
    delay(500);
}

// =====================================================
// FUNCIONES DE DIBUJO
// =====================================================
void drawProgress(uint8_t percentage, String text) {
    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 146, text);
    gfx.setColor(MINI_WHITE);
    gfx.drawRect(10, 168, 220, 15);
    gfx.setColor(MINI_BLUE);
    gfx.fillRect(12, 170, 216 * percentage / 100, 11);
    gfx.commit();
}

void drawTime() {
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    clockDrawHeader(gfx, timeinfo, configManager.config);
}

void drawCurrentWeather() {
    gfx.setTransparentColor(MINI_BLACK);
    gfx.drawPalettedBitmapFromPgm(0, 55, getMeteoconIconFromProgmem(currentWeather.icon));
    
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_BLUE);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(220, 65, configManager.config.locationName);
    
    gfx.setFont(ArialRoundedMTBold_36);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(220, 78, String(currentWeather.temp, 1) + (configManager.config.isMetric ? "°C" : "°F"));
    
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setColor(MINI_YELLOW);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(220, 118, currentWeather.description);
}

void drawForecast1(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
    drawForecastDetail(x + 10, y + 165, 0);
    drawForecastDetail(x + 95, y + 165, 1);
    drawForecastDetail(x + 180, y + 165, 2);
}

void drawForecast2(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
    drawForecastDetail(x + 10, y + 165, 3);
    drawForecastDetail(x + 95, y + 165, 4);
    drawForecastDetail(x + 180, y + 165, 5);
}

void drawForecast3(MiniGrafx *display, CarouselState* state, int16_t x, int16_t y) {
    drawForecastDetail(x + 10, y + 165, 6);
    drawForecastDetail(x + 95, y + 165, 7);
    drawForecastDetail(x + 180, y + 165, 8);
}

void drawForecastDetail(uint16_t x, uint16_t y, uint8_t dayIndex) {
    gfx.setColor(MINI_YELLOW);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    time_t time = forecasts[dayIndex].observationTime;
    struct tm *timeinfo = localtime(&time);
    
    if (configManager.config.is12hStyle) {
        gfx.drawString(x + 25, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(make12_24(timeinfo->tm_hour)));
    } else {
        gfx.drawString(x + 25, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");
    }
    
    gfx.setColor(MINI_WHITE);
    gfx.drawString(x + 25, y, String(forecasts[dayIndex].temp, 1) + (configManager.config.isMetric ? "°C" : "°F"));
    
    gfx.drawPalettedBitmapFromPgm(x, y + 15, getMiniMeteoconIconFromProgmem(forecasts[dayIndex].icon));
    gfx.setColor(MINI_BLUE);
    gfx.drawString(x + 25, y + 60, String(forecasts[dayIndex].rain, 1) + (configManager.config.isMetric ? "mm" : "in"));
}

void drawAstronomy() {
    gfx.setFont(MoonPhases_Regular_36);
    gfx.setColor(MINI_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(120, 275, String(determineMoonIcon()));
    
    gfx.setColor(MINI_WHITE);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(120, 250, MOON_PHASES[moonData.phase.index]);
    
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(5, 250, SUN_MOON_TEXT[0]);
    gfx.setColor(MINI_WHITE);
    time_t t = currentWeather.sunrise;
    gfx.drawString(5, 276, SUN_MOON_TEXT[1] + ":");
    gfx.drawString(45, 276, getTime(&t));
    t = currentWeather.sunset;
    gfx.drawString(5, 291, SUN_MOON_TEXT[2] + ":");
    gfx.drawString(45, 291, getTime(&t));
    
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(235, 250, SUN_MOON_TEXT[3]);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(190, 276, SUN_MOON_TEXT[4] + ":");
    gfx.drawString(235, 276, String(moonData.age, 1) + "d");
    gfx.drawString(190, 291, SUN_MOON_TEXT[5] + ":");
    gfx.drawString(235, 291, String(moonData.illumination * 100, 0) + "%");
}

char determineMoonIcon() {
    char index = round(moonData.illumination * 14);
    if (moonData.phase.index > 4) {
        return currentWeather.lat > 0 ? MOON_ICONS_NORTH_WANING[index] : MOON_ICONS_SOUTH_WANING[index];
    } else {
        return currentWeather.lat > 0 ? MOON_ICONS_NORTH_WAXING[index] : MOON_ICONS_SOUTH_WAXING[index];
    }
}

void drawCurrentWeatherDetail() {
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 2, "Condiciones actuales");
    
    drawLabelValue(0, "Temperatura:", String(currentWeather.temp, 1) + (configManager.config.isMetric ? "°C" : "°F"));
    drawLabelValue(1, "Sensacion:", String(currentWeather.feelsLike, 1) + (configManager.config.isMetric ? "°C" : "°F"));
    drawLabelValue(2, "Vel. Viento:", String(currentWeather.windSpeed, 1) + (configManager.config.isMetric ? "m/s" : "mph"));
    drawLabelValue(3, "Dir. Viento:", String(currentWeather.windDeg, 1) + "°");
    drawLabelValue(4, "Humedad:", String(currentWeather.humidity) + "%");
    drawLabelValue(5, "Presion:", String(currentWeather.pressure) + "hPa");
    drawLabelValue(6, "Nubosidad:", String(currentWeather.clouds) + "%");
    drawLabelValue(7, "Visibilidad:", String(currentWeather.visibility) + "m");
}

void drawLabelValue(uint8_t line, String label, String value) {
    const uint8_t labelX = 15;
    const uint8_t valueX = 150;
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(labelX, 30 + line * 15, label);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(valueX, 30 + line * 15, value);
}

int8_t getWifiQuality() {
    int32_t dbm = WiFi.RSSI();
    if (dbm <= -100) return 0;
    if (dbm >= -50) return 100;
    return 2 * (dbm + 100);
}

void drawWifiQuality() {
    int8_t quality = getWifiQuality();
    gfx.setColor(MINI_WHITE);
    gfx.setFont(ArialMT_Plain_10);
    gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
    gfx.drawString(228, 9, String(quality) + "%");
    for (int8_t i = 0; i < 4; i++) {
        for (int8_t j = 0; j < 2 * (i + 1); j++) {
            if (quality > i * 25 || j == 0) {
                gfx.setPixel(230 + 2 * i, 18 - j);
            }
        }
    }
}

void drawForecastTable(uint8_t start) {
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 2, "Pronosticos");
    
    String degreeSign = configManager.config.isMetric ? "°C" : "°F";
    
    for (uint8_t i = start; i < start + 4; i++) {
        uint16_t y = 45 + (i - start) * 75;
        if (y > 320) break;
        
        gfx.setColor(MINI_WHITE);
        gfx.setTextAlignment(TEXT_ALIGN_CENTER);
        time_t t = forecasts[i].observationTime;
        struct tm *timeinfo = localtime(&t);
        
        if (configManager.config.is12hStyle) {
            gfx.drawString(120, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(make12_24(timeinfo->tm_hour)));
        } else {
            gfx.drawString(120, y - 15, WDAY_NAMES[timeinfo->tm_wday] + " " + String(timeinfo->tm_hour) + ":00");
        }
        
        gfx.drawPalettedBitmapFromPgm(0, 5 + y, getMiniMeteoconIconFromProgmem(forecasts[i].icon));
        gfx.setTextAlignment(TEXT_ALIGN_LEFT);
        gfx.setColor(MINI_YELLOW);
        gfx.setFont(ArialRoundedMTBold_14);
        gfx.drawString(0, y - 8, forecasts[i].main);
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(50, y, "T:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(70, y, String(forecasts[i].temp, 0) + degreeSign);
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(50, y + 15, "H:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(70, y + 15, String(forecasts[i].humidity) + "%");
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(50, y + 30, "P:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(70, y + 30, String(forecasts[i].rain, 2) + (configManager.config.isMetric ? "mm" : "in"));
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(130, y, "Pr:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(170, y, String(forecasts[i].pressure, 0) + "hPa");
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(130, y + 15, "V:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(170, y + 15, String(forecasts[i].windSpeed, 0) + (configManager.config.isMetric ? "m/s" : "mph"));
        
        gfx.setColor(MINI_BLUE);
        gfx.drawString(130, y + 30, "D:");
        gfx.setColor(MINI_WHITE);
        gfx.drawString(170, y + 30, String(forecasts[i].windDeg, 0) + "°");
    }
}

void drawAbout() {
    gfx.fillBuffer(MINI_BLACK);
    gfx.setFont(ArialRoundedMTBold_14);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.setColor(MINI_WHITE);
    gfx.drawString(120, 2, "Informacion del dispositivo");
    
    drawLabelValue(0, "Version:", FIRMWARE_VERSION);
    drawLabelValue(1, "Heap:", String(ESP.getFreeHeap() / 1024) + "kb");
    drawLabelValue(2, "Flash:", String(ESP.getFlashChipRealSize() / 1024 / 1024) + "MB");
    drawLabelValue(3, "WiFi:", String(WiFi.RSSI()) + "dB");
    drawLabelValue(4, "Chip ID:", String(ESP.getChipId()));
    drawLabelValue(5, "VCC:", String(ESP.getVcc() / 1024.0) + "V");
    drawLabelValue(6, "CPU:", String(ESP.getCpuFreqMHz()) + "MHz");
    drawLabelValue(7, "Pantalla OFF:", String(configManager.config.screenOffHour) + ":" + 
        (configManager.config.screenOffMinute < 10 ? "0" : "") + String(configManager.config.screenOffMinute));
    drawLabelValue(8, "Pantalla ON:", String(configManager.config.screenOnHour) + ":" + 
        (configManager.config.screenOnMinute < 10 ? "0" : "") + String(configManager.config.screenOnMinute));
    
    char time_str[15];
    const uint32_t millis_in_day = 1000 * 60 * 60 * 24;
    const uint32_t millis_in_hour = 1000 * 60 * 60;
    const uint32_t millis_in_minute = 1000 * 60;
    uint8_t days = millis() / millis_in_day;
    uint8_t hours = (millis() - (days * millis_in_day)) / millis_in_hour;
    uint8_t minutes = (millis() - (days * millis_in_day) - (hours * millis_in_hour)) / millis_in_minute;
    sprintf(time_str, "%2dd%2dh%2dm", days, hours, minutes);
    
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 220, "Encendido: ");
    gfx.setColor(MINI_WHITE);
    gfx.drawStringMaxWidth(110, 220, 130, time_str);
    
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 240, "IP: ");
    gfx.setColor(MINI_WHITE);
    gfx.drawStringMaxWidth(30, 240, 200, WiFi.localIP().toString());
    
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 260, "Web: ");
    gfx.setColor(MINI_BLUE);
    gfx.drawStringMaxWidth(40, 260, 200, "http://" + WiFi.localIP().toString());
    
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 280, "Update: ");
    gfx.setColor(MINI_WHITE);
    gfx.drawString(70, 280, "cada " + String(configManager.config.updateIntervalMin) + " min");
    
    gfx.setColor(MINI_YELLOW);
    gfx.drawString(10, 300, "Reset: ");
    gfx.setColor(MINI_WHITE);
    gfx.setFont(ArialMT_Plain_10);
    gfx.drawStringMaxWidth(55, 300, 185, ESP.getResetInfo());
}

void calibrationCallback(int16_t x, int16_t y) {
    gfx.setColor(MINI_YELLOW);
    gfx.fillCircle(x, y, 10);
}

String getTime(time_t *timestamp) {
    struct tm *timeInfo = localtime(timestamp);
    char buf[9];
    char ampm[3] = "";
    uint8_t hour = timeInfo->tm_hour;
    
    if (configManager.config.is12hStyle) {
        sprintf(ampm, hour >= 12 ? "pm" : "am");
        hour = (hour + 11) % 12 + 1;   // 0 -> 12 am, 12 -> 12 pm
        sprintf(buf, "%2d:%02d %s", hour, timeInfo->tm_min, ampm);
    } else {
        sprintf(buf, "%02d:%02d", hour, timeInfo->tm_min);
    }
    return String(buf);
}

char* make12_24(int hour) {
    static char hr[6];
    sprintf(hr, "%2d %s", (hour + 11) % 12 + 1, hour >= 12 ? "pm" : "am");
    return hr;
}

uint8_t changeScreen(TS_Point p, uint8_t currentScreen) {
    uint8_t page = currentScreen;
    
    if (p.y < dividerTop) {
        // Top -> toggle 12h/24h
        configManager.config.is12hStyle = !configManager.config.is12hStyle;
    } else if (p.y > dividerBottom) {
        // Bottom -> settings screen
        page = SCR_SETTINGS;
    } else if (p.x > dividerMiddle) {
        // Mitad derecha -> pantalla anterior
        if (page == 0) page = screenCount;
        page--;
    } else {
        // Mitad izquierda -> pantalla siguiente
        page = (page + 1) % screenCount;
    }
    return page;
}
