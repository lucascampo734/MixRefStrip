#pragma once

#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <vector>

// Gráfico del EQ de 8 bandas: curva de respuesta, analizador de espectro
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

    int editSet = 0;                          // 0 = A (estéreo / L / Mid), 1 = B (R / Side)
    int selectedBand = 0;
    std::function<void (int)> onBandSelected;

    static juce::Colour bandColour (int band);

private:
    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    float yForSpectrumDb (float db) const;

    juce::Point<float> nodePosition (int band) const;
    int nodeAt (juce::Point<float> p) const;
    double responseDb (int set, double freq, int onlyBand = -1) const;

    float getParam (const juce::String& id) const;
    void setParam (const juce::String& id, float value);
    void gesture (int band, bool begin);

    MixRefProcessor& proc;
    int hoverNode = -1, dragNode = -1;
    bool dragTurnedOn = false;

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
