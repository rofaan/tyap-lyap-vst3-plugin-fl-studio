#pragma once
#include <JuceHeader.h>

// A stationary basket and a continuous, deforming paper cone. Frames are shaded
// once on the UI thread and shared by all editors; the audio thread never sees them.
class SpeakerVisual
{
public:
    SpeakerVisual();
    void draw (juce::Graphics&, juce::Rectangle<float>, float excursion, float opacity) const;

private:
    struct Surfaces
    {
        Surfaces();
        static constexpr int count = 33, size = 448;
        std::array<juce::Image, count> frames;
    };
    juce::SharedResourcePointer<Surfaces> surfaces;
};
