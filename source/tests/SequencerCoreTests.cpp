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
    expectNear ("a dual target contrasts with identical lane colours",
                seqwencer::combinedTargetHue (0.20f, 0.20f), 0.70f);
    expectNear ("a dual target handles the hue wraparound",
                seqwencer::combinedTargetHue (0.95f, 0.05f), 0.50f);
    expectNear ("opposing lane colours produce a distinct dual target",
                seqwencer::combinedTargetHue (0.0f, 0.5f), 0.25f);

    const auto defaultFxOrder = seqwencer::defaultAudioFxOrder();
    expectEqual ("default FX routing begins with Gate",
                 static_cast<int> (defaultFxOrder.front()),
                 static_cast<int> (seqwencer::AudioFxStage::gate));
    expectEqual ("default FX routing ends with Compressor",
                 static_cast<int> (defaultFxOrder.back()),
                 static_cast<int> (seqwencer::AudioFxStage::compressor));

    const auto compressorFirst = seqwencer::moveAudioFxStage (
        defaultFxOrder, seqwencer::AudioFxStage::compressor, 0);
    expectEqual ("FX routing can move Compressor to the first stage",
                 static_cast<int> (compressorFirst.front()),
                 static_cast<int> (seqwencer::AudioFxStage::compressor));
    expectEqual ("moving Compressor first shifts Gate to the second stage",
                 static_cast<int> (compressorFirst[1]),
                 static_cast<int> (seqwencer::AudioFxStage::gate));

    const auto repairedFxOrder = seqwencer::sanitiseAudioFxOrder (
        std::array<int, seqwencer::audioFxStageCount> {
            8, 8, -1, 2, 99, 0, 5, 4, 3 });
    std::array<bool, seqwencer::audioFxStageCount> repairedStages {};
    for (const auto stage : repairedFxOrder)
        repairedStages[static_cast<std::size_t> (
            static_cast<int> (stage))] = true;
    expectEqual ("invalid saved FX routing is repaired to nine unique stages",
                 static_cast<int> (std::count (
                     repairedStages.begin(), repairedStages.end(), true)),
                 seqwencer::audioFxStageCount);

    seqwencer::Pattern numberedSteps {};
    for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        numberedSteps[static_cast<std::size_t> (step)] =
            static_cast<float> (step);
    const auto nudgedLeft = seqwencer::nudgeStepArray (numberedSteps, -1);
    const auto nudgedRight = seqwencer::nudgeStepArray (numberedSteps, 1);
    expectNear ("left nudge moves step two into step one",
                nudgedLeft.front(), 1.0f);
    expectNear ("left nudge wraps step one to the end",
                nudgedLeft.back(), 0.0f);
    expectNear ("right nudge wraps the final step to step one",
                nudgedRight.front(),
                static_cast<float> (seqwencer::stepsPerBank - 1));
    expectNear ("right nudge moves step one into step two",
                nudgedRight[1], 0.0f);

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
    expectEqual ("Random is the fifth Direction choice",
                 static_cast<int> (seqwencer::sequenceModeFromChoice (4.0f)),
                 static_cast<int> (seqwencer::SequenceMode::random));
    expectEqual ("Played retains its original Direction choice",
                 static_cast<int> (seqwencer::sequenceModeFromChoice (3.0f)),
                 static_cast<int> (seqwencer::SequenceMode::played));
    expectEqual ("four-step Random has eight shuffled passes",
                 seqwencer::sequenceCycleLength (
                     4, seqwencer::SequenceMode::random),
                 32);
    std::array<bool, 4> firstRandomPass {};
    auto randomOffsetsAreInRange = true;
    auto randomPassesDiffer = false;
    for (int phase = 0; phase < 8; ++phase)
    {
        const auto offset = seqwencer::sequencePositionForPhase (
            static_cast<double> (phase), 4,
            seqwencer::SequenceMode::random).currentOffset;
        randomOffsetsAreInRange = randomOffsetsAreInRange
            && offset >= 0 && offset < 4;
        if (phase < 4)
            firstRandomPass[static_cast<std::size_t> (offset)] = true;
        else
        {
            const auto firstPassOffset = seqwencer::sequencePositionForPhase (
                static_cast<double> (phase - 4), 4,
                seqwencer::SequenceMode::random).currentOffset;
            randomPassesDiffer = randomPassesDiffer
                || offset != firstPassOffset;
        }
    }
    expectEqual ("Random offsets remain inside the selected range",
                 randomOffsetsAreInRange ? 1 : 0, 1);
    expectEqual ("each Random pass visits every selected step once",
                 static_cast<int> (std::count (
                     firstRandomPass.begin(), firstRandomPass.end(), true)),
                 4);
    expectEqual ("successive Random passes use different orders",
                 randomPassesDiffer ? 1 : 0, 1);
    expectEqual ("Random traversal is repeatable at the same phase",
                 seqwencer::sequencePositionForPhase (
                     19.0, 4, seqwencer::SequenceMode::random).currentOffset,
                 seqwencer::sequencePositionForPhase (
                     51.0, 4, seqwencer::SequenceMode::random).currentOffset);

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

    const auto bipolarSaw = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::saw, true);
    expectNear ("Bipolar Saw begins at the bottom", bipolarSaw[0], 0.0f);
    expectNear ("Bipolar Saw reaches the top", bipolarSaw[31], 1.0f);
    const auto unipolarSaw = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::saw, false);
    expectNear ("Unipolar Saw stores its visible bottom at canonical centre",
                unipolarSaw[0], 0.5f);
    expectNear ("Unipolar Saw still reaches the visible top",
                unipolarSaw[31], 1.0f);
    const auto doubleSaw = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::sawDouble, true);
    expectNear ("Double Saw completes its first cycle", doubleSaw[15], 1.0f);
    expectNear ("Double Saw restarts for its second cycle", doubleSaw[16], 0.0f);
    const auto sawDown = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::sawDown, true);
    expectNear ("Saw Down begins at the top", sawDown[0], 1.0f);
    expectNear ("Saw Down reaches the bottom", sawDown[31], 0.0f);
    const auto sawDownDouble = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::sawDownDouble, true);
    expectNear ("Double Saw Down completes its first cycle",
                sawDownDouble[15], 0.0f);
    expectNear ("Double Saw Down restarts at the top",
                sawDownDouble[16], 1.0f);
    const auto sine = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::sine, true);
    expectNear ("Sine reaches the top quarter-cycle", sine[8], 1.0f);
    expectNear ("Sine reaches the bottom three-quarter-cycle", sine[24], 0.0f);
    const auto triangle = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::triangle, true);
    expectNear ("Triangle begins at the bottom", triangle[0], 0.0f);
    expectNear ("Triangle reaches the top halfway through", triangle[16], 1.0f);
    const auto pulse25 = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::pulse25, true);
    expectNear ("Pulse 25 keeps its first quarter high", pulse25[7], 1.0f);
    expectNear ("Pulse 25 drops after its first quarter", pulse25[8], 0.0f);
    const auto square = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::square, true);
    expectNear ("Square keeps its first half high", square[15], 1.0f);
    expectNear ("Square drops for its second half", square[16], 0.0f);
    const auto pulse75Double = seqwencer::makeWaveformPreset (
        seqwencer::WaveformPreset::pulse75Double, true);
    expectNear ("Double Pulse 75 keeps twelve steps high",
                pulse75Double[11], 1.0f);
    expectNear ("Double Pulse 75 drops on step thirteen",
                pulse75Double[12], 0.0f);

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
    expectEqual ("Noise Gate Threshold target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (5.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::noiseGateThreshold));
    expectEqual ("Delay Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (12.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::delayMix));
    expectEqual ("Reverb Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (16.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::reverbMix));
    expectEqual ("Pan target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (17.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::panPosition));
    expectEqual ("Filter Cutoff target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (18.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::filterCutoff));
    expectEqual ("Filter Resonance target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (19.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::filterResonance));
    expectEqual ("Filter Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (20.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::filterMix));
    expectEqual ("Pitch Shift target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (21.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::pitchShift));
    expectEqual ("Pitch Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (22.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::pitchMix));
    expectEqual ("Distortion Drive target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (23.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::distortionDrive));
    expectEqual ("Distortion Tone target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (24.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::distortionTone));
    expectEqual ("Distortion Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (25.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::distortionMix));
    expectEqual ("Grain Size target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (26.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::grainSize));
    expectEqual ("Grain Shift target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (27.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::grainShift));
    expectEqual ("Grain Feedback target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (28.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::grainFeedback));
    expectEqual ("Grain Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (29.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::grainMix));
    expectEqual ("Compressor Threshold target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (30.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorThreshold));
    expectEqual ("Compressor Ratio target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (31.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorRatio));
    expectEqual ("Compressor Attack target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (32.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorAttack));
    expectEqual ("Compressor Release target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (33.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorRelease));
    expectEqual ("Compressor Makeup target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (34.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorMakeup));
    expectEqual ("Compressor Mix target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (35.0f)),
                 static_cast<int> (seqwencer::ModulationTarget::compressorMix));
    expectEqual ("Gate Sequencer A Attack target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (36.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::gateSequencerAAttack));
    expectEqual ("Compressor Sequencer B Release target choice restores",
                 static_cast<int> (seqwencer::targetFromChoice (71.0f)),
                 static_cast<int> (
                     seqwencer::ModulationTarget::compressorSequencerBRelease));
    const auto filterEnvelopeTargets = seqwencer::sequencerEnvelopeTargets (
        seqwencer::SequencerEngine::filter);
    expectEqual ("Filter exposes A Attack as its first envelope target",
                 static_cast<int> (filterEnvelopeTargets[0]),
                 static_cast<int> (
                     seqwencer::ModulationTarget::filterSequencerAAttack));
    expectEqual ("Filter exposes B Release as its fourth envelope target",
                 static_cast<int> (filterEnvelopeTargets[3]),
                 static_cast<int> (
                     seqwencer::ModulationTarget::filterSequencerBRelease));
    expectEqual ("an envelope target identifies its owning engine",
                 static_cast<int> (
                     seqwencer::sequencerEnvelopeTargetEngine (
                         seqwencer::ModulationTarget::grainSequencerARelease)),
                 static_cast<int> (seqwencer::SequencerEngine::grain));
    expectEqual ("an envelope target identifies Sequencer B",
                 seqwencer::sequencerEnvelopeTargetBank (
                     seqwencer::ModulationTarget::pitchSequencerBAttack),
                 1);
    expectEqual ("an envelope target distinguishes Attack from Release",
                 seqwencer::sequencerEnvelopeTargetIsAttack (
                     seqwencer::ModulationTarget::delaySequencerBRelease)
                     ? 1 : 0,
                 0);
    expectEqual ("PHI has no internal envelope modulation targets",
                 static_cast<int> (seqwencer::sequencerEnvelopeTargets (
                     seqwencer::SequencerEngine::phi)[0]),
                 static_cast<int> (seqwencer::ModulationTarget::none));
    expectEqual ("Gate Volume remains a unipolar destination",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::gateLevel) ? 1 : 0,
                 0);
    expectEqual ("Noise Gate targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::noiseGateThreshold) ? 1 : 0,
                 1);
    expectEqual ("Delay targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::delayTime) ? 1 : 0,
                 1);
    expectEqual ("Reverb targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::reverbSize) ? 1 : 0,
                 1);
    expectEqual ("Pan accepts bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::panPosition) ? 1 : 0,
                 1);
    expectEqual ("Filter targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::filterCutoff) ? 1 : 0,
                 1);
    expectEqual ("Pitch targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::pitchShift) ? 1 : 0,
                 1);
    expectEqual ("Distortion targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::distortionDrive) ? 1 : 0,
                 1);
    expectEqual ("Grain Shifter targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::grainShift) ? 1 : 0,
                 1);
    expectEqual ("Compressor targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::compressorThreshold) ? 1 : 0,
                 1);
    expectEqual ("Sequencer envelope targets accept bipolar movement",
                 seqwencer::targetSupportsBipolar (
                     seqwencer::ModulationTarget::reverbSequencerBAttack)
                     ? 1 : 0,
                 1);
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
