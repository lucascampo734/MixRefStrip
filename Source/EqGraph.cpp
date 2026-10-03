#include "EqGraph.h"

namespace
{
    const juce::Colour bgCol    { 0xff0d0e12 };
    const juce::Colour gridCol  { 0xff23262f };
    const juce::Colour labelCol { 0xff6b7080 };
    const juce::Colour curveCol { 0xfff5a524 };
    const juce::Colour specCol  { 0xff5b6275 };
}

EqGraph::EqGraph (MixRefProcessor& p) : proc (p)
{
    nodes = {{
        { "Pasa altos", "hpf",      nullptr,    nullptr, juce::Colour (0xfff59e0b), 0.0f },
        { "Barro",      "mudFreq",  "mudGain",  "mudQ",  juce::Colour (0xfff87171), 0.0f },
        { "Presencia",  "presFreq", "presGain", nullptr, juce::Colour (0xff4ade80), 0.0f },
        { "Aire",       "airFreq",  "airGain",  nullptr, juce::Colour (0xff38bdf8), 0.0f },
        { "Mono",       "monoFreq", nullptr,    nullptr, juce::Colour (0xffa78bfa), -14.0f },
    }};
    setOpaque (true);
}

//==============================================================================
double EqGraph::sampleRate() const
{
    const double sr = proc.getSampleRate();
    return sr > 0.0 ? sr : 48000.0;
}

juce::Rectangle<float> EqGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (16.0f).withTrimmedLeft (26.0f);
}

float EqGraph::xForFreq (float f) const
{
    auto r = plotArea();
    const float t = std::log (f / minFreq) / std::log (maxFreq / minFreq);
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

float EqGraph::getParam (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void EqGraph::setParam (const char* id, float value)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqGraph::beginGesture (int i)
{
    for (auto* id : { nodes[(size_t) i].freqId, nodes[(size_t) i].gainId })
        if (id != nullptr)
            if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
}

void EqGraph::endGesture (int i)
{
    for (auto* id : { nodes[(size_t) i].freqId, nodes[(size_t) i].gainId })
        if (id != nullptr)
            if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
}

bool EqGraph::nodeVisible (int i) const
{
    if (i == 4) return getParam ("monoOn") > 0.5f;
    return true;
}

juce::Point<float> EqGraph::nodePosition (int i) const
{
    const auto& n = nodes[(size_t) i];
    const float x = xForFreq (getParam (n.freqId));
    const float db = n.gainId != nullptr ? getParam (n.gainId) : n.fixedDb;
    return { x, yForDb (db) };
}

int EqGraph::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 12.0f;
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        if (! nodeVisible (i)) continue;
        const float d = nodePosition (i).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
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
    const double sr = sampleRate();
    const bool eqOn = getParam ("eqOn") > 0.5f;

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
            int b0 = juce::jlimit (1, (int) spectrumDb.size() - 1, (int) (f0 / binHz));
            int b1 = juce::jlimit (b0, (int) spectrumDb.size() - 1, (int) (f1 / binHz));
            float db = specMinDb;
            for (int b = b0; b <= b1; ++b) db = juce::jmax (db, spectrumDb[(size_t) b]);
            // inclinación de 4.5 dB/oct para que se lea parecido a como suena
            db += 4.5f * std::log2 (juce::jmax (f0, 20.0f) / 1000.0f);
            spec.lineTo (x, yForSpectrumDb (db));
        }
        spec.lineTo (r.getRight(), r.getBottom());
        spec.closeSubPath();
        g.setColour (specCol.withAlpha (0.45f));
        g.fillPath (spec);
    }

    // Línea de graves mono
    if (getParam ("monoOn") > 0.5f)
    {
        const float x = xForFreq (getParam ("monoFreq"));
        g.setColour (nodes[4].colour.withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (r.getX(), r.getY(), x, r.getBottom()));
        g.setColour (nodes[4].colour.withAlpha (0.6f));
        const float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine ({ x, r.getY(), x, r.getBottom() }, dashes, 2, 1.0f);
    }

    // Curva de respuesta
    {
        using C = juce::dsp::IIR::Coefficients<float>;
        const float nyq = (float) sr * 0.45f;
        auto lim = [nyq] (float f) { return juce::jmin (f, nyq); };
        std::array<C::Ptr, 5> c {
            C::makeHighPass (sr, lim (getParam ("hpf")), 0.5412f),
            C::makeHighPass (sr, lim (getParam ("hpf")), 1.3066f),
            C::makePeakFilter (sr, lim (getParam ("mudFreq")), getParam ("mudQ"), juce::Decibels::decibelsToGain (getParam ("mudGain"))),
            C::makePeakFilter (sr, lim (getParam ("presFreq")), 0.8f, juce::Decibels::decibelsToGain (getParam ("presGain"))),
            C::makeHighShelf (sr, lim (getParam ("airFreq")), 0.707f, juce::Decibels::decibelsToGain (getParam ("airGain")))
        };

        juce::Path curve;
        bool first = true;
        for (float x = r.getX(); x <= r.getRight(); x += 1.0f)
        {
            const double f = freqForX (x);
            double mag = 1.0;
            if (eqOn)
                for (auto& co : c) mag *= co->getMagnitudeForFrequency (f, sr);
            const float db = juce::jlimit (-rangeDb * 1.5f, rangeDb * 1.5f, (float) juce::Decibels::gainToDecibels (mag, -60.0));
            const float y = juce::jlimit (r.getY(), r.getBottom(), yForDb (db));
            if (first) { curve.startNewSubPath (x, y); first = false; } else curve.lineTo (x, y);
        }

        juce::Path fill (curve);
        fill.lineTo (r.getRight(), yForDb (0.0f));
        fill.lineTo (r.getX(), yForDb (0.0f));
        fill.closeSubPath();

        const auto col = eqOn ? curveCol : labelCol;
        g.setColour (col.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
    }

    // Nodos
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        if (! nodeVisible (i)) continue;
        const auto& n = nodes[(size_t) i];
        const auto pos = nodePosition (i);
        const bool active = (i == hoverNode || i == dragNode);
        const float rad = active ? 8.0f : 6.5f;
        auto col = (eqOn || i == 4) ? n.colour : labelCol;

        g.setColour (col.withAlpha (active ? 1.0f : 0.85f));
        g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2, rad * 2);
        g.setColour (bgCol);
        g.drawText (juce::String (i == 4 ? "M" : juce::String (i + 1)),
                    juce::Rectangle<float> (pos.x - rad, pos.y - rad, rad * 2, rad * 2), juce::Justification::centred);
    }

    // Etiqueta del nodo activo
    const int shown = dragNode >= 0 ? dragNode : hoverNode;
    if (shown >= 0)
    {
        const auto& n = nodes[(size_t) shown];
        juce::String txt = utf8 (n.name) + "  " + proc.apvts.getParameter (n.freqId)->getCurrentValueAsText();
        if (n.gainId != nullptr) txt << "  " << proc.apvts.getParameter (n.gainId)->getCurrentValueAsText();
        if (n.qId != nullptr)    txt << "  Q " << proc.apvts.getParameter (n.qId)->getCurrentValueAsText();

        g.setFont (juce::FontOptions (12.0f));
        const int w = (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 16;
        auto pos = nodePosition (shown);
        auto box = juce::Rectangle<int> ((int) pos.x - w / 2, (int) pos.y - 34, w, 20)
                       .constrainedWithin (r.toNearestInt().reduced (2));
        g.setColour (juce::Colour (0xee1c1e25));
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (n.colour);
        g.drawRoundedRectangle (box.toFloat(), 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.drawText (txt, box, juce::Justification::centred);
    }

    if (! eqOn)
    {
        g.setColour (labelCol);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText ("EQ desactivado", r.reduced (8).toNearestInt(), juce::Justification::topRight);
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
    if (dragNode >= 0) beginGesture (dragNode);
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    const auto& n = nodes[(size_t) dragNode];
    auto r = plotArea();
    const float x = juce::jlimit (r.getX(), r.getRight(), e.position.x);
    setParam (n.freqId, freqForX (x));
    if (n.gainId != nullptr)
        setParam (n.gainId, dbForY (juce::jlimit (r.getY(), r.getBottom(), e.position.y)));
    repaint();
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0) endGesture (dragNode);
    dragNode = -1;
    repaint();
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doble clic: la ganancia del nodo vuelve a 0 dB
    const int n = nodeAt (e.position);
    if (n >= 0 && nodes[(size_t) n].gainId != nullptr)
        if (auto* p = proc.apvts.getParameter (nodes[(size_t) n].gainId))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
            p->endChangeGesture();
        }
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Rueda sobre el nodo de barro: cambia el Q
    const int n = nodeAt (e.position);
    if (n >= 0 && nodes[(size_t) n].qId != nullptr)
        if (auto* p = proc.apvts.getParameter (nodes[(size_t) n].qId))
        {
            const float q = getParam (nodes[(size_t) n].qId) * (1.0f + w.deltaY * 0.5f);
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (q));
            p->endChangeGesture();
            repaint();
        }
}
