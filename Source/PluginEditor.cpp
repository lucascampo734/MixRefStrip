#include "PluginEditor.h"
#include <array>

namespace Colours
{
    const juce::Colour bg     { 0xff121318 };
    const juce::Colour panel  { 0xff1c1e25 };
    const juce::Colour edge   { 0xff2c2f39 };
    const juce::Colour text   { 0xffe6e7eb };
    const juce::Colour dim    { 0xff8b8f9c };
    const juce::Colour accent { 0xff8b5cf6 };
    const juce::Colour ok     { 0xff4ade80 };
    const juce::Colour warn   { 0xfffacc15 };
    const juce::Colour bad    { 0xfff87171 };
    const juce::Colour duck   { 0xff38bdf8 };
}

static constexpr float meterMinDb = -48.0f;

//==============================================================================
MixRefEditor::MixRefEditor (MixRefProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf.setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
    lnf.setColour (juce::Slider::rotarySliderOutlineColourId, Colours::edge);
    lnf.setColour (juce::Slider::thumbColourId, Colours::text);
    lnf.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    lnf.setColour (juce::Label::textColourId, Colours::dim);
    lnf.setColour (juce::ComboBox::backgroundColourId, Colours::panel);
    lnf.setColour (juce::ComboBox::outlineColourId, Colours::edge);
    lnf.setColour (juce::ComboBox::textColourId, Colours::text);
    lnf.setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    lnf.setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    lnf.setColour (juce::TextButton::buttonColourId, Colours::edge);
    lnf.setColour (juce::TextButton::textColourOffId, Colours::text);
    lnf.setColour (juce::ToggleButton::textColourId, Colours::text);
    lnf.setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setLookAndFeel (&lnf);

    // Elemento: menu agrupado como en la hoja de referencia
    {
        const auto& all = getElementPresets();
        juce::String lastGroup;
        for (int idx : getElementDisplayOrder())
        {
            const auto& e = all[(size_t) idx];
            const auto group = utf8 (e.group);
            if (group != lastGroup)
            {
                elementBox.addSectionHeading (group);
                lastGroup = group;
            }
            elementBox.addItem (utf8 (e.name), idx + 1);
        }
    }
    elementBox.onChange = [this]
    {
        const int idx = elementBox.getSelectedId() - 1;
        if (idx < 0) return;
        if (auto* param = proc.apvts.getParameter ("element"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) idx));
            param->endChangeGesture();
        }
        repaint();
    };
    syncElementBox();
    addAndMakeVisible (elementBox);

    presetButton.setTooltip ("Carga el EQ, graves mono y ducker sugeridos para el elemento elegido");
    presetButton.onClick = [this] { proc.loadElementPreset(); };
    addAndMakeVisible (presetButton);

    matchButton.onClick = [this] { proc.matchTrimToTarget (maxDb); maxDb = -100.0f; };
    resetButton.onClick = [this] { maxDb = -100.0f; };
    addAndMakeVisible (matchButton);
    addAndMakeVisible (resetButton);

    // Interruptores
    for (auto* b : { &eqOnButton, &monoButton, &duckOnButton }) addAndMakeVisible (*b);
    eqOnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "eqOn", eqOnButton);
    monoAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "monoOn", monoButton);
    duckOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "duckOn", duckOnButton);

    // Perillas
    trimKnob = &addKnob ("trim", "Ganancia");

    eqKnobs = { &addKnob ("hpf", "Pasa altos"),
                &addKnob ("mudFreq", "Barro Hz"),
                &addKnob ("mudGain", "Barro dB"),
                &addKnob ("mudQ", "Barro Q"),
                &addKnob ("presFreq", "Presencia Hz"),
                &addKnob ("presGain", "Presencia dB"),
                &addKnob ("airFreq", "Aire Hz"),
                &addKnob ("airGain", "Aire dB"),
                &addKnob ("monoFreq", "Mono hasta") };

    duckKnobs = { &addKnob ("duckDepth", "Profundidad"),
                  &addKnob ("duckThresh", "Umbral"),
                  &addKnob ("duckAttack", "Ataque"),
                  &addKnob ("duckRelease", "Release") };

    addAndMakeVisible (eqGraph);

    setSize (1000, 620);
    startTimerHz (30);
}

MixRefEditor::~MixRefEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

MixRefEditor::Knob& MixRefEditor::addKnob (const juce::String& paramId, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    k->label.setText (text, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (12.5f));
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void MixRefEditor::syncElementBox()
{
    const int idx = juce::roundToInt (proc.apvts.getRawParameterValue ("element")->load());
    if (elementBox.getSelectedId() != idx + 1)
    {
        elementBox.setSelectedId (idx + 1, juce::dontSendNotification);
        repaint();
    }
}

//==============================================================================
void MixRefEditor::timerCallback()
{
    syncElementBox();

    const float peakLin = proc.meterPeak.exchange (0.0f);
    const float peakDb = juce::Decibels::gainToDecibels (peakLin, -100.0f);
    levelDb = peakDb > levelDb ? peakDb : juce::jmax (peakDb, levelDb - 0.8f); // caida ~24 dB/s
    maxDb = juce::jmax (maxDb, peakDb);

    const float gr = proc.meterGR.exchange (0.0f);
    grDb = gr > grDb ? gr : grDb * 0.85f;

    const float sc = juce::Decibels::gainToDecibels (proc.meterSC.exchange (0.0f), -100.0f);
    scDb = sc > scDb ? sc : scDb - 1.5f;

    eqGraph.updateSpectrum();
    eqGraph.repaint();
    repaint (meterPanel);
    repaint (duckPanel);
}

static juce::Colour statusColour (float db, const ElementPreset& e)
{
    if (db < -80.0f)                return Colours::dim;
    if (db > e.highDb + 2.0f)       return Colours::bad;
    if (db > e.highDb)              return Colours::warn;
    if (db >= e.lowDb)              return Colours::ok;
    if (db >= e.lowDb - 3.0f)       return Colours::warn;
    return Colours::warn;
}

static juce::String statusText (float db, const ElementPreset& e)
{
    if (db < -80.0f)          return utf8 ("Sin señal");
    if (db > e.highDb)        return "Alto: bajar " + juce::String (db - e.targetDb, 1) + " dB";
    if (db < e.lowDb)         return "Bajo: subir " + juce::String (e.targetDb - db, 1) + " dB";
    return "En rango";
}

void MixRefEditor::paintMeter (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& e = proc.currentPreset();
    auto r = area.toFloat();
    auto yFor = [&] (float db)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (db - meterMinDb) / (0.0f - meterMinDb));
        return r.getBottom() - t * r.getHeight();
    };

    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 4.0f);

    // Zona objetivo
    g.setColour (Colours::ok.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), yFor (e.highDb), r.getRight(), yFor (e.lowDb)));

    // Barra de nivel
    if (levelDb > meterMinDb)
    {
        g.setColour (statusColour (levelDb, e));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX() + 4, yFor (levelDb), r.getRight() - 4, r.getBottom()));
    }

    // Linea de objetivo y pico maximo
    g.setColour (Colours::text);
    g.drawHorizontalLine ((int) yFor (e.targetDb), r.getX() - 6, r.getRight() + 6);
    if (maxDb > meterMinDb)
    {
        g.setColour (statusColour (maxDb, e).brighter (0.3f));
        g.fillRect (r.getX(), yFor (maxDb) - 1.0f, r.getWidth(), 2.0f);
    }

    // Escala
    g.setFont (juce::FontOptions (10.5f));
    for (float db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -36.0f, -42.0f, -48.0f })
    {
        g.setColour (Colours::dim);
        g.drawText (juce::String ((int) db), (int) r.getX() - 34, (int) yFor (db) - 6, 26, 12, juce::Justification::centredRight);
        g.setColour (Colours::edge);
        g.drawHorizontalLine ((int) yFor (db), r.getX() - 6, r.getX());
    }
}

void MixRefEditor::paintDuckMeter (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto r = area.toFloat();
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (utf8 ("Reducción"), r.removeFromTop (16), juce::Justification::centredLeft);

    auto bar = r.removeFromTop (14);
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, grDb / 24.0f);
    g.setColour (Colours::duck);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
    g.setColour (Colours::text);
    g.drawText ((grDb > 0.05f ? "-" : "") + juce::String (grDb, 1) + " dB", bar.reduced (6, 0), juce::Justification::centredRight);

    r.removeFromTop (10);
    const bool hasSc = scDb > -70.0f;
    g.setColour (hasSc ? Colours::ok : Colours::warn);
    g.fillEllipse (r.getX(), r.getY() + 4, 9, 9);
    g.setColour (Colours::text);
    g.drawText (hasSc ? utf8 ("Sidechain: recibiendo señal (") + juce::String (scDb, 0) + " dB)"
                      : utf8 ("Sidechain: sin señal"),
                r.withTrimmedLeft (16).removeFromTop (18), juce::Justification::centredLeft);
}

void MixRefEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    // Encabezado
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("MixRef Strip", header.withTrimmedLeft (20), juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Referencia: kick a -10 dBFS", header.withTrimmedLeft (170), juce::Justification::centredLeft);

    for (auto panel : { meterPanel, eqPanel, duckPanel })
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (panel.toFloat(), 8.0f);
        g.setColour (Colours::edge);
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 8.0f, 1.0f);
    }

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("Nivel", meterPanel.getX() + 14, meterPanel.getY() + 10, 120, 20, juce::Justification::centredLeft);

    paintMeter (g, meterArea);
    paintDuckMeter (g, duckMeterArea);

    // Lecturas al lado del medidor
    const auto& e = proc.currentPreset();
    auto info = juce::Rectangle<int> (meterArea.getRight() + 16, meterArea.getY(), meterPanel.getRight() - meterArea.getRight() - 26, 120);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (Colours::dim);
    g.drawText (utf8 ("Pico máximo"), info.removeFromTop (16), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.setColour (statusColour (maxDb, e));
    g.drawText (maxDb < -80.0f ? juce::String ("--") : juce::String (maxDb, 1), info.removeFromTop (30), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (Colours::dim);
    g.drawText ("Objetivo: " + juce::String (e.lowDb, 0) + " a " + juce::String (e.highDb, 0) + " dBFS",
                info.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (statusColour (maxDb, e));
    g.drawFittedText (statusText (maxDb, e), info.removeFromTop (34), juce::Justification::topLeft, 2);

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (utf8 ("Valores orientativos: escuchá y compará con tus referencias."),
                getLocalBounds().removeFromBottom (22).withTrimmedLeft (20), juce::Justification::centredLeft);
}

void MixRefEditor::resized()
{
    auto area = getLocalBounds();
    header = area.removeFromTop (58);
    area.removeFromBottom (22);
    area.reduce (14, 0);

    // Selector de elemento en el encabezado
    auto h = header.reduced (14, 13);
    presetButton.setBounds (h.removeFromRight (130));
    h.removeFromRight (8);
    elementBox.setBounds (h.removeFromRight (230));

    meterPanel = area.removeFromLeft (250);
    area.removeFromLeft (10);
    duckPanel = area.removeFromRight (220);
    area.removeFromRight (10);
    eqPanel = area;

    // Panel de nivel
    {
        auto r = meterPanel.reduced (14);
        r.removeFromTop (28);
        meterArea = juce::Rectangle<int> (r.getX() + 34, r.getY() + 6, 34, r.getHeight() - 12);
        auto right = r.withTrimmedLeft (meterArea.getRight() - r.getX() + 12);
        right.removeFromTop (130);
        trimKnob->label.setBounds (right.removeFromTop (16));
        trimKnob->slider.setBounds (right.removeFromTop (92).withSizeKeepingCentre (96, 92));
        right.removeFromTop (8);
        matchButton.setBounds (right.removeFromTop (28));
        right.removeFromTop (6);
        resetButton.setBounds (right.removeFromTop (24));
    }

    auto placeKnob = [] (Knob* k, juce::Rectangle<int> cell)
    {
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    };

    // Panel EQ: gráfico arriba, perillas abajo (5 + 4)
    {
        auto r = eqPanel.reduced (12);
        auto top = r.removeFromTop (24);
        eqOnButton.setBounds (top.removeFromLeft (70));
        top.removeFromLeft (10);
        monoButton.setBounds (top.removeFromLeft (130));
        r.removeFromTop (8);
        eqGraph.setBounds (r.removeFromTop (220));
        r.removeFromTop (8);

        const int rowH = r.getHeight() / 2;
        const int colW = r.getWidth() / 5;
        const std::array<int, 5> row1 { 0, 1, 2, 3, 8 };   // HPF, barro Hz/dB/Q, mono
        const std::array<int, 4> row2 { 4, 5, 6, 7 };      // presencia y aire
        for (int c = 0; c < 5; ++c)
            placeKnob (eqKnobs[(size_t) row1[(size_t) c]], { r.getX() + c * colW, r.getY(), colW, rowH });
        const int offset = colW / 2;
        for (int c = 0; c < 4; ++c)
            placeKnob (eqKnobs[(size_t) row2[(size_t) c]], { r.getX() + offset + c * colW, r.getY() + rowH, colW, rowH });
    }

    // Panel ducker: 2 x 2 + medidor
    {
        auto r = duckPanel.reduced (12);
        duckOnButton.setBounds (r.removeFromTop (24).removeFromLeft (120));
        r.removeFromTop (6);
        duckMeterArea = r.removeFromBottom (76);
        const int rowH = r.getHeight() / 2, colW = r.getWidth() / 2;
        for (int i = 0; i < 4; ++i)
            placeKnob (duckKnobs[(size_t) i], { r.getX() + (i % 2) * colW, r.getY() + (i / 2) * rowH, colW, rowH });
    }
}
