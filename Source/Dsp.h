// SUNRIZE · REZONANZA
// Small DSP toolkit mirroring the Web Audio nodes the browser prototype used.
#pragma once
#include <cmath>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace sz
{
constexpr float kPi = 3.14159265358979f;
enum FType { LP, HP, BP, HSHELF };

struct Rng
{
    uint32_t s = 0x2545f491u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 8388608.0f - 1.0f; }
    inline float uni() { return (next() + 1.0f) * 0.5f; }
};

// RBJ biquad with Web Audio parameter meaning (LP/HP Q in dB, shelf gain in dB, shelf slope 1)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set (FType type, float f, float q, float sr, float gainDb = 0)
    {
        f = std::clamp (f, 10.0f, sr * 0.49f);
        const float w0 = 2 * kPi * f / sr, cw = std::cos (w0), sw = std::sin (w0);
        float nb0, nb1, nb2, na0, na1, na2;
        if (type == HSHELF)
        {
            const float A = std::pow (10.0f, gainDb / 40.0f);
            const float alpha = sw / 2.0f * std::sqrt ((A + 1 / A) * (1 / 1.0f - 1) + 2);
            const float k = 2 * std::sqrt (A) * alpha;
            nb0 = A * ((A + 1) + (A - 1) * cw + k);
            nb1 = -2 * A * ((A - 1) + (A + 1) * cw);
            nb2 = A * ((A + 1) + (A - 1) * cw - k);
            na0 = (A + 1) - (A - 1) * cw + k;
            na1 = 2 * ((A - 1) - (A + 1) * cw);
            na2 = (A + 1) - (A - 1) * cw - k;
        }
        else
        {
            float alpha;
            if (type == BP) { alpha = sw / (2 * std::max (0.0001f, q)); nb0 = alpha; nb1 = 0; nb2 = -alpha; }
            else
            {
                alpha = sw / (2 * std::pow (10.0f, q / 20.0f));
                if (type == LP) { nb0 = (1 - cw) * 0.5f; nb1 = 1 - cw; nb2 = (1 - cw) * 0.5f; }
                else            { nb0 = (1 + cw) * 0.5f; nb1 = -(1 + cw); nb2 = (1 + cw) * 0.5f; }
            }
            na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
        }
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
    inline float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Web Audio setTargetAtTime-style smoother
struct Target
{
    float v = 0, target = 0, coef = 0;
    void setTau (float tau, float sr) { coef = tau <= 0 ? 0 : std::exp (-1.0f / (tau * sr)); }
    void to (float t, float tau, float sr) { target = t; setTau (tau, sr); }
    inline float next() { v = target + (v - target) * coef; return v; }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

enum Wave { SINE, TRI, SQUARE, SAW };
inline float osc (Wave w, float p, float inc)
{
    switch (w)
    {
        case SINE: return std::sin (2 * kPi * p);
        case TRI: return 1.0f - 4.0f * std::fabs (p - 0.5f);
        case SQUARE: return (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, inc) - polyBlep (std::fmod (p + 0.5f, 1.0f), inc);
        default: return 2.0f * p - 1.0f - polyBlep (p, inc);
    }
}

// Chromium DynamicsCompressor static curve + simple envelope, auto make-up, 6 ms look-ahead
struct Compressor
{
    float thr = -24, ratio = 12, knee = 30, attack = 0.003f, release = 0.25f, sr = 44100;
    float linThr = 1, kneeThr = 1, k = 1, kneeDb = 0, makeup = 1, env = 0, aC = 0, rC = 0;
    std::vector<float> buf[2]; int w = 0, len = 1;

    static float kneeCurve (float x, float k, float lt) { return x < lt ? x : lt + (1.0f - std::exp (-k * (x - lt))) / k; }
    static float db (float x) { return 20.0f * std::log10 (std::max (1e-9f, x)); }
    static float lin (float d) { return std::pow (10.0f, d / 20.0f); }
    float saturate (float x) const
    {
        if (x < kneeThr) return kneeCurve (x, k, linThr);
        return lin (kneeDb + (db (x) - (thr + knee)) / ratio);
    }
    void prepare (float sampleRate, float threshold, float rat, float kn, float att, float rel)
    {
        sr = sampleRate; thr = threshold; ratio = rat; knee = kn; attack = att; release = rel;
        linThr = lin (thr); kneeThr = lin (thr + knee);
        auto slopeAt = [this] (float x, float kk) {
            const float x2 = x * 1.001f;
            return (db (kneeCurve (x2, kk, linThr)) - db (kneeCurve (x, kk, linThr))) / (db (x2) - db (x));
        };
        float minK = 0.1f, maxK = 10000.0f;
        for (int i = 0; i < 15; ++i) { k = std::sqrt (minK * maxK); if (slopeAt (kneeThr, k) < 1.0f / ratio) maxK = k; else minK = k; }
        kneeDb = db (kneeCurve (kneeThr, k, linThr));
        makeup = std::pow (1.0f / saturate (1.0f), 0.6f);
        aC = std::exp (-1.0f / (attack * sr)); rC = std::exp (-1.0f / (release * sr));
        len = std::max (1, (int) std::round (0.006f * sr));
        for (auto& b : buf) b.assign ((size_t) len, 0.0f);
        w = 0; env = 0;
    }
    inline void process (float& l, float& r)
    {
        const float lvl = std::max (std::fabs (l), std::fabs (r));
        const float gr = lvl > 1e-6f ? db (saturate (lvl) / lvl) : 0.0f;
        env = gr < env ? aC * env + (1 - aC) * gr : rC * env + (1 - rC) * gr;
        const float g = lin (env) * makeup;
        const float dl = buf[0][(size_t) w], dr = buf[1][(size_t) w];
        buf[0][(size_t) w] = l; buf[1][(size_t) w] = r;
        w = (w + 1) % len;
        l = dl * g; r = dr * g;
    }
};
} // namespace sz
