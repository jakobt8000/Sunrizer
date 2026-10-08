// SUNRIZE · REZONANZA
#include "PluginEditor.h"
#include "BinaryData.h"

namespace szui
{
static const juce::Colour ink (0xff111111), panel (0xfff7f7f7), ring (0xffa3a3a0), labelC (0xff333333), greyLabel (0xff9a9a98);

static juce::Font mono (float px, float spacingPx = 0)
{
    static juce::Typeface::Ptr tf = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
    juce::Font f { juce::FontOptions (tf).withPointHeight (px) };
    if (spacingPx != 0) f.setExtraKerningFactor (spacingPx / px);
    return f;
}

static void needle (juce::Graphics& g, float cx, float cy, float r0, float r1, float w, float v, juce::Colour c, float corner)
{
    juce::Path p;
    p.addRoundedRectangle (-w * 0.5f, -r1, w, r1 - r0, corner);
    p.applyTransform (juce::AffineTransform::rotation ((-135.0f + v * 270.0f) * juce::MathConstants<float>::pi / 180.0f).translated (cx, cy));
    g.setColour (c);
    g.fillPath (p);
}

// ---------- Knob ----------
Knob::Knob (juce::RangedAudioParameter& p, juce::String l, bool f)
    : param (p), attach (p, [this] (float v) { value = param.convertTo0to1 (v); repaint(); }), label (l), filled (f)
{
    attach.sendInitialUpdate();
}
void Knob::paint (juce::Graphics& g)
{
    const float sz = 85, x = (getWidth() - sz) * 0.5f;
    if (filled) { g.setColour (juce::Colours::white); g.fillEllipse (x, 0, sz, sz); }
    g.setColour (ink); g.drawEllipse (x + 0.5f, 0.5f, sz - 1, sz - 1, 1.0f);
    needle (g, x + sz * 0.5f, sz * 0.5f, 2.5f, 32.5f, 3, value, ink, 0);
    g.setColour (labelC); g.setFont (mono (11, 1));
    g.drawText (label, juce::Rectangle<float> (0, sz + 6, (float) getWidth(), 14), juce::Justification::centredTop);
}
void Knob::mouseDown (const juce::MouseEvent&) { start = value; attach.beginGesture(); }
void Knob::mouseDrag (const juce::MouseEvent& e)
{
    value = juce::jlimit (0.0f, 1.0f, start - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.0015f : 0.006f));
    attach.setValueAsPartOfGesture (param.convertFrom0to1 (value));
    repaint();
}
void Knob::mouseUp (const juce::MouseEvent&) { attach.endGesture(); }
void Knob::mouseDoubleClick (const juce::MouseEvent&) { attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); }

// ---------- Sun ----------
Sun::Sun (juce::RangedAudioParameter& p, SunrizeProcessor& pr)
    : param (p), proc (pr), attach (p, [this] (float v) { value = param.convertTo0to1 (v); repaint(); })
{
    attach.sendInitialUpdate();
}
void Sun::paint (juce::Graphics& g)
{
    g.setColour (ring);
    g.drawEllipse (3, 3, 214, 214, 6);
    needle (g, 110, 110, 14, 73, 8, value, juce::Colour (0xff2a2a2a), 4);
}
void Sun::mouseDown (const juce::MouseEvent&) { start = value; moved = false; attach.beginGesture(); }
void Sun::mouseDrag (const juce::MouseEvent& e)
{
    if (std::abs (e.getDistanceFromDragStartY()) > 3) moved = true;
    if (! moved) return;
    value = juce::jlimit (0.0f, 1.0f, start - e.getDistanceFromDragStartY() * 0.006f);
    attach.setValueAsPartOfGesture (param.convertFrom0to1 (value));
    repaint();
}
void Sun::mouseUp (const juce::MouseEvent&)
{
    attach.endGesture();
    if (! moved) proc.toggleLatch();
}

// ---------- Info ----------
Info::Info()
{
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
}
void Info::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::white); g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.08f)); g.drawRect (r, 1.0f);
    const float lw = 420, lh = lw * (float) logo.getHeight() / (float) std::max (1, logo.getWidth());
    g.drawImage (logo, juce::Rectangle<float> (16, (r.getHeight() - lh) * 0.5f, lw, lh), juce::RectanglePlacement::stretchToFit);
    juce::Rectangle<float> side (452, 0, r.getWidth() - 452, r.getHeight());
    g.setColour (panel); g.fillRect (side);
    const char* rows[8][2] = { { "TYPE", "PAD SYNTH" }, { "SOUNDS", "10" }, { "KNOBS", "12" }, { "OSCILLATORS", "28" },
                               { "TUNING", "A = 440 HZ" }, { "VERSION", "0.2" }, { "BIRDS", "INCLUDED" }, { "WEATHER", "VARIABLE" } };
    g.setFont (mono (11, 1));
    for (int i = 0; i < 8; ++i)
    {
        const float y = 16 + i * (133.0f / 7.5f);
        g.setColour (greyLabel); g.drawText (rows[i][0], juce::Rectangle<float> (side.getX() + 20, y, 112, 14), juce::Justification::centredLeft);
        g.setColour (labelC); g.drawText (rows[i][1], juce::Rectangle<float> (side.getX() + 132, y, 120, 14), juce::Justification::centredLeft);
    }
    g.setColour (ink);
    g.drawLine (r.getWidth() - 19, 9, r.getWidth() - 9, 19, 1.5f);
    g.drawLine (r.getWidth() - 9, 9, r.getWidth() - 19, 19, 1.5f);
}
void Info::mouseDown (const juce::MouseEvent& e)
{
    if (e.x > getWidth() - 28 && e.y < 28) { setVisible (false); return; }
    dragger.startDraggingComponent (this, e);
}
void Info::mouseDrag (const juce::MouseEvent& e) { dragger.dragComponent (this, e, nullptr); }

// ---------- Content ----------
Content::Content (SunrizeProcessor& p) : proc (p)
{
    auto& ap = proc.apvts;
    sun = std::make_unique<Sun> (*ap.getParameter ("sun"), proc);
    addAndMakeVisible (*sun); sun->setBounds (30, 30, 220, 220);
    for (int i = 0; i < 10; ++i)
    {
        auto* k = knobs.add (new Knob (*ap.getParameter (SunrizeProcessor::knobIds[i]), SunrizeProcessor::knobNames[i], true));
        addAndMakeVisible (k);
        k->setBounds (940 + (i % 5) * 105 - 10, i < 5 ? 30 : 145, 105, 106);
    }
    output = std::make_unique<Knob> (*ap.getParameter ("output"), "OUTPUT", false);
    addAndMakeVisible (*output); output->setBounds (1495, 30, 105, 106);
    sound = dynamic_cast<juce::AudioParameterChoice*> (ap.getParameter ("sound"));
    addChildComponent (info); info.setBounds (760, 96, 702, 165);
    ys.assign (180, 0.0f);
    startTimerHz (30);
}

void Content::timerCallback()
{
    // one thin line: a slice of the real waveform, locked to a zero crossing
    float buf[2048];
    proc.readScope (buf, 2048);
    int s0 = 0;
    for (int i = 1; i < 1024; ++i) if (buf[i - 1] < 0 && buf[i] >= 0) { s0 = i; break; }
    const float step = 900.0f / 180.0f;
    for (int c = 0; c < 180; ++c) ys[(size_t) c] = ys[(size_t) c] * 0.3f + buf[std::min (2047, (int) (s0 + c * step))] * 0.7f;
    repaint (310, 30, 600, 182);
    static int lastSound = -1;
    if (sound != nullptr && sound->getIndex() != lastSound) { lastSound = sound->getIndex(); repaint(); }
}

void Content::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
    g.setColour (panel);
    g.fillRect (0, 0, 280, 280);
    g.fillRect (1475, 0, 145, 280);

    // pixel trace: height follows the level, quantised to 4 px
    const float mid = 110, A = 106, FULL = 0.22f, PX = 4, step = 720.0f / 180.0f, sx = 600.0f / 720.0f, sy = 182.0f / 220.0f;
    auto q = [&] (float v) { const float a = std::min (1.0f, std::pow (std::abs (v) / FULL, 0.85f)) * (v < 0 ? -1.0f : 1.0f); return std::round ((mid - a * A) / PX) * PX; };
    juce::Path line;
    for (int c = 0; c < 180; ++c)
    {
        const float x = std::round (c * step), x2 = std::round ((c + 1) * step), y = q (ys[(size_t) c]);
        if (c == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        line.lineTo (x2, y);
    }
    line.applyTransform (juce::AffineTransform::scale (sx, sy).translated (310, 30));
    g.setColour (ink);
    g.strokePath (line, juce::PathStrokeType (1.7f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

    // sound selector
    auto chevron = [&] (float cx, bool left) {
        juce::Path p;
        if (left) { p.startNewSubPath (cx + 4, 218 + 4); p.lineTo (cx - 6, 218 + 16); p.lineTo (cx + 4, 218 + 28); }
        else { p.startNewSubPath (cx - 4, 218 + 4); p.lineTo (cx + 6, 218 + 16); p.lineTo (cx - 4, 218 + 28); }
        g.strokePath (p, juce::PathStrokeType (3.0f));
    };
    chevron (382, true); chevron (838, false);
    const int idx = sound ? sound->getIndex() : 0;
    g.setFont (mono (19, 4));
    g.drawText (juce::String (idx + 1).paddedLeft ('0', 2) + "  " + sz::SOUNDS[idx].name, juce::Rectangle<float> (410, 218, 400, 32), juce::Justification::centred);

    // info button
    g.drawEllipse (1505.5f, 145.5f, 84, 84, 1.0f);
    g.setFont (mono (22));
    g.drawText ("i", juce::Rectangle<float> (1505, 145, 85, 85), juce::Justification::centred);
}

void Content::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (sound != nullptr && p.y >= 218 && p.y < 250)
    {
        int idx = sound->getIndex();
        if (p.x >= 354 && p.x < 410) idx = (idx + 9) % 10;
        else if (p.x >= 810 && p.x < 866) idx = (idx + 1) % 10;
        else return;
        sound->beginChangeGesture();
        *sound = idx;
        sound->endChangeGesture();
        repaint();
        return;
    }
    if (juce::Rectangle<float> (1505, 145, 85, 85).contains (p)) { info.setVisible (! info.isVisible()); info.toFront (false); }
}
} // namespace szui

SunrizeEditor::SunrizeEditor (SunrizeProcessor& p) : AudioProcessorEditor (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (810, 140, 2430, 420);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1620.0 / 280.0);
    setSize (1296, 224);
}

void SunrizeEditor::resized()
{
    content.setBounds (0, 0, 1620, 280);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / 1620.0f));
}
