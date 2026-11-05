/**
 * bmp_writer.h - Codificación BMP de 4 bits por píxel para las vistas previas
 *
 * El framebuffer usa 4 colores (2 bits por píxel). Los navegadores no leen
 * BMP de 2 bits, así que se emite un BMP de 4 bits con una paleta de 16
 * entradas (solo se usan las 4 primeras). Las filas van de abajo arriba.
 */
#ifndef BMP_WRITER_H
#define BMP_WRITER_H

#include <stdint.h>
#include <string.h>

static const uint16_t BMP4_HEADER_SIZE = 14 + 40 + 16 * 4;   // 118 bytes

inline uint32_t bmp4RowBytes(uint16_t width) {
    return ((uint32_t)width * 4 + 31) / 32 * 4;   // filas alineadas a 4 bytes
}

inline uint32_t bmp4FileSize(uint16_t width, uint16_t height) {
    return BMP4_HEADER_SIZE + bmp4RowBytes(width) * height;
}

inline void bmpPut16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
inline void bmpPut32(uint8_t *p, uint32_t v) { for (uint8_t i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 0xFF; }

// Escribe la cabecera completa (118 bytes) con la paleta RGB565 convertida a RGB888
inline void bmp4Header(uint8_t *out, uint16_t width, uint16_t height, const uint16_t palette565[4]) {
    memset(out, 0, BMP4_HEADER_SIZE);
    out[0] = 'B'; out[1] = 'M';
    bmpPut32(out + 2, bmp4FileSize(width, height));
    bmpPut32(out + 10, BMP4_HEADER_SIZE);              // inicio de los píxeles
    bmpPut32(out + 14, 40);                            // BITMAPINFOHEADER
    bmpPut32(out + 18, width);
    bmpPut32(out + 22, height);                        // positivo = filas de abajo arriba
    bmpPut16(out + 26, 1);                             // planos
    bmpPut16(out + 28, 4);                             // bits por píxel
    bmpPut32(out + 34, bmp4RowBytes(width) * height);
    bmpPut32(out + 46, 16);                            // colores en la paleta
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t c = palette565[i];
        uint8_t *e = out + 54 + i * 4;                 // orden B, G, R, 0
        e[0] = (c & 0x1F) * 255 / 31;
        e[1] = ((c >> 5) & 0x3F) * 255 / 63;
        e[2] = (c >> 11) * 255 / 31;
    }
}

// Empaqueta una fila: getIndex(x) devuelve el índice de color (0-3) de cada píxel
template <typename F>
inline void bmp4PackRow(uint8_t *out, uint16_t width, F getIndex) {
    uint32_t n = bmp4RowBytes(width);
    memset(out, 0, n);
    for (uint16_t x = 0; x < width; x++) {
        uint8_t v = getIndex(x) & 0x0F;
        out[x >> 1] |= (x & 1) ? v : (v << 4);         // píxel izquierdo en el nibble alto
    }
}

#endif
