/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct ChainSettings
{
    float peakFreq{ 800.f }, peakGainInDecibels{ 4.f }, peakQuality{ 0.3f };
    float lowShelfFreq{ 295.f }, lowShelfDecibels{ 0.f }, lowShelfQuality{ 0.6f };
    float volumeDecibels{0.f};
    bool midboost { true };
    
    };

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class SEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SEQAudioProcessor();
    ~SEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    juce::AudioProcessorValueTreeState apvts{*this, nullptr, "Parameters", createParameterLayout()};

    
private:
    
    enum
    {
        LowshelfIndex,
        PeakIndex,
        OutputIndex
    };
    
    using Filter = juce::dsp::IIR::Filter<float>;
    using OutGain = juce::dsp::Gain<float>;
    
    using MonoChain = juce::dsp::ProcessorChain<Filter, Filter, OutGain>;
    
    MonoChain leftChain, rightChain;
    
    void updateFilters();
    
 
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SEQAudioProcessor)
};
