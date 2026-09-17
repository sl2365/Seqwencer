#include "../SequencerCore.h"
#include "../SeqwencerBridgeProtocol.h"

#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void expectNear (const char* name, float actual, float expected, float tolerance = 0.0001f)
{
    if (std::abs (actual - expected) <= tolerance)
    {
        std::cout << "PASS: " << name << '\n';
        return;
    }

    std::cerr << "FAIL: " << name << " expected " << expected
              << " got " << actual << '\n';
    ++failures;
}

void expectEqual (const char* name, int actual, int expected)
{
    if (actual == expected)
    {
        std::cout << "PASS: " << name << '\n';
        return;
    }

    std::cerr << "FAIL: " << name << " expected " << expected
              << " got " << actual << '\n';
    ++failures;
}
}

int main()
{
    expectEqual (
        "HOST SYNC advances when a nested host omits all transport fields",
        seqwencer::hostTimelineShouldAdvance (
            false, false, false, false, false) ? 1 : 0,
        1);
    expectEqual (
        "HOST SYNC honours a stationary reported PPQ as stopped",
        seqwencer::hostTimelineShouldAdvance (
            false, true, false, false, false) ? 1 : 0,
        0);
    expectEqual (
        "HOST SYNC recognises a moving PPQ despite an incorrect play flag",
        seqwencer::hostTimelineShouldAdvance (
            false, true, true, false, false) ? 1 : 0,
        1);
    expectEqual (
        "HOST SYNC recognises moving sample time without PPQ",
        seqwencer::hostTimelineShouldAdvance (
            false, false, false, true, true) ? 1 : 0,
        1);
    expectEqual (
        "PHI falls back when its nested timeline remains stationary",
        seqwencer::stalledPhiTimelineShouldFreeRun (
            true, false, true, false, true, false) ? 1 : 0,
        1);
    expectEqual (
        "a directly hosted stationary timeline remains stopped",
        seqwencer::stalledPhiTimelineShouldFreeRun (
            false, false, true, false, true, false) ? 1 : 0,
        0);
    expectEqual (
        "PHI uses a moving outer timeline instead of fallback",
        seqwencer::stalledPhiTimelineShouldFreeRun (
            true, false, true, true, true, true) ? 1 : 0,
        0);

    const auto bridgePacket =
        seqwencer_bridge::encodeModulationValue (0, 0, 0.75f);
    int decodedBridgeLane = -1;
    int decodedBridgeMacro = -1;
    float decodedBridgeValue = 0.0f;
    expectEqual ("PHI bridge packet decodes",
                 seqwencer_bridge::decodeModulationValue (
                     bridgePacket.data(), bridgePacket.size(),
                     decodedBridgeLane, decodedBridgeMacro,
                     decodedBridgeValue) ? 1 : 0,
                 1);
    expectEqual ("PHI bridge packet identifies Sequencer A",
                 decodedBridgeLane, 0);
    expectEqual ("PHI bridge packet targets Macro 001",
                 decodedBridgeMacro, 0);
    expectNear ("PHI bridge preserves a 75 percent value",
                decodedBridgeValue, 0.75f, 0.0001f);
    auto invalidBridgePacket = bridgePacket;
    invalidBridgePacket[2] = static_cast<std::uint8_t> ('X');
    expectEqual ("unrelated SysEx is not accepted as a PHI bridge packet",
                 seqwencer_bridge::decodeModulationValue (
                     invalidBridgePacket.data(), invalidBridgePacket.size(),
                     decodedBridgeLane, decodedBridgeMacro,
                     decodedBridgeValue) ? 1 : 0,
                 0);
    const auto lanePacket = seqwencer_bridge::encodeLaneValue (
        seqwencer_bridge::sequencerBLane, true, true, 0.25f);
    auto decodedBipolar = false;
    auto decodedActive = false;
    expectEqual ("lane bridge packet decodes",
                 seqwencer_bridge::decodeLaneValue (
                     lanePacket.data(), lanePacket.size(),
                     decodedBridgeLane, decodedBipolar,
                     decodedActive, decodedBridgeValue) ? 1 : 0,
                 1);
    expectEqual ("lane bridge identifies Sequencer B",
                 decodedBridgeLane, seqwencer_bridge::sequencerBLane);
    expectEqual ("lane bridge preserves Bipolar state",
                 decodedBipolar ? 1 : 0, 1);
    expectEqual ("lane bridge preserves Active state",
                 decodedActive ? 1 : 0, 1);
    expectNear ("lane bridge preserves its normalized value",
                decodedBridgeValue, 0.25f, 0.0001f);

    const auto targetBrowserPacket =
        seqwencer_bridge::encodeTargetBrowserRequest (true);
    auto decodedSerialMode = false;
    expectEqual ("PHI target-browser request decodes",
                 seqwencer_bridge::decodeTargetBrowserRequest (
                     targetBrowserPacket.data(),
                     targetBrowserPacket.size(),
                     decodedSerialMode) ? 1 : 0,
                 1);
    expectEqual ("target-browser request preserves SERIAL mode",
                 decodedSerialMode ? 1 : 0, 1);
    expectEqual ("target-browser request is not modulation data",
                 seqwencer_bridge::decodeModulationValue (
                     targetBrowserPacket.data(),
                     targetBrowserPacket.size(),
                     decodedBridgeLane, decodedBridgeMacro,
                     decodedBridgeValue) ? 1 : 0,
                 0);
    expectNear ("larger positive Bipolar value wins",
                seqwencer_bridge::selectFurthestFromZero (
                    0.62f, true, 0.525f, true),
                0.62f);
    expectNear ("larger negative Bipolar value wins",
                seqwencer_bridge::selectFurthestFromZero (
                    0.34f, true, 0.40f, true),
                0.34f);
    expectNear ("largest magnitude wins across polarities",
                seqwencer_bridge::selectFurthestFromZero (
                    0.62f, true, 0.34f, true),
                0.34f);
    expectNear ("Sequencer A wins an exact magnitude tie",
                seqwencer_bridge::selectFurthestFromZero (
                    0.60f, true, 0.40f, true),
                0.60f);
    expectEqual ("Parallel A assignment leaves B untouched",
                 seqwencer_bridge::updateTargetMask (3, 0, false, false),
                 seqwencer_bridge::targetMaskB);
    expectEqual ("SERIAL assignment uses the shared A bit",
                 seqwencer_bridge::updateTargetMask (
                     seqwencer_bridge::targetMaskB, 1, true, true),
                 seqwencer_bridge::targetMaskA
                     | seqwencer_bridge::targetMaskB);
    expectEqual ("SERIAL unassignment preserves the saved Parallel B bit",
                 seqwencer_bridge::updateTargetMask (3, 0, false, true),
                 seqwencer_bridge::targetMaskB);
    const auto presencePacket = seqwencer_bridge::encodePhiPresence();
    expectEqual ("PHI presence packet decodes",
                 seqwencer_bridge::decodePhiPresence (
                     presencePacket.data(), presencePacket.size()) ? 1 : 0,
                 1);
    expectEqual ("PHI presence is not modulation data",
                 seqwencer_bridge::decodeLaneValue (
                     presencePacket.data(), presencePacket.size(),
                     decodedBridgeLane, decodedBipolar,
                     decodedActive, decodedBridgeValue) ? 1 : 0,
                 0);

    expectNear ("1/64T lasts one twenty-fourth of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (1)),
                1.0f / 24.0f);
    expectNear ("1/32T lasts one twelfth of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (3)),
                1.0f / 12.0f);
    expectNear ("1/16T lasts one sixth of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (5)),
                1.0f / 6.0f);
    expectNear ("1/8T lasts one third of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (7)),
                1.0f / 3.0f);
    expectNear ("1/4T lasts two thirds of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (9)),
                2.0f / 3.0f);
    expectNear ("1/2T lasts four thirds of a quarter-note beat",
                static_cast<float> (seqwencer::beatsForRate (11)),
                4.0f / 3.0f);
    expectNear ("the existing straight 1/16 duration is unchanged",
                static_cast<float> (seqwencer::beatsForRate (6)),
                1.0f / 4.0f);

    expectEqual ("Loop begins at Start",
                 seqwencer::sequencePositionForPhase (
                     0.0, 4, seqwencer::SequenceMode::loop).currentOffset,
                 0);
    expectEqual ("Reverse begins at End",
                 seqwencer::sequencePositionForPhase (
                     0.0, 4, seqwencer::SequenceMode::reverse).currentOffset,
                 3);
    expectEqual ("Reverse moves toward Start",
                 seqwencer::sequencePositionForPhase (
                     1.0, 4, seqwencer::SequenceMode::reverse).currentOffset,
                 2);
    expectEqual ("Played traverses forward like Loop",
                 seqwencer::sequencePositionForPhase (
                     1.0, 4, seqwencer::SequenceMode::played).currentOffset,
                 1);
    const int expectedBounceOffsets[] { 0, 1, 2, 3, 2, 1, 0 };
    for (int phase = 0; phase < 7; ++phase)
    {
        expectEqual ("Bounce visits each endpoint only once",
                     seqwencer::sequencePositionForPhase (
                         static_cast<double> (phase), 4,
                         seqwencer::SequenceMode::bounce).currentOffset,
                     expectedBounceOffsets[phase]);
    }
    expectEqual ("four-step Bounce has a six-step traversal cycle",
                 seqwencer::sequenceCycleLength (
                     4, seqwencer::SequenceMode::bounce),
                 6);

    seqwencer::NotePhraseTracker phraseTracker;
    expectEqual ("first played note starts a phrase",
                 phraseTracker.noteOn (1, 60) ? 1 : 0, 1);
    expectEqual ("an overlapping chord note does not retrigger",
                 phraseTracker.noteOn (1, 64) ? 1 : 0, 0);
    phraseTracker.noteOff (1, 60);
    expectEqual ("legato note while another remains held does not retrigger",
                 phraseTracker.noteOn (1, 67) ? 1 : 0, 0);
    phraseTracker.noteOff (1, 64);
    phraseTracker.noteOff (1, 67);
    expectEqual ("the next note after complete release starts a new phrase",
                 phraseTracker.noteOn (1, 72) ? 1 : 0, 1);
    phraseTracker.allNotesOff();
    expectEqual ("all-notes-off permits the next phrase retrigger",
                 phraseTracker.noteOn (2, 72) ? 1 : 0, 1);

    seqwencer::Pattern a {};
    seqwencer::Pattern b {};
    a.fill (0.0f);
    b.fill (0.0f);

    a[0] = 1.0f;
    int active = -1;
    expectNear ("zero attack reaches the new step immediately",
                seqwencer::evaluateBank (a, 0.0, 0.0f, 0.0f, &active), 1.0f);
    expectEqual ("bank reports step one", active, 0);
    expectNear ("half-step attack uses the IceCream-style eased curve",
                seqwencer::evaluateBank (a, 0.25, 0.5f, 0.0f), 0.75f);

    a.fill (1.0f);
    auto minimumHeldLevel = 1.0f;
    for (const auto phase : { 0.0, 0.999, 1.0, 1.999, 31.999 })
        minimumHeldLevel = std::min (minimumHeldLevel,
            seqwencer::evaluateBank (a, phase, 1.0f, 1.0f, &active));
    expectNear ("adjacent full-volume steps form one uninterrupted level",
                minimumHeldLevel, 1.0f);
    a.fill (0.75f);
    expectNear ("equal neighbouring levels remain held across their boundary",
                seqwencer::evaluateBank (
                    a, 1.0, 1.0f, 1.0f, &active), 0.75f);
    expectNear ("visually equal adjacent levels do not retrigger a transition",
                seqwencer::transitionValue (
                    0.99996f, 1.0f, 0.0f, 1.0f, 1.0f),
                1.0f);

    expectEqual ("Gate mode choice zero restores as Off",
                 static_cast<int> (seqwencer::gateStepModeFromChoice (0.0f)),
                 static_cast<int> (seqwencer::GateStepMode::off));
    expectEqual ("Gate mode choice three restores as Link",
                 static_cast<int> (seqwencer::gateStepModeFromChoice (3.0f)),
                 static_cast<int> (seqwencer::GateStepMode::linkStep));
    expectNear ("an Off Gate step is silent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::linkStep,
                    seqwencer::GateStepMode::off,
                    0.25f, 0.0f, 0.0f),
                0.0f);
    expectNear ("a Short Gate step remains open before halfway",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::shortStep,
                    0.49f, 0.0f, 0.0f),
                1.0f);
    expectNear ("a Short Gate step closes at halfway",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::shortStep,
                    0.50f, 0.0f, 0.0f),
                0.0f);
    expectNear ("a Long Gate step remains open before ninety percent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::longStep,
                    0.89f, 0.0f, 0.0f),
                1.0f);
    expectNear ("a Long Gate step closes at ninety percent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::longStep,
                    0.90f, 0.0f, 0.0f),
                0.0f);
    expectNear ("an adjustable Short Gate remains open to sixty percent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::shortStep,
                    0.59f, 0.0f, 0.0f, 0.60f, 0.90f),
                1.0f);
    expectNear ("an adjustable Short Gate closes at sixty percent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::shortStep,
                    0.60f, 0.0f, 0.0f, 0.60f, 0.90f),
                0.0f);
    expectNear ("an adjustable Long Gate closes at sixty-five percent",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::longStep,
                    0.65f, 0.0f, 0.0f, 0.50f, 0.65f),
                0.0f);
    expectNear ("Release smooths the closed half of a Short Gate step",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::longStep,
                    seqwencer::GateStepMode::shortStep,
                    0.75f, 0.0f, 1.0f),
                0.25f);
    expectNear ("equal Link Gate steps remain continuous",
                seqwencer::evaluateGateStep (
                    1.0f, 1.0f,
                    seqwencer::GateStepMode::linkStep,
                    seqwencer::GateStepMode::linkStep,
                    0.0f, 1.0f, 1.0f),
                1.0f);

    seqwencer::GateModePattern gateModesA {};
    seqwencer::GateModePattern gateModesB {};
    gateModesA.fill (seqwencer::GateStepMode::longStep);
    gateModesB.fill (seqwencer::GateStepMode::longStep);

    a.fill (1.0f);
    a[1] = 0.0f;
    expectNear ("half-step release uses an independent curve",
                seqwencer::evaluateBank (a, 1.25, 0.0f, 0.5f), 0.25f);

    a.fill (1.0f);
    b.fill (1.0f);
    expectNear ("equal SERIAL steps remain held across the A-to-B boundary",
                seqwencer::evaluateLinked (
                    a, b, 32.0, 1.0f, 1.0f, 1.0f, 1.0f,
                    nullptr, nullptr),
                1.0f);

    a.fill (0.0f);
    b.fill (0.0f);
    a[31] = 0.25f;
    b[0] = 1.0f;
    int bank = -1;
    active = -1;
    expectNear ("serial mode crosses from A32 into B1",
                seqwencer::evaluateLinked (
                    a, b, 32.25, 0.0f, 0.0f, 0.5f, 0.0f, &bank, &active),
                0.8125f);
    expectEqual ("serial mode reports bank B", bank, 1);
    expectEqual ("serial mode reports B step one", active, 0);

    b[31] = 0.5f;
    a[0] = 0.0f;
    expectNear ("serial loop crosses from B32 into A1",
                seqwencer::evaluateLinked (
                    a, b, 0.25, 0.0f, 0.5f, 0.0f, 0.0f, &bank, &active),
                0.125f);

    const auto bankRange = seqwencer::makeStepRange (5, 8, 32);
    expectEqual ("bank range converts Start to a zero-based step",
                 bankRange.first, 4);
    expectEqual ("bank range includes End", bankRange.last, 7);
    expectEqual ("bank range reports its inclusive length", bankRange.length(), 4);
    const auto clampedParallelRange = seqwencer::makeStepRange (50, 64, 32);
    expectEqual ("Parallel Start clamps to step 31",
                 clampedParallelRange.first, 30);
    expectEqual ("Parallel End clamps to step 32",
                 clampedParallelRange.last, 31);
    const auto crossedRange = seqwencer::makeStepRange (20, 10, 32);
    expectEqual ("a crossed Start is constrained to End",
                 crossedRange.first, crossedRange.last);

    a.fill (0.0f);
    a[7] = 0.25f;
    a[4] = 1.0f;
    expectNear ("a ranged bank loops from End into Start",
                seqwencer::evaluateBankRange (
                    a, 0.25, bankRange, 0.5f, 0.0f, &active),
                0.8125f);
    expectEqual ("ranged bank reports the selected Start step", active, 4);
    expectNear ("a ranged bank can begin in Reverse at End",
                seqwencer::evaluateBankRange (
                    a, 0.0, bankRange, 0.0f, 0.0f, &active,
                    seqwencer::SequenceMode::reverse),
                0.25f);
    expectEqual ("Reverse reports the selected End step", active, 7);

    a.fill (0.0f);
    b.fill (0.0f);
    a[30] = 1.0f;
    b[1] = 0.25f;
    const auto serialRange = seqwencer::makeStepRange (31, 34, 64);
    expectNear ("a ranged SERIAL run crosses from A into B",
                seqwencer::evaluateLinkedRange (
                    a, b, 2.0, serialRange,
                    0.0f, 0.0f, 0.0f, 0.0f, &bank, &active),
                0.0f);
    expectEqual ("ranged SERIAL run reports bank B", bank, 1);
    expectEqual ("ranged SERIAL run reports B step one", active, 0);
    seqwencer::evaluateLinkedRange (
        a, b, 3.0, serialRange,
        0.0f, 0.0f, 0.0f, 0.0f, &bank, &active,
        seqwencer::SequenceMode::bounce);
    expectEqual ("SERIAL Bounce reaches its End bank", bank, 1);
    expectEqual ("SERIAL Bounce reaches the selected End step", active, 1);
    seqwencer::evaluateLinkedRange (
        a, b, 4.0, serialRange,
        0.0f, 0.0f, 0.0f, 0.0f, &bank, &active,
        seqwencer::SequenceMode::bounce);
    expectEqual ("SERIAL Bounce reverses without repeating End", active, 0);

    a.fill (1.0f);
    b.fill (1.0f);
    gateModesA[31] = seqwencer::GateStepMode::linkStep;
    gateModesB[0] = seqwencer::GateStepMode::linkStep;
    const auto completeSerialRange = seqwencer::makeStepRange (1, 64, 64);
    expectNear ("a Link Gate step crosses continuously from A32 into B1",
                seqwencer::evaluateGateLinkedRange (
                    a, b, gateModesA, gateModesB, 32.0,
                    completeSerialRange,
                    1.0f, 1.0f, 1.0f, 1.0f, &bank, &active),
                1.0f);
    gateModesA[31] = seqwencer::GateStepMode::longStep;
    expectNear ("a non-Link A32 closes before SERIAL enters B1",
                seqwencer::evaluateGateLinkedRange (
                    a, b, gateModesA, gateModesB, 32.0,
                    completeSerialRange,
                    1.0f, 1.0f, 1.0f, 1.0f, &bank, &active),
                0.0f);

    expectNear ("zero depth leaves the Volume level unchanged",
                seqwencer::applyGateDepth (0.8f, 0.0f, 0.0f), 0.8f);
    expectNear ("full gate depth follows the pattern",
                seqwencer::applyGateDepth (0.8f, 0.25f, 1.0f), 0.2f);
    expectNear ("quarter depth blends dry and patterned level",
                seqwencer::applyGateDepth (0.8f, 0.0f, 0.25f), 0.6f);
    expectNear ("bipolar centre leaves Volume unchanged",
                seqwencer::applyModulationDepth (0.4f, 0.0f, 1.0f), 0.4f);
    expectNear ("bipolar maximum reaches the parameter ceiling",
                seqwencer::applyModulationDepth (0.4f, 1.0f, 1.0f), 1.0f);
    expectNear ("bipolar minimum reaches the parameter floor",
                seqwencer::applyModulationDepth (0.4f, -1.0f, 1.0f), 0.0f);
    expectNear ("bipolar Depth scales travel around Volume",
                seqwencer::applyModulationDepth (0.4f, 1.0f, 0.5f), 0.7f);
    expectNear ("unipolar 100 percent stores as canonical plus 100 percent",
                seqwencer::canonicalFromUnipolar (1.0f), 1.0f);
    expectNear ("unipolar 75 percent stores as canonical plus 75 percent",
                seqwencer::canonicalFromUnipolar (0.75f), 0.875f);
    expectNear ("unipolar 50 percent stores as canonical plus 50 percent",
                seqwencer::canonicalFromUnipolar (0.5f), 0.75f);
    expectNear ("unipolar 25 percent stores as canonical plus 25 percent",
                seqwencer::canonicalFromUnipolar (0.25f), 0.625f);
    expectNear ("positive canonical values round-trip through Unipolar",
                seqwencer::unipolarFromCanonical (0.875f), 0.75f);

    constexpr auto retainedNegativeStep = 0.25f;
    expectNear ("a negative canonical step is masked at the Unipolar baseline",
                seqwencer::displayFromCanonical (retainedNegativeStep, false), 0.0f);
    expectNear ("the same retained step returns as bipolar minus 50 percent",
                seqwencer::bipolarDeviationFromCanonical (retainedNegativeStep), -0.5f);
    expectNear ("Bipolar view keeps the original canonical step untouched",
                seqwencer::displayFromCanonical (retainedNegativeStep, true),
                retainedNegativeStep);
    expectNear ("semantic zero has the same canonical centre in both modes",
                seqwencer::canonicalFromUnipolar (0.0f), 0.5f);
    const auto bipolarPositiveForGate = seqwencer::targetValueFromCanonical (
        seqwencer::canonicalFromUnipolar (0.75f),
        seqwencer::ModulationTarget::gateLevel);
    expectNear ("bipolar plus 75 percent remains a 75 percent Gate level",
                bipolarPositiveForGate, 0.75f);
    expectNear ("bipolar plus 75 percent still gates when Volume is full",
                seqwencer::applyModulationDepth (
                    1.0f, bipolarPositiveForGate - 1.0f, 1.0f),
                0.75f);
    expectNear ("a negative bipolar step becomes zero for Gate",
                seqwencer::targetValueFromCanonical (
                    retainedNegativeStep,
                    seqwencer::ModulationTarget::gateLevel),
                0.0f);

    bool targetWasAssigned = false;
    expectNear ("a lane controls only its assigned target",
                seqwencer::combineParallelTarget (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::gateLevel,
                    0.25f, true, seqwencer::ModulationTarget::none,
                    &targetWasAssigned),
                0.5f);
    expectEqual ("target assignment is reported", targetWasAssigned ? 1 : 0, 1);

    expectNear ("two lanes assigned to one target multiply",
                seqwencer::combineParallelTarget (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::gateLevel,
                    0.25f, true, seqwencer::ModulationTarget::gateLevel),
                0.125f);

    expectNear ("two unipolar lanes preserve multiplied modulation",
                seqwencer::combineParallelModulationDeviation (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::gateLevel, false,
                    0.25f, true, seqwencer::ModulationTarget::gateLevel, false),
                -0.875f);
    expectNear ("a bipolar centre is neutral",
                seqwencer::combineParallelModulationDeviation (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::gateLevel, true,
                    1.0f, false, seqwencer::ModulationTarget::none, false),
                0.0f);
    expectNear ("unipolar and bipolar lane movements combine",
                seqwencer::combineParallelModulationDeviation (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::gateLevel, false,
                    0.75f, true, seqwencer::ModulationTarget::gateLevel, true),
                0.0f);

    targetWasAssigned = false;
    expectNear ("an unticked target remains assigned but contributes no modulation",
                seqwencer::combineParallelTarget (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, false, seqwencer::ModulationTarget::gateLevel,
                    0.25f, true, seqwencer::ModulationTarget::gateLevel,
                    &targetWasAssigned),
                0.25f);
    expectEqual ("the other enabled target remains active",
                 targetWasAssigned ? 1 : 0, 1);

    targetWasAssigned = true;
    expectNear ("an unassigned target remains neutral",
                seqwencer::combineParallelTarget (
                    seqwencer::ModulationTarget::gateLevel,
                    0.5f, true, seqwencer::ModulationTarget::none,
                    0.25f, false, seqwencer::ModulationTarget::gateLevel,
                    &targetWasAssigned),
                1.0f);
    expectEqual ("unassigned target is reported", targetWasAssigned ? 1 : 0, 0);

    targetWasAssigned = true;
    expectNear ("None can never become a modulation destination",
                seqwencer::combineParallelTarget (
                    seqwencer::ModulationTarget::none,
                    0.5f, true, seqwencer::ModulationTarget::none,
                    0.25f, true, seqwencer::ModulationTarget::none,
                    &targetWasAssigned),
                1.0f);
    expectEqual ("None remains unassigned", targetWasAssigned ? 1 : 0, 0);

    expectEqual ("gate target choice restores as Volume",
                 static_cast<int> (seqwencer::targetFromChoice (1.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::gateLevel));
    expectEqual ("unknown target choice restores safely as None",
                 static_cast<int> (seqwencer::targetFromChoice (99.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::none));
    expectEqual ("Depth target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (2.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::gateDepth));
    expectEqual ("Short Step target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (3.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::shortGateLength));
    expectEqual ("Long Step target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (4.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::longGateLength));
    expectNear ("Depth target scales from zero to its knob ceiling",
                seqwencer::modulatedCeiling (0.8f, 0.25f), 0.2f);
    expectNear ("Short Step target never passes its ten-percent floor",
                seqwencer::modulatedRangeFromMinimum (0.10f, 0.50f, 0.0f),
                0.10f);
    expectNear ("Long Step target scales inside its legal range",
                seqwencer::modulatedRangeFromMinimum (0.65f, 0.95f, 0.5f),
                0.80f);

    if (failures != 0)
    {
        std::cerr << failures << " core test(s) failed\n";
        return 1;
    }

    std::cout << "All Seqwencer core tests passed\n";
    return 0;
}
