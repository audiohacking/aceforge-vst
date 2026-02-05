# ML Bridge — Plugin ↔ Host Protocol Sketch

A minimal protocol so the AU/VST plugin and your ML host can exchange control and audio.

---

## Transport

- **TCP** on `127.0.0.1` and a fixed port (e.g. `8765`). Plugin = client, ML host = server.
- Alternative: **Unix domain socket** or **XPC** for lower latency and no network stack.

---

## Message framing (binary)

- **Header:** 8 bytes  
  - `uint32_t type`  
  - `uint32_t payload_size`
- **Payload:** `payload_size` bytes (optional).
- **Types (examples):**
  - `0x01` — Hello (plugin → host): send sample rate, block size.
  - `0x02` — Audio request (plugin → host): e.g. “N samples, at time T, with MIDI/params”.
  - `0x03` — Audio response (host → plugin): N float32 samples.
  - `0x04` — MIDI (plugin → host): raw MIDI bytes.
  - `0x05` — Params (plugin → host): key-value or index + float.

---

## Audio flow

1. Plugin’s render callback runs at DAW block size (e.g. 256 or 512).
2. Plugin sends a request: “I need 256 samples at time T” (and current MIDI/params if needed).
3. ML host runs inference, fills buffer, sends back 256 float32 samples.
4. Plugin copies into DAW output buffer (with simple double-buffer or ring if async).

For low latency, host can run in a separate thread and use a lock-free queue; or use a larger “chunk” size and buffer a few blocks.

---

## Optional: JSON over TCP

For debugging, use newline-delimited JSON instead of binary (e.g. `{"type":"audio_request","frames":256}` and `{"type":"audio_response","samples":[...]}`). Switch to binary for production.
