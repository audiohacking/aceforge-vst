# Build and CI — AceForge Bridge plugins

## Local build (macOS)

From the **repo root**:

```bash
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release
```

Then locate `AceForge Bridge.component` and `AceForge Bridge.vst3` under `build/` (e.g. with `find build -name "*.component" -o -name "*.vst3"`), copy them to:

- `~/Library/Audio/Plug-Ins/Components/` (AU)
- `~/Library/Audio/Plug-Ins/VST3/` (VST3)

Rescan plugins in your DAW. Ensure **AceForge** is running at `http://127.0.0.1:5056` when using the plugin.

---

## GitHub Actions — release workflow

- **File:** `.github/workflows/release-plugins.yml`
- **Triggers:**
  - **release published** — runs on new GitHub Release (e.g. when you publish a release for tag `v0.1.0`).
  - **workflow_dispatch** — run manually from the Actions tab.
- **Runner:** `macos-latest` (Apple Silicon).
- **Steps:**
  1. Checkout repo
  2. Configure with CMake (Xcode generator, arm64)
  3. Build Release
  4. Find built `AceForge Bridge.component` and `AceForge Bridge.vst3`
  5. Zip them as `AceForgeBridge-macOS-AU-VST3.zip`
  6. If the run was triggered by a **release**, upload the zip as a **release asset**; otherwise upload as a **workflow artifact**.

### How to release

1. In GitHub: **Releases** → **Draft a new release**.
2. Choose or create a tag (e.g. `v0.1.0`), set title and notes, then **Publish release**.
3. The workflow runs automatically and attaches `AceForgeBridge-macOS-AU-VST3.zip` to that release.
4. Users download the zip and unzip into their Plug-Ins folders.

### Manual run (no release)

Go to **Actions** → **Build and release plugins (macOS)** → **Run workflow**. The zip is available as an **artifact** for that run.
