# ROOTS DUB isolator — prototypes DSP autonomes

Les deux modules de ce dossier restent des prototypes autonomes. Aucun n'est
raccordé au bus maître du FM-1. Les coefficients sont calculés pour 44,1 kHz.

## Ancien prototype à un pôle

`firmware/src/roots_isolator.h` conserve une séparation douce SUB/BASS/MIDS/TOPS.
La simulation de sa réponse a révélé un recouvrement trop important, avec une
amplitude MIDS supérieure à l'entrée autour de 350 Hz. Il ne convient donc pas
à un isolateur DJ final.

## Prototype LR4 fixe

`firmware/src/roots_lr4_isolator.h` réalise trois crossovers Linkwitz–Riley
4e ordre à 120 Hz, 350 Hz et 2,5 kHz. Chaque passe-bas et passe-haut est formé
de deux biquads Butterworth identiques (Q = 1/sqrt(2)), en Q30. Le calcul est
direct-form I avec accumulateur 64 bits et états audio saturés.

Topologie de séparation :

```
entrée -> LR4 @ 120 Hz -> SUB (LP) + reste (HP)
reste  -> LR4 @ 350 Hz -> BASS (LP) + reste (HP)
reste  -> LR4 @ 2,5 kHz -> MIDS (LP) + TOPS (HP)
```

### Compensation de phase

Un couple LR4 LP+HP est un all-pass, pas un fil de phase nulle. Dans la cascade,
les bandes basses subissent donc moins de déphasage que les bandes hautes.
Avant sommation, SUB traverse les all-pass à 350 Hz et 2,5 kHz ; BASS traverse
celui à 2,5 kHz. La somme complexe d'un couple LP4 + HP4 correspond à un seul biquad all-pass
du second ordre, avec numérateur renversé ; il ne faut pas le doubler.

Le module contient 12 biquads de séparation et 3 de compensation par canal,
soit 15 par canal et 30 en stéréo. L'ordre de grandeur est d'environ 150
multiplications par échantillon stéréo. L'état `roots_lr4_t` occupe environ 260 octets
par canal (520 octets en stéréo) ; un test hôte impose un plafond de 264 octets.
Ce coût arithmétique et cette empreinte doivent être mesurés/confirmés sur le FM-1
avant toute intégration.

### Contrat logiciel

- Une instance d'état par canal ; ne jamais partager les états gauche/droite.
- Gains Q15 : 32768 = unité, 0 = coupure.
- Bypass au repos exact : l'échantillon est renvoyé bit pour bit. À la désactivation,
  une rampe dry/wet de 64 échantillons revient au bypass puis fige les états ; à la
  réactivation, les historiques sont remis à zéro et une rampe de 64 échantillons
  évite une commutation instantanée.
- États internes Q8 (8 bits fractionnaires) pour limiter l'erreur de quantification des filtres graves ; saturation interne à +/-131071 en unité audio, sortie à +/-131071.
- Coefficients fixes à 44,1 kHz ; aucun changement de fréquence
  d'échantillonnage n'est géré.
- Les rampes de gain, les contrôles utilisateur, la persistance FUN7 et le
  raccordement au bus maître ne sont pas encore implémentés.

## Tests

Le test `tests/roots_lr4_isolator_test.c` est référencé dans
`tests/run_tests.sh` après le test du prototype historique. Il couvre :

1. bypass bit-identique ;
2. coupure des quatre bandes ;
3. reconstruction DC à gain unitaire ;
4. réponse de la somme des bandes à ±0,5 % sur des fréquences de 20 Hz à 20 kHz ;
5. séparation fréquentielle individuelle des quatre bandes et points de crossover ;
6. les 15 masques non vides sur la même grille de fréquences ;
7. bornage sous excitation pleine échelle ;
8. rampes ON/OFF, bypass exact après extinction et réinitialisation des états à la réactivation.

Le script complet exige d'abord le build SDK (`./build.sh`). Dans cet
environnement de travail, le checkout local et le SDK JieLi ne sont pas
disponibles : le test LR4 n'a donc pas encore été compilé/exécuté ici. Les
résultats de simulation double précision précédents ne valident pas les
arrondis du code C fixe, ses transitoires, ni le matériel.

## Conditions avant intégration

- Exécuter les tests C dans un checkout complet et corriger tout écart mesuré.
- Mesurer les courbes des quatre sorties séparées, pas seulement leur somme.
- Mesurer crêtes et transitoires pour les 15 masques et tous les gains autorisés.
- Ajouter une rampe de gain sans clic et définir une marge de headroom (la rampe d'activation/bypass est maintenant présente, pas celle des gains de bande).
- Mesurer CPU et RAM sur la cible FM-1.
- Vérifier le signal stéréo, USB FULL, visualiseur et bypass après insertion au
  point d'injection identifié dans l'audit technique.
- Valider firmware, WASM, écoute réelle et flash matériel avant de déclarer
  l'isolateur prêt.

**Décision actuelle :** conserver le LR4 comme prototype isolé. Ne pas
l'intégrer au master bus ni fusionner cette PR avant validation logicielle et
mesures sur la cible.
