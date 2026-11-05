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


## Conception de remplacement : crossover Linkwitz–Riley 4e ordre

La prochaine version doit remplacer les trois séparations à un pôle par une
cascade de crossovers LR4. Pour chaque fréquence de coupure (120, 350 et
2 500 Hz), construire un passe-bas et un passe-haut LR4, chacun composé de
deux biquads Butterworth identiques (`Q = 1/sqrt(2)`). Le passe-bas devient
la bande courante ; le passe-haut alimente le séparateur suivant :

```
entrée -> LR4 @ 120 Hz -> SUB (LP) + reste (HP)
reste  -> LR4 @ 350 Hz -> BASS (LP) + reste (HP)
reste  -> LR4 @ 2.5 kHz -> MIDS (LP) + TOPS (HP)
```

Pour un biquad bilinéaire à fréquence d'échantillonnage `fs`, poser
`K = tan(pi * fc / fs)`, `D = 1 + sqrt(2)*K + K*K`.
Les coefficients normalisés sont :

- passe-bas : `b0=K*K/D, b1=2*b0, b2=b0`
- passe-haut : `b0=1/D, b1=-2*b0, b2=b0`
- les deux : `a1=2*(K*K-1)/D, a2=(1-sqrt(2)*K+K*K)/D`

Chaque biquad est exécuté deux fois en cascade pour obtenir LR4. Quantifier
les coefficients en Q30 (ou Q28 si les mesures CPU le justifient), garder les
états en 64 bits si nécessaire, et saturer les conversions vers l'audio
32 bits. Calculer les coefficients hors de l'ISR : ils sont constants à 44,1
kHz pour cette première version.

### Structure et coût à mesurer

Trois séparateurs × (deux biquads passe-bas + deux passe-haut) = 12 biquads
par canal, donc 24 pour la stéréo. Un biquad direct-form II transposé utilise
deux états par section. Prévoir deux canaux totalement indépendants. Le coût
théorique est de l'ordre de 120 multiplications par échantillon stéréo (selon
l'implémentation et les opérations fusionnées), à mesurer sur le FM-1 avant
d'activer l'effet. Si le coût est trop élevé, optimiser sans modifier la
réponse mesurée.

### Tests d'acceptation avant raccordement

1. Réponse de la somme des quatre bandes à gains unitaires : écart inférieur
   à ±0,1 dB sur 20 Hz–20 kHz (hors limites de quantification).
2. Pentes d'atténuation conformes au LR4 : environ 24 dB/octave de chaque
   côté de chaque coupure, sans bosse excessive à la coupure.
3. Vérifier les 15 combinaisons non vides de bandes (gain 0 ou 1), en réponse
   fréquentielle et en crête temporelle.
4. Vérifier la stabilité après silence, transitions bypass/enable, et
   changement de gain sans clic (rampe de gain par bloc).
5. Mesurer la consommation CPU et RAM sur la cible, ainsi que la marge de
   crête avec les quatre bandes à +gain maximal autorisé.
6. Garder un bypass strictement bit-identique quand l'effet est désactivé.

Cette conception est une spécification de travail, pas encore une
implémentation validée. Le prototype actuel reste séparé du bus maître.


## Prototype LR4 à coefficients fixes (non intégré)

Un second module expérimental, `firmware/src/roots_lr4_isolator.h`, met en
œuvre les trois séparateurs avec des biquads Butterworth en Q30 et une forme
directe I à accumulateur 64 bits. Il fournit deux sections passe-bas et deux
passe-haut par séparateur, un état indépendant par canal, des gains Q15 et un
bypass exact. Le test associé est `tests/roots_lr4_isolator_test.c`.

**Réserve importante découverte pendant le calcul de la cascade :** même si
chaque séparateur LR4 isolé est complémentaire en amplitude, la cascade
successive donne aux bandes BASS et MIDS une phase supplémentaire due aux
séparateurs situés plus haut. La somme des quatre bandes n'est donc pas plate
à ±0,1 dB sur toute la plage. Le test du prototype vérifie une enveloppe
large (environ -1,5 dB à +0,2 dB sur les fréquences échantillonnées), pas une
spécification de produit. Une compensation de phase/all-pass ou une autre
topologie multibande est nécessaire avant toute intégration au master bus.
Les mesures restent des tests logiciels à 44,1 kHz, pas des mesures sur le
FM-1 physique.
