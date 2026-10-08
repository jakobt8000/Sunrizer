// SUNRIZE · REZONANZA
// The pad engine ported 1:1 from the browser prototype.
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace sz
{
const Sound SOUNDS[10] = {
    { "MORNING DEW", { 220.0f, 246.94f, 261.63f, 329.63f }, SAW, TRI },
    { "SEA BREEZE", { 261.63f, 329.63f, 392.0f, 493.88f }, TRI, SINE },
    { "CATHEDRAL", { 146.83f, 220.0f, 261.63f, 329.63f }, SQUARE, TRI },
    { "GLASSHOUSE", { 329.63f, 415.3f, 493.88f, 622.25f }, SINE, TRI },
    { "FOG", { 174.61f, 220.0f, 261.63f, 493.88f }, SAW, SAW },
    { "NORTHERN LIGHTS", { 233.08f, 293.66f, 349.23f, 440.0f }, SQUARE, SINE },
    { "DUSK", { 196.0f, 233.08f, 293.66f, 349.23f }, SAW, TRI },
    { "STILL WATER", { 261.63f, 293.66f, 392.0f, 523.25f }, SINE, SINE },
    { "WARM AIR", { 155.56f, 196.0f, 233.08f, 293.66f }, TRI, SQUARE },
    { "DISTANT RADIO", { 220.0f, 261.63f, 329.63f, 392.0f }, SQUARE, SQUARE }
};
}

const char* SunrizeProcessor::knobIds[10] = { "tone", "weather", "width", "birds", "bloom", "afterglow", "drift", "dust", "light", "drops" };
const char* SunrizeProcessor::knobNames[10] = { "TONE", "WEATHER", "WIDTH", "BIRDS", "BLOOM", "AFTERGLOW", "DRIFT", "DUST", "LIGHT", "DROPS" };
const float SunrizeProcessor::knobDefaults[10] = { 0.35f, 0.25f, 0.4f, 0.3f, 0.55f, 0.4f, 0.4f, 0.2f, 0.45f, 0.0f };

juce::AudioProcessorValueTreeState::ParameterLayout SunrizeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "sun", 1 }, "Sun", 0.0f, 1.0f, 0.3f));
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "output", 1 }, "Output", 0.0f, 1.0f, 0.75f));
    juce::StringArray names;
    for (auto& s : sz::SOUNDS) names.add (s.name);
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "sound", 1 }, "Sound", names, 0));
    for (int i = 0; i < 10; ++i)
    {
        juce::String n (knobNames[i]);
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { knobIds[i], 1 }, n.substring (0, 1) + n.substring (1).toLowerCase(), 0.0f, 1.0f, knobDefaults[i]));
    }
    return l;
}

SunrizeProcessor::SunrizeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUNRIZE", createLayout())
{
    for (int i = 0; i < 10; ++i) kp[(size_t) i] = apvts.getRawParameterValue (knobIds[i]);
    sunP = apvts.getRawParameterValue ("sun");
    outP = apvts.getRawParameterValue ("output");
    soundP = apvts.getRawParameterValue ("sound");
    scope.assign (4096, 0.0f);
}

bool SunrizeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SunrizeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const float fs = (float) sr;
    tickLen = std::max (1, (int) std::round (sr * 0.033));
    convIn.setSize (2, samplesPerBlock);
    work.setSize (2, samplesPerBlock);

    // BLOOM: 4.5 s noise impulse (decay ^2.2), normalised like Web Audio's ConvolverNode
    const int len = (int) (sr * 4.5);
    juce::AudioBuffer<float> ir (2, len);
    sz::Rng r; double pow = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < len; ++i) { const float x = r.next() * std::pow (1.0f - (float) i / (float) len, 2.2f); ir.setSample (ch, i, x); pow += x * x; }
    const float power = (float) std::max (0.000125, std::sqrt (pow / (2.0 * len)));
    ir.applyGain (0.00125f / power * (float) (44100.0 / sr));
    reverb.reset();
    reverb.prepare ({ sr, (juce::uint32) samplesPerBlock, 2 });
    reverb.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);

    chBuf.assign ((size_t) (sr * 0.06) + 4, 0.0f); chW = 0;
    echoBuf.assign ((size_t) (sr * 0.5) + 4, 0.0f); echoW = 0;
    clickBuf.resize (220);
    for (int i = 0; i < 220; ++i) clickBuf[(size_t) i] = r.next() * std::pow (1.0f - i / 220.0f, 4.0f);
    echoLp.set (sz::LP, 2500, 1, fs);
    hissBp.set (sz::BP, 3200, 0.6f, fs);
    breezeBp.set (sz::BP, 8500, 1.2f, fs);
    comp.prepare (fs, -22, 3, 18, 0.01f, 0.35f);
    setLatencySamples (comp.len);

    curSound = -1;
    master.v = master.target = 0;
    outG.v = outG.target = 0.9f;
    cutoff.v = cutoff.target = 120 + 0.09f * 9000;
    for (auto& s : shots) s.active = false;
    timeNow = 0; tickCount = 0;
}

void SunrizeProcessor::spawnChirp()
{
    for (auto& s : shots)
        if (! s.active)
        {
            const float f = noteFreq ((int) (rng.uni() * 3.999f)) * (rng.uni() < 0.5f ? 4.0f : 8.0f);
            s = {}; s.active = true;
            s.f0 = f; s.f1 = f * (1.08f + rng.uni() * 0.2f); s.sweep = 0.1f;
            s.gPeak = 0.08f; s.gAtt = 0.01f; s.gEnd = 0.18f + rng.uni() * 0.12f; s.endT = 0.35f;
            return;
        }
}

void SunrizeProcessor::spawnDrops()
{
    const int count = 1 + (rng.uni() < 0.35f ? 1 + (int) (rng.uni() * 1.999f) : 0);
    float t = 0.01f;
    for (int c = 0; c < count; ++c)
    {
        for (auto& s : shots)
            if (! s.active)
            {
                const float f = noteFreq ((int) (rng.uni() * 3.999f)) * (rng.uni() < 0.5f ? 2.0f : 4.0f);
                const float sweep = 0.025f + rng.uni() * 0.035f;
                s = {}; s.active = true; s.t = -t;
                s.f0 = f * 0.45f; s.f1 = f; s.sweep = sweep; s.f2 = f * 1.04f; s.sweep2 = 0.15f;
                s.gPeak = 0.14f; s.gAtt = 0.004f; s.gEnd = 0.12f + rng.uni() * 0.12f; s.endT = 0.3f;
                break;
            }
        t += 0.06f + rng.uni() * 0.18f;
    }
}

void SunrizeProcessor::spawnCrackle()
{
    for (auto& s : shots)
        if (! s.active)
        {
            s = {}; s.active = true; s.click = true;
            s.rate = 0.5f + rng.uni() * 1.5f;
            s.gain = (0.03f + rng.uni() * rng.uni() * 0.3f) * (0.4f + kp[7]->load());
            s.endT = 1e9f;
            return;
        }
}

// control rate (every 33 ms, like the prototype's timer): parameters + random events
void SunrizeProcessor::tick()
{
    const float fs = (float) sr;
    float kv[10];
    for (int i = 0; i < 10; ++i) kv[i] = kp[(size_t) i]->load();
    const float tc = 0.06f;
    gTri.to ((1 - kv[0]) * 1.1f, tc, fs);
    gSaw.to (kv[0] * 0.65f, tc, fs);
    gSub.to (std::max (0.0f, kv[0] - 0.5f) * 0.5f, tc, fs);
    qTone.to (0.6f + kv[0] * kv[0] * 5, tc, fs);
    wowAmt.to (kv[1] * kv[1] * 40, tc, fs);
    spread.to (2 + kv[2] * kv[2] * 43, tc, fs);
    wetG.to (kv[4] * 1.3f, tc, fs);
    dryG.to (1 - kv[4] * 0.5f, tc, fs);
    shimG.to (kv[4] * kv[4] * 0.22f, tc, fs);
    fbG.to (kv[5] * 0.6f, tc, fs);
    echoG.to (kv[5] * 0.55f, tc, fs);
    chDepth.to (0.0004f + kv[6] * kv[6] * 0.007f, tc, fs);
    chWet.to (kv[6] * 0.7f, tc, fs);
    hissG.to (kv[7] * kv[7] * 0.07f, tc, fs);
    lightG.to (kv[8] * kv[8] * 0.11f, tc, fs);
    breezeG.to (kv[8] * kv[8] * 0.05f, tc, fs);
    airDb.to (kv[8] * 4, tc, fs);
    const float ov = outP->load();
    outG.to (ov * ov * 1.6f, tc, fs);

    const float h = sunP->load() * 100.0f, o = h / 100.0f;
    const float c = 120.0f + o * o * 9000.0f;
    if (timeNow >= cloudUntil)
    {
        if (cloudUntil >= 0) { cutoff.to (c, 0.3f, fs); cloudUntil = -1; cloudBack = timeNow + 1.5f; }
        else if (timeNow > cloudBack) cutoff.to (c, 0.04f + kv[2] * 0.5f, fs);
    }

    // sound bank
    const int snd = juce::jlimit (0, 9, (int) soundP->load());
    if (snd != curSound)
    {
        const bool first = curSound < 0;
        curSound = snd;
        waveA = sz::SOUNDS[snd].a; waveB = sz::SOUNDS[snd].b;
        for (int i = 0; i < 4; ++i)
        {
            freq[(size_t) i].to (sz::SOUNDS[snd].notes[i], first ? 0.0f : 0.12f, fs);
            if (first) freq[(size_t) i].v = sz::SOUNDS[snd].notes[i];
        }
    }

    // start / stop (big knob latch or held MIDI notes)
    const bool shouldPlay = latched.load() || heldNotes > 0;
    if (shouldPlay != gate)
    {
        gate = shouldPlay;
        if (gate) { const float att = 0.1f + kv[2] * kv[2] * 5; master.to (0.1f, att / 3, fs); }
        else { const float rel = 0.5f + kv[5] * kv[5] * 14; master.to (0.0f, rel / 4, fs); }
    }
    sounding = gate;

    if (gate)
    {
        if (rng.uni() < kv[3] * kv[3] * 0.35f) spawnChirp();
        if (rng.uni() < kv[7] * kv[7] * 0.9f) spawnCrackle();
        if (rng.uni() < kv[9] * kv[9] * 0.12f) spawnDrops();
        // WEATHER: clouds = deep random dips in the filter
        if (cloudUntil < 0 && rng.uni() < kv[1] * kv[1] * 0.06f)
        {
            cutoff.to (std::max (90.0f, c * (0.05f + 0.2f * rng.uni())), 0.06f, fs);
            cloudUntil = timeNow + 0.2f + rng.uni() * 0.8f;
        }
    }
}

void SunrizeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (work.getNumSamples() < n) { work.setSize (2, n, false, false, true); convIn.setSize (2, n, false, false, true); }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { ++heldNotes; lastNote = m.getNoteNumber(); transposeRatio = std::pow (2.0f, (lastNote - 60) / 12.0f); }
        else if (m.isNoteOff()) heldNotes = std::max (0, heldNotes - 1);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) heldNotes = 0;
    }

    const float fs = (float) sr, dt = 1.0f / fs;
    const float chBase[3] = { 0.013f, 0.019f, 0.025f }, chRate[3] = { 0.31f, 0.52f, 0.83f }, chPan[3] = { -0.85f, 0.0f, 0.85f };
    float chL[3], chR[3];
    for (int k = 0; k < 3; ++k) { const float x = (chPan[k] + 1) * 0.5f; chL[k] = std::cos (x * sz::kPi * 0.5f); chR[k] = std::sin (x * sz::kPi * 0.5f); }
    const int chLen = (int) chBuf.size(), echoLen = (int) echoBuf.size();
    const int echoD = (int) std::round (0.42 * sr);

    float* pL = work.getWritePointer (0); float* pR = work.getWritePointer (1);
    float* cL = convIn.getWritePointer (0); float* cR = convIn.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        if (tickCount-- <= 0) { tick(); tickCount = tickLen; }
        timeNow += dt;

        // --- oscillators ---
        phWow += 0.55 * dt; if (phWow >= 1) phWow -= 1;
        const float wow = wowAmt.next() * std::sin (2 * sz::kPi * (float) phWow);
        const float spr = spread.next();
        float sawSum = 0, triSum = 0, subSum = 0, shimSum = 0, lightSum = 0;
        for (int k = 0; k < 4; ++k)
        {
            const float f = freq[(size_t) k].next() * transposeRatio;
            for (int side = 0; side < 2; ++side)
            {
                const int idx = k * 2 + side;
                const float det = (side ? 1.0f : -1.0f) * spr + wow;
                const float fd = f * std::pow (2.0f, det / 1200.0f), inc = fd * dt;
                sawSum += sz::osc (waveA, (float) phS[idx], inc);
                triSum += sz::osc (waveB, (float) phT[idx], inc);
                phS[idx] += inc; if (phS[idx] >= 1) phS[idx] -= 1;
                phT[idx] += inc; if (phT[idx] >= 1) phT[idx] -= 1;
            }
            const float wr = std::pow (2.0f, wow / 1200.0f);
            const float incSub = f * 0.5f * wr * dt;
            subSum += sz::osc (sz::SQUARE, (float) phSub[k], incSub);
            phSub[k] += incSub; if (phSub[k] >= 1) phSub[k] -= 1;
            phShim[k] += f * 2 * wr * dt; if (phShim[k] >= 1) phShim[k] -= 1;
            shimSum += std::sin (2 * sz::kPi * (float) phShim[k]);
            phLight[k] += f * 4 * std::pow (2.0f, (k % 2 ? 4.0f : -4.0f) / 1200.0f) * dt; if (phLight[k] >= 1) phLight[k] -= 1;
            phLfo[k] += (0.04 + k * 0.027) * dt; if (phLfo[k] >= 1) phLfo[k] -= 1;
            lightSum += std::sin (2 * sz::kPi * (float) phLight[k]) * (0.5f + 0.5f * std::sin (2 * sz::kPi * (float) phLfo[k]));
        }

        const float q = qTone.next(), co = cutoff.next();
        if ((i & 15) == 0) filter.set (sz::LP, co, q, fs);
        const float F = filter.process (sawSum * gSaw.next() + triSum * gTri.next() + subSum * gSub.next());

        // --- birds, drops, crackle ---
        float birds = 0, crackle = 0;
        for (auto& s : shots)
        {
            if (! s.active) continue;
            if (s.click)
            {
                const int idx = (int) s.ph;
                if (idx >= 219) { s.active = false; continue; }
                const float fr = s.ph - idx;
                crackle += (clickBuf[(size_t) idx] * (1 - fr) + clickBuf[(size_t) idx + 1] * fr) * s.gain;
                s.ph += s.rate;
                continue;
            }
            s.t += dt;
            if (s.t < 0) continue;
            float f;
            if (s.t < s.sweep) f = s.f0 * std::pow (s.f1 / s.f0, s.t / s.sweep);
            else if (s.f2 > 0) f = s.f1 * std::pow (s.f2 / s.f1, std::min (1.0f, (s.t - s.sweep) / s.sweep2));
            else f = s.f1;
            float g;
            if (s.t < s.gAtt) g = 1e-4f * std::pow (s.gPeak / 1e-4f, s.t / s.gAtt);
            else if (s.t < s.gEnd) g = s.gPeak * std::pow (1e-4f / s.gPeak, (s.t - s.gAtt) / (s.gEnd - s.gAtt));
            else g = 0;
            birds += std::sin (2 * sz::kPi * s.ph) * g;
            s.ph += f * dt; if (s.ph >= 1) s.ph -= std::floor (s.ph);
            if (s.t > s.endT) s.active = false;
        }

        // --- DRIFT: three slowly swaying delays, spread in stereo ---
        chBuf[(size_t) chW] = F;
        const float depth = chDepth.next(), cw = chWet.next();
        float cOutL = 0, cOutR = 0;
        for (int k = 0; k < 3; ++k)
        {
            phCh[k] += chRate[k] * dt; if (phCh[k] >= 1) phCh[k] -= 1;
            const float d = (chBase[k] + depth * std::sin (2 * sz::kPi * (float) phCh[k])) * fs;
            float rp = (float) chW - d; while (rp < 0) rp += (float) chLen;
            const int i0 = (int) rp; const float fr = rp - i0;
            const float v = chBuf[(size_t) i0] * (1 - fr) + chBuf[(size_t) ((i0 + 1) % chLen)] * fr;
            cOutL += v * chL[k]; cOutR += v * chR[k];
        }
        chW = (chW + 1) % chLen;
        cOutL *= cw; cOutR *= cw;

        // --- AFTERGLOW: echo with feedback ---
        const int rIdx = (echoW - echoD + echoLen) % echoLen;
        const float ey = echoLp.process (echoBuf[(size_t) rIdx]);
        echoBuf[(size_t) echoW] = F + birds + ey * fbG.next();
        echoW = (echoW + 1) % echoLen;
        const float echo = ey * echoG.next();

        // --- DUST + LIGHT noise layers ---
        const float noise = rng.next();
        const float hiss = hissBp.process (noise) * hissG.next();
        const float breeze = breezeBp.process (rng.next()) * breezeG.next();
        const float light = lightSum * lightG.next();
        const float shim = shimSum * shimG.next();

        const float toConv = F + birds + echo + light + breeze + shim;
        cL[i] = toConv + cOutL; cR[i] = toConv + cOutR;
        const float pre = F * dryG.next() + birds + echo + light + hiss + crackle;
        pL[i] = pre + cOutL; pR[i] = pre + cOutR;
    }

    {
        juce::dsp::AudioBlock<float> blk (convIn.getArrayOfWritePointers(), 2, (size_t) n);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (blk));
    }

    float* oL = buffer.getWritePointer (0);
    float* oR = buffer.getWritePointer (1);
    int sw = scopeW.load();
    for (int i = 0; i < n; ++i)
    {
        const float wg = wetG.next(), mg = master.next(), og = outG.next(), ad = airDb.next();
        if ((i & 31) == 0) for (auto& a : air) a.set (sz::HSHELF, 6000, 0, fs, ad);
        float l = (pL[i] + cL[i] * wg) * mg, r = (pR[i] + cR[i] * wg) * mg;
        l = air[0].process (l); r = air[1].process (r);
        comp.process (l, r);
        oL[i] = l * og; oR[i] = r * og;
        scope[(size_t) sw] = oL[i]; sw = (sw + 1) & 4095;
    }
    scopeW = sw;
}

int SunrizeProcessor::readScope (float* dst, int n) const
{
    const int w = scopeW.load();
    for (int i = 0; i < n; ++i) dst[i] = scope[(size_t) ((w - n + i + 4096) & 4095)];
    return n;
}

void SunrizeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void SunrizeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto st = juce::ValueTree::fromXml (*xml);
        if (st.isValid()) apvts.replaceState (st);
    }
}

juce::AudioProcessorEditor* SunrizeProcessor::createEditor() { return new SunrizeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SunrizeProcessor(); }
