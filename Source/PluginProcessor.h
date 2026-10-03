#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

// Valores de partida por elemento (del checklist de mezcla, kick de referencia a -10 dBFS)
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
};

const std::array<ElementPreset, 8>& getElementPresets();

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

    juce::AudioProcessorValueTreeState apvts;

    // Lecturas para los medidores (las consume la interfaz)
    std::atomic<float> meterPeak { 0.0f };   // pico lineal desde la ultima lectura
    std::atomic<float> meterGR   { 0.0f };   // reduccion maxima del ducker (dB)
    std::atomic<float> meterSC   { 0.0f };   // pico lineal del sidechain

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (bool force);
    void setParam (const juce::String& id, float value);

    double sr = 44100.0;

    using Filter = juce::dsp::IIR::Filter<float>;
    std::array<Filter, 2> hpf1, hpf2, mud, pres, air;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;
    juce::SmoothedValue<float> trimGain;
    float duckEnv = 0.0f;

    // cache para no recalcular coeficientes en cada bloque
    float cHpf = -1, cMudF = -1, cMudG = -100, cMudQ = -1, cPresF = -1, cPresG = -100,
          cAirF = -1, cAirG = -100, cMonoF = -1;

    std::atomic<float>* pElement; std::atomic<float>* pTrim;
    std::atomic<float>* pEqOn; std::atomic<float>* pHpf;
    std::atomic<float>* pMudF; std::atomic<float>* pMudG; std::atomic<float>* pMudQ;
    std::atomic<float>* pPresF; std::atomic<float>* pPresG;
    std::atomic<float>* pAirF; std::atomic<float>* pAirG;
    std::atomic<float>* pMonoOn; std::atomic<float>* pMonoF;
    std::atomic<float>* pDuckOn; std::atomic<float>* pDuckDepth; std::atomic<float>* pDuckThresh;
    std::atomic<float>* pDuckAtt; std::atomic<float>* pDuckRel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixRefProcessor)
};
