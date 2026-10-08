#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
// Colours for one theme. Light = cream and sage, Dark = deep forest.
struct Palette
{
    juce::Colour bg, card, cardLine, ink, dim, accent, accentSoft,
                 fieldA, fieldB, fieldC, knobBody, knobRing, glow;

    static Palette light();
    static Palette dark();
};

juce::Font hazeFont (float size, bool bold = false);

//==============================================================================
class HazeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HazeLookAndFeel();
    void setDark (bool dark);
    const Palette& palette() const { return pal; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

private:
    Palette pal;
};

//==============================================================================
// Slim horizontal meter: a level meter (dBFS) or a gain-reduction meter (dB).
class Meter : public juce::Component
{
public:
    Meter (juce::String name, bool reduction) : label (std::move (name)), isReduction (reduction) {}
    void setValue (float v);   // linear peak for level meters, dB for reduction meters
    void paint (juce::Graphics&) override;
private:
    juce::String label;
    bool isReduction;
    float shown = 0.0f;        // 0..1
};

//==============================================================================
// The soft, slowly drifting "haze field" behind the main knobs.
// Haze sets how dense and bright the mist is; the output level makes it breathe.
class HazeField : public juce::Component
{
public:
    void advance (float seconds, float level, float haze);
    void paint (juce::Graphics&) override;
private:
    float time = 0.0f, glow = 0.0f, density = 0.5f;
};

//==============================================================================
class HazeVoxEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit HazeVoxEditor (HazeVoxProcessor&);
    ~HazeVoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Size { big, medium, small };

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void addKnob (Knob&, const juce::String& paramId, const juce::String& text, const juce::String& tip);
    void placeKnob (Knob&, juce::Rectangle<int> area, Size);
    void populatePresets();
    void showSaveDialog();
    void updateViewVisibility();
    void refreshAB();
    void applyTheme();
    void updateHint();

    HazeVoxProcessor& proc;
    HazeLookAndFeel lnf;

    Knob pitch, formant, glide, haze, smooth, drive, warmth, tone, deess,
         width, dTime, dNote, dFb, dMix, preDelay, vSize, decay, vMix, duck, mix, out;
    std::vector<Knob*> allKnobs;

    juce::ToggleButton link { "Tape link" }, sync { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment, syncAttachment;

    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save" }, aButton { "A" }, bButton { "B" },
                     copyButton { "Copy" }, viewButton, themeButton;

    Meter inMeter { "IN", false }, outMeter { "OUT", false },
          smoothMeter { "COMP", true }, deessMeter { "S'S", true }, duckMeter { "DUCK", true };

    HazeField field;

    std::vector<std::pair<juce::Rectangle<int>, juce::String>> cards;
    juce::Rectangle<int> hintArea, toolbarArea;
    juce::String hintText;
    juce::Array<juce::File> userPresetFiles;
    double lastTick = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HazeVoxEditor)
};
