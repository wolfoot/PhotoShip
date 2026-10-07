[简体中文](README.zh_CN.md) · [English](README.md) · [日本語](README.ja.md) · [한국어](README.ko.md) · [Français](README.fr.md) · [Deutsch](README.de.md) · [Español](README.es.md)

# PhotoShip

<p align="center">
  <img src="packaging/icons/photoship-128.png" width="96" height="96" alt="PhotoShip — Layer Sail">
</p>

**A lightweight, layer-based image editor for Ubuntu and Windows.**

Built with Qt 6 and C++17, PhotoShip offers brushes, selections, layer masks, non-destructive adjustments and editable projects for basic image editing, compositing and graphic creation. The application code is licensed under the [MIT license](LICENSE).

[Build workflow](https://github.com/wolfoot/PhotoShip/actions/workflows/build.yml) · [Report an issue](https://github.com/wolfoot/PhotoShip/issues) · [Third-party notices](THIRD_PARTY_NOTICES.md)

![PhotoShip — English interface](docs/preview.png)

## Features

| Category | Supported features |
| --- | --- |
| Documents and layers | Document tabs, nested groups, multiple selection, drag-and-drop ordering, batch duplication/deletion and adjacent layer merging |
| Painting and retouching | Brush, eraser, eyedropper, tablet pressure, clone stamp and basic healing |
| Selections and transforms | Rectangular/elliptical selections, lasso, contiguous magic wand, selection pixel move/copy, move, scale, rotate, flip and crop |
| Compositing | 12 blend modes, opacity, raster masks, clipping masks, editable text and rectangle/ellipse shapes |
| Adjustments | Levels, RGB curves, hue/saturation/brightness and invert, as adjustment layers or direct pixel edits |
| Files | PNG/JPEG import/export, basic PSD import, basic Compositor `.comp` import and editable `.psproj` projects |
| Workflow | Undo/redo, background saving/export, automatic recovery, clipboard, drag-and-drop import and viewport tile caching |
| Languages | Simplified Chinese, English, Japanese, Korean, French, German and Spanish; instant switching with saved preferences |

Current version: **0.2.1**. Ubuntu has been built, checked automatically and smoke-tested on X11 locally. Windows build and packaging scripts are provided, but still require validation on Windows. Tablet checks use simulated events; real hardware compatibility depends on the device and driver.

## Get and install

### Ubuntu

Build baseline: Ubuntu 22.04/24.04 x86_64, Qt 6.2+, CMake 3.21+ and a C++17 compiler.

```bash
git clone https://github.com/wolfoot/PhotoShip.git
cd PhotoShip
sudo apt update
sudo apt install qt6-base-dev qt6-qpa-plugins zlib1g-dev cmake ninja-build g++
./scripts/build-linux.sh
```

The script builds the application, runs checks and creates `dist/photoship-0.2.1-Linux.deb`:

```bash
sudo apt install ./dist/photoship-0.2.1-Linux.deb
photoship
```

The DEB dynamically links to system Qt; apt installs the required dependencies. Install `fonts-noto-cjk` if Chinese, Japanese or Korean fonts are missing. Offline installation requires runtime libraries and fonts to be prepared in advance.

During development, run the application directly from the build directory:

```bash
./scripts/run.sh
./scripts/run.sh --demo
./scripts/run.sh /absolute/path/to/project.psproj
```

### Windows

Target: Windows 10/11 x64. Prepare:

- Visual Studio 2022 with the **Desktop development with C++** workload.
- CMake and the **MSVC 2022 64-bit** component of Qt 6.8.3.
- vcpkg with `zlib:x64-windows-static-md` for PSD ZIP decoding.
- Inno Setup 6, only when building an installer.

Run PowerShell in the project directory:

```powershell
vcpkg install zlib:x64-windows-static-md
./scripts/build-windows.ps1 -QtPrefix 'C:\Qt\6.8.3\msvc2022_64' -ZlibToolchain 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
```

This creates `dist/PhotoShip-0.2.1-windows-x64.zip`, a portable package with Qt runtime libraries. Extract it and run `photoship.exe`. Add `-Installer` to create `dist/PhotoShip-0.2.1-windows-x64-setup.exe`, which installs to the current user’s directory by default.

The repository’s [GitHub Actions workflow](.github/workflows/build.yml) configures builds, checks and artifact uploads for both platforms. Download artifacts from successful workflow runs. Workflow configuration alone does not confirm platform validation.

## Usage

1. Create a document or open an image, then select the layer to edit in the layers panel.
2. Paint, select and transform with the left toolbar. Enter precise transform values in the right properties panel.
3. Use the Layer menu for masks, clipping and merging, and the Image menu for adjustment layers.
4. Use **Save project** to preserve editable content and **Export PNG / JPEG** for a flattened image.

For clone and healing tools, **Alt-click** a source on the same raster layer before painting. Healing uses local RGB tone matching for basic retouching. When editing masks, black hides and white reveals.

Merging requires adjacent sibling layers with Normal blending. Clipping/adjustment dependencies or translucent ancestor groups may prevent merging to avoid changing the image. Groups use pass-through compositing, so adjustment layers can affect content below and outside their group.

### Languages

Choose a language from **Language / 语言** without restarting. The first launch follows the system language, falling back to English when unsupported. Switching preserves document content, layer names and undo history.

Override the language for one launch without changing your saved preference:

```bash
./scripts/run.sh --language en
# en / zh_CN / ja / ko / fr / de / es
```

Menus, panels, editing dialogs and common messages are localized. Some raw parser and operating-system diagnostics may still be in English.

### Common shortcuts

| Action | Shortcut |
| --- | --- |
| Move / brush / eraser | V / B / E |
| Clone / heal | S / J, Alt-click to sample |
| Rectangle / ellipse / lasso / magic wand | M / Shift+M / L / W |
| Crop / rectangle shape / ellipse shape | C / U / Shift+U |
| Text / eyedropper / hand | T / I / H |
| Pan / zoom | Space-drag or middle button / mouse wheel |
| Brush size | [ / ] |
| Undo / redo | Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z |
| New / open / save / save as | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Import / export | Ctrl+Shift+O / Ctrl+Shift+E |
| Duplicate layer / merge selected layers | Ctrl+J / Ctrl+E |
| Toggle clipping mask | Ctrl+Alt+G |
| Select all / deselect | Ctrl+A / Ctrl+D |
| Fit canvas / actual pixels | Ctrl+0 / Ctrl+1 |
| Cancel stroke or drag | Esc |

## Files and data storage

### Editable projects

A `.psproj` project is a folder containing `manifest.json` and `images/`; keep the entire folder when moving or backing up a project. Projects save layers, groups, masks, transforms, text and adjustment parameters. Selections, undo history and viewport positions are not saved. Text uses local fonts, so moving a project between machines can cause font substitution.

Saving writes new assets and commits the manifest atomically, with a file lock to prevent simultaneous writes. Background saving records a snapshot taken when saving starts; subsequent edits require another save. Exporting an image does not mark the project as saved.

Automatic recovery writes a snapshot about 1.5 seconds after editing stops and checks again every 30 seconds. At startup, restore, discard or keep snapshots for later. Restoration opens separate unsaved documents and preserves original projects. Edits whose snapshot has not finished may not be recoverable.

PhotoShip retains the former Pixel Studio format identifier `org.pixelstudio.project` and data storage locations to preserve existing projects, language preferences and recovery snapshots. The current project format is version 2 and reads version 1. Set `PHOTOSHIP_RECOVERY_DIR` to override the recovery directory; the former `PIXELSTUDIO_RECOVERY_DIR` is also supported.

### PSD and Compositor import

PSD import supports PSD v1, 8-bit RGB, raster layers, groups, raster masks and some clipping relationships. Layer channels may use raw, RLE, ZIP or ZIP prediction compression. A compatibility report appears before import. Text, vectors, smart objects, effects and adjustment parameters may fall back to cached pixels or be skipped. PSD export is not supported.

Basic `.comp` import supports raster layers, groups, transforms, supported blend modes and linked raster masks. Unsupported features produce an error. Save the imported document as `.psproj` to keep the original files.

## Current limitations

- Compositing uses CPU/QPainter with 256×256 viewport tiles. Some filters, PSD parsing and structural operations still run on the main thread.
- Up to 8192 pixels per side and 16 million canvas pixels; up to 32 million source-layer pixels and another 32 million mask pixels; up to 256 layers. PSD files are limited to 256 MiB.
- Undo is limited to 100 steps with an approximate 256 MiB history pixel-buffer budget. Active images, compositing caches and background snapshots use additional memory; this is not a process-wide memory cap.
- The workspace is 8-bit sRGB. PSB, RAW, 16-bit/CMYK editing, pen paths, rich text, advanced layer styles, group masks, liquify and AI cutout are not supported.
- PNG/JPEG are the basic import/export formats; additional image formats depend on installed Qt plugins. PSD import and blend formulas do not guarantee pixel-for-pixel matches with other editors.

## Development and contribution

The project uses C++17, Qt 6 Widgets/Concurrent and zlib. The application requires no network service or account. Main directories:

```text
src/                 Editor UI, document model, compositing, project and PSD I/O
assets/i18n/         Embedded language catalogs
packaging/           Desktop entry, icons, installer configuration and licenses
scripts/             Build, launch, icon generation and X11 smoke checks
tests/               Core, v2 feature and localization checks
third_party/         Third-party code with original licenses
```

Generic build and check commands:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Automated checks cover layer compositing, selections, undo, project I/O, PSD decoding, background saving, recovery and language switching. With Xvfb installed, run the X11 window smoke check:

```bash
python3 scripts/check-x11.py --language en
```

The original **Layer Sail** icon source is `packaging/photoship.svg`, with seven PNG sizes and a Windows ICO. After editing the SVG, rebuild assets with `python3 scripts/render-icons.py`; this requires Linux librsvg, Cairo and Python Pillow. Language text lives in `assets/i18n/*.json`; rebuild after editing it.

Use [Issues](https://github.com/wolfoot/PhotoShip/issues) to report reproduction steps, operating system, Qt version and sample files. Contributions through pull requests are welcome for fixes, translations and features. Preserve existing file-format compatibility and run relevant checks. Remove personal information from submitted projects, screenshots and logs.

## License and acknowledgments

PhotoShip application code is licensed under the [MIT license](LICENSE). The layer workflow draws on Compositor, whose MIT-licensed magic wand and boundary tracing code is reused. Original copyright and license notices are preserved in [third_party/compositor/LICENSE](third_party/compositor/LICENSE).

Qt, Qt plugins and zlib have their own licenses; the application’s MIT license does not replace them. Preserve license notices and meet the applicable requirements when distributing. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

PhotoShip is an independent project, unaffiliated with Adobe, and contains no Photoshop code or assets.
