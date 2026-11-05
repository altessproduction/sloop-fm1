# FM1 ROOTS DUB — audit technique initial

Audit statique du dépôt SLOOP 2.8.5, effectué avant toute modification DSP. Les constats ci-dessous décrivent le code source lu sur `main`; ils ne constituent pas des résultats de compilation ou de test matériel.

## Briques déjà disponibles

| Besoin ROOTS DUB | Éléments SLOOP repérés | Conséquence |
| --- | --- | --- |
| Écho dub | Bus delay partagé dans `firmware/src/fx.c`; paramètres globaux `G_DTIME`, `G_DFDBK`, `G_DCOLOR`, `G_DMIX` décrits dans `params.c` | Réutiliser le delay et étudier une commande de throw/feedback, sans ajouter un second delay avant mesure. |
| Espace | Bus reverb `G_RSIZE`, `G_RDAMP` ; chorus `G_CRATE`, `G_CDEPTH` | Réutiliser les bus existants. |
| Texture vinyle | `G_DUST` : réduction de fréquence/bit, filtrage, bruit et craquements dans `fx.c` | Déjà adapté à une couleur lo-fi ; vérifier l'équilibre et les niveaux pour les presets roots. |
| Pompage au kick | `G_DUCK` et détection du kick dans `fx.c` | Brique existante à régler musicalement, plutôt que coder un sidechain séparé. |
| Filtre live | `G_FILT` sur le master et `P_TFLT` par piste ; SVF et glide dans `fx.c` | Base possible pour les mouvements de filtre dub. Ce n'est pas un isolateur 4 bandes. |
| Accords à une touche | `P_CHORD`, gamme/tonalité et logique d'accords dans `seq.c` | Étudier une extension skank rythmique sans casser le mode CHORD actuel. |
| Synchronisation | Tempo global, divisions rythmiques et LFO 2 synchronisé déjà présents | Réutiliser les unités et mécanismes existants. |

## Fichiers à examiner pour chaque chantier

- **Effets et mixage :** `firmware/src/fx.c`, puis les déclarations de paramètres dans `firmware/src/core.h` et `firmware/src/params.c`.
- **Accords, arpèges et séquenceur :** `firmware/src/seq.c`, `firmware/src/core.h`.
- **Voix et enveloppes :** `firmware/src/voice.c`, puis les moteurs dans `firmware/src/eng_*.c`.
- **Écran et commandes :** `firmware/src/ui*.c`, `firmware/src/panel.c`, `firmware/src/params.c`.
- **Éditeur et échanges :** `web/editor.html`, `web/EDITOR_PROTOCOL.md`, `firmware/src/editor.c`.
- **Aperçu navigateur :** `web/preview.js`, `web/preview-worklet.js` et le binaire `docs/webapp/editor/sloop-preview.wasm`. Le WASM est généré : il faudra identifier sa chaîne de construction avant de le remplacer.

## Risques de compatibilité à traiter

1. `core.h` indique explicitement que les paramètres de piste sont stockés dans les presets/projets et que les identifiants existants sont append-only. Ne pas insérer un paramètre au milieu des enums.
2. Les paramètres globaux et ceux des pistes n'ont pas nécessairement le même cycle de sauvegarde : certains sont des réglages FM-1, d'autres appartiennent au projet. Vérifier `project.c`, `upreset.c`, `storage.c` et les chemins de dump/restore avant tout ajout.
3. Le protocole éditeur indique que les commandes et données ajoutées doivent rester rétrocompatibles. Toute nouvelle commande SysEx doit être documentée et testée des deux côtés.
4. Un isolateur 4 bandes demande quatre bandes réellement séparées et une somme maîtrisée. Quatre simples contrôles de volume sur des bandes non séparées ne répondraient pas au besoin.
5. Le DSP est à virgule fixe et le FM-1 a un budget CPU/RAM limité. Un prototype doit être mesuré sur hôte, puis compilé pour la cible avant tout essai matériel.

## Ordre conseillé après cet audit

1. Construire et lancer les tests de référence de SLOOP avant changement, pour connaître l'état initial.
2. Prototyper l'isolateur hors du mix principal (test hôte), mesurer réponse fréquentielle, phase et coût CPU.
3. Définir ensuite le meilleur point d'insertion dans le bus master et les paramètres persistants.
4. Développer Dub Siren et One-Key Skank en petits changements séparés, chacun avec tests.
5. Ajouter les contrôles web après stabilisation des paramètres firmware et du protocole.
6. Générer le preview WASM depuis les mêmes sources DSP lorsque la chaîne existante le permet ; ne pas prétendre que l'aperçu est identique tant que cette parité n'est pas vérifiée.

## Limites de cet audit

Les sources ont été consultées via GitHub. Aucun environnement de compilation local n'a été exécuté dans le cadre de cet audit ; aucun firmware n'a été flashé et aucun FM-1 physique n'a été testé.
