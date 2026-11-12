#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
double sampleRate = 48000;
constexpr int blockSize = 512;

struct TestPlayHead final : juce::AudioPlayHead
{
    PositionInfo info;
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};

void expect (bool condition, const char* message)
{
    if (! condition) throw std::runtime_error (message);
}

void set (juce::AudioProcessor& plugin, const char* name, float normalisedValue)
{
    for (auto* parameter : plugin.getParameters())
        if (parameter->getName (128) == name)
        {
            parameter->setValueNotifyingHost (normalisedValue);
            return;
        }
    throw std::runtime_error (std::string ("Missing parameter: ") + name);
}

struct Signal
{
    std::array<double, 2> rms {};
    float peak = 0;
};

Signal measure (juce::AudioProcessor& plugin, float frequency, float level, bool bursts = false, float rightPolarity = 1)
{
    juce::AudioBuffer<float> audio (2, blockSize);
    juce::MidiBuffer midi;
    Signal result;
    int measured = 0;
    const int blockCount = (int) std::ceil (sampleRate * 1.024 / blockSize);
    for (int block = 0; block < blockCount; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const int sample = block * blockSize + i;
            const float gate = bursts && sample % 8192 >= 960 ? 0.0f : 1.0f;
            const float input = gate * level * (float) std::sin (
                juce::MathConstants<double>::twoPi * frequency * sample / sampleRate);
            audio.setSample (0, i, input);
            audio.setSample (1, i, input * rightPolarity);
        }
        plugin.processBlock (audio, midi);
        if (block < blockCount / 2) continue;
        measured += blockSize;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const auto sample = audio.getSample (ch, i);
                expect (std::isfinite (sample), "Non-finite output");
                result.rms[(size_t) ch] += sample * sample;
                result.peak = std::max (result.peak, std::abs (sample));
            }
    }
    for (auto& channel : result.rms)
        channel = std::sqrt (channel / measured);
    return result;
}
}

int main (int argc, char** argv)
{
    if (argc < 2 || argc > 3) return 2;
    if (argc == 3)
        sampleRate = juce::String (argv[2]).getDoubleValue();
    if (sampleRate < 22050 || sampleRate > 192000) return 2;
    try
    {
        juce::ScopedJuceInitialiser_GUI initialise;
        juce::VST3PluginFormatHeadless format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, juce::String::fromUTF8 (argv[1]));
        expect (! descriptions.isEmpty(), "VST3 scan failed");
        TestPlayHead playHead;
        juce::String error;
        auto plugin = format.createInstanceFromDescription (*descriptions[0], sampleRate, blockSize, error);
        if (plugin == nullptr) throw std::runtime_error (error.toStdString());
        plugin->setPlayHead (&playHead);
        plugin->prepareToPlay (sampleRate, blockSize);
        auto param = [&] (const char* name, float value) { set (*plugin, name, value); };
        param ("Bass Boost", 0);
        param ("Slowed", 0);
        param ("Reverb", 0);
        param ("12D Orbit", 0);
        param ("Car Mode", 0);
        param ("Yard Distance", 0);
        param ("Mix", 1);
        const auto dry = measure (*plugin, 1000, 0.12f);
        expect (std::abs (dry.rms[0] - 0.12 / std::sqrt (2.0)) < 0.001, "Neutral gain is not unity");
        param ("Car Mode", 1);
        param ("Sub", 0);
        param ("Punch", 0);
        param ("Voice", 0);
        const std::array<const char*, 3> controls { "Sub", "Punch", "Voice" };
        const std::array<float, 3> frequencies { 45, 115, 2200 };
        for (size_t i = 0; i < controls.size(); ++i)
        {
            const auto base = measure (*plugin, frequencies[i], 0.09f, i == 1);
            param (controls[i], 1);
            const auto boosted = measure (*plugin, frequencies[i], 0.09f, i == 1);
            const auto delta = 20.0 * std::log10 (boosted.rms[0] / base.rms[0]);
            expect (delta > 2.0, "Car control did not audibly increase its band");
            std::cout << controls[i] << ": +" << delta << " dB on test signal\n";
            param (controls[i], 0);
        }
        param ("Car Mode", 0);
        const auto indoor = measure (*plugin, 3500, 0.1f);
        param ("Yard Distance", 1);
        const auto outside = measure (*plugin, 3500, 0.1f);
        expect (outside.rms[0] < indoor.rms[0] * 0.12, "Yard did not damp high frequencies");
        param ("Yard Distance", 0);
        const auto wideBass = measure (*plugin, 35, 0.15f, false, -1);
        param ("Mono Low", 1);
        const auto monoBass = measure (*plugin, 35, 0.15f, false, -1);
        expect (monoBass.rms[0] < wideBass.rms[0] * 0.15, "Mono Low did not remove low side signal");
        param ("Mono Low", 0);
        const auto rumbleBase = measure (*plugin, 205, 0.3f);
        param ("Anti Rumble", 1);
        const auto rumbleCut = measure (*plugin, 205, 0.3f);
        expect (rumbleCut.rms[0] < rumbleBase.rms[0] * 0.5, "Anti Rumble did not reduce loud resonances");
        param ("Anti Rumble", 0);
        const auto uncompressed = measure (*plugin, 1000, 0.5f);
        param ("Compression", 1);
        const auto compressed = measure (*plugin, 1000, 0.5f);
        expect (compressed.rms[0] < uncompressed.rms[0] * 0.5, "Compression did not reduce the signal");
        param ("Compression", 0);
        measure (*plugin, 1000, 0.15f);
        param ("Tape Stop Trigger", 1);
        juce::AudioBuffer<float> tape (2, blockSize);
        juce::MidiBuffer midi;
        float stoppedPeak = 0, resumedPeak = 0, maxStep = 0, previous = 0;
        for (int block = 0; block < (int) std::ceil (sampleRate * 1.024 / blockSize); ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto input = 0.15f * (float) std::sin (juce::MathConstants<double>::twoPi
                                                         * 1000 * (block * blockSize + i) / sampleRate);
                tape.setSample (0, i, input);
                tape.setSample (1, i, input);
            }
            plugin->processBlock (tape, midi);
            for (int i = 0; i < blockSize; ++i)
            {
                const float value = tape.getSample (0, i);
                const float seconds = (float) (block * blockSize + i) / (float) sampleRate;
                if (seconds > 0.50f && seconds < 0.575f)
                    stoppedPeak = std::max (stoppedPeak, std::abs (value));
                if (seconds > 0.85f)
                    resumedPeak = std::max (resumedPeak, std::abs (value));
                if (block > 0)
                    maxStep = std::max (maxStep, std::abs (value - previous));
                expect (std::isfinite (value), "Tape Stop non-finite output");
                previous = value;
            }
        }
        expect (stoppedPeak < 0.0001f && resumedPeak > 0.14f, "Tape Stop did not stop and return");
        expect (maxStep < 0.025f, "Tape Stop produced a discontinuity");
        param ("Tape Stop Trigger", 0);
        std::cout << "Mono Low, dynamic Anti Rumble, Compression, Tape Stop and smooth return PASS\n";
        param ("12D Orbit", 1);
        param ("Orbit Tempo Sync", 0.5f); // One bar.
        playHead.info.setBpm (120.0);
        playHead.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        playHead.info.setIsPlaying (false);
        playHead.info.setPpqPosition (1.0);
        const auto right = measure (*plugin, 350, 0.1f);
        playHead.info.setPpqPosition (3.0);
        const auto left = measure (*plugin, 350, 0.1f);
        expect (right.rms[1] > right.rms[0] * 1.4 && left.rms[0] > left.rms[1] * 1.4,
                "Orbit did not align to the host quarter-note position");
        playHead.info.setPpqPosition (5.0);
        const auto nextBar = measure (*plugin, 350, 0.1f);
        expect (std::abs (right.rms[1] - nextBar.rms[1]) < 0.001, "Orbit did not repeat at one bar");
        param ("12D Orbit", 0);
        param ("Bass Boost", 0);
        const auto referenceLevel = measure (*plugin, 35, 0.08f);
        param ("Bass Boost", 0.6f);
        const auto unmatchedLevel = measure (*plugin, 35, 0.08f);
        param ("Level Match", 1);
        measure (*plugin, 35, 0.08f);
        const auto matchedLevel = measure (*plugin, 35, 0.08f);
        expect (unmatchedLevel.rms[0] > referenceLevel.rms[0] * 2.0, "Bass test did not raise output");
        expect (std::abs (matchedLevel.rms[0] / referenceLevel.rms[0] - 1.0) < 0.055, "Level Match failed to reach reference");
        param ("Level Match", 0);
        std::cout << "Level Match compensated bass boost against input reference PASS\n";
        param ("Bass Boost", 1);
        expect (measure (*plugin, 35, 0.95f).peak <= 0.89126, "Limiter exceeded ceiling");
        plugin->releaseResources();
        std::cout << "VST3 host: unity, three car controls, yard, one-bar sync and limiter PASS\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
