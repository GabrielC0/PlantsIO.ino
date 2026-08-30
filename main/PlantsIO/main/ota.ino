// ============================================================
//  S6 — Mise à jour à distance OTA (cf. §7.6)
//  EF-601..EF-610 ; SEC-02 (HTTPS exclusif)
// ============================================================

static void otaSetState(OtaState s, const char* detail = "") {
  otaState = s;
  switch (s) {
    case OTA_IDLE:
      strncpy(otaMessage, "Inactif", sizeof(otaMessage) - 1);
      break;
    case OTA_CHECKING:
      strncpy(otaMessage, "Recherche de mise a jour...", sizeof(otaMessage) - 1);
      oledPrint("== OTA ==", "Recherche...");
      break;
    case OTA_DOWNLOADING:
      strncpy(otaMessage, "Mise a jour en cours...", sizeof(otaMessage) - 1);
      oledPrint("== OTA ==", "Telechargement...");
      break;
    case OTA_SUCCESS:
      strncpy(otaMessage, "Mise a jour terminee", sizeof(otaMessage) - 1);
      oledPrint("== OTA ==", "Terminee!", "Redemarrage...");
      break;
    case OTA_NO_UPDATE:
      strncpy(otaMessage, "Aucune mise a jour disponible", sizeof(otaMessage) - 1);
      oledPrint("== OTA ==", "A jour!", "v" FW_VERSION);
      break;
    case OTA_ERROR:
      snprintf(otaMessage, sizeof(otaMessage), "Erreur: %s",
               (detail && *detail) ? detail : "inconnue");
      oledPrint("== OTA ERREUR ==", detail ? detail : "");
      break;
  }
  otaMessage[sizeof(otaMessage) - 1] = '\0';
  wlog("[OTA] %s", otaMessage);
}

// EF-601 : lecture HTTPS du fichier version
String getRemoteVersion() {
  WiFiClientSecure cli;
  secureClientInit(cli);                   // SEC-07 : chaîne TLS vérifiée
  cli.setTimeout(10000);                   // ms

  HTTPClient http;
  http.begin(cli, OTA_VERSION_URL);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    wlog("[OTA] HTTP version.txt = %d", code);
    http.end();
    return "";
  }
  String v = http.getString();
  http.end();
  v.trim();
  return v;
}

// EF-602 : comparaison sémantique X.Y.Z (1.0.10 > 1.0.2)
bool isNewerVersion(const String& remote, const String& local) {
  int rM=0,rN=0,rP=0, lM=0,lN=0,lP=0;
  // Annexe B : format strict. Un version.txt avec un BOM, un "v" en tête ou des
  // fins de ligne CRLF échouait ici en silence — l'appareil se croyait à jour
  // pour toujours. On trace le rejet pour que le cas soit diagnosticable.
  if (sscanf(remote.c_str(), "%d.%d.%d", &rM, &rN, &rP) != 3) {
    wlog("[OTA] Version distante illisible : [%s] (attendu X.Y.Z)", remote.c_str());
    return false;
  }
  if (sscanf(local.c_str(),  "%d.%d.%d", &lM, &lN, &lP) != 3) {
    wlog("[OTA] Version locale illisible : [%s]", local.c_str());
    return false;
  }
  if (rM != lM) return rM > lM;
  if (rN != lN) return rN > lN;
  return rP > lP;
}

// Vérification au boot — alimente bootSteps[2] et updateAvailable
void checkUpdateAvailable() {
  String remote = getRemoteVersion();
  strncpy(remoteVersionStr, remote.c_str(), sizeof(remoteVersionStr) - 1);
  remoteVersionStr[sizeof(remoteVersionStr) - 1] = '\0';

  if (remote.length() == 0) {
    setBootStep(2, STEP_WARN, "GitHub inaccessible");
    return;
  }
  wlog("[OTA] Local=%s Distant=%s", FW_VERSION, remote.c_str());

  if (isNewerVersion(remote, FW_VERSION)) {
    updateAvailable = true;
    char buf[22];
    snprintf(buf, sizeof(buf), "Dispo: v%.12s", remote.c_str());
    setBootStep(2, STEP_WARN, buf);
  } else {
    char buf[22];
    snprintf(buf, sizeof(buf), "A jour v%s", FW_VERSION);
    setBootStep(2, STEP_OK, buf);
  }
}

// ────────────────────────────────────────────────────────────
//  Téléchargement & flash — EF-605..EF-608, EF-610
// ────────────────────────────────────────────────────────────
void performOTA() {
  otaSetState(OTA_DOWNLOADING);

  // EF-610 : pompe forcée OFF pour la durée du download.
  // OFF de sécurité (userCommand=false) : n'acquitte pas le verrou anti-inondation.
  setPump(false);

  // SEC-02/SEC-07 : la chaîne du serveur est vérifiée avant de télécharger un
  // binaire qui va être flashé. Sans cette vérification, un MITM sur le réseau
  // servait son propre firmware et prenait le contrôle du relais.
  WiFiClientSecure cli;
  secureClientInit(cli);
  cli.setTimeout(OTA_TIMEOUT_MS);

  HTTPClient http;
  http.begin(cli, OTA_FIRMWARE_URL);
  http.setTimeout(OTA_TIMEOUT_MS);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char buf[24];
    snprintf(buf, sizeof(buf), "HTTP %d", code);
    otaSetState(OTA_ERROR, buf);
    http.end();
    return;
  }

  int len = http.getSize();
  if (len <= 0) {
    // Sans Content-Length on ne peut pas distinguer un téléchargement complet
    // d'une coupure réseau en cours de route : Update.end() validerait un
    // firmware tronqué et l'appareil redémarrerait sur une brique.
    // GitHub raw envoie toujours Content-Length, donc ce cas signale un problème.
    otaSetState(OTA_ERROR, "Taille inconnue");
    http.end();
    return;
  }
  wlog("[OTA] Taille = %d octets", len);

  if (!Update.begin((size_t)len, U_FLASH)) {
    otaSetState(OTA_ERROR, Update.errorString());
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];   // tampon plus grand : moins d'allers-retours TLS
  size_t  written  = 0;
  int     lastTen  = -1;
  unsigned long startMs = millis();
  wlog("[OTA] Debut telechargement, heap libre=%u", (unsigned)ESP.getFreeHeap());

  while (written < (size_t)len &&
         (http.connected() || stream->available() > 0)) {
    esp_task_wdt_reset();                                // OTA peut bloquer > 30s
    if (millis() - startMs > OTA_TIMEOUT_MS) {           // EF-608
      Update.abort();
      otaSetState(OTA_ERROR, "Timeout");
      http.end();
      return;
    }

    int avail = stream->available();
    if (avail > 0) {
      int toRead = min(avail, (int)sizeof(buf));
      int r = stream->readBytes(buf, toRead);
      int w = Update.write(buf, r);
      if (w != r) {                                       // EF-607
        Update.abort();
        otaSetState(OTA_ERROR, "Ecriture flash");
        http.end();
        return;
      }
      written += w;

      {                                                   // EF-605
        int pct = (int)(100L * written / len);
        if (pct / 10 != lastTen) {
          lastTen = pct / 10;
          wlog("[OTA] %u/%d (%d%%)", (unsigned)written, len, pct);
          oledPrint("== OTA ==", "Telechargement...",
                    String(pct) + "% (" + String(written / 1024) + " ko)");
        }
      }
    }
    delay(1);
  }
  http.end();

  if (written != (size_t)len) {
    Update.abort();
    otaSetState(OTA_ERROR, "Transfert incomplet");
    return;
  }
  if (!Update.end(true)) {
    otaSetState(OTA_ERROR, Update.errorString());          // EF-607
    return;
  }

  unsigned long durMs = millis() - startMs;
  unsigned kbps = durMs ? (unsigned)((written * 8UL) / durMs) : 0;
  wlog("[OTA] Telecharge %u octets en %lums (%u kbps)",
       (unsigned)written, durMs, kbps);

  otaSetState(OTA_SUCCESS);                                // EF-606
  delay(2000);
  ESP.restart();
}

// EF-604 : déclenchement seulement sur action utilisateur (/update)
// EF-609 : une seule MAJ à la fois (vérifié dans le routeur web)
void checkAndUpdate() {
  otaSetState(OTA_CHECKING);

  // Toujours relire : l'action est explicite et le cache du boot peut dater.
  String remote = getRemoteVersion();
  strncpy(remoteVersionStr, remote.c_str(), sizeof(remoteVersionStr) - 1);
  remoteVersionStr[sizeof(remoteVersionStr) - 1] = '\0';

  if (remote.length() == 0) {
    otaSetState(OTA_ERROR, "Lecture version impossible");
    return;
  }
  wlog("[OTA] Local=%s Distant=%s", FW_VERSION, remote.c_str());

  if (!isNewerVersion(remote, FW_VERSION)) {
    updateAvailable = false;
    otaSetState(OTA_NO_UPDATE);
    oledPrint("== OTA ==", "A jour!",
              "Local: v" FW_VERSION,
              "Dist:  v" + remote);
    return;
  }
  performOTA();
}
