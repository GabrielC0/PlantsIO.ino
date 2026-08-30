// ============================================================
//   PlantsIO — Arrosage automatique connecté
//   ESP32 + Relais + OLED + Adafruit IO + OTA GitHub
//
//   Cahier des charges : CAHIER_DES_CHARGES.md (v1.0, 2026-05-08)
//   Ce fichier porte le setup(), loop() et l'état global partagé.
//   Chaque service S1..S10 est implémenté dans un onglet dédié.
//
//     S1 pump.ino       S6 ota.ino
//     S2 wifi.ino       S7 oled.ino
//     S3 mqtt.ino       S8 web.ino
//     S4 time_sync.ino  S9 logs.ino
//     S5 mqtt.ino       S10 pump.ino (criticalError)
//
//   ⚠ REGLAGE DE CARTE OBLIGATOIRE
//   Outils > Partition Scheme > "No FS 4MB (2MB APP x2)"
//   Le magasin de certificats racine (SEC-07) ne tient pas dans le schema par
//   defaut (1,2 Mo par slot). Ce reglage n'etant pas modifiable par OTA, le
//   passage a cette version se fait par un flash USB ; les OTA suivantes
//   redeviennent normales.
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
#include <esp_task_wdt.h>
#include <esp_wifi.h>   // esp_wifi_get_config() (wifi.ino)
#include "secrets.h"

// ─────────────────────────────────────────
//   Identité firmware
// ─────────────────────────────────────────
#define FW_VERSION       "1.1.0"
#define HOSTNAME_MDNS    "plantsio-esp"

// ─────────────────────────────────────────
//   Debug
// ─────────────────────────────────────────
#define DEBUG_VERBOSE        0           // 1 = trace web + OLED en plus
// 1 = joue l'auto-test des securites au boot (cf. selftest.ino) et publie le
// resultat sur le moniteur serie. A laisser a 0 en production.
// Doit rester ici : les .ino sont concatenes avec le sketch principal en tete,
// un #define pose dans selftest.ino serait vu trop tard par le #if de setup().
#define PLANTSIO_SELFTEST    0
#define HEARTBEAT_PERIOD_MS  30000UL     // log periodique d'etat systeme

#if DEBUG_VERBOSE
  #define DBG(fmt, ...) wlog("[DBG] " fmt, ##__VA_ARGS__)
#else
  #define DBG(fmt, ...) ((void)0)
#endif

// ─────────────────────────────────────────
//   Cibles cloud (cf. §10.2, SEC-01)
// ─────────────────────────────────────────
#define AIO_SERVER       "io.adafruit.com"
#define AIO_SERVERPORT   8883            // SEC-01 : MQTT/TLS

// ─────────────────────────────────────────
//   Pinout & écran (cf. §9.1)
// ─────────────────────────────────────────
#define RELAY_PIN        26              // actif au niveau bas
#define I2C_SDA          21
#define I2C_SCL          22
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
#define OLED_RESET       -1
#define OLED_ADDRESS     0x3C

// ─────────────────────────────────────────
//   WiFi (cf. §7.2)
// ─────────────────────────────────────────
#define WIFI_MAX_RETRIES     5            // EF-201
#define WIFI_RETRY_DELAY     12000
#define WIFI_LOSS_REBOOT_MS  180000UL     // EF-205 : 3 min sans WiFi -> reboot
#define WIFI_NUDGE_MS        30000UL      // toutes les 30 s, secoue auto-reconnect
#define AP_SSID              "Arrosage-Setup"
#define PORTAL_TIMEOUT_S     300          // EF-207

// ─────────────────────────────────────────
//   Watchdog (recover from deadlocks)
// ─────────────────────────────────────────
#define WDT_TIMEOUT_S        90           // marge large : OTA 60 s + slack
// Le boot est legitimement long (5 essais WiFi + handshake TLS AIO). On arme
// quand meme le chien de garde des la 1re instruction, avec une laisse plus
// longue : un blocage au boot doit rebooter, pas figer l'appareil indefiniment.
#define WDT_BOOT_TIMEOUT_S   300

// ─────────────────────────────────────────
//   Sécurités locales (indépendantes du réseau)
// ─────────────────────────────────────────
// Durée d'arrosage max : au-delà, arrêt inconditionnel (anti-inondation).
// À calibrer selon le débit réel de ta pompe et le volume du pot.
#define PUMP_MAX_ON_MS       900000UL     // 15 min
// Seuil de heap sous lequel le firmware n'est plus fiable (2 mesures de suite).
#define HEAP_CRITICAL_BYTES  15000
// Nombre d'erreurs critiques consecutives avant de basculer en mode degrade au
// lieu de reboucler sur des redemarrages sans fin.
#define CRIT_BOOT_LIMIT      3
// Uptime sain au-dela duquel on considere l'incident passe (compteur remis a 0).
#define CRIT_BOOT_CLEAR_MS   300000UL

// ─────────────────────────────────────────
//   AIO / MQTT (cf. §7.3)
// ─────────────────────────────────────────
#define AIO_MAX_RETRIES      5            // EF-306
#define AIO_RETRY_DELAY_S    2
#define AIO_PING_INTERVAL_MS 30000UL      // EF-308

// ─────────────────────────────────────────
//   OTA (cf. §7.6)
// ─────────────────────────────────────────
#define OTA_TIMEOUT_MS       60000        // EF-608

// ─────────────────────────────────────────
//   Boot screen (cf. Annexe C)
// ─────────────────────────────────────────
enum BootStepState : uint8_t {
  STEP_PENDING = 0, STEP_RUNNING, STEP_OK, STEP_ERROR, STEP_WARN
};
struct BootStep {
  const char*   label;
  BootStepState state;
  char          detail[22];
};
BootStep bootSteps[4] = {
  { "WiFi", STEP_PENDING, "" },  // 0
  { "NTP ", STEP_PENDING, "" },  // 1
  { "MAJ ", STEP_PENDING, "" },  // 2
  { "AIO ", STEP_PENDING, "" },  // 3
};

// ─────────────────────────────────────────
//   État OTA (cf. §7.6)
// ─────────────────────────────────────────
enum OtaState {
  OTA_IDLE, OTA_CHECKING, OTA_DOWNLOADING,
  OTA_SUCCESS, OTA_NO_UPDATE, OTA_ERROR
};

// ─────────────────────────────────────────
//   Etat persistant (RTC RAM)
// ─────────────────────────────────────────
// Survit a un redemarrage logiciel (WDT, panique, criticalError, reboot perte
// WiFi) mais PAS a une coupure d'alimentation : debrancher l'appareil est une
// intervention humaine, elle a le droit de tout remettre a zero.
// Sans ca le compteur anti-inondation repartait de zero a chaque reboot : une
// boucle de reboot toutes les 3 min = arrosage permanent, le garde-fou des
// 15 min n'etant jamais atteint.
#define RTC_STATE_MAGIC  0x504C4E54UL     // "PLNT" — detecte la RAM non initialisee

RTC_NOINIT_ATTR uint32_t rtcMagic;
RTC_NOINIT_ATTR uint32_t rtcPumpUsedMs;    // ms de pompe ON cumules, tous reboots confondus
RTC_NOINIT_ATTR uint8_t  rtcPumpLockout;   // 1 = tout ON refuse jusqu'a un OFF explicite
RTC_NOINIT_ATTR uint8_t  rtcCritBoots;     // erreurs critiques consecutives

// Mode degrade : plus de MQTT ni d'OTA (les allocations TLS sont la premiere
// cause de heap epuise), IHM web et commande locale de la pompe conservees.
bool degradedMode = false;

// ─────────────────────────────────────────
//   Objets globaux
// ─────────────────────────────────────────
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiManager      wifiManager;
WebServer        server(80);

// EF-302 : client ID MQTT unique par puce
char _mqttClientId[40] = "plantsio_esp32";
WiFiClientSecure mqttClient;
Adafruit_MQTT_Client mqtt(&mqttClient, AIO_SERVER, AIO_SERVERPORT,
                          _mqttClientId, IO_USERNAME, IO_KEY);

// SEC-07 : magasin de racines Mozilla embarque dans le core ESP32 (~68 ko en
// flash). Seule la racine correspondant au serveur est parsee au moment du
// handshake — cout RAM minimal, et aucun certificat code en dur a maintenir
// quand une autorite tourne ses cles.
extern const uint8_t rootCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t rootCaBundleEnd[]   asm("_binary_x509_crt_bundle_end");

void secureClientInit(WiFiClientSecure& cli) {
  cli.setCACertBundle(rootCaBundleStart,
                      (size_t)(rootCaBundleEnd - rootCaBundleStart));
}

// Feeds AIO (cf. §10.4)
Adafruit_MQTT_Subscribe pumpFeed      = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/pompe");
// Ecriture sur le feed de commande : sert a effacer la valeur retenue quand la
// pompe est coupee localement (sinon le /get de la reconnexion la relance).
Adafruit_MQTT_Publish   pumpCmd       = Adafruit_MQTT_Publish  (&mqtt, IO_USERNAME "/feeds/pompe");
Adafruit_MQTT_Publish   pumpGet       = Adafruit_MQTT_Publish  (&mqtt, IO_USERNAME "/feeds/pompe/get");
Adafruit_MQTT_Publish   stateFeed     = Adafruit_MQTT_Publish  (&mqtt, IO_USERNAME "/feeds/pompe_etat");
Adafruit_MQTT_Subscribe programmeFeed = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/programme");
Adafruit_MQTT_Publish   programmeGet  = Adafruit_MQTT_Publish  (&mqtt, IO_USERNAME "/feeds/programme/get");
Adafruit_MQTT_Publish   alertFeed     = Adafruit_MQTT_Publish  (&mqtt, IO_USERNAME "/feeds/alerte");

// ─────────────────────────────────────────
//   État global (partagé entre onglets)
// ─────────────────────────────────────────
// S1 — pompe
bool          pumpRunning      = false;
bool          pumpChangeActive = false;
bool          pumpChangeState  = false;
unsigned long pumpChangeMs     = 0;
unsigned long pumpBudgetMs     = 0;      // duree max du cycle en cours (fixee au ON)

// S3 — AIO
bool aioConnected = false;

// S4 — NTP
bool          timeSynced     = false;
unsigned long lastTimeSyncMs = 0;

// S5 — programme d'arrosage
bool hasProgram          = false;
char nextWaterDate[8]    = "--/--";
char nextWaterTime[6]    = "--:--";
char nextWaterDur[8]     = "";

// S6 — OTA
OtaState otaState                = OTA_IDLE;
char     otaMessage[80]          = "Inactif";
bool     otaRequested            = false;
bool     updateAvailable         = false;
char     remoteVersionStr[16]    = "";

// S2 — reset WiFi demandé via /wifi-reset (traité dans loop pour laisser
// la réponse HTTP partir avant le redémarrage)
bool     wifiResetRequested      = false;

// S2 — suivi non bloquant de la perte WiFi (alimente handleWifiLoss)
// volatile : écrit depuis le task event WiFi, lu depuis loopTask
volatile unsigned long wifiLostAt        = 0;     // 0 = connecte, sinon millis() de la coupure
volatile bool          wifiReconnectFlag = false; // true sur GOT_IP apres une perte (declenche reannonce mDNS)

// S10 — alertes
char systemAlert[80] = "";

// ─────────────────────────────────────────
//   Prototypes (visibles dans tous les onglets)
// ─────────────────────────────────────────
// S9 — logs
void   wlog(const char* fmt, ...);
void   wlogStream();
// TLS — verification de la chaine de certificats (SEC-07)
void   secureClientInit(WiFiClientSecure& cli);
// S1/S10 — pompe & alerte
// userCommand : ordre explicite d'un humain (cloud ou web). Seul un OFF de cette
//   origine rearme le budget anti-inondation ; un OFF de securite ne l'acquitte pas.
// fromCloud : ordre venu du feed pompe. Un tel arrosage correspond au programme
//   en cours, sa duree annoncee sert donc de borne. Un ON local depuis l'IHM web
//   n'est pas concerne : il garde le plafond generique PUMP_MAX_ON_MS.
void   setPump(bool state, bool userCommand = false, bool fromCloud = false);
void   servicePumpTimeout();
void   criticalError(const char* msg);
unsigned long programmeDurationMs();
// S2 — wifi
bool   connectWifi();
void   handleWifiLoss();
void   startConfigPortal();
// S3/S5 — mqtt + programme
void   connectAdafruitIO();
void   serviceMqtt();
void   parseProgramme(const char* msg);
// S4 — time
bool   syncTimeNow();
// S6 — ota
String getRemoteVersion();
bool   isNewerVersion(const String& remote, const String& local);
void   checkUpdateAvailable();
void   checkAndUpdate();
void   performOTA();
// S7 — oled
void   oledPrint(String l1, String l2 = "", String l3 = "", String l4 = "");
void   renderBootScreen();
void   setBootStep(int idx, BootStepState state, const char* detail = "");
void   updateStatusScreen();
void   showWifiLost();
void   showWifiConfigPortal();
void   showAioReconnecting(int attempt, int maxAttempts, int countdown, const char* errorMsg);
void   showCriticalAlert(const char* msg);
// S8 — web
void   setupWebServer();


// ════════════════════════════════════════════════════════════
//                            SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  // EF-102 : pompe garantie OFF AVANT tout le reste. Ces deux lignes doivent
  // rester les toutes premieres du programme : jusqu'a leur execution, GPIO26
  // est une entree flottante et un relais actif-bas peut se fermer tout seul.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // Chien de garde arme immediatement (laisse longue le temps du boot) : un
  // blocage pendant la connexion WiFi ou le handshake TLS doit rebooter.
  esp_task_wdt_config_t wdtBootCfg = {
    .timeout_ms     = (uint32_t)(WDT_BOOT_TIMEOUT_S * 1000),
    .idle_core_mask = 0,
    .trigger_panic  = true,
  };
  if (esp_task_wdt_init(&wdtBootCfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtBootCfg);
  }
  esp_task_wdt_add(NULL);

  // Etat persistant : sur un power-on la RTC RAM contient du bruit, on la reset.
  if (rtcMagic != RTC_STATE_MAGIC) {
    rtcMagic       = RTC_STATE_MAGIC;
    rtcPumpUsedMs  = 0;
    rtcPumpLockout = 0;
    rtcCritBoots   = 0;
  }
  degradedMode = (rtcCritBoots >= CRIT_BOOT_LIMIT);

  unsigned long bootStartMs = millis();
  delay(1500);
  Serial.begin(115200);
  Serial.println("\n\n=== PlantsIO v" FW_VERSION " ===");
  wlog("[BOOT] Demarrage PlantsIO v%s", FW_VERSION);
  wlog("[BOOT] Puce: %s rev=%d cores=%d freq=%dMHz flash=%uKB heap=%u",
       ESP.getChipModel(),
       (int)ESP.getChipRevision(),
       (int)ESP.getChipCores(),
       (int)ESP.getCpuFreqMHz(),
       (unsigned)(ESP.getFlashChipSize() / 1024),
       (unsigned)ESP.getFreeHeap());

  // EF-903 : tracer la cause du dernier redémarrage
  {
    esp_reset_reason_t r = esp_reset_reason();
    const char* label;
    switch (r) {
      case ESP_RST_POWERON:   label = "Alimentation"; break;
      case ESP_RST_EXT:       label = "Reset ext"; break;
      case ESP_RST_SW:        label = "Logiciel"; break;
      case ESP_RST_PANIC:     label = "Panique/crash"; break;
      case ESP_RST_INT_WDT:   label = "WDT interruption"; break;
      case ESP_RST_TASK_WDT:  label = "WDT tache"; break;
      case ESP_RST_WDT:       label = "WDT"; break;
      case ESP_RST_DEEPSLEEP: label = "Reveil profond"; break;
      case ESP_RST_BROWNOUT:  label = "BROWNOUT"; break;
      case ESP_RST_SDIO:      label = "SDIO"; break;
      default:                label = "Inconnu"; break;
    }
    wlog("[BOOT] Cause redemarrage : %s (code %d)", label, (int)r);
  }

  // EF-302 : client ID unique dérivé du MAC
  snprintf(_mqttClientId, sizeof(_mqttClientId),
           "plantsio_%08lX", (unsigned long)ESP.getEfuseMac());
  wlog("[BOOT] MQTT Client ID : %s", _mqttClientId);
  wlog("[BOOT] Etat persistant : pompe_utilisee=%lus verrou=%d crit_boots=%d",
       (unsigned long)(rtcPumpUsedMs / 1000UL),
       (int)rtcPumpLockout, (int)rtcCritBoots);

  if (rtcPumpLockout) {
    // Le verrou a survecu au reboot : la valeur retenue "1" du feed pompe ne
    // pourra pas relancer l'arrosage tant qu'un OFF explicite n'est pas recu.
    snprintf(systemAlert, sizeof(systemAlert),
             "Verrou securite actif : arrosage bloque jusqu'a un ordre OFF");
    wlog("[BOOT] %s", systemAlert);
  }

  if (degradedMode) {
    // S10 : trop d'erreurs critiques d'affilee. Rebooter une 4e fois ne reglera
    // rien — on demarre sans cloud ni OTA pour rester joignable et diagnosticable.
    snprintf(systemAlert, sizeof(systemAlert),
             "Mode degrade : %d erreurs critiques, cloud et OTA desactives",
             (int)rtcCritBoots);
    wlog("[BOOT] %s", systemAlert);
  }

  // OLED — non bloquant si absent (§12.2)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);   // ~21 ms par display.display() (vs ~85 ms à 100 kHz)
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    wlog("[OLED] Non detecte (boot continue)");
  }
  renderBootScreen();
  delay(400);

  // ── Étape 0 : WiFi (S2) ──
  setBootStep(0, STEP_RUNNING, "Connexion...");
  if (!connectWifi()) {
    setBootStep(0, STEP_ERROR, "Echec - mode AP");
    startConfigPortal();          // EF-202..EF-204 : redémarre lui-même
  } else {
    char ipBuf[22];
    snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
    setBootStep(0, STEP_OK, ipBuf);
  }

  // EF-206 : nom .local + service HTTP annonce
  if (MDNS.begin(HOSTNAME_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    wlog("[mDNS] %s.local OK (service http:80 annonce)", HOSTNAME_MDNS);
  } else {
    wlog("[mDNS] Echec begin (conflit nom ou stack pas prete)");
  }

  // ── Étape 1 : NTP (S4) ──
  setBootStep(1, STEP_RUNNING, "Synchro NTP...");
  if (syncTimeNow()) {
    char tbuf[22];
    struct tm ti;
    if (getLocalTime(&ti, 100)) {
      snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d",
               ti.tm_hour, ti.tm_min, ti.tm_sec);
      setBootStep(1, STEP_OK, tbuf);
    } else {
      setBootStep(1, STEP_OK, "");
    }
  } else {
    setBootStep(1, STEP_WARN, "NTP inaccessible");   // EF-404 : non bloquant
  }

  // ── Étape 2 : Vérification MAJ (S6) ──
  if (degradedMode) {
    setBootStep(2, STEP_WARN, "Mode degrade");
  } else {
    setBootStep(2, STEP_RUNNING, "GitHub...");
    checkUpdateAvailable();   // alimente bootSteps[2] et updateAvailable
  }

  // Serveur web (S8) — démarré tôt pour rester accessible
  setupWebServer();
  server.begin();
  wlog("[WEB] http://%s | http://%s.local",
       WiFi.localIP().toString().c_str(), HOSTNAME_MDNS);

  // ── Étape 3 : AIO/MQTT (S3) ──
  if (degradedMode) {
    setBootStep(3, STEP_WARN, "Mode degrade");
  } else {
    setBootStep(3, STEP_RUNNING, "io.adafruit.com");
    mqtt.subscribe(&pumpFeed);
    mqtt.subscribe(&programmeFeed);
    connectAdafruitIO();
    setBootStep(3, aioConnected ? STEP_OK : STEP_ERROR,
                aioConnected ? "Connecte!" : "Hors ligne");
  }

  renderBootScreen();
  delay(2000);

  wlog("[BOOT] Termine en %lums | heap=%u/%u | wifi=%d mqtt=%d ntp=%d maj=%d",
       millis() - bootStartMs,
       (unsigned)ESP.getFreeHeap(),
       (unsigned)ESP.getHeapSize(),
       WiFi.status() == WL_CONNECTED ? 1 : 0,
       aioConnected ? 1 : 0,
       timeSynced ? 1 : 0,
       updateAvailable ? 1 : 0);

  // Boot termine : on resserre la laisse du chien de garde. Il tourne depuis la
  // 1re instruction du setup, on ne fait que reduire son timeout ici.
  esp_task_wdt_config_t wdtCfg = {
    .timeout_ms     = (uint32_t)(WDT_TIMEOUT_S * 1000),
    .idle_core_mask = 0,
    .trigger_panic  = true,
  };
  esp_task_wdt_reconfigure(&wdtCfg);
  wlog("[WDT] Resserre a %ds (boot: %ds)", WDT_TIMEOUT_S, WDT_BOOT_TIMEOUT_S);

#if PLANTSIO_SELFTEST
  runSelfTest();   // cf. selftest.ino — desactive par defaut
#endif
}


// ════════════════════════════════════════════════════════════
//                            LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  // Watchdog : kick a chaque tour. Si une operation bloque > WDT_TIMEOUT_S, reboot auto.
  esp_task_wdt_reset();

  // Reannonce mDNS apres une reconnexion WiFi (IP potentiellement changee)
  if (wifiReconnectFlag) {
    wifiReconnectFlag = false;
    MDNS.end();
    if (MDNS.begin(HOSTNAME_MDNS)) {
      MDNS.addService("http", "tcp", 80);
      wlog("[mDNS] Reannonce apres reconnexion (IP=%s)",
           WiFi.localIP().toString().c_str());
    } else {
      wlog("[mDNS] Echec reannonce");
    }
  }

  // Heartbeat système — état lisible sur le moniteur série
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= HEARTBEAT_PERIOD_MS) {
    lastHeartbeat = millis();
    wlog("[STAT] up=%lus heap=%u/%u rssi=%ddBm wifi=%d mqtt=%d pump=%d prog=%d ota=%d verrou=%d degrade=%d",
         millis() / 1000UL,
         (unsigned)ESP.getFreeHeap(),
         (unsigned)ESP.getHeapSize(),
         (int)WiFi.RSSI(),
         WiFi.status() == WL_CONNECTED ? 1 : 0,
         aioConnected ? 1 : 0,
         pumpRunning ? 1 : 0,
         hasProgram ? 1 : 0,
         (int)otaState,
         (int)rtcPumpLockout,
         degradedMode ? 1 : 0);

    // Uptime sain : l'incident qui avait declenche les erreurs critiques est
    // derriere nous, on rearme le compteur (sans quoi 3 incidents espaces de
    // plusieurs semaines finiraient par bloquer l'appareil en mode degrade).
    if (rtcCritBoots && millis() >= CRIT_BOOT_CLEAR_MS) {
      wlog("[STAT] %ds d'uptime sain : compteur d'erreurs critiques remis a zero",
           (int)(CRIT_BOOT_CLEAR_MS / 1000UL));
      rtcCritBoots = 0;
    }

    // S10 — heap epuise : le firmware n'est plus fiable (allocations TLS/HTTP
    // qui echouent en silence). Deux mesures de suite pour ignorer un creux
    // transitoire pendant une requete web.
    static uint8_t lowHeapStreak = 0;
    if (ESP.getFreeHeap() < HEAP_CRITICAL_BYTES) {
      if (++lowHeapStreak >= 2) criticalError("Memoire insuffisante");
    } else {
      lowHeapStreak = 0;
    }
  }

  // S8 — non bloquant pour la pompe et l'OLED (EF-808)
  server.handleClient();

  // S1 — securite anti-inondation. Placee avant tout "return" du loop pour
  // rester active meme sans WiFi / sans MQTT (EF-103 : source interne).
  servicePumpTimeout();

  // S6 — MAJ déclenchée explicitement par /update (EF-604)
  if (otaRequested) {
    otaRequested = false;
    if (degradedMode) {
      // Un flash OTA a besoin de heap pour TLS : c'est exactement ce qui manque.
      wlog("[OTA] Refusee : mode degrade");
    } else {
      // OFF de securite : n'acquitte pas le verrou anti-inondation.
      setPump(false);          // EF-610 : pompe OFF avant download
      checkAndUpdate();
      delay(3000);
    }
    return;
  }

  // S2 — Reset WiFi demandé via /wifi-reset : on efface les credentials NVS
  // puis on redémarre pour relancer le portail captif au prochain boot.
  if (wifiResetRequested) {
    wifiResetRequested = false;
    wlog("[WiFi] Reset demande : effacement credentials et redemarrage");
    setPump(false);            // sécurité : pompe OFF avant toute interruption
                               // (OFF de securite : n'acquitte pas le verrou)
    oledPrint("Reset WiFi", "Redemarrage...");
    wifiManager.resetSettings();   // efface SSID/password persistes en NVS
    delay(800);
    ESP.restart();
  }

  // S2 — perte WiFi : reconnexion bornée puis redémarrage si échec (EF-205)
  if (WiFi.status() != WL_CONNECTED) {
    handleWifiLoss();
    return;
  }

  // S3 — maintenir MQTT, lire messages, ping périodique (EF-308, EF-309)
  // Coupe en mode degrade : les allocations TLS sont la premiere cause de heap
  // epuise, les relancer en boucle empecherait toute recuperation.
  if (!degradedMode) serviceMqtt();

  // S4 — resynchronisation horaire toutes les 6 h (EF-403)
  if (aioConnected && (!timeSynced ||
       millis() - lastTimeSyncMs > 21600000UL)) {
    syncTimeNow();
  }

  // S7 — affichage rafraîchi (>= 1 Hz, ENF-04)
  updateStatusScreen();
}
