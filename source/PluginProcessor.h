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
    double getTailLengthSeconds() const override { return 0.0; }
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

    int getActiveStepA (bool phiEngine = false) const noexcept
    {
        return (phiEngine ? phiActiveStepA : gateActiveStepA).load();
    }
    int getActiveStepB (bool phiEngine = false) const noexcept
    {
        return (phiEngine ? phiActiveStepB : gateActiveStepB).load();
    }
    bool isPhiHostPresent() const noexcept
    {
        return phiHostPresent.load (std::memory_order_acquire);
    }
    void requestPhiTargetBrowser() noexcept
    {
        phiTargetBrowserRequestPending.store (true, std::memory_order_release);
    }

    static juce::String stepParameterID (int bank, int step);
    static juce::String phiStepParameterID (int bank, int step);
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
    void resetToInitialPreset();
    juce::String getCurrentPresetName() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    seqwencer::Pattern readPattern (
        int bank, seqwencer::ModulationTarget target) const noexcept;
    seqwencer::Pattern readPhiPattern (int bank,
                                       bool bipolar) const noexcept;
    seqwencer::GateModePattern readGateModes (int bank) const noexcept;
    void migrateStepStorageIfNeeded();

    juce::AudioProcessorValueTreeState parameters;

    juce::RangedAudioParameter* bypassParameter = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* syncToHost = nullptr;
    std::atomic<float>* playbackMode = nullptr;
    std::atomic<float>* serialProfile = nullptr;
    std::atomic<float>* startStep = nullptr;
    std::atomic<float>* endStep = nullptr;
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
    std::array<std::atomic<float>*, seqwencer::gateModulationTargetCount>
        targetsAssignedA {};
    std::array<std::atomic<float>*, seqwencer::gateModulationTargetCount>
        targetsEnabledA {};
    std::array<std::atomic<float>*, seqwencer::gateModulationTargetCount>
        targetsAssignedB {};
    std::array<std::atomic<float>*, seqwencer::gateModulationTargetCount>
        targetsEnabledB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> stepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> stepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> phiStepsA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> phiStepsB {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesA {};
    std::array<std::atomic<float>*, seqwencer::stepsPerBank> gateModesB {};

    double currentSampleRate = 44100.0;
    double gateFreeRunningPhase = 0.0;
    double phiFreeRunningPhase = 0.0;
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
    std::atomic<bool> phiHostPresent { false };
    std::atomic<bool> phiTargetBrowserRequestPending { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqwencerAudioProcessor)
};
