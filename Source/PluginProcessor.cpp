#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray HazeVoxProcessor::delayNotes { "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2" };
static const float delayBeats[] { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };

//==============================================================================
// Factory presets. Anything not listed uses the parameter's default.
//==============================================================================
namespace
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list =
    {
        { "Slowed & Low", {} }, // the parameter defaults are this preset
        { "3AM Haze", { {"pitch",-4}, {"haze",.62f}, {"drive",.35f}, {"warmth",4}, {"tone",5000},
                        {"width",.5f}, {"dnote",4}, {"dfb",.4f}, {"dmix",.2f}, {"predelay",45},
                        {"vsize",.85f}, {"decay",3.8f}, {"vmix",.4f}, {"duck",.55f}, {"out",-1.5f} } },
        { "Late Night Drive", { {"pitch",-2}, {"haze",.45f}, {"drive",.2f}, {"warmth",2.5f}, {"tone",8500},
                        {"width",.35f}, {"dnote",2}, {"dfb",.25f}, {"dmix",.12f}, {"predelay",30},
                        {"vsize",.6f}, {"decay",2.0f}, {"vmix",.22f}, {"duck",.4f}, {"out",-1} } },
        { "Deep Pitch, Clear", { {"pitch",-3}, {"link",0}, {"formant",0}, {"haze",.4f}, {"drive",.15f},
                        {"warmth",2}, {"tone",9500}, {"width",.3f}, {"decay",1.8f}, {"vmix",.2f}, {"duck",.5f} } },
        { "Chopped (Extreme)", { {"pitch",-6}, {"haze",.7f}, {"drive",.3f}, {"warmth",5}, {"tone",4000},
                        {"width",.45f}, {"dnote",3}, {"dfb",.45f}, {"dmix",.2f}, {"predelay",50},
                        {"vsize",.9f}, {"decay",4.5f}, {"vmix",.4f}, {"duck",.6f}, {"out",-2} } },
        { "Sped Up", { {"pitch",3}, {"haze",.4f}, {"drive",.15f}, {"warmth",.5f}, {"tone",14000},
                        {"dnote",1}, {"dfb",.2f}, {"dmix",.1f}, {"decay",1.6f}, {"vmix",.18f} } },
        { "Pitch Only (No FX)", { {"out",-2}, {"haze",0}, {"smooth",0}, {"drive",0}, {"warmth",0}, {"tone",20000},
                        {"deess",0}, {"width",0}, {"dmix",0}, {"vmix",0}, {"duck",0} } },
    };
    return list;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HazeVoxProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pitchFmt = [] (float v, int)
    {
        const int speed = roundToInt (std::pow (2.0f, v / 12.0f) * 100.0f);
        return (v > 0 ? "+" : "") + String (v, 1) + " st  " + String (speed) + "%";
    };
    auto semis = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; };
    auto pct   = [] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; };
    auto ms    = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto secs  = [] (float v, int) { return String (v, 1) + " s"; };
    auto glideFmt = [] (float v, int) { return v < 0.995f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };
    auto hz    = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto db    = [] (float v, int) { return String (v, 1) + " dB"; };

    auto add = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                    std::function<String (float, int)> fmt, int version = 2)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, version }, name, r, def,
            AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };

    // Voice
    add ("pitch",   "Pitch",       { -12.0f, 12.0f, 0.1f }, -3.0f, pitchFmt);
    add ("formant", "Formant",     { -12.0f, 12.0f, 0.1f },  0.0f, semis);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "link", 2 }, "Tape Link", true));
    add ("glide",   "Glide",       NormalisableRange<float> (0.0f, 2.0f, 0.01f, 0.5f), 0.15f, glideFmt, 3); // new in v2.1

    // Macro
    add ("haze",    "Haze",        { 0.0f, 1.0f }, 0.5f, pct);

    // Color
    add ("smooth",  "Smooth",      { 0.0f, 1.0f }, 0.35f, pct);
    add ("drive",   "Drive",       { 0.0f, 1.0f }, 0.25f, pct);
    add ("warmth",  "Warmth",      { 0.0f, 9.0f, 0.1f }, 3.0f, db);
    add ("tone",    "Tone",        NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.3f), 6500.0f, hz);
    add ("deess",   "De-ess",      { 0.0f, 1.0f }, 0.4f, pct);

    // Space
    add ("width",   "Width",       { 0.0f, 1.0f }, 0.4f, pct);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "dsync", 2 }, "Delay Sync", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "dnote", 2 }, "Delay Note", delayNotes, 3));
    add ("dtime",   "Delay Time",  NormalisableRange<float> (50.0f, 1000.0f, 1.0f, 0.6f), 380.0f, ms);
    add ("dfb",     "Delay Fdbk",  { 0.0f, 0.85f }, 0.30f, pct);
    add ("dmix",    "Delay Mix",   { 0.0f, 1.0f }, 0.14f, pct);
    add ("predelay","Pre-delay",   { 0.0f, 150.0f, 1.0f }, 35.0f, ms);
    add ("vsize",   "Verb Size",   { 0.0f, 1.0f }, 0.7f, pct);
    add ("decay",   "Decay",       NormalisableRange<float> (0.5f, 8.0f, 0.1f, 0.5f), 2.6f, secs);
    add ("vmix",    "Verb Mix",    { 0.0f, 1.0f }, 0.3f, pct);
    add ("duck",    "Duck",        { 0.0f, 1.0f }, 0.45f, pct);

    // Output
    add ("mix",     "Mix",         { 0.0f, 1.0f }, 1.0f, pct);
    add ("out",     "Output",      { -24.0f, 12.0f, 0.1f }, 0.0f, db);

    return { p.begin(), p.end() };
}

//==============================================================================
HazeVoxProcessor::HazeVoxProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool HazeVoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void HazeVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int numCh = juce::jlimit (1, 2, getTotalNumOutputChannels());
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };

    stretch.presetDefault (numCh, (float) sampleRate);
    stretch.setFormantBase (150.0f / (float) sampleRate);
    stretch.reset();
    {
        const float pt = apvts.getRawParameterValue ("pitch")->load();
        const bool lk = apvts.getRawParameterValue ("link")->load() > 0.5f;
        pitchSm = pt;
        formantSm = lk ? pt : apvts.getRawParameterValue ("formant")->load();
        appliedPitch = appliedFormant = 1000.0f;
    }
    setLatencySamples (stretch.inputLatency() + stretch.outputLatency());

    stretchIn.setSize (numCh, samplesPerBlock);
    cleanPitched.setSize (numCh, samplesPerBlock);
    for (auto* b : { &delayL, &delayR, &verbIn, &verbL, &verbR, &duckGainBuf })
        b->setSize (1, samplesPerBlock);

    leveler.prepare (sampleRate);
    deesser.prepare (spec);

    lowCut.prepare (spec);
    warmth.prepare (spec);
    tone.prepare (spec);
    tone.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    tone.setResonance (0.6f);
    *lowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 90.0f, 0.707f);
    lastWarmth = lastTone = -1.0f;
    warmthSm = apvts.getRawParameterValue ("warmth")->load();
    toneSm   = std::log2 (apvts.getRawParameterValue ("tone")->load());
    decaySm  = apvts.getRawParameterValue ("decay")->load();
    fbSm     = apvts.getRawParameterValue ("dfb")->load();

    dblUp.prepare   (sampleRate, 45.0f, 13.0f,  7.0f);
    dblDown.prepare (sampleRate, 52.0f, 21.0f, -7.0f);
    delay.prepare (sampleRate);
    delay.setTimeMs (380.0f, true);
    preDelay.prepare (sampleRate);
    hall.prepare (sampleRate);

    duckEnv = 0.0f;
    duckAtk = haze::onePoleCoef (sampleRate, 5.0f);
    duckRel = haze::onePoleCoef (sampleRate, 70.0f);

    for (auto* s : { &smWidth, &smDMix, &smVMix, &smMix, &smOut, &smDuckGain, &smDrive, &smSmooth })
        s->reset (sampleRate, 0.05);
    smDrive.setCurrentAndTargetValue  (apvts.getRawParameterValue ("drive")->load());
    smSmooth.setCurrentAndTargetValue (apvts.getRawParameterValue ("smooth")->load());
    smOut.setCurrentAndTargetValue (1.0f);
    smMix.setCurrentAndTargetValue (1.0f);
    smDuckGain.setCurrentAndTargetValue (1.0f);
}

void HazeVoxProcessor::updateFilters (float warmthDb, float toneHz)
{
    if (std::abs (warmthDb - lastWarmth) > 0.01f)
    {
        *warmth.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, 280.0f, 0.8f, juce::Decibels::decibelsToGain (warmthDb));
        lastWarmth = warmthDb;
    }
    if (std::abs (toneHz - lastTone) > 1.0f)
    {
        const float f = juce::jmin (toneHz, (float) (sr * 0.45));
        tone.setCutoffFrequency (f);
        lastTone = toneHz;
    }
}

// Warm, slightly asymmetric tape-style saturation with unity small-signal gain.
float HazeVoxProcessor::saturate (float x, float drive) const
{
    if (drive <= 0.0001f) return x;
    const float g = 1.0f + drive * 5.0f;
    const float b = 0.12f * drive;
    const float tb = std::tanh (g * b);
    const float slope = g * (1.0f - tb * tb);
    return (std::tanh (g * (x + b)) - tb) / slope;
}

float HazeVoxProcessor::hostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0) return (float) *bpm;
    return 120.0f;
}

//==============================================================================
void HazeVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels(), 2);
    const int n = buffer.getNumSamples();
    if (n == 0 || numCh == 0) return;

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    // ---- Haze macro: 50% = knobs exactly as set; 0% = no space, brighter;
    //      100% = double the space, longer and darker.
    const float hz  = get ("haze");
    const float k   = hz * 2.0f;
    const float toneEff  = juce::jlimit (1500.0f, 20000.0f, get ("tone") * std::pow (2.0f, -(hz - 0.5f) * 2.0f));
    const float decayEff = get ("decay") * (0.5f + hz);

    smWidth.setTargetValue (juce::jlimit (0.0f, 1.0f, get ("width") * k));
    smDMix.setTargetValue  (juce::jlimit (0.0f, 1.0f, get ("dmix") * k));
    smVMix.setTargetValue  (juce::jlimit (0.0f, 1.0f, get ("vmix") * k));
    smMix.setTargetValue   (get ("mix"));
    smOut.setTargetValue   (juce::Decibels::decibelsToGain (get ("out")));

    // grow working buffers if the host sends a bigger block than promised
    if (stretchIn.getNumSamples() < n || stretchIn.getNumChannels() < numCh)
    {
        stretchIn.setSize (numCh, n, false, false, true);
        cleanPitched.setSize (numCh, n, false, false, true);
        for (auto* b : { &delayL, &delayR, &verbIn, &verbL, &verbR, &duckGainBuf })
            b->setSize (1, n, false, false, true);
    }

    meterIn.store (juce::jmax (meterIn.load(), buffer.getMagnitude (0, n)));

    // ---- 1. Pitch / formant ----------------------------------------------------
    //      Pitch and formant glide toward the knobs in small chunks, so turning or
    //      automating them slides the voice instead of stepping. With Tape link on
    //      the formant target is the pitch itself (vari-speed); flipping Tape link
    //      glides between the two instead of jumping.
    const bool link = get ("link") > 0.5f;
    const float pitchTarget = get ("pitch");
    const float formantTarget = link ? pitchTarget : get ("formant");
    const float glideSec = juce::jmax (0.03f, get ("glide"));

    for (int c = 0; c < numCh; ++c)
        stretchIn.copyFrom (c, 0, buffer, c, 0, n);

    constexpr int pitchChunk = 64;
    for (int start = 0; start < n; start += pitchChunk)
    {
        const int len = juce::jmin (pitchChunk, n - start);
        // time constant = glide / 3, so the slide is ~95% done after "glide" seconds
        const float a = 1.0f - std::exp (-3.0f * (float) len / (glideSec * (float) sr));
        pitchSm   += a * (pitchTarget - pitchSm);
        formantSm += a * (formantTarget - formantSm);
        if (std::abs (pitchTarget - pitchSm) < 0.002f)     pitchSm = pitchTarget;
        if (std::abs (formantTarget - formantSm) < 0.002f) formantSm = formantTarget;

        // formants riding exactly with the pitch = plain vari-speed, no formant processing needed
        const bool natural = std::abs (formantSm - pitchSm) < 0.002f;
        if (pitchSm != appliedPitch || formantSm != appliedFormant || natural != appliedNatural)
        {
            stretch.setTransposeSemitones (pitchSm, 8000.0f / (float) sr);
            if (natural) stretch.setFormantFactor (1.0f, false);
            else         stretch.setFormantSemitones (formantSm, true);
            appliedPitch = pitchSm; appliedFormant = formantSm; appliedNatural = natural;
        }

        const float* ins[2]  { stretchIn.getReadPointer (0, start), stretchIn.getReadPointer (numCh > 1 ? 1 : 0, start) };
        float*       outs[2] { buffer.getWritePointer (0, start),   buffer.getWritePointer (numCh > 1 ? 1 : 0, start) };
        stretch.process (ins, len, outs, len);
    }

    // keep the pitched-but-unprocessed vocal for the Mix knob (pitch always stays applied)
    for (int c = 0; c < numCh; ++c)
        cleanPitched.copyFrom (c, 0, buffer, c, 0, n);

    auto* L = buffer.getWritePointer (0);
    auto* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // ---- 2. Smooth (leveler) + Drive --------------------------------------------
    smSmooth.setTargetValue (get ("smooth"));
    smDrive.setTargetValue (get ("drive"));
    float maxGrSmooth = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        leveler.setAmount (smSmooth.getNextValue());
        const float drive = smDrive.getNextValue();
        const float det = R ? std::max (std::abs (L[i]), std::abs (R[i])) : std::abs (L[i]);
        const float g = leveler.process (det);
        maxGrSmooth = std::max (maxGrSmooth, leveler.getGainReductionDb());
        L[i] = saturate (L[i] * g, drive);
        if (R) R[i] = saturate (R[i] * g, drive);
    }

    // ---- 3. EQ --------------------------------------------------------------------
    //      Warmth and Tone glide too (Tone in octaves), with the filters updated
    //      every 32 samples, so sweeping them or the Haze knob never zippers.
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);
        const float warmT = get ("warmth"), toneT = std::log2 (toneEff);
        constexpr int eqChunk = 32;
        for (int start = 0; start < n; start += eqChunk)
        {
            const int len = juce::jmin (eqChunk, n - start);
            const float a = 1.0f - std::exp (-(float) len / (0.04f * (float) sr));
            warmthSm += a * (warmT - warmthSm);
            toneSm   += a * (toneT - toneSm);
            updateFilters (warmthSm, std::exp2 (toneSm));

            auto part = sub.getSubBlock ((size_t) start, (size_t) len);
            juce::dsp::ProcessContextReplacing<float> ctx (part);
            lowCut.process (ctx);
            warmth.process (ctx);
            tone.process (ctx);
        }
    }

    // ---- 4. De-ess (before the reverb so the tails don't hiss) -------------------
    deesser.setAmount (get ("deess"));
    float maxGrDeess = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float frame[2] = { L[i], R ? R[i] : 0.0f };
        deesser.processFrame (frame, numCh);
        L[i] = frame[0];
        if (R) R[i] = frame[1];
        maxGrDeess = std::max (maxGrDeess, deesser.getGainReductionDb());
    }

    // ---- 5. Doubler, delay send, reverb send, ducking ------------------------------
    const bool sync = get ("dsync") > 0.5f;
    const int noteIdx = juce::jlimit (0, 5, (int) get ("dnote"));
    const float delayMs = sync ? 60000.0f / hostBpm() * delayBeats[noteIdx] : get ("dtime");
    delay.setTimeMs (juce::jmin (delayMs, 2500.0f));
    {
        const float a = 1.0f - std::exp (-(float) n / (0.08f * (float) sr));
        fbSm    += a * (get ("dfb") - fbSm);
        decaySm += a * (decayEff - decaySm);
    }
    delay.setFeedback (fbSm);
    preDelay.setTimeMs (get ("predelay"));
    hall.setParams (get ("vsize"), decaySm, 6500.0f);

    const float duckAmt = get ("duck");
    float maxGrDuck = 0.0f;

    auto* dl = delayL.getWritePointer (0);
    auto* dr = delayR.getWritePointer (0);
    auto* vi = verbIn.getWritePointer (0);
    auto* dg = duckGainBuf.getWritePointer (0);

    for (int i = 0; i < n; ++i)
    {
        const float vMono = R ? 0.5f * (L[i] + R[i]) : L[i];

        // ducking follows the vocal: effects dip while you sing and bloom in the gaps
        const float a = std::abs (vMono);
        duckEnv += (a > duckEnv ? duckAtk : duckRel) * (a - duckEnv);
        const float red = duckAmt * 18.0f * juce::jlimit (0.0f, 1.0f, (haze::gainToDb (duckEnv) + 42.0f) / 30.0f);
        maxGrDuck = std::max (maxGrDuck, red);
        dg[i] = haze::dbToGain (-red);

        // doubler: one copy a few cents up (left), one a few cents down (right)
        const float w = smWidth.getNextValue();
        const float up = dblUp.process (vMono), down = dblDown.process (vMono);
        if (R) { L[i] += w * 0.6f * up; R[i] += w * 0.6f * down; }
        else   { L[i] += w * 0.3f * (up + down); }

        const float v2 = R ? 0.5f * (L[i] + R[i]) : L[i];
        delay.process (v2, dl[i], dr[i]);
        vi[i] = preDelay.process (v2 + 0.25f * (dl[i] + dr[i]));
    }

    hall.processBlock (vi, verbL.getWritePointer (0), verbR.getWritePointer (0), n);
    const auto* vl = verbL.getReadPointer (0);
    const auto* vr = verbR.getReadPointer (0);
    const auto* cl = cleanPitched.getReadPointer (0);
    const auto* cr = numCh > 1 ? cleanPitched.getReadPointer (1) : cl;

    // ---- 6. Sum the sends, Mix, Output ------------------------------------------------
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        smDuckGain.setTargetValue (dg[i]);
        const float duck = smDuckGain.getNextValue();
        const float dm = smDMix.getNextValue() * duck;
        const float vm = smVMix.getNextValue() * duck * 1.4f;
        const float mx = smMix.getNextValue();
        const float og = smOut.getNextValue();

        if (R)
        {
            const float wl = L[i] + dm * dl[i] + vm * vl[i];
            const float wr = R[i] + dm * dr[i] + vm * vr[i];
            L[i] = (cl[i] * (1.0f - mx) + wl * mx) * og;
            R[i] = (cr[i] * (1.0f - mx) + wr * mx) * og;
            outPeak = std::max (outPeak, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        else
        {
            const float wm = L[i] + dm * 0.5f * (dl[i] + dr[i]) + vm * 0.5f * (vl[i] + vr[i]);
            L[i] = (cl[i] * (1.0f - mx) + wm * mx) * og;
            outPeak = std::max (outPeak, std::abs (L[i]));
        }
    }

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    meterOut.store (juce::jmax (meterOut.load(), outPeak));
    grSmooth.store (juce::jmax (grSmooth.load(), maxGrSmooth));
    grDeess.store  (juce::jmax (grDeess.load(),  maxGrDeess));
    grDuck.store   (juce::jmax (grDuck.load(),   maxGrDuck));
}

//==============================================================================
// Factory presets
//==============================================================================
int HazeVoxProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String HazeVoxProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets()[(size_t) index].name : "";
}

void HazeVoxProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    const auto& pr = presets()[(size_t) index];

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, v] : pr.values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));

    presetName = pr.name;
}

//==============================================================================
// User presets
//==============================================================================
juce::File HazeVoxProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("HazeVox").getChildFile ("Presets");
}

juce::Array<juce::File> HazeVoxProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.hazevox");
    files.sort();
    return files;
}

bool HazeVoxProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    folder.createDirectory();
    if (auto xml = apvts.copyState().createXml())
        if (xml->writeTo (folder.getChildFile (clean + ".hazevox")))
        {
            presetName = clean;
            return true;
        }
    return false;
}

bool HazeVoxProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presetName = file.getFileNameWithoutExtension();
            return true;
        }
    return false;
}

//==============================================================================
// A/B compare
//==============================================================================
void HazeVoxProcessor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    slots[activeSlot] = apvts.copyState();
    if (slots[slot].isValid()) apvts.replaceState (slots[slot].createCopy());
    else                       slots[slot] = apvts.copyState();
    activeSlot = slot;
}

void HazeVoxProcessor::copyActiveToOther()
{
    slots[1 - activeSlot] = apvts.copyState();
}

//==============================================================================
void HazeVoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("advanced", advancedView, nullptr);
    state.setProperty ("dark", darkTheme, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HazeVoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            presetName     = tree.getProperty ("presetName", "Slowed & Low").toString();
            advancedView   = tree.getProperty ("advanced", false);
            darkTheme      = tree.getProperty ("dark", false);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* HazeVoxProcessor::createEditor() { return new HazeVoxEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HazeVoxProcessor(); }
