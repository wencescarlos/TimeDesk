/**
 * github_updater.cpp - Actualización automática desde GitHub Releases
 */
#include "github_updater.h"
#include "version.h"
#include "ota_public_key.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Updater.h>
#include <WiFiClientSecureBearSSL.h>

static const char USER_AGENT[] = "TimeDesk-ESP8266";

// ¿Contiene el texto 'needle' (en RAM) al texto 'hay' guardado en flash?
// Los datos PROGMEM solo pueden leerse con pgm_read_*: leerlos como RAM
// (p.ej. pasarlos como primer argumento de strstr_P) provoca una excepción
// y reinicia el ESP8266.
static bool pgmContains(PGM_P hay, const char *needle) {
    size_t n = strlen_P(hay), m = strlen(needle);
    for (size_t i = 0; i + m <= n; i++) {
        size_t j = 0;
        while (j < m && (char)pgm_read_byte(hay + i + j) == needle[j]) j++;
        if (j == m) return true;
    }
    return false;
}

bool GithubUpdater::signingEnabled() {
    static int8_t cached = -1;   // la clave no cambia: se comprueba una vez
    if (cached < 0) cached = pgmContains(OTA_PUBLIC_KEY, "BEGIN PUBLIC KEY") ? 1 : 0;
    return cached == 1;
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

// El día del último intento se guarda en la memoria RTC, que sobrevive a los
// reinicios por software o por excepción: si un intento reiniciara el equipo,
// no se repetiría en bucle durante la hora de comprobación.
struct RtcCheckMark { uint32_t magic; int32_t yday; };
static const uint32_t RTC_MAGIC = 0x54444B31;   // "TDK1"
static const uint32_t RTC_OFFSET = 32;          // bloques de 4 bytes (lejos del inicio)

static int rtcLoadYday() {
    RtcCheckMark m;
    if (ESP.rtcUserMemoryRead(RTC_OFFSET, (uint32_t *)&m, sizeof(m)) && m.magic == RTC_MAGIC) return m.yday;
    return -1;
}

static void rtcSaveYday(int yday) {
    RtcCheckMark m = {RTC_MAGIC, yday};
    ESP.rtcUserMemoryWrite(RTC_OFFSET, (uint32_t *)&m, sizeof(m));
}

bool GithubUpdater::isDue(const AppConfig &cfg) {
    if (!cfg.autoUpdate) return false;
    if (!rtcLoaded) { lastCheckYday = rtcLoadYday(); rtcLoaded = true; }
    time_t now = time(nullptr);
    if (now < NTP_MIN_VALID_EPOCH) return false;   // sin hora válida aún
    struct tm *t = localtime(&now);
    // Minuto "aleatorio" fijo por dispositivo para no ir todos a la vez
    uint8_t minuteOffset = ESP.getChipId() % 45;
    if (t->tm_hour != cfg.updateHour || t->tm_min < minuteOffset) return false;
    if (t->tm_yday == lastCheckYday) return false;
    lastCheckYday = t->tm_yday;   // un único intento al día, aunque falle
    rtcSaveYday(lastCheckYday);
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

    // Descarga con el mismo cliente HTTP/1.1 que version.txt (ESPhttpUpdate
    // fuerza HTTP/1.0 y GitHub lo redirige a otro servidor de descargas que
    // respondía con un código inesperado)
    String error;
    {
        BearSSL::WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        http.setUserAgent(USER_AGENT);
        http.setTimeout(15000);
        if (!http.begin(client, base + F("firmware.bin"))) {
            error = F("No se pudo iniciar la descarga");
        } else {
            int code = http.GET();
            int len = http.getSize();
            if (code != HTTP_CODE_OK) {
                error = String(F("Descarga: HTTP ")) + code +
                        (code < 0 ? String(F(" ")) + HTTPClient::errorToString(code) : String());
            } else if (len <= 0) {
                error = F("Descarga sin tamaño conocido");
            } else if (!Update.begin(len)) {
                error = String(F("Sin espacio: ")) + Update.getErrorString();
            } else {
                Serial.printf_P(PSTR("[OTA-GH] Descargando %d bytes\n"), len);
                size_t written = Update.writeStream(*http.getStreamPtr());
                if (written != (size_t)len) {
                    error = String(F("Descarga incompleta: ")) + written + F(" de ") + len + F(" bytes");
                    Update.end();   // incompleta: end() sin argumento la descarta; el firmware actual sigue intacto
                } else if (!Update.end()) {
                    // Aquí se rechaza un binario sin la firma correcta
                    error = String(F("Verificación: ")) + Update.getErrorString();
                }
            }
            http.end();
        }
    }
    Update.installSignature(nullptr, nullptr);   // la subida manual (/update) no exige firma

    if (error.length() == 0) {
        st.message = F("Actualizado, reiniciando");
        Serial.println(F("[OTA-GH] Actualizado. Reiniciando..."));
        delay(500);
        ESP.restart();
        return st.result = GH_UPDATED;
    }
    st.message = error;
    Serial.printf_P(PSTR("[OTA-GH] Error: %s\n"), st.message.c_str());
    return st.result = GH_INSTALL_ERROR;
}
