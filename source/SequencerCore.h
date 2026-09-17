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
    played = 3
};

inline SequenceMode sequenceModeFromChoice (float choice) noexcept
{
    const auto index = std::clamp (
        static_cast<int> (std::lround (choice)),
        static_cast<int> (SequenceMode::loop),
        static_cast<int> (SequenceMode::played));
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

enum class ModulationTarget
{
    none = 0,
    gateLevel = 1,
    gateDepth = 2,
    shortGateLength = 3,
    longGateLength = 4
};

constexpr int gateModulationTargetCount = 4;

inline ModulationTarget targetFromChoice (float choice) noexcept
{
    const auto target = std::lround (choice);
    return target >= static_cast<int> (ModulationTarget::gateLevel)
            && target <= static_cast<int> (ModulationTarget::longGateLength)
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
    return mode == SequenceMode::bounce && stepCount > 1
        ? 2 * (stepCount - 1) : stepCount;
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

inline bool targetSupportsBipolar (ModulationTarget) noexcept
{
    // Gate Level is currently the only destination, and volume has no
    // meaningful negative range. Future Pan/Pitch targets can opt in here.
    return false;
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
