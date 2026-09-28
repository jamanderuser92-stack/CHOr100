#include "PluginEditor.h"

static juce::Font paintFont (float size) { return juce::Font (juce::FontOptions ("Segoe UI", size, juce::Font::plain)); }

void PaintLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float start, float end, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 3.0f;
    const auto c = b.getCentre();
    const float a = start + pos * (end - start);

    g.setColour (juce::Colours::white);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 4.0f);
    g.drawLine (c.x, c.y, c.x + (r - 6) * std::sin (a), c.y - (r - 6) * std::cos (a), 4.0f);
}

void PaintLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                   const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colours::white);
    g.fillRect (body);
    g.setColour (juce::Colours::black);
    g.drawRect (body, 2.0f);
}

juce::Font PaintLookAndFeel::getSliderPopupFont (juce::Slider&) { return paintFont (13.0f); }

// Koordinaten direkt aus der Skizze uebernommen
SuperDirtEditor::SuperDirtEditor (SuperDirtProcessor& p) : AudioProcessorEditor (&p)
{
    for (auto* s : { &dirt, &super, &output })
    {
        s->setLookAndFeel (&lnf);
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setPopupDisplayEnabled (true, true, this);
        s->setColour (juce::Slider::textBoxTextColourId, juce::Colours::black);
        addAndMakeVisible (s);
    }
    aDirt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "dirt", dirt);
    aSuper = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "super", super);
    aOut   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "out", output);
    output.setDoubleClickReturnValue (true, 0.0);
    setSize (760, 340);
}

SuperDirtEditor::~SuperDirtEditor()
{
    for (auto* s : { &dirt, &super, &output }) s->setLookAndFeel (nullptr);
}

void SuperDirtEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
    g.setColour (juce::Colours::black);
    g.drawRect (juce::Rectangle<float> (30.0f, 30.0f, 693.0f, 280.0f), 4.0f);

    g.setFont (paintFont (13.0f));
    g.drawSingleLineText ("salux super dirt", 47, 56);
    g.drawSingleLineText ("DIRT", 113, 204);
    g.drawSingleLineText ("SUPER", 210, 212);
    g.drawSingleLineText ("OUPTUTO", 390, 168);
}

void SuperDirtEditor::resized()
{
    auto knob = [] (juce::Slider& s, int cx, int cy, int r)
    {
        s.setBounds (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2);
    };
    knob (dirt, 105, 246, 35);
    knob (super, 216, 248, 37);
    knob (output, 435, 238, 43);
}
