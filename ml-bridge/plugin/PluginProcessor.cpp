#include "PluginProcessor.h"
#include "PluginEditor.h"

AceForgeBridgeAudioProcessor::AceForgeBridgeAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
}

AceForgeBridgeAudioProcessor::~AceForgeBridgeAudioProcessor() {}

const juce::String AceForgeBridgeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AceForgeBridgeAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AceForgeBridgeAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AceForgeBridgeAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AceForgeBridgeAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AceForgeBridgeAudioProcessor::getNumPrograms() { return 1; }
int AceForgeBridgeAudioProcessor::getCurrentProgram() { return 0; }
void AceForgeBridgeAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AceForgeBridgeAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}
void AceForgeBridgeAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void AceForgeBridgeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void AceForgeBridgeAudioProcessor::releaseResources() {}

bool AceForgeBridgeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    return true;
#endif
}

void AceForgeBridgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());
    // Synth: output only; clear or fill from AceForge playback buffer (future)
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());
}

bool AceForgeBridgeAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* AceForgeBridgeAudioProcessor::createEditor()
{
    return new AceForgeBridgeAudioProcessorEditor(*this);
}

void AceForgeBridgeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void AceForgeBridgeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AceForgeBridgeAudioProcessor();
}
