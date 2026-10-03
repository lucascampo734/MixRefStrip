#pragma once

// Filtros del EQ de 8 bandas (formulas RBJ "Audio EQ Cookbook").
// Lo usan el procesador de audio y el grafico, asi la curva que se ve
// es exactamente la que suena.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace eq
{
    enum Type
    {
        LowCut48 = 0, LowCut12, LowShelf, Bell, Notch, HighShelf, HighCut12, HighCut48
    };

    constexpr int numTypes = 8;
    constexpr int numBands = 8;
    constexpr double pi = 3.14159265358979323846;

    inline bool hasGain (int type) { return type == LowShelf || type == Bell || type == HighShelf; }

    struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    struct Biquad
    {
        Coefs c;
        double z1 = 0, z2 = 0;

        inline float process (float x) noexcept
        {
            const double in = x;
            const double y = c.b0 * in + z1;
            z1 = c.b1 * in - c.a1 * y + z2;
            z2 = c.b2 * in - c.a2 * y;
            return (float) y;
        }

        void reset() noexcept { z1 = z2 = 0; }
    };

    struct BandSettings
    {
        bool on = false;
        int type = Bell;
        float freq = 1000.0f, gain = 0.0f, q = 0.71f;
    };

    struct Stages
    {
        int n = 0;
        std::array<Coefs, 4> c;
    };

    //==========================================================================
    inline Coefs norm (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    struct W { double cw, al; };
    inline W omega (double sr, double f, double q)
    {
        const double w = 2.0 * pi * f / sr;
        return { std::cos (w), std::sin (w) / (2.0 * q) };
    }

    inline Coefs lowpass (double sr, double f, double q)
    {
        auto [cw, al] = omega (sr, f, q);
        return norm ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + al, -2 * cw, 1 - al);
    }

    inline Coefs highpass (double sr, double f, double q)
    {
        auto [cw, al] = omega (sr, f, q);
        return norm ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + al, -2 * cw, 1 - al);
    }

    inline Coefs bandpass (double sr, double f, double q)   // 0 dB en el pico
    {
        auto [cw, al] = omega (sr, f, q);
        return norm (al, 0, -al, 1 + al, -2 * cw, 1 - al);
    }

    inline Coefs notch (double sr, double f, double q)
    {
        auto [cw, al] = omega (sr, f, q);
        return norm (1, -2 * cw, 1, 1 + al, -2 * cw, 1 - al);
    }

    inline Coefs peak (double sr, double f, double q, double gainDb)
    {
        auto [cw, al] = omega (sr, f, q);
        const double A = std::pow (10.0, gainDb / 40.0);
        return norm (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
    }

    inline Coefs lowShelf (double sr, double f, double q, double gainDb)
    {
        auto [cw, al] = omega (sr, f, q);
        const double A = std::pow (10.0, gainDb / 40.0), s = 2.0 * std::sqrt (A) * al;
        return norm (A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - s),
                     (A + 1) + (A - 1) * cw + s, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s);
    }

    inline Coefs highShelf (double sr, double f, double q, double gainDb)
    {
        auto [cw, al] = omega (sr, f, q);
        const double A = std::pow (10.0, gainDb / 40.0), s = 2.0 * std::sqrt (A) * al;
        return norm (A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - s),
                     (A + 1) - (A - 1) * cw + s, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s);
    }

    inline double magnitude (const Coefs& c, double f, double sr)
    {
        const double w = 2.0 * pi * f / sr;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
        const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
        return std::abs (num / den);
    }

    //==========================================================================
    // Q efectivo de una campana con Adaptive Q: se cierra cuando sube la ganancia
    inline double effectiveQ (const BandSettings& b, bool adaptiveQ, double gainDb)
    {
        const double q = std::clamp ((double) b.q, 0.1, 18.0);
        return adaptiveQ ? q * (1.0 + 0.05 * std::abs (gainDb)) : q;
    }

    // Etapas de filtro de una banda. scale = 1.0 es 100 %.
    inline Stages compute (const BandSettings& b, double sr, bool adaptiveQ, double scale)
    {
        Stages s;
        if (! b.on) return s;

        const double f = std::clamp ((double) b.freq, 10.0, sr * 0.45);
        const double q = std::clamp ((double) b.q, 0.1, 18.0);
        const double g = b.gain * scale;

        // Butterworth de 8vo orden; la ultima etapa lleva la resonancia
        const std::array<double, 4> bw { 0.5098, 0.6013, 0.9000, std::max (0.3, 2.5629 * q / 0.7071) };

        switch (b.type)
        {
            case LowCut48:  s.n = 4; for (int i = 0; i < 4; ++i) s.c[(size_t) i] = highpass (sr, f, bw[(size_t) i]); break;
            case LowCut12:  s.n = 1; s.c[0] = highpass (sr, f, q); break;
            case LowShelf:  s.n = 1; s.c[0] = lowShelf (sr, f, std::clamp (q, 0.3, 2.0), g); break;
            case Bell:      s.n = 1; s.c[0] = peak (sr, f, effectiveQ (b, adaptiveQ, g), g); break;
            case Notch:     s.n = 1; s.c[0] = notch (sr, f, q); break;
            case HighShelf: s.n = 1; s.c[0] = highShelf (sr, f, std::clamp (q, 0.3, 2.0), g); break;
            case HighCut12: s.n = 1; s.c[0] = lowpass (sr, f, q); break;
            case HighCut48: s.n = 4; for (int i = 0; i < 4; ++i) s.c[(size_t) i] = lowpass (sr, f, bw[(size_t) i]); break;
            default: break;
        }
        return s;
    }

    // Filtro del auricular: deja sonar solo la zona que toca la banda.
    // En los cortes y shelves se escucha justo lo que se saca o se agrega.
    inline Stages audition (const BandSettings& b, double sr, bool adaptiveQ, double scale)
    {
        Stages s;
        const double f = std::clamp ((double) b.freq, 10.0, sr * 0.45);
        const double q = std::clamp ((double) b.q, 0.1, 18.0);

        switch (b.type)
        {
            case Bell:      s.n = 1; s.c[0] = bandpass (sr, f, effectiveQ (b, adaptiveQ, b.gain * scale)); break;
            case Notch:     s.n = 1; s.c[0] = bandpass (sr, f, q); break;
            case LowShelf:  s.n = 1; s.c[0] = lowpass (sr, f, 0.7071); break;
            case HighShelf: s.n = 1; s.c[0] = highpass (sr, f, 0.7071); break;
            case LowCut48:
            case LowCut12:  s.n = 2; s.c[0] = s.c[1] = lowpass (sr, f, 0.7071); break;
            case HighCut12:
            case HighCut48: s.n = 2; s.c[0] = s.c[1] = highpass (sr, f, 0.7071); break;
            default: break;
        }
        return s;
    }
}
