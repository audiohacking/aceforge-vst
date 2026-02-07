#pragma once

#include <functional>
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
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;

    void setOnRowDoubleClicked(std::function<void(int)> f) { onRowDoubleClicked_ = std::move(f); }

private:
    AceForgeBridgeAudioProcessor& processor;
    std::function<void(int)> onRowDoubleClicked_;
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

    void showLibraryFeedback();  // called by LibraryListBox on double-click

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
    juce::TextButton refreshLibraryButton;
    LibraryListModel libraryListModel;
    LibraryListBox libraryList;
    juce::TextButton insertIntoDawButton;
    juce::TextButton revealInFinderButton;
    juce::Label libraryHintLabel;

    void updateStatusFromProcessor();
    void startGeneration();
    void refreshLibraryList();
    void insertSelectedIntoDaw();
    void revealSelectedInFinder();

    juce::String libraryFeedbackMessage_;
    int libraryFeedbackCountdown_{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeBridgeAudioProcessorEditor)
};
