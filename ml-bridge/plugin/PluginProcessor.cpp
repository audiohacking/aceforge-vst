#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

namespace
{
const char* stateToString(AceForgeBridgeAudioProcessor::State s)
{
    switch (s)
    {
    case AceForgeBridgeAudioProcessor::State::Idle: return "Idle";
    case AceForgeBridgeAudioProcessor::State::Submitting: return "Submitting…";
    case AceForgeBridgeAudioProcessor::State::Queued: return "Queued…";
    case AceForgeBridgeAudioProcessor::State::Running: return "Generating…";
    case AceForgeBridgeAudioProcessor::State::Succeeded: return "Ready";
    case AceForgeBridgeAudioProcessor::State::Failed: return "Failed";
    }
    return "";
}
} // namespace

AceForgeBridgeAudioProcessor::AceForgeBridgeAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
    baseUrl_ = "http://127.0.0.1:5056";
    client_ = std::make_unique<aceforge::AceForgeClient>(baseUrl_.toStdString());
    playbackBuffer_.resize(kPlaybackFifoFrames * 2, 0.0f);
    {
        juce::ScopedLock l(statusLock_);
        statusText_ = "Idle – open the plugin and click Generate (10s).";
    }
}

AceForgeBridgeAudioProcessor::~AceForgeBridgeAudioProcessor() {}

void AceForgeBridgeAudioProcessor::setBaseUrl(const juce::String& url)
{
    baseUrl_ = url.isEmpty() ? "http://127.0.0.1:5056" : url;
    if (client_)
        client_->setBaseUrl(baseUrl_.toStdString());
}

void AceForgeBridgeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampleRate_ = sampleRate;
}

void AceForgeBridgeAudioProcessor::releaseResources() {}

void AceForgeBridgeAudioProcessor::startGeneration(const juce::String& prompt, int durationSeconds)
{
    if (state_.load() == State::Submitting || state_.load() == State::Queued ||
        state_.load() == State::Running)
        return;
    state_.store(State::Submitting);
    triggerAsyncUpdate();
    std::thread t(&AceForgeBridgeAudioProcessor::runGenerationThread, this, prompt, durationSeconds);
    t.detach();
}

void AceForgeBridgeAudioProcessor::runGenerationThread(juce::String prompt, int durationSec)
{
    if (!client_)
    {
        state_.store(State::Failed);
        {
            juce::ScopedLock l(statusLock_);
            lastError_ = "No client";
        }
        triggerAsyncUpdate();
        return;
    }

    client_->setBaseUrl(baseUrl_.toStdString());
    if (!client_->healthCheck())
    {
        state_.store(State::Failed);
        connected_.store(false);
        {
            juce::ScopedLock l(statusLock_);
            lastError_ = "Cannot reach AceForge at " + baseUrl_ + " – is it running?";
            statusText_ = lastError_;
        }
        triggerAsyncUpdate();
        return;
    }
    connected_.store(true);

    aceforge::GenerateParams params;
    params.songDescription = prompt.toStdString();
    params.durationSeconds = durationSec <= 0 ? 10 : durationSec;
    params.instrumental = true;
    params.lyrics = "[inst]";
    params.taskType = "text2music";
    params.title = "aceforge_bridge_export";

    std::string jobId = client_->startGeneration(params);
    if (jobId.empty())
    {
        state_.store(State::Failed);
        juce::ScopedLock l(statusLock_);
        lastError_ = juce::String(client_->lastError());
        statusText_ = lastError_;
        triggerAsyncUpdate();
        return;
    }

    state_.store(State::Queued);
    triggerAsyncUpdate();

    while (true)
    {
        aceforge::JobStatus st = client_->getStatus(jobId);
        if (st.status == "succeeded")
        {
            state_.store(State::Running);
            triggerAsyncUpdate();
            if (st.audioUrl.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = "No audio URL in result";
                triggerAsyncUpdate();
                return;
            }
            std::vector<uint8_t> wavBytes = client_->fetchAudio(st.audioUrl);
            if (wavBytes.empty())
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = juce::String(client_->lastError());
                triggerAsyncUpdate();
                return;
            }
            juce::AudioFormatManager fm;
            fm.registerFormat(new juce::WavAudioFormat(), true);
            auto mis = std::make_unique<juce::MemoryInputStream>(wavBytes.data(), static_cast<size_t>(wavBytes.size()), false);
            std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(std::move(mis)));
            if (!reader)
            {
                state_.store(State::Failed);
                juce::ScopedLock l(statusLock_);
                lastError_ = "Failed to decode WAV";
                triggerAsyncUpdate();
                return;
            }
            const double fileSampleRate = reader->sampleRate;
            const int numCh = static_cast<int>(reader->numChannels);
            const int numSamples = static_cast<int>(reader->lengthInSamples);
            juce::AudioBuffer<float> fileBuffer(numCh, numSamples);
            reader->read(&fileBuffer, 0, numSamples, 0, true, true);
            std::vector<float> interleaved(static_cast<size_t>(numSamples) * 2u);
            for (int i = 0; i < numSamples; ++i)
            {
                interleaved[static_cast<size_t>(i) * 2u] = numCh > 0 ? fileBuffer.getSample(0, i) : 0.0f;
                interleaved[static_cast<size_t>(i) * 2u + 1u] = numCh > 1 ? fileBuffer.getSample(1, i) : interleaved[static_cast<size_t>(i) * 2u];
            }
            pushSamplesToPlayback(interleaved.data(), numSamples, 2, fileSampleRate);
            playbackBufferReady_.store(true);
            state_.store(State::Succeeded);
            {
                juce::ScopedLock l(statusLock_);
                statusText_ = "Generated " + juce::String(durationSec) + "s – playing.";
            }
            triggerAsyncUpdate();
            return;
        }
        if (st.status == "failed")
        {
            state_.store(State::Failed);
            juce::ScopedLock l(statusLock_);
            lastError_ = juce::String::fromUTF8(st.error.c_str());
            statusText_ = lastError_;
            triggerAsyncUpdate();
            return;
        }
        state_.store(st.status == "running" ? State::Running : State::Queued);
        {
            juce::ScopedLock l(statusLock_);
            statusText_ = juce::String(stateToString(state_.load()));
            if (st.queuePosition > 0)
                statusText_ += " (queue: " + juce::String(st.queuePosition) + ")";
        }
        triggerAsyncUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void AceForgeBridgeAudioProcessor::pushSamplesToPlayback(const float* interleaved, int numFrames,
                                                         int sourceChannels, double sourceSampleRate)
{
    if (numFrames <= 0 || interleaved == nullptr)
        return;
    const double ratio = sourceSampleRate > 0.0 ? sampleRate_ / sourceSampleRate : 1.0;
    const int outFrames = static_cast<int>(std::round(numFrames * ratio));
    if (outFrames <= 0)
        return;

    int start1, block1, start2, block2;
    playbackFifo_.prepareToWrite(outFrames, start1, block1, start2, block2);

    auto writeFrames = [&](int start, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const double srcIdx = (double)i / ratio;
            const int i0 = std::min(static_cast<int>(srcIdx), numFrames - 1);
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
            const size_t base = static_cast<size_t>(start + i) * 2u;
            if (base + 1 < playbackBuffer_.size())
            {
                playbackBuffer_[base] = l;
                playbackBuffer_[base + 1] = r;
            }
        }
    };
    writeFrames(start1, block1);
    writeFrames(start2, block2);
    playbackFifo_.finishedWrite(block1 + block2);
}

void AceForgeBridgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh < 2)
    {
        buffer.clear();
        return;
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

juce::String AceForgeBridgeAudioProcessor::getStatusText() const
{
    juce::ScopedLock l(statusLock_);
    return statusText_;
}

juce::String AceForgeBridgeAudioProcessor::getLastError() const
{
    juce::ScopedLock l(statusLock_);
    return lastError_;
}

void AceForgeBridgeAudioProcessor::handleAsyncUpdate()
{
    // Notify listeners / editor can poll; optional repaint trigger via listener
}

const juce::String AceForgeBridgeAudioProcessor::getName() const { return JucePlugin_Name; }
bool AceForgeBridgeAudioProcessor::acceptsMidi() const { return false; }
bool AceForgeBridgeAudioProcessor::producesMidi() const { return false; }
bool AceForgeBridgeAudioProcessor::isMidiEffect() const { return false; }
double AceForgeBridgeAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int AceForgeBridgeAudioProcessor::getNumPrograms() { return 1; }
int AceForgeBridgeAudioProcessor::getCurrentProgram() { return 0; }
void AceForgeBridgeAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AceForgeBridgeAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}
void AceForgeBridgeAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

bool AceForgeBridgeAudioProcessor::isBusesLayoutSupported(const juce::AudioProcessor::BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

bool AceForgeBridgeAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* AceForgeBridgeAudioProcessor::createEditor()
{
    return new AceForgeBridgeAudioProcessorEditor(*this);
}
void AceForgeBridgeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}
void AceForgeBridgeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AceForgeBridgeAudioProcessor();
}
