#pragma once
#include <JuceHeader.h>

class TapeStop
{
public:
    void prepare (double sampleRate)
    {
        brakeSamples = juce::jmax (2, juce::roundToInt (0.50 * sampleRate));
        pauseSamples = juce::roundToInt (0.075 * sampleRate);
        returnSamples = juce::jmax (2, juce::roundToInt (0.145 * sampleRate));
        tape.setSize (2, brakeSamples);
        position = -1;
        read = 0.0;
    }

    void trigger()
    {
        if (position < 0)
        {
            position = 0;
            read = 0.0;
        }
    }

    void process (float (&frame)[2])
    {
        if (position < 0)
            return;

        if (position < brakeSamples)
        {
            const float progress = (float) position / (float) (brakeSamples - 1);
            const float speed = 1.0f - progress;
            const float gain = 1.0f - ease (juce::jlimit (0.0f, 1.0f, (progress - 0.64f) / 0.36f));
            const int first = (int) read;
            const int second = juce::jmin (first + 1, position);
            const float fraction = (float) (read - first);
            for (int ch = 0; ch < 2; ++ch)
            {
                tape.setSample (ch, position, frame[ch]);
                const float a = tape.getSample (ch, first);
                frame[ch] = gain * (a + fraction * (tape.getSample (ch, second) - a));
            }
            read += speed * speed;
        }
        else
        {
            const float gain = ease (juce::jlimit (0.0f, 1.0f,
                (float) (position - brakeSamples - pauseSamples) / (float) (returnSamples - 1)));
            for (auto& channel : frame)
                channel *= gain;
        }

        if (++position == brakeSamples + pauseSamples + returnSamples)
            position = -1;
    }

    float progress() const
    {
        return position < 0 ? 0.0f
            : (float) position / (float) (brakeSamples + pauseSamples + returnSamples);
    }

private:
    static float ease (float x) { return x * x * (3.0f - 2.0f * x); }

    juce::AudioBuffer<float> tape;
    int position = -1, brakeSamples = 0, pauseSamples = 0, returnSamples = 0;
    double read = 0.0;
};
