#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class AceForgeBridgeAudioProcessorEditor;

// ListBox model for library entries (current and previous generations)
class LibraryListModel : public juce::ListBoxModel
{
public:
    explicit LibraryListModel(AceForgeBridgeAudioProcessor& p) : processor(p) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

private:
    AceForgeBridgeAudioProcessor& processor;
};

// ListBox that starts external file drag when user drags a row (for drag-into-DAW)
class LibraryListBox : public juce::ListBox
{
public:
    LibraryListBox(AceForgeBridgeAudioProcessor& p, LibraryListModel& model);
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    AceForgeBridgeAudioProcessor& processorRef;
    bool dragStarted_{ false };
};

class AceForgeBridgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           public juce::DragAndDropContainer,
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
    juce::Label qualityLabel;
    juce::ComboBox qualityCombo;
    juce::TextButton generateButton;
    juce::Label statusLabel;
    juce::Label libraryLabel;
    LibraryListModel libraryListModel;
    LibraryListBox libraryList;

    void updateStatusFromProcessor();
    void startGeneration();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessorEditor)
};
