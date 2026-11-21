# ♟️ Plateau d'échecs automatisé (ESP32)

Un plateau d'échecs où **les pièces se déplacent toutes seules**. Un ESP32 fait
tourner un petit moteur d'échecs (« mini IA ») avec 4 niveaux de difficulté et
pilotage un portique mécanique pour déplacer les pièces à votre place.

Vous jouez les Blancs, l'ESP32 joue les Noirs et réplique physiquement chacun de
ses coups.

---

## ✨ Fonctionnalités

- **Moteur d'échecs complet** jouant les règles officielles : coups légaux,
  échec/échec et mat, pat, roque, prise en passant, promotion.
- **4 niveaux de difficulté** (EASY → EXPERT) réglables au vol via un bouton
  poussoir ou la console série.
- **Déplacement automatique des pièces** par électro-aimant sous le plateau
  sur un portique 2 axes.
- **Gestion des captures** : les pièces prises sont évacuées vers une poche de
  parking sur le côté du plateau (roque et prise en passant inclus).
- **Moteur portable et testé sur PC** (perft validé jusqu'à la profondeur 5),
  sans aucune dépendance Arduino.

---

## 🧱 Architecture du projet

```
src/
├── chess/
│   ├── chess.h          # API publique : Board, Engine, Move, Difficulty
│   └── chess.cpp        # Moteur : génération légale, make/unmake, recherche α-β
├── hardware/
│   ├── motion.h         # Brochage & constantes de réglage du portique
│   └── motion.cpp       # Pilotage steppers + électro-aimant (ESP32 / Arduino)
├── main.cpp             # Firmware : boucle de jeu, console, exécution physique
└── host_tests.cpp       # Tests unitaires du moteur (lancés sur PC)
```

Le **moteur d'échecs** (`src/chess/`) est volontairement découplé du matériel :
il ne dépend pas d'Arduino et s'utilise aussi bien sur PC que sur ESP32. La
**couche matérielle** (`src/hardware/`) et le **firmware** (`src/main.cpp`)
n'apportent que la glue avec l'ESP32.

---

## 🔩 Principe matériel

1. Chaque pièce porte une **pastille ferromagnétique** sous sa feutrine.
2. Un **électro-aimant** se déplace sous le plateau, porté par un portique
   **2 axes** (2 moteurs pas-à-pas) : axe X = colonnes a‑h, axe Y = rangées 1‑8.
3. Pour bouger une pièce : l'aimant va au-dessus de la case de départ, s'active
   pour « attraper » la pièce, la traîne jusqu'à la case d'arrivée, puis
   s'éteint pour la relâcher.
4. Les **pièces capturées** sont traînées vers une poche de parking sur le bord.

> ⚠️ **Détection du coup humain** : cette version saisit votre coup sur la
> console série (`e2e4`). La détection automatique par capteurs Reed sous
> chaque case n'est pas encore câblée, mais la structure du code permet de la
> brancher facilement en remplaçant la lecture série dans `main.cpp`.

---

## 🧠 Le moteur / la mini-IA

Le bot repose sur une **recherche alpha-bêta avec élagage** et une phase de
**quiescence** (pour éviter l'effet d'horizon sur les échanges tactiques).
L'évaluation combine :

- la valeur matérielle des pièces (en centipions) ;
- des **tables piece-square** (occupation centrale, sécurité du roi, etc.).

Les coups sont triés par **MVV-LVA** (Most Valuable Victim – Least Valuable
Attacker) pour accélérer les coupures alpha-bêta.

### Niveaux de difficulté

| Niveau   | Profondeur de recherche | Aléatoire |
|----------|------------------------|-----------|
| EASY     | 1 demi-coup            | fort      |
| MEDIUM   | 2 demi-coups           | léger     |
| HARD     | 3 demi-coups           | aucun     |
| EXPERT   | 4 demi-coups           | aucun     |

Aux niveaux faciles, le bot sélectionne de manière pondérée parmi les coups
quasi-optimaux : il peut donc jouer un coup légèrement sous-optimal, ce qui le
rend plus accessible. Aux niveaux élevés, il joue systématiquement son meilleur
coup.

---

## 🔧 Câblage (à adapter dans `src/hardware/motion.h`)

| Fonction             | ESP32 GPIO  |
|----------------------|-------------|
| Axe X — STEP / DIR / EN | 14 / 27 / 26 |
| Axe Y — STEP / DIR / EN | 25 / 33 / 32 |
| Électro-aimant       | 13          |
| Bouton de niveau     | 34          |

Les **drivers de moteurs pas-à-pas** (A4988 / DRV8825 / TMC2209) se branchent
classiquement : broches `STEP`, `DIR`, `EN`, plus `VMOT`/`GND` sur une
alimentation moteur **séparée** de la logique.

L'**électro-aimant** se commande via un **transistor MOSFET** : un GPIO ne peut
en aucun cas piloter directement un aimant (courant bien trop élevé).

---

## 🚀 Compiler & tester

### Sur PC — tests du moteur (sans matériel)

```bash
make test            # compile et lance les tests unitaires
```
ou en direct :
```bash
g++ -std=gnu++17 -O2 -DUNIT_TEST -Isrc \
    src/chess/chess.cpp src/host_tests.cpp -o tests && ./tests
```

Les tests couvrent : **perft**, roque, prise en passant, promotion, détection de
mat, et l'intégrité make/unmake. Le **perft** (comptage des coups légaux depuis
la position de départ) est une référence de correction absolue :
`perft(1)=20, perft(2)=400, perft(3)=8902, perft(4)=197281, perft(5)=4865609`.

### Sur ESP32 — firmware (avec PlatformIO)

```bash
pio run -e esp32dev              # compiler le firmware
pio run -e esp32dev -t upload    # flasher l'ESP32
pio device monitor               # console série (115200 bauds)
```

---

## 🎮 Utilisation (console série, 115200 bauds)

Au démarrage, l'ESP32 ramène le portique à l'origine, initialise une nouvelle
partie et attend votre coup (vous jouez les Blancs).

```
e2e4         votre coup (notation case de départ -> case d'arrivée)
e7e8q        coup avec promotion (q = dame, r = tour, b = fou, n = cavalier)
level 2      fixer le niveau (0=EASY 1=MEDIUM 2=HARD 3=EXPERT)
new          nouvelle partie
show         réafficher le plateau
```

Le bouton poussoir (broche `BUTTON_PIN`) cycle lui aussi les niveaux de
difficulté, sans passer par la console.

---

## ✅ État de vérification

- **Moteur d'échecs** : validé par perft (profondeurs 1 à 5 exactes) et par
  comparaison avec un générateur de coups indépendant ; 19/19 tests unitaires
  passent.
- **Firmware** : compile et linke proprement (vérifié sur machine hôte avec un
  stub Arduino). Le flash réel sur ESP32 n'a pas pu être effectué dans
  l'environnement de développement (PlatformIO/réseau indisponible).

---

## 🗺️ Pistes d'amélioration

- Brancher la **détection automatique** du coup humain (matrice de capteurs
  Reed / I2C multiplexeur) pour se passer de la console.
- Ajouter des **fins de course** (le `home()` actuel est un « soft home » sans
  butée matérielle).
- Gérer les **promotions côté humain** par sélection matérielle (bouton/encodeur)
  plutôt que par suffixe texte.
- Optimiser le moteur (table de transposition, livre d'ouverture) pour monter en
  profondeur sur l'ESP32.
