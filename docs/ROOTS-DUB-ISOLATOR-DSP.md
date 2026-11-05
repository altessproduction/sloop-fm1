# ROOTS DUB isolator — standalone DSP module

This increment adds a fixed-point C implementation and a host test, without
connecting it to the FM-1 master path yet.

## Signal topology

The signal is split successively at 120 Hz, 350 Hz and 2.5 kHz. Each split
uses two cascaded one-pole low-pass sections. The high output is the exact
sample-domain residual (input minus low), so low plus high reconstructs the
input before band gains, apart from integer rounding. This complementary
residual topology avoids assuming independently designed low/high filters sum
flat. It is a conservative first implementation, not a Linkwitz–Riley or
steep 24 dB/octave DJ isolator.

All arithmetic is integer/fixed-point. Gains are Q15 (32768 = unity; 0 = mute).
The module clamps input/output to +/-131071 to keep accumulation bounded.
When disabled, it returns the input bit-for-bit and leaves filter state alone.

## Limitations before integration

- Coefficients are for 44.1 kHz only.
- Cutoff slopes are gentle (two cascaded one-pole sections).
- Mono per call: instantiate one state per stereo channel.
- Parameter controls, smoothing, persistence and master-bus hook are not added.
- The standalone test does not replace SLOOP's full regression suite.

## Host test

From repository root:

    cc -std=c99 -Wall -Wextra -Ifirmware/src tests/roots_isolator_test.c -o /tmp/roots_isolator_test
    /tmp/roots_isolator_test

Tests cover bit-identical bypass, DC reconstruction at unity, all-band mute,
TOPS rejection of DC, and bounded full-scale stress.


## Analyse de réponse fréquentielle (simulation du prototype)

Une simulation sinusoïdale à 44,1 kHz avec les coefficients actuels (`555, 1594, 9819`) confirme que les quatre bandes à gain unitaire se recomposent, mais révèle un recouvrement important lorsque l'on écoute une bande seule. Amplitudes de sortie estimées après établissement, rapportées à une sinusoïde d'entrée d'amplitude 1 :

| Fréquence | SUB | BASS | MIDS | TOPS |
|---:|---:|---:|---:|---:|
| 30 Hz | 0,941 | 0,467 | 0,078 | 0,002 |
| 120 Hz | 0,500 | 0,994 | 0,673 | 0,054 |
| 350 Hz | 0,105 | 0,541 | **1,163** | 0,272 |
| 1 kHz | 0,014 | 0,111 | 0,943 | 0,643 |
| 2,5 kHz | 0,002 | 0,019 | 0,514 | 0,978 |
| 5 kHz | 0,001 | 0,005 | 0,209 | 1,020 |

Ces chiffres sont une simulation du modèle de filtres, pas une mesure sur FM-1. Le dépassement MIDS à 350 Hz est un signal d'alarme : le prototype n'est pas encore un isolateur DJ à bandes bien séparées. Des gains de bande extrêmes peuvent également créer des crêtes supérieures à l'entrée, avant même le limiteur maître.

**Décision : ne pas raccorder cette topologie au master bus comme isolateur final.** Garder le module comme banc d'essai et concevoir une vraie séparation complémentaire à pentes contrôlées (par exemple crossover Linkwitz–Riley 4e ordre), puis mesurer sommation, phase, crêtes et réponse pour les 15 combinaisons de bandes avant intégration. Les tests actuels de bypass, reconstruction DC et bornage ne suffisent pas à qualifier un isolateur musical.
