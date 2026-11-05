#include "web_portal.h"
#include <Updater.h>
#include "clock_page.h"
#include "version.h"
#include "bmp_writer.h"
#include "reset_log.h"

// Escapa texto para usarlo dentro de atributos value='...' del HTML
static String htmlEscape(const char *in) {
    String out;
    for (const char *p = in; *p; p++) {
        switch (*p) {
            case '&':  out += F("&amp;"); break;
            case '<':  out += F("&lt;"); break;
            case '>':  out += F("&gt;"); break;
            case '\'': out += F("&#39;"); break;
            case '"':  out += F("&quot;"); break;
            default:   out += *p;
        }
    }
    return out;
}

// Escapa texto para cadenas JSON
static String jsonEscape(const String &in) {
    String out;
    for (uint16_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if ((uint8_t)c < 0x20) out += ' ';
        else out += c;
    }
    return out;
}

WebPortal::WebPortal(ConfigManager *cfgMgr) 
    : server(WEB_SERVER_PORT), configMgr(cfgMgr), apMode(false), restartRequested(false), showClockRequested(false) {}

void WebPortal::beginAP() {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    delay(500);
    
    // DNS captive portal - redirige todo al ESP
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    Serial.printf_P(PSTR("[WebPortal] AP Mode - SSID: %s, IP: %s\n"), AP_SSID, WiFi.softAPIP().toString().c_str());
    setupRoutes();
    server.begin();
}

void WebPortal::beginSTA() {
    apMode = false;
    setupRoutes();
    server.begin();
    Serial.printf_P(PSTR("[WebPortal] STA server on http://%s/\n"), WiFi.localIP().toString().c_str());
    
    // mDNS: el portal también responde en http://timedesk.local
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", WEB_SERVER_PORT);
        Serial.printf_P(PSTR("[WebPortal] mDNS activo: http://%s.local/\n"), MDNS_NAME);
    } else {
        Serial.println(F("[WebPortal] No se pudo iniciar mDNS"));
    }
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
    } else {
        MDNS.update();
    }
    server.handleClient();
}

bool WebPortal::isAPMode() { return apMode; }
bool WebPortal::shouldRestart() { return restartRequested; }
bool WebPortal::consumeShowClockRequest() {
    bool r = showClockRequested;
    showClockRequested = false;
    return r;
}
String WebPortal::getLocalIP() { return apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

bool WebPortal::isAuthorized() {
    const AppConfig &c = configMgr->config;
    if (apMode || c.webPass[0] == 0) return true;
    return server.authenticate(c.webUser, c.webPass);
}

bool WebPortal::checkAuth() {
    if (isAuthorized()) return true;
    server.requestAuthentication(BASIC_AUTH, "TimeDesk",
        F("Hace falta el usuario y la contrasena del portal de TimeDesk"));
    return false;
}

void WebPortal::setupRoutes() {
    // Todas las páginas pasan por checkAuth()
    auto guard = [this](void (WebPortal::*fn)()) {
        return [this, fn]() { if (checkAuth()) (this->*fn)(); };
    };
    server.on("/", HTTP_GET, guard(&WebPortal::handleRoot));
    server.on("/save", HTTP_POST, guard(&WebPortal::handleSave));
    server.on("/status", HTTP_GET, guard(&WebPortal::handleStatus));
    server.on("/estado", HTTP_GET, guard(&WebPortal::handleStatusPage));
    server.on("/schedule", HTTP_GET, guard(&WebPortal::handleSchedule));
    server.on("/schedule-save", HTTP_POST, guard(&WebPortal::handleScheduleSave));
    server.on("/scan", HTTP_GET, guard(&WebPortal::handleScanWifi));
    server.on("/reset", HTTP_POST, guard(&WebPortal::handleReset));
    server.on("/update", HTTP_GET, guard(&WebPortal::handleUpdate));
    server.on("/clock", HTTP_GET, guard(&WebPortal::handleClock));
    server.on("/clock-save", HTTP_POST, guard(&WebPortal::handleClockSave));
    server.on("/clock-preview.bmp", HTTP_GET, guard(&WebPortal::handleClockPreview));
    server.on("/update", HTTP_POST,
        [this]() {
            if (!checkAuth()) return;
            server.sendHeader("Connection", "close");
            if (Update.hasError()) {
                String err = String("{\"ok\":false,\"error\":\"") + Update.getErrorString() + "\"}";
                server.send(500, "application/json", err);
            } else {
                server.send(200, "application/json", "{\"ok\":true}");
                restartRequested = true;
            }
        },
        [this]() { handleUpdateUpload(); }
    );
    server.onNotFound([this]() { handleNotFound(); });
}

void WebPortal::handleRoot() {
    // El HTML completo supera los 7KB: construirlo en un solo String agota el heap
    // del ESP8266 antes de llegar a las zonas horarias. Se envía en trozos pequeños.
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    // ── 1. DOCTYPE + CSS (estático, desde flash) ────────────────────────────
    server.sendContent_P(PSTR(
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TimeDesk</title><style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh}"
        ".header{background:linear-gradient(135deg,#16213e,#1a1a2e);padding:24px;text-align:center;"
        "border-bottom:2px solid #4cc9f0}"
        ".header h1{font-size:1.6em;color:#4cc9f0}.header p{color:#adb5bd;margin-top:6px}"
        ".container{max-width:520px;margin:0 auto;padding:16px}"
        ".card{background:#1a1a2e;border-radius:14px;padding:20px;margin:12px 0;"
        "box-shadow:0 4px 20px rgba(0,0,0,.3)}"
        ".card h2{color:#e0aaff;font-size:1.1em;margin-bottom:12px;padding-bottom:8px;"
        "border-bottom:1px solid #333}"
        "label{display:block;color:#adb5bd;font-size:.9em;margin:8px 0 4px}"
        "input[type=text],input[type=password],input[type=number],select{"
        "width:100%;padding:11px 14px;border-radius:8px;border:1px solid #333;"
        "background:#16213e;color:#fff;font-size:1em;outline:none;transition:border .2s}"
        "input:focus,select:focus{border-color:#4cc9f0}"
        ".row{display:flex;gap:10px}.row>*{flex:1}"
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
        ".wifi-item:hover{background:#1e2d4a}.signal{font-size:.8em;color:#4cc9f0}"
        ".scan-btn{background:#16213e;border:1px solid #4cc9f0;color:#4cc9f0;padding:8px 16px;"
        "border-radius:8px;cursor:pointer;font-size:.9em;margin:8px 0}"
        ".time-inputs{display:flex;align-items:center;gap:6px}"
        ".time-inputs input{width:65px;text-align:center}"
        ".nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0;flex-wrap:wrap}"
        ".nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;"
        "border-radius:8px;font-size:.9em;transition:all .2s}"
        ".nav-links a:hover{background:#4cc9f0;color:#000}"
        "</style></head><body>"));

    // ── 2. Cabecera + navegación ─────────────────────────────────────────────
    server.sendContent_P(PSTR(
        "<div class='header'><h1>&#128338; TimeDesk</h1>"
        "<p>Panel de Configuracion</p></div>"
        "<div class='container'>"
        "<div class='nav-links'>"
        "<a href='/'>&#127968; Inicio</a>"
        "<a href='/clock'>&#127912; Reloj</a>"
        "<a href='/schedule'>&#9200; Horario</a>"
        "<a href='/estado'>&#128202; Estado</a>"
        "<a href='/update'>&#128260; OTA</a>"
        "</div>"
        "<form action='/save' method='POST'>"));

    // ── 3. Tarjeta WiFi (contiene valores dinámicos) ─────────────────────────
    {
        String s;
        s += F("<div class='card'><h2>&#128246; WiFi</h2>"
            "<button type='button' class='scan-btn' onclick='scanWifi()'>&#128269; Buscar redes</button>"
            "<div id='wifiList' class='wifi-list'></div>"
            "<label>SSID de la red</label>"
            "<input type='text' name='ssid' id='ssid' value='");
        s += htmlEscape(configMgr->config.wifiSSID);
        s += F("' placeholder='Nombre de tu WiFi'>"
            "<label>Contrasena</label>"
            "<input type='password' name='pass' autocomplete='new-password' placeholder='");
        // La contraseña guardada no se envía al navegador: en blanco se conserva
        s += configMgr->config.wifiPassword[0] ? F("(sin cambios)") : F("Contrasena WiFi");
        s += F("'></div>");
        server.sendContent(s);
    }

    // ── 4. Tarjeta OpenWeatherMap ────────────────────────────────────────────
    {
        String s;
        s += F("<div class='card'><h2>&#127757; OpenWeatherMap</h2>"
            "<label>Clave API</label>"
            "<input type='text' name='api' value='");
        s += htmlEscape(configMgr->config.owmApiKey);
        s += F("' placeholder='Tu clave API de OpenWeatherMap'>"
            "<div class='row'><div><label>ID Ubicacion</label>"
            "<input type='text' name='locid' value='");
        s += htmlEscape(configMgr->config.owmLocationId);
        s += F("'></div><div><label>Nombre</label>"
            "<input type='text' name='locname' value='");
        s += htmlEscape(configMgr->config.locationName);
        s += F("'></div></div><label>Idioma</label><select name='lang'>");
        server.sendContent(s);
    }

    // ── 5. Opciones de idioma ────────────────────────────────────────────────
    {
        const char* langs[][2] = {
            {"es","Espanol"},{"en","English"},{"fr","Francais"},{"de","Deutsch"},
            {"it","Italiano"},{"pt","Portugues"},{"ca","Catala"},{"gl","Galego"},
            {"ru","Ruso"},{"ja","Japones"},{"zh_cn","Chino"},{"ar","Arabe"}
        };
        String s;
        for (auto& l : langs) {
            s += F("<option value='");
            s += l[0];
            s += F("'");
            if (configMgr->config.owmLanguage == l[0]) s += F(" selected");
            s += F(">");
            s += l[1];
            s += F("</option>");
        }
        s += F("</select></div>");
        server.sendContent(s);
    }

    // ── 6. Tarjeta Zona Horaria (crítica: se envía independiente) ────────────
    server.sendContent_P(PSTR(
        "<div class='card'><h2>&#128336; Zona Horaria</h2>"
        "<label>Selecciona tu zona horaria</label>"
        "<select name='tz'>"));
    server.sendContent(getTimezoneOptions());   // String pequeño aislado
    server.sendContent_P(PSTR("</select></div>"));

    // ── 7. Tarjeta configuración de pantalla ─────────────────────────────────
    {
        String s;
        s += F("<div class='card'><h2>&#9881;&#65039; Configuracion de Pantalla</h2>"
            "<div class='sw-row'><label class='switch'>"
            "<input type='checkbox' name='metric'");
        if (configMgr->config.isMetric) s += F(" checked");
        s += F("><span class='slider'></span></label>"
            "<span>Sistema metrico (&#176;C, m/s)</span></div>"
            "<div class='sw-row'><label class='switch'>"
            "<input type='checkbox' name='12h'");
        if (configMgr->config.is12hStyle) s += F(" checked");
        s += F("><span class='slider'></span></label>"
            "<span>Formato 12 horas (AM/PM)</span></div>"
            "<div class='sw-row'><label class='switch'>"
            "<input type='checkbox' name='hhmm'");
        if (configMgr->config.isHHMM) s += F(" checked");
        s += F("><span class='slider'></span></label>"
            "<span>Solo HH:MM (sin segundos)</span></div>"
            "<div class='row'><div><label>Actualizar cada (min)</label>"
            "<input type='number' name='update_min' min='5' max='60' value='");
        s += String(configMgr->config.updateIntervalMin);
        s += F("'></div><div><label>Brillo (%)</label>"
            "<input type='number' name='brightness' min='10' max='100' value='");
        s += String(configMgr->config.brightness);
        s += F("'></div></div></div>");
        server.sendContent(s);
    }

    // ── 8. Tarjeta horario de pantalla ───────────────────────────────────────
    {
        String s;
        s += F("<div class='card'><h2>&#9200; Horario de Pantalla</h2>"
            "<div class='sw-row'><label class='switch'>"
            "<input type='checkbox' name='scr_sleep'");
        if (configMgr->config.screenSleepEnabled) s += F(" checked");
        s += F("><span class='slider'></span></label>"
            "<span>Horario automatico</span></div>"
            "<label>Encendido</label><div class='time-inputs'>"
            "<input type='number' name='scr_on_h' min='0' max='23' value='");
        s += String(configMgr->config.screenOnHour);
        s += F("'><span>:</span>"
            "<input type='number' name='scr_on_m' min='0' max='59' value='");
        s += String(configMgr->config.screenOnMinute);
        s += F("'></div><label>Apagado</label><div class='time-inputs'>"
            "<input type='number' name='scr_off_h' min='0' max='23' value='");
        s += String(configMgr->config.screenOffHour);
        s += F("'><span>:</span>"
            "<input type='number' name='scr_off_m' min='0' max='59' value='");
        s += String(configMgr->config.screenOffMinute);
        s += F("'></div></div>");
        server.sendContent(s);
    }

    // ── 8b. Tarjeta de acceso al portal ─────────────────────────────────────
    {
        const AppConfig &c = configMgr->config;
        String s;
        s += F("<div class='card'><h2>&#128274; Acceso al portal</h2>");
        if (c.webPass[0]) {
            s += F("<p style='color:#4cc9f0;margin-bottom:8px'>&#9989; Protegido con contrasena</p>");
        } else {
            s += F("<p style='color:#f4a261;margin-bottom:8px'>&#9888;&#65039; Sin contrasena: cualquiera en tu WiFi puede "
                   "cambiar la configuracion o instalar un firmware.</p>");
        }
        s += F("<label>Usuario</label><input type='text' name='web_user' maxlength='23' autocomplete='username' value='");
        s += htmlEscape(c.webUser);
        s += F("'><label>Nueva contrasena (minimo 4 caracteres; en blanco no cambia)</label>"
               "<input type='password' name='web_pass' maxlength='32' autocomplete='new-password'>");
        if (c.webPass[0]) {
            s += F("<div class='sw-row'><label class='switch'><input type='checkbox' name='web_nopass'>"
                   "<span class='slider'></span></label><span>Quitar la contrasena</span></div>");
        }
        s += F("<p style='color:#adb5bd;font-size:.85em;margin-top:8px'>Si la olvidas: en el reloj, abre Ajustes y "
               "toca dos veces la linea \"Web\".</p></div>");
        server.sendContent(s);
    }

    // ── 9. Botones guardar / restaurar ──────────────────────────────────────
    server.sendContent_P(PSTR(
        "<button type='submit' class='btn btn-primary'>&#128190; Guardar y Reiniciar</button>"
        "</form>"
        "<form action='/reset' method='POST'>"
        "<button type='submit' class='btn btn-danger'"
        " onclick=\"return confirm('Seguro? Se perdera toda la configuracion.')\">"
        "&#128465;&#65039; Restaurar valores de fabrica</button></form>"));

    // ── 10. JavaScript (WiFi scan) ────────────────────────────────────────────
    server.sendContent_P(PSTR(
        "</div><script>"
        "function scanWifi(){"
        "document.getElementById('wifiList').innerHTML='<p style=\"color:#adb5bd\">Buscando redes...</p>';"
        "fetch('/scan').then(r=>r.json()).then(d=>{"
        "var h='';d.sort(function(a,b){return b.rssi-a.rssi;});"
        "d.forEach(function(n){"
        "var bars=n.rssi>-50?'\u2588\u2588\u2588\u2588':n.rssi>-65?'\u2588\u2588\u2588\u2591':n.rssi>-75?'\u2588\u2588\u2591\u2591':'\u2588\u2591\u2591\u2591';"
        "h+='<div class=\"wifi-item\" onclick=\"document.getElementById(\\'ssid\\').value=\\''+n.ssid+'\\'\">'"
        "+'<span>'+(n.enc?'\xF0\x9F\x94\x92 ':'')+n.ssid+'</span>'"
        "+'<span class=\"signal\">'+bars+' '+n.rssi+'dBm</span></div>';});"
        "document.getElementById('wifiList').innerHTML=h||'<p>No se encontraron redes</p>';"
        "}).catch(function(){"
        "document.getElementById('wifiList').innerHTML='<p style=\"color:#e63946\">Error al buscar</p>';});}"
        "</script></body></html>"));
}

void WebPortal::handleSave() {
    if (server.hasArg(F("ssid"))) {
        String ssid = server.arg(F("ssid"));
        String pass = server.hasArg(F("pass")) ? server.arg(F("pass")) : String();
        // Contraseña en blanco = conservar la actual, salvo que se cambie de red
        // (una red nueva sin contraseña escrita se toma como red abierta)
        bool sameNetwork = ssid == configMgr->config.wifiSSID;
        if (pass.length() || !sameNetwork)
            strlcpy(configMgr->config.wifiPassword, pass.c_str(), sizeof(configMgr->config.wifiPassword));
        strlcpy(configMgr->config.wifiSSID, ssid.c_str(), sizeof(configMgr->config.wifiSSID));
    }
    if (server.hasArg(F("api"))) strlcpy(configMgr->config.owmApiKey, server.arg(F("api")).c_str(), sizeof(configMgr->config.owmApiKey));
    if (server.hasArg(F("locid"))) strlcpy(configMgr->config.owmLocationId, server.arg(F("locid")).c_str(), sizeof(configMgr->config.owmLocationId));
    if (server.hasArg(F("locname"))) strlcpy(configMgr->config.locationName, server.arg(F("locname")).c_str(), sizeof(configMgr->config.locationName));
    if (server.hasArg(F("tz"))) strlcpy(configMgr->config.timezone, server.arg(F("tz")).c_str(), sizeof(configMgr->config.timezone));
    if (server.hasArg(F("lang"))) configMgr->config.owmLanguage = server.arg(F("lang"));
    
    configMgr->config.isMetric = server.hasArg(F("metric"));
    configMgr->config.is12hStyle = server.hasArg(F("12h"));
    configMgr->config.isHHMM = server.hasArg(F("hhmm"));
    
    if (server.hasArg(F("update_min"))) configMgr->config.updateIntervalMin = server.arg(F("update_min")).toInt();
    if (server.hasArg(F("brightness"))) configMgr->config.brightness = server.arg(F("brightness")).toInt();
    
    // Acceso al portal
    if (server.hasArg(F("web_user")) && server.arg(F("web_user")).length())
        strlcpy(configMgr->config.webUser, server.arg(F("web_user")).c_str(), sizeof(configMgr->config.webUser));
    if (server.hasArg(F("web_nopass"))) {
        configMgr->config.webPass[0] = 0;
    } else if (server.hasArg(F("web_pass")) && server.arg(F("web_pass")).length() >= 4) {
        strlcpy(configMgr->config.webPass, server.arg(F("web_pass")).c_str(), sizeof(configMgr->config.webPass));
    }
    
    // Screen schedule from main form
    if (server.hasArg(F("scr_on_h"))) configMgr->config.screenOnHour = server.arg(F("scr_on_h")).toInt();
    if (server.hasArg(F("scr_on_m"))) configMgr->config.screenOnMinute = server.arg(F("scr_on_m")).toInt();
    if (server.hasArg(F("scr_off_h"))) configMgr->config.screenOffHour = server.arg(F("scr_off_h")).toInt();
    if (server.hasArg(F("scr_off_m"))) configMgr->config.screenOffMinute = server.arg(F("scr_off_m")).toInt();
    configMgr->config.screenSleepEnabled = server.hasArg(F("scr_sleep"));
    
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
    json += String(F("\"heap\":")) + String(ESP.getFreeHeap()) + ",";
    json += String(F("\"rssi\":")) + String(WiFi.RSSI()) + ",";
    json += String(F("\"ip\":\"")) + WiFi.localIP().toString() + "\",";
    json += "\"mdns\":\"" MDNS_NAME ".local\",";
    json += String(F("\"uptime\":")) + String(millis() / 1000) + ",";
    json += String(F("\"min_heap\":")) + String(ResetLog::minFreeHeap()) + ",";
    json += String(F("\"reset_reason\":")) + String(ESP.getResetInfoPtr()->reason) + ",";
    json += String(F("\"version\":\"")) + FIRMWARE_VERSION + "\",";
    json += String(F("\"vcc\":")) + String(ESP.getVcc() / 1024.0) + ",";
    json += String(F("\"scr_on\":\"")) + String(configMgr->config.screenOnHour) + String(F(":")) + (configMgr->config.screenOnMinute < 10 ? "0" : "") + String(configMgr->config.screenOnMinute) + "\",";
    json += String(F("\"clock_style\":")) + String(configMgr->config.clockStyle) + ",";
    json += String(F("\"scr_off\":\"")) + String(configMgr->config.screenOffHour) + String(F(":")) + (configMgr->config.screenOffMinute < 10 ? "0" : "") + String(configMgr->config.screenOffMinute) + "\"";
    json += F("}");
    server.send(200, "application/json", json);
}

void WebPortal::handleSchedule() {
    streamSchedulePage();
}

void WebPortal::handleScheduleSave() {
    if (server.hasArg(F("scr_on_h"))) configMgr->config.screenOnHour = server.arg(F("scr_on_h")).toInt();
    if (server.hasArg(F("scr_on_m"))) configMgr->config.screenOnMinute = server.arg(F("scr_on_m")).toInt();
    if (server.hasArg(F("scr_off_h"))) configMgr->config.screenOffHour = server.arg(F("scr_off_h")).toInt();
    if (server.hasArg(F("scr_off_m"))) configMgr->config.screenOffMinute = server.arg(F("scr_off_m")).toInt();
    configMgr->config.screenSleepEnabled = server.hasArg(F("scr_sleep"));
    
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

void WebPortal::handleUpdate() {
    streamUpdatePage();
}

void WebPortal::handleUpdateUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) uploadAuthorized = isAuthorized();
    if (!uploadAuthorized) return;   // sin permiso: no se escribe nada en la flash
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf_P(PSTR("[OTA] Iniciando actualización: %s\n"), upload.filename.c_str());
        if (!Update.begin(ESP.getFreeSketchSpace())) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf_P(PSTR("[OTA] Completado: %u bytes\n"), upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

void WebPortal::handleClock() {
    // Pagina en flash enviada en dos trozos con el estado actual en medio
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(CLOCK_PAGE_HEAD);
    String state = String(F("var CUR=")) + String(configMgr->config.clockStyle) +
                   String(F(",START=")) + String(configMgr->config.startOnClock ? 1 : 0) +
                   String(F(",BR=")) + String(configMgr->config.brightness) + ";";
    server.sendContent(state);
    server.sendContent_P(CLOCK_PAGE_TAIL);
}

void WebPortal::handleClockPreview() {
    // Dibuja el estilo pedido en el framebuffer (sin enviarlo a la pantalla:
    // el bucle principal lo redibuja todo en la siguiente vuelta) y lo envía
    // como BMP a media resolución (120x160, 9,5 KB en vez de 38 KB): en la
    // página se ve a ese tamaño y la transferencia, que bloquea el reloj
    // mientras dura, tarda la cuarta parte.
    long style = server.hasArg(F("s")) ? server.arg(F("s")).toInt() : configMgr->config.clockStyle;
    if (!previewRender || !previewPixel || style < 0 || style >= CLOCK_STYLE_MAX) {
        server.send(404, "text/plain", "Sin vista previa");
        return;
    }
    const uint16_t W = 120, H = 160;   // la pantalla es de 240x320
    uint16_t pal[4];
    previewRender((uint8_t)style, pal);

    // El navegador puede reutilizarla 5 min (la página pide la del estilo
    // activo con un parámetro distinto cuando quiere refrescarla)
    server.sendHeader(F("Cache-Control"), F("max-age=300"));
    server.setContentLength(bmp4FileSize(W, H));
    server.send(200, "image/bmp", "");
    const uint32_t rowBytes = bmp4RowBytes(W);        // 60 bytes
    const uint8_t ROWS = 16;                           // 960 bytes por envío
    uint8_t buf[ROWS * 60];
    bmp4Header(buf, W, H, pal);
    server.sendContent((const char *)buf, BMP4_HEADER_SIZE);
    for (int16_t y = H - 1; y >= 0; y -= ROWS) {
        uint8_t rows = 0;
        for (; rows < ROWS && y - rows >= 0; rows++) {
            const uint16_t sy = (y - rows) * 2;
            // De cada bloque 2x2 se toma el primer color que no sea el fondo
            // (índice 0): así no desaparecen las líneas y letras de 1 píxel
            bmp4PackRow(buf + rows * rowBytes, W, [&](uint16_t x) {
                const uint16_t sx = x * 2;
                uint8_t c = previewPixel(sx, sy);
                if (!c) c = previewPixel(sx + 1, sy);
                if (!c) c = previewPixel(sx, sy + 1);
                if (!c) c = previewPixel(sx + 1, sy + 1);
                return c;
            });
        }
        server.sendContent((const char *)buf, rows * rowBytes);
        yield();
    }
}

void WebPortal::handleClockSave() {
    // Cambios en caliente: se guardan y se aplican sin reiniciar
    AppConfig &c = configMgr->config;
    if (server.hasArg(F("style"))) {
        long st = server.arg(F("style")).toInt();
        if (st < 0 || st >= CLOCK_STYLE_MAX) {
            server.send(400, "application/json", F("{\"ok\":false,\"error\":\"estilo no valido\"}"));
            return;
        }
        c.clockStyle = st;
    }
    if (server.hasArg(F("start_clock"))) c.startOnClock = server.arg(F("start_clock")).toInt() != 0;
    if (server.hasArg(F("brightness"))) c.brightness = constrain(server.arg(F("brightness")).toInt(), 10, 100);
    if (server.hasArg(F("show"))) showClockRequested = true;
    configMgr->save();

    String json = String(F("{\"ok\":true,\"style\":")) + String(c.clockStyle) +
                  String(F(",\"start_clock\":")) + String(c.startOnClock ? 1 : 0) +
                  String(F(",\"brightness\":")) + String(c.brightness) + "}";
    server.send(200, "application/json", json);
}

void WebPortal::handleStatusPage() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PSTR(
        "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TimeDesk · Estado</title><style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:'Segoe UI',sans-serif;background:#0f0e17;color:#fffffe;min-height:100vh}"
        ".header{background:linear-gradient(135deg,#16213e,#1a1a2e);padding:24px;text-align:center;border-bottom:2px solid #4cc9f0}"
        ".header h1{font-size:1.6em;color:#4cc9f0}.header p{color:#adb5bd;margin-top:6px}"
        ".container{max-width:620px;margin:0 auto;padding:16px}"
        ".card{background:#1a1a2e;border-radius:14px;padding:20px;margin:12px 0;box-shadow:0 4px 20px rgba(0,0,0,.3)}"
        ".card h2{color:#e0aaff;font-size:1.1em;margin-bottom:12px;padding-bottom:8px;border-bottom:1px solid #333}"
        ".nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0;flex-wrap:wrap}"
        ".nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;border-radius:8px;font-size:.9em}"
        ".grid{display:grid;grid-template-columns:auto 1fr;gap:6px 16px;font-size:.95em}"
        ".grid span{color:#adb5bd}.grid b{word-break:break-word}"
        "table{width:100%;border-collapse:collapse;font-size:.85em}"
        "th,td{text-align:left;padding:7px 6px;border-bottom:1px solid #2a2a40;vertical-align:top}"
        "th{color:#adb5bd;font-weight:normal}"
        ".bad{color:#ff6b6b}small{color:#adb5bd}code{font-size:.95em}"
        "a.raw{color:#4cc9f0;font-size:.85em}"
        "</style></head><body>"
        "<div class='header'><h1>&#128338; TimeDesk</h1><p>Estado del dispositivo</p></div>"
        "<div class='container'>"
        "<div class='nav-links'><a href='/'>&#127968; Inicio</a><a href='/clock'>&#127912; Reloj</a>"
        "<a href='/schedule'>&#9200; Horario</a><a href='/estado'>&#128202; Estado</a><a href='/update'>&#128260; OTA</a></div>"
        "<div class='card'><h2>&#128202; Ahora</h2><div class='grid'>"));

    char buf[64];
    String s;
    uint32_t up = millis() / 1000;
    snprintf_P(buf, sizeof(buf), PSTR("%lud %02luh %02lum"), (unsigned long)(up / 86400),
               (unsigned long)((up / 3600) % 24), (unsigned long)((up / 60) % 60));
    s += F("<span>Version</span><b>" FIRMWARE_VERSION "</b>");
    s += F("<span>Encendido</span><b>"); s += buf; s += F("</b>");
    s += F("<span>Memoria libre</span><b>"); s += String(ESP.getFreeHeap() / 1024.0, 1);
    s += F(" KB</b><span>Minima desde el arranque</span><b>"); s += String(ResetLog::minFreeHeap() / 1024.0, 1);
    s += F(" KB</b><span>Bloque libre mayor</span><b>"); s += String(ESP.getMaxFreeBlockSize() / 1024.0, 1);
    s += F(" KB</b><span>WiFi</span><b>"); s += htmlEscape(configMgr->config.wifiSSID);
    s += F(" ("); s += String(WiFi.RSSI()); s += F(" dBm)</b><span>Direccion</span><b>http://");
    s += WiFi.localIP().toString(); s += F(" &middot; " MDNS_NAME ".local</b>");
    s += F("<span>Portal</span><b>");
    s += configMgr->config.webPass[0] ? F("Protegido con contrasena") : F("Sin contrasena");
    s += F("</b>");
    s += F("</div></div>");
    server.sendContent(s);

    // Registro de reinicios (más reciente primero)
    server.sendContent_P(PSTR("<div class='card'><h2>&#128260; Ultimos reinicios</h2>"
        "<table><tr><th>Fecha</th><th>Motivo</th><th>Version</th></tr>"));
    ResetLog::Entry entries[ResetLog::MAX_ENTRIES];
    uint8_t n = ResetLog::load(entries, ResetLog::MAX_ENTRIES);
    for (uint8_t i = 0; i < n; i++) {
        const ResetLog::Entry &e = entries[i];
        s = F("<tr><td>");
        if (e.epoch >= NTP_MIN_VALID_EPOCH) {
            time_t t = e.epoch;
            strftime(buf, sizeof(buf), "%d/%m %H:%M", localtime(&t));
            s += buf;
        } else {
            s += F("&mdash;");
        }
        bool bad = e.reason == REASON_EXCEPTION_RST || e.reason == REASON_WDT_RST || e.reason == REASON_SOFT_WDT_RST;
        s += bad ? F("</td><td class='bad'>") : F("</td><td>");
        s += ResetLog::reasonText(e.reason);
        if (e.reason == REASON_EXCEPTION_RST) {
            snprintf_P(buf, sizeof(buf), PSTR("<br><small>Excepcion %u: "), e.exccause);
            s += buf;
            s += ResetLog::exceptionText(e.exccause);
            snprintf_P(buf, sizeof(buf), PSTR("<br>codigo <code>0x%08lx</code></small>"), (unsigned long)e.epc1);
            s += buf;
        } else if ((e.reason == REASON_SOFT_WDT_RST || e.reason == REASON_WDT_RST) && e.epc1) {
            snprintf_P(buf, sizeof(buf), PSTR("<br><small>codigo <code>0x%08lx</code></small>"), (unsigned long)e.epc1);
            s += buf;
        }
        s += F("</td><td>");
        s += e.version;
        s += F("</td></tr>");
        server.sendContent(s);
    }
    if (n == 0) server.sendContent_P(PSTR("<tr><td colspan='3'>Sin registros todavia</td></tr>"));
    server.sendContent_P(PSTR("</table><p style='margin-top:10px'><small>Si aparecen excepciones, copia la fila "
        "(sobre todo el codigo 0x...): sirve para localizar el fallo en el programa.</small></p></div>"));

    // Pila del último fallo (memoria RTC: se pierde al desenchufar)
    ResetLog::CrashStack cs;
    if (ResetLog::loadCrashStack(cs)) {
        s = F("<div class='card'><h2>&#128269; Pila del ultimo fallo</h2><p><small>");
        s += ResetLog::reasonText(cs.reason);
        snprintf_P(buf, sizeof(buf), PSTR(" &middot; v%s &middot; codigo 0x%08lx"),
                   cs.version, (unsigned long)cs.epc1);
        s += buf;
        s += F("</small></p><p><code>");
        for (uint8_t i = 0; i < cs.count; i++) {
            snprintf_P(buf, sizeof(buf), PSTR("0x%08lx "), (unsigned long)cs.addr[i]);
            s += buf;
        }
        if (cs.count == 0) s += F("(sin direcciones)");
        s += F("</code></p><p><small>Copia estas direcciones: indican la cadena de llamadas "
               "que llevo al fallo.</small></p></div>");
        server.sendContent(s);
    }
    server.sendContent_P(PSTR(
        "<p style='text-align:center'><a class='raw' href='/status'>Ver datos en JSON</a></p>"
        "</div></body></html>"));
}

void WebPortal::handleNotFound() {
    // Captive portal redirect
    if (apMode) {
        server.sendHeader("Location", String(F("http://")) + WiFi.softAPIP().toString() + "/");
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "404 Not Found");
    }
}

String WebPortal::getWifiNetworks() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += F(",");
        json += String(F("{\"ssid\":\"")) + jsonEscape(WiFi.SSID(i)) + String(F("\",\"rssi\":")) + String(WiFi.RSSI(i)) + 
                String(F(",\"enc\":")) + String(WiFi.encryptionType(i) != ENC_TYPE_NONE ? 1 : 0) + "}";
    }
    json += F("]");
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
        opts += String(F("<option value='")) + String(tz) + "'";
        if (String(tz) == String(configMgr->config.timezone)) opts += F(" selected");
        opts += String(F(">")) + String(tz) + "</option>";
    }
    return opts;
}

void WebPortal::streamSchedulePage() {
    // Se envía por trozos: construir la página entera en un String necesitaba
    // el doble de su tamaño en RAM contigua y reiniciaba el ESP8266
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PSTR("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TimeDesk · Horario</title>"
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
        "</style></head><body>"));
    
    server.sendContent_P(PSTR("<div class='card'><h1>⏰ Horario de Pantalla</h1>"
        "<p style='text-align:center;color:#adb5bd;margin-bottom:16px'>Configura cuándo se enciende y apaga la pantalla</p>"
        "<form action='/schedule-save' method='POST'>"));
    
    server.sendContent_P(PSTR("<h2>🌅 Encendido</h2>"
        "<div class='time-row'>"
        "<label>Hora:</label>"
        "<input type='number' name='scr_on_h' min='0' max='23' value='"));
    server.sendContent(String(configMgr->config.screenOnHour));
    server.sendContent_P(PSTR("'><span style='color:#adb5bd;margin:0 4px'>:</span>"
        "<input type='number' name='scr_on_m' min='0' max='59' value='"));
    server.sendContent(String(configMgr->config.screenOnMinute));
    server.sendContent_P(PSTR("'></div>"));
    
    server.sendContent_P(PSTR("<h2>🌙 Apagado</h2>"
        "<div class='time-row'>"
        "<label>Hora:</label>"
        "<input type='number' name='scr_off_h' min='0' max='23' value='"));
    server.sendContent(String(configMgr->config.screenOffHour));
    server.sendContent_P(PSTR("'><span style='color:#adb5bd;margin:0 4px'>:</span>"
        "<input type='number' name='scr_off_m' min='0' max='59' value='"));
    server.sendContent(String(configMgr->config.screenOffMinute));
    server.sendContent_P(PSTR("'></div>"));
    
    server.sendContent_P(PSTR("<div class='sw-label'><label class='switch'><input type='checkbox' name='scr_sleep'"));
    if (configMgr->config.screenSleepEnabled) server.sendContent_P(PSTR(" checked"));
    server.sendContent_P(PSTR("><span class='slider'></span></label><span>Activar horario automático</span></div>"));
    
    server.sendContent_P(PSTR("<div class='preview'>Pantalla encendida de <strong>"));
    server.sendContent(String(configMgr->config.screenOnHour) + String(F(":")) + (configMgr->config.screenOnMinute < 10 ? "0" : "") + String(configMgr->config.screenOnMinute));
    server.sendContent_P(PSTR("</strong> a <strong>"));
    server.sendContent(String(configMgr->config.screenOffHour) + String(F(":")) + (configMgr->config.screenOffMinute < 10 ? "0" : "") + String(configMgr->config.screenOffMinute));
    server.sendContent_P(PSTR("</strong></div>"));
    
    server.sendContent_P(PSTR("<button type='submit'>💾 Guardar Horario</button>"
        "</form>"
        "<a href='/'>← Inicio</a>"
        "<a href='/clock'>🎨 Estilos de reloj</a>"
        "<a href='/update'>🔄 Actualización OTA</a>"
        "</div></body></html>"));
    
}

void WebPortal::streamUpdatePage() {
    // Se envía por trozos: construir la página entera en un String necesitaba
    // el doble de su tamaño en RAM contigua y reiniciaba el ESP8266
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PSTR("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>TimeDesk · Actualizaciones</title>"
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
        ".warn{background:#2d1b00;border:1px solid #f4a261;border-radius:10px;padding:14px;"
        "color:#f4a261;font-size:.9em;margin:10px 0}"
        ".info{background:#001828;border:1px solid #4cc9f0;border-radius:10px;padding:14px;"
        "color:#adb5bd;font-size:.9em;margin:10px 0}"
        ".drop{border:2px dashed #4cc9f0;border-radius:10px;padding:28px;text-align:center;"
        "color:#adb5bd;cursor:pointer;transition:all .2s;margin:12px 0}"
        ".drop.ok{border-color:#0f3;background:rgba(0,255,51,.05)}"
        ".drop.ok .dt{color:#0f3}"
        "input[type=file]{display:none}"
        ".btn{width:100%;padding:14px;border:none;border-radius:10px;font-size:1.1em;"
        "cursor:pointer;font-weight:bold;margin-top:10px;transition:all .2s;"
        "background:linear-gradient(135deg,#4cc9f0,#7209b7);color:#fff}"
        ".btn:hover:not(:disabled){opacity:.85;transform:translateY(-1px)}"
        ".btn:disabled{opacity:.4;cursor:not-allowed}"
        ".pb{display:none;margin:14px 0}"
        ".pb-track{height:22px;background:#16213e;border-radius:11px;overflow:hidden}"
        ".pb-fill{height:100%;background:linear-gradient(90deg,#4cc9f0,#7209b7);width:0%;transition:width .25s}"
        ".pb-txt{text-align:center;margin-top:6px;color:#adb5bd;font-size:.9em}"
        ".res{margin:14px 0;padding:16px;border-radius:10px;display:none}"
        ".res.ok{background:#001b00;border:1px solid #0f3;color:#0f3}"
        ".res.err{background:#1b0000;border:1px solid #e63946;color:#e63946}"
        ".nav-links{display:flex;gap:10px;justify-content:center;margin:16px 0;flex-wrap:wrap}"
        ".nav-links a{color:#4cc9f0;text-decoration:none;padding:8px 16px;border:1px solid #4cc9f0;"
        "border-radius:8px;font-size:.9em;transition:all .2s}"
        ".nav-links a:hover{background:#4cc9f0;color:#000}"
        "</style></head><body>"));

    server.sendContent_P(PSTR("<div class='header'><h1>🕒 TimeDesk</h1>"
        "<p>Actualizaciones de firmware</p></div>"
        "<div class='container'>"
        "<div class='nav-links'>"
        "<a href='/'>🏠 Inicio</a>"
        "<a href='/clock'>🎨 Reloj</a>"
        "<a href='/schedule'>⏰ Horario</a>"
        "<a href='/estado'>📊 Estado</a>"
        "<a href='/update'>🔄 OTA</a>"
        "</div>"));

    server.sendContent_P(PSTR("<div class='card'>"
        "<h2>🔄 Actualizar Firmware</h2>"
        "<div class='warn'>⚠️ <strong>No interrumpas la actualización.</strong> "
        "El dispositivo se reiniciará automáticamente al terminar.</div>"
        "<div class='info'>ℹ️ Versión instalada: <strong>" FIRMWARE_VERSION "</strong>.<br>"
        "Descarga <strong>firmware.bin</strong> de la última Release del repositorio en GitHub "
        "(o usa <code>.pio/build/nodemcuv2/firmware.bin</code> si lo compilas tú) y súbelo aquí.</div>"
        "<div class='drop' id='dp' onclick='document.getElementById(\"fw\").click()'>"
        "<div style='font-size:2.2em;margin-bottom:8px'>📁</div>"
        "<div class='dt' id='dt'>Haz clic para seleccionar firmware.bin</div>"
        "<div style='font-size:.8em;color:#555;margin-top:4px'>Solo archivos .bin</div>"
        "</div>"
        "<input type='file' id='fw' accept='.bin'>"
        "<div class='pb' id='pb'>"
        "<div class='pb-track'><div class='pb-fill' id='pf'></div></div>"
        "<div class='pb-txt' id='pt'>Subiendo...</div>"
        "</div>"
        "<div class='res' id='rs'></div>"
        "<button class='btn' id='ub' onclick='doUpdate()' disabled>⬆️ Actualizar Firmware</button>"
        "</div></div>"
        "<script>"
        "document.getElementById('fw').addEventListener('change',function(){"
        "if(this.files[0]){"
        "var f=this.files[0];"
        "document.getElementById('dt').textContent='📦 '+f.name+' ('+Math.round(f.size/1024)+' KB)';"
        "document.getElementById('dp').classList.add('ok');"
        "document.getElementById('ub').disabled=false;"
        "}});"
        "function doUpdate(){"
        "var file=document.getElementById('fw').files[0];"
        "if(!file)return;"
        "document.getElementById('ub').disabled=true;"
        "document.getElementById('rs').style.display='none';"
        "document.getElementById('pb').style.display='block';"
        "var xhr=new XMLHttpRequest();"
        "var fd=new FormData();"
        "fd.append('firmware',file);"
        "xhr.upload.addEventListener('progress',function(e){"
        "if(e.lengthComputable){"
        "var p=Math.round(e.loaded/e.total*100);"
        "document.getElementById('pf').style.width=p+'%';"
        "document.getElementById('pt').textContent='Subiendo... '+p+'%';"
        "}});"
        "xhr.addEventListener('load',function(){"
        "document.getElementById('pb').style.display='none';"
        "var r=document.getElementById('rs');"
        "r.style.display='block';"
        "if(xhr.status===200){"
        "r.className='res ok';"
        "r.innerHTML='✅ Firmware actualizado correctamente. Reiniciando en 5 segundos...';"
        "setTimeout(function(){location.href='/';},6000);"
        "}else{"
        "r.className='res err';"
        "try{var j=JSON.parse(xhr.responseText);r.innerHTML='❌ Error: '+j.error;}"
        "catch(e){r.innerHTML='❌ Error al actualizar el firmware';}"
        "document.getElementById('ub').disabled=false;"
        "}});"
        "xhr.addEventListener('error',function(){"
        "document.getElementById('pb').style.display='none';"
        "var r=document.getElementById('rs');"
        "r.style.display='block';r.className='res err';"
        "r.innerHTML='❌ Error de conexión. Comprueba que el dispositivo sigue encendido.';"
        "document.getElementById('ub').disabled=false;});"
        "xhr.open('POST','/update');"
        "xhr.send(fd);"
        "}"
        "</script></body></html>"));

}
