/**
 * clock_styles.h - Estilos visuales del reloj (temas)
 *
 * Cada estilo define:
 *   - Una paleta de 4 colores (la pantalla trabaja a 2 bits por pixel).
 *     Los indices mantienen su significado en todos los temas:
 *       0 = fondo, 1 = texto principal, 2 = acento, 3 = secundario
 *   - Un reloj compacto para la cabecera de la pantalla principal
 *   - Un reloj a pantalla completa (pantalla "Reloj")
 */
#ifndef CLOCK_STYLES_H
#define CLOCK_STYLES_H

#include <Arduino.h>
#include <time.h>
#include <MiniGrafx.h>
#include "config.h"

enum ClockStyle : uint8_t {
    CLOCK_CLASSIC  = 0,   // Clasico (diseno original)
    CLOCK_RETRO    = 1,   // LCD de 7 segmentos color ambar
    CLOCK_ANIME    = 2,   // Vinetas manga, lineas de velocidad y brillos
    CLOCK_ARCADE   = 3,   // Videojuego arcade 8-bit (pixel art)
    CLOCK_HANDHELD = 4,   // Consola portatil de pantalla verde
    CLOCK_FUTURE   = 5,   // HUD de ciencia ficcion con anillo de segundos
    CLOCK_STYLE_COUNT
};

// Datos opcionales para enriquecer el reloj a pantalla completa
struct ClockExtras {
    String temperature;   // p.ej. "21.5°C"
    String description;   // p.ej. "cielo claro"
    String location;      // p.ej. "Madrid"
};

// Registra las fuentes ArialRounded (definidas en main.cpp)
void clockSetFonts(const char *rounded14, const char *rounded36);

// Nombre corto del estilo (para la pantalla de ajustes)
const char* clockStyleName(uint8_t style);

// Copia los 4 colores del estilo en la paleta activa
void clockApplyPalette(uint8_t style, uint16_t *palette);

// Reloj compacto de la cabecera (zona 240x56 en y=0)
void clockDrawHeader(MiniGrafx &gfx, const struct tm *t, const AppConfig &cfg);

// Reloj a pantalla completa (240x320)
void clockDrawFull(MiniGrafx &gfx, const struct tm *t, const AppConfig &cfg, const ClockExtras &extras);

#endif
