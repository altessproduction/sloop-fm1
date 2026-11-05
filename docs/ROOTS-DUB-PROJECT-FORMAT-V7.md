# ROOTS DUB — format projet FUN7

## État de l'implémentation

FUN7 est maintenant le format courant dans `firmware/src/project.c`.
FUN6 est conservé comme structure historique en lecture seule et migré à
l'import. Les conversions FUN1 à FUN5 restent en place.

### Disposition binaire

FUN6 conserve exactement sa disposition antérieure et sa taille de 3 816 octets.
FUN7 ajoute 12 octets après `sel, drdly, rsv[2]`, avant les pistes :

| Champ | Taille | Sens |
|---|---:|---|
| `roots_gain[4]` | 8 octets | quatre gains Q15, 32768 = unité, 0 = silence |
| `roots_enabled` | 1 octet | 0 désactivé, 1 activé |
| `roots_rsv[3]` | 3 octets | réservés, initialisés à zéro |
| **Ajout** | **12 octets** | |

Les assertions C imposent `sizeof(project_v6_t) == 3816`,
`sizeof(project_t) == 3828` et une taille FUN7 inférieure ou égale à
3 840 octets. L'union de staging conserve FUN5 (3 840 octets), donc les
lectures flash et les buffers partagés avec `upreset.c` / `fm6_bank.c`
gardent leur capacité historique.

### Migration et intégrité

- FUN7 valide son magic, sa taille exacte et son checksum FNV-1a.
- FUN6 valide son magic, ses 3 816 octets et son checksum historique, puis
  copie globals, métadonnées et pistes dans FUN7. Les gains deviennent 32768,
  l'isolateur reste désactivé.
- FUN1 à FUN5 passent par les conversions historiques, puis reçoivent les
  mêmes valeurs Roots neutres.
- Les formats historiques ne sont jamais réécrits en place ni interprétés
  comme FUN7.
- Un checksum invalide ou une taille incorrecte est rejeté.

### Capture, application et sauvegardes

Les quatre gains et le drapeau d'activation sont désormais copiés lors de
`proj_capture()` et restaurés par `proj_apply()`. Ils suivent donc les
sections A-D et l'autosave avec le reste du projet. Les objets de backup
conservent leurs identifiants : les chemins de restauration passent par
`proj_import()`, qui reconnaît FUN7 et migre FUN6.

Le buffer temporaire de flash contient explicitement FUN7, FUN6 et les formats
antérieurs ; sa taille reste déterminée par FUN5 à 3 840 octets. Le test hôte
ajoute une fixture FUN6 reconstruite avec la structure figée, vérifie la
migration neutre, le rejet checksum/taille, le round-trip FUN7 et le passage
des réglages dans capture/application.

## Limites restantes

- Les gains persistés sont des valeurs Q15 ; la plage de contrôle utilisateur
  et les rampes anti-clic ne sont pas encore définies.
- Le module DSP n'est pas raccordé au bus maître.
- Aucun nouveau SysEx ni contrôle de l'éditeur web n'est encore ajouté.
- Les tests hôte sont modifiés mais doivent encore être exécutés dans un
  checkout complet avec les headers générés ; le SDK JieLi n'est pas disponible
  dans cet environnement.
- Les sauvegardes binaires FUN6 sont prises en charge par le parseur de projet,
  mais un test d'intégration bout en bout des exports/restaurations JSON du
  backup reste à faire.

## Étapes suivantes

1. Exécuter `tests/project_test.c` et le script complet sur un checkout complet.
2. Vérifier la taille réelle du buffer de staging et les chemins autosave,
   sections et backup/restore.
3. Ajouter les commandes firmware/SysEx et le retour live de l'éditeur.
4. Relier les valeurs persistées au DSP après validation des tests LR4.
5. Mesurer CPU/RAM et vérifier DAC, USB MASTER/FULL, visualiseur et matériel
   avant de proposer la fusion.
