/**
 * config.h - Configuración y constantes del proyecto
 * Estación Meteorológica ESP8266 con portal web
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================
// PINES HARDWARE
// =====================================================
#define TFT_DC    D2
#define TFT_CS    D1
#define TFT_LED   D8
#define TOUCH_CS  D3
#define TOUCH_IRQ D4

// =====================================================
// CONSTANTES DEL SISTEMA
// =====================================================
#define WIFI_HOSTNAME       "Estacion-Meteo"
#define AP_SSID             "EstacionMeteo-Config"
#define AP_PASSWORD         "meteo1234"
#define CONFIG_FILE         "/config.json"
#define CALIB_FILE          "/calibration.json"

#define UPDATE_INTERVAL_SECS  600     // 10 minutos
#define SLEEP_INTERVAL_SECS   30      // Segundos antes de dormir pantalla
#define NTP_MIN_VALID_EPOCH   1533081600
#define NTP_SERVERS           "pool.ntp.org"

#define MAX_FORECASTS         12
#define WEB_SERVER_PORT       80

// =====================================================
// COLORES (paleta de 4 colores para MiniGrafx)
// =====================================================
#define MINI_BLACK  0
#define MINI_WHITE  1
#define MINI_YELLOW 2
#define MINI_BLUE   3

// =====================================================
// BITS POR PIXEL (2^2 = 4 colores)
// =====================================================
#define BITS_PER_PIXEL 2

// =====================================================
// TEXTOS EN ESPAÑOL
// =====================================================
const String WDAY_NAMES[] = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};
const String MONTH_NAMES[] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
const String SUN_MOON_TEXT[] = {"Sol", "Salid", "Pues", "Luna", "Edad", "Ilumin"};
const String MOON_PHASES[] = {"Luna Nueva", "Creciente Iluminante", "Cuarto Creciente", "Gibosa Iluminante",
                              "Luna Llena", "Gibosa Menguante", "Cuarto Menguante", "Creciente Menguante"};

// =====================================================
// ICONOS DE LUNA (norte/sur, creciente/menguante)
// =====================================================
const char MOON_ICONS_NORTH_WANING[] = {64, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 48};
const char MOON_ICONS_NORTH_WAXING[] = {64, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 48};
const char MOON_ICONS_SOUTH_WANING[] = {64, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 48};
const char MOON_ICONS_SOUTH_WAXING[] = {64, 77, 76, 75, 74, 73, 72, 71, 70, 69, 68, 67, 66, 65, 48};

// =====================================================
// ESTRUCTURA DE CONFIGURACIÓN PERSISTENTE
// =====================================================
struct AppConfig {
    // WiFi
    char wifiSSID[64];
    char wifiPassword[64];
    
    // OpenWeatherMap
    char owmApiKey[48];
    char owmLocationId[16];
    char locationName[48];
    String owmLanguage;
    
    // Zona horaria
    char timezone[64];
    
    // Formato
    bool isMetric;
    bool is12hStyle;
    bool isHHMM;
    
    // Horario pantalla
    uint8_t screenOnHour;
    uint8_t screenOnMinute;
    uint8_t screenOffHour;
    uint8_t screenOffMinute;
    bool screenSleepEnabled;
    
    // Brillo
    uint8_t brightness;
    
    // Intervalo actualización (minutos)
    uint16_t updateIntervalMin;
};

#endif
