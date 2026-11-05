# TimeDesk

Reloj y estación meteorológica de escritorio para **ESP8266** con pantalla táctil a color. Muestra la hora con 20 estilos visuales, el tiempo actual, el pronóstico y la fase lunar, y se configura por completo desde el navegador. Se actualiza solo cada día desde GitHub.

## Características

- **20 estilos de reloj** (relojes, ambientes, informática y electrónica, videojuegos) que se cambian en caliente desde la web o desde la pantalla táctil, con vistas previas reales.
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

Cuando el dispositivo está conectado a tu WiFi, abre el portal desde cualquier equipo de tu red en:

- **<http://timedesk.local>**, sin necesidad de conocer la IP.
- `http://<IP>`, como alternativa. La IP aparece en las pantallas de Ajustes e Información.

El nombre `timedesk.local` funciona en Windows 10/11, macOS, iOS, Linux (con Avahi) y Android 12 o posterior. En dispositivos más antiguos o en redes que bloquean mDNS (algunas WiFi de invitados), usa la IP. Si tienes más de un TimeDesk en la misma red, dale a cada uno un nombre distinto cambiando `MDNS_NAME` en `include/config.h`.

| Ruta | Descripción |
|------|-------------|
| `/` | Configuración completa |
| `/clock` | Estilo del reloj, brillo y pantalla de inicio (sin reiniciar) |
| `/clock-preview.bmp?s=N` | Captura del estilo N tal como se vería en la pantalla |
| `/schedule` | Horario de encendido y apagado de la pantalla |
| `/update` | Actualización automática desde GitHub y subida manual de firmware |
| `/status` | Estado del dispositivo (JSON) |
| `/update-info` | Estado de la actualización automática (JSON) |
| `/scan` | Redes WiFi cercanas (JSON) |
| `/reset` | Restaurar valores de fábrica |

## Estilos de reloj

En `http://timedesk.local/clock` aparece una **vista previa real** de cada estilo: el propio dispositivo lo dibuja y lo envía como imagen, así que ves exactamente lo que mostrará la pantalla. Al pulsar uno se aplica al momento. También se cambia tocando la fila **Estilo reloj** en la pantalla de Ajustes.

**Relojes**

| Estilo | Pantalla de reloj completa |
|--------|----------------------------|
| **Clásico** | Hora grande, barra de segundos y tiempo actual |
| **Analógico** | Esfera con números, agujas, segundero y ventanilla con el día |
| **Retro** | LCD ámbar de 7 segmentos con segmentos «fantasma», días de la semana y AM/PM |
| **Futurista** | Panel de datos cian y magenta con anillo de 60 marcas de segundos |
| **Nixie** | Tubos de neón naranja con rejilla, zócalo y lámparas en los dos puntos |
| **Flip clock** | Tarjetas abatibles; al cambiar el minuto se ve la hoja a medio caer |

**Ambientes**

| Estilo | Pantalla de reloj completa |
|--------|----------------------------|
| **Según el tiempo** | La escena y la paleta cambian con el tiempo real: sol, luna, nubes, lluvia, tormenta con rayos, nieve o niebla |
| **Noche estrellada** | Estrellas que titilan, constelación, estrella fugaz y la luna con su fase real |
| **Acuario** | Peces que nadan, algas que se mecen, burbujas y la hora dentro de una burbuja |
| **Anime** | Viñeta con líneas de velocidad, brillos, carita que parpadea y bocadillo con el tiempo |

**Informática y electrónica**

| Estilo | Pantalla de reloj completa |
|--------|----------------------------|
| **Escritorio 1-bit** | Escritorio monocromo tramado con barra de menús y ventanas de título rayado |
| **Escritorio 3D** | Ventana biselada con barra de progreso de bloques y barra de tareas con bandeja |
| **Terminal** | Sesión de shell en fósforo verde (`date`, `uptime`, `weather`) con cursor parpadeante |
| **Micro 8-bit** | Pantalla de BASIC de un microordenador doméstico con marco de color |
| **Osciloscopio** | La hora trazada por el haz sobre la rejilla, onda viva, lecturas y mandos que giran |

**Videojuegos**

| Estilo | Pantalla de reloj completa |
|--------|----------------------------|
| **Arcade** | Pixel art 8-bit: «nivel» (día del año), vidas, barra de segundos y mascota que salta |
| **Portátil** | Como Arcade, en 4 tonos de verde dentro del marco de una consola de bolsillo |
| **Aventura** | Fantasía 16-bit: corazones de vida, oro, cofre que se abre y cuadro de diálogo con el tiempo |
| **Velocidad** | Carreras arcade 16-bit: hora en cursiva, sol con franjas y carretera en perspectiva en movimiento |
| **Plataformas** | Hora hecha de bloques, nubes, colinas y una mascota que recoge estrellas |

Todos los estilos son diseños originales inspirados en épocas y géneros; no reproducen logotipos, personajes ni pantallas de productos concretos.

Cada estilo cambia la paleta de toda la interfaz y el reloj de la cabecera de la pantalla principal. La pantalla trabaja con 4 colores (2 bits por píxel) para que el framebuffer quepa en la RAM del ESP8266. Todo se dibuja con primitivas, sin imágenes, así que no ocupa espacio extra.

Para añadir un estilo nuevo:

1. Añade una entrada a `ClockStyle` en `include/clock_styles.h`, y su paleta y su nombre en `PALETTES` y `STYLE_NAMES` (`src/clock_styles.cpp`).
2. Escribe sus funciones `header*` y `full*` y añádelas a `clockDrawHeader` y `clockDrawFull`.
3. Sube `CLOCK_STYLE_MAX` en `include/config.h`.
4. Añade su nombre, descripción y grupo a las listas `S` y `G` de `include/clock_page.h`. La vista previa se genera sola.

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
| `MDNS_NAME` | `timedesk` | Nombre del portal en la red local (`http://timedesk.local`) |
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
│   ├── bmp_writer.h          # Imágenes BMP de las vistas previas
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

## Licencia

TimeDesk se distribuye bajo la licencia **MIT** (ver [`LICENSE`](LICENSE)).

Los componentes de terceros que incluye o con los que se enlaza el firmware, con sus avisos de copyright y licencias, están en [`NOTICE`](NOTICE). Si redistribuyes el código o el firmware compilado, incluye ambos ficheros.
