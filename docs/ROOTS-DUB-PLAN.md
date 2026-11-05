# FM1 ROOTS DUB — plan d'intégration

**Branche de travail :** `roots-dub-dev`  
**Base :** SLOOP 2.8.5, branche `main`  
**Statut :** cadrage technique — aucune modification du moteur audio ni du firmware matériel à ce stade.

## Objectif

Faire évoluer SLOOP vers un instrument orienté reggae, dub et roots, sans retirer les fonctions existantes. Les ajouts doivent rester utilisables depuis les commandes du FM-1 et depuis l'éditeur web, avec une même définition des paramètres et des comportements audio.

## Inventaire confirmé dans le dépôt

- Le cœur firmware est dans `firmware/src/` ; les types, pistes et paramètres communs sont décrits dans `core.h`.
- Le traitement des effets partagés et des inserts est dans `fx.c`.
- Les descripteurs, plages, valeurs par défaut et libellés des paramètres sont dans `params.c`.
- L'éditeur web est dans `web/editor.html` et le protocole USB-MIDI/SysEx est documenté dans `web/EDITOR_PROTOCOL.md`.
- Le projet comporte déjà 3 pistes synthé, une piste batterie, 64 pas, 12 moteurs de synthèse et un moteur d'aperçu audio WebAssembly.
- Le dépôt contient une licence GPL-3.0 et des fichiers de crédits. Toute réutilisation doit conserver les avis de copyright, licences et attributions des composants concernés.

## Principes d'intégration

1. **Compatibilité des projets et presets.** Ne pas renuméroter les paramètres existants ni changer leur sens. Les nouveaux paramètres persistants doivent être ajoutés selon les règles append-only du projet, avec vérification des formats de sauvegarde et de restauration.
2. **DSP réel avant interface.** Un bouton n'est considéré fonctionnel que si son paramètre commande un traitement audio réel. Pas de contrôle décoratif ni de faux crossover.
3. **Même comportement web et FM-1.** Réutiliser les paramètres et le protocole existants ; étendre le protocole seulement si nécessaire et de façon rétrocompatible.
4. **Charge temps réel maîtrisée.** Mesurer cycles CPU, mémoire, latence et comportement en polyphonie. Éviter les allocations dynamiques dans le chemin audio.
5. **Licence.** Préserver GPL-3.0, les en-têtes SPDX et les attributions SLOOP/Felucca.

## Fonctionnalités ROOTS DUB visées

### 1. Dub Siren
Oscillateur dédié ou voix de synthèse configurable : forme d'onde, hauteur, enveloppe, modulation, filtre et niveau. Le déclenchement doit être possible depuis les contrôles existants et, si l'architecture le permet, depuis MIDI. Prévoir une modulation lente et des réglages adaptés aux sirènes dub, sans saturation numérique incontrôlée.

### 2. One-Key Skank
Mode de jeu où une touche déclenche un accord court, rythmiquement adapté au skank. Les intervalles doivent suivre la tonalité et la gamme sélectionnées. Le mode doit respecter les réglages de gate, swing et tempo, et ne pas casser le mode CHORD existant.

### 3. Isolateur 4 bandes
Quatre bandes nommées SUB, BASS, MIDS et TOPS. Le traitement doit utiliser de véritables filtres de séparation complémentaires, avec fréquences de coupure définies, réponse contrôlée et gains indépendants. Vérifier la somme des bandes, la phase, les transitoires et la stabilité avant de l'exposer dans l'interface. Les fréquences de coupure et le placement (bus master ou effet dédié) restent à valider à partir du budget DSP disponible.

### 4. Contrôles dub et synchronisation
Étudier les réglages de delay, feedback, couleur, reverb, chorus et LFO synchronisé déjà présents. Réutiliser les fonctions existantes lorsque leur comportement convient ; ne pas dupliquer un effet sans raison. Les commandes web doivent refléter les valeurs réellement lues/écrites par le FM-1.

### 5. Presets Roots Dub
Créer une banque de presets identifiables (basses roots, skank, organ bubble, sirènes, textures et effets) en documentant moteur, paramètres, tonalité et niveaux. Ne pas écraser les presets d'origine.

## Étapes de réalisation et critères de validation

- [ ] **Étape A — audit DSP/UI :** cartographier les paramètres globaux, les bus d'effets, les contrôles web et le budget CPU/RAM.
- [ ] **Étape B — prototype Dub Siren :** implémentation DSP minimale, déclenchement, enveloppe et test audio natif.
- [ ] **Étape C — One-Key Skank :** réutiliser la logique d'accords/gamme, tester les notes MIDI et le séquenceur.
- [ ] **Étape D — isolateur 4 bandes :** prototype de filtres complémentaires, mesures de réponse et test de coût CPU.
- [ ] **Étape E — éditeur web :** ajouter les contrôles après stabilisation des paramètres firmware ; vérifier la synchronisation SysEx.
- [ ] **Étape F — presets et documentation :** banque dédiée, crédits, manuel et notes de version.
- [ ] **Étape G — validation :** tests unitaires/hôte, build firmware, build WebAssembly, puis essai sur FM-1 physique.

## État des validations

La création de cette feuille de route ne valide aucun binaire. Les builds, tests audio et essais sur matériel devront être exécutés et consignés séparément. En particulier, un build hôte ou WebAssembly ne remplace pas un essai sur le FM-1 physique.

## Première zone de travail recommandée

Commencer par l'audit de `firmware/src/fx.c`, `firmware/src/params.c`, `firmware/src/core.h`, `web/EDITOR_PROTOCOL.md` et du code de l'éditeur. Avant de modifier un paramètre persistant, relever les usages de sérialisation, presets, projets, éditeur et tests.
