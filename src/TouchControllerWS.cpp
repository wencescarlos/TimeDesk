#include "TouchControllerWS.h"

TouchControllerWS::TouchControllerWS(XPT2046_Touchscreen *touchScreen) {
    this->touchScreen = touchScreen;
}

bool TouchControllerWS::loadCalibration() {
    File f = LittleFS.open("/calibration.txt", "r");
    if (!f) {
        Serial.println(F("[Touch] No calibration file found"));
        return false;
    }

    String dxStr = f.readStringUntil('\n');
    String dyStr = f.readStringUntil('\n');
    String axStr = f.readStringUntil('\n');
    String ayStr = f.readStringUntil('\n');

    dx = dxStr.toFloat();
    dy = dyStr.toFloat();
    ax = axStr.toInt();
    ay = ayStr.toInt();

    f.close();
    Serial.println(F("[Touch] Calibration loaded"));
    return true;
}

bool TouchControllerWS::saveCalibration() {
    File f = LittleFS.open("/calibration.txt", "w");
    if (!f) {
        Serial.println(F("[Touch] Failed to save calibration"));
        return false;
    }
    f.println(dx);
    f.println(dy);
    f.println(ax);
    f.println(ay);
    f.close();
    Serial.println(F("[Touch] Calibration saved"));
    return true;
}

void TouchControllerWS::startCalibration(CalibrationCallback *calibrationCallback) {
    state = 0;
    this->calibrationCallback = calibrationCallback;
}

void TouchControllerWS::continueCalibration() {
    TS_Point p;
    bool touched = readStable(p);

    if (state == 0) {
        (*calibrationCallback)(10, 10);
        if (touched) {
            p1 = p;
            state++;
            lastStateChange = millis();
        }
    } else if (state == 1) {
        (*calibrationCallback)(230, 310);
        if (touched && (millis() - lastStateChange > 1000)) {
            p2 = p;
            state++;
            lastStateChange = millis();
            dx = 240.0 / abs(p1.y - p2.y);
            dy = 320.0 / abs(p1.x - p2.x);
            ax = p1.y < p2.y ? p1.y : p2.y;
            ay = p1.x < p2.x ? p1.x : p2.x;
        }
    }
}

bool TouchControllerWS::isCalibrationFinished() {
    return state == 2;
}

// Una pulsación solo es válida si varias lecturas seguidas tienen presión
// suficiente y coinciden en posición. El ruido eléctrico suele superar el
// umbral de presión de la librería (300) en una lectura suelta, pero da
// posiciones dispersas.
static const uint8_t  TOUCH_SAMPLES   = 4;
static const int16_t  TOUCH_Z_MIN     = 450;   // presión mínima (la librería usa 300)
static const int16_t  TOUCH_MAX_SPREAD = 120;  // dispersión máxima entre lecturas (0-4095)
static const uint16_t TOUCH_RELEASE_MS = 80;   // tiempo sin tocar para considerar "soltado"

bool TouchControllerWS::readStable(TS_Point &raw) {
    int32_t sx = 0, sy = 0, sz = 0;
    int16_t minX = 4095, maxX = 0, minY = 4095, maxY = 0;
    for (uint8_t i = 0; i < TOUCH_SAMPLES; i++) {
        if (i) delay(4);                       // la librería no relee antes de 3 ms
        if (!touchScreen->touched()) return false;
        TS_Point p = touchScreen->getPoint();
        if (p.z < TOUCH_Z_MIN) return false;
        sx += p.x; sy += p.y; sz += p.z;
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }
    if (maxX - minX > TOUCH_MAX_SPREAD || maxY - minY > TOUCH_MAX_SPREAD) return false;
    raw = TS_Point(sx / TOUCH_SAMPLES, sy / TOUCH_SAMPLES, sz / TOUCH_SAMPLES);
    return true;
}

bool TouchControllerWS::isTouched() {
    TS_Point raw;
    if (!readStable(raw)) return false;
    lastRaw = raw;
    hasPoint = true;
    lastSeenTouch = millis();
    return true;
}

bool TouchControllerWS::isTouched(int16_t debounceMillis) {
    unsigned long now = millis();
    if (isTouched()) {
        bool isNewPress = released && (lastTouched == 0 || now - lastTouched > (unsigned long)debounceMillis);
        released = false;
        if (isNewPress) lastTouched = now;
        return isNewPress;
    }
    if (now - lastSeenTouch > TOUCH_RELEASE_MS) released = true;
    return false;
}

TS_Point TouchControllerWS::getPoint() {
    TS_Point p;
    if (hasPoint) {
        p = lastRaw;
        hasPoint = false;
    } else if (!readStable(p)) {
        return TS_Point(-1, -1, 0);            // sin pulsación válida: fuera de cualquier zona
    }
    int x = 240 - (p.y - ax) * dx;
    int y = (p.x - ay) * dy;
    return TS_Point(x, y, p.z);
}
