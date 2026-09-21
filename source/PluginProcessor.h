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
        if (engine == seqwencer::SequencerEngine::phi)
            return phiActiveStepA.load();
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
        return gateActiveStepA.load();
    }
    int getActiveStepB (
        seqwencer::SequencerEngine engine = seqwencer::SequencerEngine::gate) const noexcept
    {
        if (engine == seqwencer::SequencerEngine::phi)
            return phiActiveStepB.load();
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

    static juce::String stepParameterID (int bank, int step);
    static juce::String phiStepParameterID (int bank, int step);
    static juce::String delayStepParameterID (int bank, int step);
    static juce::String reverbStepParameterID (int bank, int step);
    static juce::String panStepParameterID (int bank, int step);
    static juce::String filterStepParameterID (int bank, int step);
    static juce::String pitchStepParameterID (int bank, int step);
    static juce::String distortionStepParameterID (int bank, int step);
    static juce::String grainStepParameterID (int bank, int step);
    static juce::String compressorStepParameterID (int bank, int step);
    static juce::String gateModeParameterID (int bank, int step);
    static juce::String targetAssignedParameterID (
        int bank, seqwencer::ModulationTarget target);
    static juce::String targetEnabledParameterID (
        int bank, seqwencer::ModulationTarget target);
    static juce::String targetDisplayName (seqwencer::ModulationTarget target);
    static juce::File getPortableDataDirectory();
    static juce::File getPortablePresetDirectory();
    bool savePortablePreset (const juce::File& file,
                             const juce::String& presetName,
                             juce::String& errorMessage);
    bool loadPortablePreset (const juce::File& file,
                             juce::String& errorMessage);
    bool restoreSequenceFromCurrentPreset (
        seqwencer::SequencerEngine engine, int bank,
        bool gateModesOnly, juce::String& errorMessage);
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
    seqwencer::Pattern readPhiPattern (int bank,
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
    seqwencer::GateModePattern readGateModes (int bank) const noexcept;
    void migrateStepStorageIfNeeded();
    void syncAudioFxOrderFromState();
    void resetFilterProcessor() noexcept;
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
    std::atomic<float>* phiPlaybackMode = nullptr;
    std::atomic<float>* phiSerialProfile = nullptr;
    std::atomic<float>* phiStartStep = nullptr;
    std::atomic<float>* phiEndStep = nullptr;
    std::atomic<float>* phiRangeLength = nullptr;
    std::atomic<float>* phiRangeLink = nullptr;
    std::atomic<float>* phiRate = nullptr;
    std::atomic<float>* phiSequenceMode = nullptr;
    std::atomic<float>* phiSeqAEnabled = nullptr;
    std::atomic<float>* phiSeqABipolar = nullptr;
    std::atomic<float>* phiSeqAAttack = nullptr;
    std::atomic<float>* phiSeqARelease = nullptr;
    std::atomic<float>* phiSeqBEnabled = nullptr;
    std::atomic<float>* phiSeqBBipolar = nullptr;
    std::atomic<float>* phiSeqBAttack = nullptr;
    std::atomic<float>* phiSeqBRelease = nullptr;
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
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> phiStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> phiStepsB {};
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
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesB {};
    std::array<std::atomic<int>, seqwencer::audioFxStageCount> audioFxOrder {};

    double currentSampleRate = 44100.0;
    double gateFreeRunningPhase = 0.0;
    double phiFreeRunningPhase = 0.0;
    double delayFreeRunningPhase = 0.0;
    double reverbFreeRunningPhase = 0.0;
    double panFreeRunningPhase = 0.0;
    double filterFreeRunningPhase = 0.0;
    double pitchFreeRunningPhase = 0.0;
    double distortionFreeRunningPhase = 0.0;
    double grainFreeRunningPhase = 0.0;
    double compressorFreeRunningPhase = 0.0;
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
    std::atomic<int> phiActiveStepA { 0 };
    std::atomic<int> phiActiveStepB { -1 };
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
    juce::AudioBuffer<float> delayBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>
        smoothedDelaySamples;
    int delayWritePosition = 0;
    bool delayWasActive = false;
    juce::Reverb reverbProcessor;
    bool reverbWasActive = false;
    std::array<FilterState, 2> filterStates {};
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
    std::atomic<bool> phiHostPresent { false };
    std::atomic<bool> phiTargetBrowserRequestPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqwencerAudioProcessor)
};
