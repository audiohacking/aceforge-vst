# Minimal AU — ML Bridge Plugin Scaffold

Two ways to get a **working** AU that can connect to your ML host:

---

## Option A: Xcode Audio Unit Extension (recommended)

1. **New project:** Xcode → File → New → Project → **Audio Unit Extension**.
2. **Product name:** e.g. `MLBridgeAU`; **Language:** Swift or Objective-C.
3. **Bundle ID:** e.g. `com.yourname.MLBridgeAU`.
4. **In the AU’s `AUAudioUnit` subclass:**
   - Set `internalRenderBlock` to a block that:
     - Takes `(AudioUnitRenderActionFlags*, const AudioTimeStamp*, frameCount, outputBusNumber, outputBufferList, ...)`.
     - Fills `outputBufferList` with float samples (or silence for testing).
   - Optional: in `allocateRenderResources`, open a TCP socket to `127.0.0.1:8765` (or your ML host port) and store the fd; in the render block, request N frames from the host and copy into the buffers.
5. **Entitlements:** For network, add **Outgoing Connections (Client)** (or the `network.client` entitlement) so the sandbox allows `connect()`.
6. **Install:** Build; copy the `.component` from the build folder to `~/Library/Audio/Plug-Ins/Components/`, or use the Xcode scheme’s “Run” to install into the host app.
7. **Rescan** plugins in your DAW.

Apple doc: [Creating an Audio Unit Extension](https://developer.apple.com/documentation/audiotoolbox/audio_unit_v3_plug-ins/creating_an_audio_unit_extension).

---

## Option B: JUCE (AU + VST3 from one project)

1. **Get JUCE:** [juce.com](https://juce.com) or GitHub (Projucer or CMake).
2. **New project:** Projucer → New Project → **Audio Plug-In**; enable **AU** and **VST3**.
3. **In the processor’s `processBlock()`:**  
   Fill `buffer` with your audio (silence, or receive from your ML host over a socket).  
   Optional: start a background thread that connects to `127.0.0.1:8765` and enqueues received audio; in `processBlock` dequeue and copy into `buffer`.
4. **Build:** Build the AU and/or VST3 target; install the `.component` and/or `.vst3` into `~/Library/Audio/Plug-Ins/Components/` and `~/Library/Audio/Plug-Ins/VST3/`.
5. **Rescan** in your DAW.

---

## Protocol (plugin ↔ ML host)

See **../protocol.md** for a minimal binary protocol (e.g. request N frames, receive float32 buffer). ACE Bridge 2 uses plain **socket/connect** (TCP); no custom SDK in the plugin.

---

## Files here

- **MLBridgeAU.c** — Placeholder export and comments; a real AU is built with Xcode or JUCE as above.
- **README.md** — This file.
