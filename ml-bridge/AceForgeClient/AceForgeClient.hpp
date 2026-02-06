/**
 * AceForge HTTP client for AU/VST3 plugins.
 * Talks to AceForge API (e.g. http://127.0.0.1:5056) for generation and audio.
 *
 * Use from a background thread; do not call from the audio/render thread.
 * After fetchAudio(), decode WAV and feed a lock-free queue for playback.
 */
#ifndef ACEFORGE_CLIENT_HPP
#define ACEFORGE_CLIENT_HPP

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace aceforge {

struct GenerateParams {
    std::string songDescription;
    std::string lyrics = "[inst]";
    bool instrumental = true;
    int durationSeconds = 30;
    int inferenceSteps = 15;  // use 15 for faster testing; 55 for higher quality
    float guidanceScale = 6.0f;
    bool randomSeed = true;
    int64_t seed = 0;
    std::string taskType = "text2music";
    std::string title = "aceforge_export";
    std::string referenceAudioUrl;
    std::string sourceAudioUrl;
    float audioCoverStrength = 0.5f;
    float refAudioStrength = 0.5f;
};

struct JobStatus {
    std::string jobId;
    std::string status;  // "queued" | "running" | "succeeded" | "failed"
    int queuePosition = 0;
    double etaSeconds = 0;
    std::string error;
    std::string audioUrl;   // from result.audioUrls[0] when succeeded
    double durationSeconds = 0;
};

struct ProgressInfo {
    float fraction = 0.f;
    bool done = false;
    std::string error;
    std::string stage;
    int current = 0;
    int total = 0;
};

class AceForgeClient {
public:
    explicit AceForgeClient(std::string baseUrl = "http://127.0.0.1:5056");
    ~AceForgeClient();

    void setBaseUrl(const std::string& url);
    std::string getBaseUrl() const;

    /** GET /api/generate/health -> healthy */
    bool healthCheck();

    /** POST /api/generate; returns jobId or empty on error */
    std::string startGeneration(const GenerateParams& params);

    /** GET /api/generate/status/<jobId> */
    JobStatus getStatus(const std::string& jobId);

    /** GET /progress (optional) */
    ProgressInfo getProgress();

    /** GET <base>/audio/<path> or /audio/refs/<path>; returns raw bytes (WAV) */
    std::vector<uint8_t> fetchAudio(const std::string& path);

    /** Last HTTP or parse error message */
    std::string lastError() const { return lastError_; }

private:
    std::string base_;
    std::string lastError_;

    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& jsonBody);
};

} // namespace aceforge

#endif
