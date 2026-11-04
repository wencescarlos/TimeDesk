/**
 * ota_public_key.h - Clave pública para verificar el firmware de GitHub
 *
 * Si pegas aquí una clave pública RSA, el dispositivo SOLO instalará
 * actualizaciones automáticas firmadas con la clave privada correspondiente
 * (el workflow de GitHub las firma si existe el secreto OTA_SIGNING_KEY).
 * Así nadie puede colarte un firmware falso aunque intercepte la conexión.
 *
 * Generar el par de claves (una sola vez, en tu PC):
 *   openssl genrsa -out ota_private.key 2048
 *   openssl rsa -in ota_private.key -pubout -out ota_public.key
 *
 * - Contenido de ota_public.key  -> pégalo abajo, entre las dos líneas R"KEY( y )KEY"
 * - Contenido de ota_private.key -> GitHub > Settings > Secrets and variables >
 *   Actions > New repository secret, con nombre OTA_SIGNING_KEY
 *   ¡No subas nunca ota_private.key al repositorio!
 *
 * Si se deja vacía, las actualizaciones se descargan por HTTPS pero sin firma.
 */
#ifndef OTA_PUBLIC_KEY_H
#define OTA_PUBLIC_KEY_H

#include <Arduino.h>

static const char OTA_PUBLIC_KEY[] PROGMEM = R"KEY(
)KEY";

#endif
