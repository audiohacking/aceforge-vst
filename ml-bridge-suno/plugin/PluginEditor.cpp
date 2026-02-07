#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- LibraryListModelSuno ---
int LibraryListModelSuno::getNumRows()
{
    return static_cast<int>(processor.getLibraryEntries().size());
}

void LibraryListModelSuno::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
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

void LibraryListModelSuno::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (onRowDoubleClicked_)
        onRowDoubleClicked_(row);
}

// --- LibraryListBoxSuno ---
LibraryListBoxSuno::LibraryListBoxSuno(AceForgeSunoAudioProcessor& p, LibraryListModelSuno& model)
    : ListBox("Library", &model), processorRef(p)
{
    setRowHeight(28);
    setOutlineThickness(0);
}

void LibraryListBoxSuno::mouseDrag(const juce::MouseEvent& e)
{
    if (dragStarted_)
    {
        ListBox::mouseDrag(e);
        return;
    }
    if (e.getDistanceFromDragStart() < 10)
    {
        ListBox::mouseDrag(e);
        return;
    }
    int row = getRowContainingPosition(e.x, e.y);
    auto entries = processorRef.getLibraryEntries();
    if (row < 0 || row >= static_cast<int>(entries.size()))
    {
        ListBox::mouseDrag(e);
        return;
    }
    juce::String path = entries[static_cast<size_t>(row)].file.getFullPathName();
    if (path.isEmpty())
    {
        ListBox::mouseDrag(e);
        return;
    }
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    auto* editorComp = findParentComponentOfClass<AceForgeSunoAudioProcessorEditor>();
    juce::Component* sourceComp = editorComp != nullptr ? static_cast<juce::Component*>(editorComp) : static_cast<juce::Component*>(this);
    if (container && container->performExternalDragDropOfFiles(juce::StringArray(path), false, sourceComp))
        dragStarted_ = true;
    else
        ListBox::mouseDrag(e);
}

void LibraryListBoxSuno::mouseUp(const juce::MouseEvent& e)
{
    dragStarted_ = false;
    ListBox::mouseUp(e);
}

// --- Editor ---
AceForgeSunoAudioProcessorEditor::AceForgeSunoAudioProcessorEditor(AceForgeSunoAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), libraryListModel(p), libraryList(p, libraryListModel)
{
    setSize(520, 620);

    apiKeyLabel.setText("Suno API Key:", juce::dontSendNotification);
    apiKeyLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(apiKeyLabel);

    apiKeyEditor.setPasswordCharacter('*');
    apiKeyEditor.setMultiLine(false);
    apiKeyEditor.setTextToShowWhenEmpty("Paste your Suno API key", juce::Colours::grey);
    addAndMakeVisible(apiKeyEditor);

    saveApiKeyButton.setButtonText("Save");
    saveApiKeyButton.onClick = [this] { saveApiKey(); };
    addAndMakeVisible(saveApiKeyButton);

    connectionLabel.setText("Set API key and save.", juce::dontSendNotification);
    connectionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(connectionLabel);

    bpmLabel.setText("BPM: —", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(bpmLabel);

    recordButton.setButtonText("Record");
    recordButton.onClick = [this] { processorRef.setRecording(recordButton.getToggleState()); };
    addAndMakeVisible(recordButton);

    promptLabel.setText("Prompt:", juce::dontSendNotification);
    promptLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(promptLabel);
    promptEditor.setMultiLine(false);
    promptEditor.setTextToShowWhenEmpty("Describe the music or style", juce::Colours::grey);
    addAndMakeVisible(promptEditor);

    styleLabel.setText("Style:", juce::dontSendNotification);
    styleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(styleLabel);
    styleEditor.setMultiLine(false);
    styleEditor.setTextToShowWhenEmpty("e.g. pop, rock, electronic", juce::Colours::grey);
    addAndMakeVisible(styleEditor);

    titleLabel.setText("Title:", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    titleEditor.setMultiLine(false);
    titleEditor.setTextToShowWhenEmpty("Track title (optional)", juce::Colours::grey);
    addAndMakeVisible(titleEditor);

    modelLabel.setText("Model:", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(modelLabel);
    modelCombo.addItem("V4", 1);
    modelCombo.addItem("V4.5", 2);
    modelCombo.addItem("V4.5 Plus", 3);
    modelCombo.addItem("V4.5 All", 4);
    modelCombo.addItem("V5", 5);
    modelCombo.setSelectedId(4, juce::dontSendNotification);
    addAndMakeVisible(modelCombo);

    instrumentalToggle.setButtonText("Instrumental");
    instrumentalToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(instrumentalToggle);

    generateButton.setButtonText("Generate");
    generateButton.onClick = [this]
    {
        processorRef.startGenerate(
            promptEditor.getText(), styleEditor.getText(), titleEditor.getText(),
            false, instrumentalToggle.getToggleState(), modelCombo.getSelectedId() - 1);
    };
    addAndMakeVisible(generateButton);

    coverButton.setButtonText("Cover (from recorded)");
    coverButton.onClick = [this]
    {
        processorRef.startUploadCover(
            promptEditor.getText(), styleEditor.getText(), titleEditor.getText(),
            false, instrumentalToggle.getToggleState(), modelCombo.getSelectedId() - 1);
    };
    addAndMakeVisible(coverButton);

    addVocalsButton.setButtonText("Add Vocals (from recorded)");
    addVocalsButton.onClick = [this]
    {
        processorRef.startAddVocals(promptEditor.getText(), styleEditor.getText(), titleEditor.getText());
    };
    addAndMakeVisible(addVocalsButton);

    statusLabel.setText("Idle.", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(statusLabel);

    libraryLabel.setText("Library", juce::dontSendNotification);
    libraryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(libraryLabel);
    refreshLibraryButton.setButtonText("Refresh");
    refreshLibraryButton.onClick = [this] { refreshLibraryList(); };
    addAndMakeVisible(refreshLibraryButton);
    addAndMakeVisible(libraryList);
    insertIntoDawButton.setButtonText("Insert into DAW");
    insertIntoDawButton.onClick = [this] { insertSelectedIntoDaw(); };
    addAndMakeVisible(insertIntoDawButton);
    revealInFinderButton.setButtonText("Reveal in Finder");
    revealInFinderButton.onClick = [this] { revealSelectedInFinder(); };
    addAndMakeVisible(revealInFinderButton);
    libraryHintLabel.setText("Drag a row to timeline, or double-click to copy path. Insert into DAW opens in Logic.", juce::dontSendNotification);
    libraryHintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    libraryHintLabel.setFont(juce::Font(juce::FontOptions().withPointHeight(10.0f)));
    addAndMakeVisible(libraryHintLabel);

    libraryListModel.setOnRowDoubleClicked([this](int row)
    {
        auto entries = processorRef.getLibraryEntries();
        if (row < 0 || row >= static_cast<int>(entries.size()))
            return;
        juce::SystemClipboard::copyTextToClipboard(entries[static_cast<size_t>(row)].file.getFullPathName());
        showLibraryFeedback();
    });

    // Restore API key from processor (already loaded from state)
    apiKeyEditor.setText(processorRef.getApiKey(), juce::dontSendNotification);
    startTimerHz(4);
}

AceForgeSunoAudioProcessorEditor::~AceForgeSunoAudioProcessorEditor()
{
    stopTimer();
}

void AceForgeSunoAudioProcessorEditor::saveApiKey()
{
    processorRef.setApiKey(apiKeyEditor.getText());
}

void AceForgeSunoAudioProcessorEditor::timerCallback()
{
    if (libraryFeedbackCountdown_ > 0)
    {
        statusLabel.setText(libraryFeedbackMessage_, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        --libraryFeedbackCountdown_;
    }
    else
    {
        updateStatusFromProcessor();
    }
    double bpm = processorRef.getHostBpm();
    if (bpm > 0.0)
        bpmLabel.setText("BPM: " + juce::String(bpm, 1), juce::dontSendNotification);
    else
        bpmLabel.setText("BPM: —", juce::dontSendNotification);
    libraryList.updateContent();
}

void AceForgeSunoAudioProcessorEditor::updateStatusFromProcessor()
{
    const auto state = processorRef.getState();
    if (state == AceForgeSunoAudioProcessor::State::Succeeded)
        refreshLibraryList();
    if (processorRef.isConnected())
        connectionLabel.setText("Suno: connected", juce::dontSendNotification);
    else if (state == AceForgeSunoAudioProcessor::State::Failed)
        connectionLabel.setText("Suno: error (see status)", juce::dontSendNotification);
    else
        connectionLabel.setText("Suno: set API key and save", juce::dontSendNotification);

    statusLabel.setText(processorRef.getStatusText(), juce::dontSendNotification);
    if (state == AceForgeSunoAudioProcessor::State::Failed)
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::salmon);
    else
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    const bool busy = (state == AceForgeSunoAudioProcessor::State::Submitting ||
                      state == AceForgeSunoAudioProcessor::State::Running);
    generateButton.setEnabled(!busy);
    coverButton.setEnabled(!busy && processorRef.hasRecordedAudio());
    addVocalsButton.setEnabled(!busy && processorRef.hasRecordedAudio());
    recordButton.setEnabled(!busy);
}

void AceForgeSunoAudioProcessorEditor::refreshLibraryList()
{
    libraryList.updateContent();
    libraryList.repaint();
}

void AceForgeSunoAudioProcessorEditor::insertSelectedIntoDaw()
{
    const int row = libraryList.getSelectedRow();
    auto entries = processorRef.getLibraryEntries();
    if (row < 0 || row >= static_cast<int>(entries.size()))
    {
        libraryFeedbackMessage_ = "Select a library entry first.";
        libraryFeedbackCountdown_ = 8;
        return;
    }
    const juce::File& file = entries[static_cast<size_t>(row)].file;
    if (!file.existsAsFile())
    {
        libraryFeedbackMessage_ = "File not found.";
        libraryFeedbackCountdown_ = 8;
        return;
    }
    juce::String path = file.getFullPathName();
    juce::SystemClipboard::copyTextToClipboard(path);
#if JUCE_MAC
    juce::ChildProcess proc;
    juce::StringArray args;
    args.add("open");
    args.add("-a");
    args.add("Logic Pro");
    args.add(path);
    if (proc.start(args, 0))
        libraryFeedbackMessage_ = "Opened in Logic Pro. Drag into your project or use Reveal in Finder.";
    else
        libraryFeedbackMessage_ = "Path copied. Use Reveal in Finder and drag the file.";
#else
    libraryFeedbackMessage_ = "Path copied to clipboard.";
#endif
    libraryFeedbackCountdown_ = 14;
}

void AceForgeSunoAudioProcessorEditor::revealSelectedInFinder()
{
    const int row = libraryList.getSelectedRow();
    auto entries = processorRef.getLibraryEntries();
    if (row < 0 || row >= static_cast<int>(entries.size()))
    {
        libraryFeedbackMessage_ = "Select a library entry first.";
        libraryFeedbackCountdown_ = 8;
        return;
    }
    const juce::File& f = entries[static_cast<size_t>(row)].file;
    if (f.existsAsFile())
        f.revealToUser();
    else
    {
        libraryFeedbackMessage_ = "File not found.";
        libraryFeedbackCountdown_ = 8;
    }
}

void AceForgeSunoAudioProcessorEditor::showLibraryFeedback()
{
    libraryFeedbackMessage_ = "Path copied. Insert into DAW or Reveal in Finder.";
    libraryFeedbackCountdown_ = 12;
}

void AceForgeSunoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    auto r = getLocalBounds().reduced(12);
    g.drawText("AceForge-Suno", r.getX(), r.getY(), 220, 26, juce::Justification::left);
}

void AceForgeSunoAudioProcessorEditor::resized()
{
    const int pad = 12;
    auto r = getLocalBounds().reduced(pad);
    r.removeFromTop(26);

    auto row = r.removeFromTop(22);
    apiKeyLabel.setBounds(row.getX(), row.getY(), 80, 20);
    apiKeyEditor.setBounds(row.getX() + 84, row.getY(), r.getWidth() - 84 - 56, 20);
    saveApiKeyButton.setBounds(row.getX() + r.getWidth() - 54, row.getY(), 50, 20);
    r.removeFromTop(4);
    connectionLabel.setBounds(r.getX(), r.getY(), 200, 20);
    bpmLabel.setBounds(r.getX() + 220, r.getY(), 80, 20);
    r.removeFromTop(22);
    r.removeFromTop(6);

    recordButton.setBounds(r.getX(), r.getY(), 80, 22);
    r.removeFromTop(26);
    r.removeFromTop(4);

    promptLabel.setBounds(r.getX(), r.getY(), 50, 20);
    promptEditor.setBounds(r.getX() + 52, r.getY(), r.getWidth() - 52, 20);
    r.removeFromTop(24);
    styleLabel.setBounds(r.getX(), r.getY(), 50, 20);
    styleEditor.setBounds(r.getX() + 52, r.getY(), r.getWidth() - 52, 20);
    r.removeFromTop(24);
    titleLabel.setBounds(r.getX(), r.getY(), 50, 20);
    titleEditor.setBounds(r.getX() + 52, r.getY(), r.getWidth() - 52, 20);
    r.removeFromTop(24);

    row = r.removeFromTop(24);
    modelLabel.setBounds(row.getX(), row.getY(), 44, 20);
    modelCombo.setBounds(row.getX() + 46, row.getY(), 90, 20);
    instrumentalToggle.setBounds(row.getX() + 144, row.getY(), 110, 20);
    generateButton.setBounds(row.getX() + 258, row.getY(), 72, 20);
    coverButton.setBounds(row.getX() + 334, row.getY(), 90, 20);
    addVocalsButton.setBounds(row.getX() + 428, row.getY(), 84, 20);
    r.removeFromTop(8);

    statusLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 40);
    r.removeFromTop(40);

    auto libHeader = r.removeFromTop(22);
    libraryLabel.setBounds(libHeader.getX(), libHeader.getY(), 60, 22);
    refreshLibraryButton.setBounds(libHeader.getX() + 64, libHeader.getY(), 60, 22);
    r.removeFromTop(4);
    libraryList.setBounds(r.getX(), r.getY(), r.getWidth(), 140);
    r.removeFromTop(140);
    r.removeFromTop(4);
    row = r.removeFromTop(24);
    insertIntoDawButton.setBounds(row.getX(), row.getY(), 120, 22);
    revealInFinderButton.setBounds(row.getX() + 124, row.getY(), 110, 22);
    r.removeFromTop(4);
    libraryHintLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 36);
}
