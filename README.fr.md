[简体中文](README.zh_CN.md) · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Español](README.es.md)

# PhotoShip

<p align="center">
  <img src="packaging/icons/photoship-128.png" width="96" height="96" alt="PhotoShip — Layer Sail">
</p>

**Un éditeur d’images léger avec calques pour Ubuntu et Windows.**

Développé avec Qt 6 et C++17, PhotoShip propose pinceaux, sélections, masques, réglages non destructifs et projets modifiables pour la retouche de base, la composition et la création graphique. Le code de l’application est sous [licence MIT](LICENSE).

[Workflow de compilation](https://github.com/wolfoot/PhotoShip/actions/workflows/build.yml) · [Signaler un problème](https://github.com/wolfoot/PhotoShip/issues) · [Licences tierces](THIRD_PARTY_NOTICES.md)

![PhotoShip — interface en anglais](docs/preview.png)

## Fonctionnalités

| Catégorie | Fonctions disponibles |
| --- | --- |
| Documents et calques | Onglets, groupes imbriqués, sélection multiple, réorganisation par glisser-déposer, duplication/suppression par lots et fusion de calques adjacents |
| Peinture et retouche | Pinceau, gomme, pipette, pression de tablette, tampon de clonage et correction simple |
| Sélections et transformations | Sélections rectangulaires/elliptiques, lasso, baguette sur zones contiguës, déplacement/copie des pixels sélectionnés, déplacement, mise à l’échelle, rotation, retournement et recadrage |
| Composition | 12 modes de fusion, opacité, masques raster, masques d’écrêtage, texte modifiable et formes rectangulaires/elliptiques |
| Réglages | Niveaux, courbes RGB, teinte/saturation/luminosité et inversion, par calques de réglage ou modification directe des pixels |
| Fichiers | Import/export PNG/JPEG, import PSD simple, import Compositor `.comp` simple et projets `.psproj` modifiables |
| Organisation du travail | Annuler/rétablir, enregistrement/export en arrière-plan, récupération automatique, presse-papiers, import par glisser-déposer et cache de tuiles visibles |
| Langues | Chinois simplifié, anglais, japonais, coréen, français, allemand et espagnol ; changement immédiat et préférence enregistrée |

Version actuelle : **0.2.1**. Ubuntu a fait l’objet d’une compilation locale, de vérifications automatiques et d’un test de démarrage X11. Les scripts de compilation et de création des paquets Windows sont fournis, mais doivent encore être validés sous Windows. Les tests de tablette utilisent des événements simulés ; la compatibilité réelle dépend du matériel et du pilote.

## Obtenir et installer

### Ubuntu

Configuration de compilation : Ubuntu 22.04/24.04 x86_64, Qt 6.2+, CMake 3.21+ et un compilateur C++17.

```bash
git clone https://github.com/wolfoot/PhotoShip.git
cd PhotoShip
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins zlib1g-dev cmake ninja-build g++
./scripts/build-linux.sh
```

Le script compile l’application, exécute les vérifications et produit `dist/photoship-0.2.1-Linux.deb` :

```bash
sudo apt install ./dist/photoship-0.2.1-Linux.deb
photoship
```

Le DEB utilise les bibliothèques Qt du système par liaison dynamique ; apt installe les dépendances. Installez `fonts-noto-cjk` si les polices chinoises, japonaises ou coréennes manquent. Pour une installation hors ligne, préparez les bibliothèques et polices à l’avance.

En développement, lancez directement l’application depuis le répertoire de compilation :

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

### Windows

Plateforme cible : Windows 10/11 x64. Préparez :

- Visual Studio 2022 avec la charge de travail **Desktop development with C++**.
- CMake et le composant **MSVC 2022 64-bit** de Qt 6.8.3.
- vcpkg et `zlib:x64-windows-static-md` pour décoder le ZIP des PSD.
- Inno Setup 6, uniquement pour créer un installateur.

Exécutez PowerShell dans le répertoire du projet :

```powershell
vcpkg install zlib:x64-windows-static-md
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -ZlibToolchain 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
```

Le paquet portable `dist/PhotoShip-0.2.1-windows-x64.zip` inclut les bibliothèques Qt. Décompressez-le puis lancez `photoship.exe`. Ajoutez `-Installer` pour produire `dist/PhotoShip-0.2.1-windows-x64-setup.exe`, installé par défaut dans le répertoire de l’utilisateur actuel.

Le [workflow GitHub Actions](.github/workflows/build.yml) configure la compilation, les vérifications et le téléversement des artefacts pour les deux plateformes. Téléchargez les artefacts depuis une exécution réussie. La présence du workflow ne confirme pas à elle seule la validation d’une plateforme.

## Utilisation

1. Créez un document ou ouvrez une image, puis sélectionnez le calque à modifier dans le panneau des calques.
2. Utilisez la barre de gauche pour peindre, sélectionner et transformer. Saisissez des valeurs précises dans le panneau des propriétés à droite.
3. Le menu Calque propose masques, écrêtage et fusion ; le menu Image crée des calques de réglage.
4. Utilisez **Enregistrer le projet** pour garder les éléments modifiables et **Exporter en PNG / JPEG** pour obtenir une image aplatie.

Avec le tampon ou la correction, choisissez une source par **Alt-clic** sur le même calque raster avant de peindre. La correction utilise une correspondance locale des tons RGB pour des retouches simples. Dans un masque, le noir cache et le blanc révèle.

La fusion exige des calques adjacents de même niveau en mode Normal. Les dépendances d’écrêtage/réglage ou des groupes parents translucides peuvent empêcher la fusion pour préserver l’image. Les groupes utilisent une composition pass-through : les réglages peuvent affecter les éléments inférieurs situés hors du groupe.

### Langues

Choisissez une langue dans **Language / 语言** sans redémarrer. Le premier lancement suit la langue du système, avec repli vers l’anglais si nécessaire. Le changement conserve le contenu, les noms des calques et l’historique d’annulation.

Imposez une langue pour ce lancement uniquement, sans modifier la préférence enregistrée :

```bash
./scripts/run.sh --language fr
# en / zh_CN / ja / ko / fr / de / es
```

Les menus, panneaux, dialogues d’édition et messages courants sont traduits. Certains diagnostics bruts du parseur ou du système peuvent rester en anglais.

### Raccourcis courants

| Action | Raccourci |
| --- | --- |
| Déplacer / pinceau / gomme | V / B / E |
| Cloner / corriger | S / J, Alt-clic pour choisir la source |
| Rectangle / ellipse / lasso / baguette | M / Shift+M / L / W |
| Recadrer / forme rectangulaire / forme elliptique | C / U / Shift+U |
| Texte / pipette / main | T / I / H |
| Déplacement de la vue / zoom | Glisser avec Espace ou bouton central / molette |
| Taille du pinceau | [ / ] |
| Annuler / rétablir | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| Nouveau / ouvrir / enregistrer / enregistrer sous | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Importer / exporter | Ctrl+Shift+O / Ctrl+Shift+E |
| Dupliquer le calque / fusionner la sélection | Ctrl+J / Ctrl+E |
| Basculer le masque d’écrêtage | Ctrl+Alt+G |
| Tout sélectionner / désélectionner | Ctrl+A / Ctrl+D |
| Ajuster le canevas / pixels réels | Ctrl+0 / Ctrl+1 |
| Annuler le trait ou le glissement | Esc |

## Fichiers et stockage des données

### Projets modifiables

Un projet `.psproj` est un dossier contenant `manifest.json` et `images/` ; conservez le dossier entier lors d’un déplacement ou d’une sauvegarde. Il enregistre calques, groupes, masques, transformations, texte et réglages. Les sélections, l’historique d’annulation et la position de la vue ne sont pas enregistrés. Les polices sont locales : un autre ordinateur peut les remplacer.

L’enregistrement écrit de nouveaux fichiers et valide le manifeste de manière atomique, avec un verrou contre les écritures simultanées. La sauvegarde en arrière-plan capture l’état au démarrage ; les modifications ultérieures nécessitent un nouvel enregistrement. L’export d’une image ne marque pas le projet comme enregistré.

La récupération automatique écrit un instantané environ 1,5 seconde après l’arrêt des modifications et vérifie aussi toutes les 30 secondes. Au démarrage, restaurez, supprimez ou conservez les instantanés pour plus tard. La restauration ouvre des documents distincts non enregistrés et préserve les originaux. Les modifications dont l’instantané n’est pas terminé peuvent être perdues.

PhotoShip conserve l’identifiant `org.pixelstudio.project` et les emplacements de stockage de l’ancien Pixel Studio pour préserver projets, préférences et instantanés existants. Le format actuel est la version 2 et lit la version 1. Définissez `PHOTOSHIP_RECOVERY_DIR` pour choisir le dossier de récupération ; l’ancien `PIXELSTUDIO_RECOVERY_DIR` reste accepté.

### Import PSD et Compositor

L’import PSD prend en charge PSD v1, RGB 8 bits, calques raster, groupes, masques raster et certaines relations d’écrêtage. Les canaux peuvent utiliser raw, RLE, ZIP ou ZIP avec prédiction. Un rapport de compatibilité précède l’import. Texte, vecteurs, objets dynamiques, effets et paramètres de réglage peuvent être remplacés par des pixels en cache ou ignorés. L’export PSD est indisponible.

L’import `.comp` simple prend en charge calques raster, groupes, transformations, modes de fusion reconnus et masques raster liés. Les fonctions non prises en charge produisent une erreur. Enregistrez le résultat en `.psproj` pour conserver les fichiers originaux.

## Limites actuelles

- La composition utilise CPU/QPainter et des tuiles de 256×256 pour la vue. Certains filtres, l’analyse PSD et les opérations structurelles restent sur le thread principal.
- Maximum : 8192 pixels par côté et 16 millions de pixels de canevas ; 32 millions de pixels source et 32 millions supplémentaires pour les masques ; 256 calques. Fichiers PSD limités à 256 MiB.
- L’annulation est limitée à 100 étapes avec un budget de buffers de pixels d’environ 256 MiB. Images actives, caches et instantanés consomment de la mémoire supplémentaire ; ce n’est pas une limite globale du processus.
- Espace de travail sRGB 8 bits. PSB, RAW, édition 16 bits/CMYK, tracés à la plume, texte enrichi, styles avancés, masques de groupe, fluidité et détourage IA sont indisponibles.
- PNG/JPEG sont les formats de base ; les autres dépendent des plugins Qt installés. L’import PSD et les formules de fusion ne garantissent pas une correspondance pixel par pixel avec d’autres éditeurs.

## Développement et contributions

Le projet utilise C++17, Qt 6 Widgets/Concurrent et zlib. L’application ne nécessite ni service réseau ni compte. Répertoires principaux :

```text
src/                 Interface, modèle de document, composition, lecture/écriture des projets et PSD
assets/i18n/         Catalogues de langues intégrés
packaging/           Entrée de bureau, icônes, configuration d’installation et licences
scripts/             Compilation, lancement, création des icônes et tests X11
tests/               Vérifications du cœur, des fonctions v2 et de la traduction
third_party/         Code tiers avec ses licences originales
```

Commandes générales de compilation et de vérification :

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Les vérifications automatiques couvrent composition, sélections, annulation, lecture/écriture des projets, décodage PSD, enregistrement en arrière-plan, récupération et langues. Avec Xvfb installé, lancez le test de démarrage de la fenêtre X11 :

```bash
python3 scripts/check-x11.py --language fr
```

L’icône originale **Layer Sail** vient de `packaging/photoship.svg`, avec sept tailles PNG et un ICO Windows. Après modification du SVG, régénérez les fichiers avec `python3 scripts/render-icons.py` ; Linux librsvg, Cairo et Python Pillow sont nécessaires. Les traductions sont dans `assets/i18n/*.json` ; recompilez après modification.

Dans [Issues](https://github.com/wolfoot/PhotoShip/issues), indiquez les étapes de reproduction, le système, la version Qt et des fichiers d’exemple. Les pull requests de corrections, traductions et fonctions sont bienvenues. Préservez la compatibilité des formats et exécutez les vérifications pertinentes. Retirez les données personnelles des projets, captures et journaux soumis.

## Licence et remerciements

Le code de PhotoShip est sous [licence MIT](LICENSE). Le travail avec les calques s’inspire de Compositor, dont le code MIT de baguette magique et de suivi des contours est réutilisé. Les mentions originales sont conservées dans [third_party/compositor/LICENSE](third_party/compositor/LICENSE).

Qt, ses plugins et zlib ont leurs propres licences ; la licence MIT de l’application ne les remplace pas. Conservez les mentions et respectez les conditions applicables lors d’une distribution. Consultez [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

PhotoShip est un projet indépendant, sans affiliation avec Adobe, et ne contient aucun code ni ressource de Photoshop.
