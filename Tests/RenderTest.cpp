#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

namespace
{
bool writePng (const juce::Image& image, const juce::File& file)
{
    auto output = file.createOutputStream();
    juce::PNGImageFormat png;
    return output != nullptr && output->setPosition (0) && output->truncate().wasOk()
        && png.writeImageToStream (image, *output);
}

bool checkDetector (int rate)
{
    BassImpact detector;
    detector.prepare (rate);
    for (int i = 0; i < rate / 4; ++i)
        detector.push (0.001f);
    if (detector.latest() != 0) return false;
    for (int i = 0; i < rate / 5; ++i)
        detector.push (0.07f);
    const auto first = detector.latest();
    if (BassImpact::strength (first) <= 0.05f) return false;
    for (int i = 0; i < rate; ++i)
        detector.push (0.07f);
    if (detector.latest() != first) return false;
    for (int i = 0; i < rate / 3; ++i)
        detector.push (0.0f);
    for (int i = 0; i < rate / 5; ++i)
        detector.push (0.22f);
    return detector.latest() != first && BassImpact::strength (detector.latest()) > BassImpact::strength (first);
}
}

int main(int argc,char** argv)
{
    std::cout << std::unitbuf;
    juce::ScopedJuceInitialiser_GUI gui;
    NorthstarFXAudioProcessor p;
    p.prepareToPlay(48000,512);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> b(2,512);
    for(int block=0;block<100;++block){
        b.clear(); if(block==0){b.setSample(0,0,0.5f);b.setSample(1,0,0.5f);}
        p.processBlock(b,midi);
        for(int c=0;c<2;++c)for(int i=0;i<512;++i)if(!std::isfinite(b.getSample(c,i)))return 2;
    }
    auto set = [&p] (const char* id, float value) {
        auto* parameter = p.apvts.getParameter (id);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };
    set ("monoLow", 76);
    set ("antiRumble", 55);
    set ("compression", 42);
    set ("tapeStop", 1);
    juce::MemoryBlock saved;
    p.getStateInformation (saved);
    set ("monoLow", 0);
    p.setStateInformation (saved.getData(), (int) saved.getSize());
    if (p.apvts.getRawParameterValue ("monoLow")->load() != 76 ||
        p.apvts.getRawParameterValue ("tapeStop")->load() != 0) return 15;
    auto legacy = p.apvts.copyState();
    for (const char* id : { "monoLow", "antiRumble", "compression", "tapeStop" })
        legacy.removeChild (legacy.getChildWithProperty ("id", id), nullptr);
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), oldState);
    p.setStateInformation (oldState.getData(), (int) oldState.getSize());
    for (const char* id : { "monoLow", "antiRumble", "compression", "tapeStop" })
        if (p.apvts.getRawParameterValue (id)->load() != 0) return 16;
    std::cout << "State round trip and legacy parameter defaults PASS\n";
    set ("slowed", 0);
    set ("reverb", 0);
    set ("orbit", 0);
    set ("bass", 0);
    auto measure = [&] (float frequency, float level, float rightPolarity) {
        double energy[2] {};
        float peak = 0;
        for (int block = 0; block < 48; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float signal = level * std::sin (juce::MathConstants<float>::twoPi * frequency
                                      * (float) (block * 512 + i) / 48000.0f);
                b.setSample (0, i, signal);
                b.setSample (1, i, signal * rightPolarity);
            }
            p.processBlock (b, midi);
            for (int i = 0; i < 512; ++i)
            {
                const float l = b.getSample (0, i), r = b.getSample (1, i);
                peak = juce::jmax (peak, std::abs (l), std::abs (r));
                if (block >= 24)
                {
                    energy[0] += l * l;
                    energy[1] += (l - r) * (l - r);
                }
            }
        }
        return std::array<double, 3> { energy[0], energy[1], peak };
    };
    const auto stereoBass = measure (35, 0.1f, -1.0f);
    set ("car", 1);
    const auto centredBass = measure (35, 0.1f, -1.0f);
    if (centredBass[1] >= stereoBass[1] * 0.35) return 4;
    set ("sub", 0);
    const auto base = measure (40, 0.1f, 1.0f);
    set ("sub", 100);
    const auto boosted = measure (40, 0.1f, 1.0f);
    if (boosted[0] < base[0] * 1.5) return 5;
    set ("bass", 18);
    const auto loud = measure (40, 0.95f, 1.0f);
    if (loud[2] > 0.89126 || p.limiterReduction.load() <= 0) return 6;
    std::cout << "Car mode bass centering, sub boost and limiter ceiling PASS\n";
    p.limiterReduction.store (0);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    int helpCount = 0;
    juce::TextButton* firstHelp = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child);
            button != nullptr && button->getButtonText() == "?")
        {
            if (! button->onClick || button->getTitle().isEmpty()) return 17;
            if (firstHelp == nullptr) firstHelp = button;
            ++helpCount;
        }
    if (helpCount != 22) return 18;
    for (const auto* name : { "Sub", "Punch", "Voice" })
    {
        set ("car", 0);
        juce::Slider* knob = nullptr;
        for (auto* child : editor->getChildren())
            if (child->getName() == name)
                knob = dynamic_cast<juce::Slider*> (child);
        if (knob == nullptr) return 7;
        const juce::Slider::ScopedDragNotification gesture (*knob);
        if (p.apvts.getRawParameterValue ("car")->load() < 0.5f) return 8;
    }
    std::cout << "All three knobs activate Car Mode PASS\n";
    const float expectedSub[] { 35, 32, 85 };
    const auto getButton = [&editor] (const char* name) -> juce::Button*
    {
        for (auto* child : editor->getChildren())
            if (child->getName() == name)
                return dynamic_cast<juce::Button*> (child);
        return nullptr;
    };
    auto* next = getButton ("Next Preset");
    auto* comparisonA = getButton ("Compare A");
    auto* comparisonB = getButton ("Compare B");
    if (next == nullptr || comparisonA == nullptr || comparisonB == nullptr) return 13;
    for (int i = 0; i < 3; ++i)
    {
        set ("car", 0);
        next->onClick();
        if (p.apvts.getRawParameterValue ("car")->load() != 1 ||
            p.apvts.getRawParameterValue ("sub")->load() != expectedSub[i]) return 14;
    }
    comparisonB->onClick();
    set ("sub", 22);
    if (! p.presetBank.title().endsWith (" *")) return 26;
    comparisonA->onClick();
    if (p.apvts.getRawParameterValue ("sub")->load() != 85) return 27;
    juce::MemoryBlock compareState;
    p.getStateInformation (compareState);
    NorthstarFXAudioProcessor restored;
    restored.setStateInformation (compareState.getData(), (int) compareState.getSize());
    restored.presetBank.switchTo (1);
    if (restored.apvts.getRawParameterValue ("sub")->load() != 22) return 28;
    const auto directory = juce::File::getCurrentWorkingDirectory()
        .getChildFile ("qa-" + juce::Uuid().toString().substring (0, 8));
    const bool savedPreset = p.presetBank.save (juce::String::fromUTF8 ("Проверка"), directory);
    const auto entries = restored.presetBank.list (directory);
    for (const auto& file : directory.findChildFiles (juce::File::findFiles, false, "*.xml"))
        file.deleteFile();
    directory.deleteFile();
    if (! savedPreset || entries.size() != 4 || entries.back().name != juce::String::fromUTF8 ("Проверка"))
    {
        std::cerr << "Saved preset: " << savedPreset << ", library count: " << entries.size() << '\n';
        return 29;
    }
    restored.presetBank.select (entries.back());
    if (restored.apvts.getRawParameterValue ("sub")->load() != 85) return 30;
    std::cout << "Preset arrows, dirty marker, A/B project restore and custom preset disk round trip PASS\n";
    p.presetBank.select (entries[2]);
    std::cout << "All three presets apply parameters and activate Car Mode PASS\n";
    AnimatedKnob* animatedSub = nullptr;
    for (auto* child : editor->getChildren())
        if (child->getName() == "Sub")
            animatedSub = dynamic_cast<AnimatedKnob*> (child);
    if (animatedSub == nullptr) return 9;
    set ("sub", 0);
    animatedSub->snapToValue();
    set ("sub", 100);
    animatedSub->animate (1.0f / 60.0f);
    if (animatedSub->position <= 0 || animatedSub->position >= 1 || animatedSub->glow <= 0) return 10;
    for (int frame = 0; frame < 150; ++frame)
        animatedSub->animate (1.0f / 60.0f);
    if (animatedSub->position != 1 || animatedSub->glow != 0) return 11;
    std::cout << "Knob rotation interpolates and motion glow fades PASS\n";
    set ("bass", 8);
    set ("lowFreq", 110);
    set ("slowed", 3);
    set ("reverb", 26);
    set ("orbit", 1);
    set ("sub", 78);
    set ("punch", 52);
    set ("voice", 35);
    set ("yard", 38);
    set ("orbitSync", 2);
    set ("monoLow", 75);
    set ("antiRumble", 55);
    set ("compression", 42);
    for (auto* child : editor->getChildren())
        if (auto* dial = dynamic_cast<AnimatedKnob*> (child))
            dial->snapToValue();
    animatedSub->setValue (88);
    animatedSub->animate (1.0f / 60.0f);
    if (argc > 2 && juce::String (argv[2]) == "compact")
        editor->setSize (760, 570);
    if (argc > 2 && juce::String (argv[2]) == "reference")
        editor->setSize (1448, 1086);
    if (argc > 2 && juce::String (argv[2]) == "help")
    {
        firstHelp->onClick();
        auto* popup = dynamic_cast<juce::CallOutBox*> (juce::Component::getCurrentlyModalComponent());
        if (popup == nullptr || popup->getParentComponent() != editor.get()) return 19;
    }
    if (argc > 2 && juce::String (argv[2]) == "menu")
    {
        getButton ("Presets")->onClick();
        juce::Timer::callPendingTimersSynchronously();
    }
    if (argc > 2 && juce::String (argv[2]) == "scene")
    {
        // Compare the skyline at zero and full Mix with the audio thread idle.
        auto settle = []
        {
            for (int frame = 0; frame < 65; ++frame)
            {
                juce::Thread::sleep (17);
                juce::Timer::callPendingTimersSynchronously();
            }
        };
        const auto skyline = juce::Rectangle<int> (640, 12, 398, 55);
        set ("mix", 0);
        settle();
        const auto quiet = editor->createComponentSnapshot (skyline);
        set ("sub", 100);
        set ("car", 1);
        set ("mix", 100);
        settle();
        const auto active = editor->createComponentSnapshot (skyline);
        if (quiet.getWidth() != active.getWidth()) return 20;
        int changed = 0;
        for (int y = 0; y < quiet.getHeight(); ++y)
            for (int x = 0; x < quiet.getWidth(); ++x)
                changed += quiet.getPixelAt (x, y) != active.getPixelAt (x, y) ? 1 : 0;
        if (changed < 300) return 21;
        std::cout << "Scene responds to effect amounts without audio: " << changed << " pixels changed\n";
    }
    juce::Image image;
    if (argc > 2 && juce::String (argv[2]) == "shelf")
    {
        SpeakerVisual surface;
        const auto renderCone = [&surface] (float excursion)
        {
            juce::Image frame (juce::Image::RGB, 448, 448, true);
            juce::Graphics graphics (frame);
            surface.draw (graphics, { 0, 0, 448, 448 }, excursion, 1);
            return frame;
        };
        const auto inward = renderCone (-1), outward = renderCone (1);
        int fixedBasketChanges = 0, movingConeChanges = 0;
        for (int y = 0; y < 448; ++y)
            for (int x = 0; x < 448; ++x)
            {
                const float dx = ((float) x + 0.5f - 224) / 216;
                const float dy = ((float) y + 0.5f - 224) / (216 * 0.985f);
                const float r = std::sqrt (dx * dx + dy * dy);
                const auto a = inward.getPixelAt (x, y), z = outward.getPixelAt (x, y);
                const int difference = std::abs (a.getRed() - z.getRed()) + std::abs (a.getGreen() - z.getGreen());
                if (r > 0.94f && r < 0.98f && difference > 6) ++fixedBasketChanges;
                if (r < 0.70f && difference > 20) ++movingConeChanges;
            }
        if (fixedBasketChanges > 12 || movingConeChanges < 3000) return 32;
        std::cout << "Fixed speaker basket and deforming cone PASS (" << fixedBasketChanges
                  << " basket pixels, " << movingConeChanges << " moving cone pixels)\n";
        for (const int rate : { 44100, 48000, 96000 })
            if (! checkDetector (rate)) return 22;
        const auto frames = argc > 3 ? juce::File (juce::String::fromUTF8 (argv[3])) : juce::File();
        if (argc > 3 && frames.createDirectory().failed()) return 23;
        set ("slowed", 0);
        set ("reverb", 0);
        set ("orbit", 0);
        set ("car", 1);
        set ("sub", 100);
        set ("punch", 84);
        set ("yard", 0);
        p.prepareToPlay (48000, 512);
        b.clear();
        for (int block = 0; block < 54; ++block)
            p.processBlock (b, midi);
        for (int frame = 0; frame < 70; ++frame)
        {
            juce::Thread::sleep (16);
            juce::Timer::callPendingTimersSynchronously();
        }

        const auto crop = juce::Rectangle<int> (345, 92, 371, 409);
        const auto liftedDice = juce::Rectangle<int> (604, 418, 68, 19);
        const auto resting = editor->createComponentSnapshot (liftedDice);
        const auto coneSample = juce::Rectangle<int> (428, 164, 204, 167);
        const auto restingCompass = editor->createComponentSnapshot (coneSample);
        juce::AudioBuffer<float> audio (2, 800);
        const auto fillKick = [&audio] (int frame)
        {
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                const int cycle = (frame * audio.getNumSamples() + i) % 24000;
                const double t = (double) cycle / 48000.0;
                const double pitch = 48 * t + 2.1 * (1.0 - std::exp (-25 * t));
                const float input = 0.63f * (float) (std::sin (juce::MathConstants<double>::twoPi * pitch)
                    * (1.0 - std::exp (-900 * t)) * std::exp (-19 * t));
                for (int ch = 0; ch < 2; ++ch)
                    audio.setSample (ch, i, input);
            }
        };
        const auto coneDifference = [] (const juce::Image& shown, const juce::Image& original)
        {
            int count = 0;
            for (int y = 0; y < shown.getHeight(); ++y)
                for (int x = 0; x < shown.getWidth(); ++x)
                    if (std::abs (shown.getPixelAt (x, y).getRed() - original.getPixelAt (x, y).getRed()) > 31)
                        ++count;
            return count;
        };
        auto lastHit = p.latestBassImpact();
        int hits = 0, largestChange = 0, speakerPixels = 0, excursionPixels = 0;
        juce::Image previousCone;
        for (int frame = 0; frame < 96; ++frame)
        {
            fillKick (frame);
            p.processBlock (audio, midi);
            if (p.latestBassImpact() != lastHit)
            {
                lastHit = p.latestBassImpact();
                ++hits;
            }
            juce::Thread::sleep (16);
            juce::Timer::callPendingTimersSynchronously();
            if (frame < 24)
            {
                const auto active = editor->createComponentSnapshot (liftedDice);
                int difference = 0;
                for (int y = 0; y < active.getHeight(); ++y)
                    for (int x = 0; x < active.getWidth(); ++x)
                    {
                        const auto a = active.getPixelAt (x, y), r = resting.getPixelAt (x, y);
                        if (std::abs (a.getRed() - r.getRed()) + std::abs (a.getGreen() - r.getGreen()) > 44)
                            ++difference;
                    }
                if (difference > largestChange)
                {
                    largestChange = difference;
                    image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);
                }
            }
            if (frame == 14)
                speakerPixels = coneDifference (editor->createComponentSnapshot (coneSample), restingCompass);
            if (frame >= 30 && frame < 60)
            {
                const auto cone = editor->createComponentSnapshot (coneSample);
                if (previousCone.isValid())
                    excursionPixels = juce::jmax (excursionPixels, coneDifference (cone, previousCone));
                previousCone = cone;
            }
            if (argc > 3 && frame % 2 == 0)
            {
                const auto name = juce::String (frame / 2).paddedLeft ('0', 3) + ".png";
                if (! writePng (editor->createComponentSnapshot (crop), frames.getChildFile (name))) return 24;
            }
        }
        if (hits < 3 || largestChange < 70 || speakerPixels < 1300 || excursionPixels < 150) return 25;
        set ("bass", 0);
        set ("sub", 15);
        set ("punch", 15);
        for (int frame = 0; frame < 125; ++frame)
        {
            audio.clear();
            p.processBlock (audio, midi);
            juce::Thread::sleep (16);
            juce::Timer::callPendingTimersSynchronously();
        }
        const auto quietCompass = editor->createComponentSnapshot (coneSample);
        for (int frame = 0; frame < 15; ++frame)
        {
            fillKick (frame);
            p.processBlock (audio, midi);
            juce::Thread::sleep (16);
            juce::Timer::callPendingTimersSynchronously();
        }
        const int lowKnobDifference = coneDifference (editor->createComponentSnapshot (coneSample), quietCompass);
        if (lowKnobDifference > speakerPixels / 5) return 31;
        std::cout << "Bass detector, repeated kick impulses and independently lifted dice PASS ("
                  << hits << " impacts, " << largestChange << " lifted pixels, "
                  << speakerPixels << " changed cone pixels at high settings, "
                  << lowKnobDifference << " at low settings, " << excursionPixels << " excursion pixels)\n";
    }
    if (image.isNull())
        image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);
    juce::File file(juce::String::fromUTF8(argc>1?argv[1]:"C:/Users/04092/AppData/Local/Temp/tyap-preview.png"));
    if (! writePng (image, file)) return 12;
    std::cout<<"DSP finite output and editor render PASS\n";
    return 0;
}
