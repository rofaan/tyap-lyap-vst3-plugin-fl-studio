#pragma once
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>

// Continuous ITD and equal-power motion: no tap switching at the centre.
class SpatialOrbit
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        smooth = 1.0 - std::exp (-1.0 / (sr * 0.025));
        delay.assign ((size_t) (sr * 0.01) + 8, 0.0f);
        write = 0; phase = 0; amount = 0; rate = 0.18;
    }
    double getPhase() const noexcept { return phase; }

    std::array<float, 2> process (float l, float r, bool enabled, float targetRate, double timelinePhase = -1.0)
    {
        constexpr double turn = 6.283185307179586;
        amount += smooth * ((enabled ? 1.0 : 0.0) - amount);
        rate += smooth * (targetRate - rate);
        if (timelinePhase >= 0.0)
            phase += smooth * std::remainder (timelinePhase - phase, turn);
        delay[write] = 0.5f * (l + r);
        const double x = std::sin (phase);
        // Both taps always exist and vary smoothly. Sub-millisecond ITD avoids
        // the previous 12 ms chorus sweep and discontinuous ear handover.
        const float a = read ((float) (sampleRate * (0.001 + 0.00032 * x)));
        const float b = read ((float) (sampleRate * (0.001 - 0.00032 * x)));
        const float wl = 0.2f * l + 0.8f * a * (float) std::sqrt (1.0 - 0.85 * x);
        const float wr = 0.2f * r + 0.8f * b * (float) std::sqrt (1.0 + 0.85 * x);
        write = (write + 1) % delay.size();
        phase += turn * (timelinePhase >= 0.0 ? targetRate : rate) / sampleRate;
        if (phase >= turn) phase -= turn;
        if (phase < 0.0) phase += turn;
        return { l + (float) amount * (wl - l), r + (float) amount * (wr - r) };
    }
private:
    float read (float samples) const
    {
        double p = (double) write - samples;
        while (p < 0) p += (double) delay.size();
        const auto i = (size_t) p;
        return delay[i] + (float) (p - (double) i) * (delay[(i + 1) % delay.size()] - delay[i]);
    }
    std::vector<float> delay;
    size_t write = 0;
    double sampleRate = 44100, phase = 0, amount = 0, rate = 0.18;
    double smooth = 0.0;
};
