#pragma once
#include <JuceHeader.h>

class PresetBank
{
public:
    struct Preset
    {
        juce::String name;
        int style = 0;
        std::array<float, 16> values {};
    };

    explicit PresetBank (juce::AudioProcessorValueTreeState& parameters) : state (parameters) {}
    Preset capture() const;
    std::vector<Preset> list (const juce::File& directory = library()) const;
    void select (const Preset&);
    bool save (const juce::String&, const juce::File& directory = library());
    void switchTo (int);
    juce::String title() const;
    int activeSlot() const;
    int style() const;
    juce::ValueTree exportState() const;
    void importState (const juce::ValueTree&);

private:
    static juce::File library();
    static juce::ValueTree encode (const Preset&);
    std::optional<Preset> decode (const juce::ValueTree&) const;
    void apply (const Preset&);
    juce::AudioProcessorValueTreeState& state;
    struct Slot { Preset sound; std::optional<Preset> reference; };
    std::array<std::optional<Slot>, 2> slots;
    std::optional<Preset> reference;
    int active = 0;
    mutable juce::CriticalSection lock;
};
