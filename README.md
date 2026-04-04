# PlantsIO — Arrosage Automatique Connecté

Système d'arrosage automatique basé sur un **ESP32**, contrôlable depuis n'importe où via **Adafruit IO** (MQTT), avec affichage OLED, portail de configuration WiFi et mise à jour OTA via GitHub.

---

## Fonctionnalités

- **Contrôle à distance** de la pompe via Adafruit IO (bouton ON/OFF depuis le dashboard web)
- **Programmation** d'arrosages planifiés (date, heure, durée) via un feed MQTT
- **Affichage OLED** (SSD1306 128×64) avec statut en temps réel
- **Configuration WiFi sans recompilation** via portail captif (WiFiManager)
- **Mise à jour OTA** du firmware via GitHub (HTTPS), déclenché depuis une interface web embarquée
- **Synchronisation NTP** pour l'heure locale (fuseau Europe/Paris)

---

## Matériel requis

| Composant       | Modèle recommandé                  |
| --------------- | ---------------------------------- |
| Microcontrôleur | ESP32 Dev Module 38 pins           |
| Écran           | SSD1306 128×64 I2C                 |
| Relais          | Module relais 5V 1 canal optoisolé |
| Pompe           | Pompe submersible 12V DC           |
| Alimentation    | Bloc secteur 12V 2A                |
| Buck Converter  | LM2596 réglable (12V → 5V)         |

Branchements détaillés disponibles dans [arrosage_automatique.md](arrosage_automatique.md).

---

## Branchements rapides

| Pin ESP32       | Connecté à            |
| --------------- | --------------------- |
| `GPIO 26`       | Signal IN du relais   |
| `GPIO 21` (SDA) | SDA de l'écran OLED   |
| `GPIO 22` (SCL) | SCL de l'écran OLED   |
| `3.3V`          | VCC relais + VCC OLED |
| `GND`           | GND relais + GND OLED |

---

## Librairies Arduino requises

Installer via **Outils → Gérer les bibliothèques** :

| Librairie             | Auteur   |
| --------------------- | -------- |
| WiFiManager           | tzapu    |
| Adafruit GFX Library  | Adafruit |
| Adafruit SSD1306      | Adafruit |
| Adafruit MQTT Library | Adafruit |

> `WiFi`, `HTTPClient`, `WiFiClientSecure`, `Update`, `WebServer` sont inclus avec le core ESP32.

**Core ESP32** — ajouter l'URL dans Fichier → Préférences → Gestionnaire de cartes :

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

---

## Configuration

Dans `main/main.ino`, modifier les constantes suivantes :

```cpp
// Identifiants Adafruit IO
#define IO_USERNAME    "votre_username"
#define IO_KEY         "votre_cle_aio"

// Version du firmware
#define FW_VERSION     "1.0.0"

// URLs GitHub pour l'OTA (optionnel)
#define OTA_VERSION_URL  "https://raw.githubusercontent.com/USER/REPO/main/version.txt"
#define OTA_FIRMWARE_URL "https://raw.githubusercontent.com/USER/REPO/main/firmware.bin"
```

---

## Feeds Adafruit IO requis

Créer les feeds suivants sur [io.adafruit.com](https://io.adafruit.com) :

| Feed         | Type           | Rôle                                     |
| ------------ | -------------- | ---------------------------------------- |
| `pompe`      | Toggle / texte | Commande ON (`1`) / OFF (`0`)            |
| `pompe_etat` | Texte          | État publié par l'ESP32                  |
| `programme`  | Texte          | Programme planifié (`JJ/MM HH:MM durée`) |

---

## Mise à jour OTA

1. Compiler dans Arduino IDE → **Croquis → Exporter les binaires compilées**
2. Renommer le `.bin` généré en `firmware.bin`
3. Mettre à jour `version.txt` avec le nouveau numéro (ex: `1.1.0`)
4. Pousser `firmware.bin` et `version.txt` à la racine de la branche `main` du dépôt GitHub
5. Depuis un navigateur, aller sur `http://<IP_ESP32>` et cliquer **Vérifier et mettre à jour**

**Format de `version.txt` :**

```
1.1.0
```

---

## Adresse IP locale

L'IP de l'ESP32 est affichée sur l'écran OLED au démarrage et dans le moniteur série. L'interface web OTA est accessible sur :

```
http://<IP_ESP32>/
```
