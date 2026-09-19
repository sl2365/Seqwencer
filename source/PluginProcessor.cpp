#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SeqwencerBridgeProtocol.h"

#include <array>
#include <cmath>
#include <vector>

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace
{
juce::String sanitisePresetName (const juce::String& proposedName)
{
    const auto trimmedName = proposedName.trim();
    const juce::String forbiddenCharacters { "<>:\"/\\|?*" };
    juce::String safeName;
    for (int index = 0; index < trimmedName.length(); ++index)
    {
        const auto character = trimmedName[index];
        safeName += character < 32 || forbiddenCharacters.containsChar (character)
            ? '_' : character;
    }
    while (safeName.endsWithChar ('.') || safeName.endsWithChar (' '))
        safeName = safeName.dropLastCharacters (1);
    return safeName.substring (0, juce::jmin (48, safeName.length()));
}
}

SeqwencerAudioProcessor::SeqwencerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "SEQWENCER_STATE", createParameterLayout())
{
    bypassParameter = parameters.getParameter ("bypass");
    bypass = parameters.getRawParameterValue ("bypass");
    syncToHost = parameters.getRawParameterValue ("sync_to_host");
    playbackMode = parameters.getRawParameterValue ("playback_mode");
    serialProfile = parameters.getRawParameterValue ("serial_profile");
    startStep = parameters.getRawParameterValue ("start_step");
    endStep = parameters.getRawParameterValue ("end_step");
    rate = parameters.getRawParameterValue ("rate");
    sequenceMode = parameters.getRawParameterValue ("sequence_mode");
    gateEnabled = parameters.getRawParameterValue ("gate_enabled");
    gateBase = parameters.getRawParameterValue ("gate_base");
    gateDepth = parameters.getRawParameterValue ("gate_depth");
    shortGateLength = parameters.getRawParameterValue ("gate_short_length");
    longGateLength = parameters.getRawParameterValue ("gate_long_length");
    noiseGateEnabled = parameters.getRawParameterValue ("noise_gate_enabled");
    noiseGateThreshold = parameters.getRawParameterValue ("noise_gate_threshold");
    noiseGateAttack = parameters.getRawParameterValue ("noise_gate_attack");
    noiseGateHold = parameters.getRawParameterValue ("noise_gate_hold");
    noiseGateRelease = parameters.getRawParameterValue ("noise_gate_release");
    noiseGateRange = parameters.getRawParameterValue ("noise_gate_range");
    phiBridgeEnabled = parameters.getRawParameterValue ("phi_bridge_enabled");
    phiPlaybackMode = parameters.getRawParameterValue ("phi_playback_mode");
    phiSerialProfile = parameters.getRawParameterValue ("phi_serial_profile");
    phiStartStep = parameters.getRawParameterValue ("phi_start_step");
    phiEndStep = parameters.getRawParameterValue ("phi_end_step");
    phiRate = parameters.getRawParameterValue ("phi_rate");
    phiSequenceMode = parameters.getRawParameterValue ("phi_sequence_mode");
    phiSeqAEnabled = parameters.getRawParameterValue ("phi_seq_a_enabled");
    phiSeqABipolar = parameters.getRawParameterValue ("phi_seq_a_bipolar");
    phiSeqAAttack = parameters.getRawParameterValue ("phi_seq_a_attack");
    phiSeqARelease = parameters.getRawParameterValue ("phi_seq_a_release");
    phiSeqBEnabled = parameters.getRawParameterValue ("phi_seq_b_enabled");
    phiSeqBBipolar = parameters.getRawParameterValue ("phi_seq_b_bipolar");
    phiSeqBAttack = parameters.getRawParameterValue ("phi_seq_b_attack");
    phiSeqBRelease = parameters.getRawParameterValue ("phi_seq_b_release");
    delayEnabled = parameters.getRawParameterValue ("delay_enabled");
    delayTime = parameters.getRawParameterValue ("delay_time");
    delayFeedback = parameters.getRawParameterValue ("delay_feedback");
    delayMix = parameters.getRawParameterValue ("delay_mix");
    delayPlaybackMode = parameters.getRawParameterValue ("delay_playback_mode");
    delaySerialProfile = parameters.getRawParameterValue ("delay_serial_profile");
    delayStartStep = parameters.getRawParameterValue ("delay_start_step");
    delayEndStep = parameters.getRawParameterValue ("delay_end_step");
    delayRate = parameters.getRawParameterValue ("delay_rate");
    delaySequenceMode = parameters.getRawParameterValue ("delay_sequence_mode");
    delaySeqAEnabled = parameters.getRawParameterValue ("delay_seq_a_enabled");
    delaySeqABipolar = parameters.getRawParameterValue ("delay_seq_a_bipolar");
    delaySeqAAttack = parameters.getRawParameterValue ("delay_seq_a_attack");
    delaySeqARelease = parameters.getRawParameterValue ("delay_seq_a_release");
    delaySeqBEnabled = parameters.getRawParameterValue ("delay_seq_b_enabled");
    delaySeqBBipolar = parameters.getRawParameterValue ("delay_seq_b_bipolar");
    delaySeqBAttack = parameters.getRawParameterValue ("delay_seq_b_attack");
    delaySeqBRelease = parameters.getRawParameterValue ("delay_seq_b_release");
    reverbEnabled = parameters.getRawParameterValue ("reverb_enabled");
    reverbSize = parameters.getRawParameterValue ("reverb_size");
    reverbDamping = parameters.getRawParameterValue ("reverb_damping");
    reverbWidth = parameters.getRawParameterValue ("reverb_width");
    reverbMix = parameters.getRawParameterValue ("reverb_mix");
    reverbPlaybackMode = parameters.getRawParameterValue ("reverb_playback_mode");
    reverbSerialProfile = parameters.getRawParameterValue ("reverb_serial_profile");
    reverbStartStep = parameters.getRawParameterValue ("reverb_start_step");
    reverbEndStep = parameters.getRawParameterValue ("reverb_end_step");
    reverbRate = parameters.getRawParameterValue ("reverb_rate");
    reverbSequenceMode = parameters.getRawParameterValue ("reverb_sequence_mode");
    reverbSeqAEnabled = parameters.getRawParameterValue ("reverb_seq_a_enabled");
    reverbSeqABipolar = parameters.getRawParameterValue ("reverb_seq_a_bipolar");
    reverbSeqAAttack = parameters.getRawParameterValue ("reverb_seq_a_attack");
    reverbSeqARelease = parameters.getRawParameterValue ("reverb_seq_a_release");
    reverbSeqBEnabled = parameters.getRawParameterValue ("reverb_seq_b_enabled");
    reverbSeqBBipolar = parameters.getRawParameterValue ("reverb_seq_b_bipolar");
    reverbSeqBAttack = parameters.getRawParameterValue ("reverb_seq_b_attack");
    reverbSeqBRelease = parameters.getRawParameterValue ("reverb_seq_b_release");
    panEnabled = parameters.getRawParameterValue ("pan_enabled");
    panPosition = parameters.getRawParameterValue ("pan_position");
    panPlaybackMode = parameters.getRawParameterValue ("pan_playback_mode");
    panSerialProfile = parameters.getRawParameterValue ("pan_serial_profile");
    panStartStep = parameters.getRawParameterValue ("pan_start_step");
    panEndStep = parameters.getRawParameterValue ("pan_end_step");
    panRate = parameters.getRawParameterValue ("pan_rate");
    panSequenceMode = parameters.getRawParameterValue ("pan_sequence_mode");
    panSeqAEnabled = parameters.getRawParameterValue ("pan_seq_a_enabled");
    panSeqABipolar = parameters.getRawParameterValue ("pan_seq_a_bipolar");
    panSeqAAttack = parameters.getRawParameterValue ("pan_seq_a_attack");
    panSeqARelease = parameters.getRawParameterValue ("pan_seq_a_release");
    panSeqBEnabled = parameters.getRawParameterValue ("pan_seq_b_enabled");
    panSeqBBipolar = parameters.getRawParameterValue ("pan_seq_b_bipolar");
    panSeqBAttack = parameters.getRawParameterValue ("pan_seq_b_attack");
    panSeqBRelease = parameters.getRawParameterValue ("pan_seq_b_release");
    filterEnabled = parameters.getRawParameterValue ("filter_enabled");
    filterType = parameters.getRawParameterValue ("filter_type");
    filterCutoff = parameters.getRawParameterValue ("filter_cutoff");
    filterResonance = parameters.getRawParameterValue ("filter_resonance");
    filterMix = parameters.getRawParameterValue ("filter_mix");
    filterPlaybackMode = parameters.getRawParameterValue ("filter_playback_mode");
    filterSerialProfile = parameters.getRawParameterValue ("filter_serial_profile");
    filterStartStep = parameters.getRawParameterValue ("filter_start_step");
    filterEndStep = parameters.getRawParameterValue ("filter_end_step");
    filterRate = parameters.getRawParameterValue ("filter_rate");
    filterSequenceMode = parameters.getRawParameterValue ("filter_sequence_mode");
    filterSeqAEnabled = parameters.getRawParameterValue ("filter_seq_a_enabled");
    filterSeqABipolar = parameters.getRawParameterValue ("filter_seq_a_bipolar");
    filterSeqAAttack = parameters.getRawParameterValue ("filter_seq_a_attack");
    filterSeqARelease = parameters.getRawParameterValue ("filter_seq_a_release");
    filterSeqBEnabled = parameters.getRawParameterValue ("filter_seq_b_enabled");
    filterSeqBBipolar = parameters.getRawParameterValue ("filter_seq_b_bipolar");
    filterSeqBAttack = parameters.getRawParameterValue ("filter_seq_b_attack");
    filterSeqBRelease = parameters.getRawParameterValue ("filter_seq_b_release");
    pitchEnabled = parameters.getRawParameterValue ("pitch_enabled");
    pitchShift = parameters.getRawParameterValue ("pitch_shift");
    pitchMix = parameters.getRawParameterValue ("pitch_mix");
    pitchPlaybackMode = parameters.getRawParameterValue ("pitch_playback_mode");
    pitchSerialProfile = parameters.getRawParameterValue ("pitch_serial_profile");
    pitchStartStep = parameters.getRawParameterValue ("pitch_start_step");
    pitchEndStep = parameters.getRawParameterValue ("pitch_end_step");
    pitchRate = parameters.getRawParameterValue ("pitch_rate");
    pitchSequenceMode = parameters.getRawParameterValue ("pitch_sequence_mode");
    pitchSeqAEnabled = parameters.getRawParameterValue ("pitch_seq_a_enabled");
    pitchSeqABipolar = parameters.getRawParameterValue ("pitch_seq_a_bipolar");
    pitchSeqAAttack = parameters.getRawParameterValue ("pitch_seq_a_attack");
    pitchSeqARelease = parameters.getRawParameterValue ("pitch_seq_a_release");
    pitchSeqBEnabled = parameters.getRawParameterValue ("pitch_seq_b_enabled");
    pitchSeqBBipolar = parameters.getRawParameterValue ("pitch_seq_b_bipolar");
    pitchSeqBAttack = parameters.getRawParameterValue ("pitch_seq_b_attack");
    pitchSeqBRelease = parameters.getRawParameterValue ("pitch_seq_b_release");
    seqAEnabled = parameters.getRawParameterValue ("seq_a_enabled");
    seqATarget = parameters.getRawParameterValue ("seq_a_target");
    seqATargetEnabled = parameters.getRawParameterValue ("seq_a_target_enabled");
    seqABipolar = parameters.getRawParameterValue ("seq_a_bipolar");
    seqAAttack = parameters.getRawParameterValue ("seq_a_attack");
    seqARelease = parameters.getRawParameterValue ("seq_a_release");
    seqBEnabled = parameters.getRawParameterValue ("seq_b_enabled");
    seqBTarget = parameters.getRawParameterValue ("seq_b_target");
    seqBTargetEnabled = parameters.getRawParameterValue ("seq_b_target_enabled");
    seqBBipolar = parameters.getRawParameterValue ("seq_b_bipolar");
    seqBAttack = parameters.getRawParameterValue ("seq_b_attack");
    seqBRelease = parameters.getRawParameterValue ("seq_b_release");

    for (int index = 0; index < seqwencer::modulationTargetCount; ++index)
    {
        const auto target = static_cast<seqwencer::ModulationTarget> (index + 1);
        targetsAssignedA[static_cast<std::size_t> (index)] =
            parameters.getRawParameterValue (targetAssignedParameterID (0, target));
        targetsEnabledA[static_cast<std::size_t> (index)] =
            parameters.getRawParameterValue (targetEnabledParameterID (0, target));
        targetsAssignedB[static_cast<std::size_t> (index)] =
            parameters.getRawParameterValue (targetAssignedParameterID (1, target));
        targetsEnabledB[static_cast<std::size_t> (index)] =
            parameters.getRawParameterValue (targetEnabledParameterID (1, target));
    }

    for (int step = 0; step < seqwencer::stepsPerBank; ++step)
    {
        stepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (stepParameterID (0, step));
        stepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (stepParameterID (1, step));
        phiStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (phiStepParameterID (0, step));
        phiStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (phiStepParameterID (1, step));
        delayStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (delayStepParameterID (0, step));
        delayStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (delayStepParameterID (1, step));
        reverbStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverbStepParameterID (0, step));
        reverbStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverbStepParameterID (1, step));
        panStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (panStepParameterID (0, step));
        panStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (panStepParameterID (1, step));
        filterStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (filterStepParameterID (0, step));
        filterStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (filterStepParameterID (1, step));
        pitchStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (pitchStepParameterID (0, step));
        pitchStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (pitchStepParameterID (1, step));
        gateModesA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (gateModeParameterID (0, step));
        gateModesB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (gateModeParameterID (1, step));
    }

    parameters.state.setProperty ("step_encoding_version", 2, nullptr);
    parameters.state.setProperty ("step_encoding_a_bipolar", true, nullptr);
    parameters.state.setProperty ("step_encoding_b_bipolar", true, nullptr);
}

juce::String SeqwencerAudioProcessor::stepParameterID (int bank, int step)
{
    return "seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::phiStepParameterID (int bank, int step)
{
    return "phi_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::delayStepParameterID (int bank, int step)
{
    return "delay_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::reverbStepParameterID (int bank, int step)
{
    return "reverb_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::panStepParameterID (int bank, int step)
{
    return "pan_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::filterStepParameterID (int bank, int step)
{
    return "filter_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::pitchStepParameterID (int bank, int step)
{
    return "pitch_seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::gateModeParameterID (int bank, int step)
{
    return "seq_" + juce::String (bank == 0 ? "a" : "b") + "_gate_mode_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::targetAssignedParameterID (
    int bank, seqwencer::ModulationTarget target)
{
    const auto prefix = bank == 0 ? "seq_a_" : "seq_b_";
    switch (target)
    {
        case seqwencer::ModulationTarget::gateLevel:
            return juce::String (prefix) + "target";
        case seqwencer::ModulationTarget::gateDepth:
            return juce::String (prefix) + "depth_target";
        case seqwencer::ModulationTarget::shortGateLength:
            return juce::String (prefix) + "short_target";
        case seqwencer::ModulationTarget::longGateLength:
            return juce::String (prefix) + "long_target";
        case seqwencer::ModulationTarget::noiseGateThreshold:
            return juce::String (prefix) + "noise_threshold_target";
        case seqwencer::ModulationTarget::noiseGateAttack:
            return juce::String (prefix) + "noise_attack_target";
        case seqwencer::ModulationTarget::noiseGateHold:
            return juce::String (prefix) + "noise_hold_target";
        case seqwencer::ModulationTarget::noiseGateRelease:
            return juce::String (prefix) + "noise_release_target";
        case seqwencer::ModulationTarget::noiseGateRange:
            return juce::String (prefix) + "noise_range_target";
        case seqwencer::ModulationTarget::delayTime:
            return juce::String (bank == 0 ? "delay_seq_a_time_target"
                                           : "delay_seq_b_time_target");
        case seqwencer::ModulationTarget::delayFeedback:
            return juce::String (bank == 0 ? "delay_seq_a_feedback_target"
                                           : "delay_seq_b_feedback_target");
        case seqwencer::ModulationTarget::delayMix:
            return juce::String (bank == 0 ? "delay_seq_a_mix_target"
                                           : "delay_seq_b_mix_target");
        case seqwencer::ModulationTarget::reverbSize:
            return juce::String (bank == 0 ? "reverb_seq_a_size_target"
                                           : "reverb_seq_b_size_target");
        case seqwencer::ModulationTarget::reverbDamping:
            return juce::String (bank == 0 ? "reverb_seq_a_damping_target"
                                           : "reverb_seq_b_damping_target");
        case seqwencer::ModulationTarget::reverbWidth:
            return juce::String (bank == 0 ? "reverb_seq_a_width_target"
                                           : "reverb_seq_b_width_target");
        case seqwencer::ModulationTarget::reverbMix:
            return juce::String (bank == 0 ? "reverb_seq_a_mix_target"
                                           : "reverb_seq_b_mix_target");
        case seqwencer::ModulationTarget::panPosition:
            return juce::String (bank == 0 ? "pan_seq_a_position_target"
                                           : "pan_seq_b_position_target");
        case seqwencer::ModulationTarget::filterCutoff:
            return juce::String (bank == 0 ? "filter_seq_a_cutoff_target"
                                           : "filter_seq_b_cutoff_target");
        case seqwencer::ModulationTarget::filterResonance:
            return juce::String (bank == 0 ? "filter_seq_a_resonance_target"
                                           : "filter_seq_b_resonance_target");
        case seqwencer::ModulationTarget::filterMix:
            return juce::String (bank == 0 ? "filter_seq_a_mix_target"
                                           : "filter_seq_b_mix_target");
        case seqwencer::ModulationTarget::pitchShift:
            return juce::String (bank == 0 ? "pitch_seq_a_shift_target"
                                           : "pitch_seq_b_shift_target");
        case seqwencer::ModulationTarget::pitchMix:
            return juce::String (bank == 0 ? "pitch_seq_a_mix_target"
                                           : "pitch_seq_b_mix_target");
        case seqwencer::ModulationTarget::none:
            break;
    }
    return {};
}

juce::String SeqwencerAudioProcessor::targetEnabledParameterID (
    int bank, seqwencer::ModulationTarget target)
{
    const auto prefix = bank == 0 ? "seq_a_" : "seq_b_";
    switch (target)
    {
        case seqwencer::ModulationTarget::gateLevel:
            return juce::String (prefix) + "target_enabled";
        case seqwencer::ModulationTarget::gateDepth:
            return juce::String (prefix) + "depth_target_enabled";
        case seqwencer::ModulationTarget::shortGateLength:
            return juce::String (prefix) + "short_target_enabled";
        case seqwencer::ModulationTarget::longGateLength:
            return juce::String (prefix) + "long_target_enabled";
        case seqwencer::ModulationTarget::noiseGateThreshold:
            return juce::String (prefix) + "noise_threshold_target_enabled";
        case seqwencer::ModulationTarget::noiseGateAttack:
            return juce::String (prefix) + "noise_attack_target_enabled";
        case seqwencer::ModulationTarget::noiseGateHold:
            return juce::String (prefix) + "noise_hold_target_enabled";
        case seqwencer::ModulationTarget::noiseGateRelease:
            return juce::String (prefix) + "noise_release_target_enabled";
        case seqwencer::ModulationTarget::noiseGateRange:
            return juce::String (prefix) + "noise_range_target_enabled";
        case seqwencer::ModulationTarget::delayTime:
            return juce::String (bank == 0 ? "delay_seq_a_time_target_enabled"
                                           : "delay_seq_b_time_target_enabled");
        case seqwencer::ModulationTarget::delayFeedback:
            return juce::String (bank == 0 ? "delay_seq_a_feedback_target_enabled"
                                           : "delay_seq_b_feedback_target_enabled");
        case seqwencer::ModulationTarget::delayMix:
            return juce::String (bank == 0 ? "delay_seq_a_mix_target_enabled"
                                           : "delay_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::reverbSize:
            return juce::String (bank == 0 ? "reverb_seq_a_size_target_enabled"
                                           : "reverb_seq_b_size_target_enabled");
        case seqwencer::ModulationTarget::reverbDamping:
            return juce::String (bank == 0 ? "reverb_seq_a_damping_target_enabled"
                                           : "reverb_seq_b_damping_target_enabled");
        case seqwencer::ModulationTarget::reverbWidth:
            return juce::String (bank == 0 ? "reverb_seq_a_width_target_enabled"
                                           : "reverb_seq_b_width_target_enabled");
        case seqwencer::ModulationTarget::reverbMix:
            return juce::String (bank == 0 ? "reverb_seq_a_mix_target_enabled"
                                           : "reverb_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::panPosition:
            return juce::String (bank == 0 ? "pan_seq_a_position_target_enabled"
                                           : "pan_seq_b_position_target_enabled");
        case seqwencer::ModulationTarget::filterCutoff:
            return juce::String (bank == 0 ? "filter_seq_a_cutoff_target_enabled"
                                           : "filter_seq_b_cutoff_target_enabled");
        case seqwencer::ModulationTarget::filterResonance:
            return juce::String (bank == 0 ? "filter_seq_a_resonance_target_enabled"
                                           : "filter_seq_b_resonance_target_enabled");
        case seqwencer::ModulationTarget::filterMix:
            return juce::String (bank == 0 ? "filter_seq_a_mix_target_enabled"
                                           : "filter_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::pitchShift:
            return juce::String (bank == 0 ? "pitch_seq_a_shift_target_enabled"
                                           : "pitch_seq_b_shift_target_enabled");
        case seqwencer::ModulationTarget::pitchMix:
            return juce::String (bank == 0 ? "pitch_seq_a_mix_target_enabled"
                                           : "pitch_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::none:
            break;
    }
    return {};
}

juce::String SeqwencerAudioProcessor::targetDisplayName (
    seqwencer::ModulationTarget target)
{
    switch (target)
    {
        case seqwencer::ModulationTarget::gateLevel:       return "VOLUME";
        case seqwencer::ModulationTarget::gateDepth:       return "DEPTH";
        case seqwencer::ModulationTarget::shortGateLength: return "SHORT STEP";
        case seqwencer::ModulationTarget::longGateLength:  return "LONG STEP";
        case seqwencer::ModulationTarget::noiseGateThreshold: return "THRESHOLD";
        case seqwencer::ModulationTarget::noiseGateAttack: return "NOISE ATTACK";
        case seqwencer::ModulationTarget::noiseGateHold: return "HOLD";
        case seqwencer::ModulationTarget::noiseGateRelease: return "NOISE RELEASE";
        case seqwencer::ModulationTarget::noiseGateRange: return "RANGE";
        case seqwencer::ModulationTarget::delayTime: return "TIME";
        case seqwencer::ModulationTarget::delayFeedback: return "FEEDBACK";
        case seqwencer::ModulationTarget::delayMix: return "MIX";
        case seqwencer::ModulationTarget::reverbSize: return "SIZE";
        case seqwencer::ModulationTarget::reverbDamping: return "DAMPING";
        case seqwencer::ModulationTarget::reverbWidth: return "WIDTH";
        case seqwencer::ModulationTarget::reverbMix: return "MIX";
        case seqwencer::ModulationTarget::panPosition: return "PAN";
        case seqwencer::ModulationTarget::filterCutoff: return "CUTOFF";
        case seqwencer::ModulationTarget::filterResonance: return "RESONANCE";
        case seqwencer::ModulationTarget::filterMix: return "MIX";
        case seqwencer::ModulationTarget::pitchShift: return "SHIFT";
        case seqwencer::ModulationTarget::pitchMix: return "MIX";
        case seqwencer::ModulationTarget::none:            break;
    }
    return {};
}

juce::File SeqwencerAudioProcessor::getPortableDataDirectory()
{
#if JUCE_WINDOWS
    static int moduleAnchor = 0;
    HMODULE module = nullptr;
    const auto address = reinterpret_cast<LPCWSTR> (
        static_cast<void*> (&moduleAnchor));
    if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            address, &module) != 0)
    {
        std::array<wchar_t, 32768> modulePath {};
        const auto length = GetModuleFileNameW (
            module, modulePath.data(), static_cast<DWORD> (modulePath.size()));
        if (length > 0 && length < modulePath.size())
            return juce::File (juce::String (modulePath.data()))
                .getParentDirectory().getChildFile ("Data");
    }
#endif
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
        .getParentDirectory().getChildFile ("Data");
}

juce::File SeqwencerAudioProcessor::getPortablePresetDirectory()
{
    return getPortableDataDirectory().getChildFile ("Presets");
}

bool SeqwencerAudioProcessor::savePortablePreset (
    const juce::File& file,
    const juce::String& proposedName,
    juce::String& errorMessage)
{
    const auto presetName = sanitisePresetName (proposedName);
    if (presetName.isEmpty())
    {
        errorMessage = "Please enter a preset name.";
        return false;
    }

    const auto directoryResult = file.getParentDirectory().createDirectory();
    if (directoryResult.failed())
    {
        errorMessage = directoryResult.getErrorMessage();
        return false;
    }

    juce::String globalParameterLines;
    juce::String gateParameterLines;
    juce::String phiParameterLines;
    juce::String delayParameterLines;
    juce::String reverbParameterLines;
    juce::String panParameterLines;
    juce::String filterParameterLines;
    juce::String pitchParameterLines;
    auto parameterCount = 0;
    for (auto* baseParameter : getParameters())
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                baseParameter))
        {
            const auto parameterID = parameter->getParameterID();
            auto* destination = &gateParameterLines;
            if (parameterID.startsWith ("phi_"))
                destination = &phiParameterLines;
            else if (parameterID.startsWith ("delay_"))
                destination = &delayParameterLines;
            else if (parameterID.startsWith ("reverb_"))
                destination = &reverbParameterLines;
            else if (parameterID.startsWith ("pan_"))
                destination = &panParameterLines;
            else if (parameterID.startsWith ("filter_"))
                destination = &filterParameterLines;
            else if (parameterID.startsWith ("pitch_"))
                destination = &pitchParameterLines;
            else if (parameterID == "bypass" || parameterID == "sync_to_host")
                destination = &globalParameterLines;

            *destination << parameterID << "="
                         << juce::String (parameter->getValue(), 7) << "\r\n";
            ++parameterCount;
        }
    }

    juce::String contents { "; Seqwencer portable preset\r\n" };
    contents << "[Preset]\r\nFormatVersion=1\r\nName=" << presetName
             << "\r\nParameterCount=" << parameterCount
             << "\r\n\r\n[Global]\r\n" << globalParameterLines
             << "\r\n[Gate]\r\n" << gateParameterLines
             << "\r\n[PHI]\r\n" << phiParameterLines
             << "\r\n[Delay]\r\n" << delayParameterLines
             << "\r\n[Reverb]\r\n" << reverbParameterLines
             << "\r\n[Pan]\r\n" << panParameterLines
             << "\r\n[Filter]\r\n" << filterParameterLines
             << "\r\n[Pitch]\r\n" << pitchParameterLines;
    if (! file.replaceWithText (contents))
    {
        errorMessage = "Seqwencer could not write:\n" + file.getFullPathName();
        return false;
    }

    parameters.state.setProperty ("presetName", presetName, nullptr);
    return true;
}

bool SeqwencerAudioProcessor::loadPortablePreset (
    const juce::File& file, juce::String& errorMessage)
{
    if (! file.existsAsFile())
    {
        errorMessage = "Preset not found:\n" + file.getFullPathName();
        return false;
    }

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    auto inParameters = false;
    auto validVersion = false;
    juce::String loadedName;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> values;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar (';') || line.startsWithChar ('#'))
            continue;
        if (line == "[Preset]")
        {
            inParameters = false;
            continue;
        }
        if (line == "[Parameters]" || line == "[Global]"
            || line == "[Gate]" || line == "[PHI]" || line == "[Delay]"
            || line == "[Reverb]" || line == "[Pan]" || line == "[Filter]"
            || line == "[Pitch]")
        {
            inParameters = true;
            continue;
        }
        const auto separator = line.indexOfChar ('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring (0, separator).trim();
        const auto value = line.substring (separator + 1).trim();
        if (! inParameters)
        {
            if (key == "FormatVersion")
                validVersion = value.getIntValue() == 1;
            else if (key == "Name")
                loadedName = value;
            continue;
        }
        if (auto* parameter = parameters.getParameter (key))
            values.emplace_back (parameter,
                juce::jlimit (0.0f, 1.0f, value.getFloatValue()));
    }

    if (! validVersion || values.empty())
    {
        errorMessage = file.getFileName() + " is not a valid Seqwencer preset.";
        return false;
    }

    for (auto* baseParameter : getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (baseParameter);
        if (parameter == nullptr)
            continue;
        auto value = parameter->getDefaultValue();
        for (const auto& loaded : values)
            if (loaded.first == parameter)
                value = loaded.second;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (value);
        parameter->endChangeGesture();
    }
    parameters.state.setProperty (
        "presetName", loadedName.isNotEmpty()
            ? loadedName : file.getFileNameWithoutExtension(), nullptr);
    return true;
}

void SeqwencerAudioProcessor::resetToInitialPreset()
{
    for (auto* baseParameter : getParameters())
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
                baseParameter))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }
    parameters.state.setProperty ("presetName", "INITIAL", nullptr);
}

juce::String SeqwencerAudioProcessor::getCurrentPresetName() const
{
    return parameters.state.getProperty ("presetName", "INITIAL").toString();
}

juce::AudioProcessorValueTreeState::ParameterLayout
SeqwencerAudioProcessor::createParameterLayout()
{
    using Bool = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    using Float = juce::AudioParameterFloat;
    using Int = juce::AudioParameterInt;
    using ID = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<Bool> (ID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<Bool> (
        ID { "sync_to_host", 1 }, "Host Sync", true));
    layout.add (std::make_unique<Choice> (
        ID { "playback_mode", 1 }, "Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "rate", 1 }, "Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "gate_enabled", 1 }, "Gate Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "gate_base", 1 }, "Gate Volume", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<Float> (
        ID { "gate_depth", 1 }, "Gate Depth", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<Bool> (
        ID { "phi_bridge_enabled", 1 }, "PHI Bridge", false));

    layout.add (std::make_unique<Bool> (
        ID { "seq_a_enabled", 1 }, "Sequencer A Enabled", true));
    layout.add (std::make_unique<Choice> (
        ID { "seq_a_target", 1 }, "Sequencer A Target",
        juce::StringArray { "None", "Volume" }, 1));
    layout.add (std::make_unique<Bool> (
        ID { "seq_a_target_enabled", 1 }, "Sequencer A Target Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "seq_a_attack", 1 }, "Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "seq_a_release", 1 }, "Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "seq_b_enabled", 1 }, "Sequencer B Enabled", false));
    layout.add (std::make_unique<Choice> (
        ID { "seq_b_target", 1 }, "Sequencer B Target",
        juce::StringArray { "None", "Volume" }, 1));
    layout.add (std::make_unique<Bool> (
        ID { "seq_b_target_enabled", 1 }, "Sequencer B Target Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "seq_b_attack", 1 }, "Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "seq_b_release", 1 }, "Sequencer B Release", 0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { stepParameterID (bank, step), 1 },
                "Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    // Gate engine timing, range, polarity and step-mode parameters.
    layout.add (std::make_unique<Choice> (
        ID { "serial_profile", 1 }, "Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "start_step", 1 }, "Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "end_step", 1 }, "End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "seq_a_bipolar", 1 }, "Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "seq_b_bipolar", 1 }, "Sequencer B Bipolar", false));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Choice> (
                ID { gateModeParameterID (bank, step), 1 },
                "Sequencer " + juce::String (bankName) + " Gate Mode "
                    + juce::String (step + 1),
                juce::StringArray { "Off", "Short", "Long", "Link" },
                static_cast<int> (seqwencer::GateStepMode::longStep)));
        }
    }

    layout.add (std::make_unique<Float> (
        ID { "gate_short_length", 1 }, "Gate Short Step Length",
        juce::NormalisableRange<float> { 0.10f, 0.60f, 0.01f }, 0.50f));
    layout.add (std::make_unique<Float> (
        ID { "gate_long_length", 1 }, "Gate Long Step Length",
        juce::NormalisableRange<float> { 0.65f, 0.95f, 0.01f }, 0.90f));
    layout.add (std::make_unique<Choice> (
        ID { "sequence_mode", 1 }, "Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));
    layout.add (std::make_unique<Bool> (
        ID { "noise_gate_enabled", 1 }, "Noise Gate Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "noise_gate_threshold", 1 }, "Noise Gate Threshold",
        juce::NormalisableRange<float> { -80.0f, 0.0f, 0.1f }, -60.0f));
    layout.add (std::make_unique<Float> (
        ID { "noise_gate_attack", 1 }, "Noise Gate Attack",
        juce::NormalisableRange<float> { 0.1f, 100.0f, 0.1f, 0.35f }, 5.0f));
    layout.add (std::make_unique<Float> (
        ID { "noise_gate_hold", 1 }, "Noise Gate Hold",
        juce::NormalisableRange<float> { 0.0f, 500.0f, 1.0f, 0.4f }, 50.0f));
    layout.add (std::make_unique<Float> (
        ID { "noise_gate_release", 1 }, "Noise Gate Release",
        juce::NormalisableRange<float> { 5.0f, 1000.0f, 1.0f, 0.35f }, 100.0f));
    layout.add (std::make_unique<Float> (
        ID { "noise_gate_range", 1 }, "Noise Gate Range",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0 ? "Sequencer A " : "Sequencer B ";
        for (int index = 1; index < seqwencer::gateModulationTargetCount; ++index)
        {
            const auto target = static_cast<seqwencer::ModulationTarget> (index + 1);
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    // PHI owns a complete second sequencer engine. These parameters are kept
    // separate from the Gate engine so each FX page retains its own pattern,
    // timing, range, direction, envelopes and polarity.
    layout.add (std::make_unique<Choice> (
        ID { "phi_playback_mode", 1 }, "PHI Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "phi_rate", 1 }, "PHI Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_a_enabled", 1 }, "PHI Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "phi_seq_a_attack", 1 }, "PHI Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "phi_seq_a_release", 1 }, "PHI Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_b_enabled", 1 }, "PHI Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "phi_seq_b_attack", 1 }, "PHI Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "phi_seq_b_release", 1 }, "PHI Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { phiStepParameterID (bank, step), 1 },
                "PHI Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "phi_serial_profile", 1 }, "PHI Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "phi_start_step", 1 }, "PHI Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "phi_end_step", 1 }, "PHI End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_a_bipolar", 1 }, "PHI Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_b_bipolar", 1 }, "PHI Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "phi_sequence_mode", 1 }, "PHI Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    // Delay owns a third complete sequencer engine and three internal targets.
    layout.add (std::make_unique<Bool> (
        ID { "delay_enabled", 1 }, "Delay Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "delay_time", 1 }, "Delay Time",
        juce::NormalisableRange<float> { 10.0f, 2000.0f, 0.1f, 0.35f },
        375.0f));
    layout.add (std::make_unique<Float> (
        ID { "delay_feedback", 1 }, "Delay Feedback",
        juce::NormalisableRange<float> { 0.0f, 0.95f, 0.001f }, 0.35f));
    layout.add (std::make_unique<Float> (
        ID { "delay_mix", 1 }, "Delay Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.25f));
    layout.add (std::make_unique<Choice> (
        ID { "delay_playback_mode", 1 }, "Delay Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "delay_rate", 1 }, "Delay Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_a_enabled", 1 }, "Delay Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "delay_seq_a_attack", 1 }, "Delay Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "delay_seq_a_release", 1 }, "Delay Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_b_enabled", 1 }, "Delay Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "delay_seq_b_attack", 1 }, "Delay Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "delay_seq_b_release", 1 }, "Delay Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { delayStepParameterID (bank, step), 1 },
                "Delay Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "delay_serial_profile", 1 }, "Delay Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "delay_start_step", 1 }, "Delay Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "delay_end_step", 1 }, "Delay End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_a_bipolar", 1 }, "Delay Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_b_bipolar", 1 }, "Delay Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "delay_sequence_mode", 1 }, "Delay Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Delay Sequencer A " : "Delay Sequencer B ";
        for (int index = static_cast<int> (seqwencer::ModulationTarget::delayTime);
             index <= static_cast<int> (seqwencer::ModulationTarget::delayMix);
             ++index)
        {
            const auto target = static_cast<seqwencer::ModulationTarget> (index);
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    // Reverb owns a fourth complete sequencer engine and four internal targets.
    layout.add (std::make_unique<Bool> (
        ID { "reverb_enabled", 1 }, "Reverb Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "reverb_size", 1 }, "Reverb Size",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.50f));
    layout.add (std::make_unique<Float> (
        ID { "reverb_damping", 1 }, "Reverb Damping",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.50f));
    layout.add (std::make_unique<Float> (
        ID { "reverb_width", 1 }, "Reverb Width",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverb_mix", 1 }, "Reverb Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.25f));
    layout.add (std::make_unique<Choice> (
        ID { "reverb_playback_mode", 1 }, "Reverb Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "reverb_rate", 1 }, "Reverb Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_a_enabled", 1 }, "Reverb Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "reverb_seq_a_attack", 1 }, "Reverb Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverb_seq_a_release", 1 }, "Reverb Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_b_enabled", 1 }, "Reverb Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "reverb_seq_b_attack", 1 }, "Reverb Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverb_seq_b_release", 1 }, "Reverb Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { reverbStepParameterID (bank, step), 1 },
                "Reverb Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "reverb_serial_profile", 1 }, "Reverb Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "reverb_start_step", 1 }, "Reverb Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "reverb_end_step", 1 }, "Reverb End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_a_bipolar", 1 }, "Reverb Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_b_bipolar", 1 }, "Reverb Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "reverb_sequence_mode", 1 }, "Reverb Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Reverb Sequencer A " : "Reverb Sequencer B ";
        for (int index = static_cast<int> (seqwencer::ModulationTarget::reverbSize);
             index <= static_cast<int> (seqwencer::ModulationTarget::reverbMix);
             ++index)
        {
            const auto target = static_cast<seqwencer::ModulationTarget> (index);
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    // Pan owns a fifth complete sequencer engine and a bipolar position target.
    layout.add (std::make_unique<Bool> (
        ID { "pan_enabled", 1 }, "Pan Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "pan_position", 1 }, "Pan Position",
        juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<Choice> (
        ID { "pan_playback_mode", 1 }, "Pan Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "pan_rate", 1 }, "Pan Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_a_enabled", 1 }, "Pan Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "pan_seq_a_attack", 1 }, "Pan Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "pan_seq_a_release", 1 }, "Pan Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_b_enabled", 1 }, "Pan Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "pan_seq_b_attack", 1 }, "Pan Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "pan_seq_b_release", 1 }, "Pan Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { panStepParameterID (bank, step), 1 },
                "Pan Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.5f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "pan_serial_profile", 1 }, "Pan Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "pan_start_step", 1 }, "Pan Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "pan_end_step", 1 }, "Pan End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_a_bipolar", 1 }, "Pan Sequencer A Bipolar", true));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_b_bipolar", 1 }, "Pan Sequencer B Bipolar", true));
    layout.add (std::make_unique<Choice> (
        ID { "pan_sequence_mode", 1 }, "Pan Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto target = seqwencer::ModulationTarget::panPosition;
        const juce::String laneName = bank == 0
            ? "Pan Sequencer A " : "Pan Sequencer B ";
        layout.add (std::make_unique<Bool> (
            ID { targetAssignedParameterID (bank, target), 1 },
            laneName + "PAN Target", false));
        layout.add (std::make_unique<Bool> (
            ID { targetEnabledParameterID (bank, target), 1 },
            laneName + "PAN Target Enabled", true));
    }

    // Filter owns a sixth complete sequencer engine and three internal targets.
    layout.add (std::make_unique<Bool> (
        ID { "filter_enabled", 1 }, "Filter Enabled", false));
    layout.add (std::make_unique<Choice> (
        ID { "filter_type", 1 }, "Filter Type",
        juce::StringArray { "Low Pass", "High Pass", "Band Pass",
                            "Band Reject", "Peaking" }, 0));
    layout.add (std::make_unique<Float> (
        ID { "filter_cutoff", 1 }, "Filter Cutoff",
        juce::NormalisableRange<float> { 20.0f, 20000.0f, 0.1f, 0.25f },
        20000.0f));
    layout.add (std::make_unique<Float> (
        ID { "filter_resonance", 1 }, "Filter Resonance",
        juce::NormalisableRange<float> { 0.10f, 10.0f, 0.001f, 0.40f },
        0.707f));
    layout.add (std::make_unique<Float> (
        ID { "filter_mix", 1 }, "Filter Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "filter_playback_mode", 1 }, "Filter Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "filter_rate", 1 }, "Filter Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_a_enabled", 1 }, "Filter Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "filter_seq_a_attack", 1 }, "Filter Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "filter_seq_a_release", 1 }, "Filter Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_b_enabled", 1 }, "Filter Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "filter_seq_b_attack", 1 }, "Filter Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "filter_seq_b_release", 1 }, "Filter Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { filterStepParameterID (bank, step), 1 },
                "Filter Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "filter_serial_profile", 1 }, "Filter Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "filter_start_step", 1 }, "Filter Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "filter_end_step", 1 }, "Filter End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_a_bipolar", 1 }, "Filter Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_b_bipolar", 1 }, "Filter Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "filter_sequence_mode", 1 }, "Filter Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Filter Sequencer A " : "Filter Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::filterCutoff);
             index <= static_cast<int> (seqwencer::ModulationTarget::filterMix);
             ++index)
        {
            const auto target = static_cast<seqwencer::ModulationTarget> (index);
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    // Pitch owns a seventh complete sequencer engine and two internal targets.
    layout.add (std::make_unique<Bool> (
        ID { "pitch_enabled", 1 }, "Pitch Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "pitch_shift", 1 }, "Pitch Shift",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "pitch_mix", 1 }, "Pitch Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "pitch_playback_mode", 1 }, "Pitch Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "pitch_rate", 1 }, "Pitch Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_a_enabled", 1 }, "Pitch Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "pitch_seq_a_attack", 1 }, "Pitch Sequencer A Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "pitch_seq_a_release", 1 }, "Pitch Sequencer A Release",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_b_enabled", 1 }, "Pitch Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "pitch_seq_b_attack", 1 }, "Pitch Sequencer B Attack",
        0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "pitch_seq_b_release", 1 }, "Pitch Sequencer B Release",
        0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { pitchStepParameterID (bank, step), 1 },
                "Pitch Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "pitch_serial_profile", 1 }, "Pitch Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "pitch_start_step", 1 }, "Pitch Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "pitch_end_step", 1 }, "Pitch End Step", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_a_bipolar", 1 }, "Pitch Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_b_bipolar", 1 }, "Pitch Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "pitch_sequence_mode", 1 }, "Pitch Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Pitch Sequencer A " : "Pitch Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::pitchShift);
             index <= static_cast<int> (seqwencer::ModulationTarget::pitchMix);
             ++index)
        {
            const auto target = static_cast<seqwencer::ModulationTarget> (index);
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    return layout;
}

void SeqwencerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    gateFreeRunningPhase = 0.0;
    phiFreeRunningPhase = 0.0;
    delayFreeRunningPhase = 0.0;
    reverbFreeRunningPhase = 0.0;
    panFreeRunningPhase = 0.0;
    filterFreeRunningPhase = 0.0;
    pitchFreeRunningPhase = 0.0;
    previousHostPpq = 0.0;
    previousHostTimeInSamples = 0;
    previousHostPpqValid = false;
    previousHostTimeInSamplesValid = false;
    notePhraseTracker.reset();
    smoothedGain = 1.0f;
    smoothedNoiseGateGain = 1.0f;
    noiseGateHoldSamplesRemaining = 0;
    noiseGateIsOpen = true;
    gateActiveStepA.store (0);
    gateActiveStepB.store (-1);
    phiActiveStepA.store (0);
    phiActiveStepB.store (-1);
    delayActiveStepA.store (0);
    delayActiveStepB.store (-1);
    reverbActiveStepA.store (0);
    reverbActiveStepB.store (-1);
    panActiveStepA.store (0);
    panActiveStepB.store (-1);
    filterActiveStepA.store (0);
    filterActiveStepB.store (-1);
    pitchActiveStepA.store (0);
    pitchActiveStepB.store (-1);
    const auto delayBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 2.05)) + 4;
    delayBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                         delayBufferLength, false, true, false);
    delayBuffer.clear();
    delayWritePosition = 0;
    delayWasActive = false;
    smoothedDelaySamples.reset (currentSampleRate, 0.025);
    smoothedDelaySamples.setCurrentAndTargetValue (
        static_cast<float> (0.375 * currentSampleRate));
    reverbProcessor.setSampleRate (currentSampleRate);
    reverbProcessor.reset();
    reverbWasActive = false;
    juce::ignoreUnused (samplesPerBlock);
    resetFilterProcessor();
    filterWasActive = false;
    const auto pitchBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 0.10)) + 8;
    pitchBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                         pitchBufferLength, false, true, false);
    pitchBuffer.clear();
    pitchWritePosition = 0;
    pitchReadPhase = 0.0;
    pitchWasActive = false;
}

void SeqwencerAudioProcessor::releaseResources()
{
    notePhraseTracker.reset();
    delayBuffer.clear();
    delayWritePosition = 0;
    delayWasActive = false;
    reverbProcessor.reset();
    reverbWasActive = false;
    resetFilterProcessor();
    filterWasActive = false;
    pitchBuffer.clear();
    pitchWritePosition = 0;
    pitchReadPhase = 0.0;
    pitchWasActive = false;
}

void SeqwencerAudioProcessor::resetFilterProcessor() noexcept
{
    for (auto& state : filterStates)
        state = {};
}

float SeqwencerAudioProcessor::processFilterSample (
    int channel, float input, float cutoffHz, float resonance,
    int type) noexcept
{
    auto& state = filterStates[static_cast<std::size_t> (
        juce::jlimit (0, static_cast<int> (filterStates.size()) - 1,
                      channel))];
    const auto safeSampleRate = juce::jmax (1.0, currentSampleRate);
    const auto maximumCutoff = juce::jmax (
        20.0, juce::jmin (20000.0, safeSampleRate * 0.45));
    const auto safeCutoff = juce::jlimit (
        20.0, maximumCutoff, static_cast<double> (cutoffHz));
    const auto g = std::tan (
        juce::MathConstants<double>::pi * safeCutoff / safeSampleRate);
    const auto k = 1.0 / juce::jlimit (
        0.10, 10.0, static_cast<double> (resonance));
    const auto a1 = 1.0 / (1.0 + g * (g + k));
    const auto a2 = g * a1;
    const auto a3 = g * a2;
    const auto v3 = static_cast<double> (input) - state.integrator2;
    const auto bandPass = a1 * state.integrator1 + a2 * v3;
    const auto lowPass = state.integrator2
                       + a2 * state.integrator1 + a3 * v3;
    state.integrator1 = 2.0 * bandPass - state.integrator1;
    state.integrator2 = 2.0 * lowPass - state.integrator2;
    const auto highPass = static_cast<double> (input)
                        - k * bandPass - lowPass;

    switch (juce::jlimit (0, 4, type))
    {
        case 1:  return static_cast<float> (highPass);
        case 2:  return static_cast<float> (bandPass);
        case 3:  return static_cast<float> (lowPass + highPass);
        case 4:  return static_cast<float> (lowPass - highPass);
        default: return static_cast<float> (lowPass);
    }
}

bool SeqwencerAudioProcessor::isBusesLayoutSupported (
    const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (output == juce::AudioChannelSet::mono()
            || output == juce::AudioChannelSet::stereo());
}

seqwencer::Pattern SeqwencerAudioProcessor::readPattern (int bank,
    seqwencer::ModulationTarget target) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? stepsA : stepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::targetValueFromCanonical (canonical, target);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readPhiPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? phiStepsA : phiStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readDelayPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? delayStepsA : delayStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readReverbPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? reverbStepsA : reverbStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readPanPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? panStepsA : panStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 0.5f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readFilterPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? filterStepsA : filterStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readPitchPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? pitchStepsA : pitchStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::GateModePattern SeqwencerAudioProcessor::readGateModes (
    int bank) const noexcept
{
    seqwencer::GateModePattern result {};
    const auto& source = bank == 0 ? gateModesA : gateModesB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto choice = source[i] != nullptr
            ? source[i]->load()
            : static_cast<float> (seqwencer::GateStepMode::longStep);
        result[i] = seqwencer::gateStepModeFromChoice (choice);
    }
    return result;
}

void SeqwencerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    std::vector<int> phraseStartSamples;

    for (const auto metadata : midiMessages)
    {
        const auto& message = metadata.getMessage();
        if (message.isNoteOn())
        {
            if (notePhraseTracker.noteOn (message.getChannel(),
                                          message.getNoteNumber()))
            {
                phraseStartSamples.push_back (juce::jlimit (
                    0,
                    juce::jmax (0, buffer.getNumSamples() - 1),
                    metadata.samplePosition));
            }
        }
        else if (message.isNoteOff())
        {
            notePhraseTracker.noteOff (message.getChannel(),
                                       message.getNoteNumber());
        }
        else if (message.isController()
                 && (message.getControllerNumber() == 120
                     || message.getControllerNumber() == 123))
        {
            notePhraseTracker.allNotesOff (message.getChannel());
        }

        if (message.isSysEx()
            && seqwencer_bridge::decodePhiPresence (
                reinterpret_cast<const std::uint8_t*> (message.getSysExData()),
                static_cast<std::size_t> (message.getSysExDataSize())))
        {
            phiHostPresent.store (true, std::memory_order_release);
        }
    }

    const auto gateAttackA = juce::jlimit (
        0.0f, 1.0f, seqAAttack != nullptr ? seqAAttack->load() : 0.0f);
    const auto gateReleaseA = juce::jlimit (
        0.0f, 1.0f, seqARelease != nullptr ? seqARelease->load() : 0.0f);
    const auto gateAttackB = juce::jlimit (
        0.0f, 1.0f, seqBAttack != nullptr ? seqBAttack->load() : 0.0f);
    const auto gateReleaseB = juce::jlimit (
        0.0f, 1.0f, seqBRelease != nullptr ? seqBRelease->load() : 0.0f);
    const auto gateLinked = playbackMode != nullptr
                         && playbackMode->load() >= 0.5f;
    const auto gateUsesProfileB = serialProfile != nullptr
                               && serialProfile->load() >= 0.5f;
    const auto gateSerialAttack = gateUsesProfileB
        ? gateAttackB : gateAttackA;
    const auto gateSerialRelease = gateUsesProfileB
        ? gateReleaseB : gateReleaseA;
    // Gate is a unipolar destination. Bipolar-positive steps therefore retain
    // their ordinary 0..100% Gate levels, while negative steps are heard as
    // 0%. Their signed canonical values remain stored for bipolar targets.
    const auto gatePatternA = readPattern (
        0, seqwencer::ModulationTarget::gateLevel);
    const auto gatePatternB = readPattern (
        1, seqwencer::ModulationTarget::gateLevel);
    const auto gateCanonicalPatternA = readPattern (
        0, seqwencer::ModulationTarget::noiseGateThreshold);
    const auto gateCanonicalPatternB = readPattern (
        1, seqwencer::ModulationTarget::noiseGateThreshold);
    const auto gateBipolarA = seqABipolar != nullptr
                           && seqABipolar->load() >= 0.5f;
    const auto gateBipolarB = seqBBipolar != nullptr
                           && seqBBipolar->load() >= 0.5f;
    const auto gateSerialBipolar = gateUsesProfileB
        ? gateBipolarB : gateBipolarA;
    const auto gateModesForA = readGateModes (0);
    const auto gateModesForB = readGateModes (1);
    const auto gateRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (rate != nullptr ? rate->load() : 6.0f)));
    const auto gateStepBeats = seqwencer::beatsForRate (gateRateIndex);
    const auto gateTraversalMode = seqwencer::sequenceModeFromChoice (
        sequenceMode != nullptr ? sequenceMode->load() : 0.0f);
    const auto gateRetriggersFromPlayedNotes =
        gateTraversalMode == seqwencer::SequenceMode::played;

    const auto phiAttackAValue = juce::jlimit (
        0.0f, 1.0f, phiSeqAAttack != nullptr ? phiSeqAAttack->load() : 0.0f);
    const auto phiReleaseAValue = juce::jlimit (
        0.0f, 1.0f, phiSeqARelease != nullptr ? phiSeqARelease->load() : 0.0f);
    const auto phiAttackBValue = juce::jlimit (
        0.0f, 1.0f, phiSeqBAttack != nullptr ? phiSeqBAttack->load() : 0.0f);
    const auto phiReleaseBValue = juce::jlimit (
        0.0f, 1.0f, phiSeqBRelease != nullptr ? phiSeqBRelease->load() : 0.0f);
    const auto phiLinked = phiPlaybackMode != nullptr
                        && phiPlaybackMode->load() >= 0.5f;
    const auto phiUsesProfileB = phiSerialProfile != nullptr
                              && phiSerialProfile->load() >= 0.5f;
    const auto phiSerialAttack = phiUsesProfileB
        ? phiAttackBValue : phiAttackAValue;
    const auto phiSerialRelease = phiUsesProfileB
        ? phiReleaseBValue : phiReleaseAValue;
    const auto phiBipolarAValue = phiSeqABipolar != nullptr
                               && phiSeqABipolar->load() >= 0.5f;
    const auto phiBipolarBValue = phiSeqBBipolar != nullptr
                               && phiSeqBBipolar->load() >= 0.5f;
    const auto phiSerialBipolar = phiUsesProfileB
        ? phiBipolarBValue : phiBipolarAValue;
    const auto phiPatternA = readPhiPattern (
        0, phiLinked ? phiSerialBipolar : phiBipolarAValue);
    const auto phiPatternB = readPhiPattern (
        1, phiLinked ? phiSerialBipolar : phiBipolarBValue);
    const auto phiRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            phiRate != nullptr ? phiRate->load() : 6.0f)));
    const auto phiStepBeats = seqwencer::beatsForRate (phiRateIndex);
    const auto phiTraversalMode = seqwencer::sequenceModeFromChoice (
        phiSequenceMode != nullptr ? phiSequenceMode->load() : 0.0f);
    const auto phiRetriggersFromPlayedNotes =
        phiTraversalMode == seqwencer::SequenceMode::played;

    const auto delayAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        delaySeqAAttack != nullptr ? delaySeqAAttack->load() : 0.0f);
    const auto delayReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        delaySeqARelease != nullptr ? delaySeqARelease->load() : 0.0f);
    const auto delayAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        delaySeqBAttack != nullptr ? delaySeqBAttack->load() : 0.0f);
    const auto delayReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        delaySeqBRelease != nullptr ? delaySeqBRelease->load() : 0.0f);
    const auto delayLinked = delayPlaybackMode != nullptr
                          && delayPlaybackMode->load() >= 0.5f;
    const auto delayUsesProfileB = delaySerialProfile != nullptr
                                && delaySerialProfile->load() >= 0.5f;
    const auto delaySerialAttack = delayUsesProfileB
        ? delayAttackBValue : delayAttackAValue;
    const auto delaySerialRelease = delayUsesProfileB
        ? delayReleaseBValue : delayReleaseAValue;
    const auto delayBipolarAValue = delaySeqABipolar != nullptr
                                 && delaySeqABipolar->load() >= 0.5f;
    const auto delayBipolarBValue = delaySeqBBipolar != nullptr
                                 && delaySeqBBipolar->load() >= 0.5f;
    const auto delaySerialBipolar = delayUsesProfileB
        ? delayBipolarBValue : delayBipolarAValue;
    const auto delayUnipolarPatternA = readDelayPattern (0, false);
    const auto delayUnipolarPatternB = readDelayPattern (1, false);
    const auto delayCanonicalPatternA = readDelayPattern (0, true);
    const auto delayCanonicalPatternB = readDelayPattern (1, true);
    const auto delayRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            delayRate != nullptr ? delayRate->load() : 6.0f)));
    const auto delayStepBeats = seqwencer::beatsForRate (delayRateIndex);
    const auto delayTraversalMode = seqwencer::sequenceModeFromChoice (
        delaySequenceMode != nullptr ? delaySequenceMode->load() : 0.0f);
    const auto delayRetriggersFromPlayedNotes =
        delayTraversalMode == seqwencer::SequenceMode::played;

    const auto reverbAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        reverbSeqAAttack != nullptr ? reverbSeqAAttack->load() : 0.0f);
    const auto reverbReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        reverbSeqARelease != nullptr ? reverbSeqARelease->load() : 0.0f);
    const auto reverbAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        reverbSeqBAttack != nullptr ? reverbSeqBAttack->load() : 0.0f);
    const auto reverbReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        reverbSeqBRelease != nullptr ? reverbSeqBRelease->load() : 0.0f);
    const auto reverbLinked = reverbPlaybackMode != nullptr
                           && reverbPlaybackMode->load() >= 0.5f;
    const auto reverbUsesProfileB = reverbSerialProfile != nullptr
                                 && reverbSerialProfile->load() >= 0.5f;
    const auto reverbSerialAttack = reverbUsesProfileB
        ? reverbAttackBValue : reverbAttackAValue;
    const auto reverbSerialRelease = reverbUsesProfileB
        ? reverbReleaseBValue : reverbReleaseAValue;
    const auto reverbBipolarAValue = reverbSeqABipolar != nullptr
                                  && reverbSeqABipolar->load() >= 0.5f;
    const auto reverbBipolarBValue = reverbSeqBBipolar != nullptr
                                  && reverbSeqBBipolar->load() >= 0.5f;
    const auto reverbSerialBipolar = reverbUsesProfileB
        ? reverbBipolarBValue : reverbBipolarAValue;
    const auto reverbUnipolarPatternA = readReverbPattern (0, false);
    const auto reverbUnipolarPatternB = readReverbPattern (1, false);
    const auto reverbCanonicalPatternA = readReverbPattern (0, true);
    const auto reverbCanonicalPatternB = readReverbPattern (1, true);
    const auto reverbRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            reverbRate != nullptr ? reverbRate->load() : 6.0f)));
    const auto reverbStepBeats = seqwencer::beatsForRate (reverbRateIndex);
    const auto reverbTraversalMode = seqwencer::sequenceModeFromChoice (
        reverbSequenceMode != nullptr ? reverbSequenceMode->load() : 0.0f);
    const auto reverbRetriggersFromPlayedNotes =
        reverbTraversalMode == seqwencer::SequenceMode::played;

    const auto panAttackAValue = juce::jlimit (
        0.0f, 1.0f, panSeqAAttack != nullptr ? panSeqAAttack->load() : 0.0f);
    const auto panReleaseAValue = juce::jlimit (
        0.0f, 1.0f, panSeqARelease != nullptr ? panSeqARelease->load() : 0.0f);
    const auto panAttackBValue = juce::jlimit (
        0.0f, 1.0f, panSeqBAttack != nullptr ? panSeqBAttack->load() : 0.0f);
    const auto panReleaseBValue = juce::jlimit (
        0.0f, 1.0f, panSeqBRelease != nullptr ? panSeqBRelease->load() : 0.0f);
    const auto panLinked = panPlaybackMode != nullptr
                        && panPlaybackMode->load() >= 0.5f;
    const auto panUsesProfileB = panSerialProfile != nullptr
                              && panSerialProfile->load() >= 0.5f;
    const auto panSerialAttack = panUsesProfileB
        ? panAttackBValue : panAttackAValue;
    const auto panSerialRelease = panUsesProfileB
        ? panReleaseBValue : panReleaseAValue;
    const auto panBipolarAValue = panSeqABipolar == nullptr
                               || panSeqABipolar->load() >= 0.5f;
    const auto panBipolarBValue = panSeqBBipolar == nullptr
                               || panSeqBBipolar->load() >= 0.5f;
    const auto panSerialBipolar = panUsesProfileB
        ? panBipolarBValue : panBipolarAValue;
    const auto panUnipolarPatternA = readPanPattern (0, false);
    const auto panUnipolarPatternB = readPanPattern (1, false);
    const auto panCanonicalPatternA = readPanPattern (0, true);
    const auto panCanonicalPatternB = readPanPattern (1, true);
    const auto panRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            panRate != nullptr ? panRate->load() : 6.0f)));
    const auto panStepBeats = seqwencer::beatsForRate (panRateIndex);
    const auto panTraversalMode = seqwencer::sequenceModeFromChoice (
        panSequenceMode != nullptr ? panSequenceMode->load() : 0.0f);
    const auto panRetriggersFromPlayedNotes =
        panTraversalMode == seqwencer::SequenceMode::played;

    const auto filterAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        filterSeqAAttack != nullptr ? filterSeqAAttack->load() : 0.0f);
    const auto filterReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        filterSeqARelease != nullptr ? filterSeqARelease->load() : 0.0f);
    const auto filterAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        filterSeqBAttack != nullptr ? filterSeqBAttack->load() : 0.0f);
    const auto filterReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        filterSeqBRelease != nullptr ? filterSeqBRelease->load() : 0.0f);
    const auto filterLinked = filterPlaybackMode != nullptr
                           && filterPlaybackMode->load() >= 0.5f;
    const auto filterUsesProfileB = filterSerialProfile != nullptr
                                 && filterSerialProfile->load() >= 0.5f;
    const auto filterSerialAttack = filterUsesProfileB
        ? filterAttackBValue : filterAttackAValue;
    const auto filterSerialRelease = filterUsesProfileB
        ? filterReleaseBValue : filterReleaseAValue;
    const auto filterBipolarAValue = filterSeqABipolar != nullptr
                                  && filterSeqABipolar->load() >= 0.5f;
    const auto filterBipolarBValue = filterSeqBBipolar != nullptr
                                  && filterSeqBBipolar->load() >= 0.5f;
    const auto filterSerialBipolar = filterUsesProfileB
        ? filterBipolarBValue : filterBipolarAValue;
    const auto filterUnipolarPatternA = readFilterPattern (0, false);
    const auto filterUnipolarPatternB = readFilterPattern (1, false);
    const auto filterCanonicalPatternA = readFilterPattern (0, true);
    const auto filterCanonicalPatternB = readFilterPattern (1, true);
    const auto filterRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            filterRate != nullptr ? filterRate->load() : 6.0f)));
    const auto filterStepBeats = seqwencer::beatsForRate (filterRateIndex);
    const auto filterTraversalMode = seqwencer::sequenceModeFromChoice (
        filterSequenceMode != nullptr ? filterSequenceMode->load() : 0.0f);
    const auto filterRetriggersFromPlayedNotes =
        filterTraversalMode == seqwencer::SequenceMode::played;

    const auto pitchAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        pitchSeqAAttack != nullptr ? pitchSeqAAttack->load() : 0.0f);
    const auto pitchReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        pitchSeqARelease != nullptr ? pitchSeqARelease->load() : 0.0f);
    const auto pitchAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        pitchSeqBAttack != nullptr ? pitchSeqBAttack->load() : 0.0f);
    const auto pitchReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        pitchSeqBRelease != nullptr ? pitchSeqBRelease->load() : 0.0f);
    const auto pitchLinked = pitchPlaybackMode != nullptr
                          && pitchPlaybackMode->load() >= 0.5f;
    const auto pitchUsesProfileB = pitchSerialProfile != nullptr
                                && pitchSerialProfile->load() >= 0.5f;
    const auto pitchSerialAttack = pitchUsesProfileB
        ? pitchAttackBValue : pitchAttackAValue;
    const auto pitchSerialRelease = pitchUsesProfileB
        ? pitchReleaseBValue : pitchReleaseAValue;
    const auto pitchBipolarAValue = pitchSeqABipolar != nullptr
                                 && pitchSeqABipolar->load() >= 0.5f;
    const auto pitchBipolarBValue = pitchSeqBBipolar != nullptr
                                 && pitchSeqBBipolar->load() >= 0.5f;
    const auto pitchSerialBipolar = pitchUsesProfileB
        ? pitchBipolarBValue : pitchBipolarAValue;
    const auto pitchUnipolarPatternA = readPitchPattern (0, false);
    const auto pitchUnipolarPatternB = readPitchPattern (1, false);
    const auto pitchCanonicalPatternA = readPitchPattern (0, true);
    const auto pitchCanonicalPatternB = readPitchPattern (1, true);
    const auto pitchRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            pitchRate != nullptr ? pitchRate->load() : 6.0f)));
    const auto pitchStepBeats = seqwencer::beatsForRate (pitchRateIndex);
    const auto pitchTraversalMode = seqwencer::sequenceModeFromChoice (
        pitchSequenceMode != nullptr ? pitchSequenceMode->load() : 0.0f);
    const auto pitchRetriggersFromPlayedNotes =
        pitchTraversalMode == seqwencer::SequenceMode::played;
    auto hostBpm = 120.0;
    auto hostPpq = 0.0;
    auto hostPositionAvailable = false;
    auto hostTimeInSamplesAvailable = false;
    auto hostPpqChanged = false;
    auto hostTimeInSamplesChanged = false;
    auto hostReportsPlaying = true;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                hostBpm = juce::jmax (1.0, *bpm);
            if (const auto ppq = position->getPpqPosition())
            {
                hostPpq = *ppq;
                hostPositionAvailable = true;
                hostPpqChanged = previousHostPpqValid
                              && std::abs (hostPpq - previousHostPpq) > 1.0e-9;
                previousHostPpq = hostPpq;
                previousHostPpqValid = true;
            }
            else
            {
                previousHostPpqValid = false;
            }

            if (const auto timeInSamples = position->getTimeInSamples())
            {
                hostTimeInSamplesAvailable = true;
                hostTimeInSamplesChanged = previousHostTimeInSamplesValid
                                        && *timeInSamples
                                               != previousHostTimeInSamples;
                previousHostTimeInSamples = *timeInSamples;
                previousHostTimeInSamplesValid = true;
            }
            else
            {
                previousHostTimeInSamplesValid = false;
            }

            hostReportsPlaying = position->getIsPlaying();
        }
        else
        {
            previousHostPpqValid = false;
            previousHostTimeInSamplesValid = false;
        }
    }
    else
    {
        previousHostPpqValid = false;
        previousHostTimeInSamplesValid = false;
    }

    const auto phiTimelineNeedsFreeRun =
        seqwencer::stalledPhiTimelineShouldFreeRun (
            phiHostPresent.load (std::memory_order_acquire),
            hostReportsPlaying,
            hostPositionAvailable,
            hostPpqChanged,
            hostTimeInSamplesAvailable,
            hostTimeInSamplesChanged);
    const auto hostTimelineAdvancing =
        seqwencer::hostTimelineShouldAdvance (
            hostReportsPlaying,
            hostPositionAvailable,
            hostPpqChanged,
            hostTimeInSamplesAvailable,
            hostTimeInSamplesChanged)
        || phiTimelineNeedsFreeRun;

    const auto quarterNotesPerSample = hostBpm / (60.0 * currentSampleRate);
    const auto hostSyncEnabled = syncToHost != nullptr
                              && syncToHost->load() >= 0.5f;
    const auto gateUseHostPosition = hostSyncEnabled
                                  && hostPositionAvailable
                                  && ! phiTimelineNeedsFreeRun
                                  && ! gateRetriggersFromPlayedNotes;
    const auto phiUseHostPosition = hostSyncEnabled
                                 && hostPositionAvailable
                                 && ! phiTimelineNeedsFreeRun
                                 && ! phiRetriggersFromPlayedNotes;
    const auto delayUseHostPosition = hostSyncEnabled
                                   && hostPositionAvailable
                                   && ! phiTimelineNeedsFreeRun
                                   && ! delayRetriggersFromPlayedNotes;
    const auto reverbUseHostPosition = hostSyncEnabled
                                    && hostPositionAvailable
                                    && ! phiTimelineNeedsFreeRun
                                    && ! reverbRetriggersFromPlayedNotes;
    const auto panUseHostPosition = hostSyncEnabled
                                 && hostPositionAvailable
                                 && ! phiTimelineNeedsFreeRun
                                 && ! panRetriggersFromPlayedNotes;
    const auto filterUseHostPosition = hostSyncEnabled
                                    && hostPositionAvailable
                                    && ! phiTimelineNeedsFreeRun
                                    && ! filterRetriggersFromPlayedNotes;
    const auto pitchUseHostPosition = hostSyncEnabled
                                   && hostPositionAvailable
                                   && ! phiTimelineNeedsFreeRun
                                   && ! pitchRetriggersFromPlayedNotes;
    const auto gateRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (startStep != nullptr ? startStep->load() : 1.0f)),
        static_cast<int> (std::lround (endStep != nullptr ? endStep->load() : 64.0f)),
        gateLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto phiRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            phiStartStep != nullptr ? phiStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            phiEndStep != nullptr ? phiEndStep->load() : 64.0f)),
        phiLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto delayRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            delayStartStep != nullptr ? delayStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            delayEndStep != nullptr ? delayEndStep->load() : 64.0f)),
        delayLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto reverbRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            reverbStartStep != nullptr ? reverbStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            reverbEndStep != nullptr ? reverbEndStep->load() : 64.0f)),
        reverbLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto panRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            panStartStep != nullptr ? panStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            panEndStep != nullptr ? panEndStep->load() : 64.0f)),
        panLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto filterRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            filterStartStep != nullptr ? filterStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            filterEndStep != nullptr ? filterEndStep->load() : 64.0f)),
        filterLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto pitchRange = seqwencer::makeStepRange (
        static_cast<int> (std::lround (
            pitchStartStep != nullptr ? pitchStartStep->load() : 1.0f)),
        static_cast<int> (std::lround (
            pitchEndStep != nullptr ? pitchEndStep->load() : 64.0f)),
        pitchLinked ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
    const auto gateCycleLength = seqwencer::sequenceCycleLength (
        gateRange.length(), gateTraversalMode);
    const auto phiCycleLength = seqwencer::sequenceCycleLength (
        phiRange.length(), phiTraversalMode);
    const auto delayCycleLength = seqwencer::sequenceCycleLength (
        delayRange.length(), delayTraversalMode);
    const auto reverbCycleLength = seqwencer::sequenceCycleLength (
        reverbRange.length(), reverbTraversalMode);
    const auto panCycleLength = seqwencer::sequenceCycleLength (
        panRange.length(), panTraversalMode);
    const auto filterCycleLength = seqwencer::sequenceCycleLength (
        filterRange.length(), filterTraversalMode);
    const auto pitchCycleLength = seqwencer::sequenceCycleLength (
        pitchRange.length(), pitchTraversalMode);
    const auto gatePhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * gateStepBeats * currentSampleRate / hostBpm);
    const auto phiPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * phiStepBeats * currentSampleRate / hostBpm);
    const auto delayPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * delayStepBeats * currentSampleRate / hostBpm);
    const auto reverbPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * reverbStepBeats * currentSampleRate / hostBpm);
    const auto panPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * panStepBeats * currentSampleRate / hostBpm);
    const auto filterPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * filterStepBeats * currentSampleRate / hostBpm);
    const auto pitchPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * pitchStepBeats * currentSampleRate / hostBpm);
    const auto shouldBypass = bypass != nullptr && bypass->load() >= 0.5f;
    const auto shouldGate = gateEnabled != nullptr && gateEnabled->load() >= 0.5f;
    const auto base = juce::jlimit (
        0.0f, 1.0f, gateBase != nullptr ? gateBase->load() : 1.0f);
    const auto baseDepth = juce::jlimit (
        0.0f, 1.0f, gateDepth != nullptr ? gateDepth->load() : 1.0f);
    const auto baseShortLength = juce::jlimit (
        0.10f, 0.60f,
        shortGateLength != nullptr ? shortGateLength->load()
                                   : seqwencer::shortGateOpenFraction);
    const auto baseLongLength = juce::jlimit (
        0.65f, 0.95f,
        longGateLength != nullptr ? longGateLength->load()
                                  : seqwencer::longGateOpenFraction);
    const auto shouldNoiseGate = noiseGateEnabled != nullptr
                              && noiseGateEnabled->load() >= 0.5f;
    const auto baseNoiseThresholdDb = juce::jlimit (
        -80.0f, 0.0f,
        noiseGateThreshold != nullptr ? noiseGateThreshold->load() : -60.0f);
    const auto baseNoiseAttackMs = juce::jlimit (
        0.1f, 100.0f,
        noiseGateAttack != nullptr ? noiseGateAttack->load() : 5.0f);
    const auto baseNoiseHoldMs = juce::jlimit (
        0.0f, 500.0f,
        noiseGateHold != nullptr ? noiseGateHold->load() : 50.0f);
    const auto baseNoiseReleaseMs = juce::jlimit (
        5.0f, 1000.0f,
        noiseGateRelease != nullptr ? noiseGateRelease->load() : 100.0f);
    const auto baseNoiseRange = juce::jlimit (
        0.0f, 1.0f,
        noiseGateRange != nullptr ? noiseGateRange->load() : 1.0f);
    const auto shouldDelay = delayEnabled != nullptr
                          && delayEnabled->load() >= 0.5f;
    const auto baseDelayTimeMs = juce::jlimit (
        10.0f, 2000.0f, delayTime != nullptr ? delayTime->load() : 375.0f);
    const auto baseDelayFeedback = juce::jlimit (
        0.0f, 0.95f,
        delayFeedback != nullptr ? delayFeedback->load() : 0.35f);
    const auto baseDelayMix = juce::jlimit (
        0.0f, 1.0f, delayMix != nullptr ? delayMix->load() : 0.25f);
    const auto shouldReverb = reverbEnabled != nullptr
                           && reverbEnabled->load() >= 0.5f;
    const auto baseReverbSize = juce::jlimit (
        0.0f, 1.0f, reverbSize != nullptr ? reverbSize->load() : 0.50f);
    const auto baseReverbDamping = juce::jlimit (
        0.0f, 1.0f, reverbDamping != nullptr ? reverbDamping->load() : 0.50f);
    const auto baseReverbWidth = juce::jlimit (
        0.0f, 1.0f, reverbWidth != nullptr ? reverbWidth->load() : 1.0f);
    const auto baseReverbMix = juce::jlimit (
        0.0f, 1.0f, reverbMix != nullptr ? reverbMix->load() : 0.25f);
    const auto shouldPan = panEnabled != nullptr && panEnabled->load() >= 0.5f;
    const auto shouldFilter = filterEnabled != nullptr
                           && filterEnabled->load() >= 0.5f;
    const auto basePanNormalised = juce::jlimit (
        0.0f, 1.0f,
        0.5f * ((panPosition != nullptr ? panPosition->load() : 0.0f) + 1.0f));
    const auto baseFilterType = juce::jlimit (
        0, 4, static_cast<int> (std::lround (
            filterType != nullptr ? filterType->load() : 0.0f)));
    const auto baseFilterCutoff = juce::jlimit (
        20.0f, 20000.0f,
        filterCutoff != nullptr ? filterCutoff->load() : 20000.0f);
    const auto baseFilterResonance = juce::jlimit (
        0.10f, 10.0f,
        filterResonance != nullptr ? filterResonance->load() : 0.707f);
    const auto baseFilterMix = juce::jlimit (
        0.0f, 1.0f, filterMix != nullptr ? filterMix->load() : 1.0f);
    const auto shouldPitch = pitchEnabled != nullptr
                          && pitchEnabled->load() >= 0.5f;
    const auto basePitchShift = juce::jlimit (
        -24.0f, 24.0f, pitchShift != nullptr ? pitchShift->load() : 0.0f);
    const auto basePitchMix = juce::jlimit (
        0.0f, 1.0f, pitchMix != nullptr ? pitchMix->load() : 1.0f);
    const auto gateAIsEnabled = seqAEnabled == nullptr
                             || seqAEnabled->load() >= 0.5f;
    const auto gateBIsEnabled = seqBEnabled != nullptr
                             && seqBEnabled->load() >= 0.5f;
    const auto phiAIsEnabled = phiSeqAEnabled == nullptr
                            || phiSeqAEnabled->load() >= 0.5f;
    const auto phiBIsEnabled = phiSeqBEnabled != nullptr
                            && phiSeqBEnabled->load() >= 0.5f;
    const auto delayAIsEnabled = delaySeqAEnabled == nullptr
                              || delaySeqAEnabled->load() >= 0.5f;
    const auto delayBIsEnabled = delaySeqBEnabled != nullptr
                              && delaySeqBEnabled->load() >= 0.5f;
    const auto reverbAIsEnabled = reverbSeqAEnabled == nullptr
                               || reverbSeqAEnabled->load() >= 0.5f;
    const auto reverbBIsEnabled = reverbSeqBEnabled != nullptr
                               && reverbSeqBEnabled->load() >= 0.5f;
    const auto panAIsEnabled = panSeqAEnabled == nullptr
                            || panSeqAEnabled->load() >= 0.5f;
    const auto panBIsEnabled = panSeqBEnabled != nullptr
                            && panSeqBEnabled->load() >= 0.5f;
    const auto filterAIsEnabled = filterSeqAEnabled == nullptr
                               || filterSeqAEnabled->load() >= 0.5f;
    const auto filterBIsEnabled = filterSeqBEnabled != nullptr
                               && filterSeqBEnabled->load() >= 0.5f;
    const auto pitchAIsEnabled = pitchSeqAEnabled == nullptr
                              || pitchSeqAEnabled->load() >= 0.5f;
    const auto pitchBIsEnabled = pitchSeqBEnabled != nullptr
                              && pitchSeqBEnabled->load() >= 0.5f;
    const auto targetIsActive = [this] (
        int bank, seqwencer::ModulationTarget target)
    {
        const auto index = static_cast<int> (target) - 1;
        if (index < 0 || index >= seqwencer::modulationTargetCount)
            return false;

        const auto arrayIndex = static_cast<std::size_t> (index);
        const auto* assigned = bank == 0 ? targetsAssignedA[arrayIndex]
                                         : targetsAssignedB[arrayIndex];
        const auto* enabled = bank == 0 ? targetsEnabledA[arrayIndex]
                                        : targetsEnabledB[arrayIndex];
        return assigned != nullptr && assigned->load() >= 0.5f
            && (enabled == nullptr || enabled->load() >= 0.5f);
    };
    seqwencer::Pattern unityGatePattern {};
    unityGatePattern.fill (1.0f);
    const auto volumeAIsAssigned = targetIsActive (
        0, seqwencer::ModulationTarget::gateLevel);
    const auto volumeBIsAssigned = targetIsActive (
        1, seqwencer::ModulationTarget::gateLevel);
    const auto& gateLevelPatternA = volumeAIsAssigned
        ? gatePatternA : unityGatePattern;
    const auto& gateLevelPatternB = (gateLinked ? volumeAIsAssigned
                                                : volumeBIsAssigned)
        ? gatePatternB : unityGatePattern;
    const auto shouldSendPhiBridge = phiBridgeEnabled != nullptr
                                  && phiBridgeEnabled->load() >= 0.5f;
    auto lastBridgeValueA = 1.0f;
    auto lastBridgeValueB = 1.0f;
    auto lastSerialBridgeValue = 1.0f;

    // A half-millisecond safety slew prevents hard zero-crossing clicks when
    // Attack or Release is set to zero without masking their musical shape.
    const auto safetyCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / juce::jmax (1.0, 0.0005 * currentSampleRate)));
    const auto delayIsProcessing = shouldDelay && ! shouldBypass
                                && delayBuffer.getNumSamples() > 3;
    if (delayIsProcessing && ! delayWasActive)
    {
        delayBuffer.clear();
        delayWritePosition = 0;
        smoothedDelaySamples.setCurrentAndTargetValue (
            static_cast<float> (baseDelayTimeMs * 0.001 * currentSampleRate));
    }
    else if (! delayIsProcessing && delayWasActive)
    {
        delayBuffer.clear();
        delayWritePosition = 0;
    }
    delayWasActive = delayIsProcessing;
    const auto reverbIsProcessing = shouldReverb && ! shouldBypass;
    if (reverbIsProcessing && ! reverbWasActive)
        reverbProcessor.reset();
    else if (! reverbIsProcessing && reverbWasActive)
        reverbProcessor.reset();
    reverbWasActive = reverbIsProcessing;
    const auto gateIsProcessing = shouldGate && ! shouldBypass;
    const auto panIsProcessing = shouldPan && ! shouldBypass
                              && buffer.getNumChannels() >= 2;
    const auto filterIsProcessing = shouldFilter && ! shouldBypass;
    if (filterIsProcessing && ! filterWasActive)
        resetFilterProcessor();
    else if (! filterIsProcessing && filterWasActive)
        resetFilterProcessor();
    filterWasActive = filterIsProcessing;
    const auto pitchIsProcessing = shouldPitch && ! shouldBypass
                                 && pitchBuffer.getNumSamples() > 8;
    if (pitchIsProcessing != pitchWasActive)
    {
        pitchBuffer.clear();
        pitchWritePosition = 0;
        pitchReadPhase = 0.0;
    }
    pitchWasActive = pitchIsProcessing;
    if (! gateIsProcessing)
    {
        gateActiveStepA.store (-1);
        gateActiveStepB.store (-1);
    }
    if (! delayIsProcessing)
    {
        delayActiveStepA.store (-1);
        delayActiveStepB.store (-1);
    }
    if (! reverbIsProcessing)
    {
        reverbActiveStepA.store (-1);
        reverbActiveStepB.store (-1);
    }
    if (! panIsProcessing)
    {
        panActiveStepA.store (-1);
        panActiveStepB.store (-1);
    }
    if (! filterIsProcessing)
    {
        filterActiveStepA.store (-1);
        filterActiveStepB.store (-1);
    }
    if (! pitchIsProcessing)
    {
        pitchActiveStepA.store (-1);
        pitchActiveStepB.store (-1);
    }
    std::size_t nextPhraseStart = 0;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        while (nextPhraseStart < phraseStartSamples.size()
               && phraseStartSamples[nextPhraseStart] == sample)
        {
            if (gateRetriggersFromPlayedNotes)
                gateFreeRunningPhase = 0.0;
            if (phiRetriggersFromPlayedNotes)
                phiFreeRunningPhase = 0.0;
            if (delayRetriggersFromPlayedNotes)
                delayFreeRunningPhase = 0.0;
            if (reverbRetriggersFromPlayedNotes)
                reverbFreeRunningPhase = 0.0;
            if (panRetriggersFromPlayedNotes)
                panFreeRunningPhase = 0.0;
            if (filterRetriggersFromPlayedNotes)
                filterFreeRunningPhase = 0.0;
            if (pitchRetriggersFromPlayedNotes)
                pitchFreeRunningPhase = 0.0;
            ++nextPhraseStart;
        }

        auto gatePhase = gateFreeRunningPhase;
        if (gateUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            gatePhase = seqwencer::phaseFromQuarterNotes (
                ppq, gateStepBeats, gateCycleLength);
        }

        auto phiPhase = phiFreeRunningPhase;
        if (phiUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            phiPhase = seqwencer::phaseFromQuarterNotes (
                ppq, phiStepBeats, phiCycleLength);
        }

        auto delayPhase = delayFreeRunningPhase;
        if (delayUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            delayPhase = seqwencer::phaseFromQuarterNotes (
                ppq, delayStepBeats, delayCycleLength);
        }

        auto reverbPhase = reverbFreeRunningPhase;
        if (reverbUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            reverbPhase = seqwencer::phaseFromQuarterNotes (
                ppq, reverbStepBeats, reverbCycleLength);
        }

        auto panPhase = panFreeRunningPhase;
        if (panUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            panPhase = seqwencer::phaseFromQuarterNotes (
                ppq, panStepBeats, panCycleLength);
        }

        auto filterPhase = filterFreeRunningPhase;
        if (filterUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            filterPhase = seqwencer::phaseFromQuarterNotes (
                ppq, filterStepBeats, filterCycleLength);
        }

        auto pitchPhase = pitchFreeRunningPhase;
        if (pitchUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            pitchPhase = seqwencer::phaseFromQuarterNotes (
                ppq, pitchStepBeats, pitchCycleLength);
        }

        auto gateUnipolarA = 1.0f;
        auto gateUnipolarB = 1.0f;
        auto gateCanonicalA = 1.0f;
        auto gateCanonicalB = 1.0f;
        auto gateSerialUnipolar = 1.0f;
        auto gateSerialCanonical = 1.0f;

        if (gateIsProcessing && gateLinked)
        {
            gateSerialUnipolar = seqwencer::evaluateLinkedRange (
                gatePatternA, gatePatternB, gatePhase, gateRange,
                gateSerialAttack, gateSerialRelease,
                gateSerialAttack, gateSerialRelease,
                nullptr, nullptr, gateTraversalMode);
            gateSerialCanonical = seqwencer::evaluateLinkedRange (
                gateCanonicalPatternA, gateCanonicalPatternB,
                gatePhase, gateRange,
                gateSerialAttack, gateSerialRelease,
                gateSerialAttack, gateSerialRelease,
                nullptr, nullptr, gateTraversalMode);
        }
        else if (gateIsProcessing)
        {
            gateUnipolarA = seqwencer::evaluateBankRange (
                gatePatternA, gatePhase, gateRange,
                gateAttackA, gateReleaseA, nullptr, gateTraversalMode);
            gateUnipolarB = seqwencer::evaluateBankRange (
                gatePatternB, gatePhase, gateRange,
                gateAttackB, gateReleaseB, nullptr, gateTraversalMode);
            gateCanonicalA = seqwencer::evaluateBankRange (
                gateCanonicalPatternA, gatePhase, gateRange,
                gateAttackA, gateReleaseA, nullptr, gateTraversalMode);
            gateCanonicalB = seqwencer::evaluateBankRange (
                gateCanonicalPatternB, gatePhase, gateRange,
                gateAttackB, gateReleaseB, nullptr, gateTraversalMode);
        }

        const auto gateTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (gateLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return gateSerialBipolar
                           && seqwencer::targetSupportsBipolar (target)
                    ? 2.0f * gateSerialCanonical - 1.0f
                    : gateSerialUnipolar - 1.0f;
            }

            const auto useBipolarA = gateBipolarA
                                  && seqwencer::targetSupportsBipolar (target);
            const auto useBipolarB = gateBipolarB
                                  && seqwencer::targetSupportsBipolar (target);
            return seqwencer::combineParallelModulationDeviation (
                target,
                useBipolarA ? gateCanonicalA : gateUnipolarA,
                gateAIsEnabled && targetIsActive (0, target), target,
                useBipolarA,
                useBipolarB ? gateCanonicalB : gateUnipolarB,
                gateBIsEnabled && targetIsActive (1, target), target,
                useBipolarB, assigned);
        };

        bool depthIsAssigned = false;
        bool shortIsAssigned = false;
        bool longIsAssigned = false;
        const auto depthDeviation = gateTargetDeviation (
            seqwencer::ModulationTarget::gateDepth, &depthIsAssigned);
        const auto shortDeviation = gateTargetDeviation (
            seqwencer::ModulationTarget::shortGateLength, &shortIsAssigned);
        const auto longDeviation = gateTargetDeviation (
            seqwencer::ModulationTarget::longGateLength, &longIsAssigned);
        const auto depth = depthIsAssigned
            ? seqwencer::applyModulationDepth (baseDepth, depthDeviation, 1.0f)
            : baseDepth;
        const auto shortBaseNormalised = (baseShortLength - 0.10f) / 0.50f;
        const auto shortLength = shortIsAssigned
            ? 0.10f + 0.50f * seqwencer::applyModulationDepth (
                shortBaseNormalised, shortDeviation, 1.0f)
            : baseShortLength;
        const auto longBaseNormalised = (baseLongLength - 0.65f) / 0.30f;
        const auto longLength = longIsAssigned
            ? 0.65f + 0.30f * seqwencer::applyModulationDepth (
                longBaseNormalised, longDeviation, 1.0f)
            : baseLongLength;

        if (gateIsProcessing && gateLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            const auto value = seqwencer::evaluateGateLinkedRange (
                gateLevelPatternA, gateLevelPatternB,
                gateModesForA, gateModesForB,
                gatePhase, gateRange,
                gateSerialAttack, gateSerialRelease,
                gateSerialAttack, gateSerialRelease,
                &activeBank, &activeStep, shortLength, longLength,
                gateTraversalMode);
            gateActiveStepA.store (activeBank == 0 ? activeStep : -1);
            gateActiveStepB.store (activeBank == 1 ? activeStep : -1);
            const auto targetGain = shouldGate && ! shouldBypass
                ? seqwencer::applyModulationDepth (
                    base, value - 1.0f, depth)
                : 1.0f;
            smoothedGain += safetyCoefficient * (targetGain - smoothedGain);
        }
        else if (gateIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            const auto valueA = seqwencer::evaluateGateBankRange (
                gateLevelPatternA, gateModesForA, gatePhase, gateRange,
                gateAttackA, gateReleaseA, &stepA, shortLength, longLength,
                gateTraversalMode);
            const auto valueB = seqwencer::evaluateGateBankRange (
                gateLevelPatternB, gateModesForB, gatePhase, gateRange,
                gateAttackB, gateReleaseB, &stepB, shortLength, longLength,
                gateTraversalMode);
            gateActiveStepA.store (gateAIsEnabled ? stepA : -1);
            gateActiveStepB.store (gateBIsEnabled ? stepB : -1);

            auto combinedGate = 1.0f;
            if (gateAIsEnabled)
                combinedGate *= valueA;
            if (gateBIsEnabled)
                combinedGate *= valueB;
            const auto targetGain = shouldGate && ! shouldBypass
                ? seqwencer::applyModulationDepth (
                    base, combinedGate - 1.0f, depth)
                : 1.0f;
            smoothedGain += safetyCoefficient * (targetGain - smoothedGain);
        }
        else
        {
            smoothedGain += safetyCoefficient * (1.0f - smoothedGain);
        }

        if (phiLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            lastSerialBridgeValue = seqwencer::evaluateLinkedRange (
                phiPatternA, phiPatternB, phiPhase, phiRange,
                phiSerialAttack, phiSerialRelease,
                phiSerialAttack, phiSerialRelease,
                &activeBank, &activeStep, phiTraversalMode);
            phiActiveStepA.store (activeBank == 0 ? activeStep : -1);
            phiActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else
        {
            auto stepA = 0;
            auto stepB = 0;
            lastBridgeValueA = seqwencer::evaluateBankRange (
                phiPatternA, phiPhase, phiRange,
                phiAttackAValue, phiReleaseAValue,
                &stepA, phiTraversalMode);
            lastBridgeValueB = seqwencer::evaluateBankRange (
                phiPatternB, phiPhase, phiRange,
                phiAttackBValue, phiReleaseBValue,
                &stepB, phiTraversalMode);
            phiActiveStepA.store (phiAIsEnabled ? stepA : -1);
            phiActiveStepB.store (phiBIsEnabled ? stepB : -1);
        }

        auto delayUnipolarA = 1.0f;
        auto delayUnipolarB = 1.0f;
        auto delayCanonicalA = 1.0f;
        auto delayCanonicalB = 1.0f;
        auto delaySerialUnipolar = 1.0f;
        auto delaySerialCanonical = 1.0f;
        if (delayIsProcessing && delayLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            delaySerialUnipolar = seqwencer::evaluateLinkedRange (
                delayUnipolarPatternA, delayUnipolarPatternB,
                delayPhase, delayRange,
                delaySerialAttack, delaySerialRelease,
                delaySerialAttack, delaySerialRelease,
                &activeBank, &activeStep, delayTraversalMode);
            delaySerialCanonical = seqwencer::evaluateLinkedRange (
                delayCanonicalPatternA, delayCanonicalPatternB,
                delayPhase, delayRange,
                delaySerialAttack, delaySerialRelease,
                delaySerialAttack, delaySerialRelease,
                nullptr, nullptr, delayTraversalMode);
            delayActiveStepA.store (activeBank == 0 ? activeStep : -1);
            delayActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (delayIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            delayUnipolarA = seqwencer::evaluateBankRange (
                delayUnipolarPatternA, delayPhase, delayRange,
                delayAttackAValue, delayReleaseAValue,
                &stepA, delayTraversalMode);
            delayUnipolarB = seqwencer::evaluateBankRange (
                delayUnipolarPatternB, delayPhase, delayRange,
                delayAttackBValue, delayReleaseBValue,
                &stepB, delayTraversalMode);
            delayCanonicalA = seqwencer::evaluateBankRange (
                delayCanonicalPatternA, delayPhase, delayRange,
                delayAttackAValue, delayReleaseAValue,
                nullptr, delayTraversalMode);
            delayCanonicalB = seqwencer::evaluateBankRange (
                delayCanonicalPatternB, delayPhase, delayRange,
                delayAttackBValue, delayReleaseBValue,
                nullptr, delayTraversalMode);
            delayActiveStepA.store (delayAIsEnabled ? stepA : -1);
            delayActiveStepB.store (delayBIsEnabled ? stepB : -1);
        }

        const auto delayTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (delayLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return delaySerialBipolar
                    ? 2.0f * delaySerialCanonical - 1.0f
                    : delaySerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                delayBipolarAValue ? delayCanonicalA : delayUnipolarA,
                delayAIsEnabled && targetIsActive (0, target), target,
                delayBipolarAValue,
                delayBipolarBValue ? delayCanonicalB : delayUnipolarB,
                delayBIsEnabled && targetIsActive (1, target), target,
                delayBipolarBValue, assigned);
        };

        auto reverbUnipolarA = 1.0f;
        auto reverbUnipolarB = 1.0f;
        auto reverbCanonicalA = 1.0f;
        auto reverbCanonicalB = 1.0f;
        auto reverbSerialUnipolar = 1.0f;
        auto reverbSerialCanonical = 1.0f;
        if (reverbIsProcessing && reverbLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            reverbSerialUnipolar = seqwencer::evaluateLinkedRange (
                reverbUnipolarPatternA, reverbUnipolarPatternB,
                reverbPhase, reverbRange,
                reverbSerialAttack, reverbSerialRelease,
                reverbSerialAttack, reverbSerialRelease,
                &activeBank, &activeStep, reverbTraversalMode);
            reverbSerialCanonical = seqwencer::evaluateLinkedRange (
                reverbCanonicalPatternA, reverbCanonicalPatternB,
                reverbPhase, reverbRange,
                reverbSerialAttack, reverbSerialRelease,
                reverbSerialAttack, reverbSerialRelease,
                nullptr, nullptr, reverbTraversalMode);
            reverbActiveStepA.store (activeBank == 0 ? activeStep : -1);
            reverbActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (reverbIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            reverbUnipolarA = seqwencer::evaluateBankRange (
                reverbUnipolarPatternA, reverbPhase, reverbRange,
                reverbAttackAValue, reverbReleaseAValue,
                &stepA, reverbTraversalMode);
            reverbUnipolarB = seqwencer::evaluateBankRange (
                reverbUnipolarPatternB, reverbPhase, reverbRange,
                reverbAttackBValue, reverbReleaseBValue,
                &stepB, reverbTraversalMode);
            reverbCanonicalA = seqwencer::evaluateBankRange (
                reverbCanonicalPatternA, reverbPhase, reverbRange,
                reverbAttackAValue, reverbReleaseAValue,
                nullptr, reverbTraversalMode);
            reverbCanonicalB = seqwencer::evaluateBankRange (
                reverbCanonicalPatternB, reverbPhase, reverbRange,
                reverbAttackBValue, reverbReleaseBValue,
                nullptr, reverbTraversalMode);
            reverbActiveStepA.store (reverbAIsEnabled ? stepA : -1);
            reverbActiveStepB.store (reverbBIsEnabled ? stepB : -1);
        }

        const auto reverbTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (reverbLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return reverbSerialBipolar
                    ? 2.0f * reverbSerialCanonical - 1.0f
                    : reverbSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                reverbBipolarAValue ? reverbCanonicalA : reverbUnipolarA,
                reverbAIsEnabled && targetIsActive (0, target), target,
                reverbBipolarAValue,
                reverbBipolarBValue ? reverbCanonicalB : reverbUnipolarB,
                reverbBIsEnabled && targetIsActive (1, target), target,
                reverbBipolarBValue, assigned);
        };

        auto panUnipolarA = 0.5f;
        auto panUnipolarB = 0.5f;
        auto panCanonicalA = 0.5f;
        auto panCanonicalB = 0.5f;
        auto panSerialUnipolar = 0.5f;
        auto panSerialCanonical = 0.5f;
        if (panIsProcessing && panLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            panSerialUnipolar = seqwencer::evaluateLinkedRange (
                panUnipolarPatternA, panUnipolarPatternB,
                panPhase, panRange,
                panSerialAttack, panSerialRelease,
                panSerialAttack, panSerialRelease,
                &activeBank, &activeStep, panTraversalMode);
            panSerialCanonical = seqwencer::evaluateLinkedRange (
                panCanonicalPatternA, panCanonicalPatternB,
                panPhase, panRange,
                panSerialAttack, panSerialRelease,
                panSerialAttack, panSerialRelease,
                nullptr, nullptr, panTraversalMode);
            panActiveStepA.store (activeBank == 0 ? activeStep : -1);
            panActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (panIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            panUnipolarA = seqwencer::evaluateBankRange (
                panUnipolarPatternA, panPhase, panRange,
                panAttackAValue, panReleaseAValue,
                &stepA, panTraversalMode);
            panUnipolarB = seqwencer::evaluateBankRange (
                panUnipolarPatternB, panPhase, panRange,
                panAttackBValue, panReleaseBValue,
                &stepB, panTraversalMode);
            panCanonicalA = seqwencer::evaluateBankRange (
                panCanonicalPatternA, panPhase, panRange,
                panAttackAValue, panReleaseAValue,
                nullptr, panTraversalMode);
            panCanonicalB = seqwencer::evaluateBankRange (
                panCanonicalPatternB, panPhase, panRange,
                panAttackBValue, panReleaseBValue,
                nullptr, panTraversalMode);
            panActiveStepA.store (panAIsEnabled ? stepA : -1);
            panActiveStepB.store (panBIsEnabled ? stepB : -1);
        }

        const auto panTargetDeviation = [&] (bool* assigned = nullptr)
        {
            const auto target = seqwencer::ModulationTarget::panPosition;
            if (panLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return panSerialBipolar
                    ? 2.0f * panSerialCanonical - 1.0f
                    : panSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                panBipolarAValue ? panCanonicalA : panUnipolarA,
                panAIsEnabled && targetIsActive (0, target), target,
                panBipolarAValue,
                panBipolarBValue ? panCanonicalB : panUnipolarB,
                panBIsEnabled && targetIsActive (1, target), target,
                panBipolarBValue, assigned);
        };

        auto filterUnipolarA = 1.0f;
        auto filterUnipolarB = 1.0f;
        auto filterCanonicalA = 1.0f;
        auto filterCanonicalB = 1.0f;
        auto filterSerialUnipolar = 1.0f;
        auto filterSerialCanonical = 1.0f;
        if (filterIsProcessing && filterLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            filterSerialUnipolar = seqwencer::evaluateLinkedRange (
                filterUnipolarPatternA, filterUnipolarPatternB,
                filterPhase, filterRange,
                filterSerialAttack, filterSerialRelease,
                filterSerialAttack, filterSerialRelease,
                &activeBank, &activeStep, filterTraversalMode);
            filterSerialCanonical = seqwencer::evaluateLinkedRange (
                filterCanonicalPatternA, filterCanonicalPatternB,
                filterPhase, filterRange,
                filterSerialAttack, filterSerialRelease,
                filterSerialAttack, filterSerialRelease,
                nullptr, nullptr, filterTraversalMode);
            filterActiveStepA.store (activeBank == 0 ? activeStep : -1);
            filterActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (filterIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            filterUnipolarA = seqwencer::evaluateBankRange (
                filterUnipolarPatternA, filterPhase, filterRange,
                filterAttackAValue, filterReleaseAValue,
                &stepA, filterTraversalMode);
            filterUnipolarB = seqwencer::evaluateBankRange (
                filterUnipolarPatternB, filterPhase, filterRange,
                filterAttackBValue, filterReleaseBValue,
                &stepB, filterTraversalMode);
            filterCanonicalA = seqwencer::evaluateBankRange (
                filterCanonicalPatternA, filterPhase, filterRange,
                filterAttackAValue, filterReleaseAValue,
                nullptr, filterTraversalMode);
            filterCanonicalB = seqwencer::evaluateBankRange (
                filterCanonicalPatternB, filterPhase, filterRange,
                filterAttackBValue, filterReleaseBValue,
                nullptr, filterTraversalMode);
            filterActiveStepA.store (filterAIsEnabled ? stepA : -1);
            filterActiveStepB.store (filterBIsEnabled ? stepB : -1);
        }

        const auto filterTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (filterLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return filterSerialBipolar
                    ? 2.0f * filterSerialCanonical - 1.0f
                    : filterSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                filterBipolarAValue ? filterCanonicalA : filterUnipolarA,
                filterAIsEnabled && targetIsActive (0, target), target,
                filterBipolarAValue,
                filterBipolarBValue ? filterCanonicalB : filterUnipolarB,
                filterBIsEnabled && targetIsActive (1, target), target,
                filterBipolarBValue, assigned);
        };

        auto pitchUnipolarA = 1.0f;
        auto pitchUnipolarB = 1.0f;
        auto pitchCanonicalA = 1.0f;
        auto pitchCanonicalB = 1.0f;
        auto pitchSerialUnipolar = 1.0f;
        auto pitchSerialCanonical = 1.0f;
        if (pitchIsProcessing && pitchLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            pitchSerialUnipolar = seqwencer::evaluateLinkedRange (
                pitchUnipolarPatternA, pitchUnipolarPatternB,
                pitchPhase, pitchRange,
                pitchSerialAttack, pitchSerialRelease,
                pitchSerialAttack, pitchSerialRelease,
                &activeBank, &activeStep, pitchTraversalMode);
            pitchSerialCanonical = seqwencer::evaluateLinkedRange (
                pitchCanonicalPatternA, pitchCanonicalPatternB,
                pitchPhase, pitchRange,
                pitchSerialAttack, pitchSerialRelease,
                pitchSerialAttack, pitchSerialRelease,
                nullptr, nullptr, pitchTraversalMode);
            pitchActiveStepA.store (activeBank == 0 ? activeStep : -1);
            pitchActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (pitchIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            pitchUnipolarA = seqwencer::evaluateBankRange (
                pitchUnipolarPatternA, pitchPhase, pitchRange,
                pitchAttackAValue, pitchReleaseAValue,
                &stepA, pitchTraversalMode);
            pitchUnipolarB = seqwencer::evaluateBankRange (
                pitchUnipolarPatternB, pitchPhase, pitchRange,
                pitchAttackBValue, pitchReleaseBValue,
                &stepB, pitchTraversalMode);
            pitchCanonicalA = seqwencer::evaluateBankRange (
                pitchCanonicalPatternA, pitchPhase, pitchRange,
                pitchAttackAValue, pitchReleaseAValue,
                nullptr, pitchTraversalMode);
            pitchCanonicalB = seqwencer::evaluateBankRange (
                pitchCanonicalPatternB, pitchPhase, pitchRange,
                pitchAttackBValue, pitchReleaseBValue,
                nullptr, pitchTraversalMode);
            pitchActiveStepA.store (pitchAIsEnabled ? stepA : -1);
            pitchActiveStepB.store (pitchBIsEnabled ? stepB : -1);
        }

        const auto pitchTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (pitchLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return pitchSerialBipolar
                    ? 2.0f * pitchSerialCanonical - 1.0f
                    : pitchSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                pitchBipolarAValue ? pitchCanonicalA : pitchUnipolarA,
                pitchAIsEnabled && targetIsActive (0, target), target,
                pitchBipolarAValue,
                pitchBipolarBValue ? pitchCanonicalB : pitchUnipolarB,
                pitchBIsEnabled && targetIsActive (1, target), target,
                pitchBipolarBValue, assigned);
        };

        if (shouldGate && shouldNoiseGate && ! shouldBypass)
        {
            const auto modulatedActual = [&] (
                float baseValue, float minimumValue, float maximumValue,
                seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = gateTargetDeviation (target, &assigned);
                if (! assigned)
                    return baseValue;
                const auto normalised = (baseValue - minimumValue)
                                      / (maximumValue - minimumValue);
                return minimumValue + (maximumValue - minimumValue)
                    * seqwencer::applyModulationDepth (
                        normalised, deviation, 1.0f);
            };
            const auto noiseThresholdDb = modulatedActual (
                baseNoiseThresholdDb, -80.0f, 0.0f,
                seqwencer::ModulationTarget::noiseGateThreshold);
            const auto noiseAttackMs = modulatedActual (
                baseNoiseAttackMs, 0.1f, 100.0f,
                seqwencer::ModulationTarget::noiseGateAttack);
            const auto noiseHoldMs = modulatedActual (
                baseNoiseHoldMs, 0.0f, 500.0f,
                seqwencer::ModulationTarget::noiseGateHold);
            const auto noiseReleaseMs = modulatedActual (
                baseNoiseReleaseMs, 5.0f, 1000.0f,
                seqwencer::ModulationTarget::noiseGateRelease);
            const auto noiseRangeValue = modulatedActual (
                baseNoiseRange, 0.0f, 1.0f,
                seqwencer::ModulationTarget::noiseGateRange);
            const auto noiseThresholdGain =
                juce::Decibels::decibelsToGain (noiseThresholdDb);
            const auto noiseCloseThresholdGain = noiseThresholdGain
                * juce::Decibels::decibelsToGain (-3.0f);
            const auto noiseHoldSamples = static_cast<int> (std::lround (
                0.001 * noiseHoldMs * currentSampleRate));
            const auto noiseClosedGain = 1.0f - noiseRangeValue;
            const auto noiseAttackCoefficient = static_cast<float> (
                1.0 - std::exp (-1.0 / juce::jmax (
                    1.0, noiseAttackMs * 0.001 * currentSampleRate)));
            const auto noiseReleaseCoefficient = static_cast<float> (
                1.0 - std::exp (-1.0 / juce::jmax (
                    1.0, noiseReleaseMs * 0.001 * currentSampleRate)));
            auto detectorPeak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                detectorPeak = juce::jmax (
                    detectorPeak,
                    std::abs (buffer.getSample (channel, sample)
                              * smoothedGain));
            }

            if (detectorPeak >= noiseThresholdGain)
            {
                noiseGateIsOpen = true;
                noiseGateHoldSamplesRemaining = noiseHoldSamples;
            }
            else if (noiseGateIsOpen
                     && detectorPeak >= noiseCloseThresholdGain)
            {
                noiseGateHoldSamplesRemaining = noiseHoldSamples;
            }
            else if (noiseGateIsOpen)
            {
                if (noiseGateHoldSamplesRemaining > 0)
                    --noiseGateHoldSamplesRemaining;
                else
                    noiseGateIsOpen = false;
            }

            const auto noiseGateTarget = noiseGateIsOpen
                ? 1.0f : noiseClosedGain;
            const auto noiseCoefficient = noiseGateTarget > smoothedNoiseGateGain
                ? noiseAttackCoefficient : noiseReleaseCoefficient;
            smoothedNoiseGateGain += noiseCoefficient
                * (noiseGateTarget - smoothedNoiseGateGain);
        }
        else
        {
            noiseGateIsOpen = true;
            noiseGateHoldSamplesRemaining = 0;
            smoothedNoiseGateGain = 1.0f;
        }

        const auto outputGain = smoothedGain * smoothedNoiseGateGain;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample (channel, sample,
                              buffer.getSample (channel, sample) * outputGain);

        if (delayIsProcessing)
        {
            const auto modulatedDelayActual = [&] (
                float baseValue, float minimumValue, float maximumValue,
                seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = delayTargetDeviation (target, &assigned);
                if (! assigned)
                    return baseValue;
                const auto normalised = (baseValue - minimumValue)
                                      / (maximumValue - minimumValue);
                return minimumValue + (maximumValue - minimumValue)
                    * seqwencer::applyModulationDepth (
                        normalised, deviation, 1.0f);
            };
            const auto delayTimeMs = modulatedDelayActual (
                baseDelayTimeMs, 10.0f, 2000.0f,
                seqwencer::ModulationTarget::delayTime);
            const auto feedbackValue = modulatedDelayActual (
                baseDelayFeedback, 0.0f, 0.95f,
                seqwencer::ModulationTarget::delayFeedback);
            const auto mixValue = modulatedDelayActual (
                baseDelayMix, 0.0f, 1.0f,
                seqwencer::ModulationTarget::delayMix);
            smoothedDelaySamples.setTargetValue (static_cast<float> (
                delayTimeMs * 0.001 * currentSampleRate));
            const auto delaySamples = juce::jlimit (
                1.0f,
                static_cast<float> (delayBuffer.getNumSamples() - 2),
                smoothedDelaySamples.getNextValue());
            auto readPosition = static_cast<float> (delayWritePosition)
                              - delaySamples;
            while (readPosition < 0.0f)
                readPosition += static_cast<float> (delayBuffer.getNumSamples());
            const auto readIndex0 = static_cast<int> (std::floor (readPosition))
                                  % delayBuffer.getNumSamples();
            const auto readIndex1 = (readIndex0 + 1) % delayBuffer.getNumSamples();
            const auto fraction = readPosition - std::floor (readPosition);
            const auto channels = juce::jmin (buffer.getNumChannels(),
                                              delayBuffer.getNumChannels());
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto delayed0 = delayBuffer.getSample (channel, readIndex0);
                const auto delayed1 = delayBuffer.getSample (channel, readIndex1);
                const auto delayedSample = delayed0
                    + fraction * (delayed1 - delayed0);
                const auto drySample = buffer.getSample (channel, sample);
                delayBuffer.setSample (
                    channel, delayWritePosition,
                    drySample + delayedSample * feedbackValue);
                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (delayedSample - drySample));
            }
            delayWritePosition = (delayWritePosition + 1)
                               % delayBuffer.getNumSamples();
        }

        if (reverbIsProcessing)
        {
            const auto modulatedReverbActual = [&] (
                float baseValue, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = reverbTargetDeviation (target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseValue, deviation, 1.0f)
                    : baseValue;
            };

            juce::Reverb::Parameters reverbParameters;
            reverbParameters.roomSize = modulatedReverbActual (
                baseReverbSize, seqwencer::ModulationTarget::reverbSize);
            reverbParameters.damping = modulatedReverbActual (
                baseReverbDamping, seqwencer::ModulationTarget::reverbDamping);
            reverbParameters.width = modulatedReverbActual (
                baseReverbWidth, seqwencer::ModulationTarget::reverbWidth);
            reverbParameters.wetLevel = 1.0f / 3.0f;
            reverbParameters.dryLevel = 0.0f;
            reverbParameters.freezeMode = 0.0f;
            reverbProcessor.setParameters (reverbParameters);

            const auto mixValue = modulatedReverbActual (
                baseReverbMix, seqwencer::ModulationTarget::reverbMix);
            if (buffer.getNumChannels() >= 2)
            {
                const auto dryLeft = buffer.getSample (0, sample);
                const auto dryRight = buffer.getSample (1, sample);
                auto wetLeft = dryLeft;
                auto wetRight = dryRight;
                reverbProcessor.processStereo (&wetLeft, &wetRight, 1);
                buffer.setSample (0, sample,
                                  dryLeft + mixValue * (wetLeft - dryLeft));
                buffer.setSample (1, sample,
                                  dryRight + mixValue * (wetRight - dryRight));
            }
            else if (buffer.getNumChannels() == 1)
            {
                const auto drySample = buffer.getSample (0, sample);
                auto wetSample = drySample;
                reverbProcessor.processMono (&wetSample, 1);
                buffer.setSample (
                    0, sample,
                    drySample + mixValue * (wetSample - drySample));
            }
        }

        if (panIsProcessing)
        {
            bool panIsAssigned = false;
            const auto panDeviation = panTargetDeviation (&panIsAssigned);
            const auto panNormalised = panIsAssigned
                ? seqwencer::applyModulationDepth (
                    basePanNormalised, panDeviation, 1.0f)
                : basePanNormalised;
            const auto angle = juce::MathConstants<float>::pi
                             * (panNormalised - 0.5f);
            const auto leftGain = panNormalised <= 0.5f
                ? 1.0f : std::cos (angle);
            const auto rightGain = panNormalised >= 0.5f
                ? 1.0f : std::cos (angle);
            buffer.setSample (0, sample,
                              buffer.getSample (0, sample) * leftGain);
            buffer.setSample (1, sample,
                              buffer.getSample (1, sample) * rightGain);
        }

        if (filterIsProcessing)
        {
            const auto modulatedFilterNormalised = [&] (
                float baseNormalised, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = filterTargetDeviation (target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseNormalised, deviation, 1.0f)
                    : baseNormalised;
            };

            constexpr auto cutoffMinimum = 20.0f;
            constexpr auto cutoffMaximum = 20000.0f;
            const auto cutoffRatio = cutoffMaximum / cutoffMinimum;
            const auto baseCutoffNormalised = std::log (
                baseFilterCutoff / cutoffMinimum) / std::log (cutoffRatio);
            const auto cutoffNormalised = modulatedFilterNormalised (
                baseCutoffNormalised,
                seqwencer::ModulationTarget::filterCutoff);
            const auto cutoffValue = juce::jlimit (
                cutoffMinimum,
                static_cast<float> (juce::jmax (
                    static_cast<double> (cutoffMinimum),
                    currentSampleRate * 0.45)),
                cutoffMinimum * std::pow (cutoffRatio, cutoffNormalised));
            const auto baseResonanceNormalised =
                (baseFilterResonance - 0.10f) / 9.90f;
            const auto resonanceValue = 0.10f + 9.90f
                * modulatedFilterNormalised (
                    baseResonanceNormalised,
                    seqwencer::ModulationTarget::filterResonance);
            const auto mixValue = modulatedFilterNormalised (
                baseFilterMix, seqwencer::ModulationTarget::filterMix);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                const auto wetSample = processFilterSample (
                    channel, drySample, cutoffValue, resonanceValue,
                    baseFilterType);
                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (wetSample - drySample));
            }
        }

        if (pitchIsProcessing)
        {
            const auto modulatedPitchNormalised = [&] (
                float baseNormalised, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = pitchTargetDeviation (target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseNormalised, deviation, 1.0f)
                    : baseNormalised;
            };

            const auto shiftNormalised = modulatedPitchNormalised (
                (basePitchShift + 24.0f) / 48.0f,
                seqwencer::ModulationTarget::pitchShift);
            const auto semitones = -24.0f + 48.0f * shiftNormalised;
            const auto mixValue = modulatedPitchNormalised (
                basePitchMix, seqwencer::ModulationTarget::pitchMix);
            const auto ratio = std::pow (2.0, static_cast<double> (semitones)
                                             / 12.0);
            const auto windowSamples = juce::jlimit (
                64,
                pitchBuffer.getNumSamples() - 8,
                static_cast<int> (std::lround (currentSampleRate * 0.060)));
            const auto windowSpan = static_cast<double> (windowSamples - 1);
            const auto channels = juce::jmin (buffer.getNumChannels(),
                                              pitchBuffer.getNumChannels());
            const auto phaseA = pitchReadPhase;
            const auto phaseB = std::fmod (pitchReadPhase + 0.5, 1.0);
            const auto weightA = 0.5 - 0.5 * std::cos (
                juce::MathConstants<double>::twoPi * phaseA);
            const auto weightB = 0.5 - 0.5 * std::cos (
                juce::MathConstants<double>::twoPi * phaseB);
            const auto weightTotal = juce::jmax (1.0e-9, weightA + weightB);

            const auto readDelayedSample = [&] (int channel, double phase)
            {
                const auto delaySamples = 4.0 + phase * windowSpan;
                auto readPosition = static_cast<double> (pitchWritePosition)
                                  - delaySamples;
                const auto bufferLength = pitchBuffer.getNumSamples();
                while (readPosition < 0.0)
                    readPosition += static_cast<double> (bufferLength);
                while (readPosition >= static_cast<double> (bufferLength))
                    readPosition -= static_cast<double> (bufferLength);
                const auto index0 = static_cast<int> (std::floor (readPosition));
                const auto index1 = (index0 + 1) % bufferLength;
                const auto fraction = static_cast<float> (
                    readPosition - std::floor (readPosition));
                const auto sample0 = pitchBuffer.getSample (channel, index0);
                const auto sample1 = pitchBuffer.getSample (channel, index1);
                return sample0 + fraction * (sample1 - sample0);
            };

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                pitchBuffer.setSample (
                    channel, pitchWritePosition, drySample);
                const auto wetSample = std::abs (semitones) < 0.0001f
                    ? drySample
                    : static_cast<float> ((
                        weightA * readDelayedSample (channel, phaseA)
                        + weightB * readDelayedSample (channel, phaseB))
                        / weightTotal);
                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (wetSample - drySample));
            }
            pitchWritePosition = (pitchWritePosition + 1)
                               % pitchBuffer.getNumSamples();
            pitchReadPhase = std::fmod (
                pitchReadPhase + (1.0 - ratio) / windowSpan + 1.0, 1.0);
        }

        const auto mayAdvance = ! hostSyncEnabled || hostTimelineAdvancing;
        if (gateIsProcessing && ! gateUseHostPosition
            && (mayAdvance || gateRetriggersFromPlayedNotes))
        {
            gateFreeRunningPhase = seqwencer::wrapPhase (
                gateFreeRunningPhase + gatePhaseIncrement, gateCycleLength);
        }
        if (! phiUseHostPosition
            && (mayAdvance || phiRetriggersFromPlayedNotes))
        {
            phiFreeRunningPhase = seqwencer::wrapPhase (
                phiFreeRunningPhase + phiPhaseIncrement, phiCycleLength);
        }
        if (delayIsProcessing && ! delayUseHostPosition
            && (mayAdvance || delayRetriggersFromPlayedNotes))
        {
            delayFreeRunningPhase = seqwencer::wrapPhase (
                delayFreeRunningPhase + delayPhaseIncrement, delayCycleLength);
        }
        if (reverbIsProcessing && ! reverbUseHostPosition
            && (mayAdvance || reverbRetriggersFromPlayedNotes))
        {
            reverbFreeRunningPhase = seqwencer::wrapPhase (
                reverbFreeRunningPhase + reverbPhaseIncrement,
                reverbCycleLength);
        }
        if (panIsProcessing && ! panUseHostPosition
            && (mayAdvance || panRetriggersFromPlayedNotes))
        {
            panFreeRunningPhase = seqwencer::wrapPhase (
                panFreeRunningPhase + panPhaseIncrement, panCycleLength);
        }
        if (filterIsProcessing && ! filterUseHostPosition
            && (mayAdvance || filterRetriggersFromPlayedNotes))
        {
            filterFreeRunningPhase = seqwencer::wrapPhase (
                filterFreeRunningPhase + filterPhaseIncrement,
                filterCycleLength);
        }
        if (pitchIsProcessing && ! pitchUseHostPosition
            && (mayAdvance || pitchRetriggersFromPlayedNotes))
        {
            pitchFreeRunningPhase = seqwencer::wrapPhase (
                pitchFreeRunningPhase + pitchPhaseIncrement,
                pitchCycleLength);
        }
    }

    if (gateIsProcessing && gateUseHostPosition && hostTimelineAdvancing)
    {
        gateFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            gateStepBeats, gateCycleLength);
    }
    if (phiUseHostPosition && hostTimelineAdvancing)
    {
        phiFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            phiStepBeats, phiCycleLength);
    }
    if (delayIsProcessing && delayUseHostPosition && hostTimelineAdvancing)
    {
        delayFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            delayStepBeats, delayCycleLength);
    }
    if (reverbIsProcessing && reverbUseHostPosition && hostTimelineAdvancing)
    {
        reverbFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            reverbStepBeats, reverbCycleLength);
    }
    if (panIsProcessing && panUseHostPosition && hostTimelineAdvancing)
    {
        panFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            panStepBeats, panCycleLength);
    }
    if (filterIsProcessing && filterUseHostPosition && hostTimelineAdvancing)
    {
        filterFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            filterStepBeats, filterCycleLength);
    }
    if (pitchIsProcessing && pitchUseHostPosition && hostTimelineAdvancing)
    {
        pitchFreeRunningPhase = seqwencer::phaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            pitchStepBeats, pitchCycleLength);
    }

    if (shouldSendPhiBridge && buffer.getNumSamples() > 0)
    {
        if (phiLinked)
        {
            const auto packet = seqwencer_bridge::encodeLaneValue (
                seqwencer_bridge::serialLane,
                phiSerialBipolar,
                true,
                lastSerialBridgeValue);
            midiMessages.addEvent (
                juce::MidiMessage::createSysExMessage (
                    packet.data(), static_cast<int> (packet.size())),
                0);
        }
        else
        {
            const auto packetA = seqwencer_bridge::encodeLaneValue (
                seqwencer_bridge::sequencerALane,
                phiBipolarAValue,
                phiAIsEnabled,
                lastBridgeValueA);
            const auto packetB = seqwencer_bridge::encodeLaneValue (
                seqwencer_bridge::sequencerBLane,
                phiBipolarBValue,
                phiBIsEnabled,
                lastBridgeValueB);
            midiMessages.addEvent (
                juce::MidiMessage::createSysExMessage (
                    packetA.data(), static_cast<int> (packetA.size())),
                0);
            midiMessages.addEvent (
                juce::MidiMessage::createSysExMessage (
                    packetB.data(), static_cast<int> (packetB.size())),
                0);
        }
    }

    if (buffer.getNumSamples() > 0
        && phiTargetBrowserRequestPending.exchange (
            false, std::memory_order_acq_rel))
    {
        const auto packet = seqwencer_bridge::encodeTargetBrowserRequest (
            phiLinked);
        midiMessages.addEvent (
            juce::MidiMessage::createSysExMessage (
                packet.data(), static_cast<int> (packet.size())),
            0);
    }
}

void SeqwencerAudioProcessor::getStateInformation (juce::MemoryBlock& data)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, data);
}

void SeqwencerAudioProcessor::setStateInformation (const void* data, int size)
{
    if (const auto xml = getXmlFromBinary (data, size))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
            migrateStepStorageIfNeeded();
        }
    }
}

void SeqwencerAudioProcessor::migrateStepStorageIfNeeded()
{
    auto& state = parameters.state;
    const auto version = state.hasProperty ("step_encoding_version")
        ? static_cast<int> (state.getProperty ("step_encoding_version")) : 0;
    if (version >= 2)
        return;

    const auto readActual = [this] (const char* parameterID)
    {
        if (auto* parameter = parameters.getParameter (parameterID))
            return parameter->convertFrom0to1 (parameter->getValue());
        return 0.0f;
    };

    const auto serial = readActual ("playback_mode") >= 0.5f;
    const auto profileB = readActual ("serial_profile") >= 0.5f;
    const auto bipolarA = readActual ("seq_a_bipolar") >= 0.5f;
    const auto bipolarB = readActual ("seq_b_bipolar") >= 0.5f;
    const auto serialBipolar = profileB ? bipolarB : bipolarA;

    auto bankWasCanonical = [&] (int bank)
    {
        if (version == 1)
        {
            const auto property = bank == 0 ? "step_encoding_a_bipolar"
                                            : "step_encoding_b_bipolar";
            return static_cast<bool> (state.getProperty (property, false));
        }

        // Stage 2.2 stored raw screen coordinates. A bipolar display was
        // already canonical; a unipolar display needs moving into 0.5..1.0.
        if (serial)
            return serialBipolar;
        return bank == 0 ? bipolarA : bipolarB;
    };

    for (int bank = 0; bank < 2; ++bank)
    {
        if (bankWasCanonical (bank))
            continue;

        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            auto* parameter = parameters.getParameter (stepParameterID (bank, step));
            if (parameter == nullptr)
                continue;

            const auto oldUnipolar = parameter->convertFrom0to1 (
                parameter->getValue());
            const auto canonical = seqwencer::canonicalFromUnipolar (oldUnipolar);
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (canonical));
        }
    }

    state.setProperty ("step_encoding_version", 2, nullptr);
    state.setProperty ("step_encoding_a_bipolar", true, nullptr);
    state.setProperty ("step_encoding_b_bipolar", true, nullptr);
}

juce::AudioProcessorEditor* SeqwencerAudioProcessor::createEditor()
{
    return new SeqwencerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SeqwencerAudioProcessor();
}
