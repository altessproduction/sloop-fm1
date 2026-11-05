# ROOTS DUB — contrat SysEx et éditeur

État : conception préparatoire, **non implémentée** dans le firmware ni dans l'éditeur.
Branche : `roots-dub-dev`. Ne pas considérer ces commandes comme disponibles sur un FM-1.

## Constat d'architecture

- Le protocole documenté et compilé est actuellement v12 ; `ED_LIVE` utilise la commande 77.
- Les commandes 68–76 sont déjà réservées aux patches FM6 et aux kits SYN.
- Les commandes historiques 1–77 doivent rester inchangées.
- Les réglages ROOTS DUB sont stockés dans le projet FUN7 : quatre gains Q15 (32768 = unité) et un bit d'activation. Ils ne sont pas des paramètres `G_*` de la machine.
- Le chemin SysEx central est `ed_handle()` dans `firmware/src/editor.c`; les commandes spécialisées sont interceptées avant le `switch`.
- L'éditeur web est une page monolithique `web/editor.html`, et non un module `editor.js` séparé.

## Proposition de commande

Réserver **78 ROOTS_GET** et **79 ROOTS_SET**, puis annoncer le protocole v13 dans `INFO`.

| Commande | Requête | Réponse |
| --- | --- | --- |
| 78 ROOTS_GET | aucune | enabled, SUB, BASS, MIDS, TOPS |
| 79 ROOTS_SET | enabled, SUB, BASS, MIDS, TOPS | mêmes valeurs, après validation |

Chaque gain est un octet 7 bits dans la plage 0–127 ; 127 représente le gain Q15 32768 (unité), 0 le mute. La conversion vers le moteur se fait par arrondi vers le plus proche de `value * 32768 / 127`. `enabled` est normalisé à 0 ou 1. La réponse renvoie les valeurs quantifiées réellement appliquées.

Les commandes doivent :
- rejeter les trames trop courtes ou trop longues sans modifier l'état ;
- borner chaque valeur à 127 et normaliser le bit d'activation ;
- protéger la mise à jour des valeurs partagées avec l'ISR par la primitive IRQ déjà utilisée par le firmware ;
- ne pas déclencher de sauvegarde flash à chaque mouvement de curseur ;
- laisser `proj_capture()` / `project_apply()` gérer la persistance avec le projet FUN7 ;
- rester inoffensives sur les anciens projets convertis, initialisés en bypass avec gains unitaires.

## Synchronisation et interface

La première version n'a pas besoin d'une nouvelle trame push : les commandes GET/SET suffisent tant qu'aucune commande physique ne modifie les gains en parallèle. Si des contrôles matériels ROOTS sont ajoutés, prévoir un push dédié et l'intégrer au mécanisme WATCH plutôt que détourner `CHANGED`, qui ne transporte que des paramètres `P_*` / `G_*`.

Dans `web/editor.html`, ajouter une page ROOTS DUB distincte des paramètres globaux :
- interrupteur BYPASS / ON ;
- quatre curseurs SUB, BASS, MIDS, TOPS, avec repère 0 dB à 127 ;
- lecture initiale via GET après détection de v13 ;
- envoi SET avec coalescence (éviter une trame à chaque événement de pointeur) ;
- désactivation et message explicite si le firmware ne connaît pas les commandes 78/79.

Le prototype Web Audio `web/roots-dub-lab.html` reste un laboratoire autonome ; il ne doit pas être présenté comme le contrôle du DSP embarqué.

## Conditions avant intégration

1. Exposer dans le module projet une API étroite de lecture/écriture des réglages, avec validation et protection IRQ ; ne pas rendre les variables statiques accessibles directement.
2. Implémenter les commandes 78/79 et faire monter `ED_PROTO` à 13.
3. Ajouter le contrat définitif à `web/EDITOR_PROTOCOL.md` seulement lorsque le firmware est réellement implémenté.
4. Ajouter les contrôles à l'éditeur, avec détection de version et tests statiques.
5. Tester migration FUN6→FUN7, aller-retour GET/SET, bornes, bypass, persistance après capture/apply et coexistence avec WATCH.
6. Ne raccorder le module LR4 au bus master qu'après validation numérique fixe-point, budget CPU sur FM-1 et tests sur matériel.

## Limites actuelles

Aucune commande ROOTS DUB n'est encore implémentée. Le module LR4 n'est pas raccordé au mixage. Les tests LR4 et FUN7 existent dans l'arbre de travail, mais n'ont pas été exécutés dans un checkout complet ni sur la cible matérielle.
