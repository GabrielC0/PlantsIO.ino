// ============================================================
//  Auto-test des sécurités locales (S1/S10)
//
//  Désactivé par défaut. Pour l'exécuter : passer PLANTSIO_SELFTEST à 1
//  dans main.ino, flasher, et lire le moniteur série — la dernière ligne dit OK ou ECHEC.
//
//  Il n'y a pas de compilateur hôte dans la chaîne Arduino : ce test tourne
//  donc sur la cible réelle, ce qui vérifie en prime le comportement de la
//  RTC RAM et l'arithmétique 32 bits du vrai matériel.
//
//  Contrainte de sûreté : le test ne demande JAMAIS un ON qui serait accepté.
//  Il ne couvre que des chemins où le relais reste ouvert (commande refusée)
//  ou n'est pas sollicité du tout (parsing). Un auto-test qui ferait tourner
//  la pompe pour se prouver juste serait un comble.
// ============================================================

// Le drapeau PLANTSIO_SELFTEST est defini dans main.ino (voir la section Debug).
#if PLANTSIO_SELFTEST

static int _stFail = 0;

static void stCheck(bool cond, const char* what) {
  if (!cond) _stFail++;
  wlog("[TEST] %s %s", cond ? "ok  " : "ECHEC", what);
}

// Fixe le programme courant et renvoie la durée interprétée, en secondes.
static unsigned long durationOf(const char* dur) {
  hasProgram = true;
  snprintf(nextWaterDur, sizeof(nextWaterDur), "%s", dur);
  return programmeDurationMs() / 1000UL;
}

void runSelfTest() {
  wlog("[TEST] === Auto-test des securites ===");

  // ── Sauvegarde de l'état réel : le test le piétine puis le restaure ──
  bool     savedHasProg = hasProgram;
  char     savedDur[sizeof(nextWaterDur)];
  memcpy(savedDur, nextWaterDur, sizeof(savedDur));
  uint32_t savedUsed    = rtcPumpUsedMs;
  uint8_t  savedLock    = rtcPumpLockout;

  // ── Parsing de la durée (Annexe A) ──
  stCheck(durationOf("5min") == 300,  "5min -> 300 s");
  stCheck(durationOf("30s")  == 30,   "30s -> 30 s");
  stCheck(durationOf("2")    == 120,  "2 (sans unite) -> 120 s");
  stCheck(durationOf("1h")   == 3600, "1h -> 3600 s");

  // Valeurs hors normes : la sécurité doit se resserrer, jamais se desserrer.
  stCheck(durationOf("99h")  == 0,    "99h -> rejete (plafond)");
  stCheck(durationOf("0min") == 0,    "0min -> rejete");
  stCheck(durationOf("abc")  == 0,    "abc -> rejete");
  stCheck(durationOf("")     == 0,    "vide -> rejete");

  hasProgram = false;
  stCheck(programmeDurationMs() == 0, "aucun programme -> pas de borne");

  // ── Verrou anti-inondation ──
  // On part pompe à l'arrêt : tous les ON demandés ci-dessous doivent être
  // refusés, donc le relais ne bouge pas de tout le test.
  setPump(false, true);              // OFF explicite : état propre, verrou acquitté
  stCheck(!pumpRunning,        "etat initial : pompe arretee");
  stCheck(rtcPumpLockout == 0, "OFF explicite acquitte le verrou");

  rtcPumpLockout = 1;                // simule un arrêt de sécurité passé
  rtcPumpUsedMs  = PUMP_MAX_ON_MS;

  setPump(true, true, true);         // ordre cloud : doit être refusé
  stCheck(!pumpRunning, "verrou actif : ON du cloud refuse");

  setPump(true, true);               // ordre web : doit être refusé aussi
  stCheck(!pumpRunning, "verrou actif : ON local refuse");

  // Un OFF de sécurité ne doit PAS rendre la main : sinon la coupure
  // anti-inondation s'annulerait elle-même au tour de loop suivant.
  setPump(false);
  stCheck(rtcPumpLockout == 1, "OFF de securite n'acquitte pas le verrou");

  // Seul un ordre humain explicite réarme.
  setPump(false, true);
  stCheck(rtcPumpLockout == 0, "OFF explicite reacquitte le verrou");
  stCheck(rtcPumpUsedMs  == 0, "OFF explicite remet le budget a zero");

  // ── Restauration ──
  hasProgram     = savedHasProg;
  memcpy(nextWaterDur, savedDur, sizeof(nextWaterDur));
  rtcPumpUsedMs  = savedUsed;
  rtcPumpLockout = savedLock;

  wlog("[TEST] === %s ===", _stFail ? "ECHEC" : "OK, tout passe");
}

#endif  // PLANTSIO_SELFTEST
