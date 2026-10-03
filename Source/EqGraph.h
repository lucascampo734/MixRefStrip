#pragma once

#include "PluginProcessor.h"
#include <array>
#include <vector>

// Gráfico del EQ estilo EQ Eight: curva de respuesta, analizador de espectro
// y nodos que se arrastran con el mouse.
class EqGraph : public juce::Component
{
public:
    explicit EqGraph (MixRefProcessor&);

    void paint (juce::Graphics&) override;
    void updateSpectrum();   // llamar desde el timer del editor

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Node
    {
        const char* name;
        const char* freqId;
        const char* gainId;   // nullptr si no tiene ganancia
        const char* qId;      // nullptr si no tiene Q
        juce::Colour colour;
        float fixedDb;        // altura fija si no tiene ganancia
    };

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float yForSpectrumDb (float db) const;

    bool nodeVisible (int i) const;
    juce::Point<float> nodePosition (int i) const;
    int nodeAt (juce::Point<float> p) const;

    float getParam (const char* id) const;
    void setParam (const char* id, float value);
    void beginGesture (int node);
    void endGesture (int node);

    double sampleRate() const;

    MixRefProcessor& proc;
    std::array<Node, 5> nodes;
    int hoverNode = -1, dragNode = -1;

    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f, rangeDb = 18.0f;
    static constexpr float specMinDb = -90.0f, specMaxDb = 0.0f;

    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> incoming = std::vector<float> (16384, 0.0f);
    std::vector<float> ring = std::vector<float> ((size_t) fftSize, 0.0f);
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> spectrumDb = std::vector<float> ((size_t) fftSize / 2, specMinDb);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraph)
};
