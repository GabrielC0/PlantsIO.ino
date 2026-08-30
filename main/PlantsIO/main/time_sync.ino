// ============================================================
//  S4 — Synchronisation horaire (cf. §7.4)
//  EF-401..EF-405 ; ENF-14 (fr_FR), FC-06 (DST auto)
// ============================================================

bool syncTimeNow() {
  wlog("[NTP] Synchronisation...");

  // EF-402 : fuseau CET/CEST avec règle DST européenne
  // EF-405 : 3 serveurs distincts
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org",
               "time.nist.gov",
               "time.google.com");

  struct tm ti;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&ti, 1200)) {
      timeSynced     = true;
      lastTimeSyncMs = millis();
      wlog("[NTP] OK %02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
      return true;
    }
    server.handleClient();   // EF-808 : IHM web reste réactive
    yield();
    delay(200);
  }
  timeSynced = false;
  wlog("[NTP] Echec");
  return false;   // EF-404 : démarrage continue, affichage --:--
}
