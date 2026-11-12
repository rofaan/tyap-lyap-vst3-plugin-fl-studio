#pragma once
#include <JuceHeader.h>
#include "SpatialOrbit.h"
#include "TapeStop.h"
#include "BassImpact.h"
#include "PresetBank.h"

class NorthstarFXAudioProcessor final : public juce::AudioProcessor
{
public:
    NorthstarFXAudioProcessor();
    ~NorthstarFXAudioProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return juce::String::fromUTF8 ("Тяп Ляп"); }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetBank presetBank { apvts };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::atomic<float> limiterReduction { 0.0f };
    std::array<std::atomic<float>, 3> carActivity {};
    std::atomic<float> orbitDisplayPhase { 0.0f };
    std::atomic<float> orbitTempo { 0.0f };
    std::atomic<float> bassActivity { 0.0f }, rumbleReduction { 0.0f }, compressorReduction { 0.0f };
    std::atomic<float> tapeProgress { 0.0f };
    std::atomic<float> levelMatchDb { 0.0f };
    std::uint32_t latestBassImpact() const { return bassImpact.latest(); }
    void triggerTapeStop() { tapeRequested.store (true); }

private:
    struct OrbitTiming
    {
        float rate = 0.18f;
        double phase = -1.0;
    };
    OrbitTiming getOrbitTiming();
    void processSlice (juce::AudioBuffer<float>&, OrbitTiming, int offset);

    juce::dsp::IIR::Filter<float> lowShelfL, lowShelfR;
    std::array<juce::dsp::IIR::Filter<float>, 2> kickBand, presenceBand;
    std::array<juce::dsp::IIR::Filter<float>, 2> rumbleBand;
    juce::dsp::IIR::Filter<float> sideHighPass;
    TapeStop tapeStop;
    BassImpact bassImpact;
    std::atomic<bool> tapeRequested { false };
    bool tapeAutomationHigh = false;
    juce::Reverb reverb;
    SpatialOrbit spatial;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> pitchDelay;
    juce::AudioBuffer<float> yardDelay;
    int pitchWrite = 0, yardWrite = 0;
    double sampleRate = 44100.0, pitchPhase = 0.0;
    float smoothedMix = 1.0f;
    float carBlend = 0, subAmount = 0, punchAmount = 0, voiceAmount = 0;
    float subState[2] {}, yardLow[2] {}, yardFiltered[2] {};
    float yardAmount = 0, yardFilterStep = 0;
    float fastEnvelope = 0, slowEnvelope = 0, sideBass = 0, limiterGain = 1;
    float monoAmount = 0, rumbleAmount = 0, compressorAmount = 0;
    float rumbleEnvelope = 0, compressorEnvelope = 0, compressorGain = 1;
    float matchInputPower = 0, matchOutputPower = 0, matchGain = 1, matchTarget = 1;
    int preparedBlockSize = 0;

    float readDelay (const juce::AudioBuffer<float>&, int channel, int write,
                     float delaySamples) const;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorthstarFXAudioProcessor)
};
