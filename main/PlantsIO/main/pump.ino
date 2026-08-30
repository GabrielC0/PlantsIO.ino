// ============================================================
//  S1  — Commande de la pompe (cf. §7.1)
//  S10 — Erreur critique fail-safe (cf. §7.10)
//
//  EF-101..EF-106 : commande, fail-safe, séparation cmd/réel
//  EF-1001..EF-1005 : alerte, coupure, redémarrage différé
//  Relais actif-bas : LOW = ON, HIGH = OFF (cf. §9.1)
// ============================================================

// Instant de la derniere comptabilisation du budget (cf. servicePumpTimeout).
static unsigned long pumpLastAccMs = 0;

void setPump(bool state, bool userCommand, bool fromCloud) {
  // Verrou de securite : apres un arret anti-inondation, plus aucun ON n'est
  // accepte tant qu'un humain n'a pas explicitement demande OFF. Le verrou vit
  // en RTC RAM : il survit au reboot, donc la valeur retenue "1" du feed pompe
  // relue au /get de la reconnexion ne peut plus relancer l'arrosage.
  if (state && rtcPumpLockout) {
    wlog("[POMPE] ON refuse : verrou de securite actif (OFF explicite requis)");
    return;
  }

  // Un OFF explicite d'un humain acquitte le verrou et rearme le budget.
  // Un OFF de securite (timeout, OTA, reset WiFi, erreur critique) ne le fait
  // PAS : sinon la coupure de securite s'auto-annulerait immediatement.
  //
  // Cas limite assume : apres un arret de securite, servicePumpTimeout publie
  // "0" sur le feed de commande ; le broker nous renvoie cet echo, qui acquitte
  // donc le verrou. C'est coherent — si la publication a reussi, la valeur
  // retenue vaut "0" et plus aucun /get ne peut relancer la pompe, il faudra un
  // "1" explicite. Et quand cette publication echoue (reseau coupe, le cas ou
  // le verrou compte vraiment), aucun echo n'arrive : le verrou tient.
  if (!state && userCommand) {
    if (rtcPumpLockout) wlog("[POMPE] Verrou acquitte par un ordre OFF explicite");
    rtcPumpLockout = 0;
    rtcPumpUsedMs  = 0;
  }

  bool previous = pumpRunning;

  pumpRunning = state;
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);   // EF-101 : < 200 ms

  // EF-106 : pas d'écran d'événement ni de publication si pas de transition
  if (previous != state) {
    wlog(state ? "[POMPE] ON" : "[POMPE] OFF");
    if (state) {
      pumpLastAccMs   = millis();
      systemAlert[0]  = '\0';   // un nouveau départ acquitte l'alerte de sécurité

      // Budget du cycle, figé au démarrage. Un arrosage commandé par le cloud
      // correspond au programme en cours : sa durée annoncée fait une borne plus
      // serrée que le plafond générique. Un ON manuel depuis l'IHM web n'est pas
      // bridé par le programme — l'utilisateur est devant sa plante, il voit ce
      // qu'il fait.
      unsigned long progMs = fromCloud ? programmeDurationMs() : 0;
      pumpBudgetMs = progMs ? progMs : PUMP_MAX_ON_MS;
      wlog("[POMPE] Budget du cycle : %lus (%s, deja consomme %lus)",
           pumpBudgetMs / 1000UL,
           progMs ? "duree programmee" : "plafond securite",
           (unsigned long)(rtcPumpUsedMs / 1000UL));
    }
    pumpChangeMs     = millis();
    pumpChangeActive = true;
    pumpChangeState  = state;

    // EF-104 : publication best-effort de l'état réel
    if (aioConnected) {
      if (!stateFeed.publish(state ? "1" : "0")) {
        wlog("[POMPE] Erreur publication pompe_etat");
      }
    }
  }
}

// ────────────────────────────────────────────────────────────
//  Durée d'arrosage prévue par le programme, en ms (0 = inconnue).
//  Annexe A : DUREE est une chaîne libre courte, ex. "5min", "30s", "2".
//  Sans unité on interprète en minutes, comme dans l'exemple du cahier.
// ────────────────────────────────────────────────────────────
unsigned long programmeDurationMs() {
  if (!hasProgram || !nextWaterDur[0]) return 0;

  char* end = nullptr;
  unsigned long v = strtoul(nextWaterDur, &end, 10);
  if (v == 0 || end == nextWaterDur) return 0;   // pas de nombre en tête

  while (*end == ' ') end++;
  unsigned long ms;
  if (*end == 's' || *end == 'S')       ms = v * 1000UL;
  else if (*end == 'h' || *end == 'H')  ms = v * 3600000UL;
  else                                  ms = v * 60000UL;   // minutes par défaut

  // Garde-fou : une durée absurde (faute de frappe côté cloud) ne doit pas
  // desserrer la sécurité, seulement la resserrer.
  return (ms == 0 || ms > PUMP_MAX_ON_MS) ? 0 : ms;
}

// ────────────────────────────────────────────────────────────
//  Sécurité anti-inondation (EF-103 : commande d'origine interne)
//  Une commande ON perdue (cloud injoignable, onglet fermé) laissait la pompe
//  tourner indéfiniment. Au-delà du budget : arrêt inconditionnel + verrou.
//
//  Le budget est cumulé en RTC RAM et persisté à chaque tour de loop : un crash
//  au milieu d'un arrosage ne redonne pas un budget neuf au redémarrage.
//  Il est borné par la durée annoncée par le programme quand elle est connue :
//  si le WiFi tombe juste après le ON, on s'arrête à la durée prévue et non au
//  plafond de 15 min (EF-505 reste respecté : on ne déclenche rien, on borne).
// ────────────────────────────────────────────────────────────
void servicePumpTimeout() {
  if (!pumpRunning) {
    pumpLastAccMs = 0;
    return;
  }

  unsigned long now = millis();
  if (pumpLastAccMs == 0) pumpLastAccMs = now;   // ON hérité d'avant un reboot
  rtcPumpUsedMs += (uint32_t)(now - pumpLastAccMs);
  pumpLastAccMs  = now;

  // Budget figé au ON. S'il vaut 0, la pompe tournait déjà avant un reboot :
  // on retombe sur le plafond générique.
  unsigned long budget = pumpBudgetMs ? pumpBudgetMs : PUMP_MAX_ON_MS;

  if (rtcPumpUsedMs < budget) return;

  wlog("[POMPE] Budget %lus atteint (cumul %lus) : arret de securite",
       budget / 1000UL, (unsigned long)(rtcPumpUsedMs / 1000UL));

  setPump(false);            // OFF de sécurité : n'acquitte pas le verrou...
  rtcPumpLockout = 1;        // ...que l'on pose ici, et qui survit au reboot

  snprintf(systemAlert, sizeof(systemAlert),
           "Arret securite : %lu min d'arrosage cumulees",
           budget / 60000UL);

  if (aioConnected) {
    // Best-effort : efface la valeur retenue pour que l'IHM cloud soit cohérente.
    // La sécurité, elle, ne dépend plus de la réussite de cette publication.
    pumpCmd.publish("0");
    alertFeed.publish(systemAlert);
  }
}

// ────────────────────────────────────────────────────────────
//  Erreur critique : coupure pompe inconditionnelle
//  EF-1001 : fail-safe accès direct GPIO, sans dépendance réseau
//  EF-1002 : OLED en alerte
//  EF-1003 : publication best-effort
//  EF-1004 : bannière web reste accessible
//  EF-1005 : redémarrage après ~15 s
// ────────────────────────────────────────────────────────────
void criticalError(const char* msg) {
  if (!msg) msg = "";
  wlog("[CRITIQUE] %s", msg);
  strncpy(systemAlert, msg, sizeof(systemAlert) - 1);
  systemAlert[sizeof(systemAlert) - 1] = '\0';

  pumpRunning = false;
  digitalWrite(RELAY_PIN, HIGH);

  showCriticalAlert(msg);

  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
    alertFeed.publish(msg);
    stateFeed.publish("0");
    pumpCmd.publish("0");   // EF-1001 : le reboot ne doit pas relancer la pompe
  }

  // Deja en mode degrade : rebooter une fois de plus ne reglera rien et rendrait
  // l'appareil injoignable en boucle. On coupe, on alerte, et on continue a
  // tourner avec l'IHM web pour rester diagnosticable.
  if (degradedMode) {
    wlog("[CRITIQUE] Mode degrade deja actif : pas de redemarrage");
    return;
  }

  // Compteur d'erreurs consecutives : au-dela de CRIT_BOOT_LIMIT, le prochain
  // boot demarrera en mode degrade au lieu de reboucler indefiniment.
  if (rtcCritBoots < 255) rtcCritBoots++;
  wlog("[CRITIQUE] Erreur consecutive %d/%d, redemarrage",
       (int)rtcCritBoots, CRIT_BOOT_LIMIT);

  unsigned long deadline = millis() + 15000;
  while ((long)(deadline - millis()) > 0) {
    esp_task_wdt_reset();
    server.handleClient();   // l'IHM web reste consultable
    yield();
    delay(50);
  }
  ESP.restart();
}
