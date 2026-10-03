#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Tabla de elementos. Kick, bajo, clap, hats y synths salen del checklist;
// percusion, pads y vocal son puntos de partida razonables para el estilo.
const std::array<ElementPreset, 8>& getElementPresets()
{
    //                     nombre          obj    min    max    HPF   barroHz dB    Q    presHz presdB aireHz aire dB mono   duck  prof
    static const std::array<ElementPreset, 8> presets {{
        { "Kick",          -10.0f, -11.0f,  -9.0f,  28.0f, 320.0f, -3.0f, 1.2f, 3000.0f,  1.5f, 12000.0f, 0.0f,  true,  false, 0.0f },
        { "Bajo / Sub",    -13.0f, -14.0f, -12.0f,  30.0f, 250.0f, -2.0f, 1.0f, 1200.0f,  0.0f, 10000.0f, 0.0f,  true,  true,  8.0f },
        { "Clap / Snare",  -17.0f, -18.0f, -16.0f, 150.0f, 400.0f, -3.0f, 1.0f, 3000.0f,  1.5f, 10000.0f, 1.0f,  false, false, 0.0f },
        { "Hats",          -22.0f, -24.0f, -20.0f, 400.0f, 500.0f,  0.0f, 1.0f, 7000.0f, -1.5f, 12000.0f, 1.0f,  false, false, 0.0f },
        { "Percusion",     -20.0f, -22.0f, -18.0f, 200.0f, 350.0f, -2.0f, 1.0f, 3000.0f,  0.0f, 11000.0f, 1.0f,  false, false, 0.0f },
        { "Synths / Arps", -18.0f, -20.0f, -16.0f, 150.0f, 300.0f, -3.0f, 1.0f, 2500.0f,  1.0f, 12000.0f, 1.0f,  false, true,  4.0f },
        { "Pads / Atmos",  -22.0f, -24.0f, -20.0f, 200.0f, 300.0f, -3.0f, 0.8f, 2500.0f,  0.0f, 12000.0f, 1.5f,  false, true,  5.0f },
        { "Vocal / Chops", -15.0f, -17.0f, -13.0f, 100.0f, 300.0f, -2.0f, 1.0f, 3500.0f,  2.0f, 12000.0f, 2.0f,  false, false, 0.0f },
    }};
    return presets;
}

//==============================================================================
namespace
{
    juce::String fmtHz (float v, int)  { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz"
                                                             : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String fmtAmt (float v, int) { return juce::String (v, 1) + " dB"; }
    juce::String fmtMs (float v, int)  { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String fmtQ  (float v, int)  { return juce::String (v, 2); }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    bool changed (float a, float b) { return std::abs (a - b) > 1.0e-6f; }

    void atomicMax (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MixRefProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray names;
    for (auto& e : getElementPresets()) names.add (e.name);

    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction (fmtHz);
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction (fmtDb);
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction (fmtMs);
    auto amt = AudioParameterFloatAttributes().withStringFromValueFunction (fmtAmt);
    auto q  = AudioParameterFloatAttributes().withStringFromValueFunction (fmtQ);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "element", 1 }, "Elemento", names, 0));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "trim", 1 }, "Ganancia", NormalisableRange<float> (-30.0f, 24.0f, 0.1f), 0.0f, db));

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "eqOn", 1 }, "EQ activo", true));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "hpf", 1 }, "HPF", skewed (20.0f, 1000.0f, 150.0f), 28.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "mudFreq", 1 }, "Barro Hz", skewed (100.0f, 800.0f, 300.0f), 320.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "mudGain", 1 }, "Barro dB", NormalisableRange<float> (-12.0f, 6.0f, 0.1f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "mudQ", 1 }, "Barro Q", skewed (0.3f, 4.0f, 1.0f), 1.0f, q));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "presFreq", 1 }, "Presencia Hz", skewed (1000.0f, 8000.0f, 3000.0f), 3000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "presGain", 1 }, "Presencia dB", NormalisableRange<float> (-9.0f, 9.0f, 0.1f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "airFreq", 1 }, "Aire Hz", skewed (6000.0f, 16000.0f, 10000.0f), 12000.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "airGain", 1 }, "Aire dB", NormalisableRange<float> (-9.0f, 9.0f, 0.1f), 0.0f, db));

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "monoOn", 1 }, "Graves mono", false));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "monoFreq", 1 }, "Mono hasta", skewed (60.0f, 250.0f, 120.0f), 120.0f, hz));

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "duckOn", 1 }, "Ducker activo", false));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "duckDepth", 1 }, "Profundidad", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, amt));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "duckThresh", 1 }, "Umbral", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -30.0f, db));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "duckAttack", 1 }, "Ataque", skewed (0.1f, 50.0f, 5.0f), 2.0f, ms));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "duckRelease", 1 }, "Release", skewed (20.0f, 800.0f, 150.0f), 150.0f, ms));

    return { p.begin(), p.end() };
}

//==============================================================================
MixRefProcessor::MixRefProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MixRefState", createLayout())
{
    pElement    = apvts.getRawParameterValue ("element");
    pTrim       = apvts.getRawParameterValue ("trim");
    pEqOn       = apvts.getRawParameterValue ("eqOn");
    pHpf        = apvts.getRawParameterValue ("hpf");
    pMudF       = apvts.getRawParameterValue ("mudFreq");
    pMudG       = apvts.getRawParameterValue ("mudGain");
    pMudQ       = apvts.getRawParameterValue ("mudQ");
    pPresF      = apvts.getRawParameterValue ("presFreq");
    pPresG      = apvts.getRawParameterValue ("presGain");
    pAirF       = apvts.getRawParameterValue ("airFreq");
    pAirG       = apvts.getRawParameterValue ("airGain");
    pMonoOn     = apvts.getRawParameterValue ("monoOn");
    pMonoF      = apvts.getRawParameterValue ("monoFreq");
    pDuckOn     = apvts.getRawParameterValue ("duckOn");
    pDuckDepth  = apvts.getRawParameterValue ("duckDepth");
    pDuckThresh = apvts.getRawParameterValue ("duckThresh");
    pDuckAtt    = apvts.getRawParameterValue ("duckAttack");
    pDuckRel    = apvts.getRawParameterValue ("duckRelease");
}

bool MixRefProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != out)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void MixRefProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec mono   { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    juce::dsp::ProcessSpec stereo { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    updateFilters (true);
    for (size_t ch = 0; ch < 2; ++ch)
        for (auto* f : { &hpf1[ch], &hpf2[ch], &mud[ch], &pres[ch], &air[ch] })
        {
            f->prepare (mono);
            f->reset();
        }

    lowSplit.setType  (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highSplit.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lowSplit.prepare (stereo);
    highSplit.prepare (stereo);
    lowSplit.reset();
    highSplit.reset();

    trimGain.reset (sampleRate, 0.03);
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    duckEnv = 0.0f;
}

void MixRefProcessor::updateFilters (bool force)
{
    using Coefs = juce::dsp::IIR::Coefficients<float>;
    const float nyq = (float) sr * 0.45f;

    const float hpf = juce::jmin (pHpf->load(), nyq);
    if (force || changed (hpf, cHpf))
    {
        cHpf = hpf;
        // 24 dB/oct Butterworth = dos biquads en cascada
        auto a = Coefs::makeHighPass (sr, hpf, 0.5412f);
        auto b = Coefs::makeHighPass (sr, hpf, 1.3066f);
        for (size_t ch = 0; ch < 2; ++ch) { hpf1[ch].coefficients = a; hpf2[ch].coefficients = b; }
    }

    const float mf = juce::jmin (pMudF->load(), nyq), mg = pMudG->load(), mq = pMudQ->load();
    if (force || changed (mf, cMudF) || changed (mg, cMudG) || changed (mq, cMudQ))
    {
        cMudF = mf; cMudG = mg; cMudQ = mq;
        auto c = Coefs::makePeakFilter (sr, mf, mq, juce::Decibels::decibelsToGain (mg));
        for (auto& f : mud) f.coefficients = c;
    }

    const float pf = juce::jmin (pPresF->load(), nyq), pg = pPresG->load();
    if (force || changed (pf, cPresF) || changed (pg, cPresG))
    {
        cPresF = pf; cPresG = pg;
        auto c = Coefs::makePeakFilter (sr, pf, 0.8f, juce::Decibels::decibelsToGain (pg));
        for (auto& f : pres) f.coefficients = c;
    }

    const float af = juce::jmin (pAirF->load(), nyq), ag = pAirG->load();
    if (force || changed (af, cAirF) || changed (ag, cAirG))
    {
        cAirF = af; cAirG = ag;
        auto c = Coefs::makeHighShelf (sr, af, 0.707f, juce::Decibels::decibelsToGain (ag));
        for (auto& f : air) f.coefficients = c;
    }

    const float monoF = pMonoF->load();
    if (force || changed (monoF, cMonoF))
    {
        cMonoF = monoF;
        lowSplit.setCutoffFrequency (monoF);
        highSplit.setCutoffFrequency (monoF);
    }
}

void MixRefProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (main.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;

    // Sidechain (entrada auxiliar)
    const float* sc[2] = { nullptr, nullptr };
    int numSc = 0;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            numSc = juce::jmin (scBuf.getNumChannels(), 2);
            for (int c = 0; c < numSc; ++c) sc[c] = scBuf.getReadPointer (c);
        }

    updateFilters (false);

    const bool eqOn   = pEqOn->load()   > 0.5f;
    const bool monoOn = pMonoOn->load() > 0.5f && numCh == 2;
    const bool duckOn = pDuckOn->load() > 0.5f && numSc > 0;
    const float depth  = pDuckDepth->load();
    const float thresh = pDuckThresh->load();
    const float attC = std::exp (-1.0f / (0.001f * pDuckAtt->load() * (float) sr));
    const float relC = std::exp (-1.0f / (0.001f * pDuckRel->load() * (float) sr));

    trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));

    float* data[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };
    float peak = 0.0f, maxGR = 0.0f, scPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float g = trimGain.getNextValue();
        float x[2] = { 0.0f, 0.0f };

        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto c = (size_t) ch;
            float s = data[c][i] * g;
            if (eqOn)
            {
                s = hpf1[c].processSample (s);
                s = hpf2[c].processSample (s);
                s = mud[c].processSample (s);
                s = pres[c].processSample (s);
                s = air[c].processSample (s);
            }
            x[c] = s;
        }

        if (monoOn)
        {
            const float lowL  = lowSplit.processSample (0, x[0]);
            const float lowR  = lowSplit.processSample (1, x[1]);
            const float highL = highSplit.processSample (0, x[0]);
            const float highR = highSplit.processSample (1, x[1]);
            const float lowM  = 0.5f * (lowL + lowR);
            x[0] = lowM + highL;
            x[1] = lowM + highR;
        }

        // Detector del sidechain
        float det = 0.0f;
        for (int c = 0; c < numSc; ++c) det = juce::jmax (det, std::abs (sc[c][i]));
        scPeak = juce::jmax (scPeak, det);
        duckEnv = det > duckEnv ? attC * duckEnv + (1.0f - attC) * det
                                : relC * duckEnv + (1.0f - relC) * det;

        if (duckOn)
        {
            const float envDb = juce::Decibels::gainToDecibels (duckEnv, -100.0f);
            const float amount = juce::jlimit (0.0f, 1.0f, (envDb - thresh) / 6.0f); // rodilla de 6 dB
            const float gr = depth * amount;
            maxGR = juce::jmax (maxGR, gr);
            const float dg = juce::Decibels::decibelsToGain (-gr);
            for (int ch = 0; ch < numCh; ++ch) x[ch] *= dg;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            data[ch][i] = x[ch];
            peak = juce::jmax (peak, std::abs (x[ch]));
        }
    }

    atomicMax (meterPeak, peak);
    atomicMax (meterGR, maxGR);
    atomicMax (meterSC, scPeak);
}

//==============================================================================
const ElementPreset& MixRefProcessor::currentPreset() const
{
    const auto& all = getElementPresets();
    return all[(size_t) juce::jlimit (0, (int) all.size() - 1, juce::roundToInt (pElement->load()))];
}

void MixRefProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void MixRefProcessor::loadElementPreset()
{
    const auto& e = currentPreset();
    setParam ("eqOn", 1.0f);
    setParam ("hpf", e.hpf);
    setParam ("mudFreq", e.mudFreq);
    setParam ("mudGain", e.mudGain);
    setParam ("mudQ", e.mudQ);
    setParam ("presFreq", e.presFreq);
    setParam ("presGain", e.presGain);
    setParam ("airFreq", e.airFreq);
    setParam ("airGain", e.airGain);
    setParam ("monoOn", e.mono ? 1.0f : 0.0f);
    setParam ("monoFreq", 120.0f);
    setParam ("duckOn", e.duck ? 1.0f : 0.0f);
    if (e.duck) setParam ("duckDepth", e.duckDepth);
}

void MixRefProcessor::matchTrimToTarget (float measuredPeakDb)
{
    if (measuredPeakDb < -80.0f) return; // sin senal, no tocar
    const float newTrim = juce::jlimit (-30.0f, 24.0f, pTrim->load() + (currentPreset().targetDb - measuredPeakDb));
    setParam ("trim", newTrim);
}

//==============================================================================
void MixRefProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MixRefProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MixRefProcessor::createEditor() { return new MixRefEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixRefProcessor(); }
