/**
 * github_updater.cpp - Actualización automática desde GitHub Releases
 */
#include "github_updater.h"
#include "version.h"
#include "ota_public_key.h"
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <WiFiClientSecureBearSSL.h>

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
        case GH_INTERRUPTED:   return "Interrumpida";
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

// ── Petición y resultado de la actualización en la memoria RTC ──────────────
struct RtcUpdateRequest { uint32_t magic; uint32_t requestedAt; };
static const uint32_t RTC_REQ_MAGIC = 0x54444B52;   // "TDKR"
static const uint32_t RTC_REQ_OFFSET = 36;

struct RtcUpdateStatus {
    uint32_t magic;
    uint32_t result;
    uint32_t lastCheck;
    char latest[16];
    char message[128];
};
static const uint32_t RTC_ST_MAGIC = 0x54444B53;    // "TDKS"
static const uint32_t RTC_ST_OFFSET = 40;           // 39 bloques: 40..78

void GithubUpdater::requestBootUpdate() {
    RtcUpdateRequest r = {RTC_REQ_MAGIC, (uint32_t)time(nullptr)};
    ESP.rtcUserMemoryWrite(RTC_REQ_OFFSET, (uint32_t *)&r, sizeof(r));
    Serial.println(F("[OTA-GH] Reiniciando para comprobar actualizaciones"));
    delay(200);
    ESP.restart();
}

bool GithubUpdater::takeBootUpdateRequest(uint32_t &requestedAt) {
    RtcUpdateRequest r;
    if (!ESP.rtcUserMemoryRead(RTC_REQ_OFFSET, (uint32_t *)&r, sizeof(r)) || r.magic != RTC_REQ_MAGIC) return false;
    requestedAt = r.requestedAt;
    RtcUpdateRequest none = {0, 0};             // se borra antes de empezar: nunca en bucle
    ESP.rtcUserMemoryWrite(RTC_REQ_OFFSET, (uint32_t *)&none, sizeof(none));
    return true;
}

void GithubUpdater::saveStatus() {
    RtcUpdateStatus s;
    memset(&s, 0, sizeof(s));
    s.magic = RTC_ST_MAGIC;
    s.result = st.result;
    s.lastCheck = st.lastCheck;
    strlcpy(s.latest, st.latestVersion.c_str(), sizeof(s.latest));
    strlcpy(s.message, st.message.c_str(), sizeof(s.message));
    ESP.rtcUserMemoryWrite(RTC_ST_OFFSET, (uint32_t *)&s, sizeof(s));
}

void GithubUpdater::loadStatus() {
    RtcUpdateStatus s;
    if (!ESP.rtcUserMemoryRead(RTC_ST_OFFSET, (uint32_t *)&s, sizeof(s)) || s.magic != RTC_ST_MAGIC) return;
    s.latest[sizeof(s.latest) - 1] = 0;
    s.message[sizeof(s.message) - 1] = 0;
    st.result = (GhUpdateResult)s.result;
    st.lastCheck = s.lastCheck;
    st.latestVersion = s.latest;
    st.message = s.message;
}

// Destino de los pasos que anota el cliente HTTPS (solo hay un actualizador)
static GithubUpdater *crumbTarget = nullptr;

void GithubUpdater::crumb(const String &step) {
    st.result = GH_INTERRUPTED;
    st.message = String(F("Se corto en: ")) + step;
    saveStatus();
    Serial.printf_P(PSTR("[OTA-GH] %s\n"), step.c_str());
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

// ── Cliente HTTPS mínimo ────────────────────────────────────────────────────
// GitHub sirve los ficheros de las Releases tras dos redirecciones; la última
// lleva a una URL firmada de ~900 caracteres. HTTPClient copia esa cabecera en
// varios String mientras la conexión TLS ocupa ~25 KB: si alguna copia no cabe
// en memoria se queda vacía sin avisar, no sigue la redirección y devuelve 302.
// Además, cada 302 de github.com trae una cabecera Content-Security-Policy de
// ~4.000 caracteres: HTTPClient la copia entera (y sus partes) en String.
// Este cliente lee las cabeceras línea a línea en un único búfer fijo,
// recorta las que no necesita, cierra cada conexión antes de seguir la
// redirección y explica cualquier fallo.
namespace {

const size_t URL_MAX  = 1536;   // URLs firmadas de GitHub: ~900 caracteres
const size_t LINE_MAX = 1536;
const uint32_t NET_TIMEOUT_MS = 15000;
const uint8_t MAX_HOPS = 6;

// La conexión TLS necesita un bloque de ~16,7 KB seguidos: se informa del
// total libre y del mayor bloque, que es lo que decide si se puede conectar.
// ── Guardián del saludo TLS ─────────────────────────────────────────────
// Los cálculos de curva elíptica del saludo TLS pasan varios segundos sin
// ceder la CPU y el saludo completo puede pasar de 6 s. El watchdog de
// software (~3 s) reiniciaba el equipo, y si solo se para, el de hardware
// salta a los ~6 s porque nadie lo alimenta. Mientras dura client.connect()
// se para el de software y una interrupción del temporizador 0 alimenta el
// de hardware cada 0,5 s, con un límite propio: pasados TLS_GUARD_S segundos
// deja de alimentarlo y el watchdog reinicia el equipo igualmente.
// Se usa el temporizador 0 (comparador de ciclos de la CPU) y no el 1: el
// PWM del brillo deja el 1 enrutado como NMI, incluso tras ESP.restart(), y
// la interrupción saltaba a una dirección inválida (1.4.8 y 1.4.9).
static const uint16_t TLS_GUARD_S = 40;
static volatile uint16_t tlsGuardTicks = 0;
static uint32_t tlsGuardPeriod = 0;   // ciclos de CPU en 0,5 s

static void IRAM_ATTR tlsGuardIsr() {
    if (tlsGuardTicks) {
        tlsGuardTicks--;
        WDT_FEED();
        timer0_write(ESP.getCycleCount() + tlsGuardPeriod);
    }
}

void tlsGuardStart() {
    tlsGuardPeriod = (uint32_t)ESP.getCpuFreqMHz() * 500000UL;
    tlsGuardTicks = TLS_GUARD_S * 2;
    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(tlsGuardIsr);
    timer0_write(ESP.getCycleCount() + tlsGuardPeriod);
    interrupts();
    ESP.wdtDisable();
}

void tlsGuardStop() {
    ESP.wdtEnable(0);
    tlsGuardTicks = 0;
    timer0_detachInterrupt();
}

String heapNote() {
    return String(F(" (memoria ")) + String(ESP.getFreeHeap() / 1024) + F(" KB, bloque mayor ") +
           String(ESP.getMaxFreeBlockSize() / 1024) + F(" KB)");
}

struct HttpsGet {
    BearSSL::WiFiClientSecure client;
    char *url = nullptr;           // URL actual (se reutiliza para la redirección)
    char *line = nullptr;          // última línea de cabecera leída
    char host[96] = {0};
    bool truncated = false;        // la última línea no cabía y se ha recortado
    int code = 0;
    long length = -1;              // Content-Length (-1 si no viene)
    uint8_t hop = 0;
    String error;

    // Los búferes los reserva GithubUpdater antes de liberar el framebuffer
    bool init(char *urlBuf, char *lineBuf) {
        url = urlBuf;
        line = lineBuf;
        if (!url || !line) { error = String(F("Sin memoria para la descarga")) + heapNote(); return false; }
        return true;
    }
    ~HttpsGet() { client.stop(); }

    // Lee una línea terminada en \n (sin \r). Si no cabe en el búfer se
    // guarda el principio y se marca 'truncated'. false si se agota el tiempo
    // o se cierra la conexión sin datos.
    bool readLine() {
        size_t n = 0;
        bool overflow = false;
        unsigned long last = millis();
        for (uint16_t i = 0;; i++) {
            if ((i & 63) == 0) yield();            // cabeceras de 4 KB: no acaparar la CPU
            if (client.available()) {
                int c = client.read();
                if (c < 0) { delay(1); continue; }
                last = millis();
                if (c == '\n') break;
                if (c == '\r') continue;
                if (n < LINE_MAX - 1) line[n++] = (char)c; else overflow = true;
            } else if (!client.connected()) {
                if (n == 0) { error = String(F("Conexion cerrada por ")) + host; return false; }
                break;
            } else if (millis() - last > NET_TIMEOUT_MS) {
                error = String(F("Sin respuesta de ")) + host;
                return false;
            } else {
                delay(1);
            }
        }
        line[n] = 0;
        truncated = overflow;
        return true;
    }

    // Abre la URL siguiendo redirecciones. Si devuelve true, 'code' y 'length'
    // están listos y el cuerpo de la respuesta se puede leer con readBody().
    bool open(const String &startUrl) {
        if (startUrl.length() >= URL_MAX) { error = F("URL demasiado larga"); return false; }
        strlcpy(url, startUrl.c_str(), URL_MAX);
        for (hop = 1; hop <= MAX_HOPS; hop++) {
            if (strncmp_P(url, PSTR("https://"), 8) != 0) { error = F("La redireccion no es https"); return false; }
            const char *h = url + 8;
            const char *slash = strchr(h, '/');
            size_t hl = slash ? (size_t)(slash - h) : strlen(h);
            if (hl == 0 || hl >= sizeof(host)) { error = F("Servidor no valido en la URL"); return false; }
            memcpy(host, h, hl);
            host[hl] = 0;
            const char *path = slash ? slash : "/";

            if (crumbTarget) crumbTarget->crumb(String(F("salto ")) + hop + F(", conectando con ") + host + heapNote());
            client.stop();                     // libera la conexión del salto anterior
            client.setInsecure();              // la autenticidad la da la firma RSA, no el canal
            client.setTimeout(NET_TIMEOUT_MS);
            unsigned long t0 = millis();
            tlsGuardStart();
            bool connected = client.connect(host, 443);
            tlsGuardStop();
            unsigned long tlsMs = millis() - t0;
            if (!connected) {
                error = String(F("Salto ")) + hop + F(": no conecta con ") + host + heapNote();
                return false;
            }
            client.print(F("GET "));
            client.print(path);
            client.print(F(" HTTP/1.1\r\nHost: "));
            client.print(host);
            client.print(F("\r\nUser-Agent: TimeDesk-ESP8266\r\nAccept: */*\r\nConnection: close\r\n\r\n"));

            if (crumbTarget) crumbTarget->crumb(String(F("salto ")) + hop + F(", respuesta de ") + host +
                                                F(" (TLS ") + (tlsMs / 1000) + '.' + ((tlsMs / 100) % 10) + F(" s)"));
            // Línea de estado: "HTTP/1.1 302 Found"
            if (!readLine()) { error = String(F("Salto ")) + hop + F(": ") + error + heapNote(); return false; }
            if (strncmp_P(line, PSTR("HTTP/1."), 7) != 0 || strlen(line) < 12) {
                error = String(F("Respuesta no valida de ")) + host;
                return false;
            }
            code = atoi(line + 9);
            length = -1;
            bool hasLocation = false, chunked = false;
            for (;;) {
                if (!readLine()) { error = String(F("Salto ")) + hop + F(": ") + error + heapNote(); return false; }
                if (line[0] == 0) break;                       // fin de cabeceras
                char *colon = strchr(line, ':');
                if (!colon) continue;
                *colon = 0;
                char *v = colon + 1;
                while (*v == ' ' || *v == '\t') v++;
                if (strcasecmp_P(line, PSTR("location")) == 0) {
                    if (truncated) { error = F("Redireccion demasiado larga"); return false; }
                    if (v[0] == '/') {                          // relativa: mismo servidor
                        if (8 + strlen(host) + strlen(v) >= URL_MAX) { error = F("Redireccion demasiado larga"); return false; }
                        snprintf_P(url, URL_MAX, PSTR("https://%s%s"), host, v);
                    } else {
                        if (strlen(v) >= URL_MAX) { error = F("Redireccion demasiado larga"); return false; }
                        strlcpy(url, v, URL_MAX);
                    }
                    hasLocation = true;
                } else if (strcasecmp_P(line, PSTR("content-length")) == 0) {
                    length = atol(v);
                } else if (strcasecmp_P(line, PSTR("transfer-encoding")) == 0 && strstr_P(v, PSTR("chunked"))) {
                    chunked = true;
                }
            }
            if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
                if (!hasLocation) {
                    error = String(F("Salto ")) + hop + F(": HTTP ") + code + F(" sin destino desde ") + host;
                    return false;
                }
                continue;                                       // siguiente salto
            }
            if (code == 200 && chunked) { error = String(F("Respuesta troceada no soportada de ")) + host; return false; }
            return true;
        }
        error = F("Demasiadas redirecciones");
        return false;
    }

    // Lee hasta 'len' bytes del cuerpo. Devuelve los leídos (menos si se corta).
    size_t readBody(uint8_t *buf, size_t len) {
        size_t got = 0;
        unsigned long last = millis();
        while (got < len) {
            int avail = client.available();
            if (avail > 0) {
                int n = client.read(buf + got, (size_t)avail < len - got ? (size_t)avail : len - got);
                if (n > 0) { got += n; last = millis(); }
            } else if (!client.connected()) {
                break;
            } else if (millis() - last > NET_TIMEOUT_MS) {
                break;
            } else {
                delay(1);
            }
            yield();
        }
        return got;
    }
};

}  // namespace

bool GithubUpdater::reserveBuffers() {
    if (!urlBuf) urlBuf = (char *)malloc(URL_MAX);
    if (!lineBuf) lineBuf = (char *)malloc(LINE_MAX);
    return urlBuf && lineBuf;
}

void GithubUpdater::releaseBuffers() {
    free(urlBuf);
    free(lineBuf);
    urlBuf = lineBuf = nullptr;
}

GhUpdateResult GithubUpdater::run(const AppConfig &cfg) {
    reserveBuffers();   // por si no se llamó antes (lo ideal es antes de liberar la pantalla)
    st.lastCheck = checkTime ? checkTime : time(nullptr);
    st.latestVersion = "";
    crumbTarget = this;

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
    {
        crumb(F("pidiendo version.txt"));
        HttpsGet g;
        if (!g.init(urlBuf, lineBuf) || !g.open(base + F("version.txt"))) {
            st.message = g.error;
            return st.result = GH_NETWORK_ERROR;
        }
        if (g.code == 404) {
            st.message = F("El repositorio no tiene Releases con version.txt");
            return st.result = GH_NO_RELEASE;
        }
        if (g.code != 200) {
            st.message = String(F("version.txt: HTTP ")) + g.code + F(" de ") + g.host;
            return st.result = GH_NETWORK_ERROR;
        }
        char v[24];
        size_t want = (g.length > 0 && g.length < (long)sizeof(v)) ? (size_t)g.length : sizeof(v) - 1;
        size_t n = g.readBody((uint8_t *)v, want);
        v[n] = 0;
        st.latestVersion = v;
    }   // aquí se cierra la conexión y se libera su memoria
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

    // La firma se comprueba aquí y no con Update.installSignature(): el core
    // calcula el SHA-256 de todo el firmware dentro de Update.end() sin ceder
    // nunca la CPU, y con ~500 KB salta el watchdog (reinicio sin resultado).
    // Aquí el hash se va calculando mientras llegan los datos.
    // Formato firmado: <binario> + <firma RSA> + <longitud firma, u32 LE>
    static BearSSL::PublicKey *pubKey = nullptr;
    static BearSSL::SigningVerifier *verifier = nullptr;
    const bool signedOnly = signingEnabled();
    if (signedOnly && !pubKey) {
        pubKey = new BearSSL::PublicKey(OTA_PUBLIC_KEY);
        verifier = new BearSSL::SigningVerifier(pubKey);
    }
    const uint32_t sigLen = signedOnly ? verifier->length() : 0;   // 256 con RSA-2048
    const uint32_t trailer = signedOnly ? sigLen + 4 : 0;
    BearSSL::HashSHA256 hash;
    uint8_t *sigBuf = nullptr;
    Update.installSignature(nullptr, nullptr);

    String error;
    {
        crumb(F("pidiendo firmware.bin"));
        HttpsGet g;
        if (!g.init(urlBuf, lineBuf) || !g.open(base + F("firmware.bin"))) {
            error = g.error;
        } else if (g.code != 200) {
            error = String(F("firmware.bin: HTTP ")) + g.code + F(" de ") + g.host;
        } else if (g.length <= (long)trailer + 16) {
            error = F("Descarga sin tamano conocido");
        } else if (signedOnly && sigLen == 0) {
            error = F("Clave publica no valida");
        } else if (signedOnly && !(sigBuf = (uint8_t *)malloc(trailer))) {
            error = String(F("Sin memoria para la firma")) + heapNote();
        } else if (!Update.begin(g.length)) {
            error = String(F("Sin espacio: ")) + Update.getErrorString();
        } else {
            Serial.printf_P(PSTR("[OTA-GH] Descargando %ld bytes de %s\n"), g.length, g.host);
            const uint32_t total = (uint32_t)g.length;
            const uint32_t payload = total - trailer;     // lo que cubre la firma
            hash.begin();
            uint8_t buf[1024];
            uint32_t done = 0, nextCrumb = 0;
            while (done < total) {
                if (done >= nextCrumb) {
                    crumb(String(F("descargando, ")) + (done / 1024) + F(" de ") + (total / 1024) + F(" KB") + heapNote());
                    nextCrumb += 32768;
                }
                size_t n = g.readBody(buf, total - done < sizeof(buf) ? total - done : sizeof(buf));
                if (n == 0) break;                            // corte o tiempo agotado
                if (signedOnly) {
                    for (size_t i = 0; i < n; ) {            // separa binario y firma
                        uint32_t pos = done + i;
                        if (pos < payload) {
                            size_t k = (payload - pos) < (n - i) ? (payload - pos) : (n - i);
                            hash.add(buf + i, k);
                            i += k;
                        } else {
                            sigBuf[pos - payload] = buf[i++];
                        }
                    }
                }
                if (Update.write(buf, n) != n) break;
                done += n;
                yield();
            }
            if (done < total) {
                error = String(F("Descarga incompleta: ")) + done + F(" de ") + total + F(" bytes");
                Update.end();   // incompleta: end() sin argumento la descarta; el firmware actual sigue intacto
            } else {
                bool sigOk = true;
                if (signedOnly) {
                    crumb(F("comprobando la firma"));
                    hash.end();
                    uint32_t found = sigBuf[sigLen] | (sigBuf[sigLen + 1] << 8) |
                                     ((uint32_t)sigBuf[sigLen + 2] << 16) | ((uint32_t)sigBuf[sigLen + 3] << 24);
                    sigOk = found == sigLen && verifier->verify(&hash, sigBuf, sigLen);
                    yield();
                }
                if (!sigOk) {
                    // Sin Update.end(): no se marca para instalar; al reiniciar sigue el firmware actual
                    error = F("Verificacion: firma no valida");
                } else {
                    crumb(F("activando el firmware nuevo"));
                    if (!Update.end()) error = String(F("Verificacion: ")) + Update.getErrorString();
                }
            }
        }
    }
    free(sigBuf);

    if (error.length() == 0) {
        st.message = String(F("Actualizado a la version ")) + st.latestVersion;
        st.result = GH_UPDATED;
        saveStatus();                              // lo mostrará el firmware nuevo
        Serial.println(F("[OTA-GH] Actualizado. Reiniciando..."));
        delay(500);
        ESP.restart();
        return st.result = GH_UPDATED;
    }
    st.message = error;
    Serial.printf_P(PSTR("[OTA-GH] Error: %s\n"), st.message.c_str());
    return st.result = GH_INSTALL_ERROR;
}
