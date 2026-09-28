#pragma once
#include "PluginProcessor.h"

class Chor100Editor : public juce::AudioProcessorEditor
{
public:
    explicit Chor100Editor (Chor100Processor&);
    ~Chor100Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    std::array<Knob, 10> knobs;
    juce::LookAndFeel_V4 lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Chor100Editor)
};
