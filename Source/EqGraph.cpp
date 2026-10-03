#include "EqGraph.h"

namespace
{
    const juce::Colour bgCol    { 0xff0d0e12 };
    const juce::Colour gridCol  { 0xff23262f };
    const juce::Colour labelCol { 0xff6b7080 };
    const juce::Colour curveCol { 0xfff5a524 };
    const juce::Colour specCol  { 0xff5b6275 };
}

juce::Colour EqGraph::bandColour (int band)
{
    static const std::array<juce::Colour, 8> cols {
        juce::Colour (0xfff59e0b), juce::Colour (0xfffb923c), juce::Colour (0xfff87171), juce::Colour (0xfff472b6),
        juce::Colour (0xff4ade80), juce::Colour (0xff2dd4bf), juce::Colour (0xff38bdf8), juce::Colour (0xffa78bfa) };
    return cols[(size_t) juce::jlimit (0, 7, band)];
}

EqGraph::EqGraph (MixRefProcessor& p) : proc (p)
{
    setOpaque (true);
}

//==============================================================================
juce::Rectangle<float> EqGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (16.0f).withTrimmedLeft (26.0f);
}

float EqGraph::xForFreq (float f) const
{
    auto r = plotArea();
    const float t = std::log (juce::jmax (f, 1.0f) / minFreq) / std::log (maxFreq / minFreq);
    return r.getX() + t * r.getWidth();
}

float EqGraph::freqForX (float x) const
{
    auto r = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    return minFreq * std::pow (maxFreq / minFreq, t);
}

float EqGraph::yForDb (float db) const
{
    auto r = plotArea();
    return r.getCentreY() - (db / rangeDb) * (r.getHeight() * 0.5f);
}

float EqGraph::dbForY (float y) const
{
    auto r = plotArea();
    return (r.getCentreY() - y) / (r.getHeight() * 0.5f) * rangeDb;
}

float EqGraph::yForSpectrumDb (float db) const
{
    auto r = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (db - specMinDb) / (specMaxDb - specMinDb));
    return r.getBottom() - t * r.getHeight();
}

float EqGraph::getParam (const juce::String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void EqGraph::setParam (const juce::String& id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqGraph::gesture (int band, bool begin)
{
    for (auto* what : { "on", "freq", "gain" })
        if (auto* p = proc.apvts.getParameter (bandId (editSet, band, what)))
        {
            if (begin) p->beginChangeGesture();
            else       p->endChangeGesture();
        }
}

juce::Point<float> EqGraph::nodePosition (int band) const
{
    const auto b = proc.readBand (editSet, band);
    const float db = eq::hasGain (b.type) ? b.gain : 0.0f;
    return { xForFreq (b.freq), yForDb (db) };
}

int EqGraph::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 12.0f;
    // primero la seleccionada, para poder agarrarla aunque se superponga
    for (int k = 0; k < eq::numBands; ++k)
    {
        const int i = (k + selectedBand) % eq::numBands;
        const float d = nodePosition (i).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

double EqGraph::responseDb (int set, double freq, int onlyBand) const
{
    const double eqSr = proc.eqSampleRate();
    const bool adapt = getParam ("adaptQ") > 0.5f;
    const double scale = getParam ("scale") / 100.0;

    double mag = 1.0;
    for (int b = 0; b < eq::numBands; ++b)
    {
        if (onlyBand >= 0 && b != onlyBand) continue;
        const auto st = eq::compute (proc.readBand (set, b), eqSr, adapt, scale);
        for (int s = 0; s < st.n; ++s) mag *= eq::magnitude (st.c[(size_t) s], freq, eqSr);
    }
    return juce::Decibels::gainToDecibels (mag, -80.0) + getParam ("eqOut");
}

//==============================================================================
void EqGraph::updateSpectrum()
{
    const int got = proc.pullAnalyserSamples (incoming.data(), (int) incoming.size());
    for (int k = 0; k < got; ++k)
    {
        ring[(size_t) ringPos] = incoming[(size_t) k];
        ringPos = (ringPos + 1) % fftSize;
    }

    if (got > 0)
    {
        for (int k = 0; k < fftSize; ++k)
            fftData[(size_t) k] = ring[(size_t) ((ringPos + k) % fftSize)];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 4.0f / (float) fftSize; // 2/N y compensación de la ventana Hann
        for (size_t b = 0; b < spectrumDb.size(); ++b)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[b] * norm, specMinDb);
            spectrumDb[b] = db > spectrumDb[b] ? db : juce::jmax (db, spectrumDb[b] - 1.5f);
        }
    }
    else
    {
        for (auto& v : spectrumDb) v = juce::jmax (specMinDb, v - 1.5f);
    }
}

//==============================================================================
void EqGraph::paint (juce::Graphics& g)
{
    g.fillAll (bgCol);
    auto r = plotArea();
    const bool eqOn = getParam ("eqOn") > 0.5f;
    const int mode = proc.eqMode();
    const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;

    // Grilla de frecuencias
    g.setFont (juce::FontOptions (10.0f));
    const std::array<float, 10> freqs { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto f : freqs)
    {
        const float x = xForFreq (f);
        g.setColour (gridCol);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (labelCol);
        const juce::String txt = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
        if (f >= maxFreq)
            g.drawText (txt, (int) x - 36, (int) r.getBottom() + 2, 34, 12, juce::Justification::centredRight);
        else
            g.drawText (txt, (int) x - 18, (int) r.getBottom() + 2, 36, 12, juce::Justification::centred);
    }

    // Grilla de dB
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = yForDb (db);
        g.setColour (std::abs (db) < 0.01f ? gridCol.brighter (0.25f) : gridCol);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (labelCol);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), 0, (int) y - 6, 22, 12, juce::Justification::centredRight);
    }

    // Analizador de espectro (salida del plugin)
    {
        juce::Path spec;
        spec.startNewSubPath (r.getX(), r.getBottom());
        const float binHz = (float) sr / (float) fftSize;
        for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
        {
            const float f0 = freqForX (x), f1 = freqForX (x + 2.0f);
            const int b0 = juce::jlimit (1, (int) spectrumDb.size() - 1, (int) (f0 / binHz));
            const int b1 = juce::jlimit (b0, (int) spectrumDb.size() - 1, (int) (f1 / binHz));
            float db = specMinDb;
            for (int b = b0; b <= b1; ++b) db = juce::jmax (db, spectrumDb[(size_t) b]);
            if (db > specMinDb + 1.0f) // sin señal, no levantar el piso
                db += 4.5f * std::log2 (juce::jmax (f0, 20.0f) / 1000.0f);
            spec.lineTo (x, yForSpectrumDb (db));
        }
        spec.lineTo (r.getRight(), r.getBottom());
        spec.closeSubPath();
        g.setColour (specCol.withAlpha (0.45f));
        g.fillPath (spec);
    }

    // Zona de graves mono
    if (getParam ("monoOn") > 0.5f)
    {
        const float x = xForFreq (getParam ("monoFreq"));
        const auto monoCol = juce::Colour (0xffa78bfa);
        g.setColour (monoCol.withAlpha (0.10f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), r.getY(), x, r.getBottom()));
        g.setColour (monoCol.withAlpha (0.5f));
        const float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine ({ x, r.getY(), x, r.getBottom() }, dashes, 2, 1.0f);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("mono", (int) x + 3, (int) r.getY() + 2, 40, 12, juce::Justification::centredLeft);
    }

    auto makeCurve = [&] (int set, int onlyBand)
    {
        juce::Path curve;
        bool first = true;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            const float db = (float) responseDb (set, freqForX (x), onlyBand);
            const float y = juce::jlimit (r.getY(), r.getBottom(), yForDb (db));
            if (first) { curve.startNewSubPath (x, y); first = false; }
            else curve.lineTo (x, y);
        }
        return curve;
    };

    auto fillToZero = [&] (const juce::Path& curve)
    {
        juce::Path fill (curve);
        fill.lineTo (r.getRight(), yForDb (0.0f));
        fill.lineTo (r.getX(), yForDb (0.0f));
        fill.closeSubPath();
        return fill;
    };

    // Curva del otro canal (L/R o Mid/Side), tenue
    if (eqOn && mode != 0)
    {
        g.setColour (labelCol.withAlpha (0.7f));
        g.strokePath (makeCurve (1 - editSet, -1), juce::PathStrokeType (1.2f));
    }

    // Banda seleccionada, sombreada con su color
    const auto sel = proc.readBand (editSet, selectedBand);
    if (eqOn && sel.on)
    {
        g.setColour (bandColour (selectedBand).withAlpha (0.18f));
        g.fillPath (fillToZero (makeCurve (editSet, selectedBand)));
    }

    // Curva total del canal que se edita
    {
        const auto curve = makeCurve (editSet, -1);
        const auto col = eqOn ? curveCol : labelCol;
        g.setColour (col.withAlpha (0.10f));
        g.fillPath (fillToZero (curve));
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    // Nodos: lleno = banda activa, aro = apagada
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int k = 0; k < eq::numBands; ++k)
    {
        const int i = (k + selectedBand + 1) % eq::numBands;   // la seleccionada se dibuja arriba
        const auto b = proc.readBand (editSet, i);
        const auto pos = nodePosition (i);
        const bool active = (i == hoverNode || i == dragNode);
        const bool selected = (i == selectedBand);
        const float rad = active || selected ? 8.0f : 6.5f;
        const auto col = eqOn ? bandColour (i) : labelCol;
        const auto box = juce::Rectangle<float> (pos.x - rad, pos.y - rad, rad * 2, rad * 2);

        if (b.on)
        {
            g.setColour (col);
            g.fillEllipse (box);
            g.setColour (bgCol);
        }
        else
        {
            g.setColour (bgCol);
            g.fillEllipse (box);
            g.setColour (col.withAlpha (0.8f));
            g.drawEllipse (box.reduced (0.75f), 1.5f);
        }
        g.drawText (juce::String (i + 1), box, juce::Justification::centred);

        if (selected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawEllipse (box.expanded (2.5f), 1.2f);
        }
    }

    // Etiqueta del nodo bajo el mouse
    const int shown = dragNode >= 0 ? dragNode : hoverNode;
    if (shown >= 0)
    {
        const auto b = proc.readBand (editSet, shown);
        auto text = [this, shown] (const char* what) { return proc.apvts.getParameter (bandId (editSet, shown, what))->getCurrentValueAsText(); };
        juce::String txt = juce::String (shown + 1) + ". " + getEqTypeNames()[b.type] + "  " + text ("freq");
        if (eq::hasGain (b.type)) txt << "  " << text ("gain");
        txt << "  Q " << text ("q");
        if (! b.on) txt << "  (apagada)";

        g.setFont (juce::FontOptions (12.0f));
        const int w = (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 16;
        auto pos = nodePosition (shown);
        auto box = juce::Rectangle<int> ((int) pos.x - w / 2, (int) pos.y - 34, w, 20)
                       .constrainedWithin (r.toNearestInt().reduced (2));
        g.setColour (juce::Colour (0xee1c1e25));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (bandColour (shown));
        g.drawRoundedRectangle (box.toFloat(), 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.drawText (txt, box, juce::Justification::centred);
    }

    // Avisos arriba a la derecha
    juce::StringArray notes;
    if (! eqOn) notes.add ("EQ desactivado");
    if (mode == 1) notes.add (editSet == 0 ? "Editando: L" : "Editando: R");
    if (mode == 2) notes.add (editSet == 0 ? "Editando: Mid" : "Editando: Side");
    if (eqOn && getParam ("audition") > 0.5f) notes.add (utf8 ("Audición: mantené apretado un punto"));
    if (eqOn && getParam ("delta") > 0.5f) notes.add ("Delta: solo lo que cambia el EQ");
    if (! notes.isEmpty())
    {
        g.setColour (labelCol.brighter (0.4f));
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (notes.joinIntoString ("   |   "), r.reduced (8, 6).toNearestInt(), juce::Justification::topRight);
    }

    g.setColour (gridCol.brighter (0.2f));
    g.drawRect (getLocalBounds());
}

//==============================================================================
void EqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n != hoverNode)
    {
        hoverNode = n;
        setMouseCursor (n >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqGraph::mouseExit (const juce::MouseEvent&)
{
    hoverNode = -1;
    repaint();
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    dragNode = nodeAt (e.position);
    dragTurnedOn = false;
    if (dragNode < 0) return;

    selectedBand = dragNode;
    if (onBandSelected) onBandSelected (dragNode);
    gesture (dragNode, true);

    if (getParam ("audition") > 0.5f)
        proc.auditionBand = editSet * eq::numBands + dragNode;
    repaint();
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;

    // Arrastrar una banda apagada la prende (como en el EQ Eight)
    if (! dragTurnedOn && e.getDistanceFromDragStart() > 3 && ! proc.readBand (editSet, dragNode).on)
    {
        setParam (bandId (editSet, dragNode, "on"), 1.0f);
        dragTurnedOn = true;
    }

    auto r = plotArea();
    setParam (bandId (editSet, dragNode, "freq"), freqForX (juce::jlimit (r.getX(), r.getRight(), e.position.x)));
    if (eq::hasGain (proc.readBand (editSet, dragNode).type))
        setParam (bandId (editSet, dragNode, "gain"), dbForY (juce::jlimit (r.getY(), r.getBottom(), e.position.y)));
    repaint();
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0) gesture (dragNode, false);
    dragNode = -1;
    proc.auditionBand = -1;
    repaint();
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doble clic: la ganancia del nodo vuelve a 0 dB
    const int n = nodeAt (e.position);
    if (n >= 0)
        if (auto* p = proc.apvts.getParameter (bandId (editSet, n, "gain")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
            p->endChangeGesture();
        }
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Rueda sobre un nodo: cambia el Q
    const int n = nodeAt (e.position);
    if (n < 0) return;
    if (auto* p = proc.apvts.getParameter (bandId (editSet, n, "q")))
    {
        const float q = getParam (bandId (editSet, n, "q")) * (1.0f + w.deltaY * 0.5f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (q));
        p->endChangeGesture();
        repaint();
    }
}
