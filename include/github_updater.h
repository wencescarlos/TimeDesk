/**
 * github_updater.h - Actualización automática del firmware desde GitHub Releases
 *
 * Flujo:
 *   1. Descarga https://github.com/<repo>/releases/latest/download/version.txt
 *   2. Si la versión es mayor que FIRMWARE_VERSION descarga firmware.bin de la
 *      misma Release y lo instala (el dispositivo se reinicia solo).
 *
 * Si include/ota_public_key.h contiene una clave pública RSA, el firmware
 * descargado debe venir firmado con la clave privada correspondiente
 * (lo hace el workflow .github/workflows/release.yml); si no, se rechaza.
 */
#ifndef GITHUB_UPDATER_H
#define GITHUB_UPDATER_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

enum GhUpdateResult : uint8_t {
    GH_NEVER_CHECKED = 0,
    GH_UP_TO_DATE,        // ya tiene la última versión
    GH_UPDATED,           // (no se llega a ver: el equipo se reinicia)
    GH_NO_RELEASE,        // el repositorio no tiene Releases publicadas
    GH_NETWORK_ERROR,     // fallo de conexión / HTTP
    GH_INSTALL_ERROR,     // fallo al descargar o verificar firmware.bin
    GH_BAD_CONFIG,        // repositorio mal escrito
    GH_INTERRUPTED        // el equipo se reinició a mitad (ver el mensaje: último paso)
};

struct GhUpdateStatus {
    GhUpdateResult result = GH_NEVER_CHECKED;
    String latestVersion;   // versión publicada en GitHub
    String message;         // detalle legible del último intento
    time_t lastCheck = 0;   // epoch del último intento
};

class GithubUpdater {
public:
    // ¿Toca comprobar ahora? (una vez al día a la hora configurada)
    bool isDue(const AppConfig &cfg);

    // Reserva los búferes de la descarga (~3 KB). Hay que llamarlo ANTES de
    // liberar el framebuffer: así no ocupan el hueco de 19 KB que deja la
    // pantalla, que la conexión TLS necesita entero (16,7 KB seguidos).
    bool reserveBuffers();
    void releaseBuffers();

    // Comprueba y, si hay versión nueva, instala. Si instala, reinicia el ESP.
    GhUpdateResult run(const AppConfig &cfg);

    // ── Actualización en un arranque dedicado ────────────────────────────
    // La conexión TLS necesita ~27 KB y el firmware otros 4 KB: con el reloj
    // en marcha (portal, mDNS, datos del tiempo, memoria fragmentada) no
    // siempre caben. Por eso la comprobación se pide guardando una marca en
    // la memoria RTC y reiniciando; al arrancar, antes de cargar nada más,
    // se hace la comprobación con toda la memoria libre.
    static void requestBootUpdate();                      // guarda la petición y reinicia
    static bool takeBootUpdateRequest(uint32_t &requestedAt);   // la lee y la borra
    void setCheckTime(uint32_t t) { checkTime = t; st.lastCheck = t; }      // hora de la petición (sin NTP al arrancar)
    void saveStatus();                                    // resultado -> memoria RTC
    void loadStatus();                                    // memoria RTC -> resultado
    // Deja en la memoria RTC el paso en curso: si el equipo se reinicia a
    // mitad, el siguiente arranque muestra en /update dónde se quedó.
    void crumb(const String &step);

    const GhUpdateStatus &status() const { return st; }
    static bool signingEnabled();
    static int compareVersions(const String &a, const String &b);
    static const char *resultText(GhUpdateResult r);

private:
    GhUpdateStatus st;
    int lastCheckYday = -1;
    bool rtcLoaded = false;   // la marca de la memoria RTC se lee una vez por arranque
    char *urlBuf = nullptr;   // búferes de la descarga (ver reserveBuffers)
    char *lineBuf = nullptr;
    uint32_t checkTime = 0;
};

#endif
