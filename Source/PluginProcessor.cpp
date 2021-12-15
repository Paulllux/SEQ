/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SEQAudioProcessor::SEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SEQAudioProcessor::~SEQAudioProcessor()
{
}

//==============================================================================
const juce::String SEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void SEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    juce::dsp::ProcessSpec spec;
    
    spec.maximumBlockSize = samplesPerBlock;
    
    spec.numChannels = 1;
    
    spec.sampleRate = sampleRate;
    
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    
    
    updateFilters();

}

void SEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (//layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif
void SEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    
    
    updateFilters();
    

    juce::dsp::AudioBlock<float> block(buffer);
    
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);
    
}

//==============================================================================
bool SEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SEQAudioProcessor::createEditor()
{
//    return new SEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
    
}

//==============================================================================
void SEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
    
}

void SEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data,
                                              static_cast<size_t>(sizeInBytes));
    if( tree.isValid() )
    {
        apvts.replaceState(tree);
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    

    settings.lowShelfDecibels = apvts.getRawParameterValue("Bass")->load();
    settings.volumeDecibels = apvts.getRawParameterValue("Volume")->load();
    settings.midboost = apvts.getRawParameterValue("Midboost")->load() > 0.5f;
    
    return settings;
}

void SEQAudioProcessor::updateFilters()
{
    
    auto chainSettings = getChainSettings(apvts);
    
    //light midboost set with fixed values
    
    auto peakCoeffients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),chainSettings.peakFreq,chainSettings.peakQuality,juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    
    leftChain.setBypassed<PeakIndex>(!chainSettings.midboost);
    rightChain.setBypassed<PeakIndex>(!chainSettings.midboost);
    
    *leftChain.get<PeakIndex>().coefficients = *peakCoeffients;
    *rightChain.get<PeakIndex>().coefficients = *peakCoeffients;
    
    //lowshelf filter
    
    
    auto lowShelfCoefficients =
    juce::dsp::IIR::Coefficients<float>::makeLowShelf(getSampleRate(), chainSettings.lowShelfFreq, chainSettings.lowShelfQuality, juce::Decibels::decibelsToGain(chainSettings.lowShelfDecibels));
    

    *leftChain.get<LowshelfIndex>().coefficients = *lowShelfCoefficients;
    *rightChain.get<LowshelfIndex>().coefficients = *lowShelfCoefficients;
    
    //volume
    
    auto& outputgainLeft= leftChain.template get<OutputIndex>();
    auto& outputgainRight= rightChain.template get<OutputIndex>();
    
    outputgainLeft.setGainDecibels(chainSettings.volumeDecibels);
    outputgainRight.setGainDecibels(chainSettings.volumeDecibels);
}


juce::AudioProcessorValueTreeState::ParameterLayout SEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    

    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Bass", "Bass", juce::NormalisableRange<float>(-24.f, 0.f, 0.5f, 1.f), -18.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Volume", "Volume", juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), 0.f));
    
    layout.add(std::make_unique<juce::AudioParameterBool>("Midboost", "Midboost", true));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SEQAudioProcessor();
}
