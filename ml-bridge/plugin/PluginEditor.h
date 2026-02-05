#pragma once

#include <JuceHeader>
#include "PluginProcessor.h"

class AceForgeBridgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::Timer
{
public:
    explicit AceForgeBridgeAudioProcessorEditor(AceForgeBridgeAudioProcessor& p);
    ~AceForgeBridgeAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    AceForgeBridgeAudioProcessor& processorRef;

    juce::Label connectionLabel;
    juce::TextEditor promptEditor;
    juce::Label durationLabel;
    juce::ComboBox durationCombo;
    juce::TextButton generateButton;
    juce::Label statusLabel;

    void updateStatusFromProcessor();
    void startGeneration();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessorEditor)
};
