# AceForge Bridge — AU/VST3 Plugin

JUCE **AU** and **VST3** plugin that connects your DAW to the **AceForge API** (local HTTP) for text-to-music generation, playback, and adding generated audio into your project.

- **Target:** macOS (Apple Silicon); build via CMake + JUCE.
- **API:** `http://127.0.0.1:5056` (AceForge server must be running).

---

## What the plugin does

1. **Generate** — Enter a prompt (e.g. “upbeat electronic beat, 10s”), choose duration (10–30 s) and quality (Fast / High), click **Generate**. The plugin talks to AceForge, polls until the job succeeds, then downloads the WAV.
2. **Playback** — When generation succeeds, the audio plays once through the plugin output (so you can hear it and/or record the track in the DAW).
3. **Library** — Each successful generation is saved as a WAV under **~/Library/Application Support/AceForgeBridge/Generations/** (e.g. `gen_20250206_143022.wav`). The plugin UI shows a **Library** list (newest first) with a **Refresh** button.
4. **Add to DAW** — Select a library row, then:
   - **Insert into DAW** (macOS): Opens the file with **Logic Pro** (a new project with that audio). You can then drag the audio from that project into your main project, or use **Reveal in Finder** and drag the file from Finder onto your timeline.
   - **Reveal in Finder**: Opens Finder with the file selected so you can drag it into Logic (or any DAW).
   - **Double‑click** a row: Copies the file path to the clipboard.

---

## Requirements

- **AceForge** (or compatible API) running at `http://127.0.0.1:5056`. The plugin shows “AceForge: connected” when it can reach the server.
- macOS (Apple Silicon). AU and VST3 are built; install and rescan in your DAW.

---

## Build and install

From the **repo root**:

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_OSX_ARCHITECTURES=arm64
# or: cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release
```

Built artefacts:

- **AU:** `build/ml-bridge/plugin/AceForgeBridge_artefacts/Release/AU/AceForge-Bridge.component`
- **VST3:** `build/ml-bridge/plugin/AceForgeBridge_artefacts/Release/VST3/AceForge-Bridge.vst3`

Copy them to:

- `~/Library/Audio/Plug-Ins/Components/` (AU)
- `~/Library/Audio/Plug-Ins/VST3/` (VST3)

Then rescan plugins in your DAW.

### Installer (.pkg)

After building, from the repo root:

```bash
./scripts/build-installer-pkg.sh --sign-plugins --version 0.1.0
```

- **Output:** `release-artefacts/AceForgeBridge-macOS-Installer.pkg` and `release-artefacts/AceForgeBridge-macOS-AU-VST3.zip`.
- **Install:** `sudo installer -pkg release-artefacts/AceForgeBridge-macOS-Installer.pkg -target /` or open the `.pkg` in Finder.

See **BUILD_AND_CI.md** for details and GitHub Actions release.

---

## Repo layout (ml-bridge)

| Path | Description |
|------|-------------|
| **plugin/** | JUCE plugin (Processor + Editor), AU + VST3 target |
| **AceForgeClient/** | HTTP client for AceForge API (macOS; see AceForgeClient/README.md) |
| **AceForge.md** | AceForge API summary (health, generate, status, audio) |
| **BUILD_AND_CI.md** | Build steps and CI/release workflow |
| **DEBUGGING.md** | Crash / log and trace notes |

---

## Architecture (brief)

- **Plugin:** Instrument (stereo out). Background thread: `AceForgeClient` → POST `/api/generate`, poll `/api/generate/status/<jobId>`, GET audio URL → `fetchAudio(url)` → decode WAV → fill double-buffered playback; message thread decodes and saves to library.
- **AceForge:** Local server; REST API for generation, status, and serving WAVs. Base URL `http://127.0.0.1:5056` (default).

---

## Optional / future

- **Windows:** AceForgeClient is macOS-only (NSURLSession); Windows would need a different implementation (e.g. libcurl or JUCE networking).
- **Base URL in UI:** Currently fixed in code; could add a settings field.
- **Drag from plugin into DAW:** The plugin supports starting a file drag from the library list; some hosts (e.g. Logic when the plugin runs in a separate process) may not accept drops from the plugin window. Use **Insert into DAW** or **Reveal in Finder** for reliable workflows.
