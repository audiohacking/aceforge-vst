#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "SunoClient/SunoClient.hpp"
#include <atomic>
#include <memory>
#include <vector>

class AceForgeSunoAudioProcessor : public juce::AudioProcessor,
                                   public juce::AsyncUpdater
{
public:
    enum class State
    {
        Idle,
        Submitting,
        Running,
        Succeeded,
        Failed
    };

    AceForgeSunoAudioProcessor();
    ~AceForgeSunoAudioProcessor() override;

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

    // API key (persisted in state)
    void setApiKey(const juce::String& key);
    juce::String getApiKey() const { return apiKey_; }
    bool hasValidApiKey() const { return client_ && client_->hasApiKey(); }

    // Recording: when true, processBlock appends input to recordBuffer_
    void setRecording(bool on) { recording_.store(on); }
    bool isRecording() const { return recording_.load(); }
    void clearRecordedBuffer();
    bool hasRecordedAudio() const;
    int getRecordedSamples() const;

    // Generation modes
    void startGenerate(const juce::String& prompt, const juce::String& style, const juce::String& title,
                      bool customMode, bool instrumental, int modelIndex);
    void startUploadCover(const juce::String& prompt, const juce::String& style, const juce::String& title,
                          bool customMode, bool instrumental, int modelIndex);
    void startAddVocals(const juce::String& prompt, const juce::String& style, const juce::String& title);

    State getState() const { return state_.load(); }
    juce::String getStatusText() const;
    juce::String getLastError() const;
    bool isConnected() const { return connected_.load(); }

    // Host tempo when available
    double getHostBpm() const { return hostBpm_.load(); }

    // Library (saved generations on disk)
    struct LibraryEntry { juce::File file; juce::String prompt; juce::Time time; };
    juce::File getLibraryDirectory() const;
    std::vector<LibraryEntry> getLibraryEntries() const;

private:
    void runGenerateThread();
    void runUploadCoverThread();
    void runAddVocalsThread();
    void pushSamplesToPlayback(const float* interleaved, int numFrames, int sourceChannels, double sourceSampleRate);
    std::vector<uint8_t> encodeRecordedAsWav() const;

    std::unique_ptr<suno::SunoClient> client_;
    juce::String apiKey_;
    std::atomic<State> state_{ State::Idle };
    std::atomic<bool> connected_{ false };
    std::atomic<bool> recording_{ false };
    juce::CriticalSection statusLock_;
    juce::String lastError_;
    juce::String statusText_;
    std::atomic<double> hostBpm_{ 0.0 };

    // Recorded audio (audio thread appends under lock; worker copies out for WAV)
    std::vector<float> recordBuffer_;
    juce::CriticalSection recordLock_;

    // Playback (same double-buffer pattern as AceForge plugin)
    static constexpr int kPlaybackFifoFrames = 1 << 20;
    juce::AbstractFifo playbackFifo_{ kPlaybackFifoFrames };
    std::vector<float> playbackBuffer_;
    std::vector<float> pendingPlaybackBuffer_[2];
    std::atomic<int> pendingPlaybackFrames_{ 0 };
    std::atomic<int> pendingPlaybackBufferIndex_{ 0 };
    std::atomic<int> nextWriteIndex_{ 0 };
    std::atomic<bool> pendingPlaybackReady_{ false };
    std::atomic<double> sampleRate_{ 44100.0 };

    juce::CriticalSection pendingWavLock_;
    std::vector<uint8_t> pendingWavBytes_;
    juce::String pendingPrompt_;

    // Params for current job (set before starting thread)
    juce::String jobPrompt_;
    juce::String jobStyle_;
    juce::String jobTitle_;
    bool jobCustomMode_{ false };
    bool jobInstrumental_{ true };
    int jobModelIndex_{ 3 };  // V4_5ALL
    bool jobIsCover_{ false };
    bool jobIsAddVocals_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeSunoAudioProcessor)
};
