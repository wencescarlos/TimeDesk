# TimeDesk

Reloj y estación meteorológica de escritorio para **ESP8266** con pantalla táctil a color. Muestra la hora con 6 estilos visuales, el tiempo actual, el pronóstico y la fase lunar, y se configura por completo desde el navegador. Se actualiza solo cada día desde GitHub.

## Características

- **6 estilos de reloj** (Clásico, Retro, Anime, Arcade, Portátil y Futurista) que se cambian en caliente desde la web o desde la pantalla táctil.
- **Tiempo y pronóstico** de OpenWeatherMap, con fases lunares y salida y puesta del sol.
- **Portal web de configuración**: WiFi (con búsqueda de redes), API, ubicación, idioma, zona horaria, unidades, brillo e intervalo de actualización.
- **Primer arranque sin cables**: crea su propio punto de acceso con portal cautivo.
- **Horario de pantalla**: se apaga y se enciende sola a las horas que elijas.
- **Actualización automática diaria** desde las Releases de GitHub, con verificación opcional por firma RSA.
- **Actualización manual OTA** subiendo el `.bin` desde el navegador.
- **Menú de ajustes táctil** en el propio dispositivo.
- Configuración guardada en JSON en la memoria flash (LittleFS).

## Hardware

- **ESP8266**: NodeMCU v2 o Wemos D1 Mini.
- **Pantalla ILI9341** TFT de 240x320 (SPI).
- **Panel táctil XPT2046**.

### Conexiones

| Señal | Pin ESP8266 |
|-------|-------------|
| TFT CS | D1 |
| TFT DC | D2 |
| TFT LED (retroiluminación) | D8 |
| Touch CS | D3 |
| Touch IRQ | D4 |
| SPI CLK | D5 (SCK) |
| SPI MOSI | D7 (MOSI) |
| SPI MISO | D6 (MISO) |

## Instalación

1. Instala [PlatformIO](https://platformio.org/) (por ejemplo, la extensión de VS Code).
2. Clona este repositorio y abre la carpeta.
3. Conecta el ESP8266 por USB, compila y sube el firmware:
   ```bash
   pio run -t upload
   ```

Las librerías se descargan solas al compilar:

| Librería | Uso |
|----------|-----|
| `thingpulse/ESP8266 Weather Station` | Cliente de OpenWeatherMap y cálculo solar/lunar |
| `squix78/Mini Grafx` | Gráficos y framebuffer de la pantalla |
| `squix78/JsonStreamingParser` | Lectura de las respuestas de la API |
| `paulstoffregen/XPT2046_Touchscreen` | Panel táctil |
| `bblanchon/ArduinoJson` | Configuración persistente |

## Primer uso

1. Al encenderlo por primera vez, mantén pulsada la pantalla si quieres calibrar el táctil (la calibración se guarda y no vuelve a pedirse).
2. Sin WiFi configurado, el dispositivo crea un **punto de acceso**:
   - Red: `TimeDesk-Config`
   - Contraseña: `timedesk1234`
3. Conéctate desde el móvil o el PC. El portal se abre solo; si no, ve a `http://192.168.4.1`.
4. Rellena:
   - **WiFi**: busca tu red y escribe la contraseña.
   - **OpenWeatherMap**: tu clave de API y el ID de tu ubicación.
   - **Zona horaria**, **idioma** y **unidades**.
5. Pulsa **Guardar y Reiniciar**.

### Clave de OpenWeatherMap

- Consigue una clave gratuita en <https://openweathermap.org/api>.
- Busca el ID de tu ciudad en <https://openweathermap.org/find>: es el número que aparece al final de la URL de la ciudad.

## Portal web

Cuando el dispositivo está conectado a tu WiFi, el portal sigue disponible en `http://<IP>` (la IP aparece en la pantalla de Ajustes y en la de información).

| Ruta | Descripción |
|------|-------------|
| `/` | Configuración completa |
| `/clock` | Estilo del reloj, brillo y pantalla de inicio (sin reiniciar) |
| `/schedule` | Horario de encendido y apagado de la pantalla |
| `/update` | Actualización automática desde GitHub y subida manual de firmware |
| `/status` | Estado del dispositivo (JSON) |
| `/update-info` | Estado de la actualización automática (JSON) |
| `/scan` | Redes WiFi cercanas (JSON) |
| `/reset` | Restaurar valores de fábrica |

## Estilos de reloj

En `http://<IP>/clock` aparece una vista previa animada de cada estilo; al pulsar uno se aplica al momento. También se cambia tocando la fila **Estilo reloj** en la pantalla de Ajustes.

| Estilo | Colores | Pantalla de reloj completa |
|--------|---------|----------------------------|
| **Clásico** | Blanco, amarillo y azul | Hora grande, barra de segundos y tiempo actual |
| **Retro** | Ámbar sobre negro | LCD de 7 segmentos con segmentos «fantasma», días de la semana y AM/PM |
| **Anime** | Índigo, rosa y lavanda | Viñeta con líneas de velocidad, brillos, carita que parpadea y bocadillo con el tiempo |
| **Arcade** | Negro, blanco, amarillo y rojo | Pixel art de 8 bits: «nivel» (día del año), vidas, barra de segundos y mascota que salta |
| **Portátil** | 4 tonos de verde | Como Arcade, dentro del marco de una consola de bolsillo |
| **Futurista** | Cian y magenta neón | Panel de datos con anillo de 60 marcas de segundos |

Cada estilo cambia la paleta de toda la interfaz y el reloj de la cabecera de la pantalla principal. La pantalla trabaja con 4 colores (2 bits por píxel) para que el framebuffer quepa en la RAM del ESP8266. Todos los estilos se dibujan con primitivas, sin imágenes, así que no ocupan espacio extra.

Para añadir un estilo nuevo:

1. Añade una entrada a `ClockStyle` en `include/clock_styles.h` y su paleta en `PALETTES` (`src/clock_styles.cpp`).
2. Escribe sus funciones `header*` y `full*` y añádelas a `clockDrawHeader` y `clockDrawFull`.
3. Sube `CLOCK_STYLE_MAX` en `include/config.h`.
4. Añade su vista previa en `include/clock_page.h`.

## Uso en el dispositivo

Se navega tocando zonas de la pantalla:

| Zona | Acción |
|------|--------|
| **Franja superior** | Alternar formato 12 h / 24 h |
| **Mitad izquierda** | Pantalla siguiente |
| **Mitad derecha** | Pantalla anterior |
| **Franja inferior** | Ir a Ajustes |

Pantallas, en orden:

1. **Principal**: hora, fecha, tiempo actual, pronóstico en carrusel y fase lunar.
2. **Reloj**: reloj a pantalla completa con el estilo elegido.
3. **Detalle**: temperatura, sensación térmica, viento, humedad, presión, nubosidad y visibilidad.
4. **Pronóstico 1** y 5. **Pronóstico 2**: próximos periodos.
6. **Información**: versión, memoria, señal WiFi, IP y tiempo encendido.
7. **Ajustes**: menú táctil.

### Ajustes táctiles

- Formato 12 h / 24 h.
- Unidades métricas o imperiales.
- Horario automático de pantalla, con horas de encendido y apagado (botones − y +).
- Estilo del reloj.
- **Guardar** para conservar los cambios tras un reinicio.

## Actualización automática desde GitHub

Una vez al día (por defecto entre las 04:00 y las 04:45) el dispositivo lee `version.txt` de la **última Release** del repositorio. Si la versión es mayor que la instalada, descarga `firmware.bin`, lo instala y se reinicia. En `/update` puedes activarla o desactivarla, cambiar el repositorio y la hora, y pulsar **Buscar ahora**.

El repositorio debe ser público: el dispositivo descarga sin credenciales.

### Publicar una versión nueva

1. Sube el número de versión en `include/version.h` (por ejemplo, `1.3.0`).
2. Haz commit y crea el tag correspondiente:
   ```bash
   git tag v1.3.0
   git push origin v1.3.0
   ```
   También puedes lanzar el workflow **Release firmware** a mano desde la pestaña *Actions*.
3. `.github/workflows/release.yml` compila el firmware y publica una Release con `firmware.bin` y `version.txt`. Si el tag no coincide con `version.h`, el workflow falla a propósito.

Los dispositivos solo comparan números de versión: **no publiques nunca una Release sin subir la versión**.

### Firma del firmware (recomendado)

La descarga va por HTTPS, pero el ESP8266 no comprueba el certificado de GitHub. Para que nadie pueda instalarte un firmware falso, firma las Releases:

```bash
openssl genrsa -out ota_private.key 2048
openssl rsa -in ota_private.key -pubout -out ota_public.key
```

- Pega el contenido de `ota_public.key` en `include/ota_public_key.h`.
- Crea el secreto `OTA_SIGNING_KEY` en GitHub (*Settings → Secrets and variables → Actions*) con el contenido de `ota_private.key`. **No subas nunca la clave privada al repositorio.**

Con la clave pública compilada, el dispositivo rechaza cualquier actualización automática que no esté firmada con tu clave. La subida manual desde `/update` sigue aceptando binarios sin firmar. La primera versión con clave hay que instalarla por USB o con la subida manual.

## Configuración avanzada

Constantes de `include/config.h`:

| Constante | Valor por defecto | Descripción |
|-----------|-------------------|-------------|
| `SLEEP_INTERVAL_SECS` | `30` | Segundos sin tocar la pantalla antes de apagarla (solo dentro del horario de apagado) |
| `MAX_FORECASTS` | `12` | Periodos de pronóstico descargados |
| `WEB_SERVER_PORT` | `80` | Puerto del portal web |
| `WIFI_HOSTNAME` | `TimeDesk` | Nombre con el que aparece en el router |
| `AP_SSID` | `TimeDesk-Config` | Nombre del punto de acceso de configuración |
| `AP_PASSWORD` | `timedesk1234` | Contraseña del punto de acceso |
| `DEFAULT_UPDATE_REPO` | `wencescarlos/TimeDesk` | Repositorio de las actualizaciones automáticas |
| `DEFAULT_UPDATE_HOUR` | `4` | Hora de la comprobación diaria |

El intervalo de actualización del tiempo (5-60 min), el brillo y el resto de opciones se cambian desde el portal web.

## Estructura del proyecto

```
TimeDesk/
├── platformio.ini            # Placa, librerías y opciones de compilación
├── include/
│   ├── version.h             # Versión del firmware
│   ├── config.h              # Pines, constantes y estructura de configuración
│   ├── config_manager.h      # Lectura y escritura de la configuración (LittleFS + JSON)
│   ├── web_portal.h          # Portal web
│   ├── clock_styles.h        # API de los estilos de reloj
│   ├── clock_page.h          # Página /clock (guardada en flash)
│   ├── github_updater.h      # Actualización automática desde GitHub
│   ├── ota_public_key.h      # Clave pública para verificar la firma (opcional)
│   ├── TouchControllerWS.h   # Controlador y calibración del táctil
│   ├── TZinfo.h              # Zonas horarias
│   ├── ArialRounded.h        # Fuentes
│   ├── moonphases.h          # Fuente de fases lunares
│   └── weathericons.h        # Iconos del tiempo
├── src/
│   ├── main.cpp              # Programa principal y pantallas
│   ├── web_portal.cpp        # Portal web
│   ├── clock_styles.cpp      # Paletas y dibujo de los estilos de reloj
│   ├── github_updater.cpp    # Comprobación diaria e instalación desde Releases
│   └── TouchControllerWS.cpp # Táctil
├── resources/moonphases/     # Imágenes fuente de las fases lunares
├── tools/
│   └── sign_firmware.py      # Firma de firmware.bin (lo usa el workflow)
└── .github/workflows/
    └── release.yml           # Compila y publica la Release al crear un tag vX.Y.Z
```
