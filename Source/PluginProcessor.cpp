#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Tabla de elementos: rangos de la hoja "Niveles de mezcla" (progressive / melodic).
// El EQ de cada elemento es un punto de partida segun el tipo de sonido.
namespace
{
    struct EqTemplate { float hpf, mudF, mudG, mudQ, presF, presG, airF, airG; bool mono, duck; float depth; };

    //                         HPF   barroHz  dB    Q    presHz  dB    aireHz  dB   mono   duck   prof
    const EqTemplate tKick   {  28, 320, -3.0f, 1.2f, 3000,  1.5f, 12000,  0.0f, true,  false, 0 };
    const EqTemplate tBass   {  30, 250, -2.0f, 1.0f, 1200,  0.0f, 10000,  0.0f, true,  true,  8 };
    const EqTemplate tSub    {  25, 300, -3.0f, 1.0f, 1500,  0.0f,  8000, -3.0f, true,  true,  8 };
    const EqTemplate tMidB   {  40, 300, -2.0f, 1.0f, 1500,  1.0f, 10000,  0.0f, true,  true,  6 };
    const EqTemplate tReese  {  35, 300, -2.0f, 1.0f, 2000,  0.0f, 10000,  0.0f, true,  true,  6 };
    const EqTemplate tClap   { 150, 400, -3.0f, 1.0f, 3000,  1.5f, 10000,  1.0f, false, false, 0 };
    const EqTemplate tPerc   { 200, 350, -2.0f, 1.0f, 3000,  0.0f, 11000,  1.0f, false, false, 0 };
    const EqTemplate tHat    { 400, 500,  0.0f, 1.0f, 7000, -1.5f, 12000,  1.0f, false, false, 0 };
    const EqTemplate tRide   { 300, 500,  0.0f, 1.0f, 6000, -1.0f, 12000,  1.0f, false, false, 0 };
    const EqTemplate tTom    {  60, 400, -2.5f, 1.2f, 4000,  1.5f, 10000,  0.0f, false, false, 0 };
    const EqTemplate tLead   { 120, 300, -2.0f, 1.0f, 3000,  1.5f, 12000,  1.0f, false, true,  3 };
    const EqTemplate tSynth  { 150, 300, -3.0f, 1.0f, 2500,  1.0f, 12000,  1.0f, false, true,  4 };
    const EqTemplate tPad    { 200, 300, -3.0f, 0.8f, 2500,  0.0f, 12000,  1.5f, false, true,  5 };
    const EqTemplate tVocal  { 100, 300, -2.0f, 1.0f, 3500,  2.0f, 12000,  2.0f, false, false, 0 };
    const EqTemplate tFx     { 200, 400, -2.0f, 1.0f, 3000,  0.0f, 12000,  0.0f, false, true,  4 };
    const EqTemplate tImpact {  30, 300, -2.0f, 1.0f, 3000,  0.0f, 10000,  0.0f, true,  false, 0 };
    const EqTemplate tReturn { 250, 400, -2.0f, 1.0f, 3000,  0.0f, 10000, -1.0f, false, true,  4 };

    const char* gDrums = "Batería y percusión";
    const char* gBass  = "Bajos";
    const char* gMusic = "Elementos musicales";
    const char* gBack  = "Fondo, atmósferas y vocales";
    const char* gFx    = "FX, transiciones y detalles";

    ElementPreset make (const char* group, const char* name, float a, float b, const EqTemplate& t)
    {
        const float lo = std::min (a, b), hi = std::max (a, b);
        return { name, (lo + hi) * 0.5f, lo, hi, t.hpf, t.mudF, t.mudG, t.mudQ, t.presF, t.presG,
                 t.airF, t.airG, t.mono, t.duck, t.depth, group };
    }
}

const std::vector<ElementPreset>& getElementPresets()
{
    static const std::vector<ElementPreset> presets {
        // 0-8: mismos lugares que en la v1.2 (compatibilidad con sesiones guardadas)
        make (gDrums, "Kick",                  -11, -9,  tKick),   // la hoja dice -10: margen de 1 dB
        make (gBass,  "Bass principal",        -15, -14, tBass),
        make (gDrums, "Clap / Snare",          -18, -14, tClap),
        make (gDrums, "Closed hat",            -20, -16, tHat),
        make (gDrums, "Percusión principal",   -16, -12, tPerc),
        make (gMusic, "Lead / Synth principal",-19, -15, tLead),
        make (gBack,  "Pads",                  -22, -19, tPad),
        make (gBack,  "Vocal principal",       -18, -14, tVocal),
        make (gDrums, "Toms",                  -18, -14, tTom),
        // 9 en adelante: nuevos
        make (gDrums, "Percusión secundaria",  -20, -16, tPerc),
        make (gDrums, "Open hat",              -19, -15, tHat),
        make (gDrums, "Shakers",               -22, -18, tHat),
        make (gDrums, "Rides",                 -22, -18, tRide),
        make (gBass,  "Sub bass",              -18, -15, tSub),
        make (gBass,  "Bass grupo / Layer",    -20, -16, tBass),
        make (gBass,  "Reese bass",            -22, -18, tReese),
        make (gBass,  "Mid bass",              -20, -16, tMidB),
        make (gMusic, "Arp",                   -21, -17, tSynth),
        make (gMusic, "Pluck",                 -20, -16, tSynth),
        make (gMusic, "Chords / Acordes",      -22, -18, tSynth),
        make (gMusic, "Piano / Keys",          -22, -18, tSynth),
        make (gMusic, "Guitars",               -22, -18, tSynth),
        make (gBack,  "Strings",               -23, -18, tPad),
        make (gBack,  "Atmósferas",            -28, -22, tPad),
        make (gBack,  "Textures",              -30, -22, tPad),
        make (gBack,  "Vocal chops",           -22, -17, tVocal),
        make (gFx,    "Risers",                -24, -18, tFx),
        make (gFx,    "Impacts",               -20, -14, tImpact),
        make (gFx,    "Downsweeps",            -24, -18, tFx),
        make (gFx,    "Noise / Sweeps",        -26, -20, tFx),
        make (gFx,    "Ear candy",             -25, -18, tFx),
        make (gFx,    "Reverb returns",        -30, -20, tReturn),
        make (gFx,    "Delay returns",         -26, -18, tReturn),
    };
    return presets;
}

const std::vector<int>& getElementDisplayOrder()
{
    static const std::vector<int> order {
        0, 2, 4, 9, 3, 10, 11, 8, 12,      // bateria y percusion
        1, 13, 14, 15, 16,                 // bajos
        5, 17, 18, 19, 20, 21,             // elementos musicales
        6, 22, 23, 24, 7, 25,              // fondo, atmosferas y vocales
        26, 27, 28, 29, 30, 31, 32         // fx
    };
    return order;
}

//==============================================================================
juce::StringArray getEqTypeNames()
{
    return { "Corte graves 48", "Corte graves 12", "Shelf graves", "Campana",
             "Notch", "Shelf agudos", "Corte agudos 12", "Corte agudos 48" };
}

namespace
{
    juce::String fmtHz (float v, int)  { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz"
                                                             : juce::String (juce::roundToInt (v)) + " Hz"; }
    juce::String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
    juce::String fmtAmt (float v, int) { return juce::String (v, 1) + " dB"; }
    juce::String fmtMs (float v, int)  { return v < 10.0f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String fmtQ  (float v, int)  { return juce::String (v, 2); }
    juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    void atomicMax (std::atomic<float>& a, float v)
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }

    // Posiciones iniciales de las 8 bandas (como un EQ de 8 bandas vacio)
    constexpr std::array<int, 8>   defaultTypes { eq::LowCut48, eq::LowShelf, eq::Bell, eq::Bell, eq::Bell, eq::Bell, eq::HighShelf, eq::HighCut48 };
    constexpr std::array<float, 8> defaultFreqs { 30.0f, 100.0f, 250.0f, 600.0f, 1500.0f, 4000.0f, 10000.0f, 18000.0f };
}

juce::AudioProcessorValueTreeState::ParameterLayout MixRefProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    StringArray names;
    for (auto& e : getElementPresets()) names.add (utf8 (e.name));

    auto hz  = AudioParameterFloatAttributes().withStringFromValueFunction (fmtHz);
    auto db  = AudioParameterFloatAttributes().withStringFromValueFunction (fmtDb);
    auto amt = AudioParameterFloatAttributes().withStringFromValueFunction (fmtAmt);
    auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction (fmtMs);
    auto q   = AudioParameterFloatAttributes().withStringFromValueFunction (fmtQ);
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (fmtPct);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "element", 1 }, "Elemento", names, 0));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "trim", 1 }, "Ganancia", NormalisableRange<float> (-30.0f, 24.0f, 0.1f), 0.0f, db));

    // EQ general
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "eqOn", 1 }, "EQ activo", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "eqMode", 2 }, "Modo EQ", StringArray { utf8 ("Estéreo"), "L/R", "Mid/Side" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "eqEdit", 2 }, "Editar canal", StringArray { "A", "B" }, 0));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "adaptQ", 2 }, "Adaptive Q", true));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "hq", 2 }, "Alta calidad", false));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "scale", 2 }, "Escala", NormalisableRange<float> (-200.0f, 200.0f, 1.0f), 100.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "eqOut", 2 }, "Salida EQ", NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, db));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "audition", 2 }, utf8 ("Audición"), false));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "delta", 2 }, "Delta", false));

    // 8 bandas x 2 canales (A = estereo / L / Mid, B = R / Side)
    for (int set = 0; set < 2; ++set)
        for (int b = 0; b < eq::numBands; ++b)
        {
            const String n = "Banda " + String (b + 1) + (set == 0 ? " " : " (B) ");
            p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { bandId (set, b, "on"), 2 }, n + "activa", false));
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { bandId (set, b, "type"), 2 }, n + "tipo", getEqTypeNames(), defaultTypes[(size_t) b]));
            p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (set, b, "freq"), 2 }, n + "frecuencia", skewed (20.0f, 20000.0f, 1000.0f), defaultFreqs[(size_t) b], hz));
            p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (set, b, "gain"), 2 }, n + "ganancia", NormalisableRange<float> (-15.0f, 15.0f, 0.1f), 0.0f, db));
            p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { bandId (set, b, "q"), 2 }, n + "Q", skewed (0.1f, 18.0f, 1.0f), 0.71f, q));
        }

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
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    pElement = raw ("element");   pTrim = raw ("trim");
    pEqOn = raw ("eqOn");         pMode = raw ("eqMode");     pAdaptQ = raw ("adaptQ");   pHq = raw ("hq");
    pScale = raw ("scale");       pEqOut = raw ("eqOut");     pAudition = raw ("audition"); pDelta = raw ("delta");
    pMonoOn = raw ("monoOn");     pMonoF = raw ("monoFreq");
    pDuckOn = raw ("duckOn");     pDuckDepth = raw ("duckDepth"); pDuckThresh = raw ("duckThresh");
    pDuckAtt = raw ("duckAttack"); pDuckRel = raw ("duckRelease");

    for (int set = 0; set < 2; ++set)
        for (int b = 0; b < eq::numBands; ++b)
            bp[(size_t) set][(size_t) b] = { raw (bandId (set, b, "on")), raw (bandId (set, b, "type")), raw (bandId (set, b, "freq")),
                                             raw (bandId (set, b, "gain")), raw (bandId (set, b, "q")) };
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
    maxBlock = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec stereo { sampleRate, (juce::uint32) maxBlock, 2 };
    lowSplit.setType  (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highSplit.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lowSplit.prepare (stereo);
    highSplit.prepare (stereo);
    cMonoF = -1.0f;

    // Sobremuestreo x2 con filtros de fase lineal (latencia entera, para que Delta quede alineado)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) maxBlock);

    hqActive = pHq->load() > 0.5f;
    latency = hqActive ? juce::roundToInt (oversampler->getLatencyInSamples()) : 0;
    setLatencySamples (latency);

    for (auto& ch : bands) for (auto& b : ch) { b.type = -1; b.n = 0; for (auto& st : b.st) st.reset(); }
    for (auto& ch : auditionFilters) for (auto& f : ch) f.reset();
    lastAudition = -1;

    dryBuffer.setSize (2, maxBlock);
    for (auto& d : delayLine) d.assign (4096, 0.0f);
    delayPos = 0;

    trimGain.reset (sampleRate, 0.03);
    trimGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    eqOutGain.reset (sampleRate, 0.03);
    eqOutGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pEqOut->load()));
    duckEnv = 0.0f;
}

//==============================================================================
eq::BandSettings MixRefProcessor::readBand (int set, int band) const
{
    const auto& b = bp[(size_t) juce::jlimit (0, 1, set)][(size_t) juce::jlimit (0, eq::numBands - 1, band)];
    eq::BandSettings s;
    s.on   = b.on->load() > 0.5f;
    s.type = juce::roundToInt (b.type->load());
    s.freq = b.freq->load();
    s.gain = b.gain->load();
    s.q    = b.q->load();
    return s;
}

int MixRefProcessor::eqMode() const { return juce::jlimit (0, 2, juce::roundToInt (pMode->load())); }

double MixRefProcessor::eqSampleRate() const
{
    const double base = getSampleRate() > 0.0 ? getSampleRate() : 48000.0;
    return pHq->load() > 0.5f ? base * 2.0 : base;
}

void MixRefProcessor::updateEqCoefs (double eqSr, int mode)
{
    const bool adapt = pAdaptQ->load() > 0.5f;
    const double scale = pScale->load() / 100.0;

    for (int ch = 0; ch < 2; ++ch)
    {
        const int set = mode == 0 ? 0 : ch;
        for (int b = 0; b < eq::numBands; ++b)
        {
            const auto settings = readBand (set, b);
            const auto stages = eq::compute (settings, eqSr, adapt, scale);
            auto& rt = bands[(size_t) ch][(size_t) b];

            if (stages.n != rt.n || (settings.on && settings.type != rt.type))
                for (auto& st : rt.st) st.reset();

            rt.n = stages.n;
            rt.type = settings.on ? settings.type : -1;
            for (int i = 0; i < stages.n; ++i)
                rt.st[(size_t) i].c = stages.c[(size_t) i];
        }
    }
}

void MixRefProcessor::runEq (juce::dsp::AudioBlock<float>& block)
{
    const int numCh = (int) juce::jmin ((size_t) 2, block.getNumChannels());
    const int n = (int) block.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* x = block.getChannelPointer ((size_t) ch);
        for (auto& band : bands[(size_t) ch])
            for (int s = 0; s < band.n; ++s)
            {
                auto& f = band.st[(size_t) s];
                for (int i = 0; i < n; ++i) x[i] = f.process (x[i]);
            }
    }
}

static void encodeMS (float* const* d, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float l = d[0][i], r = d[1][i];
        d[0][i] = 0.5f * (l + r);
        d[1][i] = 0.5f * (l - r);
    }
}

static void decodeMS (float* const* d, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float m = d[0][i], s = d[1][i];
        d[0][i] = m + s;
        d[1][i] = m - s;
    }
}

void MixRefProcessor::processEqChunk (float* const* data, int numCh, int n, int mode)
{
    if (mode == 2) encodeMS (data, n);

    juce::dsp::AudioBlock<float> block (data, (size_t) numCh, (size_t) n);
    if (hqActive)
    {
        auto up = oversampler->processSamplesUp (block);
        runEq (up);
        oversampler->processSamplesDown (block);
    }
    else
    {
        runEq (block);
    }

    if (mode == 2) decodeMS (data, n);
}

void MixRefProcessor::runAudition (float* const* data, int numCh, int n, int mode, int which)
{
    const int set = which / eq::numBands, band = which % eq::numBands;
    const auto settings = readBand (set, band);
    const auto stages = eq::audition (settings, sr, pAdaptQ->load() > 0.5f, pScale->load() / 100.0);

    if (which != lastAudition)
    {
        for (auto& ch : auditionFilters) for (auto& f : ch) f.reset();
        lastAudition = which;
    }

    if (mode == 2) encodeMS (data, n);

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* x = data[ch];
        const bool active = (mode == 0) || numCh == 1 || ch == set;
        if (! active)
        {
            std::fill (x, x + n, 0.0f);
            continue;
        }
        for (int s = 0; s < stages.n; ++s)
        {
            auto& f = auditionFilters[(size_t) ch][(size_t) s];
            f.c = stages.c[(size_t) s];
            for (int i = 0; i < n; ++i) x[i] = f.process (x[i]);
        }
    }

    if (mode == 2) decodeMS (data, n);
}

//==============================================================================
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

    float* data[2] = { main.getWritePointer (0), numCh > 1 ? main.getWritePointer (1) : nullptr };

    // 1) Ganancia de entrada
    trimGain.setTargetValue (juce::Decibels::decibelsToGain (pTrim->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = trimGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch) data[ch][i] *= g;
    }

    // 2) Copia sin EQ (para Delta)
    if (dryBuffer.getNumSamples() < n) dryBuffer.setSize (2, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch) dryBuffer.copyFrom (ch, 0, data[ch], n);

    // Cambio de alta calidad: cambia la latencia del plugin
    const bool hq = pHq->load() > 0.5f;
    if (hq != hqActive)
    {
        hqActive = hq;
        oversampler->reset();
        for (auto& ch : bands) for (auto& b : ch) for (auto& st : b.st) st.reset();
        latency = hqActive ? juce::roundToInt (oversampler->getLatencyInSamples()) : 0;
        setLatencySamples (latency);
    }

    // 3) EQ, auricular o delta
    const bool eqOn = pEqOn->load() > 0.5f;
    const int mode = numCh == 2 ? eqMode() : 0;
    const int audition = pAudition->load() > 0.5f ? auditionBand.load() : -1;
    const bool delta = pDelta->load() > 0.5f;

    if (eqOn && audition >= 0)
    {
        runAudition (data, numCh, n, mode, audition);
    }
    else if (eqOn)
    {
        lastAudition = -1;
        updateEqCoefs (hqActive ? sr * 2.0 : sr, mode);
        for (int start = 0; start < n; start += maxBlock)
        {
            const int len = juce::jmin (maxBlock, n - start);
            float* chunk[2] = { data[0] + start, numCh > 1 ? data[1] + start : nullptr };
            processEqChunk (chunk, numCh, len, mode);
        }

        eqOutGain.setTargetValue (juce::Decibels::decibelsToGain (pEqOut->load()));
        for (int i = 0; i < n; ++i)
        {
            const float g = eqOutGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch) data[ch][i] *= g;
        }
    }

    // Linea de retardo de la señal sin EQ (alineada con la latencia del sobremuestreo)
    {
        const int size = (int) delayLine[0].size();
        for (int i = 0; i < n; ++i)
        {
            const int readPos = (delayPos - latency + size) % size;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& dl = delayLine[(size_t) ch];
                dl[(size_t) delayPos] = dryBuffer.getSample (ch, i);
                if (eqOn && delta && audition < 0)
                    data[ch][i] -= dl[(size_t) readPos];
            }
            delayPos = (delayPos + 1) % size;
        }
    }

    // 4) Graves mono, ducker, medidores
    const bool monoOn = pMonoOn->load() > 0.5f && numCh == 2;
    const bool duckOn = pDuckOn->load() > 0.5f && numSc > 0;
    const float depth  = pDuckDepth->load();
    const float thresh = pDuckThresh->load();
    const float attC = std::exp (-1.0f / (0.001f * pDuckAtt->load() * (float) sr));
    const float relC = std::exp (-1.0f / (0.001f * pDuckRel->load() * (float) sr));

    if (monoOn && std::abs (pMonoF->load() - cMonoF) > 1.0e-3f)
    {
        cMonoF = pMonoF->load();
        lowSplit.setCutoffFrequency (cMonoF);
        highSplit.setCutoffFrequency (cMonoF);
    }

    float peak = 0.0f, maxGR = 0.0f, scPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float x[2] = { data[0][i], numCh > 1 ? data[1][i] : 0.0f };

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

        float det = 0.0f;
        for (int c = 0; c < numSc; ++c) det = juce::jmax (det, std::abs (sc[c][i]));
        scPeak = juce::jmax (scPeak, det);
        duckEnv = det > duckEnv ? attC * duckEnv + (1.0f - attC) * det
                                : relC * duckEnv + (1.0f - relC) * det;

        if (duckOn)
        {
            const float envDb = juce::Decibels::gainToDecibels (duckEnv, -100.0f);
            const float amount = juce::jlimit (0.0f, 1.0f, (envDb - thresh) / 6.0f);
            const float gr = depth * amount;
            maxGR = juce::jmax (maxGR, gr);
            const float dg = juce::Decibels::decibelsToGain (-gr);
            x[0] *= dg; x[1] *= dg;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            data[ch][i] = x[ch];
            peak = juce::jmax (peak, std::abs (x[ch]));
        }
    }

    // Copia mono de la salida para el analizador
    {
        int s1, b1, s2, b2;
        analyserFifo.prepareToWrite (n, s1, b1, s2, b2);
        auto put = [&] (int start, int size, int offset)
        {
            for (int k = 0; k < size; ++k)
            {
                const int i = offset + k;
                float v = data[0][i];
                if (numCh > 1) v = 0.5f * (v + data[1][i]);
                analyserBuffer[(size_t) (start + k)] = v;
            }
        };
        put (s1, b1, 0);
        put (s2, b2, b1);
        analyserFifo.finishedWrite (b1 + b2);
    }

    atomicMax (meterPeak, peak);
    atomicMax (meterGR, maxGR);
    atomicMax (meterSC, scPeak);
}

int MixRefProcessor::pullAnalyserSamples (float* dest, int maxNum)
{
    int s1, b1, s2, b2;
    analyserFifo.prepareToRead (maxNum, s1, b1, s2, b2);
    for (int k = 0; k < b1; ++k) dest[k]      = analyserBuffer[(size_t) (s1 + k)];
    for (int k = 0; k < b2; ++k) dest[b1 + k] = analyserBuffer[(size_t) (s2 + k)];
    analyserFifo.finishedRead (b1 + b2);
    return b1 + b2;
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
    setParam ("eqMode", 0.0f);
    setParam ("eqEdit", 0.0f);
    setParam ("scale", 100.0f);
    setParam ("eqOut", 0.0f);

    // Todas las bandas a su lugar inicial y apagadas
    for (int b = 0; b < eq::numBands; ++b)
    {
        setParam (bandId (0, b, "on"), 0.0f);
        setParam (bandId (0, b, "type"), (float) defaultTypes[(size_t) b]);
        setParam (bandId (0, b, "freq"), defaultFreqs[(size_t) b]);
        setParam (bandId (0, b, "gain"), 0.0f);
        setParam (bandId (0, b, "q"), 0.71f);
    }

    auto band = [this] (int b, int type, float freq, float gain, float q, bool on)
    {
        setParam (bandId (0, b, "type"), (float) type);
        setParam (bandId (0, b, "freq"), freq);
        setParam (bandId (0, b, "gain"), gain);
        setParam (bandId (0, b, "q"), q);
        setParam (bandId (0, b, "on"), on ? 1.0f : 0.0f);
    };

    band (0, eq::LowCut48,  e.hpf,      0.0f,       0.71f,   e.hpf > 21.0f);
    band (2, eq::Bell,      e.mudFreq,  e.mudGain,  e.mudQ,  std::abs (e.mudGain) > 0.05f);
    band (4, eq::Bell,      e.presFreq, e.presGain, 0.8f,    std::abs (e.presGain) > 0.05f);
    band (6, eq::HighShelf, e.airFreq,  e.airGain,  0.71f,   std::abs (e.airGain) > 0.05f);

    setParam ("monoOn", e.mono ? 1.0f : 0.0f);
    setParam ("monoFreq", 120.0f);
    setParam ("duckOn", e.duck ? 1.0f : 0.0f);
    if (e.duck) setParam ("duckDepth", e.duckDepth);
}

void MixRefProcessor::matchTrimToTarget (float measuredPeakDb)
{
    if (measuredPeakDb < -80.0f) return; // sin señal, no tocar
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
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);

    auto find = [&tree] (const juce::String& id)
    {
        for (auto child : tree)
            if (child.getProperty ("id").toString() == id) return child;
        return juce::ValueTree();
    };

    // Sesiones de la v1.x: pasar el EQ de 4 bandas fijas a las bandas nuevas
    if (find ("hpf").isValid() && ! find ("a1freq").isValid())
    {
        auto get = [&find] (const char* id, float def)
        {
            auto c = find (id);
            return c.isValid() ? (float) c.getProperty ("value") : def;
        };
        auto add = [&tree] (const juce::String& id, float v)
        {
            juce::ValueTree c ("PARAM");
            c.setProperty ("id", id, nullptr);
            c.setProperty ("value", v, nullptr);
            tree.appendChild (c, nullptr);
        };
        auto addBand = [&add] (int b, int type, float f, float g, float q)
        {
            add (bandId (0, b, "on"), 1.0f);
            add (bandId (0, b, "type"), (float) type);
            add (bandId (0, b, "freq"), f);
            add (bandId (0, b, "gain"), g);
            add (bandId (0, b, "q"), q);
        };

        addBand (0, eq::LowCut48,  get ("hpf", 28.0f),          0.0f,                       0.71f);
        addBand (2, eq::Bell,      get ("mudFreq", 320.0f),     get ("mudGain", 0.0f),      get ("mudQ", 1.0f));
        addBand (4, eq::Bell,      get ("presFreq", 3000.0f),   get ("presGain", 0.0f),     0.8f);
        addBand (6, eq::HighShelf, get ("airFreq", 12000.0f),   get ("airGain", 0.0f),      0.707f);
        add ("adaptQ", 0.0f);   // asi suena igual que antes

        for (auto id : { "hpf", "mudFreq", "mudGain", "mudQ", "presFreq", "presGain", "airFreq", "airGain" })
        {
            auto c = find (id);
            if (c.isValid()) tree.removeChild (c, nullptr);
        }
    }

    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* MixRefProcessor::createEditor() { return new MixRefEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MixRefProcessor(); }
