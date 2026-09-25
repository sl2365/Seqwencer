#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SeqwencerBridgeProtocol.h"

#include <array>
#include <cmath>
#include <limits>
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

juce::String audioFxStageName (seqwencer::AudioFxStage stage)
{
    switch (stage)
    {
        case seqwencer::AudioFxStage::gate:       return "Gate";
        case seqwencer::AudioFxStage::delay:      return "Delay";
        case seqwencer::AudioFxStage::reverb:     return "Reverb";
        case seqwencer::AudioFxStage::pan:        return "Pan";
        case seqwencer::AudioFxStage::filter:     return "Filter";
        case seqwencer::AudioFxStage::pitch:      return "Pitch";
        case seqwencer::AudioFxStage::distortion: return "Distortion";
        case seqwencer::AudioFxStage::grain:      return "Grain";
        case seqwencer::AudioFxStage::compressor: return "Compressor";
        case seqwencer::AudioFxStage::reverse:    return "Reverse";
        case seqwencer::AudioFxStage::retrigger:  return "Retrigger";
    }
    return {};
}

bool audioFxStageFromName (const juce::String& name,
                           seqwencer::AudioFxStage& result)
{
    const auto trimmed = name.trim();
    for (const auto stage : seqwencer::defaultAudioFxOrder())
    {
        if (trimmed.equalsIgnoreCase (audioFxStageName (stage)))
        {
            result = stage;
            return true;
        }
    }
    return false;
}

juce::String serialiseAudioFxOrder (const seqwencer::AudioFxOrder& order)
{
    juce::StringArray names;
    for (const auto stage : order)
        names.add (audioFxStageName (stage));
    return names.joinIntoString (",");
}

seqwencer::AudioFxOrder parseAudioFxOrder (const juce::String& stored)
{
    std::array<int, seqwencer::audioFxStageCount> values {};
    values.fill (-1);
    juce::StringArray names;
    names.addTokens (stored, ",", {});
    for (auto index = 0;
         index < juce::jmin (names.size(), seqwencer::audioFxStageCount);
         ++index)
    {
        seqwencer::AudioFxStage stage {};
        if (audioFxStageFromName (names[index], stage))
            values[static_cast<std::size_t> (index)] =
                static_cast<int> (stage);
    }
    return seqwencer::sanitiseAudioFxOrder (values);
}

juce::String sequencerEngineID (seqwencer::SequencerEngine engine)
{
    switch (engine)
    {
        case seqwencer::SequencerEngine::gate:       return "gate";
        case seqwencer::SequencerEngine::phi:        return "phi";
        case seqwencer::SequencerEngine::delay:      return "delay";
        case seqwencer::SequencerEngine::reverb:     return "reverb";
        case seqwencer::SequencerEngine::pan:        return "pan";
        case seqwencer::SequencerEngine::filter:     return "filter";
        case seqwencer::SequencerEngine::pitch:      return "pitch";
        case seqwencer::SequencerEngine::distortion: return "distortion";
        case seqwencer::SequencerEngine::grain:      return "grain";
        case seqwencer::SequencerEngine::compressor: return "compressor";
        case seqwencer::SequencerEngine::reverse:    return "reverse";
        case seqwencer::SequencerEngine::retrigger:  return "retrigger";
        case seqwencer::SequencerEngine::phiCD:      return "phi_cd";
        case seqwencer::SequencerEngine::phiEF:      return "phi_ef";
        case seqwencer::SequencerEngine::phiGH:      return "phi_gh";
    }
    return {};
}

juce::String sequencerEngineDisplayName (seqwencer::SequencerEngine engine)
{
    auto name = sequencerEngineID (engine);
    return name.substring (0, 1).toUpperCase() + name.substring (1);
}

juce::String sequencerLaneControlParameterID (
    seqwencer::SequencerEngine engine,
    int bank,
    const juce::String& control)
{
    bank = juce::jlimit (0, 1, bank);
    if (seqwencer::isPhiSequencerEngine (engine))
    {
        return SeqwencerAudioProcessor::phiLaneParameterID (
            seqwencer::phiLaneForEngineBank (engine, bank), control);
    }
    if (engine == seqwencer::SequencerEngine::gate)
        return "seq_" + juce::String (bank == 0 ? "a_" : "b_") + control;
    return sequencerEngineID (engine) + "_seq_"
         + juce::String (bank == 0 ? "a_" : "b_") + control;
}

juce::String subdivisionPropertyID (seqwencer::SequencerEngine engine,
                                    int bank,
                                    const juce::String& valueKind)
{
    return sequencerEngineID (engine) + "_subdivision_"
         + (bank == 0 ? "a_" : "b_") + valueKind;
}

juce::String serialiseSubdivisionModes (
    const seqwencer::StepSubdivisionPattern& subdivisions)
{
    juce::StringArray values;
    for (const auto& step : subdivisions)
        values.add (juce::String (static_cast<int> (step.mode)));
    return values.joinIntoString (",");
}

juce::String serialiseSubdivisionExtraValues (
    const seqwencer::StepSubdivisionPattern& subdivisions,
    int extraIndex)
{
    juce::StringArray values;
    extraIndex = juce::jlimit (
        0, seqwencer::maximumSegmentsPerStep - 2, extraIndex);
    for (const auto& step : subdivisions)
        values.add (juce::String (step.extraValues[static_cast<std::size_t> (
                                      extraIndex)],
                                  7));
    return values.joinIntoString (",");
}

void parseSubdivisionModes (
    const juce::String& stored,
    seqwencer::StepSubdivisionPattern& subdivisions)
{
    juce::StringArray values;
    values.addTokens (stored, ",", {});
    for (auto index = 0;
         index < juce::jmin (values.size(), seqwencer::stepsPerBank);
         ++index)
    {
        subdivisions[static_cast<std::size_t> (index)].mode =
            seqwencer::stepDivisionModeFromChoice (
                values[index].trim().getIntValue());
    }
}

void parseSubdivisionExtraValues (
    const juce::String& stored,
    int extraIndex,
    seqwencer::StepSubdivisionPattern& subdivisions)
{
    juce::StringArray values;
    values.addTokens (stored, ",", {});
    extraIndex = juce::jlimit (
        0, seqwencer::maximumSegmentsPerStep - 2, extraIndex);
    for (auto index = 0;
         index < juce::jmin (values.size(), seqwencer::stepsPerBank);
         ++index)
    {
        subdivisions[static_cast<std::size_t> (index)]
            .extraValues[static_cast<std::size_t> (extraIndex)] =
                juce::jlimit (0.0f, 1.0f,
                              values[index].trim().getFloatValue());
    }
}

juce::String stepParameterIDForEngine (
    seqwencer::SequencerEngine engine,
    int bank,
    int step)
{
    switch (engine)
    {
        case seqwencer::SequencerEngine::phi:
        case seqwencer::SequencerEngine::phiCD:
        case seqwencer::SequencerEngine::phiEF:
        case seqwencer::SequencerEngine::phiGH:
            return SeqwencerAudioProcessor::phiStepParameterID (
                seqwencer::phiLaneForEngineBank (engine, bank), step);
        case seqwencer::SequencerEngine::delay:
            return SeqwencerAudioProcessor::delayStepParameterID (bank, step);
        case seqwencer::SequencerEngine::reverb:
            return SeqwencerAudioProcessor::reverbStepParameterID (bank, step);
        case seqwencer::SequencerEngine::pan:
            return SeqwencerAudioProcessor::panStepParameterID (bank, step);
        case seqwencer::SequencerEngine::filter:
            return SeqwencerAudioProcessor::filterStepParameterID (bank, step);
        case seqwencer::SequencerEngine::pitch:
            return SeqwencerAudioProcessor::pitchStepParameterID (bank, step);
        case seqwencer::SequencerEngine::distortion:
            return SeqwencerAudioProcessor::distortionStepParameterID (
                bank, step);
        case seqwencer::SequencerEngine::grain:
            return SeqwencerAudioProcessor::grainStepParameterID (bank, step);
        case seqwencer::SequencerEngine::compressor:
            return SeqwencerAudioProcessor::compressorStepParameterID (
                bank, step);
        case seqwencer::SequencerEngine::reverse:
            return SeqwencerAudioProcessor::reverseStepParameterID (
                bank, step);
        case seqwencer::SequencerEngine::retrigger:
            return SeqwencerAudioProcessor::retriggerStepParameterID (
                bank, step);
        case seqwencer::SequencerEngine::gate:
            return SeqwencerAudioProcessor::stepParameterID (bank, step);
    }
    return {};
}

juce::String serialiseSequenceSteps (const seqwencer::Pattern& steps)
{
    juce::StringArray values;
    for (const auto value : steps)
        values.add (juce::String (value, 7));
    return values.joinIntoString (",");
}

bool parseSequenceSteps (const juce::String& stored,
                         seqwencer::Pattern& steps)
{
    juce::StringArray values;
    values.addTokens (stored, ",", {});
    if (values.size() != seqwencer::stepsPerBank)
        return false;

    for (auto index = 0; index < values.size(); ++index)
    {
        const auto value = values[index].trim();
        if (value.isEmpty())
            return false;
        steps[static_cast<std::size_t> (index)] = juce::jlimit (
            0.0f, 1.0f, value.getFloatValue());
    }
    return true;
}

bool parseSequenceSubdivisionModes (
    const juce::String& stored,
    seqwencer::StepSubdivisionPattern& subdivisions)
{
    juce::StringArray values;
    values.addTokens (stored, ",", {});
    if (values.size() != seqwencer::stepsPerBank)
        return false;

    for (auto index = 0; index < values.size(); ++index)
    {
        const auto value = values[index].trim();
        const auto mode = value.getIntValue();
        if (value.isEmpty()
            || mode < static_cast<int> (seqwencer::StepDivisionMode::normal)
            || mode > static_cast<int> (seqwencer::StepDivisionMode::three))
        {
            return false;
        }
        subdivisions[static_cast<std::size_t> (index)].mode =
            static_cast<seqwencer::StepDivisionMode> (mode);
    }
    return true;
}

bool parseSequenceSubdivisionExtraValues (
    const juce::String& stored,
    int extraIndex,
    seqwencer::StepSubdivisionPattern& subdivisions)
{
    juce::StringArray values;
    values.addTokens (stored, ",", {});
    if (values.size() != seqwencer::stepsPerBank)
        return false;

    extraIndex = juce::jlimit (
        0, seqwencer::maximumSegmentsPerStep - 2, extraIndex);
    for (auto index = 0; index < values.size(); ++index)
    {
        const auto value = values[index].trim();
        if (value.isEmpty())
            return false;
        subdivisions[static_cast<std::size_t> (index)]
            .extraValues[static_cast<std::size_t> (extraIndex)] =
                juce::jlimit (0.0f, 1.0f, value.getFloatValue());
    }
    return true;
}

juce::String sequencerEnvelopeTargetParameterID (
    int sourceBank, seqwencer::ModulationTarget target, bool enabled)
{
    if (! seqwencer::isSequencerEnvelopeTarget (target))
        return {};

    const auto engine = seqwencer::sequencerEnvelopeTargetEngine (target);
    const auto destinationBank = seqwencer::sequencerEnvelopeTargetBank (target);
    const auto envelope = seqwencer::sequencerEnvelopeTargetIsAttack (target)
        ? "attack" : "release";
    return sequencerEngineID (engine)
        + "_seq_" + (sourceBank == 0 ? "a_" : "b_")
        + (destinationBank == 0 ? "a_" : "b_")
        + envelope + "_target" + (enabled ? "_enabled" : "");
}

juce::String sequencerRangeTargetParameterID (
    int sourceBank, seqwencer::ModulationTarget target, bool enabled)
{
    if (! seqwencer::isSequencerRangeTarget (target))
        return {};

    const auto engine = seqwencer::sequencerRangeTargetEngine (target);
    const auto boundary = seqwencer::sequencerRangeTargetIsStart (target)
        ? "start"
        : (seqwencer::sequencerRangeTargetIsEnd (target)
               ? "end" : "length");
    return sequencerEngineID (engine)
        + "_seq_" + (sourceBank == 0 ? "a_" : "b_")
        + boundary + "_target" + (enabled ? "_enabled" : "");
}

juce::String sequencerTransformTargetParameterID (
    int sourceBank, seqwencer::ModulationTarget target, bool enabled)
{
    if (! seqwencer::isSequencerTransformTarget (target))
        return {};

    const auto engine = seqwencer::sequencerTransformTargetEngine (target);
    const auto destinationBank =
        seqwencer::sequencerTransformTargetBank (target);
    const auto control = seqwencer::sequencerTransformTargetIsPeak (target)
        ? "peak" : "move";
    return sequencerEngineID (engine)
        + "_seq_" + (sourceBank == 0 ? "a_" : "b_")
        + (destinationBank == 0 ? "a_" : "b_")
        + control + "_target" + (enabled ? "_enabled" : "");
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
    rangeLength = parameters.getRawParameterValue ("range_length");
    rangeLink = parameters.getRawParameterValue ("range_link");
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
    for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
    {
        const auto pairIndex = static_cast<std::size_t> (pair);
        phiPlaybackModes[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "playback_mode"));
        phiSerialProfiles[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "serial_profile"));
        phiStartSteps[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "start_step"));
        phiEndSteps[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "end_step"));
        phiRangeLengths[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "range_length"));
        phiRangeLinks[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "range_link"));
        phiRates[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "rate"));
        phiSequenceModes[pairIndex] = parameters.getRawParameterValue (
            phiPairParameterID (pair, "sequence_mode"));
    }
    for (int lane = 0; lane < seqwencer::phiSequencerLaneCount; ++lane)
    {
        const auto laneIndex = static_cast<std::size_t> (lane);
        phiSeqEnabled[laneIndex] = parameters.getRawParameterValue (
            phiLaneParameterID (lane, "enabled"));
        phiSeqBipolar[laneIndex] = parameters.getRawParameterValue (
            phiLaneParameterID (lane, "bipolar"));
        phiSeqAttack[laneIndex] = parameters.getRawParameterValue (
            phiLaneParameterID (lane, "attack"));
        phiSeqRelease[laneIndex] = parameters.getRawParameterValue (
            phiLaneParameterID (lane, "release"));
    }
    delayEnabled = parameters.getRawParameterValue ("delay_enabled");
    delayTime = parameters.getRawParameterValue ("delay_time");
    delayFeedback = parameters.getRawParameterValue ("delay_feedback");
    delayMix = parameters.getRawParameterValue ("delay_mix");
    delayPlaybackMode = parameters.getRawParameterValue ("delay_playback_mode");
    delaySerialProfile = parameters.getRawParameterValue ("delay_serial_profile");
    delayStartStep = parameters.getRawParameterValue ("delay_start_step");
    delayEndStep = parameters.getRawParameterValue ("delay_end_step");
    delayRangeLength = parameters.getRawParameterValue (
        "delay_range_length");
    delayRangeLink = parameters.getRawParameterValue ("delay_range_link");
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
    reverbRangeLength = parameters.getRawParameterValue (
        "reverb_range_length");
    reverbRangeLink = parameters.getRawParameterValue ("reverb_range_link");
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
    panRangeLength = parameters.getRawParameterValue ("pan_range_length");
    panRangeLink = parameters.getRawParameterValue ("pan_range_link");
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
    filterRangeLength = parameters.getRawParameterValue (
        "filter_range_length");
    filterRangeLink = parameters.getRawParameterValue ("filter_range_link");
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
    pitchRangeLength = parameters.getRawParameterValue (
        "pitch_range_length");
    pitchRangeLink = parameters.getRawParameterValue ("pitch_range_link");
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
    distortionEnabled = parameters.getRawParameterValue ("distortion_enabled");
    distortionType = parameters.getRawParameterValue ("distortion_type");
    distortionDrive = parameters.getRawParameterValue ("distortion_drive");
    distortionTone = parameters.getRawParameterValue ("distortion_tone");
    distortionMix = parameters.getRawParameterValue ("distortion_mix");
    distortionPlaybackMode = parameters.getRawParameterValue (
        "distortion_playback_mode");
    distortionSerialProfile = parameters.getRawParameterValue (
        "distortion_serial_profile");
    distortionStartStep = parameters.getRawParameterValue (
        "distortion_start_step");
    distortionEndStep = parameters.getRawParameterValue (
        "distortion_end_step");
    distortionRangeLength = parameters.getRawParameterValue (
        "distortion_range_length");
    distortionRangeLink = parameters.getRawParameterValue (
        "distortion_range_link");
    distortionRate = parameters.getRawParameterValue ("distortion_rate");
    distortionSequenceMode = parameters.getRawParameterValue (
        "distortion_sequence_mode");
    distortionSeqAEnabled = parameters.getRawParameterValue (
        "distortion_seq_a_enabled");
    distortionSeqABipolar = parameters.getRawParameterValue (
        "distortion_seq_a_bipolar");
    distortionSeqAAttack = parameters.getRawParameterValue (
        "distortion_seq_a_attack");
    distortionSeqARelease = parameters.getRawParameterValue (
        "distortion_seq_a_release");
    distortionSeqBEnabled = parameters.getRawParameterValue (
        "distortion_seq_b_enabled");
    distortionSeqBBipolar = parameters.getRawParameterValue (
        "distortion_seq_b_bipolar");
    distortionSeqBAttack = parameters.getRawParameterValue (
        "distortion_seq_b_attack");
    distortionSeqBRelease = parameters.getRawParameterValue (
        "distortion_seq_b_release");
    grainEnabled = parameters.getRawParameterValue ("grain_enabled");
    grainSize = parameters.getRawParameterValue ("grain_size");
    grainShift = parameters.getRawParameterValue ("grain_shift");
    grainFeedback = parameters.getRawParameterValue ("grain_feedback");
    grainMix = parameters.getRawParameterValue ("grain_mix");
    grainPlaybackMode = parameters.getRawParameterValue (
        "grain_playback_mode");
    grainSerialProfile = parameters.getRawParameterValue (
        "grain_serial_profile");
    grainStartStep = parameters.getRawParameterValue ("grain_start_step");
    grainEndStep = parameters.getRawParameterValue ("grain_end_step");
    grainRangeLength = parameters.getRawParameterValue (
        "grain_range_length");
    grainRangeLink = parameters.getRawParameterValue ("grain_range_link");
    grainRate = parameters.getRawParameterValue ("grain_rate");
    grainSequenceMode = parameters.getRawParameterValue (
        "grain_sequence_mode");
    grainSeqAEnabled = parameters.getRawParameterValue (
        "grain_seq_a_enabled");
    grainSeqABipolar = parameters.getRawParameterValue (
        "grain_seq_a_bipolar");
    grainSeqAAttack = parameters.getRawParameterValue (
        "grain_seq_a_attack");
    grainSeqARelease = parameters.getRawParameterValue (
        "grain_seq_a_release");
    grainSeqBEnabled = parameters.getRawParameterValue (
        "grain_seq_b_enabled");
    grainSeqBBipolar = parameters.getRawParameterValue (
        "grain_seq_b_bipolar");
    grainSeqBAttack = parameters.getRawParameterValue (
        "grain_seq_b_attack");
    grainSeqBRelease = parameters.getRawParameterValue (
        "grain_seq_b_release");
    compressorEnabled = parameters.getRawParameterValue ("compressor_enabled");
    compressorThreshold = parameters.getRawParameterValue (
        "compressor_threshold");
    compressorRatio = parameters.getRawParameterValue ("compressor_ratio");
    compressorAttack = parameters.getRawParameterValue ("compressor_attack");
    compressorRelease = parameters.getRawParameterValue (
        "compressor_release");
    compressorMakeup = parameters.getRawParameterValue ("compressor_makeup");
    compressorMix = parameters.getRawParameterValue ("compressor_mix");
    compressorPlaybackMode = parameters.getRawParameterValue (
        "compressor_playback_mode");
    compressorSerialProfile = parameters.getRawParameterValue (
        "compressor_serial_profile");
    compressorStartStep = parameters.getRawParameterValue (
        "compressor_start_step");
    compressorEndStep = parameters.getRawParameterValue (
        "compressor_end_step");
    compressorRangeLength = parameters.getRawParameterValue (
        "compressor_range_length");
    compressorRangeLink = parameters.getRawParameterValue (
        "compressor_range_link");
    compressorRate = parameters.getRawParameterValue ("compressor_rate");
    compressorSequenceMode = parameters.getRawParameterValue (
        "compressor_sequence_mode");
    compressorSeqAEnabled = parameters.getRawParameterValue (
        "compressor_seq_a_enabled");
    compressorSeqABipolar = parameters.getRawParameterValue (
        "compressor_seq_a_bipolar");
    compressorSeqAAttack = parameters.getRawParameterValue (
        "compressor_seq_a_attack");
    compressorSeqARelease = parameters.getRawParameterValue (
        "compressor_seq_a_release");
    compressorSeqBEnabled = parameters.getRawParameterValue (
        "compressor_seq_b_enabled");
    compressorSeqBBipolar = parameters.getRawParameterValue (
        "compressor_seq_b_bipolar");
    compressorSeqBAttack = parameters.getRawParameterValue (
        "compressor_seq_b_attack");
    compressorSeqBRelease = parameters.getRawParameterValue (
        "compressor_seq_b_release");
    reverseEnabled = parameters.getRawParameterValue ("reverse_enabled");
    reverseTime = parameters.getRawParameterValue ("reverse_time");
    reversePointA = parameters.getRawParameterValue ("reverse_point_a");
    reversePointB = parameters.getRawParameterValue ("reverse_point_b");
    reverseMix = parameters.getRawParameterValue ("reverse_mix");
    reversePlaybackMode = parameters.getRawParameterValue (
        "reverse_playback_mode");
    reverseSerialProfile = parameters.getRawParameterValue (
        "reverse_serial_profile");
    reverseStartStep = parameters.getRawParameterValue (
        "reverse_start_step");
    reverseEndStep = parameters.getRawParameterValue ("reverse_end_step");
    reverseRangeLength = parameters.getRawParameterValue (
        "reverse_range_length");
    reverseRangeLink = parameters.getRawParameterValue (
        "reverse_range_link");
    reverseRate = parameters.getRawParameterValue ("reverse_rate");
    reverseSequenceMode = parameters.getRawParameterValue (
        "reverse_sequence_mode");
    reverseSeqAEnabled = parameters.getRawParameterValue (
        "reverse_seq_a_enabled");
    reverseSeqABipolar = parameters.getRawParameterValue (
        "reverse_seq_a_bipolar");
    reverseSeqAAttack = parameters.getRawParameterValue (
        "reverse_seq_a_attack");
    reverseSeqARelease = parameters.getRawParameterValue (
        "reverse_seq_a_release");
    reverseSeqBEnabled = parameters.getRawParameterValue (
        "reverse_seq_b_enabled");
    reverseSeqBBipolar = parameters.getRawParameterValue (
        "reverse_seq_b_bipolar");
    reverseSeqBAttack = parameters.getRawParameterValue (
        "reverse_seq_b_attack");
    reverseSeqBRelease = parameters.getRawParameterValue (
        "reverse_seq_b_release");
    retriggerEnabled = parameters.getRawParameterValue ("retrigger_enabled");
    retriggerInitialSpeed = parameters.getRawParameterValue (
        "retrigger_initial_speed");
    retriggerFinalSpeed = parameters.getRawParameterValue (
        "retrigger_final_speed");
    retriggerTransition = parameters.getRawParameterValue (
        "retrigger_transition");
    retriggerDecay = parameters.getRawParameterValue ("retrigger_decay");
    retriggerMix = parameters.getRawParameterValue ("retrigger_mix");
    retriggerPlaybackMode = parameters.getRawParameterValue (
        "retrigger_playback_mode");
    retriggerSerialProfile = parameters.getRawParameterValue (
        "retrigger_serial_profile");
    retriggerStartStep = parameters.getRawParameterValue (
        "retrigger_start_step");
    retriggerEndStep = parameters.getRawParameterValue (
        "retrigger_end_step");
    retriggerRangeLength = parameters.getRawParameterValue (
        "retrigger_range_length");
    retriggerRangeLink = parameters.getRawParameterValue (
        "retrigger_range_link");
    retriggerRate = parameters.getRawParameterValue ("retrigger_rate");
    retriggerSequenceMode = parameters.getRawParameterValue (
        "retrigger_sequence_mode");
    retriggerSeqAEnabled = parameters.getRawParameterValue (
        "retrigger_seq_a_enabled");
    retriggerSeqABipolar = parameters.getRawParameterValue (
        "retrigger_seq_a_bipolar");
    retriggerSeqAAttack = parameters.getRawParameterValue (
        "retrigger_seq_a_attack");
    retriggerSeqARelease = parameters.getRawParameterValue (
        "retrigger_seq_a_release");
    retriggerSeqBEnabled = parameters.getRawParameterValue (
        "retrigger_seq_b_enabled");
    retriggerSeqBBipolar = parameters.getRawParameterValue (
        "retrigger_seq_b_bipolar");
    retriggerSeqBAttack = parameters.getRawParameterValue (
        "retrigger_seq_b_attack");
    retriggerSeqBRelease = parameters.getRawParameterValue (
        "retrigger_seq_b_release");
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

    for (int engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (int bank = 0; bank < 2; ++bank)
        {
            const auto bankIndex = static_cast<std::size_t> (bank);
            sequencerPeaks[static_cast<std::size_t> (engineIndex)][bankIndex] =
                parameters.getRawParameterValue (
                    sequencerLaneControlParameterID (engine, bank, "peak"));
            sequencerMoves[static_cast<std::size_t> (engineIndex)][bankIndex] =
                parameters.getRawParameterValue (
                    sequencerLaneControlParameterID (engine, bank, "move"));
        }
    }

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
        for (int lane = 0; lane < seqwencer::phiSequencerLaneCount; ++lane)
            phiSteps[static_cast<std::size_t> (lane)]
                    [static_cast<std::size_t> (step)] =
                parameters.getRawParameterValue (
                    phiStepParameterID (lane, step));
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
        distortionStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (distortionStepParameterID (0, step));
        distortionStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (distortionStepParameterID (1, step));
        grainStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (grainStepParameterID (0, step));
        grainStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (grainStepParameterID (1, step));
        compressorStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (compressorStepParameterID (0, step));
        compressorStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (compressorStepParameterID (1, step));
        reverseStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverseStepParameterID (0, step));
        reverseStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverseStepParameterID (1, step));
        reverseModesA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverseModeParameterID (0, step));
        reverseModesB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (reverseModeParameterID (1, step));
        retriggerStepsA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (retriggerStepParameterID (0, step));
        retriggerStepsB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (retriggerStepParameterID (1, step));
        retriggerModesA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (retriggerModeParameterID (0, step));
        retriggerModesB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (retriggerModeParameterID (1, step));
        gateModesA[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (gateModeParameterID (0, step));
        gateModesB[static_cast<std::size_t> (step)] =
            parameters.getRawParameterValue (gateModeParameterID (1, step));
    }

    parameters.state.setProperty ("step_encoding_version", 2, nullptr);
    parameters.state.setProperty ("step_encoding_a_bipolar", true, nullptr);
    parameters.state.setProperty ("step_encoding_b_bipolar", true, nullptr);
    setAudioFxOrder (seqwencer::defaultAudioFxOrder());
}

seqwencer::AudioFxOrder
SeqwencerAudioProcessor::getAudioFxOrder() const noexcept
{
    std::array<int, seqwencer::audioFxStageCount> values {};
    for (int index = 0; index < seqwencer::audioFxStageCount; ++index)
        values[static_cast<std::size_t> (index)] =
            audioFxOrder[static_cast<std::size_t> (index)].load (
                std::memory_order_acquire);
    return seqwencer::sanitiseAudioFxOrder (values);
}

void SeqwencerAudioProcessor::setAudioFxOrder (
    const seqwencer::AudioFxOrder& requestedOrder)
{
    std::array<int, seqwencer::audioFxStageCount> values {};
    for (int index = 0; index < seqwencer::audioFxStageCount; ++index)
        values[static_cast<std::size_t> (index)] = static_cast<int> (
            requestedOrder[static_cast<std::size_t> (index)]);
    const auto order = seqwencer::sanitiseAudioFxOrder (values);
    for (int index = 0; index < seqwencer::audioFxStageCount; ++index)
        audioFxOrder[static_cast<std::size_t> (index)].store (
            static_cast<int> (order[static_cast<std::size_t> (index)]),
            std::memory_order_release);

    const auto stored = serialiseAudioFxOrder (order);
    if (parameters.state.getProperty ("audio_fx_order").toString() != stored)
        parameters.state.setProperty ("audio_fx_order", stored, nullptr);
}

void SeqwencerAudioProcessor::moveAudioFxStage (
    seqwencer::AudioFxStage stage, int destinationIndex)
{
    setAudioFxOrder (seqwencer::moveAudioFxStage (
        getAudioFxOrder(), stage, destinationIndex));
}

seqwencer::StepSubdivisionPattern
SeqwencerAudioProcessor::getStepSubdivisions (
    seqwencer::SequencerEngine engine, int bank) const noexcept
{
    seqwencer::StepSubdivisionPattern result {};
    const auto engineIndex = static_cast<std::size_t> (juce::jlimit (
        0, seqwencer::sequencerEngineCount - 1,
        static_cast<int> (engine)));
    const auto bankIndex = static_cast<std::size_t> (juce::jlimit (0, 1, bank));
    const auto& source = stepSubdivisions[engineIndex][bankIndex];
    for (std::size_t step = 0; step < source.size(); ++step)
    {
        result[step].mode = seqwencer::stepDivisionModeFromChoice (
            source[step].mode.load (std::memory_order_acquire));
        for (std::size_t extra = 0;
             extra < result[step].extraValues.size(); ++extra)
        {
            result[step].extraValues[extra] = juce::jlimit (
                0.0f, 1.0f,
                source[step].extraValues[extra].load (
                    std::memory_order_acquire));
        }
    }
    return result;
}

void SeqwencerAudioProcessor::setStepSubdivisions (
    seqwencer::SequencerEngine engine, int bank,
    const seqwencer::StepSubdivisionPattern& subdivisions,
    bool notifyHost) noexcept
{
    const auto engineIndex = static_cast<std::size_t> (juce::jlimit (
        0, seqwencer::sequencerEngineCount - 1,
        static_cast<int> (engine)));
    const auto bankIndex = static_cast<std::size_t> (juce::jlimit (0, 1, bank));
    auto& destination = stepSubdivisions[engineIndex][bankIndex];
    for (std::size_t step = 0; step < destination.size(); ++step)
    {
        destination[step].mode.store (
            static_cast<int> (subdivisions[step].mode),
            std::memory_order_release);
        for (std::size_t extra = 0;
             extra < subdivisions[step].extraValues.size(); ++extra)
        {
            destination[step].extraValues[extra].store (
                juce::jlimit (0.0f, 1.0f,
                              subdivisions[step].extraValues[extra]),
                std::memory_order_release);
        }
    }
    if (notifyHost)
    {
        updateHostDisplay (
            juce::AudioProcessorListener::ChangeDetails {}
                .withNonParameterStateChanged (true));
    }
}

void SeqwencerAudioProcessor::setStepDivisionMode (
    seqwencer::SequencerEngine engine, int bank, int step,
    seqwencer::StepDivisionMode mode) noexcept
{
    auto subdivisions = getStepSubdivisions (engine, bank);
    subdivisions[static_cast<std::size_t> (juce::jlimit (
        0, seqwencer::stepsPerBank - 1, step))].mode = mode;
    setStepSubdivisions (engine, bank, subdivisions);
}

void SeqwencerAudioProcessor::setStepExtraValue (
    seqwencer::SequencerEngine engine, int bank, int step,
    int extraIndex, float canonicalValue) noexcept
{
    auto subdivisions = getStepSubdivisions (engine, bank);
    subdivisions[static_cast<std::size_t> (juce::jlimit (
        0, seqwencer::stepsPerBank - 1, step))]
        .extraValues[static_cast<std::size_t> (juce::jlimit (
            0, seqwencer::maximumSegmentsPerStep - 2, extraIndex))] =
                juce::jlimit (0.0f, 1.0f, canonicalValue);
    setStepSubdivisions (engine, bank, subdivisions);
}

seqwencer::StepSubdivisionPattern
SeqwencerAudioProcessor::readStepSubdivisions (
    seqwencer::SequencerEngine engine, int bank, bool bipolar) const noexcept
{
    auto result = getStepSubdivisions (engine, bank);
    for (auto& step : result)
        for (auto& value : step.extraValues)
            value = seqwencer::displayFromCanonical (value, bipolar);
    return result;
}

void SeqwencerAudioProcessor::resetStepSubdivisions() noexcept
{
    seqwencer::StepSubdivisionPattern defaults {};
    for (auto engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        for (auto bank = 0; bank < 2; ++bank)
        {
            setStepSubdivisions (
                static_cast<seqwencer::SequencerEngine> (engineIndex),
                bank, defaults, false);
        }
    }
}

void SeqwencerAudioProcessor::writeStepSubdivisionsToState (
    juce::ValueTree& state) const
{
    for (auto engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (auto bank = 0; bank < 2; ++bank)
        {
            const auto subdivisions = getStepSubdivisions (engine, bank);
            state.setProperty (
                subdivisionPropertyID (engine, bank, "modes"),
                serialiseSubdivisionModes (subdivisions), nullptr);
            state.setProperty (
                subdivisionPropertyID (engine, bank, "extra_2"),
                serialiseSubdivisionExtraValues (subdivisions, 0), nullptr);
            state.setProperty (
                subdivisionPropertyID (engine, bank, "extra_3"),
                serialiseSubdivisionExtraValues (subdivisions, 1), nullptr);
        }
    }
    state.setProperty ("subdivision_encoding_version", 1, nullptr);
}

void SeqwencerAudioProcessor::readStepSubdivisionsFromState (
    const juce::ValueTree& state) noexcept
{
    resetStepSubdivisions();
    for (auto engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (auto bank = 0; bank < 2; ++bank)
        {
            auto subdivisions = getStepSubdivisions (engine, bank);
            const auto modesID = subdivisionPropertyID (
                engine, bank, "modes");
            const auto extra2ID = subdivisionPropertyID (
                engine, bank, "extra_2");
            const auto extra3ID = subdivisionPropertyID (
                engine, bank, "extra_3");
            if (state.hasProperty (modesID))
                parseSubdivisionModes (
                    state.getProperty (modesID).toString(), subdivisions);
            if (state.hasProperty (extra2ID))
                parseSubdivisionExtraValues (
                    state.getProperty (extra2ID).toString(), 0,
                    subdivisions);
            if (state.hasProperty (extra3ID))
                parseSubdivisionExtraValues (
                    state.getProperty (extra3ID).toString(), 1,
                    subdivisions);
            setStepSubdivisions (engine, bank, subdivisions, false);
        }
    }
}

void SeqwencerAudioProcessor::syncAudioFxOrderFromState()
{
    const auto stored = parameters.state.getProperty (
        "audio_fx_order").toString();
    setAudioFxOrder (stored.isNotEmpty()
        ? parseAudioFxOrder (stored)
        : seqwencer::defaultAudioFxOrder());
}

juce::String SeqwencerAudioProcessor::stepParameterID (int bank, int step)
{
    return "seq_" + juce::String (bank == 0 ? "a" : "b") + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::phiPairParameterID (
    int pair, const juce::String& suffix)
{
    pair = juce::jlimit (0, seqwencer::phiSequencerPairCount - 1, pair);
    if (pair == 0)
        return "phi_" + suffix;
    constexpr std::array<const char*, seqwencer::phiSequencerPairCount>
        pairNames { "ab", "cd", "ef", "gh" };
    return "phi_" + juce::String (pairNames[static_cast<std::size_t> (pair)])
         + "_" + suffix;
}

juce::String SeqwencerAudioProcessor::phiLaneParameterID (
    int lane, const juce::String& suffix)
{
    lane = juce::jlimit (0, seqwencer::phiSequencerLaneCount - 1, lane);
    return "phi_seq_" + juce::String::charToString (
        static_cast<juce::juce_wchar> ('a' + lane)) + "_" + suffix;
}

juce::String SeqwencerAudioProcessor::phiStepParameterID (int lane, int step)
{
    return phiLaneParameterID (lane, "step_")
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

juce::String SeqwencerAudioProcessor::distortionStepParameterID (
    int bank, int step)
{
    return "distortion_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::grainStepParameterID (
    int bank, int step)
{
    return "grain_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::compressorStepParameterID (
    int bank, int step)
{
    return "compressor_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::reverseStepParameterID (
    int bank, int step)
{
    return "reverse_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::reverseModeParameterID (
    int bank, int step)
{
    return "reverse_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_reverse_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::retriggerStepParameterID (
    int bank, int step)
{
    return "retrigger_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_step_"
         + juce::String (juce::jlimit (0, seqwencer::stepsPerBank - 1, step) + 1)
               .paddedLeft ('0', 2);
}

juce::String SeqwencerAudioProcessor::retriggerModeParameterID (
    int bank, int step)
{
    return "retrigger_seq_" + juce::String (bank == 0 ? "a" : "b")
         + "_retrigger_"
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
    if (seqwencer::isSequencerEnvelopeTarget (target))
        return sequencerEnvelopeTargetParameterID (bank, target, false);
    if (seqwencer::isSequencerRangeTarget (target))
        return sequencerRangeTargetParameterID (bank, target, false);
    if (seqwencer::isSequencerTransformTarget (target))
        return sequencerTransformTargetParameterID (bank, target, false);

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
        case seqwencer::ModulationTarget::distortionDrive:
            return juce::String (bank == 0 ? "distortion_seq_a_drive_target"
                                           : "distortion_seq_b_drive_target");
        case seqwencer::ModulationTarget::distortionTone:
            return juce::String (bank == 0 ? "distortion_seq_a_tone_target"
                                           : "distortion_seq_b_tone_target");
        case seqwencer::ModulationTarget::distortionMix:
            return juce::String (bank == 0 ? "distortion_seq_a_mix_target"
                                           : "distortion_seq_b_mix_target");
        case seqwencer::ModulationTarget::grainSize:
            return juce::String (bank == 0 ? "grain_seq_a_size_target"
                                           : "grain_seq_b_size_target");
        case seqwencer::ModulationTarget::grainShift:
            return juce::String (bank == 0 ? "grain_seq_a_shift_target"
                                           : "grain_seq_b_shift_target");
        case seqwencer::ModulationTarget::grainFeedback:
            return juce::String (bank == 0 ? "grain_seq_a_feedback_target"
                                           : "grain_seq_b_feedback_target");
        case seqwencer::ModulationTarget::grainMix:
            return juce::String (bank == 0 ? "grain_seq_a_mix_target"
                                           : "grain_seq_b_mix_target");
        case seqwencer::ModulationTarget::compressorThreshold:
            return juce::String (
                bank == 0 ? "compressor_seq_a_threshold_target"
                          : "compressor_seq_b_threshold_target");
        case seqwencer::ModulationTarget::compressorRatio:
            return juce::String (bank == 0 ? "compressor_seq_a_ratio_target"
                                           : "compressor_seq_b_ratio_target");
        case seqwencer::ModulationTarget::compressorAttack:
            return juce::String (bank == 0 ? "compressor_seq_a_attack_target"
                                           : "compressor_seq_b_attack_target");
        case seqwencer::ModulationTarget::compressorRelease:
            return juce::String (
                bank == 0 ? "compressor_seq_a_release_target"
                          : "compressor_seq_b_release_target");
        case seqwencer::ModulationTarget::compressorMakeup:
            return juce::String (bank == 0 ? "compressor_seq_a_makeup_target"
                                           : "compressor_seq_b_makeup_target");
        case seqwencer::ModulationTarget::compressorMix:
            return juce::String (bank == 0 ? "compressor_seq_a_mix_target"
                                           : "compressor_seq_b_mix_target");
        case seqwencer::ModulationTarget::reverseTime:
            return juce::String (bank == 0 ? "reverse_seq_a_time_target"
                                           : "reverse_seq_b_time_target");
        case seqwencer::ModulationTarget::reversePointA:
            return juce::String (bank == 0 ? "reverse_seq_a_point_a_target"
                                           : "reverse_seq_b_point_a_target");
        case seqwencer::ModulationTarget::reversePointB:
            return juce::String (bank == 0 ? "reverse_seq_a_point_b_target"
                                           : "reverse_seq_b_point_b_target");
        case seqwencer::ModulationTarget::reverseMix:
            return juce::String (bank == 0 ? "reverse_seq_a_mix_target"
                                           : "reverse_seq_b_mix_target");
        case seqwencer::ModulationTarget::retriggerInitialSpeed:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_initial_speed_target"
                          : "retrigger_seq_b_initial_speed_target");
        case seqwencer::ModulationTarget::retriggerFinalSpeed:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_final_speed_target"
                          : "retrigger_seq_b_final_speed_target");
        case seqwencer::ModulationTarget::retriggerTransition:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_transition_target"
                          : "retrigger_seq_b_transition_target");
        case seqwencer::ModulationTarget::retriggerDecay:
            return juce::String (bank == 0
                ? "retrigger_seq_a_decay_target"
                : "retrigger_seq_b_decay_target");
        case seqwencer::ModulationTarget::retriggerMix:
            return juce::String (bank == 0 ? "retrigger_seq_a_mix_target"
                                           : "retrigger_seq_b_mix_target");
        case seqwencer::ModulationTarget::none:
            break;
        default:
            break;
    }
    return {};
}

juce::String SeqwencerAudioProcessor::targetEnabledParameterID (
    int bank, seqwencer::ModulationTarget target)
{
    if (seqwencer::isSequencerEnvelopeTarget (target))
        return sequencerEnvelopeTargetParameterID (bank, target, true);
    if (seqwencer::isSequencerRangeTarget (target))
        return sequencerRangeTargetParameterID (bank, target, true);
    if (seqwencer::isSequencerTransformTarget (target))
        return sequencerTransformTargetParameterID (bank, target, true);

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
        case seqwencer::ModulationTarget::distortionDrive:
            return juce::String (
                bank == 0 ? "distortion_seq_a_drive_target_enabled"
                          : "distortion_seq_b_drive_target_enabled");
        case seqwencer::ModulationTarget::distortionTone:
            return juce::String (
                bank == 0 ? "distortion_seq_a_tone_target_enabled"
                          : "distortion_seq_b_tone_target_enabled");
        case seqwencer::ModulationTarget::distortionMix:
            return juce::String (
                bank == 0 ? "distortion_seq_a_mix_target_enabled"
                          : "distortion_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::grainSize:
            return juce::String (
                bank == 0 ? "grain_seq_a_size_target_enabled"
                          : "grain_seq_b_size_target_enabled");
        case seqwencer::ModulationTarget::grainShift:
            return juce::String (
                bank == 0 ? "grain_seq_a_shift_target_enabled"
                          : "grain_seq_b_shift_target_enabled");
        case seqwencer::ModulationTarget::grainFeedback:
            return juce::String (
                bank == 0 ? "grain_seq_a_feedback_target_enabled"
                          : "grain_seq_b_feedback_target_enabled");
        case seqwencer::ModulationTarget::grainMix:
            return juce::String (
                bank == 0 ? "grain_seq_a_mix_target_enabled"
                          : "grain_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::compressorThreshold:
            return juce::String (
                bank == 0 ? "compressor_seq_a_threshold_target_enabled"
                          : "compressor_seq_b_threshold_target_enabled");
        case seqwencer::ModulationTarget::compressorRatio:
            return juce::String (
                bank == 0 ? "compressor_seq_a_ratio_target_enabled"
                          : "compressor_seq_b_ratio_target_enabled");
        case seqwencer::ModulationTarget::compressorAttack:
            return juce::String (
                bank == 0 ? "compressor_seq_a_attack_target_enabled"
                          : "compressor_seq_b_attack_target_enabled");
        case seqwencer::ModulationTarget::compressorRelease:
            return juce::String (
                bank == 0 ? "compressor_seq_a_release_target_enabled"
                          : "compressor_seq_b_release_target_enabled");
        case seqwencer::ModulationTarget::compressorMakeup:
            return juce::String (
                bank == 0 ? "compressor_seq_a_makeup_target_enabled"
                          : "compressor_seq_b_makeup_target_enabled");
        case seqwencer::ModulationTarget::compressorMix:
            return juce::String (
                bank == 0 ? "compressor_seq_a_mix_target_enabled"
                          : "compressor_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::reverseTime:
            return juce::String (
                bank == 0 ? "reverse_seq_a_time_target_enabled"
                          : "reverse_seq_b_time_target_enabled");
        case seqwencer::ModulationTarget::reversePointA:
            return juce::String (
                bank == 0 ? "reverse_seq_a_point_a_target_enabled"
                          : "reverse_seq_b_point_a_target_enabled");
        case seqwencer::ModulationTarget::reversePointB:
            return juce::String (
                bank == 0 ? "reverse_seq_a_point_b_target_enabled"
                          : "reverse_seq_b_point_b_target_enabled");
        case seqwencer::ModulationTarget::reverseMix:
            return juce::String (
                bank == 0 ? "reverse_seq_a_mix_target_enabled"
                          : "reverse_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::retriggerInitialSpeed:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_initial_speed_target_enabled"
                          : "retrigger_seq_b_initial_speed_target_enabled");
        case seqwencer::ModulationTarget::retriggerFinalSpeed:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_final_speed_target_enabled"
                          : "retrigger_seq_b_final_speed_target_enabled");
        case seqwencer::ModulationTarget::retriggerTransition:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_transition_target_enabled"
                          : "retrigger_seq_b_transition_target_enabled");
        case seqwencer::ModulationTarget::retriggerDecay:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_decay_target_enabled"
                          : "retrigger_seq_b_decay_target_enabled");
        case seqwencer::ModulationTarget::retriggerMix:
            return juce::String (
                bank == 0 ? "retrigger_seq_a_mix_target_enabled"
                          : "retrigger_seq_b_mix_target_enabled");
        case seqwencer::ModulationTarget::none:
            break;
        default:
            break;
    }
    return {};
}

juce::String SeqwencerAudioProcessor::targetDisplayName (
    seqwencer::ModulationTarget target)
{
    if (seqwencer::isSequencerEnvelopeTarget (target))
    {
        return juce::String (
                   seqwencer::sequencerEnvelopeTargetBank (target) == 0
                       ? "A " : "B ")
            + (seqwencer::sequencerEnvelopeTargetIsAttack (target)
                   ? "ATTACK" : "RELEASE");
    }
    if (seqwencer::isSequencerRangeTarget (target))
        return seqwencer::sequencerRangeTargetIsStart (target)
            ? "START"
            : (seqwencer::sequencerRangeTargetIsEnd (target)
                   ? "END" : "LENGTH");
    if (seqwencer::isSequencerTransformTarget (target))
    {
        return juce::String (
                   seqwencer::sequencerTransformTargetBank (target) == 0
                       ? "A " : "B ")
            + (seqwencer::sequencerTransformTargetIsPeak (target)
                   ? "PEAK" : "MOVE");
    }

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
        case seqwencer::ModulationTarget::distortionDrive: return "DRIVE";
        case seqwencer::ModulationTarget::distortionTone: return "TONE";
        case seqwencer::ModulationTarget::distortionMix: return "MIX";
        case seqwencer::ModulationTarget::grainSize: return "GRAIN";
        case seqwencer::ModulationTarget::grainShift: return "SHIFT";
        case seqwencer::ModulationTarget::grainFeedback: return "FEEDBACK";
        case seqwencer::ModulationTarget::grainMix: return "MIX";
        case seqwencer::ModulationTarget::compressorThreshold: return "THRESHOLD";
        case seqwencer::ModulationTarget::compressorRatio: return "RATIO";
        case seqwencer::ModulationTarget::compressorAttack: return "ATTACK";
        case seqwencer::ModulationTarget::compressorRelease: return "RELEASE";
        case seqwencer::ModulationTarget::compressorMakeup: return "MAKEUP";
        case seqwencer::ModulationTarget::compressorMix: return "MIX";
        case seqwencer::ModulationTarget::reverseTime: return "TIME";
        case seqwencer::ModulationTarget::reversePointA: return "POINT A";
        case seqwencer::ModulationTarget::reversePointB: return "POINT B";
        case seqwencer::ModulationTarget::reverseMix: return "MIX";
        case seqwencer::ModulationTarget::retriggerInitialSpeed:
            return "INITIAL";
        case seqwencer::ModulationTarget::retriggerFinalSpeed:
            return "FINAL";
        case seqwencer::ModulationTarget::retriggerTransition:
            return "TRANSITION";
        case seqwencer::ModulationTarget::retriggerDecay: return "DECAY";
        case seqwencer::ModulationTarget::retriggerMix: return "MIX";
        case seqwencer::ModulationTarget::none:            break;
        default:                                            break;
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

juce::File SeqwencerAudioProcessor::getPortableSequenceDirectory()
{
    return getPortableDataDirectory().getChildFile ("Sequences");
}

bool SeqwencerAudioProcessor::savePortableSequence (
    const juce::File& file,
    seqwencer::SequencerEngine engine,
    int bank,
    juce::String& errorMessage)
{
    errorMessage.clear();
    if (bank < 0 || bank > 1)
    {
        errorMessage = "That sequencer cannot be saved.";
        return false;
    }

    const auto rootDirectory = getPortableSequenceDirectory();
    if (file.getParentDirectory() != rootDirectory
        && ! file.isAChildOf (rootDirectory))
    {
        errorMessage = "Sequences must be saved inside:\n"
                     + rootDirectory.getFullPathName();
        return false;
    }

    const auto directoryResult = file.getParentDirectory().createDirectory();
    if (directoryResult.failed())
    {
        errorMessage = directoryResult.getErrorMessage();
        return false;
    }

    seqwencer::Pattern steps {};
    for (auto step = 0; step < seqwencer::stepsPerBank; ++step)
    {
        auto* parameter = parameters.getParameter (
            stepParameterIDForEngine (engine, bank, step));
        if (parameter == nullptr)
        {
            errorMessage = "A sequencer parameter required for Save was not found.";
            return false;
        }
        steps[static_cast<std::size_t> (step)] = juce::jlimit (
            0.0f, 1.0f, parameter->convertFrom0to1 (parameter->getValue()));
    }

    const auto subdivisions = getStepSubdivisions (engine, bank);
    juce::String contents { "; Seqwencer user sequence\r\n" };
    contents << "[Sequence]\r\nFormatVersion=1\r\nName="
             << file.getFileNameWithoutExtension()
             << "\r\nSteps=" << serialiseSequenceSteps (steps)
             << "\r\nSubdivisionModes="
             << serialiseSubdivisionModes (subdivisions)
             << "\r\nSubdivisionExtra2="
             << serialiseSubdivisionExtraValues (subdivisions, 0)
             << "\r\nSubdivisionExtra3="
             << serialiseSubdivisionExtraValues (subdivisions, 1)
             << "\r\n";

    if (! file.replaceWithText (contents))
    {
        errorMessage = "Seqwencer could not write:\n" + file.getFullPathName();
        return false;
    }
    return true;
}

bool SeqwencerAudioProcessor::loadPortableSequence (
    const juce::File& file,
    seqwencer::SequencerEngine engine,
    int bank,
    juce::String& errorMessage)
{
    errorMessage.clear();
    if (bank < 0 || bank > 1)
    {
        errorMessage = "That sequencer cannot be loaded.";
        return false;
    }
    if (! file.existsAsFile())
    {
        errorMessage = "Sequence not found:\n" + file.getFullPathName();
        return false;
    }

    auto validSection = false;
    auto validVersion = false;
    auto stepsLoaded = false;
    auto modesLoaded = false;
    auto extra2Loaded = false;
    auto extra3Loaded = false;
    seqwencer::Pattern loadedSteps {};
    seqwencer::StepSubdivisionPattern loadedSubdivisions {};

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    for (auto line : lines)
    {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar (';')
            || line.startsWithChar ('#'))
        {
            continue;
        }
        if (line.startsWithChar ('['))
        {
            validSection = line == "[Sequence]";
            continue;
        }
        if (! validSection)
            continue;

        const auto separator = line.indexOfChar ('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring (0, separator).trim();
        const auto value = line.substring (separator + 1).trim();
        if (key == "FormatVersion")
            validVersion = value.getIntValue() == 1;
        else if (key == "Steps")
            stepsLoaded = parseSequenceSteps (value, loadedSteps);
        else if (key == "SubdivisionModes")
            modesLoaded = parseSequenceSubdivisionModes (
                value, loadedSubdivisions);
        else if (key == "SubdivisionExtra2")
            extra2Loaded = parseSequenceSubdivisionExtraValues (
                value, 0, loadedSubdivisions);
        else if (key == "SubdivisionExtra3")
            extra3Loaded = parseSequenceSubdivisionExtraValues (
                value, 1, loadedSubdivisions);
    }

    if (! validVersion || ! stepsLoaded || ! modesLoaded
        || ! extra2Loaded || ! extra3Loaded)
    {
        errorMessage = file.getFileName()
                     + " is not a valid Seqwencer sequence.";
        return false;
    }

    std::array<juce::RangedAudioParameter*, seqwencer::stepsPerBank>
        stepParameters {};
    for (auto step = 0; step < seqwencer::stepsPerBank; ++step)
    {
        auto* parameter = parameters.getParameter (
            stepParameterIDForEngine (engine, bank, step));
        if (parameter == nullptr)
        {
            errorMessage = "A sequencer parameter required for Load was not found.";
            return false;
        }
        stepParameters[static_cast<std::size_t> (step)] = parameter;
    }

    for (std::size_t step = 0; step < stepParameters.size(); ++step)
    {
        auto* parameter = stepParameters[step];
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (
            parameter->convertTo0to1 (loadedSteps[step]));
        parameter->endChangeGesture();
    }
    setStepSubdivisions (engine, bank, loadedSubdivisions);
    return true;
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
    juce::String distortionParameterLines;
    juce::String grainParameterLines;
    juce::String compressorParameterLines;
    juce::String reverseParameterLines;
    juce::String retriggerParameterLines;
    juce::String subdivisionLines;
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
            else if (parameterID.startsWith ("distortion_"))
                destination = &distortionParameterLines;
            else if (parameterID.startsWith ("grain_"))
                destination = &grainParameterLines;
            else if (parameterID.startsWith ("compressor_"))
                destination = &compressorParameterLines;
            else if (parameterID.startsWith ("reverse_"))
                destination = &reverseParameterLines;
            else if (parameterID.startsWith ("retrigger_"))
                destination = &retriggerParameterLines;
            else if (parameterID == "bypass" || parameterID == "sync_to_host")
                destination = &globalParameterLines;

            *destination << parameterID << "="
                         << juce::String (parameter->getValue(), 7) << "\r\n";
            ++parameterCount;
        }
    }

    for (auto engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (auto bank = 0; bank < 2; ++bank)
        {
            const auto subdivisions = getStepSubdivisions (engine, bank);
            subdivisionLines
                << subdivisionPropertyID (engine, bank, "modes") << "="
                << serialiseSubdivisionModes (subdivisions) << "\r\n"
                << subdivisionPropertyID (engine, bank, "extra_2") << "="
                << serialiseSubdivisionExtraValues (subdivisions, 0)
                << "\r\n"
                << subdivisionPropertyID (engine, bank, "extra_3") << "="
                << serialiseSubdivisionExtraValues (subdivisions, 1)
                << "\r\n";
        }
    }

    juce::String contents { "; Seqwencer portable preset\r\n" };
    contents << "[Preset]\r\nFormatVersion=1\r\nName=" << presetName
             << "\r\nParameterCount=" << parameterCount
             << "\r\n\r\n[Global]\r\n" << globalParameterLines
             << "\r\n[Routing]\r\nOrder="
             << serialiseAudioFxOrder (getAudioFxOrder()) << "\r\n"
             << "\r\n[Gate]\r\n" << gateParameterLines
             << "\r\n[PHI]\r\n" << phiParameterLines
             << "\r\n[Delay]\r\n" << delayParameterLines
             << "\r\n[Reverb]\r\n" << reverbParameterLines
             << "\r\n[Pan]\r\n" << panParameterLines
             << "\r\n[Filter]\r\n" << filterParameterLines
             << "\r\n[Pitch]\r\n" << pitchParameterLines
             << "\r\n[Distortion]\r\n" << distortionParameterLines
             << "\r\n[GrainShifter]\r\n" << grainParameterLines
             << "\r\n[Compressor]\r\n" << compressorParameterLines
             << "\r\n[Reverse]\r\n" << reverseParameterLines
             << "\r\n[Retrigger]\r\n" << retriggerParameterLines
             << "\r\n[Subdivisions]\r\n" << subdivisionLines;
    if (! file.replaceWithText (contents))
    {
        errorMessage = "Seqwencer could not write:\n" + file.getFullPathName();
        return false;
    }

    parameters.state.setProperty ("presetName", presetName, nullptr);
    parameters.state.setProperty (
        "presetFileName", file.getFileName(), nullptr);
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
    auto inRouting = false;
    auto inSubdivisions = false;
    auto validVersion = false;
    auto routingWasLoaded = false;
    auto loadedFxOrder = seqwencer::defaultAudioFxOrder();
    std::array<std::array<seqwencer::StepSubdivisionPattern, 2>,
               seqwencer::sequencerEngineCount> loadedSubdivisions {};
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
            inRouting = false;
            inSubdivisions = false;
            continue;
        }
        if (line == "[Routing]")
        {
            inParameters = false;
            inRouting = true;
            inSubdivisions = false;
            continue;
        }
        if (line == "[Subdivisions]")
        {
            inParameters = false;
            inRouting = false;
            inSubdivisions = true;
            continue;
        }
        if (line == "[Parameters]" || line == "[Global]"
            || line == "[Gate]" || line == "[PHI]" || line == "[Delay]"
            || line == "[Reverb]" || line == "[Pan]" || line == "[Filter]"
            || line == "[Pitch]" || line == "[Distortion]"
            || line == "[GrainShifter]" || line == "[Compressor]"
            || line == "[Reverse]" || line == "[Retrigger]")
        {
            inParameters = true;
            inRouting = false;
            inSubdivisions = false;
            continue;
        }
        const auto separator = line.indexOfChar ('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring (0, separator).trim();
        const auto value = line.substring (separator + 1).trim();
        if (inRouting)
        {
            if (key == "Order")
            {
                loadedFxOrder = parseAudioFxOrder (value);
                routingWasLoaded = true;
            }
            continue;
        }
        if (inSubdivisions)
        {
            for (auto engineIndex = 0;
                 engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
            {
                const auto engine = static_cast<seqwencer::SequencerEngine> (
                    engineIndex);
                for (auto bank = 0; bank < 2; ++bank)
                {
                    auto& subdivisions = loadedSubdivisions[
                        static_cast<std::size_t> (engineIndex)]
                        [static_cast<std::size_t> (bank)];
                    if (key == subdivisionPropertyID (
                                   engine, bank, "modes"))
                        parseSubdivisionModes (value, subdivisions);
                    else if (key == subdivisionPropertyID (
                                        engine, bank, "extra_2"))
                        parseSubdivisionExtraValues (
                            value, 0, subdivisions);
                    else if (key == subdivisionPropertyID (
                                        engine, bank, "extra_3"))
                        parseSubdivisionExtraValues (
                            value, 1, subdivisions);
                }
            }
            continue;
        }
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
    setAudioFxOrder (routingWasLoaded
        ? loadedFxOrder : seqwencer::defaultAudioFxOrder());
    for (auto engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (auto bank = 0; bank < 2; ++bank)
        {
            setStepSubdivisions (
                engine, bank,
                loadedSubdivisions[static_cast<std::size_t> (engineIndex)]
                                  [static_cast<std::size_t> (bank)],
                false);
        }
    }
    updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails {}
            .withNonParameterStateChanged (true));
    parameters.state.setProperty (
        "presetName", loadedName.isNotEmpty()
            ? loadedName : file.getFileNameWithoutExtension(), nullptr);
    parameters.state.setProperty (
        "presetFileName", file.getFileName(), nullptr);
    return true;
}

bool SeqwencerAudioProcessor::restoreSequenceFromCurrentPreset (
    seqwencer::SequencerEngine engine, int bank,
    bool stepModesOnly, juce::String& errorMessage)
{
    if (bank < 0 || bank > 1
        || (stepModesOnly
            && engine != seqwencer::SequencerEngine::gate
            && engine != seqwencer::SequencerEngine::retrigger))
    {
        errorMessage = "That sequencer cannot be reset.";
        return false;
    }

    const auto stepID = [engine] (int stepBank, int stepIndex)
    {
        switch (engine)
        {
            case seqwencer::SequencerEngine::phi:
            case seqwencer::SequencerEngine::phiCD:
            case seqwencer::SequencerEngine::phiEF:
            case seqwencer::SequencerEngine::phiGH:
                return phiStepParameterID (
                    seqwencer::phiLaneForEngineBank (
                        engine, stepBank), stepIndex);
            case seqwencer::SequencerEngine::delay:
                return delayStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::reverb:
                return reverbStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::pan:
                return panStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::filter:
                return filterStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::pitch:
                return pitchStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::distortion:
                return distortionStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::grain:
                return grainStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::compressor:
                return compressorStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::reverse:
                return reverseStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::retrigger:
                return retriggerStepParameterID (stepBank, stepIndex);
            case seqwencer::SequencerEngine::gate:
                return stepParameterID (stepBank, stepIndex);
        }
        return juce::String();
    };

    std::array<juce::RangedAudioParameter*, seqwencer::stepsPerBank>
        parametersToRestore {};
    std::array<juce::String, seqwencer::stepsPerBank> parameterIDs {};
    for (int step = 0; step < seqwencer::stepsPerBank; ++step)
    {
        const auto index = static_cast<std::size_t> (step);
        parameterIDs[index] = stepModesOnly
            ? (engine == seqwencer::SequencerEngine::gate
                ? gateModeParameterID (bank, step)
                : retriggerModeParameterID (bank, step))
            : stepID (bank, step);
        parametersToRestore[index] = parameters.getParameter (
            parameterIDs[index]);
        if (parametersToRestore[index] == nullptr)
        {
            errorMessage = "A sequencer parameter required by Reset was not found.";
            return false;
        }
    }

    std::array<float, seqwencer::stepsPerBank> restoredValues {};
    for (std::size_t index = 0; index < restoredValues.size(); ++index)
        restoredValues[index] = parametersToRestore[index]->getDefaultValue();
    seqwencer::StepSubdivisionPattern restoredSubdivisions {};

    const auto presetName = parameters.state.getProperty (
        "presetName", "INITIAL").toString();
    if (! presetName.equalsIgnoreCase ("INITIAL"))
    {
        auto fileName = parameters.state.getProperty (
            "presetFileName").toString();
        if (fileName.isEmpty())
            fileName = sanitisePresetName (presetName) + ".ini";
        const auto file = getPortablePresetDirectory().getChildFile (fileName);
        if (! file.existsAsFile())
        {
            errorMessage = "The current preset file was not found:\n"
                         + file.getFullPathName();
            return false;
        }

        juce::StringArray lines;
        lines.addLines (file.loadFileAsString());
        for (auto line : lines)
        {
            line = line.trim();
            if (line.isEmpty() || line.startsWithChar (';')
                || line.startsWithChar ('#') || line.startsWithChar ('['))
                continue;
            const auto separator = line.indexOfChar ('=');
            if (separator <= 0)
                continue;
            const auto key = line.substring (0, separator).trim();
            const auto value = line.substring (separator + 1).trim();
            for (std::size_t index = 0; index < parameterIDs.size(); ++index)
                if (key == parameterIDs[index])
                    restoredValues[index] = juce::jlimit (
                        0.0f, 1.0f, value.getFloatValue());
            if (! stepModesOnly)
            {
                if (key == subdivisionPropertyID (engine, bank, "modes"))
                    parseSubdivisionModes (value, restoredSubdivisions);
                else if (key == subdivisionPropertyID (
                                    engine, bank, "extra_2"))
                    parseSubdivisionExtraValues (
                        value, 0, restoredSubdivisions);
                else if (key == subdivisionPropertyID (
                                    engine, bank, "extra_3"))
                    parseSubdivisionExtraValues (
                        value, 1, restoredSubdivisions);
            }
        }
    }

    for (std::size_t index = 0; index < parametersToRestore.size(); ++index)
    {
        auto* parameter = parametersToRestore[index];
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (restoredValues[index]);
        parameter->endChangeGesture();
    }
    if (! stepModesOnly)
        setStepSubdivisions (engine, bank, restoredSubdivisions);
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
    setAudioFxOrder (seqwencer::defaultAudioFxOrder());
    resetStepSubdivisions();
    updateHostDisplay (
        juce::AudioProcessorListener::ChangeDetails {}
            .withNonParameterStateChanged (true));
    parameters.state.setProperty ("presetName", "INITIAL", nullptr);
    parameters.state.removeProperty ("presetFileName", nullptr);
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
    layout.add (std::make_unique<Int> (
        ID { "range_length", 1 }, "Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "range_link", 1 }, "Range Link", false));
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
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));
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
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::gateDepth);
             index <= static_cast<int> (
                 seqwencer::ModulationTarget::noiseGateRange);
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
    layout.add (std::make_unique<Int> (
        ID { "phi_range_length", 1 }, "PHI Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "phi_range_link", 1 }, "PHI Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_a_bipolar", 1 }, "PHI Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "phi_seq_b_bipolar", 1 }, "PHI Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "phi_sequence_mode", 1 }, "PHI Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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
    layout.add (std::make_unique<Int> (
        ID { "delay_range_length", 1 }, "Delay Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "delay_range_link", 1 }, "Delay Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_a_bipolar", 1 }, "Delay Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "delay_seq_b_bipolar", 1 }, "Delay Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "delay_sequence_mode", 1 }, "Delay Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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
    layout.add (std::make_unique<Int> (
        ID { "reverb_range_length", 1 }, "Reverb Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_range_link", 1 }, "Reverb Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_a_bipolar", 1 }, "Reverb Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "reverb_seq_b_bipolar", 1 }, "Reverb Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "reverb_sequence_mode", 1 }, "Reverb Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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
    layout.add (std::make_unique<Int> (
        ID { "pan_range_length", 1 }, "Pan Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "pan_range_link", 1 }, "Pan Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_a_bipolar", 1 }, "Pan Sequencer A Bipolar", true));
    layout.add (std::make_unique<Bool> (
        ID { "pan_seq_b_bipolar", 1 }, "Pan Sequencer B Bipolar", true));
    layout.add (std::make_unique<Choice> (
        ID { "pan_sequence_mode", 1 }, "Pan Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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
                            "Band Reject", "Peaking", "Comb" }, 0));
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
    layout.add (std::make_unique<Int> (
        ID { "filter_range_length", 1 }, "Filter Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "filter_range_link", 1 }, "Filter Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_a_bipolar", 1 }, "Filter Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "filter_seq_b_bipolar", 1 }, "Filter Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "filter_sequence_mode", 1 }, "Filter Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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
    layout.add (std::make_unique<Int> (
        ID { "pitch_range_length", 1 }, "Pitch Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_range_link", 1 }, "Pitch Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_a_bipolar", 1 }, "Pitch Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "pitch_seq_b_bipolar", 1 }, "Pitch Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "pitch_sequence_mode", 1 }, "Pitch Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

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

    // Distortion owns an eighth complete sequencer engine and three targets.
    layout.add (std::make_unique<Bool> (
        ID { "distortion_enabled", 1 }, "Distortion Enabled", false));
    layout.add (std::make_unique<Choice> (
        ID { "distortion_type", 1 }, "Distortion Type",
        juce::StringArray { "Soft Clip", "Hard Clip", "Tube", "Foldback" },
        0));
    layout.add (std::make_unique<Float> (
        ID { "distortion_drive", 1 }, "Distortion Drive",
        juce::NormalisableRange<float> { 0.0f, 36.0f, 0.01f }, 6.0f));
    layout.add (std::make_unique<Float> (
        ID { "distortion_tone", 1 }, "Distortion Tone",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Float> (
        ID { "distortion_mix", 1 }, "Distortion Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "distortion_playback_mode", 1 }, "Distortion Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "distortion_rate", 1 }, "Distortion Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "distortion_seq_a_enabled", 1 },
        "Distortion Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "distortion_seq_a_attack", 1 },
        "Distortion Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "distortion_seq_a_release", 1 },
        "Distortion Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "distortion_seq_b_enabled", 1 },
        "Distortion Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "distortion_seq_b_attack", 1 },
        "Distortion Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "distortion_seq_b_release", 1 },
        "Distortion Sequencer B Release", 0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { distortionStepParameterID (bank, step), 1 },
                "Distortion Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "distortion_serial_profile", 1 },
        "Distortion Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "distortion_start_step", 1 }, "Distortion Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "distortion_end_step", 1 }, "Distortion End Step", 2, 64, 64));
    layout.add (std::make_unique<Int> (
        ID { "distortion_range_length", 1 }, "Distortion Range Length",
        2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "distortion_range_link", 1 }, "Distortion Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "distortion_seq_a_bipolar", 1 },
        "Distortion Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "distortion_seq_b_bipolar", 1 },
        "Distortion Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "distortion_sequence_mode", 1 }, "Distortion Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Distortion Sequencer A " : "Distortion Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::distortionDrive);
             index <= static_cast<int> (
                 seqwencer::ModulationTarget::distortionMix);
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

    // Grain Shifter owns a ninth complete sequencer engine and four targets.
    layout.add (std::make_unique<Bool> (
        ID { "grain_enabled", 1 }, "Grain Shifter Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "grain_size", 1 }, "Grain Size",
        juce::NormalisableRange<float> { 10.0f, 250.0f, 0.1f }, 80.0f));
    layout.add (std::make_unique<Float> (
        ID { "grain_shift", 1 }, "Grain Shift",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 12.0f));
    layout.add (std::make_unique<Float> (
        ID { "grain_feedback", 1 }, "Grain Feedback",
        juce::NormalisableRange<float> { 0.0f, 0.90f, 0.001f }, 0.20f));
    layout.add (std::make_unique<Float> (
        ID { "grain_mix", 1 }, "Grain Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.35f));
    layout.add (std::make_unique<Choice> (
        ID { "grain_playback_mode", 1 }, "Grain Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "grain_rate", 1 }, "Grain Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "grain_seq_a_enabled", 1 },
        "Grain Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "grain_seq_a_attack", 1 },
        "Grain Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "grain_seq_a_release", 1 },
        "Grain Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "grain_seq_b_enabled", 1 },
        "Grain Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "grain_seq_b_attack", 1 },
        "Grain Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "grain_seq_b_release", 1 },
        "Grain Sequencer B Release", 0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { grainStepParameterID (bank, step), 1 },
                "Grain Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "grain_serial_profile", 1 },
        "Grain Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "grain_start_step", 1 }, "Grain Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "grain_end_step", 1 }, "Grain End Step", 2, 64, 64));
    layout.add (std::make_unique<Int> (
        ID { "grain_range_length", 1 }, "Grain Range Length", 2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "grain_range_link", 1 }, "Grain Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "grain_seq_a_bipolar", 1 },
        "Grain Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "grain_seq_b_bipolar", 1 },
        "Grain Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "grain_sequence_mode", 1 }, "Grain Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Grain Sequencer A " : "Grain Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::grainSize);
             index <= static_cast<int> (seqwencer::ModulationTarget::grainMix);
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

    // Compressor owns a tenth complete sequencer engine and six targets.
    layout.add (std::make_unique<Bool> (
        ID { "compressor_enabled", 1 }, "Compressor Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "compressor_threshold", 1 }, "Compressor Threshold",
        juce::NormalisableRange<float> { -60.0f, 0.0f, 0.1f }, -18.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_ratio", 1 }, "Compressor Ratio",
        juce::NormalisableRange<float> { 1.0f, 20.0f, 0.1f }, 4.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_attack", 1 }, "Compressor Attack",
        juce::NormalisableRange<float> { 0.1f, 100.0f, 0.1f }, 10.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_release", 1 }, "Compressor Release",
        juce::NormalisableRange<float> { 10.0f, 1000.0f, 1.0f }, 100.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_makeup", 1 }, "Compressor Makeup",
        juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f }, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_mix", 1 }, "Compressor Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "compressor_playback_mode", 1 }, "Compressor Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "compressor_rate", 1 }, "Compressor Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "compressor_seq_a_enabled", 1 },
        "Compressor Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "compressor_seq_a_attack", 1 },
        "Compressor Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_seq_a_release", 1 },
        "Compressor Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "compressor_seq_b_enabled", 1 },
        "Compressor Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "compressor_seq_b_attack", 1 },
        "Compressor Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "compressor_seq_b_release", 1 },
        "Compressor Sequencer B Release", 0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { compressorStepParameterID (bank, step), 1 },
                "Compressor Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "compressor_serial_profile", 1 },
        "Compressor Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "compressor_start_step", 1 }, "Compressor Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "compressor_end_step", 1 }, "Compressor End Step", 2, 64, 64));
    layout.add (std::make_unique<Int> (
        ID { "compressor_range_length", 1 }, "Compressor Range Length",
        2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "compressor_range_link", 1 }, "Compressor Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "compressor_seq_a_bipolar", 1 },
        "Compressor Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "compressor_seq_b_bipolar", 1 },
        "Compressor Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "compressor_sequence_mode", 1 }, "Compressor Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Compressor Sequencer A " : "Compressor Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::compressorThreshold);
             index <= static_cast<int> (
                 seqwencer::ModulationTarget::compressorMix);
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

    // Reverse owns an eleventh complete sequencer engine and four targets.
    layout.add (std::make_unique<Bool> (
        ID { "reverse_enabled", 1 }, "Reverse Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "reverse_time", 1 }, "Reverse Time",
        juce::NormalisableRange<float> { 25.0f, 2000.0f, 1.0f, 0.45f },
        500.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverse_point_a", 1 }, "Reverse Point A",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverse_point_b", 1 }, "Reverse Point B",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverse_mix", 1 }, "Reverse Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "reverse_playback_mode", 1 }, "Reverse Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "reverse_rate", 1 }, "Reverse Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "reverse_seq_a_enabled", 1 },
        "Reverse Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "reverse_seq_a_attack", 1 },
        "Reverse Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverse_seq_a_release", 1 },
        "Reverse Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "reverse_seq_b_enabled", 1 },
        "Reverse Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "reverse_seq_b_attack", 1 },
        "Reverse Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "reverse_seq_b_release", 1 },
        "Reverse Sequencer B Release", 0.0f, 1.0f, 0.0f));

    for (int bank = 0; bank < 2; ++bank)
    {
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto bankName = bank == 0 ? "A" : "B";
            layout.add (std::make_unique<Float> (
                ID { reverseStepParameterID (bank, step), 1 },
                "Reverse Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }

    layout.add (std::make_unique<Choice> (
        ID { "reverse_serial_profile", 1 },
        "Reverse Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "reverse_start_step", 1 }, "Reverse Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "reverse_end_step", 1 }, "Reverse End Step", 2, 64, 64));
    layout.add (std::make_unique<Int> (
        ID { "reverse_range_length", 1 }, "Reverse Range Length",
        2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "reverse_range_link", 1 }, "Reverse Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "reverse_seq_a_bipolar", 1 },
        "Reverse Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "reverse_seq_b_bipolar", 1 },
        "Reverse Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "reverse_sequence_mode", 1 }, "Reverse Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" }, 0));

    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Reverse Sequencer A " : "Reverse Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::reverseTime);
             index <= static_cast<int> (
                 seqwencer::ModulationTarget::reverseMix);
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

    constexpr std::array<seqwencer::SequencerEngine, 10>
        internalSequencerEngines {
            seqwencer::SequencerEngine::gate,
            seqwencer::SequencerEngine::delay,
            seqwencer::SequencerEngine::reverb,
            seqwencer::SequencerEngine::pan,
            seqwencer::SequencerEngine::filter,
            seqwencer::SequencerEngine::pitch,
            seqwencer::SequencerEngine::distortion,
            seqwencer::SequencerEngine::grain,
            seqwencer::SequencerEngine::compressor,
            seqwencer::SequencerEngine::reverse
        };
    for (const auto engine : internalSequencerEngines)
    {
        const auto envelopeTargets = seqwencer::sequencerEnvelopeTargets (
            engine);
        const auto rangeTargets = seqwencer::sequencerRangeTargets (engine);
        for (int bank = 0; bank < 2; ++bank)
        {
            const auto laneName = sequencerEngineDisplayName (engine)
                + " Sequencer " + (bank == 0 ? "A " : "B ");
            for (const auto target : envelopeTargets)
            {
                const auto displayName = targetDisplayName (target);
                layout.add (std::make_unique<Bool> (
                    ID { targetAssignedParameterID (bank, target), 1 },
                    laneName + displayName + " Target", false));
                layout.add (std::make_unique<Bool> (
                    ID { targetEnabledParameterID (bank, target), 1 },
                    laneName + displayName + " Target Enabled", true));
            }
            for (const auto target : rangeTargets)
            {
                const auto displayName = targetDisplayName (target);
                layout.add (std::make_unique<Bool> (
                    ID { targetAssignedParameterID (bank, target), 1 },
                    laneName + displayName + " Target", false));
                layout.add (std::make_unique<Bool> (
                    ID { targetEnabledParameterID (bank, target), 1 },
                    laneName + displayName + " Target Enabled", true));
            }
        }
    }

    // Keep these appended after every existing parameter so older parameter
    // indices remain stable. New and previously saved projects start Off.
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto bankName = bank == 0 ? "A" : "B";
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            layout.add (std::make_unique<Bool> (
                ID { reverseModeParameterID (bank, step), 1 },
                "Reverse Sequencer " + juce::String (bankName)
                    + " Reverse Step " + juce::String (step + 1),
                false));
        }
    }

    // Retrigger is appended after every v1.3.21.1 parameter so existing
    // automation indices remain stable.
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_enabled", 1 }, "Retrigger Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_initial_speed", 1 }, "Retrigger Initial Speed",
        juce::NormalisableRange<float> { 1.0f, 16.0f, 1.0f }, 2.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_final_speed", 1 }, "Retrigger Final Speed",
        juce::NormalisableRange<float> { 1.0f, 16.0f, 1.0f }, 2.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_transition", 1 }, "Retrigger Transition",
        juce::NormalisableRange<float> { 0.0f, 16.0f, 1.0f }, 4.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_decay", 1 }, "Retrigger Decay",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_mix", 1 }, "Retrigger Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));
    layout.add (std::make_unique<Choice> (
        ID { "retrigger_playback_mode", 1 }, "Retrigger Playback Mode",
        juce::StringArray { "Parallel", "Serial" }, 0));
    layout.add (std::make_unique<Choice> (
        ID { "retrigger_rate", 1 }, "Retrigger Rate",
        juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                            "1/32", "1/16T", "1/16", "1/8T",
                            "1/8", "1/4T", "1/4", "1/2T",
                            "1/2", "1/1" }, 6));
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_seq_a_enabled", 1 },
        "Retrigger Sequencer A Enabled", true));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_seq_a_attack", 1 },
        "Retrigger Sequencer A Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_seq_a_release", 1 },
        "Retrigger Sequencer A Release", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_seq_b_enabled", 1 },
        "Retrigger Sequencer B Enabled", false));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_seq_b_attack", 1 },
        "Retrigger Sequencer B Attack", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<Float> (
        ID { "retrigger_seq_b_release", 1 },
        "Retrigger Sequencer B Release", 0.0f, 1.0f, 0.0f));
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto bankName = bank == 0 ? "A" : "B";
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            layout.add (std::make_unique<Float> (
                ID { retriggerStepParameterID (bank, step), 1 },
                "Retrigger Sequencer " + juce::String (bankName) + " Step "
                    + juce::String (step + 1),
                juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
        }
    }
    layout.add (std::make_unique<Choice> (
        ID { "retrigger_serial_profile", 1 },
        "Retrigger Serial Control Profile",
        juce::StringArray { "A", "B" }, 0));
    layout.add (std::make_unique<Int> (
        ID { "retrigger_start_step", 1 }, "Retrigger Start Step", 1, 63, 1));
    layout.add (std::make_unique<Int> (
        ID { "retrigger_end_step", 1 }, "Retrigger End Step", 2, 64, 64));
    layout.add (std::make_unique<Int> (
        ID { "retrigger_range_length", 1 }, "Retrigger Range Length",
        2, 64, 64));
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_range_link", 1 }, "Retrigger Range Link", false));
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_seq_a_bipolar", 1 },
        "Retrigger Sequencer A Bipolar", false));
    layout.add (std::make_unique<Bool> (
        ID { "retrigger_seq_b_bipolar", 1 },
        "Retrigger Sequencer B Bipolar", false));
    layout.add (std::make_unique<Choice> (
        ID { "retrigger_sequence_mode", 1 }, "Retrigger Direction",
        juce::StringArray { "Loop", "Bounce", "Reverse", "Played", "Random" },
        0));
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto laneName = bank == 0
            ? "Retrigger Sequencer A " : "Retrigger Sequencer B ";
        for (int index = static_cast<int> (
                 seqwencer::ModulationTarget::retriggerInitialSpeed);
             index <= static_cast<int> (
                 seqwencer::ModulationTarget::retriggerMix);
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

        for (const auto target : seqwencer::sequencerEnvelopeTargets (
                 seqwencer::SequencerEngine::retrigger))
        {
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
        for (const auto target : seqwencer::sequencerRangeTargets (
                 seqwencer::SequencerEngine::retrigger))
        {
            const auto displayName = targetDisplayName (target);
            layout.add (std::make_unique<Bool> (
                ID { targetAssignedParameterID (bank, target), 1 },
                laneName + displayName + " Target", false));
            layout.add (std::make_unique<Bool> (
                ID { targetEnabledParameterID (bank, target), 1 },
                laneName + displayName + " Target Enabled", true));
        }
    }

    // Retrigger step switches are appended after every v1.3.22.0 parameter
    // so all existing automation indices remain stable. They deliberately
    // default Off, leaving the effect silent until a block is drawn.
    for (int bank = 0; bank < 2; ++bank)
    {
        const auto bankName = bank == 0 ? "A" : "B";
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            layout.add (std::make_unique<Bool> (
                ID { retriggerModeParameterID (bank, step), 1 },
                "Retrigger Sequencer " + juce::String (bankName)
                    + " Retrigger Step " + juce::String (step + 1),
                false));
        }
    }

    // PHI pairs C/D, E/F and G/H are appended after every v1.3.23.0
    // parameter. Existing A/B IDs and every existing automation index remain
    // unchanged.
    for (int pair = 1; pair < seqwencer::phiSequencerPairCount; ++pair)
    {
        const auto firstLane = pair * 2;
        const auto firstName = juce::String::charToString (
            static_cast<juce::juce_wchar> ('A' + firstLane));
        const auto secondName = juce::String::charToString (
            static_cast<juce::juce_wchar> ('A' + firstLane + 1));
        const auto pairName = firstName + "/" + secondName;

        layout.add (std::make_unique<Choice> (
            ID { phiPairParameterID (pair, "playback_mode"), 1 },
            "PHI " + pairName + " Playback Mode",
            juce::StringArray { "Parallel", "Serial" }, 0));
        layout.add (std::make_unique<Choice> (
            ID { phiPairParameterID (pair, "rate"), 1 },
            "PHI " + pairName + " Rate",
            juce::StringArray { "1/128", "1/64T", "1/64", "1/32T",
                                "1/32", "1/16T", "1/16", "1/8T",
                                "1/8", "1/4T", "1/4", "1/2T",
                                "1/2", "1/1" }, 6));

        for (int localLane = 0; localLane < 2; ++localLane)
        {
            const auto lane = firstLane + localLane;
            const auto laneName = localLane == 0 ? firstName : secondName;
            layout.add (std::make_unique<Bool> (
                ID { phiLaneParameterID (lane, "enabled"), 1 },
                "PHI Sequencer " + laneName + " Enabled",
                localLane == 0));
            layout.add (std::make_unique<Float> (
                ID { phiLaneParameterID (lane, "attack"), 1 },
                "PHI Sequencer " + laneName + " Attack",
                0.0f, 1.0f, 0.0f));
            layout.add (std::make_unique<Float> (
                ID { phiLaneParameterID (lane, "release"), 1 },
                "PHI Sequencer " + laneName + " Release",
                0.0f, 1.0f, 0.0f));
            for (int step = 0; step < seqwencer::stepsPerBank; ++step)
            {
                layout.add (std::make_unique<Float> (
                    ID { phiStepParameterID (lane, step), 1 },
                    "PHI Sequencer " + laneName + " Step "
                        + juce::String (step + 1),
                    juce::NormalisableRange<float> { 0.0f, 1.0f }, 1.0f));
            }
        }

        layout.add (std::make_unique<Choice> (
            ID { phiPairParameterID (pair, "serial_profile"), 1 },
            "PHI " + pairName + " Serial Control Profile",
            juce::StringArray { firstName, secondName }, 0));
        layout.add (std::make_unique<Int> (
            ID { phiPairParameterID (pair, "start_step"), 1 },
            "PHI " + pairName + " Start Step", 1, 63, 1));
        layout.add (std::make_unique<Int> (
            ID { phiPairParameterID (pair, "end_step"), 1 },
            "PHI " + pairName + " End Step", 2, 64, 64));
        layout.add (std::make_unique<Int> (
            ID { phiPairParameterID (pair, "range_length"), 1 },
            "PHI " + pairName + " Range Length", 2, 64, 64));
        layout.add (std::make_unique<Bool> (
            ID { phiPairParameterID (pair, "range_link"), 1 },
            "PHI " + pairName + " Range Link", false));
        for (int localLane = 0; localLane < 2; ++localLane)
        {
            const auto lane = firstLane + localLane;
            const auto laneName = localLane == 0 ? firstName : secondName;
            layout.add (std::make_unique<Bool> (
                ID { phiLaneParameterID (lane, "bipolar"), 1 },
                "PHI Sequencer " + laneName + " Bipolar", false));
        }
        layout.add (std::make_unique<Choice> (
            ID { phiPairParameterID (pair, "sequence_mode"), 1 },
            "PHI " + pairName + " Direction",
            juce::StringArray { "Loop", "Bounce", "Reverse", "Played",
                                "Random" }, 0));
    }

    // Peak and Move are appended after every v1.3.24.0 parameter so existing
    // automation indices remain stable. A neutral value of zero preserves the
    // exact sound of older projects and presets.
    for (int engineIndex = 0;
         engineIndex < seqwencer::sequencerEngineCount; ++engineIndex)
    {
        const auto engine = static_cast<seqwencer::SequencerEngine> (
            engineIndex);
        for (int bank = 0; bank < 2; ++bank)
        {
            juce::String laneName;
            if (seqwencer::isPhiSequencerEngine (engine))
            {
                const auto lane = seqwencer::phiLaneForEngineBank (
                    engine, bank);
                laneName = "PHI Sequencer "
                    + juce::String::charToString (
                        static_cast<juce::juce_wchar> ('A' + lane));
            }
            else
            {
                laneName = sequencerEngineDisplayName (engine)
                    + " Sequencer " + (bank == 0 ? "A" : "B");
            }

            layout.add (std::make_unique<Float> (
                ID { sequencerLaneControlParameterID (
                         engine, bank, "peak"), 1 },
                laneName + " Peak",
                juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
                0.0f));
            layout.add (std::make_unique<Float> (
                ID { sequencerLaneControlParameterID (
                         engine, bank, "move"), 1 },
                laneName + " Move",
                juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f },
                0.0f));
        }
    }

    constexpr std::array<seqwencer::SequencerEngine, 11> internalEngines {
        seqwencer::SequencerEngine::gate,
        seqwencer::SequencerEngine::delay,
        seqwencer::SequencerEngine::reverb,
        seqwencer::SequencerEngine::pan,
        seqwencer::SequencerEngine::filter,
        seqwencer::SequencerEngine::pitch,
        seqwencer::SequencerEngine::distortion,
        seqwencer::SequencerEngine::grain,
        seqwencer::SequencerEngine::compressor,
        seqwencer::SequencerEngine::reverse,
        seqwencer::SequencerEngine::retrigger
    };
    for (const auto engine : internalEngines)
    {
        for (int bank = 0; bank < 2; ++bank)
        {
            const auto sourceName = sequencerEngineDisplayName (engine)
                + " Sequencer " + (bank == 0 ? "A " : "B ");
            for (const auto target :
                 seqwencer::sequencerTransformTargets (engine))
            {
                const auto displayName = targetDisplayName (target);
                layout.add (std::make_unique<Bool> (
                    ID { targetAssignedParameterID (bank, target), 1 },
                    sourceName + displayName + " Target", false));
                layout.add (std::make_unique<Bool> (
                    ID { targetEnabledParameterID (bank, target), 1 },
                    sourceName + displayName + " Target Enabled", true));
            }
        }
    }

    return layout;
}

void SeqwencerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    gateFreeRunningPhase = 0.0;
    phiFreeRunningPhases.fill (0.0);
    delayFreeRunningPhase = 0.0;
    reverbFreeRunningPhase = 0.0;
    panFreeRunningPhase = 0.0;
    filterFreeRunningPhase = 0.0;
    pitchFreeRunningPhase = 0.0;
    distortionFreeRunningPhase = 0.0;
    grainFreeRunningPhase = 0.0;
    compressorFreeRunningPhase = 0.0;
    reverseFreeRunningPhase = 0.0;
    retriggerFreeRunningPhase = 0.0;
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
    for (int lane = 0; lane < seqwencer::phiSequencerLaneCount; ++lane)
        phiActiveSteps[static_cast<std::size_t> (lane)].store (
            (lane & 1) == 0 ? 0 : -1);
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
    distortionActiveStepA.store (0);
    distortionActiveStepB.store (-1);
    grainActiveStepA.store (0);
    grainActiveStepB.store (-1);
    compressorActiveStepA.store (0);
    compressorActiveStepB.store (-1);
    reverseActiveStepA.store (0);
    reverseActiveStepB.store (-1);
    retriggerActiveStepA.store (0);
    retriggerActiveStepB.store (-1);
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
    const auto filterCombBufferLength = static_cast<int> (
        std::ceil (currentSampleRate / 20.0)) + 8;
    filterCombBuffer.setSize (
        juce::jmax (1, getTotalNumOutputChannels()),
        filterCombBufferLength, false, true, false);
    resetFilterProcessor();
    filterPreviousType = -1;
    filterWasActive = false;
    const auto pitchBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 0.10)) + 8;
    pitchBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                         pitchBufferLength, false, true, false);
    pitchBuffer.clear();
    pitchWritePosition = 0;
    pitchReadPhase = 0.0;
    pitchWasActive = false;
    distortionToneStates.fill (0.0f);
    distortionWasActive = false;
    const auto grainBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 0.30)) + 8;
    grainBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()),
                         grainBufferLength, false, true, false);
    grainBuffer.clear();
    grainWritePosition = 0;
    grainReadPhase = 0.0;
    grainFeedbackStates.fill (0.0f);
    grainWasActive = false;
    compressorGain = 1.0f;
    compressorWasActive = false;
    const auto reverseBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 2.05)) + 8;
    for (auto& reverseBuffer : reverseBuffers)
    {
        reverseBuffer.setSize (
            juce::jmax (1, getTotalNumOutputChannels()),
            reverseBufferLength, false, true, false);
        reverseBuffer.clear();
    }
    resetReverseProcessor();
    const auto retriggerBufferLength = static_cast<int> (
        std::ceil (currentSampleRate * 8.05)) + 8;
    retriggerBuffer.setSize (
        juce::jmax (1, getTotalNumOutputChannels()),
        retriggerBufferLength, false, true, false);
    retriggerBuffer.clear();
    resetRetriggerProcessor();
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
    filterPreviousType = -1;
    filterWasActive = false;
    pitchBuffer.clear();
    pitchWritePosition = 0;
    pitchReadPhase = 0.0;
    pitchWasActive = false;
    distortionToneStates.fill (0.0f);
    distortionWasActive = false;
    grainBuffer.clear();
    grainWritePosition = 0;
    grainReadPhase = 0.0;
    grainFeedbackStates.fill (0.0f);
    grainWasActive = false;
    compressorGain = 1.0f;
    compressorWasActive = false;
    for (auto& reverseBuffer : reverseBuffers)
        reverseBuffer.clear();
    resetReverseProcessor();
    retriggerBuffer.clear();
    resetRetriggerProcessor();
}

void SeqwencerAudioProcessor::resetReverseProcessor() noexcept
{
    reverseCaptureBufferIndex = 0;
    reversePlaybackBufferIndex = 1;
    reverseCaptureWritePosition = 0;
    reverseCaptureAvailableSamples = 0;
    reverseCapturedSamples = 0;
    reversePendingCaptureSamples = 2;
    reversePlaybackStartPosition = 0;
    reverseReadPosition = 0.0;
    reversePreviousSequencePhase = -1.0;
    reverseReadDirection = 1;
    reverseWetFade = 0.0f;
    reverseIsPlaying = false;
    reverseStepWasOn = false;
    reverseWasActive = false;
}

void SeqwencerAudioProcessor::resetRetriggerProcessor() noexcept
{
    retriggerCaptureSamples = 0;
    retriggerCaptureTargetSamples = 2;
    retriggerReadPosition = 0;
    retriggerSamplesUntilRepeat = 0;
    retriggerPlaybackSamples = 0;
    retriggerRepeatIndex = 0;
    retriggerWetFade = 0.0f;
    retriggerIsPlaying = false;
    retriggerStepWasOn = false;
    retriggerBlockIsActive = false;
    retriggerWasActive = false;
}

void SeqwencerAudioProcessor::resetFilterProcessor() noexcept
{
    for (auto& state : filterStates)
        state = {};
    filterCombBuffer.clear();
    filterCombWritePositions.fill (0);
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

    if (type == 5 && filterCombBuffer.getNumSamples() > 3)
    {
        const auto channelIndex = static_cast<std::size_t> (juce::jlimit (
            0, static_cast<int> (filterCombWritePositions.size()) - 1,
            channel));
        const auto bufferChannel = juce::jlimit (
            0, filterCombBuffer.getNumChannels() - 1, channel);
        const auto capacity = filterCombBuffer.getNumSamples();
        auto& writePosition = filterCombWritePositions[channelIndex];
        const auto delaySamples = seqwencer::combDelaySamples (
            safeSampleRate, static_cast<float> (safeCutoff), capacity);
        auto readPosition = static_cast<double> (writePosition) - delaySamples;
        while (readPosition < 0.0)
            readPosition += static_cast<double> (capacity);
        const auto index0 = static_cast<int> (std::floor (readPosition))
                          % capacity;
        const auto index1 = (index0 + 1) % capacity;
        const auto fraction = static_cast<float> (
            readPosition - std::floor (readPosition));
        const auto delayed0 = filterCombBuffer.getSample (
            bufferChannel, index0);
        const auto delayed1 = filterCombBuffer.getSample (
            bufferChannel, index1);
        const auto delayed = delayed0 + fraction * (delayed1 - delayed0);
        const auto feedback = seqwencer::combFeedbackFromResonance (resonance);
        const auto feedbackSample = input + feedback * delayed;
        filterCombBuffer.setSample (
            bufferChannel, writePosition, feedbackSample);
        writePosition = (writePosition + 1) % capacity;
        return (1.0f - feedback) * feedbackSample;
    }

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

    switch (juce::jlimit (0, 5, type))
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
    int lane, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = phiSteps[static_cast<std::size_t> (juce::jlimit (
        0, seqwencer::phiSequencerLaneCount - 1, lane))];

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

seqwencer::Pattern SeqwencerAudioProcessor::readDistortionPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? distortionStepsA : distortionStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readGrainPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? grainStepsA : grainStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readCompressorPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? compressorStepsA : compressorStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readReversePattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? reverseStepsA : reverseStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::Pattern SeqwencerAudioProcessor::readRetriggerPattern (
    int bank, bool bipolar) const noexcept
{
    seqwencer::Pattern result {};
    const auto& source = bank == 0 ? retriggerStepsA : retriggerStepsB;

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        const auto canonical = juce::jlimit (
            0.0f, 1.0f, source[i] != nullptr ? source[i]->load() : 1.0f);
        result[i] = seqwencer::displayFromCanonical (canonical, bipolar);
    }
    return result;
}

seqwencer::ReverseStepPattern SeqwencerAudioProcessor::readReverseModes (
    int bank) const noexcept
{
    seqwencer::ReverseStepPattern result {};
    const auto& source = bank == 0 ? reverseModesA : reverseModesB;

    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = source[i] != nullptr && source[i]->load() >= 0.5f;
    return result;
}

seqwencer::RetriggerStepPattern SeqwencerAudioProcessor::readRetriggerModes (
    int bank) const noexcept
{
    seqwencer::RetriggerStepPattern result {};
    const auto& source = bank == 0 ? retriggerModesA : retriggerModesB;

    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = source[i] != nullptr && source[i]->load() >= 0.5f;
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
    const auto gateUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::gate, 0, false);
    const auto gateUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::gate, 1, false);
    const auto gateCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::gate, 0, true);
    const auto gateCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::gate, 1, true);

    struct SequencerTransformBlockState
    {
        std::array<float, seqwencer::sequencerTransformTargetCount>
            baseValues {};
        std::array<seqwencer::SequencerValueBounds, 2> unipolarBounds {};
        std::array<seqwencer::SequencerValueBounds, 2> canonicalBounds {};
    };
    const auto makeTransformBlockState = [&] (
        seqwencer::SequencerEngine engine,
        const seqwencer::Pattern& unipolarA,
        const seqwencer::Pattern& unipolarB,
        const seqwencer::Pattern& canonicalA,
        const seqwencer::Pattern& canonicalB,
        const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsA,
        const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsB,
        const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsA,
        const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsB)
    {
        SequencerTransformBlockState state;
        const auto engineIndex = static_cast<std::size_t> (engine);
        for (int bank = 0; bank < 2; ++bank)
        {
            const auto index = static_cast<std::size_t> (bank);
            state.baseValues[index * 2] = juce::jlimit (
                -1.0f, 1.0f,
                sequencerPeaks[engineIndex][index] != nullptr
                    ? sequencerPeaks[engineIndex][index]->load() : 0.0f);
            state.baseValues[index * 2 + 1] = juce::jlimit (
                -1.0f, 1.0f,
                sequencerMoves[engineIndex][index] != nullptr
                    ? sequencerMoves[engineIndex][index]->load() : 0.0f);
        }
        state.unipolarBounds[0] = seqwencer::sequencerValueBounds (
            unipolarA, unipolarSubdivisionsA, false);
        state.unipolarBounds[1] = seqwencer::sequencerValueBounds (
            unipolarB, unipolarSubdivisionsB, false);
        state.canonicalBounds[0] = seqwencer::sequencerValueBounds (
            canonicalA, canonicalSubdivisionsA, true);
        state.canonicalBounds[1] = seqwencer::sequencerValueBounds (
            canonicalB, canonicalSubdivisionsB, true);
        return state;
    };
    const auto gateTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::gate,
        gatePatternA, gatePatternB,
        gateCanonicalPatternA, gateCanonicalPatternB,
        gateUnipolarSubdivisionsA, gateUnipolarSubdivisionsB,
        gateCanonicalSubdivisionsA, gateCanonicalSubdivisionsB);
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

    struct PhiPairBlockState
    {
        std::array<float, 2> attack {};
        std::array<float, 2> release {};
        SequencerTransformBlockState transform;
        std::array<bool, 2> bipolar {};
        std::array<bool, 2> enabled {};
        std::array<seqwencer::Pattern, 2> pattern {};
        std::array<seqwencer::StepSubdivisionPattern, 2> subdivisions {};
        bool linked = false;
        bool usesSecondProfile = false;
        bool serialBipolar = false;
        float serialAttack = 0.0f;
        float serialRelease = 0.0f;
        double stepBeats = 0.25;
        seqwencer::SequenceMode traversalMode = seqwencer::SequenceMode::loop;
        bool retriggersFromPlayedNotes = false;
        bool useHostPosition = false;
        double phaseIncrement = 0.0;
        double phase = 0.0;
        seqwencer::StepRange range {};
        std::array<float, 2> lastValues { 1.0f, 1.0f };
    };
    std::array<PhiPairBlockState, seqwencer::phiSequencerPairCount> phiPairs;
    auto phiSerialPairMask = 0;
    for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
    {
        auto& state = phiPairs[static_cast<std::size_t> (pair)];
        const auto pairIndex = static_cast<std::size_t> (pair);
        const auto firstLane = pair * 2;
        state.linked = phiPlaybackModes[pairIndex] != nullptr
                    && phiPlaybackModes[pairIndex]->load() >= 0.5f;
        state.usesSecondProfile = phiSerialProfiles[pairIndex] != nullptr
                               && phiSerialProfiles[pairIndex]->load() >= 0.5f;
        if (state.linked)
            phiSerialPairMask |= 1 << pair;
        for (int localLane = 0; localLane < 2; ++localLane)
        {
            const auto lane = firstLane + localLane;
            const auto laneIndex = static_cast<std::size_t> (lane);
            state.attack[static_cast<std::size_t> (localLane)] = juce::jlimit (
                0.0f, 1.0f, phiSeqAttack[laneIndex] != nullptr
                    ? phiSeqAttack[laneIndex]->load() : 0.0f);
            state.release[static_cast<std::size_t> (localLane)] = juce::jlimit (
                0.0f, 1.0f, phiSeqRelease[laneIndex] != nullptr
                    ? phiSeqRelease[laneIndex]->load() : 0.0f);
            state.bipolar[static_cast<std::size_t> (localLane)] =
                phiSeqBipolar[laneIndex] != nullptr
                && phiSeqBipolar[laneIndex]->load() >= 0.5f;
            state.enabled[static_cast<std::size_t> (localLane)] =
                phiSeqEnabled[laneIndex] != nullptr
                && phiSeqEnabled[laneIndex]->load() >= 0.5f;
        }
        const auto profile = state.usesSecondProfile ? 1U : 0U;
        state.serialAttack = state.attack[profile];
        state.serialRelease = state.release[profile];
        state.serialBipolar = state.bipolar[profile];
        const auto engine = seqwencer::phiEngineForPair (pair);
        for (int localLane = 0; localLane < 2; ++localLane)
        {
            const auto index = static_cast<std::size_t> (localLane);
            const auto bipolar = state.linked
                ? state.serialBipolar : state.bipolar[index];
            state.pattern[index] = readPhiPattern (
                firstLane + localLane, bipolar);
            state.subdivisions[index] = readStepSubdivisions (
                engine, localLane, bipolar);
        }
        state.transform = makeTransformBlockState (
            engine,
            state.pattern[0], state.pattern[1],
            state.pattern[0], state.pattern[1],
            state.subdivisions[0], state.subdivisions[1],
            state.subdivisions[0], state.subdivisions[1]);
        const auto rateIndex = juce::jlimit (
            0, seqwencer::rateChoiceCount - 1,
            static_cast<int> (std::lround (
                phiRates[pairIndex] != nullptr
                    ? phiRates[pairIndex]->load() : 6.0f)));
        state.stepBeats = seqwencer::beatsForRate (rateIndex);
        state.traversalMode = seqwencer::sequenceModeFromChoice (
            phiSequenceModes[pairIndex] != nullptr
                ? phiSequenceModes[pairIndex]->load() : 0.0f);
        state.retriggersFromPlayedNotes =
            state.traversalMode == seqwencer::SequenceMode::played;
    }

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
    const auto delayUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::delay, 0, false);
    const auto delayUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::delay, 1, false);
    const auto delayCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::delay, 0, true);
    const auto delayCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::delay, 1, true);
    const auto delayTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::delay,
        delayUnipolarPatternA, delayUnipolarPatternB,
        delayCanonicalPatternA, delayCanonicalPatternB,
        delayUnipolarSubdivisionsA, delayUnipolarSubdivisionsB,
        delayCanonicalSubdivisionsA, delayCanonicalSubdivisionsB);
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
    const auto reverbUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::reverb, 0, false);
    const auto reverbUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::reverb, 1, false);
    const auto reverbCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::reverb, 0, true);
    const auto reverbCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::reverb, 1, true);
    const auto reverbTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::reverb,
        reverbUnipolarPatternA, reverbUnipolarPatternB,
        reverbCanonicalPatternA, reverbCanonicalPatternB,
        reverbUnipolarSubdivisionsA, reverbUnipolarSubdivisionsB,
        reverbCanonicalSubdivisionsA, reverbCanonicalSubdivisionsB);
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
    const auto panUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::pan, 0, false);
    const auto panUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::pan, 1, false);
    const auto panCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::pan, 0, true);
    const auto panCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::pan, 1, true);
    const auto panTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::pan,
        panUnipolarPatternA, panUnipolarPatternB,
        panCanonicalPatternA, panCanonicalPatternB,
        panUnipolarSubdivisionsA, panUnipolarSubdivisionsB,
        panCanonicalSubdivisionsA, panCanonicalSubdivisionsB);
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
    const auto filterUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::filter, 0, false);
    const auto filterUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::filter, 1, false);
    const auto filterCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::filter, 0, true);
    const auto filterCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::filter, 1, true);
    const auto filterTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::filter,
        filterUnipolarPatternA, filterUnipolarPatternB,
        filterCanonicalPatternA, filterCanonicalPatternB,
        filterUnipolarSubdivisionsA, filterUnipolarSubdivisionsB,
        filterCanonicalSubdivisionsA, filterCanonicalSubdivisionsB);
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
    const auto pitchUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::pitch, 0, false);
    const auto pitchUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::pitch, 1, false);
    const auto pitchCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::pitch, 0, true);
    const auto pitchCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::pitch, 1, true);
    const auto pitchTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::pitch,
        pitchUnipolarPatternA, pitchUnipolarPatternB,
        pitchCanonicalPatternA, pitchCanonicalPatternB,
        pitchUnipolarSubdivisionsA, pitchUnipolarSubdivisionsB,
        pitchCanonicalSubdivisionsA, pitchCanonicalSubdivisionsB);
    const auto pitchRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            pitchRate != nullptr ? pitchRate->load() : 6.0f)));
    const auto pitchStepBeats = seqwencer::beatsForRate (pitchRateIndex);
    const auto pitchTraversalMode = seqwencer::sequenceModeFromChoice (
        pitchSequenceMode != nullptr ? pitchSequenceMode->load() : 0.0f);
    const auto pitchRetriggersFromPlayedNotes =
        pitchTraversalMode == seqwencer::SequenceMode::played;

    const auto distortionAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        distortionSeqAAttack != nullptr ? distortionSeqAAttack->load() : 0.0f);
    const auto distortionReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        distortionSeqARelease != nullptr ? distortionSeqARelease->load() : 0.0f);
    const auto distortionAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        distortionSeqBAttack != nullptr ? distortionSeqBAttack->load() : 0.0f);
    const auto distortionReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        distortionSeqBRelease != nullptr ? distortionSeqBRelease->load() : 0.0f);
    const auto distortionLinked = distortionPlaybackMode != nullptr
                               && distortionPlaybackMode->load() >= 0.5f;
    const auto distortionUsesProfileB = distortionSerialProfile != nullptr
                                     && distortionSerialProfile->load() >= 0.5f;
    const auto distortionBipolarAValue = distortionSeqABipolar != nullptr
                                      && distortionSeqABipolar->load() >= 0.5f;
    const auto distortionBipolarBValue = distortionSeqBBipolar != nullptr
                                      && distortionSeqBBipolar->load() >= 0.5f;
    const auto distortionSerialBipolar = distortionUsesProfileB
        ? distortionBipolarBValue : distortionBipolarAValue;
    const auto distortionUnipolarPatternA = readDistortionPattern (0, false);
    const auto distortionUnipolarPatternB = readDistortionPattern (1, false);
    const auto distortionCanonicalPatternA = readDistortionPattern (0, true);
    const auto distortionCanonicalPatternB = readDistortionPattern (1, true);
    const auto distortionUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::distortion, 0, false);
    const auto distortionUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::distortion, 1, false);
    const auto distortionCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::distortion, 0, true);
    const auto distortionCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::distortion, 1, true);
    const auto distortionTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::distortion,
        distortionUnipolarPatternA, distortionUnipolarPatternB,
        distortionCanonicalPatternA, distortionCanonicalPatternB,
        distortionUnipolarSubdivisionsA, distortionUnipolarSubdivisionsB,
        distortionCanonicalSubdivisionsA,
        distortionCanonicalSubdivisionsB);
    const auto distortionRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            distortionRate != nullptr ? distortionRate->load() : 6.0f)));
    const auto distortionStepBeats = seqwencer::beatsForRate (distortionRateIndex);
    const auto distortionTraversalMode = seqwencer::sequenceModeFromChoice (
        distortionSequenceMode != nullptr ? distortionSequenceMode->load() : 0.0f);
    const auto distortionRetriggersFromPlayedNotes =
        distortionTraversalMode == seqwencer::SequenceMode::played;

    const auto grainAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        grainSeqAAttack != nullptr ? grainSeqAAttack->load() : 0.0f);
    const auto grainReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        grainSeqARelease != nullptr ? grainSeqARelease->load() : 0.0f);
    const auto grainAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        grainSeqBAttack != nullptr ? grainSeqBAttack->load() : 0.0f);
    const auto grainReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        grainSeqBRelease != nullptr ? grainSeqBRelease->load() : 0.0f);
    const auto grainLinked = grainPlaybackMode != nullptr
                          && grainPlaybackMode->load() >= 0.5f;
    const auto grainUsesProfileB = grainSerialProfile != nullptr
                                && grainSerialProfile->load() >= 0.5f;
    const auto grainBipolarAValue = grainSeqABipolar != nullptr
                                 && grainSeqABipolar->load() >= 0.5f;
    const auto grainBipolarBValue = grainSeqBBipolar != nullptr
                                 && grainSeqBBipolar->load() >= 0.5f;
    const auto grainSerialBipolar = grainUsesProfileB
        ? grainBipolarBValue : grainBipolarAValue;
    const auto grainUnipolarPatternA = readGrainPattern (0, false);
    const auto grainUnipolarPatternB = readGrainPattern (1, false);
    const auto grainCanonicalPatternA = readGrainPattern (0, true);
    const auto grainCanonicalPatternB = readGrainPattern (1, true);
    const auto grainUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::grain, 0, false);
    const auto grainUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::grain, 1, false);
    const auto grainCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::grain, 0, true);
    const auto grainCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::grain, 1, true);
    const auto grainTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::grain,
        grainUnipolarPatternA, grainUnipolarPatternB,
        grainCanonicalPatternA, grainCanonicalPatternB,
        grainUnipolarSubdivisionsA, grainUnipolarSubdivisionsB,
        grainCanonicalSubdivisionsA, grainCanonicalSubdivisionsB);
    const auto grainRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            grainRate != nullptr ? grainRate->load() : 6.0f)));
    const auto grainStepBeats = seqwencer::beatsForRate (grainRateIndex);
    const auto grainTraversalMode = seqwencer::sequenceModeFromChoice (
        grainSequenceMode != nullptr ? grainSequenceMode->load() : 0.0f);
    const auto grainRetriggersFromPlayedNotes =
        grainTraversalMode == seqwencer::SequenceMode::played;

    const auto compressorAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        compressorSeqAAttack != nullptr ? compressorSeqAAttack->load() : 0.0f);
    const auto compressorReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        compressorSeqARelease != nullptr ? compressorSeqARelease->load() : 0.0f);
    const auto compressorAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        compressorSeqBAttack != nullptr ? compressorSeqBAttack->load() : 0.0f);
    const auto compressorReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        compressorSeqBRelease != nullptr ? compressorSeqBRelease->load() : 0.0f);
    const auto compressorLinked = compressorPlaybackMode != nullptr
                               && compressorPlaybackMode->load() >= 0.5f;
    const auto compressorUsesProfileB = compressorSerialProfile != nullptr
                                     && compressorSerialProfile->load() >= 0.5f;
    const auto compressorBipolarAValue = compressorSeqABipolar != nullptr
                                      && compressorSeqABipolar->load() >= 0.5f;
    const auto compressorBipolarBValue = compressorSeqBBipolar != nullptr
                                      && compressorSeqBBipolar->load() >= 0.5f;
    const auto compressorSerialBipolar = compressorUsesProfileB
        ? compressorBipolarBValue : compressorBipolarAValue;
    const auto compressorUnipolarPatternA = readCompressorPattern (0, false);
    const auto compressorUnipolarPatternB = readCompressorPattern (1, false);
    const auto compressorCanonicalPatternA = readCompressorPattern (0, true);
    const auto compressorCanonicalPatternB = readCompressorPattern (1, true);
    const auto compressorUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::compressor, 0, false);
    const auto compressorUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::compressor, 1, false);
    const auto compressorCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::compressor, 0, true);
    const auto compressorCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::compressor, 1, true);
    const auto compressorTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::compressor,
        compressorUnipolarPatternA, compressorUnipolarPatternB,
        compressorCanonicalPatternA, compressorCanonicalPatternB,
        compressorUnipolarSubdivisionsA,
        compressorUnipolarSubdivisionsB,
        compressorCanonicalSubdivisionsA,
        compressorCanonicalSubdivisionsB);
    const auto compressorRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            compressorRate != nullptr ? compressorRate->load() : 6.0f)));
    const auto compressorStepBeats = seqwencer::beatsForRate (
        compressorRateIndex);
    const auto compressorTraversalMode = seqwencer::sequenceModeFromChoice (
        compressorSequenceMode != nullptr
            ? compressorSequenceMode->load() : 0.0f);
    const auto compressorRetriggersFromPlayedNotes =
        compressorTraversalMode == seqwencer::SequenceMode::played;

    const auto reverseAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        reverseSeqAAttack != nullptr ? reverseSeqAAttack->load() : 0.0f);
    const auto reverseReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        reverseSeqARelease != nullptr ? reverseSeqARelease->load() : 0.0f);
    const auto reverseAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        reverseSeqBAttack != nullptr ? reverseSeqBAttack->load() : 0.0f);
    const auto reverseReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        reverseSeqBRelease != nullptr ? reverseSeqBRelease->load() : 0.0f);
    const auto reverseLinked = reversePlaybackMode != nullptr
                            && reversePlaybackMode->load() >= 0.5f;
    const auto reverseUsesProfileB = reverseSerialProfile != nullptr
                                  && reverseSerialProfile->load() >= 0.5f;
    const auto reverseBipolarAValue = reverseSeqABipolar != nullptr
                                   && reverseSeqABipolar->load() >= 0.5f;
    const auto reverseBipolarBValue = reverseSeqBBipolar != nullptr
                                   && reverseSeqBBipolar->load() >= 0.5f;
    const auto reverseSerialBipolar = reverseUsesProfileB
        ? reverseBipolarBValue : reverseBipolarAValue;
    const auto reverseUnipolarPatternA = readReversePattern (0, false);
    const auto reverseUnipolarPatternB = readReversePattern (1, false);
    const auto reverseCanonicalPatternA = readReversePattern (0, true);
    const auto reverseCanonicalPatternB = readReversePattern (1, true);
    const auto reverseModesForA = readReverseModes (0);
    const auto reverseModesForB = readReverseModes (1);
    const auto reverseUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::reverse, 0, false);
    const auto reverseUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::reverse, 1, false);
    const auto reverseCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::reverse, 0, true);
    const auto reverseCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::reverse, 1, true);
    const auto reverseTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::reverse,
        reverseUnipolarPatternA, reverseUnipolarPatternB,
        reverseCanonicalPatternA, reverseCanonicalPatternB,
        reverseUnipolarSubdivisionsA, reverseUnipolarSubdivisionsB,
        reverseCanonicalSubdivisionsA, reverseCanonicalSubdivisionsB);
    const auto reverseRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            reverseRate != nullptr ? reverseRate->load() : 6.0f)));
    const auto reverseStepBeats = seqwencer::beatsForRate (reverseRateIndex);
    const auto reverseTraversalMode = seqwencer::sequenceModeFromChoice (
        reverseSequenceMode != nullptr ? reverseSequenceMode->load() : 0.0f);
    const auto reverseRetriggersFromPlayedNotes =
        reverseTraversalMode == seqwencer::SequenceMode::played;
    const auto retriggerAttackAValue = juce::jlimit (
        0.0f, 1.0f,
        retriggerSeqAAttack != nullptr ? retriggerSeqAAttack->load() : 0.0f);
    const auto retriggerReleaseAValue = juce::jlimit (
        0.0f, 1.0f,
        retriggerSeqARelease != nullptr ? retriggerSeqARelease->load() : 0.0f);
    const auto retriggerAttackBValue = juce::jlimit (
        0.0f, 1.0f,
        retriggerSeqBAttack != nullptr ? retriggerSeqBAttack->load() : 0.0f);
    const auto retriggerReleaseBValue = juce::jlimit (
        0.0f, 1.0f,
        retriggerSeqBRelease != nullptr ? retriggerSeqBRelease->load() : 0.0f);
    const auto retriggerLinked = retriggerPlaybackMode != nullptr
                              && retriggerPlaybackMode->load() >= 0.5f;
    const auto retriggerUsesProfileB = retriggerSerialProfile != nullptr
                                    && retriggerSerialProfile->load() >= 0.5f;
    const auto retriggerBipolarAValue = retriggerSeqABipolar != nullptr
                                     && retriggerSeqABipolar->load() >= 0.5f;
    const auto retriggerBipolarBValue = retriggerSeqBBipolar != nullptr
                                     && retriggerSeqBBipolar->load() >= 0.5f;
    const auto retriggerSerialBipolar = retriggerUsesProfileB
        ? retriggerBipolarBValue : retriggerBipolarAValue;
    const auto retriggerUnipolarPatternA = readRetriggerPattern (0, false);
    const auto retriggerUnipolarPatternB = readRetriggerPattern (1, false);
    const auto retriggerCanonicalPatternA = readRetriggerPattern (0, true);
    const auto retriggerCanonicalPatternB = readRetriggerPattern (1, true);
    const auto retriggerModesForA = readRetriggerModes (0);
    const auto retriggerModesForB = readRetriggerModes (1);
    const auto retriggerUnipolarSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::retrigger, 0, false);
    const auto retriggerUnipolarSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::retrigger, 1, false);
    const auto retriggerCanonicalSubdivisionsA = readStepSubdivisions (
        seqwencer::SequencerEngine::retrigger, 0, true);
    const auto retriggerCanonicalSubdivisionsB = readStepSubdivisions (
        seqwencer::SequencerEngine::retrigger, 1, true);
    const auto retriggerTransformState = makeTransformBlockState (
        seqwencer::SequencerEngine::retrigger,
        retriggerUnipolarPatternA, retriggerUnipolarPatternB,
        retriggerCanonicalPatternA, retriggerCanonicalPatternB,
        retriggerUnipolarSubdivisionsA,
        retriggerUnipolarSubdivisionsB,
        retriggerCanonicalSubdivisionsA,
        retriggerCanonicalSubdivisionsB);
    const auto retriggerRateIndex = juce::jlimit (
        0, seqwencer::rateChoiceCount - 1,
        static_cast<int> (std::lround (
            retriggerRate != nullptr ? retriggerRate->load() : 6.0f)));
    const auto retriggerStepBeats = seqwencer::beatsForRate (
        retriggerRateIndex);
    const auto retriggerTraversalMode = seqwencer::sequenceModeFromChoice (
        retriggerSequenceMode != nullptr
            ? retriggerSequenceMode->load() : 0.0f);
    const auto retriggerRetriggersFromPlayedNotes =
        retriggerTraversalMode == seqwencer::SequenceMode::played;
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
    for (auto& state : phiPairs)
        state.useHostPosition = hostSyncEnabled
                             && hostPositionAvailable
                             && ! phiTimelineNeedsFreeRun
                             && ! state.retriggersFromPlayedNotes;
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
    const auto distortionUseHostPosition = hostSyncEnabled
                                        && hostPositionAvailable
                                        && ! phiTimelineNeedsFreeRun
                                        && ! distortionRetriggersFromPlayedNotes;
    const auto grainUseHostPosition = hostSyncEnabled
                                   && hostPositionAvailable
                                   && ! phiTimelineNeedsFreeRun
                                   && ! grainRetriggersFromPlayedNotes;
    const auto compressorUseHostPosition = hostSyncEnabled
                                        && hostPositionAvailable
                                        && ! phiTimelineNeedsFreeRun
                                        && ! compressorRetriggersFromPlayedNotes;
    const auto reverseUseHostPosition = hostSyncEnabled
                                     && hostPositionAvailable
                                     && ! phiTimelineNeedsFreeRun
                                     && ! reverseRetriggersFromPlayedNotes;
    const auto retriggerUseHostPosition = hostSyncEnabled
                                       && hostPositionAvailable
                                       && ! phiTimelineNeedsFreeRun
                                       && ! retriggerRetriggersFromPlayedNotes;
    const auto configuredRange = [] (const std::atomic<float>* startParameter,
                                     const std::atomic<float>* endParameter,
                                     const std::atomic<float>* lengthParameter,
                                     bool linkedBoundaries,
                                     int maximumStep)
    {
        const auto start = static_cast<int> (std::lround (
            startParameter != nullptr ? startParameter->load() : 1.0f));
        if (linkedBoundaries)
        {
            return seqwencer::makeLengthStepRange (
                start,
                static_cast<int> (std::lround (
                    lengthParameter != nullptr
                        ? lengthParameter->load() : 64.0f)),
                maximumStep);
        }
        return seqwencer::makeStepRange (
            start,
            static_cast<int> (std::lround (
                endParameter != nullptr ? endParameter->load() : 64.0f)),
            maximumStep);
    };
    const auto gateMaximumStep = gateLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto delayMaximumStep = delayLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto reverbMaximumStep = reverbLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto panMaximumStep = panLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto filterMaximumStep = filterLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto pitchMaximumStep = pitchLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto distortionMaximumStep = distortionLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto grainMaximumStep = grainLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto compressorMaximumStep = compressorLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto reverseMaximumStep = reverseLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto retriggerMaximumStep = retriggerLinked
        ? seqwencer::linkedStepCount : seqwencer::stepsPerBank;
    const auto gateRangeIsLinked = rangeLink != nullptr
                                && rangeLink->load() >= 0.5f;
    const auto delayRangeIsLinked = delayRangeLink != nullptr
                                 && delayRangeLink->load() >= 0.5f;
    const auto reverbRangeIsLinked = reverbRangeLink != nullptr
                                  && reverbRangeLink->load() >= 0.5f;
    const auto panRangeIsLinked = panRangeLink != nullptr
                               && panRangeLink->load() >= 0.5f;
    const auto filterRangeIsLinked = filterRangeLink != nullptr
                                  && filterRangeLink->load() >= 0.5f;
    const auto pitchRangeIsLinked = pitchRangeLink != nullptr
                                 && pitchRangeLink->load() >= 0.5f;
    const auto distortionRangeIsLinked = distortionRangeLink != nullptr
                                      && distortionRangeLink->load() >= 0.5f;
    const auto grainRangeIsLinked = grainRangeLink != nullptr
                                 && grainRangeLink->load() >= 0.5f;
    const auto compressorRangeIsLinked = compressorRangeLink != nullptr
                                      && compressorRangeLink->load() >= 0.5f;
    const auto reverseRangeIsLinked = reverseRangeLink != nullptr
                                   && reverseRangeLink->load() >= 0.5f;
    const auto retriggerRangeIsLinked = retriggerRangeLink != nullptr
                                     && retriggerRangeLink->load() >= 0.5f;
    const auto gateRange = configuredRange (
        startStep, endStep, rangeLength, gateRangeIsLinked, gateMaximumStep);
    for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
    {
        auto& state = phiPairs[static_cast<std::size_t> (pair)];
        const auto pairIndex = static_cast<std::size_t> (pair);
        const auto rangeIsLinked = phiRangeLinks[pairIndex] != nullptr
                                && phiRangeLinks[pairIndex]->load() >= 0.5f;
        state.range = configuredRange (
            phiStartSteps[pairIndex], phiEndSteps[pairIndex],
            phiRangeLengths[pairIndex], rangeIsLinked,
            state.linked ? seqwencer::linkedStepCount
                         : seqwencer::stepsPerBank);
    }
    const auto delayRange = configuredRange (
        delayStartStep, delayEndStep, delayRangeLength,
        delayRangeIsLinked, delayMaximumStep);
    const auto reverbRange = configuredRange (
        reverbStartStep, reverbEndStep, reverbRangeLength,
        reverbRangeIsLinked, reverbMaximumStep);
    const auto panRange = configuredRange (
        panStartStep, panEndStep, panRangeLength,
        panRangeIsLinked, panMaximumStep);
    const auto filterRange = configuredRange (
        filterStartStep, filterEndStep, filterRangeLength,
        filterRangeIsLinked, filterMaximumStep);
    const auto pitchRange = configuredRange (
        pitchStartStep, pitchEndStep, pitchRangeLength,
        pitchRangeIsLinked, pitchMaximumStep);
    const auto distortionRange = configuredRange (
        distortionStartStep, distortionEndStep, distortionRangeLength,
        distortionRangeIsLinked, distortionMaximumStep);
    const auto grainRange = configuredRange (
        grainStartStep, grainEndStep, grainRangeLength,
        grainRangeIsLinked, grainMaximumStep);
    const auto compressorRange = configuredRange (
        compressorStartStep, compressorEndStep, compressorRangeLength,
        compressorRangeIsLinked, compressorMaximumStep);
    const auto reverseRange = configuredRange (
        reverseStartStep, reverseEndStep, reverseRangeLength,
        reverseRangeIsLinked, reverseMaximumStep);
    const auto retriggerRange = configuredRange (
        retriggerStartStep, retriggerEndStep, retriggerRangeLength,
        retriggerRangeIsLinked, retriggerMaximumStep);
    const auto gatePhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * gateStepBeats * currentSampleRate / hostBpm);
    for (auto& state : phiPairs)
        state.phaseIncrement = 1.0 / juce::jmax (
            1.0, 60.0 * state.stepBeats * currentSampleRate / hostBpm);
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
    const auto distortionPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * distortionStepBeats * currentSampleRate / hostBpm);
    const auto grainPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * grainStepBeats * currentSampleRate / hostBpm);
    const auto compressorPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * compressorStepBeats * currentSampleRate / hostBpm);
    const auto reversePhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * reverseStepBeats * currentSampleRate / hostBpm);
    const auto retriggerPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * retriggerStepBeats * currentSampleRate / hostBpm);
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
        0, 5, static_cast<int> (std::lround (
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
    const auto shouldDistortion = distortionEnabled != nullptr
                               && distortionEnabled->load() >= 0.5f;
    const auto baseDistortionType = juce::jlimit (
        0, 3, static_cast<int> (std::lround (
            distortionType != nullptr ? distortionType->load() : 0.0f)));
    const auto baseDistortionDrive = juce::jlimit (
        0.0f, 36.0f,
        distortionDrive != nullptr ? distortionDrive->load() : 6.0f);
    const auto baseDistortionTone = juce::jlimit (
        0.0f, 1.0f,
        distortionTone != nullptr ? distortionTone->load() : 1.0f);
    const auto baseDistortionMix = juce::jlimit (
        0.0f, 1.0f,
        distortionMix != nullptr ? distortionMix->load() : 1.0f);
    const auto shouldGrain = grainEnabled != nullptr
                          && grainEnabled->load() >= 0.5f;
    const auto baseGrainSizeMs = juce::jlimit (
        10.0f, 250.0f, grainSize != nullptr ? grainSize->load() : 80.0f);
    const auto baseGrainShift = juce::jlimit (
        -24.0f, 24.0f, grainShift != nullptr ? grainShift->load() : 12.0f);
    const auto baseGrainFeedback = juce::jlimit (
        0.0f, 0.90f,
        grainFeedback != nullptr ? grainFeedback->load() : 0.20f);
    const auto baseGrainMix = juce::jlimit (
        0.0f, 1.0f, grainMix != nullptr ? grainMix->load() : 0.35f);
    const auto shouldCompressor = compressorEnabled != nullptr
                               && compressorEnabled->load() >= 0.5f;
    const auto baseCompressorThreshold = juce::jlimit (
        -60.0f, 0.0f,
        compressorThreshold != nullptr ? compressorThreshold->load() : -18.0f);
    const auto baseCompressorRatio = juce::jlimit (
        1.0f, 20.0f,
        compressorRatio != nullptr ? compressorRatio->load() : 4.0f);
    const auto baseCompressorAttack = juce::jlimit (
        0.1f, 100.0f,
        compressorAttack != nullptr ? compressorAttack->load() : 10.0f);
    const auto baseCompressorRelease = juce::jlimit (
        10.0f, 1000.0f,
        compressorRelease != nullptr ? compressorRelease->load() : 100.0f);
    const auto baseCompressorMakeup = juce::jlimit (
        0.0f, 24.0f,
        compressorMakeup != nullptr ? compressorMakeup->load() : 0.0f);
    const auto baseCompressorMix = juce::jlimit (
        0.0f, 1.0f,
        compressorMix != nullptr ? compressorMix->load() : 1.0f);
    const auto shouldReverse = reverseEnabled != nullptr
                            && reverseEnabled->load() >= 0.5f;
    const auto baseReverseTimeMs = juce::jlimit (
        25.0f, 2000.0f,
        reverseTime != nullptr ? reverseTime->load() : 500.0f);
    const auto baseReversePointA = juce::jlimit (
        0.0f, 1.0f,
        reversePointA != nullptr ? reversePointA->load() : 0.0f);
    const auto baseReversePointB = juce::jlimit (
        0.0f, 1.0f,
        reversePointB != nullptr ? reversePointB->load() : 1.0f);
    const auto baseReverseMix = juce::jlimit (
        0.0f, 1.0f,
        reverseMix != nullptr ? reverseMix->load() : 1.0f);
    const auto shouldRetrigger = retriggerEnabled != nullptr
                              && retriggerEnabled->load() >= 0.5f;
    const auto baseRetriggerInitialSpeed = juce::jlimit (
        1.0f, 16.0f,
        retriggerInitialSpeed != nullptr
            ? retriggerInitialSpeed->load() : 2.0f);
    const auto baseRetriggerFinalSpeed = juce::jlimit (
        1.0f, 16.0f,
        retriggerFinalSpeed != nullptr ? retriggerFinalSpeed->load() : 2.0f);
    const auto baseRetriggerTransitionSteps = juce::jlimit (
        0.0f, 16.0f,
        retriggerTransition != nullptr ? retriggerTransition->load() : 4.0f);
    const auto baseRetriggerDecay = juce::jlimit (
        0.0f, 1.0f,
        retriggerDecay != nullptr ? retriggerDecay->load() : 0.0f);
    const auto baseRetriggerMix = juce::jlimit (
        0.0f, 1.0f,
        retriggerMix != nullptr ? retriggerMix->load() : 1.0f);
    const auto gateAIsEnabled = seqAEnabled == nullptr
                             || seqAEnabled->load() >= 0.5f;
    const auto gateBIsEnabled = seqBEnabled != nullptr
                             && seqBEnabled->load() >= 0.5f;
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
    const auto distortionAIsEnabled = distortionSeqAEnabled == nullptr
                                   || distortionSeqAEnabled->load() >= 0.5f;
    const auto distortionBIsEnabled = distortionSeqBEnabled != nullptr
                                   && distortionSeqBEnabled->load() >= 0.5f;
    const auto grainAIsEnabled = grainSeqAEnabled == nullptr
                              || grainSeqAEnabled->load() >= 0.5f;
    const auto grainBIsEnabled = grainSeqBEnabled != nullptr
                              && grainSeqBEnabled->load() >= 0.5f;
    const auto compressorAIsEnabled = compressorSeqAEnabled == nullptr
                                   || compressorSeqAEnabled->load() >= 0.5f;
    const auto compressorBIsEnabled = compressorSeqBEnabled != nullptr
                                   && compressorSeqBEnabled->load() >= 0.5f;
    const auto reverseAIsEnabled = reverseSeqAEnabled == nullptr
                                || reverseSeqAEnabled->load() >= 0.5f;
    const auto reverseBIsEnabled = reverseSeqBEnabled != nullptr
                                && reverseSeqBEnabled->load() >= 0.5f;
    const auto retriggerAIsEnabled = retriggerSeqAEnabled == nullptr
                                  || retriggerSeqAEnabled->load() >= 0.5f;
    const auto retriggerBIsEnabled = retriggerSeqBEnabled != nullptr
                                  && retriggerSeqBEnabled->load() >= 0.5f;
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
    if (filterIsProcessing
        && (! filterWasActive || baseFilterType != filterPreviousType))
        resetFilterProcessor();
    else if (! filterIsProcessing && filterWasActive)
        resetFilterProcessor();
    filterPreviousType = filterIsProcessing ? baseFilterType : -1;
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
    const auto distortionIsProcessing = shouldDistortion && ! shouldBypass;
    if (distortionIsProcessing != distortionWasActive)
        distortionToneStates.fill (0.0f);
    distortionWasActive = distortionIsProcessing;
    const auto grainIsProcessing = shouldGrain && ! shouldBypass
                                && grainBuffer.getNumSamples() > 8;
    if (grainIsProcessing != grainWasActive)
    {
        grainBuffer.clear();
        grainWritePosition = 0;
        grainReadPhase = 0.0;
        grainFeedbackStates.fill (0.0f);
    }
    grainWasActive = grainIsProcessing;
    const auto compressorIsProcessing = shouldCompressor && ! shouldBypass;
    if (compressorIsProcessing != compressorWasActive)
        compressorGain = 1.0f;
    compressorWasActive = compressorIsProcessing;
    const auto reverseIsProcessing = shouldReverse && ! shouldBypass
                                  && reverseBuffers[0].getNumSamples() > 8;
    if (reverseIsProcessing != reverseWasActive)
    {
        for (auto& reverseBuffer : reverseBuffers)
            reverseBuffer.clear();
        resetReverseProcessor();
    }
    reverseWasActive = reverseIsProcessing;
    const auto retriggerIsProcessing = shouldRetrigger && ! shouldBypass
                                    && retriggerBuffer.getNumSamples() > 8;
    if (retriggerIsProcessing != retriggerWasActive)
    {
        retriggerBuffer.clear();
        resetRetriggerProcessor();
    }
    retriggerWasActive = retriggerIsProcessing;
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
    if (! distortionIsProcessing)
    {
        distortionActiveStepA.store (-1);
        distortionActiveStepB.store (-1);
    }
    if (! grainIsProcessing)
    {
        grainActiveStepA.store (-1);
        grainActiveStepB.store (-1);
    }
    if (! compressorIsProcessing)
    {
        compressorActiveStepA.store (-1);
        compressorActiveStepB.store (-1);
    }
    if (! reverseIsProcessing)
    {
        reverseActiveStepA.store (-1);
        reverseActiveStepB.store (-1);
    }
    if (! retriggerIsProcessing)
    {
        retriggerActiveStepA.store (-1);
        retriggerActiveStepB.store (-1);
    }
    const auto orderedAudioFx = getAudioFxOrder();
    std::size_t nextPhraseStart = 0;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        while (nextPhraseStart < phraseStartSamples.size()
               && phraseStartSamples[nextPhraseStart] == sample)
        {
            if (gateRetriggersFromPlayedNotes)
                gateFreeRunningPhase = 0.0;
            for (int pair = 0;
                 pair < seqwencer::phiSequencerPairCount; ++pair)
            {
                if (phiPairs[static_cast<std::size_t> (pair)]
                        .retriggersFromPlayedNotes)
                {
                    phiFreeRunningPhases[static_cast<std::size_t> (pair)] = 0.0;
                }
            }
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
            if (distortionRetriggersFromPlayedNotes)
                distortionFreeRunningPhase = 0.0;
            if (grainRetriggersFromPlayedNotes)
                grainFreeRunningPhase = 0.0;
            if (compressorRetriggersFromPlayedNotes)
                compressorFreeRunningPhase = 0.0;
            if (reverseRetriggersFromPlayedNotes)
                reverseFreeRunningPhase = 0.0;
            if (retriggerRetriggersFromPlayedNotes)
                retriggerFreeRunningPhase = 0.0;
            ++nextPhraseStart;
        }

        auto gatePhase = gateFreeRunningPhase;
        if (gateUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            gatePhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, gateStepBeats);
        }

        for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
        {
            auto& state = phiPairs[static_cast<std::size_t> (pair)];
            state.phase = phiFreeRunningPhases[static_cast<std::size_t> (pair)];
            if (state.useHostPosition)
            {
                const auto ppq = hostPpq
                    + (hostTimelineAdvancing
                           ? sample * quarterNotesPerSample
                           : 0.0);
                state.phase = seqwencer::unwrappedPhaseFromQuarterNotes (
                    ppq, state.stepBeats);
            }
        }

        auto delayPhase = delayFreeRunningPhase;
        if (delayUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            delayPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, delayStepBeats);
        }

        auto reverbPhase = reverbFreeRunningPhase;
        if (reverbUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            reverbPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, reverbStepBeats);
        }

        auto panPhase = panFreeRunningPhase;
        if (panUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            panPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, panStepBeats);
        }

        auto filterPhase = filterFreeRunningPhase;
        if (filterUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            filterPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, filterStepBeats);
        }

        auto pitchPhase = pitchFreeRunningPhase;
        if (pitchUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            pitchPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, pitchStepBeats);
        }

        auto distortionPhase = distortionFreeRunningPhase;
        if (distortionUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            distortionPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, distortionStepBeats);
        }

        auto grainPhase = grainFreeRunningPhase;
        if (grainUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            grainPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, grainStepBeats);
        }

        auto compressorPhase = compressorFreeRunningPhase;
        if (compressorUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            compressorPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, compressorStepBeats);
        }

        auto reversePhase = reverseFreeRunningPhase;
        if (reverseUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            reversePhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, reverseStepBeats);
        }

        auto retriggerPhase = retriggerFreeRunningPhase;
        if (retriggerUseHostPosition)
        {
            const auto ppq = hostPpq
                + (hostTimelineAdvancing
                       ? sample * quarterNotesPerSample
                       : 0.0);
            retriggerPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
                ppq, retriggerStepBeats);
        }

        const auto modulatedSequenceRange = [&] (
            bool engineIsProcessing,
            const seqwencer::Pattern& unipolarPatternA,
            const seqwencer::Pattern& unipolarPatternB,
            const seqwencer::Pattern& canonicalPatternA,
            const seqwencer::Pattern& canonicalPatternB,
            const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsA,
            const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsB,
            const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsA,
            const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsB,
            double phase,
            seqwencer::StepRange baseRange,
            seqwencer::SequenceMode traversalMode,
            bool serialPlayback,
            bool serialBipolar,
            bool bipolarA,
            bool bipolarB,
            bool laneAEnabled,
            bool laneBEnabled,
            bool linkedBoundaries,
            const std::array<seqwencer::ModulationTarget,
                             seqwencer::sequencerRangeTargetCount>& targets,
            seqwencer::SequencerEngine sequencerEngine,
            int maximumStep)
        {
            if (! engineIsProcessing)
                return baseRange;

            const auto targetMayBeActive = [&] (
                seqwencer::ModulationTarget target)
            {
                if (serialPlayback)
                    return targetIsActive (0, target);
                return (laneAEnabled && targetIsActive (0, target))
                    || (laneBEnabled && targetIsActive (1, target));
            };
            const auto startTarget = targets[0];
            const auto secondaryTarget = targets[static_cast<std::size_t> (
                linkedBoundaries ? 2 : 1)];
            if (! targetMayBeActive (startTarget)
                && ! targetMayBeActive (secondaryTarget))
                return baseRange;

            auto rawUnipolarA = 1.0f;
            auto rawUnipolarB = 1.0f;
            auto rawCanonicalA = 1.0f;
            auto rawCanonicalB = 1.0f;
            auto rawSerialUnipolar = 1.0f;
            auto rawSerialCanonical = 1.0f;
            const auto randomStreamA = seqwencer::sequencerRandomStream (
                sequencerEngine, 0);
            const auto randomStreamB = seqwencer::sequencerRandomStream (
                sequencerEngine, 1);
            const auto randomStreamSerial = seqwencer::sequencerRandomStream (
                sequencerEngine, 2);
            if (serialPlayback)
            {
                rawSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                    unipolarPatternA, unipolarPatternB,
                    unipolarSubdivisionsA, unipolarSubdivisionsB,
                    phase, baseRange,
                    0.0f, 0.0f, 0.0f, 0.0f, nullptr, nullptr,
                    traversalMode, 0.0f, randomStreamSerial);
                rawSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                    canonicalPatternA, canonicalPatternB,
                    canonicalSubdivisionsA, canonicalSubdivisionsB,
                    phase, baseRange,
                    0.0f, 0.0f, 0.0f, 0.0f, nullptr, nullptr,
                    traversalMode, 0.5f, randomStreamSerial);
            }
            else
            {
                rawUnipolarA = seqwencer::evaluateSubdividedBankRange (
                    unipolarPatternA, unipolarSubdivisionsA,
                    phase, baseRange,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.0f, randomStreamA);
                rawUnipolarB = seqwencer::evaluateSubdividedBankRange (
                    unipolarPatternB, unipolarSubdivisionsB,
                    phase, baseRange,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.0f, randomStreamB);
                rawCanonicalA = seqwencer::evaluateSubdividedBankRange (
                    canonicalPatternA, canonicalSubdivisionsA,
                    phase, baseRange,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.5f, randomStreamA);
                rawCanonicalB = seqwencer::evaluateSubdividedBankRange (
                    canonicalPatternB, canonicalSubdivisionsB,
                    phase, baseRange,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.5f, randomStreamB);
            }

            const auto modulationDeviation = [&] (
                seqwencer::ModulationTarget target, bool& assigned)
            {
                if (serialPlayback)
                {
                    assigned = targetIsActive (0, target);
                    return assigned
                        ? (serialBipolar
                               ? 2.0f * rawSerialCanonical - 1.0f
                               : rawSerialUnipolar - 1.0f)
                        : 0.0f;
                }

                const auto useBipolarA = bipolarA
                    && seqwencer::targetSupportsBipolar (target);
                const auto useBipolarB = bipolarB
                    && seqwencer::targetSupportsBipolar (target);
                return seqwencer::combineParallelModulationDeviation (
                    target,
                    useBipolarA ? rawCanonicalA : rawUnipolarA,
                    laneAEnabled && targetIsActive (0, target), target,
                    useBipolarA,
                    useBipolarB ? rawCanonicalB : rawUnipolarB,
                    laneBEnabled && targetIsActive (1, target), target,
                    useBipolarB, &assigned);
            };

            const auto travel = static_cast<float> (std::max (
                1, maximumStep - 2));
            auto startNormalised = static_cast<float> (baseRange.first)
                                 / travel;
            bool startAssigned = false;
            const auto startDeviation = modulationDeviation (
                startTarget, startAssigned);
            if (startAssigned)
            {
                startNormalised = seqwencer::applyModulationDepth (
                    startNormalised, startDeviation, 1.0f);
            }

            bool secondaryAssigned = false;
            const auto secondaryDeviation = modulationDeviation (
                secondaryTarget, secondaryAssigned);
            if (linkedBoundaries)
            {
                auto lengthNormalised = static_cast<float> (
                    baseRange.length() - 2) / travel;
                if (secondaryAssigned)
                {
                    lengthNormalised = seqwencer::applyModulationDepth (
                        lengthNormalised, secondaryDeviation, 1.0f);
                }
                return seqwencer::resolveModulatedLengthStepRange (
                    baseRange, maximumStep,
                    startNormalised, startAssigned,
                    lengthNormalised, secondaryAssigned);
            }

            auto endNormalised = static_cast<float> (baseRange.last - 1)
                               / travel;
            if (secondaryAssigned)
            {
                endNormalised = seqwencer::applyModulationDepth (
                    endNormalised, secondaryDeviation, 1.0f);
            }
            return seqwencer::resolveModulatedStepRange (
                baseRange, maximumStep, false,
                startNormalised, startAssigned,
                endNormalised, secondaryAssigned);
        };

        const auto gateSampleRange = modulatedSequenceRange (
            gateIsProcessing,
            gatePatternA, gatePatternB,
            gateCanonicalPatternA, gateCanonicalPatternB,
            gateUnipolarSubdivisionsA, gateUnipolarSubdivisionsB,
            gateCanonicalSubdivisionsA, gateCanonicalSubdivisionsB,
            gatePhase, gateRange, gateTraversalMode,
            gateLinked, gateSerialBipolar, gateBipolarA, gateBipolarB,
            gateAIsEnabled, gateBIsEnabled, gateRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::gate),
            seqwencer::SequencerEngine::gate,
            gateMaximumStep);
        const auto delaySampleRange = modulatedSequenceRange (
            delayIsProcessing,
            delayUnipolarPatternA, delayUnipolarPatternB,
            delayCanonicalPatternA, delayCanonicalPatternB,
            delayUnipolarSubdivisionsA, delayUnipolarSubdivisionsB,
            delayCanonicalSubdivisionsA, delayCanonicalSubdivisionsB,
            delayPhase, delayRange, delayTraversalMode,
            delayLinked, delaySerialBipolar,
            delayBipolarAValue, delayBipolarBValue,
            delayAIsEnabled, delayBIsEnabled, delayRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::delay),
            seqwencer::SequencerEngine::delay,
            delayMaximumStep);
        const auto reverbSampleRange = modulatedSequenceRange (
            reverbIsProcessing,
            reverbUnipolarPatternA, reverbUnipolarPatternB,
            reverbCanonicalPatternA, reverbCanonicalPatternB,
            reverbUnipolarSubdivisionsA, reverbUnipolarSubdivisionsB,
            reverbCanonicalSubdivisionsA, reverbCanonicalSubdivisionsB,
            reverbPhase, reverbRange, reverbTraversalMode,
            reverbLinked, reverbSerialBipolar,
            reverbBipolarAValue, reverbBipolarBValue,
            reverbAIsEnabled, reverbBIsEnabled, reverbRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::reverb),
            seqwencer::SequencerEngine::reverb,
            reverbMaximumStep);
        const auto panSampleRange = modulatedSequenceRange (
            panIsProcessing,
            panUnipolarPatternA, panUnipolarPatternB,
            panCanonicalPatternA, panCanonicalPatternB,
            panUnipolarSubdivisionsA, panUnipolarSubdivisionsB,
            panCanonicalSubdivisionsA, panCanonicalSubdivisionsB,
            panPhase, panRange, panTraversalMode,
            panLinked, panSerialBipolar,
            panBipolarAValue, panBipolarBValue,
            panAIsEnabled, panBIsEnabled, panRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::pan),
            seqwencer::SequencerEngine::pan,
            panMaximumStep);
        const auto filterSampleRange = modulatedSequenceRange (
            filterIsProcessing,
            filterUnipolarPatternA, filterUnipolarPatternB,
            filterCanonicalPatternA, filterCanonicalPatternB,
            filterUnipolarSubdivisionsA, filterUnipolarSubdivisionsB,
            filterCanonicalSubdivisionsA, filterCanonicalSubdivisionsB,
            filterPhase, filterRange, filterTraversalMode,
            filterLinked, filterSerialBipolar,
            filterBipolarAValue, filterBipolarBValue,
            filterAIsEnabled, filterBIsEnabled, filterRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::filter),
            seqwencer::SequencerEngine::filter,
            filterMaximumStep);
        const auto pitchSampleRange = modulatedSequenceRange (
            pitchIsProcessing,
            pitchUnipolarPatternA, pitchUnipolarPatternB,
            pitchCanonicalPatternA, pitchCanonicalPatternB,
            pitchUnipolarSubdivisionsA, pitchUnipolarSubdivisionsB,
            pitchCanonicalSubdivisionsA, pitchCanonicalSubdivisionsB,
            pitchPhase, pitchRange, pitchTraversalMode,
            pitchLinked, pitchSerialBipolar,
            pitchBipolarAValue, pitchBipolarBValue,
            pitchAIsEnabled, pitchBIsEnabled, pitchRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::pitch),
            seqwencer::SequencerEngine::pitch,
            pitchMaximumStep);
        const auto distortionSampleRange = modulatedSequenceRange (
            distortionIsProcessing,
            distortionUnipolarPatternA, distortionUnipolarPatternB,
            distortionCanonicalPatternA, distortionCanonicalPatternB,
            distortionUnipolarSubdivisionsA,
            distortionUnipolarSubdivisionsB,
            distortionCanonicalSubdivisionsA,
            distortionCanonicalSubdivisionsB,
            distortionPhase, distortionRange, distortionTraversalMode,
            distortionLinked, distortionSerialBipolar,
            distortionBipolarAValue, distortionBipolarBValue,
            distortionAIsEnabled, distortionBIsEnabled,
            distortionRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::distortion),
            seqwencer::SequencerEngine::distortion,
            distortionMaximumStep);
        const auto grainSampleRange = modulatedSequenceRange (
            grainIsProcessing,
            grainUnipolarPatternA, grainUnipolarPatternB,
            grainCanonicalPatternA, grainCanonicalPatternB,
            grainUnipolarSubdivisionsA, grainUnipolarSubdivisionsB,
            grainCanonicalSubdivisionsA, grainCanonicalSubdivisionsB,
            grainPhase, grainRange, grainTraversalMode,
            grainLinked, grainSerialBipolar,
            grainBipolarAValue, grainBipolarBValue,
            grainAIsEnabled, grainBIsEnabled, grainRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::grain),
            seqwencer::SequencerEngine::grain,
            grainMaximumStep);
        const auto compressorSampleRange = modulatedSequenceRange (
            compressorIsProcessing,
            compressorUnipolarPatternA, compressorUnipolarPatternB,
            compressorCanonicalPatternA, compressorCanonicalPatternB,
            compressorUnipolarSubdivisionsA,
            compressorUnipolarSubdivisionsB,
            compressorCanonicalSubdivisionsA,
            compressorCanonicalSubdivisionsB,
            compressorPhase, compressorRange, compressorTraversalMode,
            compressorLinked, compressorSerialBipolar,
            compressorBipolarAValue, compressorBipolarBValue,
            compressorAIsEnabled, compressorBIsEnabled,
            compressorRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::compressor),
            seqwencer::SequencerEngine::compressor,
            compressorMaximumStep);
        const auto reverseSampleRange = modulatedSequenceRange (
            reverseIsProcessing,
            reverseUnipolarPatternA, reverseUnipolarPatternB,
            reverseCanonicalPatternA, reverseCanonicalPatternB,
            reverseUnipolarSubdivisionsA,
            reverseUnipolarSubdivisionsB,
            reverseCanonicalSubdivisionsA,
            reverseCanonicalSubdivisionsB,
            reversePhase, reverseRange, reverseTraversalMode,
            reverseLinked, reverseSerialBipolar,
            reverseBipolarAValue, reverseBipolarBValue,
            reverseAIsEnabled, reverseBIsEnabled,
            reverseRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::reverse),
            seqwencer::SequencerEngine::reverse,
            reverseMaximumStep);
        const auto retriggerSampleRange = modulatedSequenceRange (
            retriggerIsProcessing,
            retriggerUnipolarPatternA, retriggerUnipolarPatternB,
            retriggerCanonicalPatternA, retriggerCanonicalPatternB,
            retriggerUnipolarSubdivisionsA,
            retriggerUnipolarSubdivisionsB,
            retriggerCanonicalSubdivisionsA,
            retriggerCanonicalSubdivisionsB,
            retriggerPhase, retriggerRange, retriggerTraversalMode,
            retriggerLinked, retriggerSerialBipolar,
            retriggerBipolarAValue, retriggerBipolarBValue,
            retriggerAIsEnabled, retriggerBIsEnabled,
            retriggerRangeIsLinked,
            seqwencer::sequencerRangeTargets (
                seqwencer::SequencerEngine::retrigger),
            seqwencer::SequencerEngine::retrigger,
            retriggerMaximumStep);

        const auto modulatedEnvelopeValues = [&] (
            bool engineIsProcessing,
            const seqwencer::Pattern& unipolarPatternA,
            const seqwencer::Pattern& unipolarPatternB,
            const seqwencer::Pattern& canonicalPatternA,
            const seqwencer::Pattern& canonicalPatternB,
            const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsA,
            const seqwencer::StepSubdivisionPattern& unipolarSubdivisionsB,
            const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsA,
            const seqwencer::StepSubdivisionPattern& canonicalSubdivisionsB,
            double phase,
            seqwencer::StepRange range,
            seqwencer::SequenceMode traversalMode,
            bool linked,
            bool serialBipolar,
            bool bipolarA,
            bool bipolarB,
            bool laneAEnabled,
            bool laneBEnabled,
            seqwencer::SequencerEngine sequencerEngine,
            const std::array<seqwencer::ModulationTarget,
                             seqwencer::sequencerEnvelopeTargetCount>& targets,
            std::array<float,
                       seqwencer::sequencerEnvelopeTargetCount> baseValues)
        {
            if (! engineIsProcessing)
                return baseValues;

            auto hasEnvelopeTarget = false;
            for (const auto target : targets)
            {
                hasEnvelopeTarget = hasEnvelopeTarget
                    || targetIsActive (0, target)
                    || (! linked && targetIsActive (1, target));
            }
            if (! hasEnvelopeTarget)
                return baseValues;

            auto rawUnipolarA = 1.0f;
            auto rawUnipolarB = 1.0f;
            auto rawCanonicalA = 1.0f;
            auto rawCanonicalB = 1.0f;
            auto rawSerialUnipolar = 1.0f;
            auto rawSerialCanonical = 1.0f;
            const auto randomStreamA = seqwencer::sequencerRandomStream (
                sequencerEngine, 0);
            const auto randomStreamB = seqwencer::sequencerRandomStream (
                sequencerEngine, 1);
            const auto randomStreamSerial = seqwencer::sequencerRandomStream (
                sequencerEngine, 2);
            if (linked)
            {
                rawSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                    unipolarPatternA, unipolarPatternB,
                    unipolarSubdivisionsA, unipolarSubdivisionsB,
                    phase, range,
                    0.0f, 0.0f, 0.0f, 0.0f, nullptr, nullptr,
                    traversalMode, 0.0f, randomStreamSerial);
                rawSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                    canonicalPatternA, canonicalPatternB,
                    canonicalSubdivisionsA, canonicalSubdivisionsB,
                    phase, range,
                    0.0f, 0.0f, 0.0f, 0.0f, nullptr, nullptr,
                    traversalMode, 0.5f, randomStreamSerial);
            }
            else
            {
                rawUnipolarA = seqwencer::evaluateSubdividedBankRange (
                    unipolarPatternA, unipolarSubdivisionsA,
                    phase, range,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.0f, randomStreamA);
                rawUnipolarB = seqwencer::evaluateSubdividedBankRange (
                    unipolarPatternB, unipolarSubdivisionsB,
                    phase, range,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.0f, randomStreamB);
                rawCanonicalA = seqwencer::evaluateSubdividedBankRange (
                    canonicalPatternA, canonicalSubdivisionsA,
                    phase, range,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.5f, randomStreamA);
                rawCanonicalB = seqwencer::evaluateSubdividedBankRange (
                    canonicalPatternB, canonicalSubdivisionsB,
                    phase, range,
                    0.0f, 0.0f, nullptr, traversalMode,
                    0.5f, randomStreamB);
            }

            for (std::size_t index = 0; index < targets.size(); ++index)
            {
                const auto target = targets[index];
                auto assigned = false;
                auto deviation = 0.0f;
                if (linked)
                {
                    assigned = targetIsActive (0, target);
                    if (assigned)
                    {
                        deviation = serialBipolar
                            ? 2.0f * rawSerialCanonical - 1.0f
                            : rawSerialUnipolar - 1.0f;
                    }
                }
                else
                {
                    const auto useBipolarA = bipolarA
                        && seqwencer::targetSupportsBipolar (target);
                    const auto useBipolarB = bipolarB
                        && seqwencer::targetSupportsBipolar (target);
                    deviation = seqwencer::combineParallelModulationDeviation (
                        target,
                        useBipolarA ? rawCanonicalA : rawUnipolarA,
                        laneAEnabled && targetIsActive (0, target), target,
                        useBipolarA,
                        useBipolarB ? rawCanonicalB : rawUnipolarB,
                        laneBEnabled && targetIsActive (1, target), target,
                        useBipolarB, &assigned);
                }

                if (assigned)
                    baseValues[index] = seqwencer::applyModulationDepth (
                        baseValues[index], deviation, 1.0f);
            }
            return baseValues;
        };

        const auto applySequencerTransforms = [&] (
            bool engineIsProcessing,
            bool linked,
            bool usesSecondProfile,
            bool serialBipolar,
            bool bipolarA,
            bool bipolarB,
            bool laneAEnabled,
            bool laneBEnabled,
            seqwencer::SequencerEngine engine,
            const SequencerTransformBlockState& transform,
            float& unipolarA,
            float& unipolarB,
            float& canonicalA,
            float& canonicalB,
            float& serialUnipolar,
            float& serialCanonical)
        {
            if (! engineIsProcessing)
                return transform.baseValues;

            // All transform target modulation is calculated from this raw
            // pre-transform snapshot. Peak/Move therefore cannot feed their
            // own result back into themselves or make A/B processing order
            // affect the result.
            auto effective = transform.baseValues;
            const auto targets = seqwencer::sequencerTransformTargets (engine);
            for (std::size_t index = 0; index < targets.size(); ++index)
            {
                const auto target = targets[index];
                auto assigned = false;
                auto deviation = 0.0f;
                if (linked)
                {
                    assigned = targetIsActive (0, target);
                    if (assigned)
                    {
                        deviation = serialBipolar
                            ? 2.0f * serialCanonical - 1.0f
                            : serialUnipolar - 1.0f;
                    }
                }
                else
                {
                    const auto useBipolarA = bipolarA
                        && seqwencer::targetSupportsBipolar (target);
                    const auto useBipolarB = bipolarB
                        && seqwencer::targetSupportsBipolar (target);
                    deviation = seqwencer::combineParallelModulationDeviation (
                        target,
                        useBipolarA ? canonicalA : unipolarA,
                        laneAEnabled && targetIsActive (0, target), target,
                        useBipolarA,
                        useBipolarB ? canonicalB : unipolarB,
                        laneBEnabled && targetIsActive (1, target), target,
                        useBipolarB, &assigned);
                }

                if (assigned)
                {
                    const auto normalisedBase = 0.5f * (
                        effective[index] + 1.0f);
                    effective[index] = 2.0f
                        * seqwencer::applyModulationDepth (
                            normalisedBase, deviation, 1.0f)
                        - 1.0f;
                }
            }

            if (linked)
            {
                const auto profile = usesSecondProfile ? 1U : 0U;
                const auto peak = effective[profile * 2];
                const auto move = effective[profile * 2 + 1];
                const auto unipolarBounds =
                    seqwencer::combineSequencerValueBounds (
                        transform.unipolarBounds[0],
                        transform.unipolarBounds[1]);
                const auto canonicalBounds =
                    seqwencer::combineSequencerValueBounds (
                        transform.canonicalBounds[0],
                        transform.canonicalBounds[1]);
                serialUnipolar = seqwencer::applySequencerPeakMove (
                    serialUnipolar, unipolarBounds, peak, move, false);
                serialCanonical = seqwencer::applySequencerPeakMove (
                    serialCanonical, canonicalBounds, peak, move, true);
                return effective;
            }

            unipolarA = seqwencer::applySequencerPeakMove (
                unipolarA, transform.unipolarBounds[0],
                effective[0], effective[1], false);
            canonicalA = seqwencer::applySequencerPeakMove (
                canonicalA, transform.canonicalBounds[0],
                effective[0], effective[1], true);
            unipolarB = seqwencer::applySequencerPeakMove (
                unipolarB, transform.unipolarBounds[1],
                effective[2], effective[3], false);
            canonicalB = seqwencer::applySequencerPeakMove (
                canonicalB, transform.canonicalBounds[1],
                effective[2], effective[3], true);
            return effective;
        };

        auto gateUnipolarA = 1.0f;
        auto gateUnipolarB = 1.0f;
        auto gateCanonicalA = 1.0f;
        auto gateCanonicalB = 1.0f;
        auto gateSerialUnipolar = 1.0f;
        auto gateSerialCanonical = 1.0f;
        const auto gateEnvelopeValues = modulatedEnvelopeValues (
            gateIsProcessing,
            gatePatternA, gatePatternB,
            gateCanonicalPatternA, gateCanonicalPatternB,
            gateUnipolarSubdivisionsA, gateUnipolarSubdivisionsB,
            gateCanonicalSubdivisionsA, gateCanonicalSubdivisionsB,
            gatePhase, gateSampleRange, gateTraversalMode,
            gateLinked, gateSerialBipolar, gateBipolarA, gateBipolarB,
            gateAIsEnabled, gateBIsEnabled,
            seqwencer::SequencerEngine::gate,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::gate),
            { gateAttackA, gateReleaseA, gateAttackB, gateReleaseB });
        const auto effectiveGateAttackA = gateEnvelopeValues[0];
        const auto effectiveGateReleaseA = gateEnvelopeValues[1];
        const auto effectiveGateAttackB = gateEnvelopeValues[2];
        const auto effectiveGateReleaseB = gateEnvelopeValues[3];
        const auto effectiveGateSerialAttack = gateUsesProfileB
            ? effectiveGateAttackB : effectiveGateAttackA;
        const auto effectiveGateSerialRelease = gateUsesProfileB
            ? effectiveGateReleaseB : effectiveGateReleaseA;

        if (gateIsProcessing && gateLinked)
        {
            gateSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                gatePatternA, gatePatternB,
                gateUnipolarSubdivisionsA, gateUnipolarSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                nullptr, nullptr, gateTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 2));
            gateSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                gateCanonicalPatternA, gateCanonicalPatternB,
                gateCanonicalSubdivisionsA, gateCanonicalSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                nullptr, nullptr, gateTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 2));
        }
        else if (gateIsProcessing)
        {
            gateUnipolarA = seqwencer::evaluateSubdividedBankRange (
                gatePatternA, gateUnipolarSubdivisionsA,
                gatePhase, gateSampleRange,
                effectiveGateAttackA, effectiveGateReleaseA,
                nullptr, gateTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 0));
            gateUnipolarB = seqwencer::evaluateSubdividedBankRange (
                gatePatternB, gateUnipolarSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateAttackB, effectiveGateReleaseB,
                nullptr, gateTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 1));
            gateCanonicalA = seqwencer::evaluateSubdividedBankRange (
                gateCanonicalPatternA, gateCanonicalSubdivisionsA,
                gatePhase, gateSampleRange,
                effectiveGateAttackA, effectiveGateReleaseA,
                nullptr, gateTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 0));
            gateCanonicalB = seqwencer::evaluateSubdividedBankRange (
                gateCanonicalPatternB, gateCanonicalSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateAttackB, effectiveGateReleaseB,
                nullptr, gateTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 1));
        }

        const auto effectiveGateTransforms = applySequencerTransforms (
            gateIsProcessing, gateLinked, gateUsesProfileB,
            gateSerialBipolar, gateBipolarA, gateBipolarB,
            gateAIsEnabled, gateBIsEnabled,
            seqwencer::SequencerEngine::gate, gateTransformState,
            gateUnipolarA, gateUnipolarB,
            gateCanonicalA, gateCanonicalB,
            gateSerialUnipolar, gateSerialCanonical);

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
            const auto profile = gateUsesProfileB ? 1U : 0U;
            const auto serialBounds =
                seqwencer::combineSequencerValueBounds (
                    gateTransformState.unipolarBounds[0],
                    gateTransformState.unipolarBounds[1]);
            const auto value = seqwencer::evaluateSubdividedGateLinkedRange (
                gateLevelPatternA, gateLevelPatternB,
                gateModesForA, gateModesForB,
                gateUnipolarSubdivisionsA, gateUnipolarSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                effectiveGateSerialAttack, effectiveGateSerialRelease,
                &activeBank, &activeStep, shortLength, longLength,
                gateTraversalMode,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 2),
                volumeAIsAssigned,
                serialBounds.minimum, serialBounds.maximum,
                effectiveGateTransforms[profile * 2],
                effectiveGateTransforms[profile * 2 + 1]);
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
            const auto valueA = seqwencer::evaluateSubdividedGateBankRange (
                gateLevelPatternA, gateModesForA, gateUnipolarSubdivisionsA,
                gatePhase, gateSampleRange,
                effectiveGateAttackA, effectiveGateReleaseA,
                &stepA, shortLength, longLength,
                gateTraversalMode,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 0),
                volumeAIsAssigned,
                gateTransformState.unipolarBounds[0].minimum,
                gateTransformState.unipolarBounds[0].maximum,
                effectiveGateTransforms[0],
                effectiveGateTransforms[1]);
            const auto valueB = seqwencer::evaluateSubdividedGateBankRange (
                gateLevelPatternB, gateModesForB, gateUnipolarSubdivisionsB,
                gatePhase, gateSampleRange,
                effectiveGateAttackB, effectiveGateReleaseB,
                &stepB, shortLength, longLength,
                gateTraversalMode,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::gate, 1),
                volumeBIsAssigned,
                gateTransformState.unipolarBounds[1].minimum,
                gateTransformState.unipolarBounds[1].maximum,
                effectiveGateTransforms[2],
                effectiveGateTransforms[3]);
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

        for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
        {
            auto& state = phiPairs[static_cast<std::size_t> (pair)];
            const auto engine = seqwencer::phiEngineForPair (pair);
            const auto firstLane = pair * 2;
            if (state.linked)
            {
                auto activeBank = 0;
                auto activeStep = 0;
                const auto serialValue =
                    seqwencer::evaluateSubdividedLinkedRange (
                        state.pattern[0], state.pattern[1],
                        state.subdivisions[0], state.subdivisions[1],
                        state.phase, state.range,
                        state.serialAttack, state.serialRelease,
                        state.serialAttack, state.serialRelease,
                        &activeBank, &activeStep, state.traversalMode,
                        state.serialBipolar ? 0.5f : 0.0f,
                        seqwencer::sequencerRandomStream (engine, 2));
                const auto profile = state.usesSecondProfile ? 1U : 0U;
                const auto& laneBounds = state.serialBipolar
                    ? state.transform.canonicalBounds
                    : state.transform.unipolarBounds;
                const auto serialBounds =
                    seqwencer::combineSequencerValueBounds (
                        laneBounds[0], laneBounds[1]);
                state.lastValues.fill (seqwencer::applySequencerPeakMove (
                    serialValue, serialBounds,
                    state.transform.baseValues[profile * 2],
                    state.transform.baseValues[profile * 2 + 1],
                    state.serialBipolar));
                phiActiveSteps[static_cast<std::size_t> (firstLane)].store (
                    activeBank == 0 ? activeStep : -1);
                phiActiveSteps[static_cast<std::size_t> (firstLane + 1)].store (
                    activeBank == 1 ? activeStep : -1);
            }
            else
            {
                for (int localLane = 0; localLane < 2; ++localLane)
                {
                    auto step = 0;
                    const auto index = static_cast<std::size_t> (localLane);
                    state.lastValues[index] =
                        seqwencer::evaluateSubdividedBankRange (
                            state.pattern[index], state.subdivisions[index],
                            state.phase, state.range,
                            state.attack[index], state.release[index],
                            &step, state.traversalMode,
                            state.bipolar[index] ? 0.5f : 0.0f,
                            seqwencer::sequencerRandomStream (
                                engine, localLane));
                    const auto bounds = state.bipolar[index]
                        ? state.transform.canonicalBounds[index]
                        : state.transform.unipolarBounds[index];
                    state.lastValues[index] =
                        seqwencer::applySequencerPeakMove (
                            state.lastValues[index], bounds,
                            state.transform.baseValues[index * 2],
                            state.transform.baseValues[index * 2 + 1],
                            state.bipolar[index]);
                    phiActiveSteps[static_cast<std::size_t> (
                        firstLane + localLane)].store (
                            state.enabled[index] ? step : -1);
                }
            }
        }

        auto delayUnipolarA = 1.0f;
        auto delayUnipolarB = 1.0f;
        auto delayCanonicalA = 1.0f;
        auto delayCanonicalB = 1.0f;
        auto delaySerialUnipolar = 1.0f;
        auto delaySerialCanonical = 1.0f;
        const auto delayEnvelopeValues = modulatedEnvelopeValues (
            delayIsProcessing,
            delayUnipolarPatternA, delayUnipolarPatternB,
            delayCanonicalPatternA, delayCanonicalPatternB,
            delayUnipolarSubdivisionsA, delayUnipolarSubdivisionsB,
            delayCanonicalSubdivisionsA, delayCanonicalSubdivisionsB,
            delayPhase, delaySampleRange, delayTraversalMode,
            delayLinked, delaySerialBipolar,
            delayBipolarAValue, delayBipolarBValue,
            delayAIsEnabled, delayBIsEnabled,
            seqwencer::SequencerEngine::delay,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::delay),
            { delayAttackAValue, delayReleaseAValue,
              delayAttackBValue, delayReleaseBValue });
        const auto effectiveDelayAttackA = delayEnvelopeValues[0];
        const auto effectiveDelayReleaseA = delayEnvelopeValues[1];
        const auto effectiveDelayAttackB = delayEnvelopeValues[2];
        const auto effectiveDelayReleaseB = delayEnvelopeValues[3];
        const auto effectiveDelaySerialAttack = delayUsesProfileB
            ? effectiveDelayAttackB : effectiveDelayAttackA;
        const auto effectiveDelaySerialRelease = delayUsesProfileB
            ? effectiveDelayReleaseB : effectiveDelayReleaseA;
        if (delayIsProcessing && delayLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            delaySerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                delayUnipolarPatternA, delayUnipolarPatternB,
                delayUnipolarSubdivisionsA, delayUnipolarSubdivisionsB,
                delayPhase, delaySampleRange,
                effectiveDelaySerialAttack, effectiveDelaySerialRelease,
                effectiveDelaySerialAttack, effectiveDelaySerialRelease,
                &activeBank, &activeStep, delayTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 2));
            delaySerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                delayCanonicalPatternA, delayCanonicalPatternB,
                delayCanonicalSubdivisionsA, delayCanonicalSubdivisionsB,
                delayPhase, delaySampleRange,
                effectiveDelaySerialAttack, effectiveDelaySerialRelease,
                effectiveDelaySerialAttack, effectiveDelaySerialRelease,
                nullptr, nullptr, delayTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 2));
            delayActiveStepA.store (activeBank == 0 ? activeStep : -1);
            delayActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (delayIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            delayUnipolarA = seqwencer::evaluateSubdividedBankRange (
                delayUnipolarPatternA, delayUnipolarSubdivisionsA,
                delayPhase, delaySampleRange,
                effectiveDelayAttackA, effectiveDelayReleaseA,
                &stepA, delayTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 0));
            delayUnipolarB = seqwencer::evaluateSubdividedBankRange (
                delayUnipolarPatternB, delayUnipolarSubdivisionsB,
                delayPhase, delaySampleRange,
                effectiveDelayAttackB, effectiveDelayReleaseB,
                &stepB, delayTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 1));
            delayCanonicalA = seqwencer::evaluateSubdividedBankRange (
                delayCanonicalPatternA, delayCanonicalSubdivisionsA,
                delayPhase, delaySampleRange,
                effectiveDelayAttackA, effectiveDelayReleaseA,
                nullptr, delayTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 0));
            delayCanonicalB = seqwencer::evaluateSubdividedBankRange (
                delayCanonicalPatternB, delayCanonicalSubdivisionsB,
                delayPhase, delaySampleRange,
                effectiveDelayAttackB, effectiveDelayReleaseB,
                nullptr, delayTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::delay, 1));
            delayActiveStepA.store (delayAIsEnabled ? stepA : -1);
            delayActiveStepB.store (delayBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            delayIsProcessing, delayLinked, delayUsesProfileB,
            delaySerialBipolar, delayBipolarAValue, delayBipolarBValue,
            delayAIsEnabled, delayBIsEnabled,
            seqwencer::SequencerEngine::delay, delayTransformState,
            delayUnipolarA, delayUnipolarB,
            delayCanonicalA, delayCanonicalB,
            delaySerialUnipolar, delaySerialCanonical);

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
        const auto reverbEnvelopeValues = modulatedEnvelopeValues (
            reverbIsProcessing,
            reverbUnipolarPatternA, reverbUnipolarPatternB,
            reverbCanonicalPatternA, reverbCanonicalPatternB,
            reverbUnipolarSubdivisionsA, reverbUnipolarSubdivisionsB,
            reverbCanonicalSubdivisionsA, reverbCanonicalSubdivisionsB,
            reverbPhase, reverbSampleRange, reverbTraversalMode,
            reverbLinked, reverbSerialBipolar,
            reverbBipolarAValue, reverbBipolarBValue,
            reverbAIsEnabled, reverbBIsEnabled,
            seqwencer::SequencerEngine::reverb,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::reverb),
            { reverbAttackAValue, reverbReleaseAValue,
              reverbAttackBValue, reverbReleaseBValue });
        const auto effectiveReverbAttackA = reverbEnvelopeValues[0];
        const auto effectiveReverbReleaseA = reverbEnvelopeValues[1];
        const auto effectiveReverbAttackB = reverbEnvelopeValues[2];
        const auto effectiveReverbReleaseB = reverbEnvelopeValues[3];
        const auto effectiveReverbSerialAttack = reverbUsesProfileB
            ? effectiveReverbAttackB : effectiveReverbAttackA;
        const auto effectiveReverbSerialRelease = reverbUsesProfileB
            ? effectiveReverbReleaseB : effectiveReverbReleaseA;
        if (reverbIsProcessing && reverbLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            reverbSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                reverbUnipolarPatternA, reverbUnipolarPatternB,
                reverbUnipolarSubdivisionsA, reverbUnipolarSubdivisionsB,
                reverbPhase, reverbSampleRange,
                effectiveReverbSerialAttack, effectiveReverbSerialRelease,
                effectiveReverbSerialAttack, effectiveReverbSerialRelease,
                &activeBank, &activeStep, reverbTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 2));
            reverbSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                reverbCanonicalPatternA, reverbCanonicalPatternB,
                reverbCanonicalSubdivisionsA, reverbCanonicalSubdivisionsB,
                reverbPhase, reverbSampleRange,
                effectiveReverbSerialAttack, effectiveReverbSerialRelease,
                effectiveReverbSerialAttack, effectiveReverbSerialRelease,
                nullptr, nullptr, reverbTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 2));
            reverbActiveStepA.store (activeBank == 0 ? activeStep : -1);
            reverbActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (reverbIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            reverbUnipolarA = seqwencer::evaluateSubdividedBankRange (
                reverbUnipolarPatternA, reverbUnipolarSubdivisionsA,
                reverbPhase, reverbSampleRange,
                effectiveReverbAttackA, effectiveReverbReleaseA,
                &stepA, reverbTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 0));
            reverbUnipolarB = seqwencer::evaluateSubdividedBankRange (
                reverbUnipolarPatternB, reverbUnipolarSubdivisionsB,
                reverbPhase, reverbSampleRange,
                effectiveReverbAttackB, effectiveReverbReleaseB,
                &stepB, reverbTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 1));
            reverbCanonicalA = seqwencer::evaluateSubdividedBankRange (
                reverbCanonicalPatternA, reverbCanonicalSubdivisionsA,
                reverbPhase, reverbSampleRange,
                effectiveReverbAttackA, effectiveReverbReleaseA,
                nullptr, reverbTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 0));
            reverbCanonicalB = seqwencer::evaluateSubdividedBankRange (
                reverbCanonicalPatternB, reverbCanonicalSubdivisionsB,
                reverbPhase, reverbSampleRange,
                effectiveReverbAttackB, effectiveReverbReleaseB,
                nullptr, reverbTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverb, 1));
            reverbActiveStepA.store (reverbAIsEnabled ? stepA : -1);
            reverbActiveStepB.store (reverbBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            reverbIsProcessing, reverbLinked, reverbUsesProfileB,
            reverbSerialBipolar, reverbBipolarAValue, reverbBipolarBValue,
            reverbAIsEnabled, reverbBIsEnabled,
            seqwencer::SequencerEngine::reverb, reverbTransformState,
            reverbUnipolarA, reverbUnipolarB,
            reverbCanonicalA, reverbCanonicalB,
            reverbSerialUnipolar, reverbSerialCanonical);

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
        const auto panEnvelopeValues = modulatedEnvelopeValues (
            panIsProcessing,
            panUnipolarPatternA, panUnipolarPatternB,
            panCanonicalPatternA, panCanonicalPatternB,
            panUnipolarSubdivisionsA, panUnipolarSubdivisionsB,
            panCanonicalSubdivisionsA, panCanonicalSubdivisionsB,
            panPhase, panSampleRange, panTraversalMode,
            panLinked, panSerialBipolar,
            panBipolarAValue, panBipolarBValue,
            panAIsEnabled, panBIsEnabled,
            seqwencer::SequencerEngine::pan,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::pan),
            { panAttackAValue, panReleaseAValue,
              panAttackBValue, panReleaseBValue });
        const auto effectivePanAttackA = panEnvelopeValues[0];
        const auto effectivePanReleaseA = panEnvelopeValues[1];
        const auto effectivePanAttackB = panEnvelopeValues[2];
        const auto effectivePanReleaseB = panEnvelopeValues[3];
        const auto effectivePanSerialAttack = panUsesProfileB
            ? effectivePanAttackB : effectivePanAttackA;
        const auto effectivePanSerialRelease = panUsesProfileB
            ? effectivePanReleaseB : effectivePanReleaseA;
        if (panIsProcessing && panLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            panSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                panUnipolarPatternA, panUnipolarPatternB,
                panUnipolarSubdivisionsA, panUnipolarSubdivisionsB,
                panPhase, panSampleRange,
                effectivePanSerialAttack, effectivePanSerialRelease,
                effectivePanSerialAttack, effectivePanSerialRelease,
                &activeBank, &activeStep, panTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 2));
            panSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                panCanonicalPatternA, panCanonicalPatternB,
                panCanonicalSubdivisionsA, panCanonicalSubdivisionsB,
                panPhase, panSampleRange,
                effectivePanSerialAttack, effectivePanSerialRelease,
                effectivePanSerialAttack, effectivePanSerialRelease,
                nullptr, nullptr, panTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 2));
            panActiveStepA.store (activeBank == 0 ? activeStep : -1);
            panActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (panIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            panUnipolarA = seqwencer::evaluateSubdividedBankRange (
                panUnipolarPatternA, panUnipolarSubdivisionsA,
                panPhase, panSampleRange,
                effectivePanAttackA, effectivePanReleaseA,
                &stepA, panTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 0));
            panUnipolarB = seqwencer::evaluateSubdividedBankRange (
                panUnipolarPatternB, panUnipolarSubdivisionsB,
                panPhase, panSampleRange,
                effectivePanAttackB, effectivePanReleaseB,
                &stepB, panTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 1));
            panCanonicalA = seqwencer::evaluateSubdividedBankRange (
                panCanonicalPatternA, panCanonicalSubdivisionsA,
                panPhase, panSampleRange,
                effectivePanAttackA, effectivePanReleaseA,
                nullptr, panTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 0));
            panCanonicalB = seqwencer::evaluateSubdividedBankRange (
                panCanonicalPatternB, panCanonicalSubdivisionsB,
                panPhase, panSampleRange,
                effectivePanAttackB, effectivePanReleaseB,
                nullptr, panTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pan, 1));
            panActiveStepA.store (panAIsEnabled ? stepA : -1);
            panActiveStepB.store (panBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            panIsProcessing, panLinked, panUsesProfileB,
            panSerialBipolar, panBipolarAValue, panBipolarBValue,
            panAIsEnabled, panBIsEnabled,
            seqwencer::SequencerEngine::pan, panTransformState,
            panUnipolarA, panUnipolarB,
            panCanonicalA, panCanonicalB,
            panSerialUnipolar, panSerialCanonical);

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
        const auto filterEnvelopeValues = modulatedEnvelopeValues (
            filterIsProcessing,
            filterUnipolarPatternA, filterUnipolarPatternB,
            filterCanonicalPatternA, filterCanonicalPatternB,
            filterUnipolarSubdivisionsA, filterUnipolarSubdivisionsB,
            filterCanonicalSubdivisionsA, filterCanonicalSubdivisionsB,
            filterPhase, filterSampleRange, filterTraversalMode,
            filterLinked, filterSerialBipolar,
            filterBipolarAValue, filterBipolarBValue,
            filterAIsEnabled, filterBIsEnabled,
            seqwencer::SequencerEngine::filter,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::filter),
            { filterAttackAValue, filterReleaseAValue,
              filterAttackBValue, filterReleaseBValue });
        const auto effectiveFilterAttackA = filterEnvelopeValues[0];
        const auto effectiveFilterReleaseA = filterEnvelopeValues[1];
        const auto effectiveFilterAttackB = filterEnvelopeValues[2];
        const auto effectiveFilterReleaseB = filterEnvelopeValues[3];
        const auto effectiveFilterSerialAttack = filterUsesProfileB
            ? effectiveFilterAttackB : effectiveFilterAttackA;
        const auto effectiveFilterSerialRelease = filterUsesProfileB
            ? effectiveFilterReleaseB : effectiveFilterReleaseA;
        if (filterIsProcessing && filterLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            filterSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                filterUnipolarPatternA, filterUnipolarPatternB,
                filterUnipolarSubdivisionsA, filterUnipolarSubdivisionsB,
                filterPhase, filterSampleRange,
                effectiveFilterSerialAttack, effectiveFilterSerialRelease,
                effectiveFilterSerialAttack, effectiveFilterSerialRelease,
                &activeBank, &activeStep, filterTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 2));
            filterSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                filterCanonicalPatternA, filterCanonicalPatternB,
                filterCanonicalSubdivisionsA, filterCanonicalSubdivisionsB,
                filterPhase, filterSampleRange,
                effectiveFilterSerialAttack, effectiveFilterSerialRelease,
                effectiveFilterSerialAttack, effectiveFilterSerialRelease,
                nullptr, nullptr, filterTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 2));
            filterActiveStepA.store (activeBank == 0 ? activeStep : -1);
            filterActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (filterIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            filterUnipolarA = seqwencer::evaluateSubdividedBankRange (
                filterUnipolarPatternA, filterUnipolarSubdivisionsA,
                filterPhase, filterSampleRange,
                effectiveFilterAttackA, effectiveFilterReleaseA,
                &stepA, filterTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 0));
            filterUnipolarB = seqwencer::evaluateSubdividedBankRange (
                filterUnipolarPatternB, filterUnipolarSubdivisionsB,
                filterPhase, filterSampleRange,
                effectiveFilterAttackB, effectiveFilterReleaseB,
                &stepB, filterTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 1));
            filterCanonicalA = seqwencer::evaluateSubdividedBankRange (
                filterCanonicalPatternA, filterCanonicalSubdivisionsA,
                filterPhase, filterSampleRange,
                effectiveFilterAttackA, effectiveFilterReleaseA,
                nullptr, filterTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 0));
            filterCanonicalB = seqwencer::evaluateSubdividedBankRange (
                filterCanonicalPatternB, filterCanonicalSubdivisionsB,
                filterPhase, filterSampleRange,
                effectiveFilterAttackB, effectiveFilterReleaseB,
                nullptr, filterTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::filter, 1));
            filterActiveStepA.store (filterAIsEnabled ? stepA : -1);
            filterActiveStepB.store (filterBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            filterIsProcessing, filterLinked, filterUsesProfileB,
            filterSerialBipolar, filterBipolarAValue, filterBipolarBValue,
            filterAIsEnabled, filterBIsEnabled,
            seqwencer::SequencerEngine::filter, filterTransformState,
            filterUnipolarA, filterUnipolarB,
            filterCanonicalA, filterCanonicalB,
            filterSerialUnipolar, filterSerialCanonical);

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
        const auto pitchEnvelopeValues = modulatedEnvelopeValues (
            pitchIsProcessing,
            pitchUnipolarPatternA, pitchUnipolarPatternB,
            pitchCanonicalPatternA, pitchCanonicalPatternB,
            pitchUnipolarSubdivisionsA, pitchUnipolarSubdivisionsB,
            pitchCanonicalSubdivisionsA, pitchCanonicalSubdivisionsB,
            pitchPhase, pitchSampleRange, pitchTraversalMode,
            pitchLinked, pitchSerialBipolar,
            pitchBipolarAValue, pitchBipolarBValue,
            pitchAIsEnabled, pitchBIsEnabled,
            seqwencer::SequencerEngine::pitch,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::pitch),
            { pitchAttackAValue, pitchReleaseAValue,
              pitchAttackBValue, pitchReleaseBValue });
        const auto effectivePitchAttackA = pitchEnvelopeValues[0];
        const auto effectivePitchReleaseA = pitchEnvelopeValues[1];
        const auto effectivePitchAttackB = pitchEnvelopeValues[2];
        const auto effectivePitchReleaseB = pitchEnvelopeValues[3];
        const auto effectivePitchSerialAttack = pitchUsesProfileB
            ? effectivePitchAttackB : effectivePitchAttackA;
        const auto effectivePitchSerialRelease = pitchUsesProfileB
            ? effectivePitchReleaseB : effectivePitchReleaseA;
        if (pitchIsProcessing && pitchLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            pitchSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                pitchUnipolarPatternA, pitchUnipolarPatternB,
                pitchUnipolarSubdivisionsA, pitchUnipolarSubdivisionsB,
                pitchPhase, pitchSampleRange,
                effectivePitchSerialAttack, effectivePitchSerialRelease,
                effectivePitchSerialAttack, effectivePitchSerialRelease,
                &activeBank, &activeStep, pitchTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 2));
            pitchSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                pitchCanonicalPatternA, pitchCanonicalPatternB,
                pitchCanonicalSubdivisionsA, pitchCanonicalSubdivisionsB,
                pitchPhase, pitchSampleRange,
                effectivePitchSerialAttack, effectivePitchSerialRelease,
                effectivePitchSerialAttack, effectivePitchSerialRelease,
                nullptr, nullptr, pitchTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 2));
            pitchActiveStepA.store (activeBank == 0 ? activeStep : -1);
            pitchActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (pitchIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            pitchUnipolarA = seqwencer::evaluateSubdividedBankRange (
                pitchUnipolarPatternA, pitchUnipolarSubdivisionsA,
                pitchPhase, pitchSampleRange,
                effectivePitchAttackA, effectivePitchReleaseA,
                &stepA, pitchTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 0));
            pitchUnipolarB = seqwencer::evaluateSubdividedBankRange (
                pitchUnipolarPatternB, pitchUnipolarSubdivisionsB,
                pitchPhase, pitchSampleRange,
                effectivePitchAttackB, effectivePitchReleaseB,
                &stepB, pitchTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 1));
            pitchCanonicalA = seqwencer::evaluateSubdividedBankRange (
                pitchCanonicalPatternA, pitchCanonicalSubdivisionsA,
                pitchPhase, pitchSampleRange,
                effectivePitchAttackA, effectivePitchReleaseA,
                nullptr, pitchTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 0));
            pitchCanonicalB = seqwencer::evaluateSubdividedBankRange (
                pitchCanonicalPatternB, pitchCanonicalSubdivisionsB,
                pitchPhase, pitchSampleRange,
                effectivePitchAttackB, effectivePitchReleaseB,
                nullptr, pitchTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::pitch, 1));
            pitchActiveStepA.store (pitchAIsEnabled ? stepA : -1);
            pitchActiveStepB.store (pitchBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            pitchIsProcessing, pitchLinked, pitchUsesProfileB,
            pitchSerialBipolar, pitchBipolarAValue, pitchBipolarBValue,
            pitchAIsEnabled, pitchBIsEnabled,
            seqwencer::SequencerEngine::pitch, pitchTransformState,
            pitchUnipolarA, pitchUnipolarB,
            pitchCanonicalA, pitchCanonicalB,
            pitchSerialUnipolar, pitchSerialCanonical);

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

        auto distortionUnipolarA = 1.0f;
        auto distortionUnipolarB = 1.0f;
        auto distortionCanonicalA = 1.0f;
        auto distortionCanonicalB = 1.0f;
        auto distortionSerialUnipolar = 1.0f;
        auto distortionSerialCanonical = 1.0f;
        const auto distortionEnvelopeValues = modulatedEnvelopeValues (
            distortionIsProcessing,
            distortionUnipolarPatternA, distortionUnipolarPatternB,
            distortionCanonicalPatternA, distortionCanonicalPatternB,
            distortionUnipolarSubdivisionsA,
            distortionUnipolarSubdivisionsB,
            distortionCanonicalSubdivisionsA,
            distortionCanonicalSubdivisionsB,
            distortionPhase, distortionSampleRange, distortionTraversalMode,
            distortionLinked, distortionSerialBipolar,
            distortionBipolarAValue, distortionBipolarBValue,
            distortionAIsEnabled, distortionBIsEnabled,
            seqwencer::SequencerEngine::distortion,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::distortion),
            { distortionAttackAValue, distortionReleaseAValue,
              distortionAttackBValue, distortionReleaseBValue });
        const auto effectiveDistortionAttackA = distortionEnvelopeValues[0];
        const auto effectiveDistortionReleaseA = distortionEnvelopeValues[1];
        const auto effectiveDistortionAttackB = distortionEnvelopeValues[2];
        const auto effectiveDistortionReleaseB = distortionEnvelopeValues[3];
        const auto effectiveDistortionSerialAttack = distortionUsesProfileB
            ? effectiveDistortionAttackB : effectiveDistortionAttackA;
        const auto effectiveDistortionSerialRelease = distortionUsesProfileB
            ? effectiveDistortionReleaseB : effectiveDistortionReleaseA;
        if (distortionIsProcessing && distortionLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            distortionSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                distortionUnipolarPatternA, distortionUnipolarPatternB,
                distortionUnipolarSubdivisionsA,
                distortionUnipolarSubdivisionsB,
                distortionPhase, distortionSampleRange,
                effectiveDistortionSerialAttack,
                effectiveDistortionSerialRelease,
                effectiveDistortionSerialAttack,
                effectiveDistortionSerialRelease,
                &activeBank, &activeStep, distortionTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 2));
            distortionSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                distortionCanonicalPatternA, distortionCanonicalPatternB,
                distortionCanonicalSubdivisionsA,
                distortionCanonicalSubdivisionsB,
                distortionPhase, distortionSampleRange,
                effectiveDistortionSerialAttack,
                effectiveDistortionSerialRelease,
                effectiveDistortionSerialAttack,
                effectiveDistortionSerialRelease,
                nullptr, nullptr, distortionTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 2));
            distortionActiveStepA.store (activeBank == 0 ? activeStep : -1);
            distortionActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (distortionIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            distortionUnipolarA = seqwencer::evaluateSubdividedBankRange (
                distortionUnipolarPatternA, distortionUnipolarSubdivisionsA,
                distortionPhase,
                distortionSampleRange,
                effectiveDistortionAttackA, effectiveDistortionReleaseA,
                &stepA, distortionTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 0));
            distortionUnipolarB = seqwencer::evaluateSubdividedBankRange (
                distortionUnipolarPatternB, distortionUnipolarSubdivisionsB,
                distortionPhase,
                distortionSampleRange,
                effectiveDistortionAttackB, effectiveDistortionReleaseB,
                &stepB, distortionTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 1));
            distortionCanonicalA = seqwencer::evaluateSubdividedBankRange (
                distortionCanonicalPatternA, distortionCanonicalSubdivisionsA,
                distortionPhase,
                distortionSampleRange,
                effectiveDistortionAttackA, effectiveDistortionReleaseA,
                nullptr, distortionTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 0));
            distortionCanonicalB = seqwencer::evaluateSubdividedBankRange (
                distortionCanonicalPatternB, distortionCanonicalSubdivisionsB,
                distortionPhase,
                distortionSampleRange,
                effectiveDistortionAttackB, effectiveDistortionReleaseB,
                nullptr, distortionTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::distortion, 1));
            distortionActiveStepA.store (distortionAIsEnabled ? stepA : -1);
            distortionActiveStepB.store (distortionBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            distortionIsProcessing, distortionLinked,
            distortionUsesProfileB, distortionSerialBipolar,
            distortionBipolarAValue, distortionBipolarBValue,
            distortionAIsEnabled, distortionBIsEnabled,
            seqwencer::SequencerEngine::distortion,
            distortionTransformState,
            distortionUnipolarA, distortionUnipolarB,
            distortionCanonicalA, distortionCanonicalB,
            distortionSerialUnipolar, distortionSerialCanonical);

        const auto distortionTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (distortionLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return distortionSerialBipolar
                    ? 2.0f * distortionSerialCanonical - 1.0f
                    : distortionSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                distortionBipolarAValue
                    ? distortionCanonicalA : distortionUnipolarA,
                distortionAIsEnabled && targetIsActive (0, target), target,
                distortionBipolarAValue,
                distortionBipolarBValue
                    ? distortionCanonicalB : distortionUnipolarB,
                distortionBIsEnabled && targetIsActive (1, target), target,
                distortionBipolarBValue, assigned);
        };

        auto grainUnipolarA = 1.0f;
        auto grainUnipolarB = 1.0f;
        auto grainCanonicalA = 1.0f;
        auto grainCanonicalB = 1.0f;
        auto grainSerialUnipolar = 1.0f;
        auto grainSerialCanonical = 1.0f;
        const auto grainEnvelopeValues = modulatedEnvelopeValues (
            grainIsProcessing,
            grainUnipolarPatternA, grainUnipolarPatternB,
            grainCanonicalPatternA, grainCanonicalPatternB,
            grainUnipolarSubdivisionsA, grainUnipolarSubdivisionsB,
            grainCanonicalSubdivisionsA, grainCanonicalSubdivisionsB,
            grainPhase, grainSampleRange, grainTraversalMode,
            grainLinked, grainSerialBipolar,
            grainBipolarAValue, grainBipolarBValue,
            grainAIsEnabled, grainBIsEnabled,
            seqwencer::SequencerEngine::grain,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::grain),
            { grainAttackAValue, grainReleaseAValue,
              grainAttackBValue, grainReleaseBValue });
        const auto effectiveGrainAttackA = grainEnvelopeValues[0];
        const auto effectiveGrainReleaseA = grainEnvelopeValues[1];
        const auto effectiveGrainAttackB = grainEnvelopeValues[2];
        const auto effectiveGrainReleaseB = grainEnvelopeValues[3];
        const auto effectiveGrainSerialAttack = grainUsesProfileB
            ? effectiveGrainAttackB : effectiveGrainAttackA;
        const auto effectiveGrainSerialRelease = grainUsesProfileB
            ? effectiveGrainReleaseB : effectiveGrainReleaseA;
        if (grainIsProcessing && grainLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            grainSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                grainUnipolarPatternA, grainUnipolarPatternB,
                grainUnipolarSubdivisionsA, grainUnipolarSubdivisionsB,
                grainPhase, grainSampleRange,
                effectiveGrainSerialAttack, effectiveGrainSerialRelease,
                effectiveGrainSerialAttack, effectiveGrainSerialRelease,
                &activeBank, &activeStep, grainTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 2));
            grainSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                grainCanonicalPatternA, grainCanonicalPatternB,
                grainCanonicalSubdivisionsA, grainCanonicalSubdivisionsB,
                grainPhase, grainSampleRange,
                effectiveGrainSerialAttack, effectiveGrainSerialRelease,
                effectiveGrainSerialAttack, effectiveGrainSerialRelease,
                nullptr, nullptr, grainTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 2));
            grainActiveStepA.store (activeBank == 0 ? activeStep : -1);
            grainActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (grainIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            grainUnipolarA = seqwencer::evaluateSubdividedBankRange (
                grainUnipolarPatternA, grainUnipolarSubdivisionsA,
                grainPhase, grainSampleRange,
                effectiveGrainAttackA, effectiveGrainReleaseA,
                &stepA, grainTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 0));
            grainUnipolarB = seqwencer::evaluateSubdividedBankRange (
                grainUnipolarPatternB, grainUnipolarSubdivisionsB,
                grainPhase, grainSampleRange,
                effectiveGrainAttackB, effectiveGrainReleaseB,
                &stepB, grainTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 1));
            grainCanonicalA = seqwencer::evaluateSubdividedBankRange (
                grainCanonicalPatternA, grainCanonicalSubdivisionsA,
                grainPhase, grainSampleRange,
                effectiveGrainAttackA, effectiveGrainReleaseA,
                nullptr, grainTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 0));
            grainCanonicalB = seqwencer::evaluateSubdividedBankRange (
                grainCanonicalPatternB, grainCanonicalSubdivisionsB,
                grainPhase, grainSampleRange,
                effectiveGrainAttackB, effectiveGrainReleaseB,
                nullptr, grainTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::grain, 1));
            grainActiveStepA.store (grainAIsEnabled ? stepA : -1);
            grainActiveStepB.store (grainBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            grainIsProcessing, grainLinked, grainUsesProfileB,
            grainSerialBipolar, grainBipolarAValue, grainBipolarBValue,
            grainAIsEnabled, grainBIsEnabled,
            seqwencer::SequencerEngine::grain, grainTransformState,
            grainUnipolarA, grainUnipolarB,
            grainCanonicalA, grainCanonicalB,
            grainSerialUnipolar, grainSerialCanonical);

        const auto grainTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (grainLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return grainSerialBipolar
                    ? 2.0f * grainSerialCanonical - 1.0f
                    : grainSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                grainBipolarAValue ? grainCanonicalA : grainUnipolarA,
                grainAIsEnabled && targetIsActive (0, target), target,
                grainBipolarAValue,
                grainBipolarBValue ? grainCanonicalB : grainUnipolarB,
                grainBIsEnabled && targetIsActive (1, target), target,
                grainBipolarBValue, assigned);
        };

        auto compressorUnipolarA = 1.0f;
        auto compressorUnipolarB = 1.0f;
        auto compressorCanonicalA = 1.0f;
        auto compressorCanonicalB = 1.0f;
        auto compressorSerialUnipolar = 1.0f;
        auto compressorSerialCanonical = 1.0f;
        const auto compressorEnvelopeValues = modulatedEnvelopeValues (
            compressorIsProcessing,
            compressorUnipolarPatternA, compressorUnipolarPatternB,
            compressorCanonicalPatternA, compressorCanonicalPatternB,
            compressorUnipolarSubdivisionsA,
            compressorUnipolarSubdivisionsB,
            compressorCanonicalSubdivisionsA,
            compressorCanonicalSubdivisionsB,
            compressorPhase, compressorSampleRange, compressorTraversalMode,
            compressorLinked, compressorSerialBipolar,
            compressorBipolarAValue, compressorBipolarBValue,
            compressorAIsEnabled, compressorBIsEnabled,
            seqwencer::SequencerEngine::compressor,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::compressor),
            { compressorAttackAValue, compressorReleaseAValue,
              compressorAttackBValue, compressorReleaseBValue });
        const auto effectiveCompressorAttackA = compressorEnvelopeValues[0];
        const auto effectiveCompressorReleaseA = compressorEnvelopeValues[1];
        const auto effectiveCompressorAttackB = compressorEnvelopeValues[2];
        const auto effectiveCompressorReleaseB = compressorEnvelopeValues[3];
        const auto effectiveCompressorSerialAttack = compressorUsesProfileB
            ? effectiveCompressorAttackB : effectiveCompressorAttackA;
        const auto effectiveCompressorSerialRelease = compressorUsesProfileB
            ? effectiveCompressorReleaseB : effectiveCompressorReleaseA;
        if (compressorIsProcessing && compressorLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            compressorSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                compressorUnipolarPatternA, compressorUnipolarPatternB,
                compressorUnipolarSubdivisionsA,
                compressorUnipolarSubdivisionsB,
                compressorPhase, compressorSampleRange,
                effectiveCompressorSerialAttack,
                effectiveCompressorSerialRelease,
                effectiveCompressorSerialAttack,
                effectiveCompressorSerialRelease,
                &activeBank, &activeStep, compressorTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 2));
            compressorSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                compressorCanonicalPatternA, compressorCanonicalPatternB,
                compressorCanonicalSubdivisionsA,
                compressorCanonicalSubdivisionsB,
                compressorPhase, compressorSampleRange,
                effectiveCompressorSerialAttack,
                effectiveCompressorSerialRelease,
                effectiveCompressorSerialAttack,
                effectiveCompressorSerialRelease,
                nullptr, nullptr, compressorTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 2));
            compressorActiveStepA.store (activeBank == 0 ? activeStep : -1);
            compressorActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (compressorIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            compressorUnipolarA = seqwencer::evaluateSubdividedBankRange (
                compressorUnipolarPatternA, compressorUnipolarSubdivisionsA,
                compressorPhase,
                compressorSampleRange,
                effectiveCompressorAttackA, effectiveCompressorReleaseA,
                &stepA, compressorTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 0));
            compressorUnipolarB = seqwencer::evaluateSubdividedBankRange (
                compressorUnipolarPatternB, compressorUnipolarSubdivisionsB,
                compressorPhase,
                compressorSampleRange,
                effectiveCompressorAttackB, effectiveCompressorReleaseB,
                &stepB, compressorTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 1));
            compressorCanonicalA = seqwencer::evaluateSubdividedBankRange (
                compressorCanonicalPatternA, compressorCanonicalSubdivisionsA,
                compressorPhase,
                compressorSampleRange,
                effectiveCompressorAttackA, effectiveCompressorReleaseA,
                nullptr, compressorTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 0));
            compressorCanonicalB = seqwencer::evaluateSubdividedBankRange (
                compressorCanonicalPatternB, compressorCanonicalSubdivisionsB,
                compressorPhase,
                compressorSampleRange,
                effectiveCompressorAttackB, effectiveCompressorReleaseB,
                nullptr, compressorTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::compressor, 1));
            compressorActiveStepA.store (
                compressorAIsEnabled ? stepA : -1);
            compressorActiveStepB.store (
                compressorBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            compressorIsProcessing, compressorLinked,
            compressorUsesProfileB, compressorSerialBipolar,
            compressorBipolarAValue, compressorBipolarBValue,
            compressorAIsEnabled, compressorBIsEnabled,
            seqwencer::SequencerEngine::compressor,
            compressorTransformState,
            compressorUnipolarA, compressorUnipolarB,
            compressorCanonicalA, compressorCanonicalB,
            compressorSerialUnipolar, compressorSerialCanonical);

        const auto compressorTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (compressorLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return compressorSerialBipolar
                    ? 2.0f * compressorSerialCanonical - 1.0f
                    : compressorSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                compressorBipolarAValue
                    ? compressorCanonicalA : compressorUnipolarA,
                compressorAIsEnabled && targetIsActive (0, target), target,
                compressorBipolarAValue,
                compressorBipolarBValue
                    ? compressorCanonicalB : compressorUnipolarB,
                compressorBIsEnabled && targetIsActive (1, target), target,
                compressorBipolarBValue, assigned);
        };

        auto reverseUnipolarA = 1.0f;
        auto reverseUnipolarB = 1.0f;
        auto reverseCanonicalA = 1.0f;
        auto reverseCanonicalB = 1.0f;
        auto reverseSerialUnipolar = 1.0f;
        auto reverseSerialCanonical = 1.0f;
        const auto reverseEnvelopeValues = modulatedEnvelopeValues (
            reverseIsProcessing,
            reverseUnipolarPatternA, reverseUnipolarPatternB,
            reverseCanonicalPatternA, reverseCanonicalPatternB,
            reverseUnipolarSubdivisionsA,
            reverseUnipolarSubdivisionsB,
            reverseCanonicalSubdivisionsA,
            reverseCanonicalSubdivisionsB,
            reversePhase, reverseSampleRange, reverseTraversalMode,
            reverseLinked, reverseSerialBipolar,
            reverseBipolarAValue, reverseBipolarBValue,
            reverseAIsEnabled, reverseBIsEnabled,
            seqwencer::SequencerEngine::reverse,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::reverse),
            { reverseAttackAValue, reverseReleaseAValue,
              reverseAttackBValue, reverseReleaseBValue });
        const auto effectiveReverseAttackA = reverseEnvelopeValues[0];
        const auto effectiveReverseReleaseA = reverseEnvelopeValues[1];
        const auto effectiveReverseAttackB = reverseEnvelopeValues[2];
        const auto effectiveReverseReleaseB = reverseEnvelopeValues[3];
        const auto effectiveReverseSerialAttack = reverseUsesProfileB
            ? effectiveReverseAttackB : effectiveReverseAttackA;
        const auto effectiveReverseSerialRelease = reverseUsesProfileB
            ? effectiveReverseReleaseB : effectiveReverseReleaseA;
        if (reverseIsProcessing && reverseLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            reverseSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                reverseUnipolarPatternA, reverseUnipolarPatternB,
                reverseUnipolarSubdivisionsA,
                reverseUnipolarSubdivisionsB,
                reversePhase, reverseSampleRange,
                effectiveReverseSerialAttack,
                effectiveReverseSerialRelease,
                effectiveReverseSerialAttack,
                effectiveReverseSerialRelease,
                &activeBank, &activeStep, reverseTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 2));
            reverseSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                reverseCanonicalPatternA, reverseCanonicalPatternB,
                reverseCanonicalSubdivisionsA,
                reverseCanonicalSubdivisionsB,
                reversePhase, reverseSampleRange,
                effectiveReverseSerialAttack,
                effectiveReverseSerialRelease,
                effectiveReverseSerialAttack,
                effectiveReverseSerialRelease,
                nullptr, nullptr, reverseTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 2));
            reverseActiveStepA.store (activeBank == 0 ? activeStep : -1);
            reverseActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (reverseIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            reverseUnipolarA = seqwencer::evaluateSubdividedBankRange (
                reverseUnipolarPatternA, reverseUnipolarSubdivisionsA,
                reversePhase, reverseSampleRange,
                effectiveReverseAttackA, effectiveReverseReleaseA,
                &stepA, reverseTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 0));
            reverseUnipolarB = seqwencer::evaluateSubdividedBankRange (
                reverseUnipolarPatternB, reverseUnipolarSubdivisionsB,
                reversePhase, reverseSampleRange,
                effectiveReverseAttackB, effectiveReverseReleaseB,
                &stepB, reverseTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 1));
            reverseCanonicalA = seqwencer::evaluateSubdividedBankRange (
                reverseCanonicalPatternA, reverseCanonicalSubdivisionsA,
                reversePhase, reverseSampleRange,
                effectiveReverseAttackA, effectiveReverseReleaseA,
                nullptr, reverseTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 0));
            reverseCanonicalB = seqwencer::evaluateSubdividedBankRange (
                reverseCanonicalPatternB, reverseCanonicalSubdivisionsB,
                reversePhase, reverseSampleRange,
                effectiveReverseAttackB, effectiveReverseReleaseB,
                nullptr, reverseTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::reverse, 1));
            reverseActiveStepA.store (reverseAIsEnabled ? stepA : -1);
            reverseActiveStepB.store (reverseBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            reverseIsProcessing, reverseLinked, reverseUsesProfileB,
            reverseSerialBipolar, reverseBipolarAValue,
            reverseBipolarBValue, reverseAIsEnabled, reverseBIsEnabled,
            seqwencer::SequencerEngine::reverse, reverseTransformState,
            reverseUnipolarA, reverseUnipolarB,
            reverseCanonicalA, reverseCanonicalB,
            reverseSerialUnipolar, reverseSerialCanonical);

        const auto reverseTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (reverseLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return reverseSerialBipolar
                    ? 2.0f * reverseSerialCanonical - 1.0f
                    : reverseSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                reverseBipolarAValue
                    ? reverseCanonicalA : reverseUnipolarA,
                reverseAIsEnabled && targetIsActive (0, target), target,
                reverseBipolarAValue,
                reverseBipolarBValue
                    ? reverseCanonicalB : reverseUnipolarB,
                reverseBIsEnabled && targetIsActive (1, target), target,
                reverseBipolarBValue, assigned);
        };

        auto retriggerUnipolarA = 1.0f;
        auto retriggerUnipolarB = 1.0f;
        auto retriggerCanonicalA = 1.0f;
        auto retriggerCanonicalB = 1.0f;
        auto retriggerSerialUnipolar = 1.0f;
        auto retriggerSerialCanonical = 1.0f;
        const auto retriggerEnvelopeValues = modulatedEnvelopeValues (
            retriggerIsProcessing,
            retriggerUnipolarPatternA, retriggerUnipolarPatternB,
            retriggerCanonicalPatternA, retriggerCanonicalPatternB,
            retriggerUnipolarSubdivisionsA,
            retriggerUnipolarSubdivisionsB,
            retriggerCanonicalSubdivisionsA,
            retriggerCanonicalSubdivisionsB,
            retriggerPhase, retriggerSampleRange, retriggerTraversalMode,
            retriggerLinked, retriggerSerialBipolar,
            retriggerBipolarAValue, retriggerBipolarBValue,
            retriggerAIsEnabled, retriggerBIsEnabled,
            seqwencer::SequencerEngine::retrigger,
            seqwencer::sequencerEnvelopeTargets (
                seqwencer::SequencerEngine::retrigger),
            { retriggerAttackAValue, retriggerReleaseAValue,
              retriggerAttackBValue, retriggerReleaseBValue });
        const auto effectiveRetriggerAttackA = retriggerEnvelopeValues[0];
        const auto effectiveRetriggerReleaseA = retriggerEnvelopeValues[1];
        const auto effectiveRetriggerAttackB = retriggerEnvelopeValues[2];
        const auto effectiveRetriggerReleaseB = retriggerEnvelopeValues[3];
        const auto effectiveRetriggerSerialAttack = retriggerUsesProfileB
            ? effectiveRetriggerAttackB : effectiveRetriggerAttackA;
        const auto effectiveRetriggerSerialRelease = retriggerUsesProfileB
            ? effectiveRetriggerReleaseB : effectiveRetriggerReleaseA;
        if (retriggerIsProcessing && retriggerLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            retriggerSerialUnipolar = seqwencer::evaluateSubdividedLinkedRange (
                retriggerUnipolarPatternA, retriggerUnipolarPatternB,
                retriggerUnipolarSubdivisionsA,
                retriggerUnipolarSubdivisionsB,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerSerialAttack,
                effectiveRetriggerSerialRelease,
                effectiveRetriggerSerialAttack,
                effectiveRetriggerSerialRelease,
                &activeBank, &activeStep, retriggerTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 2));
            retriggerSerialCanonical = seqwencer::evaluateSubdividedLinkedRange (
                retriggerCanonicalPatternA, retriggerCanonicalPatternB,
                retriggerCanonicalSubdivisionsA,
                retriggerCanonicalSubdivisionsB,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerSerialAttack,
                effectiveRetriggerSerialRelease,
                effectiveRetriggerSerialAttack,
                effectiveRetriggerSerialRelease,
                nullptr, nullptr, retriggerTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 2));
            retriggerActiveStepA.store (activeBank == 0 ? activeStep : -1);
            retriggerActiveStepB.store (activeBank == 1 ? activeStep : -1);
        }
        else if (retriggerIsProcessing)
        {
            auto stepA = 0;
            auto stepB = 0;
            retriggerUnipolarA = seqwencer::evaluateSubdividedBankRange (
                retriggerUnipolarPatternA, retriggerUnipolarSubdivisionsA,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerAttackA, effectiveRetriggerReleaseA,
                &stepA, retriggerTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 0));
            retriggerUnipolarB = seqwencer::evaluateSubdividedBankRange (
                retriggerUnipolarPatternB, retriggerUnipolarSubdivisionsB,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerAttackB, effectiveRetriggerReleaseB,
                &stepB, retriggerTraversalMode, 0.0f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 1));
            retriggerCanonicalA = seqwencer::evaluateSubdividedBankRange (
                retriggerCanonicalPatternA, retriggerCanonicalSubdivisionsA,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerAttackA, effectiveRetriggerReleaseA,
                nullptr, retriggerTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 0));
            retriggerCanonicalB = seqwencer::evaluateSubdividedBankRange (
                retriggerCanonicalPatternB, retriggerCanonicalSubdivisionsB,
                retriggerPhase, retriggerSampleRange,
                effectiveRetriggerAttackB, effectiveRetriggerReleaseB,
                nullptr, retriggerTraversalMode, 0.5f,
                seqwencer::sequencerRandomStream (
                    seqwencer::SequencerEngine::retrigger, 1));
            retriggerActiveStepA.store (retriggerAIsEnabled ? stepA : -1);
            retriggerActiveStepB.store (retriggerBIsEnabled ? stepB : -1);
        }

        applySequencerTransforms (
            retriggerIsProcessing, retriggerLinked,
            retriggerUsesProfileB, retriggerSerialBipolar,
            retriggerBipolarAValue, retriggerBipolarBValue,
            retriggerAIsEnabled, retriggerBIsEnabled,
            seqwencer::SequencerEngine::retrigger,
            retriggerTransformState,
            retriggerUnipolarA, retriggerUnipolarB,
            retriggerCanonicalA, retriggerCanonicalB,
            retriggerSerialUnipolar, retriggerSerialCanonical);

        const auto retriggerTargetDeviation = [&] (
            seqwencer::ModulationTarget target, bool* assigned = nullptr)
        {
            if (retriggerLinked)
            {
                const auto active = targetIsActive (0, target);
                if (assigned != nullptr)
                    *assigned = active;
                if (! active)
                    return 0.0f;
                return retriggerSerialBipolar
                    ? 2.0f * retriggerSerialCanonical - 1.0f
                    : retriggerSerialUnipolar - 1.0f;
            }

            return seqwencer::combineParallelModulationDeviation (
                target,
                retriggerBipolarAValue
                    ? retriggerCanonicalA : retriggerUnipolarA,
                retriggerAIsEnabled && targetIsActive (0, target), target,
                retriggerBipolarAValue,
                retriggerBipolarBValue
                    ? retriggerCanonicalB : retriggerUnipolarB,
                retriggerBIsEnabled && targetIsActive (1, target), target,
                retriggerBipolarBValue, assigned);
        };

        const auto activeRetriggerStepA = retriggerActiveStepA.load();
        const auto activeRetriggerStepB = retriggerActiveStepB.load();
        const auto retriggerStepOn = seqwencer::retriggerStepIsOn (
            retriggerModesForA, retriggerModesForB, retriggerLinked,
            retriggerAIsEnabled, retriggerBIsEnabled,
            activeRetriggerStepA, activeRetriggerStepB);
        const auto retriggerModeAt = [] (
            const seqwencer::RetriggerStepPattern& modes, int step)
        {
            return step >= 0 && step < seqwencer::stepsPerBank
                && modes[static_cast<std::size_t> (step)];
        };
        auto retriggerStepWetAmount = 0.0f;
        if (retriggerLinked)
        {
            if (retriggerStepOn)
                retriggerStepWetAmount = retriggerSerialUnipolar;
        }
        else
        {
            if (retriggerAIsEnabled
                && retriggerModeAt (retriggerModesForA,
                                    activeRetriggerStepA))
            {
                retriggerStepWetAmount = retriggerUnipolarA;
            }
            if (retriggerBIsEnabled
                && retriggerModeAt (retriggerModesForB,
                                    activeRetriggerStepB))
            {
                retriggerStepWetAmount = juce::jmax (
                    retriggerStepWetAmount, retriggerUnipolarB);
            }
        }
        retriggerStepWetAmount = juce::jlimit (
            0.0f, 1.0f, retriggerStepWetAmount);

        const auto processGateStage = [&]
        {
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
                const auto noiseCoefficient =
                    noiseGateTarget > smoothedNoiseGateGain
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
                buffer.setSample (
                    channel, sample,
                    buffer.getSample (channel, sample) * outputGain);
        };

        const auto processDelayStage = [&]
        {
            if (! delayIsProcessing)
                return;
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
        };

        const auto processReverbStage = [&]
        {
            if (! reverbIsProcessing)
                return;
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
        };

        const auto processPanStage = [&]
        {
            if (! panIsProcessing)
                return;
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
        };

        const auto processFilterStage = [&]
        {
            if (! filterIsProcessing)
                return;
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
        };

        const auto processPitchStage = [&]
        {
            if (! pitchIsProcessing)
                return;
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
        };

        const auto processDistortionStage = [&]
        {
            if (! distortionIsProcessing)
                return;
            const auto modulatedDistortionNormalised = [&] (
                float baseNormalised, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = distortionTargetDeviation (
                    target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseNormalised, deviation, 1.0f)
                    : baseNormalised;
            };

            const auto driveDb = 36.0f * modulatedDistortionNormalised (
                baseDistortionDrive / 36.0f,
                seqwencer::ModulationTarget::distortionDrive);
            const auto toneValue = modulatedDistortionNormalised (
                baseDistortionTone,
                seqwencer::ModulationTarget::distortionTone);
            const auto mixValue = modulatedDistortionNormalised (
                baseDistortionMix,
                seqwencer::ModulationTarget::distortionMix);
            const auto inputGain = juce::Decibels::decibelsToGain (driveDb);
            constexpr auto toneMinimum = 800.0f;
            constexpr auto toneMaximum = 20000.0f;
            const auto toneCutoff = juce::jlimit (
                toneMinimum,
                static_cast<float> (juce::jmax (
                    static_cast<double> (toneMinimum),
                    currentSampleRate * 0.45)),
                toneMinimum * std::pow (
                    toneMaximum / toneMinimum, toneValue));
            const auto toneCoefficient = static_cast<float> (
                1.0 - std::exp (-juce::MathConstants<double>::twoPi
                                * toneCutoff / currentSampleRate));
            const auto channels = juce::jmin (
                buffer.getNumChannels(),
                static_cast<int> (distortionToneStates.size()));

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                const auto drivenSample = drySample * inputGain;
                auto wetSample = drivenSample;

                switch (baseDistortionType)
                {
                    case 1:
                        wetSample = juce::jlimit (-1.0f, 1.0f, drivenSample);
                        break;
                    case 2:
                    {
                        const auto tubeInput = juce::jlimit (
                            -1.5f, 1.5f, drivenSample);
                        wetSample = (tubeInput
                            * (1.0f - tubeInput * tubeInput / 9.0f)) / 1.125f;
                        break;
                    }
                    case 3:
                        wetSample = drivenSample > 1.0f || drivenSample < -1.0f
                            ? std::abs (std::abs (
                                std::fmod (drivenSample - 1.0f, 4.0f)) - 2.0f)
                                - 1.0f
                            : drivenSample;
                        break;
                    default:
                        wetSample = std::tanh (drivenSample);
                        break;
                }

                if (toneValue < 0.999f)
                {
                    auto& toneState = distortionToneStates[
                        static_cast<std::size_t> (channel)];
                    toneState += toneCoefficient * (wetSample - toneState);
                    wetSample = toneState;
                }
                else
                {
                    distortionToneStates[
                        static_cast<std::size_t> (channel)] = wetSample;
                }

                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (wetSample - drySample));
            }
        };

        const auto processGrainStage = [&]
        {
            if (! grainIsProcessing)
                return;
            const auto modulatedGrainNormalised = [&] (
                float baseNormalised, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = grainTargetDeviation (target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseNormalised, deviation, 1.0f)
                    : baseNormalised;
            };

            const auto sizeNormalised = modulatedGrainNormalised (
                (baseGrainSizeMs - 10.0f) / 240.0f,
                seqwencer::ModulationTarget::grainSize);
            const auto sizeMs = 10.0f + 240.0f * sizeNormalised;
            const auto shiftNormalised = modulatedGrainNormalised (
                (baseGrainShift + 24.0f) / 48.0f,
                seqwencer::ModulationTarget::grainShift);
            const auto semitones = -24.0f + 48.0f * shiftNormalised;
            const auto feedbackValue = 0.90f * modulatedGrainNormalised (
                baseGrainFeedback / 0.90f,
                seqwencer::ModulationTarget::grainFeedback);
            const auto mixValue = modulatedGrainNormalised (
                baseGrainMix, seqwencer::ModulationTarget::grainMix);
            const auto ratio = std::pow (
                2.0, static_cast<double> (semitones) / 12.0);
            const auto windowSamples = juce::jlimit (
                64,
                grainBuffer.getNumSamples() - 8,
                static_cast<int> (std::lround (
                    currentSampleRate * 0.001 * sizeMs)));
            const auto windowSpan = static_cast<double> (windowSamples - 1);
            const auto phaseA = grainReadPhase;
            const auto phaseB = std::fmod (grainReadPhase + 0.5, 1.0);
            const auto weightA = 0.5 - 0.5 * std::cos (
                juce::MathConstants<double>::twoPi * phaseA);
            const auto weightB = 0.5 - 0.5 * std::cos (
                juce::MathConstants<double>::twoPi * phaseB);
            const auto weightTotal = juce::jmax (1.0e-9, weightA + weightB);
            const auto channels = juce::jmin (
                buffer.getNumChannels(), grainBuffer.getNumChannels());

            const auto readGrainSample = [&] (int channel, double phase)
            {
                const auto delaySamples = 4.0 + phase * windowSpan;
                auto readPosition = static_cast<double> (grainWritePosition)
                                  - delaySamples;
                const auto bufferLength = grainBuffer.getNumSamples();
                while (readPosition < 0.0)
                    readPosition += static_cast<double> (bufferLength);
                while (readPosition >= static_cast<double> (bufferLength))
                    readPosition -= static_cast<double> (bufferLength);
                const auto index0 = static_cast<int> (std::floor (readPosition));
                const auto index1 = (index0 + 1) % bufferLength;
                const auto fraction = static_cast<float> (
                    readPosition - std::floor (readPosition));
                const auto sample0 = grainBuffer.getSample (channel, index0);
                const auto sample1 = grainBuffer.getSample (channel, index1);
                return sample0 + fraction * (sample1 - sample0);
            };

            for (int channel = 0; channel < channels; ++channel)
            {
                const auto stateIndex = static_cast<std::size_t> (channel);
                const auto drySample = buffer.getSample (channel, sample);
                const auto feedbackSample = juce::jlimit (
                    -4.0f, 4.0f,
                    drySample + feedbackValue * grainFeedbackStates[stateIndex]);
                grainBuffer.setSample (
                    channel, grainWritePosition, feedbackSample);
                const auto wetSample = static_cast<float> ((
                    weightA * readGrainSample (channel, phaseA)
                    + weightB * readGrainSample (channel, phaseB))
                    / weightTotal);
                grainFeedbackStates[stateIndex] = wetSample;
                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (wetSample - drySample));
            }

            grainWritePosition = (grainWritePosition + 1)
                               % grainBuffer.getNumSamples();
            grainReadPhase = std::fmod (
                grainReadPhase + (1.0 - ratio) / windowSpan + 1.0, 1.0);
        };

        const auto processCompressorStage = [&]
        {
            if (! compressorIsProcessing)
                return;
            const auto modulatedCompressorActual = [&] (
                float baseValue, float minimumValue, float maximumValue,
                seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = compressorTargetDeviation (
                    target, &assigned);
                if (! assigned)
                    return baseValue;
                const auto normalised = (baseValue - minimumValue)
                                      / (maximumValue - minimumValue);
                return minimumValue + (maximumValue - minimumValue)
                    * seqwencer::applyModulationDepth (
                        normalised, deviation, 1.0f);
            };

            const auto thresholdDb = modulatedCompressorActual (
                baseCompressorThreshold, -60.0f, 0.0f,
                seqwencer::ModulationTarget::compressorThreshold);
            const auto ratioValue = modulatedCompressorActual (
                baseCompressorRatio, 1.0f, 20.0f,
                seqwencer::ModulationTarget::compressorRatio);
            const auto attackMs = modulatedCompressorActual (
                baseCompressorAttack, 0.1f, 100.0f,
                seqwencer::ModulationTarget::compressorAttack);
            const auto releaseMs = modulatedCompressorActual (
                baseCompressorRelease, 10.0f, 1000.0f,
                seqwencer::ModulationTarget::compressorRelease);
            const auto makeupDb = modulatedCompressorActual (
                baseCompressorMakeup, 0.0f, 24.0f,
                seqwencer::ModulationTarget::compressorMakeup);
            const auto mixValue = modulatedCompressorActual (
                baseCompressorMix, 0.0f, 1.0f,
                seqwencer::ModulationTarget::compressorMix);

            auto detectorPeak = 0.0f;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                detectorPeak = juce::jmax (
                    detectorPeak,
                    std::abs (buffer.getSample (channel, sample)));
            const auto inputDb = juce::Decibels::gainToDecibels (
                detectorPeak, -100.0f);
            const auto overThreshold = juce::jmax (0.0f, inputDb - thresholdDb);
            const auto gainReductionDb = -overThreshold
                * (1.0f - 1.0f / juce::jmax (1.0f, ratioValue));
            const auto targetGain = juce::Decibels::decibelsToGain (
                gainReductionDb);
            const auto timeMs = targetGain < compressorGain
                ? attackMs : releaseMs;
            const auto coefficient = static_cast<float> (std::exp (
                -1.0 / juce::jmax (
                    1.0, 0.001 * static_cast<double> (timeMs)
                         * currentSampleRate)));
            compressorGain = coefficient * compressorGain
                           + (1.0f - coefficient) * targetGain;
            const auto makeupGain = juce::Decibels::decibelsToGain (makeupDb);
            const auto wetGain = compressorGain * makeupGain;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                const auto wetSample = drySample * wetGain;
                buffer.setSample (
                    channel, sample,
                    drySample + mixValue * (wetSample - drySample));
            }
        };

        const auto processReverseStage = [&]
        {
            if (! reverseIsProcessing)
                return;
            const auto modulatedReverseNormalised = [&] (
                float baseNormalised, seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = reverseTargetDeviation (
                    target, &assigned);
                return assigned
                    ? seqwencer::applyModulationDepth (
                        baseNormalised, deviation, 1.0f)
                    : baseNormalised;
            };

            const auto timeNormalised = modulatedReverseNormalised (
                (baseReverseTimeMs - 25.0f) / 1975.0f,
                seqwencer::ModulationTarget::reverseTime);
            const auto timeMs = 25.0f + 1975.0f * timeNormalised;
            const auto pointAValue = modulatedReverseNormalised (
                baseReversePointA,
                seqwencer::ModulationTarget::reversePointA);
            const auto pointBValue = modulatedReverseNormalised (
                baseReversePointB,
                seqwencer::ModulationTarget::reversePointB);
            const auto mixValue = modulatedReverseNormalised (
                baseReverseMix,
                seqwencer::ModulationTarget::reverseMix);
            const auto bufferCapacity = reverseBuffers[0].getNumSamples();
            reversePendingCaptureSamples = juce::jlimit (
                2, bufferCapacity,
                static_cast<int> (std::lround (
                    timeMs * 0.001f
                        * static_cast<float> (currentSampleRate))));
            const auto reverseStepOn = seqwencer::reverseStepIsOn (
                reverseModesForA, reverseModesForB,
                reverseLinked, reverseAIsEnabled, reverseBIsEnabled,
                reverseActiveStepA.load (std::memory_order_relaxed),
                reverseActiveStepB.load (std::memory_order_relaxed));
            const auto previousOrdinal = reversePreviousSequencePhase >= 0.0
                ? static_cast<juce::int64> (std::floor (
                    reversePreviousSequencePhase))
                : std::numeric_limits<juce::int64>::min();
            const auto currentOrdinal = static_cast<juce::int64> (
                std::floor (reversePhase));
            const auto sequenceRestarted = reversePreviousSequencePhase >= 0.0
                && reversePhase + 1.0e-9 < reversePreviousSequencePhase;
            const auto enteredNewStep = reversePreviousSequencePhase < 0.0
                || currentOrdinal != previousOrdinal || sequenceRestarted;
            const auto shouldTriggerReverse = reverseStepOn
                && (! reverseStepWasOn || enteredNewStep);
            reversePreviousSequencePhase = reversePhase;
            reverseStepWasOn = reverseStepOn;

            if (shouldTriggerReverse)
            {
                const auto availableSamples = juce::jmin (
                    reversePendingCaptureSamples,
                    reverseCaptureAvailableSamples);
                if (availableSamples >= 2)
                {
                    reversePlaybackBufferIndex = reverseCaptureBufferIndex;
                    reverseCapturedSamples = availableSamples;
                    reversePlaybackStartPosition =
                        (reverseCaptureWritePosition - reverseCapturedSamples
                            + bufferCapacity) % bufferCapacity;
                    reverseCaptureBufferIndex = 1 - reverseCaptureBufferIndex;
                    reverseCaptureWritePosition = 0;
                    reverseCaptureAvailableSamples = 0;
                    const auto bounds = seqwencer::resolveReverseLoopBounds (
                        reverseCapturedSamples, pointAValue, pointBValue);
                    reverseReadPosition = static_cast<double> (bounds.last);
                    reverseReadDirection = -1;
                    reverseWetFade = 0.0f;
                    reverseIsPlaying = true;
                }
                else if (reverseIsPlaying && reverseCapturedSamples >= 2)
                {
                    const auto bounds = seqwencer::resolveReverseLoopBounds (
                        reverseCapturedSamples, pointAValue, pointBValue);
                    reverseReadPosition = static_cast<double> (bounds.last);
                    reverseReadDirection = -1;
                    reverseWetFade = 0.0f;
                }
            }

            const auto fadeIncrement = static_cast<float> (
                1.0 / juce::jmax (1.0, currentSampleRate * 0.005));
            const auto wetTarget = reverseStepOn && reverseIsPlaying
                ? 1.0f : 0.0f;
            reverseWetFade = wetTarget > reverseWetFade
                ? juce::jmin (wetTarget, reverseWetFade + fadeIncrement)
                : juce::jmax (wetTarget, reverseWetFade - fadeIncrement);

            auto bounds = seqwencer::ReverseLoopBounds {};
            auto index0 = 0;
            auto index1 = 1;
            auto fraction = 0.0f;
            const auto canPlayReverse = reverseIsPlaying
                && reverseCapturedSamples >= 2 && reverseWetFade > 0.0f;
            if (canPlayReverse)
            {
                bounds = seqwencer::resolveReverseLoopBounds (
                    reverseCapturedSamples, pointAValue, pointBValue);
                reverseReadPosition = juce::jlimit (
                    static_cast<double> (bounds.first),
                    static_cast<double> (bounds.last), reverseReadPosition);
                index0 = juce::jlimit (
                    bounds.first, bounds.last,
                    static_cast<int> (std::floor (reverseReadPosition)));
                index1 = juce::jmin (bounds.last, index0 + 1);
                fraction = static_cast<float> (
                    reverseReadPosition - std::floor (reverseReadPosition));
            }

            auto& captureBuffer = reverseBuffers[static_cast<std::size_t> (
                reverseCaptureBufferIndex)];
            const auto& playbackBuffer = reverseBuffers[
                static_cast<std::size_t> (reversePlaybackBufferIndex)];
            const auto channels = juce::jmin (
                buffer.getNumChannels(), captureBuffer.getNumChannels());
            const auto effectiveMix = mixValue * reverseWetFade;
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                captureBuffer.setSample (
                    channel, reverseCaptureWritePosition, drySample);
                if (canPlayReverse)
                {
                    const auto physicalIndex0 =
                        (reversePlaybackStartPosition + index0)
                            % bufferCapacity;
                    const auto physicalIndex1 =
                        (reversePlaybackStartPosition + index1)
                            % bufferCapacity;
                    const auto sample0 = playbackBuffer.getSample (
                        channel, physicalIndex0);
                    const auto sample1 = playbackBuffer.getSample (
                        channel, physicalIndex1);
                    const auto wetSample = sample0
                                         + fraction * (sample1 - sample0);
                    buffer.setSample (
                        channel, sample,
                        drySample + effectiveMix * (wetSample - drySample));
                }
            }
            reverseCaptureWritePosition =
                (reverseCaptureWritePosition + 1) % bufferCapacity;
            reverseCaptureAvailableSamples = juce::jmin (
                bufferCapacity, reverseCaptureAvailableSamples + 1);

            if (canPlayReverse)
            {
                const auto next = seqwencer::advancePingPongPosition (
                    reverseReadPosition, reverseReadDirection, bounds);
                reverseReadPosition = next.position;
                reverseReadDirection = next.direction;
            }
        };

        const auto processRetriggerStage = [&]
        {
            if (! retriggerIsProcessing)
                return;

            const auto modulatedRetriggerActual = [&] (
                float baseValue, float minimumValue, float maximumValue,
                seqwencer::ModulationTarget target)
            {
                bool assigned = false;
                const auto deviation = retriggerTargetDeviation (
                    target, &assigned);
                if (! assigned)
                    return baseValue;
                const auto normalised = (baseValue - minimumValue)
                                      / (maximumValue - minimumValue);
                return minimumValue + (maximumValue - minimumValue)
                    * seqwencer::applyModulationDepth (
                        normalised, deviation, 1.0f);
            };

            const auto initialRepeatsValue = modulatedRetriggerActual (
                baseRetriggerInitialSpeed, 1.0f, 16.0f,
                seqwencer::ModulationTarget::retriggerInitialSpeed);
            const auto finalRepeatsValue = modulatedRetriggerActual (
                baseRetriggerFinalSpeed, 1.0f, 16.0f,
                seqwencer::ModulationTarget::retriggerFinalSpeed);
            const auto transitionSteps = modulatedRetriggerActual (
                baseRetriggerTransitionSteps, 0.0f, 16.0f,
                seqwencer::ModulationTarget::retriggerTransition);
            const auto decayValue = modulatedRetriggerActual (
                baseRetriggerDecay, 0.0f, 1.0f,
                seqwencer::ModulationTarget::retriggerDecay);
            const auto mixValue = modulatedRetriggerActual (
                baseRetriggerMix, 0.0f, 1.0f,
                seqwencer::ModulationTarget::retriggerMix);
            const auto capacity = retriggerBuffer.getNumSamples();
            const auto sequencerStepSamples = juce::jmax (
                2.0, 1.0 / juce::jmax (1.0e-12,
                                       retriggerPhaseIncrement));

            if (retriggerStepOn && ! retriggerStepWasOn)
            {
                retriggerCaptureSamples = 0;
                retriggerCaptureTargetSamples = 2;
                retriggerReadPosition = 0;
                retriggerSamplesUntilRepeat = 0;
                retriggerPlaybackSamples = 0;
                retriggerRepeatIndex = 0;
                retriggerWetFade = 0.0f;
                retriggerIsPlaying = false;
                retriggerBlockIsActive = true;
            }
            else if (! retriggerStepOn && retriggerStepWasOn)
            {
                retriggerBlockIsActive = false;
            }
            retriggerStepWasOn = retriggerStepOn;

            const auto fadeIncrement = static_cast<float> (
                1.0 / juce::jmax (1.0, currentSampleRate * 0.005));
            const auto wetTarget = retriggerBlockIsActive
                && retriggerIsPlaying ? retriggerStepWetAmount : 0.0f;
            if (retriggerWetFade < wetTarget)
                retriggerWetFade = juce::jmin (
                    wetTarget, retriggerWetFade + fadeIncrement);
            else if (retriggerWetFade > wetTarget)
                retriggerWetFade = juce::jmax (
                    wetTarget, retriggerWetFade - fadeIncrement);

            if (! retriggerBlockIsActive && retriggerWetFade <= 0.0f)
            {
                retriggerCaptureSamples = 0;
                retriggerIsPlaying = false;
                return;
            }

            if (! retriggerIsPlaying)
            {
                if (! retriggerBlockIsActive)
                    return;

                if (retriggerCaptureSamples == 0)
                {
                    retriggerCaptureTargetSamples = juce::jlimit (
                        2, capacity,
                        seqwencer::retriggerIntervalSamples (
                            sequencerStepSamples,
                            juce::jmin (initialRepeatsValue,
                                        finalRepeatsValue)));
                }

                const auto channels = juce::jmin (
                    buffer.getNumChannels(), retriggerBuffer.getNumChannels());
                for (int channel = 0; channel < channels; ++channel)
                    retriggerBuffer.setSample (
                        channel, retriggerCaptureSamples,
                        buffer.getSample (channel, sample));
                ++retriggerCaptureSamples;
                if (retriggerCaptureSamples >= retriggerCaptureTargetSamples)
                {
                    retriggerIsPlaying = true;
                    retriggerReadPosition = 0;
                    retriggerSamplesUntilRepeat = 0;
                    retriggerPlaybackSamples = 0;
                    retriggerRepeatIndex = 0;
                }
                return;
            }

            const auto elapsedSteps = static_cast<double> (
                retriggerPlaybackSamples) / sequencerStepSamples;
            const auto currentRepeats = seqwencer::retriggerRepeatsAtStep (
                initialRepeatsValue, finalRepeatsValue,
                transitionSteps, elapsedSteps);
            const auto intervalSamples = seqwencer::retriggerIntervalSamples (
                sequencerStepSamples, currentRepeats);
            if (retriggerSamplesUntilRepeat <= 0)
            {
                if (retriggerPlaybackSamples > 0)
                    ++retriggerRepeatIndex;
                retriggerReadPosition = 0;
                retriggerSamplesUntilRepeat = intervalSamples;
            }
            const auto readIndex = retriggerReadPosition
                                 % retriggerCaptureTargetSamples;
            const auto repeatGain = seqwencer::retriggerRepeatGain (
                decayValue, retriggerRepeatIndex);
            const auto effectiveMix = mixValue * retriggerWetFade;
            const auto channels = juce::jmin (
                buffer.getNumChannels(), retriggerBuffer.getNumChannels());
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto drySample = buffer.getSample (channel, sample);
                const auto wetSample = repeatGain
                    * retriggerBuffer.getSample (channel, readIndex);
                buffer.setSample (
                    channel, sample,
                    drySample + effectiveMix * (wetSample - drySample));
            }

            ++retriggerReadPosition;
            --retriggerSamplesUntilRepeat;
            ++retriggerPlaybackSamples;
        };

        for (const auto stage : orderedAudioFx)
        {
            switch (stage)
            {
                case seqwencer::AudioFxStage::gate:       processGateStage(); break;
                case seqwencer::AudioFxStage::delay:      processDelayStage(); break;
                case seqwencer::AudioFxStage::reverb:     processReverbStage(); break;
                case seqwencer::AudioFxStage::pan:        processPanStage(); break;
                case seqwencer::AudioFxStage::filter:     processFilterStage(); break;
                case seqwencer::AudioFxStage::pitch:      processPitchStage(); break;
                case seqwencer::AudioFxStage::distortion: processDistortionStage(); break;
                case seqwencer::AudioFxStage::grain:      processGrainStage(); break;
                case seqwencer::AudioFxStage::compressor: processCompressorStage(); break;
                case seqwencer::AudioFxStage::reverse:    processReverseStage(); break;
                case seqwencer::AudioFxStage::retrigger:  processRetriggerStage(); break;
            }
        }

        const auto mayAdvance = ! hostSyncEnabled || hostTimelineAdvancing;
        if (gateIsProcessing && ! gateUseHostPosition
            && (mayAdvance || gateRetriggersFromPlayedNotes))
        {
            gateFreeRunningPhase += gatePhaseIncrement;
        }
        for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
        {
            const auto pairIndex = static_cast<std::size_t> (pair);
            const auto& state = phiPairs[pairIndex];
            if (! state.useHostPosition
                && (mayAdvance || state.retriggersFromPlayedNotes))
            {
                phiFreeRunningPhases[pairIndex] += state.phaseIncrement;
            }
        }
        if (delayIsProcessing && ! delayUseHostPosition
            && (mayAdvance || delayRetriggersFromPlayedNotes))
        {
            delayFreeRunningPhase += delayPhaseIncrement;
        }
        if (reverbIsProcessing && ! reverbUseHostPosition
            && (mayAdvance || reverbRetriggersFromPlayedNotes))
        {
            reverbFreeRunningPhase += reverbPhaseIncrement;
        }
        if (panIsProcessing && ! panUseHostPosition
            && (mayAdvance || panRetriggersFromPlayedNotes))
        {
            panFreeRunningPhase += panPhaseIncrement;
        }
        if (filterIsProcessing && ! filterUseHostPosition
            && (mayAdvance || filterRetriggersFromPlayedNotes))
        {
            filterFreeRunningPhase += filterPhaseIncrement;
        }
        if (pitchIsProcessing && ! pitchUseHostPosition
            && (mayAdvance || pitchRetriggersFromPlayedNotes))
        {
            pitchFreeRunningPhase += pitchPhaseIncrement;
        }
        if (distortionIsProcessing && ! distortionUseHostPosition
            && (mayAdvance || distortionRetriggersFromPlayedNotes))
        {
            distortionFreeRunningPhase += distortionPhaseIncrement;
        }
        if (grainIsProcessing && ! grainUseHostPosition
            && (mayAdvance || grainRetriggersFromPlayedNotes))
        {
            grainFreeRunningPhase += grainPhaseIncrement;
        }
        if (compressorIsProcessing && ! compressorUseHostPosition
            && (mayAdvance || compressorRetriggersFromPlayedNotes))
        {
            compressorFreeRunningPhase += compressorPhaseIncrement;
        }
        if (reverseIsProcessing && ! reverseUseHostPosition
            && (mayAdvance || reverseRetriggersFromPlayedNotes))
        {
            reverseFreeRunningPhase += reversePhaseIncrement;
        }
        if (retriggerIsProcessing && ! retriggerUseHostPosition
            && (mayAdvance || retriggerRetriggersFromPlayedNotes))
        {
            retriggerFreeRunningPhase += retriggerPhaseIncrement;
        }
    }

    if (gateIsProcessing && gateUseHostPosition && hostTimelineAdvancing)
    {
        gateFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            gateStepBeats);
    }
    for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
    {
        const auto pairIndex = static_cast<std::size_t> (pair);
        const auto& state = phiPairs[pairIndex];
        if (state.useHostPosition && hostTimelineAdvancing)
        {
            phiFreeRunningPhases[pairIndex] =
                seqwencer::unwrappedPhaseFromQuarterNotes (
                    hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
                    state.stepBeats);
        }
    }
    if (delayIsProcessing && delayUseHostPosition && hostTimelineAdvancing)
    {
        delayFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            delayStepBeats);
    }
    if (reverbIsProcessing && reverbUseHostPosition && hostTimelineAdvancing)
    {
        reverbFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            reverbStepBeats);
    }
    if (panIsProcessing && panUseHostPosition && hostTimelineAdvancing)
    {
        panFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            panStepBeats);
    }
    if (filterIsProcessing && filterUseHostPosition && hostTimelineAdvancing)
    {
        filterFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            filterStepBeats);
    }
    if (pitchIsProcessing && pitchUseHostPosition && hostTimelineAdvancing)
    {
        pitchFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            pitchStepBeats);
    }
    if (distortionIsProcessing && distortionUseHostPosition
        && hostTimelineAdvancing)
    {
        distortionFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            distortionStepBeats);
    }
    if (grainIsProcessing && grainUseHostPosition && hostTimelineAdvancing)
    {
        grainFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            grainStepBeats);
    }
    if (compressorIsProcessing && compressorUseHostPosition
        && hostTimelineAdvancing)
    {
        compressorFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            compressorStepBeats);
    }
    if (reverseIsProcessing && reverseUseHostPosition
        && hostTimelineAdvancing)
    {
        reverseFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            reverseStepBeats);
    }
    if (retriggerIsProcessing && retriggerUseHostPosition
        && hostTimelineAdvancing)
    {
        retriggerFreeRunningPhase = seqwencer::unwrappedPhaseFromQuarterNotes (
            hostPpq + buffer.getNumSamples() * quarterNotesPerSample,
            retriggerStepBeats);
    }

    if (shouldSendPhiBridge && buffer.getNumSamples() > 0)
    {
        for (int pair = 0; pair < seqwencer::phiSequencerPairCount; ++pair)
        {
            const auto& state = phiPairs[static_cast<std::size_t> (pair)];
            for (int localLane = 0; localLane < 2; ++localLane)
            {
                const auto index = static_cast<std::size_t> (localLane);
                const auto packet = seqwencer_bridge::encodeLaneValue (
                    pair * 2 + localLane,
                    state.linked ? state.serialBipolar : state.bipolar[index],
                    state.linked ? localLane == 0 : state.enabled[index],
                    state.lastValues[index],
                    phiSerialPairMask);
                midiMessages.addEvent (
                    juce::MidiMessage::createSysExMessage (
                        packet.data(), static_cast<int> (packet.size())),
                    0);
            }
        }
    }

    if (buffer.getNumSamples() > 0
        && phiTargetBrowserRequestPending.exchange (
            false, std::memory_order_acq_rel))
    {
        const auto packet = seqwencer_bridge::encodeTargetBrowserRequest (
            phiSerialPairMask);
        midiMessages.addEvent (
            juce::MidiMessage::createSysExMessage (
                packet.data(), static_cast<int> (packet.size())),
            0);
    }
}

void SeqwencerAudioProcessor::getStateInformation (juce::MemoryBlock& data)
{
    auto state = parameters.copyState();
    state.setProperty ("audio_fx_order",
                       serialiseAudioFxOrder (getAudioFxOrder()), nullptr);
    writeStepSubdivisionsToState (state);
    if (const auto xml = state.createXml())
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
            syncAudioFxOrderFromState();
            readStepSubdivisionsFromState (parameters.state);
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
