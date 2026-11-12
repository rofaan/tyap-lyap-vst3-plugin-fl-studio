#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ShelfScene.h"
#include "SpeakerVisual.h"

class AnimatedKnob final : public juce::Slider
{
public:
    void snapToValue();
    bool animate (float elapsed);
    float position = 0.0f;
    float glow = 0.0f;

private:
    float target = 0.0f;
};

class NorthstarLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NorthstarLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    juce::Font getPopupMenuFont() override;

private:
    juce::Image knob, cabinet;
};

class NorthstarFXAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NorthstarFXAudioProcessorEditor (NorthstarFXAudioProcessor&);
    ~NorthstarFXAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawCompassEffects (juce::Graphics&) const;
    void drawSpeaker (juce::Graphics&) const;
    void drawCabinetScene (juce::Graphics&) const;
    float getSceneTarget() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float>) const;
    void choosePreset (const PresetBank::Preset&);
    void showPresetMenu();
    void stepPreset (int direction);
    void savePreset();
    void refreshPresetControls();

    NorthstarFXAudioProcessor& processor;
    NorthstarLookAndFeel look;
    juce::Image skin, wood;
    ShelfScene shelf;
    SpeakerVisual speaker;
    std::array<juce::TextButton, 22> helpButtons;
    juce::Component::SafePointer<juce::CallOutBox> helpPopup;
    std::array<AnimatedKnob, 13> knobs;
    juce::ToggleButton orbit { "12D ORBIT" };
    juce::ToggleButton car { juce::String::fromUTF8 ("ТАЧКА") };
    juce::TextButton presetName, presetPrev, presetNext, compareA, compareB, storePreset;
    juce::ImageButton telegram { "Telegram" };
    juce::ToggleButton levelMatch;
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;
    juce::ComboBox orbitSync;
    juce::TextButton tapeButton { juce::String::fromUTF8 ("СБРОС") };
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::array<std::unique_ptr<SA>, 13> attachments;
    std::unique_ptr<BA> aOrbit, aCar, aLevelMatch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> aOrbitSync;
    double visualAngle = 0.0;
    double lastTick = 0.0;
    float animationClock = 0.0f;
    float sceneIntensity = 0.0f, sceneTime = 0.0f, sceneFrame = 0.0f;
    float paintedSceneIntensity = 0.0f;
    float previousSceneTarget = 0, knobMomentum = 0, knobStrikeCooldown = 0;
    std::uint32_t previousBassImpact = 0;
    bool reducedMotion = false;
    float displayedReduction = 0.0f;
    float displayedTempo = 0.0f;
    float displayedBass = 0, displayedRumble = 0, displayedCompression = 0, displayedTape = 0;
    std::array<float, 3> displayedActivity {};
    float presetFlash = 0.0f;
    float presetUiClock = 0, shownMatchDb = 0;
    std::array<float, 3> sceneStyle {};
    float speakerReveal = 0, speakerHit = 0, coneTravel = 0, coneVelocity = 0;
    float speakerHold = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NorthstarFXAudioProcessorEditor)
};
