/**
 * github_updater.cpp - Actualización automática desde GitHub Releases
 */
#include "github_updater.h"
#include "version.h"
#include "ota_public_key.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>

static const char USER_AGENT[] = "TimeDesk-ESP8266";

bool GithubUpdater::signingEnabled() {
    return strstr_P(OTA_PUBLIC_KEY, PSTR("BEGIN PUBLIC KEY")) != nullptr;
}

// Compara "1.2.10" con "1.10.0" numéricamente. Devuelve <0, 0 o >0.
int GithubUpdater::compareVersions(const String &a, const String &b) {
    int ia = 0, ib = 0;
    for (uint8_t part = 0; part < 3; part++) {
        long va = 0, vb = 0;
        while (ia < (int)a.length() && isDigit(a[ia])) va = va * 10 + (a[ia++] - '0');
        while (ib < (int)b.length() && isDigit(b[ib])) vb = vb * 10 + (b[ib++] - '0');
        if (va != vb) return va < vb ? -1 : 1;
        if (ia < (int)a.length() && a[ia] == '.') ia++;
        if (ib < (int)b.length() && b[ib] == '.') ib++;
    }
    return 0;
}

const char *GithubUpdater::resultText(GhUpdateResult r) {
    switch (r) {
        case GH_UP_TO_DATE:    return "Al dia";
        case GH_UPDATED:       return "Actualizado";
        case GH_NO_RELEASE:    return "Sin Releases publicadas";
        case GH_NETWORK_ERROR: return "Error de red";
        case GH_INSTALL_ERROR: return "Error al instalar";
        case GH_BAD_CONFIG:    return "Repositorio no valido";
        default:               return "Nunca comprobado";
    }
}

// "usuario/repositorio" con caracteres válidos de GitHub
static bool validRepo(const char *repo) {
    uint8_t slashes = 0;
    size_t n = strlen(repo);
    if (n < 3) return false;
    for (size_t i = 0; i < n; i++) {
        char c = repo[i];
        if (c == '/') { slashes++; if (i == 0 || i == n - 1) return false; continue; }
        if (!isAlphaNumeric(c) && c != '-' && c != '_' && c != '.') return false;
    }
    return slashes == 1;
}

static bool validVersion(const String &v) {
    if (v.length() == 0 || v.length() > 16 || !isDigit(v[0])) return false;
    for (uint16_t i = 0; i < v.length(); i++) if (!isDigit(v[i]) && v[i] != '.') return false;
    return true;
}

bool GithubUpdater::isDue(const AppConfig &cfg) {
    if (!cfg.autoUpdate) return false;
    time_t now = time(nullptr);
    if (now < NTP_MIN_VALID_EPOCH) return false;   // sin hora válida aún
    struct tm *t = localtime(&now);
    // Minuto "aleatorio" fijo por dispositivo para no ir todos a la vez
    uint8_t minuteOffset = ESP.getChipId() % 45;
    if (t->tm_hour != cfg.updateHour || t->tm_min < minuteOffset) return false;
    if (t->tm_yday == lastCheckYday) return false;
    lastCheckYday = t->tm_yday;   // un único intento al día, aunque falle
    return true;
}

GhUpdateResult GithubUpdater::run(const AppConfig &cfg) {
    st.lastCheck = time(nullptr);
    st.latestVersion = "";

    if (!validRepo(cfg.updateRepo)) {
        st.message = F("Escribe el repositorio como usuario/nombre");
        return st.result = GH_BAD_CONFIG;
    }
    if (WiFi.status() != WL_CONNECTED) {
        st.message = F("WiFi desconectado");
        return st.result = GH_NETWORK_ERROR;
    }

    String base = String(F("https://github.com/")) + cfg.updateRepo + F("/releases/latest/download/");
    Serial.printf_P(PSTR("[OTA-GH] Comprobando %sversion.txt (heap %u)\n"), base.c_str(), ESP.getFreeHeap());

    // ── 1. Versión publicada ────────────────────────────────────────────────
    // HTTPS sin validar el certificado: la autenticidad del firmware la
    // garantiza la firma RSA (si está configurada), no el canal.
    {
        BearSSL::WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http.setUserAgent(USER_AGENT);
        http.setTimeout(15000);
        if (!http.begin(client, base + F("version.txt"))) {
            st.message = F("No se pudo iniciar la conexion");
            return st.result = GH_NETWORK_ERROR;
        }
        int code = http.GET();
        if (code == HTTP_CODE_NOT_FOUND) {
            http.end();
            st.message = F("El repositorio no tiene Releases con version.txt");
            return st.result = GH_NO_RELEASE;
        }
        if (code != HTTP_CODE_OK) {
            st.message = String(F("HTTP ")) + code + String(F(" ")) + (code < 0 ? HTTPClient::errorToString(code) : String());
            http.end();
            return st.result = GH_NETWORK_ERROR;
        }
        st.latestVersion = http.getString();
        http.end();
    }
    st.latestVersion.trim();
    if (!validVersion(st.latestVersion)) {
        st.message = F("version.txt no tiene formato X.Y.Z");
        st.latestVersion = "";
        return st.result = GH_NO_RELEASE;
    }

    if (compareVersions(st.latestVersion, FIRMWARE_VERSION) <= 0) {
        st.message = String(F("Version instalada ")) + FIRMWARE_VERSION;
        Serial.printf_P(PSTR("[OTA-GH] Al dia (%s)\n"), FIRMWARE_VERSION);
        return st.result = GH_UP_TO_DATE;
    }

    // ── 2. Descarga e instalación ───────────────────────────────────────────
    Serial.printf_P(PSTR("[OTA-GH] Nueva version %s -> instalando\n"), st.latestVersion.c_str());

    static BearSSL::PublicKey *pubKey = nullptr;
    static BearSSL::HashSHA256 *hash = nullptr;
    static BearSSL::SigningVerifier *verifier = nullptr;
    if (signingEnabled()) {
        if (!pubKey) {
            pubKey = new BearSSL::PublicKey(OTA_PUBLIC_KEY);
            hash = new BearSSL::HashSHA256();
            verifier = new BearSSL::SigningVerifier(pubKey);
        }
        Update.installSignature(hash, verifier);   // rechaza binarios sin firma válida
    }

    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    ESPhttpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, base + F("firmware.bin"), FIRMWARE_VERSION);

    // Solo se llega aquí si falló (si va bien, el ESP se reinicia)
    Update.installSignature(nullptr, nullptr);   // la subida manual (/update) no exige firma
    if (ret == HTTP_UPDATE_OK) {
        st.message = F("Actualizado");
        return st.result = GH_UPDATED;
    }
    st.message = ESPhttpUpdate.getLastErrorString();
    Serial.printf_P(PSTR("[OTA-GH] Error: %s\n"), st.message.c_str());
    return st.result = GH_INSTALL_ERROR;
}
