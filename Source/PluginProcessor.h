#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChoirEngine.h"

class Chor100Processor : public juce::AudioProcessor
{
public:
    Chor100Processor();

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    choir::ChoirEngine engine;
    juce::Reverb reverb;
    static constexpr int kChunk = 1024;
    std::vector<float> mono, wetL, wetR;
    std::vector<float> dryL, dryR;
    int dryPos = 0;
    juce::SmoothedValue<float> mixS, gainS;

    std::atomic<float> *pVoices, *pDetune, *pTiming, *pVibrato, *pSpread, *pTone, *pWidth, *pRoom, *pMix, *pOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Chor100Processor)
};
