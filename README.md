# 🌤️ Estación Meteorológica ESP8266 v2.0

Rediseño completo del proyecto ThingPulse Weather Station para **PlatformIO** con portal web de configuración, menú táctil de ajustes y gestión de horario de pantalla.

## ✨ Novedades respecto al original

| Característica | Original | v2.0 |
|---|---|---|
| Framework | Arduino IDE | **PlatformIO** |
| Filesystem | SPIFFS (deprecated) | **LittleFS** |
| Config WiFi | WiFiManager (hardcoded) | **Portal web propio con AP** |
| Almacenamiento | `application.properties` | **JSON con ArduinoJson** |
| API Key / Location | Hardcoded en código | **Configurable por web** |
| Zona horaria | Hardcoded | **Seleccionable por web** |
| Horario pantalla | Solo en código | **Web + menú táctil** |
| Servidor web | No tiene | **Activo en modo STA** |
| Pantalla ajustes | No tiene | **Nueva pantalla táctil** |
| Escaneo WiFi | No tiene | **Buscar redes desde web** |
| Idioma API | Hardcoded | **Seleccionable por web** |
| Intervalo update | Hardcoded 10 min | **Configurable 5-60 min** |
| Reset fábrica | No tiene | **Desde web** |
| DNS Captive Portal | No tiene | **Redirige automáticamente** |

## 📋 Hardware necesario

- **ESP8266** (NodeMCU v2 o Wemos D1 Mini)
- **ILI9341** TFT 240×320 SPI
- **XPT2046** Touch overlay

### Conexiones

| Señal | Pin ESP8266 |
|-------|-------------|
| TFT CS | D1 |
| TFT DC | D2 |
| TFT LED | D8 |
| Touch CS | D3 |
| Touch IRQ | D4 |
| SPI CLK | D5 (SCK) |
| SPI MOSI | D7 (MOSI) |
| SPI MISO | D6 (MISO) |

## 🚀 Instalación

1. Instalar [PlatformIO](https://platformio.org/)
2. Clonar/copiar este proyecto
3. Abrir la carpeta en VS Code con PlatformIO
4. Compilar y subir:
   ```bash
   pio run -t upload
   ```

## 🔧 Primer uso

1. Al encender por primera vez, el ESP crea un **punto de acceso WiFi**:
   - SSID: `EstacionMeteo-Config`
   - Contraseña: `meteo1234`
2. Conéctate a esa red desde tu móvil/PC
3. Se abrirá automáticamente el portal de configuración (o ve a `http://192.168.4.1`)
4. Configura:
   - **WiFi**: selecciona tu red y pon la contraseña
   - **API**: tu clave de OpenWeatherMap y ID de ubicación
   - **Zona horaria**: selecciona tu zona
   - **Horario de pantalla**: hora de encendido y apagado
5. Pulsa "Guardar y Reiniciar"

## 📱 Configuración web (modo STA)

Una vez conectado a tu WiFi, el servidor web sigue activo. Accede desde cualquier dispositivo en tu red local a la IP mostrada en la pantalla del dispositivo.

### Páginas disponibles

- `/` → Configuración completa
- `/schedule` → Ajuste de horario de pantalla
- `/status` → Estado del dispositivo (JSON)
- `/scan` → Escaneo de redes WiFi (JSON)
- `/reset` → Restaurar valores de fábrica

## 🖥️ Pantallas del dispositivo

| Toque | Acción |
|-------|--------|
| **Arriba** | Cambiar formato 12h/24h |
| **Izquierda** | Pantalla anterior |
| **Derecha** | Pantalla siguiente |
| **Abajo** | Ir a pantalla de Ajustes |

### Pantalla de Ajustes (nueva)

Desde el menú táctil puedes:
- Cambiar formato 12h/24h
- Cambiar unidades métricas/imperiales
- Activar/desactivar horario automático de pantalla
- Ajustar hora de encendido (+/- con botones)
- Ajustar hora de apagado (+/- con botones)
- Guardar cambios
- Ver IP del servidor web

## 📂 Estructura del proyecto

```
weather-station/
├── platformio.ini          # Configuración PlatformIO
├── README.md
├── include/
│   ├── config.h            # Constantes y estructura de config
│   ├── config_manager.h    # Gestión JSON persistente
│   ├── web_portal.h        # Portal web (header)
│   ├── TouchControllerWS.h # Touch (actualizado a LittleFS)
│   ├── TZinfo.h            # Base de datos de zonas horarias
│   ├── ArialRounded.h      # Fuentes
│   ├── moonphases.h        # Iconos de fases lunares
│   └── weathericons.h      # Iconos meteorológicos
├── src/
│   ├── main.cpp            # Aplicación principal
│   ├── web_portal.cpp      # Implementación del portal web
│   └── TouchControllerWS.cpp
└── data/                   # (vacío, config se crea en runtime)
```

## 🔑 API Key

Obtén tu clave gratuita en: https://openweathermap.org/api

Para encontrar tu ID de ubicación: https://openweathermap.org/find

## 📄 Licencia

Basado en el proyecto original de [ThingPulse](https://thingpulse.com/) bajo licencia MIT.
