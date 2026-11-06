# Tester ROOTS DUB dans l'émulateur web

## Façade physique FM1

La page principale est désormais la façade visuelle du M-VAVE FM1 :

`web/fm1-roots-emulator.html`

Elle représente le boîtier, les molettes MASTER / SELECT / PRESETS / ALGORITHM, les quatre encodeurs KNOB 1–4, les boutons de fonction disposés en deux colonnes, l'écran couleur, une rangée de huit sélecteurs ovales OP1–OP6 / PIT / GLO et les 27 touches. Les molettes peuvent être glissées à la souris ou au doigt ; les touches s'illuminent au contact. ROOTS affiche les quatre gains ; SIREN affiche une page de paramètres de démonstration.

Cette façade est une maquette interactive de présentation. Le clavier déclenche maintenant des notes synthétisées localement avec Web Audio, et le bouton SIREN DUB joue une sirène avec LFO, filtre et écho. Il faut d'abord cliquer sur **ACTIVER LE SON** (politique audio du navigateur). Les commandes n'envoient pas de MIDI au firmware et les molettes ne pilotent pas encore le DSP embarqué : ce moteur est un instrument de démonstration navigateur, pas une émulation bit-exacte du FM1. Pour écouter le prototype audio séparé, ouvrir le laboratoire audio ci-dessous.

## Éditeur et laboratoire audio

- Éditeur simulé : `web/editor.html?mock=1&roots=1`. Cliquer sur Connect puis ouvrir SOUND / ROOTS DUB.
- Laboratoire audio : `web/roots-dub-lab.html`. Il permet d'écouter la sirène et le prototype d'isolateur Web Audio.
- Façade FM1 : `web/fm1-roots-emulator.html`.

Le mock de l'éditeur simule les commandes ROOTS_GET (78) et ROOTS_SET (79), mais ne simule pas la mémoire flash FUN7 ni le traitement audio du firmware.

## Publication

Le workflow `.github/workflows/pages.yml` publie les trois pages sur GitHub Pages. La page d'accueil redirige vers la façade FM1 après le déploiement.

- Accueil : `https://altessproduction.github.io/sloop-fm1/`
- Façade FM1 : `https://altessproduction.github.io/sloop-fm1/web/fm1-roots-emulator.html`
- Éditeur SLOOP : `https://altessproduction.github.io/sloop-fm1/web/editor.html?mock=1&roots=1`
- Laboratoire audio : `https://altessproduction.github.io/sloop-fm1/web/roots-dub-lab.html`

Le site n'est disponible qu'après fusion de la modification, activation de Pages avec **Settings → Pages → Build and deployment → Source: GitHub Actions**, puis réussite du workflow Actions.
