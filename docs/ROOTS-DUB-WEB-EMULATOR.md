# Tester ROOTS DUB dans l'émulateur web

Ce mode permet d'essayer l'interface ROOTS DUB sans FM-1 et sans envoyer de MIDI.

## Ouvrir l'éditeur simulé

Depuis la racine du dépôt, servir le site comme pour l'éditeur SLOOP, puis ouvrir :

`http://localhost:8766/webapp/editor/?mock=1&roots=1`

- `mock=1` active le périphérique FM-1 simulé intégré à l'éditeur.
- `roots=1` lui annonce le protocole éditeur v13 afin d'afficher le panneau ROOTS DUB.
- Cliquer sur **Connect** dans l'éditeur. Aucun matériel n'est contacté.

Si le site est servi directement depuis le dossier source plutôt que depuis le site généré, le chemin de la page peut être `/web/editor.html?mock=1&roots=1`.

## Scénarios à essayer

1. Dans SOUND, ouvrir le panneau **ROOTS DUB · 4-band isolator**.
2. Déplacer SUB, BASS, MIDS et TOPS séparément : les valeurs affichées doivent suivre les curseurs.
3. Cocher/décocher « Activer l'isolation (préparation) ».
4. Modifier plusieurs bandes rapidement : les écritures sont mises en file pour éviter que les réponses du protocole se croisent.
5. Déconnecter puis reconnecter le mock : les valeurs reviennent aux valeurs neutres. Le mock est volontairement éphémère et ne simule pas la mémoire flash FUN7.

## Ce que ce test valide — et ce qu'il ne valide pas

- Il valide le panneau de l'éditeur, les commandes simulées ROOTS_GET (78) et ROOTS_SET (79), les valeurs 0–127 et l'affichage des réponses.
- Le mode mock ne traite pas le son : les curseurs ROOTS ne changent pas le signal de la préécoute audio de l'éditeur.
- Il ne valide ni la sauvegarde/restauration FUN7 sur flash, ni le DSP LR4 embarqué, ni les performances du FM-1.
- Pour écouter le prototype Web Audio indépendant (sirène et isolateur), ouvrir `web/roots-dub-lab.html` dans un navigateur moderne en autorisant l'audio. Ce laboratoire est distinct du moteur audio firmware.

Les tests de protocole automatisés ROOTS DUB sont dans `web/test_web.mjs` (fonction `editorRoots`). Les tests de persistance FUN7 sont séparés des tests du navigateur.
