// ============================================================
//  S8 — Interface web embarquée (cf. §7.8, §10.3)
//  EF-801..EF-808 ; FC-08 (pilotable hors cloud)
// ============================================================

static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Arrosage Automatique</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,Arial,sans-serif;background:#f4f6f9;min-height:100vh;padding:24px 12px}
.page{max-width:520px;margin:0 auto;display:flex;flex-direction:column;gap:16px}
.card{background:#fff;border-radius:12px;padding:24px;box-shadow:0 4px 20px rgba(0,0,0,.08)}
.card-title{font-size:13px;font-weight:700;text-transform:uppercase;letter-spacing:.06em;color:#888;margin-bottom:16px}
h1{font-size:20px;color:#1a1a2e;margin-bottom:2px}
.subtitle{font-size:13px;color:#aaa}
.info-row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #f0f0f0;font-size:14px}
.info-row:last-of-type{border-bottom:none}
.label{color:#888}.value{font-weight:600;color:#222}
.pump-row{display:flex;align-items:center;justify-content:space-between;gap:16px}
.pump-label{font-size:15px;font-weight:600;color:#333}
.pump-sub{font-size:12px;color:#aaa;margin-top:2px}
.toggle-wrap{position:relative;width:56px;height:30px;flex-shrink:0}
.toggle-wrap input{opacity:0;width:0;height:0;position:absolute}
.slider{position:absolute;inset:0;background:#ccc;border-radius:30px;cursor:pointer;transition:background .3s}
.slider:before{content:'';position:absolute;width:22px;height:22px;left:4px;bottom:4px;background:#fff;border-radius:50%;transition:transform .3s;box-shadow:0 1px 4px rgba(0,0,0,.2)}
input:checked+.slider{background:#4caf50}
input:checked+.slider:before{transform:translateX(26px)}
.indicator-row{display:flex;align-items:center;gap:12px}
.ind-dot{width:14px;height:14px;border-radius:50%;background:#ccc;flex-shrink:0;transition:background .3s}
.ind-dot.on{background:#4caf50;box-shadow:0 0 6px #4caf5088}
.ind-dot.off{background:#ef5350}
.ind-text{font-size:15px;font-weight:600;color:#333}
.ind-sub{font-size:12px;color:#aaa}
.status-box{background:#f8f9ff;border:1px solid #e0e4ff;border-radius:8px;padding:14px;margin-bottom:14px;font-size:14px;color:#333;display:flex;align-items:center;gap:10px}
.dot{width:10px;height:10px;border-radius:50%;background:#ccc;flex-shrink:0}
.dot.active{background:#4caf50}
.dot.working{background:#ff9800;animation:blink 1s infinite}
.dot.error{background:#f44336}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.btn{width:100%;padding:13px;background:#3f51b5;color:#fff;border:none;border-radius:8px;font-size:14px;cursor:pointer;transition:background .2s}
.btn:hover:not(:disabled){background:#303f9f}
.btn:disabled{background:#9fa8da;cursor:not-allowed}
.note{font-size:12px;color:#aaa;text-align:center;margin-top:10px}
.alert-box{display:none;background:#ffebee;border:1px solid #f44336;border-radius:8px;padding:12px 16px;color:#c62828;font-size:14px;font-weight:600}
.prog-box{background:#f8fdf8;border:1px solid #c8e6c9;border-radius:8px;padding:14px;font-size:14px;color:#333;min-height:48px}
.prog-box.empty{color:#aaa;font-style:italic}
</style></head>
<body>
<div class="page">

<div class="card">
<h1>Arrosage Automatique</h1>
<p class="subtitle" id="ip">...</p>
</div>

<!-- EF-805 / EF-1004 : banniere alerte -->
<div class="alert-box" id="alert-banner">&#9888; <span id="alert-text"></span></div>

<!-- EF-803 : commande pompe -->
<div class="card">
<div class="card-title">Commande</div>
<div class="pump-row">
<div>
<div class="pump-label">Pompe</div>
<div class="pump-sub">Allumer / Eteindre manuellement</div>
</div>
<label class="toggle-wrap">
<input type="checkbox" id="pump-toggle" onchange="togglePump(this)">
<span class="slider"></span>
</label>
</div>
</div>

<!-- EF-105 : etat physique reel separe de la commande -->
<div class="card">
<div class="card-title">Etat reel</div>
<div class="indicator-row">
<div class="ind-dot" id="ind-dot"></div>
<div>
<div class="ind-text" id="ind-text">...</div>
<div class="ind-sub">Retour de l'ESP32</div>
</div>
</div>
</div>

<!-- EF-502 : programme planifie -->
<div class="card">
<div class="card-title">Prochain arrosage programme</div>
<div class="prog-box empty" id="prog-box">Chargement...</div>
</div>

<!-- EF-803 : OTA -->
<div class="card">
<div class="card-title">Mise a jour firmware (OTA)</div>
<div class="info-row">
<span class="label">Version</span>
<span class="value" id="ver">...</span>
</div>
<div class="status-box" style="margin-top:14px;">
<div class="dot" id="dot"></div>
<span id="status-text">Chargement...</span>
</div>
<button class="btn" id="btn-update" onclick="lancerUpdate()">Verifier et mettre a jour</button>
<p class="note" id="note">L'ESP32 redemarrera automatiquement apres la mise a jour.</p>
</div>

<!-- EF-803 / EF-807 : logs -->
<div class="card">
<div class="card-title">Logs systeme</div>
<a href="/logs" target="_blank" style="display:block;padding:13px;background:#607d8b;color:#fff;border-radius:8px;text-align:center;text-decoration:none;font-size:14px">Ouvrir les logs en temps reel</a>
</div>

</div>
<script>
function setDot(s){var d=document.getElementById('dot');d.className='dot';if(s==='working')d.classList.add('working');else if(s==='ok')d.classList.add('active');else if(s==='err')d.classList.add('error');}
function refresh(){
  fetch('/state.json').then(function(r){return r.json();}).then(function(s){
    document.getElementById('pump-toggle').checked=(s.pump===1);
    var d=document.getElementById('ind-dot'),x=document.getElementById('ind-text');
    d.className='ind-dot '+(s.pump?'on':'off');
    x.textContent=s.pump?'Pompe ON - en fonctionnement':'Pompe OFF - arretee';

    document.getElementById('status-text').textContent=s.ota.msg;
    document.getElementById('ver').textContent='v'+s.fw;
    var m=s.ota.msg.toLowerCase();
    var btn=document.getElementById('btn-update');
    if(m.indexOf('cours')!==-1||m.indexOf('recherche')!==-1){setDot('working');btn.disabled=true;}
    else if(m.indexOf('terminee')!==-1){setDot('ok');document.getElementById('note').textContent='Redemarrage en cours...';}
    else if(m.indexOf('erreur')!==-1){setDot('err');btn.disabled=false;}
    else{setDot('');btn.disabled=false;}

    var pb=document.getElementById('prog-box');
    if(s.programme&&s.programme.length>0){pb.className='prog-box';pb.textContent=s.programme;}
    else{pb.className='prog-box empty';pb.textContent='Aucun programme configure';}

    var ab=document.getElementById('alert-banner'),msgs=[];
    if(s.degraded)msgs.push('Mode degrade : cloud et OTA desactives');
    if(s.lock)msgs.push('Verrou securite : arrosage bloque, envoyez un ordre OFF pour rearmer');
    if(s.alert&&s.alert.length>0)msgs.push(s.alert);
    if(msgs.length){document.getElementById('alert-text').textContent=msgs.join(' | ');ab.style.display='block';}
    else ab.style.display='none';
  }).catch(function(){});
}
function lancerUpdate(){
  document.getElementById('btn-update').disabled=true;setDot('working');
  document.getElementById('status-text').textContent='Demarrage...';
  fetch('/update',{method:'POST',headers:{'X-PlantsIO':'1'}}).catch(function(){});
}
function togglePump(cb){
  fetch('/pump',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-PlantsIO':'1'},body:'state='+(cb.checked?'1':'0')}).catch(function(){});
}
document.getElementById('ip').textContent=location.hostname;
refresh();
setInterval(refresh,2000);
</script>
</body></html>
)rawliteral";

static const char LOGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='UTF-8'>
<title>Logs PlantsIO</title>
<style>
body{background:#111;color:#0f0;font-family:monospace;padding:16px;margin:0}
pre{font-size:12px;line-height:1.5;white-space:pre-wrap;word-break:break-all}
.bar{color:#888;font-size:11px;margin-bottom:10px;display:flex;gap:16px;align-items:center}
a{color:#888}
</style></head><body>
<div class='bar'>
<b style='color:#0f0'>PlantsIO - Logs</b>
<a href='/'>&larr; Accueil</a>
<span>Refresh: <span id='t'>2</span>s</span>
<span id='ts'></span>
</div>
<pre id='log'>Chargement...</pre>
<script>
function upd(){fetch('/logs.txt').then(r=>r.text()).then(t=>{document.getElementById('log').textContent=t;document.getElementById('ts').textContent=new Date().toLocaleTimeString();window.scrollTo(0,document.body.scrollHeight);});}
upd();var n=2;setInterval(()=>{n--;if(n<=0){n=2;upd();}document.getElementById('t').textContent=n;},1000);
</script></body></html>
)rawliteral";


// ────────────────────────────────────────────────────────────
//  Routes (cf. §10.3)
// ────────────────────────────────────────────────────────────
// Copie JSON-safe : remplace " et \ par ' et / pour éviter de casser le JSON.
// Les messages embarqués (otaMessage, systemAlert, programme) ne contiennent
// pas de caractères spéciaux dans le code actuel, c'est une défense en profondeur.
static void copyJsonSafe(char* dst, size_t cap, const char* src) {
  size_t i = 0;
  if (cap == 0) return;
  while (src[i] && i + 1 < cap) {
    unsigned char c = (unsigned char)src[i];
    // Les caractères de contrôle (dont \n et \t) cassent le JSON s'ils passent
    // tels quels : un message d'erreur système peut en contenir.
    if (c < 0x20 || c == 0x7F) dst[i] = ' ';
    else if (c == '"')         dst[i] = '\'';
    else if (c == '\\')        dst[i] = '/';
    else                       dst[i] = (char)c;
    i++;
  }
  dst[i] = '\0';
}

// ────────────────────────────────────────────────────────────
//  Contrôle d'accès (§10.3)
//  - Basic auth : empêche n'importe qui sur le LAN de piloter la pompe.
//  - En-tête custom : un formulaire posté depuis un site tiers ne peut pas en
//    poser un sans déclencher un preflight CORS auquel l'ESP32 ne répond pas.
//    Sans ça, un simple <img src=".../update"> ou un <form> caché suffisait à
//    déclencher un flash ou à effacer les credentials WiFi (CSRF), la Basic auth
//    étant rejouée automatiquement par le navigateur.
//  Limite assumée : HTTP en clair sur le LAN, les identifiants circulent en
//  base64. C'est un cran au-dessus de rien, pas un remplacement de TLS.
// ────────────────────────────────────────────────────────────
static bool requireAuth() {
  if (server.authenticate(WEB_USER, WEB_PASS)) return true;
  wlog("[WEB] Auth refusee pour %s depuis %s",
       server.uri().c_str(), server.client().remoteIP().toString().c_str());
  server.requestAuthentication();
  return false;
}

// À appeler sur toute route qui modifie l'état (POST).
static bool requireAuthAndOrigin() {
  if (!requireAuth()) return false;
  if (!server.hasHeader("X-PlantsIO")) {
    wlog("[WEB] %s rejete : en-tete X-PlantsIO absent (CSRF ?)", server.uri().c_str());
    server.send(403, "text/plain", "Requete refusee");
    return false;
  }
  return true;
}

// Prochain arrosage formaté (chaîne vide si aucun programme) — /programme + /state.json
static void formatProgramme(char* dst, size_t cap) {
  if (!hasProgram)          dst[0] = '\0';
  else if (nextWaterDur[0]) snprintf(dst, cap, "%s a %s - %s", nextWaterDate, nextWaterTime, nextWaterDur);
  else                      snprintf(dst, cap, "%s a %s", nextWaterDate, nextWaterTime);
}

void setupWebServer() {
  wlog("[WEB] Routes: / /state.json /update /pump(POST) /wifi-reset(POST) /version /status /pump /pump-state /programme /alert /logs /logs.txt");

  // Seuls les en-tetes explicitement collectes sont lisibles par hasHeader().
  static const char* collected[] = { "X-PlantsIO" };
  server.collectHeaders(collected, 1);

  // EF-801 : page principale port 80
  server.on("/", HTTP_GET, []() {
    if (!requireAuth()) return;
    DBG("WEB GET /");
    server.send_P(200, "text/html", HTML_PAGE);
  });

  // État agrégé : un seul fetch côté JS au lieu de 6
  server.on("/state.json", HTTP_GET, []() {
    if (!requireAuth()) return;
    DBG("WEB GET /state.json");
    char safeOta[96], safeAlert[96], prog[80], safeProg[80];
    copyJsonSafe(safeOta,   sizeof(safeOta),   otaMessage);
    copyJsonSafe(safeAlert, sizeof(safeAlert), systemAlert);

    formatProgramme(prog, sizeof(prog));
    copyJsonSafe(safeProg, sizeof(safeProg), prog);

    char buf[640];
    int n = snprintf(buf, sizeof(buf),
      "{\"pump\":%d,\"alert\":\"%s\","
      "\"ota\":{\"msg\":\"%s\"},"
      "\"programme\":\"%s\",\"fw\":\"" FW_VERSION "\","
      "\"degraded\":%s,\"lock\":%s,"
      "\"lowpower\":%s,\"brownouts\":%d,\"vin\":%d,"
      "\"update\":{\"available\":%s,\"remote\":\"%s\"}}",
      pumpRunning ? 1 : 0,
      safeAlert,
      safeOta,
      safeProg,
      degradedMode   ? "true" : "false",
      rtcPumpLockout ? "true" : "false",
      lowPowerMode   ? "true" : "false",
      (int)rtcBrownouts,
      lastVinMv,
      updateAvailable ? "true" : "false",
      remoteVersionStr);

    // Une troncature produirait du JSON invalide que le navigateur rejetterait
    // en silence : mieux vaut un objet minimal mais valide.
    if (n < 0 || n >= (int)sizeof(buf)) {
      wlog("[WEB] /state.json tronque (%d octets requis)", n);
      snprintf(buf, sizeof(buf),
        "{\"pump\":%d,\"alert\":\"Etat trop long\",\"ota\":{\"msg\":\"\"},"
        "\"programme\":\"\",\"fw\":\"" FW_VERSION "\","
        "\"degraded\":%s,\"lock\":%s,"
        "\"lowpower\":%s,\"brownouts\":%d,\"vin\":%d,"
        "\"update\":{\"available\":false,\"remote\":\"\"}}",
        pumpRunning ? 1 : 0,
        degradedMode   ? "true" : "false",
        rtcPumpLockout ? "true" : "false",
        lowPowerMode   ? "true" : "false",
        (int)rtcBrownouts,
        lastVinMv);
    }

    server.send(200, "application/json", buf);
  });

  // Endpoints "legacy" : conservés pour le contrat API documenté (§10.3).
  // L'IHM web n'en dépend plus — un seul /state.json par cycle suffit.
  server.on("/version", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", FW_VERSION);
  });
  server.on("/status", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain; charset=utf-8", (const char*)otaMessage);
  });
  server.on("/pump", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", pumpRunning ? "1" : "0");
  });
  server.on("/pump-state", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain", pumpRunning ? "1" : "0");
  });
  server.on("/programme", HTTP_GET, []() {
    if (!requireAuth()) return;
    char p[80];
    formatProgramme(p, sizeof(p));
    server.send(200, "text/plain; charset=utf-8", p[0] ? p : "--");
  });
  server.on("/alert", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send(200, "text/plain; charset=utf-8", (const char*)systemAlert);
  });

  // EF-604 / EF-609 : déclenche la MAJ, refuse si déjà en cours.
  // POST et non GET : un GET ne doit jamais modifier l'état, sans quoi une
  // simple balise <img src="http://plantsio-esp.local/update"> lance un flash.
  server.on("/update", HTTP_POST, []() {
    if (!requireAuthAndOrigin()) return;
    if (otaState == OTA_CHECKING || otaState == OTA_DOWNLOADING) {
      wlog("[WEB] /update refuse : OTA deja en cours (state=%d)", (int)otaState);
      server.send(200, "text/plain", "Mise a jour deja en cours...");
      return;
    }
    wlog("[WEB] /update demande par client %s",
         server.client().remoteIP().toString().c_str());
    otaRequested = true;
    server.send(200, "text/plain", "Verification demarree");
  });

  // Reinitialise les credentials WiFi (NVS) puis redemarre.
  // Le reset est differe au prochain tour de loop() pour que la reponse HTTP
  // soit envoyee avant le redemarrage.
  server.on("/wifi-reset", HTTP_POST, []() {
    if (!requireAuthAndOrigin()) return;
    wlog("[WEB] /wifi-reset demande par client %s",
         server.client().remoteIP().toString().c_str());
    wifiResetRequested = true;
    server.send(200, "text/plain", "Reset WiFi planifie, redemarrage imminent");
  });

  // EF-806 / FC-08 : commande locale possible même si MQTT down
  server.on("/pump", HTTP_POST, []() {
    if (!requireAuthAndOrigin()) return;
    if (!server.hasArg("state")) {
      wlog("[WEB] POST /pump rejete : argument state manquant");
      server.send(400, "text/plain", "Missing state");
      return;
    }
    String v = server.arg("state");
    bool on = (v == "1" || v == "on" || v == "ON");
    wlog("[WEB] POST /pump client=%s -> %s",
         server.client().remoteIP().toString().c_str(),
         on ? "ON" : "OFF");
    // userCommand=true : ordre humain explicite, un OFF acquitte le verrou.
    setPump(on, true);
    if (on && rtcPumpLockout) {
      server.send(409, "text/plain", "Verrou de securite actif : envoyez OFF d'abord");
      return;
    }
    server.send(200, "text/plain", "OK");
  });

  // EF-807 / EF-904 : logs HTML + brut
  server.on("/logs", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.send_P(200, "text/html; charset=utf-8", LOGS_PAGE);
  });
  // Envoi en flux : wlogDump() construisait une String de ~5 ko que send()
  // recopiait encore, soit un pic de plus de 10 ko de heap toutes les 2 s. Avec
  // HEAP_CRITICAL_BYTES a 15000, la page de logs pouvait provoquer elle-meme
  // l'erreur critique qu'elle sert a diagnostiquer.
  server.on("/logs.txt", HTTP_GET, []() {
    if (!requireAuth()) return;
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/plain; charset=utf-8", "");
    wlogStream();
    server.sendContent("");   // chunk vide = fin de reponse
  });

  server.onNotFound([]() {
    wlog("[WEB] 404 %s%s",
         server.method() == HTTP_GET ? "GET " : "POST ",
         server.uri().c_str());
    server.send(404, "text/plain", "Not found");
  });
}
