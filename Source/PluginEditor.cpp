#include "PluginEditor.h"

namespace
{
constexpr int W = 1000, H = 640, factoryIdBase = 1, userIdBase = 1001, openFolderId = 999;
const juce::String defaultHint { "Hover any control for a quick guide.  Double-click a knob to reset it." };

const Palette& palOf (juce::Component& c)
{
    if (auto* l = dynamic_cast<HazeLookAndFeel*> (&c.getLookAndFeel()))
        return l->palette();
    static const Palette fallback = Palette::light();
    return fallback;
}

juce::Font tracked (float size, bool bold, float tracking)
{
    return hazeFont (size, bold).withExtraKerningFactor (tracking);
}
} // namespace

//==============================================================================
// Palette + fonts
//==============================================================================
Palette Palette::light()
{
    return { juce::Colour (0xfff3f0e8), juce::Colour (0xfffaf8f2), juce::Colour (0xffdcd7cb),
             juce::Colour (0xff23332c), juce::Colour (0xff7d8781), juce::Colour (0xff2f4a3d), juce::Colour (0xff8fa894),
             juce::Colour (0xffdde5e5), juce::Colour (0xffcbd6c6), juce::Colour (0xff8ea689),
             juce::Colour (0xfff8f7f2), juce::Colour (0xff3b4e45), juce::Colours::white };
}

Palette Palette::dark()
{
    return { juce::Colour (0xff121815), juce::Colour (0xff1a221e), juce::Colour (0xff2c3832),
             juce::Colour (0xffe4ebe6), juce::Colour (0xff8c9a92), juce::Colour (0xffa8c4ad), juce::Colour (0xff5e7a66),
             juce::Colour (0xff1b2523), juce::Colour (0xff22302a), juce::Colour (0xff364b3e),
             juce::Colour (0xff27312c), juce::Colour (0xffc9d8cd), juce::Colour (0xffa8c4ad) };
}

juce::Font hazeFont (float size, bool bold)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Avenir Next", bold ? "Demi Bold" : "Medium", size));
   #elif JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
   #endif
}

//==============================================================================
// Look and feel
//==============================================================================
HazeLookAndFeel::HazeLookAndFeel() { setDark (false); }

void HazeLookAndFeel::setDark (bool dark)
{
    pal = dark ? Palette::dark() : Palette::light();
    const auto none = juce::Colours::transparentBlack;

    setColour (juce::ResizableWindow::backgroundColourId, pal.bg);
    setColour (juce::Slider::textBoxTextColourId, pal.dim);
    setColour (juce::Slider::textBoxOutlineColourId, none);
    setColour (juce::Slider::textBoxBackgroundColourId, none);
    setColour (juce::Slider::textBoxHighlightColourId, pal.accentSoft.withAlpha (0.4f));
    setColour (juce::Label::textColourId, pal.ink);
    setColour (juce::Label::outlineColourId, none);
    setColour (juce::Label::textWhenEditingColourId, pal.ink);
    setColour (juce::Label::backgroundWhenEditingColourId, pal.card);
    setColour (juce::Label::outlineWhenEditingColourId, pal.cardLine);
    setColour (juce::TextEditor::backgroundColourId, pal.card);
    setColour (juce::TextEditor::textColourId, pal.ink);
    setColour (juce::TextEditor::outlineColourId, pal.cardLine);
    setColour (juce::TextEditor::focusedOutlineColourId, pal.accent);
    setColour (juce::TextEditor::highlightColourId, pal.accentSoft.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, pal.ink);
    setColour (juce::ComboBox::textColourId, pal.ink);
    setColour (juce::ComboBox::backgroundColourId, pal.card);
    setColour (juce::ComboBox::outlineColourId, pal.cardLine);
    setColour (juce::ComboBox::arrowColourId, pal.dim);
    setColour (juce::PopupMenu::backgroundColourId, pal.card);
    setColour (juce::PopupMenu::textColourId, pal.ink);
    setColour (juce::PopupMenu::headerTextColourId, pal.dim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal.accent);
    setColour (juce::PopupMenu::highlightedTextColourId, pal.bg);
    setColour (juce::TextButton::buttonColourId, pal.card);
    setColour (juce::TextButton::textColourOffId, pal.ink);
    setColour (juce::TextButton::textColourOnId, pal.bg);
    setColour (juce::AlertWindow::backgroundColourId, pal.bg);
    setColour (juce::AlertWindow::textColourId, pal.ink);
    setColour (juce::AlertWindow::outlineColourId, pal.cardLine);
}

// Minimal knob: a soft glowing disc, a thin value arc and a needle.
void HazeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    if (r < 8.0f) return;
    const auto c = bounds.getCentre();
    const float bodyR = r * 0.64f, arcR = r * 0.84f;
    const float alpha = s.isEnabled() ? 1.0f : 0.35f;
    const float hover = s.isMouseOverOrDragging() ? 1.0f : 0.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float stroke = juce::jlimit (1.5f, 2.6f, r * 0.035f);

    auto circle = [&] (float rad) { return juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f); };
    auto polar  = [&] (float a, float len) { return c + juce::Point<float> (std::sin (a), -std::cos (a)) * len; };

    // halo
    for (int i = 5; i >= 1; --i)
    {
        g.setColour (pal.glow.withAlpha ((0.06f + 0.03f * hover) * alpha));
        g.fillEllipse (circle (bodyR + (float) i * r * 0.05f));
    }

    // body
    g.setGradientFill (juce::ColourGradient (pal.knobBody.brighter (0.06f).withMultipliedAlpha (alpha), c.x, c.y - bodyR,
                                             pal.knobBody.darker (0.07f).withMultipliedAlpha (alpha), c.x, c.y + bodyR, false));
    g.fillEllipse (circle (bodyR));
    g.setColour (pal.knobRing.withAlpha (0.20f * alpha));
    g.drawEllipse (circle (bodyR), 1.0f);

    // arcs: faint full track + value (bipolar knobs grow from the top)
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (pal.ink.withAlpha (0.10f * alpha));
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (pal.ink.withAlpha (0.85f * alpha));
        g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // needle + centre dot
    g.setColour (pal.ink.withAlpha (0.9f * alpha));
    g.drawLine ({ polar (angle, bodyR * 0.18f), polar (angle, bodyR * 0.86f) }, stroke * 0.8f);
    g.fillEllipse (circle (stroke * 0.9f));
}

// Pill toggle with a status dot, e.g. "● TAPE LINK".
void HazeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;

    g.setColour (pal.card);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? pal.accent.withAlpha (0.55f) : (highlighted ? pal.ink.withAlpha (0.3f) : pal.cardLine));
    g.drawRoundedRectangle (r, rad, 1.0f);

    const float d = 7.0f;
    auto dot = juce::Rectangle<float> (r.getX() + rad - d * 0.5f + 2.0f, r.getCentreY() - d * 0.5f, d, d);
    if (on) { g.setColour (pal.accent); g.fillEllipse (dot); }
    else    { g.setColour (pal.dim);    g.drawEllipse (dot, 1.2f); }

    g.setColour (on ? pal.ink : pal.dim);
    g.setFont (tracked (11.0f, true, 0.08f));
    g.drawText (b.getButtonText().toUpperCase(), r.withTrimmedLeft (rad + 8.0f).withTrimmedRight (8.0f),
                juce::Justification::centred);
}

void HazeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? pal.accent : (down ? pal.cardLine : pal.card));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (on ? pal.accent : (highlighted ? pal.ink.withAlpha (0.35f) : pal.cardLine));
    g.drawRoundedRectangle (r, 10.0f, 1.0f);
}

juce::Font HazeLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return tracked (12.0f, true, 0.06f); }

void HazeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<int> (width, height).toFloat().reduced (1.0f);
    g.setColour (pal.card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (box.isMouseOver (true) ? pal.ink.withAlpha (0.35f) : pal.cardLine);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    const float cx = r.getRight() - 18.0f, cy = r.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.5f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.5f, cy - 2.0f);
    g.setColour (pal.dim);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font HazeLookAndFeel::getComboBoxFont (juce::ComboBox&) { return hazeFont (13.0f, true); }

void HazeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font HazeLookAndFeel::getPopupMenuFont() { return hazeFont (14.0f); }

juce::Font HazeLookAndFeel::getLabelFont (juce::Label& label)
{
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return hazeFont (11.5f);
    return label.getFont();
}

//==============================================================================
// Meter
//==============================================================================
void Meter::setValue (float v)
{
    float target;
    if (isReduction) target = juce::jlimit (0.0f, 1.0f, v / 18.0f);
    else             target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 48.0f) / 48.0f);
    const float next = target > shown ? target : shown * 0.86f + target * 0.14f; // fast up, smooth fall
    if (std::abs (next - shown) > 0.001f) { shown = next; repaint(); }
}

void Meter::paint (juce::Graphics& g)
{
    const auto& p = palOf (*this);
    auto r = getLocalBounds().toFloat();
    g.setColour (p.dim);
    g.setFont (tracked (10.0f, true, 0.08f));
    g.drawText (label, r.removeFromLeft (40.0f), juce::Justification::centredLeft);

    auto bar = r.withSizeKeepingCentre (r.getWidth(), 4.0f);
    g.setColour (p.ink.withAlpha (0.10f));
    g.fillRoundedRectangle (bar, 2.0f);
    if (shown > 0.002f)
    {
        g.setColour (isReduction ? p.accentSoft : p.accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * shown), 2.0f);
    }
}

//==============================================================================
// Haze field
//==============================================================================
void HazeField::advance (float seconds, float level, float haze)
{
    density += 0.1f * (haze - density);
    time += seconds * (0.25f + density * 0.5f);
    const float target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (level, -60.0f) + 42.0f) / 36.0f);
    glow += (target > glow ? 0.3f : 0.05f) * (target - glow);
    repaint();
}

void HazeField::paint (juce::Graphics& g)
{
    const auto& p = palOf (*this);
    const auto r = getLocalBounds().toFloat();
    const float w = r.getWidth(), h = r.getHeight();

    g.saveState();
    juce::Path clip;
    clip.addRoundedRectangle (r, 18.0f);
    g.reduceClipRegion (clip);

    juce::ColourGradient base (p.fieldA, 0.0f, 0.0f, p.fieldC, w, h, false);
    base.addColour (0.55, p.fieldB);
    g.setGradientFill (base);
    g.fillRect (r);

    // drifting translucent slabs; soft edges come from a few stacked, growing fills
    struct Slab { float x, y, w, h, speed, phase; bool light; };
    static const Slab slabs[] = {
        { 0.08f, 0.14f, 0.20f, 0.30f, 0.21f, 0.0f, true  }, { 0.17f, 0.36f, 0.16f, 0.22f, 0.17f, 1.3f, false },
        { 0.31f, 0.30f, 0.09f, 0.62f, 0.13f, 2.1f, false }, { 0.40f, 0.10f, 0.13f, 0.40f, 0.19f, 0.7f, true  },
        { 0.45f, 0.55f, 0.18f, 0.20f, 0.23f, 3.3f, true  }, { 0.55f, 0.24f, 0.10f, 0.66f, 0.11f, 4.2f, false },
        { 0.63f, 0.08f, 0.14f, 0.24f, 0.16f, 5.0f, true  }, { 0.70f, 0.30f, 0.17f, 0.18f, 0.20f, 2.7f, false },
        { 0.78f, 0.48f, 0.12f, 0.34f, 0.14f, 1.9f, true  }, { 0.86f, 0.12f, 0.09f, 0.50f, 0.18f, 3.9f, false },
        { 0.03f, 0.58f, 0.15f, 0.30f, 0.12f, 4.6f, false }, { 0.24f, 0.70f, 0.20f, 0.18f, 0.22f, 0.4f, true  },
    };
    const bool darkBg = p.bg.getBrightness() < 0.5f;
    const float a = (0.04f + 0.09f * density) * (0.8f + 0.5f * glow) * (darkBg ? 0.55f : 1.0f);
    for (const auto& s : slabs)
    {
        const float dx = std::sin (time * s.speed * 2.0f + s.phase) * 0.03f;
        const float dy = std::cos (time * s.speed * 1.4f + s.phase) * 0.025f;
        const juce::Rectangle<float> rect ((s.x + dx) * w, (s.y + dy) * h, s.w * w, s.h * h);
        g.setColour ((s.light ? p.glow : p.accentSoft).withAlpha (a * 0.2f));
        for (int k = 7; k >= 0; --k)
            g.fillRoundedRectangle (rect.expanded ((float) k * 3.0f), 4.0f + (float) k * 3.0f);
    }

    // horizon: two slow sine waves, filled below
    juce::Path line, fill;
    const float baseY = h * 0.68f;
    for (float x = 0.0f; x <= w + 8.0f; x += 8.0f)
    {
        const float t = x / w * juce::MathConstants<float>::twoPi;
        const float y = baseY + std::sin (t * 1.1f + time * 0.6f) * h * 0.025f
                              + std::sin (t * 2.6f - time * 0.4f) * h * 0.012f;
        if (x == 0.0f) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }
    fill = line;
    fill.lineTo (w + 8.0f, h);
    fill.lineTo (0.0f, h);
    fill.closeSubPath();
    g.setColour (p.fieldC.withAlpha (0.28f));
    g.fillPath (fill);
    g.setColour (p.glow.withAlpha (0.35f + 0.25f * glow));
    g.strokePath (line, juce::PathStrokeType (1.2f));

    g.setColour (p.dim);
    g.setFont (tracked (10.0f, true, 0.08f));
    g.drawText ("HAZE VOX  /  HAZE FIELD", 20, 14, 300, 14, juce::Justification::centredLeft);
    g.restoreState();

    g.setColour (p.cardLine);
    g.drawRoundedRectangle (r.reduced (0.5f), 18.0f, 1.0f);
}

//==============================================================================
// Editor
//==============================================================================
HazeVoxEditor::HazeVoxEditor (HazeVoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    lnf.setDark (proc.darkTheme);
    setLookAndFeel (&lnf);
    addAndMakeVisible (field);

    addKnob (pitch,    "pitch",    "Pitch",      "How far the voice moves. -3 st is the classic slowed sound (84% speed).");
    addKnob (formant,  "formant",  "Formant",    "Voice size without changing the note. Turn Tape link off to use it.");
    addKnob (glide,    "glide",    "Glide",      "How slowly the voice slides when Pitch or Formant changes. Longer = smoother sweeps.");
    addKnob (haze,     "haze",     "Haze",       "Master amount of space and darkness. 50% = knobs exactly as set.");
    addKnob (smooth,   "smooth",   "Smooth",     "Evens out loud and quiet words so nothing gets lost in the reverb.");
    addKnob (drive,    "drive",    "Drive",      "Warm tape-style saturation.");
    addKnob (warmth,   "warmth",   "Warmth",     "Adds body around 280 Hz.");
    addKnob (tone,     "tone",     "Tone",       "Rolls off the highs. Lower = darker, more late-night.");
    addKnob (deess,    "deess",    "De-ess",     "Softens harsh S sounds before they hit the reverb.");
    addKnob (width,    "width",    "Width",      "Adds two doubles a few cents up and down, panned left and right.");
    addKnob (dTime,    "dtime",    "Delay",      "Delay time in milliseconds (Sync off).");
    addKnob (dNote,    "dnote",    "Delay",      "Delay time locked to your song tempo (Sync on).");
    addKnob (dFb,      "dfb",      "Feedback",   "How many times the delay repeats.");
    addKnob (dMix,     "dmix",     "Delay mix",  "How loud the delay is.");
    addKnob (preDelay, "predelay", "Pre-delay",  "Gap before the reverb starts. Keeps your words clear inside the haze.");
    addKnob (vSize,    "vsize",    "Size",       "Size of the reverb space.");
    addKnob (decay,    "decay",    "Decay",      "How long the reverb tail rings, in seconds.");
    addKnob (vMix,     "vmix",     "Verb mix",   "How loud the reverb is.");
    addKnob (duck,     "duck",     "Duck",       "Pushes the delay and reverb down while you sing, so they bloom in the gaps.");
    addKnob (mix,      "mix",      "Mix",        "Blends the effects in. The pitch shift always stays on.");
    addKnob (out,      "out",      "Output",     "Final volume.");

    addAndMakeVisible (link);
    link.setName ("Tape link");
    linkAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "link", link);
    link.setTooltip ("On: formants move with the pitch, like a slowed-down record. Off: use the Formant knob.");

    addAndMakeVisible (sync);
    sync.setName ("Sync");
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "dsync", sync);
    sync.setTooltip ("Lock the delay to your project tempo.");

    // toolbar
    addAndMakeVisible (presetBox);
    presetBox.setName ("Presets");
    presetBox.setTooltip ("Factory sounds and your own saved presets.");
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id == openFolderId)
        {
            auto f = HazeVoxProcessor::getUserPresetFolder();
            f.createDirectory();
            f.revealToUser();
        }
        else if (id >= userIdBase)
        {
            const int idx = id - userIdBase;
            if (juce::isPositiveAndBelow (idx, userPresetFiles.size()))
                proc.loadUserPreset (userPresetFiles[idx]);
        }
        else if (id >= factoryIdBase)
            proc.setCurrentProgram (id - factoryIdBase);
        presetBox.setText (proc.presetName, juce::dontSendNotification);
    };
    populatePresets();

    saveButton.setButtonText ("SAVE");
    for (auto* b : { &saveButton, &aButton, &bButton, &copyButton, &viewButton, &themeButton })
        addAndMakeVisible (*b);
    saveButton.setName ("Save");
    saveButton.setTooltip ("Save the current sound as your own preset.");
    saveButton.onClick = [this] { showSaveDialog(); };
    aButton.setName ("A / B");
    bButton.setName ("A / B");
    aButton.setTooltip ("Compare two settings: A and B each remember their own knobs.");
    bButton.setTooltip ("Compare two settings: A and B each remember their own knobs.");
    copyButton.setName ("Copy");
    copyButton.setTooltip ("Copy the current side to the other side.");
    aButton.onClick = [this] { proc.selectSlot (0); refreshAB(); };
    bButton.onClick = [this] { proc.selectSlot (1); refreshAB(); };
    copyButton.onClick = [this] { proc.copyActiveToOther(); };

    viewButton.setName ("View");
    viewButton.setTooltip ("Simple shows the main knobs. Advanced shows every control.");
    viewButton.onClick = [this]
    {
        proc.advancedView = ! proc.advancedView;
        updateViewVisibility();
        resized();
        repaint();
    };
    themeButton.setName ("Theme");
    themeButton.setTooltip ("Switch between the light and dark look.");
    themeButton.onClick = [this]
    {
        proc.darkTheme = ! proc.darkTheme;
        applyTheme();
    };
    refreshAB();

    for (auto* m : { &inMeter, &outMeter, &smoothMeter, &deessMeter, &duckMeter })
        addAndMakeVisible (*m);

    hintText = defaultHint;
    applyTheme();
    setSize (W, H);
    updateViewVisibility();
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

HazeVoxEditor::~HazeVoxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HazeVoxEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& text, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setName (text);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    if (auto* param = proc.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    k.label.setText (text.toUpperCase(), juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);
    allKnobs.push_back (&k);
}

void HazeVoxEditor::placeKnob (Knob& k, juce::Rectangle<int> area, Size size)
{
    const float labelPt = size == Size::big ? 14.0f : (size == Size::medium ? 12.5f : 11.0f);
    const int labelH = (int) labelPt + 6;
    const int valueH = size == Size::small ? 16 : 18;

    k.label.setFont (tracked (labelPt, true, 0.08f));
    k.label.setBounds (area.removeFromTop (labelH));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, area.getWidth(), valueH);
    k.slider.setBounds (area);
}

void HazeVoxEditor::applyTheme()
{
    lnf.setDark (proc.darkTheme);
    themeButton.setButtonText (proc.darkTheme ? "LIGHT" : "DARK");
    const auto& p = lnf.palette();
    for (auto* k : allKnobs)
    {
        k->slider.setColour (juce::Slider::textBoxTextColourId, p.dim);
        k->label.setColour (juce::Label::textColourId, p.ink);
    }
    sendLookAndFeelChange();
    repaint();
}

void HazeVoxEditor::populatePresets()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), factoryIdBase + i);

    userPresetFiles = proc.getUserPresets();
    presetBox.addSeparator();
    presetBox.addSectionHeading ("Your presets");
    for (int i = 0; i < userPresetFiles.size(); ++i)
        presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    presetBox.addItem ("Open presets folder", openFolderId);

    presetBox.setText (proc.presetName, juce::dontSendNotification);
}

void HazeVoxEditor::showSaveDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.presetName + " (mine)");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            if (proc.saveUserPreset (w->getTextEditorContents ("name")))
                populatePresets();
        }
    }), true);
}

void HazeVoxEditor::refreshAB()
{
    aButton.setToggleState (proc.getActiveSlot() == 0, juce::dontSendNotification);
    bButton.setToggleState (proc.getActiveSlot() == 1, juce::dontSendNotification);
    copyButton.setButtonText (proc.getActiveSlot() == 0 ? "COPY A TO B" : "COPY B TO A");
}

void HazeVoxEditor::updateViewVisibility()
{
    const bool adv = proc.advancedView;
    viewButton.setButtonText (adv ? "SIMPLE" : "ADVANCED");

    for (auto* k : { &formant, &smooth, &drive, &warmth, &tone, &deess, &width, &dTime, &dNote,
                     &dFb, &dMix, &preDelay, &vSize, &decay, &vMix, &duck })
    {
        k->slider.setVisible (adv);
        k->label.setVisible (adv);
    }
    sync.setVisible (adv);
    timerCallback(); // fixes delay knob + formant state immediately
}

// Shows the guide text for whatever control the mouse is over.
void HazeVoxEditor::updateHint()
{
    juce::String text = defaultHint;
    for (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
         c != nullptr && c != this && isParentOf (c); c = c->getParentComponent())
    {
        if (auto* tc = dynamic_cast<juce::SettableTooltipClient*> (c))
            if (tc->getTooltip().isNotEmpty())
            {
                text = c->getName().toUpperCase() + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + tc->getTooltip();
                break;
            }
    }
    if (text != hintText)
    {
        hintText = text;
        repaint (hintArea);
    }
}

void HazeVoxEditor::timerCallback()
{
    const float outPeak = proc.meterOut.exchange (0.0f);
    inMeter.setValue     (proc.meterIn.exchange (0.0f));
    outMeter.setValue    (outPeak);
    smoothMeter.setValue (proc.grSmooth.exchange (0.0f));
    deessMeter.setValue  (proc.grDeess.exchange (0.0f));
    duckMeter.setValue   (proc.grDuck.exchange (0.0f));

    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.1, (now - lastTick) * 0.001);
    lastTick = now;
    field.advance (dt, outPeak, (float) haze.slider.getValue());

    const bool adv = proc.advancedView;
    const bool synced = sync.getToggleState();
    dTime.slider.setVisible (adv && ! synced); dTime.label.setVisible (adv && ! synced);
    dNote.slider.setVisible (adv && synced);   dNote.label.setVisible (adv && synced);
    formant.slider.setEnabled (! link.getToggleState());
    formant.label.setAlpha (link.getToggleState() ? 0.4f : 1.0f);

    if (! presetBox.isPopupActive() && presetBox.getText() != proc.presetName)
        presetBox.setText (proc.presetName, juce::dontSendNotification);

    updateHint();
}

//==============================================================================
void HazeVoxEditor::paint (juce::Graphics& g)
{
    const auto& p = lnf.palette();
    g.fillAll (p.bg);

    // title
    g.setColour (p.ink);
    g.setFont (tracked (34.0f, true, 0.04f));
    g.drawText ("HAZE VOX", 24, 18, 230, 48, juce::Justification::centredLeft);

    auto drawCard = [&] (juce::Rectangle<int> r, float radius)
    {
        g.setColour (p.card);
        g.fillRoundedRectangle (r.toFloat(), radius);
        g.setColour (p.cardLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), radius, 1.0f);
    };

    // guide bar
    drawCard (hintArea, 12.0f);
    g.setColour (p.accentSoft);
    g.fillEllipse ((float) hintArea.getX() + 16.0f, (float) hintArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (hintText == defaultHint ? p.dim : p.ink);
    g.setFont (hazeFont (13.0f, true));
    g.drawFittedText (hintText, hintArea.reduced (32, 4).withTrimmedRight (-12), juce::Justification::centredLeft, 2);

    // toolbar
    drawCard (toolbarArea, 14.0f);
    g.setColour (p.dim);
    g.setFont (tracked (10.5f, true, 0.08f));
    g.drawText ("LATE-NIGHT VOCAL HAZE", toolbarArea.getX() + 20, toolbarArea.getY(), 200, toolbarArea.getHeight(),
                juce::Justification::centredLeft);

    // advanced cards
    for (auto& [r, title] : cards)
    {
        drawCard (r, 14.0f);
        g.setColour (p.dim);
        g.setFont (tracked (10.0f, true, 0.08f));
        g.drawText (title, r.getX() + 16, r.getY() + 9, 200, 14, juce::Justification::centredLeft);
    }
}

void HazeVoxEditor::resized()
{
    cards.clear();

    // header: title, guide bar, theme
    themeButton.setBounds (W - 24 - 96, 22, 96, 40);
    hintArea = { 262, 20, themeButton.getX() - 12 - 262, 44 };

    // toolbar
    toolbarArea = { 16, 80, W - 32, 56 };
    auto tb = toolbarArea.reduced (12, 11);
    viewButton.setBounds (tb.removeFromRight (112));
    tb.removeFromLeft (200);
    presetBox.setBounds (tb.removeFromLeft (250));
    tb.removeFromLeft (8);
    saveButton.setBounds (tb.removeFromLeft (72));
    tb.removeFromLeft (16);
    aButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (6);
    bButton.setBounds (tb.removeFromLeft (40));
    tb.removeFromLeft (6);
    copyButton.setBounds (tb.removeFromLeft (124));

    auto area = juce::Rectangle<int> (16, 148, W - 32, H - 148 - 16);

    // place a component centred at a relative spot in a rectangle
    auto at = [] (juce::Rectangle<int> r, float fx, float fy, int w, int h)
    {
        return juce::Rectangle<int> (w, h).withCentre ({ r.getX() + juce::roundToInt ((float) r.getWidth() * fx),
                                                         r.getY() + juce::roundToInt ((float) r.getHeight() * fy) });
    };

    if (! proc.advancedView)
    {
        // ---------------- Simple view: everything floats on the haze field ----------------
        field.setBounds (area);
        auto meters = area.removeFromBottom (44).reduced (24, 12);
        for (auto* m : { &inMeter, &outMeter, &smoothMeter, &deessMeter, &duckMeter })
        {
            m->setBounds (meters.removeFromLeft (150));
            meters.removeFromLeft (18);
        }
        auto f = area.withTrimmedTop (20);
        placeKnob (pitch, at (f, 0.18f, 0.38f, 170, 196), Size::big);
        link.setBounds (at (f, 0.18f, 0.77f, 136, 32));
        placeKnob (glide, at (f, 0.34f, 0.78f, 100, 116), Size::small);
        placeKnob (haze,  at (f, 0.50f, 0.44f, 220, 250), Size::big);
        placeKnob (mix,   at (f, 0.74f, 0.32f, 130, 150), Size::medium);
        placeKnob (out,   at (f, 0.86f, 0.70f, 130, 150), Size::medium);
        return;
    }

    // ---------------- Advanced view ----------------
    auto fieldArea = area.removeFromTop (216);
    field.setBounds (fieldArea);
    auto f = fieldArea.reduced (16, 10).withTrimmedTop (24);
    const int fs = f.getWidth() / 7;
    placeKnob (pitch,   f.removeFromLeft (fs).reduced (4, 0), Size::medium);
    placeKnob (formant, f.removeFromLeft (fs).reduced (4, 0), Size::medium);
    placeKnob (glide,   f.removeFromLeft (fs).reduced (4, 0), Size::medium);
    link.setBounds (f.removeFromLeft (fs).withSizeKeepingCentre (128, 32));
    placeKnob (haze,    f.removeFromLeft (fs).reduced (0, 0), Size::big);
    placeKnob (mix,     f.removeFromLeft (fs).reduced (4, 0), Size::medium);
    placeKnob (out,     f.reduced (4, 0), Size::medium);

    area.removeFromTop (12);
    auto row1 = area.removeFromTop ((area.getHeight() - 12) / 2);
    area.removeFromTop (12);
    auto row2 = area;

    auto inner = [] (juce::Rectangle<int> r) { return r.reduced (12, 6).withTrimmedTop (18); };

    auto colorCard = row1.removeFromLeft (540);
    row1.removeFromLeft (12);
    cards.push_back ({ colorCard, "COLOR" });
    cards.push_back ({ row1, "METERS" });

    auto c = inner (colorCard);
    const int cs = c.getWidth() / 5;
    for (auto* k : { &smooth, &drive, &warmth, &tone, &deess })
        placeKnob (*k, c.removeFromLeft (cs), Size::small);

    auto m = inner (row1).reduced (8, 0);
    const int mh = m.getHeight() / 5;
    for (auto* mt : { &inMeter, &outMeter, &smoothMeter, &deessMeter, &duckMeter })
        mt->setBounds (m.removeFromTop (mh));

    cards.push_back ({ row2, "SPACE" });
    auto s = inner (row2);
    const int ss = s.getWidth() / 10;
    placeKnob (width, s.removeFromLeft (ss), Size::small);
    sync.setBounds (s.removeFromLeft (ss).withSizeKeepingCentre (ss - 8, 30));
    auto delaySlot = s.removeFromLeft (ss);
    placeKnob (dTime, delaySlot, Size::small);
    placeKnob (dNote, delaySlot, Size::small);
    for (auto* k : { &dFb, &dMix, &preDelay, &vSize, &decay, &vMix, &duck })
        placeKnob (*k, s.removeFromLeft (ss), Size::small);
}
