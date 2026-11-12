#include "../Source/SpatialOrbit.h"
#include <iostream>
#include <cstdlib>
int main()
{
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        SpatialOrbit orbit;
        orbit.prepare (sr);
        float previous = 0, maxStep = 0;
        for (int i = 0; i < (int) (sr * 12); ++i)
        {
            // Constant input exposes tap handovers and enable/disable clicks.
            const bool enabled = i > (int) sr && i < (int) (sr * 10);
            const auto out = orbit.process (0.5f, 0.5f, enabled, i < (int) (sr * 6) ? 1.2f : 0.03f);
            if (!std::isfinite(out[0]) || !std::isfinite(out[1])) return EXIT_FAILURE;
            if (i > 1) maxStep = std::max(maxStep, std::abs(out[0] - previous));
            previous = out[0];
        }
        if (maxStep > 0.001f) return EXIT_FAILURE;
        std::cout << sr << " Hz: toggle + centre crossings + rate change max step " << maxStep << " PASS\n";
    }
    return EXIT_SUCCESS;
}
