/**
 * reset_log.cpp - Registro de los últimos reinicios
 */
#include "reset_log.h"
#include "version.h"
#include <LittleFS.h>
#include <user_interface.h>

static const char LOG_FILE[] = "/resets.csv";
static uint32_t g_minHeap = UINT32_MAX;

namespace ResetLog {

const __FlashStringHelper *reasonText(uint8_t reason) {
    switch (reason) {
        case REASON_DEFAULT_RST:      return F("Encendido");
        case REASON_WDT_RST:          return F("Bloqueo (watchdog de hardware)");
        case REASON_EXCEPTION_RST:    return F("Excepción (fallo del programa)");
        case REASON_SOFT_WDT_RST:     return F("Bloqueo (watchdog de software)");
        case REASON_SOFT_RESTART:     return F("Reinicio por software");
        case REASON_DEEP_SLEEP_AWAKE: return F("Despertar de reposo profundo");
        case REASON_EXT_SYS_RST:      return F("Botón de reset");
        default:                      return F("Desconocido");
    }
}

const __FlashStringHelper *exceptionText(uint8_t cause) {
    switch (cause) {
        case 0:  return F("IllegalInstruction");
        case 2:  return F("InstructionFetchError");
        case 3:  return F("LoadStoreError (lectura indebida de flash)");
        case 6:  return F("IntegerDivideByZero");
        case 9:  return F("LoadStoreAlignment");
        case 28: return F("LoadProhibited (puntero nulo o no válido)");
        case 29: return F("StoreProhibited (puntero nulo o no válido)");
        default: return F("Otra");
    }
}

// Formato de cada línea: epoch;reason;exccause;epc1;excvaddr;version
static bool parseLine(const String &line, Entry &e) {
    int p[5], from = 0;
    for (uint8_t i = 0; i < 5; i++) {
        p[i] = line.indexOf(';', from);
        if (p[i] < 0) return false;
        from = p[i] + 1;
    }
    e.epoch = strtoul(line.substring(0, p[0]).c_str(), nullptr, 10);
    e.reason = line.substring(p[0] + 1, p[1]).toInt();
    e.exccause = line.substring(p[1] + 1, p[2]).toInt();
    e.epc1 = strtoul(line.substring(p[2] + 1, p[3]).c_str(), nullptr, 16);
    e.excvaddr = strtoul(line.substring(p[3] + 1, p[4]).c_str(), nullptr, 16);
    String v = line.substring(p[4] + 1);
    v.trim();
    strlcpy(e.version, v.c_str(), sizeof(e.version));
    return true;
}

void record(time_t now) {
    struct rst_info *ri = ESP.getResetInfoPtr();
    char line[64];
    snprintf_P(line, sizeof(line), PSTR("%lu;%u;%u;%08lx;%08lx;%s\n"),
               (unsigned long)(now > 0 ? now : 0), (unsigned)ri->reason, (unsigned)ri->exccause,
               (unsigned long)ri->epc1, (unsigned long)ri->excvaddr, FIRMWARE_VERSION);
    Serial.printf_P(PSTR("[Reset] Motivo: %u (excepcion %u en 0x%08lx)\n"),
                    (unsigned)ri->reason, (unsigned)ri->exccause, (unsigned long)ri->epc1);

    // Conserva las (MAX_ENTRIES - 1) últimas líneas y añade la nueva
    String keep;
    File f = LittleFS.open(LOG_FILE, "r");
    if (f) {
        String lines[MAX_ENTRIES];
        uint8_t n = 0;
        while (f.available()) {
            String l = f.readStringUntil('\n');
            if (l.length() < 8) continue;
            if (n == MAX_ENTRIES) {               // desplaza para quedarse con las últimas
                for (uint8_t i = 1; i < MAX_ENTRIES; i++) lines[i - 1] = lines[i];
                n--;
            }
            lines[n++] = l;
        }
        f.close();
        uint8_t start = n >= MAX_ENTRIES ? n - (MAX_ENTRIES - 1) : 0;
        for (uint8_t i = start; i < n; i++) { keep += lines[i]; keep += '\n'; }
    }
    f = LittleFS.open(LOG_FILE, "w");
    if (!f) return;
    f.print(keep);
    f.print(line);
    f.close();
}

uint8_t load(Entry *out, uint8_t max) {
    Entry all[MAX_ENTRIES];
    uint8_t n = 0;
    File f = LittleFS.open(LOG_FILE, "r");
    if (!f) return 0;
    while (f.available() && n < MAX_ENTRIES) {
        String l = f.readStringUntil('\n');
        if (parseLine(l, all[n])) n++;
    }
    f.close();
    uint8_t m = n < max ? n : max;
    for (uint8_t i = 0; i < m; i++) out[i] = all[n - 1 - i];   // más reciente primero
    return m;
}

void noteHeap() {
    uint32_t h = ESP.getFreeHeap();
    if (h < g_minHeap) g_minHeap = h;
}

uint32_t minFreeHeap() {
    return g_minHeap == UINT32_MAX ? ESP.getFreeHeap() : g_minHeap;
}

}  // namespace ResetLog
