#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs
{
    constexpr auto bass = "bass";
    constexpr auto lowFreq = "lowFreq";
    constexpr auto slowed = "slowed";
    constexpr auto reverb = "reverb";
    constexpr auto orbit = "orbit";
    constexpr auto orbitRate = "orbitRate";
    constexpr auto mix = "mix";
}

NorthstarFXAudioProcessor::NorthstarFXAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout NorthstarFXAudioProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add (std::make_unique<P> (IDs::bass, "Bass Boost", juce::NormalisableRange<float> (0, 18, 0.1f), 6));
    p.add (std::make_unique<P> (IDs::lowFreq, "Low End", juce::NormalisableRange<float> (35, 220, 1, 0.45f), 110));
    p.add (std::make_unique<P> (IDs::slowed, "Slowed", juce::NormalisableRange<float> (0, 12, 0.1f), 4));
    p.add (std::make_unique<P> (IDs::reverb, "Reverb", juce::NormalisableRange<float> (0, 100, 1), 28));
    p.add (std::make_unique<juce::AudioParameterBool> (IDs::orbit, "12D Orbit", true));
    p.add (std::make_unique<P> (IDs::orbitRate, "Orbit Rate", juce::NormalisableRange<float> (0.03f, 1.2f, 0.01f, 0.45f), 0.18f));
    p.add (std::make_unique<P> (IDs::mix, "Mix", juce::NormalisableRange<float> (0, 100, 1), 100));
    p.add (std::make_unique<juce::AudioParameterBool> ("car", "Car Mode", false));
    for (const auto& control : { std::pair<const char*, const char*> {"sub", "Sub"},
                                  {"punch", "Punch"}, {"voice", "Voice"} })
        p.add (std::make_unique<P> (control.first, control.second, juce::NormalisableRange<float> (0, 100, 1), 35.0f));
    p.add (std::make_unique<P> ("yard", "Yard Distance", juce::NormalisableRange<float> (0, 100, 1), 0.0f));
    p.add (std::make_unique<juce::AudioParameterChoice> ("orbitSync", "Orbit Tempo Sync",
            juce::StringArray { "Free", "1/2 bar", "1 bar", "2 bars", "4 bars" }, 0));
    p.add (std::make_unique<P> ("monoLow", "Mono Low", juce::NormalisableRange<float> (0, 100, 1), 0));
    p.add (std::make_unique<P> ("antiRumble", "Anti Rumble", juce::NormalisableRange<float> (0, 100, 1), 0));
    p.add (std::make_unique<P> ("compression", "Compression", juce::NormalisableRange<float> (0, 100, 1), 0));
    p.add (std::make_unique<juce::AudioParameterBool> ("tapeStop", "Tape Stop Trigger", false));
    p.add (std::make_unique<juce::AudioParameterBool> ("levelMatch", "Level Match", false));
    return p;
}

bool NorthstarFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NorthstarFXAudioProcessor::prepareToPlay (double sr, int maximumBlockSize)
{
    sampleRate = sr;
    preparedBlockSize = juce::jmax (1, maximumBlockSize);
    dryBuffer.setSize (2, preparedBlockSize, false, true, true);
    pitchDelay.setSize (2, (int) (sr * 0.14) + preparedBlockSize, false, true, true);
    yardDelay.setSize (2, juce::jmax (1, (int) (sr * 0.019)), false, true, true);
    pitchDelay.clear();
    yardDelay.clear();
    pitchWrite = yardWrite = 0;
    pitchPhase = 0.0;
    reverb.setSampleRate (sr);
    spatial.prepare (sr);
    tapeStop.prepare (sr);
    bassImpact.prepare (sr);
    matchInputPower = matchOutputPower = 0;
    matchGain = matchTarget = 1;
    levelMatchDb.store (0);
    tapeRequested.store (false);
    tapeAutomationHigh = apvts.getRawParameterValue ("tapeStop")->load() > 0.5f;
    tapeProgress.store (0);
    bassActivity.store (0);
    rumbleReduction.store (0);
    compressorReduction.store (0);
    monoAmount = rumbleAmount = compressorAmount = rumbleEnvelope = compressorEnvelope = 0;
    compressorGain = 1;
    *sideHighPass.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeHighPass (sr, 110.0f);
    sideHighPass.reset();
    carBlend = subAmount = punchAmount = voiceAmount = 0;
    fastEnvelope = slowEnvelope = sideBass = 0;
    limiterGain = 1;
    limiterReduction.store (0);
    orbitDisplayPhase.store (0);
    orbitTempo.store (0);
    for (auto& meter : carActivity)
        meter.store (0);
    yardAmount = 0;
    yardFilterStep = 1;
    for (int ch = 0; ch < 2; ++ch)
    {
        subState[ch] = yardLow[ch] = yardFiltered[ch] = 0;
        *kickBand[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 115.0f, 0.85f);
        *presenceBand[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 2200.0f, 0.8f);
        kickBand[ch].reset();
        presenceBand[ch].reset();
        *rumbleBand[ch].coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeBandPass (sr, 205.0f, 0.85f);
        rumbleBand[ch].reset();
    }
    *lowShelfL.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (sr, 110.0f, 0.72f, 1.0f);
    lowShelfR.coefficients = lowShelfL.coefficients;
    lowShelfL.reset(); lowShelfR.reset();
}

float NorthstarFXAudioProcessor::readDelay (const juce::AudioBuffer<float>& b, int ch,
                                             int write, float delay) const
{
    const int n = b.getNumSamples();
    float pos = (float) write - delay;
    while (pos < 0) pos += (float) n;
    const int a = (int) pos % n, c = (a + 1) % n;
    const float f = pos - std::floor (pos);
    return b.getSample (ch, a) + f * (b.getSample (ch, c) - b.getSample (ch, a));
}

void NorthstarFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    if (preparedBlockSize <= 0 || buffer.getNumChannels() < 2)
        return;
    const auto timing = getOrbitTiming();
    for (int offset = 0; offset < buffer.getNumSamples(); offset += preparedBlockSize)
    {
        const int length = juce::jmin (preparedBlockSize, buffer.getNumSamples() - offset);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), 2, offset, length);
        processSlice (slice, timing, offset);
    }
}

NorthstarFXAudioProcessor::OrbitTiming NorthstarFXAudioProcessor::getOrbitTiming()
{
    OrbitTiming timing { apvts.getRawParameterValue (IDs::orbitRate)->load(), -1.0 };
    orbitTempo.store (0);
    const int division = (int) apvts.getRawParameterValue ("orbitSync")->load();
    auto* hostPlayhead = getPlayHead();
    if (division == 0 || hostPlayhead == nullptr)
        return timing;
    const auto position = hostPlayhead->getPosition();
    if (! position)
        return timing;
    const auto bpm = position->getBpm();
    if (! bpm || ! std::isfinite (*bpm) || *bpm <= 0)
        return timing;
    const auto signature = position->getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature { 4, 4 });
    const double beatsPerBar = 4.0 * juce::jmax (1, signature.numerator) / juce::jmax (1, signature.denominator);
    constexpr double bars[] { 0, 0.5, 1, 2, 4 };
    const double beatsPerTurn = beatsPerBar * bars[juce::jlimit (1, 4, division)];
    timing.rate = (float) (*bpm / (60.0 * beatsPerTurn));
    orbitTempo.store ((float) *bpm);
    if (const auto ppq = position->getPpqPosition(); ppq && std::isfinite (*ppq))
    {
        const double turns = *ppq / beatsPerTurn;
        timing.phase = juce::MathConstants<double>::twoPi * (turns - std::floor (turns));
        if (! position->getIsPlaying())
            timing.rate = 0;
    }
    return timing;
}

void NorthstarFXAudioProcessor::processSlice (juce::AudioBuffer<float>& buffer, OrbitTiming timing, int offset)
{
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    const float bass = apvts.getRawParameterValue (IDs::bass)->load();
    const float freq = apvts.getRawParameterValue (IDs::lowFreq)->load();
    *lowShelfL.coefficients = juce::dsp::IIR::ArrayCoefficients<float>::makeLowShelf (
        sampleRate, freq, 0.72f, juce::Decibels::decibelsToGain (bass));
    juce::dsp::AudioBlock<float> audioBlock (buffer);
    auto leftBlock = audioBlock.getSingleChannelBlock (0);
    auto rightBlock = audioBlock.getSingleChannelBlock (1);
    juce::dsp::ProcessContextReplacing<float> left (leftBlock);
    juce::dsp::ProcessContextReplacing<float> right (rightBlock);
    lowShelfL.process (left); lowShelfR.process (right);

    // Dual-tap continuously moving delay: a smooth tape-style downward pitch effect.
    const float semitones = apvts.getRawParameterValue (IDs::slowed)->load();
    if (semitones > 0.01f)
    {
        const double ratio = std::pow (2.0, -semitones / 12.0);
        const double phaseStep = (1.0 - ratio) / (sampleRate * 0.075);
        const float window = (float) (sampleRate * 0.075);
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < 2; ++ch) pitchDelay.setSample (ch, pitchWrite, buffer.getSample (ch, i));
            const float p1 = (float) pitchPhase;
            const float p2 = std::fmod (p1 + 0.5f, 1.0f);
            const float g1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p1);
            const float g2 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p2);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float a = readDelay (pitchDelay, ch, pitchWrite, 64.0f + p1 * window);
                const float b = readDelay (pitchDelay, ch, pitchWrite, 64.0f + p2 * window);
                buffer.setSample (ch, i, (a * g1 + b * g2) / juce::jmax (0.1f, g1 + g2));
            }
            pitchWrite = (pitchWrite + 1) % pitchDelay.getNumSamples();
            pitchPhase = std::fmod (pitchPhase + phaseStep, 1.0);
        }
    }

    juce::Reverb::Parameters rp;
    const float rv = apvts.getRawParameterValue (IDs::reverb)->load() * 0.01f;
    rp.roomSize = 0.25f + rv * 0.7f; rp.damping = 0.38f; rp.wetLevel = rv * 0.52f;
    rp.dryLevel = 0.5f * (1.0f - rv * 0.18f); // JUCE doubles dryLevel internally.
    rp.width = 1.0f; rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    const bool orbitEnabled = apvts.getRawParameterValue (IDs::orbit)->load() > 0.5f;
    const double phaseStep = juce::MathConstants<double>::twoPi * timing.rate / sampleRate;
    for (int i = 0; i < numSamples; ++i)
    {
        const double phase = timing.phase < 0 ? -1.0
            : std::fmod (timing.phase + phaseStep * (offset + i), juce::MathConstants<double>::twoPi);
        auto out = spatial.process (buffer.getSample (0, i), buffer.getSample (1, i), orbitEnabled, timing.rate, phase);
        buffer.setSample (0, i, out[0]);
        buffer.setSample (1, i, out[1]);
    }
    orbitDisplayPhase.store ((float) spatial.getPhase());

    const float targetMix = apvts.getRawParameterValue (IDs::mix)->load() * 0.01f;
    const float carTarget = apvts.getRawParameterValue ("car")->load();
    const float subTarget = apvts.getRawParameterValue ("sub")->load() * 0.01f;
    const float punchTarget = apvts.getRawParameterValue ("punch")->load() * 0.01f;
    const float voiceTarget = apvts.getRawParameterValue ("voice")->load() * 0.01f;
    const float yardTarget = apvts.getRawParameterValue ("yard")->load() * 0.01f;
    const float monoTarget = apvts.getRawParameterValue ("monoLow")->load() * 0.01f;
    const float rumbleTarget = apvts.getRawParameterValue ("antiRumble")->load() * 0.01f;
    const float compressorTarget = apvts.getRawParameterValue ("compression")->load() * 0.01f;
    const bool automationHigh = apvts.getRawParameterValue ("tapeStop")->load() > 0.5f;
    const bool requested = tapeRequested.exchange (false);
    if (requested || (automationHigh && ! tapeAutomationHigh))
        tapeStop.trigger();
    tapeAutomationHigh = automationHigh;
    const bool matchEnabled = apvts.getRawParameterValue ("levelMatch")->load() > 0.5f;
    const bool canMeasure = matchEnabled && ! requested && tapeStop.progress() == 0.0f;
    const float matchStep = (float) (1.0 - std::exp (-1.0 / (sampleRate * (matchEnabled ? 0.32 : 0.055))));
    float inputPower = 0, outputPower = 0;
    auto lowPassStep = [this] (float hz) {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sampleRate);
    };
    const float subStep = lowPassStep (75);
    const float yardStep = lowPassStep (9500.0f * std::pow (0.052f, yardTarget));
    const float smooth = lowPassStep (7), release = lowPassStep (1.7f);
    const float fastStep = lowPassStep (70), slowStep = lowPassStep (6);
    const float detectorAttack = 1.0f - std::exp (-1.0f / (0.001f * (float) sampleRate));
    const float detectorRelease = 1.0f - std::exp (-1.0f / (0.045f * (float) sampleRate));
    const float gainAttack = 1.0f - std::exp (-1.0f / (0.015f * (float) sampleRate));
    const float gainRelease = 1.0f - std::exp (-1.0f / (0.120f * (float) sampleRate));
    float bassPeak = 0, rumbleCut = 0, compressionCut = 0;
    float reduction = limiterReduction.load() * std::exp (-(float) numSamples / ((float) sampleRate * 0.3f));
    std::array<float, 3> activity {};
    float signalPeak = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        carBlend += smooth * (carTarget - carBlend);
        subAmount += smooth * (subTarget - subAmount);
        punchAmount += smooth * (punchTarget - punchAmount);
        voiceAmount += smooth * (voiceTarget - voiceAmount);
        yardAmount += smooth * (yardTarget - yardAmount);
        monoAmount += smooth * (monoTarget - monoAmount);
        rumbleAmount += smooth * (rumbleTarget - rumbleAmount);
        compressorAmount += smooth * (compressorTarget - compressorAmount);
        yardFilterStep += smooth * (yardStep - yardFilterStep);
        float wet[2] { buffer.getSample (0, i), buffer.getSample (1, i) };
        float kick[2], presence[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            subState[ch] += subStep * (wet[ch] - subState[ch]);
            kick[ch] = kickBand[ch].processSample (wet[ch]);
            presence[ch] = presenceBand[ch].processSample (wet[ch]);
        }
        const float kickLevel = 0.5f * (std::abs (kick[0]) + std::abs (kick[1]));
        fastEnvelope += fastStep * (kickLevel - fastEnvelope);
        slowEnvelope += slowStep * (kickLevel - slowEnvelope);
        const float transient = juce::jlimit (0.0f, 1.0f, (fastEnvelope - slowEnvelope) / (slowEnvelope + 0.002f));
        const float sub = 0.5f * (subState[0] + subState[1]);
        bassPeak = juce::jmax (bassPeak, std::abs (sub));
        const float voice = 0.5f * (presence[0] + presence[1]);
        sideBass += subStep * (0.5f * (wet[0] - wet[1]) - sideBass);
        smoothedMix += smooth * (targetMix - smoothedMix);
        const float stereoLow = juce::jmax (std::abs (subState[0]), std::abs (subState[1]));
        const float carBass = carBlend * (1.20f * subAmount * std::abs (sub)
                                       + 0.55f * punchAmount * kickLevel);
        bassImpact.push ((0.65f * stereoLow + 0.37f * kickLevel + carBass) * smoothedMix);
        for (int ch = 0; ch < 2; ++ch)
        {
            signalPeak = juce::jmax (signalPeak, std::abs (wet[ch]));
            const float centredBass = ch == 0 ? -sideBass : sideBass;
            const float subBoost = carBlend * 1.35f * subAmount * sub;
            const float kickBoost = carBlend * 1.55f * punchAmount * (0.26f + 0.74f * transient) * kick[ch];
            const float voiceBoost = carBlend * 1.45f * voiceAmount * voice;
            activity[0] = juce::jmax (activity[0], std::abs (subBoost));
            activity[1] = juce::jmax (activity[1], std::abs (kickBoost));
            activity[2] = juce::jmax (activity[2], std::abs (voiceBoost));
            wet[ch] += carBlend * centredBass + subBoost + kickBoost + voiceBoost;
            yardLow[ch] += yardFilterStep * (wet[ch] - yardLow[ch]);
            yardFiltered[ch] += yardFilterStep * (yardLow[ch] - yardFiltered[ch]);
        }
        const float yardMono = 0.5f * (yardFiltered[0] + yardFiltered[1]);
        const float distanceGain = 1.0f / (1.0f + 2.4f * yardAmount);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float reflection = yardDelay.getSample (ch, yardWrite);
            yardDelay.setSample (ch, yardWrite, yardFiltered[ch]);
            const float exterior = (yardMono + (yardFiltered[ch] - yardMono) * (1.0f - 0.82f * yardAmount)
                                    + 0.15f * yardAmount * reflection) * distanceGain;
            wet[ch] += yardAmount * (exterior - wet[ch]);
        }
        yardWrite = (yardWrite + 1) % yardDelay.getNumSamples();
        // Remove low-frequency side information while preserving the stereo mid.
        const float side = 0.5f * (wet[0] - wet[1]);
        const float sideCorrection = monoAmount * (sideHighPass.processSample (side) - side);
        wet[0] += sideCorrection;
        wet[1] -= sideCorrection;

        float rumble[2] { rumbleBand[0].processSample (wet[0]), rumbleBand[1].processSample (wet[1]) };
        const float rumblePeak = juce::jmax (std::abs (rumble[0]), std::abs (rumble[1]));
        const float rumbleStep = rumblePeak > rumbleEnvelope ? gainAttack : gainRelease;
        rumbleEnvelope += rumbleStep * (rumblePeak - rumbleEnvelope);
        const float cut = rumbleAmount * 0.82f * juce::jmax (0.0f, 1.0f - 0.025f / (rumbleEnvelope + 0.00001f));
        rumbleCut = juce::jmax (rumbleCut, cut);
        for (int ch = 0; ch < 2; ++ch)
            wet[ch] -= cut * rumble[ch];

        const float detector = juce::jmax (std::abs (wet[0]), std::abs (wet[1]));
        const float detectorStep = detector > compressorEnvelope ? detectorAttack : detectorRelease;
        compressorEnvelope += detectorStep * (detector - compressorEnvelope);
        // Stereo-linked 4:1 compressor above -24 dBFS, blended by the amount control.
        const float desiredGain = compressorEnvelope > 0.0630957f
            ? std::pow (0.0630957f / compressorEnvelope, 0.75f) : 1.0f;
        const float gainStep = desiredGain < compressorGain ? gainAttack : gainRelease;
        compressorGain += gainStep * (desiredGain - compressorGain);
        const float appliedGain = 1.0f + compressorAmount * (compressorGain - 1.0f);
        compressionCut = juce::jmax (compressionCut, 1.0f - appliedGain);
        for (auto& channel : wet)
            channel *= appliedGain * (1.0f + 0.35f * compressorAmount);
        tapeStop.process (wet);
        for (int ch = 0; ch < 2; ++ch)
            wet[ch] = dryBuffer.getSample (ch, i) * (1.0f - smoothedMix) + wet[ch] * smoothedMix;
        if (canMeasure)
        {
            const float l = dryBuffer.getSample (0, i), r = dryBuffer.getSample (1, i);
            inputPower += l * l + r * r;
            outputPower += wet[0] * wet[0] + wet[1] * wet[1];
        }
        matchGain += matchStep * ((matchEnabled ? matchTarget : 1.0f) - matchGain);
        wet[0] *= matchGain;
        wet[1] *= matchGain;
        const float peak = juce::jmax (std::abs (wet[0]), std::abs (wet[1]));
        const float ceiling = 0.89125094f; // -1 dBFS sample peak
        const float requestedGain = peak > ceiling ? ceiling / peak : 1.0f;
        limiterGain = juce::jmin (requestedGain, limiterGain + release * (1.0f - limiterGain));
        reduction = juce::jmax (reduction, 1.0f - limiterGain);
        for (int ch = 0; ch < 2; ++ch)
            buffer.setSample (ch, i, wet[ch] * limiterGain);
    }
    limiterReduction.store (reduction);
    if (canMeasure && numSamples > 0)
    {
        const float input = inputPower / (2.0f * numSamples);
        const float output = outputPower / (2.0f * numSamples);
        if (input > 0.000002f && output > 0.0000002f)
        {
            const float rate = (float) (1.0 - std::exp (-numSamples / (sampleRate * 0.42)));
            matchInputPower += rate * (input - matchInputPower);
            matchOutputPower += rate * (output - matchOutputPower);
            matchTarget = juce::jlimit (0.2511886f, 3.981072f, std::sqrt (matchInputPower / matchOutputPower));
        }
    }
    if (! matchEnabled)
    {
        matchInputPower = matchOutputPower = 0;
        matchTarget = 1;
        if (std::abs (matchGain - 1.0f) < 0.00001f)
            matchGain = 1;
    }
    levelMatchDb.store (juce::Decibels::gainToDecibels (matchGain));
    tapeProgress.store (tapeStop.progress());
    const float meterDecay = std::exp (-(float) numSamples / ((float) sampleRate * 0.15f));
    bassActivity.store (juce::jmax (juce::jlimit (0.0f, 1.0f, bassPeak * 5.0f * smoothedMix),
                                   meterDecay * bassActivity.load()));
    rumbleReduction.store (juce::jmax (rumbleCut * smoothedMix, meterDecay * rumbleReduction.load()));
    compressorReduction.store (juce::jmax (compressionCut * smoothedMix, meterDecay * compressorReduction.load()));
    for (size_t i = 0; i < activity.size(); ++i)
    {
        const float measured = juce::jlimit (0.0f, 1.0f, activity[i] * smoothedMix / (signalPeak + 0.015f));
        const float decay = std::exp (-(float) numSamples / ((float) sampleRate * 0.15f));
        carActivity[i].store (juce::jmax (measured, decay * carActivity[i].load()));
    }
}

void NorthstarFXAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.appendChild (presetBank.exportState(), nullptr);
    for (auto child : state)
        if (child.getProperty ("id").toString() == "tapeStop")
            child.setProperty ("value", 0.0f, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void NorthstarFXAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            const auto bank = state.getChildWithName ("PRESET_BANK");
            presetBank.importState (bank);
            if (bank.isValid())
                state.removeChild (bank, nullptr);
            for (const char* id : { "monoLow", "antiRumble", "compression", "tapeStop", "levelMatch" })
            {
                auto parameter = state.getChildWithProperty ("id", id);
                if (! parameter.isValid())
                {
                    parameter = juce::ValueTree ("PARAM");
                    parameter.setProperty ("id", id, nullptr);
                    parameter.setProperty ("value", 0.0f, nullptr);
                    state.addChild (parameter, -1, nullptr);
                }
                if (juce::String (id) == "tapeStop")
                    parameter.setProperty ("value", 0.0f, nullptr);
            }
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* NorthstarFXAudioProcessor::createEditor()
{
    return new NorthstarFXAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NorthstarFXAudioProcessor(); }
