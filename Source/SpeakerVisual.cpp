#include "SpeakerVisual.h"

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float extent = 216.0f;
constexpr float centre = 224.0f;

float grain (float x, float y)
{
    // Surface-locked, low-contrast fibres, with no new random noise on each frame.
    const auto hash = [] (int a, int b)
    {
        auto n = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u;
        n = (n ^ (n >> 13u)) * 1274126177u;
        return (float) ((n ^ (n >> 16u)) & 1023u) / 1023.0f;
    };
    const int ix = (int) std::floor (x), iy = (int) std::floor (y);
    const float fx = x - (float) ix, fy = y - (float) iy;
    return juce::jmap (fy, juce::jmap (fx, hash (ix, iy), hash (ix + 1, iy)),
                           juce::jmap (fx, hash (ix, iy + 1), hash (ix + 1, iy + 1))) - 0.5f;
}
}

SpeakerVisual::SpeakerVisual() = default;

SpeakerVisual::Surfaces::Surfaces()
{
    for (int frame = 0; frame < count; ++frame)
    {
        const float travel = -1.0f + 2.0f * (float) frame / (float) (count - 1);
        const float cap = 0.235f + travel * 0.028f;
        const float edge = 0.775f + travel * 0.030f;
        const float depth = -0.32f + travel * 0.29f;
        const auto height = [=] (float r)
        {
            if (r < cap)
                return depth + 0.123f * std::sqrt (juce::jmax (0.0f, 1.0f - (r / cap) * (r / cap)));
            if (r < edge)
            {
                const float t = (r - cap) / (edge - cap);
                const float ribs = 0.008f * std::sin (t * pi * 20.0f) * std::sin (t * pi);
                return depth * (1.0f - t) + (-0.016f + 0.065f * travel) * t + ribs;
            }
            if (r < 0.92f)
            {
                const float t = (r - edge) / (0.92f - edge);
                return (-0.016f + 0.065f * travel) * (1.0f - t) + 0.064f * std::sin (t * pi);
            }
            return 0.012f + 0.020f * std::sin ((r - 0.92f) / 0.08f * pi);
        };
        constexpr int samples = 2048;
        std::array<float, samples + 1> heights {}, slopes {};
        for (int i = 0; i <= samples; ++i)
        {
            const float r = (float) i / (float) samples;
            heights[(size_t) i] = height (r);
            slopes[(size_t) i] = (height (r + 0.0005f) - height (juce::jmax (0.0f, r - 0.0005f))) * 1000.0f;
        }
        const auto sample = [&] (const auto& values, float r)
        {
            const float pos = juce::jlimit (0.0f, (float) samples - 0.001f, r * (float) samples);
            const int i = (int) pos;
            return juce::jmap (pos - (float) i, values[(size_t) i], values[(size_t) i + 1]);
        };
        auto& image = frames[(size_t) frame];
        image = juce::Image (juce::Image::ARGB, size, size, true);
        {
            juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);
            for (int py = 0; py < size; ++py)
                for (int px = 0; px < size; ++px)
                {
                    const float x = ((float) px + 0.5f - centre) / extent;
                    const float screenY = ((float) py + 0.5f - centre) / extent;
                    float y = screenY / 0.985f;
                    for (int step = 0; step < 3; ++step)
                    {
                        const float r = std::sqrt (x * x + y * y);
                        y = (screenY + 0.21f * sample (heights, r)) / 0.985f;
                    }
                    const float r = std::sqrt (x * x + y * y);
                    if (r > 1.003f) continue;
                    const float alpha = juce::jlimit (0.0f, 1.0f, (1.003f - r) * extent);
                    const float slope = sample (slopes, r);
                    const float inv = 1.0f / std::sqrt (1.0f + slope * slope);
                    const float nx = -slope * x / juce::jmax (0.001f, r) * inv;
                    const float ny = -slope * y / juce::jmax (0.001f, r) * inv;
                    const float diffuse = juce::jmax (0.0f, nx * -0.54f + ny * -0.53f + inv * 0.65f);
                    const float shine = std::pow (juce::jmax (0.0f, nx * -0.30f + ny * -0.29f + inv * 0.909f), r < cap ? 24.0f : 48.0f);
                    const bool surround = r >= edge && r < 0.92f, rim = r >= 0.92f;
                    const float paper = grain (x * 178, y * 178);
                    const float cavity = r < edge ? 0.72f + 0.28f * juce::jlimit (0.0f, 1.0f, (r - cap) / (edge - cap)) : 1.0f;
                    const float light = (0.28f + 0.95f * diffuse) * cavity;
                    const float texture = 1.0f + paper * (rim ? 0.16f : 0.055f);
                    const float specular = shine * (rim ? 49.0f : (surround ? 14.0f : 19.0f));
                    const float red = rim ? 114.0f : (surround ? 39.0f : 99.0f);
                    const float green = rim ? 74.0f : (surround ? 29.0f : 62.0f);
                    const float blue = rim ? 45.0f : (surround ? 22.0f : 39.0f);
                    const auto channel = [=] (float base) { return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (base * light * texture + specular)); };
                    data.setPixelColour (px, py, juce::Colour (channel (red), channel (green), channel (blue))
                        .withAlpha (alpha));
                }
        }
        juce::Graphics g (image);
        // Small embossed rose stays attached to the dust cap, rather than becoming flying wedges.
        const juce::Point<float> hub (centre, centre - (depth + 0.123f) * 0.21f * extent);
        const float radius = cap * extent * 0.69f;
        for (int i = 0; i < 8; ++i)
        {
            const float angle = (float) i * pi / 4;
            const auto point = [&] (float length, float offset)
            {
                return hub + juce::Point<float> (std::sin (angle + offset) * length,
                                                -std::cos (angle + offset) * length * 0.985f);
            };
            const auto tip = point (radius * (i % 2 == 0 ? 1.0f : 0.63f), 0);
            for (const float side : { -1.0f, 1.0f })
            {
                juce::Path facet;
                facet.startNewSubPath (hub);
                facet.lineTo (point (radius * 0.23f, side * pi / 4));
                facet.lineTo (tip);
                facet.closeSubPath();
                g.setGradientFill (juce::ColourGradient (juce::Colour (side < 0 ? 0xff9b7048 : 0xff392115), tip,
                                                         juce::Colour (0xff4d301d), hub, false));
                g.fillPath (facet);
            }
        }
        // The screws belong to the basket: their positions never follow the cone.
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * pi / 4 + pi / 8;
            const auto bolt = juce::Point<float> (centre + std::sin (a) * extent * 0.963f,
                                                centre - std::cos (a) * extent * 0.963f * 0.985f);
            g.setColour (juce::Colour (0xff100a07));
            g.fillEllipse (juce::Rectangle<float> (7, 7).withCentre (bolt));
            g.setColour (juce::Colour (0xff886140));
            g.drawLine (bolt.x - 2, bolt.y + 1, bolt.x + 2, bolt.y - 1, 1);
        }
    }
}

void SpeakerVisual::draw (juce::Graphics& g, juce::Rectangle<float> bounds, float excursion, float opacity) const
{
    const float position = (juce::jlimit (-1.0f, 1.0f, excursion) + 1.0f) * 0.5f * (float) (Surfaces::count - 1);
    const int lower = (int) position, upper = juce::jmin (lower + 1, Surfaces::count - 1);
    juce::Graphics::ScopedSaveState state (g);
    g.beginTransparencyLayer (opacity);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (surfaces->frames[(size_t) lower], bounds);
    if (upper != lower)
    {
        g.setOpacity (position - (float) lower);
        g.drawImage (surfaces->frames[(size_t) upper], bounds);
    }
    g.endTransparencyLayer();
}
