# AceForge Bridge — AU/VST3 Plugin for AceForge

Experimental **AU** and **VST3** plugin that connects your DAW to the [AceForge](https://github.com/audiohacking/aceforge) API for text-to-music generation and playback (ACE-Step, etc.).

- **Target:** macOS (Apple Silicon); build via CMake + JUCE.
- **API:** Local HTTP, e.g. `http://127.0.0.1:5056` (AceForge server must be running).

## What it does

- **Generate** — Enter a prompt, set duration and quality, click Generate. The plugin talks to AceForge and plays the result once through the plugin output.
- **Library** — Successful generations are saved as WAVs; the plugin lists them (newest first). You can **Insert into DAW** (opens the file in Logic Pro) or **Reveal in Finder** and drag the file into your DAW timeline.

Full description, build details, and installer steps: **ml-bridge/README.md**.

## Quick start

1. **Build** (from repo root):
   ```bash
   cmake -B build -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES=arm64
   cmake --build build --config Release
   ```
   Or use the Xcode generator: `-G Xcode` instead of `-G "Unix Makefiles"`.

2. **Install** — Copy the built `.component` and `.vst3` from the build tree into your plug-in folders, or build the installer:
   ```bash
   ./scripts/build-installer-pkg.sh --sign-plugins --version 0.1.0
   ```
   Then install `release-artefacts/AceForgeBridge-macOS-Installer.pkg` (or open it in Finder).

3. Rescan plugins in your DAW; run AceForge so the API is available at `http://127.0.0.1:5056`.

## Repo layout

| Path | Description |
|------|-------------|
| **ml-bridge/** | Plugin source, AceForge client, and docs |
| **ml-bridge/plugin/** | JUCE AU + VST3 target (CMake) |
| **ml-bridge/AceForgeClient/** | C++ HTTP client for AceForge API (macOS) |
| **ml-bridge/AceForge.md** | API summary for the plugin |
| **ml-bridge/BUILD_AND_CI.md** | Build and GitHub Actions release |
| **.github/workflows/release-plugins.yml** | Build and release on GitHub Release |

## Releases

Creating a **GitHub Release** (e.g. tag `v0.1.0`) triggers the workflow and attaches **AceForgeBridge-macOS-AU-VST3.zip** to that release.

## License

See repository license. Plugin uses JUCE (GPL mode) and talks to the AceForge API.
