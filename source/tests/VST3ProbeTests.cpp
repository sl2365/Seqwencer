#include <JuceHeader.h>

#include "../SequencerCore.h"
#include "../SeqwencerBridgeProtocol.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>

namespace
{
class StalledNestedHostPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        position.setBpm(120.0);
        position.setPpqPosition(0.0);
        position.setTimeInSamples(0);
        position.setIsPlaying(false);
        return position;
    }
};
}

int main (int argumentCount, char* arguments[])
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;

    if (argumentCount != 2)
    {
        std::cout << "FAIL: expected the Seqwencer.vst3 path\n";
        return 1;
    }

    const juce::String pluginPath (arguments[1]);
    juce::AudioPluginFormatManager formatManager;
    auto format = std::make_unique<juce::VST3PluginFormat>();
    auto* formatPointer = format.get();
    formatManager.addFormat (std::move (format));
    juce::OwnedArray<juce::PluginDescription> descriptions;
    formatPointer->findAllTypesForFile (descriptions, pluginPath);

    if (descriptions.isEmpty())
    {
        std::cout << "FAIL: VST3 scan found no Seqwencer type\n";
        return 1;
    }

    juce::String creationError;
    auto instance = formatManager.createPluginInstance (
        *descriptions.getFirst(), 48000.0, 480, creationError);

    if (instance == nullptr)
    {
        std::cout << "FAIL: VST3 instance creation failed: "
                  << creationError.toStdString() << "\n";
        return 1;
    }

    std::cout << "PASS: VST3 scan and instance creation completed" << std::endl;

    if (! instance->producesMidi())
    {
        std::cout << "FAIL: VST3 did not report PHI bridge event output\n";
        return 1;
    }

    if (! instance->acceptsMidi())
    {
        std::cout << "FAIL: VST3 did not accept PHI presence events\n";
        return 1;
    }

    const auto findParameterByName = [&] (const juce::String& name)
        -> juce::AudioProcessorParameter*
    {
        for (auto* parameter : instance->getParameters())
            if (parameter != nullptr && parameter->getName (128) == name)
                return parameter;
        return nullptr;
    };

    auto* targetAEnabled = findParameterByName ("Sequencer A Target Enabled");
    auto* targetBEnabled = findParameterByName ("Sequencer B Target Enabled");
    if (targetAEnabled == nullptr || targetBEnabled == nullptr)
    {
        std::cout << "FAIL: independent target-enable parameters were not found\n";
        return 1;
    }

    targetAEnabled->setValueNotifyingHost (0.0f);
    targetBEnabled->setValueNotifyingHost (1.0f);
    if (targetAEnabled->getValue() >= 0.5f || targetBEnabled->getValue() < 0.5f)
    {
        std::cout << "FAIL: changing Target A also changed Target B\n";
        return 1;
    }

    targetBEnabled->setValueNotifyingHost (0.0f);
    targetAEnabled->setValueNotifyingHost (1.0f);
    if (targetAEnabled->getValue() < 0.5f || targetBEnabled->getValue() >= 0.5f)
    {
        std::cout << "FAIL: changing Target B also changed Target A\n";
        return 1;
    }

    targetBEnabled->setValueNotifyingHost (1.0f);
    std::cout << "PASS: Parallel target-enable parameters are independent" << std::endl;

    auto* startStep = findParameterByName ("Start Step");
    auto* endStep = findParameterByName ("End Step");
    auto* serialProfile = findParameterByName ("Serial Control Profile");
    auto* bipolarA = findParameterByName ("Sequencer A Bipolar");
    auto* bipolarB = findParameterByName ("Sequencer B Bipolar");
    auto* rate = findParameterByName ("Rate");
    auto* hostSync = findParameterByName ("Host Sync");
    auto* phiBridge = findParameterByName ("PHI Bridge");
    auto* shortGateLength = findParameterByName ("Gate Short Step Length");
    auto* longGateLength = findParameterByName ("Gate Long Step Length");
    auto* sequenceMode = findParameterByName ("Direction");
    auto* noiseGateEnabled = findParameterByName ("Noise Gate Enabled");
    auto* noiseGateThreshold = findParameterByName ("Noise Gate Threshold");
    auto* noiseGateAttack = findParameterByName ("Noise Gate Attack");
    auto* noiseGateHold = findParameterByName ("Noise Gate Hold");
    auto* noiseGateRelease = findParameterByName ("Noise Gate Release");
    auto* noiseGateRange = findParameterByName ("Noise Gate Range");
    auto* phiStartStep = findParameterByName ("PHI Start Step");
    auto* phiEndStep = findParameterByName ("PHI End Step");
    auto* phiSerialProfile = findParameterByName (
        "PHI Serial Control Profile");
    auto* phiBipolarA = findParameterByName ("PHI Sequencer A Bipolar");
    auto* phiBipolarB = findParameterByName ("PHI Sequencer B Bipolar");
    auto* phiRate = findParameterByName ("PHI Rate");
    auto* phiSequenceMode = findParameterByName ("PHI Direction");
    auto* phiAttackA = findParameterByName ("PHI Sequencer A Attack");
    auto* phiReleaseA = findParameterByName ("PHI Sequencer A Release");
    auto* phiSequencerAEnabled = findParameterByName (
        "PHI Sequencer A Enabled");
    auto* phiSequencerBEnabled = findParameterByName (
        "PHI Sequencer B Enabled");
    auto* phiStepA1 = findParameterByName ("PHI Sequencer A Step 1");
    auto* phiStepA2 = findParameterByName ("PHI Sequencer A Step 2");
    auto* delayEnabled = findParameterByName ("Delay Enabled");
    auto* delayTime = findParameterByName ("Delay Time");
    auto* delayFeedback = findParameterByName ("Delay Feedback");
    auto* delayMix = findParameterByName ("Delay Mix");
    auto* delayRate = findParameterByName ("Delay Rate");
    auto* delayStartStep = findParameterByName ("Delay Start Step");
    auto* delayEndStep = findParameterByName ("Delay End Step");
    auto* delaySequenceMode = findParameterByName ("Delay Direction");
    auto* delayStepA1 = findParameterByName ("Delay Sequencer A Step 1");
    auto* delayStepA2 = findParameterByName ("Delay Sequencer A Step 2");
    auto* reverbEnabled = findParameterByName ("Reverb Enabled");
    auto* reverbSize = findParameterByName ("Reverb Size");
    auto* reverbDamping = findParameterByName ("Reverb Damping");
    auto* reverbWidth = findParameterByName ("Reverb Width");
    auto* reverbMix = findParameterByName ("Reverb Mix");
    auto* reverbRate = findParameterByName ("Reverb Rate");
    auto* reverbStartStep = findParameterByName ("Reverb Start Step");
    auto* reverbEndStep = findParameterByName ("Reverb End Step");
    auto* reverbSequenceMode = findParameterByName ("Reverb Direction");
    auto* reverbStepA1 = findParameterByName ("Reverb Sequencer A Step 1");
    auto* reverbStepA2 = findParameterByName ("Reverb Sequencer A Step 2");
    auto* panEnabled = findParameterByName ("Pan Enabled");
    auto* panPosition = findParameterByName ("Pan Position");
    auto* panRate = findParameterByName ("Pan Rate");
    auto* panStartStep = findParameterByName ("Pan Start Step");
    auto* panEndStep = findParameterByName ("Pan End Step");
    auto* panSequenceMode = findParameterByName ("Pan Direction");
    auto* panStepA1 = findParameterByName ("Pan Sequencer A Step 1");
    auto* panStepA2 = findParameterByName ("Pan Sequencer A Step 2");
    auto* filterEnabled = findParameterByName ("Filter Enabled");
    auto* filterType = findParameterByName ("Filter Type");
    auto* filterCutoff = findParameterByName ("Filter Cutoff");
    auto* filterResonance = findParameterByName ("Filter Resonance");
    auto* filterMix = findParameterByName ("Filter Mix");
    auto* filterRate = findParameterByName ("Filter Rate");
    auto* filterStartStep = findParameterByName ("Filter Start Step");
    auto* filterEndStep = findParameterByName ("Filter End Step");
    auto* filterSequenceMode = findParameterByName ("Filter Direction");
    auto* filterStepA1 = findParameterByName ("Filter Sequencer A Step 1");
    auto* filterStepA2 = findParameterByName ("Filter Sequencer A Step 2");
    auto* pitchEnabled = findParameterByName ("Pitch Enabled");
    auto* pitchShift = findParameterByName ("Pitch Shift");
    auto* pitchMix = findParameterByName ("Pitch Mix");
    auto* pitchRate = findParameterByName ("Pitch Rate");
    auto* pitchStartStep = findParameterByName ("Pitch Start Step");
    auto* pitchEndStep = findParameterByName ("Pitch End Step");
    auto* pitchSequenceMode = findParameterByName ("Pitch Direction");
    auto* pitchStepA1 = findParameterByName ("Pitch Sequencer A Step 1");
    auto* pitchStepA2 = findParameterByName ("Pitch Sequencer A Step 2");
    if (startStep == nullptr || endStep == nullptr || serialProfile == nullptr
        || bipolarA == nullptr || bipolarB == nullptr || rate == nullptr
        || hostSync == nullptr || phiBridge == nullptr
        || shortGateLength == nullptr || longGateLength == nullptr
        || sequenceMode == nullptr
        || noiseGateEnabled == nullptr || noiseGateThreshold == nullptr
        || noiseGateAttack == nullptr || noiseGateHold == nullptr
        || noiseGateRelease == nullptr || noiseGateRange == nullptr
        || phiStartStep == nullptr || phiEndStep == nullptr
        || phiSerialProfile == nullptr || phiBipolarA == nullptr
        || phiBipolarB == nullptr || phiRate == nullptr
        || phiSequenceMode == nullptr || phiAttackA == nullptr
        || phiReleaseA == nullptr || phiSequencerAEnabled == nullptr
        || phiSequencerBEnabled == nullptr || phiStepA1 == nullptr
        || phiStepA2 == nullptr || delayEnabled == nullptr
        || delayTime == nullptr || delayFeedback == nullptr
        || delayMix == nullptr || delayRate == nullptr
        || delayStartStep == nullptr || delayEndStep == nullptr
        || delaySequenceMode == nullptr || delayStepA1 == nullptr
        || delayStepA2 == nullptr || reverbEnabled == nullptr
        || reverbSize == nullptr || reverbDamping == nullptr
        || reverbWidth == nullptr || reverbMix == nullptr
        || reverbRate == nullptr || reverbStartStep == nullptr
        || reverbEndStep == nullptr || reverbSequenceMode == nullptr
        || reverbStepA1 == nullptr || reverbStepA2 == nullptr
        || panEnabled == nullptr || panPosition == nullptr
        || panRate == nullptr || panStartStep == nullptr
        || panEndStep == nullptr || panSequenceMode == nullptr
        || panStepA1 == nullptr || panStepA2 == nullptr
        || filterEnabled == nullptr || filterType == nullptr
        || filterCutoff == nullptr || filterResonance == nullptr
        || filterMix == nullptr || filterRate == nullptr
        || filterStartStep == nullptr || filterEndStep == nullptr
        || filterSequenceMode == nullptr || filterStepA1 == nullptr
        || filterStepA2 == nullptr || pitchEnabled == nullptr
        || pitchShift == nullptr || pitchMix == nullptr
        || pitchRate == nullptr || pitchStartStep == nullptr
        || pitchEndStep == nullptr || pitchSequenceMode == nullptr
        || pitchStepA1 == nullptr || pitchStepA2 == nullptr)
    {
        std::cout << "FAIL: a Gate, PHI, Delay, Reverb, Pan or Filter engine parameter was not found\n";
        return 1;
    }
    if (findParameterByName ("Mix") != nullptr)
    {
        std::cout << "FAIL: the redundant Gate Mix parameter is still exposed\n";
        return 1;
    }

    bipolarA->setValueNotifyingHost (1.0f);
    bipolarB->setValueNotifyingHost (0.0f);
    if (bipolarA->getValue() < 0.5f || bipolarB->getValue() >= 0.5f)
    {
        std::cout << "FAIL: Sequencer bipolar parameters are not independent\n";
        return 1;
    }
    bipolarA->setValueNotifyingHost (0.0f);
    if (rate->getNumSteps() != seqwencer::rateChoiceCount)
    {
        std::cout << "FAIL: Rate did not expose all 14 timing choices\n";
        return 1;
    }
    rate->setValueNotifyingHost (5.0f / 13.0f);
    if (rate->getCurrentValueAsText() != "1/16T")
    {
        std::cout << "FAIL: the Rate parameter did not select 1/16T\n";
        return 1;
    }
    rate->setValueNotifyingHost (6.0f / 13.0f);
    if (rate->getCurrentValueAsText() != "1/16")
    {
        std::cout << "FAIL: the Rate parameter did not select straight 1/16\n";
        return 1;
    }
    constexpr auto shortDefaultNormalized = (0.50f - 0.10f) / (0.60f - 0.10f);
    constexpr auto longDefaultNormalized = (0.90f - 0.65f) / (0.95f - 0.65f);
    if (std::abs (shortGateLength->getValue()
                      - shortDefaultNormalized) > 0.001f
        || std::abs (longGateLength->getValue()
                         - longDefaultNormalized) > 0.001f)
    {
        std::cout << "FAIL: adjustable Gate lengths did not restore their defaults\n";
        return 1;
    }
    if (sequenceMode->getNumSteps() != 4
        || sequenceMode->getCurrentValueAsText() != "Loop")
    {
        std::cout << "FAIL: Direction did not expose Loop, Bounce, Reverse and Played\n";
        return 1;
    }
    if (noiseGateEnabled->getValue() >= 0.5f
        || std::abs (noiseGateRange->getValue() - 1.0f) > 0.001f)
    {
        std::cout << "FAIL: the noise gate did not restore its safe defaults\n";
        return 1;
    }
    if (delayEnabled->getValue() >= 0.5f
        || delayRate->getNumSteps() != seqwencer::rateChoiceCount
        || delaySequenceMode->getNumSteps() != 4)
    {
        std::cout << "FAIL: Delay did not restore its safe state or complete timing controls\n";
        return 1;
    }
    if (reverbEnabled->getValue() >= 0.5f
        || reverbRate->getNumSteps() != seqwencer::rateChoiceCount
        || reverbSequenceMode->getNumSteps() != 4)
    {
        std::cout << "FAIL: Reverb did not restore its safe state or complete timing controls\n";
        return 1;
    }
    if (filterEnabled->getValue() >= 0.5f
        || filterType->getNumSteps() != 5
        || filterType->getCurrentValueAsText() != "Low Pass"
        || filterRate->getNumSteps() != seqwencer::rateChoiceCount
        || filterSequenceMode->getNumSteps() != 4)
    {
        std::cout << "FAIL: Filter did not restore its safe state or complete controls\n";
        return 1;
    }
    int filterTypeIndex = 0;
    for (const auto* expectedName : {
             "Low Pass", "High Pass", "Band Pass", "Band Reject", "Peaking" })
    {
        filterType->setValueNotifyingHost (
            static_cast<float> (filterTypeIndex++) / 4.0f);
        if (filterType->getCurrentValueAsText() != expectedName)
        {
            std::cout << "FAIL: Filter Type did not expose the expected response: "
                      << expectedName << "\n";
            return 1;
        }
    }
    filterType->setValueNotifyingHost (0.0f);
    if (pitchEnabled->getValue() >= 0.5f
        || pitchRate->getNumSteps() != seqwencer::rateChoiceCount
        || pitchSequenceMode->getNumSteps() != 4
        || std::abs (pitchShift->getValue() - 0.5f) > 0.001f
        || std::abs (pitchMix->getValue() - 1.0f) > 0.001f)
    {
        std::cout << "FAIL: Pitch did not restore its safe state or complete controls\n";
        return 1;
    }
    std::cout << "PASS: Gate, PHI, Delay, Reverb, Filter and Pitch engine controls are exposed" << std::endl;

    rate->setValueNotifyingHost (5.0f / 13.0f);
    phiRate->setValueNotifyingHost (9.0f / 13.0f);
    delayRate->setValueNotifyingHost (11.0f / 13.0f);
    reverbRate->setValueNotifyingHost (3.0f / 13.0f);
    filterRate->setValueNotifyingHost (8.0f / 13.0f);
    pitchRate->setValueNotifyingHost (1.0f / 13.0f);
    phiStepA1->setValueNotifyingHost (0.80f);
    delayStepA1->setValueNotifyingHost (0.65f);
    reverbStepA1->setValueNotifyingHost (0.35f);
    filterStepA1->setValueNotifyingHost (0.90f);
    pitchStepA1->setValueNotifyingHost (0.55f);
    auto* gateStepA1ForIndependence = findParameterByName (
        "Sequencer A Step 1");
    if (gateStepA1ForIndependence == nullptr)
    {
        std::cout << "FAIL: Gate Sequencer A Step 1 was not found\n";
        return 1;
    }
    gateStepA1ForIndependence->setValueNotifyingHost (0.20f);
    if (rate->getCurrentValueAsText() != "1/16T"
        || phiRate->getCurrentValueAsText() != "1/4T"
        || delayRate->getCurrentValueAsText() != "1/2T"
        || reverbRate->getCurrentValueAsText() != "1/32T"
        || filterRate->getCurrentValueAsText() != "1/8"
        || pitchRate->getCurrentValueAsText() != "1/64T"
        || std::abs (gateStepA1ForIndependence->getValue() - 0.20f) > 0.001f
        || std::abs (phiStepA1->getValue() - 0.80f) > 0.001f
        || std::abs (delayStepA1->getValue() - 0.65f) > 0.001f
        || std::abs (reverbStepA1->getValue() - 0.35f) > 0.001f
        || std::abs (filterStepA1->getValue() - 0.90f) > 0.001f
        || std::abs (pitchStepA1->getValue() - 0.55f) > 0.001f)
    {
        std::cout << "FAIL: Gate, PHI, Delay, Reverb, Filter or Pitch timing or step data were coupled\n";
        return 1;
    }
    rate->setValueNotifyingHost (6.0f / 13.0f);
    phiRate->setValueNotifyingHost (6.0f / 13.0f);
    delayRate->setValueNotifyingHost (6.0f / 13.0f);
    reverbRate->setValueNotifyingHost (6.0f / 13.0f);
    filterRate->setValueNotifyingHost (6.0f / 13.0f);
    pitchRate->setValueNotifyingHost (6.0f / 13.0f);
    gateStepA1ForIndependence->setValueNotifyingHost (1.0f);
    phiStepA1->setValueNotifyingHost (1.0f);
    delayStepA1->setValueNotifyingHost (1.0f);
    reverbStepA1->setValueNotifyingHost (1.0f);
    filterStepA1->setValueNotifyingHost (1.0f);
    pitchStepA1->setValueNotifyingHost (1.0f);
    std::cout << "PASS: Gate, PHI, Delay, Reverb, Filter and Pitch sequencer engines are independent" << std::endl;

    for (const auto* name : {
             "Sequencer A DEPTH Target", "Sequencer A SHORT STEP Target",
             "Sequencer A LONG STEP Target", "Sequencer A THRESHOLD Target",
             "Sequencer A NOISE ATTACK Target", "Sequencer A HOLD Target",
             "Sequencer A NOISE RELEASE Target", "Sequencer A RANGE Target",
             "Sequencer B DEPTH Target", "Sequencer B SHORT STEP Target",
             "Sequencer B LONG STEP Target", "Sequencer B THRESHOLD Target",
             "Sequencer B NOISE ATTACK Target", "Sequencer B HOLD Target",
             "Sequencer B NOISE RELEASE Target", "Sequencer B RANGE Target" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Gate target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    std::cout << "PASS: Gate and Noise Gate controls expose independent A/B targets" << std::endl;

    for (const auto* name : {
             "Delay Sequencer A TIME Target",
             "Delay Sequencer A FEEDBACK Target",
             "Delay Sequencer A MIX Target",
             "Delay Sequencer B TIME Target",
             "Delay Sequencer B FEEDBACK Target",
             "Delay Sequencer B MIX Target" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Delay target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    std::cout << "PASS: Time, Feedback and Mix expose independent Delay targets" << std::endl;

    for (const auto* name : {
             "Reverb Sequencer A SIZE Target",
             "Reverb Sequencer A SIZE Target Enabled",
             "Reverb Sequencer A DAMPING Target",
             "Reverb Sequencer A DAMPING Target Enabled",
             "Reverb Sequencer A WIDTH Target",
             "Reverb Sequencer A WIDTH Target Enabled",
             "Reverb Sequencer A MIX Target",
             "Reverb Sequencer A MIX Target Enabled",
             "Reverb Sequencer B SIZE Target",
             "Reverb Sequencer B SIZE Target Enabled",
             "Reverb Sequencer B DAMPING Target",
             "Reverb Sequencer B DAMPING Target Enabled",
             "Reverb Sequencer B WIDTH Target",
             "Reverb Sequencer B WIDTH Target Enabled",
             "Reverb Sequencer B MIX Target",
             "Reverb Sequencer B MIX Target Enabled" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Reverb target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    auto* reverbSizeTargetA = findParameterByName (
        "Reverb Sequencer A SIZE Target");
    auto* reverbSizeTargetB = findParameterByName (
        "Reverb Sequencer B SIZE Target");
    reverbSizeTargetA->setValueNotifyingHost (1.0f);
    reverbSizeTargetB->setValueNotifyingHost (0.0f);
    if (reverbSizeTargetA->getValue() < 0.5f
        || reverbSizeTargetB->getValue() >= 0.5f)
    {
        std::cout << "FAIL: assigning Reverb Size to A also changed B\n";
        return 1;
    }
    reverbSizeTargetA->setValueNotifyingHost (0.0f);
    std::cout << "PASS: Size, Damping, Width and Mix expose independent Reverb targets" << std::endl;

    for (const auto* name : {
             "Pan Sequencer A PAN Target",
             "Pan Sequencer A PAN Target Enabled",
             "Pan Sequencer B PAN Target",
             "Pan Sequencer B PAN Target Enabled" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Pan target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    std::cout << "PASS: Pan exposes independent A/B targets" << std::endl;

    for (const auto* name : {
             "Filter Sequencer A CUTOFF Target",
             "Filter Sequencer A CUTOFF Target Enabled",
             "Filter Sequencer A RESONANCE Target",
             "Filter Sequencer A RESONANCE Target Enabled",
             "Filter Sequencer A MIX Target",
             "Filter Sequencer A MIX Target Enabled",
             "Filter Sequencer B CUTOFF Target",
             "Filter Sequencer B CUTOFF Target Enabled",
             "Filter Sequencer B RESONANCE Target",
             "Filter Sequencer B RESONANCE Target Enabled",
             "Filter Sequencer B MIX Target",
             "Filter Sequencer B MIX Target Enabled" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Filter target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    std::cout << "PASS: Cutoff, Resonance and Mix expose independent Filter targets" << std::endl;

    for (const auto* name : {
             "Pitch Sequencer A SHIFT Target",
             "Pitch Sequencer A SHIFT Target Enabled",
             "Pitch Sequencer A MIX Target",
             "Pitch Sequencer A MIX Target Enabled",
             "Pitch Sequencer B SHIFT Target",
             "Pitch Sequencer B SHIFT Target Enabled",
             "Pitch Sequencer B MIX Target",
             "Pitch Sequencer B MIX Target Enabled" })
    {
        if (findParameterByName (name) == nullptr)
        {
            std::cout << "FAIL: assignable Pitch target parameter was not found: "
                      << name << "\n";
            return 1;
        }
    }
    std::cout << "PASS: Shift and Mix expose independent Pitch targets" << std::endl;

    auto* gateEnabled = findParameterByName ("Gate Enabled");
    auto* gateVolume = findParameterByName ("Gate Volume");
    auto* gateDepth = findParameterByName ("Gate Depth");
    auto* sequencerAEnabled = findParameterByName ("Sequencer A Enabled");
    auto* sequencerBEnabled = findParameterByName ("Sequencer B Enabled");
    auto* targetA = findParameterByName ("Sequencer A Target");
    auto* attackA = findParameterByName ("Sequencer A Attack");
    auto* releaseA = findParameterByName ("Sequencer A Release");
    auto* stepA1 = findParameterByName ("Sequencer A Step 1");
    auto* gateModeA1 = findParameterByName ("Sequencer A Gate Mode 1");
    if (gateEnabled == nullptr || gateVolume == nullptr || gateDepth == nullptr
        || sequencerAEnabled == nullptr || sequencerBEnabled == nullptr
        || targetA == nullptr || attackA == nullptr || releaseA == nullptr
        || stepA1 == nullptr || gateModeA1 == nullptr)
    {
        std::cout << "FAIL: parameters required for the Gate mode audio tests were not found\n";
        return 1;
    }

    if (gateModeA1->getNumSteps() != 4
        || gateModeA1->getCurrentValueAsText() != "Long")
    {
        std::cout << "FAIL: Gate mode did not expose Off, Short, Long and Link with Long as default\n";
        return 1;
    }
    std::cout << "PASS: four-state Gate modes are exposed with Long as default" << std::endl;

    gateEnabled->setValueNotifyingHost (1.0f);
    gateVolume->setValueNotifyingHost (1.0f);
    gateDepth->setValueNotifyingHost (1.0f);
    sequencerAEnabled->setValueNotifyingHost (1.0f);
    sequencerBEnabled->setValueNotifyingHost (0.0f);
    targetA->setValueNotifyingHost (1.0f);
    targetAEnabled->setValueNotifyingHost (1.0f);
    attackA->setValueNotifyingHost (1.0f);
    releaseA->setValueNotifyingHost (1.0f);
    rate->setValueNotifyingHost (0.0f);

    for (int step = 1; step <= seqwencer::stepsPerBank; ++step)
    {
        auto* stepParameter = findParameterByName (
            "Sequencer A Step " + juce::String (step));
        auto* gateModeParameter = findParameterByName (
            "Sequencer A Gate Mode " + juce::String (step));
        if (stepParameter == nullptr || gateModeParameter == nullptr)
        {
            std::cout << "FAIL: a Sequencer A step or Gate mode parameter was not found\n";
            return 1;
        }
        stepParameter->setValueNotifyingHost (1.0f);
        gateModeParameter->setValueNotifyingHost (1.0f);
    }

    constexpr int testSampleCount = 4096;
    juce::AudioBuffer<float> heldStepAudio (2, testSampleCount);
    for (int channel = 0; channel < heldStepAudio.getNumChannels(); ++channel)
        std::fill_n (heldStepAudio.getWritePointer (channel), testSampleCount, 1.0f);

    juce::MidiBuffer midi;
    instance->prepareToPlay (48000.0, testSampleCount);
    instance->processBlock (heldStepAudio, midi);
    instance->releaseResources();

    auto maximumError = 0.0f;
    for (int channel = 0; channel < heldStepAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < testSampleCount; ++sample)
            maximumError = std::max (
                maximumError,
                std::abs (heldStepAudio.getSample (channel, sample) - 1.0f));

    if (maximumError > 0.00001f)
    {
        std::cout << "FAIL: equal full-volume steps altered the audio signal\n";
        return 1;
    }
    std::cout << "PASS: equal full-volume steps pass audio without boundary dips" << std::endl;

    targetA->setValueNotifyingHost (0.0f);
    gateModeA1->setValueNotifyingHost (0.0f);
    attackA->setValueNotifyingHost (0.0f);
    releaseA->setValueNotifyingHost (0.0f);
    constexpr int offTestSampleCount = 512;
    juce::AudioBuffer<float> offStepAudio (2, offTestSampleCount);
    for (int channel = 0; channel < offStepAudio.getNumChannels(); ++channel)
        std::fill_n (offStepAudio.getWritePointer (channel),
                     offTestSampleCount, 1.0f);

    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (offStepAudio, midi);
    instance->releaseResources();
    if (std::abs (offStepAudio.getSample (0, offTestSampleCount - 1)) > 0.001f)
    {
        std::cout << "FAIL: an Off Gate step required a Volume target to close the audio\n";
        return 1;
    }
    targetA->setValueNotifyingHost (1.0f);

    gateEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> bypassedGateAudio (2, offTestSampleCount);
    for (int channel = 0; channel < bypassedGateAudio.getNumChannels(); ++channel)
        std::fill_n (bypassedGateAudio.getWritePointer (channel),
                     offTestSampleCount, 1.0f);

    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (bypassedGateAudio, midi);
    instance->releaseResources();
    auto bypassError = 0.0f;
    for (int channel = 0; channel < bypassedGateAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
            bypassError = std::max (
                bypassError,
                std::abs (bypassedGateAudio.getSample (channel, sample) - 1.0f));
    if (bypassError > 0.00001f)
    {
        std::cout << "FAIL: disabled Gate modes still altered the audio\n";
        return 1;
    }
    std::cout << "PASS: Gate timing works without a Volume target and Gate disable restores passthrough" << std::endl;

    gateEnabled->setValueNotifyingHost (1.0f);
    gateModeA1->setValueNotifyingHost (1.0f);
    noiseGateEnabled->setValueNotifyingHost (1.0f);
    noiseGateThreshold->setValueNotifyingHost (0.925f);
    noiseGateHold->setValueNotifyingHost (0.0f);
    noiseGateRelease->setValueNotifyingHost (0.0f);
    noiseGateRange->setValueNotifyingHost (1.0f);
    constexpr int noiseGateTestSampleCount = 4096;
    juce::AudioBuffer<float> loudStepAudio (2, noiseGateTestSampleCount);
    for (int channel = 0; channel < loudStepAudio.getNumChannels(); ++channel)
        std::fill_n (loudStepAudio.getWritePointer (channel),
                     noiseGateTestSampleCount, 1.0f);

    instance->prepareToPlay (48000.0, noiseGateTestSampleCount);
    instance->processBlock (loudStepAudio, midi);
    instance->releaseResources();
    if (std::abs (loudStepAudio.getSample (
            0, noiseGateTestSampleCount - 1) - 1.0f) > 0.001f)
    {
        std::cout << "FAIL: the noise gate did not pass a loud sequenced step\n";
        return 1;
    }

    for (int step = 1; step <= seqwencer::stepsPerBank; ++step)
    {
        auto* stepParameter = findParameterByName (
            "Sequencer A Step " + juce::String (step));
        if (stepParameter == nullptr)
        {
            std::cout << "FAIL: a step required for the post-Gate detector test was not found\n";
            return 1;
        }
        stepParameter->setValueNotifyingHost (0.625f);
    }

    juce::AudioBuffer<float> quietStepAudio (2, noiseGateTestSampleCount);
    for (int channel = 0; channel < quietStepAudio.getNumChannels(); ++channel)
        std::fill_n (quietStepAudio.getWritePointer (channel),
                     noiseGateTestSampleCount, 1.0f);
    instance->prepareToPlay (48000.0, noiseGateTestSampleCount);
    instance->processBlock (quietStepAudio, midi);
    instance->releaseResources();
    if (std::abs (quietStepAudio.getSample (
            0, noiseGateTestSampleCount - 1)) > 0.001f)
    {
        std::cout << "FAIL: the noise gate ignored the quieter sequenced Volume level\n";
        return 1;
    }

    noiseGateEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> disabledNoiseGateAudio (2, offTestSampleCount);
    for (int channel = 0; channel < disabledNoiseGateAudio.getNumChannels(); ++channel)
        std::fill_n (disabledNoiseGateAudio.getWritePointer (channel),
                     offTestSampleCount, 1.0f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (disabledNoiseGateAudio, midi);
    instance->releaseResources();
    if (std::abs (disabledNoiseGateAudio.getSample (
            0, offTestSampleCount - 1) - 0.25f) > 0.001f)
    {
        std::cout << "FAIL: disabling the noise gate did not restore the normal Gate level\n";
        return 1;
    }
    for (int step = 1; step <= seqwencer::stepsPerBank; ++step)
        if (auto* stepParameter = findParameterByName (
                "Sequencer A Step " + juce::String (step)))
            stepParameter->setValueNotifyingHost (1.0f);
    std::cout << "PASS: noise gate responds to post-sequencer Volume and disables cleanly" << std::endl;

    gateEnabled->setValueNotifyingHost (0.0f);
    noiseGateEnabled->setValueNotifyingHost (0.0f);
    delayEnabled->setValueNotifyingHost (1.0f);
    delayTime->setValueNotifyingHost (0.0f);
    delayFeedback->setValueNotifyingHost (0.0f);
    delayMix->setValueNotifyingHost (1.0f);
    constexpr int delayTestSampleCount = 2048;
    juce::AudioBuffer<float> delayAudio (2, delayTestSampleCount);
    delayAudio.clear();
    delayAudio.setSample (0, 0, 1.0f);
    delayAudio.setSample (1, 0, 1.0f);
    instance->prepareToPlay (48000.0, delayTestSampleCount);
    instance->processBlock (delayAudio, midi);
    instance->releaseResources();

    auto delayedPeak = 0.0f;
    for (int sample = 476; sample <= 484; ++sample)
        delayedPeak = std::max (delayedPeak,
                                std::abs (delayAudio.getSample (0, sample)));
    if (std::abs (delayAudio.getSample (0, 0)) > 0.001f
        || delayedPeak < 0.80f)
    {
        std::cout << "FAIL: the tape Delay did not move a fully wet impulse to 10 ms\n";
        return 1;
    }
    delayEnabled->setValueNotifyingHost (0.0f);
    std::cout << "PASS: tape Delay produces a smooth interpolated delayed signal" << std::endl;

    reverbEnabled->setValueNotifyingHost (1.0f);
    reverbSize->setValueNotifyingHost (0.75f);
    reverbDamping->setValueNotifyingHost (0.20f);
    reverbWidth->setValueNotifyingHost (1.0f);
    reverbMix->setValueNotifyingHost (1.0f);
    constexpr int reverbTestSampleCount = 16384;
    juce::AudioBuffer<float> reverbAudio (2, reverbTestSampleCount);
    reverbAudio.clear();
    reverbAudio.setSample (0, 0, 1.0f);
    reverbAudio.setSample (1, 0, 1.0f);
    instance->prepareToPlay (48000.0, reverbTestSampleCount);
    instance->processBlock (reverbAudio, midi);
    instance->releaseResources();

    auto reverbTailPeak = 0.0f;
    for (int channel = 0; channel < reverbAudio.getNumChannels(); ++channel)
        for (int sample = 1024; sample < reverbTestSampleCount; ++sample)
            reverbTailPeak = std::max (
                reverbTailPeak, std::abs (reverbAudio.getSample (channel, sample)));
    // JUCE deliberately slews its internal wet/dry gains for 10 ms after a
    // parameter change.  The first sample may therefore retain some of the
    // previous dry gain even though Seqwencer's Mix is fully wet.  That is not
    // a Reverb failure; the useful regression here is that a real delayed tail
    // appears after the impulse.
    if (reverbTailPeak < 0.0001f)
    {
        std::cout << "FAIL: the fully wet Reverb did not produce an audio tail\n";
        return 1;
    }

    reverbEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> bypassedReverbAudio (2, offTestSampleCount);
    for (int channel = 0; channel < bypassedReverbAudio.getNumChannels(); ++channel)
        std::fill_n (bypassedReverbAudio.getWritePointer (channel),
                     offTestSampleCount, 0.5f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (bypassedReverbAudio, midi);
    instance->releaseResources();
    auto reverbBypassError = 0.0f;
    for (int channel = 0; channel < bypassedReverbAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
            reverbBypassError = std::max (
                reverbBypassError,
                std::abs (bypassedReverbAudio.getSample (channel, sample) - 0.5f));
    if (reverbBypassError > 0.00001f)
    {
        std::cout << "FAIL: disabled Reverb still altered the audio\n";
        return 1;
    }
    std::cout << "PASS: Reverb produces a wet tail and disables cleanly" << std::endl;

    panPosition->setValueNotifyingHost (1.0f);
    panEnabled->setValueNotifyingHost (1.0f);
    juce::AudioBuffer<float> panAudio (2, offTestSampleCount);
    for (int channel = 0; channel < panAudio.getNumChannels(); ++channel)
        std::fill_n (panAudio.getWritePointer (channel),
                     offTestSampleCount, 0.5f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (panAudio, midi);
    instance->releaseResources();
    if (std::abs (panAudio.getSample (0, 0)) > 0.0001f
        || std::abs (panAudio.getSample (1, 0) - 0.5f) > 0.0001f)
    {
        std::cout << "FAIL: hard-right Pan did not attenuate only the left channel\n";
        return 1;
    }

    panEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> bypassedPanAudio (2, offTestSampleCount);
    for (int channel = 0; channel < bypassedPanAudio.getNumChannels(); ++channel)
        std::fill_n (bypassedPanAudio.getWritePointer (channel),
                     offTestSampleCount, 0.5f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (bypassedPanAudio, midi);
    instance->releaseResources();
    if (std::abs (bypassedPanAudio.getSample (0, 0) - 0.5f) > 0.0001f
        || std::abs (bypassedPanAudio.getSample (1, 0) - 0.5f) > 0.0001f)
    {
        std::cout << "FAIL: disabled Pan still altered the audio\n";
        return 1;
    }
    std::cout << "PASS: Pan balances stereo audio and disables cleanly" << std::endl;

    filterType->setValueNotifyingHost (0.0f);
    filterCutoff->setValueNotifyingHost (0.0f);
    filterResonance->setValueNotifyingHost (0.0f);
    filterMix->setValueNotifyingHost (1.0f);
    filterEnabled->setValueNotifyingHost (1.0f);
    constexpr int filterTestSampleCount = 4096;
    juce::AudioBuffer<float> filterAudio (2, filterTestSampleCount);
    for (int channel = 0; channel < filterAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < filterTestSampleCount; ++sample)
            filterAudio.setSample (
                channel, sample, (sample & 1) == 0 ? 0.5f : -0.5f);
    instance->prepareToPlay (48000.0, filterTestSampleCount);
    instance->processBlock (filterAudio, midi);
    instance->releaseResources();

    auto filteredPeak = 0.0f;
    for (int channel = 0; channel < filterAudio.getNumChannels(); ++channel)
        for (int sample = filterTestSampleCount / 2;
             sample < filterTestSampleCount; ++sample)
            filteredPeak = std::max (
                filteredPeak,
                std::abs (filterAudio.getSample (channel, sample)));
    if (filteredPeak > 0.05f)
    {
        std::cout << "FAIL: low Filter Cutoff did not attenuate high-frequency audio\n";
        return 1;
    }

    const auto measureFilterRms = [&] (float typeValue, bool useDcInput)
    {
        filterType->setValueNotifyingHost (typeValue);
        filterCutoff->setValueNotifyingHost (1.0f);
        filterResonance->setValueNotifyingHost (
            filterResonance->getDefaultValue());
        juce::AudioBuffer<float> typeAudio (2, filterTestSampleCount);
        for (int channel = 0; channel < typeAudio.getNumChannels(); ++channel)
            for (int sample = 0; sample < filterTestSampleCount; ++sample)
                typeAudio.setSample (
                    channel, sample,
                    useDcInput ? 0.5f : static_cast<float> (
                        0.5 * std::sin (
                            juce::MathConstants<double>::twoPi * 20000.0
                            * static_cast<double> (sample) / 48000.0)));
        instance->prepareToPlay (48000.0, filterTestSampleCount);
        instance->processBlock (typeAudio, midi);
        instance->releaseResources();
        auto squaredTotal = 0.0;
        auto measuredSamples = 0;
        for (int channel = 0; channel < typeAudio.getNumChannels(); ++channel)
            for (int sample = filterTestSampleCount / 2;
                 sample < filterTestSampleCount; ++sample)
            {
                const auto value = static_cast<double> (
                    typeAudio.getSample (channel, sample));
                squaredTotal += value * value;
                ++measuredSamples;
            }
        return static_cast<float> (std::sqrt (
            squaredTotal / static_cast<double> (measuredSamples)));
    };

    const auto highPassDcRms = measureFilterRms (0.25f, true);
    const auto bandPassRms = measureFilterRms (0.50f, false);
    const auto bandRejectRms = measureFilterRms (0.75f, false);
    const auto peakingRms = measureFilterRms (1.0f, false);
    if (highPassDcRms > 0.01f || bandPassRms < 0.10f
        || bandRejectRms > 0.05f || peakingRms < 0.30f)
    {
        std::cout << "FAIL: one or more Filter Type responses were incorrect\n";
        return 1;
    }

    filterEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> bypassedFilterAudio (2, offTestSampleCount);
    for (int channel = 0; channel < bypassedFilterAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
            bypassedFilterAudio.setSample (
                channel, sample, (sample & 1) == 0 ? 0.5f : -0.5f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (bypassedFilterAudio, midi);
    instance->releaseResources();

    auto filterBypassError = 0.0f;
    for (int channel = 0; channel < bypassedFilterAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
        {
            const auto expected = (sample & 1) == 0 ? 0.5f : -0.5f;
            filterBypassError = std::max (
                filterBypassError,
                std::abs (bypassedFilterAudio.getSample (channel, sample)
                          - expected));
        }
    if (filterBypassError > 0.00001f)
    {
        std::cout << "FAIL: disabled Filter still altered the audio\n";
        return 1;
    }
    std::cout << "PASS: all five Filter Types process correctly and disable cleanly" << std::endl;

    gateEnabled->setValueNotifyingHost (0.0f);
    delayEnabled->setValueNotifyingHost (0.0f);
    reverbEnabled->setValueNotifyingHost (0.0f);
    panEnabled->setValueNotifyingHost (0.0f);
    filterEnabled->setValueNotifyingHost (0.0f);
    pitchMix->setValueNotifyingHost (1.0f);
    pitchShift->setValueNotifyingHost (0.75f); // +12 semitones.
    pitchEnabled->setValueNotifyingHost (1.0f);
    constexpr int pitchTestSampleCount = 48000;
    juce::AudioBuffer<float> pitchAudio (2, pitchTestSampleCount);
    for (int channel = 0; channel < pitchAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < pitchTestSampleCount; ++sample)
            pitchAudio.setSample (
                channel, sample, static_cast<float> (
                    0.25 * std::sin (
                        juce::MathConstants<double>::twoPi * 440.0
                        * static_cast<double> (sample) / 48000.0)));
    instance->prepareToPlay (48000.0, 480);
    instance->processBlock (pitchAudio, midi);
    instance->releaseResources();

    auto upwardZeroCrossings = 0;
    auto previousPitchSample = pitchAudio.getSample (
        0, pitchTestSampleCount / 2);
    for (int sample = pitchTestSampleCount / 2 + 1;
         sample < pitchTestSampleCount; ++sample)
    {
        const auto currentPitchSample = pitchAudio.getSample (0, sample);
        if (previousPitchSample <= 0.0f && currentPitchSample > 0.0f)
            ++upwardZeroCrossings;
        previousPitchSample = currentPitchSample;
    }
    if (upwardZeroCrossings < 360 || upwardZeroCrossings > 520)
    {
        std::cout << "FAIL: +12 semitone Pitch did not approximately double frequency ("
                  << upwardZeroCrossings << " positive crossings)\n";
        return 1;
    }

    pitchEnabled->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> bypassedPitchAudio (2, offTestSampleCount);
    for (int channel = 0; channel < bypassedPitchAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
            bypassedPitchAudio.setSample (
                channel, sample, (sample & 1) == 0 ? 0.25f : -0.25f);
    instance->prepareToPlay (48000.0, offTestSampleCount);
    instance->processBlock (bypassedPitchAudio, midi);
    instance->releaseResources();
    auto pitchBypassError = 0.0f;
    for (int channel = 0; channel < bypassedPitchAudio.getNumChannels(); ++channel)
        for (int sample = 0; sample < offTestSampleCount; ++sample)
        {
            const auto expected = (sample & 1) == 0 ? 0.25f : -0.25f;
            pitchBypassError = std::max (
                pitchBypassError,
                std::abs (bypassedPitchAudio.getSample (channel, sample)
                          - expected));
        }
    if (pitchBypassError > 0.00001f)
    {
        std::cout << "FAIL: disabled Pitch still altered the audio\n";
        return 1;
    }
    std::cout << "PASS: Pitch shifts by an octave and disables cleanly" << std::endl;

    auto* stepA2 = findParameterByName ("Sequencer A Step 2");
    auto* gateModeA2 = findParameterByName ("Sequencer A Gate Mode 2");
    if (stepA2 == nullptr || gateModeA2 == nullptr)
    {
        std::cout << "FAIL: parameters required for the nested HOST SYNC test were not found\n";
        return 1;
    }

    hostSync->setValueNotifyingHost (0.0f);
    phiBridge->setValueNotifyingHost (1.0f);
    phiSequencerAEnabled->setValueNotifyingHost (1.0f);
    phiSequencerBEnabled->setValueNotifyingHost (0.0f);
    phiSequenceMode->setValueNotifyingHost (2.0f / 3.0f);
    phiStartStep->setValueNotifyingHost (0.0f);
    phiEndStep->setValueNotifyingHost (0.0f);
    phiRate->setValueNotifyingHost (0.0f);
    phiStepA1->setValueNotifyingHost (1.0f);
    phiStepA2->setValueNotifyingHost (0.5f);
    juce::AudioBuffer<float> reverseAudio (2, 480);
    reverseAudio.clear();
    juce::MidiBuffer reverseMidi;
    instance->prepareToPlay (48000.0, 480);
    instance->processBlock (reverseAudio, reverseMidi);
    instance->releaseResources();

    auto reverseStartedAtEnd = false;
    for (const auto metadata : reverseMidi)
    {
        const auto& message = metadata.getMessage();
        int sourceLane = -1;
        bool bipolar = false;
        bool active = false;
        float normalizedValue = 1.0f;
        if (message.isSysEx()
            && seqwencer_bridge::decodeLaneValue (
                reinterpret_cast<const std::uint8_t*> (
                    message.getSysExData()),
                static_cast<std::size_t> (message.getSysExDataSize()),
                sourceLane, bipolar, active, normalizedValue)
            && sourceLane == seqwencer_bridge::sequencerALane
            && active && ! bipolar && normalizedValue < 0.05f)
        {
            reverseStartedAtEnd = true;
        }
    }
    if (! reverseStartedAtEnd)
    {
        std::cout << "FAIL: Reverse did not begin at the selected End step\n";
        return 1;
    }
    phiSequenceMode->setValueNotifyingHost (0.0f);
    std::cout << "PASS: Reverse begins at the selected End step" << std::endl;

    phiSequenceMode->setValueNotifyingHost (1.0f);
    hostSync->setValueNotifyingHost (0.0f);
    phiAttackA->setValueNotifyingHost (0.0f);
    phiReleaseA->setValueNotifyingHost (0.0f);
    juce::AudioBuffer<float> playedAudio (2, 480);
    playedAudio.clear();
    instance->prepareToPlay (48000.0, 480);
    for (int block = 0; block < 2; ++block)
    {
        juce::MidiBuffer advanceMidi;
        instance->processBlock (playedAudio, advanceMidi);
    }
    juce::MidiBuffer playedMidi;
    playedMidi.addEvent (
        juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (100)),
        0);
    instance->processBlock (playedAudio, playedMidi);
    instance->releaseResources();

    auto playedRestartedAtStart = false;
    for (const auto metadata : playedMidi)
    {
        const auto& message = metadata.getMessage();
        int sourceLane = -1;
        bool bipolar = false;
        bool active = false;
        float normalizedValue = 0.0f;
        if (message.isSysEx()
            && seqwencer_bridge::decodeLaneValue (
                reinterpret_cast<const std::uint8_t*> (message.getSysExData()),
                static_cast<std::size_t> (message.getSysExDataSize()),
                sourceLane, bipolar, active, normalizedValue)
            && sourceLane == seqwencer_bridge::sequencerALane
            && active && normalizedValue > 0.95f)
        {
            playedRestartedAtStart = true;
        }
    }
    if (! playedRestartedAtStart)
    {
        std::cout << "FAIL: Played did not restart from Start on the first note\n";
        return 1;
    }
    phiSequenceMode->setValueNotifyingHost (0.0f);
    std::cout << "PASS: Played restarts from Start on the first note of a phrase" << std::endl;

    hostSync->setValueNotifyingHost (1.0f);
    phiBridge->setValueNotifyingHost (1.0f);
    gateEnabled->setValueNotifyingHost (1.0f);
    phiStepA1->setValueNotifyingHost (1.0f);
    phiStepA2->setValueNotifyingHost (0.5f);
    gateModeA1->setValueNotifyingHost (1.0f);
    gateModeA2->setValueNotifyingHost (1.0f);
    phiStartStep->setValueNotifyingHost (0.0f);
    phiEndStep->setValueNotifyingHost (0.0f);
    phiRate->setValueNotifyingHost (0.0f);

    constexpr int nestedHostBlockSize = 480;
    StalledNestedHostPlayHead stalledNestedHost;
    instance->setPlayHead (&stalledNestedHost);
    instance->prepareToPlay (48000.0, nestedHostBlockSize);
    auto nestedHostAdvanced = false;
    auto nestedHostLanePacketCount = 0;
    auto lowestNestedHostValue = 1.0f;
    for (int block = 0; block < 4; ++block)
    {
        juce::AudioBuffer<float> nestedHostAudio (2, nestedHostBlockSize);
        for (int channel = 0; channel < nestedHostAudio.getNumChannels(); ++channel)
            std::fill_n (nestedHostAudio.getWritePointer (channel),
                         nestedHostBlockSize, 1.0f);
        juce::MidiBuffer nestedHostMidi;
        if (block == 0)
        {
            const auto presencePacket = seqwencer_bridge::encodePhiPresence();
            nestedHostMidi.addEvent (
                juce::MidiMessage::createSysExMessage (
                    presencePacket.data(),
                    static_cast<int> (presencePacket.size())),
                0);
        }
        instance->processBlock (nestedHostAudio, nestedHostMidi);

        for (const auto metadata : nestedHostMidi)
        {
            const auto& message = metadata.getMessage();
            int sourceLane = -1;
            bool bipolar = false;
            bool active = false;
            float normalizedValue = 1.0f;
            if (message.isSysEx()
                && seqwencer_bridge::decodeLaneValue (
                    reinterpret_cast<const std::uint8_t*> (
                        message.getSysExData()),
                    static_cast<std::size_t> (message.getSysExDataSize()),
                    sourceLane, bipolar, active, normalizedValue)
                && sourceLane == seqwencer_bridge::sequencerALane)
            {
                ++nestedHostLanePacketCount;
                lowestNestedHostValue = std::min (
                    lowestNestedHostValue, normalizedValue);
                if (active && ! bipolar && normalizedValue < 0.05f)
                    nestedHostAdvanced = true;
            }
        }
    }
    instance->releaseResources();
    instance->setPlayHead (nullptr);

    if (! nestedHostAdvanced)
    {
        std::cout << "FAIL: HOST SYNC froze on PHI's stalled nested timeline"
                  << " (A packets=" << nestedHostLanePacketCount
                  << ", lowest value=" << lowestNestedHostValue << ")\n";
        return 1;
    }
    std::cout << "PASS: HOST SYNC free-runs when PHI's nested timeline stalls" << std::endl;

    if (phiBridge == nullptr || phiStepA1 == nullptr)
    {
        std::cout << "FAIL: PHI bridge parameters were not found\n";
        return 1;
    }

    phiBridge->setValueNotifyingHost (1.0f);
    gateEnabled->setValueNotifyingHost (0.0f);
    phiRate->setValueNotifyingHost (1.0f);
    phiStartStep->setValueNotifyingHost (0.0f);
    phiEndStep->setValueNotifyingHost (0.0f);
    phiAttackA->setValueNotifyingHost (0.0f);
    phiReleaseA->setValueNotifyingHost (0.0f);
    phiStepA1->setValueNotifyingHost (0.875f);

    juce::AudioBuffer<float> bridgeAudio (2, 512);
    bridgeAudio.clear();
    juce::MidiBuffer bridgeMidi;
    const auto presencePacket = seqwencer_bridge::encodePhiPresence();
    bridgeMidi.addEvent (
        juce::MidiMessage::createSysExMessage (
            presencePacket.data(), static_cast<int> (presencePacket.size())),
        0);
    instance->prepareToPlay (48000.0, 512);
    instance->processBlock (bridgeAudio, bridgeMidi);
    instance->releaseResources();

    auto bridgePacketFound = false;
    for (const auto metadata : bridgeMidi)
    {
        const auto& message = metadata.getMessage();
        int sourceLane = -1;
        bool bipolar = false;
        bool active = false;
        float normalizedValue = 0.0f;
        if (message.isSysEx()
            && seqwencer_bridge::decodeLaneValue (
                reinterpret_cast<const std::uint8_t*> (message.getSysExData()),
                static_cast<std::size_t> (message.getSysExDataSize()),
                sourceLane, bipolar, active, normalizedValue))
        {
            if (sourceLane == seqwencer_bridge::sequencerALane
                && (! active || bipolar
                || std::abs (normalizedValue - 0.75f) > 0.0001f)
               )
            {
                std::cout << "FAIL: PHI bridge packet contained the wrong A state or value\n";
                return 1;
            }
            if (sourceLane == seqwencer_bridge::sequencerALane)
                bridgePacketFound = true;
        }
    }

    if (! bridgePacketFound)
    {
        std::cout << "FAIL: VST3 emitted no PHI bridge packet\n";
        return 1;
    }
    std::cout << "PASS: VST3 emits a 75 percent Sequencer A routing value" << std::endl;

    if (! instance->hasEditor())
    {
        std::cout << "FAIL: VST3 reported no editor\n";
        return 1;
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (
        instance->createEditorAndMakeActive());
    if (editor == nullptr)
    {
        std::cout << "FAIL: VST3 editor creation returned null\n";
        return 1;
    }

    instance->editorBeingDeleted (editor.get());
    editor.reset();
    std::cout << "PASS: VST3 editor probe completed" << std::endl;
    std::cout << "All Seqwencer VST3 probe tests passed" << std::endl;
    return 0;
}
