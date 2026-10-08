// SUNRIZE · REZONANZA
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "Dsp.h"

namespace sz
{
struct Sound { const char* name; float notes[4]; Wave a, b; };
extern const Sound SOUNDS[10];
}

class SunrizeProcessor : public juce::AudioProcessor
{
public:
    static const char* knobIds[10];
    static const char* knobNames[10];
    static const float knobDefaults[10];

    SunrizeProcessor();
    ~SunrizeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SUNRIZE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // editor helpers
    void toggleLatch() { latched = ! latched.load(); }
    bool isPlaying() const { return sounding.load(); }
    int readScope (float* dst, int n) const; // copies the latest n output samples, returns n
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    struct OneShot
    {
        bool active = false, click = false;
        float t = 0, endT = 0, ph = 0, rate = 1, gain = 1;
        float f0 = 0, f1 = 0, sweep = 0, f2 = 0, sweep2 = 0; // freq: f0 -> f1 over sweep, then -> f2 over sweep2
        float g0t = 0, gPeak = 0, gAtt = 0, gEnd = 0;       // gain: 1e-4 at g0t, exp to gPeak over gAtt, exp to 1e-4 at gEnd
    };
    void tick();
    void spawnChirp();
    void spawnDrops();
    void spawnCrackle();
    float noteFreq (int i) const { return freq[(size_t) i].v * transposeRatio; }

    std::array<std::atomic<float>*, 10> kp {};
    std::atomic<float>* sunP = nullptr; std::atomic<float>* outP = nullptr; std::atomic<float>* soundP = nullptr;
    std::atomic<bool> latched { false }, sounding { false };
    int heldNotes = 0, lastNote = 60, curSound = -1;
    float transposeRatio = 1.0f;
    bool gate = false;

    double sr = 44100.0;
    float timeNow = 0;
    int tickCount = 0, tickLen = 1455;
    sz::Rng rng;

    // oscillators
    std::array<sz::Target, 4> freq;
    sz::Wave waveA = sz::SAW, waveB = sz::TRI;
    double phS[8] = {}, phT[8] = {}, phSub[4] = {}, phShim[4] = {}, phLight[4] = {}, phLfo[4] = {}, phWow = 0, phCh[3] = {};
    sz::Target gSaw, gTri, gSub, qTone, wowAmt, spread, wetG, dryG, shimG, fbG, echoG, chDepth, chWet, hissG, lightG, breezeG, airDb, outG, cutoff, master;
    float cloudUntil = -1, cloudBack = 0;
    sz::Biquad filter, echoLp, hissBp, breezeBp, air[2];
    std::vector<float> chBuf, echoBuf, clickBuf;
    int chW = 0, echoW = 0;
    std::array<OneShot, 48> shots;
    sz::Compressor comp;
    juce::dsp::Convolution reverb;
    juce::AudioBuffer<float> convIn, work;

    std::vector<float> scope;
    std::atomic<int> scopeW { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SunrizeProcessor)
};
