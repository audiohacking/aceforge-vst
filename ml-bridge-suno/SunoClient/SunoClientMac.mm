/**
 * SunoClient implementation for macOS using NSURLSession (synchronous).
 * Auth: Bearer token. API base: https://api.sunoapi.org
 */
#ifdef __APPLE__

#include "SunoClient.hpp"
#include <Foundation/Foundation.h>
#include <algorithm>
#include <sstream>

static std::string nsstringToStd(NSString* s) {
    if (!s) return {};
    const char* c = [s UTF8String];
    return c ? std::string(c) : std::string();
}

static NSString* stdToNSString(const std::string& s) {
    return [NSString stringWithUTF8String:s.c_str()];
}

namespace suno {

static std::string escapeJsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if ((unsigned char)c >= 32) out += c;
    }
    return out;
}

// Perform request with Bearer auth; copy body in block so no NSData* used after block.
static void performRequest(NSURLRequest* request, const std::string& bearerToken,
                          std::string* outBody, NSHTTPURLResponse** outResponse, NSError** outError) {
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block std::string body;
    __block NSHTTPURLResponse* resultResp = nil;
    __block NSError* resultErr = nil;
    NSURLSession* session = [NSURLSession sharedSession];
    NSMutableURLRequest* req = [request isKindOfClass:[NSMutableURLRequest class]] ? (NSMutableURLRequest*)request : [request mutableCopy];
    if (!bearerToken.empty())
        [req setValue:[NSString stringWithFormat:@"Bearer %s", bearerToken.c_str()] forHTTPHeaderField:@"Authorization"];
    [[session dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        resultResp = (NSHTTPURLResponse*)response;
        resultErr = error;
        if (data && [data length] > 0) {
            NSData* dataCopy = [data copy];
            if (dataCopy)
                body.assign((const char*)[dataCopy bytes], (size_t)[dataCopy length]);
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    if (outResponse) *outResponse = resultResp;
    if (outError) *outError = resultErr;
    if (outBody) *outBody = std::move(body);
}

SunoClient::SunoClient(std::string apiKey) : apiKey_(std::move(apiKey)) {}

SunoClient::~SunoClient() = default;

std::string SunoClient::get(const std::string& path) {
    lastError_.clear();
    if (apiKey_.empty()) { lastError_ = "No API key"; return {}; }
    std::string urlStr = std::string(kBaseUrl) + (path.empty() || path[0] != '/' ? "/" : "") + path;
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid URL"; return {}; }
    NSURLRequest* req = [NSURLRequest requestWithURL:url];
    NSHTTPURLResponse* resp = nil;
    NSError* err = nil;
    std::string body;
    performRequest(req, apiKey_, &body, &resp, &err);
    if (err) { lastError_ = nsstringToStd([err localizedDescription]); return {}; }
    if (resp && resp.statusCode >= 400) {
        lastError_ = "HTTP " + std::to_string((int)resp.statusCode) + (body.empty() ? "" : " " + body.substr(0, 200));
        return {};
    }
    return body;
}

std::string SunoClient::post(const std::string& path, const std::string& jsonBody) {
    lastError_.clear();
    if (apiKey_.empty()) { lastError_ = "No API key"; return {}; }
    std::string urlStr = std::string(kBaseUrl) + (path.empty() || path[0] != '/' ? "/" : "") + path;
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid URL"; return {}; }
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [req setHTTPBody:[NSData dataWithBytes:jsonBody.data() length:jsonBody.size()]];
    NSHTTPURLResponse* resp = nil;
    NSError* err = nil;
    std::string body;
    performRequest(req, apiKey_, &body, &resp, &err);
    if (err) { lastError_ = nsstringToStd([err localizedDescription]); return {}; }
    if (resp && resp.statusCode >= 400) {
        lastError_ = "HTTP " + std::to_string((int)resp.statusCode) + (body.empty() ? "" : " " + body.substr(0, 200));
        return {};
    }
    return body;
}

bool SunoClient::checkCredits() {
    std::string body = get("/api/v1/generate/credit");
    if (body.empty()) return false;
    return body.find("\"code\":200") != std::string::npos || body.find("\"data\":") != std::string::npos;
}

// Build JSON for generate (callBackUrl required by API; we poll so use placeholder)
static std::string buildGenerateJson(const GenerateParams& p, const std::string* uploadUrl = nullptr) {
    std::ostringstream o;
    o << "{\"customMode\":" << (p.customMode ? "true" : "false")
      << ",\"instrumental\":" << (p.instrumental ? "true" : "false")
      << ",\"model\":\"" << modelToString(p.model) << "\""
      << ",\"callBackUrl\":\"https://example.com/callback\"";
    if (uploadUrl && !uploadUrl->empty())
        o << ",\"uploadUrl\":\"" << escapeJsonString(*uploadUrl) << "\"";
    if (!p.prompt.empty()) o << ",\"prompt\":\"" << escapeJsonString(p.prompt) << "\"";
    if (!p.style.empty()) o << ",\"style\":\"" << escapeJsonString(p.style) << "\"";
    if (!p.title.empty()) o << ",\"title\":\"" << escapeJsonString(p.title) << "\"";
    if (!p.personaId.empty()) o << ",\"personaId\":\"" << escapeJsonString(p.personaId) << "\"";
    if (!p.negativeTags.empty()) o << ",\"negativeTags\":\"" << escapeJsonString(p.negativeTags) << "\"";
    if (!p.vocalGender.empty()) o << ",\"vocalGender\":\"" << escapeJsonString(p.vocalGender) << "\"";
    o << ",\"styleWeight\":" << p.styleWeight;
    o << ",\"weirdnessConstraint\":" << p.weirdnessConstraint;
    o << ",\"audioWeight\":" << p.audioWeight << "}";
    return o.str();
}

std::string SunoClient::startGenerate(const GenerateParams& params) {
    std::string body = post("/api/v1/generate", buildGenerateJson(params));
    if (body.empty()) return {};
    // Parse data.taskId
    size_t i = body.find("\"taskId\"");
    if (i == std::string::npos) { lastError_ = "No taskId in response"; return {}; }
    i = body.find(':', i);
    if (i == std::string::npos) return {};
    i = body.find('"', i);
    if (i == std::string::npos) return {};
    size_t j = body.find('"', i + 1);
    if (j == std::string::npos) return {};
    return body.substr(i + 1, j - (i + 1));
}

std::string SunoClient::startUploadCover(const std::string& uploadUrl, const GenerateParams& params) {
    std::string json = buildGenerateJson(params, &uploadUrl);
    std::string body = post("/api/v1/generate/upload-cover", json);
    if (body.empty()) return {};
    size_t i = body.find("\"taskId\"");
    if (i == std::string::npos) { lastError_ = "No taskId in response"; return {}; }
    i = body.find(':', i); if (i == std::string::npos) return {};
    i = body.find('"', i); if (i == std::string::npos) return {};
    size_t j = body.find('"', i + 1); if (j == std::string::npos) return {};
    return body.substr(i + 1, j - (i + 1));
}

std::string SunoClient::startAddVocals(const AddVocalsParams& params) {
    std::ostringstream o;
    o << "{\"uploadUrl\":\"" << escapeJsonString(params.uploadUrl) << "\""
      << ",\"prompt\":\"" << escapeJsonString(params.prompt) << "\""
      << ",\"title\":\"" << escapeJsonString(params.title) << "\""
      << ",\"negativeTags\":\"" << escapeJsonString(params.negativeTags) << "\""
      << ",\"style\":\"" << escapeJsonString(params.style) << "\""
      << ",\"callBackUrl\":\"https://example.com/callback\"";
    if (!params.vocalGender.empty()) o << ",\"vocalGender\":\"" << escapeJsonString(params.vocalGender) << "\"";
    o << ",\"styleWeight\":" << params.styleWeight
      << ",\"weirdnessConstraint\":" << params.weirdnessConstraint
      << ",\"audioWeight\":" << params.audioWeight
      << ",\"model\":\"" << modelToString(params.model) << "\"}";
    std::string body = post("/api/v1/generate/add-vocals", o.str());
    if (body.empty()) return {};
    size_t i = body.find("\"taskId\"");
    if (i == std::string::npos) { lastError_ = "No taskId in response"; return {}; }
    i = body.find(':', i); if (i == std::string::npos) return {};
    i = body.find('"', i); if (i == std::string::npos) return {};
    size_t j = body.find('"', i + 1); if (j == std::string::npos) return {};
    return body.substr(i + 1, j - (i + 1));
}

// Parse GET record-info response for status and audioUrls
static void parseRecordInfo(const std::string& body, TaskStatus* out) {
    if (!out) return;
    auto pos = body.find("\"status\"");
    if (pos != std::string::npos) {
        pos = body.find('"', body.find(':', pos) + 1);
        size_t end = body.find('"', pos + 1);
        if (pos != std::string::npos && end != std::string::npos)
            out->status = body.substr(pos + 1, end - (pos + 1));
    }
    pos = body.find("\"errorMessage\"");
    if (pos != std::string::npos) {
        pos = body.find('"', body.find(':', pos) + 1);
        size_t end = body.find('"', pos + 1);
        if (pos != std::string::npos && end != std::string::npos)
            out->errorMessage = body.substr(pos + 1, end - (pos + 1));
    }
    pos = body.find("\"taskId\"");
    if (pos != std::string::npos) {
        pos = body.find('"', body.find(':', pos) + 1);
        size_t end = body.find('"', pos + 1);
        if (pos != std::string::npos && end != std::string::npos)
            out->taskId = body.substr(pos + 1, end - (pos + 1));
    }
    // sunoData array: each element has "audioUrl"
    size_t idx = 0;
    while ((idx = body.find("\"audioUrl\"", idx)) != std::string::npos) {
        idx = body.find(':', idx);
        if (idx == std::string::npos) break;
        size_t q = body.find('"', idx);
        if (q == std::string::npos) break;
        size_t r = body.find('"', q + 1);
        if (r != std::string::npos)
            out->audioUrls.push_back(body.substr(q + 1, r - (q + 1)));
        idx = r + 1;
    }
}

TaskStatus SunoClient::getTaskStatus(const std::string& taskId) {
    TaskStatus out;
    out.taskId = taskId;
    lastError_.clear();
    if (apiKey_.empty()) { lastError_ = "No API key"; return out; }
    std::string path = "/api/v1/generate/record-info?taskId=" + taskId;
    std::string body = get(path);
    if (body.empty()) return out;
    parseRecordInfo(body, &out);
    return out;
}

// Upload audio via multipart to Suno file upload (if same host) or base64
// Suno docs: file-stream-upload at sunoapiorg.redpandaai.co or api.sunoapi.org
std::string SunoClient::uploadAudio(const std::vector<uint8_t>& audioWavOrMp3, const std::string& fileName) {
    lastError_.clear();
    if (apiKey_.empty()) { lastError_ = "No API key"; return {}; }
    std::string name = fileName.empty() ? "audio.wav" : fileName;
    std::string urlStr = std::string(kUploadBaseUrl) + "/api/file-stream-upload";
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid upload URL"; return {}; }
    NSString* boundary = @"----SunoUploadBoundary";
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];
    [req setValue:[NSString stringWithFormat:@"Bearer %s", apiKey_.c_str()] forHTTPHeaderField:@"Authorization"];
    [req setValue:[NSString stringWithFormat:@"multipart/form-data; boundary=%@", boundary] forHTTPHeaderField:@"Content-Type"];
    NSMutableData* body = [NSMutableData data];
    [body appendData:[[NSString stringWithFormat:@"--%@\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];
    [body appendData:[[NSString stringWithFormat:@"Content-Disposition: form-data; name=\"file\"; filename=\"%@\"\r\n", stdToNSString(name)] dataUsingEncoding:NSUTF8StringEncoding]];
    [body appendData:[@"Content-Type: application/octet-stream\r\n\r\n" dataUsingEncoding:NSUTF8StringEncoding]];
    [body appendData:[NSData dataWithBytes:audioWavOrMp3.data() length:audioWavOrMp3.size()]];
    [body appendData:[[NSString stringWithFormat:@"\r\n--%@--\r\n", boundary] dataUsingEncoding:NSUTF8StringEncoding]];
    [req setHTTPBody:body];
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block std::string responseStr;
    __block NSHTTPURLResponse* resp = nil;
    __block NSError* err = nil;
    [[[NSURLSession sharedSession] dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* r, NSError* e) {
        resp = (NSHTTPURLResponse*)r;
        err = e;
        if (data && [data length] > 0) {
            NSData* copy = [data copy];
            if (copy) responseStr.assign((const char*)[copy bytes], (size_t)[copy length]);
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    if (err) { lastError_ = nsstringToStd([err localizedDescription]); return {}; }
    if (resp && resp.statusCode >= 400) { lastError_ = "Upload HTTP " + std::to_string((int)resp.statusCode); return {}; }
    // Parse data.fileUrl from response
    size_t i = responseStr.find("\"fileUrl\"");
    if (i == std::string::npos) { i = responseStr.find("\"downloadUrl\""); }
    if (i == std::string::npos) { lastError_ = "No fileUrl in upload response"; return {}; }
    i = responseStr.find('"', responseStr.find(':', i) + 1);
    size_t j = responseStr.find('"', i + 1);
    if (j != std::string::npos)
        return responseStr.substr(i + 1, j - (i + 1));
    return {};
}

std::vector<uint8_t> SunoClient::fetchAudio(const std::string& url) {
    lastError_.clear();
    NSURL* nsUrl = [NSURL URLWithString:stdToNSString(url)];
    if (!nsUrl) { lastError_ = "Invalid audio URL"; return {}; }
    NSURLRequest* req = [NSURLRequest requestWithURL:nsUrl];
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block std::vector<uint8_t> out;
    [[[NSURLSession sharedSession] dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        if (data && [data length] > 0) {
            NSData* dataCopy = [data copy];
            if (dataCopy) {
                size_t len = (size_t)[dataCopy length];
                out.resize(len);
                [dataCopy getBytes:out.data() length:len];
            }
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return out;
}

} // namespace suno

#endif
