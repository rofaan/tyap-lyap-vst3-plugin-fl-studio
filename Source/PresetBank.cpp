#include "PresetBank.h"

namespace
{
constexpr const char* ids[] { "bass", "lowFreq", "slowed", "reverb", "orbit", "orbitRate", "mix",
                              "car", "sub", "punch", "voice", "yard", "orbitSync", "monoLow",
                              "antiRumble", "compression" };
const juce::String unnamed = juce::String::fromUTF8 ("Текущие настройки");
}

PresetBank::Preset PresetBank::capture() const
{
    Preset result;
    static_assert (std::size (ids) == std::tuple_size_v<decltype (result.values)>);
    for (size_t i = 0; i < result.values.size(); ++i)
        result.values[i] = state.getParameter (ids[i])->getValue();
    return result;
}

void PresetBank::apply (const Preset& preset)
{
    for (size_t i = 0; i < preset.values.size(); ++i)
    {
        auto* parameter = state.getParameter (ids[i]);
        if (std::abs (parameter->getValue() - preset.values[i]) > 0.00001f)
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (preset.values[i]);
            parameter->endChangeGesture();
        }
    }
}

std::vector<PresetBank::Preset> PresetBank::list (const juce::File& directory) const
{
    constexpr const char* names[] { "По району", "Ночной город", "Багажник" };
    const float factory[][16] {
        { 3, 95, 0, 8, 0, .18f, 100, 1, 35, 75, 65, 0, 0, 20, 18, 16 },
        { 2, 90, 4, 36, 1, .18f, 100, 1, 32, 22, 35, 12, 2, 0, 14, 0 },
        { 4, 75, 0, 5, 0, .18f, 100, 1, 85, 36, 28, 0, 0, 65, 30, 24 }
    };
    std::vector<Preset> result;
    for (int p = 0; p < 3; ++p)
    {
        Preset preset { juce::String::fromUTF8 (names[p]), p + 1, {} };
        for (size_t i = 0; i < preset.values.size(); ++i)
            preset.values[i] = state.getParameter (ids[i])->convertTo0to1 (factory[p][i]);
        result.push_back (preset);
    }
    std::vector<Preset> custom;
    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.xml"))
        if (file.getSize() < 65536)
            if (auto xml = juce::parseXML (file))
                if (auto entry = decode (juce::ValueTree::fromXml (*xml)); entry && entry->name.isNotEmpty())
                    custom.push_back (*entry);
    std::stable_sort (custom.begin(), custom.end(), [] (const auto& a, const auto& b) {
        return a.name.compareNatural (b.name) < 0;
    });
    result.insert (result.end(), custom.begin(), custom.end());
    return result;
}

void PresetBank::select (const Preset& preset)
{
    {
        const juce::ScopedLock guard (lock);
        reference = preset;
    }
    apply (preset);
}

bool PresetBank::save (const juce::String& text, const juce::File& directory)
{
    const auto name = text.trim().substring (0, 48);
    if (name.isEmpty() || directory.createDirectory().failed())
        return false;
    for (const auto& entry : list (directory))
        if (entry.name.equalsIgnoreCase (name))
            return false;
    auto preset = capture();
    preset.name = name;
    preset.style = style();
    const auto file = directory.getChildFile (juce::Uuid().toString() + ".xml");
    juce::TemporaryFile temporary (file);
    const auto xml = encode (preset).createXml();
    if (xml == nullptr || ! xml->writeTo (temporary.getFile()) || ! temporary.overwriteTargetFileWithTemporary())
        return false;
    const juce::ScopedLock guard (lock);
    reference = preset;
    return true;
}

void PresetBank::switchTo (int index)
{
    if (index < 0 || index >= (int) slots.size())
        return;
    Preset next;
    {
        const juce::ScopedLock guard (lock);
        if (index == active)
            return;
        slots[(size_t) active] = Slot { capture(), reference };
        if (! slots[(size_t) index])
            slots[(size_t) index] = slots[(size_t) active];
        active = index;
        const auto& selected = *slots[(size_t) active];
        next = selected.sound;
        reference = selected.reference;
    }
    apply (next);
}

juce::String PresetBank::title() const
{
    const juce::ScopedLock guard (lock);
    if (! reference)
        return unnamed;
    const auto actual = capture();
    for (size_t i = 0; i < actual.values.size(); ++i)
        if (std::abs (actual.values[i] - reference->values[i]) > 0.0001f)
            return reference->name + " *";
    return reference->name;
}

int PresetBank::activeSlot() const
{
    const juce::ScopedLock guard (lock);
    return active;
}

int PresetBank::style() const
{
    const juce::ScopedLock guard (lock);
    return reference ? reference->style : 0;
}

juce::File PresetBank::library()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Tyap Lyap").getChildFile ("Presets");
}

juce::ValueTree PresetBank::encode (const Preset& preset)
{
    juce::ValueTree tree ("PRESET");
    tree.setProperty ("name", preset.name, nullptr);
    tree.setProperty ("style", preset.style, nullptr);
    for (size_t i = 0; i < preset.values.size(); ++i)
        tree.setProperty (ids[i], preset.values[i], nullptr);
    return tree;
}

std::optional<PresetBank::Preset> PresetBank::decode (const juce::ValueTree& tree) const
{
    if (! tree.hasType ("PRESET"))
        return {};
    Preset result { tree.getProperty ("name").toString().trim().substring (0, 48),
                    juce::jlimit (0, 3, (int) tree.getProperty ("style", 0)), {} };
    for (size_t i = 0; i < result.values.size(); ++i)
    {
        const float fallback = state.getParameter (ids[i])->getDefaultValue();
        const float value = (float) tree.getProperty (ids[i], fallback);
        result.values[i] = std::isfinite (value) ? juce::jlimit (0.0f, 1.0f, value) : fallback;
    }
    return result;
}

juce::ValueTree PresetBank::exportState() const
{
    const juce::ScopedLock guard (lock);
    juce::ValueTree tree ("PRESET_BANK");
    tree.setProperty ("active", active, nullptr);
    if (reference)
        tree.appendChild (encode (*reference), nullptr);
    for (size_t i = 0; i < slots.size(); ++i)
    {
        if (! slots[i] || (int) i == active)
            continue;
        juce::ValueTree item (i == 0 ? "SLOT_A" : "SLOT_B");
        item.appendChild (encode (slots[i]->sound), nullptr);
        if (slots[i]->reference)
            item.appendChild (encode (*slots[i]->reference), nullptr);
        tree.appendChild (item, nullptr);
    }
    return tree;
}

void PresetBank::importState (const juce::ValueTree& tree)
{
    const juce::ScopedLock guard (lock);
    active = juce::jlimit (0, 1, (int) tree.getProperty ("active", 0));
    reference = decode (tree.getChildWithName ("PRESET"));
    slots = {};
    const auto item = tree.getChildWithName (active == 0 ? "SLOT_B" : "SLOT_A");
    if (auto sound = decode (item.getChild (0)))
        slots[(size_t) (1 - active)] = Slot { *sound, decode (item.getChild (1)) };
}
