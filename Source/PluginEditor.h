// SUNRIZE · REZONANZA
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace szui
{
// Round SUNRIZE knob (1 px ring, 3 px needle) bound to a parameter
class Knob : public juce::Component
{
public:
    Knob (juce::RangedAudioParameter& p, juce::String label, bool filled);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attach;
    juce::String label; bool filled;
    float value = 0, start = 0;
};

// The big sun: drag = filter, click = start / stop
class Sun : public juce::Component
{
public:
    Sun (juce::RangedAudioParameter& p, SunrizeProcessor& proc);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    SunrizeProcessor& proc;
    juce::ParameterAttachment attach;
    float value = 0.3f, start = 0; bool moved = false;
};

class Info : public juce::Component
{
public:
    Info();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    juce::Image logo;
    juce::ComponentDragger dragger;
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (SunrizeProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    SunrizeProcessor& proc;
    std::unique_ptr<Sun> sun;
    juce::OwnedArray<Knob> knobs;
    std::unique_ptr<Knob> output;
    Info info;
    std::vector<float> ys;
    juce::AudioParameterChoice* sound = nullptr;
};
} // namespace szui

class SunrizeEditor : public juce::AudioProcessorEditor
{
public:
    explicit SunrizeEditor (SunrizeProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::white); }
private:
    szui::Content content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SunrizeEditor)
};
