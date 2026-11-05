# FM1 ROOTS DUB — audit d’intégration DSP

Branche : `roots-dub-dev`  
Base examinée : SLOOP 2.8.5  
Nature : audit statique des sources, sans compilation ni test matériel.

## Décision technique

Ne pas ajouter encore de paramètres persistants ni brancher le prototype web sur le moteur. Le chemin audio est déjà très intégré : les paramètres sont exposés par le protocole SysEx, les projets et presets ont des formats binaires historiques, et le moteur tourne dans un budget temps réel serré. Une modification naïve peut casser la compatibilité des projets, user presets, contrôleurs web et builds FM-1.

La prochaine intégration doit être incrémentale : d’abord un module DSP autonome et testable, ensuite son insertion dans le bus master, puis seulement les contrôles et la sauvegarde.

## Ce que les sources confirment

### 1. Le chemin audio existe déjà

Dans `firmware/src/fx.c` :

- chaque partie synthé est rendue par `mix_part_run()`;
- DIST et INSERT sont appliqués sur chaque piste;
- le filtre de piste `P_TFLT` intervient après le slicer et avant le niveau/pan;
- chaque piste alimente les bus chorus, delay et reverb;
- les bus sont calculés dans `fx_buses()`;
- le mix stéréo passe ensuite par le ducking, DUST, le filtre DJ global `G_FILT`, le traitement master, le limiteur et le knee de sortie.

Conséquence : le point d’insertion le plus cohérent pour un isolateur général est dans `mix_block()`, après `dust_process()`, `punch_process()` et `djf_process()`, mais avant l’application du MASTER et l’appel à `lim_block()`. À cet endroit, il traite le mix complet, les retours d’effets et la texture, sans modifier les sends individuels. Le mettre dans chaque piste changerait le comportement des sends et multiplierait inutilement le coût CPU.

### 2. Le DSP actuel est majoritairement entier/fixed-point

Le code de mix utilise des accumulateurs `int32_t`, coefficients Q12/Q15 et des états persistants. Le nouveau traitement doit suivre cette convention, borner les accumulations et éviter les flottants dans l’interruption audio. Le prototype HTML utilise Web Audio et des filtres natifs : il sert de maquette sonore/ergonomique, pas de code à copier directement dans le firmware.

### 3. Les paramètres sont une interface publique persistante

- `P_COUNT` et `G_COUNT` déterminent les tableaux et les commandes `INFO/DESC/GET/SET/DUMP` de `editor.c`.
- Les paramètres de piste sont enregistrés dans les projets et user presets.
- `upreset.c` a une capacité fixe de 72 paramètres et un enregistrement de 192 octets; le nombre courant doit rester dans cette limite.
- Les paramètres globaux ne sont pas tous stockés de façon uniforme : certains réglages sont des préférences de l’appareil, d’autres sont conservés via des octets de réserve ou des champs détournés.

**Règle : aucun identifiant existant ne doit être renuméroté.** Les ajouts de paramètres doivent être append-only et accompagnés d’une mise à jour explicite des formats de stockage et de leurs tests.

### 4. Les projets ont un format compact versionné

`project.c` utilise actuellement le format 6 (`FUN6`). Il conserve 32 globals historiques dans `g[]`, puis des champs dédiés (dont le délai drums et la texture), et sépare les paramètres de piste en trois groupes : paramètres 16 bits historiques, paramètres ajoutés en octets, et paramètres moteur.

Des assertions statiques imposent notamment :

- `G_DRDLY == PROJ_NG`;
- `G_TXSRC == PROJ_NG + 5`;
- `G_FXFADE` est le dernier global;
- le bloc de paramètres compacts de piste a exactement 10 octets.

Cela interdit de supposer qu’un simple `G_COUNT++` ou `P_COUNT++` sera sauvegardé correctement. Une évolution persistante exige un nouveau format de projet (et les conversions de lecture des formats anciens) ou une utilisation documentée d’un champ de réserve réellement disponible, avec preuve par tests de compatibilité.

### 5. Le preview navigateur peut utiliser le vrai moteur

`firmware/host/preview.c` inclut directement les mêmes sources C principales, dont `fx.c`, `params.c`, `voice.c` et `seq.c`. `tools/build_preview.py` construit le WASM à partir de ce moteur. C’est la bonne voie pour remplacer progressivement le prototype Web Audio par un aperçu cohérent avec le firmware. Il faut reconstruire le WASM depuis les sources; le fichier WASM déjà commité ne prouve pas qu’il correspond aux nouveaux changements.

## Architecture proposée pour l’Isolator ROOTS DUB

### Emplacement

Un seul traitement stéréo sur le bus master, inséré dans `mix_block()` après `dust_process()`, `punch_process()` et `djf_process()`, puis avant le gain MASTER et le limiteur. Ce point a été confirmé par lecture du chemin complet de `mix_block()`.

### Banque de filtres

- Quatre bandes complémentaires : SUB, BASS, MIDS, TOPS.
- Trois fréquences de coupure communes, configurables plus tard.
- Même réseau de coupure pour gauche et droite.
- Gains indépendants par bande, avec valeurs neutres au démarrage.
- États persistants par canal et par section de filtre.
- Coefficients calculés hors boucle d’échantillons ou pré-calculés au changement de fréquence.
- Saturation/bornage des gains et des états pour éviter les débordements.
- Bypass strict : lorsque l’isolateur est désactivé, la sortie doit être bit-identique au signal d’entrée et les états ne doivent pas créer de clic à la réactivation.

Le prototype utilise des cascades de sections Butterworth en Web Audio. Avant d’adopter exactement cette topologie en fixed-point, mesurer la somme des bandes autour des coupures, la réponse en phase, le gain de crête, le coût CPU et les cas extrêmes. La somme complémentaire ne doit pas être présumée parfaite simplement parce que les bandes ont des fréquences de coupure identiques.

### Paramètres et commandes

Ne pas exposer de nouveaux paramètres persistants avant d’avoir arrêté le format de stockage. Première étape : module avec configuration de test interne, bypass par défaut, et tests unitaires. Ensuite choisir entre :
1. extension versionnée des projets et des presets;
2. paramètres temporaires non sauvegardés pour valider l’ergonomie;
3. réutilisation de champs de réserve uniquement après preuve qu’ils sont libres sur tous les formats concernés.

## Dub Siren et One-Key Skank

- **Dub Siren** : réutiliser les oscillateurs, enveloppes, LFO et delay déjà présents; éviter d’ajouter un second moteur audio parallèle. Le prototype reste une référence d’interface jusqu’à l’identification d’un moteur existant qui peut accueillir la sirène sans casser les presets.
- **One-Key Skank** : s’appuyer sur le moteur d’accords/voicings et la logique de séquence existants. Le mode doit être un comportement de jeu clairement défini, pas un accord figé qui contourne la gamme, la transposition ou le séquenceur.
- **Tempo** : le LFO 2 synchronisé existe déjà; le réutiliser au lieu d’introduire une horloge indépendante.

## Plan de validation obligatoire

Avant de considérer une intégration comme prête :

1. Compiler le firmware avec la toolchain officielle.
2. Exécuter `tests/run_tests.sh` et examiner les tests golden audio : tout changement sonore attendu doit être justifié et les golden mis à jour volontairement.
3. Construire le preview WASM avec `python3 tools/build_preview.py`.
4. Exécuter `tests/run_preview_tests.sh`.
5. Ajouter des tests dédiés à l’isolateur : réponse par bande, somme à plat, phase/transition, silence, signal extrême, bypass bit-identique, changement de fréquence sans clic.
6. Vérifier la taille mémoire, le budget CPU et la latence.
7. Tester sur FM-1 réel : audio, commandes, sauvegarde/chargement de projets et user presets.

## État de validation

- Audit des sources GitHub : effectué.
- Prototype Web Audio : présent dans la branche, avec limites documentées.
- Compilation firmware : non effectuée.
- Tests C hôte : non exécutés.
- Build WASM depuis cette branche : non effectué.
- Écoute navigateur / appareil : non effectuée.
- Flash et test FM-1 : non effectués.

Aucune affirmation de fonctionnement matériel n’est faite.


## Relevé complémentaire — vrai point d'insertion et sauvegarde

L'inspection de `mix_block()` dans `firmware/src/fx.c` confirme l'ordre exact :

1. Les unités audio des deux cœurs sont additionnées dans `acc[0]`.
2. TEXTURE et les retours CHORUS / DELAY / REVERB sont ajoutés.
3. DUST, PUNCH et le filtre DJ global sont appliqués.
4. Le mix stéréo passe ensuite dans le volume MASTER.
5. `master_pre()`, le limiteur et le knee final alimentent le DAC.
6. Le chemin USB FULL repart du mix avant MASTER et possède ses propres états de pré-traitement et son limiteur.

**Point d'insertion recommandé :** juste après `djf_process(mix_l, mix_r, n)`, avant la mise à l'échelle MASTER. À cet endroit, l'isolateur traite le mix stéréo complet, reste indépendant du volume principal et agit aussi sur USB FULL et sur le signal du visualiseur. Il ne doit pas être placé dans les unités parallèles ni dans `master_pre()`, qui est appelé séparément pour DAC et USB.

### Pourquoi les commandes et la sauvegarde ne sont pas encore ajoutées

Les paramètres globaux ne sont pas tous persistés de la même façon :

- `G_COUNT` comprend aussi des réglages système qui ne sont pas stockés dans les projets.
- Le format projet 6 (`FUN6`) stocke explicitement 32 globals historiques dans `PROJ_NG`, puis des champs réservés et des extensions séparées.
- Le format 6 a été dimensionné pour tenir dans un objet flash de 3840 octets; sa structure et ses conversions historiques sont protégées par des assertions de taille.
- Les paramètres de piste sont append-only et les huit paramètres moteur `P_E0..P_E7` doivent rester en fin de liste.

Une intégration propre des quatre gains doit donc introduire un nouveau format de projet versionné (format 7 ou extension latérale explicitement versionnée), conserver le chargement des formats 1 à 6, mettre à jour l'autosave et le backup/restore de l'éditeur, puis ajouter les commandes UI/SysEx. Réutiliser arbitrairement des globals existants ou déplacer des IDs serait une régression de compatibilité.

### Décision d'ingénierie

Le module `roots_isolator.h` reste autonome pour le moment. L'insertion DSP et les contrôles seront réalisés ensemble, après le schéma de persistance et la version projet définis et testables. Cela évite un isolateur qui marche en RAM mais perd ses réglages à l'arrêt, ou qui change les projets anciens.


## 2026-11-05 — décision DSP après simulation fréquentielle

La simulation du module `roots_isolator.h` (44,1 kHz, sinusoïdes établies) révèle un recouvrement trop important pour un isolateur de performance : à 350 Hz, MIDS atteint environ 1,163 fois l'amplitude d'entrée lorsqu'elle est seule ; à 1 kHz, TOPS reçoit encore environ 0,643. L'unité des quatre gains recompose le signal, mais cela ne garantit pas une séparation correcte lors de l'atténuation de bandes. Le module reste donc expérimental et ne doit pas être raccordé au master bus en l'état. Il faut d'abord remplacer la topologie par un crossover à pentes contrôlées, puis tester phase, sommation, crêtes et les combinaisons de bandes. Simulation documentée dans `ROOTS-DUB-ISOLATOR-DSP.md`.

Le format FUN7 est maintenant spécifié plus précisément : 12 octets de configuration Roots Dub ajoutés au FUN6, taille attendue 3 828 octets ; FUN5 impose que l'union de staging reste à 3 840 octets. L'implémentation binaire reste à faire avec une structure FUN6 historique distincte et des tests hôte de migration.


## Mise à jour — 2026-11-05, prototypes LR4 et FUN7

Les prototypes ont progressé depuis cet audit initial :

- Le module autonome `roots_lr4_isolator.h` remplace désormais la topologie à un pôle pour les essais de séparation : trois crossovers LR4 fixes Q30, un all-pass par crossover ultérieur, et états internes Q8 pour limiter les erreurs de quantification.
- Le laboratoire Web Audio `roots-dub-lab.html` applique lui aussi cette compensation : un all-pass à 350 Hz puis un à 2,5 kHz sur SUB ; un all-pass à 2,5 kHz sur BASS.
- `tests/roots_lr4_isolator_test.c` vise la reconstruction unitaire à ±0,5 % et balaie les 15 masques non vides sur une grille de fréquences. Un défaut de portée de la grille a été corrigé.
- `web/test_web.mjs` contient maintenant des contrôles statiques de la topologie all-pass et de la syntaxe JavaScript du laboratoire.
- Le format FUN7 est implémenté sur la branche avec migration FUN1..FUN6 et gains Roots neutres pour les anciens projets.

Ces ajouts ne changent pas la décision d'intégration : aucun de ces tests n'a encore été exécuté dans un checkout complet, aucun build firmware/WASM n'a été produit et aucun essai sur FM-1 n'a eu lieu. Le LR4 reste isolé du bus master. Les contrôles web ajoutés doivent être exécutés avec `node web/test_web.mjs` dans un environnement disposant des dépendances/outils du dépôt.


### 2026-11-05 — transitions et séparation LR4

Le prototype LR4 a été renforcé avant toute intégration :
- activation/désactivation avec fondu dry/wet de 64 échantillons ;
- retour au bypass bit-identique après extinction complète ;
- remise à zéro des historiques de biquads à la réactivation, pour ne pas réutiliser des états périmés ;
- assertions de réponse individuelle : SUB sous 120 Hz, BASS entre 120 et 350 Hz, MIDS entre 350 Hz et 2,5 kHz, TOPS au-dessus de 2,5 kHz ;
- maintien des tests de somme unitaire, des 15 masques, du mute et du stress pleine échelle.

Ces tests sont ajoutés au code mais ne sont pas exécutés dans cet environnement. Le moteur reste volontairement non raccordé à `mix_block()` tant que la mesure CPU sur cible, la validation fixe-point et les tests FM-1 ne sont pas disponibles.
