# FM1 ROOTS DUB — audit d’intégration DSP

Branche : `roots-dub-dev`  
Base examinée : SLOOP 2.8.5  
Nature : audit statique des sources, sans compilation ni test matériel.

## Décision technique

La persistance FUN7 et les commandes SysEx ROOTS DUB 78/79 sont maintenant implémentées, avec les curseurs correspondants dans l’éditeur web v13. En revanche, le prototype LR4 reste autonome et n’est pas raccordé au moteur audio. Le chemin audio est très intégré et le budget temps réel doit être mesuré : aucune insertion dans le bus master ne doit être faite avant validation des tests fixes, de la fréquence d’échantillonnage, du coût CPU et des transitoires.

La suite est donc limitée à la validation logicielle et à la mesure sur cible ; l’interface et la persistance ne sont plus des travaux à venir pour cette première étape.

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

`project.c` utilisait le format 6 (FUN6) au moment de cet audit initial. La branche actuelle écrit FUN7 et conserve FUN6 comme structure historique de lecture. FUN6 stockait 32 globals historiques dans `g[]`, puis des champs dédiés (dont le délai drums et la texture), et séparait les paramètres de piste en trois groupes : paramètres 16 bits historiques, paramètres ajoutés en octets, et paramètres moteur.

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

Cette proposition décrivait les précautions à prendre avant d'ajouter des paramètres. Elles ont depuis été traitées pour les projets avec FUN7 : le format est versionné, FUN6 reste lisible et les anciens formats reçoivent des gains neutres. Les presets utilisateur et les paramètres moteur ne sont pas modifiés par cette extension.

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

### Persistance et commandes ROOTS DUB — état actuel

Le format FUN7 ajoute 12 octets au format FUN6 historique : quatre gains Q15, un indicateur d'activation et trois octets réservés. FUN6 reste une structure de lecture immuable ; les imports FUN1 à FUN6 initialisent l'isolateur à l'état neutre (désactivé, gains unitaires). FUN7 est utilisé pour les captures, les sections, l'autosave et les sauvegardes/restaurations projet.

Les commandes SysEx 78/79 sont exposées par le protocole v13. Elles lisent et modifient les réglages runtime, que la capture normale du projet persiste ensuite. L'éditeur web v13 affiche les quatre curseurs SUB/BASS/MIDS/TOPS et l'activation ; les versions antérieures masquent ce panneau.

Ces réglages ne pilotent pour l'instant que les données du projet. Le DSP LR4 n'est pas connecté au mixage : déplacer les curseurs ne modifie donc pas encore le son du FM-1.

### Décision d'ingénierie

Le module `roots_lr4_isolator.h` reste autonome pour le moment. La persistance FUN7, les commandes SysEx et les curseurs web sont déjà implémentés ; ils ne pilotent toutefois pas encore le DSP. L'insertion audio est reportée jusqu'à validation des tests, confirmation de la fréquence d'échantillonnage et mesure CPU sur cible. Les gains par bande devront également être lissés pour éviter les clics lors des changements.


## 2026-11-05 — décision DSP après simulation fréquentielle

La simulation du module `roots_isolator.h` (44,1 kHz, sinusoïdes établies) révèle un recouvrement trop important pour un isolateur de performance : à 350 Hz, MIDS atteint environ 1,163 fois l'amplitude d'entrée lorsqu'elle est seule ; à 1 kHz, TOPS reçoit encore environ 0,643. L'unité des quatre gains recompose le signal, mais cela ne garantit pas une séparation correcte lors de l'atténuation de bandes. Le module reste donc expérimental et ne doit pas être raccordé au master bus en l'état. Il faut d'abord remplacer la topologie par un crossover à pentes contrôlées, puis tester phase, sommation, crêtes et les combinaisons de bandes. Simulation documentée dans `ROOTS-DUB-ISOLATOR-DSP.md`.

Le format FUN7 est maintenant implémenté : 12 octets de configuration Roots Dub ajoutés au FUN6, taille attendue 3 828 octets ; FUN5 impose que l'union de staging reste à 3 840 octets. Une structure FUN6 historique distincte et des tests hôte de migration sont présents ; ils restent à exécuter dans un checkout complet.


## Mise à jour — 2026-11-05, prototypes LR4 et FUN7

Les prototypes ont progressé depuis cet audit initial :

- Le module autonome `roots_lr4_isolator.h` remplace désormais la topologie à un pôle pour les essais de séparation : trois crossovers LR4 fixes Q30, un all-pass par crossover ultérieur, et états internes Q8 pour limiter les erreurs de quantification.
- Le laboratoire Web Audio `roots-dub-lab.html` applique lui aussi cette compensation : un all-pass à 350 Hz puis un à 2,5 kHz sur SUB ; un all-pass à 2,5 kHz sur BASS.
- `tests/roots_lr4_isolator_test.c` vise la reconstruction unitaire à ±0,5 % et balaie les 15 masques non vides sur une grille de fréquences. Un défaut de portée de la grille a été corrigé.
- `web/test_web.mjs` contient maintenant des contrôles statiques de la topologie all-pass et de la syntaxe JavaScript du laboratoire.
- Le format FUN7 est implémenté sur la branche avec migration FUN1..FUN6 et gains Roots neutres pour les anciens projets.

Ces ajouts ne changent pas la décision d'intégration : aucun de ces tests n'a encore été exécuté dans un checkout complet, aucun build firmware/WASM n'a été produit et aucun essai sur FM-1 n'a eu lieu. Le LR4 reste isolé du bus master. Les contrôles web ajoutés doivent être exécutés avec `node web/test_web.mjs` dans un environnement disposant des dépendances/outils du dépôt.

Vérification complémentaire : `firmware/src/core.h` définit `FS` à 44 100 Hz par défaut (avec possibilité de surcharge à la compilation), ce qui correspond aux coefficients fixes du prototype LR4. La fréquence nominale n'est donc plus une inconnue pour la configuration standard ; il reste à vérifier qu'aucune configuration de build ne surcharge `FS`, puis à mesurer le coût CPU, les crêtes et les transitoires sur la cible. Cette vérification statique ne remplace pas un build ni un essai audio.


### 2026-11-05 — transitions et séparation LR4

Le prototype LR4 a été renforcé avant toute intégration :
- correction de la compensation : un biquad all-pass du second ordre par crossover ultérieur, pas deux en cascade ;
- états de biquad conservés en Q8 pour éviter la forte erreur de quantification constatée avec des états entiers ;
- plage interne étendue à +/-1048575 en unité audio pour préserver la marge du bus ;
- activation/désactivation avec fondu dry/wet de 64 échantillons ;
- retour au bypass bit-identique après extinction complète ;
- remise à zéro des historiques de biquads à la réactivation, pour ne pas réutiliser des états périmés ;
- assertions de réponse individuelle : SUB sous 120 Hz, BASS entre 120 et 350 Hz, MIDS entre 350 Hz et 2,5 kHz, TOPS au-dessus de 2,5 kHz ;
- maintien des tests de somme unitaire, des 15 masques, du mute et du stress pleine échelle.

Ces tests sont ajoutés au code mais ne sont pas exécutés dans cet environnement. Le moteur reste volontairement non raccordé à `mix_block()` tant que la mesure CPU sur cible, la validation fixe-point et les tests FM-1 ne sont pas disponibles.


### 2026-11-05 — cas limites backup et stockage A/B

La passe complémentaire a confirmé que `tests/storage_test.c` couvre déjà les coupures simulées pendant l'écriture du payload, avant le commit header, la corruption du payload récent avec repli sur l'ancienne copie, ainsi que l'écriture d'un objet vide. Ces tests portent sur le moteur de stockage A/B simulé ; ils ne simulent pas à eux seuls le parseur BK_PUT ni une restauration complète de projet.

Ajouts de cette passe :
- `tests/project_test.c` rejette explicitement FUN7 tronqué d'un octet, FUN7 avec un octet surnuméraire et une longueur négative ; les tests FUN6 tronqué et checksum invalide existaient déjà.
- `tests/storage_test.c` vérifie qu'un payload de 3 840 octets est sauvegardé/relu sans altération et qu'un payload de 3 841 octets est refusé.
- Le chemin de restauration éditeur `project_restore()` n'a pas de test hôte direct : il dépend de l'état flash et de l'application du projet. Les tests existants couvrent séparément l'import des formats et les transactions A/B, pas leur orchestration de bout en bout.

Ces tests ont été ajoutés au dépôt mais ne sont pas compilés/exécutés ici. Il reste nécessaire de lancer le banc complet, puis de tester des backups FUN6/FUN7 réels sur appareil avant fusion.


### Relecture de sûreté des nouveaux tests

La relecture du test de dépassement a aussi corrigé son propre buffer : la source de test est maintenant allouée à 3 841 octets, de sorte que le scénario « taille supérieure à 3 840 » ne transmet pas un pointeur vers un tableau trop court. Le buffer de lecture reste limité à 3 840 octets.

La restauration BK_PUT complète n'a volontairement pas été simulée par un faux test superficiel. `ed_backup()` dépend de l'éditeur, du staging partagé, de l'état transport, de la flash et des fonctions d'application UI/audio. Pour la tester correctement, il faut un banc d'intégration qui compile réellement cette unité avec des doubles cohérents de la flash et du runtime. En attendant, la vérification statique confirme l'ordre des contrôles : longueur/CRC de transfert, dispatch d'objet, puis validation/migration projet avant écriture flash. Les retours 2 (données invalides), 3 (transport actif) et 4 (flash/écriture indisponible) sont préservés.


## Complément — banc hôte de restauration BK_PUT (2026-11-05)

La relecture approfondie a retrouvé un banc d'intégration existant, plus complet que le test isolé initialement envisagé :

- `tests/editor_host.c` inclut les vrais `storage.c`, `upreset.c`, `project.c` et `editor.c`, dans l'ordre de compilation firmware pertinent, avec `FELUCCA_FLASH=1`, `FELUCCA_OTA=1` et une image NOR de 1 Mio en RAM.
- Les opérations erase/program respectent les contraintes NOR (bits seulement effaçables de 1 vers 0, pages de 256 octets) et disposent d'une injection de panne `fl_fail_at`.
- `tests/editor_fuzz.c` en mode 6 exerce réellement LIST/GET/PUT/commit pour l'objet 0, restaure via le protocole SysEx, compare le projet capturé octet par octet, puis teste les sections et l'autosave/reprise.
- Le mode 1 du même fuzzer génère aussi des contenus de sauvegarde de projets et les soumet au véritable chemin BK_PUT ; il mêle objets valides, migrés, mutés et transferts interrompus.
- `tests/editor_fuzz.sh` compile ce banc sous AddressSanitizer et UndefinedBehaviorSanitizer, puis lance notamment les modes 1 et 6.

Cette découverte a permis d'ajouter une suite déterministe dédiée, décrite ci-dessous. Les scénarios ciblés ne sont donc plus seulement des travaux à faire : ils sont implémentés dans `tests/editor_bkput_test.c`, qui réutilise le banc réel. Ils restent toutefois non validés tant que la compilation et l'exécution sous ASan/UBSan n'ont pas été réalisées dans un environnement disposant des artefacts générés par `./build.sh`.

### 2026-11-05 — régressions BK_PUT déterministes ajoutées

Une suite dédiée `tests/editor_bkput_test.c` réutilise le banc d'intégration réel de `tests/editor_fuzz.c` / `tests/editor_host.c` et ajoute des scénarios reproductibles :

- restauration SysEx d'un backup FUN6 construit avec le layout historique immuable, puis vérification de sa migration FUN7 et de l'initialisation neutre des quatre gains ROOTS ;
- restauration SysEx FUN7 valide avec quatre gains distincts (dont zéro et unity), vérification de l'application effective des paramètres, puis remise en place du projet initial pour isoler les scénarios suivants ;
- transfert FUN7 avec CRC annoncé incorrect : le commit doit renvoyer le code 2 et le projet de travail doit rester identique ;
- transfert FUN7 dont le CRC de transport est correct mais dont le checksum interne est invalide : le commit doit renvoyer le code 2 et le projet de travail doit rester identique ;
- transfert incomplet : le commit doit renvoyer le code 2 et ne pas modifier le projet ;
- restauration d'une section avec panne injectée au premier programme flash : le commit doit renvoyer le code 4, la copie RAM ne doit pas bouger et le chargement depuis flash doit retrouver l'ancienne copie A/B.

Le script `tests/editor_fuzz.sh` compile et exécute désormais ce test déterministe sous AddressSanitizer et UndefinedBehaviorSanitizer avant les campagnes fuzz. Ces ajouts sont en attente de compilation/exécution dans un environnement équipé des artefacts générés par `./build.sh` ; ils ne constituent pas encore un résultat de test validé.
