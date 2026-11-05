#ifndef _TOUCH_CONTROLLERWSH_
#define _TOUCH_CONTROLLERWSH_

#include <LittleFS.h>
#include <XPT2046_Touchscreen.h>

typedef void (*CalibrationCallback)(int16_t x, int16_t y);

class TouchControllerWS {
public:
    TouchControllerWS(XPT2046_Touchscreen *touchScreen);
    bool loadCalibration();
    bool saveCalibration();
    void startCalibration(CalibrationCallback *callback);
    void continueCalibration();
    bool isCalibrationFinished();
    // true si hay una pulsación válida ahora mismo (lecturas filtradas)
    bool isTouched();
    // true solo al empezar una pulsación nueva: hay que soltar antes de que
    // vuelva a contar, y además deben pasar debounceMillis desde la anterior
    bool isTouched(int16_t debounceMillis);
    // Punto calibrado de la última pulsación válida
    TS_Point getPoint();

private:
    XPT2046_Touchscreen *touchScreen;
    float dx = 0.0;
    float dy = 0.0;
    int ax = 0;
    int ay = 0;
    int state = 0;
    long lastStateChange = 0;
    long lastTouched = 0;
    CalibrationCallback *calibrationCallback;
    TS_Point p1, p2;

    // Filtro anti-toques fantasma (ruido eléctrico, p.ej. PWM de la retroiluminación)
    bool readStable(TS_Point &raw);
    TS_Point lastRaw;
    bool hasPoint = false;
    bool released = true;
    unsigned long lastSeenTouch = 0;
};

#endif
