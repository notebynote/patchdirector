# Patchdirector

## Was ist das?

Sound-Repository / Patch-Library-Manager für Synthesizer (Hardware und virtuelle Instrumente).
Funktionen: Multi-Attribut-System, Audio-Playback, Wellenform-Visualisierung, Drag-and-Drop-Import, Audio-Metadaten-Extraktion.

## Technologie-Stack

- **Framework**: Qt (C++)
- **Build-System**: qmake (`Patchdirector.pro`)
- **Audio-Encoding**: LAME (für MP3)
- **Lizenz**: GNU LGPL v2.1

## Plattform-Status

- **Aktiv unterstützt**: macOS **und** Windows (cross-platform via Qt)
- **Plattform-spezifische Dateien im Repo**:
  - `Patchdirector.icns` (Mac-Icon)
  - `Patchdirector.ico` (Windows-Icon)
  - `Patchdirector.rc` (Windows-Resource-File)
- **Cross-Platform-Hinweise beim Code-Schreiben**:
  - Qt-eigene APIs nutzen, keine Linux- oder POSIX-spezifischen Aufrufe
  - File-Pfade via `QDir`, `QFileInfo`, `QStandardPaths` (nie `/` oder `\` hartcodieren)
  - `QString` statt `std::string`, wenn Qt-API beteiligt ist
  - Bei plattform-spezifischem Verhalten: `#ifdef Q_OS_MAC` / `#ifdef Q_OS_WIN` verwenden

## Wichtige Dateien

| Datei | Zweck |
|---|---|
| `Patchdirector.pro` | Qt-Projektdatei (Build-Config) |
| `audiorecorder.cpp/.h` | Audio-Aufnahme |
| `editordialog.cpp/.h/.ui` | Editor-Dialog |
| `licensedialog.cpp/.h/.ui` | Lizenz-Dialog |
| `logdialog.cpp/.h/.ui` | Log-Anzeige |
| `lame/` | LAME-Bibliothek |
| `Patchdirector.icns` / `Patchdirector.ico` | App-Icons (Mac/Win) |
| `Patchdirector.rc` | Windows-Resource-File |

## Build-Kommandos (Mac und Windows)

In Qt Creator: Projekt öffnen → Build.
Per CLI grundsätzlich:
```
qmake Patchdirector.pro
make           # macOS / Linux
nmake          # Windows mit MSVC
mingw32-make   # Windows mit MinGW
```

## Status

Letzter Repo-Commit: August 2025. Aktiv im Maintenance-Modus, kein primärer Fokus.

## Verwandte Ordner

- Marketing-Material: `~/Projekte/haakemusic/patchdirector-assets/`
- Manuals und Bedienungsanleitungen sind dort abgelegt

## Hinweis

Dieses Projekt war historisch auf SourceForge gehostet, jetzt auf GitHub (`notebynote/patchdirector`, public).
