# Arrosage Automatique — Documentation Complète

> Firmware : v1.0.0 | Carte : ESP32 | Dépôt GitHub : [GabrielC0/PlantsIO.ino](https://github.com/GabrielC0/PlantsIO.ino)

---

## Table des matières

1. [Composants nécessaires](#composants-nécessaires)
2. [Branchements GPIO](#branchements-gpio)
3. [Schéma de câblage](#schéma-de-câblage)
4. [Écran OLED SSD1306](#écran-oled-ssd1306)
5. [Module Relais → Pompe](#module-relais--pompe)
6. [Alimentation](#alimentation)
7. [Bibliothèques Arduino requises](#bibliothèques-arduino-requises)
8. [Configuration WiFi](#configuration-wifi)
9. [Adafruit IO (MQTT)](#adafruit-io-mqtt)
10. [Mise à jour OTA](#mise-à-jour-ota-over-the-air)
11. [Serveur Web embarqué](#serveur-web-embarqué)
12. [Feeds Adafruit IO](#feeds-adafruit-io)
13. [Synchronisation NTP](#synchronisation-ntp)
14. [Programme d'arrosage automatique](#programme-darrosage-automatique)

---

## Composants nécessaires

| Composant       | Modèle / Détails                            |
| --------------- | ------------------------------------------- |
| Microcontrôleur | ESP32 (WROOM ou DevKit)                     |
| Écran           | OLED 0.96" I²C — SSD1306 — 128×64 px        |
| Relais          | Module relais 5V (1 canal)                  |
| Pompe           | Pompe à eau 5V ou 12V (selon module relais) |
| Alimentation    | 5V USB ou alimentation externe              |
| Câbles          | Dupont mâle-femelle                         |

---

## Branchements GPIO

### Résumé des pins utilisées

| Pin ESP32     | Direction | Rôle                | Composant     | Couleur câble conseillé |
| ------------- | --------- | ------------------- | ------------- | ----------------------- |
| GPIO 21 (SDA) | Sortie    | Data I²C            | OLED SSD1306  | Jaune                   |
| GPIO 22 (SCL) | Sortie    | Clock I²C           | OLED SSD1306  | Vert                    |
| GPIO 26       | Sortie    | Commande relais     | Module relais | Bleu                    |
| 3.3V          | Sortie    | Alimentation OLED   | OLED SSD1306  | Rouge                   |
| GND           | —         | Masse commune       | OLED + Relais | Noir                    |
| 5V (VIN)      | Sortie    | Alimentation relais | Module relais | Rouge                   |

---

## Schéma global — Vue d'ensemble

```
                    ┌─────────────────────────────────┐
                    │           ESP32 DevKit           │
                    │                                  │
          USB/5V ──►│ VIN (5V)  ──────────────────────┼──► VCC  [Relais]
                    │                                  │
              3.3V ◄│ 3V3       ──────────────────────┼──► VCC  [OLED]
                    │                                  │
              GND  ◄│ GND  ─────────────┬─────────────┼──► GND  [OLED]
                    │                   └─────────────►│    GND  [Relais]
                    │                                  │
         I²C Data  ◄│ GPIO 21 (SDA) ───────────────── ┼──► SDA  [OLED]
                    │                                  │
         I²C Clock ◄│ GPIO 22 (SCL) ───────────────── ┼──► SCL  [OLED]
                    │                                  │
   Commande relais ◄│ GPIO 26      ───────────────────►│    IN   [Relais]
                    │                                  │
                    └─────────────────────────────────┘
```

---

## Schéma détaillé — ESP32 DevKit (pinout)

```
                         ┌──────────────┐
                    EN ──┤ 1          2 ├── 3V3  ──────────────────► VCC OLED
                   VP ───┤ 3          4 ├── GND  ──┬───────────────► GND OLED
                   VN ───┤ 5          6 ├── D23       └─────────────► GND Relais
                  D34 ───┤ 7          8 ├── D22  ──────────────────► SCL OLED
                  D35 ───┤ 9         10 ├── D1
                  D32 ───┤11         12 ├── D3
                  D33 ───┤13         14 ├── D21  ──────────────────► SDA OLED
                  D25 ───┤15         16 ├── GND
                  D26 ───┤17 ◄OUTPUT 18 ├── D19  ← GPIO 26 → IN Relais
                  D27 ───┤19         20 ├── D18
                  D14 ───┤21         22 ├── D5
                  D12 ───┤23         24 ├── TX
                  GND ───┤25         26 ├── RX
                  D13 ───┤27         28 ├── D22 (SCL)  (déjà ci-dessus)
                  SD2 ───┤29         30 ├── D21 (SDA)  (déjà ci-dessus)
                  SD3 ───┤31         32 ├── GND
                  CMD ───┤33         34 ├── 5V (VIN) ─────────────► VCC Relais
                  SD0 ───┤35         36 ├── GND
                  CLK ───┤37         38 ├── D15
                         └──────────────┘

  Pins utilisées dans ce projet :
  ┌──────────┬────────────────┬───────────┬──────────────────────────┐
  │ Pin      │ Label board    │ Direction │ Connecté à               │
  ├──────────┼────────────────┼───────────┼──────────────────────────┤
  │ 3V3      │ 3.3V           │ Sortie    │ VCC — OLED SSD1306       │
  │ GND      │ GND (x2)       │ Masse     │ GND — OLED + GND Relais  │
  │ VIN      │ 5V             │ Sortie    │ VCC — Module Relais      │
  │ GPIO 21  │ D21 / SDA      │ Sortie    │ SDA — OLED SSD1306       │
  │ GPIO 22  │ D22 / SCL      │ Sortie    │ SCL — OLED SSD1306       │
  │ GPIO 26  │ D26            │ Sortie    │ IN  — Module Relais      │
  └──────────┴────────────────┴───────────┴──────────────────────────┘
```

---

## Schéma détaillé — OLED SSD1306 (0.96" I²C 128×64)

```
              Face avant de l'écran OLED
         ┌────────────────────────────────────┐
         │  ┌──────────────────────────────┐  │
         │  │                              │  │
         │  │   Écran OLED 128 × 64 px     │  │
         │  │                              │  │
         │  └──────────────────────────────┘  │
         └────────────────────────────────────┘
                  │    │    │    │
                 GND  VCC  SCL  SDA
                  │    │    │    │
                  ▼    ▼    ▼    ▼
          Broches du module SSD1306 (de gauche à droite)
         ┌──────┬──────┬──────┬──────┐
         │ GND  │ VCC  │ SCL  │ SDA  │
         │  1   │  2   │  3   │  4   │
         └──────┴──────┴──────┴──────┘
              │    │      │      │
              │    │      │      └──► GPIO 21 (SDA) — ESP32  [Data bidirectionnel I²C]
              │    │      └─────────► GPIO 22 (SCL) — ESP32  [Clock sortie ESP32]
              │    └────────────────► 3.3V          — ESP32  [Alimentation +]
              └─────────────────────► GND           — ESP32  [Masse]

  Détail des broches OLED :
  ┌──────┬────────────┬──────────────┬──────────────────────────────────────┐
  │ Pin  │ Nom        │ Direction    │ Description                          │
  ├──────┼────────────┼──────────────┼──────────────────────────────────────┤
  │  1   │ GND        │ Masse        │ Référence 0V — relier au GND ESP32   │
  │  2   │ VCC        │ Entrée alim  │ 3.3V — broche 3V3 de l'ESP32         │
  │  3   │ SCL        │ Entrée clock │ Signal d'horloge I²C — GPIO 22       │
  │  4   │ SDA        │ Bidir. data  │ Données I²C — GPIO 21                │
  └──────┴────────────┴──────────────┴──────────────────────────────────────┘

  Paramètres I²C :
  • Adresse : 0x3C  (certains modules : 0x3D — vérifier le cavalier A0)
  • Fréquence I²C : 400 kHz (Fast Mode) supportée
  • Protocole : Wire.begin(21, 22) dans le code
```

---

## Schéma détaillé — Module Relais 1 canal (5V)

```
         Face avant du module relais
        ┌───────────────────────────────────────┐
        │  ┌─────┐    LED              ┌──────┐ │
        │  │OPTO │   (état)            │RELAY │ │
        │  └─────┘                    └──────┘ │
        │                                       │
        │  [VCC]  [GND]  [IN]   [COM] [NO] [NC]│
        └───┬──────┬──────┬──────┬─────┬────┬──┘
            │      │      │      │     │    │
            │      │      │      │     │    └──► NC  = Normalement Fermé (non utilisé)
            │      │      │      │     └───────► NO  = Normalement Ouvert ← UTILISÉ
            │      │      │      └─────────────► COM = Commun (borne centrale relais)
            │      │      └────────────────────► IN  = Signal commande  ← GPIO 26 ESP32
            │      └───────────────────────────► GND = Masse            ← GND ESP32
            └──────────────────────────────────► VCC = Alimentation 5V  ← VIN ESP32

  Broches côté COMMANDE (signal bas voltage) :
  ┌──────┬────────────┬──────────┬──────────────────────────────────────────┐
  │ Pin  │ Nom        │ Tension  │ Connecté à                               │
  ├──────┼────────────┼──────────┼──────────────────────────────────────────┤
  │ VCC  │ Alim. +    │ 5V       │ VIN (5V) — ESP32                         │
  │ GND  │ Masse      │ 0V       │ GND      — ESP32                         │
  │ IN   │ Signal     │ 0–3.3V   │ GPIO 26  — ESP32  (OUTPUT)               │
  └──────┴────────────┴──────────┴──────────────────────────────────────────┘

  Broches côté PUISSANCE (circuit pompe, haute tension possible) :
  ┌──────┬─────────────────────┬──────────────────────────────────────────────────┐
  │ Pin  │ Nom                 │ Rôle                                             │
  ├──────┼─────────────────────┼──────────────────────────────────────────────────┤
  │ COM  │ Commun              │ Toujours connecté au + de l'alimentation pompe   │
  │ NO   │ Normally Open       │ Connecté au + de la pompe (circuit ouvert au repos│
  │ NC   │ Normally Closed     │ Non utilisé dans ce projet                       │
  └──────┴─────────────────────┴──────────────────────────────────────────────────┘

  Logique de commande :
  ┌───────────────┬──────────────────┬─────────────────────────────┐
  │ GPIO 26       │ État relais       │ Pompe                       │
  ├───────────────┼──────────────────┼─────────────────────────────┤
  │ LOW  (0V)     │ Ouvert (NO↔COM)  │ ÉTEINTE — circuit ouvert    │
  │ HIGH (3.3V)   │ Fermé  (NO↔COM)  │ ALLUMÉE — circuit fermé     │
  └───────────────┴──────────────────┴─────────────────────────────┘

  ⚠️  La plupart des modules relais ont une logique INVERSE (active LOW).
      Vérifier avec ta LED : si elle s'allume quand GPIO 26 = LOW → relais actif LOW.
      Adapter le code : digitalWrite(RELAY_PIN, LOW) pour allumer la pompe.
```

---

## Schéma détaillé — Pompe à eau

```
  Pompe à eau DC (5V ou 12V)
  ┌─────────────────────────┐
  │       POMPE DC          │
  │                         │
  │  [+]  ──────────────────┼──► NO  — Module Relais
  │  [-]  ──────────────────┼──► GND (masse alimentation pompe)
  └─────────────────────────┘

  Circuit de puissance complet :
                                          ┌─────────────────┐
  Alim. pompe (+) ───────────────────────►│ COM   Relais     │
                                          │                  │
                                          │ NO ──────────────┼──► (+) Pompe
                                          └─────────────────┘

  Alim. pompe (-) ─────────────────────────────────────────────► (-) Pompe

  ┌──────┬────────────────┬──────────────────────────────────────────────────────┐
  │ Fil  │ Pompe          │ Connecté à                                           │
  ├──────┼────────────────┼──────────────────────────────────────────────────────┤
  │  +   │ Positif pompe  │ NO du module relais                                  │
  │  -   │ Négatif pompe  │ GND de l'alimentation pompe (masse commune si 5V)    │
  └──────┴────────────────┴──────────────────────────────────────────────────────┘

  Selon la tension de la pompe :
  ┌──────────┬──────────────────────────────────────────────────────────────────┐
  │ Pompe 5V │ Alimentation pompe = 5V USB ou 5V de l'ESP32 (VIN)               │
  │ Pompe 12V│ Alimentation pompe = source externe 12V (JAMAIS via l'ESP32)     │
  │          │ Dans ce cas : relier les GND ensemble (masse commune obligatoire) │
  └──────────┴──────────────────────────────────────────────────────────────────┘
```

---

## Schéma de câblage complet — Vue synthétique

```
  ALIMENTATION USB 5V
         │
         ▼
  ┌──────────────┐      3.3V ──────────────────────────────► [VCC] OLED
  │              │
  │   ESP32      │      GND ───────────────┬───────────────► [GND] OLED
  │   DevKit     │                         └───────────────► [GND] Relais
  │              │
  │  GPIO 21 ───────────────────────────────────────────────► [SDA] OLED
  │  (SDA)   │
  │              │
  │  GPIO 22 ───────────────────────────────────────────────► [SCL] OLED
  │  (SCL)   │
  │              │
  │  GPIO 26 ───────────────────────────────────────────────► [IN]  Relais
  │              │
  │  VIN (5V)────────────────────────────────────────────────► [VCC] Relais
  └──────────────┘
                                     ┌───────────────────────┐
                                     │     Module Relais      │
                                     │                        │
                                     │  COM ──► (+) Alim pompe│
                                     │  NO  ──► (+) Pompe     │
                                     │  NC  (non connecté)    │
                                     └───────────────────────┘
                                                  │
                                                  ▼
                                           ┌────────────┐
                                           │   POMPE    │
                                           │    DC      │
                                           └────────────┘
                                                  │
                                     (-) Pompe ───┴───► GND alimentation
```

---

## Schéma détaillé — OLED SSD1306

| Paramètre    | Valeur                              |
| ------------ | ----------------------------------- |
| Protocole    | I²C                                 |
| Adresse I²C  | `0x3C`                              |
| Résolution   | 128 × 64 px                         |
| Pin SDA      | GPIO 21                             |
| Pin SCL      | GPIO 22                             |
| Bibliothèque | `Adafruit_SSD1306` + `Adafruit_GFX` |

L'écran affiche en temps réel :

- La date et l'heure synchronisées (NTP)
- L'état de la pompe (ON / OFF)
- La connexion WiFi et Adafruit IO
- Le prochain arrosage programmé
- L'état des mises à jour OTA

---

## Module Relais → Pompe

| Paramètre         | Valeur            |
| ----------------- | ----------------- |
| Pin de commande   | GPIO 26           |
| Niveau actif      | `HIGH` = pompe ON |
| État au démarrage | `LOW` (pompe OFF) |

Le relais est initialisé à `LOW` dans `setup()` pour garantir que la pompe est éteinte au démarrage.

---

## Alimentation

| Composant     | Tension requise                  | Source                      |
| ------------- | -------------------------------- | --------------------------- |
| ESP32         | 5V USB (régulateur interne 3.3V) | Port USB ou VIN             |
| OLED SSD1306  | 3.3V                             | Broche 3.3V de l'ESP32      |
| Module relais | 5V                               | Broche VIN (5V) de l'ESP32  |
| Pompe         | 5V ou 12V selon modèle           | Alimentation externe si 12V |

> **Attention :** Si la pompe est alimentée en 12V, utiliser une alimentation externe et un relais supportant 12V. Ne **pas** alimenter une pompe 12V via l'ESP32.

---

## Bibliothèques Arduino requises

Installer via le **Gestionnaire de bibliothèques** de l'IDE Arduino :

| Bibliothèque                       | Usage                         |
| ---------------------------------- | ----------------------------- |
| `WiFi` (incluse ESP32)             | Connexion WiFi                |
| `WiFiManager` par tzapu            | Portail de configuration WiFi |
| `Wire` (incluse ESP32)             | Communication I²C             |
| `Adafruit GFX Library`             | Moteur graphique OLED         |
| `Adafruit SSD1306`                 | Driver écran OLED             |
| `Adafruit MQTT Library`            | Client MQTT pour Adafruit IO  |
| `HTTPClient` (incluse ESP32)       | Requêtes HTTP/HTTPS           |
| `WiFiClientSecure` (incluse ESP32) | HTTPS (TLS)                   |
| `Update` (incluse ESP32)           | Écriture OTA en flash         |
| `WebServer` (incluse ESP32)        | Serveur HTTP embarqué         |
| `time.h` (incluse ESP32)           | Synchronisation NTP           |

---

## Configuration WiFi

### Premier démarrage (ou credentials perdus)

1. L'ESP32 tente de se connecter au dernier réseau connu (3 essais, 10 s chacun).
2. En cas d'échec, il démarre un **point d'accès de configuration** :

| Paramètre             | Valeur                |
| --------------------- | --------------------- |
| SSID du point d'accès | `Arrosage-Setup`      |
| Mot de passe          | aucun (réseau ouvert) |
| IP du portail         | `192.168.4.1`         |
| Timeout portail       | 300 secondes          |

3. Connecte-toi au réseau `Arrosage-Setup` puis ouvre `http://192.168.4.1` dans un navigateur.
4. Saisis tes credentials WiFi → l'ESP32 redémarre et se connecte.

### Reconnexion automatique

Si le WiFi est perdu en cours de fonctionnement, l'ESP32 redémarre automatiquement (`ESP.restart()`).

---

## Adafruit IO (MQTT)

### Credentials (à modifier dans le code si nécessaire)

| Paramètre         | Valeur                |
| ----------------- | --------------------- |
| Serveur           | `io.adafruit.com`     |
| Port              | `1883` (non chiffré)  |
| Nom d'utilisateur | `GabrielC0`           |
| Clé API           | définie dans `IO_KEY` |

### Reconnexion MQTT

Si la connexion MQTT est perdue, l'ESP32 tente de se reconnecter automatiquement (5 essais, 5 s entre chaque). En cas d'échec définitif, le programme se bloque avec un message d'erreur sur l'OLED.

---

## Feeds Adafruit IO

| Feed                            | Direction               | Rôle                                            |
| ------------------------------- | ----------------------- | ----------------------------------------------- |
| `GabrielC0/feeds/pompe`         | **Subscribe** (lecture) | Reçoit la commande ON/OFF de la pompe           |
| `GabrielC0/feeds/pompe/get`     | **Publish** (écriture)  | Demande l'état actuel de la pompe au démarrage  |
| `GabrielC0/feeds/pompe_etat`    | **Publish** (écriture)  | Publie l'état réel de la pompe après changement |
| `GabrielC0/feeds/programme`     | **Subscribe** (lecture) | Reçoit le programme d'arrosage                  |
| `GabrielC0/feeds/programme/get` | **Publish** (écriture)  | Demande le programme actuel au démarrage        |

### Format du feed `pompe`

| Valeur reçue       | Action           |
| ------------------ | ---------------- |
| `1`, `ON`, `on`    | Pompe activée    |
| Toute autre valeur | Pompe désactivée |

### Format du feed `programme`

```
DD/MM HH:MM <durée>
```

Exemple : `22/04 07:30 120` → arrosage le 22/04 à 07h30 pendant 120 secondes.

| Valeur reçue          | Action               |
| --------------------- | -------------------- |
| `0` ou vide           | Programme annulé     |
| `DD/MM HH:MM <durée>` | Programme enregistré |

---

## Mise à jour OTA (Over-The-Air)

Les mises à jour se font via **GitHub Raw** (HTTPS).

### Fichiers GitHub requis

| Fichier        | URL configurée     | Contenu                                 |
| -------------- | ------------------ | --------------------------------------- |
| `version.txt`  | `OTA_VERSION_URL`  | Numéro de version distant (ex: `1.1.0`) |
| `firmware.bin` | `OTA_FIRMWARE_URL` | Binaire compilé du firmware             |

> Les URLs pointent vers : `https://raw.githubusercontent.com/GabrielC0/PlantsIO.ino/main/`

### Processus de mise à jour

1. L'ESP32 télécharge `version.txt` depuis GitHub.
2. Compare la version distante avec la version locale (`FW_VERSION`).
3. Si distante > locale : téléchargement du binaire `firmware.bin` en streaming.
4. Écriture en flash via la bibliothèque `Update`.
5. Redémarrage automatique.

### Paramètres OTA

| Paramètre                   | Valeur                               |
| --------------------------- | ------------------------------------ |
| Version locale              | `FW_VERSION` (défini dans le code)   |
| Timeout téléchargement      | `60 000 ms` (60 s)                   |
| Vérification certificat TLS | Désactivée (`setInsecure()`)         |
| Déclenchement               | Via `GET /update` sur le serveur web |

---

## Serveur Web embarqué

L'ESP32 héberge un serveur HTTP sur le **port 80**. Accès via l'IP locale (affichée sur l'OLED et dans le moniteur série).

| Route      | Méthode | Description                                 |
| ---------- | ------- | ------------------------------------------- |
| `/`        | GET     | Page web principale (interface OTA)         |
| `/update`  | GET     | Déclenche la vérification + mise à jour OTA |
| `/status`  | GET     | Retourne l'état OTA en texte brut           |
| `/version` | GET     | Retourne la version locale du firmware      |

### Interface web OTA

La page `/` affiche :

- La version actuelle du firmware
- L'adresse IP de l'ESP32
- L'état en temps réel de la mise à jour (polling automatique toutes les 2 s)
- Un bouton **"Vérifier et mettre à jour"**

---

## Synchronisation NTP

| Paramètre               | Valeur                                             |
| ----------------------- | -------------------------------------------------- |
| Fuseau horaire          | `CET-1CEST,M3.5.0/2,M10.5.0/3` (Europe/Paris)      |
| Serveurs NTP            | `pool.ntp.org`, `time.nist.gov`, `time.google.com` |
| Resynchronisation       | Toutes les **6 heures** automatiquement            |
| Tentatives au démarrage | 10 essais (200 ms entre chaque)                    |

---

## Programme d'arrosage automatique

Le programme est reçu via le feed MQTT `programme`. Il contient :

- **Date** : `DD/MM`
- **Heure** : `HH:MM`
- **Durée** : en secondes

L'ESP32 stocke ces données et les affiche sur l'OLED dans la section "Prochain arrosage".

---

## Moniteur Série

| Paramètre               | Valeur     |
| ----------------------- | ---------- |
| Baudrate                | `115200`   |
| Délai avant init Serial | `1 500 ms` |

Les logs sont préfixés par catégorie : `[WiFi]`, `[AIO]`, `[OTA]`, `[POMPE]`, `[TIME]`, `[WEB]`, `[PROG]`.
