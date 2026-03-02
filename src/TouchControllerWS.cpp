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
    TS_Point p = touchScreen->getPoint();

    if (state == 0) {
        (*calibrationCallback)(10, 10);
        if (touchScreen->touched()) {
            p1 = p;
            state++;
            lastStateChange = millis();
        }
    } else if (state == 1) {
        (*calibrationCallback)(230, 310);
        if (touchScreen->touched() && (millis() - lastStateChange > 1000)) {
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

bool TouchControllerWS::isTouched() {
    return touchScreen->touched();
}

bool TouchControllerWS::isTouched(int16_t debounceMillis) {
    if (touchScreen->touched() && millis() - lastTouched > debounceMillis) {
        lastTouched = millis();
        return true;
    }
    return false;
}

TS_Point TouchControllerWS::getPoint() {
    TS_Point p = touchScreen->getPoint();
    int x = 240 - (p.y - ax) * dx;
    int y = (p.x - ay) * dy;
    //int x = (p.y - ax) * dx; tactil invertido
    //int y = 320 - (p.x - ay) * dy;
    p.x = x;
    p.y = y;
    return p;
}
