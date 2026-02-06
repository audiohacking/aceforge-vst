#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- LibraryListModel ---
int LibraryListModel::getNumRows()
{
    return static_cast<int>(processor.getLibraryEntries().size());
}

void LibraryListModel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto entries = processor.getLibraryEntries();
    if (rowNumber < 0 || rowNumber >= static_cast<int>(entries.size()))
        return;
    const auto& e = entries[static_cast<size_t>(rowNumber)];
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff2a2a4e));
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(e.file.getFileName(), 6, 0, width - 12, height, juce::Justification::centredLeft);
    g.setColour(juce::Colours::lightgrey);
    g.setFont(11.0f);
    g.drawText(e.time.formatted("%Y-%m-%d %H:%M"), 6, 0, width - 12, height, juce::Justification::centredRight);
}

// --- LibraryListBox ---
LibraryListBox::LibraryListBox(AceForgeBridgeAudioProcessor& p, LibraryListModel& model)
    : ListBox("Library", &model), processorRef(p)
{
    setRowHeight(28);
    setOutlineThickness(0);
}

void LibraryListBox::mouseDrag(const juce::MouseEvent& e)
{
    ListBox::mouseDrag(e);
    if (dragStarted_)
        return;
    if (e.getDistanceFromDragStart() < 8)
        return;
    int row = getRowContainingPosition(e.x, e.y);
    auto entries = processorRef.getLibraryEntries();
    if (row < 0 || row >= static_cast<int>(entries.size()))
        return;
    juce::String path = entries[static_cast<size_t>(row)].file.getFullPathName();
    if (path.isEmpty())
        return;
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (container && container->performExternalDragDropOfFiles(juce::StringArray(path), false, this))
        dragStarted_ = true;
}

void LibraryListBox::mouseUp(const juce::MouseEvent& e)
{
    dragStarted_ = false;
    ListBox::mouseUp(e);
}

// --- Editor ---
AceForgeBridgeAudioProcessorEditor::AceForgeBridgeAudioProcessorEditor(
    AceForgeBridgeAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), libraryListModel(p), libraryList(p, libraryListModel)
{
    setSize(460, 420);

    connectionLabel.setText("Checking...", juce::dontSendNotification);
    connectionLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    connectionLabel.setJustificationType(juce::Justification::left);
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

    qualityLabel.setText("Quality:", juce::dontSendNotification);
    qualityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(qualityLabel);

    qualityCombo.addItem("Fast (15 steps)", 15);
    qualityCombo.addItem("High (55 steps)", 55);
    qualityCombo.setSelectedId(15, juce::dontSendNotification);
    addAndMakeVisible(qualityCombo);

    generateButton.setButtonText("Generate");
    generateButton.onClick = [this] { startGeneration(); };
    addAndMakeVisible(generateButton);

    statusLabel.setText("Idle - enter a prompt and click Generate.", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::topLeft);
    statusLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(statusLabel);

    libraryLabel.setText("Library (drag into DAW):", juce::dontSendNotification);
    libraryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(libraryLabel);
    addAndMakeVisible(libraryList);

    startTimerHz(4);
}

AceForgeBridgeAudioProcessorEditor::~AceForgeBridgeAudioProcessorEditor()
{
    stopTimer();
}

void AceForgeBridgeAudioProcessorEditor::timerCallback()
{
    updateStatusFromProcessor();
    libraryList.updateContent();
}

void AceForgeBridgeAudioProcessorEditor::updateStatusFromProcessor()
{
    const auto state = processorRef.getState();
    if (processorRef.isConnected())
        connectionLabel.setText("AceForge: connected", juce::dontSendNotification);
    else if (state == AceForgeBridgeAudioProcessor::State::Failed)
        connectionLabel.setText("AceForge: error (see status)", juce::dontSendNotification);
    else
        connectionLabel.setText("AceForge: not connected - start AceForge?", juce::dontSendNotification);

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
    const int steps = qualityCombo.getSelectedId();
    processorRef.startGeneration(promptEditor.getText(), durationSec > 0 ? durationSec : 10, steps > 0 ? steps : 15);
}

void AceForgeBridgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    auto r = getLocalBounds().reduced(12);
    g.drawText("AceForge-Bridge", r.getX(), r.getY(), 220, 26, juce::Justification::left);
}

void AceForgeBridgeAudioProcessorEditor::resized()
{
    const int pad = 12;
    auto r = getLocalBounds().reduced(pad);
    r.removeFromTop(26);

    connectionLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 22);
    r.removeFromTop(22);
    r.removeFromTop(6);

    auto row = r.removeFromTop(24);
    promptEditor.setBounds(row.getX(), row.getY(), row.getWidth(), 24);
    r.removeFromTop(6);

    row = r.removeFromTop(24);
    durationLabel.setBounds(row.getX(), row.getY(), 80, 22);
    durationCombo.setBounds(row.getX() + 82, row.getY(), 56, 22);
    qualityLabel.setBounds(row.getX() + 146, row.getY(), 52, 22);
    qualityCombo.setBounds(row.getX() + 200, row.getY(), 120, 22);
    generateButton.setBounds(row.getX() + 324, row.getY(), 100, 22);
    r.removeFromTop(8);

    statusLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 44);
    r.removeFromTop(44);

    libraryLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 20);
    r.removeFromTop(20);
    libraryList.setBounds(r.getX(), r.getY(), r.getWidth(), r.getHeight());
}
