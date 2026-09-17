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

    for (int index = 0; index < seqwencer::gateModulationTargetCount; ++index)
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
             << "\r\n[PHI]\r\n" << phiParameterLines;
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
            || line == "[Gate]" || line == "[PHI]")
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

    return layout;
}

void SeqwencerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    gateFreeRunningPhase = 0.0;
    phiFreeRunningPhase = 0.0;
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
}

void SeqwencerAudioProcessor::releaseResources()
{
    notePhraseTracker.reset();
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
    const auto gateCycleLength = seqwencer::sequenceCycleLength (
        gateRange.length(), gateTraversalMode);
    const auto phiCycleLength = seqwencer::sequenceCycleLength (
        phiRange.length(), phiTraversalMode);
    const auto gatePhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * gateStepBeats * currentSampleRate / hostBpm);
    const auto phiPhaseIncrement = 1.0 / juce::jmax (
        1.0, 60.0 * phiStepBeats * currentSampleRate / hostBpm);
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
    const auto noiseThresholdDb = juce::jlimit (
        -80.0f, 0.0f,
        noiseGateThreshold != nullptr ? noiseGateThreshold->load() : -60.0f);
    const auto noiseThresholdGain =
        juce::Decibels::decibelsToGain (noiseThresholdDb);
    const auto noiseCloseThresholdGain = noiseThresholdGain
                                       * juce::Decibels::decibelsToGain (-3.0f);
    const auto noiseAttackSeconds = 0.001 * juce::jlimit (
        0.1f, 100.0f,
        noiseGateAttack != nullptr ? noiseGateAttack->load() : 5.0f);
    const auto noiseHoldSamples = static_cast<int> (std::lround (
        0.001 * juce::jlimit (
            0.0f, 500.0f,
            noiseGateHold != nullptr ? noiseGateHold->load() : 50.0f)
        * currentSampleRate));
    const auto noiseReleaseSeconds = 0.001 * juce::jlimit (
        5.0f, 1000.0f,
        noiseGateRelease != nullptr ? noiseGateRelease->load() : 100.0f);
    const auto noiseClosedGain = 1.0f - juce::jlimit (
        0.0f, 1.0f,
        noiseGateRange != nullptr ? noiseGateRange->load() : 1.0f);
    const auto gateAIsEnabled = seqAEnabled == nullptr
                             || seqAEnabled->load() >= 0.5f;
    const auto gateBIsEnabled = seqBEnabled != nullptr
                             && seqBEnabled->load() >= 0.5f;
    const auto phiAIsEnabled = phiSeqAEnabled == nullptr
                            || phiSeqAEnabled->load() >= 0.5f;
    const auto phiBIsEnabled = phiSeqBEnabled != nullptr
                            && phiSeqBEnabled->load() >= 0.5f;
    const auto targetIsActive = [this] (
        int bank, seqwencer::ModulationTarget target)
    {
        const auto index = static_cast<int> (target) - 1;
        if (index < 0 || index >= seqwencer::gateModulationTargetCount)
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
    const auto noiseAttackCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / juce::jmax (
            1.0, noiseAttackSeconds * currentSampleRate)));
    const auto noiseReleaseCoefficient = static_cast<float> (
        1.0 - std::exp (-1.0 / juce::jmax (
            1.0, noiseReleaseSeconds * currentSampleRate)));
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

        if (gateLinked)
        {
            auto activeBank = 0;
            auto activeStep = 0;
            const auto serialValue = seqwencer::evaluateLinkedRange (
                gatePatternA, gatePatternB, gatePhase, gateRange,
                gateSerialAttack, gateSerialRelease,
                gateSerialAttack, gateSerialRelease,
                nullptr, nullptr, gateTraversalMode);
            const auto depth = targetIsActive (
                                   0, seqwencer::ModulationTarget::gateDepth)
                ? seqwencer::modulatedCeiling (baseDepth, serialValue)
                : baseDepth;
            const auto shortLength = targetIsActive (
                    0, seqwencer::ModulationTarget::shortGateLength)
                ? seqwencer::modulatedRangeFromMinimum (
                    0.10f, baseShortLength, serialValue)
                : baseShortLength;
            const auto longLength = targetIsActive (
                    0, seqwencer::ModulationTarget::longGateLength)
                ? seqwencer::modulatedRangeFromMinimum (
                    0.65f, baseLongLength, serialValue)
                : baseLongLength;
            const auto value = seqwencer::evaluateGateLinkedRange (
                gateLevelPatternA, gateLevelPatternB,
                gateModesForA, gateModesForB,
                gatePhase, gateRange,
                gateSerialAttack, gateSerialRelease,
                gateSerialAttack, gateSerialRelease,
                &activeBank, &activeStep, shortLength, longLength,
                gateTraversalMode);
            const auto gateDeviation = value - 1.0f;
            gateActiveStepA.store (activeBank == 0 ? activeStep : -1);
            gateActiveStepB.store (activeBank == 1 ? activeStep : -1);

            const auto targetGain = shouldGate && ! shouldBypass
                ? seqwencer::applyModulationDepth (base, gateDeviation, depth)
                : 1.0f;
            smoothedGain += safetyCoefficient * (targetGain - smoothedGain);
        }
        else
        {
            auto stepA = 0;
            auto stepB = 0;
            const auto sequenceValueA = seqwencer::evaluateBankRange (
                gatePatternA, gatePhase, gateRange,
                gateAttackA, gateReleaseA, nullptr, gateTraversalMode);
            const auto sequenceValueB = seqwencer::evaluateBankRange (
                gatePatternB, gatePhase, gateRange,
                gateAttackB, gateReleaseB, nullptr, gateTraversalMode);
            const auto combinedTargetValue = [&] (
                seqwencer::ModulationTarget target, bool* assigned = nullptr)
            {
                auto result = 1.0f;
                auto found = false;
                if (gateAIsEnabled && targetIsActive (0, target))
                {
                    result *= sequenceValueA;
                    found = true;
                }
                if (gateBIsEnabled && targetIsActive (1, target))
                {
                    result *= sequenceValueB;
                    found = true;
                }
                if (assigned != nullptr)
                    *assigned = found;
                return result;
            };
            bool depthIsAssigned = false;
            bool shortIsAssigned = false;
            bool longIsAssigned = false;
            const auto depthValue = combinedTargetValue (
                seqwencer::ModulationTarget::gateDepth, &depthIsAssigned);
            const auto shortValue = combinedTargetValue (
                seqwencer::ModulationTarget::shortGateLength, &shortIsAssigned);
            const auto longValue = combinedTargetValue (
                seqwencer::ModulationTarget::longGateLength, &longIsAssigned);
            const auto depth = depthIsAssigned
                ? seqwencer::modulatedCeiling (baseDepth, depthValue)
                : baseDepth;
            const auto shortLength = shortIsAssigned
                ? seqwencer::modulatedRangeFromMinimum (
                    0.10f, baseShortLength, shortValue)
                : baseShortLength;
            const auto longLength = longIsAssigned
                ? seqwencer::modulatedRangeFromMinimum (
                    0.65f, baseLongLength, longValue)
                : baseLongLength;
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
            const auto gateDeviation = combinedGate - 1.0f;

            const auto targetGain = shouldGate && ! shouldBypass
                ? seqwencer::applyModulationDepth (base, gateDeviation, depth)
                : 1.0f;
            smoothedGain += safetyCoefficient * (targetGain - smoothedGain);
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

        if (shouldGate && shouldNoiseGate && ! shouldBypass)
        {
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

        const auto mayAdvance = ! hostSyncEnabled || hostTimelineAdvancing;
        if (! gateUseHostPosition
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
    }

    if (gateUseHostPosition && hostTimelineAdvancing)
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
