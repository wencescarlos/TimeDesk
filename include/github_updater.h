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
    GH_BAD_CONFIG         // repositorio mal escrito
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

    // Comprueba y, si hay versión nueva, instala. Si instala, reinicia el ESP.
    GhUpdateResult run(const AppConfig &cfg);

    const GhUpdateStatus &status() const { return st; }
    static bool signingEnabled();
    static int compareVersions(const String &a, const String &b);
    static const char *resultText(GhUpdateResult r);

private:
    GhUpdateStatus st;
    int lastCheckYday = -1;
};

#endif
