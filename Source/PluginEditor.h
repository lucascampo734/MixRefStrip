#pragma once

#include "PluginProcessor.h"
#include "EqGraph.h"
#include <memory>
#include <vector>

class MixRefEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MixRefEditor (MixRefProcessor&);
    ~MixRefEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintMeter (juce::Graphics&, juce::Rectangle<int> area);
    void paintDuckMeter (juce::Graphics&, juce::Rectangle<int> area);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    Knob& addKnob (const juce::String& paramId, const juce::String& text);

    MixRefProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    juce::ComboBox elementBox;
    void syncElementBox();
    juce::TextButton presetButton { "Cargar preset" };
    juce::TextButton matchButton  { "Ajustar al objetivo" };
    juce::TextButton resetButton  { "Reset pico" };

    juce::ToggleButton eqOnButton { "EQ" }, monoButton { "Graves mono" }, duckOnButton { "Ducker" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqOnAtt, monoAtt, duckOnAtt;

    EqGraph eqGraph { proc };

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* trimKnob = nullptr;
    std::vector<Knob*> eqKnobs, duckKnobs;

    // Estado de los medidores (en dB)
    float levelDb = -100.0f;   // nivel con caida suave
    float maxDb   = -100.0f;   // pico maximo desde el ultimo reset
    float grDb    = 0.0f;
    float scDb    = -100.0f;

    juce::Rectangle<int> meterArea, meterPanel, eqPanel, duckPanel, duckMeterArea, header;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixRefEditor)
};
