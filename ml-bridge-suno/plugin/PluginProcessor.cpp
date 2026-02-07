#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <thread>
#include <vector>

namespace
{
suno::Model modelFromIndex(int index)
{
    switch (index)
    {
    case 0: return suno::Model::V4;
    case 1: return suno::Model::V4_5;
    case 2: return suno::Model::V4_5PLUS;
    case 3: return suno::Model::V4_5ALL;
    case 4: return suno::Model::V5;
    default: return suno::Model::V4_5ALL;
    }
}
} // namespace

AceForgeSunoAudioProcessor::AceForgeSunoAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
    client_ = std::make_unique<suno::SunoClient>("");
    playbackBuffer_.resize(kPlaybackFifoFrames * 2, 0.0f);
    {
        juce::ScopedLock l(statusLock_);
        statusText_ = "Set API key and click Generate, or record and use Cover / Add Vocals.";
    }
}

AceForgeSunoAudioProcessor::~AceForgeSunoAudioProcessor()
{
    cancelPendingUpdate();
}

void AceForgeSunoAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampleRate_.store(sampleRate);
}

void AceForgeSunoAudioProcessor::releaseResources() {}

void AceForgeSunoAudioProcessor::setApiKey(const juce::String& key)
{
    apiKey_ = key.trim();
    if (client_)
        client_->setApiKey(apiKey_.toStdString());
    connected_.store(false);
    if (client_ && client_->hasApiKey() && client_->checkCredits())
        connected_.store(true);
}

void AceForgeSunoAudioProcessor::clearRecordedBuffer()
{
    juce::ScopedLock l(recordLock_);
    recordBuffer_.clear();
}

bool AceForgeSunoAudioProcessor::hasRecordedAudio() const
{
    juce::ScopedLock l(recordLock_);
    return recordBuffer_.size() >= 2 * 44100; // at least ~1 sec stereo at 44.1k
}

int AceForgeSunoAudioProcessor::getRecordedSamples() const
{
    juce::ScopedLock l(recordLock_);
    const int ch = 2;
    const size_t n = recordBuffer_.size();
    return static_cast<int>((n / ch) * ch); // frames * channels
}

std::vector<uint8_t> AceForgeSunoAudioProcessor::encodeRecordedAsWav() const
{
    std::vector<float> copy;
    double rate = 44100.0;
    {
        juce::ScopedLock l(recordLock_);
        if (recordBuffer_.empty())
            return {};
        copy = recordBuffer_;
        rate = sampleRate_.load();
    }
    const int numCh = 2;
    const int numFrames = static_cast<int>(copy.size()) / numCh;
    if (numFrames <= 0)
        return {};

    juce::MemoryBlock block;
    juce::WavAudioFormat wavFormat;
    juce::AudioBuffer<float> buf(numCh, numFrames);
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < numFrames; ++i)
            buf.setSample(c, i, copy[static_cast<size_t>(i) * 2u + static_cast<size_t>(c)]);

    std::unique_ptr<juce::MemoryOutputStream> mos(new juce::MemoryOutputStream(block, true));
    if (auto* writer = wavFormat.createWriterFor(mos.get(), rate, static_cast<unsigned int>(numCh), 24, {}, 0))
    {
        writer->writeFromAudioSampleBuffer(buf, 0, numFrames);
        writer->flush();
        mos->flush();
        size_t sz = block.getSize();
        if (sz > 0 && block.getData() != nullptr)
            return std::vector<uint8_t>(static_cast<const uint8_t*>(block.getData()),
                                        static_cast<const uint8_t*>(block.getData()) + sz);
    }
    return {};
}

void AceForgeSunoAudioProcessor::startGenerate(const juce::String& prompt, const juce::String& style, const juce::String& title,
                                                bool customMode, bool instrumental, int modelIndex)
{
    State expected = state_.load();
    while (expected == State::Submitting || expected == State::Running)
        return;
    if (!state_.compare_exchange_strong(expected, State::Submitting))
        return;

    jobPrompt_ = prompt;
    jobStyle_ = style;
    jobTitle_ = title.isEmpty() ? "aceforge_suno" : title;
    jobCustomMode_ = customMode;
    jobInstrumental_ = instrumental;
    jobModelIndex_ = modelIndex;
    jobIsCover_ = false;
    jobIsAddVocals_ = false;
    triggerAsyncUpdate();
    std::thread t(&AceForgeSunoAudioProcessor::runGenerateThread, this);
    t.detach();
}

void AceForgeSunoAudioProcessor::startUploadCover(const juce::String& prompt, const juce::String& style, const juce::String& title,
                                                  bool customMode, bool instrumental, int modelIndex)
{
    if (!hasRecordedAudio())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "No recorded audio. Record first, then Cover.";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    State expected = state_.load();
    while (expected == State::Submitting || expected == State::Running)
        return;
    if (!state_.compare_exchange_strong(expected, State::Submitting))
        return;

    jobPrompt_ = prompt;
    jobStyle_ = style;
    jobTitle_ = title.isEmpty() ? "aceforge_suno_cover" : title;
    jobCustomMode_ = customMode;
    jobInstrumental_ = instrumental;
    jobModelIndex_ = modelIndex;
    jobIsCover_ = true;
    jobIsAddVocals_ = false;
    triggerAsyncUpdate();
    std::thread t(&AceForgeSunoAudioProcessor::runUploadCoverThread, this);
    t.detach();
}

void AceForgeSunoAudioProcessor::startAddVocals(const juce::String& prompt, const juce::String& style, const juce::String& title)
{
    if (!hasRecordedAudio())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "No recorded audio. Record instrumental first, then Add Vocals.";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    State expected = state_.load();
    while (expected == State::Submitting || expected == State::Running)
        return;
    if (!state_.compare_exchange_strong(expected, State::Submitting))
        return;

    jobPrompt_ = prompt;
    jobStyle_ = style;
    jobTitle_ = title.isEmpty() ? "aceforge_suno_vocals" : title;
    jobIsCover_ = false;
    jobIsAddVocals_ = true;
    triggerAsyncUpdate();
    std::thread t(&AceForgeSunoAudioProcessor::runAddVocalsThread, this);
    t.detach();
}

void AceForgeSunoAudioProcessor::runGenerateThread()
{
    if (!client_ || !client_->hasApiKey())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "No API key";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    if (!client_->checkCredits())
    {
        connected_.store(false);
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "API key invalid or no credits: " + juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    connected_.store(true);

    suno::GenerateParams p;
    p.prompt = jobPrompt_.toStdString();
    p.style = jobStyle_.toStdString();
    p.title = jobTitle_.toStdString();
    p.customMode = jobCustomMode_;
    p.instrumental = jobInstrumental_;
    p.model = modelFromIndex(jobModelIndex_);

    std::string taskId = client_->startGenerate(p);
    if (taskId.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    state_.store(State::Running);
    {
        juce::ScopedLock l(statusLock_);
        statusText_ = "Generating…";
    }
    triggerAsyncUpdate();

    while (true)
    {
        suno::TaskStatus st = client_->getTaskStatus(taskId);
        juce::String statusStr(st.status.c_str());
        if (statusStr.equalsIgnoreCase("SUCCESS") || statusStr.equalsIgnoreCase("success"))
        {
            if (st.audioUrls.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = "No audio URL in result";
                statusText_ = lastError_;
                triggerAsyncUpdate();
                return;
            }
            std::vector<uint8_t> wavBytes = client_->fetchAudio(st.audioUrls[0]);
            if (wavBytes.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = juce::String(client_->lastError());
                triggerAsyncUpdate();
                return;
            }
            {
                juce::ScopedLock l(pendingWavLock_);
                pendingWavBytes_ = std::move(wavBytes);
                pendingPrompt_ = jobPrompt_;
            }
            triggerAsyncUpdate();
            return;
        }
        if (statusStr.isNotEmpty() && (statusStr.contains("fail") || statusStr.contains("error")))
        {
            state_.store(State::Failed);
            juce::ScopedLock l(statusLock_);
            lastError_ = juce::String(st.errorMessage.empty() ? st.status.c_str() : st.errorMessage.c_str());
            statusText_ = lastError_;
            triggerAsyncUpdate();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void AceForgeSunoAudioProcessor::runUploadCoverThread()
{
    if (!client_ || !client_->hasApiKey())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "No API key";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    if (!client_->checkCredits())
    {
        connected_.store(false);
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "API key invalid or no credits";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    connected_.store(true);

    std::vector<uint8_t> wavBytes = encodeRecordedAsWav();
    if (wavBytes.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "Failed to encode recorded audio as WAV";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    state_.store(State::Running);
    { juce::ScopedLock l(statusLock_); statusText_ = "Uploading…"; }
    triggerAsyncUpdate();

    std::string uploadUrl = client_->uploadAudio(wavBytes, "recorded.wav");
    if (uploadUrl.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    suno::GenerateParams p;
    p.prompt = jobPrompt_.toStdString();
    p.style = jobStyle_.toStdString();
    p.title = jobTitle_.toStdString();
    p.customMode = jobCustomMode_;
    p.instrumental = jobInstrumental_;
    p.model = modelFromIndex(jobModelIndex_);

    std::string taskId = client_->startUploadCover(uploadUrl, p);
    if (taskId.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    { juce::ScopedLock l(statusLock_); statusText_ = "Generating cover…"; }
    triggerAsyncUpdate();

    while (true)
    {
        suno::TaskStatus st = client_->getTaskStatus(taskId);
        juce::String statusStr(st.status.c_str());
        if (statusStr.equalsIgnoreCase("SUCCESS") || statusStr.equalsIgnoreCase("success"))
        {
            if (st.audioUrls.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = "No audio URL in result";
                statusText_ = lastError_;
                triggerAsyncUpdate();
                return;
            }
            std::vector<uint8_t> audioBytes = client_->fetchAudio(st.audioUrls[0]);
            if (audioBytes.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = juce::String(client_->lastError());
                triggerAsyncUpdate();
                return;
            }
            {
                juce::ScopedLock l(pendingWavLock_);
                pendingWavBytes_ = std::move(audioBytes);
                pendingPrompt_ = jobPrompt_;
            }
            triggerAsyncUpdate();
            return;
        }
        if (statusStr.isNotEmpty() && (statusStr.contains("fail") || statusStr.contains("error")))
        {
            state_.store(State::Failed);
            juce::ScopedLock l(statusLock_);
            lastError_ = juce::String(st.errorMessage.empty() ? st.status.c_str() : st.errorMessage.c_str());
            statusText_ = lastError_;
            triggerAsyncUpdate();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void AceForgeSunoAudioProcessor::runAddVocalsThread()
{
    if (!client_ || !client_->hasApiKey())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "No API key";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    if (!client_->checkCredits())
    {
        connected_.store(false);
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "API key invalid or no credits";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }
    connected_.store(true);

    std::vector<uint8_t> wavBytes = encodeRecordedAsWav();
    if (wavBytes.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "Failed to encode recorded audio";
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    state_.store(State::Running);
    { juce::ScopedLock l(statusLock_); statusText_ = "Uploading…"; }
    triggerAsyncUpdate();

    std::string uploadUrl = client_->uploadAudio(wavBytes, "instrumental.wav");
    if (uploadUrl.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    suno::AddVocalsParams p;
    p.uploadUrl = uploadUrl;
    p.prompt = jobPrompt_.toStdString();
    p.style = jobStyle_.toStdString();
    p.title = jobTitle_.toStdString();
    p.model = modelFromIndex(jobModelIndex_);

    std::string taskId = client_->startAddVocals(p);
    if (taskId.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    { juce::ScopedLock l(statusLock_); statusText_ = "Adding vocals…"; }
    triggerAsyncUpdate();

    while (true)
    {
        suno::TaskStatus st = client_->getTaskStatus(taskId);
        juce::String statusStr(st.status.c_str());
        if (statusStr.equalsIgnoreCase("SUCCESS") || statusStr.equalsIgnoreCase("success"))
        {
            if (st.audioUrls.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = "No audio URL in result";
                statusText_ = lastError_;
                triggerAsyncUpdate();
                return;
            }
            std::vector<uint8_t> audioBytes = client_->fetchAudio(st.audioUrls[0]);
            if (audioBytes.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = juce::String(client_->lastError());
                triggerAsyncUpdate();
                return;
            }
            {
                juce::ScopedLock l(pendingWavLock_);
                pendingWavBytes_ = std::move(audioBytes);
                pendingPrompt_ = jobPrompt_;
            }
            triggerAsyncUpdate();
            return;
        }
        if (statusStr.isNotEmpty() && (statusStr.contains("fail") || statusStr.contains("error")))
        {
            state_.store(State::Failed);
            juce::ScopedLock l(statusLock_);
            lastError_ = juce::String(st.errorMessage.empty() ? st.status.c_str() : st.errorMessage.c_str());
            statusText_ = lastError_;
            triggerAsyncUpdate();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void AceForgeSunoAudioProcessor::pushSamplesToPlayback(const float* interleaved, int numFrames,
                                                       int sourceChannels, double sourceSampleRate)
{
    if (numFrames <= 0 || interleaved == nullptr)
        return;
    const double hostRate = sampleRate_.load(std::memory_order_relaxed);
    const double ratio = sourceSampleRate > 0.0 ? hostRate / sourceSampleRate : 1.0;
    const int outFrames = static_cast<int>(std::round(static_cast<double>(numFrames) * ratio));
    if (outFrames <= 0 || outFrames > kPlaybackFifoFrames)
        return;

    const int writeIdx = nextWriteIndex_.load(std::memory_order_relaxed);
    std::vector<float>& outBuf = pendingPlaybackBuffer_[writeIdx];
    outBuf.resize(static_cast<size_t>(outFrames) * 2u);
    float* out = outBuf.data();
    for (int i = 0; i < outFrames; ++i)
    {
        const double srcIdx = ratio > 0.0 ? (double)i / ratio : (double)i;
        const int i0 = std::min(std::max(0, static_cast<int>(srcIdx)), numFrames - 1);
        const int i1 = std::min(i0 + 1, numFrames - 1);
        const float t = static_cast<float>(srcIdx - std::floor(srcIdx));
        float l = 0.0f, r = 0.0f;
        if (sourceChannels >= 1)
        {
            l = interleaved[i0 * sourceChannels] * (1.0f - t) + interleaved[i1 * sourceChannels] * t;
            r = sourceChannels >= 2
                    ? interleaved[i0 * sourceChannels + 1] * (1.0f - t) + interleaved[i1 * sourceChannels + 1] * t
                    : l;
        }
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
    pendingPlaybackFrames_.store(outFrames, std::memory_order_release);
    pendingPlaybackBufferIndex_.store(writeIdx, std::memory_order_release);
    pendingPlaybackReady_.store(true, std::memory_order_release);
    nextWriteIndex_.store(1 - writeIdx, std::memory_order_release);
}

void AceForgeSunoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    // Host BPM from playhead when available
    if (auto* playhead = getPlayHead())
    {
        juce::Optional<juce::AudioPlayHead::PositionInfo> pos = playhead->getPosition();
        if (pos && pos->getBpm().hasValue())
            hostBpm_.store(*pos->getBpm());
    }

    // Recording: append input under lock
    if (recording_.load() && numCh >= 2)
    {
        juce::ScopedLock l(recordLock_);
        const size_t start = recordBuffer_.size();
        recordBuffer_.resize(start + static_cast<size_t>(numSamples) * 2u);
        for (int i = 0; i < numSamples; ++i)
        {
            recordBuffer_[start + static_cast<size_t>(i) * 2u] = buffer.getSample(0, i);
            recordBuffer_[start + static_cast<size_t>(i) * 2u + 1u] = buffer.getSample(1, i);
        }
    }

    // Playback: same double-buffer / fifo pattern as AceForge
    if (pendingPlaybackReady_.exchange(false, std::memory_order_acq_rel))
    {
        const int N = pendingPlaybackFrames_.load(std::memory_order_acquire);
        const int bufIdx = pendingPlaybackBufferIndex_.load(std::memory_order_acquire);
        const std::vector<float>& srcBuf = pendingPlaybackBuffer_[bufIdx];
        if (N > 0 && N <= kPlaybackFifoFrames && srcBuf.size() >= static_cast<size_t>(N) * 2u)
        {
            playbackFifo_.reset();
            int start1, block1, start2, block2;
            playbackFifo_.prepareToWrite(N, start1, block1, start2, block2);
            const float* src = srcBuf.data();
            auto copyBlock = [&](int fifoStart, int count, int srcOffset)
            {
                for (int i = 0; i < count && (fifoStart + i) * 2 + 1 < static_cast<int>(playbackBuffer_.size()); ++i)
                {
                    const int s = (srcOffset + i) * 2;
                    playbackBuffer_[static_cast<size_t>(fifoStart + i) * 2u] = src[s];
                    playbackBuffer_[static_cast<size_t>(fifoStart + i) * 2u + 1u] = src[s + 1];
                }
            };
            copyBlock(start1, block1, 0);
            copyBlock(start2, block2, block1);
            playbackFifo_.finishedWrite(block1 + block2);
        }
    }

    int start1, block1, start2, block2;
    playbackFifo_.prepareToRead(numSamples, start1, block1, start2, block2);
    auto readFrames = [&](int fifoStart, int count, int bufferOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            const size_t base = static_cast<size_t>(fifoStart + i) * 2u;
            if (base + 1 < playbackBuffer_.size() && bufferOffset + i < numSamples)
            {
                buffer.setSample(0, bufferOffset + i, playbackBuffer_[base]);
                buffer.setSample(1, bufferOffset + i, playbackBuffer_[base + 1]);
            }
        }
    };
    readFrames(start1, block1, 0);
    readFrames(start2, block2, block1);
    playbackFifo_.finishedRead(block1 + block2);

    const int readCount = block1 + block2;
    for (int i = readCount; i < numSamples; ++i)
    {
        buffer.setSample(0, i, 0.0f);
        buffer.setSample(1, i, 0.0f);
    }
}

void AceForgeSunoAudioProcessor::handleAsyncUpdate()
{
    std::vector<uint8_t> wavBytes;
    juce::String promptForLibrary;
    {
        juce::ScopedLock l(pendingWavLock_);
        if (pendingWavBytes_.empty())
            return;
        wavBytes = std::move(pendingWavBytes_);
        pendingWavBytes_.clear();
        promptForLibrary = pendingPrompt_;
    }

    juce::AudioFormatManager fm;
    fm.registerFormat(new juce::WavAudioFormat(), true);
    std::unique_ptr<juce::MemoryInputStream> mis(new juce::MemoryInputStream(wavBytes.data(), wavBytes.size(), false));
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(mis)));
    if (!reader)
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "Failed to decode WAV";
        statusText_ = lastError_;
        return;
    }
    const double fileSampleRate = reader->sampleRate;
    const int numCh = static_cast<int>(reader->numChannels);
    const int numSamples = static_cast<int>(reader->lengthInSamples);
    if (numSamples <= 0 || numCh <= 0)
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "Invalid WAV";
        statusText_ = lastError_;
        return;
    }
    juce::AudioBuffer<float> fileBuffer(numCh, numSamples);
    if (!reader->read(&fileBuffer, 0, numSamples, 0, true, true))
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = "Failed to read WAV samples";
        statusText_ = lastError_;
        return;
    }
    std::vector<float> interleaved(static_cast<size_t>(numSamples) * 2u);
    for (int i = 0; i < numSamples; ++i)
    {
        interleaved[static_cast<size_t>(i) * 2u] = numCh > 0 ? fileBuffer.getSample(0, i) : 0.0f;
        interleaved[static_cast<size_t>(i) * 2u + 1u] = numCh > 1 ? fileBuffer.getSample(1, i) : interleaved[static_cast<size_t>(i) * 2u];
    }
    pushSamplesToPlayback(interleaved.data(), numSamples, 2, fileSampleRate);
    state_.store(State::Succeeded);
    {
        juce::ScopedLock l(statusLock_);
        statusText_ = "Generated - playing.";
    }

    juce::File libDir = getLibraryDirectory();
    juce::String baseName = "suno_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    juce::File wavFile = libDir.getChildFile(baseName + ".wav");
    std::unique_ptr<juce::OutputStream> outStream = wavFile.createOutputStream();
    if (outStream != nullptr)
    {
        juce::WavAudioFormat wavFormat;
        auto options = juce::AudioFormatWriterOptions{}
                          .withSampleRate(fileSampleRate)
                          .withNumChannels(numCh)
                          .withBitsPerSample(24);
        if (auto writer = wavFormat.createWriterFor(outStream, options))
        {
            writer->writeFromAudioSampleBuffer(fileBuffer, 0, numSamples);
            writer->flush();
        }
    }
}

juce::String AceForgeSunoAudioProcessor::getStatusText() const
{
    juce::ScopedLock l(statusLock_);
    return statusText_;
}

juce::String AceForgeSunoAudioProcessor::getLastError() const
{
    juce::ScopedLock l(statusLock_);
    return lastError_;
}

juce::File AceForgeSunoAudioProcessor::getLibraryDirectory() const
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                         .getChildFile("AceForgeSuno")
                         .getChildFile("Generations");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

std::vector<AceForgeSunoAudioProcessor::LibraryEntry> AceForgeSunoAudioProcessor::getLibraryEntries() const
{
    juce::Array<juce::File> wavs;
    getLibraryDirectory().findChildFiles(wavs, juce::File::findFiles, false, "*.wav");
    std::vector<LibraryEntry> entries;
    for (const juce::File& f : wavs)
        entries.push_back({ f, f.getFileName().upToFirstOccurrenceOf(".", false, false), f.getLastModificationTime() });
    std::sort(entries.begin(), entries.end(),
              [](const LibraryEntry& a, const LibraryEntry& b) { return a.time > b.time; });
    return entries;
}

// --- Boilerplate ---
juce::AudioProcessorEditor* AceForgeSunoAudioProcessor::createEditor() { return new AceForgeSunoAudioProcessorEditor(*this); }
bool AceForgeSunoAudioProcessor::hasEditor() const { return true; }
const juce::String AceForgeSunoAudioProcessor::getName() const { return "AceForge-Suno"; }
bool AceForgeSunoAudioProcessor::acceptsMidi() const { return false; }
bool AceForgeSunoAudioProcessor::producesMidi() const { return false; }
bool AceForgeSunoAudioProcessor::isMidiEffect() const { return false; }
double AceForgeSunoAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AceForgeSunoAudioProcessor::getNumPrograms() { return 1; }
int AceForgeSunoAudioProcessor::getCurrentProgram() { return 0; }
void AceForgeSunoAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AceForgeSunoAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return "Default"; }
void AceForgeSunoAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }
bool AceForgeSunoAudioProcessor::isBusesLayoutSupported(const juce::AudioProcessor::BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void AceForgeSunoAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeString(apiKey_);
}

void AceForgeSunoAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    apiKey_ = stream.readString();
    if (client_)
        client_->setApiKey(apiKey_.toStdString());
    if (client_ && client_->hasApiKey() && client_->checkCredits())
        connected_.store(true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AceForgeSunoAudioProcessor();
}
