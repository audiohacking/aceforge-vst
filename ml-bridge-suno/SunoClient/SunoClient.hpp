/**
 * Suno API client for Music Generation (https://docs.sunoapi.org/).
 * Uses Bearer token auth. No AceForge compatibility.
 */
#ifndef SUNO_CLIENT_HPP
#define SUNO_CLIENT_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace suno {

// Model enum per API
enum class Model { V4, V4_5, V4_5PLUS, V4_5ALL, V5 };
inline const char* modelToString(Model m) {
    switch (m) { case Model::V4: return "V4"; case Model::V4_5: return "V4_5"; case Model::V4_5PLUS: return "V4_5PLUS"; case Model::V4_5ALL: return "V4_5ALL"; case Model::V5: return "V5"; }
    return "V4_5ALL";
}

// Common params for generate / upload-cover
struct GenerateParams {
    bool customMode = false;
    bool instrumental = true;
    Model model = Model::V4_5ALL;
    std::string prompt;
    std::string style;
    std::string title;
    std::string personaId;
    std::string negativeTags;
    std::string vocalGender;  // "m" | "f"
    double styleWeight = 0.65;
    double weirdnessConstraint = 0.65;
    double audioWeight = 0.65;
};

// Task status from GET record-info
struct TaskStatus {
    std::string taskId;
    std::string status;  // PENDING, TEXT_SUCCESS, FIRST_SUCCESS, SUCCESS, ...
    std::string errorMessage;
    std::vector<std::string> audioUrls;  // from sunoData[].audioUrl
};

// Add Vocals params (instrumental -> add vocals)
struct AddVocalsParams {
    std::string uploadUrl;  // instrumental audio URL
    std::string prompt;
    std::string title;
    std::string negativeTags;
    std::string style;
    std::string vocalGender;
    double styleWeight = 0.61;
    double weirdnessConstraint = 0.72;
    double audioWeight = 0.65;
    Model model = Model::V4_5PLUS;
};

class SunoClient {
public:
    static constexpr const char* kBaseUrl = "https://api.sunoapi.org";
    static constexpr const char* kUploadBaseUrl = "https://api.sunoapi.org";  // or sunoapiorg.redpandaai.co if needed

    explicit SunoClient(std::string apiKey = "");
    ~SunoClient();

    void setApiKey(const std::string& key) { apiKey_ = key; }
    std::string getApiKey() const { return apiKey_; }
    bool hasApiKey() const { return !apiKey_.empty(); }

    /** GET /api/v1/credit/balance or similar to check key */
    bool checkCredits();

    /** POST /api/v1/generate - returns taskId or empty */
    std::string startGenerate(const GenerateParams& params);

    /** POST /api/v1/generate/upload-cover - uploadUrl + same params; returns taskId */
    std::string startUploadCover(const std::string& uploadUrl, const GenerateParams& params);

    /** POST /api/v1/generate/add-vocals - returns taskId */
    std::string startAddVocals(const AddVocalsParams& params);

    /** GET /api/v1/generate/record-info?taskId=xxx */
    TaskStatus getTaskStatus(const std::string& taskId);

    /** Upload audio bytes via file-stream or base64; returns URL for uploadUrl, or empty */
    std::string uploadAudio(const std::vector<uint8_t>& audioWavOrMp3, const std::string& fileName);

    /** Fetch audio from URL (e.g. from task result); returns raw bytes */
    std::vector<uint8_t> fetchAudio(const std::string& url);

    std::string lastError() const { return lastError_; }

private:
    std::string apiKey_;
    mutable std::string lastError_;

    std::string get(const std::string& path);
    std::string post(const std::string& path, const std::string& jsonBody);
    std::string postMultipart(const std::string& path, const std::string& filePartName,
                              const void* fileData, size_t fileSize, const std::string& fileName);
};

} // namespace suno

#endif
