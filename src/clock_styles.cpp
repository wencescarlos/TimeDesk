/**
 * clock_styles.cpp - Renderizado de los estilos de reloj
 *
 * La pantalla usa 2 bits por pixel (4 colores), asi que cada estilo se
 * construye solo con primitivas de MiniGrafx (rectangulos, lineas, circulos)
 * y una paleta propia. No se usan bitmaps extra: todo cabe en el firmware.
 */
#include "clock_styles.h"
#include <math.h>

static_assert(CLOCK_STYLE_COUNT == CLOCK_STYLE_MAX, "Actualiza CLOCK_STYLE_MAX en config.h");

// Las fuentes ArialRounded estan definidas (static) en main.cpp; se registran
// desde alli para no duplicar ~10KB de datos de fuente en flash.
static const char *fontRounded14 = ArialMT_Plain_16;
static const char *fontRounded36 = ArialMT_Plain_24;

void clockSetFonts(const char *rounded14, const char *rounded36) {
    fontRounded14 = rounded14;
    fontRounded36 = rounded36;
}

// =====================================================
// PALETAS (RGB565)  [fondo, principal, acento, secundario]
// =====================================================
static const uint16_t PALETTES[CLOCK_STYLE_COUNT][4] = {
    /* Clasico   */ {0x0000, 0xFFFF, 0xFFE0, 0x7E3C},
    /* Retro     */ {0x0000, 0xFD20, 0xFFB5, 0x5180},   // negro, ambar, crema, ambar apagado
    /* Anime     */ {0x1846, 0xFFFF, 0xFB56, 0x9D5F},   // indigo, blanco, rosa sakura, lavanda
    /* Arcade    */ {0x0000, 0xFFFF, 0xFFE0, 0xF8A0},   // negro, blanco, amarillo, rojo
    /* Portatil  */ {0x9DE1, 0x09C1, 0x3306, 0x6463},   // 4 tonos de verde LCD
    /* Futurista */ {0x0000, 0x07FF, 0xF81F, 0x0459},   // negro, cian, magenta, azul acero
    /* Analogico */ {0x0866, 0xFFFF, 0xFB00, 0x8C71},   // azul noche, blanco, naranja, gris
    /* 1-bit     */ {0xFFFF, 0x0000, 0x8410, 0xC618},   // blanco, negro, grises
    /* Escrit.3D */ {0x0410, 0xFFFF, 0x0010, 0xC618},   // verde azulado, blanco, azul marino, plata
    /* Terminal  */ {0x0000, 0x07E0, 0xAFF5, 0x0320},   // fosforo verde
    /* Micro 8bit*/ {0x18B4, 0x8CBF, 0xFFFF, 0xFFE0},   // azul, azul claro, blanco, amarillo
    /* Aventura  */ {0x0180, 0xFFB9, 0xFDA0, 0xA145},   // bosque, pergamino, oro, rojo
    /* Velocidad */ {0x0008, 0xFFFF, 0xFFE0, 0xF81F},   // noche, blanco, amarillo, magenta
    /* Plataform.*/ {0x5D1F, 0xFFFF, 0xD340, 0x2444},   // cielo, blanco, ladrillo, verde
    /* Tiempo    */ {0x3D7F, 0xFFFF, 0xFFE0, 0x1A7A},   // (cambia según el tiempo: WX_PALETTES)
    /* Noche     */ {0x0004, 0xFFFF, 0xFFB4, 0x632F},   // azul noche, blanco, luna, gris azulado
    /* Acuario   */ {0x0230, 0xFFFF, 0xFC40, 0x2D25},   // agua, blanco, pez naranja, alga
    /* Oscilosc. */ {0x0080, 0x07E0, 0xAFF5, 0x0260},   // pantalla verde, haz, núcleo, rejilla
    /* Nixie     */ {0x0000, 0xFD00, 0xFF9A, 0x4A29},   // negro, neón naranja, filamento, cristal
    /* Flip      */ {0x2104, 0xFFFF, 0x4228, 0x0000},   // gris oscuro, blanco, tarjeta, negro
};

static const char *STYLE_NAMES[CLOCK_STYLE_COUNT] = {
    "Clasico", "Retro", "Anime", "Arcade", "Portatil", "Futurista",
    "Analogico", "1-bit", "Escritorio", "Terminal", "Micro 8bit", "Aventura", "Velocidad", "Plataformas",
    "Tiempo", "Noche", "Acuario", "Osciloscopio", "Nixie", "Flip"
};

const char *clockStyleName(uint8_t style) {
    return STYLE_NAMES[style < CLOCK_STYLE_COUNT ? style : 0];
}

static const uint16_t *weatherPalette();   // definida con el estilo "Según el tiempo"

void clockApplyPalette(uint8_t style, uint16_t *palette) {
    if (style >= CLOCK_STYLE_COUNT) style = CLOCK_CLASSIC;
    const uint16_t *src = (style == CLOCK_WEATHER) ? weatherPalette() : PALETTES[style];
    for (uint8_t i = 0; i < 4; i++) palette[i] = src[i];
}

// Indices de color (mismo significado en todos los temas)
static const uint8_t C_BG = 0, C_FG = 1, C_ACC = 2, C_SEC = 3;
static const uint8_t C_NONE = 0xFF;

// =====================================================
// UTILIDADES COMUNES
// =====================================================
static uint8_t displayHour(const struct tm *t, const AppConfig &cfg) {
    return cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour;
}

// Construye "HH:MM" o "HH:MM:SS". En 12h la decena vacia se deja en blanco.
static void buildTimeString(char *out, const struct tm *t, const AppConfig &cfg, bool withSeconds) {
    uint8_t h = displayHour(t, cfg);
    char h0 = (cfg.is12hStyle && h < 10) ? ' ' : char('0' + h / 10);
    if (withSeconds) {
        sprintf_P(out, PSTR("%c%d:%02d:%02d"), h0, h % 10, t->tm_min, t->tm_sec);
    } else {
        sprintf_P(out, PSTR("%c%d:%02d"), h0, h % 10, t->tm_min);
    }
}

static String shortDate(const struct tm *t) {
    char buf[8];
    sprintf_P(buf, PSTR("%02d"), t->tm_mday % 100);
    return WDAY_NAMES[t->tm_wday] + String(F(" ")) + String(buf) + String(F(" ")) + MONTH_NAMES[t->tm_mon];
}

// Estrella de 4 puntas (brillo estilo anime)
static void drawSparkle(MiniGrafx &g, int16_t x, int16_t y, uint8_t r, uint8_t color) {
    g.setColor(color);
    g.drawLine(x - r, y, x + r, y);
    g.drawLine(x, y - r, x, y + r);
    if (r > 3) {
        uint8_t d = r / 3;
        g.drawLine(x - d, y - d, x + d, y + d);
        g.drawLine(x - d, y + d, x + d, y - d);
        g.setPixel(x, y);
    }
}

// Rectangulo relleno con esquinas redondeadas
static void fillRoundRect(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
    g.fillRect(x + r, y, w - 2 * r, h);
    g.fillRect(x, y + r, r, h - 2 * r);
    g.fillRect(x + w - r, y + r, r, h - 2 * r);
    g.fillCircle(x + r, y + r, r);
    g.fillCircle(x + w - r - 1, y + r, r);
    g.fillCircle(x + r, y + h - r - 1, r);
    g.fillCircle(x + w - r - 1, y + h - r - 1, r);
}

// Esquinas tipo "mira" de HUD
static void drawCornerBrackets(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, int16_t len) {
    g.drawHorizontalLine(x, y, len);              g.drawVerticalLine(x, y, len);
    g.drawHorizontalLine(x + w - len, y, len);    g.drawVerticalLine(x + w - 1, y, len);
    g.drawHorizontalLine(x, y + h - 1, len);      g.drawVerticalLine(x, y + h - len, len);
    g.drawHorizontalLine(x + w - len, y + h - 1, len); g.drawVerticalLine(x + w - 1, y + h - len, len);
}

// =====================================================
// DIGITOS DE 7 SEGMENTOS (retro / anime / futurista)
// =====================================================
enum SegMode { SEG_BLOCK, SEG_ROUND, SEG_HEX };

//                         0     1     2     3     4     5     6     7     8     9     -     blanco
static const uint8_t SEG_MAP[12] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x40, 0x00};

// Dibuja un segmento fila a fila; el "inset" de cada fila da la forma de los extremos.
static void drawSegment(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, SegMode mode) {
    bool horizontal = w >= h;
    int16_t thick = horizontal ? h : w;
    int16_t len = horizontal ? w : h;
    float r = thick / 2.0f;
    for (int16_t i = 0; i < thick; i++) {
        float d = i - (thick - 1) / 2.0f;
        int16_t inset = 0;
        if (mode == SEG_HEX) inset = (int16_t)fabsf(d);
        else if (mode == SEG_ROUND) inset = (int16_t)(r - sqrtf(fmaxf(r * r - d * d, 0)));
        if (len - 2 * inset <= 0) continue;
        if (horizontal) g.drawHorizontalLine(x + inset, y + i, len - 2 * inset);
        else g.drawVerticalLine(x + i, y + inset, len - 2 * inset);
    }
}

static void drawSegDigit(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, int16_t th,
                         uint8_t code, uint8_t onColor, uint8_t offColor, SegMode mode) {
    // Separacion entre segmentos: los bloques no deben solaparse; hex/round encajan en diagonal
    int16_t gap = (mode == SEG_BLOCK) ? th / 2 + 1 : 1;
    int16_t xL = x + th / 2, xR = x + w - 1 - th / 2;
    int16_t yT = y + th / 2, yM = y + h / 2, yB = y + h - 1 - th / 2;

    for (uint8_t pass = 0; pass < 2; pass++) {
        for (uint8_t s = 0; s < 7; s++) {
            bool on = SEG_MAP[code] & (1 << s);
            if ((pass == 0 && on) || (pass == 1 && !on)) continue;   // apagados primero
            uint8_t color = on ? onColor : offColor;
            if (color == C_NONE) continue;
            g.setColor(color);
            switch (s) {
                case 0: drawSegment(g, xL + gap, yT - th / 2, xR - xL - 2 * gap + 1, th, mode); break; // a
                case 1: drawSegment(g, xR - th / 2, yT + gap, th, yM - yT - 2 * gap + 1, mode); break; // b
                case 2: drawSegment(g, xR - th / 2, yM + gap, th, yB - yM - 2 * gap + 1, mode); break; // c
                case 3: drawSegment(g, xL + gap, yB - th / 2, xR - xL - 2 * gap + 1, th, mode); break; // d
                case 4: drawSegment(g, xL - th / 2, yM + gap, th, yB - yM - 2 * gap + 1, mode); break; // e
                case 5: drawSegment(g, xL - th / 2, yT + gap, th, yM - yT - 2 * gap + 1, mode); break; // f
                case 6: drawSegment(g, xL + gap, yM - th / 2, xR - xL - 2 * gap + 1, th, mode); break; // g
            }
        }
    }
}

// Dibuja una cadena tipo "12:34:56" centrada en cx. Devuelve el ancho total.
static int16_t drawSegString(MiniGrafx &g, const char *s, int16_t cx, int16_t y, int16_t w, int16_t h,
                             int16_t th, int16_t spacing, uint8_t onColor, uint8_t offColor,
                             SegMode mode, bool colonOn) {
    int16_t colonW = th + 4;
    int16_t total = 0;
    uint8_t n = strlen(s);
    for (uint8_t i = 0; i < n; i++) total += (s[i] == ':' ? colonW : w) + (i ? spacing : 0);
    int16_t x = cx - total / 2;

    for (uint8_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == ':') {
            uint8_t col = colonOn ? onColor : offColor;
            if (col != C_NONE) {
                g.setColor(col);
                int16_t dx = x + (colonW - th) / 2;
                if (mode == SEG_ROUND) {
                    g.fillCircle(dx + th / 2, y + h / 3, th / 2);
                    g.fillCircle(dx + th / 2, y + 2 * h / 3, th / 2);
                } else {
                    g.fillRect(dx, y + h / 3 - th / 2, th, th);
                    g.fillRect(dx, y + 2 * h / 3 - th / 2, th, th);
                }
            }
            x += colonW + spacing;
        } else {
            uint8_t code = (c >= '0' && c <= '9') ? c - '0' : (c == '-' ? 10 : 11);
            drawSegDigit(g, x, y, w, h, th, code, onColor, offColor, mode);
            x += w + spacing;
        }
    }
    return total;
}

// =====================================================
// FUENTE PIXEL 3x5 (arcade / portatil)
// =====================================================
// Cada glifo: 5 filas de 'w' bits (el bit mas alto = columna izquierda).
// La mayoria mide 3; ':' '.' '!' miden 1 y M/N/W son mas anchas para que se lean bien.
struct PixelGlyph { char c; uint8_t w; uint8_t rows[5]; };

static const PixelGlyph PIXEL_FONT[] PROGMEM = {
    {'0',3,{7,5,5,5,7}}, {'1',3,{2,6,2,2,7}}, {'2',3,{7,1,7,4,7}}, {'3',3,{7,1,7,1,7}},
    {'4',3,{5,5,7,1,1}}, {'5',3,{7,4,7,1,7}}, {'6',3,{7,4,7,5,7}}, {'7',3,{7,1,1,2,2}},
    {'8',3,{7,5,7,5,7}}, {'9',3,{7,5,7,1,7}},
    {'A',3,{2,5,7,5,5}}, {'B',3,{6,5,6,5,6}}, {'C',3,{3,4,4,4,3}}, {'D',3,{6,5,5,5,6}},
    {'E',3,{7,4,6,4,7}}, {'F',3,{7,4,6,4,4}}, {'G',3,{3,4,5,5,3}}, {'H',3,{5,5,7,5,5}},
    {'I',3,{7,2,2,2,7}}, {'J',3,{1,1,1,5,2}}, {'K',3,{5,5,6,5,5}}, {'L',3,{4,4,4,4,7}},
    {'M',5,{17,27,21,17,17}}, {'N',4,{9,13,11,9,9}}, {'O',3,{2,5,5,5,2}}, {'P',3,{6,5,6,4,4}},
    {'Q',3,{2,5,5,6,3}}, {'R',3,{6,5,6,5,5}}, {'S',3,{3,4,2,1,6}}, {'T',3,{7,2,2,2,2}},
    {'U',3,{5,5,5,5,7}}, {'V',3,{5,5,5,5,2}}, {'W',5,{17,17,21,27,17}}, {'X',3,{5,5,2,5,5}},
    {'Y',3,{5,5,2,2,2}}, {'Z',3,{7,1,2,4,7}},
    {':',1,{0,1,0,1,0}}, {'.',1,{0,0,0,0,1}}, {'!',1,{1,1,1,0,1}}, {'-',3,{0,0,7,0,0}},
    {'%',3,{5,1,2,4,5}}, {'/',3,{1,1,2,4,4}}, {'+',3,{0,2,7,2,0}}, {'\x01',3,{6,6,0,0,0}}, // \x01 = grado
    {'$',3,{3,6,2,3,6}}, {'@',3,{2,5,7,4,3}}, {'~',3,{0,3,6,0,0}}, {'#',3,{5,7,5,7,5}},
    {'[',2,{3,2,2,2,3}}, {']',2,{3,1,1,1,3}}, {'(',2,{1,2,2,2,1}}, {')',2,{2,1,1,1,2}},
    {'_',3,{0,0,0,0,7}}, {'=',3,{0,7,0,7,0}}, {'>',3,{4,2,1,2,4}}, {'<',3,{1,2,4,2,1}},
    {',',2,{0,0,0,1,2}}, {'?',3,{7,1,2,0,2}}, {'*',3,{0,5,2,5,0}}, {'\'',1,{1,1,0,0,0}},
    {' ',3,{0,0,0,0,0}},
};
static const uint8_t PIXEL_FONT_LEN = sizeof(PIXEL_FONT) / sizeof(PIXEL_FONT[0]);

static bool findGlyph(char c, PixelGlyph &out) {
    if (c >= 'a' && c <= 'z') c -= 32;
    for (uint8_t i = 0; i < PIXEL_FONT_LEN; i++) {
        memcpy_P(&out, &PIXEL_FONT[i], sizeof(PixelGlyph));
        if (out.c == c) return true;
    }
    memcpy_P(&out, &PIXEL_FONT[PIXEL_FONT_LEN - 1], sizeof(PixelGlyph));   // espacio
    return false;
}

// Normaliza UTF-8 (acentos espanoles y simbolo de grado) a la fuente pixel
static String pixelNormalize(const String &in) {
    String out;
    for (uint16_t i = 0; i < in.length(); i++) {
        uint8_t c = in[i];
        if (c < 0x80) { out += (char)c; continue; }
        uint8_t n = (i + 1 < in.length()) ? (uint8_t)in[i + 1] : 0;
        if (c == 0xC2 && n == 0xB0) { out += '\x01'; i++; continue; }
        if (c == 0xC3) {
            i++;
            switch (n | 0x20) {     // minusculas y mayusculas juntas
                case 0xA1: out += 'A'; break;
                case 0xA9: out += 'E'; break;
                case 0xAD: out += 'I'; break;
                case 0xB3: out += 'O'; break;
                case 0xBA: case 0xBC: out += 'U'; break;
                case 0xB1: out += 'N'; break;
                default: out += ' ';
            }
            continue;
        }
        // Otros multibyte: saltar bytes de continuacion
        while (i + 1 < in.length() && ((uint8_t)in[i + 1] & 0xC0) == 0x80) i++;
        out += ' ';
    }
    return out;
}

static int16_t pixelTextWidth(const String &s, uint8_t scale) {
    PixelGlyph gl;
    int16_t w = 0;
    for (uint16_t i = 0; i < s.length(); i++) {
        findGlyph(s[i], gl);
        w += (gl.w + 1) * scale;
    }
    return w > 0 ? w - scale : 0;
}

// Dibuja texto pixel. align: 0 izq, 1 centro, 2 der. shadow: color de sombra (C_NONE = sin sombra)
static void drawPixelText(MiniGrafx &g, const String &raw, int16_t x, int16_t y, uint8_t scale,
                          uint8_t color, uint8_t align = 0, uint8_t shadow = C_NONE, uint16_t maxW = 240,
                          int8_t slant = 0) {   // slant > 0: cursiva (desplaza las filas superiores)
    String s = pixelNormalize(raw);
    while (s.length() > 1 && pixelTextWidth(s, scale) > maxW) s.remove(s.length() - 1);
    int16_t w = pixelTextWidth(s, scale);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;

    uint8_t block = scale > 3 ? scale - 1 : scale;   // con escala grande se ve la rejilla de pixeles
    int16_t so = scale > 3 ? scale / 3 + 1 : 1;      // desplazamiento de la sombra
    PixelGlyph gl;
    for (uint8_t pass = (shadow == C_NONE ? 1 : 0); pass < 2; pass++) {
        g.setColor(pass == 0 ? shadow : color);
        int16_t off = pass == 0 ? so : 0;
        int16_t cx = x;
        for (uint16_t i = 0; i < s.length(); i++) {
            findGlyph(s[i], gl);
            for (uint8_t r = 0; r < 5; r++) {
                for (uint8_t c = 0; c < gl.w; c++) {
                    if (gl.rows[r] & (1 << (gl.w - 1 - c))) {
                        g.fillRect(cx + c * scale + off + (4 - r) * slant, y + r * scale + off, block, block);
                    }
                }
            }
            cx += (gl.w + 1) * scale;
        }
    }
}

// Sprite de 8x8 (1 bit por pixel, MSB a la izquierda)
static void drawSprite8(MiniGrafx &g, const uint8_t *rows, int16_t x, int16_t y, uint8_t scale, uint8_t color) {
    g.setColor(color);
    for (uint8_t r = 0; r < 8; r++)
        for (uint8_t c = 0; c < 8; c++)
            if (rows[r] & (0x80 >> c)) g.fillRect(x + c * scale, y + r * scale, scale, scale);
}

static const uint8_t SPRITE_HEART[8]   = {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00};
static const uint8_t SPRITE_BLOB_A[8]  = {0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x7E, 0x00};  // mascota original
static const uint8_t SPRITE_BLOB_B[8]  = {0x00, 0x00, 0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0xFF};  // agachada (salto)
static const uint8_t SPRITE_STAR[8]    = {0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x66, 0x42, 0x00};

// =====================================================
// ESTILO CLASICO
// =====================================================
static void headerClassic(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setFont(fontRounded14);
    g.setColor(C_FG);
    g.drawString(120, 6, WDAY_NAMES[t->tm_wday] + String(F(" ")) + MONTH_NAMES[t->tm_mon] + String(F(" ")) +
                 String(t->tm_mday) + String(F(" ")) + String(1900 + t->tm_year));
    g.setFont(fontRounded36);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.drawString(120, 20, buf);
    if (cfg.is12hStyle) {
        g.setTextAlignment(TEXT_ALIGN_LEFT);
        g.setFont(ArialMT_Plain_10);
        g.setColor(C_SEC);
        g.drawString(195, 27, t->tm_hour >= 12 ? "PM" : "AM");
    }
}

static void fullClassic(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setFont(fontRounded14);
    g.setColor(C_ACC);
    g.drawString(120, 70, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));
    g.setFont(fontRounded36);
    g.setColor(C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.drawString(120, 100, buf);
    if (cfg.is12hStyle) {
        g.setFont(fontRounded14);
        g.setColor(C_SEC);
        g.drawString(120, 146, t->tm_hour >= 12 ? "PM" : "AM");
    }
    // Barra de segundos
    g.setColor(C_SEC);
    g.drawRect(30, 170, 180, 6);
    g.setColor(C_ACC);
    g.fillRect(31, 171, 178 * t->tm_sec / 59, 4);

    g.setFont(fontRounded36);
    g.setColor(C_FG);
    g.drawString(120, 200, ex.temperature);
    g.setFont(fontRounded14);
    g.setColor(C_ACC);
    g.drawString(120, 246, ex.description);
    g.setColor(C_SEC);
    g.drawString(120, 290, ex.location);
}

// =====================================================
// ESTILO RETRO (LCD ambar de 7 segmentos)
// =====================================================
static void headerRetro(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    bool secs = !cfg.isHHMM;
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    g.drawString(6, 2, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));
    if (cfg.is12hStyle) {
        g.setColor(t->tm_hour < 12 ? C_ACC : C_SEC);  g.drawString(150, 2, F("AM"));
        g.setColor(t->tm_hour >= 12 ? C_ACC : C_SEC); g.drawString(170, 2, F("PM"));
    }
    buildTimeString(buf, t, cfg, secs);
    int16_t w = secs ? 21 : 30;
    int16_t total = drawSegString(g, buf, 120, 17, w, 34, 4, 4, C_FG, C_SEC, SEG_BLOCK,
                                  secs || (t->tm_sec % 2 == 0));
    g.setColor(C_SEC);
    g.drawRect(120 - total / 2 - 6, 13, total + 12, 42);
}

static void fullRetro(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    // Indicador de dia de la semana (como en los relojes digitales de los 80)
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    for (uint8_t d = 0; d < 7; d++) {
        uint8_t wd = (d + 1) % 7;   // empieza en lunes
        int16_t x = 18 + d * 34;
        bool today = wd == t->tm_wday;
        if (today) { g.setColor(C_FG); g.fillRect(x - 15, 26, 30, 14); g.setColor(C_BG); }
        else g.setColor(C_SEC);
        g.drawString(x, 27, WDAY_NAMES[wd]);
    }

    // Panel LCD con doble marco
    g.setColor(C_SEC);
    g.drawRect(6, 52, 228, 136);
    g.drawRect(9, 55, 222, 130);

    // Indicadores AM/PM y 24H
    if (cfg.is12hStyle) {
        g.setColor(t->tm_hour < 12 ? C_FG : C_SEC);  g.drawString(26, 62, F("AM"));
        g.setColor(t->tm_hour >= 12 ? C_FG : C_SEC); g.drawString(54, 62, F("PM"));
    } else {
        g.setColor(C_FG); g.drawString(30, 62, F("24H"));
    }

    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 120, 78, 40, 76, 7, 6, C_FG, C_SEC, SEG_BLOCK, t->tm_sec % 2 == 0);

    // Segundos en pequeno, abajo a la derecha del panel
    sprintf_P(buf, PSTR("%02d"), t->tm_sec);
    drawSegString(g, buf, 196, 160, 12, 20, 3, 3, C_ACC, C_NONE, SEG_BLOCK, true);

    // Fecha en 7 segmentos: DD-MM
    sprintf_P(buf, PSTR("%02d-%02d"), t->tm_mday, t->tm_mon + 1);
    drawSegString(g, buf, 120, 200, 20, 36, 4, 4, C_ACC, C_NONE, SEG_BLOCK, true);

    g.setFont(ArialMT_Plain_16);
    g.setColor(C_FG);
    g.drawString(120, 252, ex.temperature + String(F("  ")) + ex.location);
    g.setColor(C_ACC);
    g.drawString(120, 276, ex.description);
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_SEC);
    g.drawString(120, 302, F("-- MODO LCD --"));
}

// =====================================================
// ESTILO ANIME (vinetas manga, lineas de velocidad, brillos)
// =====================================================
static void headerAnime(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_ACC);
    g.drawString(8, 2, shortDate(t));
    if (cfg.is12hStyle) {
        g.setColor(C_SEC);
        g.drawString(150, 2, t->tm_hour >= 12 ? "PM" : "AM");
    }

    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    // Sombra rosa desplazada (efecto "cel shading")
    g.setColor(C_ACC);
    g.drawString(122, 18, buf);
    g.setColor(C_FG);
    g.drawString(120, 16, buf);

    uint16_t tw = g.getStringWidth(buf, strlen(buf));
    g.setColor(C_ACC);
    fillRoundRect(g, 120 - tw / 2, 54, tw, 3, 1);

    // Brillos que parpadean con los segundos
    uint8_t s = t->tm_sec;
    drawSparkle(g, 120 - tw / 2 - 12, 26, (s % 2) ? 6 : 3, C_FG);
    drawSparkle(g, 120 + tw / 2 + 12, 44, (s % 2) ? 3 : 6, C_ACC);
    drawSparkle(g, 120 + tw / 2 + 4, 22, 2, C_SEC);
}

static void drawKawaiiFace(MiniGrafx &g, int16_t cx, int16_t cy, uint8_t sec) {
    g.setColor(C_BG);
    g.fillCircle(cx, cy, 30);
    g.setColor(C_FG);
    g.drawCircle(cx, cy, 30);
    g.drawCircle(cx, cy, 29);
    bool blink = (sec % 10) == 0;
    if (blink) {
        // Ojos cerrados: ^ ^
        g.drawLine(cx - 16, cy - 2, cx - 11, cy - 7); g.drawLine(cx - 11, cy - 7, cx - 6, cy - 2);
        g.drawLine(cx + 6, cy - 2, cx + 11, cy - 7);  g.drawLine(cx + 11, cy - 7, cx + 16, cy - 2);
    } else {
        g.fillCircle(cx - 11, cy - 4, 5);
        g.fillCircle(cx + 11, cy - 4, 5);
        g.setColor(C_BG);   // brillo en los ojos
        g.fillCircle(cx - 13, cy - 6, 1);
        g.fillCircle(cx + 9, cy - 6, 1);
    }
    // Mofletes
    g.setColor(C_ACC);
    g.fillCircle(cx - 19, cy + 7, 4);
    g.fillCircle(cx + 19, cy + 7, 4);
    // Boca en "w"
    g.setColor(C_FG);
    g.drawLine(cx - 6, cy + 8, cx - 3, cy + 12); g.drawLine(cx - 3, cy + 12, cx, cy + 8);
    g.drawLine(cx, cy + 8, cx + 3, cy + 12);     g.drawLine(cx + 3, cy + 12, cx + 6, cy + 8);
}

static void fullAnime(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;

    // Lineas de velocidad radiales (estilo manga); giran ligeramente cada segundo
    g.setColor(C_SEC);
    for (uint8_t i = 0; i < 36; i++) {
        float a = (i * 10 + (s % 2) * 5) * DEG_TO_RAD;
        float r0 = (i % 3 == 0) ? 120 : 140;
        g.drawLine(120 + cosf(a) * r0, 116 + sinf(a) * r0, 120 + cosf(a) * 260, 116 + sinf(a) * 260);
    }

    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_ACC);
    g.drawString(120, 30, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));

    // Vineta blanca con la hora en digitos redondeados
    g.setColor(C_ACC);
    fillRoundRect(g, 14, 62, 212, 108, 14);
    g.setColor(C_FG);
    fillRoundRect(g, 17, 65, 206, 102, 12);
    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 120, 82, 38, 68, 10, 7, C_BG, C_NONE, SEG_ROUND, true);

    // Insignia de segundos
    g.setColor(C_ACC);
    g.fillCircle(212, 66, 17);
    g.setColor(C_FG);
    g.drawCircle(212, 66, 17);
    sprintf_P(buf, PSTR("%02d"), s);
    g.setFont(fontRounded14);
    g.drawString(212, 57, buf);
    if (cfg.is12hStyle) {
        g.setColor(C_ACC);
        g.drawString(40, 70, t->tm_hour >= 12 ? "PM" : "AM");
    }

    // Brillos alrededor de la vineta
    drawSparkle(g, 24, 186, ((s + 0) % 3 + 1) * 3, C_FG);
    drawSparkle(g, 214, 196, ((s + 1) % 3 + 1) * 3, C_ACC);
    drawSparkle(g, 64, 232, ((s + 2) % 3 + 1) * 2, C_ACC);
    drawSparkle(g, 182, 236, ((s + 1) % 3 + 1) * 2, C_FG);

    drawKawaiiFace(g, 120, 212, s);

    // Bocadillo de dialogo con el tiempo
    g.setColor(C_FG);
    fillRoundRect(g, 12, 254, 216, 34, 10);
    g.fillTriangle(110, 255, 130, 255, 120, 245);
    g.setColor(C_BG);
    g.setFont(fontRounded14);
    g.drawString(120, 262, ex.temperature + String(F(" - ")) + ex.description);
    g.setColor(C_BG);   // franja limpia sobre las lineas de velocidad
    g.fillRect(30, 296, 180, 20);
    g.setColor(C_SEC);
    g.drawString(120, 298, ex.location);
}

// =====================================================
// ESTILO PIXEL (arcade 8-bit y consola portatil)
// =====================================================
static void headerPixel(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, bool handheld) {
    char buf[24];
    bool secs = !cfg.isHHMM;
    drawPixelText(g, shortDate(t), 6, 3, 2, C_ACC);
    if (handheld) {   // pequeno indicador de "pila" de la consola
        g.setColor(C_ACC);
        g.drawRect(120, 3, 10, 6);
        g.fillRect(130, 4, 2, 4);
        g.fillRect(122, 5, 6, 2);
    }
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 150, 3, 2, C_FG);
    buildTimeString(buf, t, cfg, secs);
    drawPixelText(g, buf, 120, secs ? 18 : 15, secs ? 7 : 8, C_FG, 1, C_SEC);
}

static void fullPixel(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex, bool handheld) {
    char buf[24];
    uint8_t s = t->tm_sec;

    if (handheld) {
        // Marco de la pantalla de la consola + LED de encendido
        g.setColor(C_ACC);
        g.drawRect(2, 2, 236, 316);
        g.drawRect(3, 3, 234, 314);
        g.fillCircle(14, 14, 3);
        drawPixelText(g, F("PILA"), 22, 12, 1, C_ACC);
    }

    // Fila superior: nivel (dia del ano) y vidas
    sprintf_P(buf, PSTR("NIVEL %d"), t->tm_yday + 1);
    drawPixelText(g, buf, handheld ? 48 : 10, 10, 2, C_ACC);
    for (uint8_t i = 0; i < 3; i++) drawSprite8(g, SPRITE_HEART, 176 + i * 18, 8, 2, handheld ? C_FG : C_SEC);

    // Fecha
    drawPixelText(g, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year), 120, 40, 3, C_FG, 1);

    // Hora gigante con sombra
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 76, 12, C_FG, 1, C_SEC);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 222, 146, 3, C_ACC, 2);

    // Barra de "experiencia": 20 bloques, uno cada 3 segundos
    sprintf_P(buf, PSTR("SEG %02d"), s);
    drawPixelText(g, buf, 20, 152, 2, C_ACC);
    for (uint8_t i = 0; i < 20; i++) {
        int16_t x = 20 + i * 10;
        g.setColor(C_SEC);
        g.drawRect(x, 166, 9, 10);
        if (i * 3 < s + 1) { g.setColor(C_ACC); g.fillRect(x + 1, 167, 7, 8); }
    }

    // Mascota original que cruza la pantalla saltando, con una estrella al final
    int16_t px = 10 + s * 3;
    bool jump = s % 2;
    drawSprite8(g, jump ? SPRITE_BLOB_A : SPRITE_BLOB_B, px, jump ? 198 : 206, 3, C_FG);
    drawSprite8(g, SPRITE_STAR, 206, 206, 3, (s % 2) ? C_ACC : C_SEC);
    g.setColor(C_SEC);
    for (int16_t x = 4; x < 236; x += 8) g.fillRect(x, 232, 6, 3);

    // Tiempo meteorologico
    drawPixelText(g, ex.temperature, 120, 246, 4, C_FG, 1, C_SEC);
    drawPixelText(g, ex.description, 120, 276, 2, C_ACC, 1, C_NONE, 228);
    drawPixelText(g, ex.location, 120, 296, 2, C_SEC, 1, C_NONE, 228);
}

// =====================================================
// ESTILO FUTURISTA (HUD con anillo de segundos)
// =====================================================
static void headerFuture(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    bool secs = !cfg.isHHMM;
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_SEC);
    sprintf_P(buf, PSTR("%02d.%02d.%04d"), t->tm_mday, t->tm_mon + 1, 1900 + t->tm_year);
    g.drawString(6, 1, String(F("// ")) + WDAY_NAMES[t->tm_wday] + String(F(" ")) + String(buf));
    if (cfg.is12hStyle) {
        g.setColor(C_ACC);
        g.drawString(150, 1, t->tm_hour >= 12 ? "[PM]" : "[AM]");
    }

    buildTimeString(buf, t, cfg, secs);
    int16_t total = drawSegString(g, buf, 120, 16, secs ? 20 : 28, 32, 3, 5, C_FG, C_NONE, SEG_HEX, true);
    g.setColor(C_SEC);
    drawCornerBrackets(g, 120 - total / 2 - 8, 13, total + 16, 38, 6);

    // Barra de progreso de los segundos con marcas cada 5 s
    g.setColor(C_SEC);
    for (uint8_t i = 0; i <= 12; i++) g.drawVerticalLine(20 + i * 50 / 3, 52, i % 3 == 0 ? 4 : 2);
    g.setColor(C_ACC);
    g.fillRect(20, 54, 200 * t->tm_sec / 59, 2);
}

static void fullFuture(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;
    const int16_t cx = 120, cy = 150;

    g.setColor(C_SEC);
    drawCornerBrackets(g, 2, 2, 236, 316, 16);

    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(10, 8, F("SYS.TIME // NTP OK"));
    g.setTextAlignment(TEXT_ALIGN_RIGHT);
    sprintf_P(buf, PSTR("D%03d"), t->tm_yday + 1);
    g.drawString(230, 8, buf);

    g.setFont(ArialMT_Plain_16);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    sprintf_P(buf, PSTR("%02d.%02d.%04d"), t->tm_mday, t->tm_mon + 1, 1900 + t->tm_year);
    g.drawString(cx, 28, WDAY_NAMES[t->tm_wday] + String(F("  ")) + String(buf));

    // Anillo de 60 marcas: las transcurridas en magenta
    for (uint8_t i = 0; i < 60; i++) {
        float a = (i * 6 - 90) * DEG_TO_RAD;
        float ca = cosf(a), sa = sinf(a);
        uint8_t len = (i % 5 == 0) ? 12 : 6;
        g.setColor(i <= s ? C_ACC : C_SEC);
        g.drawLine(cx + ca * (96 - len), cy + sa * (96 - len), cx + ca * 96, cy + sa * 96);
    }
    g.setColor(C_SEC);
    g.drawCircle(cx, cy, 78);
    // Marcador del segundo actual
    float a = (s * 6 - 90) * DEG_TO_RAD;
    g.setColor(C_FG);
    g.fillCircle(cx + cosf(a) * 102, cy + sinf(a) * 102, 3);

    g.setFont(ArialMT_Plain_10);
    g.setColor(C_SEC);
    g.drawString(cx, cy - 46, cfg.is12hStyle ? (t->tm_hour >= 12 ? "HH:MM  PM" : "HH:MM  AM") : "HH:MM  24H");

    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, cx, cy - 28, 28, 52, 5, 5, C_FG, C_NONE, SEG_HEX, s % 2 == 0);

    sprintf_P(buf, PSTR("%02d"), s);
    drawSegString(g, buf, cx, cy + 34, 12, 20, 3, 3, C_ACC, C_NONE, SEG_HEX, true);

    // Paneles de datos
    g.setColor(C_SEC);
    drawCornerBrackets(g, 8, 258, 108, 48, 6);
    drawCornerBrackets(g, 124, 258, 108, 48, 6);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(14, 262, F("TEMP"));
    g.drawString(130, 262, F("LOC"));
    g.setFont(ArialMT_Plain_16);
    g.setColor(C_FG);
    g.drawString(14, 280, ex.temperature);
    g.setFont(ArialMT_Plain_10);
    g.drawStringMaxWidth(130, 276, 98, ex.location);
    g.setColor(C_ACC);
    g.drawStringMaxWidth(130, 290, 98, ex.description);
}

// =====================================================
// UTILIDADES DE LOS ESTILOS "INFORMATICA Y VIDEOJUEGOS"
// =====================================================

// Biselado 3D: luz arriba/izquierda y sombra abajo/derecha (o al reves si hundido)
static void drawBevel(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h,
                      uint8_t light, uint8_t dark, bool sunken = false, uint8_t depth = 2) {
    for (uint8_t i = 0; i < depth; i++) {
        g.setColor(sunken ? dark : light);
        g.drawHorizontalLine(x + i, y + i, w - 2 * i);
        g.drawVerticalLine(x + i, y + i, h - 2 * i);
        g.setColor(sunken ? light : dark);
        g.drawHorizontalLine(x + i, y + h - 1 - i, w - 2 * i);
        g.drawVerticalLine(x + w - 1 - i, y + i, h - 2 * i);
    }
}

// Trama de puntos al 50% (gris simulado en pantallas de 1 bit)
static void fillDither(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    g.setColor(color);
    for (int16_t yy = y; yy < y + h; yy++)
        for (int16_t xx = x + ((x + yy) & 1); xx < x + w; xx += 2) g.setPixel(xx, yy);
}

// Aguja de reloj: rombo alargado de la punta a la cola
static void drawHand(MiniGrafx &g, int16_t cx, int16_t cy, float angleDeg, int16_t len,
                     int16_t halfWidth, int16_t tail, uint8_t color) {
    float a = angleDeg * DEG_TO_RAD;
    float sx = sinf(a), cy_ = -cosf(a);              // direccion de la aguja
    int16_t tx = cx + sx * len, ty = cy + cy_ * len;  // punta
    int16_t bx = cx - sx * tail, by = cy - cy_ * tail;// cola
    int16_t lx = cx - cy_ * halfWidth, ly = cy + sx * halfWidth;
    int16_t rx = cx + cy_ * halfWidth, ry = cy - sx * halfWidth;
    g.setColor(color);
    g.fillTriangle(tx, ty, lx, ly, rx, ry);
    g.fillTriangle(bx, by, lx, ly, rx, ry);
}

// Texto hecho de bloques con brillo (estilo plataformas)
static void drawBlockText(MiniGrafx &g, const String &raw, int16_t cx, int16_t y, uint8_t scale,
                          uint8_t fill, uint8_t light, uint8_t dark) {
    String s = pixelNormalize(raw);
    int16_t x = cx - pixelTextWidth(s, scale) / 2;
    PixelGlyph gl;
    for (uint16_t i = 0; i < s.length(); i++) {
        findGlyph(s[i], gl);
        for (uint8_t r = 0; r < 5; r++)
            for (uint8_t c = 0; c < gl.w; c++)
                if (gl.rows[r] & (1 << (gl.w - 1 - c))) {
                    int16_t bx = x + c * scale, by = y + r * scale;
                    g.setColor(dark);  g.fillRect(bx, by, scale, scale);
                    g.setColor(fill);  g.fillRect(bx, by, scale - 1, scale - 1);
                    g.setColor(light);
                    g.drawHorizontalLine(bx + 1, by + 1, scale - 3);
                    g.drawVerticalLine(bx + 1, by + 1, scale - 3);
                }
        x += (gl.w + 1) * scale;
    }
}

static String uptimeText(uint32_t secs) {
    char buf[24];
    uint32_t d = secs / 86400, h = (secs / 3600) % 24, m = (secs / 60) % 60;
    sprintf_P(buf, PSTR("UP %luD %02lu:%02lu"), (unsigned long)d, (unsigned long)h, (unsigned long)m);
    return String(buf);
}

static const uint8_t SPRITE_HEART_EMPTY[8] = {0x00, 0x66, 0x99, 0x81, 0x42, 0x24, 0x18, 0x00};
static const uint8_t SPRITE_CHEST_SHUT[8]  = {0x00, 0x7E, 0x81, 0xFF, 0x99, 0x81, 0xFF, 0x00};
static const uint8_t SPRITE_CHEST_OPEN[8]  = {0x7E, 0x81, 0x7E, 0x00, 0xFF, 0x99, 0x81, 0xFF};

// =====================================================
// ESTILO ANALOGICO
// =====================================================
static void drawDial(MiniGrafx &g, int16_t cx, int16_t cy, int16_t r, const struct tm *t, bool detailed) {
    g.setColor(C_FG);
    g.drawCircle(cx, cy, r);
    if (detailed) g.drawCircle(cx, cy, r - 1);
    for (uint8_t i = 0; i < 60; i++) {
        if (!detailed && i % 5) continue;
        float a = i * 6 * DEG_TO_RAD;
        float sx = sinf(a), sy = -cosf(a);
        int16_t len = (i % 5 == 0) ? r / 8 : r / 20;
        g.setColor(i % 5 == 0 ? C_FG : C_SEC);
        g.drawLine(cx + sx * (r - 3 - len), cy + sy * (r - 3 - len), cx + sx * (r - 3), cy + sy * (r - 3));
        if (detailed && i % 15 == 0)   // marcas horarias principales, mas gruesas
            g.drawLine(cx + sx * (r - 3 - len) + 1, cy + sy * (r - 3 - len), cx + sx * (r - 3) + 1, cy + sy * (r - 3));
    }
    float hourA = ((t->tm_hour % 12) + t->tm_min / 60.0f) * 30;
    float minA = (t->tm_min + t->tm_sec / 60.0f) * 6;
    drawHand(g, cx, cy, hourA, r * 50 / 100, detailed ? 5 : 2, detailed ? 10 : 2, C_FG);
    drawHand(g, cx, cy, minA, r * 75 / 100, detailed ? 4 : 2, detailed ? 12 : 3, C_FG);
    float sa = t->tm_sec * 6 * DEG_TO_RAD;
    g.setColor(C_ACC);
    g.drawLine(cx - sinf(sa) * r / 6, cy + cosf(sa) * r / 6, cx + sinf(sa) * r * 88 / 100, cy - cosf(sa) * r * 88 / 100);
    g.fillCircle(cx, cy, detailed ? 5 : 2);
    if (detailed) { g.setColor(C_BG); g.fillCircle(cx, cy, 2); }
}

static void headerAnalog(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawDial(g, 28, 28, 25, t, false);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_SEC);
    g.drawString(62, 1, shortDate(t) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""));
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.drawString(146, 12, buf);
}

static void fullAnalog(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    const int16_t cx = 120, cy = 146, r = 106;
    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_SEC);
    g.drawString(cx, 6, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));

    // Numeros de las horas
    g.setFont(ArialMT_Plain_16);
    g.setColor(C_FG);
    for (uint8_t h = 1; h <= 12; h++) {
        float a = h * 30 * DEG_TO_RAD;
        g.drawString(cx + sinf(a) * (r - 28), cy - cosf(a) * (r - 28) - 9, String(h));
    }
    // Ventanilla con el dia del mes
    g.setColor(C_SEC);
    g.drawRect(cx + 38, cy - 9, 24, 18);
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_ACC);
    g.drawString(cx + 50, cy - 7, String(t->tm_mday));
    g.setColor(C_SEC);
    g.drawString(cx, cy + 34, F("TIMEDESK"));

    drawDial(g, cx, cy, r, t, true);

    g.setFont(fontRounded14);
    g.setColor(C_FG);
    buildTimeString(buf, t, cfg, true);
    g.drawString(cx, 262, String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? " PM" : " AM") : ""));
    g.setColor(C_SEC);
    g.drawString(cx, 286, ex.temperature + String(F("  ")) + ex.description);
}

// =====================================================
// ESTILO ESCRITORIO 1-BIT (ordenadores de los 80)
// =====================================================
// Ventana monocroma con barra de titulo rayada, cuadro de cierre y sombra
static void drawMonoWindow(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, const String &title) {
    g.setColor(C_FG);  g.fillRect(x + 2, y + 2, w, h);   // sombra
    g.setColor(C_BG);  g.fillRect(x, y, w, h);
    g.setColor(C_FG);  g.drawRect(x, y, w, h);
    for (int16_t yy = y + 4; yy <= y + 15; yy += 2) g.drawHorizontalLine(x + 2, yy, w - 4);
    g.drawHorizontalLine(x, y + 19, w);
    // Cuadro de cierre
    g.setColor(C_BG);  g.fillRect(x + 7, y + 4, 13, 12);
    g.setColor(C_FG);  g.drawRect(x + 8, y + 5, 11, 10);
    // Titulo sobre fondo liso
    g.setFont(ArialMT_Plain_10);
    uint16_t tw = g.getStringWidth(title.c_str(), title.length()) + 12;
    g.setColor(C_BG);  g.fillRect(x + w / 2 - tw / 2, y + 2, tw, 16);
    g.setColor(C_FG);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.drawString(x + w / 2, y + 4, title);
}

static void headerMono(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setColor(C_FG);
    for (int16_t yy = 2; yy <= 12; yy += 2) g.drawHorizontalLine(0, yy, 190);
    g.setFont(ArialMT_Plain_10);
    String title = String(F("TimeDesk - ")) + shortDate(t);
    uint16_t tw = g.getStringWidth(title.c_str(), title.length()) + 12;
    g.setColor(C_BG); g.fillRect(95 - tw / 2, 0, tw, 15);
    g.setColor(C_FG); g.setTextAlignment(TEXT_ALIGN_CENTER); g.drawString(95, 1, title);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawPixelText(g, buf, 120, 20, 6, C_FG, 1);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 236, 42, 2, C_FG, 2);
}

static void fullMono(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    fillDither(g, 0, 20, 240, 300, C_FG);   // escritorio gris tramado

    // Barra de menus
    g.setColor(C_FG);
    g.drawHorizontalLine(0, 19, 240);
    g.drawCircle(12, 9, 5);                  // icono de reloj
    g.drawLine(12, 9, 12, 6); g.drawLine(12, 9, 14, 10);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(24, 3, F("Archivo  Edicion  Ver  Hora"));
    sprintf_P(buf, PSTR("%02d:%02d"), (cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour), t->tm_min);
    g.setTextAlignment(TEXT_ALIGN_RIGHT);
    g.drawString(235, 3, buf);

    // Ventana del reloj
    drawMonoWindow(g, 12, 32, 214, 168, "Reloj");
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 119, 66, 10, C_FG, 1);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 216, 122, 2, C_FG, 2);
    // Barra de segundos rellenada con trama, como un indicador de progreso
    g.setColor(C_FG);
    g.drawRect(32, 132, 174, 14);
    fillDither(g, 34, 134, 170 * t->tm_sec / 59, 10, C_FG);
    g.setFont(ArialMT_Plain_16);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    g.drawString(119, 156, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));

    // Ventana del tiempo
    drawMonoWindow(g, 34, 218, 184, 84, "Tiempo");
    g.setColor(C_FG);
    g.setFont(ArialMT_Plain_16);
    g.drawString(126, 242, ex.temperature + String(F("  ")) + ex.location);
    g.setFont(ArialMT_Plain_10);
    g.drawStringMaxWidth(126, 264, 170, ex.description);
}

// =====================================================
// ESTILO ESCRITORIO 3D (sistemas de los 90)
// =====================================================
static void drawTitleBar(MiniGrafx &g, int16_t x, int16_t y, int16_t w, const String &title) {
    g.setColor(C_ACC);
    g.fillRect(x, y, w, 18);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    g.drawString(x + 5, y + 3, title);
    // Botones minimizar / maximizar / cerrar
    for (uint8_t i = 0; i < 3; i++) {
        int16_t bx = x + w - 52 + i * 17;
        g.setColor(C_SEC); g.fillRect(bx, y + 2, 15, 14);
        drawBevel(g, bx, y + 2, 15, 14, C_FG, C_ACC, false, 1);
        g.setColor(C_ACC);
        if (i == 0) g.fillRect(bx + 4, y + 11, 6, 2);
        if (i == 1) { g.drawRect(bx + 4, y + 5, 7, 7); g.drawHorizontalLine(bx + 4, y + 6, 7); }
        if (i == 2) { g.drawLine(bx + 4, y + 5, bx + 10, y + 11); g.drawLine(bx + 10, y + 5, bx + 4, y + 11);
                      g.drawLine(bx + 5, y + 5, bx + 11, y + 11); g.drawLine(bx + 11, y + 5, bx + 5, y + 11); }
    }
}

static void headerDesktop(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setColor(C_SEC);
    g.fillRect(0, 0, 240, 56);
    drawBevel(g, 0, 0, 240, 56, C_FG, C_ACC);
    g.setColor(C_ACC);
    g.fillRect(3, 3, 190, 13);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    g.drawString(7, 3, String(F("Reloj - ")) + shortDate(t));
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_ACC);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.drawString(120, 13, buf);
}

static void fullDesktop(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    // Ventana principal
    const int16_t wx = 8, wy = 14, ww = 224, wh = 196;
    g.setColor(C_SEC);
    g.fillRect(wx, wy, ww, wh);
    drawBevel(g, wx, wy, ww, wh, C_FG, C_ACC);
    drawTitleBar(g, wx + 3, wy + 3, ww - 6, "Reloj - TimeDesk");

    // Panel hundido con la hora
    g.setColor(C_FG);
    g.fillRect(wx + 12, wy + 30, ww - 24, 66);
    drawBevel(g, wx + 12, wy + 30, ww - 24, 66, C_FG, C_ACC, true);
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_ACC);
    buildTimeString(buf, t, cfg, true);
    g.drawString(120, wy + 41, buf);

    g.setFont(ArialMT_Plain_16);
    g.drawString(120, wy + 104, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year) +
                 (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""));

    // Barra de progreso de bloques (segundos)
    g.setColor(C_FG);
    g.fillRect(wx + 12, wy + 132, ww - 24, 18);
    drawBevel(g, wx + 12, wy + 132, ww - 24, 18, C_FG, C_ACC, true, 1);
    uint8_t blocks = (t->tm_sec + 1) * 20 / 60;
    g.setColor(C_ACC);
    for (uint8_t i = 0; i < blocks; i++) g.fillRect(wx + 16 + i * 10, wy + 136, 8, 10);

    // Botones Aceptar / Cancelar
    const char *labels[] = {"Aceptar", "Cancelar"};
    g.setFont(ArialMT_Plain_10);
    for (uint8_t i = 0; i < 2; i++) {
        int16_t bx = wx + 46 + i * 70;
        drawBevel(g, bx, wy + 160, 62, 22, C_FG, C_ACC);
        g.setColor(C_ACC);
        g.drawString(bx + 31, wy + 165, labels[i]);
    }

    // Icono del tiempo en el escritorio
    g.setColor(C_FG);
    g.fillCircle(30, 232, 9); g.fillCircle(42, 228, 12); g.fillCircle(54, 234, 8); g.fillRect(26, 234, 32, 8);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(14, 246, ex.temperature + String(F("  ")) + ex.location);
    g.drawStringMaxWidth(14, 260, 210, ex.description);

    // Barra de tareas con boton de menu y bandeja con la hora
    g.setColor(C_SEC);
    g.fillRect(0, 294, 240, 26);
    g.setColor(C_FG);
    g.drawHorizontalLine(0, 295, 240);
    drawBevel(g, 3, 298, 62, 20, C_FG, C_ACC);
    g.setColor(C_ACC);
    g.drawCircle(14, 308, 5); g.drawLine(14, 308, 14, 305); g.drawLine(14, 308, 16, 309);
    g.drawString(24, 302, F("Menu"));
    g.drawString(25, 302, F("Menu"));   // negrita simulada
    drawBevel(g, 176, 298, 61, 20, C_FG, C_ACC, true, 1);
    sprintf_P(buf, PSTR("%02d:%02d"), (cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour), t->tm_min);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.drawString(206, 302, buf);
}

// =====================================================
// ESTILO TERMINAL (fosforo verde)
// =====================================================
static int16_t termPrompt(MiniGrafx &g, int16_t y, const String &cmd) {
    static const char PROMPT[] = "TIMEDESK:~$ ";
    drawPixelText(g, PROMPT, 6, y, 2, C_ACC);
    int16_t x = 6 + pixelTextWidth(PROMPT, 2) + 2;
    drawPixelText(g, cmd, x, y, 2, C_FG);
    return x + pixelTextWidth(cmd, 2) + (cmd.length() ? 6 : 0);
}

static void fullTerminal(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[40];
    // Lineas de barrido del CRT
    g.setColor(C_SEC);
    for (int16_t y = 2; y < 320; y += 4) g.drawHorizontalLine(0, y, 240);

    termPrompt(g, 6, "date");
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", t);
    drawPixelText(g, WDAY_NAMES[t->tm_wday] + String(F(" ")) + String(buf), 6, 20, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 40, "clock --big");
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 58, 10, C_FG, 1);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 234, 98, 2, C_ACC, 2);
    String bar = "[";
    for (uint8_t i = 0; i < 20; i++) bar += (i * 3 <= t->tm_sec) ? '#' : '.';
    sprintf_P(buf, PSTR("] %02dS"), t->tm_sec);
    drawPixelText(g, bar + buf, 6, 116, 2, C_FG);

    termPrompt(g, 140, "uptime");
    drawPixelText(g, uptimeText(ex.uptimeSec) + String(F("  MEM ")) + String(ex.freeHeap) + "B", 6, 154, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 174, "weather");
    drawPixelText(g, ex.temperature + String(F(" ")) + ex.location, 6, 188, 2, C_FG, 0, C_NONE, 228);
    drawPixelText(g, ex.description, 6, 202, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 222, "uname -a");
    drawPixelText(g, F("TIMEDESK ESP8266 XTENSA"), 6, 236, 2, C_FG, 0, C_NONE, 228);

    int16_t cx = termPrompt(g, 256, "");
    if (t->tm_sec % 2 == 0) { g.setColor(C_FG); g.fillRect(cx, 255, 8, 12); }
}

static void headerTerminal(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawPixelText(g, String(F("~$ DATE  ")) + shortDate(t), 4, 3, 2, C_ACC);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawPixelText(g, buf, 8, 18, 6, C_FG);
    int16_t x = 8 + pixelTextWidth(pixelNormalize(buf), 6) + 6;
    if (t->tm_sec % 2 == 0) { g.setColor(C_FG); g.fillRect(x, 42, 12, 6); }
}

// =====================================================
// ESTILO MICROORDENADOR 8-BIT (BASIC)
// =====================================================
static void fullMicro(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    // Marco de color alrededor de la pantalla de texto
    g.setColor(C_FG);
    g.fillRect(0, 0, 240, 20);
    g.fillRect(0, 300, 240, 20);
    g.fillRect(0, 20, 12, 280);
    g.fillRect(228, 20, 12, 280);

    drawPixelText(g, F("** TIMEDESK BASIC **"), 120, 28, 2, C_FG, 1);
    drawPixelText(g, String(F("RAM LIBRE ")) + String(ex.freeHeap) + " BYTES", 120, 42, 2, C_FG, 1);
    drawPixelText(g, F("LISTO."), 16, 62, 2, C_FG);
    drawPixelText(g, F("PRINT HORA$"), 16, 76, 2, C_FG);
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 94, 9, C_ACC, 1);
    sprintf_P(buf, PSTR("SEG %02d"), t->tm_sec);
    drawPixelText(g, String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""), 16, 146, 2, C_SEC);
    drawPixelText(g, F("LISTO."), 16, 166, 2, C_FG);
    drawPixelText(g, F("PRINT TIEMPO$"), 16, 180, 2, C_FG);
    drawPixelText(g, ex.temperature + String(F(" ")) + ex.location, 16, 196, 2, C_ACC, 0, C_NONE, 208);
    drawPixelText(g, ex.description, 16, 210, 2, C_ACC, 0, C_NONE, 208);
    drawPixelText(g, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year), 16, 224, 2, C_ACC);
    drawPixelText(g, F("LISTO."), 16, 246, 2, C_FG);
    if (t->tm_sec % 2 == 0) { g.setColor(C_FG); g.fillRect(16, 260, 8, 10); }
}

static void headerMicro(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setColor(C_FG);
    g.fillRect(0, 0, 6, 56);
    drawPixelText(g, String(F("LISTO.  ")) + shortDate(t), 12, 3, 2, C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawPixelText(g, buf, 120, 18, 6, C_ACC, 1);
}

// =====================================================
// ESTILO AVENTURA (fantasia 16-bit)
// =====================================================
static void drawHearts(MiniGrafx &g, int16_t x, int16_t y, uint8_t filled, uint8_t total, uint8_t scale) {
    for (uint8_t i = 0; i < total; i++)
        drawSprite8(g, i < filled ? SPRITE_HEART : SPRITE_HEART_EMPTY, x + i * 9 * scale, y, scale,
                    i < filled ? C_SEC : C_FG);
}

static void drawDialogBox(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h) {
    g.setColor(C_BG);
    g.fillRect(x, y, w, h);
    g.setColor(C_FG);
    g.drawRect(x, y, w, h);
    g.drawRect(x + 3, y + 3, w - 6, h - 6);
    g.setColor(C_ACC);   // remaches dorados en las esquinas
    g.fillRect(x, y, 3, 3); g.fillRect(x + w - 3, y, 3, 3);
    g.fillRect(x, y + h - 3, 3, 3); g.fillRect(x + w - 3, y + h - 3, 3, 3);
}

static void fullAdventure(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;

    // Marcador: vida (un corazon cada 6 segundos) y oro (dia del ano)
    drawPixelText(g, F("-VIDA-"), 10, 6, 2, C_SEC);
    drawHearts(g, 10, 20, s / 6 + 1, 10, 1);
    g.setColor(C_ACC);
    g.fillCircle(176, 24, 5);
    g.setColor(C_BG);
    g.drawVerticalLine(176, 21, 7);
    drawPixelText(g, F("ORO"), 188, 6, 2, C_ACC);
    sprintf_P(buf, PSTR("X%03d"), t->tm_yday + 1);
    drawPixelText(g, buf, 186, 20, 2, C_FG);
    g.setColor(C_FG);
    g.drawHorizontalLine(6, 34, 228);

    // Escenario: hierba y piedras en posiciones fijas
    for (uint8_t i = 0; i < 26; i++) {
        int16_t x = (i * 73 + 17) % 228 + 6, y = (i * 41 + 29) % 150 + 44;
        g.setColor(i % 4 ? C_FG : C_SEC);
        if (i % 4) { g.drawLine(x, y, x - 2, y - 3); g.drawLine(x, y, x + 2, y - 3); g.drawLine(x, y, x, y - 4); }
        else g.fillCircle(x, y, 2);
    }

    // Hora en grande con sombra
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 58, 10, C_ACC, 1, C_SEC);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 228, 112, 2, C_FG, 2);

    // Cofre que se abre la primera mitad de cada 10 segundos
    bool open = (s % 10) < 5;
    drawSprite8(g, open ? SPRITE_CHEST_OPEN : SPRITE_CHEST_SHUT, 96, 136, 6, C_ACC);
    if (open) drawSparkle(g, 120, 128, (s % 2) ? 8 : 5, C_FG);
    sprintf_P(buf, PSTR("%02d"), s);
    drawPixelText(g, buf, 120, 190, 2, C_FG, 1);

    // Cuadro de dialogo
    drawDialogBox(g, 8, 210, 224, 98);
    buildTimeString(buf, t, cfg, false);
    String hora = String(buf);
    hora.trim();
    drawPixelText(g, String(F("SON LAS ")) + hora + (cfg.is12hStyle ? (t->tm_hour >= 12 ? " PM." : " AM.") : "."), 20, 224, 2, C_FG);
    drawPixelText(g, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year), 20, 240, 2, C_FG);
    drawPixelText(g, String(F("HACE ")) + ex.temperature + " EN", 20, 256, 2, C_FG, 0, C_NONE, 200);
    drawPixelText(g, ex.location + String(F(": ")) + ex.description, 20, 272, 2, C_FG, 0, C_NONE, 200);
    if (s % 2 == 0) { g.setColor(C_ACC); g.fillTriangle(212, 290, 222, 290, 217, 297); }
}

static void headerAdventure(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawHearts(g, 4, 3, t->tm_sec / 6 + 1, 10, 1);
    drawPixelText(g, shortDate(t), 100, 4, 1, C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawPixelText(g, buf, 120, 16, 7, C_ACC, 1, C_SEC);
}

// =====================================================
// ESTILO VELOCIDAD (carreras arcade 16-bit)
// =====================================================
static void fullSpeed(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;
    const int16_t hy = 186, vx = 120;   // horizonte y punto de fuga

    // Marcador
    drawPixelText(g, F("TIEMPO"), 8, 6, 2, C_ACC);
    sprintf_P(buf, PSTR("VUELTA %02d/24"), t->tm_hour + 1);
    drawPixelText(g, buf, 232, 6, 2, C_ACC, 2);
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 114, 24, 10, C_FG, 1, C_SEC, 240, 2);   // cursiva
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 232, 66, 2, C_ACC, 2);
    drawPixelText(g, ex.temperature + String(F("  ")) + ex.description, 120, 88, 2, C_ACC, 1, C_NONE, 228);

    // Sol con franjas
    g.setColor(C_ACC);
    g.fillCircle(vx, hy - 10, 46);
    g.setColor(C_BG);
    for (int16_t i = 0, y = hy - 22; y < hy; i++, y += 6) g.fillRect(vx - 50, y, 100, 1 + i);
    g.fillRect(0, hy - 10 + 47, 240, 20);   // borra el sol bajo el horizonte

    // Montanas
    g.setColor(C_SEC);
    const int16_t peaks[] = {0, 160, 30, 140, 60, 170, 180, 150, 210, 132, 240, 162};
    for (uint8_t i = 0; i < 5; i++) g.drawLine(peaks[i * 2], peaks[i * 2 + 1], peaks[i * 2 + 2], peaks[i * 2 + 3]);
    g.drawLine(60, 170, 90, hy); g.drawLine(150, hy, 180, 150);

    // Suelo en rejilla con perspectiva (se desplaza con los segundos)
    g.setColor(C_SEC);
    g.drawHorizontalLine(0, hy, 240);
    for (uint8_t k = 1; k < 12; k++) {
        float z = k + (s % 4) / 4.0f;
        int16_t y = hy + (int16_t)(z * z * 1.1f);
        if (y < 320) g.drawHorizontalLine(0, y, 240);
    }
    for (int8_t k = -6; k <= 6; k++) g.drawLine(vx + k * 4, hy, vx + k * 44, 320);

    // Carretera
    g.setColor(C_BG);
    g.fillTriangle(vx, hy, vx - 110, 320, vx + 110, 320);
    g.setColor(C_FG);
    g.drawLine(vx - 2, hy, vx - 112, 319);
    g.drawLine(vx + 2, hy, vx + 112, 319);
    for (uint8_t k = 0; k < 8; k++) {   // lineas centrales en movimiento
        float z0 = k * 2 + (s % 2), z1 = z0 + 1;
        int16_t y0 = hy + (int16_t)(z0 * z0 * 0.55f), y1 = hy + (int16_t)(z1 * z1 * 0.55f);
        if (y0 >= 320) break;
        g.fillRect(vx - 1 - z0 / 6, y0, 3 + z0 / 3, y1 - y0);
    }

    // Velocimetro
    g.setColor(C_BG);
    g.fillRect(2, 280, 66, 38);
    g.setColor(C_SEC);
    g.drawRect(2, 280, 66, 38);
    sprintf_P(buf, PSTR("%d"), 180 + s * 2);
    drawPixelText(g, buf, 8, 284, 4, C_ACC, 0, C_NONE, 240, 1);
    drawPixelText(g, F("KM/H"), 8, 306, 2, C_FG);
    drawPixelText(g, shortDate(t), 232, 306, 2, C_FG, 2);
}

static void headerSpeed(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    for (uint8_t i = 0; i < 5; i++) {   // lineas de velocidad
        g.setColor(i % 2 ? C_SEC : C_ACC);
        g.drawHorizontalLine(2 + i * 3, 18 + i * 7, 22 - i * 3);
    }
    drawPixelText(g, String(F("TIEMPO  ")) + shortDate(t), 30, 3, 2, C_ACC);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawPixelText(g, buf, 126, 17, 7, C_FG, 1, C_SEC, 240, 1);
}

// =====================================================
// ESTILO PLATAFORMAS (8-bit)
// =====================================================
static void drawCloud(MiniGrafx &g, int16_t x, int16_t y, uint8_t size) {
    g.setColor(C_FG);
    g.fillCircle(x, y, size);
    g.fillCircle(x + size, y - size / 2, size);
    g.fillCircle(x + size * 2, y, size);
    g.fillRect(x, y, size * 2, size);
}

static void fullPlatform(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;

    // Nubes que avanzan despacio
    drawCloud(g, (s * 2 + 10) % 280 - 30, 64, 10);
    drawCloud(g, (s * 2 + 150) % 280 - 30, 44, 8);

    drawPixelText(g, ex.temperature + String(F(" ")) + ex.location, 8, 6, 2, C_FG, 0, C_NONE, 140);
    drawPixelText(g, shortDate(t), 232, 6, 2, C_FG, 2);
    drawPixelText(g, ex.description, 8, 20, 2, C_FG, 0, C_NONE, 224);

    // Colinas
    g.setColor(C_SEC);
    g.fillCircle(48, 300, 62);
    g.fillCircle(196, 306, 46);

    // Hora hecha de bloques
    buildTimeString(buf, t, cfg, false);
    drawBlockText(g, buf, 120, 88, 10, C_ACC, C_FG, C_SEC);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 232, 142, 2, C_FG, 2);

    // Plataforma flotante con los segundos
    sprintf_P(buf, PSTR("%02d"), s);
    drawBlockText(g, buf, 120, 166, 5, C_ACC, C_FG, C_SEC);

    // Estrellas que la mascota va recogiendo
    int16_t px = 8 + s * 3;
    for (uint8_t i = 0; i < 5; i++) {
        int16_t sx = 36 + i * 40;
        if (sx > px + 12) drawSprite8(g, SPRITE_STAR, sx, 214, 2, C_FG);
    }
    bool jump = s % 2;
    drawSprite8(g, jump ? SPRITE_BLOB_A : SPRITE_BLOB_B, px, jump ? 232 : 248, 3, C_FG);

    // Suelo de bloques con hierba
    g.setColor(C_SEC);
    g.fillRect(0, 272, 240, 6);
    for (int16_t x = 0; x < 240; x += 20)
        for (int16_t y = 278; y < 320; y += 20) {
            g.setColor(C_ACC); g.fillRect(x, y, 19, 19);
            g.setColor(C_FG);  g.drawHorizontalLine(x, y, 19); g.drawVerticalLine(x, y, 19);
        }
}

static void headerPlatform(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawCloud(g, 6, 12, 6);
    drawPixelText(g, shortDate(t), 40, 3, 2, C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawBlockText(g, buf, 128, 18, 6, C_ACC, C_FG, C_SEC);
}

// =====================================================
// UTILIDADES: TRAZO GRUESO, ELIPSES Y FUENTE VECTORIAL
// =====================================================

// Pincel redondo de radio r, solo dentro de las filas [yMin, yMax]
static void brush(MiniGrafx &g, int16_t x, int16_t y, uint8_t r, int16_t yMin, int16_t yMax) {
    for (int16_t dy = -r; dy <= r; dy++) {
        int16_t yy = y + dy;
        if (yy < yMin || yy > yMax) continue;
        int16_t hw = (int16_t)sqrtf((float)(r * r - dy * dy));
        g.drawHorizontalLine(x - hw, yy, 2 * hw + 1);
    }
}

static void thickLine(MiniGrafx &g, float x0, float y0, float x1, float y1, uint8_t r,
                      int16_t yMin = 0, int16_t yMax = 319) {
    float dx = x1 - x0, dy = y1 - y0;
    int16_t steps = (int16_t)fmaxf(fabsf(dx), fabsf(dy));
    if (steps < 1) steps = 1;
    // Con pincel grueso no hace falta estampar en cada pixel
    int16_t stride = r > 2 ? r / 2 : 1;
    for (int16_t i = 0; i <= steps; i += stride)
        brush(g, (int16_t)lroundf(x0 + dx * i / steps), (int16_t)lroundf(y0 + dy * i / steps), r, yMin, yMax);
    brush(g, (int16_t)lroundf(x1), (int16_t)lroundf(y1), r, yMin, yMax);
}

static void fillEllipse(MiniGrafx &g, int16_t cx, int16_t cy, int16_t rx, int16_t ry) {
    for (int16_t dy = -ry; dy <= ry; dy++) {
        int16_t hw = (int16_t)(rx * sqrtf(1.0f - (float)(dy * dy) / (ry * ry)));
        g.drawHorizontalLine(cx - hw, cy + dy, 2 * hw + 1);
    }
}

// Dígitos de trazo continuo en una rejilla de 10x16 (255 = levantar el lápiz).
// Sirven para los tubos Nixie (trazo fino con brillo) y las tarjetas abatibles
// (trazo grueso). El 0 y el 8 se dibujan con elipses.
static const uint8_t VDIGIT_1[] = {3,3, 6,0, 6,16};
static const uint8_t VDIGIT_2[] = {1,4, 2,1, 5,0, 8,1, 9,4, 8,7, 1,16, 10,16};
static const uint8_t VDIGIT_3[] = {1,2, 4,0, 8,1, 9,4, 7,7, 4,8, 255,255, 7,8, 9,11, 9,14, 6,16, 2,16, 0,14};
static const uint8_t VDIGIT_4[] = {7,16, 7,0, 0,11, 10,11};
static const uint8_t VDIGIT_5[] = {9,0, 2,0, 1,7, 5,6, 8,7, 9,10, 9,13, 6,16, 3,16, 0,14};
static const uint8_t VDIGIT_6[] = {8,1, 5,0, 2,2, 1,6, 1,12, 3,16, 7,16, 9,13, 9,10, 7,7, 3,7, 1,9};
static const uint8_t VDIGIT_7[] = {0,0, 10,0, 4,16};
static const uint8_t VDIGIT_9[] = {2,15, 5,16, 8,14, 9,10, 9,4, 7,1, 4,0, 1,2, 1,6, 3,9, 7,9, 9,7};
static const uint8_t *const VDIGITS[10] = {nullptr, VDIGIT_1, VDIGIT_2, VDIGIT_3, VDIGIT_4,
                                           VDIGIT_5, VDIGIT_6, VDIGIT_7, nullptr, VDIGIT_9};
static const uint8_t VDIGIT_LEN[10] = {0, sizeof(VDIGIT_1), sizeof(VDIGIT_2), sizeof(VDIGIT_3), sizeof(VDIGIT_4),
                                       sizeof(VDIGIT_5), sizeof(VDIGIT_6), sizeof(VDIGIT_7), 0, sizeof(VDIGIT_9)};

static void vEllipse(MiniGrafx &g, float cx, float cy, float rx, float ry, uint8_t r, int16_t yMin, int16_t yMax) {
    const uint8_t N = 28;
    float px = cx + rx, py = cy;
    for (uint8_t i = 1; i <= N; i++) {
        float a = i * 2 * PI / N;
        float nx = cx + cosf(a) * rx, ny = cy + sinf(a) * ry;
        thickLine(g, px, py, nx, ny, r, yMin, yMax);
        px = nx; py = ny;
    }
}

// Dibuja el dígito d (0-9; otro valor = nada) en la caja (x, y, w, h)
static void drawVectorDigit(MiniGrafx &g, int8_t d, int16_t x, int16_t y, int16_t w, int16_t h,
                            uint8_t r, int16_t yMin = 0, int16_t yMax = 319) {
    if (d < 0 || d > 9) return;
    float sx = w / 10.0f, sy = h / 16.0f;
    if (d == 0) { vEllipse(g, x + 5 * sx, y + 8 * sy, 4.6f * sx, 8 * sy, r, yMin, yMax); return; }
    if (d == 8) {
        vEllipse(g, x + 5 * sx, y + 3.8f * sy, 3.8f * sx, 3.8f * sy, r, yMin, yMax);
        vEllipse(g, x + 5 * sx, y + 11.6f * sy, 4.6f * sx, 4.4f * sy, r, yMin, yMax);
        return;
    }
    const uint8_t *p = VDIGITS[d];
    for (uint8_t i = 2; i + 1 < VDIGIT_LEN[d]; i += 2) {
        if (p[i] == 255 || p[i - 2] == 255) continue;
        thickLine(g, x + p[i - 2] * sx, y + p[i - 1] * sy, x + p[i] * sx, y + p[i + 1] * sy, r, yMin, yMax);
    }
}

static int8_t digitAt(const char *s, uint8_t i) {
    return (s[i] >= '0' && s[i] <= '9') ? s[i] - '0' : -1;
}

// Pseudoaleatorio determinista (posiciones fijas de estrellas, burbujas...)
static uint16_t prand(uint16_t i, uint16_t salt) {
    uint32_t v = (uint32_t)(i + 1) * 2654435761UL ^ (uint32_t)salt * 40503UL;
    return (v >> 13) & 0xFFFF;
}

// =====================================================
// ESTILO SEGUN EL TIEMPO
// =====================================================
enum WxKind : uint8_t { WX_CLEAR_DAY, WX_CLEAR_NIGHT, WX_CLOUDS, WX_RAIN, WX_STORM, WX_SNOW, WX_MIST };

static const uint16_t WX_PALETTES[7][4] = {
    {0x3D7F, 0xFFFF, 0xFFE0, 0x1A7A},   // despejado de dia: cielo, blanco, sol, azul oscuro
    {0x0006, 0xFFFF, 0xFFB0, 0x4A6F},   // despejado de noche
    {0x7D19, 0xFFFF, 0xCE59, 0x4A49},   // nubes
    {0x31A8, 0xFFFF, 0x5D9F, 0x7BEF},   // lluvia
    {0x1003, 0xFFFF, 0xFFE0, 0x6A6F},   // tormenta
    {0x4A92, 0xFFFF, 0xBEFF, 0x8D1A},   // nieve
    {0x5AEB, 0xFFFF, 0xBDF7, 0x8410},   // niebla
};
static char currentWxIcon[4] = "01d";

void clockSetWeatherIcon(const String &icon) {
    strlcpy(currentWxIcon, icon.length() >= 3 ? icon.c_str() : "01d", sizeof(currentWxIcon));
}

static WxKind weatherKind();
static const uint16_t *weatherPalette() { return WX_PALETTES[weatherKind()]; }

static WxKind weatherKind() {
    int code = atoi(currentWxIcon);
    bool night = currentWxIcon[2] == 'n';
    switch (code) {
        case 1: case 2: return night ? WX_CLEAR_NIGHT : WX_CLEAR_DAY;
        case 3: case 4: return WX_CLOUDS;
        case 9: case 10: return WX_RAIN;
        case 11: return WX_STORM;
        case 13: return WX_SNOW;
        case 50: return WX_MIST;
        default: return night ? WX_CLEAR_NIGHT : WX_CLEAR_DAY;
    }
}

static void drawPuffCloud(MiniGrafx &g, int16_t x, int16_t y, uint8_t s, uint8_t body, uint8_t shade) {
    g.setColor(shade);
    fillEllipse(g, x + 2, y + 3, s * 2, s * 2 / 3 + 1);
    g.setColor(body);
    g.fillCircle(x - s, y, s * 2 / 3);
    g.fillCircle(x, y - s / 2, s);
    g.fillCircle(x + s, y, s * 3 / 4);
    fillEllipse(g, x, y + 1, s * 2 - 2, s * 2 / 3);
}

// Escena del tiempo dentro de la caja (0, 0, 240, sceneH)
static void drawWeatherScene(MiniGrafx &g, uint8_t s, int16_t sunX, int16_t sunY, uint8_t size, int16_t sceneH, bool full) {
    WxKind k = weatherKind();
    bool fewClouds = atoi(currentWxIcon) == 2;
    if (k == WX_CLEAR_DAY) {
        g.setColor(C_ACC);
        g.fillCircle(sunX, sunY, size);
        for (uint8_t i = 0; i < 12; i++) {   // rayos que giran despacio
            float a = (i * 30 + s * 3) * DEG_TO_RAD;
            thickLine(g, sunX + cosf(a) * (size + 5), sunY + sinf(a) * (size + 5),
                      sunX + cosf(a) * (size + 5 + size / 2), sunY + sinf(a) * (size + 5 + size / 2), full ? 1 : 0);
        }
    } else if (k == WX_CLEAR_NIGHT) {
        for (uint8_t i = 0; i < (full ? 40 : 12); i++) {
            int16_t x = prand(i, 7) % 240, y = prand(i, 11) % sceneH;
            g.setColor(((i + s) % 6) ? C_FG : C_SEC);
            g.setPixel(x, y);
            if (i % 5 == 0) { g.setPixel(x + 1, y); g.setPixel(x, y + 1); g.setPixel(x - 1, y); g.setPixel(x, y - 1); }
        }
        g.setColor(C_ACC);
        g.fillCircle(sunX, sunY, size);
        g.setColor(C_BG);
        g.fillCircle(sunX + size / 2, sunY - size / 4, size * 9 / 10);
    }
    if (fewClouds || k == WX_CLOUDS || k == WX_RAIN || k == WX_STORM || k == WX_SNOW) {
        uint8_t body = (k == WX_CLOUDS || fewClouds) ? C_FG : C_SEC;
        uint8_t shade = (k == WX_CLOUDS) ? C_ACC : (fewClouds ? C_SEC : C_BG);
        uint8_t cs = full ? 18 : 8;
        int16_t drift = (s * 2) % 280;
        drawPuffCloud(g, (40 + drift) % 280 - 20, full ? 50 : 18, cs, body, shade);
        if (!fewClouds) {
            drawPuffCloud(g, (170 + drift) % 280 - 20, full ? 34 : 12, cs * 4 / 5, body, shade);
            if (full) drawPuffCloud(g, (110 + drift) % 280 - 20, 86, cs * 3 / 4, body, shade);
        }
    }
    int16_t top = full ? 60 : 24, h = (full ? 320 : 56) - top;
    if (k == WX_RAIN || k == WX_STORM) {
        g.setColor(C_ACC);
        for (uint8_t i = 0; i < (full ? 46 : 14); i++) {
            int16_t x = prand(i, 3) % 250, y = top + (prand(i, 5) + s * 23) % h;
            g.drawLine(x, y, x - 3, y + 8);
        }
    }
    if (k == WX_STORM && (s % 6 == 0 || s % 6 == 1)) {   // rayo
        g.setColor(C_ACC);
        int16_t bx = full ? 150 : 200, by = full ? 70 : 20, sc = full ? 2 : 1;
        int16_t pts[] = {0,0, -10,26, 2,26, -8,52, 14,18, 2,18, 10,0};
        for (uint8_t i = 0; i < 6; i++)
            thickLine(g, bx + pts[i * 2] * sc, by + pts[i * 2 + 1] * sc, bx + pts[i * 2 + 2] * sc, by + pts[i * 2 + 3] * sc, sc);
    }
    if (k == WX_SNOW) {
        g.setColor(C_FG);
        for (uint8_t i = 0; i < (full ? 40 : 12); i++) {
            int16_t y = top + (prand(i, 9) + s * 6) % h;
            int16_t x = prand(i, 13) % 240 + (int16_t)(sinf((y + i * 20) * 0.05f) * 6);
            g.drawLine(x - 2, y, x + 2, y); g.drawLine(x, y - 2, x, y + 2);
            g.setPixel(x - 1, y - 1); g.setPixel(x + 1, y + 1); g.setPixel(x + 1, y - 1); g.setPixel(x - 1, y + 1);
        }
    }
    if (k == WX_MIST) {
        for (uint8_t i = 0; i < (full ? 9 : 3); i++) {
            int16_t y = (full ? 20 : 6) + i * (full ? 22 : 16);
            int16_t off = ((s + i * 7) * 3) % 60;
            g.setColor(i % 2 ? C_SEC : C_ACC);
            for (int16_t x = -60 + off; x < 240; x += 70) g.fillRect(x, y, 46, full ? 4 : 2);
        }
    }
}

static void headerWeather(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawWeatherScene(g, t->tm_sec, 26, 24, 11, 56, false);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    g.drawString(60, 1, shortDate(t));
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.setColor(C_SEC); g.drawString(150, 14, buf);
    g.setColor(C_FG);  g.drawString(148, 12, buf);
}

static void fullWeather(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    drawWeatherScene(g, t->tm_sec, 186, 50, 26, 120, true);

    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 123, 131, 38, 66, 9, 7, C_SEC, C_NONE, SEG_ROUND, true);   // sombra
    drawSegString(g, buf, 120, 128, 38, 66, 9, 7, C_FG, C_NONE, SEG_ROUND, true);
    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    sprintf_P(buf, PSTR("%02d"), t->tm_sec);
    g.drawString(120, 200, shortDate(t) + String(F("   :")) + String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""));
    g.setFont(fontRounded36);
    g.setColor(C_SEC); g.drawString(122, 228, ex.temperature);
    g.setColor(C_FG);  g.drawString(120, 226, ex.temperature);
    g.setFont(fontRounded14);
    g.drawString(120, 274, ex.description);
    g.setFont(ArialMT_Plain_10);
    g.drawString(120, 298, ex.location);
}

// =====================================================
// ESTILO NOCHE ESTRELLADA (con la fase lunar real)
// =====================================================
// Luna con la parte iluminada según la fase (terminador elíptico por filas)
static void drawMoonPhase(MiniGrafx &g, int16_t cx, int16_t cy, int16_t r, float illum, bool litRight) {
    for (int16_t dy = -r; dy <= r; dy++) {
        float w = sqrtf((float)(r * r - dy * dy));
        float xt = w * (1.0f - 2.0f * illum);          // posición del terminador
        int16_t a = litRight ? (int16_t)xt : (int16_t)-w;
        int16_t b = litRight ? (int16_t)w : (int16_t)-xt;
        g.setColor(C_SEC);                              // luz cenicienta
        g.drawHorizontalLine(cx - (int16_t)w, cy + dy, 2 * (int16_t)w + 1);
        if (b > a) { g.setColor(C_ACC); g.drawHorizontalLine(cx + a, cy + dy, b - a + 1); }
    }
}

static void drawStars(MiniGrafx &g, uint8_t n, int16_t maxY, uint8_t s) {
    for (uint8_t i = 0; i < n; i++) {
        int16_t x = prand(i, 21) % 240, y = prand(i, 37) % maxY;
        bool twinkleOff = (i + s) % 9 == 0;
        g.setColor((i % 4 == 0 && !twinkleOff) ? C_FG : C_SEC);
        g.setPixel(x, y);
        if (i % 7 == 0 && !twinkleOff) { g.setPixel(x - 1, y); g.setPixel(x + 1, y); g.setPixel(x, y - 1); g.setPixel(x, y + 1); }
    }
}

static void fullNight(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[32];
    uint8_t s = t->tm_sec;
    drawStars(g, 90, 270, s);

    // Osa Mayor
    static const int16_t DIPPER[7][2] = {{22,36}, {46,42}, {66,56}, {86,66}, {92,92}, {124,98}, {128,72}};
    g.setColor(C_SEC);
    for (uint8_t i = 0; i < 6; i++) g.drawLine(DIPPER[i][0], DIPPER[i][1], DIPPER[i + 1][0], DIPPER[i + 1][1]);
    g.drawLine(DIPPER[3][0], DIPPER[3][1], DIPPER[6][0], DIPPER[6][1]);
    g.setColor(C_FG);
    for (uint8_t i = 0; i < 7; i++) { g.fillCircle(DIPPER[i][0], DIPPER[i][1], 1); }

    // Estrella fugaz unos segundos de cada minuto
    if (s >= 20 && s <= 22) {
        int16_t x0 = 60 + (s - 20) * 40, y0 = 20 + (s - 20) * 12;
        g.setColor(C_FG);  g.drawLine(x0, y0, x0 + 26, y0 + 8);
        g.setColor(C_SEC); g.drawLine(x0 - 24, y0 - 7, x0, y0);
    }

    bool litRight = ex.moonWaxing == ex.northernHemisphere;
    drawMoonPhase(g, 182, 72, 30, ex.moonIllumination, litRight);

    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 120, 140, 34, 60, 4, 6, C_FG, C_NONE, SEG_HEX, s % 2 == 0);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_ACC);
    sprintf_P(buf, PSTR("Luna %s %d%%"), ex.moonWaxing ? "creciente" : "menguante", (int)lroundf(ex.moonIllumination * 100));
    g.drawString(120, 210, buf);
    g.setFont(fontRounded14);
    g.setColor(C_FG);
    g.drawString(120, 228, shortDate(t) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""));

    // Colinas con pinos en silueta
    g.setColor(C_SEC);
    g.fillCircle(40, 380, 130);
    g.fillCircle(210, 400, 140);
    g.setColor(C_BG);
    static const int16_t PINES[][3] = {{18,262,14}, {34,258,18}, {196,272,16}, {214,268,20}, {230,274,12}};
    for (auto &p : PINES) {
        g.fillTriangle(p[0], p[1] - p[2], p[0] - p[2] / 2, p[1] + 4, p[0] + p[2] / 2, p[1] + 4);
        g.fillRect(p[0] - 1, p[1] + 4, 3, 5);
    }
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_FG);
    g.drawString(120, 290, ex.temperature + String(F("  ")) + ex.description);
    g.drawString(120, 304, ex.location);
}

static void headerNight(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawStars(g, 30, 56, t->tm_sec);
    drawMoonPhase(g, 22, 28, 15, 0.35f, true);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_ACC);
    g.drawString(48, 1, shortDate(t));
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawSegString(g, buf, 140, 16, cfg.isHHMM ? 28 : 20, 34, 3, 4, C_FG, C_NONE, SEG_HEX, true);
}

// =====================================================
// ESTILO ACUARIO
// =====================================================
static void drawFish(MiniGrafx &g, int16_t x, int16_t y, int16_t size, bool right) {
    int8_t d = right ? 1 : -1;
    g.setColor(C_ACC);
    fillEllipse(g, x, y, size, size * 3 / 5);
    g.fillTriangle(x - d * size + d * 2, y, x - d * (size + size * 3 / 4), y - size / 2, x - d * (size + size * 3 / 4), y + size / 2);
    g.fillTriangle(x - d * size / 3, y - size / 2, x + d * size / 4, y - size / 2, x - d * size / 2, y - size);   // aleta
    g.setColor(C_FG);   // franjas y ojo
    g.drawVerticalLine(x - d * size / 3, y - size / 2 + 1, size - 1);
    g.drawVerticalLine(x - d * size / 3 + 1, y - size / 2 + 1, size - 1);
    g.fillCircle(x + d * size * 3 / 5, y - size / 6, size > 12 ? 3 : 2);
    g.setColor(C_BG);
    g.setPixel(x + d * size * 3 / 5 + d, y - size / 6);
}

static void drawSeaweed(MiniGrafx &g, int16_t x, int16_t height, uint8_t s, uint8_t salt) {
    g.setColor(C_SEC);
    float px = x, py = 320;
    for (int16_t y = 316; y > 320 - height; y -= 8) {
        float nx = x + sinf(y * 0.07f + s * 0.6f + salt) * 6 * (320 - y) / height;
        thickLine(g, px, py, nx, y, 2);
        px = nx; py = y;
    }
}

static void drawBubbles(MiniGrafx &g, uint8_t n, int16_t maxY, uint8_t s, uint8_t speed) {
    g.setColor(C_FG);
    for (uint8_t i = 0; i < n; i++) {
        int16_t y = maxY - (prand(i, 17) + s * speed) % maxY;
        int16_t x = prand(i, 29) % 230 + 5 + (int16_t)(sinf(y * 0.08f + i) * 3);
        uint8_t r = 1 + i % 4;
        g.drawCircle(x, y, r);
        if (r > 2) g.setPixel(x - 1, y - 1);
    }
}

static void fullAquarium(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    uint8_t s = t->tm_sec;

    // Peces: van y vienen, ondulando
    for (uint8_t i = 0; i < 4; i++) {
        bool right = i % 2 == 0;
        int16_t span = 320, pos = (s * 5 + i * 83) % span;
        int16_t x = right ? pos - 40 : 280 - pos;
        static const int16_t ROWS[4] = {34, 92, 190, 280};   // fuera de la franja del texto
        int16_t y = ROWS[i] + (int16_t)(sinf(s * 0.5f + i) * 6);
        drawFish(g, x, y, 10 + (i * 5) % 9, right);
    }
    drawSeaweed(g, 12, 150, s, 0);  drawSeaweed(g, 30, 110, s, 2);
    drawSeaweed(g, 210, 130, s, 4); drawSeaweed(g, 228, 170, s, 1);

    // Gran burbuja con la hora (tapa a los peces que pasan por detrás)
    const int16_t bx = 120, by = 128, br = 74;
    g.setColor(C_BG);
    g.fillCircle(bx, by, br);
    g.setColor(C_FG);
    g.drawCircle(bx, by, br);
    g.drawCircle(bx, by, br - 1);
    for (int16_t a = 200; a < 250; a += 2) {   // brillo
        float r = a * DEG_TO_RAD;
        g.setPixel(bx + cosf(r) * (br - 10), by + sinf(r) * (br - 10));
        g.setPixel(bx + cosf(r) * (br - 11), by + sinf(r) * (br - 11));
    }
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    buildTimeString(buf, t, cfg, false);
    g.drawString(bx, by - 30, buf);
    g.setFont(fontRounded14);
    g.setColor(C_ACC);
    sprintf_P(buf, PSTR("%02d"), s);
    g.drawString(bx, by + 14, String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? " PM" : " AM") : ""));
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_FG);
    g.drawString(bx, by + 36, shortDate(t));

    drawBubbles(g, 18, 320, s, 9);

    // Fondo con piedras
    for (uint8_t i = 0; i < 14; i++) {
        int16_t x = prand(i, 41) % 240, r = 4 + prand(i, 43) % 7;
        g.setColor(i % 3 ? C_SEC : C_FG);
        fillEllipse(g, x, 316, r + 4, r);
    }
    g.setFont(fontRounded14);
    g.setColor(C_FG);
    g.drawString(120, 222, ex.temperature + String(F("  ")) + ex.location);
    g.setFont(ArialMT_Plain_10);
    g.drawString(120, 244, ex.description);
}

static void headerAquarium(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    uint8_t s = t->tm_sec;
    drawBubbles(g, 8, 56, s, 3);
    drawFish(g, 22 + (s % 4), 30, 9, true);
    g.setFont(fontRounded36);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    g.drawString(140, 12, buf);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_ACC);
    g.drawString(48, 1, shortDate(t));
}

// =====================================================
// ESTILO OSCILOSCOPIO
// =====================================================
static void drawScopeGrid(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t divX, uint8_t divY) {
    g.setColor(C_SEC);
    g.drawRect(x, y, w, h);
    for (uint8_t i = 1; i < divX; i++)
        for (int16_t yy = y + 2; yy < y + h; yy += 4) g.setPixel(x + i * w / divX, yy);
    for (uint8_t j = 1; j < divY; j++)
        for (int16_t xx = x + 2; xx < x + w; xx += 4) g.setPixel(xx, y + j * h / divY);
    // Ejes centrales con marcas finas
    int16_t cx = x + w / 2, cy = y + h / 2;
    for (int16_t xx = x; xx < x + w; xx += w / divX / 5) g.drawVerticalLine(xx, cy - 2, 5);
    for (int16_t yy = y; yy < y + h; yy += h / divY / 5) g.drawHorizontalLine(cx - 2, yy, 5);
}

static void drawWave(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t amp, float cycles, float phase) {
    float py = y;
    for (int16_t i = 0; i <= w; i += 2) {
        float ny = y - sinf(i * 2 * PI * cycles / w + phase) * amp;
        g.setColor(C_FG);
        if (i) g.drawLine(x + i - 2, py, x + i, ny);
        g.setColor(C_ACC);   // núcleo brillante del trazo
        g.setPixel(x + i, ny);
        py = ny;
    }
}

static void fullScope(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[32];
    uint8_t s = t->tm_sec;
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_ACC);
    g.drawString(10, 3, F("CH1  DC  2V/div"));
    g.setTextAlignment(TEXT_ALIGN_RIGHT);
    g.drawString(230, 3, s % 2 ? "AUTO  TRIG'D" : "AUTO");

    const int16_t gx = 10, gy = 18, gw = 220, gh = 200;
    drawScopeGrid(g, gx, gy, gw, gh, 10, 8);

    // La hora dibujada con el haz (trazo fino con halo)
    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 120, 40, 38, 70, 4, 7, C_FG, C_NONE, SEG_HEX, true);
    drawSegString(g, buf, 120, 41, 38, 68, 1, 7, C_ACC, C_NONE, SEG_HEX, true);

    // Onda viva: la frecuencia sube con los segundos
    drawWave(g, gx + 2, gy + 160, gw - 4, 22, 1.5f + s / 12.0f, s * 0.6f);

    // Lecturas de medida
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    sprintf_P(buf, PSTR("Freq %2d.%d Hz"), 1 + s / 12, (s % 12) * 10 / 12);
    g.drawString(10, 224, buf);
    sprintf_P(buf, PSTR("Vpp %02d.%02d V"), (cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour), t->tm_min);
    g.drawString(10, 238, buf);
    sprintf_P(buf, PSTR("Seg %02d"), s);
    g.setTextAlignment(TEXT_ALIGN_RIGHT);
    g.drawString(230, 224, buf);
    g.drawString(230, 238, shortDate(t));

    // Mandos que giran
    for (uint8_t i = 0; i < 3; i++) {
        int16_t kx = 34 + i * 40, ky = 280;
        g.setColor(C_SEC);
        g.drawCircle(kx, ky, 13);
        g.drawCircle(kx, ky, 12);
        float a = ((s * (i + 2) * 6) % 360 - 90) * DEG_TO_RAD;
        g.setColor(C_ACC);
        g.drawLine(kx, ky, kx + cosf(a) * 11, ky + sinf(a) * 11);
    }
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_SEC);
    g.drawString(34, 298, F("VOLTS"));
    g.drawString(74, 298, F("TIME"));
    g.drawString(114, 298, F("POS"));
    g.setColor(C_FG);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(144, 262, ex.temperature);
    g.drawStringMaxWidth(144, 276, 92, ex.description);
}

static void headerScope(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawScopeGrid(g, 0, 0, 240, 54, 10, 2);
    drawWave(g, 2, 46, 236, 4, 3, t->tm_sec * 0.6f);
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    drawSegString(g, buf, 120, 6, cfg.isHHMM ? 26 : 20, 32, 3, 5, C_FG, C_NONE, SEG_HEX, true);
}

// =====================================================
// ESTILO NIXIE
// =====================================================
static void drawNixieTube(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h, int8_t digit, uint8_t glowR) {
    // Rejilla (malla del ánodo) y cristal
    g.setColor(C_SEC);
    for (int16_t xx = x + 5; xx < x + w - 4; xx += 3)
        for (int16_t yy = y + 10; yy < y + h - 12; yy += 3) g.setPixel(xx, yy);
    g.drawRect(x + 2, y + 6, w - 4, h - 14);
    g.fillCircle(x + w / 2, y + 6, 6);           // remate superior
    g.setColor(C_BG);
    g.fillRect(x + w / 2 - 6, y + 7, 13, 6);
    g.setColor(C_SEC);
    g.drawHorizontalLine(x + w / 2 - 6, y + 6, 13);
    g.fillRect(x, y + h - 8, w, 8);               // zócalo
    g.setColor(C_BG);
    for (int16_t px = x + 4; px < x + w - 2; px += 6) g.fillRect(px, y + h - 3, 2, 3);
    // Dígito: halo naranja + filamento brillante
    int16_t dw = (w - 16), dh = (h - 30);
    g.setColor(C_FG);
    drawVectorDigit(g, digit, x + 8, y + 14, dw, dh, glowR);
    g.setColor(C_ACC);
    drawVectorDigit(g, digit, x + 8, y + 14, dw, dh, glowR > 2 ? 1 : 0);
    g.setColor(C_SEC);   // reflejo en el cristal
    g.drawVerticalLine(x + 5, y + 14, h / 3);
}

static void fullNixie(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char buf[24];
    buildTimeString(buf, t, cfg, true);   // "HH:MM:SS"
    const int16_t tw = 50, th = 150, y = 60;
    int16_t xs[4] = {6, 60, 126, 180};
    uint8_t idx[4] = {0, 1, 3, 4};
    for (uint8_t i = 0; i < 4; i++) drawNixieTube(g, xs[i], y, tw, th, digitAt(buf, idx[i]), 3);
    // Lámparas de neón de los dos puntos
    g.setColor(t->tm_sec % 2 ? C_SEC : C_FG);
    g.fillCircle(120, y + 52, 4);
    g.fillCircle(120, y + 96, 4);

    // Segundos en tubos pequeños
    drawNixieTube(g, 86, 222, 32, 70, digitAt(buf, 6), 2);
    drawNixieTube(g, 122, 222, 32, 70, digitAt(buf, 7), 2);

    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    g.drawString(120, 20, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""));
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_SEC);
    g.drawString(120, 300, ex.temperature + String(F("  ")) + ex.description);
}

static void headerNixie(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    buildTimeString(buf, t, cfg, !cfg.isHHMM);
    uint8_t n = strlen(buf);
    int16_t dw = 20, gap = 6, cw = 10, total = 0;
    for (uint8_t i = 0; i < n; i++) total += (buf[i] == ':' ? cw : dw) + gap;
    int16_t x = 120 - total / 2;
    for (uint8_t i = 0; i < n; i++) {
        if (buf[i] == ':') {
            g.setColor(C_FG);
            g.fillCircle(x + cw / 2, 22, 2); g.fillCircle(x + cw / 2, 40, 2);
            x += cw + gap;
            continue;
        }
        g.setColor(C_SEC);
        g.drawRect(x - 3, 6, dw + 6, 48);
        g.setColor(C_FG);  drawVectorDigit(g, digitAt(buf, i), x, 12, dw, 38, 2);
        g.setColor(C_ACC); drawVectorDigit(g, digitAt(buf, i), x, 12, dw, 38, 0);
        x += dw + gap;
    }
}

// =====================================================
// ESTILO FLIP CLOCK (tarjetas abatibles)
// =====================================================
// Tarjeta con dos dígitos. Si "oldDigits" no es nulo, se dibuja la tarjeta
// a medio abatir: arriba los dígitos nuevos y abajo los anteriores.
static void drawFlipCard(MiniGrafx &g, int16_t x, int16_t y, int16_t w, int16_t h,
                         const int8_t digits[2], const int8_t *oldDigits, uint8_t stroke) {
    int16_t mid = y + h / 2;
    g.setColor(C_SEC);
    fillRoundRect(g, x + 2, y + 3, w, h, 8);          // sombra
    g.setColor(C_ACC);
    fillRoundRect(g, x, y, w, h, 8);
    int16_t dw = (w - 24) / 2, dh = h - 24;
    for (uint8_t half = 0; half < 2; half++) {
        const int8_t *d = (half == 1 && oldDigits) ? oldDigits : digits;
        int16_t yMin = half == 0 ? y : mid + 1, yMax = half == 0 ? mid - 1 : y + h;
        g.setColor(C_FG);
        drawVectorDigit(g, d[0], x + 8, y + 12, dw, dh, stroke, yMin, yMax);
        drawVectorDigit(g, d[1], x + 16 + dw, y + 12, dw, dh, stroke, yMin, yMax);
    }
    // Línea de corte y bisagras
    g.setColor(C_SEC);
    g.fillRect(x, mid - 1, w, 3);
    g.setColor(C_BG);
    g.fillRect(x - 1, mid - 4, 4, 9);
    g.fillRect(x + w - 3, mid - 4, 4, 9);
    if (oldDigits) {   // brillo en el borde de la hoja que cae
        g.setColor(C_SEC);
        g.drawHorizontalLine(x + 6, mid + 4, w - 12);
    }
}

static void splitDigits(const char *s, uint8_t i, int8_t out[2]) {
    out[0] = digitAt(s, i);
    out[1] = digitAt(s, i + 1);
}

static void fullFlip(MiniGrafx &g, const struct tm *t, const AppConfig &cfg, const ClockExtras &ex) {
    char now[24], prev[24];
    buildTimeString(now, t, cfg, true);
    struct tm cur = *t, tp;              // copia: localtime() reutiliza un buffer estático
    time_t before = mktime(&cur) - 1;
    localtime_r(&before, &tp);
    buildTimeString(prev, &tp, cfg, true);

    int8_t hh[2], mm[2], ss[2], ohh[2], omm[2];
    splitDigits(now, 0, hh); splitDigits(now, 3, mm); splitDigits(now, 6, ss);
    splitDigits(prev, 0, ohh); splitDigits(prev, 3, omm);
    // Durante el primer segundo de cada cambio, la tarjeta se ve a medio abatir
    bool flipH = hh[0] != ohh[0] || hh[1] != ohh[1];
    bool flipM = mm[0] != omm[0] || mm[1] != omm[1];

    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    g.drawString(120, 24, shortDate(t) + String(F(" ")) + String(1900 + t->tm_year));

    drawFlipCard(g, 8, 60, 108, 136, hh, flipH ? ohh : nullptr, 5);
    drawFlipCard(g, 124, 60, 108, 136, mm, flipM ? omm : nullptr, 5);
    if (cfg.is12hStyle) {
        g.setFont(ArialMT_Plain_10);
        g.setTextAlignment(TEXT_ALIGN_LEFT);
        g.setColor(C_FG);
        g.drawString(16, 182, t->tm_hour >= 12 ? "PM" : "AM");
    }
    drawFlipCard(g, 84, 210, 72, 56, ss, nullptr, 2);

    g.setFont(fontRounded14);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    g.drawString(120, 276, ex.temperature + String(F("  ")) + ex.location);
    g.setFont(ArialMT_Plain_10);
    g.drawString(120, 298, ex.description);
}

static void headerFlip(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    buildTimeString(buf, t, cfg, true);
    int8_t hh[2], mm[2];
    splitDigits(buf, 0, hh); splitDigits(buf, 3, mm);
    drawFlipCard(g, 48, 2, 66, 52, hh, nullptr, 2);
    drawFlipCard(g, 122, 2, 66, 52, mm, nullptr, 2);
    if (!cfg.isHHMM) {
        g.setFont(fontRounded14);
        g.setTextAlignment(TEXT_ALIGN_CENTER);
        g.setColor(C_FG);
        g.drawString(212, 34, String(buf + 6));
    }
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.setColor(C_FG);
    g.drawString(4, 4, WDAY_NAMES[t->tm_wday]);
    g.drawString(4, 18, String(t->tm_mday));
    if (cfg.is12hStyle) g.drawString(4, 32, t->tm_hour >= 12 ? "PM" : "AM");
}

// =====================================================
// API PUBLICA
// =====================================================
void clockDrawHeader(MiniGrafx &gfx, const struct tm *t, const AppConfig &cfg) {
    gfx.setTransparentColor(0xFFFF);   // permitir dibujar con el color de fondo
    switch (cfg.clockStyle) {
        case CLOCK_RETRO:    headerRetro(gfx, t, cfg); break;
        case CLOCK_ANIME:    headerAnime(gfx, t, cfg); break;
        case CLOCK_ARCADE:   headerPixel(gfx, t, cfg, false); break;
        case CLOCK_HANDHELD: headerPixel(gfx, t, cfg, true); break;
        case CLOCK_FUTURE:   headerFuture(gfx, t, cfg); break;
        case CLOCK_ANALOG:   headerAnalog(gfx, t, cfg); break;
        case CLOCK_MONO:     headerMono(gfx, t, cfg); break;
        case CLOCK_DESKTOP:  headerDesktop(gfx, t, cfg); break;
        case CLOCK_TERMINAL: headerTerminal(gfx, t, cfg); break;
        case CLOCK_MICRO:    headerMicro(gfx, t, cfg); break;
        case CLOCK_ADVENTURE:headerAdventure(gfx, t, cfg); break;
        case CLOCK_SPEED:    headerSpeed(gfx, t, cfg); break;
        case CLOCK_PLATFORM: headerPlatform(gfx, t, cfg); break;
        case CLOCK_WEATHER:  headerWeather(gfx, t, cfg); break;
        case CLOCK_NIGHT:    headerNight(gfx, t, cfg); break;
        case CLOCK_AQUARIUM: headerAquarium(gfx, t, cfg); break;
        case CLOCK_SCOPE:    headerScope(gfx, t, cfg); break;
        case CLOCK_NIXIE:    headerNixie(gfx, t, cfg); break;
        case CLOCK_FLIP:     headerFlip(gfx, t, cfg); break;
        default:             headerClassic(gfx, t, cfg); break;
    }
    gfx.setTransparentColor(C_BG);   // los iconos del tiempo usan el fondo como transparente
}

void clockDrawFull(MiniGrafx &gfx, const struct tm *t, const AppConfig &cfg, const ClockExtras &extras) {
    gfx.setTransparentColor(0xFFFF);
    switch (cfg.clockStyle) {
        case CLOCK_RETRO:    fullRetro(gfx, t, cfg, extras); break;
        case CLOCK_ANIME:    fullAnime(gfx, t, cfg, extras); break;
        case CLOCK_ARCADE:   fullPixel(gfx, t, cfg, extras, false); break;
        case CLOCK_HANDHELD: fullPixel(gfx, t, cfg, extras, true); break;
        case CLOCK_FUTURE:   fullFuture(gfx, t, cfg, extras); break;
        case CLOCK_ANALOG:   fullAnalog(gfx, t, cfg, extras); break;
        case CLOCK_MONO:     fullMono(gfx, t, cfg, extras); break;
        case CLOCK_DESKTOP:  fullDesktop(gfx, t, cfg, extras); break;
        case CLOCK_TERMINAL: fullTerminal(gfx, t, cfg, extras); break;
        case CLOCK_MICRO:    fullMicro(gfx, t, cfg, extras); break;
        case CLOCK_ADVENTURE:fullAdventure(gfx, t, cfg, extras); break;
        case CLOCK_SPEED:    fullSpeed(gfx, t, cfg, extras); break;
        case CLOCK_PLATFORM: fullPlatform(gfx, t, cfg, extras); break;
        case CLOCK_WEATHER:  fullWeather(gfx, t, cfg, extras); break;
        case CLOCK_NIGHT:    fullNight(gfx, t, cfg, extras); break;
        case CLOCK_AQUARIUM: fullAquarium(gfx, t, cfg, extras); break;
        case CLOCK_SCOPE:    fullScope(gfx, t, cfg, extras); break;
        case CLOCK_NIXIE:    fullNixie(gfx, t, cfg, extras); break;
        case CLOCK_FLIP:     fullFlip(gfx, t, cfg, extras); break;
        default:             fullClassic(gfx, t, cfg, extras); break;
    }
    gfx.setTransparentColor(C_BG);   // los iconos del tiempo usan el fondo como transparente
}
