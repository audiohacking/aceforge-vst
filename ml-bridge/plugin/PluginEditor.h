#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class AceForgeBridgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AceForgeBridgeAudioProcessorEditor(AceForgeBridgeAudioProcessor& p);
    ~AceForgeBridgeAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AceForgeBridgeAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessorEditor)
};
