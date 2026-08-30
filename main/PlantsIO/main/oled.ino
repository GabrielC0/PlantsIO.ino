// ============================================================
//  S7 — Affichage local OLED 128x64 SSD1306 (cf. §7.7, Annexe D)
//  EF-701..EF-707
// ============================================================

void oledPrint(String l1, String l2, String l3, String l4) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.println(l1);
  if (l2.length()) display.println(l2);
  if (l3.length()) display.println(l3);
  if (l4.length()) display.println(l4);
  display.display();
}

// ────────────────────────────────────────────────────────────
//  EF-701 : écran de boot structuré (Annexe C)
//  Layout 128×64 :
//    y=0    "PlantsIO v1.0.0"
//    y=12   WiFi  [ OK ]
//    y=23   NTP   [ OK ]
//    y=34   MAJ   [WARN!]
//    y=45   AIO   [ OK ]
//    y=56   detail dernière étape active
// ────────────────────────────────────────────────────────────
void renderBootScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(14, 0);
  display.print("PlantsIO v" FW_VERSION);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  for (int i = 0; i < 4; i++) {
    int y = 12 + i * 11;

    if (bootSteps[i].state == STEP_ERROR) {
      display.fillRect(0, y - 1, 128, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }

    display.setCursor(0, y);
    display.print(bootSteps[i].label);
    display.print("  ");

    switch (bootSteps[i].state) {
      case STEP_PENDING: display.print("          "); break;
      case STEP_RUNNING: {
        const char* fr[] = { "[  .   ]", "[ ..   ]", "[ ...  ]", "[ ..   ]" };
        display.print(fr[(millis() / 300) % 4]);
        break;
      }
      case STEP_OK:    display.print("[  OK  ]"); break;
      case STEP_ERROR: display.print("[ECHEC ]"); break;
      case STEP_WARN:  display.print("[ MAJ! ]"); break;
    }

    if (bootSteps[i].state == STEP_ERROR)
      display.setTextColor(SSD1306_WHITE);
  }

  // Détail bas : dernière étape avec un message
  int di = -1;
  for (int i = 3; i >= 0; i--) {
    if (bootSteps[i].detail[0]) { di = i; break; }
  }
  if (di >= 0) {
    display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
    if (bootSteps[di].state == STEP_ERROR) {
      display.fillRect(0, 55, 128, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(0, 56);
    display.print(bootSteps[di].detail);
    display.setTextColor(SSD1306_WHITE);
  }
  display.display();
}

void setBootStep(int idx, BootStepState state, const char* detail) {
  if (idx < 0 || idx >= 4) return;
  bootSteps[idx].state = state;
  strncpy(bootSteps[idx].detail, detail, 21);
  bootSteps[idx].detail[21] = '\0';
  renderBootScreen();
}

// EF-703 : écran d'événement pompe ON/OFF (~2 s)
static void showPumpStatusChange(bool state) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(28, 0); display.print("STATUT POMPE");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setTextSize(3);                       // Annexe D : police 3
  if (state) {
    display.fillRect(24, 14, 80, 28, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(28, 16); display.print(" ON ");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.drawRect(24, 14, 80, 28, SSD1306_WHITE);
    display.setCursor(28, 16); display.print(" OFF");
  }

  struct tm ti;
  char tbuf[6] = "--:--";
  // Lecture non bloquante : la struct est deja a jour si NTP a marche au moins une fois
  if (timeSynced && getLocalTime(&ti, 0))
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d", ti.tm_hour, ti.tm_min);

  display.setTextSize(1);
  display.setCursor(43, 54);
  display.print("a "); display.print(tbuf);
  display.display();
}

// EF-705 : message clair en cas de perte WiFi
void showWifiLost() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(8, 0);  display.print("! WiFi perdu !");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 18); display.print("Reconnexion en cours");
  display.setCursor(0, 30); display.print("Tentative auto < 15s");
  display.setCursor(0, 50); display.print("Pompe garde son etat");
  display.display();
}

// EF-707 : portail captif
void showWifiConfigPortal() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 0);  display.print("! WiFi impossible !");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 14); display.print("SSID: " AP_SSID);
  display.setCursor(0, 26); display.print("MDP:  aucun");
  display.setCursor(0, 38); display.print("IP:   192.168.4.1");
  display.setCursor(0, 52); display.print("Ouvre ton navigateur");
  display.display();
}

// EF-307 : décompte tentatives AIO
void showAioReconnecting(int attempt, int maxAttempts,
                         int countdown, const char* errorMsg) {
  static uint8_t dotStep = 0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(16, 0); display.print("== Connexion AIO ==");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("Tentative ");
  display.print(attempt);
  display.print(" / ");
  display.print(maxAttempts);

  if (errorMsg) {
    char buf[22];
    strncpy(buf, errorMsg, 21); buf[21] = '\0';
    display.setCursor(0, 23); display.print(buf);
  }

  display.setCursor(0, 34);
  display.print("Reessai dans ");
  display.print(countdown); display.print("s");

  int elapsed = AIO_RETRY_DELAY_S - countdown;
  int barFill = (128 * elapsed) / AIO_RETRY_DELAY_S;
  display.drawRect(0, 44, 128, 8, SSD1306_WHITE);
  if (barFill > 0) display.fillRect(0, 44, barFill, 8, SSD1306_WHITE);

  dotStep = (dotStep + 1) % 4;
  display.setCursor(0, 55);
  display.print("AIO");
  for (uint8_t i = 0; i < dotStep; i++) display.print(".");
  display.display();
}

// EF-706 / EF-1002 : mode alerte critique distinctif
void showCriticalAlert(const char* msg) {
  if (!msg) msg = "";
  display.clearDisplay();
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_BLACK);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);

  display.setCursor(8, 4); display.print("** ERREUR CRITIQUE **");

  // Découpe du message sur 3 lignes max (~21 chars/ligne)
  int total = strnlen(msg, 63);
  char line[22];
  for (int i = 0; i < 3; i++) {
    int s = i * 21;
    if (s >= total) break;
    int n = total - s;
    if (n > 21) n = 21;
    memcpy(line, msg + s, n);
    line[n] = '\0';
    display.setCursor(2, 18 + i * 10);
    display.print(line);
  }

  display.setCursor(4, 50);
  display.print("Pompe OFF - Reboot 15s");
  display.display();
}

// ────────────────────────────────────────────────────────────
//  EF-702 / EF-703 / EF-704 : écran nominal
// ────────────────────────────────────────────────────────────
void updateStatusScreen() {
  // ENF-04 : 5 Hz max (le filtre dirty saute le draw quand rien ne change)
  static unsigned long lastTick = 0;
  if (millis() - lastTick < 200) return;
  lastTick = millis();

  // EF-703 : fenêtre d'événement pompe ~1.8 s
  bool pumpEvent = pumpChangeActive;
  if (pumpEvent && (millis() - pumpChangeMs >= 1800)) {
    pumpChangeActive = false;
    pumpEvent = false;
  }

  struct tm ti;
  // Non bloquant : evite de pegguer 100 ms a chaque tic d'OLED quand NTP est down
  bool hasTime = timeSynced && getLocalTime(&ti, 0);

  // Fingerprint : on saute le draw + la transmission I2C si rien n'a changé
  uint32_t fp;
  if (pumpEvent) {
    fp = 0x80000000UL | (pumpChangeState ? 1u : 0u);
    if (hasTime) fp |= ((uint32_t)ti.tm_hour << 8) | (uint32_t)ti.tm_min;
  } else {
    fp = 0;
    if (hasTime) fp = ((uint32_t)(ti.tm_year & 0x7F) << 24)
                    | ((uint32_t)(ti.tm_mon  & 0x0F) << 20)
                    | ((uint32_t)(ti.tm_mday & 0x1F) << 14)
                    | ((uint32_t)(ti.tm_hour & 0x1F) <<  8)
                    | (uint32_t)(ti.tm_min   & 0x3F);
    if (hasProgram)             fp ^= 0x40000000UL;
    if (updateAvailable)        fp ^= 0x20000000UL;
    if (otaState != OTA_IDLE)   fp ^= 0x10000000UL;
    if ((millis() / 3000) & 1)  fp ^= 0x08000000UL;
    for (const char* p = nextWaterDate; *p; p++) fp = fp * 31u + (uint8_t)*p;
    for (const char* p = nextWaterTime; *p; p++) fp = fp * 31u + (uint8_t)*p;
    for (const char* p = nextWaterDur;  *p; p++) fp = fp * 31u + (uint8_t)*p;
  }

  static uint32_t lastFp = 0xDEADBEEFUL;
  if (fp == lastFp) return;
  lastFp = fp;

  if (pumpEvent) {
    showPumpStatusChange(pumpChangeState);
    return;
  }

  static const char* moisFR[] = {
    "Janvier", "Fevrier", "Mars",     "Avril",  "Mai",      "Juin",
    "Juillet", "Aout",    "Septembre","Octobre","Novembre", "Decembre"
  };

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // ── Date (fr_FR, ENF-14) ──
  if (hasTime) {
    char dateBuf[24];
    snprintf(dateBuf, sizeof(dateBuf), "%d %s %d",
             ti.tm_mday, moisFR[ti.tm_mon], 1900 + ti.tm_year);
    int w = strlen(dateBuf) * 6;
    display.setCursor((128 - w) / 2, 1); display.print(dateBuf);
  } else {
    display.setCursor(5, 1); display.print("Synchro en cours...");
  }
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  // ── Heure (police 3, Annexe D) ──
  display.setTextSize(3);
  char tBuf[6];
  if (hasTime) snprintf(tBuf, sizeof(tBuf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  else         strcpy(tBuf, "--:--");
  display.setCursor(19, 15); display.print(tBuf);
  display.drawLine(0, 43, 127, 43, SSD1306_WHITE);
  display.setTextSize(1);

  // EF-704 : alterner avec bandeau MAJ disponible
  bool showUpdateBanner = updateAvailable && otaState == OTA_IDLE
                          && (millis() / 3000) % 2 == 1;

  if (showUpdateBanner) {
    display.fillRect(0, 44, 128, 20, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    char banner[28];
    snprintf(banner, sizeof(banner), "  MAJ  v" FW_VERSION "->v%.9s", remoteVersionStr);
    display.setCursor(2, 46);
    display.print(banner);
    display.setCursor(14, 56);
    display.print("Mettre a jour : web");
    display.setTextColor(SSD1306_WHITE);
  } else if (!hasProgram) {
    // EF-503 : message non ambigu
    display.setCursor(19, 52); display.print("Aucun programme");
  } else {
    // EF-502 : prochain arrosage
    display.setCursor(1, 45); display.print("Prochain arrosage:");
    display.setCursor(1, 55);
    display.print(nextWaterDate);
    display.print(" a ");
    display.print(nextWaterTime);
    if (nextWaterDur[0]) {
      display.print(" "); display.print(nextWaterDur);
    }
  }
  display.display();
}
