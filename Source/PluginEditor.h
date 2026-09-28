#pragma once
#include "PluginProcessor.h"

// Look wie in MS Paint gemalt: weiss, schwarze dicke Linien, Standardschrift
class PaintLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&,
                     const juce::Rectangle<float>& body) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
};

class SuperDirtEditor : public juce::AudioProcessorEditor
{
public:
    explicit SuperDirtEditor (SuperDirtProcessor&);
    ~SuperDirtEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PaintLookAndFeel lnf;
    juce::Slider dirt, super, output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aDirt, aSuper, aOut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperDirtEditor)
};
