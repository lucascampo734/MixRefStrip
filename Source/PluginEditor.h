#pragma once

#include "PluginProcessor.h"
#include "EqGraph.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

class MixRefEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MixRefEditor (MixRefProcessor&);
    ~MixRefEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    void paintMeter (juce::Graphics&, juce::Rectangle<int> area);
    void paintDuckMeter (juce::Graphics&, juce::Rectangle<int> area);
    void syncElementBox();
    void syncEqEditState();
    void selectBand (int band);
    void bindBandControls();
    void bindBandButtons();
    void startAudition();
    void stopAudition();

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };
    Knob& addKnob (const juce::String& paramId, const juce::String& text);

    MixRefProcessor& proc;
    juce::LookAndFeel_V4 lnf;

    // Encabezado
    juce::ComboBox elementBox;
    juce::TextButton presetButton { "Cargar preset" };

    // Nivel
    juce::TextButton matchButton  { "Ajustar al objetivo" };
    juce::TextButton resetButton  { "Reset pico" };

    // EQ
    juce::ToggleButton eqOnButton { "EQ" }, monoButton { "Graves mono" }, duckOnButton { "Ducker" };
    juce::ToggleButton adaptQButton { "Adaptive Q" }, hqButton { "Alta calidad (x2)" };
    juce::TextButton auditionButton, deltaButton { "Delta" };
    juce::ComboBox modeBox;
    juce::TextButton editAButton, editBButton;
    std::unique_ptr<APVTS::ButtonAttachment> eqOnAtt, monoAtt, duckOnAtt, adaptQAtt, hqAtt, auditionAtt, deltaAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt;

    // Botón de banda: solo selecciona cuando lo toca el usuario (no cuando cambia el parámetro)
    struct BandButton : public juce::TextButton
    {
        std::function<void()> onUserClick;
        void mouseUp (const juce::MouseEvent& e) override
        {
            const bool inside = isMouseOver();
            juce::TextButton::mouseUp (e);
            if (inside && onUserClick) onUserClick();
        }
    };
    std::array<BandButton, eq::numBands> bandButtons;
    std::array<std::unique_ptr<APVTS::ButtonAttachment>, eq::numBands> bandButtonAtts;

    juce::Label bandLabel;
    juce::ComboBox typeBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> typeAtt;
    Knob freqKnob, gainKnob, qKnob;

    EqGraph eqGraph { proc };

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* trimKnob = nullptr;
    std::vector<Knob*> eqGlobalKnobs, duckKnobs;

    int selectedBand = 0, editSet = 0, shownMode = -1;

    // Estado de los medidores (en dB)
    float levelDb = -100.0f;
    float maxDb   = -100.0f;
    float grDb    = 0.0f;
    float scDb    = -100.0f;

    juce::Rectangle<int> meterArea, meterPanel, eqPanel, duckPanel, duckMeterArea, header;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixRefEditor)
};
