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
};

static const char *STYLE_NAMES[CLOCK_STYLE_COUNT] = {
    "Clasico", "Retro", "Anime", "Arcade", "Portatil", "Futurista",
    "Analogico", "1-bit", "Escritorio", "Terminal", "Micro 8bit", "Aventura", "Velocidad", "Plataformas"
};

const char *clockStyleName(uint8_t style) {
    return STYLE_NAMES[style < CLOCK_STYLE_COUNT ? style : 0];
}

void clockApplyPalette(uint8_t style, uint16_t *palette) {
    if (style >= CLOCK_STYLE_COUNT) style = CLOCK_CLASSIC;
    for (uint8_t i = 0; i < 4; i++) palette[i] = PALETTES[style][i];
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
        sprintf(out, "%c%d:%02d:%02d", h0, h % 10, t->tm_min, t->tm_sec);
    } else {
        sprintf(out, "%c%d:%02d", h0, h % 10, t->tm_min);
    }
}

static String shortDate(const struct tm *t) {
    char buf[8];
    sprintf(buf, "%02d", t->tm_mday % 100);
    return WDAY_NAMES[t->tm_wday] + " " + String(buf) + " " + MONTH_NAMES[t->tm_mon];
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
    g.drawString(120, 6, WDAY_NAMES[t->tm_wday] + " " + MONTH_NAMES[t->tm_mon] + " " +
                 String(t->tm_mday) + " " + String(1900 + t->tm_year));
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
    g.drawString(120, 70, shortDate(t) + " " + String(1900 + t->tm_year));
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
    g.drawString(6, 2, shortDate(t) + " " + String(1900 + t->tm_year));
    if (cfg.is12hStyle) {
        g.setColor(t->tm_hour < 12 ? C_ACC : C_SEC);  g.drawString(150, 2, "AM");
        g.setColor(t->tm_hour >= 12 ? C_ACC : C_SEC); g.drawString(170, 2, "PM");
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
        g.setColor(t->tm_hour < 12 ? C_FG : C_SEC);  g.drawString(26, 62, "AM");
        g.setColor(t->tm_hour >= 12 ? C_FG : C_SEC); g.drawString(54, 62, "PM");
    } else {
        g.setColor(C_FG); g.drawString(30, 62, "24H");
    }

    buildTimeString(buf, t, cfg, false);
    drawSegString(g, buf, 120, 78, 40, 76, 7, 6, C_FG, C_SEC, SEG_BLOCK, t->tm_sec % 2 == 0);

    // Segundos en pequeno, abajo a la derecha del panel
    sprintf(buf, "%02d", t->tm_sec);
    drawSegString(g, buf, 196, 160, 12, 20, 3, 3, C_ACC, C_NONE, SEG_BLOCK, true);

    // Fecha en 7 segmentos: DD-MM
    sprintf(buf, "%02d-%02d", t->tm_mday, t->tm_mon + 1);
    drawSegString(g, buf, 120, 200, 20, 36, 4, 4, C_ACC, C_NONE, SEG_BLOCK, true);

    g.setFont(ArialMT_Plain_16);
    g.setColor(C_FG);
    g.drawString(120, 252, ex.temperature + "  " + ex.location);
    g.setColor(C_ACC);
    g.drawString(120, 276, ex.description);
    g.setFont(ArialMT_Plain_10);
    g.setColor(C_SEC);
    g.drawString(120, 302, "-- MODO LCD --");
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
    g.drawString(120, 30, shortDate(t) + " " + String(1900 + t->tm_year));

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
    sprintf(buf, "%02d", s);
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
    g.drawString(120, 262, ex.temperature + " - " + ex.description);
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
        drawPixelText(g, "PILA", 22, 12, 1, C_ACC);
    }

    // Fila superior: nivel (dia del ano) y vidas
    sprintf(buf, "NIVEL %d", t->tm_yday + 1);
    drawPixelText(g, buf, handheld ? 48 : 10, 10, 2, C_ACC);
    for (uint8_t i = 0; i < 3; i++) drawSprite8(g, SPRITE_HEART, 176 + i * 18, 8, 2, handheld ? C_FG : C_SEC);

    // Fecha
    drawPixelText(g, shortDate(t) + " " + String(1900 + t->tm_year), 120, 40, 3, C_FG, 1);

    // Hora gigante con sombra
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 76, 12, C_FG, 1, C_SEC);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 222, 146, 3, C_ACC, 2);

    // Barra de "experiencia": 20 bloques, uno cada 3 segundos
    sprintf(buf, "SEG %02d", s);
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
    sprintf(buf, "%02d.%02d.%04d", t->tm_mday, t->tm_mon + 1, 1900 + t->tm_year);
    g.drawString(6, 1, "// " + WDAY_NAMES[t->tm_wday] + " " + String(buf));
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
    g.drawString(10, 8, "SYS.TIME // NTP OK");
    g.setTextAlignment(TEXT_ALIGN_RIGHT);
    sprintf(buf, "D%03d", t->tm_yday + 1);
    g.drawString(230, 8, buf);

    g.setFont(ArialMT_Plain_16);
    g.setTextAlignment(TEXT_ALIGN_CENTER);
    g.setColor(C_FG);
    sprintf(buf, "%02d.%02d.%04d", t->tm_mday, t->tm_mon + 1, 1900 + t->tm_year);
    g.drawString(cx, 28, WDAY_NAMES[t->tm_wday] + "  " + String(buf));

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

    sprintf(buf, "%02d", s);
    drawSegString(g, buf, cx, cy + 34, 12, 20, 3, 3, C_ACC, C_NONE, SEG_HEX, true);

    // Paneles de datos
    g.setColor(C_SEC);
    drawCornerBrackets(g, 8, 258, 108, 48, 6);
    drawCornerBrackets(g, 124, 258, 108, 48, 6);
    g.setFont(ArialMT_Plain_10);
    g.setTextAlignment(TEXT_ALIGN_LEFT);
    g.drawString(14, 262, "TEMP");
    g.drawString(130, 262, "LOC");
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
    sprintf(buf, "UP %luD %02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m);
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
    g.drawString(cx, 6, shortDate(t) + " " + String(1900 + t->tm_year));

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
    g.drawString(cx, cy + 34, "TIMEDESK");

    drawDial(g, cx, cy, r, t, true);

    g.setFont(fontRounded14);
    g.setColor(C_FG);
    buildTimeString(buf, t, cfg, true);
    g.drawString(cx, 262, String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? " PM" : " AM") : ""));
    g.setColor(C_SEC);
    g.drawString(cx, 286, ex.temperature + "  " + ex.description);
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
    String title = "TimeDesk - " + shortDate(t);
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
    g.drawString(24, 3, "Archivo  Edicion  Ver  Hora");
    sprintf(buf, "%02d:%02d", (cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour), t->tm_min);
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
    g.drawString(119, 156, shortDate(t) + " " + String(1900 + t->tm_year));

    // Ventana del tiempo
    drawMonoWindow(g, 34, 218, 184, 84, "Tiempo");
    g.setColor(C_FG);
    g.setFont(ArialMT_Plain_16);
    g.drawString(126, 242, ex.temperature + "  " + ex.location);
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
    g.drawString(7, 3, "Reloj - " + shortDate(t));
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
    g.drawString(120, wy + 104, shortDate(t) + " " + String(1900 + t->tm_year) +
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
    g.drawString(14, 246, ex.temperature + "  " + ex.location);
    g.drawStringMaxWidth(14, 260, 210, ex.description);

    // Barra de tareas con boton de menu y bandeja con la hora
    g.setColor(C_SEC);
    g.fillRect(0, 294, 240, 26);
    g.setColor(C_FG);
    g.drawHorizontalLine(0, 295, 240);
    drawBevel(g, 3, 298, 62, 20, C_FG, C_ACC);
    g.setColor(C_ACC);
    g.drawCircle(14, 308, 5); g.drawLine(14, 308, 14, 305); g.drawLine(14, 308, 16, 309);
    g.drawString(24, 302, "Menu");
    g.drawString(25, 302, "Menu");   // negrita simulada
    drawBevel(g, 176, 298, 61, 20, C_FG, C_ACC, true, 1);
    sprintf(buf, "%02d:%02d", (cfg.is12hStyle ? (t->tm_hour + 11) % 12 + 1 : t->tm_hour), t->tm_min);
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
    drawPixelText(g, WDAY_NAMES[t->tm_wday] + " " + String(buf), 6, 20, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 40, "clock --big");
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 58, 10, C_FG, 1);
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 234, 98, 2, C_ACC, 2);
    String bar = "[";
    for (uint8_t i = 0; i < 20; i++) bar += (i * 3 <= t->tm_sec) ? '#' : '.';
    sprintf(buf, "] %02dS", t->tm_sec);
    drawPixelText(g, bar + buf, 6, 116, 2, C_FG);

    termPrompt(g, 140, "uptime");
    drawPixelText(g, uptimeText(ex.uptimeSec) + "  MEM " + String(ex.freeHeap) + "B", 6, 154, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 174, "weather");
    drawPixelText(g, ex.temperature + " " + ex.location, 6, 188, 2, C_FG, 0, C_NONE, 228);
    drawPixelText(g, ex.description, 6, 202, 2, C_FG, 0, C_NONE, 228);

    termPrompt(g, 222, "uname -a");
    drawPixelText(g, "TIMEDESK ESP8266 XTENSA", 6, 236, 2, C_FG, 0, C_NONE, 228);

    int16_t cx = termPrompt(g, 256, "");
    if (t->tm_sec % 2 == 0) { g.setColor(C_FG); g.fillRect(cx, 255, 8, 12); }
}

static void headerTerminal(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    drawPixelText(g, "~$ DATE  " + shortDate(t), 4, 3, 2, C_ACC);
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

    drawPixelText(g, "** TIMEDESK BASIC **", 120, 28, 2, C_FG, 1);
    drawPixelText(g, "RAM LIBRE " + String(ex.freeHeap) + " BYTES", 120, 42, 2, C_FG, 1);
    drawPixelText(g, "LISTO.", 16, 62, 2, C_FG);
    drawPixelText(g, "PRINT HORA$", 16, 76, 2, C_FG);
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 120, 94, 9, C_ACC, 1);
    sprintf(buf, "SEG %02d", t->tm_sec);
    drawPixelText(g, String(buf) + (cfg.is12hStyle ? (t->tm_hour >= 12 ? "  PM" : "  AM") : ""), 16, 146, 2, C_SEC);
    drawPixelText(g, "LISTO.", 16, 166, 2, C_FG);
    drawPixelText(g, "PRINT TIEMPO$", 16, 180, 2, C_FG);
    drawPixelText(g, ex.temperature + " " + ex.location, 16, 196, 2, C_ACC, 0, C_NONE, 208);
    drawPixelText(g, ex.description, 16, 210, 2, C_ACC, 0, C_NONE, 208);
    drawPixelText(g, shortDate(t) + " " + String(1900 + t->tm_year), 16, 224, 2, C_ACC);
    drawPixelText(g, "LISTO.", 16, 246, 2, C_FG);
    if (t->tm_sec % 2 == 0) { g.setColor(C_FG); g.fillRect(16, 260, 8, 10); }
}

static void headerMicro(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    g.setColor(C_FG);
    g.fillRect(0, 0, 6, 56);
    drawPixelText(g, "LISTO.  " + shortDate(t), 12, 3, 2, C_FG);
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
    drawPixelText(g, "-VIDA-", 10, 6, 2, C_SEC);
    drawHearts(g, 10, 20, s / 6 + 1, 10, 1);
    g.setColor(C_ACC);
    g.fillCircle(176, 24, 5);
    g.setColor(C_BG);
    g.drawVerticalLine(176, 21, 7);
    drawPixelText(g, "ORO", 188, 6, 2, C_ACC);
    sprintf(buf, "X%03d", t->tm_yday + 1);
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
    sprintf(buf, "%02d", s);
    drawPixelText(g, buf, 120, 190, 2, C_FG, 1);

    // Cuadro de dialogo
    drawDialogBox(g, 8, 210, 224, 98);
    buildTimeString(buf, t, cfg, false);
    String hora = String(buf);
    hora.trim();
    drawPixelText(g, "SON LAS " + hora + (cfg.is12hStyle ? (t->tm_hour >= 12 ? " PM." : " AM.") : "."), 20, 224, 2, C_FG);
    drawPixelText(g, shortDate(t) + " " + String(1900 + t->tm_year), 20, 240, 2, C_FG);
    drawPixelText(g, "HACE " + ex.temperature + " EN", 20, 256, 2, C_FG, 0, C_NONE, 200);
    drawPixelText(g, ex.location + ": " + ex.description, 20, 272, 2, C_FG, 0, C_NONE, 200);
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
    drawPixelText(g, "TIEMPO", 8, 6, 2, C_ACC);
    sprintf(buf, "VUELTA %02d/24", t->tm_hour + 1);
    drawPixelText(g, buf, 232, 6, 2, C_ACC, 2);
    buildTimeString(buf, t, cfg, false);
    drawPixelText(g, buf, 114, 24, 10, C_FG, 1, C_SEC, 240, 2);   // cursiva
    if (cfg.is12hStyle) drawPixelText(g, t->tm_hour >= 12 ? "PM" : "AM", 232, 66, 2, C_ACC, 2);
    drawPixelText(g, ex.temperature + "  " + ex.description, 120, 88, 2, C_ACC, 1, C_NONE, 228);

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
    sprintf(buf, "%d", 180 + s * 2);
    drawPixelText(g, buf, 8, 284, 4, C_ACC, 0, C_NONE, 240, 1);
    drawPixelText(g, "KM/H", 8, 306, 2, C_FG);
    drawPixelText(g, shortDate(t), 232, 306, 2, C_FG, 2);
}

static void headerSpeed(MiniGrafx &g, const struct tm *t, const AppConfig &cfg) {
    char buf[24];
    for (uint8_t i = 0; i < 5; i++) {   // lineas de velocidad
        g.setColor(i % 2 ? C_SEC : C_ACC);
        g.drawHorizontalLine(2 + i * 3, 18 + i * 7, 22 - i * 3);
    }
    drawPixelText(g, "TIEMPO  " + shortDate(t), 30, 3, 2, C_ACC);
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

    drawPixelText(g, ex.temperature + " " + ex.location, 8, 6, 2, C_FG, 0, C_NONE, 140);
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
    sprintf(buf, "%02d", s);
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
        default:             fullClassic(gfx, t, cfg, extras); break;
    }
    gfx.setTransparentColor(C_BG);   // los iconos del tiempo usan el fondo como transparente
}
