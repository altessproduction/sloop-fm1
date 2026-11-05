# ROOTS DUB — projet version 7 : plan de compatibilité

## Objectif

Persister les quatre gains de l'isolateur master SUB / BASS / MIDS / TOPS sans
modifier les identifiants historiques, sans agrandir silencieusement FUN6 et
sans faire disparaître les projets enregistrés par SLOOP antérieur.

## Contraintes établies dans le code

- `project.c` écrit actuellement FUN6 et vérifie `magic`, `size` et checksum.
- FUN1 à FUN5 restent lisibles par des structures historiques dédiées.
- FUN6 stocke les paramètres de piste en trois zones (paramètres communs,
  paramètres 2.6, huit paramètres moteur) et les métadonnées de séquence.
- `PROJ_NG` est figé à 32; les globals ultérieurs ont des règles de stockage
  particulières ou sont des réglages système.
- `storage.c` limite un objet à 3840 octets de charge utile.
- L'autosave et les quatre slots de projet partagent le format projet.
- L'éditeur web possède un protocole SysEx versionné et un système backup/restore.

## Proposition FUN7

Ne pas ajouter les gains à `song.g[]` et ne pas déplacer les paramètres P_*.
Ajouter une structure explicite de configuration Roots Dub dans le projet FUN7,
par exemple quatre gains 7 bits et un bit d'activation, avec des valeurs par
défaut neutres (127 ou représentation documentée de 0 dB selon l'échelle UI).

Le champ doit être inclus dans le checksum. Le format FUN7 doit avoir son propre
magic, sa taille exacte et une assertion de taille. La structure FUN6 reste
inchangée et devient lecture seule.

## Migration au chargement

- FUN7 : charger les gains, les borner à leur plage et initialiser l'état DSP.
- FUN6 : gains neutres, isolateur désactivé.
- FUN5 / FUN4 / FUN3 / FUN2 / FUN1 : reprendre les conversions existantes,
  puis appliquer les mêmes valeurs neutres.
- Projet vierge : valeurs neutres.
- Projet corrompu : conserver le mécanisme de rejet actuel; ne jamais interpréter
  un ancien champ réservé comme des gains sans magic/version explicite.

## Autosave, backup et éditeur

- Faire pointer l'autosave sur le format courant, avec migration à l'ouverture.
- Conserver les mêmes identifiants d'objets de backup : les objets projet 0..4
  sont validés par `proj_import`, qui doit accepter FUN7 et convertir FUN6.
  Le format JSON de backup n'a pas besoin d'un nouvel identifiant si le contenu
  binaire est validé par magic, taille et checksum. Tester les backups FUN6
  existants et les exports/restaurations FUN7.
- Ajouter quatre commandes de contrôle dans une page ROOTS DUB dédiée, avec
  WATCH/push pour que le navigateur reflète les changements du panneau matériel.
- Tester les exports/imports de projets et de backups, pas seulement le chemin
  de lecture local.

## Ordre d'implémentation obligatoire

1. Écrire les structures FUN7 et les conversions, avec tests hôte de tailles,
   checksums, aller-retour et fixtures FUN1..FUN6.
2. Ajouter les quatre valeurs de gains dans la structure runtime du morceau.
3. Ajouter les commandes firmware / SysEx et leur documentation.
4. Ajouter la page web et le retour live.
5. Raccorder le DSP après le filtre DJ global et avant MASTER.
6. Vérifier que DAC, USB MASTER, USB FULL et visualiseur ont le comportement
   attendu, avec gains neutres bit-identiques au chemin antérieur.
7. Mesurer le coût CPU et la mémoire sur la cible avant d'activer par défaut.

## Pourquoi cette migration n'est pas encore codée

Une modification partielle des seuls tableaux `song.g[]`, de `project_t` ou
du protocole rendrait les sauvegardes incohérentes. Ce plan fixe le contrat de
compatibilité avant de modifier les formats binaires et les commandes live.


## Précision après lecture complète de project.c et project_test.c

La taille hôte vérifiée par le test de régression est exactement **3 816 octets**
pour FUN6, avec une limite de charge utile de **3 840 octets** dans
`storage.c`. Il reste donc 24 octets, et non une marge théorique à supposer.

Un bloc candidat compact pour FUN7 tient dans 12 octets :
- quatre gains `uint16_t` Q15 (0 = silence, 32768 = gain unitaire) ;
- un octet d'activation ;
- trois octets réservés initialisés à zéro ;
- checksum final déplacé après ce bloc.

La taille attendue serait 3 828 octets, sous réserve de la confirmer par
`_Static_assert` dans le code et par le test hôte. Ce choix conserve une
représentation DSP sans conversion ambiguë depuis une échelle UI 7 bits.
Le gain maximal autorisé à l'interface devra toutefois être défini séparément
et borné avant conversion vers Q15.

### Contrat de migration à tester

Le test hôte existant est `tests/project_test.c` et inclut directement
`project.c` sous `PROJ_HOST`. Il vérifie déjà les fixtures FUN1 à FUN5,
le checksum et le round-trip FUN6. FUN7 devra ajouter explicitement :

1. une structure historique FUN6 de taille figée à 3 816 octets (ne pas
   redéfinir FUN6 en réutilisant la nouvelle structure) ;
2. import FUN6 -> FUN7 avec isolateur désactivé et gains unitaires ;
3. round-trip FUN7 -> FUN7 des quatre gains et du bit d'activation ;
4. rejet d'un checksum invalide et d'une taille incohérente ;
5. assertion de taille FUN7 <= 3 840 octets ;
6. vérification que les conversions FUN1 à FUN5 conservent leurs résultats
   actuels et reçoivent les valeurs neutres Roots Dub.

### Point d'attention de l'implémentation

Le code actuel utilise `project_t` à la fois comme format courant et comme
buffer d'application, tandis que le buffer temporaire de flash est une union
FUN6/FUN5/FUN4/FUN3/FUN2/FUN1. La migration ne peut donc pas consister à
ajouter simplement un champ à `project_t` : il faut figer une structure
`project_v6_t`, introduire FUN7 comme format courant, agrandir l'union
temporaire et vérifier les chemins backup/restore ainsi que l'autosave.


### Backup/restore : correction du contrat

La lecture de `project_restore()` confirme que les objets projet du backup
passent par `proj_import()` avant écriture. Il n'est donc pas nécessaire
d'inventer un nouvel identifiant d'objet de backup : les IDs existants restent
stables, et c'est le parseur projet qui accepte FUN6 et FUN7. Le buffer de
staging doit simplement pouvoir contenir le plus grand format, et les tests
doivent couvrir une restauration d'un ancien FUN6 ainsi qu'un aller-retour
FUN7. Cette compatibilité doit aussi être vérifiée pour l'objet autosave et les
quatre sections, pas seulement pour un import isolé.
