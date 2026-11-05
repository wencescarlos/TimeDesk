/**
 * reset_log.h - Registro de los últimos reinicios y de la memoria mínima
 *
 * En cada arranque se guarda en LittleFS (/resets.csv) el motivo del reinicio
 * anterior que da el ESP8266 (encendido, excepción, watchdog, software...),
 * con la causa y la dirección si fue una excepción. Se conservan los 10
 * últimos y se muestran en la página /estado del portal.
 */
#ifndef RESET_LOG_H
#define RESET_LOG_H

#include <Arduino.h>
#include <time.h>

namespace ResetLog {
    static const uint8_t MAX_ENTRIES = 10;

    struct Entry {
        uint32_t epoch;      // hora del arranque (0 si aún no había hora)
        uint8_t reason;      // REASON_* del SDK
        uint8_t exccause;    // causa de la excepción (si reason == 2)
        uint32_t epc1;       // dirección del código que falló
        uint32_t excvaddr;   // dirección de memoria implicada
        char version[12];    // firmware que arrancó
    };

    // Guarda el motivo de este arranque. Llamar una vez, con la hora si ya la hay.
    void record(time_t now);

    // Lee las entradas guardadas, de la más reciente a la más antigua.
    uint8_t load(Entry *out, uint8_t max);

    // Texto legible del motivo (p.ej. "Excepción (fallo del programa)")
    const __FlashStringHelper *reasonText(uint8_t reason);
    // Nombre de la excepción (p.ej. "LoadStoreError")
    const __FlashStringHelper *exceptionText(uint8_t cause);

    // Memoria libre mínima vista desde el arranque (llamar a noteHeap() a menudo)
    void noteHeap();
    uint32_t minFreeHeap();
}

#endif
