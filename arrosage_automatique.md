# 🌱 Arrosage Automatique Connecté — ESP32 + WiFiManager + Adafruit IO

> **Document de référence** — Logique complète du système, branchements et flux de fonctionnement.
> Aucun code source dans ce document — voir le fichier `main.ino` pour l'implémentation.

---

## 🗺️ Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────┐
│                     SYSTÈME COMPLET                         │
│                                                             │
│  [Alimentation 12V]                                         │
│       │                                                     │
│       ├──► [Buck 12V→5V] ──► [ESP32] ──► [OLED]            │
│       │                        │                            │
│       └──► [Relais COM]        ├──► WiFiManager             │
│                │               └──► Adafruit IO (MQTT)      │
│           [Pompe 12V]                    │                  │
│            (NO du relais)               │                  │
│                                  [Dashboard web]            │
│                                  Bouton ON / OFF            │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔩 Matériel nécessaire

| Composant | Modèle recommandé | Rôle |
|---|---|---|
| **ESP32** | ESP32 Dev Module 38 pins | Cerveau du système + WiFi |
| **Écran OLED** | SSD1306 128×64 I2C | Affichage des étapes en temps réel |
| **Alimentation** | Bloc secteur 12V DC 2A min. | Source d'énergie principale |
| **Pompe à eau** | Pompe submersible 12V DC | Distribue l'eau |
| **Buck Converter** | LM2596 réglable | Convertit 12V → 5V pour l'ESP32 |
| **Module Relais** | Relais 5V 1 canal optoisolé | Commute le circuit 12V de la pompe |
| **Câbles** | Fil 1mm² pour 12V, 0.25mm² pour signal | Connexions électriques |
| **Fusible** | Fusible lame 2A | Protection du circuit pompe |

---

## 🔌 Branchements électriques

### 1 — Alimentation générale (12V)

Le **+12V** de l'adaptateur se divise en deux branches :
- **Branche 1** → Entrée du Buck Converter
- **Branche 2** → Borne `COM` du module relais (fil commun de la pompe)

Le **GND 12V** se divise également :
- **Branche 1** → GND du Buck Converter
- **Branche 2** → Fil négatif de la pompe

> ⚠️ Insérer un **fusible 2A** sur le fil +12V allant vers le relais, avant la borne COM.

---

### 2 — Buck Converter (12V → 5V)

> ⚙️ **Impératif** : Régler la tension de sortie à exactement **5.0V** avec un multimètre **avant** de brancher l'ESP32. Une tension trop haute endommage l'ESP32 de manière irréversible.

| Borne Buck | Branché sur |
|---|---|
| `VIN+` | +12V alimentation |
| `VIN−` | GND alimentation |
| `VOUT+` | Pin `5V` (VIN) de l'ESP32 |
| `VOUT−` | Pin `GND` de l'ESP32 |

---

### 3 — Module Relais ↔ ESP32 ↔ Pompe

Le relais joue le rôle d'interrupteur commandé électroniquement.

| Borne Relais | Branché sur | Rôle |
|---|---|---|
| `VCC` | 3.3V ESP32 | Alimentation logique du module |
| `GND` | GND ESP32 | Masse commune |
| `IN` | GPIO 26 ESP32 | Signal de commande (HIGH = ON) |
| `COM` | +12V alimentation | Fil commun du circuit pompe |
| `NO` | + de la pompe | Connexion fermée quand relais actif |
| `NC` | Non connecté | — |

> `NO` = **Normalement Ouvert** → le circuit pompe est ouvert (pompe arrêtée) par défaut.
> Quand l'ESP32 envoie `HIGH` sur GPIO 26, le relais se ferme → courant passe dans la pompe.

---

### 4 — Écran OLED (I2C)

| Borne OLED | Branché sur | Rôle |
|---|---|---|
| `VCC` | 3.3V ESP32 | Alimentation |
| `GND` | GND ESP32 | Masse |
| `SDA` | GPIO 21 ESP32 | Bus I2C — données |
| `SCL` | GPIO 22 ESP32 | Bus I2C — horloge |

---

## 🗂️ Tableau de câblage complet

> Ce tableau regroupe **chaque connexion fil par fil** de l'ensemble du système.
> Colonne "Couleur conseillée" = convention de câblage recommandée pour s'y retrouver.

### Alimentation 12V

| Départ | Arrivée | Couleur conseillée | Remarque |
|---|---|---|---|
| Adaptateur 12V `+` | Buck Converter `VIN+` | 🔴 Rouge | Alimentation principale |
| Adaptateur 12V `+` | Fusible 2A → Relais `COM` | 🔴 Rouge | Protéger avec fusible lame 2A |
| Adaptateur 12V `−` | Buck Converter `VIN−` | ⚫ Noir | Masse principale |
| Adaptateur 12V `−` | Pompe `−` | ⚫ Noir | Retour masse pompe |

### Buck Converter (12V → 5V)

| Départ | Arrivée | Couleur conseillée | Remarque |
|---|---|---|---|
| Buck `VOUT+` | ESP32 pin `5V (VIN)` | 🔴 Rouge | **Vérifier 5V au multimètre avant branchement !** |
| Buck `VOUT−` | ESP32 pin `GND` | ⚫ Noir | Masse commune de tout le circuit logique |

### Circuit pompe → Relais

| Départ | Arrivée | Couleur conseillée | Remarque |
|---|---|---|---|
| Fusible 2A | Relais `COM` | 🔴 Rouge | Fil +12V après fusible |
| Relais `NO` | Pompe `+` | 🟠 Orange | Circuit fermé quand relais actif |
| `NC` | Non connecté | — | Laisser libre |

### Relais → ESP32 (commande logique)

| Borne Relais | Pin ESP32 | Couleur conseillée | Tension | Remarque |
|---|---|---|---|---|
| `VCC` | `3.3V` | 🔴 Rouge | 3.3V | Alimentation logique du module |
| `GND` | `GND` | ⚫ Noir | 0V | Masse commune |
| `IN` | `GPIO 26` | 🟡 Jaune | 0–3.3V | Signal de commande |

### OLED SSD1306 → ESP32 (bus I2C)

| Borne OLED | Pin ESP32 | Couleur conseillée | Tension | Remarque |
|---|---|---|---|---|
| `VCC` | `3.3V` | 🔴 Rouge | 3.3V | Alimentation écran |
| `GND` | `GND` | ⚫ Noir | 0V | Masse |
| `SDA` | `GPIO 21` | 🟢 Vert | 3.3V | Données I2C |
| `SCL` | `GPIO 22` | 🔵 Bleu | 3.3V | Horloge I2C |

### Synthèse globale — tous les fils de l'ESP32

| Pin ESP32 | Direction | Vers | Tension | Fonction |
|---|---|---|---|---|
| `5V (VIN)` | Entrée | Buck Converter VOUT+ | 5V | Alimentation ESP32 |
| `GND` | Entrée | Buck Converter VOUT− | 0V | Masse commune |
| `3.3V` | Sortie | OLED VCC + Relais VCC | 3.3V | Alim. composants |
| `GPIO 21` | Bidirectionnel | OLED SDA | 3.3V logique | I2C Data (OLED) |
| `GPIO 22` | Sortie | OLED SCL | 3.3V logique | I2C Clock (OLED) |
| `GPIO 26` | Sortie | Relais IN | 3.3V logique | Commande pompe |

### Adresses et paramètres logiciels

| Paramètre | Valeur | Description |
|---|---|---|
| Adresse I2C OLED | `0x3C` | Adresse par défaut SSD1306 |
| SSID du point d'accès | `Arrosage-Setup` | Nom du WiFi de configuration |
| IP du portail captif | `192.168.4.1` | Adresse accès interface config |
| Feed Adafruit IO | `pompe` | Nom exact du feed à créer |
| Valeur pompe ON | `1` | Envoyée dans le feed pour activer |
| Valeur pompe OFF | `0` | Envoyée dans le feed pour désactiver |
| Baudrate série | `115200` | Moniteur série Arduino IDE |
| Tentatives WiFi | `3` | Avant passage en mode config |

---

## ⚙️ Logique de fonctionnement détaillée

### PHASE 1 — Démarrage

À la mise sous tension, l'ESP32 s'initialise dans l'ordre suivant :

1. **Réinitialisation des GPIO** → le relais est forcé à `LOW` (pompe OFF)
2. **Démarrage de l'écran OLED** → affiche `"Demarrage..."` pendant l'init
3. **Lecture de la mémoire Flash** → recherche des credentials WiFi précédemment sauvegardés

---

### PHASE 2 — Connexion WiFi (3 tentatives)

L'ESP32 tente de se reconnecter au dernier réseau WiFi connu.

```
Tentative 1 sur 3 → attente 10s → OK ? → passer à la Phase 3
                                → NON ? → Tentative 2
Tentative 2 sur 3 → attente 10s → OK ? → passer à la Phase 3
                                → NON ? → Tentative 3
Tentative 3 sur 3 → attente 10s → OK ? → passer à la Phase 3
                                → NON ? → passer à la Phase 2b (mode config)
```

**Affichage OLED pendant cette phase :**
```
WiFi: connexion...
Tentative 1/3
SSID: MonReseau
```

---

### PHASE 2b — Mode Configuration WiFi (si 3 échecs)

Si aucune tentative n'a réussi (WiFi inconnu ou credentials invalides), l'ESP32 **crée son propre réseau WiFi** temporaire pour permettre à l'utilisateur de le reconfigurer.

**Ce qui se passe :**

1. L'ESP32 crée un point d'accès WiFi nommé **`Arrosage-Setup`** (sans mot de passe)
2. L'écran OLED affiche les instructions de connexion :
   ```
   ╔══════════════════╗
   ║  MODE SETUP      ║
   ║  WiFi: Arrosage  ║
   ║        -Setup    ║
   ║  IP: 192.168.4.1 ║
   ╚══════════════════╝
   ```
3. L'utilisateur **connecte son téléphone ou PC** au WiFi `Arrosage-Setup`
4. Un **portail captif** s'ouvre automatiquement (ou manuellement via `192.168.4.1`)

**Interface web du portail de configuration :**

```
┌──────────────────────────────────────┐
│     🌱 Arrosage — Config WiFi        │
│                                      │
│  Réseau WiFi détecté :               │
│  ○ MonReseau_5G                      │
│  ○ Freebox-ABCD       ← sélection   │
│  ○ SFR_2GHz                          │
│                                      │
│  Mot de passe : [______________]     │
│                                      │
│         [ Enregistrer ]              │
└──────────────────────────────────────┘
```

5. L'utilisateur sélectionne son réseau, entre le mot de passe, et appuie sur **Enregistrer**
6. Les credentials sont **sauvegardés dans la mémoire Flash** de l'ESP32 (persistants même après coupure de courant)
7. Le point d'accès `Arrosage-Setup` est **automatiquement désactivé**
8. L'ESP32 **redémarre** et reprend depuis la Phase 1 → la Phase 2 fonctionnera cette fois

---

### PHASE 3 — Connexion à Adafruit IO

Une fois connecté au WiFi, l'ESP32 établit une connexion **MQTT sécurisée** avec les serveurs Adafruit IO.

**Étapes :**
1. Connexion au broker MQTT `io.adafruit.com` avec username + clé API
2. **Abonnement au feed `pompe`** → l'ESP32 sera notifié en temps réel de tout changement
3. Récupération immédiate de la **dernière valeur connue** du feed (pour être synchronisé dès le démarrage)
4. Affichage du statut sur l'OLED :
   ```
   WiFi: ✓ OK
   AIO:  ✓ OK
   IP: 192.168.1.XX
   Feed: pompe [0]
   Pompe: OFF
   ```

Si la connexion Adafruit IO échoue (mauvaise clé, pas d'internet), l'ESP32 affiche l'erreur et réessaie toutes les 30 secondes sans bloquer.

---

### PHASE 4 — Fonctionnement normal (boucle principale)

Une fois tout connecté, l'ESP32 entre dans sa **boucle de fonctionnement continue** :

```
Toutes les secondes :
  → Vérifier que le WiFi est toujours connecté
  → Traiter les messages MQTT entrants (Adafruit IO)
  → Mettre à jour l'affichage OLED

  Si perte WiFi → retour Phase 2 (3 tentatives)
  Si perte AIO  → retentative connexion AIO (sans reboot)
```

---

## 🌐 Contrôle via le Dashboard Adafruit IO

### Comment ça fonctionne

```
Utilisateur (n'importe où dans le monde)
        │
        ▼
  Dashboard Adafruit IO
  ┌─────────────────────┐
  │  💧 Pompe           │
  │  [  OFF  ●──  ON  ] │  ← toggle
  └─────────────────────┘
        │  clic
        ▼
  Feed "pompe" mis à jour
  (valeur: 0 ou 1)
        │  MQTT push
        ▼
  ESP32 reçoit la notification
        │
        ├─ valeur = 1 → GPIO 26 HIGH → Relais fermé → Pompe ON
        └─ valeur = 0 → GPIO 26 LOW  → Relais ouvert → Pompe OFF
        │
        ▼
  OLED mis à jour instantanément
```

### Délai de réaction

| Étape | Délai typique |
|---|---|
| Clic utilisateur → serveur Adafruit IO | < 200 ms |
| Serveur Adafruit IO → ESP32 (MQTT push) | < 500 ms |
| ESP32 → activation relais | < 10 ms |
| **Total bout-en-bout** | **< 1 seconde** |

---

## 📺 Affichage OLED — Référence des écrans

| Situation | Contenu affiché |
|---|---|
| Démarrage | `"Demarrage..."` |
| Tentative WiFi | `"WiFi: connexion... Tentative X/3"` |
| Mode config AP | `"MODE SETUP / WiFi: Arrosage-Setup / 192.168.4.1"` |
| Connexion AIO | `"WiFi OK / Connexion AIO..."` |
| Fonctionnement normal | IP, état WiFi, état AIO, état pompe (ON/OFF) |
| Perte WiFi | `"WiFi perdu! / Reconnexion..."` |
| Erreur AIO | `"Erreur AIO! / Reconnexion 30s..."` |

---

## ☁️ Configuration Adafruit IO

### Création du compte et du feed

1. Aller sur [io.adafruit.com](https://io.adafruit.com) → créer un compte gratuit
2. **Feeds** → **New Feed** → nommer le feed exactement **`pompe`**
3. **My Key** (en haut à droite de l'interface) → noter le `Username` et la `Active Key`

### Création du Dashboard

1. **Dashboards** → **New Dashboard** → nommer "Arrosage"
2. Dans le dashboard → `+` → **Toggle Block**
3. Lier le bloc au feed **`pompe`**
4. Valeur ON = `1` / Valeur OFF = `0`
5. Accesssible depuis **n'importe quel navigateur** (mobile, PC) → `io.adafruit.com/votreuser/dashboards/arrosage`

---

## ⚠️ Précautions importantes

> **Électricité**
> - Toujours couper l'alimentation 12V avant de modifier le câblage
> - Régler le buck converter à 5V **avant** de brancher l'ESP32
> - Ne jamais connecter la pompe et l'ESP32 directement (toujours via relais)
> - Prévoir le fusible 2A pour protéger le circuit pompe

> **Eau et électronique**
> - Placer l'électronique dans un **boîtier étanche IP65** minimum
> - Utiliser des **connecteurs waterproof** pour les câbles exposés à l'humidité
> - Garder la pompe **submergée** avant de la démarrer (ne jamais la faire tourner à vide)

> **Réseau et sécurité**
> - Le point d'accès `Arrosage-Setup` est ouvert (sans MDP) — ne l'activer qu'en environnement de confiance
> - La clé AIO est stockée dans le firmware — ne pas partager le fichier `.ino` compilé

---

## 🔄 Réinitialisation du WiFi

Si vous souhaitez **changer de réseau WiFi** (déménagement, nouveau box…) :

1. Décommenter temporairement la ligne de reset WiFiManager dans le code
2. Recompiler et uploader → l'ESP32 efface les credentials et passe directement en mode config
3. Recommencer la Phase 2b — re-commenter la ligne ensuite et re-uploader

---

## 🔮 Évolutions possibles

- **Capteur d'humidité du sol** → arroser uniquement si le sol est sec (condition sur le feed Adafruit)
- **Feed de durée** → définir depuis le dashboard combien de secondes la pompe tourne
- **Feed de statut** → l'ESP32 publie son état dans un feed `status` pour confirmation aller-retour
- **Historique** → Adafruit IO conserve l'historique de toutes les activations (30 jours gratuit)
- **Règle automatique** → déclencher la pompe à heure fixe via les "Actions" Adafruit IO (sans toucher au code)
- **OTA** → mise à jour du firmware à distance via WiFi (sans câble USB)