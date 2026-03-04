# Estacion Meteorologica ESP8266 v1.0.0

Rediseno completo del proyecto ThingPulse Weather Station para **PlatformIO** con portal web de configuracion, menu tactil de ajustes y gestion de horario de pantalla.

## Novedades respecto al original

| Caracteristica | Original | v1.0.0 |
|---|---|---|
| Framework | Arduino IDE | **PlatformIO** |
| Filesystem | SPIFFS (deprecated) | **LittleFS** |
| Config WiFi | WiFiManager (hardcoded) | **Portal web propio con AP** |
| Almacenamiento | `application.properties` | **JSON con ArduinoJson** |
| API Key / Location | Hardcoded en codigo | **Configurable por web** |
| Zona horaria | Hardcoded | **Seleccionable por web** |
| Horario pantalla | Solo en codigo | **Web + menu tactil** |
| Servidor web | No tiene | **Activo en modo STA** |
| Pantalla ajustes | No tiene | **Nueva pantalla tactil** |
| Escaneo WiFi | No tiene | **Buscar redes desde web** |
| Idioma API | Hardcoded | **Seleccionable por web** |
| Intervalo update | Hardcoded 10 min | **Configurable 5-60 min** |
| Brillo pantalla | Hardcoded | **Configurable por web** |
| Reset fabrica | No tiene | **Desde web** |
| DNS Captive Portal | No tiene | **Redirige automaticamente** |

## Hardware necesario

- **ESP8266** (NodeMCU v2 o Wemos D1 Mini)
- **ILI9341** TFT 240x320 SPI
- **XPT2046** Touch overlay

### Conexiones

| Senal | Pin ESP8266 |
|-------|-------------|
| TFT CS | D1 |
| TFT DC | D2 |
| TFT LED | D8 |
| Touch CS | D3 |
| Touch IRQ | D4 |
| SPI CLK | D5 (SCK) |
| SPI MOSI | D7 (MOSI) |
| SPI MISO | D6 (MISO) |

## Dependencias

Las librerias se instalan automaticamente con PlatformIO:

| Libreria | Version | Uso |
|----------|---------|-----|
| `thingpulse/ESP8266 Weather Station` | ^2.2.0 | API OpenWeatherMap |
| `squix78/Mini Grafx` | ^1.0.0 | Motor grafico TFT |
| `squix78/JsonStreamingParser` | ^1.0.5 | Parseo JSON streaming |
| `paulstoffregen/XPT2046_Touchscreen` | latest | Control tactil |
| `bblanchon/ArduinoJson` | ^6.21.0 | Config persistente |

## Instalacion

1. Instalar [PlatformIO](https://platformio.org/)
2. Clonar este repositorio
3. Abrir la carpeta en VS Code con PlatformIO
4. Compilar y subir firmware:
   ```bash
   pio run -t upload
   ```
5. Subir sistema de archivos (si es necesario):
   ```bash
   pio run -t uploadfs
   ```

## Primer uso

1. Al encender por primera vez, el ESP crea un **punto de acceso WiFi**:
   - SSID: `EstacionMeteo-Config`
   - Contrasena: `meteo1234`
2. Conectate a esa red desde tu movil/PC
3. Se abrira automaticamente el portal de configuracion (o ve a `http://192.168.4.1`)
4. Configura:
   - **WiFi**: selecciona tu red y pon la contrasena
   - **API**: tu clave de OpenWeatherMap y ID de ubicacion
   - **Zona horaria**: selecciona tu zona
   - **Horario de pantalla**: hora de encendido y apagado
   - **Brillo**: nivel de retroiluminacion
5. Pulsa "Guardar y Reiniciar"

## Configuracion web (modo STA)

Una vez conectado a tu WiFi, el servidor web sigue activo en el puerto 80. Accede desde cualquier dispositivo en tu red local a la IP mostrada en la pantalla.

### Paginas disponibles

| Ruta | Descripcion |
|------|-------------|
| `/` | Configuracion completa |
| `/schedule` | Ajuste de horario de pantalla |
| `/status` | Estado del dispositivo (JSON) |
| `/scan` | Escaneo de redes WiFi (JSON) |
| `/reset` | Restaurar valores de fabrica |

## Pantallas del dispositivo

| Toque | Accion |
|-------|--------|
| **Arriba** | Cambiar formato 12h/24h |
| **Izquierda** | Pantalla anterior |
| **Derecha** | Pantalla siguiente |
| **Abajo** | Ir a pantalla de Ajustes |

Las pantallas disponibles son:

- **Principal** (0): Tiempo actual, hora y fecha
- **Detalle** (1): Datos extendidos del tiempo
- **Prevision 1** (2): Pronostico proximos dias
- **Prevision 2** (3): Pronostico extendido
- **Sobre el dispositivo** (4): IP, version firmware, estado
- **Ajustes** (5): Configuracion tactil

### Pantalla de Ajustes

Desde el menu tactil puedes:
- Cambiar formato 12h/24h
- Cambiar unidades metricas/imperiales
- Activar/desactivar horario automatico de pantalla
- Ajustar hora de encendido (+/- con botones)
- Ajustar hora de apagado (+/- con botones)
- Guardar cambios
- Ver IP del servidor web

## Configuracion avanzada

Los parametros clave del sistema se encuentran en `include/config.h`:

| Constante | Valor por defecto | Descripcion |
|-----------|------------------|-------------|
| `UPDATE_INTERVAL_SECS` | 600 | Intervalo de actualizacion (segundos) |
| `SLEEP_INTERVAL_SECS` | 30 | Tiempo antes de apagar pantalla |
| `MAX_FORECASTS` | 12 | Numero maximo de periodos de pronostico |
| `WEB_SERVER_PORT` | 80 | Puerto del servidor web |
| `AP_SSID` | `EstacionMeteo-Config` | SSID del portal de configuracion |
| `AP_PASSWORD` | `meteo1234` | Contrasena del portal |

## Estructura del proyecto

```
weather-station/
|-- platformio.ini          # Configuracion PlatformIO y dependencias
|-- README.md
|-- include/
|   |-- version.h           # Version del firmware (MAJOR.MINOR.PATCH)
|   |-- config.h            # Constantes, pines y estructura de config
|   |-- config_manager.h    # Gestion JSON persistente con LittleFS
|   |-- web_portal.h        # Portal web (header)
|   |-- TouchControllerWS.h # Controlador tactil (actualizado a LittleFS)
|   |-- TZinfo.h            # Base de datos de zonas horarias
|   |-- ArialRounded.h      # Fuentes tipograficas
|   |-- moonphases.h        # Iconos de fases lunares
|   `-- weathericons.h      # Iconos meteorologicos
|-- src/
|   |-- main.cpp            # Aplicacion principal
|   |-- web_portal.cpp      # Implementacion del portal web
|   `-- TouchControllerWS.cpp
`-- data/                   # (vacio, config se crea en runtime)
```

## API Key

Obtén tu clave gratuita en: https://openweathermap.org/api

Para encontrar tu ID de ubicacion: https://openweathermap.org/find

## Compilacion automatica (CI)

El repositorio incluye un workflow de GitHub Actions que compila el firmware automaticamente en cada push y genera el artefacto `.bin` listo para flashear. El nombre del artefacto incluye la version definida en `include/version.h`.

## Licencia

Basado en el proyecto original de [ThingPulse](https://thingpulse.com/) bajo licencia MIT.
