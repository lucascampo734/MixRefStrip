#include "PluginEditor.h"

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
    lnf.setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    lnf.setColour (juce::TextButton::textColourOffId, Colours::text);
    lnf.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    lnf.setColour (juce::ToggleButton::textColourId, Colours::text);
    lnf.setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setLookAndFeel (&lnf);

    // Elemento: menú agrupado como en la hoja de referencia
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
    presetButton.onClick = [this] { proc.loadElementPreset(); selectBand (0); };
    addAndMakeVisible (presetButton);

    matchButton.onClick = [this] { proc.matchTrimToTarget (maxDb); maxDb = -100.0f; };
    resetButton.onClick = [this] { maxDb = -100.0f; };
    addAndMakeVisible (matchButton);
    addAndMakeVisible (resetButton);

    // Interruptores
    for (auto* b : { &eqOnButton, &monoButton, &duckOnButton, &adaptQButton, &hqButton }) addAndMakeVisible (*b);
    eqOnAtt   = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "eqOn", eqOnButton);
    monoAtt   = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "monoOn", monoButton);
    duckOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "duckOn", duckOnButton);
    adaptQAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "adaptQ", adaptQButton);
    hqAtt     = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "hq", hqButton);
    adaptQButton.setTooltip ("El Q de las campanas se cierra cuando subís mucha ganancia");
    hqButton.setTooltip ("Procesa el EQ al doble de frecuencia de muestreo: agudos más naturales (suma un poco de latencia)");

    auditionButton.setButtonText (utf8 ("Audición"));
    auditionButton.setClickingTogglesState (true);
    deltaButton.setClickingTogglesState (true);
    auditionButton.setTooltip (utf8 ("Con esto prendido, mantené apretado un punto o una perilla de banda para escuchar solo esa zona"));
    deltaButton.setTooltip ("Escuchás solo lo que el EQ saca o agrega");
    auditionAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "audition", auditionButton);
    deltaAtt    = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "delta", deltaButton);
    addAndMakeVisible (auditionButton);
    addAndMakeVisible (deltaButton);

    // Modo estéreo / L-R / M-S y canal que se edita
    modeBox.addItemList ({ utf8 ("Estéreo"), "L/R", "Mid/Side" }, 1);
    modeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "eqMode", modeBox);
    addAndMakeVisible (modeBox);

    for (int i = 0; i < 2; ++i)
    {
        auto& b = i == 0 ? editAButton : editBButton;
        b.setClickingTogglesState (true);
        b.setRadioGroupId (42);
        b.onClick = [this, i]
        {
            if (auto* param = proc.apvts.getParameter ("eqEdit"))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
                param->endChangeGesture();
            }
            syncEqEditState();
        };
        addChildComponent (b);
    }

    // Botones de banda 1..8 (prenden/apagan y seleccionan)
    for (int i = 0; i < eq::numBands; ++i)
    {
        auto& b = bandButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, EqGraph::bandColour (i));
        b.setColour (juce::TextButton::textColourOnId, Colours::bg);
        b.onUserClick = [this, i] { selectBand (i); };
        addAndMakeVisible (b);
    }

    // Controles de la banda seleccionada
    bandLabel.setJustificationType (juce::Justification::centredLeft);
    bandLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (bandLabel);
    typeBox.addItemList (getEqTypeNames(), 1);
    addAndMakeVisible (typeBox);

    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
    {
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
        k->slider.onDragStart = [this] { startAudition(); };
        k->slider.onDragEnd   = [this] { stopAudition(); };
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (12.5f));
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
    }
    freqKnob.label.setText ("Frecuencia", juce::dontSendNotification);
    gainKnob.label.setText ("Ganancia", juce::dontSendNotification);
    qKnob.label.setText ("Q", juce::dontSendNotification);

    // Gráfico
    eqGraph.onBandSelected = [this] (int b) { selectBand (b); };
    addAndMakeVisible (eqGraph);

    // Perillas fijas
    trimKnob = &addKnob ("trim", "Ganancia");
    eqGlobalKnobs = { &addKnob ("scale", "Escala"),
                      &addKnob ("eqOut", "Salida EQ"),
                      &addKnob ("monoFreq", "Mono hasta") };
    duckKnobs = { &addKnob ("duckDepth", "Profundidad"),
                  &addKnob ("duckThresh", "Umbral"),
                  &addKnob ("duckAttack", "Ataque"),
                  &addKnob ("duckRelease", "Release") };

    syncEqEditState();
    bindBandButtons();
    bindBandControls();

    setSize (1100, 680);
    startTimerHz (30);
}

MixRefEditor::~MixRefEditor()
{
    stopTimer();
    proc.auditionBand = -1;
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
    k->attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, k->slider);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

//==============================================================================
void MixRefEditor::selectBand (int band)
{
    selectedBand = juce::jlimit (0, eq::numBands - 1, band);
    eqGraph.selectedBand = selectedBand;
    bindBandControls();
    repaint();
}

void MixRefEditor::bindBandControls()
{
    typeAtt.reset();
    freqKnob.attachment.reset();
    gainKnob.attachment.reset();
    qKnob.attachment.reset();

    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, bandId (editSet, selectedBand, "type"), typeBox);
    freqKnob.attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, bandId (editSet, selectedBand, "freq"), freqKnob.slider);
    gainKnob.attachment = std::make_unique<APVTS::SliderAttachment> (proc.apvts, bandId (editSet, selectedBand, "gain"), gainKnob.slider);
    qKnob.attachment    = std::make_unique<APVTS::SliderAttachment> (proc.apvts, bandId (editSet, selectedBand, "q"), qKnob.slider);

    bandLabel.setText ("Banda " + juce::String (selectedBand + 1), juce::dontSendNotification);
    bandLabel.setColour (juce::Label::textColourId, EqGraph::bandColour (selectedBand));
    const auto col = EqGraph::bandColour (selectedBand);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, col);
}

void MixRefEditor::bindBandButtons()
{
    for (int i = 0; i < eq::numBands; ++i)
    {
        bandButtonAtts[(size_t) i].reset();
        bandButtonAtts[(size_t) i] = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, bandId (editSet, i, "on"), bandButtons[(size_t) i]);
    }
}

void MixRefEditor::syncEqEditState()
{
    const int mode = proc.eqMode();
    const int wanted = mode == 0 ? 0 : juce::roundToInt (proc.apvts.getRawParameterValue ("eqEdit")->load());

    if (mode != shownMode)
    {
        shownMode = mode;
        editAButton.setVisible (mode != 0);
        editBButton.setVisible (mode != 0);
        editAButton.setButtonText (mode == 2 ? "Mid" : "L");
        editBButton.setButtonText (mode == 2 ? "Side" : "R");
    }

    editAButton.setToggleState (wanted == 0, juce::dontSendNotification);
    editBButton.setToggleState (wanted == 1, juce::dontSendNotification);

    if (wanted != editSet)
    {
        editSet = wanted;
        eqGraph.editSet = editSet;
        bindBandButtons();
        bindBandControls();
        repaint();
    }
}

void MixRefEditor::startAudition()
{
    if (proc.apvts.getRawParameterValue ("audition")->load() > 0.5f)
        proc.auditionBand = editSet * eq::numBands + selectedBand;
}

void MixRefEditor::stopAudition()
{
    proc.auditionBand = -1;
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
    syncEqEditState();

    // Ganancia solo en campana y shelves
    const bool gainUsed = eq::hasGain (proc.readBand (editSet, selectedBand).type);
    gainKnob.slider.setEnabled (gainUsed);
    gainKnob.slider.setAlpha (gainUsed ? 1.0f : 0.35f);
    gainKnob.label.setAlpha (gainUsed ? 1.0f : 0.35f);

    const float peakLin = proc.meterPeak.exchange (0.0f);
    const float peakDb = juce::Decibels::gainToDecibels (peakLin, -100.0f);
    levelDb = peakDb > levelDb ? peakDb : juce::jmax (peakDb, levelDb - 0.8f);
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

    g.setColour (Colours::ok.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), yFor (e.highDb), r.getRight(), yFor (e.lowDb)));

    if (levelDb > meterMinDb)
    {
        g.setColour (statusColour (levelDb, e));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX() + 4, yFor (levelDb), r.getRight() - 4, r.getBottom()));
    }

    g.setColour (Colours::text);
    g.drawHorizontalLine ((int) yFor (e.targetDb), r.getX() - 6, r.getRight() + 6);
    if (maxDb > meterMinDb)
    {
        g.setColour (statusColour (maxDb, e).brighter (0.3f));
        g.fillRect (r.getX(), yFor (maxDb) - 1.0f, r.getWidth(), 2.0f);
    }

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
    g.drawFittedText (hasSc ? utf8 ("Sidechain: recibiendo señal (") + juce::String (scDb, 0) + " dB)"
                            : utf8 ("Sidechain: sin señal"),
                      r.withTrimmedLeft (16).removeFromTop (34).toNearestInt(), juce::Justification::topLeft, 2);
}

void MixRefEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("MixRef Strip", header.withTrimmedLeft (20), juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v2.0  |  Referencia: kick a -10 dBFS", header.withTrimmedLeft (170), juce::Justification::centredLeft);

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

    // separador entre controles de banda y globales del EQ
    if (! eqGlobalKnobs.empty())
    {
        const int x = eqGlobalKnobs[0]->label.getX() - 8;
        g.setColour (Colours::edge);
        g.drawVerticalLine (x, (float) qKnob.label.getY(), (float) qKnob.slider.getBottom());
    }

    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (utf8 ("Valores orientativos: escuchá y compará con tus referencias."),
                getLocalBounds().removeFromBottom (22).withTrimmedLeft (20), juce::Justification::centredLeft);
}

void MixRefEditor::paintOverChildren (juce::Graphics& g)
{
    // Marco blanco en el botón de la banda seleccionada
    auto r = bandButtons[(size_t) selectedBand].getBounds().toFloat().expanded (2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
}

void MixRefEditor::resized()
{
    auto area = getLocalBounds();
    header = area.removeFromTop (58);
    area.removeFromBottom (22);
    area.reduce (14, 0);

    auto h = header.reduced (14, 13);
    presetButton.setBounds (h.removeFromRight (130));
    h.removeFromRight (8);
    elementBox.setBounds (h.removeFromRight (230));

    meterPanel = area.removeFromLeft (240);
    area.removeFromLeft (10);
    duckPanel = area.removeFromRight (200);
    area.removeFromRight (10);
    eqPanel = area;

    auto placeKnob = [] (Knob* k, juce::Rectangle<int> cell)
    {
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    };

    // Panel de nivel
    {
        auto r = meterPanel.reduced (14);
        r.removeFromTop (28);
        meterArea = juce::Rectangle<int> (r.getX() + 34, r.getY() + 6, 34, r.getHeight() - 12);
        auto right = r.withTrimmedLeft (meterArea.getRight() - r.getX() + 12);
        right.removeFromTop (140);
        trimKnob->label.setBounds (right.removeFromTop (16));
        trimKnob->slider.setBounds (right.removeFromTop (92).withSizeKeepingCentre (96, 92));
        right.removeFromTop (8);
        matchButton.setBounds (right.removeFromTop (28));
        right.removeFromTop (6);
        resetButton.setBounds (right.removeFromTop (24));
    }

    // Panel EQ
    {
        auto r = eqPanel.reduced (12);

        auto top = r.removeFromTop (26);
        eqOnButton.setBounds (top.removeFromLeft (60));
        monoButton.setBounds (top.removeFromLeft (125));
        editBButton.setBounds (top.removeFromRight (52));
        top.removeFromRight (4);
        editAButton.setBounds (top.removeFromRight (52));
        top.removeFromRight (8);
        modeBox.setBounds (top.removeFromRight (110));
        r.removeFromTop (6);

        auto bottom = r.removeFromBottom (24);
        adaptQButton.setBounds (bottom.removeFromLeft (115));
        hqButton.setBounds (bottom.removeFromLeft (160));
        r.removeFromBottom (6);

        auto knobRow = r.removeFromBottom (118);
        r.removeFromBottom (8);
        auto bandRow = r.removeFromBottom (30);
        r.removeFromBottom (8);
        eqGraph.setBounds (r);

        for (int i = 0; i < eq::numBands; ++i)
        {
            bandButtons[(size_t) i].setBounds (bandRow.removeFromLeft (34).reduced (0, 2));
            bandRow.removeFromLeft (5);
        }
        deltaButton.setBounds (bandRow.removeFromRight (70).reduced (0, 2));
        bandRow.removeFromRight (6);
        auditionButton.setBounds (bandRow.removeFromRight (90).reduced (0, 2));

        auto typeCol = knobRow.removeFromLeft (112);
        bandLabel.setBounds (typeCol.removeFromTop (22));
        typeCol.removeFromTop (6);
        typeBox.setBounds (typeCol.removeFromTop (26));
        knobRow.removeFromLeft (6);

        const int kw = 72;
        placeKnob (&freqKnob, knobRow.removeFromLeft (kw));
        placeKnob (&gainKnob, knobRow.removeFromLeft (kw));
        placeKnob (&qKnob, knobRow.removeFromLeft (kw));
        knobRow.removeFromLeft (16);
        for (auto* k : eqGlobalKnobs) placeKnob (k, knobRow.removeFromLeft (kw));
    }

    // Panel ducker
    {
        auto r = duckPanel.reduced (12);
        duckOnButton.setBounds (r.removeFromTop (24).removeFromLeft (120));
        r.removeFromTop (6);
        duckMeterArea = r.removeFromBottom (84);
        const int rowH = r.getHeight() / 2, colW = r.getWidth() / 2;
        for (int i = 0; i < 4; ++i)
            placeKnob (duckKnobs[(size_t) i], { r.getX() + (i % 2) * colW, r.getY() + (i / 2) * rowH, colW, juce::jmin (rowH, 130) });
    }
}
