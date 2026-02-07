/**
 * AceForgeClient implementation for macOS using NSURLSession (synchronous).
 * Compile as Objective-C++: clang++ -x objective-c++ -framework Foundation -framework Security ...
 */
#ifdef __APPLE__

#include "AceForgeClient.hpp"
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

namespace aceforge {

static std::string trimPath(const std::string& path) {
    auto start = path.find_first_not_of('/');
    if (start == std::string::npos) return path;
    return path.substr(start);
}

AceForgeClient::AceForgeClient(std::string baseUrl) : base_(std::move(baseUrl)) {
    while (!base_.empty() && base_.back() == '/') base_.pop_back();
}

AceForgeClient::~AceForgeClient() = default;

void AceForgeClient::setBaseUrl(const std::string& url) {
    base_ = url;
    while (!base_.empty() && base_.back() == '/') base_.pop_back();
}

std::string AceForgeClient::getBaseUrl() const { return base_; }

// Perform request and copy response body into a __block buffer so we never use NSData* after the block.
static void performRequestCopyBody(NSURLRequest* request, std::string* outBody, NSHTTPURLResponse** outResponse, NSError** outError) {
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block std::string body;
    __block NSHTTPURLResponse* resultResp = nil;
    __block NSError* resultErr = nil;
    NSURLSession* session = [NSURLSession sharedSession];
    [[session dataTaskWithRequest:request completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
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

std::string AceForgeClient::get(const std::string& path) {
    lastError_.clear();
    std::string urlStr = base_ + (path.empty() || path[0] != '/' ? "/" : "") + path;
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid URL"; return {}; }
    NSURLRequest* req = [NSURLRequest requestWithURL:url];
    NSError* err = nil;
    NSHTTPURLResponse* resp = nil;
    std::string body;
    performRequestCopyBody(req, &body, &resp, &err);
    if (err) {
        lastError_ = nsstringToStd([err localizedDescription]);
        return {};
    }
    if (resp && resp.statusCode >= 400) {
        lastError_ = "HTTP " + std::to_string((int)resp.statusCode);
        return {};
    }
    return body;
}

std::string AceForgeClient::post(const std::string& path, const std::string& jsonBody) {
    lastError_.clear();
    std::string urlStr = base_ + (path.empty() || path[0] != '/' ? "/" : "") + path;
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid URL"; return {}; }
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [req setHTTPBody:[NSData dataWithBytes:jsonBody.data() length:jsonBody.size()]];
    NSError* err = nil;
    NSHTTPURLResponse* resp = nil;
    std::string body;
    performRequestCopyBody(req, &body, &resp, &err);
    if (err) {
        lastError_ = nsstringToStd([err localizedDescription]);
        return {};
    }
    if (resp && resp.statusCode >= 400) {
        lastError_ = "HTTP " + std::to_string((int)resp.statusCode);
        return {};
    }
    return body;
}

bool AceForgeClient::healthCheck() {
    std::string body = get("/api/generate/health");
    if (body.empty()) return false;
    return body.find("\"healthy\":true") != std::string::npos;
}

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

std::string AceForgeClient::startGeneration(const GenerateParams& params) {
    std::string sd = escapeJsonString(params.songDescription);
    std::string ly = escapeJsonString(params.lyrics);
    std::string ti = escapeJsonString(params.title);
    std::ostringstream json;
    json << "{"
         << "\"customMode\":false,"
         << "\"songDescription\":\"" << sd << "\","
         << "\"lyrics\":\"" << ly << "\","
         << "\"instrumental\":" << (params.instrumental ? "true" : "false") << ","
         << "\"duration\":" << params.durationSeconds << ","
         << "\"inferenceSteps\":" << params.inferenceSteps << ","
         << "\"guidanceScale\":" << params.guidanceScale << ","
         << "\"randomSeed\":" << (params.randomSeed ? "true" : "false") << ","
         << "\"seed\":" << params.seed << ","
         << "\"taskType\":\"" << params.taskType << "\","
         << "\"title\":\"" << ti << "\"";
    if (!params.referenceAudioUrl.empty()) {
        json << ",\"referenceAudioUrl\":\"" << escapeJsonString(params.referenceAudioUrl) << "\"";
        json << ",\"ref_audio_strength\":" << params.refAudioStrength;
    }
    if (!params.sourceAudioUrl.empty()) {
        json << ",\"sourceAudioUrl\":\"" << escapeJsonString(params.sourceAudioUrl) << "\"";
        json << ",\"audioCoverStrength\":" << params.audioCoverStrength;
    }
    json << "}";
    std::string body = post("/api/generate", json.str());
    if (body.empty()) return {};
    // Parse jobId: "jobId":"<uuid>"
    size_t pos = body.find("\"jobId\"");
    if (pos == std::string::npos) { lastError_ = "No jobId in response"; return {}; }
    pos = body.find(':', pos);
    if (pos == std::string::npos) return {};
    pos = body.find('"', pos);
    if (pos == std::string::npos) return {};
    size_t end = body.find('"', pos + 1);
    if (end == std::string::npos) return {};
    return body.substr(pos + 1, end - (pos + 1));
}

JobStatus AceForgeClient::getStatus(const std::string& jobId) {
    JobStatus out;
    out.jobId = jobId;
    std::string body = get("/api/generate/status/" + jobId);
    if (body.empty()) return out;
    out.status = "unknown";
    if (body.find("\"status\"") != std::string::npos) {
        size_t p = body.find("\"status\"");
        p = body.find('"', body.find(':', p) + 1);
        size_t e = body.find('"', p + 1);
        if (p != std::string::npos && e != std::string::npos)
            out.status = body.substr(p + 1, e - (p + 1));
    }
    if (body.find("\"error\"") != std::string::npos) {
        size_t p = body.find("\"error\"");
        p = body.find('"', body.find(':', p) + 1);
        size_t e = body.find('"', p + 1);
        if (p != std::string::npos && e != std::string::npos)
            out.error = body.substr(p + 1, e - (p + 1));
    }
    if (out.status == "succeeded" && body.find("\"audioUrls\"") != std::string::npos) {
        size_t p = body.find("[", body.find("\"audioUrls\""));
        size_t q = body.find('"', p);
        size_t r = body.find('"', q + 1);
        if (q != std::string::npos && r != std::string::npos)
            out.audioUrl = body.substr(q + 1, r - (q + 1));
    }
    return out;
}

ProgressInfo AceForgeClient::getProgress() {
    ProgressInfo out;
    std::string body = get("/progress");
    if (body.empty()) return out;
    // fraction
    size_t p = body.find("\"fraction\"");
    if (p != std::string::npos) {
        p = body.find(':', p);
        if (p != std::string::npos) out.fraction = (float)atof(body.c_str() + p + 1);
    }
    p = body.find("\"done\"");
    if (p != std::string::npos) out.done = body.find("true", p) < body.find("false", p) || body.find("false", p) == std::string::npos;
    return out;
}

std::vector<uint8_t> AceForgeClient::fetchAudio(const std::string& path) {
    lastError_.clear();
    std::string p = trimPath(path);
    std::string urlStr = base_ + "/" + p;
    NSURL* url = [NSURL URLWithString:stdToNSString(urlStr)];
    if (!url) { lastError_ = "Invalid path"; return {}; }
    NSURLRequest* req = [NSURLRequest requestWithURL:url];
    __block NSError* err = nil;
    __block NSHTTPURLResponse* resp = nil;
    __block std::vector<uint8_t> out;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSession* session = [NSURLSession sharedSession];
    [[session dataTaskWithRequest:req completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
        resp = (NSHTTPURLResponse*)response;
        err = error;
        if (data && [data length] > 0) {
            NSData* dataCopy = [data copy];  // Own the bytes; avoid using session's data after block
            if (dataCopy) {
                const NSUInteger len = [dataCopy length];
                out.resize((size_t)len);
                [dataCopy getBytes:out.data() length:len];
            }
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    if (err) {
        lastError_ = nsstringToStd([err localizedDescription]);
        return {};
    }
    if (resp && resp.statusCode >= 400) {
        lastError_ = "HTTP " + std::to_string((int)resp.statusCode);
        return {};
    }
    return out;
}

} // namespace aceforge

#endif // __APPLE__
