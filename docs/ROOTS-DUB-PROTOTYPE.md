# ROOTS DUB Audio Lab — guide du prototype

## Ouvrir le laboratoire

Depuis la branche `roots-dub-dev`, ouvrir `web/roots-dub-lab.html` dans un navigateur récent (Chrome, Edge ou Firefox). Aucun serveur, paquet npm ou fichier externe n'est nécessaire. Cliquer sur **Démarrer l'audio** pour autoriser le moteur audio du navigateur.

## Dub Siren

- **Hauteur** règle la fréquence centrale de l'oscillateur.
- **Profondeur du sweep** règle l'excursion du LFO appliquée à la hauteur.
- **Vitesse LFO** règle la fréquence de modulation.
- **Filtre** règle la fréquence de coupure du passe-bas résonant.
- **Feedback écho** règle le niveau de retour de la boucle de delay.
- **Temps d'écho** règle le délai entre répétitions.
- **Déclencher / Relâcher** ouvre et referme l'enveloppe de sirène.

Le générateur tourne dans le navigateur et son enveloppe est fermée au démarrage. Le feedback reste inférieur à l'unité pour éviter une boucle théoriquement auto-entretenue.

## Isolateur 4 bandes

Le signal de test passe dans quatre branches parallèles :

- SUB : passe-bas à 120 Hz, deux étages Butterworth.
- BASS : passe-haut à 120 Hz puis passe-bas à 350 Hz, deux étages par filtre.
- MIDS : passe-haut à 350 Hz puis passe-bas à 2 500 Hz, deux étages par filtre.
- TOPS : passe-haut à 2 500 Hz, deux étages Butterworth.

Chaque branche possède un gain indépendant de -24 à +6 dB. Les pentes visées sont des Linkwitz–Riley d'ordre 4 (cascades de deux Butterworth d'ordre 2). Pour réaligner les phases avant sommation, SUB traverse un biquad all-pass à 350 Hz puis un à 2 500 Hz ; BASS traverse un biquad all-pass à 2 500 Hz. La somme d'un couple LR4 LP/HP correspond à un seul all-pass du second ordre. Les fréquences sont provisoires et le navigateur utilise les biquads de Web Audio.

## Limites et prochaines validations

Ce laboratoire est volontairement indépendant du firmware SLOOP :

- Il ne modifie aucun fichier C, aucune table de paramètres et aucun preset.
- Il n'échange pas de SysEx avec le FM-1.
- Il ne partage pas le code DSP en virgule fixe de SLOOP.
- La somme à gains unitaires est compensée en phase dans le graphe Web Audio ; elle doit néanmoins être mesurée avec un balayage, ainsi que les réponses de chaque bande, les transitoires et le niveau crête.
- Le prototype ne prouve ni le budget CPU/RAM ni la faisabilité temps réel sur le microcontrôleur.
- Le son de sirène est une maquette Web Audio, pas un nouveau moteur firmware.

## Vérifications effectuées

Le contenu HTML et le bloc JavaScript ont été relus depuis la branche GitHub. Le bloc JavaScript a passé une vérification de syntaxe ECMAScript. Ce contrôle statique ne remplace pas un essai d'écoute dans un navigateur, les tests de non-régression SLOOP, une compilation cible ou un essai sur FM-1.


## Mise à jour — compensation de phase

Le graphe du laboratoire applique désormais la même stratégie de compensation que le prototype C LR4 : un biquad all-pass à 350 Hz puis un à 2 500 Hz sur SUB, et un à 2 500 Hz sur BASS. Le test web vérifie la présence de ces branches et compile le bloc JavaScript pour détecter les erreurs de syntaxe. Cela reste un contrôle statique : il ne mesure pas la réponse fréquentielle réelle du navigateur.
