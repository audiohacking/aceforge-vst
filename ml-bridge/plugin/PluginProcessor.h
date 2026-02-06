#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "AceForgeClient/AceForgeClient.hpp"
#include <atomic>
#include <memory>
#include <vector>

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

    // Library of saved generations (on disk) for drag-into-DAW
    struct LibraryEntry
    {
        juce::File file;
        juce::String prompt;
        juce::Time time;
    };
    juce::File getLibraryDirectory() const;
    std::vector<LibraryEntry> getLibraryEntries() const;
    void addToLibrary(const juce::File& wavFile, const juce::String& prompt);

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

    // Double-buffer handoff: message thread writes to one buffer, audio thread reads from the other. Avoids race where message thread resize() invalidates pointer audio thread is reading.
    std::vector<float> pendingPlaybackBuffer_[2];
    std::atomic<int> pendingPlaybackFrames_{ 0 };
    std::atomic<int> pendingPlaybackBufferIndex_{ 0 }; // which buffer has new data (0 or 1)
    std::atomic<int> nextWriteIndex_{ 0 };           // which buffer message thread will write next
    std::atomic<bool> pendingPlaybackReady_{ false };

    std::atomic<double> sampleRate_{ 44100.0 };

    // Pending WAV bytes from background thread; decoded on message thread (JUCE not thread-safe)
    juce::CriticalSection pendingWavLock_;
    std::vector<uint8_t> pendingWavBytes_;
    juce::String pendingPrompt_;
    int pendingDurationSec_{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessor)
};
