#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace seqwencer
{
constexpr int stepsPerBank = 32;
constexpr int linkedStepCount = 64;
constexpr int rateChoiceCount = 14;
using Pattern = std::array<float, stepsPerBank>;

template <typename Value>
inline std::array<Value, stepsPerBank> nudgeStepArray (
    std::array<Value, stepsPerBank> values, int direction) noexcept
{
    if (direction < 0)
        std::rotate (values.begin(), values.begin() + 1, values.end());
    else if (direction > 0)
        std::rotate (values.rbegin(), values.rbegin() + 1, values.rend());
    return values;
}

enum class GateStepMode
{
    off = 0,
    shortStep = 1,
    longStep = 2,
    linkStep = 3
};

enum class SequenceMode
{
    loop = 0,
    bounce = 1,
    reverse = 2,
    played = 3,
    random = 4
};

enum class SequencerEngine
{
    gate = 0,
    phi,
    delay,
    reverb,
    pan,
    filter,
    pitch,
    distortion,
    grain,
    compressor
};

inline float combinedTargetHue (float hueA, float hueB) noexcept
{
    const auto wrapHue = [] (float hue)
    {
        hue = std::fmod (hue, 1.0f);
        return hue < 0.0f ? hue + 1.0f : hue;
    };
    hueA = wrapHue (hueA);
    hueB = wrapHue (hueB);

    constexpr auto twoPi = 6.28318530717958647692;
    const auto x = std::cos (static_cast<double> (hueA) * twoPi)
                 + std::cos (static_cast<double> (hueB) * twoPi);
    const auto y = std::sin (static_cast<double> (hueA) * twoPi)
                 + std::sin (static_cast<double> (hueB) * twoPi);

    if (std::hypot (x, y) < 0.00001)
        return wrapHue (std::min (hueA, hueB) + 0.25f);

    auto midpoint = static_cast<float> (std::atan2 (y, x) / twoPi);
    midpoint = wrapHue (midpoint);
    return wrapHue (midpoint + 0.5f);
}

enum class AudioFxStage
{
    gate = 0,
    delay,
    reverb,
    pan,
    filter,
    pitch,
    distortion,
    grain,
    compressor
};

constexpr int audioFxStageCount = 9;
using AudioFxOrder = std::array<AudioFxStage, audioFxStageCount>;

inline constexpr AudioFxOrder defaultAudioFxOrder() noexcept
{
    return { AudioFxStage::gate,
             AudioFxStage::delay,
             AudioFxStage::reverb,
             AudioFxStage::pan,
             AudioFxStage::filter,
             AudioFxStage::pitch,
             AudioFxStage::distortion,
             AudioFxStage::grain,
             AudioFxStage::compressor };
}

inline AudioFxOrder sanitiseAudioFxOrder (
    const std::array<int, audioFxStageCount>& storedOrder) noexcept
{
    AudioFxOrder result {};
    std::array<bool, audioFxStageCount> used {};
    auto destination = 0;
    for (const auto value : storedOrder)
    {
        if (value < 0 || value >= audioFxStageCount
            || used[static_cast<std::size_t> (value)])
            continue;

        result[static_cast<std::size_t> (destination++)] =
            static_cast<AudioFxStage> (value);
        used[static_cast<std::size_t> (value)] = true;
    }

    for (const auto stage : defaultAudioFxOrder())
    {
        const auto value = static_cast<int> (stage);
        if (! used[static_cast<std::size_t> (value)])
            result[static_cast<std::size_t> (destination++)] = stage;
    }
    return result;
}

inline AudioFxOrder moveAudioFxStage (AudioFxOrder order,
                                      AudioFxStage stage,
                                      int destinationIndex) noexcept
{
    auto sourceIndex = -1;
    for (int index = 0; index < audioFxStageCount; ++index)
        if (order[static_cast<std::size_t> (index)] == stage)
            sourceIndex = index;

    if (sourceIndex < 0)
        return order;

    destinationIndex = std::clamp (
        destinationIndex, 0, audioFxStageCount - 1);
    if (sourceIndex == destinationIndex)
        return order;

    const auto movedStage = order[static_cast<std::size_t> (sourceIndex)];
    if (sourceIndex < destinationIndex)
    {
        for (auto index = sourceIndex; index < destinationIndex; ++index)
            order[static_cast<std::size_t> (index)] =
                order[static_cast<std::size_t> (index + 1)];
    }
    else
    {
        for (auto index = sourceIndex; index > destinationIndex; --index)
            order[static_cast<std::size_t> (index)] =
                order[static_cast<std::size_t> (index - 1)];
    }
    order[static_cast<std::size_t> (destinationIndex)] = movedStage;
    return order;
}

enum class WaveformPreset
{
    saw = 0,
    sawDouble,
    sawDown,
    sawDownDouble,
    sine,
    sineDouble,
    triangle,
    triangleDouble,
    pulse25,
    pulse25Double,
    square,
    squareDouble,
    pulse75,
    pulse75Double
};

inline SequenceMode sequenceModeFromChoice (float choice) noexcept
{
    const auto index = std::clamp (
        static_cast<int> (std::lround (choice)),
        static_cast<int> (SequenceMode::loop),
        static_cast<int> (SequenceMode::random));
    return static_cast<SequenceMode> (index);
}

class NotePhraseTracker
{
public:
    bool noteOn (int midiChannel, int noteNumber) noexcept
    {
        const auto index = noteIndex (midiChannel, noteNumber);
        if (index < 0 || heldNotes[static_cast<std::size_t> (index)])
            return false;

        const auto startsPhrase = heldNoteCount == 0;
        heldNotes[static_cast<std::size_t> (index)] = true;
        ++heldNoteCount;
        return startsPhrase;
    }

    void noteOff (int midiChannel, int noteNumber) noexcept
    {
        const auto index = noteIndex (midiChannel, noteNumber);
        if (index < 0 || ! heldNotes[static_cast<std::size_t> (index)])
            return;

        heldNotes[static_cast<std::size_t> (index)] = false;
        heldNoteCount = std::max (0, heldNoteCount - 1);
    }

    void allNotesOff (int midiChannel = 0) noexcept
    {
        if (midiChannel < 1 || midiChannel > 16)
        {
            reset();
            return;
        }

        const auto first = (midiChannel - 1) * 128;
        for (int note = 0; note < 128; ++note)
        {
            auto& held = heldNotes[static_cast<std::size_t> (first + note)];
            if (held)
            {
                held = false;
                heldNoteCount = std::max (0, heldNoteCount - 1);
            }
        }
    }

    void reset() noexcept
    {
        heldNotes.fill (false);
        heldNoteCount = 0;
    }

    int getHeldNoteCount() const noexcept { return heldNoteCount; }

private:
    static int noteIndex (int midiChannel, int noteNumber) noexcept
    {
        if (midiChannel < 1 || midiChannel > 16
            || noteNumber < 0 || noteNumber > 127)
            return -1;
        return (midiChannel - 1) * 128 + noteNumber;
    }

    std::array<bool, 16 * 128> heldNotes {};
    int heldNoteCount = 0;
};

using GateModePattern = std::array<GateStepMode, stepsPerBank>;

constexpr float shortGateOpenFraction = 0.50f;
constexpr float longGateOpenFraction = 0.90f;

inline GateStepMode gateStepModeFromChoice (float choice) noexcept
{
    const auto index = std::clamp (
        static_cast<int> (std::lround (choice)),
        static_cast<int> (GateStepMode::off),
        static_cast<int> (GateStepMode::linkStep));
    return static_cast<GateStepMode> (index);
}

inline double beatsForRate (int rateIndex) noexcept
{
    constexpr std::array<double, rateChoiceCount> beatsPerRate {
        1.0 / 32.0, 1.0 / 24.0, 1.0 / 16.0, 1.0 / 12.0,
        1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0, 1.0 / 3.0,
        1.0 / 2.0, 2.0 / 3.0, 1.0, 4.0 / 3.0, 2.0, 4.0
    };

    rateIndex = std::clamp (rateIndex, 0, rateChoiceCount - 1);
    return beatsPerRate[static_cast<std::size_t> (rateIndex)];
}

struct StepRange
{
    int first = 0;
    int last = stepsPerBank - 1;

    int length() const noexcept
    {
        return std::max (1, last - first + 1);
    }
};

inline StepRange makeStepRange (int startOneBased,
                                int endOneBased,
                                int maximumStep) noexcept
{
    maximumStep = std::max (2, maximumStep);
    const auto start = std::clamp (startOneBased, 1, maximumStep - 1);
    const auto end = std::clamp (endOneBased, 2, maximumStep);
    return { std::min (start, end) - 1, end - 1 };
}

inline StepRange moveLinkedStepRange (StepRange range,
                                      int requestedBoundaryOneBased,
                                      bool movingStart,
                                      int maximumStep) noexcept
{
    maximumStep = std::max (2, maximumStep);
    range = makeStepRange (range.first + 1, range.last + 1, maximumStep);
    const auto length = std::clamp (range.length(), 2, maximumStep);
    if (movingStart)
    {
        const auto first = std::clamp (
            requestedBoundaryOneBased - 1, 0, maximumStep - length);
        return { first, first + length - 1 };
    }

    const auto last = std::clamp (
        requestedBoundaryOneBased - 1, length - 1, maximumStep - 1);
    return { last - length + 1, last };
}

inline StepRange makeLengthStepRange (int startOneBased,
                                      int length,
                                      int maximumStep) noexcept
{
    maximumStep = std::max (2, maximumStep);
    length = std::clamp (length, 2, maximumStep);
    const auto first = std::clamp (
        startOneBased - 1, 0, maximumStep - length);
    return { first, first + length - 1 };
}

inline StepRange resolveModulatedStepRange (
    StepRange baseRange,
    int maximumStep,
    bool linkedBoundaries,
    float normalisedStart,
    bool startAssigned,
    float normalisedEnd,
    bool endAssigned) noexcept
{
    maximumStep = std::max (2, maximumStep);
    baseRange = makeStepRange (
        baseRange.first + 1, baseRange.last + 1, maximumStep);
    normalisedStart = std::clamp (normalisedStart, 0.0f, 1.0f);
    normalisedEnd = std::clamp (normalisedEnd, 0.0f, 1.0f);
    const auto boundaryTravel = std::max (1, maximumStep - 2);
    const auto requestedStart = 1 + static_cast<int> (std::lround (
        normalisedStart * static_cast<float> (boundaryTravel)));
    const auto requestedEnd = 2 + static_cast<int> (std::lround (
        normalisedEnd * static_cast<float> (boundaryTravel)));

    if (! linkedBoundaries)
    {
        return makeStepRange (
            startAssigned ? requestedStart : baseRange.first + 1,
            endAssigned ? requestedEnd : baseRange.last + 1,
            maximumStep);
    }

    if (! startAssigned && ! endAssigned)
        return baseRange;

    const auto length = baseRange.length();
    const auto maximumFirst = maximumStep - length;
    auto requestedFirst = baseRange.first;
    if (startAssigned && endAssigned)
    {
        const auto fromStart = std::clamp (
            requestedStart - 1, 0, maximumFirst);
        const auto fromEnd = std::clamp (
            requestedEnd - length, 0, maximumFirst);
        requestedFirst = static_cast<int> (std::lround (
            0.5 * static_cast<double> (fromStart + fromEnd)));
    }
    else if (startAssigned)
    {
        requestedFirst = requestedStart - 1;
    }
    else
    {
        requestedFirst = requestedEnd - length;
    }

    requestedFirst = std::clamp (requestedFirst, 0, maximumFirst);
    return { requestedFirst, requestedFirst + length - 1 };
}

inline StepRange resolveModulatedLengthStepRange (
    StepRange baseRange,
    int maximumStep,
    float normalisedStart,
    bool startAssigned,
    float normalisedLength,
    bool lengthAssigned) noexcept
{
    maximumStep = std::max (2, maximumStep);
    baseRange = makeStepRange (
        baseRange.first + 1, baseRange.last + 1, maximumStep);
    normalisedStart = std::clamp (normalisedStart, 0.0f, 1.0f);
    normalisedLength = std::clamp (normalisedLength, 0.0f, 1.0f);
    const auto boundaryTravel = std::max (1, maximumStep - 2);
    const auto requestedStart = 1 + static_cast<int> (std::lround (
        normalisedStart * static_cast<float> (boundaryTravel)));
    const auto requestedLength = 2 + static_cast<int> (std::lround (
        normalisedLength * static_cast<float> (boundaryTravel)));
    return makeLengthStepRange (
        startAssigned ? requestedStart : baseRange.first + 1,
        lengthAssigned ? requestedLength : baseRange.length(),
        maximumStep);
}

enum class ModulationTarget
{
    none = 0,
    gateLevel = 1,
    gateDepth = 2,
    shortGateLength = 3,
    longGateLength = 4,
    noiseGateThreshold = 5,
    noiseGateAttack = 6,
    noiseGateHold = 7,
    noiseGateRelease = 8,
    noiseGateRange = 9,
    delayTime = 10,
    delayFeedback = 11,
    delayMix = 12,
    reverbSize = 13,
    reverbDamping = 14,
    reverbWidth = 15,
    reverbMix = 16,
    panPosition = 17,
    filterCutoff = 18,
    filterResonance = 19,
    filterMix = 20,
    pitchShift = 21,
    pitchMix = 22,
    distortionDrive = 23,
    distortionTone = 24,
    distortionMix = 25,
    grainSize = 26,
    grainShift = 27,
    grainFeedback = 28,
    grainMix = 29,
    compressorThreshold = 30,
    compressorRatio = 31,
    compressorAttack = 32,
    compressorRelease = 33,
    compressorMakeup = 34,
    compressorMix = 35,
    gateSequencerAAttack = 36,
    gateSequencerARelease = 37,
    gateSequencerBAttack = 38,
    gateSequencerBRelease = 39,
    delaySequencerAAttack = 40,
    delaySequencerARelease = 41,
    delaySequencerBAttack = 42,
    delaySequencerBRelease = 43,
    reverbSequencerAAttack = 44,
    reverbSequencerARelease = 45,
    reverbSequencerBAttack = 46,
    reverbSequencerBRelease = 47,
    panSequencerAAttack = 48,
    panSequencerARelease = 49,
    panSequencerBAttack = 50,
    panSequencerBRelease = 51,
    filterSequencerAAttack = 52,
    filterSequencerARelease = 53,
    filterSequencerBAttack = 54,
    filterSequencerBRelease = 55,
    pitchSequencerAAttack = 56,
    pitchSequencerARelease = 57,
    pitchSequencerBAttack = 58,
    pitchSequencerBRelease = 59,
    distortionSequencerAAttack = 60,
    distortionSequencerARelease = 61,
    distortionSequencerBAttack = 62,
    distortionSequencerBRelease = 63,
    grainSequencerAAttack = 64,
    grainSequencerARelease = 65,
    grainSequencerBAttack = 66,
    grainSequencerBRelease = 67,
    compressorSequencerAAttack = 68,
    compressorSequencerARelease = 69,
    compressorSequencerBAttack = 70,
    compressorSequencerBRelease = 71,
    gateSequencerStart = 72,
    gateSequencerEnd = 73,
    gateSequencerLength = 74,
    delaySequencerStart = 75,
    delaySequencerEnd = 76,
    delaySequencerLength = 77,
    reverbSequencerStart = 78,
    reverbSequencerEnd = 79,
    reverbSequencerLength = 80,
    panSequencerStart = 81,
    panSequencerEnd = 82,
    panSequencerLength = 83,
    filterSequencerStart = 84,
    filterSequencerEnd = 85,
    filterSequencerLength = 86,
    pitchSequencerStart = 87,
    pitchSequencerEnd = 88,
    pitchSequencerLength = 89,
    distortionSequencerStart = 90,
    distortionSequencerEnd = 91,
    distortionSequencerLength = 92,
    grainSequencerStart = 93,
    grainSequencerEnd = 94,
    grainSequencerLength = 95,
    compressorSequencerStart = 96,
    compressorSequencerEnd = 97,
    compressorSequencerLength = 98
};

constexpr int sequencerEnvelopeTargetCount = 4;
constexpr int sequencerRangeTargetCount = 3;
constexpr int gateModulationTargetCount = 16;
constexpr int delayModulationTargetCount = 10;
constexpr int reverbModulationTargetCount = 11;
constexpr int panModulationTargetCount = 8;
constexpr int filterModulationTargetCount = 10;
constexpr int pitchModulationTargetCount = 9;
constexpr int distortionModulationTargetCount = 10;
constexpr int grainModulationTargetCount = 11;
constexpr int compressorModulationTargetCount = 13;
constexpr int modulationTargetCount = 98;

inline bool isSequencerEnvelopeTarget (ModulationTarget target) noexcept
{
    const auto value = static_cast<int> (target);
    return value >= static_cast<int> (ModulationTarget::gateSequencerAAttack)
        && value <= static_cast<int> (
            ModulationTarget::compressorSequencerBRelease);
}

inline SequencerEngine sequencerEnvelopeTargetEngine (
    ModulationTarget target) noexcept
{
    if (! isSequencerEnvelopeTarget (target))
        return SequencerEngine::phi;

    const auto group = (static_cast<int> (target)
        - static_cast<int> (ModulationTarget::gateSequencerAAttack))
        / sequencerEnvelopeTargetCount;
    switch (group)
    {
        case 0:  return SequencerEngine::gate;
        case 1:  return SequencerEngine::delay;
        case 2:  return SequencerEngine::reverb;
        case 3:  return SequencerEngine::pan;
        case 4:  return SequencerEngine::filter;
        case 5:  return SequencerEngine::pitch;
        case 6:  return SequencerEngine::distortion;
        case 7:  return SequencerEngine::grain;
        case 8:  return SequencerEngine::compressor;
        default: return SequencerEngine::phi;
    }
}

inline int sequencerEnvelopeTargetBank (ModulationTarget target) noexcept
{
    if (! isSequencerEnvelopeTarget (target))
        return -1;
    const auto offset = (static_cast<int> (target)
        - static_cast<int> (ModulationTarget::gateSequencerAAttack))
        % sequencerEnvelopeTargetCount;
    return offset >= 2 ? 1 : 0;
}

inline bool sequencerEnvelopeTargetIsAttack (
    ModulationTarget target) noexcept
{
    if (! isSequencerEnvelopeTarget (target))
        return false;
    const auto offset = (static_cast<int> (target)
        - static_cast<int> (ModulationTarget::gateSequencerAAttack))
        % sequencerEnvelopeTargetCount;
    return offset == 0 || offset == 2;
}

inline std::array<ModulationTarget, sequencerEnvelopeTargetCount>
sequencerEnvelopeTargets (SequencerEngine engine) noexcept
{
    auto first = static_cast<int> (ModulationTarget::none);
    switch (engine)
    {
        case SequencerEngine::gate:
            first = static_cast<int> (ModulationTarget::gateSequencerAAttack);
            break;
        case SequencerEngine::delay:
            first = static_cast<int> (ModulationTarget::delaySequencerAAttack);
            break;
        case SequencerEngine::reverb:
            first = static_cast<int> (ModulationTarget::reverbSequencerAAttack);
            break;
        case SequencerEngine::pan:
            first = static_cast<int> (ModulationTarget::panSequencerAAttack);
            break;
        case SequencerEngine::filter:
            first = static_cast<int> (ModulationTarget::filterSequencerAAttack);
            break;
        case SequencerEngine::pitch:
            first = static_cast<int> (ModulationTarget::pitchSequencerAAttack);
            break;
        case SequencerEngine::distortion:
            first = static_cast<int> (
                ModulationTarget::distortionSequencerAAttack);
            break;
        case SequencerEngine::grain:
            first = static_cast<int> (ModulationTarget::grainSequencerAAttack);
            break;
        case SequencerEngine::compressor:
            first = static_cast<int> (
                ModulationTarget::compressorSequencerAAttack);
            break;
        case SequencerEngine::phi:
            break;
    }

    if (first == static_cast<int> (ModulationTarget::none))
        return { ModulationTarget::none, ModulationTarget::none,
                 ModulationTarget::none, ModulationTarget::none };
    return { static_cast<ModulationTarget> (first),
             static_cast<ModulationTarget> (first + 1),
             static_cast<ModulationTarget> (first + 2),
             static_cast<ModulationTarget> (first + 3) };
}

inline bool isSequencerRangeTarget (ModulationTarget target) noexcept
{
    const auto value = static_cast<int> (target);
    return value >= static_cast<int> (ModulationTarget::gateSequencerStart)
        && value <= static_cast<int> (
            ModulationTarget::compressorSequencerLength);
}

inline SequencerEngine sequencerRangeTargetEngine (
    ModulationTarget target) noexcept
{
    if (! isSequencerRangeTarget (target))
        return SequencerEngine::phi;

    const auto group = (static_cast<int> (target)
        - static_cast<int> (ModulationTarget::gateSequencerStart))
        / sequencerRangeTargetCount;
    switch (group)
    {
        case 0:  return SequencerEngine::gate;
        case 1:  return SequencerEngine::delay;
        case 2:  return SequencerEngine::reverb;
        case 3:  return SequencerEngine::pan;
        case 4:  return SequencerEngine::filter;
        case 5:  return SequencerEngine::pitch;
        case 6:  return SequencerEngine::distortion;
        case 7:  return SequencerEngine::grain;
        case 8:  return SequencerEngine::compressor;
        default: return SequencerEngine::phi;
    }
}

inline bool sequencerRangeTargetIsStart (ModulationTarget target) noexcept
{
    return isSequencerRangeTarget (target)
        && (static_cast<int> (target)
            - static_cast<int> (ModulationTarget::gateSequencerStart))
               % sequencerRangeTargetCount == 0;
}

inline bool sequencerRangeTargetIsEnd (ModulationTarget target) noexcept
{
    return isSequencerRangeTarget (target)
        && (static_cast<int> (target)
            - static_cast<int> (ModulationTarget::gateSequencerStart))
               % sequencerRangeTargetCount == 1;
}

inline bool sequencerRangeTargetIsLength (ModulationTarget target) noexcept
{
    return isSequencerRangeTarget (target)
        && (static_cast<int> (target)
            - static_cast<int> (ModulationTarget::gateSequencerStart))
               % sequencerRangeTargetCount == 2;
}

inline std::array<ModulationTarget, sequencerRangeTargetCount>
sequencerRangeTargets (SequencerEngine engine) noexcept
{
    auto first = static_cast<int> (ModulationTarget::none);
    switch (engine)
    {
        case SequencerEngine::gate:
            first = static_cast<int> (ModulationTarget::gateSequencerStart);
            break;
        case SequencerEngine::delay:
            first = static_cast<int> (ModulationTarget::delaySequencerStart);
            break;
        case SequencerEngine::reverb:
            first = static_cast<int> (ModulationTarget::reverbSequencerStart);
            break;
        case SequencerEngine::pan:
            first = static_cast<int> (ModulationTarget::panSequencerStart);
            break;
        case SequencerEngine::filter:
            first = static_cast<int> (ModulationTarget::filterSequencerStart);
            break;
        case SequencerEngine::pitch:
            first = static_cast<int> (ModulationTarget::pitchSequencerStart);
            break;
        case SequencerEngine::distortion:
            first = static_cast<int> (
                ModulationTarget::distortionSequencerStart);
            break;
        case SequencerEngine::grain:
            first = static_cast<int> (ModulationTarget::grainSequencerStart);
            break;
        case SequencerEngine::compressor:
            first = static_cast<int> (
                ModulationTarget::compressorSequencerStart);
            break;
        case SequencerEngine::phi:
            break;
    }

    if (first == static_cast<int> (ModulationTarget::none))
        return { ModulationTarget::none, ModulationTarget::none,
                 ModulationTarget::none };
    return { static_cast<ModulationTarget> (first),
             static_cast<ModulationTarget> (first + 1),
             static_cast<ModulationTarget> (first + 2) };
}

inline ModulationTarget targetFromChoice (float choice) noexcept
{
    const auto target = std::lround (choice);
    return target >= static_cast<int> (ModulationTarget::gateLevel)
            && target <= static_cast<int> (
                ModulationTarget::compressorSequencerLength)
        ? static_cast<ModulationTarget> (target)
        : ModulationTarget::none;
}

inline double wrapPhase (double phase, int length) noexcept
{
    const auto safeLength = static_cast<double> (std::max (1, length));
    phase = std::fmod (phase, safeLength);
    return phase < 0.0 ? phase + safeLength : phase;
}

inline int sequenceCycleLength (int stepCount, SequenceMode mode) noexcept
{
    stepCount = std::max (1, stepCount);
    if (mode == SequenceMode::random)
        return stepCount * 8;
    return mode == SequenceMode::bounce && stepCount > 1
        ? 2 * (stepCount - 1) : stepCount;
}

inline unsigned randomDirectionHash (unsigned value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

inline int greatestCommonDivisor (int left, int right) noexcept
{
    while (right != 0)
    {
        const auto remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

inline int randomDirectionOffset (int cycleTick, int stepCount) noexcept
{
    if (stepCount <= 1)
        return 0;

    const auto shuffle = cycleTick / stepCount;
    const auto position = cycleTick % stepCount;
    const auto seed = randomDirectionHash (
        static_cast<unsigned> (stepCount * 131 + shuffle * 977));
    auto stride = 1 + static_cast<int> (seed
        % static_cast<unsigned> (stepCount - 1));
    while (greatestCommonDivisor (stride, stepCount) != 1)
        stride = stride % (stepCount - 1) + 1;

    const auto baseOffset = static_cast<int> (
        randomDirectionHash (static_cast<unsigned> (stepCount)
                             ^ 0x9e3779b9U)
        % static_cast<unsigned> (stepCount));
    const auto offset = (baseOffset + (shuffle & 1)) % stepCount;
    return (offset + position * stride) % stepCount;
}

struct SequencePosition
{
    int currentOffset = 0;
    int previousOffset = 0;
    float fraction = 0.0f;
};

inline SequencePosition sequencePositionForPhase (
    double phase, int stepCount, SequenceMode mode) noexcept
{
    stepCount = std::max (1, stepCount);
    const auto cycleLength = sequenceCycleLength (stepCount, mode);
    const auto wrapped = wrapPhase (phase, cycleLength);
    const auto tick = std::clamp (
        static_cast<int> (std::floor (wrapped)), 0, cycleLength - 1);

    const auto offsetAt = [stepCount, cycleLength, mode] (int cycleTick)
    {
        cycleTick = (cycleTick % cycleLength + cycleLength) % cycleLength;
        if (mode == SequenceMode::reverse)
            return stepCount - 1 - cycleTick;
        if (mode == SequenceMode::bounce && cycleTick >= stepCount)
            return cycleLength - cycleTick;
        if (mode == SequenceMode::random)
            return randomDirectionOffset (cycleTick, stepCount);
        return cycleTick;
    };

    return { offsetAt (tick),
             offsetAt (tick - 1),
             static_cast<float> (wrapped - std::floor (wrapped)) };
}

inline float transitionValue (float previous,
                              float current,
                              float positionWithinStep,
                              float attackFraction,
                              float releaseFraction) noexcept
{
    previous = std::clamp (previous, 0.0f, 1.0f);
    current = std::clamp (current, 0.0f, 1.0f);
    positionWithinStep = std::clamp (positionWithinStep, 0.0f, 1.0f);

    // Equal adjacent steps form one continuous held level. The small tolerance
    // also prevents imperceptible drawing-rounding differences from creating
    // a fresh transition at an otherwise flat boundary.
    if (std::abs (current - previous) <= 0.0001f)
        return current;

    const auto transitionFraction = std::clamp (
        current >= previous ? attackFraction : releaseFraction, 0.0f, 1.0f);

    if (transitionFraction <= 0.000001f)
        return current;

    const auto progress = std::clamp (
        positionWithinStep / transitionFraction, 0.0f, 1.0f);
    const auto easedProgress = 1.0f - (1.0f - progress) * (1.0f - progress);
    return previous + easedProgress * (current - previous);
}

inline float evaluateGateStep (float previousValue,
                               float currentValue,
                               GateStepMode previousMode,
                               GateStepMode currentMode,
                               float positionWithinStep,
                               float attackFraction,
                               float releaseFraction,
                               float shortOpenFraction = shortGateOpenFraction,
                               float longOpenFraction = longGateOpenFraction) noexcept
{
    previousValue = std::clamp (previousValue, 0.0f, 1.0f);
    currentValue = std::clamp (currentValue, 0.0f, 1.0f);
    positionWithinStep = std::clamp (positionWithinStep, 0.0f, 1.0f);
    attackFraction = std::clamp (attackFraction, 0.0f, 1.0f);
    releaseFraction = std::clamp (releaseFraction, 0.0f, 1.0f);
    shortOpenFraction = std::clamp (shortOpenFraction, 0.10f, 0.60f);
    longOpenFraction = std::clamp (longOpenFraction, 0.65f, 0.95f);

    if (currentMode == GateStepMode::off || currentValue <= 0.000001f)
        return 0.0f;

    const auto openFraction = currentMode == GateStepMode::shortStep
        ? shortOpenFraction
        : currentMode == GateStepMode::longStep
            ? longOpenFraction : 1.0f;
    const auto startValue = previousMode == GateStepMode::linkStep
        ? previousValue : 0.0f;
    const auto openingTransition = std::clamp (
        (currentValue >= startValue ? attackFraction : releaseFraction)
            * openFraction,
        0.0f, openFraction);
    const auto openValue = transitionValue (
        startValue, currentValue, positionWithinStep,
        openingTransition, openingTransition);

    if (currentMode == GateStepMode::linkStep
        || positionWithinStep < openFraction)
        return openValue;

    const auto closingDuration = releaseFraction * (1.0f - openFraction);
    if (closingDuration <= 0.000001f)
        return 0.0f;

    const auto closingPosition = std::clamp (
        (positionWithinStep - openFraction) / closingDuration, 0.0f, 1.0f);
    return transitionValue (
        currentValue, 0.0f, closingPosition, 0.0f, 1.0f);
}

inline float evaluateBank (const Pattern& pattern,
                           double phase,
                           float attackFraction,
                           float releaseFraction,
                           int* activeStep = nullptr) noexcept
{
    const auto wrapped = wrapPhase (phase, stepsPerBank);
    const auto step = std::clamp (
        static_cast<int> (std::floor (wrapped)), 0, stepsPerBank - 1);
    const auto previousStep = (step + stepsPerBank - 1) % stepsPerBank;
    const auto position = static_cast<float> (wrapped - std::floor (wrapped));

    if (activeStep != nullptr)
        *activeStep = step;

    return transitionValue (pattern[static_cast<std::size_t> (previousStep)],
                            pattern[static_cast<std::size_t> (step)],
                            position,
                            attackFraction,
                            releaseFraction);
}

inline float evaluateBankRange (const Pattern& pattern,
                                double phase,
                                StepRange range,
                                float attackFraction,
                                float releaseFraction,
                                int* activeStep = nullptr,
                                SequenceMode mode = SequenceMode::loop) noexcept
{
    range.first = std::clamp (range.first, 0, stepsPerBank - 1);
    range.last = std::clamp (range.last, range.first, stepsPerBank - 1);
    const auto position = sequencePositionForPhase (
        phase, range.length(), mode);
    const auto step = range.first + position.currentOffset;
    const auto previousStep = range.first + position.previousOffset;

    if (activeStep != nullptr)
        *activeStep = step;

    return transitionValue (pattern[static_cast<std::size_t> (previousStep)],
                            pattern[static_cast<std::size_t> (step)],
                            position.fraction,
                            attackFraction,
                            releaseFraction);
}

inline float evaluateGateBankRange (const Pattern& pattern,
                                    const GateModePattern& modes,
                                    double phase,
                                    StepRange range,
                                    float attackFraction,
                                    float releaseFraction,
                                    int* activeStep = nullptr,
                                    float shortOpenFraction = shortGateOpenFraction,
                                    float longOpenFraction = longGateOpenFraction,
                                    SequenceMode mode = SequenceMode::loop) noexcept
{
    range.first = std::clamp (range.first, 0, stepsPerBank - 1);
    range.last = std::clamp (range.last, range.first, stepsPerBank - 1);
    const auto position = sequencePositionForPhase (
        phase, range.length(), mode);
    const auto step = range.first + position.currentOffset;
    const auto previousStep = range.first + position.previousOffset;

    if (activeStep != nullptr)
        *activeStep = step;

    return evaluateGateStep (
        pattern[static_cast<std::size_t> (previousStep)],
        pattern[static_cast<std::size_t> (step)],
        modes[static_cast<std::size_t> (previousStep)],
        modes[static_cast<std::size_t> (step)],
        position.fraction, attackFraction, releaseFraction,
        shortOpenFraction, longOpenFraction);
}

inline float evaluateLinked (const Pattern& a,
                             const Pattern& b,
                             double phase,
                             float attackA,
                             float releaseA,
                             float attackB,
                             float releaseB,
                             int* activeBank,
                             int* activeStep) noexcept
{
    const auto wrapped = wrapPhase (phase, linkedStepCount);
    const auto globalStep = std::clamp (
        static_cast<int> (std::floor (wrapped)), 0, linkedStepCount - 1);
    const auto previousGlobalStep = (globalStep + linkedStepCount - 1)
                                  % linkedStepCount;

    const auto valueAt = [&] (int index)
    {
        return index < stepsPerBank
            ? a[static_cast<std::size_t> (index)]
            : b[static_cast<std::size_t> (index - stepsPerBank)];
    };

    const auto bank = globalStep < stepsPerBank ? 0 : 1;
    const auto localStep = globalStep % stepsPerBank;
    const auto position = static_cast<float> (wrapped - std::floor (wrapped));

    if (activeBank != nullptr)
        *activeBank = bank;
    if (activeStep != nullptr)
        *activeStep = localStep;

    return transitionValue (valueAt (previousGlobalStep),
                            valueAt (globalStep),
                            position,
                            bank == 0 ? attackA : attackB,
                            bank == 0 ? releaseA : releaseB);
}

inline float evaluateLinkedRange (const Pattern& a,
                                  const Pattern& b,
                                  double phase,
                                  StepRange range,
                                  float attackA,
                                  float releaseA,
                                  float attackB,
                                  float releaseB,
                                  int* activeBank,
                                  int* activeStep,
                                  SequenceMode mode = SequenceMode::loop) noexcept
{
    range.first = std::clamp (range.first, 0, linkedStepCount - 1);
    range.last = std::clamp (range.last, range.first, linkedStepCount - 1);
    const auto position = sequencePositionForPhase (
        phase, range.length(), mode);
    const auto globalStep = range.first + position.currentOffset;
    const auto previousGlobalStep = range.first + position.previousOffset;

    const auto valueAt = [&] (int index)
    {
        return index < stepsPerBank
            ? a[static_cast<std::size_t> (index)]
            : b[static_cast<std::size_t> (index - stepsPerBank)];
    };

    const auto bank = globalStep < stepsPerBank ? 0 : 1;
    const auto localStep = globalStep % stepsPerBank;

    if (activeBank != nullptr)
        *activeBank = bank;
    if (activeStep != nullptr)
        *activeStep = localStep;

    return transitionValue (valueAt (previousGlobalStep),
                            valueAt (globalStep),
                            position.fraction,
                            bank == 0 ? attackA : attackB,
                            bank == 0 ? releaseA : releaseB);
}

inline float evaluateGateLinkedRange (const Pattern& a,
                                      const Pattern& b,
                                      const GateModePattern& modesA,
                                      const GateModePattern& modesB,
                                      double phase,
                                      StepRange range,
                                      float attackA,
                                      float releaseA,
                                      float attackB,
                                      float releaseB,
                                      int* activeBank,
                                      int* activeStep,
                                      float shortOpenFraction = shortGateOpenFraction,
                                      float longOpenFraction = longGateOpenFraction,
                                      SequenceMode mode = SequenceMode::loop) noexcept
{
    range.first = std::clamp (range.first, 0, linkedStepCount - 1);
    range.last = std::clamp (range.last, range.first, linkedStepCount - 1);
    const auto position = sequencePositionForPhase (
        phase, range.length(), mode);
    const auto globalStep = range.first + position.currentOffset;
    const auto previousGlobalStep = range.first + position.previousOffset;

    const auto valueAt = [&] (int index)
    {
        return index < stepsPerBank
            ? a[static_cast<std::size_t> (index)]
            : b[static_cast<std::size_t> (index - stepsPerBank)];
    };
    const auto modeAt = [&] (int index)
    {
        return index < stepsPerBank
            ? modesA[static_cast<std::size_t> (index)]
            : modesB[static_cast<std::size_t> (index - stepsPerBank)];
    };

    const auto bank = globalStep < stepsPerBank ? 0 : 1;
    const auto localStep = globalStep % stepsPerBank;

    if (activeBank != nullptr)
        *activeBank = bank;
    if (activeStep != nullptr)
        *activeStep = localStep;

    return evaluateGateStep (
        valueAt (previousGlobalStep), valueAt (globalStep),
        modeAt (previousGlobalStep), modeAt (globalStep),
        position.fraction,
        bank == 0 ? attackA : attackB,
        bank == 0 ? releaseA : releaseB,
        shortOpenFraction, longOpenFraction);
}

inline double phaseFromQuarterNotes (double quarterNotes,
                                     double beatsPerStep,
                                     int length) noexcept
{
    if (! std::isfinite (quarterNotes) || beatsPerStep <= 0.0)
        return 0.0;

    return wrapPhase (quarterNotes / beatsPerStep, length);
}

inline double unwrappedPhaseFromQuarterNotes (double quarterNotes,
                                              double beatsPerStep) noexcept
{
    if (! std::isfinite (quarterNotes) || beatsPerStep <= 0.0)
        return 0.0;
    return quarterNotes / beatsPerStep;
}

inline bool hostTimelineShouldAdvance (bool hostReportsPlaying,
                                       bool ppqAvailable,
                                       bool ppqChanged,
                                       bool sampleTimeAvailable,
                                       bool sampleTimeChanged) noexcept
{
    if (hostReportsPlaying || ppqChanged || sampleTimeChanged)
        return true;

    // Some nested plug-in hosts omit every transport-position field and also
    // report isPlaying=false while their audio graph is running. In that
    // incomplete case, HOST SYNC falls back to tempo-synchronised free-run
    // instead of freezing. If the host supplies a stationary PPQ or sample
    // position, honour it as a genuine stopped transport.
    return ! ppqAvailable && ! sampleTimeAvailable;
}

inline bool stalledPhiTimelineShouldFreeRun (bool phiHostPresent,
                                             bool hostReportsPlaying,
                                             bool ppqAvailable,
                                             bool ppqChanged,
                                             bool sampleTimeAvailable,
                                             bool sampleTimeChanged) noexcept
{
    return phiHostPresent
        && ! hostReportsPlaying
        && (ppqAvailable || sampleTimeAvailable)
        && ! ppqChanged
        && ! sampleTimeChanged;
}

inline float applyGateDepth (float baseLevel,
                             float patternValue,
                             float depth) noexcept
{
    baseLevel = std::clamp (baseLevel, 0.0f, 1.0f);
    patternValue = std::clamp (patternValue, 0.0f, 1.0f);
    depth = std::clamp (depth, 0.0f, 1.0f);
    return baseLevel * ((1.0f - depth) + depth * patternValue);
}

inline float modulatedCeiling (float baseValue,
                               float sequenceValue) noexcept
{
    return std::clamp (baseValue, 0.0f, 1.0f)
         * std::clamp (sequenceValue, 0.0f, 1.0f);
}

inline float modulatedRangeFromMinimum (float minimumValue,
                                        float baseValue,
                                        float sequenceValue) noexcept
{
    minimumValue = std::clamp (minimumValue, 0.0f, 1.0f);
    baseValue = std::clamp (baseValue, minimumValue, 1.0f);
    return minimumValue
         + (baseValue - minimumValue)
             * std::clamp (sequenceValue, 0.0f, 1.0f);
}

inline float applyModulationDepth (float baseValue,
                                   float modulationDeviation,
                                   float depth) noexcept
{
    baseValue = std::clamp (baseValue, 0.0f, 1.0f);
    modulationDeviation = std::clamp (modulationDeviation, -1.0f, 1.0f);
    depth = std::clamp (depth, 0.0f, 1.0f);
    const auto availableRange = modulationDeviation >= 0.0f
        ? 1.0f - baseValue : baseValue;
    return std::clamp (baseValue
                           + modulationDeviation * availableRange * depth,
                       0.0f, 1.0f);
}

// Step parameters always store one canonical signed coordinate:
// 0.0 = -100%, 0.5 = 0%, 1.0 = +100%. Unipolar mode is only a view of that
// data, so hidden negative values survive polarity switches unchanged.
inline float canonicalFromUnipolar (float unipolarValue) noexcept
{
    return 0.5f + 0.5f * std::clamp (unipolarValue, 0.0f, 1.0f);
}

inline float unipolarFromCanonical (float canonicalValue) noexcept
{
    return std::clamp (2.0f * canonicalValue - 1.0f, 0.0f, 1.0f);
}

inline float bipolarDeviationFromCanonical (float canonicalValue) noexcept
{
    return std::clamp (2.0f * canonicalValue - 1.0f, -1.0f, 1.0f);
}

inline float displayFromCanonical (float canonicalValue,
                                   bool bipolar) noexcept
{
    canonicalValue = std::clamp (canonicalValue, 0.0f, 1.0f);
    return bipolar ? canonicalValue : unipolarFromCanonical (canonicalValue);
}

inline Pattern makeWaveformPreset (WaveformPreset preset,
                                   bool bipolar) noexcept
{
    Pattern result {};
    const auto presetIndex = static_cast<int> (preset);
    const auto doubled = (presetIndex % 2) != 0;
    const auto shape = presetIndex / 2;
    const auto period = doubled ? stepsPerBank / 2 : stepsPerBank;
    constexpr auto twoPi = 6.28318530717958647692f;

    for (int step = 0; step < stepsPerBank; ++step)
    {
        const auto localStep = step % period;
        const auto phase = static_cast<float> (localStep)
                         / static_cast<float> (period);
        auto displayed = 0.0f;

        switch (shape)
        {
            case 0:
                displayed = static_cast<float> (localStep)
                          / static_cast<float> (period - 1);
                break;
            case 1:
                displayed = 1.0f - static_cast<float> (localStep)
                                   / static_cast<float> (period - 1);
                break;
            case 2:
                displayed = 0.5f + 0.5f * std::sin (twoPi * phase);
                break;
            case 3:
                displayed = 1.0f - std::abs (2.0f * phase - 1.0f);
                break;
            case 4:
                displayed = phase < 0.25f ? 1.0f : 0.0f;
                break;
            case 5:
                displayed = phase < 0.50f ? 1.0f : 0.0f;
                break;
            case 6:
                displayed = phase < 0.75f ? 1.0f : 0.0f;
                break;
            default:
                break;
        }

        result[static_cast<std::size_t> (step)] = bipolar
            ? std::clamp (displayed, 0.0f, 1.0f)
            : canonicalFromUnipolar (displayed);
    }

    return result;
}

inline bool targetSupportsBipolar (ModulationTarget target) noexcept
{
    if (isSequencerEnvelopeTarget (target)
        || isSequencerRangeTarget (target))
        return true;

    switch (target)
    {
        case ModulationTarget::noiseGateThreshold:
        case ModulationTarget::noiseGateAttack:
        case ModulationTarget::noiseGateHold:
        case ModulationTarget::noiseGateRelease:
        case ModulationTarget::noiseGateRange:
        case ModulationTarget::delayTime:
        case ModulationTarget::delayFeedback:
        case ModulationTarget::delayMix:
        case ModulationTarget::reverbSize:
        case ModulationTarget::reverbDamping:
        case ModulationTarget::reverbWidth:
        case ModulationTarget::reverbMix:
        case ModulationTarget::panPosition:
        case ModulationTarget::filterCutoff:
        case ModulationTarget::filterResonance:
        case ModulationTarget::filterMix:
        case ModulationTarget::pitchShift:
        case ModulationTarget::pitchMix:
        case ModulationTarget::distortionDrive:
        case ModulationTarget::distortionTone:
        case ModulationTarget::distortionMix:
        case ModulationTarget::grainSize:
        case ModulationTarget::grainShift:
        case ModulationTarget::grainFeedback:
        case ModulationTarget::grainMix:
        case ModulationTarget::compressorThreshold:
        case ModulationTarget::compressorRatio:
        case ModulationTarget::compressorAttack:
        case ModulationTarget::compressorRelease:
        case ModulationTarget::compressorMakeup:
        case ModulationTarget::compressorMix:
            return true;
        case ModulationTarget::none:
        case ModulationTarget::gateLevel:
        case ModulationTarget::gateDepth:
        case ModulationTarget::shortGateLength:
        case ModulationTarget::longGateLength:
            return false;
        default:
            return false;
    }
}

inline float targetValueFromCanonical (float canonicalValue,
                                       ModulationTarget target) noexcept
{
    return displayFromCanonical (canonicalValue,
                                 targetSupportsBipolar (target));
}

inline float combineParallelTarget (ModulationTarget requestedTarget,
                                    float valueA,
                                    bool enabledA,
                                    ModulationTarget targetA,
                                    float valueB,
                                    bool enabledB,
                                    ModulationTarget targetB,
                                    bool* wasAssigned = nullptr) noexcept
{
    auto combined = 1.0f;
    auto assigned = false;

    if (requestedTarget == ModulationTarget::none)
    {
        if (wasAssigned != nullptr)
            *wasAssigned = false;
        return combined;
    }

    if (enabledA && targetA == requestedTarget)
    {
        combined *= std::clamp (valueA, 0.0f, 1.0f);
        assigned = true;
    }

    if (enabledB && targetB == requestedTarget)
    {
        combined *= std::clamp (valueB, 0.0f, 1.0f);
        assigned = true;
    }

    if (wasAssigned != nullptr)
        *wasAssigned = assigned;

    return combined;
}

inline float combineParallelModulationDeviation (
    ModulationTarget requestedTarget,
    float valueA,
    bool enabledA,
    ModulationTarget targetA,
    bool bipolarA,
    float valueB,
    bool enabledB,
    ModulationTarget targetB,
    bool bipolarB,
    bool* wasAssigned = nullptr) noexcept
{
    auto unipolarProduct = 1.0f;
    auto bipolarSum = 0.0f;
    auto hasUnipolar = false;
    auto assigned = false;

    const auto addSource = [&] (float value,
                                bool enabled,
                                ModulationTarget target,
                                bool bipolar)
    {
        if (! enabled || requestedTarget == ModulationTarget::none
            || target != requestedTarget)
            return;

        value = std::clamp (value, 0.0f, 1.0f);
        if (bipolar)
            bipolarSum += 2.0f * value - 1.0f;
        else
        {
            unipolarProduct *= value;
            hasUnipolar = true;
        }
        assigned = true;
    };

    addSource (valueA, enabledA, targetA, bipolarA);
    addSource (valueB, enabledB, targetB, bipolarB);

    if (wasAssigned != nullptr)
        *wasAssigned = assigned;

    if (! assigned)
        return 0.0f;

    const auto unipolarDeviation = hasUnipolar ? unipolarProduct - 1.0f : 0.0f;
    return std::clamp (unipolarDeviation + bipolarSum, -1.0f, 1.0f);
}
}
