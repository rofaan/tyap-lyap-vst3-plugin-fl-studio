#pragma once

#include <JuceHeader.h>

// The cabinet uses the original 1513 x 1039 artwork coordinates.
class ShelfScene
{
public:
    void prepare (const juce::Image& cabinet);
    void strike (float amount);
    bool advance (float seconds, float drive);
    void draw (juce::Graphics&, float headlightX, float headlightLevel) const;
    bool isMoving() const { return moving; }

    static juce::Rectangle<float> bounds() { return { 236, 662, 1258, 115 }; }

private:
    struct Body
    {
        juce::Image cutout, cleanPlate;
        juce::Rectangle<int> source;
        juce::Point<float> foot;
        float mass = 1, hop = 0, rebound = 0;
        float x = 0, height = 0, angle = 0;
        float vx = 0, vy = 0, spin = 0;
    };

    struct Dust
    {
        juce::Point<float> position, velocity;
        float life = 0, duration = 0, radius = 0;
    };

    void land (const Body&, float impact);
    static constexpr float gravity = 1040.0f;
    std::array<Body, 10> bodies;
    std::array<Dust, 24> dust;
    juce::Random random { 571889 };
    size_t nextDust = 0;
    float clock = 0;
    bool moving = false;
};
