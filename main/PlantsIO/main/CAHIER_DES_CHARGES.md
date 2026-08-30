# Cahier des charges — PlantsIO

**Système d'arrosage automatique connecté**

| Champ              | Valeur                                       |
| ------------------ | -------------------------------------------- |
| Projet             | PlantsIO                                     |
| Type de document   | Cahier des charges fonctionnel et technique  |
| Version du document | 1.0                                         |
| Date               | 2026-05-08                                   |
| Auteur             | Gabriel C.                                   |
| Cible matérielle   | Microcontrôleur ESP32 + relais + écran OLED  |

---

## Sommaire

1. [Présentation du projet](#1-présentation-du-projet)
2. [Glossaire et acronymes](#2-glossaire-et-acronymes)
3. [Contexte, enjeux et objectifs](#3-contexte-enjeux-et-objectifs)
4. [Périmètre du système](#4-périmètre-du-système)
5. [Acteurs et utilisateurs](#5-acteurs-et-utilisateurs)
6. [Analyse fonctionnelle — Missions et services](#6-analyse-fonctionnelle--missions-et-services)
7. [Exigences fonctionnelles détaillées](#7-exigences-fonctionnelles-détaillées)
8. [Exigences non fonctionnelles](#8-exigences-non-fonctionnelles)
9. [Contraintes techniques et environnementales](#9-contraintes-techniques-et-environnementales)
10. [Interfaces du système](#10-interfaces-du-système)
11. [Modes de fonctionnement](#11-modes-de-fonctionnement)
12. [Sûreté de fonctionnement et gestion des erreurs](#12-sûreté-de-fonctionnement-et-gestion-des-erreurs)
13. [Sécurité et confidentialité](#13-sécurité-et-confidentialité)
14. [Livrables et critères de recette](#14-livrables-et-critères-de-recette)
15. [Évolutions envisagées](#15-évolutions-envisagées)
16. [Annexes](#16-annexes)

---

## 1. Présentation du projet

**PlantsIO** est un système embarqué destiné à automatiser l'arrosage de plantes domestiques ou de petites cultures. Il combine :

- une **commande locale** d'une électrovanne ou pompe par relais ;
- une **interface web embarquée** accessible depuis le réseau local ;
- une **supervision et un pilotage à distance** via une plateforme cloud (Adafruit IO) en MQTT/TLS ;
- un **affichage de proximité** sur écran OLED ;
- une **mise à jour à distance** du firmware (OTA) sécurisée via dépôt distant.

Le but est de fournir à l'utilisateur final un dispositif **autonome, fiable, supervisable et maintenable à distance**, capable de fonctionner 24 h/24 sans intervention en conditions normales.

---

## 2. Glossaire et acronymes

| Terme        | Définition                                                                 |
| ------------ | -------------------------------------------------------------------------- |
| AIO          | Adafruit IO — plateforme cloud IoT utilisée pour la supervision MQTT       |
| AP           | Access Point — mode point d'accès WiFi du module                           |
| Feed         | Flux de données nommé hébergé sur Adafruit IO                              |
| Firmware     | Logiciel embarqué exécuté par le microcontrôleur                           |
| HMI / IHM    | Interface Homme-Machine                                                    |
| mDNS         | Multicast DNS — résolution de nom local (`*.local`)                        |
| MQTT         | Message Queuing Telemetry Transport — protocole de messagerie IoT          |
| NTP          | Network Time Protocol — synchronisation horaire réseau                     |
| OTA          | Over-The-Air — mise à jour logicielle à distance                           |
| Pompe        | Désigne indistinctement la pompe ou l'électrovanne pilotée par le relais   |
| TLS          | Transport Layer Security — chiffrement des communications                  |
| Watchdog     | Mécanisme de surveillance qui redémarre le système en cas de blocage       |

---

## 3. Contexte, enjeux et objectifs

### 3.1 Contexte d'usage

L'utilisateur dispose d'une ou plusieurs plantes nécessitant un arrosage régulier mais ne peut pas garantir une présence physique quotidienne. Les solutions du marché sont soit :

- mécaniques (timers), peu flexibles et non supervisables ;
- propriétaires fermées (cloud constructeur), avec dépendance forte au fournisseur.

PlantsIO vise un compromis **ouvert, supervisable et personnalisable**.

### 3.2 Enjeux

- **Fiabilité** : un échec d'arrosage peut entraîner la perte des végétaux ; un arrosage continu non maîtrisé peut entraîner un dégât des eaux.
- **Disponibilité** : le système doit rester opérationnel malgré les coupures réseau ou les redémarrages.
- **Observabilité** : l'utilisateur doit pouvoir vérifier à tout moment l'état du dispositif.
- **Maintenabilité** : les correctifs doivent être déployables sans intervention physique.

### 3.3 Objectifs généraux du système

| Réf.   | Objectif                                                                                  |
| ------ | ----------------------------------------------------------------------------------------- |
| OBJ-01 | Permettre la commande de la pompe à distance (cloud) et localement (web/UI)               |
| OBJ-02 | Garantir la coupure de la pompe en cas d'anomalie critique                                |
| OBJ-03 | Synchroniser et afficher l'heure légale locale en permanence                              |
| OBJ-04 | Recevoir et afficher un programme d'arrosage planifié                                     |
| OBJ-05 | Maintenir la connectivité réseau de manière autonome                                      |
| OBJ-06 | Permettre la mise à jour du firmware sans intervention physique                           |
| OBJ-07 | Fournir un retour visuel clair, à proximité du dispositif                                 |
| OBJ-08 | Tracer le fonctionnement pour diagnostic à posteriori                                     |
| OBJ-09 | Être configurable sans connaissance technique pour le WiFi (portail captif)               |
| OBJ-10 | Sécuriser les communications avec le cloud                                                |

---

## 4. Périmètre du système

### 4.1 Inclus dans le périmètre

- Pilotage d'une charge unique (pompe / électrovanne 1 voie) via un relais.
- Connexion à un réseau WiFi 2,4 GHz domestique.
- Communication avec une plateforme cloud unique (Adafruit IO).
- Interface web embarquée monopage, accessible en LAN.
- Affichage local sur un écran OLED 128×64.
- Mécanisme OTA depuis une source HTTPS publique.
- Système de journalisation circulaire en mémoire.

### 4.2 Hors périmètre

- Pilotage de plusieurs zones d'arrosage indépendantes.
- Mesure de l'humidité du sol, du débit ou du niveau de réservoir.
- Stockage local persistant des historiques d'arrosage.
- Calcul autonome du programme d'arrosage (le programme est fourni par le cloud).
- Application mobile dédiée.
- Authentification utilisateur sur l'interface web locale.
- Fonctionnement sur réseau cellulaire (4G/LTE).

---

## 5. Acteurs et utilisateurs

| Acteur                         | Rôle                                                                            |
| ------------------------------ | ------------------------------------------------------------------------------- |
| **Utilisateur final**          | Pilote la pompe, consulte l'état, déclenche les MAJ, configure le WiFi          |
| **Plateforme Adafruit IO**     | Émet les commandes distantes, reçoit l'état et les alertes                      |
| **Service NTP**                | Fournit l'heure de référence                                                    |
| **Dépôt OTA (GitHub)**         | Héberge la version et le binaire firmware                                       |
| **Routeur WiFi domestique**    | Fournit la connectivité réseau                                                  |
| **Mainteneur / Développeur**   | Publie les nouvelles versions de firmware, surveille les logs                   |

---

## 6. Analyse fonctionnelle — Missions et services

Le système réalise les missions suivantes, classées en **fonctions principales (FP)** et **fonctions de contrainte (FC)** au sens de la méthode APTE.

### 6.1 Fonctions principales

| Réf.  | Fonction principale                                                                   |
| ----- | ------------------------------------------------------------------------------------- |
| FP-01 | Permettre à l'utilisateur de **commander manuellement** l'état de la pompe            |
| FP-02 | Permettre à un système distant de **commander la pompe à distance**                   |
| FP-03 | **Afficher en temps réel** l'état du système à proximité                              |
| FP-04 | **Exposer une interface web** de supervision et de commande                           |
| FP-05 | **Annoncer un programme d'arrosage** reçu du cloud                                    |
| FP-06 | **Mettre à jour à distance** le firmware                                              |
| FP-07 | **Notifier les anomalies** (alertes critiques)                                        |
| FP-08 | **Fournir une trace** d'exploitation consultable                                      |

### 6.2 Fonctions de contrainte

| Réf.  | Contrainte                                                                            |
| ----- | ------------------------------------------------------------------------------------- |
| FC-01 | S'adapter au réseau WiFi domestique (configurable par l'utilisateur sans outil)       |
| FC-02 | Sécuriser les échanges avec le cloud et le serveur OTA                                |
| FC-03 | Résister aux pertes de connectivité (auto-reprise)                                    |
| FC-04 | Garantir un état sûr (pompe coupée) en cas de défaillance                             |
| FC-05 | Tenir dans la mémoire programme et RAM d'un ESP32 standard                            |
| FC-06 | Respecter l'heure légale française (CET/CEST avec DST automatique)                    |
| FC-07 | Être identifiable de façon unique sur le broker MQTT                                  |
| FC-08 | Rester pilotable en LAN même si le cloud est indisponible                             |

---

## 7. Exigences fonctionnelles détaillées

Chaque exigence est rédigée selon le format : **identifiant — description du comportement attendu — critère de validation**.

### 7.1 Service S1 — Commande de la pompe

| Réf.   | Comportement attendu                                                                                                              | Critère de validation                                               |
| ------ | --------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| EF-101 | Le système doit pouvoir activer ou désactiver la pompe sur ordre.                                                                  | La sortie relais bascule en moins de 200 ms après la commande.      |
| EF-102 | À la mise sous tension, la pompe doit être impérativement à l'état OFF.                                                            | Mesure de l'état du relais à t = 0 après reset.                     |
| EF-103 | La commande peut provenir : (a) de l'interface web locale, (b) du cloud, (c) d'une logique interne (sécurité).                    | Les trois sources produisent le même comportement final.            |
| EF-104 | Toute modification d'état doit être publiée au cloud (feed d'état) si la connexion est disponible.                                | L'état distant reflète l'état local en moins de 5 s.                |
| EF-105 | L'état physique réel de la pompe doit être distinct de l'état "commande" dans l'interface web.                                    | Deux indicateurs séparés sur la page web : commande / retour réel.  |
| EF-106 | Une commande identique à l'état courant ne doit pas générer d'animation parasite ni d'événement bruyant.                          | Pas de surclignotement OLED si pas de transition.                   |

### 7.2 Service S2 — Connectivité WiFi

| Réf.   | Comportement attendu                                                                                                                          | Critère de validation                                            |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| EF-201 | Au démarrage, le système tente de se connecter au dernier réseau WiFi connu, jusqu'à un nombre maximal de tentatives.                          | Au moins 5 tentatives avant échec, avec retour visuel par essai. |
| EF-202 | Si aucun réseau n'est connu ou que toutes les tentatives échouent, le système ouvre un **portail captif** d'auto-configuration.                | Un AP nommé `Arrosage-Setup` est visible.                        |
| EF-203 | Le portail captif doit présenter une page web sans authentification permettant à l'utilisateur de saisir SSID + mot de passe.                   | Validation par un test sur smartphone.                           |
| EF-204 | Après configuration réussie via le portail, le système redémarre automatiquement.                                                              | Reboot observé en moins de 5 s.                                  |
| EF-205 | En cas de perte de WiFi en cours d'exploitation, le système tente une reconnexion rapide (< 15 s) avant de redémarrer.                         | Reconnexion sans perte d'état pompe si possible.                 |
| EF-206 | Le système doit être joignable par un nom local convivial via mDNS (ex. `plantsio-esp.local`).                                                 | Ping/HTTP réussi par le nom .local depuis un poste du LAN.       |
| EF-207 | Le portail captif a un délai d'expiration borné pour éviter un blocage permanent.                                                              | Timeout ≤ 5 minutes, redémarrage automatique ensuite.            |

### 7.3 Service S3 — Communication cloud (Adafruit IO / MQTT)

| Réf.   | Comportement attendu                                                                                                                  | Critère de validation                                              |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| EF-301 | Le système se connecte au broker MQTT du cloud via TLS.                                                                                | Port chiffré utilisé ; échec si TLS désactivé.                     |
| EF-302 | Chaque dispositif doit utiliser un identifiant client unique pour éviter les collisions de session.                                    | L'identifiant intègre une donnée propre à la puce (MAC).           |
| EF-303 | Le système s'abonne aux canaux : commande pompe, programme d'arrosage.                                                                  | Un message publié sur ces canaux est reçu en moins de 3 s.         |
| EF-304 | Le système publie sur les canaux : état réel pompe, alertes critiques, demandes de rafraîchissement (`/get`).                           | Vérifié par lecture du tableau de bord cloud.                      |
| EF-305 | Au moment de la souscription, le système doit demander la dernière valeur retenue (`/get`) pour réinitialiser son état.                | L'état du système après reboot est cohérent avec le cloud.         |
| EF-306 | En cas de déconnexion MQTT, une stratégie de reconnexion automatique est appliquée avec attente bornée entre tentatives.                | Au moins 5 tentatives avant abandon, retour à l'idle ensuite.      |
| EF-307 | Pendant les tentatives de reconnexion, l'utilisateur doit voir un retour visuel local (OLED) avec décompte et cause de l'erreur.        | Affichage tentative X/N + message d'erreur tronqué.                |
| EF-308 | Un mécanisme de keep-alive (ping périodique) doit confirmer la vitalité de la connexion.                                                | Ping émis toutes les 30 s ; déconnexion si échec.                  |
| EF-309 | La perte du cloud ne doit jamais bloquer le pilotage local via l'interface web.                                                          | La page web reste accessible et fonctionnelle hors-ligne du cloud. |

### 7.4 Service S4 — Synchronisation horaire

| Réf.   | Comportement attendu                                                                                            | Critère de validation                                              |
| ------ | --------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ |
| EF-401 | Au démarrage (après WiFi), le système synchronise son horloge sur des serveurs NTP publics.                       | Heure cohérente affichée à ± 2 s en moins de 10 s après boot.     |
| EF-402 | L'heure affichée respecte le fuseau horaire local et applique automatiquement l'heure d'été / d'hiver (DST).      | Heure cohérente entre mars et octobre.                             |
| EF-403 | L'heure est resynchronisée périodiquement pour limiter la dérive.                                                 | Resynchronisation au moins toutes les 6 heures.                    |
| EF-404 | En cas d'échec de synchronisation, le système n'est pas bloqué : il continue avec un affichage dégradé `--:--`.    | Démarrage complet possible même sans NTP.                         |
| EF-405 | Plusieurs serveurs NTP de secours doivent être utilisés.                                                          | Au moins 3 serveurs distincts configurés.                          |

### 7.5 Service S5 — Programme d'arrosage planifié

| Réf.   | Comportement attendu                                                                                                            | Critère de validation                                                  |
| ------ | ------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| EF-501 | Le système reçoit du cloud un message décrivant le **prochain arrosage** : date, heure, durée.                                    | Format unique documenté ; trois champs interprétés correctement.       |
| EF-502 | Le système affiche le prochain arrosage sur l'écran OLED et sur la page web.                                                      | Affichage cohérent en temps réel.                                      |
| EF-503 | Si aucun programme n'est défini ou si le message est vide, l'interface indique clairement « Aucun programme ».                    | Message non ambigu sur les deux interfaces.                            |
| EF-504 | Toute mise à jour du programme reçue du cloud remplace immédiatement la valeur affichée.                                         | Délai de mise à jour visuel < 5 s.                                     |
| EF-505 | Le système ne déclenche pas l'arrosage automatiquement à partir du programme : la commande effective reste pilotée par le cloud.  | Aucun arrosage déclenché en l'absence de message de commande explicite. |

> **NOTE** — La logique de planification reste hors du périmètre de l'objet : elle est portée par le cloud, qui pousse les commandes pompe au moment opportun. L'objet sert d'**afficheur** et d'**actionneur**.

### 7.6 Service S6 — Mise à jour à distance (OTA)

| Réf.   | Comportement attendu                                                                                                                            | Critère de validation                                                |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------- |
| EF-601 | Au démarrage, le système interroge un dépôt distant pour comparer la version locale et la version disponible.                                     | Version distante lue avec succès en HTTPS.                           |
| EF-602 | La comparaison de versions suit un schéma sémantique `MAJEUR.MINEUR.CORRECTIF`.                                                                   | Cas-tests : 1.0.0 vs 1.0.1, 1.0.10 vs 1.0.2, 2.0.0 vs 1.9.9.         |
| EF-603 | Si une mise à jour est disponible, l'OLED et la page web l'annoncent sans bloquer le fonctionnement nominal.                                       | Bandeau visible au moins une fois toutes les 6 secondes en alternance. |
| EF-604 | La mise à jour effective n'est lancée **que sur action explicite de l'utilisateur** (via la page web).                                            | Aucune MAJ déclenchée seule.                                         |
| EF-605 | Pendant le téléchargement, l'OLED affiche la progression (% ou ko) et la page web affiche l'état (en cours / terminée / erreur).                   | Progression mise à jour au moins tous les 10 %.                      |
| EF-606 | À l'issue d'une mise à jour réussie, le système redémarre automatiquement après un court délai.                                                   | Reboot effectué en moins de 5 s après succès.                        |
| EF-607 | En cas d'échec (HTTP, écriture flash, timeout), l'état antérieur du firmware est conservé et un message d'erreur est exposé.                       | Le système redémarre sur l'ancienne version sans corruption.         |
| EF-608 | Le téléchargement OTA a un délai d'expiration global pour éviter un blocage indéfini.                                                              | Timeout strict ≤ 60 s.                                               |
| EF-609 | Une seule mise à jour peut être en cours à la fois.                                                                                                | Tout déclenchement supplémentaire est ignoré ou notifié comme tel.   |
| EF-610 | Pendant la procédure OTA, la pompe doit rester ou être mise à OFF par sécurité.                                                                    | Vérification : la commande pompe est ignorée pendant le download.    |

### 7.7 Service S7 — Affichage local (OLED)

| Réf.   | Comportement attendu                                                                                                                | Critère de validation                                          |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------- |
| EF-701 | Au démarrage, un **écran de boot structuré** affiche les étapes : WiFi, NTP, MAJ, AIO, avec leur statut individuel.                   | Chaque étape passe par les états : en attente / en cours / OK / avertissement / échec. |
| EF-702 | En fonctionnement nominal, l'écran principal affiche : la date du jour en français, l'heure courante, le prochain arrosage.           | Lisible à 1 m, sans clignotement.                              |
| EF-703 | Lors d'un changement d'état pompe, un **écran d'événement** s'affiche temporairement (ON/OFF en gros, horodaté).                       | Affiché environ 2 secondes, puis retour à l'écran principal.    |
| EF-704 | En cas de mise à jour disponible, un bandeau alterne avec l'écran principal pour annoncer la nouvelle version.                        | Alternance visible toutes les 3 s environ.                     |
| EF-705 | En cas de perte WiFi, l'écran l'indique explicitement avec mention d'une tentative de reconnexion.                                    | Message clair et lisible.                                      |
| EF-706 | En cas d'erreur critique, l'écran passe en mode alerte distinctif (inversion vidéo / cadre) avec message court et compte à rebours.    | L'utilisateur identifie immédiatement la criticité.            |
| EF-707 | L'écran affiche le portail de configuration WiFi avec les éléments nécessaires (SSID, IP par défaut).                                  | L'utilisateur peut s'y connecter sans aide externe.            |

### 7.8 Service S8 — Interface web embarquée

| Réf.   | Comportement attendu                                                                                                                                            | Critère de validation                                       |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------- |
| EF-801 | Le système expose un serveur HTTP sur le port standard 80 du LAN.                                                                                                 | Page accessible sans authentification depuis le LAN.        |
| EF-802 | L'interface est responsive (utilisable sur smartphone et desktop).                                                                                                | Test visuel sur écrans 360 px et 1280 px.                  |
| EF-803 | L'interface présente : un toggle de commande pompe, l'état physique réel, le prochain arrosage programmé, l'état OTA, un bouton MAJ, un lien vers les logs.       | Vérifié par inspection visuelle.                            |
| EF-804 | L'interface se rafraîchit automatiquement (sans rechargement utilisateur) à des intervalles courts.                                                                | Rafraîchissement ≤ 10 s pour chaque indicateur.             |
| EF-805 | Une bannière d'alerte critique apparaît en haut de page lorsqu'une alerte système est active.                                                                      | Bannière visible et explicite, masquée hors alerte.         |
| EF-806 | L'interface fonctionne hors-ligne du cloud : le pilotage de la pompe doit rester possible si MQTT est tombé.                                                       | Test : couper le cloud, le toggle continue d'agir.          |
| EF-807 | Une page **Logs** affiche en temps quasi réel le journal interne avec auto-rafraîchissement.                                                                       | Rafraîchissement toutes les ≤ 3 s.                          |
| EF-808 | Toute requête HTTP reçue est non bloquante pour la boucle principale du système.                                                                                   | Aucune coupure visible côté pompe ou OLED pendant un fetch. |

### 7.9 Service S9 — Journalisation (logs)

| Réf.   | Comportement attendu                                                                                                | Critère de validation                                        |
| ------ | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| EF-901 | Le système conserve en mémoire un journal circulaire des événements significatifs (boot, WiFi, MQTT, OTA, pompe).     | Au moins les 50 dernières lignes consultables.               |
| EF-902 | Chaque événement est également émis sur le port série pour debug filaire.                                             | Visible avec un terminal série à 115 200 bauds.              |
| EF-903 | La cause du dernier redémarrage doit être journalisée au boot (alimentation, watchdog, panique, brownout, etc.).      | Présent dans la première ligne du log post-reboot.           |
| EF-904 | Les logs sont consultables via la page web sans outil tiers.                                                          | Endpoint web dédié, format texte simple.                     |
| EF-905 | Le journal n'est pas persistant : il est réinitialisé à chaque redémarrage (volontaire — pas de nécessité de Flash).  | Comportement documenté pour l'utilisateur.                   |

### 7.10 Service S10 — Alertes et erreurs critiques

| Réf.    | Comportement attendu                                                                                                   | Critère de validation                                        |
| ------- | ---------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------ |
| EF-1001 | Toute erreur jugée critique entraîne la coupure **immédiate et inconditionnelle** de la pompe, sans dépendance réseau.  | La sortie relais passe à OFF même hors WiFi/MQTT.            |
| EF-1002 | L'écran OLED bascule sur un affichage d'alerte distinctif.                                                               | Inversion vidéo + message visible.                           |
| EF-1003 | L'alerte est publiée au cloud en best-effort si la connexion est disponible.                                            | Trace dans le canal d'alerte distant.                        |
| EF-1004 | Une alerte est exposée sur la page web (bannière) jusqu'au redémarrage.                                                  | Présente tant que l'utilisateur ne redémarre pas.            |
| EF-1005 | Après une alerte critique, le système redémarre automatiquement après un délai (de l'ordre de 15 s).                     | Reboot observé sans intervention.                            |

---

## 8. Exigences non fonctionnelles

| Réf.   | Catégorie         | Exigence                                                                                            |
| ------ | ----------------- | --------------------------------------------------------------------------------------------------- |
| ENF-01 | Disponibilité     | Le système doit pouvoir tourner ≥ 30 jours en continu sans intervention manuelle.                    |
| ENF-02 | Disponibilité     | Une perte de WiFi de moins de 15 s ne doit pas provoquer de redémarrage.                             |
| ENF-03 | Performance       | Le délai entre un ordre cloud et l'exécution physique doit rester ≤ 5 s en conditions nominales.     |
| ENF-04 | Performance       | L'affichage OLED doit être rafraîchi au moins 1 fois par seconde en mode nominal.                    |
| ENF-05 | Robustesse        | Aucun blocage indéfini : tout traitement bloquant doit être borné par un timeout.                    |
| ENF-06 | Ergonomie         | Aucune intervention de type ligne de commande ne doit être nécessaire à l'utilisateur final.         |
| ENF-07 | Ergonomie         | La langue de l'IHM est le **français**.                                                              |
| ENF-08 | Maintenabilité    | Toute évolution doit être déployable par OTA sans accès physique au boîtier.                         |
| ENF-09 | Observabilité     | L'utilisateur doit pouvoir constater l'état du système sans outil externe (OLED + web).             |
| ENF-10 | Sécurité          | Toute communication réseau sortante doit être chiffrée (TLS/HTTPS).                                  |
| ENF-11 | Confidentialité   | Les identifiants (clés API, SSID, mot de passe WiFi) ne doivent pas être versionnés dans les sources publiques. |
| ENF-12 | Empreinte         | Le firmware doit tenir dans la flash standard d'un ESP32 (4 Mo) avec partition OTA double.           |
| ENF-13 | Durabilité        | Le système doit tolérer ≥ 10 000 cycles de commutation du relais.                                    |
| ENF-14 | Internationalisation | L'heure et la date affichées suivent les conventions locales fr_FR.                              |

---

## 9. Contraintes techniques et environnementales

### 9.1 Contraintes matérielles

- **Microcontrôleur** : ESP32 (WiFi 2,4 GHz intégré, double cœur, ≥ 4 Mo flash).
- **Actionneur** : module relais 1 voie compatible logique 3,3 V, **actif au niveau bas** (un état bas ferme le contact, un état haut le laisse ouvert — la pompe doit donc être à l'arrêt par défaut quand le GPIO est haut ou non initialisé).
- **Affichage** : écran OLED I²C, contrôleur SSD1306, résolution 128×64 monochrome, adresse I²C `0x3C`.
- **Alimentation** : 5 V DC stable, capable de soutenir l'appel de courant du relais et du module ESP32 simultanément.
- **GPIO** : un GPIO de sortie pour le relais (par défaut GPIO 26), I²C sur GPIO 21/22.

### 9.2 Contraintes environnementales

- Fonctionnement intérieur ou extérieur abrité (boîtier IP nécessaire si exposition à l'humidité).
- Plage de température d'utilisation indicative : 0 °C à +50 °C.
- Le système doit tolérer une instabilité réseau modérée (latence, jitter).

### 9.3 Contraintes logicielles

- Environnement de développement **Arduino / PlatformIO** pour ESP32.
- Bibliothèques requises : pile WiFi/HTTP/Update intégrée à l'ESP32, gestionnaire de portail captif (WiFiManager), client MQTT (Adafruit MQTT), pilote OLED (Adafruit SSD1306 + Adafruit GFX).
- Format de version firmware : **chaîne sémantique X.Y.Z**.
- Format du programme d'arrosage reçu : `<DATE> <HEURE> <DUREE>` séparés par des espaces, ou `0` / chaîne vide pour signifier l'absence de programme.

### 9.4 Contraintes réseau

- Bande WiFi 2,4 GHz exclusivement.
- DNS opérationnel pour résoudre les serveurs cloud, OTA et NTP.
- Sortie HTTPS (443) et MQTT/TLS (8883) ouverte sur le routeur.
- Multicast DNS (5353) toléré sur le LAN si l'utilisateur souhaite utiliser le nom `.local`.

---

## 10. Interfaces du système

### 10.1 Interfaces utilisateur

| Interface          | Description                                                                                          |
| ------------------ | ---------------------------------------------------------------------------------------------------- |
| **OLED 128×64**    | Sortie passive d'information. Pas d'interaction directe (pas de bouton).                             |
| **Page web**       | IHM principale, accessible en LAN via IP ou nom .local. Permet la commande, la supervision et la MAJ.|
| **Portail captif** | IHM exceptionnelle, utilisée uniquement pour la (re)configuration WiFi.                              |

### 10.2 Interfaces réseau / protocoles

| Interface           | Protocole          | Direction               | Usage                                            |
| ------------------- | ------------------ | ----------------------- | ------------------------------------------------ |
| Serveur HTTP local  | HTTP/1.1           | Entrée (LAN)            | IHM principale et endpoints API simples           |
| Client MQTT         | MQTT 3.1.1 / TLS   | Bidirectionnel (Cloud)  | Commandes, états, alertes, programme              |
| Client HTTPS        | HTTPS              | Sortie                  | Téléchargement version + binaire firmware (OTA)   |
| Client NTP          | UDP (NTP)          | Sortie                  | Synchronisation horaire                           |
| Service mDNS        | UDP multicast      | LAN                     | Résolution du nom `<hostname>.local`              |

### 10.3 Endpoints HTTP exposés (interface API locale)

| Méthode | Chemin           | Comportement attendu                                                          |
| ------- | ---------------- | ----------------------------------------------------------------------------- |
| GET     | `/`              | Retourne l'IHM principale (HTML).                                             |
| GET     | `/version`       | Retourne la version locale du firmware.                                        |
| GET     | `/status`        | Retourne l'état OTA courant en texte lisible.                                  |
| GET     | `/update`        | Déclenche une vérification + mise à jour si plus récente disponible.           |
| GET     | `/pump`          | Retourne `0` / `1` selon l'état commandé de la pompe.                          |
| POST    | `/pump`          | Reçoit `state=0` ou `state=1`, applique l'état correspondant.                  |
| GET     | `/pump-state`    | Retourne `0` / `1` selon l'état physique réel.                                 |
| GET     | `/programme`     | Retourne le prochain arrosage formaté ou `--` si absent.                       |
| GET     | `/alert`         | Retourne le dernier message d'alerte critique (ou chaîne vide).                |
| GET     | `/logs`          | Retourne une page web de visualisation des logs.                               |
| GET     | `/logs.txt`      | Retourne le journal brut au format texte.                                      |

### 10.4 Canaux cloud (Adafruit IO Feeds)

| Canal logique           | Direction      | Sémantique                                                          |
| ----------------------- | -------------- | ------------------------------------------------------------------- |
| `pompe`                 | Cloud → Objet  | Ordre de commande pompe (`0`/`1` ou équivalent textuel).            |
| `pompe/get`             | Objet → Cloud  | Demande de la dernière valeur connue après reconnexion.             |
| `pompe_etat`            | Objet → Cloud  | État réel de la pompe.                                              |
| `programme`             | Cloud → Objet  | Description du prochain arrosage planifié.                          |
| `programme/get`         | Objet → Cloud  | Demande de la dernière valeur connue.                               |
| `alerte`                | Objet → Cloud  | Message d'alerte critique.                                          |

---

## 11. Modes de fonctionnement

### 11.1 Mode **Initialisation / Boot**
Le système exécute la séquence : initialisation matérielle → tentative WiFi → mDNS → synchronisation NTP → vérification de mise à jour → connexion MQTT. Chaque étape produit un retour visuel sur l'OLED. La pompe est garantie à OFF dès l'entrée dans ce mode.

### 11.2 Mode **Configuration WiFi (portail captif)**
Activé si la connexion WiFi est impossible. Le système devient point d'accès, expose une page de configuration et redémarre après saisie. Borné dans le temps.

### 11.3 Mode **Nominal**
Mode courant : WiFi connecté, MQTT connecté, NTP synchronisé. Toutes les fonctionnalités sont disponibles. La consommation cloud est minimale (publications événementielles + ping).

### 11.4 Mode **Dégradé**
Au moins l'un des éléments suivants est indisponible :
- WiFi en cours de reconnexion ;
- Cloud MQTT indisponible (le contrôle local et l'OLED restent fonctionnels) ;
- NTP indisponible (l'heure est affichée comme `--:--`).

Le système doit rester pilotable autant que possible, sans dégrader la sécurité (pompe coupable manuellement).

### 11.5 Mode **Mise à jour OTA**
La logique applicative est suspendue le temps du téléchargement. La pompe est forcée à OFF. L'OLED affiche la progression. À l'issue, le système redémarre.

### 11.6 Mode **Erreur critique**
Pompe coupée, OLED en alerte, alerte publiée si possible, redémarrage différé. Aucun retour à un mode « nominal » sans redémarrage.

---

## 12. Sûreté de fonctionnement et gestion des erreurs

### 12.1 Principes

- **Fail-safe** : le défaut sûr est **pompe à l'arrêt**. Toute défaillance non gérée doit converger vers cet état.
- **Indépendance des couches de sécurité** : la coupure de la pompe en cas d'erreur ne doit pas dépendre du WiFi ni du cloud.
- **Bornage temporel** : aucun appel réseau ne doit pouvoir bloquer la boucle principale plus de quelques secondes.
- **Reprise automatique** : après toute erreur recouvrable, le système doit revenir au mode nominal sans intervention humaine.

### 12.2 Cas d'erreur et comportement attendu

| Cas d'erreur                              | Comportement attendu                                                                       |
| ----------------------------------------- | ------------------------------------------------------------------------------------------ |
| Perte WiFi                                | Reconnexion rapide (< 15 s), sinon redémarrage. Pompe maintenue dans son état courant.     |
| Échec connexion MQTT                      | Retries bornés avec backoff. UI locale toujours opérationnelle.                            |
| Échec NTP                                 | Affichage horaire dégradé. Pas de blocage du démarrage.                                    |
| Échec téléchargement OTA                  | Restauration de l'ancien firmware, message d'erreur exposé, retour au mode nominal.        |
| OLED non détecté                          | Loggué au boot ; pas de blocage. L'IHM web reste accessible.                                |
| Réception MQTT incompréhensible           | Message ignoré ou interprété comme « pas de programme ». Pas de plantage.                  |
| Brownout / coupure de courant             | Au redémarrage, la pompe est garantie à OFF avant toute autre action.                      |
| Watchdog matériel / panique logicielle    | Le redémarrage est journalisé avec sa cause à l'itération suivante.                        |

---

## 13. Sécurité et confidentialité

| Réf.    | Exigence                                                                                                         |
| ------- | ---------------------------------------------------------------------------------------------------------------- |
| SEC-01  | Les communications avec le cloud MQTT utilisent TLS sur le port 8883.                                             |
| SEC-02  | Les téléchargements OTA utilisent HTTPS exclusivement.                                                            |
| SEC-03  | Les secrets (clé Adafruit IO, identifiants WiFi) ne sont jamais inclus dans les binaires distribués publiquement. |
| SEC-04  | Les identifiants sont externalisés dans un fichier de configuration non versionné (`secrets.h` exclu du dépôt).   |
| SEC-05  | L'identifiant client MQTT est unique par dispositif pour empêcher les conflits de session sur le broker partagé.  |
| SEC-06  | L'IHM web locale ne nécessite pas d'authentification — la sécurité repose sur l'isolation du LAN (à documenter).  |
| SEC-07  | À terme, la vérification stricte du certificat racine est recommandée (remplacement du mode `setInsecure()`).     |
| SEC-08  | Les logs ne doivent pas afficher de secrets en clair.                                                              |

---

## 14. Livrables et critères de recette

### 14.1 Livrables

- Code source du firmware (dépôt Git public ou privé).
- Fichier de configuration secrets exemplifié (`secrets.example.h`).
- Document de version (`version.txt`) et binaire signé/horodaté (`firmware.bin`) hébergés sur le dépôt OTA.
- Présent cahier des charges.
- (Optionnel) Schéma de câblage du relais et de l'écran.

### 14.2 Critères de recette

La recette est validée si **tous** les scénarios suivants se déroulent comme attendu :

1. Premier démarrage sans configuration → portail captif visible et fonctionnel.
2. Configuration WiFi via le portail → reboot automatique → connexion établie → page web accessible.
3. Bascule de la pompe via la page web → relais commute → état réel publié au cloud → confirmation visible OLED + cloud.
4. Bascule de la pompe via le cloud → relais commute → confirmation visible sur OLED et page web.
5. Coupure WiFi simulée < 15 s → le système se reconnecte sans redémarrage.
6. Coupure WiFi simulée > 15 s → le système redémarre proprement et reprend son état.
7. Publication d'un programme depuis le cloud → affichage immédiat sur OLED + page web.
8. Mise en ligne d'une nouvelle version OTA → notification au démarrage suivant → MAJ déclenchée par l'utilisateur → reboot sur la nouvelle version.
9. Simulation d'erreur critique → pompe coupée < 1 s → bannière web visible → reboot après délai.
10. Logs accessibles, lisibles, et incluant la cause du dernier reboot.

---

## 15. Évolutions envisagées

Ces fonctionnalités sont **hors périmètre** de la version actuelle, mais structurent les évolutions futures :

- Pilotage multi-zone (plusieurs sorties relais).
- Capteurs : humidité du sol, niveau du réservoir, débit, pluie.
- Calcul autonome du programme d'arrosage à bord (mode hors-ligne complet).
- Authentification HTTP locale.
- Vérification stricte des certificats TLS (CA pinning).
- Persistance des historiques sur SD ou Flash.
- Notifications push (Telegram, e-mail).
- Internationalisation multi-langue.
- Application mobile dédiée.

---

## 16. Annexes

### Annexe A — Format du programme d'arrosage

Le message reçu sur le canal `programme` respecte le format suivant :

```
<DATE> <HEURE> <DUREE>
```

- `DATE`  : chaîne courte (ex. `JJ/MM`)
- `HEURE` : chaîne `HH:MM`
- `DUREE` : chaîne libre courte (ex. `5min`)

Les valeurs spéciales `0` ou la chaîne vide signifient « aucun programme ».

### Annexe B — Format de version firmware

Format strict : `MAJEUR.MINEUR.CORRECTIF` (entiers, séparés par `.`). La comparaison se fait dans cet ordre. Toute valeur ne respectant pas ce format est rejetée.

### Annexe C — Étapes de boot affichées

| Index | Libellé | Signification                                  |
| ----- | ------- | ---------------------------------------------- |
| 0     | WiFi    | Connexion au réseau local                      |
| 1     | NTP     | Synchronisation horaire                         |
| 2     | MAJ     | Vérification d'une mise à jour disponible       |
| 3     | AIO     | Connexion au broker MQTT cloud                  |

### Annexe D — Conventions visuelles OLED

| Convention                | Usage                                                  |
| ------------------------- | ------------------------------------------------------ |
| Texte normal              | Information courante                                   |
| Inversion vidéo (bandeau) | Alerte, mise à jour disponible, étape en échec         |
| Police taille 3           | Heure courante, indicateur d'état pompe (ON/OFF)       |
| Police taille 1           | Tout le reste                                           |
| Animation par points `...` | Étape en cours                                         |

---

*Fin du cahier des charges.*
