# AceForge API — Plugin Integration

The plugin talks to **AceForge** over HTTP (no custom socket protocol). Typical base URL: **`http://127.0.0.1:5056`**.

---

## Base URL and health

- **Base URL:** `http://127.0.0.1:5056` (configurable in plugin; e.g. host/port params).
- **Health:** `GET /api/generate/health` → `{ "healthy": true }`. Use on init or before starting a job.
- **Content-Type:** `application/json` for POST/PATCH bodies.

---

## Plugin flow: generate → poll → play

1. **Start generation**  
   `POST /api/generate`  
   Body (minimal text-to-music):
   ```json
   {
     "customMode": false,
     "songDescription": "upbeat electronic beat with synth",
     "instrumental": true,
     "duration": 30,
     "inferenceSteps": 55,
     "guidanceScale": 6.0,
     "taskType": "text2music",
     "title": "aceforge_export",
     "lyrics": "[inst]"
   }
   ```
   Response: `{ "jobId": "<uuid>", "status": "queued", "queuePosition": 1 }`.

2. **Poll until done**  
   `GET /api/generate/status/<job_id>`  
   Response: `{ "jobId", "status": "queued"|"running"|"succeeded"|"failed", "queuePosition?", "etaSeconds?", "result?", "error?" }`.  
   On `status === "succeeded"`, `result` contains e.g. `audioUrls`, `duration`, `status`.  
   On `status === "failed"`, use `error` for the message.

3. **Optional progress**  
   `GET /progress`  
   Response: `{ "fraction", "done", "error", "stage?", "current?", "total?" }`.  
   Use for a progress bar while polling.

4. **Fetch audio**  
   From `result.audioUrls[0]` you get a path like `/audio/filename.wav`.  
   Full URL: `GET <base>/audio/<filename>` (e.g. `http://127.0.0.1:5056/audio/aceforge_export.wav`).  
   Returns raw WAV bytes. Decode to float (e.g. 44.1 kHz stereo) and feed the plugin’s playback buffer.

5. **Playback**  
   In the plugin’s render callback, read from a ring buffer filled by the “fetch audio” step (on a background thread). Output silence when no buffer or when not playing.

---

## Endpoints the plugin needs

| Purpose        | Method | Path                           | Notes                                      |
|----------------|--------|---------------------------------|--------------------------------------------|
| Health         | GET    | `/api/generate/health`         | Check server before starting a job.        |
| Start job      | POST   | `/api/generate`                | JSON body; returns `jobId`.                 |
| Job status     | GET    | `/api/generate/status/<job_id>`| Poll until `succeeded` or `failed`.        |
| Progress       | GET    | `/progress`                    | Optional; for progress UI.                 |
| Generated audio| GET    | `/audio/<filename>`            | Binary WAV; filename from `result.audioUrls`|
| Ref audio      | GET    | `/audio/refs/<filename>`       | If using reference tracks.                 |

---

## Generation request fields (POST /api/generate)

Relevant for the plugin (see AceForge API reference for full list):

- **customMode**: boolean. If `false`, **songDescription** is required.
- **songDescription** (or **style**): text prompt.
- **lyrics**: optional; use `"[inst]"` for instrumental.
- **instrumental**: boolean (default `true`).
- **duration**: seconds (15–240).
- **inferenceSteps**: int (e.g. 55).
- **guidanceScale**: float (e.g. 6.0).
- **seed**: int; if **randomSeed** is true, server may override.
- **taskType**: `"text2music"` | `"retake"` | `"repaint"` | `"extend"` | `"cover"` | `"audio2audio"`.
- **referenceAudioUrl** / **sourceAudioUrl**: for cover/repaint/etc. (e.g. `/audio/refs/...`).
- **audioCoverStrength** / **ref_audio_strength**: 0–1.
- **title**: base name for output file (used in `result.audioUrls`).
- **keyScale**, **timeSignature**, **vocalLanguage**, **bpm**: optional.

---

## Status response (succeeded)

Example when `status === "succeeded"`:

```json
{
  "jobId": "...",
  "status": "succeeded",
  "result": {
    "audioUrls": ["/audio/aceforge_export.wav"],
    "duration": 30,
    "status": "succeeded"
  }
}
```

Plugin: take `result.audioUrls[0]`, strip leading `/` if needed, and request `GET <base>/audio/aceforge_export.wav` to get the WAV bytes.

---

## Errors

- `GET /api/generate/status/<id>` may return 4xx/5xx with JSON `{ "error": "..." }`.
- `POST /api/generate` may return 400/500 with an error body; treat as job failed and show message.

---

## CORS and local use

AceForge is for local use; the plugin runs in the same machine. Use `127.0.0.1` and ensure the plugin process is allowed outgoing HTTP (e.g. entitlements `network.client` or “Outgoing Connections (Client)”).
