#include "PluginEditor.h"
#include <BinaryData.h>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace
{
constexpr float cabinetWidth = 1513.0f;
constexpr float cabinetHeight = 1039.0f;
constexpr float designHeight = 1259.0f;
constexpr float windowRatio = 1448.0f / 1086.0f;
constexpr float compassX = 758.0f;
constexpr float compassY = 397.0f;
const juce::Colour amber (0xffeca969);
const juce::Colour ember (0xffed5e38);
const juce::Rectangle<float> compassArea (509, 150, 499, 495);
const juce::Rectangle<float> limiterPlate (1298, 928, 124, 36);
const juce::Rectangle<float> searchlightArea (888, 12, 613, 93);
const juce::Rectangle<float> smokeArea (56, 545, 112, 192);
const juce::Rectangle<float> leftShadows (494, 118, 47, 586);
const juce::Rectangle<float> rightShadows (968, 118, 52, 586);
const juce::Rectangle<float> shelfArea (18, 706, 1476, 101);
const juce::Rectangle<float> presetArea (1112, 124, 351, 46);
const juce::Rectangle<float> compareArea (790, 834, 430, 147);
const juce::Rectangle<float> hingeGlint (716, 19, 85, 75);
const juce::Rectangle<float> carTrim (22, 1000, 1465, 14);

// Reuse the photographed cabinet's actual metalwork, rather than drawing flat UI outlines.
void slice (juce::Graphics& g, const juce::Image& image, juce::Rectangle<int> source,
            juce::Rectangle<float> destination)
{
    g.drawImage (image.getClippedImage (source), destination);
}

void skinPlate (juce::Graphics& g, const juce::Image& skin, juce::Rectangle<float> area)
{
    constexpr int sx = 120, sy = 324, sw = 133, sh = 37, border = 7;
    const float edge = juce::jmin (6.0f, area.getHeight() * 0.19f);
    const int xs[] { sx, sx + border, sx + sw - border, sx + sw };
    const int ys[] { sy, sy + border, sy + sh - border, sy + sh };
    const float dx[] { area.getX(), area.getX() + edge, area.getRight() - edge, area.getRight() };
    const float dy[] { area.getY(), area.getY() + edge, area.getBottom() - edge, area.getBottom() };
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            slice (g, skin, { xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y] },
                   { dx[x], dy[y], dx[x + 1] - dx[x], dy[y + 1] - dy[y] });
}

void cabinetPanel (juce::Graphics& g, const juce::Image& skin, const juce::Image& wood,
                   juce::Rectangle<float> area)
{
    juce::Graphics::ScopedSaveState state (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, 16);
    g.reduceClipRegion (clip);
    g.setTiledImageFill (wood, (int) area.getX(), (int) area.getY(), 1);
    g.fillRect (area);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x0a110903), area.getTopLeft(),
                                            juce::Colour (0x85090201), area.getBottomLeft(), false));
    g.fillRect (area);
    const float x = area.getX(), y = area.getY(), w = area.getWidth(), h = area.getHeight();
    // Corners keep their round screw heads; the narrow edge strips contain no baked lettering.
    slice (g, skin, { 9, 824, 27, 27 }, { x, y, 27, 27 });
    slice (g, skin, { 1477, 824, 27, 27 }, { x + w - 27, y, 27, 27 });
    slice (g, skin, { 9, 985, 27, 27 }, { x, y + h - 27, 27, 27 });
    slice (g, skin, { 1477, 985, 27, 27 }, { x + w - 27, y + h - 27, 27, 27 });
    slice (g, skin, { 36, 824, 1441, 11 }, { x + 27, y, w - 54, 11 });
    slice (g, skin, { 36, 998, 1441, 14 }, { x + 27, y + h - 14, w - 54, 14 });
    slice (g, skin, { 9, 851, 17, 134 }, { x, y + 27, 17, h - 54 });
    slice (g, skin, { 1487, 851, 17, 134 }, { x + w - 17, y + 27, 17, h - 54 });
}

enum KnobIndex { bass, low, slow, room, speed, mix, sub, punch, voice, yard, monoLow, antiRumble, compression };

struct KnobLayout
{
    const char* parameter;
    const char* name;
    const char* unit;
    int decimals;
    juce::Rectangle<float> dial;
    juce::Rectangle<float> readout;
};

const KnobLayout layout[] {
    { "bass",     "Bass Boost", " dB", 1, {129, 199, 116, 116},   {120, 324, 133, 37} },
    { "lowFreq",  "Low End",    " Hz", 0, {315, 199, 116, 116},   {307, 324, 130, 37} },
    { "slowed",   "Slowed",     " st", 1, {129, 474, 116, 116},   {120, 602, 133, 36} },
    { "reverb",   "Reverb",     " %",  0, {315, 474, 116, 116},   {308, 602, 129, 36} },
    { "orbitRate","Orbit Speed"," Hz", 2, {1138, 232, 249, 73},  {1190, 324, 146, 37} },
    { "mix",      "Master Mix", " %",  0, {1194, 467, 138, 138}, {1190, 610, 146, 38} },
    { "sub",      "Sub",        " %",  0, {301, 860, 86, 80},     {294, 945, 100, 34} },
    { "punch",    "Punch",      " %",  0, {462, 860, 86, 80},     {455, 945, 100, 34} },
    { "voice",    "Voice",      " %",  0, {623, 860, 86, 80},     {616, 945, 100, 34} },
    { "yard",     "Дистанция",  " %",  0, {304, 1020, 96, 96},   {419, 1049, 142, 39} },
    { "monoLow", "Моно низ", " %", 0, {200, 1145, 90, 90}, {296, 1170, 104, 37} },
    { "antiRumble", "Антигудёж", " %", 0, {614, 1145, 90, 90}, {710, 1170, 104, 37} },
    { "compression", "Поджим", " %", 0, {1009, 1145, 90, 90}, {1105, 1170, 104, 37} }
};

juce::Point<float> atAngle (juce::Point<float> centre, float radius, float angle)
{
    return centre + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
}

struct Help
{
    const char* title;
    const char* description;
    juce::Point<float> centre;
};

constexpr Help help[] {
    { "Bass Boost", "Усиливает низкие частоты до +18 dB. Ручка Low End задаёт, как высоко по частотам поднимается усиление.", {259, 252} },
    { "Low End", "Задаёт границу усиления баса от 35 до 220 Гц. Чем выше частота, тем больше усиление затрагивает нижнюю середину.", {446, 252} },
    { "Slowed", "Понижает высоту звука до 12 полутонов. Темп проекта сохраняется. При 0 st эффект выключен.", {259, 526} },
    { "Reverb", "Добавляет отражения и хвост реверберации. Чем выше процент, тем заметнее и длиннее хвост. При 0% реверберация выключена.", {446, 526} },
    { "Скорость орбиты", "Задаёт скорость 12D Orbit. Например, 0,5 Гц — один оборот за 2 секунды. Для ручной скорости выбери «Свободно».", {1410, 268} },
    { "Master Mix", "Смешивает исходный и обработанный сигнал. При 0% остаётся исходный звук, при 100% слышна вся обработка. Выходной лимитер работает при любом Mix.", {1351, 530} },
    { "Саб", "Добавляет низ примерно до 75 Гц. Поворот ручки включает «Тачку». Полоса под значением показывает добавленный низ.", {395, 897} },
    { "Панч", "Подчёркивает удар бочки и атаки в области 115 Гц. Поворот ручки включает «Тачку».", {553, 897} },
    { "Голос", "Поднимает середину около 2,2 кГц, чтобы голос лучше читался в миксе. Поворот ручки включает «Тачку».", {718, 897} },
    { "Двор", "Имитирует звук машины с расстояния. С ростом процента звук становится тише, уже и глуше. При 0% эффект выключен.", {143, 1069} },
    { "Моно низ", "Сужает стереобас с частотой среза 110 Гц. При 100% самые низкие частоты почти по центру. При 0% эффект выключен.", {182, 1188} },
    { "Антигудёж", "Прибирает громкий гул около 205 Гц. Чем выше процент, тем сильнее ослабление. Индикатор dB показывает, сколько убирается.", {596, 1188} },
    { "Поджим", "Компрессор приглушает громкие места и выравнивает сигнал. Чем выше процент, тем заметнее эффект. Индикатор dB показывает ослабление.", {988, 1188} },
    { "12D Orbit", "Плавно перемещает звук между левым и правым каналами. В наушниках движение слышнее. Скорость задаёт шкала ниже или «Орбита в такт».", {1420, 198} },
    { "Тачка", "Включает и выключает обработку ручками «Саб», «Панч» и «Голос». Значения ручек сохраняются.", {267, 904} },
    { "Пресеты", "Нажми на название, чтобы открыть список. Стрелки листают пресеты. Звёздочка означает, что ты изменил настройки.", {1477, 147} },
    { "A/B", "Переключайся между двумя наборами настроек. При первом переключении второй набор копирует первый. Изменения сохраняются в выбранном наборе.", {976, 888} },
    { "Сохранить свой", "Дай текущим настройкам имя. Пресет появится в общем списке и будет доступен в других проектах на этом компьютере.", {1197, 888} },
    { "Ровный уровень", "Подстраивает средний уровень обработанного сигнала под исходный. Включи для сравнения пресетов и дай уровню установиться.", {1197, 938} },
    { "Без хрипа", "Лимитер удерживает выходные пики на уровне до −1 dBFS. Лампа загорается при срабатывании, а число показывает ослабление.", {1451, 859} },
    { "Орбита в такт", "Привязывает полный оборот 12D Orbit к темпу проекта. Выбери длину от половины до четырёх тактов. «Свободно» возвращает ручную скорость.", {1005, 1069} },
    { "Сброс", "За 0,72 секунды звук замедляется, на миг останавливается и плавно возвращается. Для полной паузы выставь Master Mix на 100%.", {1482, 1176} }
};

class HelpCard final : public juce::Component
{
public:
    explicit HelpCard (const Help& item) : title (juce::String::fromUTF8 (item.title)),
                                          body (juce::String::fromUTF8 (item.description))
    {
        setTitle (title);
        setDescription (body);
        setSize (306, 124);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff22150e));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6);
        g.setColour (amber);
        g.setFont (juce::FontOptions ("Georgia", 17.0f, juce::Font::bold));
        g.drawText (title, 14, 10, getWidth() - 28, 24, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xfff1d8bc));
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (body, {14, 41, getWidth() - 28, 72}, juce::Justification::topLeft, 4);
    }

private:
    juce::String title, body;
};
}

void AnimatedKnob::snapToValue()
{
    position = target = (float) valueToProportionOfLength (getValue());
}

bool AnimatedKnob::animate (float elapsed)
{
    const float next = (float) valueToProportionOfLength (getValue());
    const bool changed = std::abs (next - target) > 0.00001f;
    target = next;
    const float oldPosition = position;
    const float oldGlow = glow;
    position += (target - position) * (1.0f - std::exp (-elapsed / 0.048f));
    if (std::abs (position - target) < 0.0003f)
        position = target;
    if (changed)
        glow = 1.0f;
    else
        glow *= std::exp (-elapsed / 0.34f);
    if (glow < 0.004f)
        glow = 0.0f;
    const bool animating = position != oldPosition || glow != oldGlow;
    if (animating)
        repaint();
    return animating;
}

NorthstarLookAndFeel::NorthstarLookAndFeel()
{
    cabinet = juce::ImageCache::getFromMemory (BinaryData::cabinet_png, BinaryData::cabinet_pngSize);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d110a));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe4b990));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xffb58156));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xb357301b));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xffffd9ad));
    const auto image = juce::ImageCache::getFromMemory (BinaryData::knob_png, BinaryData::knob_pngSize);
    // Crop the transparent padding around the supplied knob. The PNG remains intact.
    knob = image.getClippedImage ({ 152, 150, 940, 940 }).rescaled (320, 320, juce::Graphics::highResamplingQuality);
}

void NorthstarLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float pos, float start, float end, juce::Slider& slider)
{
    const auto& dial = static_cast<const AnimatedKnob&> (slider);
    if (slider.getName() == "Orbit Speed")
    {
        const auto track = juce::Rectangle<float> ((float) x + 3, y + h * 0.20f, (float) w - 6, h * 0.60f);
        g.setColour (juce::Colour (0xff100905));
        g.fillEllipse (track);
        g.setColour (juce::Colour (0xff83532f));
        g.drawEllipse (track, 1.0f);
        const float cy = track.getCentreY();
        const float cx = track.getX() + 16 + dial.position * (track.getWidth() - 32);
        g.setColour (amber.withAlpha (0.75f));
        g.drawLine (track.getX() + 12, cy, track.getRight() - 12, cy, 1.2f);
        juce::Path star;
        star.addStar ({cx, cy}, 12, track.getHeight() * 0.22f, track.getHeight() * 0.42f);
        g.fillPath (star);
        g.setColour (juce::Colour (0xff22150e));
        g.fillEllipse (cx - 2, cy - 2, 4, 4);
        if (slider.hasKeyboardFocus (false))
        {
            g.setColour (amber);
            g.drawEllipse (track.expanded (2.0f), 1.0f);
        }
        return;
    }
    const float radius = (float) juce::jmin (w, h) * 0.385f;
    const juce::Point<float> centre (x + w * 0.5f, y + h * 0.5f);
    const float angle = start + dial.position * (end - start);
    const auto disc = juce::Rectangle<float> (radius * 2, radius * 2).withCentre (centre);
    const float ringRadius = radius * 1.16f;

    g.setColour (juce::Colour (0x9b080401));
    g.fillEllipse (disc.expanded (radius * 0.08f).translated (0, radius * 0.09f));
    if (dial.position > 0.0f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, start, angle, true);
        const auto lit = amber.interpolatedWith (ember, dial.position * dial.position);
        g.setColour (lit.withAlpha (0.12f * dial.glow * dial.position));
        g.strokePath (arc, juce::PathStrokeType (radius * 0.22f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lit.withAlpha (0.22f + 0.58f * dial.glow));
        g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.0f, radius * 0.054f)));
    }

    // The photo's red marker points approximately 46 degrees left of twelve o'clock.
    const auto transform = juce::AffineTransform::scale (disc.getWidth() / (float) knob.getWidth())
        .translated (disc.getX(), disc.getY())
        .rotated (angle + 0.81f, centre.x, centre.y);
    g.setOpacity (1.0f);
    g.drawImageTransformed (knob, transform);
    if (dial.glow > 0.0f)
    {
        const auto tip = atAngle (centre, ringRadius, angle);
        const float size = 2.0f + radius * 0.12f * pos;
        g.setColour (amber.withAlpha (dial.glow * (0.35f + 0.65f * dial.position)));
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (tip));
    }
    if (slider.hasKeyboardFocus (false))
    {
        g.setColour (amber.withAlpha (0.55f));
        g.drawEllipse (disc.expanded (radius * 0.24f), 1.0f);
    }
}

void NorthstarLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                            bool hover, bool pressed)
{
    const auto plate = button.getLocalBounds().toFloat().reduced (2);
    if (button.getProperties().contains ("plate"))
    {
        drawButtonBackground (g, button, {}, hover, pressed);
        const auto dot = juce::Rectangle<float> (7, 7).withCentre ({ plate.getX() + 11, plate.getCentreY() });
        g.setColour (button.getToggleState() ? ember : juce::Colour (0xff593823));
        g.fillEllipse (dot);
        g.setColour (amber);
        g.setFont (juce::FontOptions ("Georgia", juce::jmax (10.0f, plate.getHeight() * 0.48f), juce::Font::bold));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft (21).withTrimmedRight (5),
                          juce::Justification::centredLeft, 1);
        return;
    }
    if (button.getToggleState())
    {
        g.setColour (ember.withAlpha (0.15f));
        g.fillRoundedRectangle (plate, 4.0f);
        g.setColour (amber.withAlpha (0.76f));
        g.drawRoundedRectangle (plate, 4.0f, 1.2f);
    }
    if (button.getName() == "Car Mode")
    {
        const float lampSize = plate.getHeight() * 0.13f;
        g.setColour (button.getToggleState() ? ember : juce::Colour (0xff613725));
        g.fillEllipse (plate.getRight() - lampSize * 2.7f, plate.getCentreY() - lampSize * 0.5f, lampSize, lampSize);
    }
    drawButtonBackground (g, button, {}, hover, pressed);
}

void NorthstarLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool hover, bool pressed)
{
    if (button.getButtonText() == "?")
        return;
    if (button.getProperties().contains ("plate"))
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        skinPlate (g, cabinet, bounds);
        if (button.getToggleState() || hover || pressed || button.hasKeyboardFocus (false))
        {
            g.setColour ((pressed ? juce::Colours::black : amber).withAlpha (pressed ? 0.26f : 0.12f));
            g.fillRoundedRectangle (bounds.reduced (3), 2);
            g.setColour (amber.withAlpha (button.getToggleState() ? 0.80f : 0.60f));
            g.drawRoundedRectangle (bounds.reduced (1), 3, 1);
        }
        return;
    }
    if (hover || pressed || button.hasKeyboardFocus (false))
    {
        g.setColour (amber.withAlpha (pressed ? 0.55f : 0.30f));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.5f), 4.0f, 1.4f);
    }
}

void NorthstarLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                         bool hover, bool pressed)
{
    if (button.getProperties().contains ("plate"))
    {
        auto bounds = button.getLocalBounds().reduced (5, 2);
        const bool preset = button.getName() == "Presets";
        if (preset)
        {
            const float x = (float) bounds.getRight() - 8, y = (float) bounds.getCentreY();
            juce::Path arrow;
            arrow.addTriangle (x - 3, y - 1.5f, x + 3, y - 1.5f, x, y + 2.5f);
            g.setColour (amber.withAlpha (0.8f));
            g.fillPath (arrow);
            bounds.removeFromRight (17);
        }
        g.setColour (hover ? juce::Colour (0xfffbd9ad) : juce::Colour (0xffedbf94));
        g.setFont (juce::FontOptions ("Georgia", juce::jmax (10.0f, bounds.getHeight() * (preset ? 0.51f : 0.54f)), juce::Font::bold));
        g.drawFittedText (button.getButtonText(), bounds, juce::Justification::centred, 1);
        return;
    }
    if (button.getButtonText() != "?")
        return;
    const auto mark = juce::Rectangle<float> (13, 13).withCentre (button.getLocalBounds().toFloat().getCentre());
    const bool active = hover || pressed || button.hasKeyboardFocus (false);
    g.setColour (juce::Colour (active ? 0xff63361f : 0xff1d120b));
    g.fillEllipse (mark);
    g.setColour (amber.withAlpha (active ? 1.0f : 0.75f));
    g.drawEllipse (mark, 1);
    g.setFont (juce::FontOptions ("Georgia", 11.0f, juce::Font::bold));
    g.drawText ("?", mark, juce::Justification::centred);
}

void NorthstarLookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g,
                                                      const juce::Path& outline, juce::Image&)
{
    g.setColour (juce::Colour (0xff22150e));
    g.fillPath (outline);
    g.setColour (juce::Colour (0xffad6940));
    g.strokePath (outline, juce::PathStrokeType (1.5f));
}

void NorthstarLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool pressed,
                                       int, int, int, int, juce::ComboBox& box)
{
    const auto area = juce::Rectangle<float> ((float) width, (float) height).reduced (1);
    skinPlate (g, cabinet, area);
    if (pressed || box.hasKeyboardFocus (false))
    {
        g.setColour (amber.withAlpha (0.65f));
        g.drawRoundedRectangle (area.reduced (1), 3, 1);
    }
    const float x = area.getRight() - 16, y = area.getCentreY();
    juce::Path arrow;
    arrow.addTriangle (x - 4, y - 2, x + 4, y - 2, x, y + 3);
    g.setColour (amber);
    g.fillPath (arrow);
}

juce::Font NorthstarLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return juce::Font (juce::FontOptions ("Georgia", juce::jmax (10.0f, (float) box.getHeight() * 0.46f), juce::Font::plain));
}

void NorthstarLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void NorthstarLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (juce::Colour (0xff1d110a));
    skinPlate (g, cabinet, { 0, 0, (float) width, (float) height });
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (6, 6, width - 12, height - 12);
}

juce::Font NorthstarLookAndFeel::getPopupMenuFont()
{
    return juce::Font (juce::FontOptions ("Georgia", 16.0f, juce::Font::plain));
}

NorthstarFXAudioProcessorEditor::NorthstarFXAudioProcessorEditor (NorthstarFXAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    skin = juce::ImageCache::getFromMemory (BinaryData::cabinet_png, BinaryData::cabinet_pngSize);
    const juce::Rectangle<int> textureArea ((int) (skin.getWidth() * 0.70f), (int) (skin.getHeight() * 0.48f),
                                             (int) (skin.getWidth() * 0.074f), (int) (skin.getHeight() * 0.115f));
    wood = juce::Image (juce::Image::RGB, textureArea.getWidth(), textureArea.getHeight(), true);
    juce::Graphics texture (wood);
    texture.drawImageAt (skin, -textureArea.getX(), -textureArea.getY());
    setLookAndFeel (&look);
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (760, 570, 1513, 1135);
    getConstrainer()->setFixedAspectRatio (windowRatio);

    auto& state = processor.apvts;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.setName (juce::String::fromUTF8 (layout[i].name));
        knob.setTooltip (knob.getName());
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setTextValueSuffix (layout[i].unit);
        addAndMakeVisible (knob);
        attachments[i] = std::make_unique<SA> (state, layout[i].parameter, knob);
        knob.snapToValue();
        knob.onValueChange = [this, i] { repaint (scaled (layout[i].readout)); };
        if (i >= sub && i <= voice)
            knob.onDragStart = [this] {
                if (! car.getToggleState())
                    car.setToggleState (true, juce::sendNotificationSync);
            };
    }
    knobs[yard].setTooltip (juce::String::fromUTF8 ("Дистанция от машины. 0% — выключено"));
    knobs[monoLow].setTooltip (juce::String::fromUTF8 ("Сужение стереониза ниже 110 Гц. 0% — выключено"));
    knobs[antiRumble].setTooltip (juce::String::fromUTF8 ("Динамическое ослабление гула около 205 Гц"));
    knobs[compression].setTooltip (juce::String::fromUTF8 ("Стереокомпрессор 4:1, порог −24 dBFS. 0% — выключено"));
    tapeButton.setName ("Tape Stop");
    tapeButton.getProperties().set ("plate", true);
    tapeButton.setTooltip (juce::String::fromUTF8 ("Остановка ленты с автоматическим возвратом за 0,72 с"));
    tapeButton.onClick = [this] { processor.triggerTapeStop(); };
    addAndMakeVisible (tapeButton);
    orbit.setTooltip ("Enable spatial orbit");
    car.setName ("Car Mode");
    car.setTooltip (juce::String::fromUTF8 ("Включить САБ, ПАНЧ и ГОЛОС"));
    addAndMakeVisible (orbit);
    addAndMakeVisible (car);
    aOrbit = std::make_unique<BA> (state, "orbit", orbit);
    aCar = std::make_unique<BA> (state, "car", car);
    orbit.onStateChange = [this] { repaint (scaled (compassArea)); };
    orbitSync.setName (juce::String::fromUTF8 ("Орбита в такт"));
    orbitSync.setTooltip (juce::String::fromUTF8 ("Длительность оборота 12D Orbit по темпу проекта"));
    const char* divisions[] { "Свободно", "½ такта", "1 такт", "2 такта", "4 такта" };
    for (int i = 0; i < 5; ++i)
        orbitSync.addItem (juce::String::fromUTF8 (divisions[i]), i + 1);
    orbitSync.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff27180e));
    orbitSync.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff9a6a47));
    orbitSync.setColour (juce::ComboBox::textColourId, juce::Colour (0xffe4b990));
    orbitSync.setColour (juce::ComboBox::arrowColourId, amber);
    addAndMakeVisible (orbitSync);
    aOrbitSync = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "orbitSync", orbitSync);
    orbitSync.onChange = [this] { repaint (scaled ({ 1260, 1044, 211, 48 })); };

    const auto plate = [this] (juce::Button& button, const char* text, const char* name)
    {
        button.setButtonText (juce::String::fromUTF8 (text));
        button.setName (name);
        button.setTitle (juce::String::fromUTF8 (text));
        button.getProperties().set ("plate", true);
        addAndMakeVisible (button);
    };
    plate (presetName, "Текущие настройки", "Presets");
    plate (presetPrev, "‹", "Previous Preset");
    plate (presetNext, "›", "Next Preset");
    plate (compareA, "A", "Compare A");
    plate (compareB, "B", "Compare B");
    plate (storePreset, "Сохранить свой", "Save Preset");
    plate (levelMatch, "Ровный уровень", "Level Match");
    presetPrev.setTitle (juce::String::fromUTF8 ("Предыдущий пресет"));
    presetNext.setTitle (juce::String::fromUTF8 ("Следующий пресет"));
    presetName.onClick = [this] { showPresetMenu(); };
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (1); };
    compareA.onClick = [this] { processor.presetBank.switchTo (0); refreshPresetControls(); };
    compareB.onClick = [this] { processor.presetBank.switchTo (1); refreshPresetControls(); };
    storePreset.onClick = [this] { savePreset(); };
    aLevelMatch = std::make_unique<BA> (state, "levelMatch", levelMatch);
    const auto telegramIcon = juce::ImageCache::getFromMemory (BinaryData::telegram_png, BinaryData::telegram_pngSize)
        .rescaled (192, 192, juce::Graphics::highResamplingQuality);
    telegram.setImages (false, true, true, telegramIcon, 1.0f, {},
                         {}, 1.0f, amber.withAlpha (0.10f), {}, 1.0f, juce::Colours::black.withAlpha (0.20f));
    telegram.setTitle ("Telegram · @shur2016");
    telegram.setDescription (juce::String::fromUTF8 ("Открыть Telegram-канал https://t.me/shur2016"));
    telegram.setTooltip ("Telegram · t.me/shur2016");
    telegram.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    telegram.setWantsKeyboardFocus (true);
    telegram.onClick = [] { juce::URL ("https://t.me/shur2016").launchInDefaultBrowser(); };
    addAndMakeVisible (telegram);
    refreshPresetControls();
    displayedReduction = processor.limiterReduction.load();
    static_assert (std::size (help) == std::tuple_size_v<decltype (helpButtons)>);
    for (size_t i = 0; i < helpButtons.size(); ++i)
    {
        auto& button = helpButtons[i];
        const auto name = juce::String::fromUTF8 ("Объяснить · ") + juce::String::fromUTF8 (help[i].title);
        button.setButtonText ("?");
        button.setTitle (name);
        button.setTooltip (name);
        button.onClick = [this, i]
        {
            if (helpPopup != nullptr)
            {
                helpPopup->exitModalState (0);
                helpPopup->setVisible (false);
            }
            auto& popup = juce::CallOutBox::launchAsynchronously (std::make_unique<HelpCard> (help[i]),
                                                                 helpButtons[i].getBounds(), this);
            popup.setDismissalMouseClicksAreAlwaysConsumed (false);
            helpPopup = &popup;
        };
        addAndMakeVisible (button);
    }
    setSize (1059, 794);
#if JUCE_WINDOWS
    BOOL animationsEnabled = TRUE;
    if (::SystemParametersInfoW (SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0))
        reducedMotion = animationsEnabled == FALSE;
#endif
    sceneIntensity = getSceneTarget();
    previousSceneTarget = sceneIntensity;
    previousBassImpact = processor.latestBassImpact();
    if (! reducedMotion)
        shelf.prepare (skin);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

NorthstarFXAudioProcessorEditor::~NorthstarFXAudioProcessorEditor()
{
    stopTimer();
    if (helpPopup != nullptr)
        helpPopup->exitModalState (0);
    if (saveDialog != nullptr)
        saveDialog->exitModalState (0);
    setLookAndFeel (nullptr);
}

void NorthstarFXAudioProcessorEditor::choosePreset (const PresetBank::Preset& preset)
{
    presetFlash = 1;
    processor.presetBank.select (preset);
    refreshPresetControls();
}

void NorthstarFXAudioProcessorEditor::refreshPresetControls()
{
    const auto title = processor.presetBank.title();
    if (presetName.getButtonText() != title)
    {
        presetName.setButtonText (title);
        presetName.setTitle (juce::String::fromUTF8 ("Пресеты · ") + title);
    }
    const int selected = processor.presetBank.activeSlot();
    compareA.setToggleState (selected == 0, juce::dontSendNotification);
    compareB.setToggleState (selected == 1, juce::dontSendNotification);
}

void NorthstarFXAudioProcessorEditor::stepPreset (int direction)
{
    const auto entries = processor.presetBank.list();
    const auto title = processor.presetBank.title();
    const auto found = std::find_if (entries.begin(), entries.end(), [&title] (const auto& entry) {
        return title == entry.name || title == entry.name + " *";
    });
    const int count = (int) entries.size();
    const int current = found == entries.end() ? (direction > 0 ? -1 : 0) : (int) (found - entries.begin());
    choosePreset (entries[(size_t) ((current + direction + count) % count)]);
}

void NorthstarFXAudioProcessorEditor::showPresetMenu()
{
    auto entries = processor.presetBank.list();
    juce::PopupMenu menu;
    const auto title = processor.presetBank.title();
    menu.addSectionHeader (juce::String::fromUTF8 ("Пресеты"));
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i == 3)
            menu.addSectionHeader (juce::String::fromUTF8 ("Свои пресеты"));
        const auto& entry = entries[i];
        menu.addItem ((int) i + 1, entry.name, true, title == entry.name || title == entry.name + " *");
    }
    const juce::Component::SafePointer<NorthstarFXAudioProcessorEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName).withParentComponent (this)
                        .withMinimumWidth (270).withStandardItemHeight (30),
                        [safe, entries = std::move (entries)] (int result) {
        if (safe != nullptr && result > 0 && (size_t) result <= entries.size())
            safe->choosePreset (entries[(size_t) result - 1]);
    });
}

void NorthstarFXAudioProcessorEditor::savePreset()
{
    if (saveDialog != nullptr)
        return;
    auto* dialog = new juce::AlertWindow (juce::String::fromUTF8 ("Сохранить свой пресет"),
        juce::String::fromUTF8 ("Введи имя для текущих настроек."), juce::MessageBoxIconType::NoIcon, this);
    dialog->setLookAndFeel (&juce::LookAndFeel::getDefaultLookAndFeel());
    dialog->addTextEditor ("name", {}, juce::String::fromUTF8 ("Имя пресета"));
    dialog->getTextEditor ("name")->setInputRestrictions (48);
    dialog->addButton (juce::String::fromUTF8 ("Сохранить"), 1, juce::KeyPress (juce::KeyPress::returnKey));
    dialog->addButton (juce::String::fromUTF8 ("Отмена"), 0, juce::KeyPress (juce::KeyPress::escapeKey));
    dialog->addToDesktop (juce::ComponentPeer::windowHasDropShadow);
    dialog->centreAroundComponent (this, dialog->getWidth(), dialog->getHeight());
    saveDialog = dialog;
    const juce::Component::SafePointer<NorthstarFXAudioProcessorEditor> safe (this);
    const juce::Component::SafePointer<juce::AlertWindow> window (dialog);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe == nullptr || window == nullptr)
            return;
        safe->saveDialog = nullptr;
        if (result == 1)
        {
            if (safe->processor.presetBank.save (window->getTextEditorContents ("name")))
                safe->refreshPresetControls();
            else
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                    juce::String::fromUTF8 ("Пресет не сохранён"),
                    juce::String::fromUTF8 ("Введи новое имя и проверь доступ к папке пресетов."));
        }
    }), true);
}

void NorthstarFXAudioProcessorEditor::drawCompassEffects (juce::Graphics& g) const
{
    const juce::Point<float> centre (compassX, compassY);
    const auto power = [this] (size_t i) { return knobs[i].position * knobs[i].glow * (1.0f - speakerReveal); };
    const float bassGlow = juce::jmax (displayedBass * 0.65f * (1.0f - speakerReveal),
                                      power (bass), juce::jmax (power (sub), power (punch)));
    const float roomGlow = juce::jmax (power (room), power (slow), power (yard));
    const float sweepGlow = juce::jmax (power (low), power (mix), power (speed), power (voice));
    const float compressionRadius = 191.0f - 27.0f * displayedCompression;
    g.setColour (amber.withAlpha (0.7f * displayedCompression * (1.0f - speakerReveal)));
    g.drawEllipse (juce::Rectangle<float> (compressionRadius * 2, compressionRadius * 2).withCentre (centre), 3);
    if (displayedTape > 0)
    {
        const float fade = std::sin (juce::MathConstants<float>::pi * displayedTape);
        g.setColour (juce::Colours::black.withAlpha (0.65f * fade));
        g.fillEllipse (juce::Rectangle<float> (400, 400).withCentre (centre));
        g.setColour (ember.withAlpha (0.85f));
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, 198, 198, 0, 0,
                          juce::MathConstants<float>::twoPi * displayedTape, true);
        g.strokePath (arc, juce::PathStrokeType (4));
    }

    // Bass and kick produce expanding amber pulses. Reverb and tape add a softer set of echoes.
    for (int i = 0; i < 3; ++i)
    {
        const float phase = std::fmod (animationClock * 1.45f + (float) i / 3.0f, 1.0f);
        const float radius = 30.0f + phase * 174.0f;
        const float alpha = (1.0f - phase) * (1.0f - phase);
        if (bassGlow > 0.0f)
        {
            g.setColour (ember.withAlpha (alpha * bassGlow * 0.72f));
            g.drawEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (centre), 2.2f);
        }
        if (roomGlow > 0.0f)
        {
            const float echo = 28.0f + phase * 155.0f;
            g.setColour (amber.withAlpha (alpha * roomGlow * 0.58f));
            g.drawEllipse (juce::Rectangle<float> (echo * 2, echo * 2).withCentre (centre), 1.4f);
        }
    }
    if (sweepGlow > 0.0f)
    {
        const float start = animationClock * 2.0f;
        juce::Path sweep;
        sweep.addCentredArc (centre.x, centre.y, 173, 173, 0, start, start + 0.45f + 3.5f * sweepGlow, true);
        g.setColour (amber.withAlpha (sweepGlow * 0.72f));
        g.strokePath (sweep, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (orbit.getToggleState())
    {
        for (int i = 7; i >= 0; --i)
        {
            const auto point = atAngle (centre, 204.0f, (float) visualAngle - 0.045f * i);
            const float diameter = 8.0f - 0.58f * i;
            g.setColour (ember.withAlpha (0.84f - 0.10f * i));
            g.fillEllipse (juce::Rectangle<float> (diameter, diameter).withCentre (point));
        }
    }
}

void NorthstarFXAudioProcessorEditor::drawSpeaker (juce::Graphics& g) const
{
    if (speakerReveal < 0.005f || ! g.clipRegionIntersects (compassArea.getSmallestIntegerContainer()))
        return;
    // The basket stays bolted to the wood. Only the surround, cone and dust cap flex.
    const auto face = juce::Rectangle<float> (443, 443).withCentre ({ compassX, compassY });
    speaker.draw (g, face, reducedMotion ? 0.0f : coneTravel, speakerReveal);
}

float NorthstarFXAudioProcessorEditor::getSceneTarget() const
{
    const auto amount = [this] (size_t index) { return knobs[index].position; };
    const float carAmount = car.getToggleState() ? 1.0f : 0.0f;
    const float orbitAmount = orbit.getToggleState() ? amount (speed) : 0.0f;
    const std::array<float, 12> levels {
        amount (bass), amount (low) * amount (bass), amount (slow), amount (room), orbitAmount,
        amount (yard), amount (monoLow), amount (antiRumble), amount (compression),
        carAmount * amount (sub), carAmount * amount (punch), carAmount * amount (voice)
    };
    float peak = 0.0f, sum = 0.0f;
    for (const float level : levels)
    {
        peak = juce::jmax (peak, level);
        sum += level;
    }
    return amount (mix) * (0.70f * peak + 0.30f * sum / (float) levels.size());
}

void NorthstarFXAudioProcessorEditor::drawCabinetScene (juce::Graphics& g) const
{
    const float strength = sceneIntensity;
    const float time = reducedMotion ? 0.28f : sceneTime;
    const float passing = std::fmod (time + 0.2f, 1.0f);
    const float fade = std::sin (juce::MathConstants<float>::pi * passing);
    const float headlightX = 42 + 1430 * passing;
    const float roadLight = 0.87f + 0.38f * sceneStyle[0];
    const float nightLight = 0.88f + 0.42f * sceneStyle[1];
    shelf.draw (g, headlightX + 18.0f, strength * fade * roadLight);
    if (strength < 0.002f)
        return;
    const float bassHit = juce::jlimit (0.0f, 1.0f, displayedBass + 0.45f * displayedActivity[0]);
    const float cycle = juce::MathConstants<float>::twoPi * time;
    const auto visible = [&g] (juce::Rectangle<float> area) { return g.clipRegionIntersects (area.getSmallestIntegerContainer()); };
    if (visible (hingeGlint))
    {
        const float glint = 0.14f + 0.56f * juce::jmax (0.0f, std::sin (cycle - 0.9f)) * roadLight;
        const float y = 42 + 20 * std::sin (cycle);
        const juce::Point<float> hinge (758, y);
        g.setGradientFill (juce::ColourGradient (amber.withAlpha (glint * strength), hinge,
                                                  amber.withAlpha (0.0f), hinge.translated (0, 24), true));
        g.fillEllipse (juce::Rectangle<float> (4.5f, 34.0f).withCentre (hinge));
    }
    if (visible (carTrim))
    {
        const auto lamp = juce::Point<float> (headlightX, 1007);
        g.setGradientFill (juce::ColourGradient (ember.withAlpha (strength * fade * (0.20f + 0.33f * speakerHit)),
                                                  lamp, ember.withAlpha (0.0f), lamp.translated (130, 0), true));
        g.fillEllipse (juce::Rectangle<float> (260, 3.5f).withCentre (lamp));
    }
    if (visible (searchlightArea))
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (searchlightArea.getSmallestIntegerContainer());
        const juce::Point<float> lamp (1360, 53);
        const auto end = lamp + juce::Point<float> (-430, 25 + 49 * std::sin (cycle));
        const float width = 10 + 53 * strength;
        for (int i = 3; i >= 1; --i)
        {
            const float spread = width * (0.50f + 0.25f * i);
            juce::Path cone;
            cone.startNewSubPath (lamp);
            cone.lineTo (end.x, end.y - spread);
            cone.lineTo (end.x, end.y + spread);
            cone.closeSubPath();
            g.setGradientFill (juce::ColourGradient (amber.withAlpha (strength * nightLight * 0.075f), lamp,
                                                      amber.withAlpha (0.0f), end, false));
            g.fillPath (cone);
        }
        g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.47f * strength), lamp,
                                                  amber.withAlpha (0.0f), lamp.translated (13, 0), true));
        g.fillEllipse (juce::Rectangle<float> (26, 26).withCentre (lamp));
        // Distant lights sit along the skyline on the same panel.
        for (int i = 0; i < 7; ++i)
        {
            const float light = 0.58f + 0.22f * std::sin (cycle * 2 + (float) i * 2.3f);
            g.setColour (amber.withAlpha (strength * light * 0.42f));
            g.fillRect (1082.0f + 21.0f * i, 86.0f - (float) (i % 3) * 3.0f, 2.2f, 1.6f);
        }
    }
    for (const auto strip : { leftShadows, rightShadows })
    {
        if (! visible (strip))
            continue;
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (strip.getSmallestIntegerContainer());
        for (int i = 0; i < 3; ++i)
        {
            const float x = strip.getX() - 10.0f + 16.0f * i + 12.0f * std::sin (cycle);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.40f * strength), x, 0,
                                                      juce::Colours::black.withAlpha (0.0f), x + 8, 0, false));
            g.fillRect (x, strip.getY(), 8.0f, strip.getHeight());
        }
    }
    if (visible (smokeArea))
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (smokeArea.getSmallestIntegerContainer());
        const juce::Colour haze (0xffd6c4b5);
        for (int i = 0; i < 4; ++i)
        {
            const float rise = std::fmod (time * 2 + 0.25f * i, 1.0f);
            const float y = 717 - 132 * rise;
            const float x = 109 + (8 + 22 * rise) * std::sin (cycle * 3 + i);
            const float alpha = strength * nightLight * (0.16f + 0.08f * bassHit)
                * std::sin (juce::MathConstants<float>::pi * rise);
            juce::Path wisp;
            wisp.startNewSubPath (x - 6, y + 22);
            wisp.cubicTo (x + 24, y + 9, x - 22, y - 9, x + 8, y - 20);
            g.setColour (haze.withAlpha (alpha * 0.42f));
            g.strokePath (wisp, juce::PathStrokeType (8 + 5 * rise, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (haze.withAlpha (alpha));
            g.strokePath (wisp, juce::PathStrokeType (2.3f + 2.5f * rise, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (ember.withAlpha (strength * (0.23f + 0.36f * bassHit)));
        g.fillEllipse (104, 713, 5, 3);
        if (! reducedMotion)
            for (int i = 0; i < 5; ++i)
            {
                const float rise = std::fmod (time * 3.0f + 0.19f * i, 1.0f);
                const float x = 108.0f + rise * (10.0f + i * 2.5f) * std::sin (cycle * 2.0f + i);
                g.setColour (amber.withAlpha (0.57f * strength * (1 - rise) * (0.22f + 0.78f * speakerHit)));
                g.fillEllipse (x, 714.0f - rise * 86.0f, 1.9f - rise, 1.7f - rise);
            }
    }
    if (visible (shelfArea))
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (shelfArea.getSmallestIntegerContainer());
        const float y = 782;
        for (int i = 0; i < 2; ++i)
        {
            const juce::Point<float> headlight (headlightX + 36 * i, y + 4 * i);
            g.setGradientFill (juce::ColourGradient (amber.withAlpha (0.41f * strength * fade * roadLight), headlight,
                                                      amber.withAlpha (0.0f), headlight.translated (74, 0), true));
            g.fillEllipse (juce::Rectangle<float> (148, 12).withCentre (headlight));
        }
    }
}

void NorthstarFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0706));
    g.addTransform (juce::AffineTransform::scale (getWidth() / cabinetWidth, getHeight() / designHeight));
    g.drawImage (skin, { 0, 0, cabinetWidth, cabinetHeight });
    drawCabinetScene (g);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setTiledImageFill (wood, 2, 12, 1.0f);
        g.fillRoundedRectangle (presetArea, 5);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (presetArea, 5);
    }
    cabinetPanel (g, skin, wood, compareArea);
    g.setColour (juce::Colour (0xffdeb58d));
    g.setFont (juce::FontOptions ("Georgia", 16.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("ПРЕСЕТ"), 1115, 131, 70, 32, juce::Justification::centred);
    g.setFont (juce::FontOptions ("Georgia", 18.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("СРАВНЕНИЕ"), 814, 845, 160, 22, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions ("Georgia", 17.0f, juce::Font::plain));
    const auto db = (shownMatchDb >= 0.05f ? "+" : "") + juce::String (shownMatchDb, 1) + " dB";
    g.drawText (levelMatch.getToggleState() ? db : juce::String::fromUTF8 ("Выкл."),
                1062, 918, 112, 38, juce::Justification::centred);
    g.setColour (juce::Colour (0xff422918));
    g.fillRoundedRectangle (804, 965, 383, 3, 1.5f);
    g.setColour (amber.withAlpha (0.4f + 0.24f * speakerHit));
    const float width = levelMatch.getToggleState() ? juce::jlimit (-185.0f, 185.0f, shownMatchDb * 15.0f) : 0.0f;
    g.fillRoundedRectangle (995 + juce::jmin (0.0f, width), 965, std::abs (width), 3, 1.5f);
    const juce::Rectangle<float> extension (9, 1018, 1495, 102);
    const juce::Rectangle<float> effectsPanel (9, 1130, 1495, 120);
    for (auto panel : { extension, effectsPanel })
        cabinetPanel (g, skin, wood, panel);
    for (size_t i = monoLow; i <= compression; ++i)
        skinPlate (g, skin, layout[i].readout);
    g.setColour (juce::Colour (0xff68432a));
    for (int x : { 428, 842, 1230 })
        g.drawVerticalLine (x, 1147, 1235);
    g.setColour (juce::Colour (0xffe4b990));
    g.setFont (juce::FontOptions ("Georgia", 20.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("МОНО НИЗ"), 32, 1170, 173, 37, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("АНТИГУДЁЖ"), 449, 1170, 168, 37, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("ПОДЖИМ"), 869, 1170, 137, 37, juce::Justification::centredLeft);

    const float cuts[] { displayedRumble, displayedCompression };
    for (int i = 0; i < 2; ++i)
    {
        const float x = i == 0 ? 710.0f : 1105.0f;
        g.setColour (amber);
        g.fillRoundedRectangle (x, 1215, 104 * cuts[i], 4, 2);
        g.setFont (juce::FontOptions ("Georgia", 16.0f, juce::Font::plain));
        g.drawText (juce::String (juce::Decibels::gainToDecibels (1.0f - cuts[i]), 1) + " dB",
                    (int) x, 1222, 104, 20, juce::Justification::centred);
    }
    g.setColour (ember);
    g.fillRoundedRectangle (1264, 1215, 202 * displayedTape, 4, 2);
    g.setFont (juce::FontOptions ("Georgia", 16.0f, juce::Font::plain));
    g.drawText (juce::String::fromUTF8 (displayedTape > 0 ? "ЛЕНТА → СТАРТ" : "0,72 с"),
                1254, 1222, 222, 20, juce::Justification::centred);
    g.setColour (juce::Colour (0xff83532f));
    skinPlate (g, skin, layout[yard].readout);
    g.drawVerticalLine (686, 1037, 1101);
    g.setColour (juce::Colour (0xffdcb186));
    g.setFont (juce::FontOptions ("Georgia", 27.0f, juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("ДВОР"), 40, 1047, 231, 44, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("ОРБИТА В ТАКТ"), 728, 1047, 313, 44, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions ("Georgia", 19.0f, juce::Font::plain));
    if (orbitSync.getSelectedId() > 1)
    {
        const auto tempo = displayedTempo > 0.0f ? juce::String (displayedTempo, 1) + " BPM"
                                                  : juce::String::fromUTF8 ("ЖДЁТ ТЕМП");
        g.drawText (tempo, 1260, 1044, 211, 48, juce::Justification::centred);
    }
    drawSpeaker (g);
    drawCompassEffects (g);
    g.setColour (juce::Colour (0xffe4b990));
    g.setFont (juce::FontOptions ("Georgia", 22.0f, juce::Font::plain));
    for (size_t i = 0; i < knobs.size(); ++i)
        g.drawText (juce::String (knobs[i].getValue(), layout[i].decimals) + layout[i].unit,
                    layout[i].readout, juce::Justification::centred);
    const float decibels = juce::Decibels::gainToDecibels (1.0f - displayedReduction);
    g.drawText (juce::String (decibels, 1) + " dB", limiterPlate, juce::Justification::centred);
    const float lamp = juce::jlimit (0.0f, 1.0f, displayedReduction * 7.0f);
    if (lamp > 0.005f)
    {
        g.setColour (ember.withAlpha (0.22f * lamp));
        g.fillEllipse (1344, 884, 30, 30);
        g.setColour (ember.withAlpha (lamp));
        g.fillEllipse (1351, 891, 16, 16);
    }
    for (size_t i = 0; i < displayedActivity.size(); ++i)
    {
        const float x = layout[sub + i].readout.getX();
        g.setColour (juce::Colour (0xff312017));
        g.fillRoundedRectangle (x, 983, 100, 3, 1.5f);
        g.setColour (amber);
        g.fillRoundedRectangle (x, 983, 100 * displayedActivity[i], 3, 1.5f);
    }
    if (presetFlash > 0)
    {
        g.setColour (amber.withAlpha (0.55f * presetFlash));
        g.drawRoundedRectangle (presetArea, 6, 2.2f);
    }
}

juce::Rectangle<int> NorthstarFXAudioProcessorEditor::scaled (juce::Rectangle<float> area) const
{
    return area.transformedBy (juce::AffineTransform::scale (getWidth() / cabinetWidth,
                                                            getHeight() / designHeight)).getSmallestIntegerContainer();
}

void NorthstarFXAudioProcessorEditor::resized()
{
    telegram.setBounds (scaled ({ 592, 12, 87, 87 }));
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].setBounds (scaled (layout[i].dial));
    orbit.setBounds (scaled ({ 1121, 176, 273, 45 }));
    car.setBounds (scaled ({ 56, 873, 188, 65 }));
    orbitSync.setBounds (scaled ({ 1040, 1047, 210, 43 }));
    tapeButton.setBounds (scaled ({ 1254, 1148, 209, 56 }));
    presetPrev.setBounds (scaled ({ 1187, 128, 34, 39 }));
    presetName.setBounds (scaled ({ 1223, 128, 199, 39 }));
    presetNext.setBounds (scaled ({ 1424, 128, 34, 39 }));
    compareA.setBounds (scaled ({ 813, 868, 71, 39 }));
    compareB.setBounds (scaled ({ 890, 868, 71, 39 }));
    storePreset.setBounds (scaled ({ 995, 868, 184, 39 }));
    levelMatch.setBounds (scaled ({ 813, 918, 243, 39 }));
    for (size_t i = 0; i < helpButtons.size(); ++i)
        helpButtons[i].setBounds (scaled (juce::Rectangle<float> (28, 31).withCentre (help[i].centre)));
}

void NorthstarFXAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float elapsed = juce::jlimit (0.0f, 0.05f, (float) ((now - lastTick) * 0.001));
    lastTick = now;
    animationClock = std::fmod (animationClock + elapsed, 600.0f);
    const float response = 1.0f - std::exp (-elapsed / 0.04f);
    displayedBass += response * (processor.bassActivity.load() - displayedBass);
    displayedRumble += response * (processor.rumbleReduction.load() - displayedRumble);
    displayedCompression += response * (processor.compressorReduction.load() - displayedCompression);
    const float previousTape = displayedTape;
    displayedTape = processor.tapeProgress.load();
    if (displayedBass > 0.003f || displayedCompression > 0.003f || displayedTape > 0 || previousTape > 0)
        repaint (scaled (compassArea));
    repaint (scaled ({ 700, 1213, 780, 33 }));
    bool moving = false;
    for (auto& knob : knobs)
        moving |= knob.animate (elapsed);
    const float sceneTarget = getSceneTarget();
    sceneIntensity += (sceneTarget - sceneIntensity) * (1.0f - std::exp (-elapsed / 0.14f));
    if (std::abs (sceneTarget - sceneIntensity) < 0.001f)
        sceneIntensity = sceneTarget;
    const int style = processor.presetBank.style();
    for (size_t i = 0; i < sceneStyle.size(); ++i)
        sceneStyle[i] += (1.0f - std::exp (-elapsed / 0.30f)) * (((int) i + 1 == style ? 1.0f : 0.0f) - sceneStyle[i]);
    const float bassControl = juce::jmax (knobs[bass].position, car.getToggleState()
        ? juce::jmax (knobs[sub].position, 0.88f * knobs[punch].position) : 0.0f);
    const float load = juce::jlimit (0.0f, 1.0f, (bassControl - 0.56f) / 0.36f);
    const float bassDrive = load * load * (3.0f - 2.0f * load) * knobs[mix].position;
    const auto impact = processor.latestBassImpact();
    const bool hit = impact != previousBassImpact;
    previousBassImpact = impact;
    speakerHit *= std::exp (-elapsed / 0.24f);
    if (hit)
    {
        const float attack = BassImpact::strength (impact) * bassDrive;
        speakerHit = juce::jmax (speakerHit, attack);
        if (! reducedMotion)
        {
            coneVelocity = juce::jlimit (-45.0f, 45.0f, coneVelocity + 37.0f * attack);
        }
    }
    const float previousReveal = speakerReveal;
    // Hold the physical speaker through gaps between kicks: no compass/speaker flashing.
    if (bassDrive > 0.02f && displayedBass > 0.028f)
        speakerHold = 0.85f;
    else
        speakerHold = juce::jmax (0.0f, speakerHold - elapsed);
    const float revealTarget = speakerHold > 0 ? bassDrive : 0.0f;
    speakerReveal += (revealTarget - speakerReveal) * (1.0f - std::exp (-elapsed / (revealTarget > speakerReveal ? 0.043f : 0.46f)));
    if (speakerReveal > 0.976f && revealTarget > 0.995f)
        speakerReveal = 1.0f;
    if (speakerReveal > 0.005f || previousReveal > 0.005f)
        repaint (scaled (compassArea));
    presetUiClock += elapsed;
    if (presetUiClock >= 0.10f)
    {
        presetUiClock = 0;
        refreshPresetControls();
        const float correction = processor.levelMatchDb.load();
        if (std::abs (correction - shownMatchDb) > 0.05f || (correction == 0 && shownMatchDb != 0))
        {
            shownMatchDb = correction;
            repaint (scaled (compareArea));
        }
    }
    if (! reducedMotion)
    {
        const int steps = juce::jmax (1, (int) std::ceil (elapsed * 360.0f));
        const float dt = elapsed / (float) steps;
        const float drive = std::sin (animationClock * 27.0f) * 138.0f * speakerReveal * displayedBass;
        for (int i = 0; i < steps; ++i)
        {
            coneVelocity += dt * (drive - 720.0f * coneTravel - 14.0f * coneVelocity);
            const float next = coneTravel + coneVelocity * dt;
            coneTravel = juce::jlimit (-1.0f, 1.0f, next);
            if (next != coneTravel)
                coneVelocity *= -0.20f;
        }
        sceneTime = std::fmod (sceneTime + elapsed * (0.10f + 0.23f * sceneIntensity)
                               * (1.0f - 0.75f * std::sin (juce::MathConstants<float>::pi * displayedTape)), 1.0f);
        const float mixValue = knobs[mix].position;
        if (hit && sceneIntensity > 0.008f)
        {
            const float strength = BassImpact::strength (impact) * (0.36f + 0.73f * sceneIntensity + 0.26f * sceneStyle[2]);
            shelf.strike (strength * (1.0f - 0.78f * std::sin (juce::MathConstants<float>::pi * displayedTape)));
            knobStrikeCooldown = 0.32f;
        }
        knobMomentum = knobMomentum * std::exp (-elapsed / 0.22f)
            + juce::jmax (0.0f, sceneTarget - previousSceneTarget);
        previousSceneTarget = sceneTarget;
        knobStrikeCooldown -= elapsed;
        if (knobMomentum > 0.095f && knobStrikeCooldown <= 0)
        {
            shelf.strike (juce::jmin (0.53f, 0.18f + knobMomentum * 1.7f) * mixValue);
            knobStrikeCooldown = 0.29f;
            knobMomentum = 0;
        }
        const float shelfDrive = displayedBass * sceneIntensity * (0.47f + 0.19f * sceneStyle[2]);
        if (shelf.advance (elapsed, shelfDrive))
            repaint (scaled (ShelfScene::bounds()));
    }
    sceneFrame += elapsed;
    if (sceneFrame >= 1.0f / 30.0f)
    {
        sceneFrame = std::fmod (sceneFrame, 1.0f / 30.0f);
        if ((! reducedMotion && sceneIntensity >= 0.002f)
            || std::abs (sceneIntensity - paintedSceneIntensity) >= 0.002f
            || (sceneIntensity == 0.0f && paintedSceneIntensity > 0.0f))
        {
            for (const auto area : { searchlightArea, smokeArea, leftShadows, rightShadows, shelfArea, hingeGlint, carTrim })
                repaint (scaled (area));
            paintedSceneIntensity = sceneIntensity;
        }
    }
    if (orbit.getToggleState())
    {
        const double phase = processor.orbitDisplayPhase.load();
        moving |= phase != visualAngle;
        visualAngle = phase;
    }
    if (moving)
        repaint (scaled (compassArea));
    const float reduction = processor.limiterReduction.load();
    if (reduction != displayedReduction)
    {
        displayedReduction = reduction;
        repaint (scaled ({ 1294, 880, 132, 86 }));
    }
    for (size_t i = 0; i < displayedActivity.size(); ++i)
    {
        const float activity = processor.carActivity[i].load();
        if (std::abs (displayedActivity[i] - activity) > 0.004f)
        {
            displayedActivity[i] = activity;
            repaint (scaled ({ layout[sub + i].readout.getX(), 982, 101, 6 }));
        }
    }
    const float tempo = processor.orbitTempo.load();
    if (tempo != displayedTempo)
    {
        displayedTempo = tempo;
        repaint (scaled ({ 1260, 1044, 211, 48 }));
    }
    if (presetFlash > 0.0f)
    {
        presetFlash = juce::jmax (0.0f, presetFlash - 2.4f * elapsed);
        repaint (scaled (presetArea.expanded (3)));
    }
}
