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
- Étendre les sauvegardes complètes de l'éditeur avec un nouvel identifiant
  d'objet/version; les anciennes sauvegardes doivent continuer à se restaurer.
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
