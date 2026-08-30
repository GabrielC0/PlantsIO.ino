#pragma once

// Modèle de configuration. Copie ce fichier en `secrets.h` et remplis tes
// valeurs — `secrets.h` est ignoré par git et ne doit JAMAIS être commité.

// ─────────────────────────────────────────
//   Adafruit IO
// ─────────────────────────────────────────
#define IO_USERNAME    "ton_username_aio"
#define IO_KEY         "aio_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// ─────────────────────────────────────────
//   Interface web locale (§10.3)
// ─────────────────────────────────────────
// Sans ces identifiants, n'importe qui sur le reseau local peut declencher la
// pompe ou effacer les credentials WiFi. Change-les.
#define WEB_USER       "plantsio"
#define WEB_PASS       "change-moi"

// ─────────────────────────────────────────
//   OTA — URLs GitHub (RAW)
// ─────────────────────────────────────────
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/<user>/<repo>/main/main/PlantsIO/main/version.txt"
#define OTA_FIRMWARE_URL "https://raw.githubusercontent.com/<user>/<repo>/main/main/PlantsIO/main/firmware.bin"
