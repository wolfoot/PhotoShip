[简体中文](README.zh_CN.md) · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Español](README.es.md)

# PhotoShip

<p align="center">
  <img src="packaging/icons/photoship-128.png" width="96" height="96" alt="PhotoShip — Layer Sail">
</p>

**Ein schlanker Bildeditor mit Ebenen für Ubuntu und Windows.**

PhotoShip basiert auf Qt 6 und C++17 und bietet Pinsel, Auswahlbereiche, Ebenenmasken, nicht destruktive Anpassungen und bearbeitbare Projekte für grundlegende Bildbearbeitung, Komposition und Grafikgestaltung. Der Anwendungscode steht unter der [MIT-Lizenz](LICENSE).

[Build-Workflow](https://github.com/wolfoot/PhotoShip/actions/workflows/build.yml) · [Problem melden](https://github.com/wolfoot/PhotoShip/issues) · [Drittanbieter-Lizenzen](THIRD_PARTY_NOTICES.md)

![PhotoShip — englische Oberfläche](docs/preview.png)

## Funktionen

| Kategorie | Unterstützte Funktionen |
| --- | --- |
| Dokumente und Ebenen | Dokument-Tabs, verschachtelte Gruppen, Mehrfachauswahl, Sortieren per Drag-and-drop, mehrfaches Duplizieren/Löschen und Zusammenführen benachbarter Ebenen |
| Malen und Retuschieren | Pinsel, Radierer, Pipette, Tablettdruck, Klonstempel und einfache Reparatur |
| Auswahl und Transformation | Rechteck-/Ellipsenauswahl, Lasso, Zauberstab für zusammenhängende Bereiche, ausgewählte Pixel verschieben/kopieren, verschieben, skalieren, drehen, spiegeln und zuschneiden |
| Komposition | 12 Mischmodi, Deckkraft, Rastermasken, Schnittmasken, bearbeitbarer Text sowie Rechteck- und Ellipsenformen |
| Anpassungen | Tonwerte, RGB-Kurven, Farbton/Sättigung/Helligkeit und Umkehrung als Anpassungsebenen oder direkte Pixelbearbeitung |
| Dateien | PNG/JPEG-Import und -Export, grundlegender PSD- und Compositor-`.comp`-Import, bearbeitbare `.psproj`-Projekte |
| Arbeitsablauf | Rückgängig/Wiederholen, Speichern und Export im Hintergrund, automatische Wiederherstellung, Zwischenablage, Drag-and-drop-Import und Kachelcache für den sichtbaren Bereich |
| Sprachen | Vereinfachtes Chinesisch, Englisch, Japanisch, Koreanisch, Französisch, Deutsch und Spanisch; sofortiger Wechsel mit gespeicherter Einstellung |

Aktuelle Version: **0.2.1**. Ubuntu wurde lokal gebaut, automatisch geprüft und unter X11 auf erfolgreichen Start getestet. Windows-Build- und Paket-Skripte stehen bereit, müssen aber noch unter Windows validiert werden. Tablettprüfungen verwenden simulierte Ereignisse; echte Hardware-Unterstützung hängt von Gerät und Treiber ab.

## Bezug und Installation

### Ubuntu

Build-Basis: Ubuntu 22.04/24.04 x86_64, Qt 6.2+, CMake 3.21+ und ein C++17-Compiler.

```bash
git clone https://github.com/wolfoot/PhotoShip.git
cd PhotoShip
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins zlib1g-dev cmake ninja-build g++
./scripts/build-linux.sh
```

Das Skript baut die Anwendung, führt Prüfungen aus und erstellt `dist/photoship-0.2.1-Linux.deb`:

```bash
sudo apt install ./dist/photoship-0.2.1-Linux.deb
photoship
```

Das DEB bindet System-Qt dynamisch ein; apt installiert die nötigen Abhängigkeiten. Bei fehlenden chinesischen, japanischen oder koreanischen Schriften installieren Sie `fonts-noto-cjk`. Für eine Offline-Installation müssen Laufzeitbibliotheken und Schriften vorher bereitstehen.

Während der Entwicklung lässt sich die Anwendung direkt aus dem Build-Verzeichnis starten:

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

### Windows

Zielplattform: Windows 10/11 x64. Benötigt werden:

- Visual Studio 2022 mit der Workload **Desktop development with C++**.
- CMake und die Komponente **MSVC 2022 64-bit** von Qt 6.8.3.
- vcpkg mit `zlib:x64-windows-static-md` für die PSD-ZIP-Dekodierung.
- Inno Setup 6, nur zur Erstellung eines Installationsprogramms.

Führen Sie PowerShell im Projektverzeichnis aus:

```powershell
vcpkg install zlib:x64-windows-static-md
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -ZlibToolchain 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
```

Es entsteht das portable Paket `dist/PhotoShip-0.2.1-windows-x64.zip` mit Qt-Laufzeitbibliotheken. Entpacken Sie es und starten Sie `photoship.exe`. Mit `-Installer` wird `dist/PhotoShip-0.2.1-windows-x64-setup.exe` erstellt; standardmäßig wird im Verzeichnis des aktuellen Benutzers installiert.

Der [GitHub-Actions-Workflow](.github/workflows/build.yml) konfiguriert Builds, Prüfungen und Artefakt-Uploads für beide Plattformen. Artefakte lassen sich aus erfolgreichen Läufen herunterladen. Eine Workflow-Konfiguration allein bestätigt keine Plattformvalidierung.

## Verwendung

1. Erstellen Sie ein Dokument oder öffnen Sie ein Bild und wählen Sie im Ebenenbereich die zu bearbeitende Ebene.
2. Malen, wählen und transformieren Sie mit der linken Werkzeugleiste. Exakte Transformationswerte lassen sich rechts in den Eigenschaften eingeben.
3. Das Ebenenmenü bietet Masken, Schnittmasken und Zusammenführen; das Bildmenü erstellt Anpassungsebenen.
4. Mit **Projekt speichern** bleiben Inhalte bearbeitbar. **PNG / JPEG exportieren** erzeugt ein zusammengeführtes Bild.

Wählen Sie bei Klonen und Reparieren zuerst per **Alt-Klick** eine Quelle auf derselben Rasterebene. Die Reparatur gleicht lokale RGB-Farbtöne für einfache Retusche an. In Masken verbirgt Schwarz und zeigt Weiß.

Zusammenführen erfordert benachbarte Ebenen derselben Hierarchie mit Normal-Mischmodus. Schnittmasken-/Anpassungsabhängigkeiten oder durchscheinende übergeordnete Gruppen können dies verhindern, damit das Bild unverändert bleibt. Gruppen verwenden pass-through-Komposition; Anpassungen können auch tiefer liegende Inhalte außerhalb der Gruppe beeinflussen.

### Sprachen

Im Menü **Language / 语言** wechseln Sie ohne Neustart die Sprache. Beim ersten Start gilt die Systemsprache, bei fehlender Unterstützung Englisch. Dokumentinhalt, Ebenennamen und Rückgängig-Verlauf bleiben erhalten.

Für einen einzelnen Start lässt sich die Sprache überschreiben, ohne die gespeicherte Einstellung zu ändern:

```bash
./scripts/run.sh --language de
# en / zh_CN / ja / ko / fr / de / es
```

Menüs, Bereiche, Bearbeitungsdialoge und häufige Meldungen sind übersetzt. Einige unverarbeitete Parser- und Betriebssystemdiagnosen können englisch bleiben.

### Häufige Tastenkürzel

| Aktion | Tastenkürzel |
| --- | --- |
| Verschieben / Pinsel / Radierer | V / B / E |
| Klonen / Reparieren | S / J, Alt-Klick wählt die Quelle |
| Rechteck / Ellipse / Lasso / Zauberstab | M / Shift+M / L / W |
| Zuschneiden / Rechteckform / Ellipsenform | C / U / Shift+U |
| Text / Pipette / Hand | T / I / H |
| Ansicht verschieben / Zoom | Mit Leertaste ziehen oder mittlere Taste / Mausrad |
| Pinselgröße | [ / ] |
| Rückgängig / Wiederholen | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| Neu / Öffnen / Speichern / Speichern unter | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Import / Export | Ctrl+Shift+O / Ctrl+Shift+E |
| Ebene duplizieren / ausgewählte Ebenen zusammenführen | Ctrl+J / Ctrl+E |
| Schnittmaske umschalten | Ctrl+Alt+G |
| Alles auswählen / Auswahl aufheben | Ctrl+A / Ctrl+D |
| Leinwand einpassen / tatsächliche Pixel | Ctrl+0 / Ctrl+1 |
| Pinselstrich oder Ziehen abbrechen | Esc |

## Dateien und Datenspeicherung

### Bearbeitbare Projekte

Ein `.psproj`-Projekt ist ein Ordner mit `manifest.json` und `images/`. Verschieben oder sichern Sie den gesamten Ordner. Projekte speichern Ebenen, Gruppen, Masken, Transformationen, Text und Anpassungsparameter. Auswahl, Rückgängig-Verlauf und Ansichtsposition werden nicht gespeichert. Text nutzt lokale Schriften, die auf anderen Rechnern ersetzt werden können.

Speichern schreibt neue Dateien und bestätigt das Manifest atomar. Eine Dateisperre verhindert gleichzeitige Schreibzugriffe. Beim Speichern im Hintergrund wird der Zustand zum Start erfasst; spätere Änderungen benötigen einen weiteren Speichervorgang. Bildexport markiert das Projekt nicht als gespeichert.

Die automatische Wiederherstellung schreibt etwa 1,5 Sekunden nach Ende der Bearbeitung einen Snapshot und prüft zusätzlich alle 30 Sekunden. Beim Start können Sie wiederherstellen, verwerfen oder für später behalten. Wiederherstellung öffnet separate ungespeicherte Dokumente und bewahrt Originalprojekte. Änderungen mit unvollständigem Snapshot lassen sich möglicherweise nicht wiederherstellen.

PhotoShip behält die Formatkennung `org.pixelstudio.project` und Speicherorte des früheren Pixel Studio, um Projekte, Spracheinstellungen und Wiederherstellungssnapshots zu erhalten. Das aktuelle Format ist Version 2 und liest Version 1. `PHOTOSHIP_RECOVERY_DIR` überschreibt das Wiederherstellungsverzeichnis; das frühere `PIXELSTUDIO_RECOVERY_DIR` wird ebenfalls unterstützt.

### PSD- und Compositor-Import

PSD-Import unterstützt PSD v1, 8-Bit RGB, Rasterebenen, Gruppen, Rastermasken und einige Schnittmaskenbeziehungen. Kanäle können raw, RLE, ZIP oder ZIP mit Prädiktion verwenden. Vor dem Import erscheint ein Kompatibilitätsbericht. Text, Vektoren, Smartobjekte, Effekte und Anpassungsparameter können durch zwischengespeicherte Pixel ersetzt oder übersprungen werden. PSD-Export wird nicht unterstützt.

Der grundlegende `.comp`-Import unterstützt Rasterebenen, Gruppen, Transformationen, bekannte Mischmodi und verknüpfte Rastermasken. Nicht unterstützte Funktionen erzeugen Fehler. Speichern Sie das importierte Dokument als `.psproj`, um Originaldateien zu behalten.

## Aktuelle Einschränkungen

- Die Komposition verwendet CPU/QPainter und 256×256-Kacheln für den sichtbaren Bereich. Einige Filter, PSD-Analyse und Strukturänderungen laufen im Hauptthread.
- Maximal 8192 Pixel pro Seite und 16 Millionen Leinwandpixel; 32 Millionen Quellpixel und weitere 32 Millionen Maskenpixel; 256 Ebenen. PSD-Dateien sind auf 256 MiB begrenzt.
- Rückgängig ist auf 100 Schritte und etwa 256 MiB für Verlaufspixelpuffer begrenzt. Aktive Bilder, Kompositionscache und Hintergrundsnapshots benötigen zusätzlich Speicher; dies ist keine prozessweite Grenze.
- Arbeitsraum: 8-Bit sRGB. PSB, RAW, 16-Bit/CMYK, Stiftpfade, Rich Text, erweiterte Ebenenstile, Gruppenmasken, Verflüssigen und KI-Freistellung werden nicht unterstützt.
- PNG/JPEG sind die Basisformate; weitere Formate hängen von installierten Qt-Plugins ab. PSD-Import und Mischformeln garantieren keine pixelgenaue Übereinstimmung mit anderen Editoren.

## Entwicklung und Beiträge

Das Projekt nutzt C++17, Qt 6 Widgets/Concurrent und zlib. Die Anwendung benötigt keinen Netzwerkdienst und kein Konto. Wichtige Verzeichnisse:

```text
src/                 Oberfläche, Dokumentmodell, Komposition, Projekt- und PSD-Ein-/Ausgabe
assets/i18n/         Eingebettete Sprachkataloge
packaging/           Desktop-Eintrag, Symbole, Installer-Konfiguration und Lizenzen
scripts/             Build, Start, Symbolerstellung und X11-Startprüfungen
tests/               Kern-, v2-Funktions- und Sprachprüfungen
third_party/         Drittanbietercode mit ursprünglichen Lizenzen
```

Allgemeine Build- und Prüfkommandos:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Automatische Prüfungen decken Komposition, Auswahl, Rückgängig, Projektdateien, PSD-Dekodierung, Hintergrundspeicherung, Wiederherstellung und Sprachwechsel ab. Mit installiertem Xvfb lässt sich der Start des X11-Fensters prüfen:

```bash
python3 scripts/check-x11.py --language de
```

Das originale **Layer Sail**-Symbol basiert auf `packaging/photoship.svg`, mit sieben PNG-Größen und einem Windows-ICO. Nach SVG-Änderungen erzeugt `python3 scripts/render-icons.py` die Dateien neu; benötigt werden Linux librsvg, Cairo und Python Pillow. Sprachtexte liegen in `assets/i18n/*.json`; nach Änderungen neu bauen.

Melden Sie unter [Issues](https://github.com/wolfoot/PhotoShip/issues) Reproduktionsschritte, Betriebssystem, Qt-Version und Beispieldateien. Pull Requests für Fehlerkorrekturen, Übersetzungen und Funktionen sind willkommen. Erhalten Sie die Dateiformat-Kompatibilität und führen Sie passende Prüfungen aus. Entfernen Sie persönliche Daten aus eingereichten Projekten, Screenshots und Logs.

## Lizenz und Danksagung

PhotoShip steht unter der [MIT-Lizenz](LICENSE). Der Ebenenablauf orientiert sich an Compositor; dessen MIT-lizenzierter Zauberstab- und Konturverfolgungscode wird wiederverwendet. Ursprüngliche Hinweise bleiben in [third_party/compositor/LICENSE](third_party/compositor/LICENSE) erhalten.

Qt, Qt-Plugins und zlib haben eigene Lizenzen, die nicht durch die MIT-Lizenz der Anwendung ersetzt werden. Bewahren Sie Lizenzhinweise und erfüllen Sie die jeweiligen Bedingungen bei Weitergabe. Siehe [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

PhotoShip ist unabhängig von Adobe und enthält keinen Code und keine Ressourcen von Photoshop.
