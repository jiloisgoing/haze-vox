#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>

// =============================================================================
// Small, self-contained DSP blocks used by Haze Vox v2.
// =============================================================================
namespace haze
{
constexpr float twoPi = 6.283185307179586f;

inline float dbToGain (float db)   { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)    { return 20.0f * std::log10 (std::max (g, 1.0e-6f)); }
inline float onePoleCoef (double sr, float ms) { return 1.0f - std::exp (-1.0f / (float) (sr * ms * 0.001)); }

// Linear-interpolated read from a circular buffer.
inline float readFrac (const std::vector<float>& buf, int writeIdx, float delay)
{
    const int n = (int) buf.size();
    float rp = (float) writeIdx - delay;
    while (rp < 0.0f) rp += (float) n;
    const int i0 = (int) rp;
    const int i1 = (i0 + 1) % n;
    const float f = rp - (float) i0;
    return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
}

// -----------------------------------------------------------------------------
// Vocal leveler ("Smooth"): soft-knee feed-forward compressor with auto makeup.
// -----------------------------------------------------------------------------
class Leveler
{
public:
    void prepare (double sampleRate)
    {
        atk = onePoleCoef (sampleRate, 6.0f);
        rel = onePoleCoef (sampleRate, 140.0f);
        envDb = -120.0f; grDb = 0.0f;
    }
    void setAmount (float a) { amount = a; }
    float getGainReductionDb() const { return grDb; }

    // Returns the gain to apply for this sample, given the linked detector level.
    float process (float detector)
    {
        if (amount <= 0.001f) { grDb = 0.0f; return 1.0f; }
        const float threshold = -6.0f - amount * 26.0f;   // -6 .. -32 dBFS
        const float ratio = 3.0f, knee = 8.0f;
        const float inDb = gainToDb (detector);
        envDb += (inDb > envDb ? atk : rel) * (inDb - envDb);

        const float over = envDb - threshold;
        float target = 0.0f;
        if (over > knee * 0.5f)       target = over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) target = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        grDb = target;

        const float makeup = amount * 26.0f * (1.0f - 1.0f / ratio) * 0.45f;
        return dbToGain (makeup - grDb);
    }

private:
    float amount = 0.0f, atk = 0.0f, rel = 0.0f, envDb = -120.0f, grDb = 0.0f;
};

// -----------------------------------------------------------------------------
// Split-band de-esser. Compares the energy above ~6 kHz to the full-band energy,
// so it reacts to "s" sounds regardless of how loud the vocal is.
// -----------------------------------------------------------------------------
class DeEsser
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        split.prepare (spec);
        split.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        split.setCutoffFrequency (6000.0f);
        atk = onePoleCoef (spec.sampleRate, 1.5f);
        rel = onePoleCoef (spec.sampleRate, 60.0f);
        envHi = envAll = 0.0f; grDb = 0.0f;
        lo.assign (spec.numChannels, 0.0f);
        hi.assign (spec.numChannels, 0.0f);
    }
    void setAmount (float a) { amount = a; }
    float getGainReductionDb() const { return grDb; }

    // Process one frame (all channels) in place.
    void processFrame (float* frame, int numCh)
    {
        float aHi = 0.0f, aAll = 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            split.processSample (c, frame[c], lo[(size_t) c], hi[(size_t) c]);
            aHi  = std::max (aHi,  std::abs (hi[(size_t) c]));
            aAll = std::max (aAll, std::abs (frame[c]));
        }
        envHi  += (aHi  > envHi  ? atk : rel) * (aHi  - envHi);
        envAll += (aAll > envAll ? atk : rel) * (aAll - envAll);

        float g = 1.0f;
        if (amount > 0.001f && envAll > 1.0e-4f)
        {
            const float ratioDb = gainToDb (envHi / envAll);  // how dominant the highs are
            const float over = ratioDb - (-9.0f);
            grDb = over > 0.0f ? std::min (over * 1.2f, 2.0f + amount * 8.0f) : 0.0f;
            g = dbToGain (-grDb);
        }
        else grDb = 0.0f;

        for (int c = 0; c < numCh; ++c)
            frame[c] = lo[(size_t) c] + hi[(size_t) c] * g;
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> split;
    std::vector<float> lo, hi;
    float amount = 0.0f, atk = 0.0f, rel = 0.0f, envHi = 0.0f, envAll = 0.0f, grDb = 0.0f;
};

// -----------------------------------------------------------------------------
// Micro-pitch shifter used for the doubler: a couple of cents up or down plus a
// short delay, which sounds like a second take sung on top.
// -----------------------------------------------------------------------------
class MicroShifter
{
public:
    void prepare (double sr, float windowMs, float baseDelayMs, float cents)
    {
        window = (float) (sr * windowMs * 0.001);
        base   = (float) (sr * baseDelayMs * 0.001);
        buf.assign ((size_t) (window + base) + 8, 0.0f);
        step = (1.0f - std::pow (2.0f, cents / 1200.0f)) / window;
        w = 0; phase = 0.0f;
    }
    float process (float x)
    {
        buf[(size_t) w] = x;
        float y = 0.0f;
        for (float off : { 0.0f, 0.5f })
        {
            float q = phase + off; if (q >= 1.0f) q -= 1.0f;
            const float s = std::sin (juce::MathConstants<float>::pi * q);
            y += readFrac (buf, w, base + q * window) * s * s;
        }
        phase += step; if (phase >= 1.0f) phase -= 1.0f; if (phase < 0.0f) phase += 1.0f;
        if (++w >= (int) buf.size()) w = 0;
        return y;
    }
private:
    std::vector<float> buf;
    int w = 0;
    float window = 1.0f, base = 0.0f, step = 0.0f, phase = 0.0f;
};

// -----------------------------------------------------------------------------
// Ping-pong delay with a darkening feedback path.
// -----------------------------------------------------------------------------
class PingPong
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufL.assign ((size_t) (sr * 2.6) + 4, 0.0f);
        bufR.assign (bufL.size(), 0.0f);
        w = 0; dampL = dampR = 0.0f;
        dampC = 1.0f - std::exp (-twoPi * 3200.0f / (float) sr);
        hpC   = 1.0f - std::exp (-twoPi * 250.0f  / (float) sr);
        hpL = hpR = 0.0f;
        time.reset (sr, 0.2);
    }
    void setTimeMs (float ms, bool snap = false)
    {
        const float s = juce::jlimit (1.0f, (float) bufL.size() - 4.0f, (float) (ms * 0.001 * sr));
        if (snap) time.setCurrentAndTargetValue (s); else time.setTargetValue (s);
    }
    void setFeedback (float f) { fb = f; }

    void process (float in, float& outL, float& outR)
    {
        const float d = time.getNextValue();
        const float tL = readFrac (bufL, w, d), tR = readFrac (bufR, w, d);
        dampL += dampC * (tL - dampL);
        dampR += dampC * (tR - dampR);
        // gentle high-pass in the loop keeps repeats from getting boomy
        hpL += hpC * (dampL - hpL);
        hpR += hpC * (dampR - hpR);
        const float fL = dampL - hpL, fR = dampR - hpR;

        bufL[(size_t) w] = in + fR * fb;
        bufR[(size_t) w] = fL * fb;
        outL = tL; outR = tR;
        if (++w >= (int) bufL.size()) w = 0;
    }
private:
    double sr = 44100.0;
    std::vector<float> bufL, bufR;
    int w = 0;
    float fb = 0.3f, dampC = 0.3f, dampL = 0.0f, dampR = 0.0f, hpC = 0.0f, hpL = 0.0f, hpR = 0.0f;
    juce::SmoothedValue<float> time;
};

// -----------------------------------------------------------------------------
// Simple mono pre-delay line.
// -----------------------------------------------------------------------------
class PreDelay
{
public:
    void prepare (double sampleRate) { sr = sampleRate; buf.assign ((size_t) (sr * 0.25) + 4, 0.0f); w = 0; time.reset (sr, 0.1); }
    void setTimeMs (float ms) { time.setTargetValue ((float) (ms * 0.001 * sr)); }
    float process (float x)
    {
        buf[(size_t) w] = x;
        const float y = readFrac (buf, w, std::max (0.0f, time.getNextValue()));
        if (++w >= (int) buf.size()) w = 0;
        return y;
    }
private:
    double sr = 44100.0;
    std::vector<float> buf;
    int w = 0;
    juce::SmoothedValue<float> time;
};

// -----------------------------------------------------------------------------
// 8-line feedback-delay-network hall reverb with input diffusion,
// high-frequency damping and slow modulation (no metallic ringing).
// -----------------------------------------------------------------------------
class HallReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const size_t maxLen = (size_t) (sr * 0.18) + 64;
        for (auto& l : lines) l.assign (maxLen, 0.0f);
        w = 0;
        damp.fill (0.0f);
        const float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.31f };
        for (int i = 0; i < 4; ++i) { ap[(size_t) i].buf.assign ((size_t) (sr * apMs[i] * 0.001) + 1, 0.0f); ap[(size_t) i].idx = 0; }
        for (int i = 0; i < N; ++i) { modPhase[(size_t) i] = (float) i / N; modInc[(size_t) i] = (0.17f + 0.07f * (float) i) / (float) sr; }
        sizeSm.reset (sr, 0.4);
        sizeSm.setCurrentAndTargetValue (1.0f);
        inHp = 0.0f;
        inHpC = 1.0f - std::exp (-twoPi * 180.0f / (float) sr);
    }

    void setParams (float size01, float decaySeconds, float dampHz)
    {
        sizeSm.setTargetValue (0.55f + size01 * 0.9f);
        decay = std::max (0.2f, decaySeconds);
        dampC = 1.0f - std::exp (-twoPi * dampHz / (float) sr);
    }

    void processBlock (const float* in, float* outL, float* outR, int n)
    {
        // feedback gains from the block-start size (cheap, smooth enough)
        const float scale = sizeSm.getCurrentValue();
        std::array<float, N> g {};
        for (int i = 0; i < N; ++i)
            g[(size_t) i] = std::pow (10.0f, -3.0f * (baseMs[i] * scale * 0.001f) / decay);

        for (int s = 0; s < n; ++s)
        {
            const float sc = sizeSm.getNextValue();

            // input: high-pass (keeps the tail clean) then diffuse
            inHp += inHpC * (in[s] - inHp);
            float x = in[s] - inHp;
            for (auto& a : ap) x = a.process (x, 0.62f);

            std::array<float, N> y {};
            float sum = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                modPhase[(size_t) i] += modInc[(size_t) i]; if (modPhase[(size_t) i] >= 1.0f) modPhase[(size_t) i] -= 1.0f;
                const float len = baseMs[i] * sc * 0.001f * (float) sr + 5.0f * std::sin (twoPi * modPhase[(size_t) i]);
                const float r = readFrac (lines[(size_t) i], w, len);
                y[(size_t) i] = r;
                damp[(size_t) i] += dampC * (r - damp[(size_t) i]);
                sum += damp[(size_t) i] * g[(size_t) i];
            }
            const float hh = sum * (2.0f / N); // Householder mix
            float l = 0.0f, rr = 0.0f;
            for (int i = 0; i < N; ++i)
            {
                lines[(size_t) i][(size_t) w] = damp[(size_t) i] * g[(size_t) i] - hh + x * inSign[i] * 0.35f;
                l  += y[(size_t) i] * outL_[i];
                rr += y[(size_t) i] * outR_[i];
            }
            outL[s] = l * 0.35f;
            outR[s] = rr * 0.35f;
            if (++w >= (int) lines[0].size()) w = 0;
        }
    }

    void clear()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        damp.fill (0.0f);
        for (auto& a : ap) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
    }

private:
    struct AllPass
    {
        std::vector<float> buf; int idx = 0;
        float process (float x, float gain)
        {
            const float b = buf[(size_t) idx];
            const float y = -gain * x + b;
            buf[(size_t) idx] = x + gain * y;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };

    static constexpr float baseMs[N]  = { 31.7f, 37.3f, 41.9f, 47.1f, 53.3f, 59.1f, 67.9f, 73.7f };
    static constexpr float inSign[N]  = { 1, -1, 1, -1, 1, -1, 1, -1 };
    static constexpr float outL_[N]   = { 1, 1, -1, -1, 1, 1, -1, -1 };
    static constexpr float outR_[N]   = { 1, -1, -1, 1, -1, 1, 1, -1 };

    double sr = 44100.0;
    std::array<std::vector<float>, N> lines;
    std::array<float, N> damp {}, modPhase {}, modInc {};
    std::array<AllPass, 4> ap;
    int w = 0;
    float decay = 2.5f, dampC = 0.5f, inHp = 0.0f, inHpC = 0.0f;
    juce::SmoothedValue<float> sizeSm;
};

} // namespace haze
