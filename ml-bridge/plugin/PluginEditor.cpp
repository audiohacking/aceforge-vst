#include "PluginProcessor.h"
#include "PluginEditor.h"

AceForgeBridgeAudioProcessorEditor::AceForgeBridgeAudioProcessorEditor(
    AceForgeBridgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(420, 320);

    connectionLabel.setText("Connection: checking…", juce::dontSendNotification);
    connectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(connectionLabel);

    promptEditor.setMultiLine(false);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setText("upbeat electronic beat, 10s");
    promptEditor.setTextToShowWhenEmpty("Describe the music (e.g. calm piano, 10s)", juce::Colours::grey);
    addAndMakeVisible(promptEditor);

    durationLabel.setText("Duration (s):", juce::dontSendNotification);
    durationLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(durationLabel);

    durationCombo.addItem("10", 10);
    durationCombo.addItem("15", 15);
    durationCombo.addItem("20", 20);
    durationCombo.addItem("30", 30);
    durationCombo.setSelectedId(10, juce::dontSendNotification);
    addAndMakeVisible(durationCombo);

    generateButton.setButtonText("Generate");
    generateButton.onClick = [this] { startGeneration(); };
    addAndMakeVisible(generateButton);

    statusLabel.setText("Idle – enter a prompt and click Generate.", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    startTimerHz(4);
}

AceForgeBridgeAudioProcessorEditor::~AceForgeBridgeAudioProcessorEditor()
{
    stopTimer();
}

void AceForgeBridgeAudioProcessorEditor::timerCallback()
{
    updateStatusFromProcessor();
}

void AceForgeBridgeAudioProcessorEditor::updateStatusFromProcessor()
{
    const auto state = processorRef.getState();
    if (processorRef.isConnected())
        connectionLabel.setText("AceForge: connected", juce::dontSendNotification);
    else if (state == AceForgeBridgeAudioProcessor::State::Failed)
        connectionLabel.setText("AceForge: error (see status)", juce::dontSendNotification);
    else
        connectionLabel.setText("AceForge: not connected – start AceForge?", juce::dontSendNotification);

    statusLabel.setText(processorRef.getStatusText(), juce::dontSendNotification);
    if (state == AceForgeBridgeAudioProcessor::State::Failed)
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::salmon);
    else
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    const bool busy = (state == AceForgeBridgeAudioProcessor::State::Submitting ||
                      state == AceForgeBridgeAudioProcessor::State::Queued ||
                      state == AceForgeBridgeAudioProcessor::State::Running);
    generateButton.setEnabled(!busy);
}

void AceForgeBridgeAudioProcessorEditor::startGeneration()
{
    const int durationSec = durationCombo.getSelectedId();
    processorRef.startGeneration(promptEditor.getText(), durationSec > 0 ? durationSec : 10);
}

void AceForgeBridgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("AceForge Bridge", 10, 8, 200, 22, juce::Justification::left);
}

void AceForgeBridgeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    connectionLabel.setBounds(r.removeFromTop(22));
    r.removeFromTop(8);

    auto row = r.removeFromTop(24);
    promptEditor.setBounds(row.removeFromLeft(r.getWidth() - 8));
    r.removeFromTop(6);

    row = r.removeFromTop(24);
    durationLabel.setBounds(row.removeFromLeft(90));
    durationCombo.setBounds(row.removeFromLeft(80));
    generateButton.setBounds(row.removeFromLeft(100).reduced(2));
    r.removeFromTop(8);

    statusLabel.setBounds(r.removeFromTop(60));
}
