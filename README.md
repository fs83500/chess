# Plateau d'échecs automatisé (ESP32)

Un plateau d'échecs où les pièces se déplacent toutes seules, propulsé par un
ESP32 qui fait tourner un petit moteur d'échecs (« mini IA ») avec plusieurs
niveaux de difficulté. L'ESP32 joue le rôle du bot ; vous jouez les Blancs.

## Principe matériel

* Les pièces ont une pastille ferromagnétique sous leur feutrine.
* Un **électro-aimant** se déplace sous le plateau sur un portique **2 axes**
  (2 moteurs pas-à-pas, X = colonnes a‑h, Y = rangées 1‑8).
* Pour bouger une pièce : l'aimant va au‑dessus de la case de départ, s'active
  pour « attraper » la pièce, la traîne jusqu'à la case d'arrivée, puis
  s'éteint pour la relâcher.
* Les pièces capturées sont traînées vers une « poche » de parking sur le côté.

Tout ceci est piloté par la couche `src/hardware/motion.*` (broches à adapter
dans `motion.h`). Le firmware complet est dans `src/main.cpp`.

> Note : la détection automatique du coup humain (via capteurs Reed sous chaque
> case) n'est pas câblée dans cette version ; le coup est saisi sur le port
> série (`e2e4`). La structure permet de brancher facilement la détection
> matérielle en remplaçant la lecture série.

## Logiciel

* `src/chess/chess.{h,cpp}` — moteur d'échecs portable (génération légale,
  make/unmake, roque, prise en passant, promotion, recherche alpha-bêta avec
  quiescence et tables piece-square). **Aucune dépendance Arduino**, testable
  sur PC.
* `src/hardware/motion.{h,cpp}` — pilotage du portique et de l'aimant (ESP32).
* `src/main.cpp` — firmware : boucle de jeu, niveaux de difficulté, exécution
  physique des coups du bot.
* `src/host_tests.cpp` — tests unitaires du moteur (perft, roque, en passant,
  promotion, mat, intégrité make/unmake).

### Niveaux de difficulté

| Niveau   | Profondeur | Aléatoire |
|----------|-----------|-----------|
| EASY     | 1         | fort      |
| MEDIUM   | 2         | léger     |
| HARD     | 3         | aucun     |
| EXPERT   | 4         | aucun     |

Le bot sélectionne parmi les coups quasi‑optimaux avec une composante aléatoire
plus ou moins forte selon le niveau (plus le niveau est faible, plus il peut
choisir un coup légèrement sous‑optimal).

## Compiler / tester

### Sur PC (tests du moteur, sans matériel)
```bash
make test            # compile et lance les tests unitaires
# ou directement :
g++ -std=gnu++17 -O2 -DUNIT_TEST -Isrc src/chess/chess.cpp src/host_tests.cpp -o tests && ./tests
```
Les tests vérifient notamment les valeurs de **perft** (comptage de coups légaux)
depuis la position de départ, qui sont des références connues :
`perft(1)=20, perft(2)=400, perft(3)=8902, perft(4)=197281, perft(5)=4865609`.

### Sur ESP32 (avec PlatformIO)
```bash
pio run -e esp32dev              # compiler le firmware
pio run -e esp32dev -t upload    # flasher
pio device monitor               # console série (115200 bauds)
```

## Utilisation (console série)

```
e2e4         -> jouer votre coup (notation case de départ -> case d'arrivée)
e7e8q        -> coup avec promotion (q/r/b/n)
level 2      -> fixer le niveau (0=EASY 1=MEDIUM 2=HARD 3=EXPERT)
new          -> nouvelle partie
show         -> réafficher le plateau
```
Le bouton poussoir (broche `BUTTON_PIN`) cycle aussi les niveaux de difficulté.

## Câblage (à adapter dans `src/hardware/motion.h`)

| Fonction          | ESP32 GPIO |
|-------------------|-----------|
| X STEP / DIR / EN | 14 / 27 / 26 |
| Y STEP / DIR / EN | 25 / 33 / 32 |
| Électro-aimant    | 13 |
| Bouton niveau     | 34 |

Les drivers de moteurs pas-à-pas (A4988/DRV8825/TMC2209) se branchent
classiquement : `STEP`, `DIR`, `EN`, plus `VMOT`/`GND` en alimentation moteur
séparée. L'électro-aimant se commande via un transistor MOSFET (le GPIO ne peut
pas piloter directement un aimant).
