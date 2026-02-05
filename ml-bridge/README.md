# ML Bridge — AU/VST3 Plugin for AceForge

Scaffold and implementation for an **AU/VST3 plugin** that talks to the **AceForge API** (local HTTP, e.g. `http://127.0.0.1:5056`) for **generation and playback** — inspired by ACE Studio’s ACE Bridge 2.

---

## How ACE Bridge 2 Works (from reverse engineering)

- **Binary:** Standalone; links **only system frameworks** (no libACEAudioSDK in the plugin).
- **UI:** WebKit + Cocoa (likely a web view for the bridge UI).
- **Audio:** CoreAudio, AudioToolbox, CoreMIDI.
- **Connection:** Uses **`socket` / `connect`** (BSD TCP) — the plugin is a **network client** that talks to ACE Studio (or a local server) for audio/MIDI and control.
- **Entitlements:** `network.client`, `temporary-exception.files.all.read-write` (sandbox exceptions for the plugin process).
- **Formats:** VST3 (Instrument), AAX, AU (aumu, subtype `Brdg`, manufacturer `Acdt`).

So the pattern is: **plugin in DAW ↔ TCP (or local IPC) ↔ host app (ACE Studio or your ML host).**

---

## Architecture: Plugin ↔ AceForge (HTTP)

```
┌─────────────────┐         ┌──────────────────────────┐         ┌─────────────────┐
│  DAW            │  audio  │  AceForge Bridge plugin  │  HTTP   │  AceForge       │
│  (Logic, etc.)  │ ◄──────  │  - POST /api/generate   │ ◄────► │  (127.0.0.1:    │
│                 │          │  - GET status, /audio   │         │   5056)         │
└─────────────────┘          │  - Playback buffer      │         │  ACE-Step, etc. │
                              └──────────────────────────┘         └─────────────────┘
```

- **Plugin:** Instrument (aumu) or Effect (aufx). **Background thread:** AceForgeClient → POST `/api/generate`, poll `/api/generate/status/<jobId>`, GET `/audio/<filename>` → decode WAV → fill ring buffer. **Render callback:** read from ring buffer (or silence).
- **AceForge:** Local server; REST API for generation, status, and audio serving. See **AceForge.md** and the AceForge API reference.

---

## Options to Build the Plugin

| Option | AU | VST3 | Effort | Notes |
|--------|----|------|--------|-------|
| **JUCE** | ✅ | ✅ | Low | Projucer → Audio Plug-In, enable AU + VST3. Easiest cross-format. |
| **Raw Apple AU** | ✅ | ❌ | Medium | AudioToolbox + ComponentManager; no VST3. See `MinimalAU/` and Apple’s “Audio Unit Programming Guide”. |
| **Steinberg VST3 SDK** | ❌ | ✅ | Medium | C++; license terms apply. Need separate AU (e.g. JUCE or raw) for AU. |
| **DPF (DISTRHO)** | ❌ | ✅ | Low | C++; VST3 only on macOS unless wrapped. |

**Recommendation:** Use **JUCE** for both AU and VST3 with one codebase; or start with **raw AU** (this repo’s `MinimalAU/`) if you want no external deps and AU-only first.

---

## This Repo Layout

- **README.md** (this file) — Overview and build options.
- **AceForge.md** — AceForge API summary for the plugin (health, generate, status, audio).
- **AceForgeClient/** — C++ HTTP client for AceForge (macOS impl; use from background thread). See AceForgeClient/README.md.
- **MinimalAU/** — AU scaffold: pattern and placeholder; **working AU** via Xcode “Audio Unit Extension” or JUCE (see MinimalAU/README.md).
- **protocol.md** — Optional low-level protocol sketch (AceForge uses HTTP, so this is secondary).

---

## AceForge plugin flow

1. **Config:** Base URL `http://127.0.0.1:5056` (or user setting). Optional: `healthCheck()` on init.
2. **UI / params:** User enters prompt (songDescription), duration, task type, etc.
3. **Generate:** `AceForgeClient::startGeneration(params)` → `jobId`.
4. **Poll:** Timer or background thread calls `getStatus(jobId)` until `succeeded` or `failed`.
5. **Fetch:** On success, `fetchAudio(result.audioUrl)` → WAV bytes → decode to float → fill playback ring buffer.
6. **Play:** Render callback reads from ring buffer; output silence when idle or no buffer.

---

## Build and install

1. Build the component (see `MinimalAU/README.md`).
2. Copy the built `.component` to:
   - `~/Library/Audio/Plug-Ins/Components/`
3. Rescan plugins in your DAW (e.g. Logic: Preferences → Plug-In Manager → Reset & Rescan).
4. Run your ML host on a fixed port (e.g. `127.0.0.1:8765`); in the plugin you’ll connect to that port (see `MinimalAU/` source comments).

---

## Next steps

1. **UI:** Prompt field, duration, Generate button, progress, play/stop (optional: list songs, reference tracks).
2. **WAV decoder:** In plugin or use framework (e.g. JUCE) to convert fetchAudio() bytes to float for the ring buffer.
3. **Windows:** Implement AceForgeClient with libcurl or JUCE networking for VST3 on Windows.
