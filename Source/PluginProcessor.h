#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "EqMath.h"

// Valores de partida por elemento (hoja de referencia de niveles, kick a -10 dBFS)
struct ElementPreset
{
    const char* name;
    float targetDb, lowDb, highDb;          // pico objetivo y rango aceptable (dBFS)
    float hpf;                              // filtro pasa altos (Hz)
    float mudFreq, mudGain, mudQ;           // corte de barro
    float presFreq, presGain;               // presencia
    float airFreq, airGain;                 // aire (shelf agudo)
    bool  mono;                             // graves en mono
    bool  duck;                             // sidechain activado
    float duckDepth;                        // dB de reduccion
    const char* group;                      // grupo del menu
};

// Orden del parametro (no cambiar: las sesiones guardadas usan este indice)
const std::vector<ElementPreset>& getElementPresets();
// Orden en que se muestran en el menu, agrupados como en la hoja
const std::vector<int>& getElementDisplayOrder();

// Textos con tildes y eñe
inline juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }

// Id de un parametro de banda: set 0 = "a" (estereo / L / Mid), set 1 = "b" (R / Side)
inline juce::String bandId (int set, int band, const char* what)
{
    return juce::String (set == 0 ? "a" : "b") + juce::String (band + 1) + what;
}

juce::StringArray getEqTypeNames();

class MixRefProcessor : public juce::AudioProcessor
{
public:
    MixRefProcessor();
    ~MixRefProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Acciones desde la interfaz
    void loadElementPreset();
    void matchTrimToTarget (float measuredPeakDb);
    const ElementPreset& currentPreset() const;
    eq::BandSettings readBand (int set, int band) const;
    int eqMode() const;                       // 0 estereo, 1 L/R, 2 Mid/Side
    double eqSampleRate() const;              // frecuencia de trabajo del EQ (x2 en alta calidad)

    juce::AudioProcessorValueTreeState apvts;

    // Lecturas para los medidores (las consume la interfaz)
    std::atomic<float> meterPeak { 0.0f };   // pico lineal desde la ultima lectura
    std::atomic<float> meterGR   { 0.0f };   // reduccion maxima del ducker (dB)
    std::atomic<float> meterSC   { 0.0f };   // pico lineal del sidechain

    // Banda que se esta escuchando con el auricular (set * 8 + banda, -1 = ninguna)
    std::atomic<int> auditionBand { -1 };

    // Muestras de salida para el analizador de espectro
    int pullAnalyserSamples (float* dest, int maxNum);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const juce::String& id, float value);
    void updateEqCoefs (double eqSr, int mode);
    void runEq (juce::dsp::AudioBlock<float>& block);
    void processEqChunk (float* const* data, int numCh, int n, int mode);
    void runAudition (float* const* data, int numCh, int n, int mode, int which);

    double sr = 44100.0;
    int maxBlock = 512;

    struct BandRuntime
    {
        int type = -1, n = 0;
        std::array<eq::Biquad, 4> st;
    };
    std::array<std::array<BandRuntime, eq::numBands>, 2> bands;   // [canal][banda]
    std::array<std::array<eq::Biquad, 2>, 2> auditionFilters;
    int lastAudition = -1;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    bool hqActive = false;
    int latency = 0;

    juce::AudioBuffer<float> dryBuffer;
    std::array<std::vector<float>, 2> delayLine;
    int delayPos = 0;

    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;
    juce::SmoothedValue<float> trimGain, eqOutGain;
    float duckEnv = 0.0f;
    float cMonoF = -1.0f;

    static constexpr int analyserFifoSize = 16384;
    juce::AbstractFifo analyserFifo { analyserFifoSize };
    std::vector<float> analyserBuffer = std::vector<float> ((size_t) analyserFifoSize, 0.0f);

    struct BandParams { std::atomic<float>* on; std::atomic<float>* type; std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; };
    std::array<std::array<BandParams, eq::numBands>, 2> bp;

    std::atomic<float>* pElement; std::atomic<float>* pTrim;
    std::atomic<float>* pEqOn; std::atomic<float>* pMode; std::atomic<float>* pAdaptQ; std::atomic<float>* pHq;
    std::atomic<float>* pScale; std::atomic<float>* pEqOut; std::atomic<float>* pAudition; std::atomic<float>* pDelta;
    std::atomic<float>* pMonoOn; std::atomic<float>* pMonoF;
    std::atomic<float>* pDuckOn; std::atomic<float>* pDuckDepth; std::atomic<float>* pDuckThresh;
    std::atomic<float>* pDuckAtt; std::atomic<float>* pDuckRel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixRefProcessor)
};
