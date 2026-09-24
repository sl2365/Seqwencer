#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>

class SeqwencerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                            public juce::DragAndDropContainer,
                                            public juce::DragAndDropTarget,
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
    class PatternNudgeControls;
    class RangeLinkButton;
    class ModulationParameterLabel;
    class TargetList;
    class PresetBrowser;
    class SequenceComboBox;

    enum class SelectedFx
    {
        gate,
        delay,
        reverb,
        pan,
        filter,
        pitch,
        distortion,
        grain,
        compressor,
        reverse,
        phi,
        retrigger
    };

    void timerCallback() override;
    void configureKnob (juce::Slider&, const juce::String& suffix = "%");
    void configureColourKnob (juce::Slider&);
    void configureStepKnob (juce::Slider&);
    void configureRateKnob (juce::Slider&);
    void configureLabel (juce::Label&, const juce::String& text);
    void handleLaneButton (int lane);
    void handleBipolarButton (int lane);
    void handleRangeControl (bool movingStart);
    void handleRangeLinkButton();
    void updateRangeControls();
    void updateLaneVisuals();
    void selectFx (SelectedFx);
    void selectPhiPair (int pair);
    void bindSelectedEngine();
    void updateFxPanel();
    void updateLaneColours();
    void applyWaveformPreset (int bank);
    void refreshUserSequenceMenus();
    bool populateUserSequenceMenu (
        juce::PopupMenu& menu,
        const juce::File& directory,
        juce::StringArray& visitedDirectories,
        int depth);
    void loadUserSequence (int bank);
    void showPresetBrowser();
    bool isInterestedInDragSource (
        const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragMove (
        const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDragExit (
        const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDropped (
        const juce::DragAndDropTarget::SourceDetails&) override;
    void updateFxSelectorBounds();
    FxSelectorButton* buttonForAudioFxStage (
        seqwencer::AudioFxStage) const noexcept;

    SeqwencerAudioProcessor& processor;
    juce::Component content;
    std::unique_ptr<SeqwencerLookAndFeel> lookAndFeel;
    std::unique_ptr<StepGrid> gridA;
    std::unique_ptr<StepGrid> gridB;
    std::unique_ptr<PatternNudgeControls> nudgeControlsA;
    std::unique_ptr<PatternNudgeControls> nudgeControlsB;
    std::unique_ptr<RangeLinkButton> rangeLinkButton;
    std::unique_ptr<FxSelectorButton> gateFxButton;
    std::unique_ptr<FxSelectorButton> delayFxButton;
    std::unique_ptr<FxSelectorButton> reverbFxButton;
    std::unique_ptr<FxSelectorButton> panFxButton;
    std::unique_ptr<FxSelectorButton> filterFxButton;
    std::unique_ptr<FxSelectorButton> pitchFxButton;
    std::unique_ptr<FxSelectorButton> distortionFxButton;
    std::unique_ptr<FxSelectorButton> grainFxButton;
    std::unique_ptr<FxSelectorButton> compressorFxButton;
    std::unique_ptr<FxSelectorButton> reverseFxButton;
    std::unique_ptr<FxSelectorButton> retriggerFxButton;
    std::unique_ptr<FxSelectorButton> phiFxButton;
    std::unique_ptr<ModulationParameterLabel> baseParameterLabel;
    std::unique_ptr<ModulationParameterLabel> depthParameterLabel;
    std::unique_ptr<ModulationParameterLabel> shortParameterLabel;
    std::unique_ptr<ModulationParameterLabel> longParameterLabel;
    std::unique_ptr<ModulationParameterLabel> noiseThresholdParameterLabel;
    std::unique_ptr<ModulationParameterLabel> noiseAttackParameterLabel;
    std::unique_ptr<ModulationParameterLabel> noiseHoldParameterLabel;
    std::unique_ptr<ModulationParameterLabel> noiseReleaseParameterLabel;
    std::unique_ptr<ModulationParameterLabel> noiseRangeParameterLabel;
    std::unique_ptr<ModulationParameterLabel> delayTimeParameterLabel;
    std::unique_ptr<ModulationParameterLabel> delayFeedbackParameterLabel;
    std::unique_ptr<ModulationParameterLabel> delayMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverbSizeParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverbDampingParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverbWidthParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverbMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> attackAParameterLabel;
    std::unique_ptr<ModulationParameterLabel> releaseAParameterLabel;
    std::unique_ptr<ModulationParameterLabel> attackBParameterLabel;
    std::unique_ptr<ModulationParameterLabel> releaseBParameterLabel;
    std::unique_ptr<ModulationParameterLabel> panPositionParameterLabel;
    std::unique_ptr<ModulationParameterLabel> filterCutoffParameterLabel;
    std::unique_ptr<ModulationParameterLabel> filterResonanceParameterLabel;
    std::unique_ptr<ModulationParameterLabel> filterMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> pitchShiftParameterLabel;
    std::unique_ptr<ModulationParameterLabel> pitchMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> distortionDriveParameterLabel;
    std::unique_ptr<ModulationParameterLabel> distortionToneParameterLabel;
    std::unique_ptr<ModulationParameterLabel> distortionMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> grainSizeParameterLabel;
    std::unique_ptr<ModulationParameterLabel> grainShiftParameterLabel;
    std::unique_ptr<ModulationParameterLabel> grainFeedbackParameterLabel;
    std::unique_ptr<ModulationParameterLabel> grainMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorThresholdParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorRatioParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorAttackParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorReleaseParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorMakeupParameterLabel;
    std::unique_ptr<ModulationParameterLabel> compressorMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverseTimeParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reversePointAParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reversePointBParameterLabel;
    std::unique_ptr<ModulationParameterLabel> reverseMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> retriggerInitialParameterLabel;
    std::unique_ptr<ModulationParameterLabel> retriggerFinalParameterLabel;
    std::unique_ptr<ModulationParameterLabel> retriggerTransitionParameterLabel;
    std::unique_ptr<ModulationParameterLabel> retriggerDecayParameterLabel;
    std::unique_ptr<ModulationParameterLabel> retriggerMixParameterLabel;
    std::unique_ptr<ModulationParameterLabel> startParameterLabel;
    std::unique_ptr<ModulationParameterLabel> endParameterLabel;
    std::unique_ptr<TargetList> targetListA;
    std::unique_ptr<TargetList> targetListB;
    std::unique_ptr<TargetList> delayTargetListA;
    std::unique_ptr<TargetList> delayTargetListB;
    std::unique_ptr<TargetList> reverbTargetListA;
    std::unique_ptr<TargetList> reverbTargetListB;
    std::unique_ptr<TargetList> panTargetListA;
    std::unique_ptr<TargetList> panTargetListB;
    std::unique_ptr<TargetList> filterTargetListA;
    std::unique_ptr<TargetList> filterTargetListB;
    std::unique_ptr<TargetList> pitchTargetListA;
    std::unique_ptr<TargetList> pitchTargetListB;
    std::unique_ptr<TargetList> distortionTargetListA;
    std::unique_ptr<TargetList> distortionTargetListB;
    std::unique_ptr<TargetList> grainTargetListA;
    std::unique_ptr<TargetList> grainTargetListB;
    std::unique_ptr<TargetList> compressorTargetListA;
    std::unique_ptr<TargetList> compressorTargetListB;
    std::unique_ptr<TargetList> reverseTargetListA;
    std::unique_ptr<TargetList> reverseTargetListB;
    std::unique_ptr<TargetList> retriggerTargetListA;
    std::unique_ptr<TargetList> retriggerTargetListB;
    juce::Component::SafePointer<juce::DialogWindow> presetWindow;
    juce::RangedAudioParameter* seqAEnabledParameter = nullptr;
    juce::RangedAudioParameter* seqBEnabledParameter = nullptr;
    juce::RangedAudioParameter* modeParameter = nullptr;
    juce::RangedAudioParameter* serialProfileParameter = nullptr;
    juce::RangedAudioParameter* startStepParameter = nullptr;
    juce::RangedAudioParameter* endStepParameter = nullptr;
    juce::RangedAudioParameter* rangeLengthParameter = nullptr;
    juce::RangedAudioParameter* rangeLinkParameter = nullptr;
    juce::RangedAudioParameter* bipolarAParameter = nullptr;
    juce::RangedAudioParameter* bipolarBParameter = nullptr;

    juce::ComboBox modeBox;
    juce::ComboBox sequenceModeBox;
    juce::ComboBox filterTypeBox;
    juce::ComboBox distortionTypeBox;
    juce::ComboBox waveformABox;
    juce::ComboBox waveformBBox;
    std::unique_ptr<SequenceComboBox> userSequenceABox;
    std::unique_ptr<SequenceComboBox> userSequenceBBox;
    juce::Array<juce::File> userSequenceFiles;
    juce::ToggleButton syncButton { "HOST SYNC" };
    juce::ToggleButton phiTargetButton { "TARGETS" };
    juce::ToggleButton phiPairABButton { "A/B" };
    juce::ToggleButton phiPairCDButton { "C/D" };
    juce::ToggleButton phiPairEFButton { "E/F" };
    juce::ToggleButton phiPairGHButton { "G/H" };
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
    juce::Slider delayTimeSlider;
    juce::Slider delayFeedbackSlider;
    juce::Slider delayMixSlider;
    juce::Slider reverbSizeSlider;
    juce::Slider reverbDampingSlider;
    juce::Slider reverbWidthSlider;
    juce::Slider reverbMixSlider;
    juce::Slider panPositionSlider;
    juce::Slider filterCutoffSlider;
    juce::Slider filterResonanceSlider;
    juce::Slider filterMixSlider;
    juce::Slider pitchShiftSlider;
    juce::Slider pitchMixSlider;
    juce::Slider distortionDriveSlider;
    juce::Slider distortionToneSlider;
    juce::Slider distortionMixSlider;
    juce::Slider grainSizeSlider;
    juce::Slider grainShiftSlider;
    juce::Slider grainFeedbackSlider;
    juce::Slider grainMixSlider;
    juce::Slider compressorThresholdSlider;
    juce::Slider compressorRatioSlider;
    juce::Slider compressorAttackSlider;
    juce::Slider compressorReleaseSlider;
    juce::Slider compressorMakeupSlider;
    juce::Slider compressorMixSlider;
    juce::Slider reverseTimeSlider;
    juce::Slider reversePointASlider;
    juce::Slider reversePointBSlider;
    juce::Slider reverseMixSlider;
    juce::Slider retriggerInitialSlider;
    juce::Slider retriggerFinalSlider;
    juce::Slider retriggerTransitionSlider;
    juce::Slider retriggerDecaySlider;
    juce::Slider retriggerMixSlider;
    juce::Slider attackASlider;
    juce::Slider releaseASlider;
    juce::Slider attackBSlider;
    juce::Slider releaseBSlider;
    juce::Slider colourASlider;
    juce::Slider colourBSlider;

    juce::Label rateLabel;
    juce::Label sequenceModeLabel;
    juce::Label laneATitle;
    juce::Label laneBTitle;
    juce::Label colourALabel;
    juce::Label colourBLabel;
    juce::Label filterTypeLabel;
    juce::Label distortionTypeLabel;

    using ButtonAttachment =
        juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment =
        juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment =
        juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ButtonAttachment> syncAttachment;
    std::unique_ptr<ButtonAttachment> gateAttachment;
    std::unique_ptr<ButtonAttachment> delayAttachment;
    std::unique_ptr<ButtonAttachment> reverbAttachment;
    std::unique_ptr<ButtonAttachment> panAttachment;
    std::unique_ptr<ButtonAttachment> filterAttachment;
    std::unique_ptr<ButtonAttachment> pitchAttachment;
    std::unique_ptr<ButtonAttachment> distortionAttachment;
    std::unique_ptr<ButtonAttachment> grainAttachment;
    std::unique_ptr<ButtonAttachment> compressorAttachment;
    std::unique_ptr<ButtonAttachment> reverseAttachment;
    std::unique_ptr<ButtonAttachment> retriggerAttachment;
    std::unique_ptr<ButtonAttachment> phiBridgeAttachment;
    std::unique_ptr<ButtonAttachment> noiseGateAttachment;
    std::unique_ptr<ButtonAttachment> rangeLinkAttachment;
    std::unique_ptr<ComboAttachment> modeAttachment;
    std::unique_ptr<ComboAttachment> sequenceModeAttachment;
    std::unique_ptr<ComboAttachment> filterTypeAttachment;
    std::unique_ptr<ComboAttachment> distortionTypeAttachment;
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
    std::unique_ptr<SliderAttachment> delayTimeAttachment;
    std::unique_ptr<SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<SliderAttachment> delayMixAttachment;
    std::unique_ptr<SliderAttachment> reverbSizeAttachment;
    std::unique_ptr<SliderAttachment> reverbDampingAttachment;
    std::unique_ptr<SliderAttachment> reverbWidthAttachment;
    std::unique_ptr<SliderAttachment> reverbMixAttachment;
    std::unique_ptr<SliderAttachment> panPositionAttachment;
    std::unique_ptr<SliderAttachment> filterCutoffAttachment;
    std::unique_ptr<SliderAttachment> filterResonanceAttachment;
    std::unique_ptr<SliderAttachment> filterMixAttachment;
    std::unique_ptr<SliderAttachment> pitchShiftAttachment;
    std::unique_ptr<SliderAttachment> pitchMixAttachment;
    std::unique_ptr<SliderAttachment> distortionDriveAttachment;
    std::unique_ptr<SliderAttachment> distortionToneAttachment;
    std::unique_ptr<SliderAttachment> distortionMixAttachment;
    std::unique_ptr<SliderAttachment> grainSizeAttachment;
    std::unique_ptr<SliderAttachment> grainShiftAttachment;
    std::unique_ptr<SliderAttachment> grainFeedbackAttachment;
    std::unique_ptr<SliderAttachment> grainMixAttachment;
    std::unique_ptr<SliderAttachment> compressorThresholdAttachment;
    std::unique_ptr<SliderAttachment> compressorRatioAttachment;
    std::unique_ptr<SliderAttachment> compressorAttackAttachment;
    std::unique_ptr<SliderAttachment> compressorReleaseAttachment;
    std::unique_ptr<SliderAttachment> compressorMakeupAttachment;
    std::unique_ptr<SliderAttachment> compressorMixAttachment;
    std::unique_ptr<SliderAttachment> reverseTimeAttachment;
    std::unique_ptr<SliderAttachment> reversePointAAttachment;
    std::unique_ptr<SliderAttachment> reversePointBAttachment;
    std::unique_ptr<SliderAttachment> reverseMixAttachment;
    std::unique_ptr<SliderAttachment> retriggerInitialAttachment;
    std::unique_ptr<SliderAttachment> retriggerFinalAttachment;
    std::unique_ptr<SliderAttachment> retriggerTransitionAttachment;
    std::unique_ptr<SliderAttachment> retriggerDecayAttachment;
    std::unique_ptr<SliderAttachment> retriggerMixAttachment;
    std::unique_ptr<SliderAttachment> attackAAttachment;
    std::unique_ptr<SliderAttachment> releaseAAttachment;
    std::unique_ptr<SliderAttachment> attackBAttachment;
    std::unique_ptr<SliderAttachment> releaseBAttachment;
    std::array<seqwencer::ModulationTarget,
               seqwencer::sequencerRangeTargetCount> boundRangeTargets {
        seqwencer::ModulationTarget::none,
        seqwencer::ModulationTarget::none,
        seqwencer::ModulationTarget::none
    };
    bool updatingRangeControls = false;
    bool rangeLabelShowsLength = false;
    SelectedFx selectedFx = SelectedFx::gate;
    SelectedFx boundFx = SelectedFx::gate;
    int selectedPhiPair = 0;
    int boundPhiPair = -1;
    seqwencer::AudioFxOrder displayedAudioFxOrder =
        seqwencer::defaultAudioFxOrder();
    int fxDropIndex = -1;
    bool bindingsInitialised = false;
    bool lastPhiAvailability = false;
    juce::Colour laneAColour { 0xff34d6c6 };
    juce::Colour laneBColour { 0xffff9d4d };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqwencerAudioProcessorEditor)
};
