// ============================================================
//  S3 — Communication cloud Adafruit IO / MQTT (cf. §7.3)
//  S5 — Programme d'arrosage (cf. §7.5, Annexe A)
//
//  EF-301..EF-309, EF-501..EF-505, FC-08
// ============================================================

void connectAdafruitIO() {
  wlog("[AIO] %s:%d user=%s id=%s",
       AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, _mqttClientId);

  // SEC-07 : la chaîne du broker est vérifiée contre le magasin de racines
  // embarqué dans le core. Sans ça, n'importe quel MITM sur le LAN pouvait se
  // faire passer pour io.adafruit.com et piloter la pompe.
  secureClientInit(mqttClient);

  for (int attempt = 1; attempt <= AIO_MAX_RETRIES; attempt++) {
    // Pas de disconnect inutile au 1er essai : on n'est pas encore connecté
    if (attempt > 1) {
      mqtt.disconnect();
      delay(200);
    }
    wlog("[AIO] Tentative %d/%d", attempt, AIO_MAX_RETRIES);

    int8_t ret = mqtt.connect();
    if (ret == 0) {
      wlog("[AIO] Connecte (essai %d) — feeds: pompe, programme",
           attempt);
      aioConnected = true;
      // EF-305 : demande de la dernière valeur retenue
      pumpGet.publish("\0");
      programmeGet.publish("\0");
      wlog("[AIO] /get publie sur pompe + programme (last value request)");
      return;
    }

    String err = String(mqtt.connectErrorString(ret));
    wlog("[AIO] Echec %d (code=%d) : %s", attempt, ret, err.c_str());

    // EF-307 : retour visuel local + décompte + cause
    if (attempt < AIO_MAX_RETRIES) {
      for (int c = AIO_RETRY_DELAY_S; c > 0; c--) {
        showAioReconnecting(attempt, AIO_MAX_RETRIES, c, err.c_str());
        server.handleClient();          // EF-309 : web reste réactif
        yield();
        delay(1000);
      }
    }
  }

  wlog("[AIO] Echec global, fonctionnement degrade");
  aioConnected = false;
}

// ────────────────────────────────────────────────────────────
//  Service périodique appelé depuis loop()
//  EF-306 : reconnexion non bloquante avec backoff exponentiel (2 s -> 30 s)
//  EF-308 : ping périodique 30 s
//  EF-309 : non bloquant pour l'IHM locale (1 tentative max par appel)
// ────────────────────────────────────────────────────────────
void serviceMqtt() {
  if (!mqtt.connected()) {
    // Etat de reconnexion persistant entre appels.
    // Plancher a 5 s : mqtt.connect() est bloquant (handshake TLS de plusieurs
    // secondes) et gele l'OLED + l'IHM web pendant ce temps. Reessayer toutes
    // les 2 s rendait l'interface locale inutilisable des que le cloud tombait.
    static unsigned long nextAttempt = 0;
    static unsigned long backoffMs   = 5000;

    if (aioConnected) {
      aioConnected = false;
      wlog("[AIO] Lien tombe, reconnect en arriere-plan");
    }

    unsigned long now = millis();
    // (long)(now - nextAttempt) >= 0 : tolere le wraparound de millis()
    if ((long)(now - nextAttempt) < 0) return;

    secureClientInit(mqttClient);  // SEC-07 : chaîne TLS vérifiée
    int8_t ret = mqtt.connect();   // bloque ~quelques secondes au pire
    if (ret == 0) {
      wlog("[AIO] Reconnecte (backoff reset)");
      aioConnected = true;
      backoffMs    = 5000;
      // EF-305 : redemande la derniere valeur retenue des feeds
      pumpGet.publish("\0");
      programmeGet.publish("\0");
    } else {
      // connectErrorString() renvoie un __FlashStringHelper* : cast explicite
      // pour %s (la flash est memory-mapped sur ESP32).
      wlog("[AIO] Reconnect echec (code=%d : %s), retry +%lums",
           (int)ret, (const char*)mqtt.connectErrorString(ret), backoffMs);
      nextAttempt = now + backoffMs;
      // backoff exponentiel borne a 30 s
      unsigned long doubled = backoffMs * 2;
      backoffMs = (doubled > 30000UL) ? 30000UL : doubled;
    }
    return;
  }

  // Lecture des messages — timeout court pour laisser le loop tourner vite
  Adafruit_MQTT_Subscribe* sub;
  int8_t pendingPump = -1;   // -1=rien, 0=OFF, 1=ON

  while ((sub = mqtt.readSubscription(10))) {
    if (sub == &pumpFeed) {
      const char* v = (const char*)pumpFeed.lastread;
      while (*v == ' ' || *v == '\t') v++;
      wlog("[AIO] pompe = [%s]", (const char*)pumpFeed.lastread);
      pendingPump = (v[0] == '1' ||
                     ((v[0] == 'O' || v[0] == 'o') &&
                      (v[1] == 'N' || v[1] == 'n'))) ? 1 : 0;
    } else if (sub == &programmeFeed) {
      parseProgramme((const char*)programmeFeed.lastread);
    }
  }

  // Application différée : évite publish/read entrelacés (EF-103)
  // userCommand=true : un ordre venu du cloud est explicite, un OFF acquitte
  //   donc le verrou anti-inondation et réarme le budget d'arrosage.
  // fromCloud=true : la durée du programme borne ce cycle d'arrosage.
  if (pendingPump >= 0) {
    DBG("Application pendingPump=%d", (int)pendingPump);
    setPump(pendingPump == 1, true, true);
  }

  // EF-308 : keep-alive 30 s
  static unsigned long lastPing = 0;
  if (millis() - lastPing >= AIO_PING_INTERVAL_MS) {
    lastPing = millis();
    if (!mqtt.ping()) {
      wlog("[AIO] Ping echoue, deconnexion forcee");
      mqtt.disconnect();
    } else {
      DBG("MQTT ping OK");
    }
  }
}

// ────────────────────────────────────────────────────────────
//  S5 — Parsing du programme d'arrosage (Annexe A)
//  Format : "<DATE> <HEURE> <DUREE>" séparés par espaces
//  Valeurs spéciales : "0" ou chaîne vide => aucun programme
//  EF-501..EF-505 ; §12.2 : ne plante jamais sur format invalide
// ────────────────────────────────────────────────────────────
void parseProgramme(const char* msg) {
  if (!msg) msg = "";
  while (*msg == ' ' || *msg == '\t') msg++;
  wlog("[PROG] Recu: %s", msg);

  if (msg[0] == '\0' || (msg[0] == '0' && (msg[1] == '\0' || msg[1] == ' '))) {
    hasProgram = false;
    return;
  }

  const char* sp1 = strchr(msg, ' ');
  if (!sp1 || sp1 == msg) { hasProgram = false; wlog("[PROG] Format invalide, ignore"); return; }
  const char* sp2 = strchr(sp1 + 1, ' ');
  if (!sp2 || sp2 == sp1 + 1) { hasProgram = false; wlog("[PROG] Format invalide, ignore"); return; }

  auto copySeg = [](char* dst, size_t cap, const char* s, size_t n) {
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = '\0';
  };

  copySeg(nextWaterDate, sizeof(nextWaterDate), msg,       sp1 - msg);
  copySeg(nextWaterTime, sizeof(nextWaterTime), sp1 + 1,   sp2 - sp1 - 1);
  copySeg(nextWaterDur,  sizeof(nextWaterDur),  sp2 + 1,   strlen(sp2 + 1));
  hasProgram = true;
}
