#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>

class SeqwencerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::DragAndDropContainer,
                                            private juce::Timer
{
public:
    explicit SeqwencerAudioProcessorEditor (SeqwencerAudioProcessor&);
    ~SeqwencerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class StepGrid;
    class SeqwencerLookAndFeel;
    class FxSelectorButton;
    class ModulationParameterLabel;
    class TargetList;
    class PresetBrowser;

    enum class SelectedFx
    {
        gate,
        phi
    };

    void timerCallback() override;
    void configureKnob (juce::Slider&, const juce::String& suffix = "%");
    void configureStepKnob (juce::Slider&);
    void configureRateKnob (juce::Slider&);
    void configureLabel (juce::Label&, const juce::String& text);
    void handleLaneButton (int lane);
    void handleBipolarButton (int lane);
    void handleRangeControl (juce::Slider&, juce::RangedAudioParameter&);
    void updateRangeControls();
    void updateLaneVisuals();
    void selectFx (SelectedFx);
    void bindSelectedEngine();
    void updateFxPanel();
    void showPresetBrowser();

    SeqwencerAudioProcessor& processor;
    juce::Component content;
    std::unique_ptr<SeqwencerLookAndFeel> lookAndFeel;
    std::unique_ptr<StepGrid> gridA;
    std::unique_ptr<StepGrid> gridB;
    std::unique_ptr<FxSelectorButton> gateFxButton;
    std::unique_ptr<FxSelectorButton> phiFxButton;
    std::unique_ptr<ModulationParameterLabel> baseParameterLabel;
    std::unique_ptr<ModulationParameterLabel> depthParameterLabel;
    std::unique_ptr<ModulationParameterLabel> shortParameterLabel;
    std::unique_ptr<ModulationParameterLabel> longParameterLabel;
    std::unique_ptr<TargetList> targetListA;
    std::unique_ptr<TargetList> targetListB;
    juce::Component::SafePointer<juce::DialogWindow> presetWindow;
    juce::RangedAudioParameter* seqAEnabledParameter = nullptr;
    juce::RangedAudioParameter* seqBEnabledParameter = nullptr;
    juce::RangedAudioParameter* modeParameter = nullptr;
    juce::RangedAudioParameter* serialProfileParameter = nullptr;
    juce::RangedAudioParameter* startStepParameter = nullptr;
    juce::RangedAudioParameter* endStepParameter = nullptr;
    juce::RangedAudioParameter* bipolarAParameter = nullptr;
    juce::RangedAudioParameter* bipolarBParameter = nullptr;

    juce::ComboBox modeBox;
    juce::ComboBox sequenceModeBox;
    juce::ToggleButton syncButton { "HOST SYNC" };
    juce::ToggleButton phiTargetButton { "TARGET" };
    juce::ToggleButton enableAButton { "A" };
    juce::ToggleButton enableBButton { "B" };
    juce::ToggleButton bipolarAButton { "BIPOLAR" };
    juce::ToggleButton bipolarBButton { "BIPOLAR" };
    juce::ToggleButton noiseGateButton { "NOISE GATE" };
    juce::TextButton presetsButton { "PRESETS" };
    juce::Slider startSlider;
    juce::Slider endSlider;
    juce::Slider rateSlider;
    juce::Slider baseSlider;
    juce::Slider depthSlider;
    juce::Slider shortLengthSlider;
    juce::Slider longLengthSlider;
    juce::Slider noiseThresholdSlider;
    juce::Slider noiseAttackSlider;
    juce::Slider noiseHoldSlider;
    juce::Slider noiseReleaseSlider;
    juce::Slider noiseRangeSlider;
    juce::Slider attackASlider;
    juce::Slider releaseASlider;
    juce::Slider attackBSlider;
    juce::Slider releaseBSlider;

    juce::Label rateLabel;
    juce::Label sequenceModeLabel;
    juce::Label startLabel;
    juce::Label endLabel;
    juce::Label noiseThresholdLabel;
    juce::Label noiseAttackLabel;
    juce::Label noiseHoldLabel;
    juce::Label noiseReleaseLabel;
    juce::Label noiseRangeLabel;
    juce::Label attackALabel;
    juce::Label releaseALabel;
    juce::Label attackBLabel;
    juce::Label releaseBLabel;
    juce::Label laneATitle;
    juce::Label laneBTitle;

    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ButtonAttachment> syncAttachment;
    std::unique_ptr<ButtonAttachment> gateAttachment;
    std::unique_ptr<ButtonAttachment> phiBridgeAttachment;
    std::unique_ptr<ButtonAttachment> noiseGateAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<ComboAttachment> sequenceModeAttachment;
    std::unique_ptr<SliderAttachment> rateAttachment;
    std::unique_ptr<SliderAttachment> baseAttachment;
    std::unique_ptr<SliderAttachment> depthAttachment;
    std::unique_ptr<SliderAttachment> shortLengthAttachment;
    std::unique_ptr<SliderAttachment> longLengthAttachment;
    std::unique_ptr<SliderAttachment> noiseThresholdAttachment;
    std::unique_ptr<SliderAttachment> noiseAttackAttachment;
    std::unique_ptr<SliderAttachment> noiseHoldAttachment;
    std::unique_ptr<SliderAttachment> noiseReleaseAttachment;
    std::unique_ptr<SliderAttachment> noiseRangeAttachment;
    std::unique_ptr<SliderAttachment> attackAAttachment;
    std::unique_ptr<SliderAttachment> releaseAAttachment;
    std::unique_ptr<SliderAttachment> attackBAttachment;
    std::unique_ptr<SliderAttachment> releaseBAttachment;
    bool updatingRangeControls = false;
    SelectedFx selectedFx = SelectedFx::gate;
    SelectedFx boundFx = SelectedFx::gate;
    bool bindingsInitialised = false;
    bool lastPhiAvailability = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqwencerAudioProcessorEditor)
};
