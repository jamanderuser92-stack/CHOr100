#include "PluginEditor.h"

namespace
{
    const juce::Colour bg (0xff15171c), panel (0xff1e2129), accent (0xffe8b04a), text (0xffe6e6e6);
    const char* ids[]    = { "voices", "detune", "timing", "vibrato", "spread", "tone", "width", "room", "mix", "out" };
    const char* names[]  = { "Stimmen", "Verstimmung", "Timing", "Vibrato", "Klangfarbe", "Tonlage", "Breite", "Raum", "Mix", "Ausgang" };
}

Chor100Editor::Chor100Editor (Chor100Processor& p) : AudioProcessorEditor (&p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, accent);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3f4b));
    lnf.setColour (juce::Slider::thumbColourId, accent);
    lnf.setColour (juce::Slider::textBoxTextColourId, text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Label::textColourId, text);
    setLookAndFeel (&lnf);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        addAndMakeVisible (k.slider);
        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, ids[i], k.slider);
    }
    setSize (620, 360);
}

Chor100Editor::~Chor100Editor() { setLookAndFeel (nullptr); }

void Chor100Editor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().reduced (10).withTrimmedTop (50).toFloat(), 10.0f);
    g.setColour (accent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("CHOR100", 20, 12, 200, 32, juce::Justification::centredLeft);
    g.setColour (text.withAlpha (0.6f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Eine Stimme rein, ein Chor raus", 200, 12, 400, 32, juce::Justification::centredRight);
}

void Chor100Editor::resized()
{
    auto area = getLocalBounds().reduced (20).withTrimmedTop (50);
    const int rowH = area.getHeight() / 2;
    const int colW = area.getWidth() / 5;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int row = (int) i / 5, col = (int) i % 5;
        auto cell = juce::Rectangle<int> (area.getX() + col * colW, area.getY() + row * rowH, colW, rowH).reduced (4);
        knobs[i].label.setBounds (cell.removeFromTop (20));
        knobs[i].slider.setBounds (cell);
    }
}
