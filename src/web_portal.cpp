#include "web_portal.h"

WebPortal::WebPortal(ConfigManager *cfgMgr) 
    : server(WEB_SERVER_PORT), configMgr(cfgMgr), apMode(false), restartRequested(false) {}

void WebPortal::beginAP() {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    delay(500);
    
    // DNS captive portal - redirige todo al ESP
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    Serial.printf("[WebPortal] AP Mode - SSID: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    setupRoutes();
    server.begin();
}

void WebPortal::beginSTA() {
    apMode = false;
    setupRoutes();
    server.begin();
    Serial.printf("[WebPortal] STA server on http://%s/\n", WiFi.localIP().toString().c_str());
}

void WebPortal::stop() {
    server.stop();
    if (apMode) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
    }
}

void WebPortal::handleClient() {
    if (apMode) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

bool WebPortal::isAPMode() { return apMode; }
bool WebPortal::shouldRestart() { return restartRequested; }
String WebPortal::getLocalIP() { return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

void WebPortal::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/status", HTTP_GET, [this]() { handleStatus(); });
    server.on("/schedule", HTTP_GET, [this]() { handleSchedule(); });
    server.on("/schedule-save", HTTP_POST, [this]() { handleScheduleSave(); });
    server.on("/scan", HTTP_GET, [this]() { handleScanWifi(); });
    server.on("/reset", HTTP_POST, [this]() { handleReset(); });
    server.onNotFound([this]() { handleNotFound(); });
}

void WebPortal::handleRoot() {
    server.send(200, "text/html", generateHTML());
}

void WebPortal::handleSave() {
    if (server.hasArg("ssid")) strlcpy(configMgr->config.wifiSSID, server.arg("ssid").c_str(), sizeof(configMgr->config.wifiSSID));
    if (server.hasArg("pass")) strlcpy(configMgr->config.wifiPassword, server.arg("pass").c_str(), sizeof(configMgr->config.wifiPassword));
    if (server.hasArg("api")) strlcpy(configMgr->config.owmApiKey, server.arg("api").c_str(), sizeof(configMgr->config.owmApiKey));
    if (server.hasArg("locid")) strlcpy(configMgr->config.owmLocationId, server.arg("locid").c_str(), sizeof(configMgr->config.owmLocationId));
    if (server.hasArg("locname")) strlcpy(configMgr->config.locationName, server.arg("locname").c_str(), sizeof(configMgr->config.locationName));
    if (server.hasArg("tz")) strlcpy(configMgr->config.timezone, server.arg("tz").c_str(), sizeof(configMgr->config.timezone));
    if (server.hasArg("lang")) configMgr->config.owmLanguage = server.arg("lang");
    
    configMgr->config.isMetric = server.hasArg("metric");
    configMgr->config.is12hStyle = server.hasArg("12h");
    configMgr->config.isHHMM = server.hasArg("hhmm");
    
    if (server.hasArg("update_min")) configMgr->config.updateIntervalMin = server.arg("update_min").toInt();
    if (server.hasArg("brightness")) configMgr->config.brightness = server.arg("brightness").toInt();
    
    // Screen schedule from main form
    if (server.hasArg("scr_on_h")) configMgr->config.screenOnHour = server.arg("scr_on_h").toInt();
    if (server.hasArg("scr_on_m")) configMgr->config.screenOnMinute = server.arg("scr_on_m").toInt();
    if (server.hasArg("scr_off_h")) configMgr->config.screenOffHour = server.arg("scr_off_h").toInt();
    if (server.hasArg("scr_off_m")) configMgr->config.screenOffMinute = server.arg("scr_off_m").toInt();
    configMgr->config.screenSleepEnabled = server.hasArg("scr_sleep");
    
    configMgr->save();
    
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px}"
        ".ok{background:#16213e;border:2px solid #0f3;border-radius:12px;padding:30px;max-width:400px;margin:auto}"
        "a{color:#4cc9f0;font-size:1.1em}</style></head><body>"
        "<div class='ok'><h2>✅ Configuración guardada</h2>"
        "<p>El dispositivo se reiniciará en 3 segundos...</p>"
        "<a href='/'>Volver</a></div></body></html>");
    server.send(200, "text/html", html);
    restartRequested = true;
}

void WebPortal::handleStatus() {
    String json = "{";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"vcc\":" + String(ESP.getVcc() / 1024.0) + ",";
    json += "\"scr_on\":\"" + String(configMgr->config.screenOnHour) + ":" + (configMgr->config.screenOnMinute < 10 ? "0" : "") + String(configMgr->config.screenOnMinute) + "\",";
    json += "\"scr_off\":\"" + String(configMgr->config.screenOffHour) + ":" + (configMgr->config.screenOffMinute < 10 ? "0" : "") + String(configMgr->config.screenOffMinute) + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void WebPortal::handleSchedule() {
    server.send(200, "text/html", generateScheduleHTML());
}

void WebPortal::handleScheduleSave() {
    if (server.hasArg("scr_on_h")) configMgr->config.screenOnHour = server.arg("scr_on_h").toInt();
    if (server.hasArg("scr_on_m")) configMgr->config.screenOnMinute = server.arg("scr_on_m").toInt();
    if (server.hasArg("scr_off_h")) configMgr->config.screenOffHour = server.arg("scr_off_h").toInt();
    if (server.hasArg("scr_off_m")) configMgr->config.screenOffMinute = server.arg("scr_off_m").toInt();
    configMgr->config.screenSleepEnabled = server.hasArg("scr_sleep");
    
    configMgr->save();
    
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px}"
        ".ok{background:#16213e;border:2px solid #0f3;border-radius:12px;padding:30px;max-width:400px;margin:auto}"
        "a{color:#4cc9f0;font-size:1.1em}</style></head><body>"
        "<div class='ok'><h2>✅ Horario actualizado</h2>"
        "<p>Los cambios se aplicarán de inmediato.</p>"
        "<a href='/schedule'>Volver al horario</a> | <a href='/'>Inicio</a></div></body></html>");
    server.send(200, "text/html", html);
}

void WebPortal::handleScanWifi() {
    server.send(200, "application/json", getWifiNetworks());
}

void WebPortal::handleReset() {
    configMgr->setDefaults();
    configMgr->save();
    server.send(200, "text/html", F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='3;url=/'>"
        "<style>body{font-family:sans-serif;background:#1a1a2e;color:#eee;text-align:center;padding:40px}</style>"
        "</head><body><h2>🔄 Configuración reseteada</h2><p>Reiniciando...</p></body></html>"));
    restartRequested = true;
}

void WebPortal::handleNotFound() {
    // Captive portal redirect
    if (apMode) {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "404 Not Found");
    }
}

String WebPortal::getWifiNetworks() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + 
                ",\"enc\":" + String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? 1 : 0) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    return json;
}

String WebPortal::getTimezoneOptions() {
    // Zonas horarias más comunes
    const char* zones[] = {
        "Europe/Madrid", "Europe/London", "Europe/Paris", "Europe/Berlin", "Europe/Rome",
        "Europe/Lisbon", "Europe/Moscow", "Europe/Istanbul",
        "America/New_York", "America/Chicago", "America/Denver", "America/Los_Angeles",
        "America/Mexico_City", "America/Bogota", "America/Buenos_Aires", "America/Sao_Paulo",
        "America/Santiago", "America/Lima", "America/Caracas",
        "Asia/Tokyo", "Asia/Shanghai", "Asia/Kolkata", "Asia/Dubai", "Asia/Seoul",
        "Australia/Sydney", "Australia/Melbourne", "Pacific/Auckland",
        "Africa/Cairo", "Africa/Johannesburg", "Africa/Lagos"
    };
    
    String opts = "";
    for (const char* tz : zones) {
        opts += "<option value='" + String(tz) + "'";
        if (String(tz) == String(configMgr->config.timezone)) opts += " selected";
        opts += ">" + String(tz) + "</option>";
    }
    return opts;
}

String WebPortal::generateScheduleHTML() {
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Horario Pantalla</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh;padding:20px}"
        ".card{background:#1a1a2e;border-radius:16px;padding:24px;max-width:480px;margin:20px auto;"
        "box-shadow:0 8px 32px rgba(0,0,0,.4)}"
        "h1{font-size:1.5em;margin-bottom:8px;text-align:center;color:#4cc9f0}"
        "h2{font-size:1.1em;color:#e0aaff;margin:16px 0 10px}"
        ".time-row{display:flex;gap:10px;align-items:center;margin:8px 0}"
        ".time-row label{flex:0 0 80px;color:#adb5bd}"
        "input[type=number]{width:70px;padding:10px;border-radius:8px;border:1px solid #333;background:#16213e;color:#fff;"
        "font-size:1.1em;text-align:center}"
        ".switch{position:relative;display:inline-block;width:52px;height:28px;margin:8px 0}"
        ".switch input{opacity:0;width:0;height:0}"
        ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#333;border-radius:28px;transition:.3s}"
        ".slider:before{position:absolute;content:'';height:22px;width:22px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.3s}"
        "input:checked+.slider{background:#4cc9f0}"
        "input:checked+.slider:before{transform:translateX(24px)}"
        ".sw-label{display:flex;align-items:center;gap:10px;margin:12px 0}"
        "button{width:100%;padding:14px;border:none;border-radius:10px;font-size:1.1em;cursor:pointer;"
        "background:linear-gradient(135deg,#4cc9f0,#7209b7);color:#fff;margin-top:16px;font-weight:bold}"
        "button:hover{opacity:.9}"
        "a{color:#4cc9f0;display:block;text-align:center;margin-top:14px}"
        ".preview{background:#16213e;border-radius:10px;padding:12px;margin:12px 0;text-align:center;font-size:.95em;color:#adb5bd}"
        "</style></head><body>");
    
    html += F("<div class='card'><h1>⏰ Horario de Pantalla</h1>"
        "<p style='text-align:center;color:#adb5bd;margin-bottom:16px'>Configura cuándo se enciende y apaga la pantalla</p>"
        "<form action='/schedule-save' method='POST'>");
    
    html += F("<h2>🌅 Encendido</h2>"
        "<div class='time-row'>"
        "<label>Hora:</label>"
        "<input type='number' name='scr_on_h' min='0' max='23' value='");
    html += String(configMgr->config.screenOnHour);
    html += F("'><span style='color:#adb5bd;margin:0 4px'>:</span>"
        "<input type='number' name='scr_on_m' min='0' max='59' value='");
    html += String(configMgr->config.screenOnMinute);
    html += F("'></div>");
    
    html += F("<h2>🌙 Apagado</h2>"
        "<div class='time-row'>"
        "<label>Hora:</label>"
        "<input type='number' name='scr_off_h' min='0' max='23' value='");
    html += String(configMgr->config.screenOffHour);
    html += F("'><span style='color:#adb5bd;margin:0 4px'>:</span>"
        "<input type='number' name='scr_off_m' min='0' max='59' value='");
    html += String(configMgr->config.screenOffMinute);
    html += F("'></div>");
    
    html += F("<div class='sw-label'><label class='switch'><input type='checkbox' name='scr_sleep'");
    if (configMgr->config.screenSleepEnabled) html += " checked";
    html += F("><span class='slider'></span></label><span>Activar horario automático</span></div>");
    
    html += F("<div class='preview'>Pantalla encendida de <strong>");
    html += String(configMgr->config.screenOnHour) + ":" + (configMgr->config.screenOnMinute < 10 ? "0" : "") + String(configMgr->config.screenOnMinute);
    html += F("</strong> a <strong>");
    html += String(configMgr->config.screenOffHour) + ":" + (configMgr->config.screenOffMinute < 10 ? "0" : "") + String(configMgr->config.screenOffMinute);
    html += F("</strong></div>");
    
    html += F("<button type='submit'>💾 Guardar Horario</button>"
        "</form><a href='/'>← Volver al inicio</a></div></body></html>");
    
    return html;
}

String WebPortal::generateHTML() {
    String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Estación Meteorológica - Configuración</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh}"
        ".header{background:linear-gradient(135deg,#16213e,#1a1a2e);padding:24px;text-align:center;"
        "border-bottom:2px solid #4cc9f0}"
        ".header h1{font-size:1.6em;color:#4cc9f0}"
        ".header p{color:#adb5bd;margin-top:6px}"
        ".container{max-width:520px;margin:0 auto;padding:16px}"
        ".card{background:#1a1a2e;border-radius:14px;padding:20px;margin:12px 0;"
        "box-shadow:0 4px 20px rgba(0,0,0,.3)}"
        ".card h2{color:#e0aaff;font-size:1.1em;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #333}"
        "label{display:block;color:#adb5bd;font-size:.9em;margin:8px 0 4px}"
        "input[type=text],input[type=password],input[type=number],select{"
        "width:100%;padding:11px 14px;border-radius:8px;border:1px solid #333;"
        "background:#16213e;color:#fff;font-size:1em;outline:none;transition:border .2s}"
        "input:focus,select:focus{border-color:#4cc9f0}"
        ".row{display:flex;gap:10px}"
        ".row>*{flex:1}"
        ".switch{position:relative;display:inline-block;width:48px;height:26px}"
        ".switch input{opacity:0;width:0;height:0}"
        ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;"
        "background:#333;border-radius:26px;transition:.3s}"
        ".slider:before{position:absolute;content:'';height:20px;width:20px;left:3px;bottom:3px;"
        "background:#fff;border-radius:50%;transition:.3s}"
        "input:checked+.slider{background:#4cc9f0}"
        "input:checked+.slider:before{transform:translateX(22px)}"
        ".sw-row{display:flex;align-items:center;gap:10px;margin:6px 0}"
        ".btn{width:100%;padding:14px;border:none;border-radius:10px;font-size:1.1em;"
        "cursor:pointer;font-weight:bold;margin-top:10px;transition:all .2s}"
        ".btn-primary{background:linear-gradient(135deg,#4cc9f0,#7209b7);color:#fff}"
        ".btn-danger{background:#e63946;color:#fff;font-size:.95em;margin-top:20px}"
        ".btn:hover{opacity:.85;transform:translateY(-1px)}"
        ".wifi-list{max-height:200px;overflow-y:auto;margin:8px 0}"
        ".wifi-item{padding:10px;background:#16213e;border-radius:8px;margin:4px 0;cursor:pointer;"
        "display:flex;justify-content:space-between;align-items:center;transition:background .2s}"
        ".wifi-item:hover{background:#1e2d4a}"
        ".signal{font-size:.8em;color:#4cc9f0}"
        ".scan-btn{background:#16213e;border:1px solid #4cc9f0;color:#4cc9f0;padding:8px 16px;"
        "border-radius:8px;cursor:pointer;font-size:.9em;margin:8px 0}"
        ".time-inputs{display:flex;align-items:center;gap:6px}"
        ".time-inputs input{width:65px;text-align:center}"
        ".nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0}"
        ".nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;"
        "border-radius:8px;font-size:.9em;transition:all .2s}"
        ".nav-links a:hover{background:#4cc9f0;color:#000}"
        "</style></head><body>");

    // Header
    html += F("<div class='header'><h1>🌤️ Estación Meteorológica</h1>"
        "<p>Panel de Configuración</p></div>"
        "<div class='container'>");
    
    // Navigation
    html += F("<div class='nav-links'>"
        "<a href='/'>🏠 Inicio</a>"
        "<a href='/schedule'>⏰ Horario</a>"
        "<a href='/status'>📊 Estado</a>"
        "</div>");
    
    html += F("<form action='/save' method='POST'>");
    
    // WiFi Card
    html += F("<div class='card'><h2>📶 Configuración WiFi</h2>"
        "<button type='button' class='scan-btn' onclick='scanWifi()'>🔍 Buscar redes</button>"
        "<div id='wifiList' class='wifi-list'></div>"
        "<label>SSID de la red</label>"
        "<input type='text' name='ssid' id='ssid' value='");
    html += String(configMgr->config.wifiSSID);
    html += F("' placeholder='Nombre de tu WiFi'>"
        "<label>Contraseña</label>"
        "<input type='password' name='pass' value='");
    html += String(configMgr->config.wifiPassword);
    html += F("' placeholder='Contraseña WiFi'></div>");
    
    // OpenWeatherMap Card
    html += F("<div class='card'><h2>🌍 OpenWeatherMap</h2>"
        "<label>Clave API</label>"
        "<input type='text' name='api' value='");
    html += String(configMgr->config.owmApiKey);
    html += F("' placeholder='Tu clave API de OpenWeatherMap'>"
        "<div class='row'><div><label>ID Ubicación</label>"
        "<input type='text' name='locid' value='");
    html += String(configMgr->config.owmLocationId);
    html += F("'></div><div><label>Nombre</label>"
        "<input type='text' name='locname' value='");
    html += String(configMgr->config.locationName);
    html += F("'></div></div>"
        "<label>Idioma</label>"
        "<select name='lang'>");
    
    const char* langs[][2] = {
        {"es","Español"},{"en","English"},{"fr","Français"},{"de","Deutsch"},
        {"it","Italiano"},{"pt","Português"},{"ca","Català"},{"gl","Galego"},
        {"ru","Русский"},{"ja","日本語"},{"zh_cn","中文"},{"ar","العربية"}
    };
    for (auto& l : langs) {
        html += "<option value='" + String(l[0]) + "'";
        if (configMgr->config.owmLanguage == l[0]) html += " selected";
        html += ">" + String(l[1]) + "</option>";
    }
    html += F("</select></div>");
    
    // Timezone Card
    html += F("<div class='card'><h2>🕐 Zona Horaria</h2>"
        "<label>Selecciona tu zona horaria</label>"
        "<select name='tz'>");
    html += getTimezoneOptions();
    html += F("</select></div>");
    
    // Display Settings Card
    html += F("<div class='card'><h2>⚙️ Configuración de Pantalla</h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' name='metric'");
    if (configMgr->config.isMetric) html += " checked";
    html += F("><span class='slider'></span></label><span>Sistema métrico (°C, m/s)</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' name='12h'");
    if (configMgr->config.is12hStyle) html += " checked";
    html += F("><span class='slider'></span></label><span>Formato 12 horas (AM/PM)</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' name='hhmm'");
    if (configMgr->config.isHHMM) html += " checked";
    html += F("><span class='slider'></span></label><span>Solo HH:MM (sin segundos)</span></div>"
        "<div class='row'><div><label>Actualizar cada (min)</label>"
        "<input type='number' name='update_min' min='5' max='60' value='");
    html += String(configMgr->config.updateIntervalMin);
    html += F("'></div><div><label>Brillo (%)</label>"
        "<input type='number' name='brightness' min='10' max='100' value='");
    html += String(configMgr->config.brightness);
    html += F("'></div></div></div>");
    
    // Screen Schedule Card (also in main form)
    html += F("<div class='card'><h2>⏰ Horario de Pantalla</h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' name='scr_sleep'");
    if (configMgr->config.screenSleepEnabled) html += " checked";
    html += F("><span class='slider'></span></label><span>Horario automático</span></div>"
        "<label>Encendido</label><div class='time-inputs'>"
        "<input type='number' name='scr_on_h' min='0' max='23' value='");
    html += String(configMgr->config.screenOnHour);
    html += F("'><span>:</span><input type='number' name='scr_on_m' min='0' max='59' value='");
    html += String(configMgr->config.screenOnMinute);
    html += F("'></div><label>Apagado</label><div class='time-inputs'>"
        "<input type='number' name='scr_off_h' min='0' max='23' value='");
    html += String(configMgr->config.screenOffHour);
    html += F("'><span>:</span><input type='number' name='scr_off_m' min='0' max='59' value='");
    html += String(configMgr->config.screenOffMinute);
    html += F("'></div></div>");
    
    // Submit
    html += F("<button type='submit' class='btn btn-primary'>💾 Guardar y Reiniciar</button>"
        "</form>");
    
    // Reset
    html += F("<form action='/reset' method='POST'>"
        "<button type='submit' class='btn btn-danger' onclick=\"return confirm('¿Estás seguro? Se perderá toda la configuración.')\">"
        "🗑️ Restaurar valores de fábrica</button></form>");
    
    html += F("</div>"
        "<script>"
        "function scanWifi(){"
        "document.getElementById('wifiList').innerHTML='<p style=\"color:#adb5bd\">Buscando redes...</p>';"
        "fetch('/scan').then(r=>r.json()).then(d=>{"
        "let h='';"
        "d.sort((a,b)=>b.rssi-a.rssi);"
        "d.forEach(n=>{"
        "let bars=n.rssi>-50?'████':n.rssi>-65?'███░':n.rssi>-75?'██░░':'█░░░';"
        "h+='<div class=\"wifi-item\" onclick=\"document.getElementById(\\'ssid\\').value=\\''+n.ssid+'\\'\">'"
        "+'<span>'+(n.enc?'🔒 ':'')+n.ssid+'</span>'"
        "+'<span class=\"signal\">'+bars+' '+n.rssi+'dBm</span></div>';"
        "});"
        "document.getElementById('wifiList').innerHTML=h||'<p>No se encontraron redes</p>';"
        "}).catch(()=>{document.getElementById('wifiList').innerHTML='<p style=\"color:#e63946\">Error al buscar</p>'});"
        "}"
        "</script></body></html>");
    
    return html;
}
