#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "AceForgeClient/AceForgeClient.hpp"
#include <atomic>
#include <memory>

class AceForgeBridgeAudioProcessor : public juce::AudioProcessor,
                                     public juce::AsyncUpdater
{
public:
    enum class State
    {
        Idle,
        Submitting,
        Queued,
        Running,
        Succeeded,
        Failed
    };

    AceForgeBridgeAudioProcessor();
    ~AceForgeBridgeAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    
    bool isBusesLayoutSupported(const juce::AudioProcessor::BusesLayout& layouts) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void handleAsyncUpdate() override;

    // Generation (call from UI or elsewhere; runs on background thread)
    void startGeneration(const juce::String& prompt, int durationSeconds = 10, int inferenceSteps = 15);
    void setBaseUrl(const juce::String& url);

    State getState() const { return state_.load(); }
    juce::String getStatusText() const;
    juce::String getLastError() const;
    bool isConnected() const { return connected_; }

private:
    void runGenerationThread(juce::String prompt, int durationSec, int inferenceSteps);
    void pushSamplesToPlayback(const float* interleaved, int numFrames, int sourceChannels, double sourceSampleRate);

    std::unique_ptr<aceforge::AceForgeClient> client_;
    juce::String baseUrl_;
    std::atomic<State> state_{ State::Idle };
    std::atomic<bool> connected_{ false };
    juce::CriticalSection statusLock_;
    juce::String lastError_;
    juce::String statusText_;

    static constexpr int kPlaybackFifoFrames = 1 << 20; // ~10s at 44.1k stereo
    juce::AbstractFifo playbackFifo_{ kPlaybackFifoFrames };
    std::vector<float> playbackBuffer_;
    std::atomic<bool> playbackBufferReady_{ false };

    std::atomic<double> sampleRate_{ 44100.0 };

    // Pending WAV bytes from background thread; decoded on message thread (JUCE not thread-safe)
    juce::CriticalSection pendingWavLock_;
    std::vector<uint8_t> pendingWavBytes_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessor)
};
