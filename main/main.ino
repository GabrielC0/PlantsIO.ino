// ============================================================
//   🌱 Arrosage Automatique — ESP32 + WiFiManager + Adafruit IO
// ============================================================
//
//  Bibliothèques requises (Gestionnaire de bibliothèques) :
//    - WiFiManager       (tzapu)
//    - Adafruit MQTT Library
//    - Adafruit SSD1306
//    - Adafruit GFX Library
// ============================================================

#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ─────────────────────────────────────────
//   ⚠️  VOS IDENTIFIANTS ADAFRUIT IO
// ─────────────────────────────────────────
#define IO_USERNAME    ""
#define IO_KEY         ""

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883

// ─────────────────────────────────────────
//   Pins
// ─────────────────────────────────────────
#define RELAY_PIN     26
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

// ─────────────────────────────────────────
//   Constantes
// ─────────────────────────────────────────
#define WIFI_MAX_RETRIES  3       
#define WIFI_RETRY_DELAY  10000  
#define AP_SSID           "Arrosage-Setup"

// ─────────────────────────────────────────
//   Objets
// ─────────────────────────────────────────
Adafruit_SSD1306  display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WiFiManager       wifiManager;

// Création du client MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_USERNAME, IO_KEY);

// Feed de commande : reçoit ON/OFF depuis le dashboard
Adafruit_MQTT_Subscribe pumpFeed = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/pompe");
Adafruit_MQTT_Publish pumpGet = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/pompe/get");
Adafruit_MQTT_Publish stateFeed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/pompe_etat");

// ─────────────────────────────────────────
//   État global
// ─────────────────────────────────────────
bool pumpRunning  = false;
bool aioConnected = false;

// ─────────────────────────────────────────
//   Prototypes
// ─────────────────────────────────────────
void     oledPrint(String line1, String line2 = "", String line3 = "", String line4 = "");
bool     connectWifi();
void     startConfigPortal();
void     connectAdafruitIO();
void     setPump(bool state);
void     updateStatusScreen();


// ════════════════════════════════════════════════════════════
//                          SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  delay(1500); // Laisse le temps à l'ESP32 et à l'alimentation de se stabiliser
  Serial.begin(115200);
  Serial.println("\n\n=== Arrosage Automatique ===");

  // ── GPIO ──
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Pompe OFF au démarrage (sécurité)

  // ── OLED ──
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[ERREUR] OLED non détecté !");
    // On continue quand même, sans OLED
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  oledPrint("== Arrosage Auto ==", "Demarrage...");
  delay(1000);

  // ── Phase 2 : Connexion WiFi avec 3 tentatives ──
  bool wifiOk = connectWifi();

  // ── Phase 2b : Mode config AP si 3 échecs ──
  if (!wifiOk) {
    startConfigPortal();
    // startConfigPortal() est bloquante jusqu'à la config
    // Après config → l'ESP32 redémarre automatiquement
  }

  // Configuration MQTT : on s'abonne AU feed Pompe !
  mqtt.subscribe(&pumpFeed);
  
  // ── Phase 3 : Connexion Adafruit IO ──
  connectAdafruitIO();
}


// ════════════════════════════════════════════════════════════
//                          LOOP
// ════════════════════════════════════════════════════════════
void loop() {

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
      Serial.println("[AIO] Perte de connexion MQTT. Reconnexion...");
      aioConnected = false;
      connectAdafruitIO();
  } else {
      // ── Lecture des messages entrants (on bloque pnd 5000ms max) ──
      // C'est ici que l'ESP32 va VRAIMENT écouter les messages envoyés par le Dashboard
      Adafruit_MQTT_Subscribe *subscription;
      
      // On lit le flux avec un timeout de 5 secondes.
      // S'il y a une valeur en attente (quand on clique ou quand on vient de se connecter),
      // elle sera traitée ici.
      while ((subscription = mqtt.readSubscription(5000))) {
        if (subscription == &pumpFeed) {
          
          // Conversion de la charge utile en String
          char* pValue = (char *)pumpFeed.lastread;
          String value = String(pValue);
          
          Serial.println("\n-----------------------------------------");
          Serial.print("📥 [AIO] MESSAGE RECU DU DASHBOARD : [");
          Serial.print(value);
          Serial.println("]");
          Serial.println("-----------------------------------------");
        
          if (value == "1" || value == "ON" || value == "on") {
            setPump(true);
          } else {
            setPump(false);
          }
        }
      }
      
      // Ping obligatoire pour garder la connexion ouverte
      if(! mqtt.ping()) {
        mqtt.disconnect();
      }
  }

  // ── Mise à jour OLED ──
  updateStatusScreen();
}


// ════════════════════════════════════════════════════════════
//                   FONCTIONS WIFI
// ════════════════════════════════════════════════════════════

// Tente de se connecter au WiFi enregistré (3 essais max)
bool connectWifi() {
  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
    Serial.printf("[WiFi] Tentative %d/%d...\n", attempt, WIFI_MAX_RETRIES);

    oledPrint(
      "== Arrosage Auto ==",
      "WiFi: connexion...",
      "Tentative " + String(attempt) + "/" + String(WIFI_MAX_RETRIES)
    );

    WiFi.mode(WIFI_STA);
    WiFi.begin();  // Utilise les credentials sauvegardés en Flash

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_RETRY_DELAY) {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connecté ! IP : " + WiFi.localIP().toString());
      oledPrint(
        "== Arrosage Auto ==",
        "WiFi: OK",
        WiFi.localIP().toString()
      );
      delay(1500);
      return true;
    }

    Serial.printf("[WiFi] Tentative %d echouee.\n", attempt);
  }

  Serial.println("[WiFi] 3 tentatives echouees → mode config");
  return false;
}

// Lance le point d'accès WiFiManager pour configurer le WiFi
// Bloquant jusqu'à ce que l'utilisateur entre les credentials
void startConfigPortal() {
  Serial.println("[WiFiManager] Demarrage du portail de config...");

  oledPrint(
    "MODE CONFIG",
    "WiFi: " + String(AP_SSID),
    "IP: 192.168.4.1",
    "Ouvre navigateur"
  );

  // IP statique du portail
  wifiManager.setAPStaticIPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );

  // Timeout : si personne ne se connecte en 5 min → reboot
  wifiManager.setConfigPortalTimeout(300);

  // Lance le portail (bloquant)
  bool configured = wifiManager.startConfigPortal(AP_SSID);

  if (configured) {
    Serial.println("[WiFiManager] Config OK → redemarrage...");
    oledPrint("Config WiFi OK!", "Redemarrage...");
    delay(2000);
    ESP.restart();
  } else {
    Serial.println("[WiFiManager] Timeout → redemarrage...");
    oledPrint("Timeout config", "Redemarrage...");
    delay(2000);
    ESP.restart();
  }
}


// ════════════════════════════════════════════════════════════
//                   FONCTIONS ADAFRUIT IO
// ════════════════════════════════════════════════════════════

// Connexion au broker MQTT Adafruit IO (via librairie standard)
void connectAdafruitIO() {
  Serial.println("[AIO] Connexion Adafruit IO (MQTT)...");
  oledPrint(
    "== Arrosage Auto ==",
    "WiFi: OK",
    "Connexion AIO..."
  );

  int8_t ret;
  int retries = 5;

  // On boucle jusqu'à ce que l'on soit connecté
  while ((ret = mqtt.connect()) != 0) {
       Serial.print("[AIO] Echec: ");
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("[AIO] Nouvelle tentative dans 5 secondes...");
       
       oledPrint(
         "== AIO ERREUR ==",
         String(mqtt.connectErrorString(ret)),
         "Reessai dans 5s"
       );
       
       mqtt.disconnect();
       delay(5000);
       retries--;
       
       if (retries == 0) {
         Serial.println("❌ ECHEC DEFINITIF DE CONNEXION ❌");
         oledPrint("== AIO ERREUR ==", "Echec critique", "Prog. stope");
         while (1) { delay(1000); }
       }
  }

  Serial.println("[AIO] Connecté avec succès !");
  aioConnected = true;
  
  // 🔥 ASTUCE ADAFRUIT IO : on publie un octet nul sur le topic /get
  // pour forcer le serveur à nous renvoyer IMMÉDIATEMENT 
  // la dernière valeur du feed "pompe"
  Serial.println("[AIO] Demande de la dernière valeur...");
  pumpGet.publish("\0");

  updateStatusScreen();
}


// ════════════════════════════════════════════════════════════
//                   CONTRÔLE POMPE
// ════════════════════════════════════════════════════════════

void setPump(bool state) {
  pumpRunning = state;
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  Serial.println(state ? "[POMPE] ON  — Relais fermé" : "[POMPE] OFF — Relais ouvert");

  // Publier l'état réel vers Adafruit IO (feed "pompe_etat")
  // → visible sur le dashboard comme confirmation
  if (aioConnected) {
    if (stateFeed.publish(state ? "1" : "0")) {
      Serial.println("[AIO] pompe_etat publié : " + String(state ? "1" : "0"));
    } else {
      Serial.println("[AIO] Erreur publication pompe_etat !");
    }
  }

  // Mettre à jour l'écran immédiatement
  updateStatusScreen();
}


// ════════════════════════════════════════════════════════════
//                   AFFICHAGE OLED
// ════════════════════════════════════════════════════════════

// Affiche jusqu'à 4 lignes sur l'OLED
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

// Écran de statut normal (fonctionnement)
void updateStatusScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  // Ligne 1 : titre
  display.println("== Arrosage Auto ==");

  // Ligne 2 : WiFi
  display.print("WiFi: ");
  display.println(WiFi.status() == WL_CONNECTED ? "OK" : "NON");

  // Ligne 3 : Adafruit IO
  display.print("AIO:  ");
  display.println(aioConnected ? "OK" : "NON");

  // Ligne 4 : IP
  display.print("IP: ");
  display.println(WiFi.localIP().toString());

  // Ligne 5 : état pompe (grande taille)
  display.drawLine(0, 40, 127, 40, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print("Pompe:");
  display.println(pumpRunning ? " ON" : "OFF");

  display.display();
}