// ============================================================
//   Arrosage Automatique — ESP32 + WiFiManager + Adafruit IO
//   + OTA via GitHub (HTTP/HTTPS)
// ============================================================

#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <WebServer.h>
#include "secrets.h"

// ─────────────────────────────────────────
//   Adafruit IO
// ─────────────────────────────────────────
// IO_USERNAME et IO_KEY sont définis dans secrets.h

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883

// ─────────────────────────────────────────
//   Pins & Display
// ─────────────────────────────────────────
#define RELAY_PIN     26
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

// ─────────────────────────────────────────
//   WiFi
// ─────────────────────────────────────────
#define WIFI_MAX_RETRIES  3
#define WIFI_RETRY_DELAY  10000
#define AP_SSID           "Arrosage-Setup"

// ─────────────────────────────────────────
//   Adafruit IO — reconnexion
// ─────────────────────────────────────────
#define AIO_MAX_RETRIES   5
#define AIO_RETRY_DELAY_S 5

// ─────────────────────────────────────────
//   OTA — À CONFIGURER
// ─────────────────────────────────────────
// Version locale du firmware (format X.Y.Z)
#define FW_VERSION        "1.0.0"

// OTA_VERSION_URL, OTA_FIRMWARE_URL → secrets.h

// Timeout téléchargement (ms)
#define OTA_TIMEOUT_MS    60000

// ─────────────────────────────────────────
//   Objets
// ─────────────────────────────────────────
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiManager       wifiManager;
WebServer         server(80);

// Client ID unique basé sur le MAC de la puce (évite code=-2 / doublon de session)
char _mqttClientId[40] = "plantsio_esp32";
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, _mqttClientId, IO_USERNAME, IO_KEY);

Adafruit_MQTT_Subscribe pumpFeed     = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/pompe");
Adafruit_MQTT_Publish   pumpGet      = Adafruit_MQTT_Publish(&mqtt,   IO_USERNAME "/feeds/pompe/get");
Adafruit_MQTT_Publish   stateFeed    = Adafruit_MQTT_Publish(&mqtt,   IO_USERNAME "/feeds/pompe_etat");
Adafruit_MQTT_Subscribe programmeFeed = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/programme");
Adafruit_MQTT_Publish   programmeGet  = Adafruit_MQTT_Publish(&mqtt,  IO_USERNAME "/feeds/programme/get");
Adafruit_MQTT_Publish   alertFeed     = Adafruit_MQTT_Publish(&mqtt,  IO_USERNAME "/feeds/alerte");

// ─────────────────────────────────────────
//   État global — Arrosage
// ─────────────────────────────────────────
bool pumpRunning  = false;
bool aioConnected = false;
bool timeSynced   = false;
unsigned long lastTimeSyncMs = 0;

bool hasProgram      = false;
char nextWaterDate[8] = "--/--";
char nextWaterTime[6] = "--:--";
char nextWaterDur[8]  = "";

// ─────────────────────────────────────────
//   État global — OTA
// ─────────────────────────────────────────
enum OtaState {
  OTA_IDLE,
  OTA_CHECKING,
  OTA_DOWNLOADING,
  OTA_SUCCESS,
  OTA_NO_UPDATE,
  OTA_ERROR
};

OtaState otaState     = OTA_IDLE;
String   otaMessage   = "Inactif";
bool     otaRequested = false;  // Flag déclenché par GET /update
String   systemAlert  = "";    // Dernière alerte critique système

bool     updateAvailable  = false;  // Mise à jour détectée au boot
String   remoteVersionStr = "";     // Version distante disponible

// ─────────────────────────────────────────
//   Prototypes
// ─────────────────────────────────────────
void     oledPrint(String line1, String line2 = "", String line3 = "", String line4 = "");
bool     connectWifi();
void     startConfigPortal();
void     connectAdafruitIO();
void     setPump(bool state);
void     updateStatusScreen();
bool     syncTimeNow();
String   getCurrentTimeString();
void     showPumpStatusChange(bool state);
void     showWifiConnecting(int attempt);
void     showWifiConfigPortal();

void     criticalError(String msg);
void     setupWebServer();
void     setOtaState(OtaState state, String detail = "");
void     checkAndUpdate();
void     checkUpdateAvailable();
void     showUpdateAvailableAnim(const String& remoteVer);
String   getRemoteVersion();
bool     isNewerVersion(const String& remoteVer, const String& localVer);
void     performOTA();
void     showAioReconnecting(int attempt, int maxAttempts, int countdown, const char* errorMsg = nullptr);


// ════════════════════════════════════════════════════════════
//                   PAGE WEB EMBARQUÉE
// ════════════════════════════════════════════════════════════

static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Arrosage Automatique</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, Arial, sans-serif;
      background: #f4f6f9;
      min-height: 100vh;
      padding: 24px 12px;
    }
    .page { max-width: 520px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
    .card {
      background: white;
      border-radius: 12px;
      padding: 24px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.08);
    }
    .card-title {
      font-size: 13px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: .06em;
      color: #888;
      margin-bottom: 16px;
    }
    h1 { font-size: 20px; color: #1a1a2e; margin-bottom: 2px; }
    .subtitle { font-size: 13px; color: #aaa; margin-bottom: 0; }
    .info-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 8px 0;
      border-bottom: 1px solid #f0f0f0;
      font-size: 14px;
    }
    .info-row:last-of-type { border-bottom: none; }
    .label { color: #888; }
    .value { font-weight: 600; color: #222; }
    /* Pompe toggle */
    .pump-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }
    .pump-label { font-size: 15px; font-weight: 600; color: #333; }
    .pump-sub { font-size: 12px; color: #aaa; margin-top: 2px; }
    .toggle-wrap { position: relative; width: 56px; height: 30px; flex-shrink: 0; }
    .toggle-wrap input { opacity: 0; width: 0; height: 0; position: absolute; }
    .slider {
      position: absolute; inset: 0;
      background: #ccc;
      border-radius: 30px;
      cursor: pointer;
      transition: background .3s;
    }
    .slider:before {
      content: '';
      position: absolute;
      width: 22px; height: 22px;
      left: 4px; bottom: 4px;
      background: white;
      border-radius: 50%;
      transition: transform .3s;
      box-shadow: 0 1px 4px rgba(0,0,0,.2);
    }
    input:checked + .slider { background: #4caf50; }
    input:checked + .slider:before { transform: translateX(26px); }
    /* Indicateur état */
    .indicator-row { display: flex; align-items: center; gap: 12px; }
    .ind-dot {
      width: 14px; height: 14px;
      border-radius: 50%;
      background: #ccc;
      flex-shrink: 0;
      transition: background .3s;
    }
    .ind-dot.on  { background: #4caf50; box-shadow: 0 0 6px #4caf5088; }
    .ind-dot.off { background: #ef5350; }
    .ind-text { font-size: 15px; font-weight: 600; color: #333; }
    .ind-sub { font-size: 12px; color: #aaa; }
    /* OTA */
    .status-box {
      background: #f8f9ff;
      border: 1px solid #e0e4ff;
      border-radius: 8px;
      padding: 14px;
      margin-bottom: 14px;
      font-size: 14px;
      color: #333;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #ccc;
      flex-shrink: 0;
    }
    .dot.active  { background: #4caf50; }
    .dot.working { background: #ff9800; animation: blink 1s infinite; }
    .dot.error   { background: #f44336; }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:.2} }
    .btn {
      width: 100%;
      padding: 13px;
      background: #3f51b5;
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 14px;
      cursor: pointer;
      transition: background .2s;
    }
    .btn:hover:not(:disabled) { background: #303f9f; }
    .btn:disabled { background: #9fa8da; cursor: not-allowed; }
    .note { font-size: 12px; color: #aaa; text-align: center; margin-top: 10px; }
    /* Alerte */
    .alert-box {
      display: none;
      background: #ffebee;
      border: 1px solid #f44336;
      border-radius: 8px;
      padding: 12px 16px;
      color: #c62828;
      font-size: 14px;
      font-weight: 600;
    }
    /* Programme */
    .prog-box {
      background: #f8fdf8;
      border: 1px solid #c8e6c9;
      border-radius: 8px;
      padding: 14px;
      font-size: 14px;
      color: #333;
      min-height: 48px;
    }
    .prog-box.empty { color: #aaa; font-style: italic; }
  </style>
</head>
<body>
<div class="page">

  <!-- En-tête -->
  <div class="card">
    <h1>Arrosage Automatique</h1>
    <p class="subtitle" id="ip">...</p>
  </div>

  <!-- Alerte critique -->
  <div class="alert-box" id="alert-banner">&#9888; <span id="alert-text"></span></div>

  <!-- Commande pompe -->
  <div class="card">
    <div class="card-title">Commande</div>
    <div class="pump-row">
      <div>
        <div class="pump-label">Pompe</div>
        <div class="pump-sub">Allumer / Eteindre manuellement</div>
      </div>
      <label class="toggle-wrap">
        <input type="checkbox" id="pump-toggle" onchange="togglePump(this)">
        <span class="slider"></span>
      </label>
    </div>
  </div>

  <!-- Etat reel pompe -->
  <div class="card">
    <div class="card-title">Etat reel</div>
    <div class="indicator-row">
      <div class="ind-dot" id="ind-dot"></div>
      <div>
        <div class="ind-text" id="ind-text">...</div>
        <div class="ind-sub">Retour de l'ESP32</div>
      </div>
    </div>
  </div>

  <!-- Programme -->
  <div class="card">
    <div class="card-title">Prochain arrosage programme</div>
    <div class="prog-box empty" id="prog-box">Chargement...</div>
  </div>

  <!-- OTA -->
  <div class="card">
    <div class="card-title">Mise a jour firmware (OTA)</div>
    <div class="info-row">
      <span class="label">Version</span>
      <span class="value" id="ver">...</span>
    </div>
    <div class="status-box" style="margin-top:14px;">
      <div class="dot" id="dot"></div>
      <span id="status-text">Chargement...</span>
    </div>
    <button class="btn" id="btn-update" onclick="lancerUpdate()">Verifier et mettre a jour</button>
    <p class="note" id="note">L'ESP32 redemarrera automatiquement apres la mise a jour.</p>
  </div>

</div>
<script>
  var polling = null;

  // ── OTA ──
  function setDot(state) {
    var d = document.getElementById('dot');
    d.className = 'dot';
    if (state === 'working') d.classList.add('working');
    else if (state === 'ok')  d.classList.add('active');
    else if (state === 'err') d.classList.add('error');
  }
  function updateOTA() {
    fetch('/status').then(function(r){ return r.text(); }).then(function(t){
      document.getElementById('status-text').textContent = t;
      var low = t.toLowerCase();
      if (low.indexOf('cours') !== -1 || low.indexOf('recherche') !== -1) {
        setDot('working'); document.getElementById('btn-update').disabled = true;
      } else if (low.indexOf('terminee') !== -1) {
        setDot('ok'); document.getElementById('note').textContent = 'Redemarrage en cours...'; stopPolling();
      } else if (low.indexOf('erreur') !== -1) {
        setDot('err'); document.getElementById('btn-update').disabled = false; stopPolling();
      } else {
        setDot(''); document.getElementById('btn-update').disabled = false;
      }
    }).catch(function(){});
    fetch('/version').then(function(r){ return r.text(); }).then(function(t){
      document.getElementById('ver').textContent = 'v' + t;
    }).catch(function(){});
  }
  function stopPolling() { if (polling) { clearInterval(polling); polling = null; } }
  function lancerUpdate() {
    document.getElementById('btn-update').disabled = true;
    setDot('working');
    document.getElementById('status-text').textContent = 'Demarrage...';
    fetch('/update').catch(function(){});
    polling = setInterval(updateOTA, 2000);
  }

  // ── Pompe ──
  function togglePump(cb) {
    var val = cb.checked ? '1' : '0';
    fetch('/pump', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'state=' + val
    }).catch(function(){});
  }
  function updatePump() {
    fetch('/pump').then(function(r){ return r.text(); }).then(function(t){
      var on = (t.trim() === '1');
      document.getElementById('pump-toggle').checked = on;
    }).catch(function(){});
    fetch('/pump-state').then(function(r){ return r.text(); }).then(function(t){
      var on = (t.trim() === '1');
      var dot = document.getElementById('ind-dot');
      var txt = document.getElementById('ind-text');
      dot.className = 'ind-dot ' + (on ? 'on' : 'off');
      txt.textContent = on ? 'Pompe ON — en fonctionnement' : 'Pompe OFF — arretee';
    }).catch(function(){});
  }

  // ── Programme ──
  function updateProg() {
    fetch('/programme').then(function(r){ return r.text(); }).then(function(t){
      var box = document.getElementById('prog-box');
      if (t && t.trim().length > 0 && t.trim() !== '--') {
        box.className = 'prog-box';
        box.textContent = t.trim();
      } else {
        box.className = 'prog-box empty';
        box.textContent = 'Aucun programme configure';
      }
    }).catch(function(){});
  }

  // ── Alerte ──
  function checkAlert() {
    fetch('/alert').then(function(r){ return r.text(); }).then(function(t){
      var banner = document.getElementById('alert-banner');
      if (t && t.trim().length > 0) {
        document.getElementById('alert-text').textContent = t.trim();
        banner.style.display = 'block';
      } else {
        banner.style.display = 'none';
      }
    }).catch(function(){});
  }

  // ── Init ──
  document.getElementById('ip').textContent = location.hostname;
  updateOTA(); updatePump(); updateProg(); checkAlert();
  setInterval(updateOTA,  8000);
  setInterval(updatePump, 3000);
  setInterval(updateProg, 10000);
  setInterval(checkAlert, 5000);
</script>
</body>
</html>
)rawliteral";


// ════════════════════════════════════════════════════════════
//                          SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  delay(1500);
  Serial.begin(115200);
  Serial.println("\n\n=== Arrosage Automatique v" FW_VERSION " ===");

  // ── ID MQTT unique (MAC ESP32) ──
  snprintf(_mqttClientId, sizeof(_mqttClientId), "plantsio_%08X", (uint32_t)ESP.getEfuseMac());
  Serial.printf("[MQTT] Client ID : %s\n", _mqttClientId);

  // ── GPIO ──
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relais actif-bas : HIGH = pompe OFF au démarrage

  // ── OLED ──
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERREUR] OLED non detecte !");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  oledPrint("== Arrosage Auto ==", "v" FW_VERSION, "Demarrage...");
  delay(1000);

  // ── WiFi ──
  bool wifiOk = connectWifi();
  if (!wifiOk) {
    startConfigPortal();
  }

  // ── mDNS (hostname fixe : plantsio-esp.local) ──
  if (MDNS.begin("plantsio-esp")) {
    Serial.println("[mDNS] Hostname : plantsio-esp.local");
  } else {
    Serial.println("[mDNS] Echec demarrage mDNS");
  }

  // ── Vérification mise à jour disponible au boot ──
  checkUpdateAvailable();

  // ── Serveur web OTA ──
  setupWebServer();
  server.begin();
  Serial.println("[WEB] Serveur HTTP demarre sur port 80");
  Serial.print("[WEB] Adresse : http://");
  Serial.println(WiFi.localIP());
  Serial.println("[WEB] Adresse mDNS : http://plantsio-esp.local");

  // ── MQTT ──
  mqtt.subscribe(&pumpFeed);
  mqtt.subscribe(&programmeFeed);
  connectAdafruitIO();
}


// ════════════════════════════════════════════════════════════
//                          LOOP
// ════════════════════════════════════════════════════════════
void loop() {

  // ── Traitement des requêtes HTTP (non bloquant) ──
  server.handleClient();

  // ── Déclenchement OTA si demandé via /update ──
  if (otaRequested) {
    otaRequested = false;
    checkAndUpdate();   // Bloquant pendant le téléchargement
    delay(5000);        // Laisser le résultat visible 5s sur l'écran
    return;             // Reprend le loop proprement
  }

  // ── Vérifier WiFi ──
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connexion perdue !");
    oledPrint("== Arrosage Auto ==", "WiFi perdu!", "Reconnexion...");
    delay(3000);
    ESP.restart();
    return;
  }

  // ── Maintenir connexion Adafruit IO ──
  if (!mqtt.connected()) {
    Serial.printf("[AIO] Perte de connexion MQTT (status WiFi=%d). Reconnexion...\n", WiFi.status());
    aioConnected = false;
    connectAdafruitIO();
  } else {
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(5000))) {
      if (subscription == &pumpFeed) {
        char* pValue = (char *)pumpFeed.lastread;
        String value = String(pValue);
        Serial.println("\n-----------------------------------------");
        Serial.print("[AIO] MESSAGE RECU : [");
        Serial.print(value);
        Serial.println("]");
        Serial.println("-----------------------------------------");
        if (value == "1" || value == "ON" || value == "on") {
          setPump(true);
        } else {
          setPump(false);
        }
      } else if (subscription == &programmeFeed) {
        String prog = String((char*)programmeFeed.lastread);
        prog.trim();
        Serial.print("[PROG] Recu: "); Serial.println(prog);
        if (prog == "0" || prog.length() == 0) {
          hasProgram = false;
        } else {
          int sp1 = prog.indexOf(' ');
          int sp2 = prog.indexOf(' ', sp1 + 1);
          if (sp1 > 0 && sp2 > sp1) {
            prog.substring(0, sp1).toCharArray(nextWaterDate, sizeof(nextWaterDate));
            prog.substring(sp1 + 1, sp2).toCharArray(nextWaterTime, sizeof(nextWaterTime));
            prog.substring(sp2 + 1).toCharArray(nextWaterDur, sizeof(nextWaterDur));
            hasProgram = true;
          } else {
            hasProgram = false;
          }
        }
      }
    }
    if (!mqtt.ping()) {
      mqtt.disconnect();
    }
  }

  // ── Resynchronisation NTP (toutes les 6h) ──
  if (WiFi.status() == WL_CONNECTED && aioConnected) {
    if (!timeSynced || (millis() - lastTimeSyncMs > 21600000UL)) {
      syncTimeNow();
    }
  }

  // ── OLED ──
  updateStatusScreen();
}


// ════════════════════════════════════════════════════════════
//                   SERVEUR WEB — ROUTES
// ════════════════════════════════════════════════════════════

void setupWebServer() {
  // Page principale
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", HTML_PAGE);
  });

  // Lance la vérification/mise à jour
  server.on("/update", HTTP_GET, []() {
    if (otaState == OTA_CHECKING || otaState == OTA_DOWNLOADING) {
      server.send(200, "text/plain", "Mise a jour deja en cours...");
      return;
    }
    otaRequested = true;
    server.send(200, "text/plain", "Verification demarree");
  });

  // Retourne l'état OTA actuel (texte brut)
  server.on("/status", HTTP_GET, []() {
    server.send(200, "text/plain; charset=utf-8", otaMessage);
  });

  // Retourne la version locale
  server.on("/version", HTTP_GET, []() {
    server.send(200, "text/plain", FW_VERSION);
  });

  // Alerte système courante
  server.on("/alert", HTTP_GET, []() {
    server.send(200, "text/plain; charset=utf-8", systemAlert);
  });

  // Etat commande pompe (GET) + commande (POST)
  server.on("/pump", HTTP_GET, []() {
    server.send(200, "text/plain", pumpRunning ? "1" : "0");
  });
  server.on("/pump", HTTP_POST, []() {
    if (server.hasArg("state")) {
      String val = server.arg("state");
      bool newState = (val == "1" || val == "on" || val == "ON");
      setPump(newState);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Missing state");
    }
  });

  // Etat reel de la pompe (retour physique)
  server.on("/pump-state", HTTP_GET, []() {
    server.send(200, "text/plain", pumpRunning ? "1" : "0");
  });

  // Prochain programme
  server.on("/programme", HTTP_GET, []() {
    if (hasProgram) {
      String prog = String(nextWaterDate) + " a " + String(nextWaterTime);
      if (strlen(nextWaterDur) > 0) prog += " — " + String(nextWaterDur);
      server.send(200, "text/plain; charset=utf-8", prog);
    } else {
      server.send(200, "text/plain", "--");
    }
  });

  // 404
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
}


// ════════════════════════════════════════════════════════════
//                   ERREUR CRITIQUE
// ════════════════════════════════════════════════════════════

void criticalError(String msg) {
  Serial.println("[CRITIQUE] " + msg);
  systemAlert = msg;

  // Couper la pompe immédiatement (accès direct GPIO, sans MQTT)
  pumpRunning = false;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("[CRITIQUE] Pompe forcee OFF.");

  // Affichage OLED
  oledPrint("!! ERREUR CRITIQUE !!", msg.substring(0, 21), "Pompe: OFF", "Reboot dans 15s");

  // Tentative de publication sur Adafruit IO (best-effort)
  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
    alertFeed.publish(msg.c_str());
    stateFeed.publish("0");
    Serial.println("[CRITIQUE] Alerte publiee sur Adafruit IO.");
  } else {
    Serial.println("[CRITIQUE] Pas de connexion — alerte non publiee sur AIO.");
  }

  // Attente avant redémarrage (la page web reste accessible)
  delay(15000);
  Serial.println("[CRITIQUE] Redemarrage...");
  ESP.restart();
}


// ════════════════════════════════════════════════════════════
//                   LOGIQUE OTA
// ════════════════════════════════════════════════════════════

// Met à jour l'état OTA + Serial + OLED
void setOtaState(OtaState state, String detail) {
  otaState = state;
  switch (state) {
    case OTA_IDLE:
      otaMessage = "Inactif";
      break;
    case OTA_CHECKING:
      otaMessage = "Recherche de mise a jour...";
      oledPrint("== OTA ==", "Recherche...");
      break;
    case OTA_DOWNLOADING:
      otaMessage = "Mise a jour en cours...";
      oledPrint("== OTA ==", "Telechargement...");
      break;
    case OTA_SUCCESS:
      otaMessage = "Mise a jour terminee";
      oledPrint("== OTA ==", "Terminee!", "Redemarrage...");
      break;
    case OTA_NO_UPDATE:
      otaMessage = "Aucune mise a jour disponible";
      oledPrint("== OTA ==", "A jour !", "v" FW_VERSION);
      break;
    case OTA_ERROR:
      otaMessage = "Erreur: " + (detail.length() ? detail : "inconnue");
      oledPrint("== OTA ERREUR ==", detail);
      break;
  }
  Serial.println("[OTA] Etat : " + otaMessage);
}

// Télécharge version.txt depuis GitHub et retourne la version distante
String getRemoteVersion() {
  WiFiClientSecure secClient;
  secClient.setInsecure();  // Pas de vérification du certificat (pratique pour GitHub RAW)
  secClient.setTimeout(10);

  HTTPClient http;
  http.begin(secClient, OTA_VERSION_URL);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Erreur HTTP version.txt : %d\n", code);
    http.end();
    return "";
  }

  String ver = http.getString();
  http.end();
  ver.trim();
  return ver;
}

// Compare deux versions X.Y.Z — retourne true si remoteVer > localVer
bool isNewerVersion(const String& remoteVer, const String& localVer) {
  int rMaj = 0, rMin = 0, rPat = 0;
  int lMaj = 0, lMin = 0, lPat = 0;
  sscanf(remoteVer.c_str(), "%d.%d.%d", &rMaj, &rMin, &rPat);
  sscanf(localVer.c_str(), "%d.%d.%d", &lMaj, &lMin, &lPat);

  if (rMaj != lMaj) return rMaj > lMaj;
  if (rMin != lMin) return rMin > lMin;
  return rPat > lPat;
}

// Télécharge firmware.bin et l'écrit en flash via la lib Update
void performOTA() {
  setOtaState(OTA_DOWNLOADING);

  WiFiClientSecure secClient;
  secClient.setInsecure();
  secClient.setTimeout(OTA_TIMEOUT_MS / 1000);

  HTTPClient http;
  http.begin(secClient, OTA_FIRMWARE_URL);
  http.setTimeout(OTA_TIMEOUT_MS);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setOtaState(OTA_ERROR, "HTTP " + String(httpCode));
    http.end();
    return;
  }

  int contentLength = http.getSize();
  Serial.printf("[OTA] Taille firmware : %d bytes\n", contentLength);

  if (contentLength <= 0) {
    setOtaState(OTA_ERROR, "Taille inconnue");
    http.end();
    return;
  }

  if (!Update.begin(contentLength, U_FLASH)) {
    String err = Update.errorString();
    setOtaState(OTA_ERROR, err);
    http.end();
    return;
  }

  // Écriture en streaming (évite de charger le binaire entier en RAM)
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[512];
  int written = 0;
  unsigned long startMs = millis();

  while (http.connected() && written < contentLength) {
    if (millis() - startMs > OTA_TIMEOUT_MS) {
      Update.abort();
      setOtaState(OTA_ERROR, "Timeout");
      http.end();
      return;
    }
    int available = stream->available();
    if (available > 0) {
      int toRead = min(available, (int)sizeof(buf));
      int r = stream->readBytes(buf, toRead);
      int w = Update.write(buf, r);
      if (w != r) {
        Update.abort();
        setOtaState(OTA_ERROR, "Ecriture echouee");
        http.end();
        return;
      }
      written += w;

      // Progression toutes les 50 ko
      if (written % (50 * 1024) < sizeof(buf)) {
        Serial.printf("[OTA] %d / %d bytes (%.0f%%)\n",
          written, contentLength, 100.0f * written / contentLength);
      }
    }
    delay(1);
  }

  http.end();

  if (written != contentLength) {
    Update.abort();
    setOtaState(OTA_ERROR, "Transfert incomplet");
    return;
  }

  if (!Update.end(true)) {
    setOtaState(OTA_ERROR, Update.errorString());
    return;
  }

  setOtaState(OTA_SUCCESS);
  Serial.println("[OTA] Redemarrage dans 3 secondes...");
  delay(3000);
  ESP.restart();
}

// Point d'entrée OTA : vérifie la version puis lance la mise à jour si nécessaire
void checkAndUpdate() {
  setOtaState(OTA_CHECKING);

  String remoteVer = getRemoteVersion();
  if (remoteVer.length() == 0) {
    setOtaState(OTA_ERROR, "Impossible de lire version.txt");
    return;
  }

  Serial.printf("[OTA] Version locale : %s — Version distante : %s\n",
                FW_VERSION, remoteVer.c_str());

  if (!isNewerVersion(remoteVer, FW_VERSION)) {
    setOtaState(OTA_NO_UPDATE);
    oledPrint("== OTA ==", "A jour!", "Local:  v" FW_VERSION, "Dist:   v" + remoteVer);
    return;
  }

  Serial.printf("[OTA] Nouvelle version disponible : %s\n", remoteVer.c_str());
  performOTA();
}

// Animation OLED quand une mise à jour est détectée au boot
void showUpdateAvailableAnim(const String& remoteVer) {
  // ── Phase 1 : slide-in depuis le bas (8 frames) ──
  for (int y = 64; y >= 0; y -= 8) {
    display.clearDisplay();
    // Cadre arrondi simulé avec des rectangles
    display.drawRect(0, y, 128, 64, SSD1306_WHITE);
    display.fillRect(1, y + 1, 126, 62, SSD1306_BLACK);
    // Barre de titre pleine
    display.fillRect(1, y + 1, 126, 14, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(22, y + 4);
    display.print("MISE A JOUR DISPO");
    // Icône flèche download dessinée en pixels (centre-haut)
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(8, y + 20);
    display.print("Nouvelle version :");
    display.setCursor(22, y + 32);
    display.setTextSize(1);
    display.print("v" FW_VERSION "  ->  v" + remoteVer);
    display.setCursor(4, y + 46);
    display.print("Mettre a jour : site web");
    display.display();
    delay(30);
  }

  // ── Phase 2 : icône download animée (3x) ──
  String arrow[] = {" [      ] ", " [ >    ] ", " [ >>   ] ", " [ >>>  ] ", " [ >>>> ] " };
  for (int rep = 0; rep < 3; rep++) {
    for (int f = 0; f < 5; f++) {
      display.fillRect(1, 30, 126, 12, SSD1306_BLACK);
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(18, 33);
      display.print(arrow[f]);
      display.display();
      delay(120);
    }
  }

  // ── Phase 3 : affichage stable 4 secondes ──
  display.fillRect(1, 30, 126, 12, SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(22, 33);
  display.print("v" FW_VERSION "  ->  v" + remoteVer);
  display.display();
  delay(4000);
}

// Vérifie la version GitHub au boot sans bloquer longtemps
void checkUpdateAvailable() {
  Serial.println("[OTA] Verification de mise a jour au demarrage...");
  oledPrint("== Demarrage ==", "Verif MAJ...");
  String remoteVer = getRemoteVersion();
  if (remoteVer.length() == 0) {
    Serial.println("[OTA] Impossible de joindre GitHub (version.txt)");
    return;
  }
  Serial.printf("[OTA] Local: %s — Distant: %s\n", FW_VERSION, remoteVer.c_str());
  if (isNewerVersion(remoteVer, FW_VERSION)) {
    updateAvailable  = true;
    remoteVersionStr = remoteVer;
    Serial.printf("[OTA] Mise a jour disponible : v%s\n", remoteVer.c_str());
    showUpdateAvailableAnim(remoteVer);
  } else {
    Serial.println("[OTA] Firmware a jour.");
  }
}


// ════════════════════════════════════════════════════════════
//                   FONCTIONS WIFI
// ════════════════════════════════════════════════════════════

bool connectWifi() {
  Serial.printf("[WiFi] SSID cible : %s\n", WiFi.SSID().c_str());
  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
    Serial.printf("[WiFi] Tentative %d/%d...\n", attempt, WIFI_MAX_RETRIES);
    showWifiConnecting(attempt);
    WiFi.disconnect(true);
    delay(300);
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.begin();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_RETRY_DELAY) {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connecte ! SSID: %s | IP: %s | RSSI: %d dBm\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
      oledPrint("== Arrosage Auto ==", "WiFi: OK", WiFi.localIP().toString());
      delay(1500);
      return true;
    }
    Serial.printf("[WiFi] Tentative %d/%d echouee (status=%d)\n", attempt, WIFI_MAX_RETRIES, WiFi.status());
  }
  Serial.println("[WiFi] Toutes les tentatives echouees → mode config");
  return false;
}

void startConfigPortal() {
  Serial.println("[WiFiManager] Demarrage du portail de config...");
  showWifiConfigPortal();
  wifiManager.setAPStaticIPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );
  wifiManager.setConfigPortalTimeout(300);
  Serial.printf("[WiFiManager] AP SSID: %s | IP: 192.168.4.1 | Timeout: 300s\n", AP_SSID);
  bool configured = wifiManager.startConfigPortal(AP_SSID);
  if (configured) {
    Serial.println("[WiFiManager] Configuration reussie ! Redemarrage...");
    oledPrint("Config WiFi OK!", "Redemarrage...");
  } else {
    Serial.println("[WiFiManager] Timeout du portail. Redemarrage...");
    oledPrint("Timeout config", "Redemarrage...");
  }
  delay(2000);
  ESP.restart();
}


// ════════════════════════════════════════════════════════════
//                   ADAFRUIT IO
// ════════════════════════════════════════════════════════════

void connectAdafruitIO() {
  Serial.printf("[AIO] Connexion Adafruit IO (MQTT) — serveur: %s:%d — user: %s — clientId: %s\n",
                AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, _mqttClientId);

  for (int attempt = 1; attempt <= AIO_MAX_RETRIES; attempt++) {
    // Nettoyage propre avant chaque tentative
    mqtt.disconnect();
    delay(200);

    Serial.printf("[AIO] Tentative %d/%d...\n", attempt, AIO_MAX_RETRIES);
    oledPrint("== Connexion AIO ==", "Tentative " + String(attempt) + "/" + String(AIO_MAX_RETRIES), "Connexion...");

    int8_t ret = mqtt.connect();
    if (ret == 0) {
      Serial.printf("[AIO] Connecte avec succes a la tentative %d !\n", attempt);
      aioConnected = true;
      if (WiFi.status() == WL_CONNECTED) syncTimeNow();
      pumpGet.publish("\0");
      programmeGet.publish("\0");
      updateStatusScreen();
      return;
    }

    String errStr = String(mqtt.connectErrorString(ret));
    const char* errMsg = errStr.c_str();
    Serial.printf("[AIO] Echec tentative %d/%d (code=%d) : %s\n",
                  attempt, AIO_MAX_RETRIES, ret, errMsg);

    if (attempt < AIO_MAX_RETRIES) {
      // Décompte animé sur l'OLED
      for (int c = AIO_RETRY_DELAY_S; c > 0; c--) {
        showAioReconnecting(attempt, AIO_MAX_RETRIES, c, errMsg);
        delay(1000);
      }
    }
  }

  criticalError("Echec connexion AIO (" + String(AIO_MAX_RETRIES) + "/" + String(AIO_MAX_RETRIES) + ")");
}


// ════════════════════════════════════════════════════════════
//                   CONTRÔLE POMPE
// ════════════════════════════════════════════════════════════

void setPump(bool state) {
  bool previousState = pumpRunning;
  pumpRunning = state;
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);
  Serial.println(state ? "[POMPE] ON" : "[POMPE] OFF");

  if (aioConnected) {
    if (!stateFeed.publish(state ? "1" : "0")) {
      Serial.println("[AIO] Erreur publication pompe_etat !");
    }
  }

  if (previousState != state) {
    showPumpStatusChange(state);
  }
  updateStatusScreen();
}

bool syncTimeNow() {
  Serial.println("[TIME] Synchronisation NTP...");
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov", "time.google.com");
  struct tm timeInfo;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&timeInfo, 1200)) {
      timeSynced = true;
      lastTimeSyncMs = millis();
      Serial.println("[TIME] Heure synchronisee.");
      return true;
    }
    delay(200);
  }
  timeSynced = false;
  Serial.println("[TIME] Echec synchronisation NTP.");
  return false;
}

String getCurrentTimeString() {
  if (!timeSynced) return "--:--:--";
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 100)) {
    timeSynced = false;
    return "--:--:--";
  }
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeInfo);
  return String(buffer);
}


// ════════════════════════════════════════════════════════════
//                   AFFICHAGE OLED
// ════════════════════════════════════════════════════════════

void oledPrint(String line1, String line2, String line3, String line4) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 != "") display.println(line2);
  if (line3 != "") display.println(line3);
  if (line4 != "") display.println(line4);
  display.display();
}

void showPumpStatusChange(bool state) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(28, 0);
  display.print("STATUT POMPE");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(3);
  if (state) {
    display.fillRect(24, 14, 80, 28, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(28, 16);
    display.print(" ON ");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.drawRect(24, 14, 80, 28, SSD1306_WHITE);
    display.setCursor(28, 16);
    display.print(" OFF");
  }

  struct tm _ti;
  char _tbuf[6];
  strcpy(_tbuf, "--:--");
  if (timeSynced && getLocalTime(&_ti, 100)) {
    sprintf(_tbuf, "%02d:%02d", _ti.tm_hour, _ti.tm_min);
  }
  display.setTextSize(1);
  display.setCursor(43, 54);
  display.print("a ");
  display.print(_tbuf);
  display.display();
  delay(1800);
}

void showWifiConnecting(int attempt) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(13, 0);
  display.print("WiFi en cours...");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 15);
  display.print("Tentative ");
  display.print(attempt);
  display.print(" / ");
  display.print(WIFI_MAX_RETRIES);
  display.drawRect(0, 28, 128, 10, SSD1306_WHITE);
  int barFill = (128 * attempt) / WIFI_MAX_RETRIES;
  display.fillRect(0, 28, barFill, 10, SSD1306_WHITE);
  display.setCursor(20, 46);
  display.print("Connexion");
  for (int i = 0; i < attempt; i++) display.print(".");
  display.display();
}

void showWifiConfigPortal() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 0);
  display.print("! WiFi impossible !");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 14);
  display.print("SSID: ");
  display.print(AP_SSID);
  display.setCursor(0, 26);
  display.print("MDP:  aucun");
  display.setCursor(0, 38);
  display.print("IP:   192.168.4.1");
  display.setCursor(0, 52);
  display.print("Ouvre ton navigateur");
  display.display();
}

// ─────────────────────────────────────────────────────────────
//  Écran animé de reconnexion AIO
//  Appelé 1x par seconde pendant le délai entre deux tentatives
// ─────────────────────────────────────────────────────────────
void showAioReconnecting(int attempt, int maxAttempts, int countdown, const char* errorMsg) {
  static uint8_t dotStep = 0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ── Titre ──
  display.setCursor(16, 0);
  display.print("== Connexion AIO ==");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // ── Tentative X / N ──
  display.setCursor(0, 13);
  display.print("Tentative ");
  display.print(attempt);
  display.print(" / ");
  display.print(maxAttempts);

  // ── Message d'erreur court (tronqué à 21 chars) ──
  if (errorMsg != nullptr) {
    display.setCursor(0, 23);
    char errBuf[22];
    strncpy(errBuf, errorMsg, 21);
    errBuf[21] = '\0';
    display.print(errBuf);
  }

  // ── Décompte ──
  display.setCursor(0, 34);
  display.print("Reessai dans ");
  display.print(countdown);
  display.print("s");

  // ── Barre de progression du décompte ──
  // La barre se remplit au fur et à mesure que le délai s'écoule
  int elapsed = AIO_RETRY_DELAY_S - countdown;
  int barFill = (128 * elapsed) / AIO_RETRY_DELAY_S;
  display.drawRect(0, 44, 128, 8, SSD1306_WHITE);
  if (barFill > 0) display.fillRect(0, 44, barFill, 8, SSD1306_WHITE);

  // ── Points animés ──
  dotStep = (dotStep + 1) % 4;
  display.setCursor(0, 55);
  display.print("AIO");
  for (uint8_t i = 0; i < dotStep; i++) display.print(".");

  display.display();
}

void updateStatusScreen() {
  static const char* moisFR[] = {
    "Janvier", "Fevrier", "Mars",    "Avril",   "Mai",      "Juin",
    "Juillet", "Aout",   "Septembre","Octobre", "Novembre", "Decembre"
  };

  struct tm timeInfo;
  bool hasTime = timeSynced && getLocalTime(&timeInfo, 100);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  if (hasTime) {
    char dateBuf[24];
    sprintf(dateBuf, "%d %s %d",
      timeInfo.tm_mday, moisFR[timeInfo.tm_mon], 1900 + timeInfo.tm_year);
    int dateW = strlen(dateBuf) * 6;
    display.setCursor((128 - dateW) / 2, 1);
    display.print(dateBuf);
  } else {
    display.setCursor(5, 1);
    display.print("Synchro en cours...");
  }

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setTextSize(3);
  char timeBuf[6];
  if (hasTime) {
    sprintf(timeBuf, "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  } else {
    strcpy(timeBuf, "--:--");
  }
  display.setCursor(19, 15);
  display.print(timeBuf);

  display.drawLine(0, 43, 127, 43, SSD1306_WHITE);

  display.setTextSize(1);

  // Si mise à jour disponible : alterner toutes les 3s entre le programme et le bandeau MAJ
  bool showUpdateBanner = updateAvailable && otaState == OTA_IDLE && (millis() / 3000) % 2 == 1;

  if (showUpdateBanner) {
    // Bandeau pro : fond blanc inversé
    display.fillRect(0, 44, 128, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, 46);
    display.print("  MAJ  v" FW_VERSION "->v" + remoteVersionStr);
    display.setCursor(14, 56);
    display.print("Mettre a jour : web");
    display.setTextColor(SSD1306_WHITE);
  } else if (!hasProgram) {
    display.setCursor(19, 52);
    display.print("Aucun programme");
  } else {
    display.setCursor(1, 45);
    display.print("Prochain arrosage:");
    display.setCursor(1, 55);
    display.print(nextWaterDate);
    display.print(" a ");
    display.print(nextWaterTime);
    if (nextWaterDur[0] != '\0') {
      display.print(" ");
      display.print(nextWaterDur);
    }
  }

  display.display();
}
