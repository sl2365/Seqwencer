#pragma once

#include <JuceHeader.h>

#include "SequencerCore.h"

#include <array>
#include <atomic>

class SeqwencerAudioProcessor final : public juce::AudioProcessor
{
public:
    SeqwencerAudioProcessor();
    ~SeqwencerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return bypassParameter;
    }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getParameterState() noexcept
    {
        return parameters;
    }

    int getActiveStepA (
        seqwencer::SequencerEngine engine = seqwencer::SequencerEngine::gate) const noexcept
    {
        if (seqwencer::isPhiSequencerEngine (engine))
            return phiActiveSteps[static_cast<std::size_t> (
                seqwencer::phiLaneForEngineBank (engine, 0))].load();
        if (engine == seqwencer::SequencerEngine::delay)
            return delayActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::reverb)
            return reverbActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::pan)
            return panActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::filter)
            return filterActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::pitch)
            return pitchActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::distortion)
            return distortionActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::grain)
            return grainActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::compressor)
            return compressorActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::reverse)
            return reverseActiveStepA.load();
        if (engine == seqwencer::SequencerEngine::retrigger)
            return retriggerActiveStepA.load();
        return gateActiveStepA.load();
    }
    int getActiveStepB (
        seqwencer::SequencerEngine engine = seqwencer::SequencerEngine::gate) const noexcept
    {
        if (seqwencer::isPhiSequencerEngine (engine))
            return phiActiveSteps[static_cast<std::size_t> (
                seqwencer::phiLaneForEngineBank (engine, 1))].load();
        if (engine == seqwencer::SequencerEngine::delay)
            return delayActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::reverb)
            return reverbActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::pan)
            return panActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::filter)
            return filterActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::pitch)
            return pitchActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::distortion)
            return distortionActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::grain)
            return grainActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::compressor)
            return compressorActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::reverse)
            return reverseActiveStepB.load();
        if (engine == seqwencer::SequencerEngine::retrigger)
            return retriggerActiveStepB.load();
        return gateActiveStepB.load();
    }
    bool isPhiHostPresent() const noexcept
    {
        return phiHostPresent.load (std::memory_order_acquire);
    }
    void requestPhiTargetBrowser() noexcept
    {
        phiTargetBrowserRequestPending.store (true, std::memory_order_release);
    }
    seqwencer::AudioFxOrder getAudioFxOrder() const noexcept;
    void setAudioFxOrder (const seqwencer::AudioFxOrder& order);
    void moveAudioFxStage (seqwencer::AudioFxStage stage,
                           int destinationIndex);
    seqwencer::StepSubdivisionPattern getStepSubdivisions (
        seqwencer::SequencerEngine engine, int bank) const noexcept;
    void setStepSubdivisions (
        seqwencer::SequencerEngine engine, int bank,
        const seqwencer::StepSubdivisionPattern& subdivisions,
        bool notifyHost = true) noexcept;
    void setStepDivisionMode (
        seqwencer::SequencerEngine engine, int bank, int step,
        seqwencer::StepDivisionMode mode) noexcept;
    void setStepExtraValue (
        seqwencer::SequencerEngine engine, int bank, int step,
        int extraIndex, float canonicalValue) noexcept;

    static juce::String stepParameterID (int bank, int step);
    static juce::String phiPairParameterID (
        int pair, const juce::String& suffix);
    static juce::String phiLaneParameterID (
        int lane, const juce::String& suffix);
    static juce::String phiStepParameterID (int lane, int step);
    static juce::String delayStepParameterID (int bank, int step);
    static juce::String reverbStepParameterID (int bank, int step);
    static juce::String panStepParameterID (int bank, int step);
    static juce::String filterStepParameterID (int bank, int step);
    static juce::String pitchStepParameterID (int bank, int step);
    static juce::String distortionStepParameterID (int bank, int step);
    static juce::String grainStepParameterID (int bank, int step);
    static juce::String compressorStepParameterID (int bank, int step);
    static juce::String reverseStepParameterID (int bank, int step);
    static juce::String retriggerStepParameterID (int bank, int step);
    static juce::String reverseModeParameterID (int bank, int step);
    static juce::String retriggerModeParameterID (int bank, int step);
    static juce::String gateModeParameterID (int bank, int step);
    static juce::String targetAssignedParameterID (
        int bank, seqwencer::ModulationTarget target);
    static juce::String targetEnabledParameterID (
        int bank, seqwencer::ModulationTarget target);
    static juce::String targetDisplayName (seqwencer::ModulationTarget target);
    static juce::File getPortableDataDirectory();
    static juce::File getPortablePresetDirectory();
    static juce::File getPortableSequenceDirectory();
    bool savePortablePreset (const juce::File& file,
                             const juce::String& presetName,
                             juce::String& errorMessage);
    bool loadPortablePreset (const juce::File& file,
                             juce::String& errorMessage);
    bool savePortableSequence (
        const juce::File& file,
        seqwencer::SequencerEngine engine,
        int bank,
        juce::String& errorMessage);
    bool loadPortableSequence (
        const juce::File& file,
        seqwencer::SequencerEngine engine,
        int bank,
        juce::String& errorMessage);
    bool restoreSequenceFromCurrentPreset (
        seqwencer::SequencerEngine engine, int bank,
        bool stepModesOnly, juce::String& errorMessage);
    void resetToInitialPreset();
    juce::String getCurrentPresetName() const;

private:
    struct FilterState
    {
        double integrator1 = 0.0;
        double integrator2 = 0.0;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    seqwencer::Pattern readPattern (
        int bank, seqwencer::ModulationTarget target) const noexcept;
    seqwencer::Pattern readPhiPattern (int lane,
                                       bool bipolar) const noexcept;
    seqwencer::Pattern readDelayPattern (int bank,
                                         bool bipolar) const noexcept;
    seqwencer::Pattern readReverbPattern (int bank,
                                          bool bipolar) const noexcept;
    seqwencer::Pattern readPanPattern (int bank,
                                       bool bipolar) const noexcept;
    seqwencer::Pattern readFilterPattern (int bank,
                                          bool bipolar) const noexcept;
    seqwencer::Pattern readPitchPattern (int bank,
                                         bool bipolar) const noexcept;
    seqwencer::Pattern readDistortionPattern (int bank,
                                              bool bipolar) const noexcept;
    seqwencer::Pattern readGrainPattern (int bank,
                                         bool bipolar) const noexcept;
    seqwencer::Pattern readCompressorPattern (int bank,
                                              bool bipolar) const noexcept;
    seqwencer::Pattern readReversePattern (int bank,
                                           bool bipolar) const noexcept;
    seqwencer::Pattern readRetriggerPattern (int bank,
                                             bool bipolar) const noexcept;
    seqwencer::ReverseStepPattern readReverseModes (
        int bank) const noexcept;
    seqwencer::RetriggerStepPattern readRetriggerModes (
        int bank) const noexcept;
    seqwencer::GateModePattern readGateModes (int bank) const noexcept;
    seqwencer::StepSubdivisionPattern readStepSubdivisions (
        seqwencer::SequencerEngine engine, int bank,
        bool bipolar) const noexcept;
    void resetStepSubdivisions() noexcept;
    void writeStepSubdivisionsToState (juce::ValueTree& state) const;
    void readStepSubdivisionsFromState (const juce::ValueTree& state) noexcept;
    void migrateStepStorageIfNeeded();
    void syncAudioFxOrderFromState();
    void resetFilterProcessor() noexcept;
    void resetReverseProcessor() noexcept;
    void resetRetriggerProcessor() noexcept;
    float processFilterSample (int channel, float input,
                               float cutoffHz, float resonance,
                               int type) noexcept;

    juce::AudioProcessorValueTreeState parameters;

    juce::RangedAudioParameter* bypassParameter = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* syncToHost = nullptr;
    std::atomic<float>* playbackMode = nullptr;
    std::atomic<float>* serialProfile = nullptr;
    std::atomic<float>* startStep = nullptr;
    std::atomic<float>* endStep = nullptr;
    std::atomic<float>* rangeLength = nullptr;
    std::atomic<float>* rangeLink = nullptr;
    std::atomic<float>* rate = nullptr;
    std::atomic<float>* sequenceMode = nullptr;
    std::atomic<float>* gateEnabled = nullptr;
    std::atomic<float>* gateBase = nullptr;
    std::atomic<float>* gateDepth = nullptr;
    std::atomic<float>* shortGateLength = nullptr;
    std::atomic<float>* longGateLength = nullptr;
    std::atomic<float>* noiseGateEnabled = nullptr;
    std::atomic<float>* noiseGateThreshold = nullptr;
    std::atomic<float>* noiseGateAttack = nullptr;
    std::atomic<float>* noiseGateHold = nullptr;
    std::atomic<float>* noiseGateRelease = nullptr;
    std::atomic<float>* noiseGateRange = nullptr;
    std::atomic<float>* phiBridgeEnabled = nullptr;
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiPlaybackModes {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiSerialProfiles {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiStartSteps {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiEndSteps {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiRangeLengths {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiRangeLinks {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiRates {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerPairCount>
        phiSequenceModes {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerLaneCount>
        phiSeqEnabled {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerLaneCount>
        phiSeqBipolar {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerLaneCount>
        phiSeqAttack {};
    std::array<std::atomic<float>*, seqwencer::phiSequencerLaneCount>
        phiSeqRelease {};
    std::atomic<float>* delayEnabled = nullptr;
    std::atomic<float>* delayTime = nullptr;
    std::atomic<float>* delayFeedback = nullptr;
    std::atomic<float>* delayMix = nullptr;
    std::atomic<float>* delayPlaybackMode = nullptr;
    std::atomic<float>* delaySerialProfile = nullptr;
    std::atomic<float>* delayStartStep = nullptr;
    std::atomic<float>* delayEndStep = nullptr;
    std::atomic<float>* delayRangeLength = nullptr;
    std::atomic<float>* delayRangeLink = nullptr;
    std::atomic<float>* delayRate = nullptr;
    std::atomic<float>* delaySequenceMode = nullptr;
    std::atomic<float>* delaySeqAEnabled = nullptr;
    std::atomic<float>* delaySeqABipolar = nullptr;
    std::atomic<float>* delaySeqAAttack = nullptr;
    std::atomic<float>* delaySeqARelease = nullptr;
    std::atomic<float>* delaySeqBEnabled = nullptr;
    std::atomic<float>* delaySeqBBipolar = nullptr;
    std::atomic<float>* delaySeqBAttack = nullptr;
    std::atomic<float>* delaySeqBRelease = nullptr;
    std::atomic<float>* reverbEnabled = nullptr;
    std::atomic<float>* reverbSize = nullptr;
    std::atomic<float>* reverbDamping = nullptr;
    std::atomic<float>* reverbWidth = nullptr;
    std::atomic<float>* reverbMix = nullptr;
    std::atomic<float>* reverbPlaybackMode = nullptr;
    std::atomic<float>* reverbSerialProfile = nullptr;
    std::atomic<float>* reverbStartStep = nullptr;
    std::atomic<float>* reverbEndStep = nullptr;
    std::atomic<float>* reverbRangeLength = nullptr;
    std::atomic<float>* reverbRangeLink = nullptr;
    std::atomic<float>* reverbRate = nullptr;
    std::atomic<float>* reverbSequenceMode = nullptr;
    std::atomic<float>* reverbSeqAEnabled = nullptr;
    std::atomic<float>* reverbSeqABipolar = nullptr;
    std::atomic<float>* reverbSeqAAttack = nullptr;
    std::atomic<float>* reverbSeqARelease = nullptr;
    std::atomic<float>* reverbSeqBEnabled = nullptr;
    std::atomic<float>* reverbSeqBBipolar = nullptr;
    std::atomic<float>* reverbSeqBAttack = nullptr;
    std::atomic<float>* reverbSeqBRelease = nullptr;
    std::atomic<float>* panEnabled = nullptr;
    std::atomic<float>* panPosition = nullptr;
    std::atomic<float>* panPlaybackMode = nullptr;
    std::atomic<float>* panSerialProfile = nullptr;
    std::atomic<float>* panStartStep = nullptr;
    std::atomic<float>* panEndStep = nullptr;
    std::atomic<float>* panRangeLength = nullptr;
    std::atomic<float>* panRangeLink = nullptr;
    std::atomic<float>* panRate = nullptr;
    std::atomic<float>* panSequenceMode = nullptr;
    std::atomic<float>* panSeqAEnabled = nullptr;
    std::atomic<float>* panSeqABipolar = nullptr;
    std::atomic<float>* panSeqAAttack = nullptr;
    std::atomic<float>* panSeqARelease = nullptr;
    std::atomic<float>* panSeqBEnabled = nullptr;
    std::atomic<float>* panSeqBBipolar = nullptr;
    std::atomic<float>* panSeqBAttack = nullptr;
    std::atomic<float>* panSeqBRelease = nullptr;
    std::atomic<float>* filterEnabled = nullptr;
    std::atomic<float>* filterType = nullptr;
    std::atomic<float>* filterCutoff = nullptr;
    std::atomic<float>* filterResonance = nullptr;
    std::atomic<float>* filterMix = nullptr;
    std::atomic<float>* filterPlaybackMode = nullptr;
    std::atomic<float>* filterSerialProfile = nullptr;
    std::atomic<float>* filterStartStep = nullptr;
    std::atomic<float>* filterEndStep = nullptr;
    std::atomic<float>* filterRangeLength = nullptr;
    std::atomic<float>* filterRangeLink = nullptr;
    std::atomic<float>* filterRate = nullptr;
    std::atomic<float>* filterSequenceMode = nullptr;
    std::atomic<float>* filterSeqAEnabled = nullptr;
    std::atomic<float>* filterSeqABipolar = nullptr;
    std::atomic<float>* filterSeqAAttack = nullptr;
    std::atomic<float>* filterSeqARelease = nullptr;
    std::atomic<float>* filterSeqBEnabled = nullptr;
    std::atomic<float>* filterSeqBBipolar = nullptr;
    std::atomic<float>* filterSeqBAttack = nullptr;
    std::atomic<float>* filterSeqBRelease = nullptr;
    std::atomic<float>* pitchEnabled = nullptr;
    std::atomic<float>* pitchShift = nullptr;
    std::atomic<float>* pitchMix = nullptr;
    std::atomic<float>* pitchPlaybackMode = nullptr;
    std::atomic<float>* pitchSerialProfile = nullptr;
    std::atomic<float>* pitchStartStep = nullptr;
    std::atomic<float>* pitchEndStep = nullptr;
    std::atomic<float>* pitchRangeLength = nullptr;
    std::atomic<float>* pitchRangeLink = nullptr;
    std::atomic<float>* pitchRate = nullptr;
    std::atomic<float>* pitchSequenceMode = nullptr;
    std::atomic<float>* pitchSeqAEnabled = nullptr;
    std::atomic<float>* pitchSeqABipolar = nullptr;
    std::atomic<float>* pitchSeqAAttack = nullptr;
    std::atomic<float>* pitchSeqARelease = nullptr;
    std::atomic<float>* pitchSeqBEnabled = nullptr;
    std::atomic<float>* pitchSeqBBipolar = nullptr;
    std::atomic<float>* pitchSeqBAttack = nullptr;
    std::atomic<float>* pitchSeqBRelease = nullptr;
    std::atomic<float>* distortionEnabled = nullptr;
    std::atomic<float>* distortionType = nullptr;
    std::atomic<float>* distortionDrive = nullptr;
    std::atomic<float>* distortionTone = nullptr;
    std::atomic<float>* distortionMix = nullptr;
    std::atomic<float>* distortionPlaybackMode = nullptr;
    std::atomic<float>* distortionSerialProfile = nullptr;
    std::atomic<float>* distortionStartStep = nullptr;
    std::atomic<float>* distortionEndStep = nullptr;
    std::atomic<float>* distortionRangeLength = nullptr;
    std::atomic<float>* distortionRangeLink = nullptr;
    std::atomic<float>* distortionRate = nullptr;
    std::atomic<float>* distortionSequenceMode = nullptr;
    std::atomic<float>* distortionSeqAEnabled = nullptr;
    std::atomic<float>* distortionSeqABipolar = nullptr;
    std::atomic<float>* distortionSeqAAttack = nullptr;
    std::atomic<float>* distortionSeqARelease = nullptr;
    std::atomic<float>* distortionSeqBEnabled = nullptr;
    std::atomic<float>* distortionSeqBBipolar = nullptr;
    std::atomic<float>* distortionSeqBAttack = nullptr;
    std::atomic<float>* distortionSeqBRelease = nullptr;
    std::atomic<float>* grainEnabled = nullptr;
    std::atomic<float>* grainSize = nullptr;
    std::atomic<float>* grainShift = nullptr;
    std::atomic<float>* grainFeedback = nullptr;
    std::atomic<float>* grainMix = nullptr;
    std::atomic<float>* grainPlaybackMode = nullptr;
    std::atomic<float>* grainSerialProfile = nullptr;
    std::atomic<float>* grainStartStep = nullptr;
    std::atomic<float>* grainEndStep = nullptr;
    std::atomic<float>* grainRangeLength = nullptr;
    std::atomic<float>* grainRangeLink = nullptr;
    std::atomic<float>* grainRate = nullptr;
    std::atomic<float>* grainSequenceMode = nullptr;
    std::atomic<float>* grainSeqAEnabled = nullptr;
    std::atomic<float>* grainSeqABipolar = nullptr;
    std::atomic<float>* grainSeqAAttack = nullptr;
    std::atomic<float>* grainSeqARelease = nullptr;
    std::atomic<float>* grainSeqBEnabled = nullptr;
    std::atomic<float>* grainSeqBBipolar = nullptr;
    std::atomic<float>* grainSeqBAttack = nullptr;
    std::atomic<float>* grainSeqBRelease = nullptr;
    std::atomic<float>* compressorEnabled = nullptr;
    std::atomic<float>* compressorThreshold = nullptr;
    std::atomic<float>* compressorRatio = nullptr;
    std::atomic<float>* compressorAttack = nullptr;
    std::atomic<float>* compressorRelease = nullptr;
    std::atomic<float>* compressorMakeup = nullptr;
    std::atomic<float>* compressorMix = nullptr;
    std::atomic<float>* compressorPlaybackMode = nullptr;
    std::atomic<float>* compressorSerialProfile = nullptr;
    std::atomic<float>* compressorStartStep = nullptr;
    std::atomic<float>* compressorEndStep = nullptr;
    std::atomic<float>* compressorRangeLength = nullptr;
    std::atomic<float>* compressorRangeLink = nullptr;
    std::atomic<float>* compressorRate = nullptr;
    std::atomic<float>* compressorSequenceMode = nullptr;
    std::atomic<float>* compressorSeqAEnabled = nullptr;
    std::atomic<float>* compressorSeqABipolar = nullptr;
    std::atomic<float>* compressorSeqAAttack = nullptr;
    std::atomic<float>* compressorSeqARelease = nullptr;
    std::atomic<float>* compressorSeqBEnabled = nullptr;
    std::atomic<float>* compressorSeqBBipolar = nullptr;
    std::atomic<float>* compressorSeqBAttack = nullptr;
    std::atomic<float>* compressorSeqBRelease = nullptr;
    std::atomic<float>* reverseEnabled = nullptr;
    std::atomic<float>* reverseTime = nullptr;
    std::atomic<float>* reversePointA = nullptr;
    std::atomic<float>* reversePointB = nullptr;
    std::atomic<float>* reverseMix = nullptr;
    std::atomic<float>* reversePlaybackMode = nullptr;
    std::atomic<float>* reverseSerialProfile = nullptr;
    std::atomic<float>* reverseStartStep = nullptr;
    std::atomic<float>* reverseEndStep = nullptr;
    std::atomic<float>* reverseRangeLength = nullptr;
    std::atomic<float>* reverseRangeLink = nullptr;
    std::atomic<float>* reverseRate = nullptr;
    std::atomic<float>* reverseSequenceMode = nullptr;
    std::atomic<float>* reverseSeqAEnabled = nullptr;
    std::atomic<float>* reverseSeqABipolar = nullptr;
    std::atomic<float>* reverseSeqAAttack = nullptr;
    std::atomic<float>* reverseSeqARelease = nullptr;
    std::atomic<float>* reverseSeqBEnabled = nullptr;
    std::atomic<float>* reverseSeqBBipolar = nullptr;
    std::atomic<float>* reverseSeqBAttack = nullptr;
    std::atomic<float>* reverseSeqBRelease = nullptr;
    std::atomic<float>* retriggerEnabled = nullptr;
    std::atomic<float>* retriggerInitialSpeed = nullptr;
    std::atomic<float>* retriggerFinalSpeed = nullptr;
    std::atomic<float>* retriggerTransition = nullptr;
    std::atomic<float>* retriggerDecay = nullptr;
    std::atomic<float>* retriggerMix = nullptr;
    std::atomic<float>* retriggerPlaybackMode = nullptr;
    std::atomic<float>* retriggerSerialProfile = nullptr;
    std::atomic<float>* retriggerStartStep = nullptr;
    std::atomic<float>* retriggerEndStep = nullptr;
    std::atomic<float>* retriggerRangeLength = nullptr;
    std::atomic<float>* retriggerRangeLink = nullptr;
    std::atomic<float>* retriggerRate = nullptr;
    std::atomic<float>* retriggerSequenceMode = nullptr;
    std::atomic<float>* retriggerSeqAEnabled = nullptr;
    std::atomic<float>* retriggerSeqABipolar = nullptr;
    std::atomic<float>* retriggerSeqAAttack = nullptr;
    std::atomic<float>* retriggerSeqARelease = nullptr;
    std::atomic<float>* retriggerSeqBEnabled = nullptr;
    std::atomic<float>* retriggerSeqBBipolar = nullptr;
    std::atomic<float>* retriggerSeqBAttack = nullptr;
    std::atomic<float>* retriggerSeqBRelease = nullptr;
    std::atomic<float>* seqAEnabled = nullptr;
    std::atomic<float>* seqATarget = nullptr;
    std::atomic<float>* seqATargetEnabled = nullptr;
    std::atomic<float>* seqABipolar = nullptr;
    std::atomic<float>* seqAAttack = nullptr;
    std::atomic<float>* seqARelease = nullptr;
    std::atomic<float>* seqBEnabled = nullptr;
    std::atomic<float>* seqBTarget = nullptr;
    std::atomic<float>* seqBTargetEnabled = nullptr;
    std::atomic<float>* seqBBipolar = nullptr;
    std::atomic<float>* seqBAttack = nullptr;
    std::atomic<float>* seqBRelease = nullptr;
    std::array<std::atomic<float>*, seqwencer::modulationTargetCount>
        targetsAssignedA {};
    std::array<std::atomic<float>*, seqwencer::modulationTargetCount>
        targetsEnabledA {};
    std::array<std::atomic<float>*, seqwencer::modulationTargetCount>
        targetsAssignedB {};
    std::array<std::atomic<float>*, seqwencer::modulationTargetCount>
        targetsEnabledB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> stepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> stepsB {};
    std::array<std::array<std::atomic<float>*, seqwencer::stepsPerBank>,
               seqwencer::phiSequencerLaneCount> phiSteps {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> delayStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> delayStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverbStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverbStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> panStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> panStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> filterStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> filterStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> pitchStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> pitchStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> distortionStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> distortionStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> grainStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> grainStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> compressorStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> compressorStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverseStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverseStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverseModesA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> reverseModesB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> retriggerStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> retriggerStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> retriggerModesA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> retriggerModesB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesB {};
    struct StoredStepSubdivision
    {
        std::atomic<int> mode {
            static_cast<int> (seqwencer::StepDivisionMode::normal) };
        std::array<std::atomic<float>,
                   seqwencer::maximumSegmentsPerStep - 1> extraValues;

        StoredStepSubdivision() noexcept
        {
            for (auto& value : extraValues)
                value.store (1.0f, std::memory_order_relaxed);
        }
    };
    std::array<std::array<std::array<StoredStepSubdivision,
                                     seqwencer::stepsPerBank>, 2>,
               seqwencer::sequencerEngineCount> stepSubdivisions;
    std::array<std::atomic<int>, seqwencer::audioFxStageCount> audioFxOrder {};

    double currentSampleRate = 44100.0;
    double gateFreeRunningPhase = 0.0;
    std::array<double, seqwencer::phiSequencerPairCount>
        phiFreeRunningPhases {};
    double delayFreeRunningPhase = 0.0;
    double reverbFreeRunningPhase = 0.0;
    double panFreeRunningPhase = 0.0;
    double filterFreeRunningPhase = 0.0;
    double pitchFreeRunningPhase = 0.0;
    double distortionFreeRunningPhase = 0.0;
    double grainFreeRunningPhase = 0.0;
    double compressorFreeRunningPhase = 0.0;
    double reverseFreeRunningPhase = 0.0;
    double retriggerFreeRunningPhase = 0.0;
    double previousHostPpq = 0.0;
    juce::int64 previousHostTimeInSamples = 0;
    bool previousHostPpqValid = false;
    bool previousHostTimeInSamplesValid = false;
    float smoothedGain = 1.0f;
    float smoothedNoiseGateGain = 1.0f;
    int noiseGateHoldSamplesRemaining = 0;
    bool noiseGateIsOpen = true;
    seqwencer::NotePhraseTracker notePhraseTracker;
    std::atomic<int> gateActiveStepA { 0 };
    std::atomic<int> gateActiveStepB { -1 };
    std::array<std::atomic<int>, seqwencer::phiSequencerLaneCount>
        phiActiveSteps {};
    std::atomic<int> delayActiveStepA { 0 };
    std::atomic<int> delayActiveStepB { -1 };
    std::atomic<int> reverbActiveStepA { 0 };
    std::atomic<int> reverbActiveStepB { -1 };
    std::atomic<int> panActiveStepA { 0 };
    std::atomic<int> panActiveStepB { -1 };
    std::atomic<int> filterActiveStepA { 0 };
    std::atomic<int> filterActiveStepB { -1 };
    std::atomic<int> pitchActiveStepA { 0 };
    std::atomic<int> pitchActiveStepB { -1 };
    std::atomic<int> distortionActiveStepA { 0 };
    std::atomic<int> distortionActiveStepB { -1 };
    std::atomic<int> grainActiveStepA { 0 };
    std::atomic<int> grainActiveStepB { -1 };
    std::atomic<int> compressorActiveStepA { 0 };
    std::atomic<int> compressorActiveStepB { -1 };
    std::atomic<int> reverseActiveStepA { 0 };
    std::atomic<int> reverseActiveStepB { -1 };
    std::atomic<int> retriggerActiveStepA { 0 };
    std::atomic<int> retriggerActiveStepB { -1 };
    juce::AudioBuffer<float> delayBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        smoothedDelaySamples;
    int delayWritePosition = 0;
    bool delayWasActive = false;
    juce::Reverb reverbProcessor;
    bool reverbWasActive = false;
    std::array<FilterState, 2> filterStates {};
    juce::AudioBuffer<float> filterCombBuffer;
    std::array<int, 2> filterCombWritePositions {};
    int filterPreviousType = -1;
    bool filterWasActive = false;
    juce::AudioBuffer<float> pitchBuffer;
    int pitchWritePosition = 0;
    double pitchReadPhase = 0.0;
    bool pitchWasActive = false;
    std::array<float, 2> distortionToneStates {};
    bool distortionWasActive = false;
    juce::AudioBuffer<float> grainBuffer;
    int grainWritePosition = 0;
    double grainReadPhase = 0.0;
    std::array<float, 2> grainFeedbackStates {};
    bool grainWasActive = false;
    float compressorGain = 1.0f;
    bool compressorWasActive = false;
    std::array<juce::AudioBuffer<float>, 2> reverseBuffers;
    int reverseCaptureBufferIndex = 0;
    int reversePlaybackBufferIndex = 1;
    int reverseCaptureWritePosition = 0;
    int reverseCaptureAvailableSamples = 0;
    int reverseCapturedSamples = 0;
    int reversePendingCaptureSamples = 0;
    int reversePlaybackStartPosition = 0;
    double reverseReadPosition = 0.0;
    double reversePreviousSequencePhase = -1.0;
    int reverseReadDirection = 1;
    float reverseWetFade = 0.0f;
    bool reverseIsPlaying = false;
    bool reverseStepWasOn = false;
    bool reverseWasActive = false;
    juce::AudioBuffer<float> retriggerBuffer;
    int retriggerCaptureSamples = 0;
    int retriggerCaptureTargetSamples = 0;
    int retriggerReadPosition = 0;
    int retriggerSamplesUntilRepeat = 0;
    juce::int64 retriggerPlaybackSamples = 0;
    int retriggerRepeatIndex = 0;
    float retriggerWetFade = 0.0f;
    bool retriggerIsPlaying = false;
    bool retriggerStepWasOn = false;
    bool retriggerBlockIsActive = false;
    bool retriggerWasActive = false;
    std::atomic<bool> phiHostPresent { false };
    std::atomic<bool> phiTargetBrowserRequestPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqwencerAudioProcessor)
};
