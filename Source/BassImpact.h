#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

// One event word lets the UI read the hit strength and sequence together.
class BassImpact
{
public:
    void prepare (double sampleRate)
    {
        const auto coefficient = [sampleRate] (double seconds) {
            return (float) (1.0 - std::exp (-1.0 / (sampleRate * seconds)));
        };
        attack = coefficient (0.0035);
        release = coefficient (0.027);
        baselineStep = coefficient (0.12);
        captureSamples = std::max (1, (int) (sampleRate * 0.012));
        holdSamples = std::max (1, (int) (sampleRate * 0.092));
        fast = baseline = hit = 0;
        capture = hold = 0;
        // Keep the sequence across prepareToPlay, including when the host changes buffers.
    }

    void push (float magnitude) noexcept
    {
        fast += (magnitude - fast) * (magnitude > fast ? attack : release);
        baseline += (fast - baseline) * baselineStep;
        const float rise = fast - baseline;
        if (hold > 0)
            --hold;

        if (capture > 0)
        {
            hit = std::max (hit, rise);
            if (--capture == 0)
            {
                const float level = std::clamp ((hit - 0.010f) * 7.0f, 0.0f, 1.0f);
                if (level > 0.025f)
                {
                    sequence = (sequence + 1u) & 0xffffu;
                    event.store ((sequence << 16u) | (std::uint32_t) (level * 65535.0f),
                                 std::memory_order_relaxed);
                }
                hold = holdSamples;
            }
        }
        else if (hold == 0 && rise > 0.016f + baseline * 0.24f && fast > 0.023f)
        {
            hit = rise;
            capture = captureSamples;
        }
    }

    std::uint32_t latest() const noexcept { return event.load (std::memory_order_relaxed); }
    static float strength (std::uint32_t word) noexcept { return (float) (word & 0xffffu) / 65535.0f; }

private:
    std::atomic<std::uint32_t> event { 0 };
    std::uint32_t sequence = 0;
    float attack = 0, release = 0, baselineStep = 0;
    float fast = 0, baseline = 0, hit = 0;
    int capture = 0, hold = 0, captureSamples = 1, holdSamples = 1;
};
