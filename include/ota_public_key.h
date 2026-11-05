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
-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxF/1jqJzCwJ/ctR/AVuy
OVuyuzNdoKH7hjYeIF7hb10+s6bgn0Mzll/DaI/VTx0TYfuWAck34rTkZxYais8B
XOToEtlDQqzURqbw+cGaaL33reXslHbrchOpfR+RjII+Cwod3T5kmgMnQWZw2ZDR
Ar7DPTWL6M6YqZFB0osQX7u9XQVRnDbGCnrpIo/C/c6aqTy4qIXG3lPCNaDKx9CI
T+MbIgi+/JdkJ6EED0A2M0OkhapWUi9xdbcCLBJ8RcuQCxY2w4Boo0GvEd20+dDT
d6p0+rsll874eH59NyZjpdkoWFjSlCFStl99OxmolsCFbfiVuxx8bfohf1+3F+xa
CwIDAQAB
-----END PUBLIC KEY-----
)KEY";

#endif
