# AceForgeClient

C++ HTTP client for the **AceForge API**. Use it from your AU/VST3 plugin (on a **background thread**) to start generation, poll status, and fetch audio.

## macOS build

Requires **Foundation.framework** (NSURLSession). Compile the implementation only on Apple:

```bash
# Example: build a static lib or object file for linking into your plugin
clang++ -x objective-c++ -std=c++17 -O2 -c AceForgeClientMac.mm -o AceForgeClientMac.o \
  -I. -framework Foundation
```

Link your plugin with `AceForgeClientMac.o` and `-framework Foundation`.

## Usage (from plugin)

1. **Create client:** `aceforge::AceForgeClient client("http://127.0.0.1:5056");`
2. **Optional:** `client.setBaseUrl(...)` if the user configures host/port.
3. **Health:** `if (!client.healthCheck()) { /* show "AceForge not running" */ }`
4. **Start job:**  
   `aceforge::GenerateParams params;`  
   `params.songDescription = "upbeat electronic";`  
   `params.durationSeconds = 30;`  
   `std::string jobId = client.startGeneration(params);`
5. **Poll (e.g. timer every 1–2 s):**  
   `auto status = client.getStatus(jobId);`  
   If `status.status == "succeeded"`, use `status.audioUrl` (e.g. `/audio/aceforge_export.wav`).
6. **Fetch WAV:**  
   `auto wavBytes = client.fetchAudio(status.audioUrl);`  
   Decode WAV to float (stereo, 44.1k or match DAW). Push samples into a lock-free ring buffer.
7. **Render callback:** Read from the ring buffer and fill the DAW output; output silence when empty or not playing.

## WAV decoding

The API returns raw WAV bytes. You need a small WAV parser to get sample rate, channels, and PCM (or float) data. Typical: 44.1 kHz, 16-bit or 32-bit, stereo. Convert to float and feed your playback buffer. Many plugin frameworks (e.g. JUCE) have `juce::WavAudioFormat` or similar; otherwise use a minimal WAV header parser (44-byte header, then interleaved or per-channel samples).

## Windows / JUCE

On Windows, either:

- Implement `AceForgeClient` using **libcurl** or **WinHTTP** with the same interface in `AceForgeClient.hpp`, or  
- Use **JUCE** `juce::URL`, `juce::WebInputStream`, and build the same request/response flow (health, POST generate, GET status, GET audio) and keep this header’s structs and flow.

See **AceForge.md** in the parent directory for the API summary.
