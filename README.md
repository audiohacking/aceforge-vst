# AceForge Bridge — AU/VST3 plugin for AceForge

Experimental **AU** and **VST3** plugin that connects your DAW to the [AceForge](https://github.com/audiohacking/aceforge) API for generation and playback (ACE-Step, text-to-music).

- **Target:** macOS (Apple Silicon); build via CMake + JUCE.
- **API:** Local HTTP, e.g. `http://127.0.0.1:5056` (AceForge server).

## Quick start

1. **Build** (from repo root):
   ```bash
   cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
   cmake --build build --config Release
   ```
2. Copy `AceForge Bridge.component` and `AceForge Bridge.vst3` from the build tree into `~/Library/Audio/Plug-Ins/Components/` and `~/Library/Audio/Plug-Ins/VST3/`.
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

See repository license. Plugin uses JUCE (GPL mode) and links to AceForge API.
