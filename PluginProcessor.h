#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include "DSP.h"

//==============================================================================
// Haze Vox v2: a dreamy late-night R&B vocal chain.
//
//  Pitch/Formant ──► Smooth (leveler) ► Drive ► Low cut ► Warmth ► Tone ► De-ess = V
//  V ► Doubler (Width) = V2
//  V2 ──► Ping-pong delay ─┐
//  V2 ──► Pre-delay ► Hall ┼─► ducked by the vocal (Duck) ─► + V2 = Wet
//  Mix blends the pitched clean vocal with Wet (pitch always stays applied).
//==============================================================================
class HazeVoxProcessor : public juce::AudioProcessor
{
public:
    HazeVoxProcessor();
    ~HazeVoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Haze Vox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    // Factory presets
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // User presets (saved as files)
    static juce::File getUserPresetFolder();
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);

    // A/B compare
    void selectSlot (int slot);       // 0 = A, 1 = B
    int getActiveSlot() const { return activeSlot; }
    void copyActiveToOther();

    // UI state that should be remembered with the session
    bool advancedView = false;
    bool darkTheme = false;
    juce::String presetName { "Slowed & Low" };

    // Meters (written by audio thread, read by the editor)
    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f },
                       grSmooth { 0.0f }, grDeess { 0.0f }, grDuck { 0.0f };

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray delayNotes;

private:
    void updateFilters (float warmthDb, float toneHz);
    float saturate (float x, float drive) const;
    float hostBpm() const;

    double sr = 44100.0;
    int currentProgram = 0;

    // Pitch / formant
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> stretchIn, cleanPitched, delayL, delayR, verbIn, verbL, verbR, duckGainBuf;
    // Pitch and formant glide toward the knobs (in semitones) so voice changes are smooth.
    float pitchSm = 0.0f, formantSm = 0.0f, appliedPitch = 1000.0f, appliedFormant = 1000.0f;
    bool appliedNatural = false;

    // Color
    haze::Leveler leveler;
    haze::DeEsser deesser;
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    Filter lowCut, warmth;
    juce::dsp::StateVariableTPTFilter<float> tone;   // TPT filter: safe to sweep without clicks
    float lastWarmth = -1.0f, lastTone = -1.0f;
    float warmthSm = 0.0f, toneSm = 0.0f;   // toneSm is log2(Hz)

    // Space
    haze::MicroShifter dblUp, dblDown;
    haze::PingPong delay;
    haze::PreDelay preDelay;
    haze::HallReverb hall;
    float duckEnv = 0.0f, duckAtk = 0.0f, duckRel = 0.0f;
    float decaySm = 2.6f, fbSm = 0.3f;

    juce::SmoothedValue<float> smWidth, smDMix, smVMix, smMix, smOut, smDuckGain, smDrive, smSmooth;

    // A/B slots
    juce::ValueTree slots[2];
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HazeVoxProcessor)
};
