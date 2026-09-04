// ============================================================
//  S2 — Connectivité WiFi (cf. §7.2)
//  EF-201..EF-207, FC-01, FC-03
// ============================================================

// Trace des événements WiFi sur le moniteur série + mise à jour des flags
// partagés (wifiLostAt, wifiReconnectFlag, aioConnected).
static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      wlog("[WIFI-EVT] STA_START");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      wlog("[WIFI-EVT] CONNECTED ssid=%.32s ch=%d",
           (const char*)info.wifi_sta_connected.ssid,
           (int)info.wifi_sta_connected.channel);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      wlog("[WIFI-EVT] GOT_IP %s rssi=%ddBm",
           WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      // Si on sortait d'une perte, demander la reannonce mDNS dans loop().
      if (wifiLostAt != 0) wifiReconnectFlag = true;
      wifiLostAt = 0;
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      wlog("[WIFI-EVT] LOST_IP");
      aioConnected = false;
      if (wifiLostAt == 0) wifiLostAt = millis();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      wlog("[WIFI-EVT] DISCONNECTED reason=%d",
           (int)info.wifi_sta_disconnected.reason);
      aioConnected = false;  // evite des publish vers un client mort
      if (wifiLostAt == 0) wifiLostAt = millis();
      break;
    default:
      break;
  }
}

bool connectWifi() {
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);

  // Anti-crash tension : le core emet a la puissance maximale par defaut
  // (+19.5 dBm), soit des pics de ~300 mA sur le 3V3 qui, cumules a
  // l'enclenchement du relais, faisaient brownouter l'ESP32.
  WiFi.setTxPower(lowPowerMode ? WIFI_TX_POWER_LOW : WIFI_TX_POWER);
  // Modem sleep : le radio s'endort entre deux beacons, la conso moyenne chute.
  // MIN_MODEM respecte le DTIM, l'IHM web et mDNS restent reactifs.
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  wlog("[WiFi] TX=%d (unites de 0.25 dBm)%s, modem sleep actif",
       (int)WiFi.getTxPower(), lowPowerMode ? " [bas-conso]" : "");

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setHostname(HOSTNAME_MDNS);

  // Court-circuit : si la NVS ne contient aucun SSID (apres reset WiFi ou
  // premier boot), inutile d'attendre 5x12s pour rien -> portail captif direct.
  // NB : WiFi.SSID() retourne le SSID *connecte*, pas celui en NVS. On lit
  // directement la config IDF qui est chargee depuis NVS par esp_wifi_init().
  wifi_config_t storedCfg;
  memset(&storedCfg, 0, sizeof(storedCfg));
  esp_wifi_get_config(WIFI_IF_STA, &storedCfg);
  const char* storedSsid = (const char*)storedCfg.sta.ssid;
  if (storedSsid[0] == '\0') {
    wlog("[WiFi] Aucun SSID enregistre (NVS vide) -> portail captif");
    setBootStep(0, STEP_WARN, "Pas de credentials");
    return false;
  }

  wlog("[WiFi] Cible NVS : %s", storedSsid);

  // EF-201 : >= 5 tentatives, retour visuel par essai
  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
    char detail[22];
    snprintf(detail, sizeof(detail), "Essai %d/%d", attempt, WIFI_MAX_RETRIES);
    setBootStep(0, STEP_RUNNING, detail);

    WiFi.disconnect(false);
    delay(400);
    WiFi.begin();

    unsigned long start = millis();
    unsigned long lastAnim = 0;
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_RETRY_DELAY) {
      if (millis() - lastAnim >= 400) {
        lastAnim = millis();
        renderBootScreen();
      }
      delay(100);   // détection de connexion plus rapide
    }

    if (WiFi.status() == WL_CONNECTED) {
      wlog("[WiFi] OK SSID=%s IP=%s RSSI=%d",
           WiFi.SSID().c_str(),
           WiFi.localIP().toString().c_str(),
           WiFi.RSSI());
      wifiLostAt = 0;
      return true;
    }
    wlog("[WiFi] Tentative %d echouee (st=%d)", attempt, WiFi.status());
  }
  wlog("[WiFi] Toutes les tentatives epuisees");
  return false;
}

// EF-205 / ENF-02 : perte WiFi geree NON bloquante.
// L'ESP32 a deja setAutoReconnect(true), on lui laisse faire son job.
// Toutes les WIFI_NUDGE_MS on secoue (disconnect+reconnect) au cas ou
// la machine d'etat WiFi soit bloquee. Au-dela de WIFI_LOSS_REBOOT_MS de perte
// continue, on redemarre franchement.
void handleWifiLoss() {
  // Securite : si l'event task n'a pas (encore) ecrit wifiLostAt, on l'initialise.
  if (wifiLostAt == 0) wifiLostAt = millis();

  unsigned long now  = millis();
  unsigned long lost = now - wifiLostAt;

  // "Premier passage d'un episode de perte". On memorise l'horodatage de la
  // perte traitee plutot que de comparer lastNudge a wifiLostAt : cette
  // comparaison d'instants absolus s'inversait au wraparound de millis()
  // (~49 jours d'uptime) et le nudge ne partait plus.
  static unsigned long lastNudge      = 0;
  static unsigned long handledLossAt  = 0;
  bool firstInEpisode = (handledLossAt != wifiLostAt);
  if (firstInEpisode) handledLossAt = wifiLostAt;
  if (firstInEpisode || now - lastNudge >= WIFI_NUDGE_MS) {
    lastNudge = now;
    wlog("[WiFi] Perdue depuis %lus, %s",
         lost / 1000, firstInEpisode ? "attente auto-reconnect" : "nudge");
    showWifiLost();
    if (!firstInEpisode) {
      // Reveille la machine d'etat WiFi si auto-reconnect a cale.
      WiFi.disconnect(false);
      WiFi.reconnect();
    }
  }

  if (lost >= WIFI_LOSS_REBOOT_MS) {
    wlog("[WiFi] Perte > %lus, redemarrage", WIFI_LOSS_REBOOT_MS / 1000);
    oledPrint("WiFi perdu", "Redemarrage...");
    delay(1500);
    ESP.restart();
  }

  // Yield au scheduler, evite de pegguer le CPU en boucle serree.
  delay(100);
}

// EF-202 / EF-203 / EF-204 / EF-207 : portail captif borné
void startConfigPortal() {
  wlog("[CONFIG] Portail captif demarre");
  showWifiConfigPortal();

  wifiManager.setAPStaticIPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0));
  wifiManager.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

  bool ok = wifiManager.startConfigPortal(AP_SSID);
  oledPrint(ok ? "Config OK!" : "Timeout config", "Redemarrage...");
  delay(1500);
  ESP.restart();
}
