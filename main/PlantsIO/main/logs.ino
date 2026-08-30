// ============================================================
//  S9 — Journalisation (cf. §7.9)
//  Buffer circulaire en RAM, non persistant (EF-905).
//  EF-901 : >= 50 lignes
//  EF-902 : double émission Serial 115200
// ============================================================

#define WLOG_N 50
#define WLOG_W 96

static char    _wlog[WLOG_N][WLOG_W];
static uint8_t _wlogH = 0;
static uint8_t _wlogC = 0;

// wlog() est appelé depuis loopTask ET depuis la task d'événements WiFi
// (onWifiEvent) : le buffer circulaire doit être protégé, sinon deux écritures
// concurrentes corrompent une ligne ou l'index.
//
// Un mutex FreeRTOS et non un portMUX : il couvre aussi le Serial.println, qui
// prend ~8 ms à 115200 bauds. Laisser le Serial hors du verrou faisait
// s'entrelacer les lignes des deux tasks sur le moniteur ; le mettre dans une
// section critique aurait coupé les interruptions pendant 8 ms, ce qui est
// bien pire. Le mutex donne l'ordre sans bloquer les interruptions.
// Récursif : wlog() reste sûr même appelé indirectement depuis lui-même.
static SemaphoreHandle_t wlogLock() {
  // Initialisation à la première utilisation, qui a lieu dans setup() alors que
  // la task d'événements WiFi n'existe pas encore : pas de course possible.
  static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
  return m;
}

void wlog(const char* fmt, ...) {
  char buf[WLOG_W];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  SemaphoreHandle_t m = wlogLock();
  if (m) xSemaphoreTakeRecursive(m, portMAX_DELAY);

  Serial.println(buf);                                  // EF-902

  strncpy(_wlog[_wlogH], buf, WLOG_W - 1);
  _wlog[_wlogH][WLOG_W - 1] = '\0';
  _wlogH = (_wlogH + 1) % WLOG_N;
  if (_wlogC < WLOG_N) _wlogC++;

  if (m) xSemaphoreGiveRecursive(m);
}

// EF-904 : envoi du buffer au client web en flux, par paquets.
// L'ancienne version construisait une String de ~5 ko que server.send()
// recopiait encore : un pic de plus de 10 ko de heap toutes les 2 s pour une
// page qui se rafraîchit toute seule. Ici le pic est borné à la taille du lot.
void wlogStream() {
  char lot[512];
  size_t used = 0;

  for (uint8_t i = 0; ; i++) {
    char line[WLOG_W];
    SemaphoreHandle_t m = wlogLock();
    if (m) xSemaphoreTakeRecursive(m, portMAX_DELAY);
    uint8_t count = _wlogC;
    uint8_t start = (_wlogC < WLOG_N) ? 0 : _wlogH;
    if (i < count) {
      strncpy(line, _wlog[(start + i) % WLOG_N], WLOG_W - 1);
      line[WLOG_W - 1] = '\0';
    }
    if (m) xSemaphoreGiveRecursive(m);
    if (i >= count) break;

    size_t n = strlen(line);
    if (used + n + 1 >= sizeof(lot)) {          // lot plein : on l'expédie
      server.sendContent(lot, used);
      used = 0;
    }
    memcpy(lot + used, line, n);
    used += n;
    lot[used++] = '\n';
  }
  if (used) server.sendContent(lot, used);
}
