# ROOTS DUB — contrat SysEx et éditeur

État au 2026-11-05 : commandes firmware 78/79 et conversion Q15/7 bits implémentées sur la branche ; interface web non implémentée ; aucun test compilé/exécuté ici.
Branche : `roots-dub-dev`. Ne pas considérer ces commandes comme disponibles sur un FM-1.

## Constat d'architecture

- Le protocole firmware est maintenant v13 ; `ED_LIVE` utilise toujours la commande 77.
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

L'implémentation actuelle des commandes :
- rejette les trames dont le nombre d'arguments n'est pas exactement celui attendu ;
- borner chaque valeur à 127 et normaliser le bit d'activation ;
- protège l'écriture des valeurs partagées avec `fm1_irq_off()` / `fm1_irq_on()` ;
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

1. Ajouter les contrôles à l'éditeur, avec détection de version et tests statiques.
2. Ajouter un test d'intégration SysEx GET/SET et vérifier les bornes, le bypass, la persistance après capture/apply et la coexistence avec WATCH.
3. Compiler et exécuter les tests FUN7 et LR4 dans un checkout complet.
4. Ne raccorder le module LR4 au bus master qu'après validation numérique fixe-point, budget CPU sur FM-1 et tests sur matériel.

## Limites actuelles

Les commandes SysEx 78/79 sont implémentées pour lire/écrire les réglages, mais l'éditeur web n'a pas encore ses curseurs. Aucun test n'a été compilé/exécuté dans un checkout complet ni sur la cible matérielle. Le module LR4 n'est pas raccordé au mixage.
