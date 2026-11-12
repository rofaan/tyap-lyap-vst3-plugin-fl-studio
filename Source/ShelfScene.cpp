#include "ShelfScene.h"

namespace
{
struct Prop
{
    std::initializer_list<juce::Point<float>> edge;
    juce::Point<float> foot;
    float mass, hop, rebound;
};

// Outline vertices follow the baked silhouettes. Feet sit on the shelf plane.
const Prop props[] {
    { {{252,731},{255,723},{268,717},{287,714},{309,716},{326,723},{333,730},
       {333,745},{327,752},{310,757},{278,758},{259,752},{252,745}}, {292,754}, 1.0f, 16, 0.30f },
    { {{327,708},{332,700},{347,694},{371,690},{393,693},{405,699},{408,707},
       {407,741},{401,750},{383,756},{355,756},{338,751},{330,743}}, {370,752}, 2.3f, 6, 0.25f },
    { {{406,735},{412,727},{428,722},{446,719},{464,722},{477,728},{485,734},
       {484,748},{478,754},{458,759},{431,759},{413,754},{407,747}}, {445,756}, 0.9f, 19, 0.34f },
    { {{476,710},{481,701},{498,696},{519,694},{541,697},{552,703},{556,711},
       {554,742},{549,750},{532,755},{503,755},{484,750},{478,742}}, {515,752}, 2.7f, 5, 0.21f },
    { {{857,719},{859,711},{867,705},{879,701},{895,703},{907,708},{912,716},
       {911,739},{906,748},{898,754},{878,754},{863,749},{856,742}}, {886,750}, 1.3f, 22, 0.43f },
    { {{917,710},{921,701},{930,696},{944,693},{959,696},{967,702},{970,710},
       {966,733},{961,742},{954,747},{938,747},{922,742},{915,733}}, {943,744}, 1.1f, 24, 0.46f },
    { {{1024,731},{1029,724},{1044,718},{1067,714},{1088,717},{1103,723},
       {1110,731},{1108,745},{1102,752},{1082,758},{1050,757},{1031,751},{1025,744}}, {1067,754}, 1.0f, 16, 0.30f },
    { {{1114,707},{1120,700},{1134,694},{1156,691},{1178,693},{1191,699},{1197,708},
       {1196,741},{1190,750},{1174,755},{1144,755},{1126,751},{1117,742}}, {1157,752}, 2.4f, 6, 0.24f },
    { {{1193,731},{1198,724},{1214,717},{1238,714},{1259,717},{1272,723},{1278,731},
       {1277,745},{1270,752},{1251,758},{1220,758},{1202,753},{1195,745}}, {1236,754}, 1.1f, 15, 0.29f },
    { {{1355,723},{1386,706},{1422,692},{1440,683},{1462,700},{1484,715},{1481,731},
       {1449,749},{1410,768},{1388,759},{1354,744},{1357,735}}, {1417,759}, 2.0f, 9, 0.26f }
};

juce::Path pathFor (const Prop& prop)
{
    juce::Path outline;
    outline.startNewSubPath (*prop.edge.begin());
    for (auto point = prop.edge.begin() + 1; point != prop.edge.end(); ++point)
        outline.lineTo (*point);
    outline.closeSubPath();
    return outline;
}

juce::Colour uncoveredWood (const juce::Image& cabinet, int x, int y)
{
    // Project the vertical wood above the shelf down behind the dice and the end of the board.
    const int projectedRow = 676 + (y % 13);
    const bool inlay = x >= 501 && x < 1018;
    const int wallX = x < 501 ? 209 + x % 42 : 1291 + x % 47;
    const auto wall = inlay ? cabinet.getPixelAt (x, projectedRow)
                            : cabinet.getPixelAt (wallX, juce::jmin (y, 726));
    // Sample the unobstructed horizontal grain at the same height.
    const int floorX = 569 + ((x * 3 / 4) % 154);
    const auto floor = cabinet.getPixelAt (floorX, y);
    const float seam = juce::jlimit (0.0f, 1.0f, ((float) y - 727.0f) / 7.0f);
    return wall.interpolatedWith (floor, seam);
}
}

void ShelfScene::prepare (const juce::Image& cabinet)
{
    static_assert (std::size (props) == std::tuple_size_v<decltype (bodies)>);
    for (size_t i = 0; i < bodies.size(); ++i)
    {
        const auto& prop = props[i];
        auto& body = bodies[i];
        const auto outline = pathFor (prop);
        body.source = outline.getBounds().getSmallestIntegerContainer().expanded (6, 7);
        body.foot = prop.foot;
        body.mass = prop.mass;
        body.hop = prop.hop;
        body.rebound = prop.rebound;
        const auto rect = body.source;
        const auto local = juce::AffineTransform::translation ((float) -rect.getX(), (float) -rect.getY());
        body.cutout = juce::Image (juce::Image::ARGB, rect.getWidth(), rect.getHeight(), true);
        body.cleanPlate = juce::Image (juce::Image::ARGB, rect.getWidth(), rect.getHeight(), true);
        {
            juce::Graphics extract (body.cutout);
            extract.reduceClipRegion (outline, local);
            extract.drawImageAt (cabinet, -rect.getX(), -rect.getY());
        }
        juce::Image mask (juce::Image::ARGB, rect.getWidth(), rect.getHeight(), true);
        {
            juce::Graphics g (mask);
            g.addTransform (local);
            // Feather only the edge of the replacement, keeping the original object fully covered.
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.strokePath (outline, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved));
            g.strokePath (outline, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
            g.setColour (juce::Colours::white);
            g.strokePath (outline, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved));
            g.fillPath (outline);
        }
        juce::Image::BitmapData patch (body.cleanPlate, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData matte (mask, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < rect.getHeight(); ++y)
            for (int x = 0; x < rect.getWidth(); ++x)
            {
                const auto alpha = matte.getPixelColour (x, y).getAlpha();
                if (alpha != 0)
                    patch.setPixelColour (x, y, uncoveredWood (cabinet, x + rect.getX(), y + rect.getY()).withAlpha (alpha));
            }
    }
}

void ShelfScene::strike (float amount)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    if (amount < 0.018f)
        return;
    for (auto& body : bodies)
    {
        const float scatter = 0.78f + 0.30f * random.nextFloat();
        const float direction = 2.0f * random.nextFloat() - 1.0f;
        const float height = body.hop * amount * amount * scatter;
        // Each hit adds lateral and rotational momentum. A hit during flight gives a smaller kick.
        const float contact = body.height < 2.0f ? 1.0f : 0.30f;
        const float launch = std::sqrt (2.0f * gravity * height) * contact;
        if (height >= 0.16f)
            body.vy = juce::jlimit (-260.0f, 255.0f, juce::jmax (0.0f, body.vy) * 0.46f + launch);
        body.vx += direction * (135.0f / body.mass) * amount * scatter * contact;
        body.spin += direction * (3.0f / body.mass) * amount * scatter * contact;
    }
    moving = true;
}

void ShelfScene::land (const Body& body, float impact)
{
    if (impact < 84.0f)
        return;
    const float speed = juce::jmin (impact * 0.46f, 85.0f);
    for (int i = 0; i < (impact > 150 ? 3 : 1); ++i)
    {
        auto& particle = dust[nextDust];
        nextDust = (nextDust + 1) % dust.size();
        const float direction = 2.0f * random.nextFloat() - 1.0f;
        particle.position = body.foot.translated (body.x + direction * body.source.getWidth() * 0.25f, 0);
        particle.velocity = { speed * direction, -10.0f - 29.0f * random.nextFloat() };
        particle.life = particle.duration = 0.12f + 0.22f * random.nextFloat();
        particle.radius = 0.55f + 0.62f * random.nextFloat();
    }
}

bool ShelfScene::advance (float seconds, float drive)
{
    const bool wasMoving = moving;
    if (! moving && drive < 0.008f)
        return false;

    const int steps = juce::jmax (1, (int) std::ceil (seconds * 240.0f));
    const float dt = seconds / (float) steps;
    moving = false;
    for (int step = 0; step < steps; ++step)
    {
        clock = std::fmod (clock + dt, 120.0f);
        for (size_t i = 0; i < bodies.size(); ++i)
        {
            auto& body = bodies[i];
            const bool air = body.height > 0 || body.vy > 0;
            const float buzz = std::sin (juce::MathConstants<float>::twoPi * (13.4f + 0.57f * i) * clock
                                         + 0.79f * i) * drive / body.mass;
            body.vx += (buzz * 1400.0f - 102.0f * body.x - (air ? 4.0f : 14.0f) * body.vx) * dt;
            body.x = juce::jlimit (-16.0f, 16.0f, body.x + body.vx * dt);
            body.spin += (buzz * 43.0f - 145.0f * body.angle - (air ? 4.5f : 16.0f) * body.spin) * dt;
            body.angle = juce::jlimit (-0.26f, 0.26f, body.angle + body.spin * dt);

            if (air)
            {
                body.vy -= gravity * dt;
                body.height = juce::jmin (32.0f, body.height + body.vy * dt);
                if (body.height < 0)
                {
                    const float impact = -body.vy;
                    land (body, impact);
                    body.height = 0;
                    body.vy = impact > 34.0f ? impact * body.rebound : 0.0f;
                    body.spin = -body.spin * 0.35f + body.vx * 0.031f;
                    body.vx *= 0.62f;
                }
            }
        }
        // Close neighbours can knock against each other before the friction settles them.
        for (size_t i = 1; i < bodies.size(); ++i)
        {
            auto& left = bodies[i - 1];
            auto& right = bodies[i];
            const float gap = right.foot.x - left.foot.x;
            const float overlap = left.x - right.x - 4.0f;
            if (gap > 90.0f || overlap <= 0.0f || std::abs (left.height - right.height) > 15.0f)
                continue;
            const float leftShare = right.mass / (left.mass + right.mass);
            left.x -= overlap * leftShare;
            right.x += overlap * (1.0f - leftShare);
            const float impulse = juce::jmax (0.0f, left.vx - right.vx) * 1.24f;
            left.vx -= impulse * leftShare;
            right.vx += impulse * (1.0f - leftShare);
            left.spin -= impulse * leftShare * 0.016f;
            right.spin += impulse * (1.0f - leftShare) * 0.016f;
        }
    }
    for (auto& particle : dust)
        if (particle.life > 0)
        {
            particle.life -= seconds;
            particle.velocity.y += 70.0f * seconds;
            particle.position += particle.velocity * seconds;
            moving |= particle.life > 0;
        }
    for (auto& body : bodies)
    {
        if (drive < 0.008f && body.height == 0.0f && body.vy == 0.0f && std::abs (body.x) < 0.018f
            && std::abs (body.vx) < 0.30f && std::abs (body.angle) < 0.0007f && std::abs (body.spin) < 0.015f)
            body.x = body.vx = body.angle = body.spin = 0;
        moving |= std::abs (body.x) > 0.018f || body.height > 0 || body.vy > 0 || std::abs (body.angle) > 0.0007f;
    }
    return wasMoving || moving;
}

void ShelfScene::draw (juce::Graphics& g, float headlightX, float headlightLevel) const
{
    if (! moving || ! g.clipRegionIntersects (bounds().getSmallestIntegerContainer()))
        return;
    juce::Graphics::ScopedSaveState sceneState (g);
    g.reduceClipRegion (bounds().getSmallestIntegerContainer());
    for (const auto& body : bodies)
        g.drawImageAt (body.cleanPlate, body.source.getX(), body.source.getY());

    // Keep the shadow on the wood. It broadens and softens as the object lifts off.
    for (const auto& body : bodies)
    {
        const auto centre = body.foot.translated (body.x + body.height * 0.36f, 3 + body.height * 0.075f);
        const float width = body.source.getWidth() * (0.86f + body.height * 0.012f);
        const float alpha = 0.66f / (1.0f + body.height * 0.060f);
        const float height = 12 + body.height * 0.37f;
        juce::Graphics::ScopedSaveState flatten (g);
        g.addTransform (juce::AffineTransform::scale (1.0f, height / width, centre.x, centre.y));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (alpha), centre,
                                                 juce::Colours::black.withAlpha (0.0f),
                                                 centre.translated (width * 0.5f, 0), true));
        g.fillEllipse (juce::Rectangle<float> (width, width).withCentre (centre));
    }

    // Back objects are drawn first at the original depth, with the two dice separately composited.
    constexpr size_t backToFront[] { 5, 3, 1, 7, 9, 0, 4, 6, 8, 2 };
    for (const auto i : backToFront)
    {
        const auto& body = bodies[i];
        const auto centre = body.foot;
        const auto transform = juce::AffineTransform::translation ((float) body.source.getX(), (float) body.source.getY())
            .rotated (body.angle, centre.x, centre.y - 4.0f).translated (body.x, -body.height);
        g.setOpacity (1.0f);
        g.drawImageTransformed (body.cutout, transform);
        const float gleam = headlightLevel * juce::jmax (0.0f, 1.0f - std::abs (headlightX - centre.x - body.x) / 155.0f);
        if (gleam > 0.008f)
        {
            // Tint only the silhouette, never the rectangular source crop.
            g.setColour (juce::Colour (0xffd5a875).withAlpha (gleam * 0.22f));
            g.drawImageTransformed (body.cutout, transform, true);
        }
    }
    for (const auto& particle : dust)
        if (particle.life > 0)
        {
            g.setColour (juce::Colour (0xffdb9d63).withAlpha (0.43f * particle.life / particle.duration));
            g.fillEllipse (juce::Rectangle<float> (particle.radius * 2, particle.radius * 1.2f).withCentre (particle.position));
        }
}
