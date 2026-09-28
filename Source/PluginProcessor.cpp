#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioParameterFloatAttributes unit (const juce::String& u, int decimals = 0)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [u, decimals] (float v, int) { return juce::String (v, decimals) + " " + u; });
}

juce::AudioProcessorValueTreeState::ParameterLayout Chor100Processor::createLayout()
{
    using P = juce::AudioParameterFloat;
    using R = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "voices", 1 }, "Stimmen", 1, 100, 60));
    l.add (std::make_unique<P> (juce::ParameterID { "detune", 1 },  "Verstimmung", R (0.f, 50.f, 0.1f), 14.f, unit ("ct")));
    l.add (std::make_unique<P> (juce::ParameterID { "timing", 1 },  "Timing",      R (0.f, 80.f, 0.1f), 25.f, unit ("ms")));
    l.add (std::make_unique<P> (juce::ParameterID { "vibrato", 1 }, "Vibrato",     R (0.f, 100.f, 1.f), 30.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "spread", 1 },  "Klangfarbe",  R (0.f, 100.f, 1.f), 60.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "tone", 1 },    "Tonlage",     R (-100.f, 100.f, 1.f), 0.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "width", 1 },   "Breite",      R (0.f, 100.f, 1.f), 80.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "room", 1 },    "Raum",        R (0.f, 100.f, 1.f), 35.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "mix", 1 },     "Mix",         R (0.f, 100.f, 1.f), 100.f, unit ("%")));
    l.add (std::make_unique<P> (juce::ParameterID { "out", 1 },     "Ausgang",     R (-24.f, 12.f, 0.1f), 0.f, unit ("dB", 1)));
    return l;
}

Chor100Processor::Chor100Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pVoices = apvts.getRawParameterValue ("voices");
    pDetune = apvts.getRawParameterValue ("detune");
    pTiming = apvts.getRawParameterValue ("timing");
    pVibrato = apvts.getRawParameterValue ("vibrato");
    pSpread = apvts.getRawParameterValue ("spread");
    pTone = apvts.getRawParameterValue ("tone");
    pWidth = apvts.getRawParameterValue ("width");
    pRoom = apvts.getRawParameterValue ("room");
    pMix = apvts.getRawParameterValue ("mix");
    pOut = apvts.getRawParameterValue ("out");
}

bool Chor100Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void Chor100Processor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    const int lat = engine.getLatency();
    setLatencySamples (lat);

    mono.assign (kChunk, 0.f); wetL.assign (kChunk, 0.f); wetR.assign (kChunk, 0.f);
    dryL.assign ((size_t) lat, 0.f); dryR.assign ((size_t) lat, 0.f); dryPos = 0;

    reverb.setSampleRate (sampleRate);
    reverb.reset();
    mixS.reset (sampleRate, 0.05); mixS.setCurrentAndTargetValue (pMix->load() * 0.01f);
    gainS.reset (sampleRate, 0.05); gainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
}

void Chor100Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int total = buffer.getNumSamples();
    if (numIn == 0 || buffer.getNumChannels() < 2) { buffer.clear(); return; }

    choir::Params p;
    p.voices = (int) pVoices->load();
    p.detune = pDetune->load();
    p.timingMs = pTiming->load();
    p.vibrato = pVibrato->load() * 0.01f;
    p.spread = pSpread->load() * 0.01f;
    p.tone = pTone->load() * 0.01f;
    p.width = pWidth->load() * 0.01f;

    const float room = pRoom->load() * 0.01f;
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.5f + 0.45f * room;
    rp.damping = 0.45f;
    rp.wetLevel = 0.55f * room;
    rp.dryLevel = 1.0f - 0.35f * room;
    rp.width = 1.0f;
    reverb.setParameters (rp);

    mixS.setTargetValue (pMix->load() * 0.01f);
    gainS.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    float* chL = buffer.getWritePointer (0);
    float* chR = buffer.getWritePointer (1);
    const bool stereoIn = numIn > 1;
    const int lat = (int) dryL.size();

    for (int start = 0; start < total; start += kChunk)
    {
        const int n = std::min (kChunk, total - start);
        float* l = chL + start;
        float* r = chR + start;

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = stereoIn ? 0.5f * (l[i] + r[i]) : l[i];

        engine.process (mono.data(), wetL.data(), wetR.data(), n, p);
        reverb.processStereo (wetL.data(), wetR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i];
            const float inR = stereoIn ? r[i] : l[i];
            float dl = inL, dr = inR;
            if (lat > 0)
            {
                dl = dryL[(size_t) dryPos]; dr = dryR[(size_t) dryPos];
                dryL[(size_t) dryPos] = inL; dryR[(size_t) dryPos] = inR;
                if (++dryPos >= lat) dryPos = 0;
            }
            const float m = mixS.getNextValue();
            const float g = gainS.getNextValue();
            l[i] = (dl * (1.0f - m) + wetL[(size_t) i] * m) * g;
            r[i] = (dr * (1.0f - m) + wetR[(size_t) i] * m) * g;
        }
    }
}

juce::AudioProcessorEditor* Chor100Processor::createEditor() { return new Chor100Editor (*this); }

void Chor100Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void Chor100Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Chor100Processor(); }
