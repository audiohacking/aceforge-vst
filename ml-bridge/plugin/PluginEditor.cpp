#include "PluginProcessor.h"
#include "PluginEditor.h"

AceForgeBridgeAudioProcessorEditor::AceForgeBridgeAudioProcessorEditor(
    AceForgeBridgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(400, 300);
}

AceForgeBridgeAudioProcessorEditor::~AceForgeBridgeAudioProcessorEditor() {}

void AceForgeBridgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("AceForge Bridge – connect to AceForge API", getLocalBounds(),
                    juce::Justification::centred, 2);
}

void AceForgeBridgeAudioProcessorEditor::resized() {}
