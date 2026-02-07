#pragma once

#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class AceForgeSunoAudioProcessorEditor;

class LibraryListModelSuno : public juce::ListBoxModel
{
public:
    explicit LibraryListModelSuno(AceForgeSunoAudioProcessor& p) : processor(p) {}
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void setOnRowDoubleClicked(std::function<void(int)> f) { onRowDoubleClicked_ = std::move(f); }

private:
    AceForgeSunoAudioProcessor& processor;
    std::function<void(int)> onRowDoubleClicked_;
};

class LibraryListBoxSuno : public juce::ListBox
{
public:
    LibraryListBoxSuno(AceForgeSunoAudioProcessor& p, LibraryListModelSuno& model);
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    AceForgeSunoAudioProcessor& processorRef;
    bool dragStarted_{ false };
};

class AceForgeSunoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::DragAndDropContainer,
                                         public juce::Timer
{
public:
    explicit AceForgeSunoAudioProcessorEditor(AceForgeSunoAudioProcessor& p);
    ~AceForgeSunoAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void showLibraryFeedback();

private:
    AceForgeSunoAudioProcessor& processorRef;

    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyEditor;
    juce::TextButton saveApiKeyButton;
    juce::Label connectionLabel;
    juce::Label bpmLabel;

    juce::ToggleButton recordButton;
    juce::Label promptLabel;
    juce::TextEditor promptEditor;
    juce::Label styleLabel;
    juce::TextEditor styleEditor;
    juce::Label titleLabel;
    juce::TextEditor titleEditor;
    juce::Label modelLabel;
    juce::ComboBox modelCombo;
    juce::ToggleButton instrumentalToggle;
    juce::TextButton generateButton;
    juce::TextButton coverButton;
    juce::TextButton addVocalsButton;

    juce::Label statusLabel;
    juce::Label libraryLabel;
    juce::TextButton refreshLibraryButton;
    LibraryListModelSuno libraryListModel;
    LibraryListBoxSuno libraryList;
    juce::TextButton insertIntoDawButton;
    juce::TextButton revealInFinderButton;
    juce::Label libraryHintLabel;

    juce::String libraryFeedbackMessage_;
    int libraryFeedbackCountdown_{ 0 };

    void updateStatusFromProcessor();
    void saveApiKey();
    void refreshLibraryList();
    void insertSelectedIntoDaw();
    void revealSelectedInFinder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AceForgeSunoAudioProcessorEditor)
};
