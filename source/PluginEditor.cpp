#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
constexpr auto background = 0xff101419;
constexpr auto panel = 0xff1b222a;
constexpr auto panelOutline = 0xff34414d;
constexpr auto text = 0xffedf4f7;
constexpr auto mutedText = 0xff9aabb7;
constexpr auto colourA = 0xff34d6c6;
constexpr auto colourB = 0xffff9d4d;
constexpr auto globalAccent = 0xff8aa3b5;
constexpr auto gateAccent = 0xff62cf8a;
constexpr auto delayAccent = 0xffc58aff;
constexpr auto reverbAccent = 0xffff6f91;
constexpr auto panAccent = 0xffffc857;
constexpr auto filterAccent = 0xff50d8a8;
constexpr auto pitchAccent = 0xff66d9ff;
constexpr auto distortionAccent = 0xffff684f;
constexpr auto grainAccent = 0xffa8e063;
constexpr auto compressorAccent = 0xff8f9cff;
constexpr auto phiAccent = 0xff68a8ff;
constexpr auto targetDragPrefix = "seqwencer-target:";
constexpr int designWidth = 1280;
constexpr int designHeight = 680;

struct StoredEditorSettings
{
    juce::Point<int> size { designWidth, designHeight };
    float hueA = juce::Colour (colourA).getHue();
    float hueB = juce::Colour (colourB).getHue();
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::gateModulationTargetCount> gateTargets {
    seqwencer::ModulationTarget::gateLevel,
    seqwencer::ModulationTarget::gateDepth,
    seqwencer::ModulationTarget::shortGateLength,
    seqwencer::ModulationTarget::longGateLength,
    seqwencer::ModulationTarget::noiseGateThreshold,
    seqwencer::ModulationTarget::noiseGateAttack,
    seqwencer::ModulationTarget::noiseGateHold,
    seqwencer::ModulationTarget::noiseGateRelease,
    seqwencer::ModulationTarget::noiseGateRange,
    seqwencer::ModulationTarget::gateSequencerAAttack,
    seqwencer::ModulationTarget::gateSequencerARelease,
    seqwencer::ModulationTarget::gateSequencerBAttack,
    seqwencer::ModulationTarget::gateSequencerBRelease,
    seqwencer::ModulationTarget::gateSequencerStart,
    seqwencer::ModulationTarget::gateSequencerEnd,
    seqwencer::ModulationTarget::gateSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::delayModulationTargetCount> delayTargets {
    seqwencer::ModulationTarget::delayTime,
    seqwencer::ModulationTarget::delayFeedback,
    seqwencer::ModulationTarget::delayMix,
    seqwencer::ModulationTarget::delaySequencerAAttack,
    seqwencer::ModulationTarget::delaySequencerARelease,
    seqwencer::ModulationTarget::delaySequencerBAttack,
    seqwencer::ModulationTarget::delaySequencerBRelease,
    seqwencer::ModulationTarget::delaySequencerStart,
    seqwencer::ModulationTarget::delaySequencerEnd,
    seqwencer::ModulationTarget::delaySequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::reverbModulationTargetCount> reverbTargets {
    seqwencer::ModulationTarget::reverbSize,
    seqwencer::ModulationTarget::reverbDamping,
    seqwencer::ModulationTarget::reverbWidth,
    seqwencer::ModulationTarget::reverbMix,
    seqwencer::ModulationTarget::reverbSequencerAAttack,
    seqwencer::ModulationTarget::reverbSequencerARelease,
    seqwencer::ModulationTarget::reverbSequencerBAttack,
    seqwencer::ModulationTarget::reverbSequencerBRelease,
    seqwencer::ModulationTarget::reverbSequencerStart,
    seqwencer::ModulationTarget::reverbSequencerEnd,
    seqwencer::ModulationTarget::reverbSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::panModulationTargetCount> panTargets {
    seqwencer::ModulationTarget::panPosition,
    seqwencer::ModulationTarget::panSequencerAAttack,
    seqwencer::ModulationTarget::panSequencerARelease,
    seqwencer::ModulationTarget::panSequencerBAttack,
    seqwencer::ModulationTarget::panSequencerBRelease,
    seqwencer::ModulationTarget::panSequencerStart,
    seqwencer::ModulationTarget::panSequencerEnd,
    seqwencer::ModulationTarget::panSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::filterModulationTargetCount> filterTargets {
    seqwencer::ModulationTarget::filterCutoff,
    seqwencer::ModulationTarget::filterResonance,
    seqwencer::ModulationTarget::filterMix,
    seqwencer::ModulationTarget::filterSequencerAAttack,
    seqwencer::ModulationTarget::filterSequencerARelease,
    seqwencer::ModulationTarget::filterSequencerBAttack,
    seqwencer::ModulationTarget::filterSequencerBRelease,
    seqwencer::ModulationTarget::filterSequencerStart,
    seqwencer::ModulationTarget::filterSequencerEnd,
    seqwencer::ModulationTarget::filterSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::pitchModulationTargetCount> pitchTargets {
    seqwencer::ModulationTarget::pitchShift,
    seqwencer::ModulationTarget::pitchMix,
    seqwencer::ModulationTarget::pitchSequencerAAttack,
    seqwencer::ModulationTarget::pitchSequencerARelease,
    seqwencer::ModulationTarget::pitchSequencerBAttack,
    seqwencer::ModulationTarget::pitchSequencerBRelease,
    seqwencer::ModulationTarget::pitchSequencerStart,
    seqwencer::ModulationTarget::pitchSequencerEnd,
    seqwencer::ModulationTarget::pitchSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::distortionModulationTargetCount> distortionTargets {
    seqwencer::ModulationTarget::distortionDrive,
    seqwencer::ModulationTarget::distortionTone,
    seqwencer::ModulationTarget::distortionMix,
    seqwencer::ModulationTarget::distortionSequencerAAttack,
    seqwencer::ModulationTarget::distortionSequencerARelease,
    seqwencer::ModulationTarget::distortionSequencerBAttack,
    seqwencer::ModulationTarget::distortionSequencerBRelease,
    seqwencer::ModulationTarget::distortionSequencerStart,
    seqwencer::ModulationTarget::distortionSequencerEnd,
    seqwencer::ModulationTarget::distortionSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::grainModulationTargetCount> grainTargets {
    seqwencer::ModulationTarget::grainSize,
    seqwencer::ModulationTarget::grainShift,
    seqwencer::ModulationTarget::grainFeedback,
    seqwencer::ModulationTarget::grainMix,
    seqwencer::ModulationTarget::grainSequencerAAttack,
    seqwencer::ModulationTarget::grainSequencerARelease,
    seqwencer::ModulationTarget::grainSequencerBAttack,
    seqwencer::ModulationTarget::grainSequencerBRelease,
    seqwencer::ModulationTarget::grainSequencerStart,
    seqwencer::ModulationTarget::grainSequencerEnd,
    seqwencer::ModulationTarget::grainSequencerLength
};

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::compressorModulationTargetCount>
    compressorTargets {
        seqwencer::ModulationTarget::compressorThreshold,
        seqwencer::ModulationTarget::compressorRatio,
        seqwencer::ModulationTarget::compressorAttack,
        seqwencer::ModulationTarget::compressorRelease,
        seqwencer::ModulationTarget::compressorMakeup,
        seqwencer::ModulationTarget::compressorMix,
        seqwencer::ModulationTarget::compressorSequencerAAttack,
        seqwencer::ModulationTarget::compressorSequencerARelease,
        seqwencer::ModulationTarget::compressorSequencerBAttack,
        seqwencer::ModulationTarget::compressorSequencerBRelease,
        seqwencer::ModulationTarget::compressorSequencerStart,
        seqwencer::ModulationTarget::compressorSequencerEnd,
        seqwencer::ModulationTarget::compressorSequencerLength
    };

juce::String targetDragID (seqwencer::ModulationTarget target)
{
    return juce::String (targetDragPrefix) + juce::String (static_cast<int> (target));
}

seqwencer::ModulationTarget targetFromDragID (const juce::String& dragID)
{
    if (! dragID.startsWith (targetDragPrefix))
        return seqwencer::ModulationTarget::none;
    return seqwencer::targetFromChoice (
        static_cast<float> (dragID.substring (
            static_cast<int> (juce::String (targetDragPrefix).length()))
                .getIntValue()));
}

StoredEditorSettings loadStoredEditorSettings()
{
    StoredEditorSettings settings;
    const auto file = SeqwencerAudioProcessor::getPortableDataDirectory()
                          .getChildFile ("Settings.ini");
    if (! file.existsAsFile())
        return settings;

    juce::StringArray lines;
    lines.addLines (file.loadFileAsString());
    auto inInterface = false;
    for (auto line : lines)
    {
        line = line.trim();
        if (line.startsWithChar ('['))
        {
            inInterface = line.equalsIgnoreCase ("[Interface]");
            continue;
        }
        if (! inInterface)
            continue;
        const auto separator = line.indexOfChar ('=');
        if (separator <= 0)
            continue;
        const auto key = line.substring (0, separator).trim();
        const auto valueText = line.substring (separator + 1).trim();
        if (key.equalsIgnoreCase ("Width"))
            settings.size.x = valueText.getIntValue();
        else if (key.equalsIgnoreCase ("Height"))
            settings.size.y = valueText.getIntValue();
        else if (key.equalsIgnoreCase ("SequencerAColour"))
            settings.hueA = juce::jlimit (0.0f, 1.0f,
                                          valueText.getFloatValue());
        else if (key.equalsIgnoreCase ("SequencerBColour"))
            settings.hueB = juce::jlimit (0.0f, 1.0f,
                                          valueText.getFloatValue());
    }
    settings.size.x = juce::jlimit (960, 1600, settings.size.x);
    settings.size.y = static_cast<int> (std::lround (
        static_cast<double> (settings.size.x) * designHeight / designWidth));
    return settings;
}

void saveStoredEditorSettings (int width, int height,
                               float hueA, float hueB)
{
    const auto dataDirectory = SeqwencerAudioProcessor::getPortableDataDirectory();
    if (dataDirectory.createDirectory().failed())
        return;
    juce::String contents { "; Seqwencer portable settings\r\n" };
    contents << "[Interface]\r\nWidth=" << width
             << "\r\nHeight=" << height
             << "\r\nSequencerAColour=" << juce::String (hueA, 6)
             << "\r\nSequencerBColour=" << juce::String (hueB, 6)
             << "\r\n";
    dataDirectory.getChildFile ("Settings.ini").replaceWithText (contents);
}

juce::Colour laneColourFromHue (int lane, float hue)
{
    const auto defaultColour = juce::Colour (lane == 0 ? colourA : colourB);
    return juce::Colour::fromHSV (
        juce::jlimit (0.0f, 1.0f, hue),
        defaultColour.getSaturation(), defaultColour.getBrightness(), 1.0f);
}
}

class SeqwencerAudioProcessorEditor::SeqwencerLookAndFeel final
    : public juce::LookAndFeel_V4
{
public:
    SeqwencerLookAndFeel()
    {
        setColour (juce::Label::textColourId, juce::Colour (text));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff252e37));
        setColour (juce::ComboBox::textColourId, juce::Colour (text));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (panelOutline));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202831));
        setColour (juce::PopupMenu::textColourId, juce::Colour (text));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (text));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ToggleButton::textColourId, juce::Colour (text));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions { 11.0f });
        label->setMinimumHorizontalScale (1.0f);
        return label;
    }

    void drawRotarySlider (juce::Graphics& graphics,
                           int x, int y, int width, int height,
                           float sliderPosition,
                           float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        const auto diameter = static_cast<float> (juce::jmin (width, height)) - 10.0f;
        const auto area = juce::Rectangle<float> (
            static_cast<float> (x), static_cast<float> (y),
            static_cast<float> (width), static_cast<float> (height))
            .withSizeKeepingCentre (diameter, diameter);
        const auto angle = startAngle + sliderPosition * (endAngle - startAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

        graphics.setColour (juce::Colour (0xff0d1115));
        graphics.fillEllipse (area);
        graphics.setColour (juce::Colour (panelOutline));
        graphics.drawEllipse (area, 1.5f);

        juce::Path arc;
        arc.addCentredArc (area.getCentreX(), area.getCentreY(),
                           area.getWidth() * 0.42f, area.getHeight() * 0.42f,
                           0.0f, startAngle, angle, true);
        graphics.setColour (accent);
        graphics.strokePath (arc, juce::PathStrokeType (3.0f,
                             juce::PathStrokeType::curved,
                             juce::PathStrokeType::rounded));

        const auto centre = area.getCentre();
        const auto pointerLength = area.getWidth() * 0.31f;
        const auto pointer = juce::Point<float> (
            centre.x + std::sin (angle) * pointerLength,
            centre.y - std::cos (angle) * pointerLength);
        graphics.setColour (accent.brighter (0.35f));
        graphics.drawLine (juce::Line<float> { centre, pointer }, 2.0f);
    }

    void drawToggleButton (juce::Graphics& graphics,
                           juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const auto on = button.getToggleState();
        const auto accent = button.findColour (
            juce::ToggleButton::tickColourId, true);

        auto border = on ? accent.brighter (0.35f) : juce::Colour (panelOutline);
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            border = border.brighter (0.30f);

        const auto enabledAlpha = button.isEnabled() ? 1.0f : 0.38f;
        graphics.setColour (border.withMultipliedAlpha (enabledAlpha));
        graphics.drawRoundedRectangle (bounds, 5.0f, on ? 1.6f : 1.0f);

        graphics.setColour (juce::Colour (text).withMultipliedAlpha (enabledAlpha));
        graphics.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
        graphics.drawText (button.getButtonText(), button.getLocalBounds(),
                           juce::Justification::centred, false);
    }
};

class SeqwencerAudioProcessorEditor::FxSelectorButton final
    : public juce::Button
{
public:
    FxSelectorButton (const juce::String& name,
                      juce::Colour accentColour,
                      int audioStageIndex = -1)
        : juce::Button (name), accent (accentColour),
          ledButton (accentColour), stageIndex (audioStageIndex)
    {
        addAndMakeVisible (ledButton);
        setMouseCursor (stageIndex >= 0
            ? juce::MouseCursor::DraggingHandCursor
            : juce::MouseCursor::PointingHandCursor);
    }

    juce::Button& getEnableButton() noexcept { return ledButton; }

    void setSelected (bool shouldBeSelected)
    {
        if (selected == shouldBeSelected)
            return;

        selected = shouldBeSelected;
        repaint();
    }

    void paintButton (juce::Graphics& graphics,
                      bool highlighted,
                      bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        auto border = selected ? accent.brighter (0.28f)
                               : juce::Colour (panelOutline);
        if (highlighted || down)
            border = border.brighter (0.25f);

        if (selected)
        {
            graphics.setColour (accent.withAlpha (
                down ? 0.18f : 0.10f));
            graphics.fillRoundedRectangle (bounds, 5.0f);
        }

        graphics.setColour (border);
        graphics.drawRoundedRectangle (bounds, 5.0f,
                                       selected ? 1.6f : 1.0f);
        graphics.setColour (juce::Colour (0xffedf4f7));
        graphics.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
        graphics.drawText (getButtonText(),
                           getLocalBounds().withTrimmedLeft (22),
                           juce::Justification::centred, false);
    }

    void resized() override
    {
        ledButton.setBounds (5, juce::jmax (0, (getHeight() - 22) / 2), 22, 22);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragStarted = false;
        juce::Button::mouseDown (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        juce::Button::mouseDrag (event);
        if (stageIndex < 0 || dragStarted
            || event.getDistanceFromDragStart() < 5)
            return;

        if (auto* container =
                juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragStarted = true;
            container->startDragging (
                "seqwencer-fx:" + juce::String (stageIndex),
                this, juce::ScaledImage(), false, nullptr, &event.source);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        juce::Button::mouseUp (event);
        dragStarted = false;
    }

private:
    class LedButton final : public juce::Button
    {
    public:
        explicit LedButton (juce::Colour accentColour)
            : juce::Button ("FX power"), accent (accentColour)
        {
            setClickingTogglesState (true);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paintButton (juce::Graphics& graphics,
                          bool highlighted,
                          bool down) override
        {
            auto led = getLocalBounds().toFloat()
                .withSizeKeepingCentre (9.0f, 9.0f);
            const auto on = getToggleState();
            auto colour = on ? accent.brighter (0.25f)
                             : juce::Colour (0xff12181e);
            if (highlighted || down)
                colour = colour.brighter (0.30f);

            graphics.setColour (colour);
            graphics.fillEllipse (led);
            graphics.setColour ((on ? accent.brighter (0.55f)
                                    : juce::Colour (panelOutline)));
            graphics.drawEllipse (led, on ? 1.4f : 1.0f);
        }

    private:
        juce::Colour accent;
    };

    juce::Colour accent;
    LedButton ledButton;
    int stageIndex = -1;
    bool selected = false;
    bool dragStarted = false;
};

class SeqwencerAudioProcessorEditor::StepGrid final : public juce::Component
{
public:
    StepGrid (SeqwencerAudioProcessor& audioProcessor,
              int bankIndex,
              juce::Colour accentColour)
        : processor (audioProcessor), bank (bankIndex), accent (accentColour)
    {
        auto& state = processor.getParameterState();
        gateEnabledParameter = state.getParameter ("gate_enabled");
        jassert (gateEnabledParameter != nullptr);
        setEngine (seqwencer::SequencerEngine::gate);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    }

    ~StepGrid() override
    {
        endGestures();
    }

    void setAccentColour (juce::Colour newAccent)
    {
        accent = newAccent;
        repaint();
    }

    void nudgeSteps (int direction)
    {
        setCanonicalStepValues (seqwencer::nudgeStepArray (
            readCanonicalValues(), direction));
        setSubdivisions (seqwencer::nudgeStepArray (
            processor.getStepSubdivisions (engine, bank), direction));
    }

    void nudgeGateModes (int direction)
    {
        if (engine != seqwencer::SequencerEngine::gate)
            return;
        setGateModes (seqwencer::nudgeStepArray (
            readGateModes(), direction));
    }

    void applyWaveform (seqwencer::WaveformPreset preset)
    {
        endGestures();
        const auto values = seqwencer::makeWaveformPreset (
            preset, usesBipolarDisplay());
        auto subdivisions = processor.getStepSubdivisions (engine, bank);
        for (std::size_t step = 0; step < values.size(); ++step)
        {
            auto* parameter = stepParameters[step];
            if (parameter == nullptr)
                continue;

            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (values[step]));
            parameter->endChangeGesture();
            subdivisions[step].extraValues.fill (values[step]);
        }
        processor.setStepSubdivisions (engine, bank, subdivisions);
        repaint();
    }

    void setEngine (seqwencer::SequencerEngine newEngine)
    {
        if (engine == newEngine && stepParameters[0] != nullptr)
            return;

        endGestures();
        engine = newEngine;
        auto& state = processor.getParameterState();
        const auto parameterID = [newEngine] (const juce::String& gateID)
        {
            if (newEngine == seqwencer::SequencerEngine::phi)
                return "phi_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::delay)
                return "delay_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::reverb)
                return "reverb_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::pan)
                return "pan_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::filter)
                return "filter_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::pitch)
                return "pitch_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::distortion)
                return "distortion_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::grain)
                return "grain_" + gateID;
            if (newEngine == seqwencer::SequencerEngine::compressor)
                return "compressor_" + gateID;
            return gateID;
        };

        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto index = static_cast<std::size_t> (step);
            const auto stepID = [newEngine] (int stepBank, int stepIndex)
            {
                if (newEngine == seqwencer::SequencerEngine::phi)
                    return SeqwencerAudioProcessor::phiStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::delay)
                    return SeqwencerAudioProcessor::delayStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::reverb)
                    return SeqwencerAudioProcessor::reverbStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::pan)
                    return SeqwencerAudioProcessor::panStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::filter)
                    return SeqwencerAudioProcessor::filterStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::pitch)
                    return SeqwencerAudioProcessor::pitchStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::distortion)
                    return SeqwencerAudioProcessor::distortionStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::grain)
                    return SeqwencerAudioProcessor::grainStepParameterID (
                        stepBank, stepIndex);
                if (newEngine == seqwencer::SequencerEngine::compressor)
                    return SeqwencerAudioProcessor::compressorStepParameterID (
                        stepBank, stepIndex);
                return SeqwencerAudioProcessor::stepParameterID (
                    stepBank, stepIndex);
            };
            stepParameters[index] = state.getParameter (stepID (bank, step));
            otherBankStepParameters[index] = state.getParameter (
                stepID (bank == 0 ? 1 : 0, step));
            gateModeParameters[index] = state.getParameter (
                SeqwencerAudioProcessor::gateModeParameterID (bank, step));
            jassert (stepParameters[index] != nullptr);
            jassert (otherBankStepParameters[index] != nullptr);
        }

        attackAParameter = state.getParameter (parameterID ("seq_a_attack"));
        releaseAParameter = state.getParameter (parameterID ("seq_a_release"));
        attackBParameter = state.getParameter (parameterID ("seq_b_attack"));
        releaseBParameter = state.getParameter (parameterID ("seq_b_release"));
        modeParameter = state.getParameter (parameterID ("playback_mode"));
        serialProfileParameter = state.getParameter (
            parameterID ("serial_profile"));
        startStepParameter = state.getParameter (parameterID ("start_step"));
        endStepParameter = state.getParameter (parameterID ("end_step"));
        rangeLengthParameter = state.getParameter (
            parameterID ("range_length"));
        rangeLinkParameter = state.getParameter (parameterID ("range_link"));
        bipolarAParameter = state.getParameter (parameterID ("seq_a_bipolar"));
        bipolarBParameter = state.getParameter (parameterID ("seq_b_bipolar"));
        repaint();
    }

    void paint (juce::Graphics& graphics) override
    {
        auto bounds = getLocalBounds().toFloat();
        graphics.setColour (juce::Colour (0xff0c1014));
        graphics.fillRoundedRectangle (bounds, 7.0f);

        const auto graph = getGraphBounds();
        const auto columnWidth = graph.getWidth()
                               / static_cast<float> (seqwencer::stepsPerBank);
        const auto values = readValues();
        const auto subdivisions = readSubdivisions();
        const auto otherValues = readOtherValues();
        const auto otherSubdivisions = readSubdivisionsForBank (
            bank == 0 ? 1 : 0);
        const auto bipolar = usesBipolarDisplay();
        const auto activeStep = bank == 0
            ? processor.getActiveStepA (engine)
            : processor.getActiveStepB (engine);

        if (engine == seqwencer::SequencerEngine::gate)
        {
            const auto gateModes = readGateModes();
            const auto modeRow = getGateModeBounds();
            const auto modeIsActive = isGateModeActive();
            for (int step = 0; step < seqwencer::stepsPerBank; ++step)
            {
                const auto left = modeRow.getX()
                                + columnWidth * static_cast<float> (step);
                const auto cell = juce::Rectangle<float> {
                    left + 1.0f, modeRow.getY(),
                    juce::jmax (1.0f, columnWidth - 2.0f), modeRow.getHeight()
                };
                const auto mode = gateModes[static_cast<std::size_t> (step)];
                const auto cellAccent = accent.withMultipliedAlpha (
                    modeIsActive ? (step == activeStep ? 0.95f : 0.72f) : 0.22f);

                graphics.setColour (juce::Colour (0xff111820));
                graphics.fillRoundedRectangle (cell, 2.0f);

                if (mode == seqwencer::GateStepMode::shortStep)
                {
                    auto fill = cell.reduced (2.0f);
                    fill.setWidth (fill.getWidth() * 0.5f);
                    graphics.setColour (cellAccent);
                    graphics.fillRoundedRectangle (fill, 1.5f);
                }
                else if (mode == seqwencer::GateStepMode::longStep
                         || mode == seqwencer::GateStepMode::linkStep)
                {
                    graphics.setColour (cellAccent);
                    graphics.fillRoundedRectangle (cell.reduced (2.0f), 1.5f);

                    if (mode == seqwencer::GateStepMode::linkStep)
                    {
                        const auto arrowColour = juce::Colour (background)
                            .withMultipliedAlpha (modeIsActive ? 0.90f : 0.45f);
                        const auto centre = cell.getCentre();
                        const auto arrowHalfWidth = cell.getWidth() * 0.22f;
                        const auto arrowHalfHeight = cell.getHeight() * 0.19f;
                        juce::Path arrow;
                        arrow.startNewSubPath (centre.x - arrowHalfWidth, centre.y);
                        arrow.lineTo (centre.x + arrowHalfWidth, centre.y);
                        arrow.startNewSubPath (centre.x + arrowHalfWidth, centre.y);
                        arrow.lineTo (centre.x + arrowHalfWidth - arrowHalfHeight,
                                      centre.y - arrowHalfHeight);
                        arrow.startNewSubPath (centre.x + arrowHalfWidth, centre.y);
                        arrow.lineTo (centre.x + arrowHalfWidth - arrowHalfHeight,
                                      centre.y + arrowHalfHeight);
                        graphics.setColour (arrowColour);
                        graphics.strokePath (arrow, juce::PathStrokeType (1.4f));
                    }
                }

                graphics.setColour ((mode == seqwencer::GateStepMode::off
                                         ? juce::Colour (panelOutline)
                                         : cellAccent.brighter (0.18f))
                                        .withMultipliedAlpha (
                                            modeIsActive ? 1.0f : 0.55f));
                graphics.drawRoundedRectangle (cell, 2.0f,
                                               step == activeStep ? 1.4f : 0.8f);

                if (! isStepInPlaybackRange (step))
                {
                    graphics.setColour (juce::Colour (0xb0101419));
                    graphics.fillRoundedRectangle (cell, 2.0f);
                }
            }
        }

        const auto subdivisionRow = getSubdivisionBounds();
        graphics.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto left = subdivisionRow.getX()
                            + columnWidth * static_cast<float> (step);
            const auto cell = juce::Rectangle<float> {
                left + 1.0f, subdivisionRow.getY(),
                juce::jmax (1.0f, columnWidth - 2.0f),
                subdivisionRow.getHeight()
            };
            const auto mode = subdivisions[static_cast<std::size_t> (step)].mode;
            const auto label = mode == seqwencer::StepDivisionMode::half ? "H"
                : mode == seqwencer::StepDivisionMode::two ? "2"
                : mode == seqwencer::StepDivisionMode::three ? "3" : "";
            const auto inRange = isStepInPlaybackRange (step);
            graphics.setColour (juce::Colour (0xff111820));
            graphics.fillRoundedRectangle (cell, 2.0f);
            graphics.setColour ((mode == seqwencer::StepDivisionMode::normal
                                      ? juce::Colour (mutedText)
                                      : accent.brighter (0.20f))
                                    .withMultipliedAlpha (inRange ? 0.90f : 0.25f));
            if (mode == seqwencer::StepDivisionMode::normal)
            {
                const auto dotSize = juce::jlimit (
                    2.5f, 4.0f, juce::jmin (cell.getWidth(), cell.getHeight()) * 0.30f);
                graphics.fillEllipse (cell.getCentreX() - dotSize * 0.5f,
                                      cell.getCentreY() - dotSize * 0.5f,
                                      dotSize, dotSize);
            }
            else
            {
                graphics.drawText (label, cell.toNearestInt(),
                                   juce::Justification::centred, false);
            }
            graphics.setColour (juce::Colour (panelOutline)
                                    .withMultipliedAlpha (inRange ? 0.85f : 0.28f));
            graphics.drawRoundedRectangle (cell, 2.0f, 0.8f);
        }

        juce::Path clip;
        clip.addRoundedRectangle (graph, 4.0f);
        graphics.saveState();
        graphics.reduceClipRegion (clip);
        graphics.setColour (juce::Colour (0xff121920));
        graphics.fillRect (graph);

        if (bipolar)
        {
            graphics.setColour (accent.withAlpha (0.22f));
            graphics.drawHorizontalLine (
                static_cast<int> (std::round (graph.getCentreY())),
                graph.getX(), graph.getRight());
        }

        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto left = graph.getX() + columnWidth * static_cast<float> (step);
            const auto cell = juce::Rectangle<float> {
                left, graph.getY(), columnWidth, graph.getHeight()
            };

            if ((step / 4) % 2 != 0)
            {
                graphics.setColour (juce::Colour (0x0dffffff));
                graphics.fillRect (cell);
            }

            if (step == activeStep)
            {
                graphics.setColour (accent.withAlpha (0.20f));
                graphics.fillRect (cell);
            }

            const auto& subdivision = subdivisions[
                static_cast<std::size_t> (step)];
            const auto segmentCount = seqwencer::stepDivisionSegmentCount (
                subdivision.mode);
            const auto segmentWidth = columnWidth
                                    / static_cast<float> (segmentCount);
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const auto clearHalf = subdivision.mode
                                           == seqwencer::StepDivisionMode::half
                                    && segment == 1;
                const auto value = clearHalf ? (bipolar ? 0.5f : 0.0f)
                    : segment == 0
                        ? values[static_cast<std::size_t> (step)]
                        : subdivision.extraValues[static_cast<std::size_t> (
                              segment - 1)];
                const auto segmentLeft = left
                    + segmentWidth * static_cast<float> (segment);
                const auto height = graph.getHeight() * value;
                graphics.setColour (accent.withAlpha (
                    step == activeStep ? 0.92f : 0.68f));
                if (bipolar)
                {
                    const auto valueY = graph.getBottom() - height;
                    const auto centreY = graph.getCentreY();
                    graphics.fillRect (
                        segmentLeft + 1.0f,
                        juce::jmin (valueY, centreY),
                        juce::jmax (1.0f, segmentWidth - 2.0f),
                        std::abs (valueY - centreY));
                }
                else
                {
                    graphics.fillRect (
                        segmentLeft + 1.0f,
                        graph.getBottom() - height,
                        juce::jmax (1.0f, segmentWidth - 2.0f), height);
                }

                if (segment > 0)
                {
                    graphics.setColour (accent.withAlpha (0.32f));
                    graphics.drawVerticalLine (
                        static_cast<int> (std::round (segmentLeft)),
                        graph.getY(), graph.getBottom());
                }
            }

            graphics.setColour (juce::Colour (0x553b4650));
            graphics.drawVerticalLine (static_cast<int> (std::round (left + columnWidth)),
                                       graph.getY(), graph.getBottom());
        }

        juce::Path curve;
        constexpr int pointsPerStep = 16;
        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            for (int point = 0; point <= pointsPerStep; ++point)
            {
                const auto position = static_cast<float> (point)
                                    / static_cast<float> (pointsPerStep);
                const auto x = graph.getX()
                             + columnWidth * (static_cast<float> (step) + position);
                const auto y = graph.getBottom()
                             - graph.getHeight() * displayedValueAt (
                                   values, subdivisions,
                                   otherValues, otherSubdivisions,
                                   step, position);
                if (step == 0 && point == 0)
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }
        }

        graphics.setColour (juce::Colour (0xb0000000));
        graphics.strokePath (curve, juce::PathStrokeType (3.6f));
        graphics.setColour (accent.brighter (0.55f));
        graphics.strokePath (curve, juce::PathStrokeType (1.5f));

        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            if (isStepInPlaybackRange (step))
                continue;

            const auto left = graph.getX() + columnWidth * static_cast<float> (step);
            graphics.setColour (juce::Colour (0xb0101419));
            graphics.fillRect (left, graph.getY(), columnWidth, graph.getHeight());
        }
        graphics.restoreState();

        graphics.setColour (accent.withAlpha (0.75f));
        graphics.drawRoundedRectangle (graph, 4.0f, 1.2f);

        graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        for (int step = 0; step < seqwencer::stepsPerBank; step += 4)
        {
            const auto left = static_cast<int> (std::round (
                graph.getX() + columnWidth * static_cast<float> (step)));
            graphics.setColour (juce::Colour (mutedText));
            graphics.drawText (juce::String (step + 1), left, 1,
                               static_cast<int> (columnWidth * 4.0f), 13,
                               juce::Justification::centredLeft, false);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const auto inGateModeRow = engine == seqwencer::SequencerEngine::gate
                                && getGateModeBounds().contains (event.position);
        const auto inSubdivisionRow = getSubdivisionBounds().contains (
            event.position);
        const auto inGraph = getGraphBounds().contains (event.position);
        if (! inGateModeRow && ! inSubdivisionRow && ! inGraph)
            return;

        const auto step = stepAtPosition (event.position);
        if (inSubdivisionRow)
        {
            if (event.mods.isRightButtonDown())
                showSubdivisionMenu();
            else if (event.mods.isLeftButtonDown())
                cycleDivisionMode (step);
            return;
        }
        if (inGateModeRow)
        {
            if (event.mods.isRightButtonDown())
                showGateMenu();
            else if (event.mods.isLeftButtonDown())
                cycleGateMode (step);
            return;
        }

        if (event.getNumberOfClicks() > 1)
            return;

        if (event.mods.isRightButtonDown())
        {
            showStepMenu();
            return;
        }

        if (event.mods.isMiddleButtonDown())
        {
            const auto segment = segmentAtPosition (event.position, step);
            if (segment >= 0)
                setStoredStepValueOnce (
                    step, segment, usesBipolarDisplay() ? 0.0f : 0.5f);
            return;
        }

        if (! event.mods.isLeftButtonDown())
            return;

        dragging = true;
        hasLastDragPosition = false;
        updateFromMouse (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragging)
        {
            if (! hasLastDragPosition)
            {
                updateFromMouse (event.position);
                return;
            }

            const auto graph = getGraphBounds();
            const auto smallestSegmentWidth = graph.getWidth()
                / static_cast<float> (seqwencer::stepsPerBank
                                      * seqwencer::maximumSegmentsPerStep);
            const auto samples = juce::jmax (
                1, static_cast<int> (std::ceil (
                    std::abs (event.position.x - lastDragPosition.x)
                    / juce::jmax (1.0f, smallestSegmentWidth))));
            const auto start = lastDragPosition;
            for (int sample = 1; sample <= samples; ++sample)
            {
                const auto amount = static_cast<float> (sample)
                                  / static_cast<float> (samples);
                updateFromMouse (start + (event.position - start) * amount);
            }
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        endGestures();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! getGraphBounds().contains (event.position))
            return;

        endGestures();
        const auto step = stepAtPosition (event.position);
        const auto segment = segmentAtPosition (event.position, step);
        if (segment >= 0)
            setStoredStepValueOnce (step, segment, 0.5f);
    }

private:
    enum MenuCommand
    {
        stepZero = 1,
        stepMaximum,
        stepMinimum,
        stepRandom,
        stepReset,
        stepCopy,
        stepPaste,
        gateShort = 101,
        gateLong,
        gateRandom,
        gateReset,
        gateCopy,
        gatePaste,
        subdivisionOff = 201,
        subdivisionHalf,
        subdivisionTwo,
        subdivisionThree
    };

    juce::Rectangle<float> getGateModeBounds() const
    {
        const auto inner = getLocalBounds().toFloat().reduced (7.0f);
        const auto columnWidth = inner.getWidth()
                               / static_cast<float> (seqwencer::stepsPerBank);
        const auto modeHeight = juce::jmax (8.0f,
                                           std::floor (columnWidth * 0.68f));
        return { inner.getX(), inner.getY() + 9.0f,
                 inner.getWidth(), modeHeight };
    }

    juce::Rectangle<float> getSubdivisionBounds() const
    {
        const auto inner = getLocalBounds().toFloat().reduced (7.0f);
        const auto columnWidth = inner.getWidth()
                               / static_cast<float> (seqwencer::stepsPerBank);
        const auto modeHeight = juce::jmax (9.0f,
                                           std::floor (columnWidth * 0.68f));
        return { inner.getX(), inner.getBottom() - modeHeight,
                 inner.getWidth(), modeHeight };
    }

    juce::Rectangle<float> getGraphBounds() const
    {
        const auto inner = getLocalBounds().toFloat().reduced (7.0f);
        const auto graphTop = engine == seqwencer::SequencerEngine::gate
            ? getGateModeBounds().getBottom() + 4.0f
            : inner.getY() + 9.0f;
        const auto graphBottom = getSubdivisionBounds().getY() - 4.0f;
        return { inner.getX(), graphTop, inner.getWidth(),
                 juce::jmax (1.0f, graphBottom - graphTop) };
    }

    seqwencer::Pattern readValues() const
    {
        seqwencer::Pattern values {};
        const auto bipolar = usesBipolarDisplay();
        for (std::size_t step = 0; step < values.size(); ++step)
        {
            auto* parameter = stepParameters[step];
            const auto canonical = parameter != nullptr
                ? parameter->convertFrom0to1 (parameter->getValue()) : 1.0f;
            values[step] = seqwencer::displayFromCanonical (canonical, bipolar);
        }
        return values;
    }

    seqwencer::Pattern readCanonicalValues() const
    {
        seqwencer::Pattern values {};
        for (std::size_t step = 0; step < values.size(); ++step)
            values[step] = readParameter (stepParameters[step], 1.0f);
        return values;
    }

    seqwencer::Pattern readOtherValues() const
    {
        seqwencer::Pattern values {};
        const auto bipolar = usesBipolarDisplay();
        for (std::size_t step = 0; step < values.size(); ++step)
            values[step] = seqwencer::displayFromCanonical (
                readParameter (otherBankStepParameters[step], 1.0f), bipolar);
        return values;
    }

    seqwencer::StepSubdivisionPattern readSubdivisionsForBank (
        int requestedBank) const
    {
        auto subdivisions = processor.getStepSubdivisions (
            engine, requestedBank);
        const auto bipolar = usesBipolarDisplay();
        for (auto& step : subdivisions)
            for (auto& value : step.extraValues)
                value = seqwencer::displayFromCanonical (value, bipolar);
        return subdivisions;
    }

    seqwencer::StepSubdivisionPattern readSubdivisions() const
    {
        return readSubdivisionsForBank (bank);
    }

    seqwencer::GateModePattern readGateModes() const
    {
        seqwencer::GateModePattern modes {};
        for (std::size_t step = 0; step < modes.size(); ++step)
        {
            modes[step] = seqwencer::gateStepModeFromChoice (
                readParameter (gateModeParameters[step],
                    static_cast<float> (seqwencer::GateStepMode::longStep)));
        }
        return modes;
    }

    static float readParameter (const juce::RangedAudioParameter* parameter,
                                float fallback = 0.0f)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : fallback;
    }

    bool isSerial() const
    {
        return readParameter (modeParameter) >= 0.5f;
    }

    bool isGateModeActive() const
    {
        return readParameter (gateEnabledParameter) >= 0.5f;
    }

    seqwencer::StepRange getPlaybackRange() const
    {
        const auto maximumStep = isSerial() ? seqwencer::linkedStepCount
                                            : seqwencer::stepsPerBank;
        if (readParameter (rangeLinkParameter) >= 0.5f)
        {
            return seqwencer::makeLengthStepRange (
                static_cast<int> (std::lround (
                    readParameter (startStepParameter, 1.0f))),
                static_cast<int> (std::lround (
                    readParameter (rangeLengthParameter, 64.0f))),
                maximumStep);
        }
        return seqwencer::makeStepRange (
            static_cast<int> (std::lround (readParameter (startStepParameter, 1.0f))),
            static_cast<int> (std::lround (readParameter (endStepParameter, 64.0f))),
            maximumStep);
    }

    bool usesBipolarDisplay() const
    {
        if (! isSerial())
            return readParameter (bank == 0 ? bipolarAParameter
                                            : bipolarBParameter) >= 0.5f;

        const auto profileB = readParameter (serialProfileParameter) >= 0.5f;
        return readParameter (profileB ? bipolarBParameter
                                       : bipolarAParameter) >= 0.5f;
    }

    bool isStepInPlaybackRange (int step) const
    {
        const auto range = getPlaybackRange();
        const auto index = isSerial() ? bank * seqwencer::stepsPerBank + step
                                      : step;
        return index >= range.first && index <= range.last;
    }

    float valueAtGlobalStep (const seqwencer::Pattern& ownValues,
                             int globalStep) const
    {
        const auto globalBank = globalStep / seqwencer::stepsPerBank;
        const auto localStep = globalStep % seqwencer::stepsPerBank;
        if (globalBank == bank)
            return ownValues[static_cast<std::size_t> (localStep)];

        auto* parameter = otherBankStepParameters[static_cast<std::size_t> (localStep)];
        return seqwencer::displayFromCanonical (
            readParameter (parameter, 1.0f), usesBipolarDisplay());
    }

    float displayedValueAt (
        const seqwencer::Pattern& values,
        const seqwencer::StepSubdivisionPattern& subdivisions,
        const seqwencer::Pattern& otherValues,
        const seqwencer::StepSubdivisionPattern& otherSubdivisions,
        int step,
        float position) const
    {
        const auto linked = isSerial();
        const auto range = getPlaybackRange();
        auto previousStep = 0;
        auto previousIsOtherBank = false;
        if (linked)
        {
            const auto globalStep = bank * seqwencer::stepsPerBank + step;
            const auto previousGlobalStep = globalStep == range.first
                ? range.last
                : (globalStep + seqwencer::linkedStepCount - 1)
                    % seqwencer::linkedStepCount;
            previousStep = previousGlobalStep % seqwencer::stepsPerBank;
            previousIsOtherBank = previousGlobalStep
                                    / seqwencer::stepsPerBank != bank;
        }
        else
        {
            previousStep = step == range.first
                ? range.last
                : (step + seqwencer::stepsPerBank - 1)
                    % seqwencer::stepsPerBank;
        }

        const auto useProfileB = linked && serialProfileParameter != nullptr
            && serialProfileParameter->convertFrom0to1 (
                   serialProfileParameter->getValue()) >= 0.5f;
        auto* attackParameter = linked
            ? (useProfileB ? attackBParameter : attackAParameter)
            : (bank == 0 ? attackAParameter : attackBParameter);
        auto* releaseParameter = linked
            ? (useProfileB ? releaseBParameter : releaseAParameter)
            : (bank == 0 ? releaseAParameter : releaseBParameter);
        const auto attack = attackParameter != nullptr
            ? attackParameter->convertFrom0to1 (attackParameter->getValue()) : 0.0f;
        const auto release = releaseParameter != nullptr
            ? releaseParameter->convertFrom0to1 (releaseParameter->getValue()) : 0.0f;
        const auto neutral = usesBipolarDisplay() ? 0.5f : 0.0f;
        const auto divisionMode = subdivisions[static_cast<std::size_t> (
            step)].mode;
        const auto segmentCount = seqwencer::stepDivisionSegmentCount (
            divisionMode);
        const auto scaledPosition = juce::jmin (
            static_cast<float> (segmentCount) - 0.000001f,
            juce::jlimit (0.0f, 1.0f, position)
                * static_cast<float> (segmentCount));
        const auto segment = juce::jlimit (
            0, segmentCount - 1,
            static_cast<int> (std::floor (scaledPosition)));
        const auto segmentPosition = scaledPosition
                                   - static_cast<float> (segment);
        const auto previous = segment > 0
            ? seqwencer::subdividedStepSegmentValue (
                  values, subdivisions, step, segment - 1, neutral)
            : previousIsOtherBank
                ? seqwencer::subdividedStepFinalValue (
                      otherValues, otherSubdivisions,
                      previousStep, neutral)
                : seqwencer::subdividedStepFinalValue (
                      values, subdivisions, previousStep, neutral);
        const auto current = seqwencer::subdividedStepSegmentValue (
            values, subdivisions, step, segment, neutral);
        return seqwencer::transitionValue (
            previous, current, segmentPosition, attack, release);
    }

    void setStepValue (int step, float value)
    {
        const auto index = static_cast<std::size_t> (
            juce::jlimit (0, seqwencer::stepsPerBank - 1, step));
        auto* parameter = stepParameters[index];
        if (parameter == nullptr)
            return;

        if (! gestureActive[index])
        {
            parameter->beginChangeGesture();
            gestureActive[index] = true;
        }

        const auto displayed = juce::jlimit (0.0f, 1.0f, value);
        const auto canonical = usesBipolarDisplay()
            ? displayed : seqwencer::canonicalFromUnipolar (displayed);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (canonical));
    }

    void setSegmentValue (int step, int segment, float displayedValue)
    {
        const auto displayed = juce::jlimit (0.0f, 1.0f, displayedValue);
        if (segment <= 0)
        {
            setStepValue (step, displayed);
            return;
        }

        const auto canonical = usesBipolarDisplay()
            ? displayed : seqwencer::canonicalFromUnipolar (displayed);
        processor.setStepExtraValue (
            engine, bank, step, segment - 1, canonical);
    }

    int stepAtPosition (juce::Point<float> position) const
    {
        const auto graph = getGraphBounds();
        const auto x = juce::jlimit (graph.getX(), graph.getRight() - 0.001f,
                                    position.x);
        return juce::jlimit (
            0, seqwencer::stepsPerBank - 1,
            static_cast<int> ((x - graph.getX())
                * static_cast<float> (seqwencer::stepsPerBank) / graph.getWidth()));
    }

    int segmentAtPosition (juce::Point<float> position, int step) const
    {
        const auto graph = getGraphBounds();
        const auto columnWidth = graph.getWidth()
                               / static_cast<float> (seqwencer::stepsPerBank);
        step = juce::jlimit (0, seqwencer::stepsPerBank - 1, step);
        const auto localPosition = juce::jlimit (
            0.0f, 0.999999f,
            (position.x - (graph.getX()
                + columnWidth * static_cast<float> (step))) / columnWidth);
        const auto subdivisions = processor.getStepSubdivisions (engine, bank);
        const auto mode = subdivisions[static_cast<std::size_t> (step)].mode;
        if (mode == seqwencer::StepDivisionMode::half)
            return localPosition < 0.5f ? 0 : -1;
        const auto segmentCount = seqwencer::stepDivisionSegmentCount (mode);
        return juce::jlimit (
            0, segmentCount - 1,
            static_cast<int> (localPosition
                * static_cast<float> (segmentCount)));
    }

    void cycleDivisionMode (int step)
    {
        step = juce::jlimit (0, seqwencer::stepsPerBank - 1, step);
        auto subdivisions = processor.getStepSubdivisions (engine, bank);
        auto& subdivision = subdivisions[static_cast<std::size_t> (step)];
        const auto current = static_cast<int> (subdivision.mode);
        const auto next = seqwencer::stepDivisionModeFromChoice (
            (current + 1)
                % (static_cast<int> (seqwencer::StepDivisionMode::three) + 1));
        if (subdivision.mode == seqwencer::StepDivisionMode::half
            && next == seqwencer::StepDivisionMode::two)
        {
            subdivision.extraValues[0] = readParameter (
                stepParameters[static_cast<std::size_t> (step)], 1.0f);
        }
        else if (subdivision.mode == seqwencer::StepDivisionMode::two
                 && next == seqwencer::StepDivisionMode::three)
        {
            subdivision.extraValues[1] = subdivision.extraValues[0];
        }
        subdivision.mode = next;
        processor.setStepSubdivisions (engine, bank, subdivisions);
        repaint();
    }

    void cycleGateMode (int step)
    {
        const auto index = static_cast<std::size_t> (
            juce::jlimit (0, seqwencer::stepsPerBank - 1, step));
        auto* parameter = gateModeParameters[index];
        if (parameter == nullptr)
            return;

        const auto current = static_cast<int> (seqwencer::gateStepModeFromChoice (
            parameter->convertFrom0to1 (parameter->getValue())));
        const auto next = (current + 1)
                        % (static_cast<int> (seqwencer::GateStepMode::linkStep) + 1);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (
            static_cast<float> (next)));
        parameter->endChangeGesture();
        repaint();
    }

    void setCanonicalStepValues (const seqwencer::Pattern& values)
    {
        endGestures();
        for (std::size_t step = 0; step < values.size(); ++step)
        {
            auto* parameter = stepParameters[step];
            if (parameter == nullptr)
                continue;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (
                juce::jlimit (0.0f, 1.0f, values[step])));
            parameter->endChangeGesture();
        }
        repaint();
    }

    void setGateModes (const seqwencer::GateModePattern& modes)
    {
        endGestures();
        for (std::size_t step = 0; step < modes.size(); ++step)
        {
            auto* parameter = gateModeParameters[step];
            if (parameter == nullptr)
                continue;
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (
                static_cast<float> (modes[step])));
            parameter->endChangeGesture();
        }
        repaint();
    }

    void setSubdivisions (
        const seqwencer::StepSubdivisionPattern& subdivisions)
    {
        processor.setStepSubdivisions (engine, bank, subdivisions);
        repaint();
    }

    void showStepMenu()
    {
        endGestures();
        juce::PopupMenu menu;
        menu.addItem (stepZero, "Zero");
        menu.addItem (stepMaximum, "Max");
        menu.addItem (stepMinimum, "Min");
        menu.addItem (stepRandom, "Random");
        menu.addSeparator();
        menu.addItem (stepReset, "Reset");
        menu.addSeparator();
        menu.addItem (stepCopy, "Copy");
        menu.addItem (stepPaste, "Paste",
                      hasStepClipboard
                          && (stepClipboardEngine != engine
                              || stepClipboardBank != bank));
        juce::Component::SafePointer<StepGrid> safeThis (this);
        menu.showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (this),
            [safeThis] (int result)
            {
                if (safeThis != nullptr && result != 0)
                    safeThis->performStepMenuCommand (result);
            });
    }

    void showGateMenu()
    {
        endGestures();
        juce::PopupMenu menu;
        menu.addItem (gateShort, "Short");
        menu.addItem (gateLong, "Long");
        menu.addItem (gateRandom, "Random");
        menu.addSeparator();
        menu.addItem (gateReset, "Reset");
        menu.addSeparator();
        menu.addItem (gateCopy, "Copy");
        menu.addItem (gatePaste, "Paste",
                      hasGateClipboard && gateClipboardBank != bank);
        juce::Component::SafePointer<StepGrid> safeThis (this);
        menu.showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (this),
            [safeThis] (int result)
            {
                if (safeThis != nullptr && result != 0)
                    safeThis->performGateMenuCommand (result);
            });
    }

    void showSubdivisionMenu()
    {
        endGestures();
        juce::PopupMenu menu;
        menu.addItem (subdivisionOff, "Off");
        menu.addItem (subdivisionHalf, "H");
        menu.addItem (subdivisionTwo, "2");
        menu.addItem (subdivisionThree, "3");
        juce::Component::SafePointer<StepGrid> safeThis (this);
        menu.showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (this),
            [safeThis] (int result)
            {
                if (safeThis != nullptr && result != 0)
                    safeThis->performSubdivisionMenuCommand (result);
            });
    }

    void performStepMenuCommand (int command)
    {
        if (command == stepCopy)
        {
            stepClipboard = readCanonicalValues();
            subdivisionClipboard = processor.getStepSubdivisions (
                engine, bank);
            stepClipboardEngine = engine;
            stepClipboardBank = bank;
            hasStepClipboard = true;
            return;
        }
        if (command == stepPaste)
        {
            if (hasStepClipboard
                && (stepClipboardEngine != engine
                    || stepClipboardBank != bank))
            {
                setCanonicalStepValues (stepClipboard);
                setSubdivisions (subdivisionClipboard);
            }
            return;
        }
        if (command == stepReset)
        {
            restoreFromPreset (false);
            return;
        }

        seqwencer::Pattern values {};
        auto subdivisions = processor.getStepSubdivisions (engine, bank);
        if (command == stepRandom)
        {
            auto& random = juce::Random::getSystemRandom();
            const auto bipolar = usesBipolarDisplay();
            for (std::size_t step = 0; step < values.size(); ++step)
            {
                const auto displayed = random.nextFloat();
                values[step] = bipolar ? displayed
                    : seqwencer::canonicalFromUnipolar (displayed);
                for (auto& extra : subdivisions[step].extraValues)
                {
                    const auto extraDisplayed = random.nextFloat();
                    extra = bipolar ? extraDisplayed
                        : seqwencer::canonicalFromUnipolar (extraDisplayed);
                }
            }
        }
        else
        {
            const auto value = command == stepMaximum ? 1.0f
                : command == stepMinimum && usesBipolarDisplay() ? 0.0f
                                                                 : 0.5f;
            values.fill (value);
            for (auto& subdivision : subdivisions)
                subdivision.extraValues.fill (value);
        }
        setCanonicalStepValues (values);
        setSubdivisions (subdivisions);
    }

    void performGateMenuCommand (int command)
    {
        if (command == gateCopy)
        {
            gateClipboard = readGateModes();
            gateClipboardBank = bank;
            hasGateClipboard = true;
            return;
        }
        if (command == gatePaste)
        {
            if (hasGateClipboard && gateClipboardBank != bank)
                setGateModes (gateClipboard);
            return;
        }
        if (command == gateReset)
        {
            restoreFromPreset (true);
            return;
        }

        seqwencer::GateModePattern modes {};
        if (command == gateRandom)
        {
            auto& random = juce::Random::getSystemRandom();
            for (auto& mode : modes)
                mode = static_cast<seqwencer::GateStepMode> (
                    random.nextInt (static_cast<int> (
                        seqwencer::GateStepMode::linkStep) + 1));
        }
        else
        {
            modes.fill (command == gateShort
                ? seqwencer::GateStepMode::shortStep
                : seqwencer::GateStepMode::longStep);
        }
        setGateModes (modes);
    }

    void performSubdivisionMenuCommand (int command)
    {
        const auto targetMode = command == subdivisionHalf
            ? seqwencer::StepDivisionMode::half
            : command == subdivisionTwo
                ? seqwencer::StepDivisionMode::two
                : command == subdivisionThree
                    ? seqwencer::StepDivisionMode::three
                    : seqwencer::StepDivisionMode::normal;
        auto subdivisions = processor.getStepSubdivisions (engine, bank);
        for (std::size_t step = 0; step < subdivisions.size(); ++step)
        {
            auto& subdivision = subdivisions[step];
            const auto previousMode = subdivision.mode;
            const auto mainValue = readParameter (stepParameters[step], 1.0f);
            if (targetMode == seqwencer::StepDivisionMode::two
                && previousMode != seqwencer::StepDivisionMode::two
                && previousMode != seqwencer::StepDivisionMode::three)
            {
                subdivision.extraValues[0] = mainValue;
            }
            else if (targetMode == seqwencer::StepDivisionMode::three)
            {
                if (previousMode == seqwencer::StepDivisionMode::two)
                {
                    subdivision.extraValues[1] = subdivision.extraValues[0];
                }
                else if (previousMode != seqwencer::StepDivisionMode::three)
                {
                    subdivision.extraValues.fill (mainValue);
                }
            }
            subdivision.mode = targetMode;
        }
        setSubdivisions (subdivisions);
    }

    void restoreFromPreset (bool gateModesOnly)
    {
        juce::String error;
        if (! processor.restoreSequenceFromCurrentPreset (
                engine, bank, gateModesOnly, error))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Sequence Not Reset", error, "OK", this);
        }
        repaint();
    }

    void setStoredStepValueOnce (int step, int segment, float canonical)
    {
        if (segment > 0)
        {
            processor.setStepExtraValue (
                engine, bank, step, segment - 1,
                juce::jlimit (0.0f, 1.0f, canonical));
            repaint();
            return;
        }
        const auto index = static_cast<std::size_t> (
            juce::jlimit (0, seqwencer::stepsPerBank - 1, step));
        auto* parameter = stepParameters[index];
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (
            juce::jlimit (0.0f, 1.0f, canonical)));
        parameter->endChangeGesture();
        repaint();
    }

    void updateFromMouse (juce::Point<float> position)
    {
        const auto graph = getGraphBounds();
        const auto y = juce::jlimit (graph.getY(), graph.getBottom(), position.y);
        const auto step = stepAtPosition (position);
        const auto segment = segmentAtPosition (position, step);
        const auto value = juce::jlimit (
            0.0f, 1.0f, (graph.getBottom() - y) / graph.getHeight());
        if (segment >= 0)
            setSegmentValue (step, segment, value);
        lastDragPosition = position;
        hasLastDragPosition = true;
        repaint();
    }

    void endGestures()
    {
        for (std::size_t step = 0; step < gestureActive.size(); ++step)
        {
            if (gestureActive[step] && stepParameters[step] != nullptr)
                stepParameters[step]->endChangeGesture();
            gestureActive[step] = false;
        }
        dragging = false;
        hasLastDragPosition = false;
    }

    SeqwencerAudioProcessor& processor;
    int bank = 0;
    juce::Colour accent;
    seqwencer::SequencerEngine engine = seqwencer::SequencerEngine::gate;
    std::array<juce::RangedAudioParameter*, seqwencer::stepsPerBank> stepParameters {};
    std::array<juce::RangedAudioParameter*, seqwencer::stepsPerBank>
        otherBankStepParameters {};
    std::array<juce::RangedAudioParameter*, seqwencer::stepsPerBank>
        gateModeParameters {};
    std::array<bool, seqwencer::stepsPerBank> gestureActive {};
    juce::RangedAudioParameter* attackAParameter = nullptr;
    juce::RangedAudioParameter* releaseAParameter = nullptr;
    juce::RangedAudioParameter* attackBParameter = nullptr;
    juce::RangedAudioParameter* releaseBParameter = nullptr;
    juce::RangedAudioParameter* modeParameter = nullptr;
    juce::RangedAudioParameter* serialProfileParameter = nullptr;
    juce::RangedAudioParameter* startStepParameter = nullptr;
    juce::RangedAudioParameter* endStepParameter = nullptr;
    juce::RangedAudioParameter* rangeLengthParameter = nullptr;
    juce::RangedAudioParameter* rangeLinkParameter = nullptr;
    juce::RangedAudioParameter* bipolarAParameter = nullptr;
    juce::RangedAudioParameter* bipolarBParameter = nullptr;
    juce::RangedAudioParameter* gateEnabledParameter = nullptr;
    bool dragging = false;
    bool hasLastDragPosition = false;
    juce::Point<float> lastDragPosition;
    inline static seqwencer::Pattern stepClipboard {};
    inline static seqwencer::StepSubdivisionPattern subdivisionClipboard {};
    inline static seqwencer::GateModePattern gateClipboard {};
    inline static seqwencer::SequencerEngine stepClipboardEngine =
        seqwencer::SequencerEngine::gate;
    inline static int stepClipboardBank = -1;
    inline static int gateClipboardBank = -1;
    inline static bool hasStepClipboard = false;
    inline static bool hasGateClipboard = false;
};

class SeqwencerAudioProcessorEditor::PatternNudgeControls final
    : public juce::Component
{
public:
    PatternNudgeControls (StepGrid& stepGrid, juce::Colour accentColour)
        : grid (stepGrid), accent (accentColour)
    {
        configureButton (gateLeft, "Nudge all Gate modes left by one step");
        configureButton (gateRight, "Nudge all Gate modes right by one step");
        configureButton (stepsLeft,
                         "Nudge the entire step sequence left by one step");
        configureButton (stepsRight,
                         "Nudge the entire step sequence right by one step");
        gateLeft.onClick = [this] { grid.nudgeGateModes (-1); };
        gateRight.onClick = [this] { grid.nudgeGateModes (1); };
        stepsLeft.onClick = [this] { grid.nudgeSteps (-1); };
        stepsRight.onClick = [this] { grid.nudgeSteps (1); };
    }

    void setAccentColour (juce::Colour newAccent)
    {
        accent = newAccent;
        for (auto* button : { &gateLeft, &gateRight, &stepsLeft, &stepsRight })
            button->setColour (juce::TextButton::textColourOffId, accent);
        repaint();
    }

    void setGateVisible (bool shouldShowGate)
    {
        gateVisible = shouldShowGate;
        gateLeft.setVisible (gateVisible);
        gateRight.setVisible (gateVisible);
        repaint();
    }

    void paint (juce::Graphics& graphics) override
    {
        graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        graphics.setColour (accent.withAlpha (0.90f));
        if (gateVisible)
            graphics.drawText ("GATE", 0, 0, 49, rowHeight,
                               juce::Justification::centredRight, false);
        graphics.drawText ("STEPS", 0, rowHeight, 49, rowHeight,
                           juce::Justification::centredRight, false);
    }

    void resized() override
    {
        gateLeft.setBounds (55, 1, 31, rowHeight - 2);
        gateRight.setBounds (92, 1, 31, rowHeight - 2);
        stepsLeft.setBounds (55, rowHeight + 1, 31, rowHeight - 2);
        stepsRight.setBounds (92, rowHeight + 1, 31, rowHeight - 2);
    }

private:
    void configureButton (juce::TextButton& button,
                          const juce::String& tooltip)
    {
        button.setColour (juce::TextButton::buttonColourId,
                          juce::Colour (0xff182028));
        button.setColour (juce::TextButton::buttonOnColourId,
                          juce::Colour (0xff26323d));
        button.setColour (juce::TextButton::textColourOffId, accent);
        button.setTooltip (tooltip);
        addAndMakeVisible (button);
    }

    static constexpr int rowHeight = 22;
    StepGrid& grid;
    juce::Colour accent;
    bool gateVisible = true;
    juce::TextButton gateLeft { "<" };
    juce::TextButton gateRight { ">" };
    juce::TextButton stepsLeft { "<" };
    juce::TextButton stepsRight { ">" };
};

class SeqwencerAudioProcessorEditor::RangeLinkButton final
    : public juce::Button
{
public:
    RangeLinkButton()
        : juce::Button ("Sequence range link")
    {
        setClickingTogglesState (true);
        setTooltip (
            "Link Start to a sequence Length; the End knob becomes Length");
    }

    void setAccentColour (juce::Colour newAccent)
    {
        accent = newAccent;
        repaint();
    }

    void paintButton (juce::Graphics& graphics,
                      bool highlighted,
                      bool down) override
    {
        const auto active = getToggleState();
        const auto iconColour = active
            ? accent.brighter (highlighted ? 0.25f : 0.10f)
            : juce::Colour (mutedText).withAlpha (highlighted ? 0.95f : 0.62f);
        if (active || down)
        {
            graphics.setColour (accent.withAlpha (down ? 0.30f : 0.18f));
            graphics.fillRoundedRectangle (
                getLocalBounds().toFloat().reduced (1.0f), 5.0f);
        }

        graphics.setColour (iconColour);
        const auto stroke = active ? 1.8f : 1.4f;
        graphics.drawRoundedRectangle ({ 2.0f, 4.0f, 10.0f, 7.0f },
                                       3.5f, stroke);
        graphics.drawRoundedRectangle ({ 10.0f, 9.0f, 10.0f, 7.0f },
                                       3.5f, stroke);
        graphics.drawLine (8.0f, 9.0f, 14.0f, 11.0f, stroke);
    }

private:
    juce::Colour accent { gateAccent };
};

class SeqwencerAudioProcessorEditor::ModulationParameterLabel final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    ModulationParameterLabel (SeqwencerAudioProcessor& audioProcessor,
                              seqwencer::ModulationTarget modulationTarget,
                              juce::String compactCaption = {})
        : processor (&audioProcessor)
    {
        setTarget (modulationTarget, std::move (compactCaption));
    }

    void setTarget (seqwencer::ModulationTarget modulationTarget,
                    juce::String compactCaption = {})
    {
        target = modulationTarget;
        caption = compactCaption.isNotEmpty()
            ? std::move (compactCaption)
            : SeqwencerAudioProcessor::targetDisplayName (target);
        targetAParameter = nullptr;
        targetBParameter = nullptr;
        if (target == seqwencer::ModulationTarget::none)
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            setTooltip (caption);
            repaint();
            return;
        }

        auto& state = processor->getParameterState();
        targetAParameter = state.getParameter (
            SeqwencerAudioProcessor::targetAssignedParameterID (0, target));
        targetBParameter = state.getParameter (
            SeqwencerAudioProcessor::targetAssignedParameterID (1, target));
        jassert (targetAParameter != nullptr);
        jassert (targetBParameter != nullptr);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag " + caption
                    + " to a sequencer. Click the cross to clear both assignments.");
        repaint();
    }

    void setAssignmentColours (juce::Colour newColourA,
                               juce::Colour newColourB,
                               juce::Colour newColourBoth)
    {
        assignmentColourA = newColourA;
        assignmentColourB = newColourB;
        assignmentColourBoth = newColourBoth;
        repaint();
    }

    void paint (juce::Graphics& graphics) override
    {
        if (target == seqwencer::ModulationTarget::none)
        {
            graphics.setColour (juce::Colour (text));
            graphics.setFont (captionFont());
            graphics.drawText (caption, getLocalBounds(),
                               juce::Justification::centred, false);
            return;
        }

        const auto assignedA = hasAssignment (targetAParameter);
        const auto assignedB = hasAssignment (targetBParameter);
        const auto assignmentColour = assignedA && assignedB
            ? assignmentColourBoth
            : assignedA ? assignmentColourA
                        : assignedB ? assignmentColourB
                                    : juce::Colour (text);
        const auto layout = getItemLayout (assignedA || assignedB);

        drawDragHandle (graphics, layout.handle, assignmentColour);
        graphics.setColour (assignmentColour);
        graphics.setFont (captionFont());
        graphics.drawText (caption, layout.caption,
                           juce::Justification::centred, false);

        if (assignedA || assignedB)
        {
            graphics.setColour (assignmentColour.brighter (0.20f));
            graphics.setFont (juce::FontOptions { 11.0f, juce::Font::bold });
            graphics.drawText (juce::String::charToString (
                                   static_cast<juce::juce_wchar> (0x00d7)),
                               layout.clear, juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        clearPressed = target != seqwencer::ModulationTarget::none
                    && event.mods.isLeftButtonDown()
                    && getItemLayout (hasAnyAssignment()).clear.contains (
                           event.getPosition());
        dragStarted = false;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (target == seqwencer::ModulationTarget::none
            || clearPressed || dragStarted || ! event.mods.isLeftButtonDown()
            || event.getDistanceFromDragStart() < 4)
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragStarted = true;
            container->startDragging (juce::var (targetDragID (target)), this);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (target != seqwencer::ModulationTarget::none
            && clearPressed && getItemLayout (hasAnyAssignment()).clear.contains (
                                event.getPosition()))
        {
            setAssigned (targetAParameter, false);
            setAssigned (targetBParameter, false);
        }

        clearPressed = false;
        dragStarted = false;
        repaint();
    }

private:
    struct ItemLayout
    {
        juce::Rectangle<int> handle;
        juce::Rectangle<int> caption;
        juce::Rectangle<int> clear;
    };

    ItemLayout getItemLayout (bool showClear) const
    {
        constexpr int handleWidth = 8;
        const auto requiredCaptionWidth =
            juce::GlyphArrangement::getStringWidthInt (captionFont(), caption) + 4;
        const auto captionWidth = juce::jlimit (
            30, juce::jmax (30, getWidth() - 18), requiredCaptionWidth);
        constexpr int clearWidth = 8;
        constexpr int gap = 1;
        const auto captionX = (getWidth() - captionWidth) / 2;
        const auto centreY = getHeight() / 2;

        ItemLayout result;
        result.handle = { captionX - gap - handleWidth,
                          centreY - 4, handleWidth, 8 };
        result.caption = { captionX, 0, captionWidth, getHeight() };
        result.clear = showClear
            ? juce::Rectangle<int> { result.caption.getRight() + gap,
                                     0, clearWidth, getHeight() }
            : juce::Rectangle<int>();
        return result;
    }

    static juce::Font captionFont()
    {
        return juce::Font (juce::FontOptions { 10.0f, juce::Font::bold });
    }

    static void drawDragHandle (juce::Graphics& graphics,
                                juce::Rectangle<int> bounds,
                                juce::Colour colour)
    {
        graphics.setColour (colour.withAlpha (0.82f));
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column)
                graphics.fillEllipse (static_cast<float> (bounds.getX() + column * 4),
                                      static_cast<float> (bounds.getY() + row * 4),
                                      2.2f, 2.2f);
    }

    static bool hasAssignment (const juce::RangedAudioParameter* parameter)
    {
        return parameter != nullptr
            && parameter->convertFrom0to1 (parameter->getValue()) >= 0.5f;
    }

    bool hasAnyAssignment() const
    {
        return hasAssignment (targetAParameter) || hasAssignment (targetBParameter);
    }

    static void setAssigned (juce::RangedAudioParameter* parameter,
                             bool shouldAssign)
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (
            shouldAssign ? 1.0f : 0.0f));
        parameter->endChangeGesture();
    }

    SeqwencerAudioProcessor* processor = nullptr;
    juce::RangedAudioParameter* targetAParameter = nullptr;
    juce::RangedAudioParameter* targetBParameter = nullptr;
    seqwencer::ModulationTarget target = seqwencer::ModulationTarget::none;
    juce::String caption;
    juce::Colour assignmentColourA { colourA };
    juce::Colour assignmentColourB { colourB };
    juce::Colour assignmentColourBoth { 0xffb98cff };
    bool clearPressed = false;
    bool dragStarted = false;
};

class SeqwencerAudioProcessorEditor::TargetList final
    : public juce::Component,
      public juce::DragAndDropTarget,
      public juce::SettableTooltipClient
{
private:
    class Content final : public juce::Component
    {
    public:
        explicit Content (TargetList& targetList) : owner (targetList)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& graphics) override
        {
            owner.paintRows (graphics);
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            owner.handleRowMouseUp (event.getPosition());
        }

    private:
        TargetList& owner;
    };

public:
    TargetList (SeqwencerAudioProcessor& audioProcessor,
                int bankIndex,
                juce::Colour accentColour,
                std::vector<seqwencer::ModulationTarget> availableTargets,
                juce::String modeParameterID)
        : bank (bankIndex), accent (accentColour),
          targets (std::move (availableTargets)), content (*this)
    {
        auto& state = audioProcessor.getParameterState();
        ownTargetParameters.resize (targets.size());
        ownTargetEnabledParameters.resize (targets.size());
        targetAParameters.resize (targets.size());
        targetAEnabledParameters.resize (targets.size());
        for (std::size_t index = 0; index < targets.size(); ++index)
        {
            const auto target = targets[index];
            ownTargetParameters[index] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetAssignedParameterID (
                        bank, target));
            ownTargetEnabledParameters[index] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetEnabledParameterID (
                        bank, target));
            targetAParameters[index] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetAssignedParameterID (
                        0, target));
            targetAEnabledParameters[index] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetEnabledParameterID (
                        0, target));
        }
        modeParameter = state.getParameter (modeParameterID);
        jassert (modeParameter != nullptr);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Drop parameters here. Tick to bypass or restore a target; cross to remove it.");
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (7);
        viewport.setColour (juce::ScrollBar::thumbColourId,
                            accent.withAlpha (0.58f));
        viewport.setColour (juce::ScrollBar::trackColourId,
                            juce::Colours::transparentBlack);
        addAndMakeVisible (viewport);
    }

    void setAccentColour (juce::Colour newAccent)
    {
        accent = newAccent;
        viewport.setColour (juce::ScrollBar::thumbColourId,
                            accent.withAlpha (0.58f));
        refresh();
    }

    void paint (juce::Graphics& graphics) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        graphics.setColour (juce::Colour (0xff111820));
        graphics.fillRoundedRectangle (bounds, 6.0f);
        graphics.setColour (dragHighlighted ? accent.brighter (0.45f)
                                            : accent.withAlpha (0.50f));
        graphics.drawRoundedRectangle (bounds, 6.0f,
                                       dragHighlighted ? 2.0f : 1.0f);

        graphics.setColour (accent.withAlpha (0.90f));
        graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
        graphics.drawText (isShared() ? "SHARED TARGETS" : "TARGETS",
                           8, 3, getWidth() - 16, headerHeight - 3,
                           juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        const auto viewBounds = getLocalBounds().withTrimmedTop (headerHeight)
                                               .reduced (1, 0);
        viewport.setBounds (viewBounds);
        updateContentSize();
    }

    void refresh()
    {
        updateContentSize();
        repaint();
        content.repaint();
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return targetIndex (targetFromDragID (details.description.toString())) >= 0;
    }

    void itemDragEnter (const SourceDetails&) override
    {
        dragHighlighted = true;
        refresh();
    }

    void itemDragExit (const SourceDetails&) override
    {
        dragHighlighted = false;
        refresh();
    }

    void itemDropped (const SourceDetails& details) override
    {
        dragHighlighted = false;
        const auto target = targetFromDragID (details.description.toString());
        setTargetAssigned (target, true);
        setTargetEnabled (target, true);
        refresh();
    }

private:
    void paintRows (juce::Graphics& graphics)
    {

        const auto assignedTargets = getAssignedTargets();
        if (assignedTargets.empty())
        {
            graphics.setColour ((dragHighlighted ? accent : juce::Colour (mutedText))
                                    .withAlpha (0.78f));
            graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            graphics.drawText ("DROP PARAMETER",
                               8, 0, juce::jmax (0, content.getWidth() - 16),
                               content.getHeight(),
                               juce::Justification::centred, false);
            return;
        }

        for (std::size_t rowIndex = 0; rowIndex < assignedTargets.size(); ++rowIndex)
        {
            const auto target = assignedTargets[rowIndex];
            const auto row = getRowBounds (static_cast<int> (rowIndex));
            graphics.setColour (juce::Colour (0xff1b252e));
            graphics.fillRoundedRectangle (row.toFloat(), 4.0f);
            graphics.setColour (accent.withAlpha (0.42f));
            graphics.drawRoundedRectangle (row.toFloat(), 4.0f, 1.0f);

            const auto checkbox = getCheckboxBounds (static_cast<int> (rowIndex));
            const auto targetEnabled = isTargetEnabled (target);
            graphics.setColour (targetEnabled ? accent
                                              : juce::Colour (panelOutline));
            graphics.drawRoundedRectangle (checkbox.toFloat(), 2.0f, 1.2f);
            if (targetEnabled)
            {
                juce::Path tick;
                tick.startNewSubPath (static_cast<float> (checkbox.getX() + 3),
                                      static_cast<float> (checkbox.getCentreY()));
                tick.lineTo (static_cast<float> (checkbox.getX() + 6),
                             static_cast<float> (checkbox.getBottom() - 3));
                tick.lineTo (static_cast<float> (checkbox.getRight() - 2),
                             static_cast<float> (checkbox.getY() + 3));
                graphics.strokePath (tick, juce::PathStrokeType (1.5f));
            }

            graphics.setColour (juce::Colour (text).withMultipliedAlpha (
                targetEnabled ? 1.0f : 0.42f));
            graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
            graphics.drawText (
                SeqwencerAudioProcessor::targetDisplayName (target),
                checkbox.getRight() + 6, row.getY(),
                juce::jmax (0, getClearBounds (static_cast<int> (rowIndex)).getX()
                                      - checkbox.getRight() - 9),
                row.getHeight(), juce::Justification::centredLeft, false);

            graphics.setColour (accent.withMultipliedAlpha (0.88f));
            graphics.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
            graphics.drawText (juce::String::charToString (
                                   static_cast<juce::juce_wchar> (0x00d7)),
                               getClearBounds (static_cast<int> (rowIndex)),
                               juce::Justification::centred, false);
        }
    }

    void handleRowMouseUp (juce::Point<int> position)
    {
        const auto assignedTargets = getAssignedTargets();
        if (assignedTargets.empty())
            return;

        for (std::size_t rowIndex = 0; rowIndex < assignedTargets.size(); ++rowIndex)
        {
            const auto target = assignedTargets[rowIndex];
            if (getCheckboxBounds (static_cast<int> (rowIndex)).contains (position))
                setTargetEnabled (target, ! isTargetEnabled (target));
            else if (getClearBounds (static_cast<int> (rowIndex)).contains (position))
                setTargetAssigned (target, false);
        }

        refresh();
    }
    bool isShared() const
    {
        return bank == 1 && modeParameter != nullptr
            && modeParameter->convertFrom0to1 (modeParameter->getValue()) >= 0.5f;
    }

    int targetIndex (seqwencer::ModulationTarget target) const
    {
        const auto found = std::find (targets.begin(), targets.end(), target);
        return found == targets.end()
            ? -1 : static_cast<int> (std::distance (targets.begin(), found));
    }

    juce::RangedAudioParameter* targetParameterToEdit (
        seqwencer::ModulationTarget target) const
    {
        const auto foundIndex = targetIndex (target);
        if (foundIndex < 0)
            return nullptr;
        const auto index = static_cast<std::size_t> (foundIndex);
        return isShared() ? targetAParameters[index] : ownTargetParameters[index];
    }

    juce::RangedAudioParameter* enabledParameterToEdit (
        seqwencer::ModulationTarget target) const
    {
        const auto foundIndex = targetIndex (target);
        if (foundIndex < 0)
            return nullptr;
        const auto index = static_cast<std::size_t> (foundIndex);
        return isShared() ? targetAEnabledParameters[index]
                          : ownTargetEnabledParameters[index];
    }

    bool isTargetAssigned (seqwencer::ModulationTarget target) const
    {
        const auto* parameter = targetParameterToEdit (target);
        return parameter != nullptr
            && parameter->convertFrom0to1 (parameter->getValue()) >= 0.5f;
    }

    bool isTargetEnabled (seqwencer::ModulationTarget target) const
    {
        const auto* parameter = enabledParameterToEdit (target);
        return parameter == nullptr
            || parameter->convertFrom0to1 (parameter->getValue()) >= 0.5f;
    }

    std::vector<seqwencer::ModulationTarget> getAssignedTargets() const
    {
        std::vector<seqwencer::ModulationTarget> result;
        for (const auto target : targets)
            if (isTargetAssigned (target))
                result.push_back (target);
        return result;
    }

    void setTargetAssigned (seqwencer::ModulationTarget target,
                            bool shouldAssign)
    {
        setParameterValue (targetParameterToEdit (target),
                           shouldAssign ? 1.0f : 0.0f);
    }

    void setTargetEnabled (seqwencer::ModulationTarget target,
                           bool shouldEnable)
    {
        setParameterValue (enabledParameterToEdit (target),
                           shouldEnable ? 1.0f : 0.0f);
    }

    static void setParameterValue (juce::RangedAudioParameter* parameter,
                                   float value)
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }

    void updateContentSize()
    {
        const auto minimumHeight = juce::jmax (1, viewport.getHeight());
        const auto rowCount = static_cast<int> (getAssignedTargets().size());
        const auto rowContentHeight = rowCount == 0
            ? minimumHeight : rowCount * rowHeight + 10;
        content.setSize (juce::jmax (1, viewport.getWidth()),
                         juce::jmax (minimumHeight, rowContentHeight));
    }

    juce::Rectangle<int> getRowBounds (int rowIndex) const
    {
        return { 6, 5 + rowIndex * rowHeight,
                 juce::jmax (0, content.getWidth() - 12), rowHeight - 2 };
    }

    juce::Rectangle<int> getCheckboxBounds (int rowIndex) const
    {
        const auto row = getRowBounds (rowIndex);
        return { row.getX() + 7, row.getCentreY() - 6, 12, 12 };
    }

    juce::Rectangle<int> getClearBounds (int rowIndex) const
    {
        const auto row = getRowBounds (rowIndex);
        return { row.getRight() - 21, row.getY(), 19, row.getHeight() };
    }

    static constexpr int headerHeight = 23;
    static constexpr int rowHeight = 28;
    int bank = 0;
    juce::Colour accent;
    std::vector<seqwencer::ModulationTarget> targets;
    std::vector<juce::RangedAudioParameter*> ownTargetParameters;
    std::vector<juce::RangedAudioParameter*> ownTargetEnabledParameters;
    std::vector<juce::RangedAudioParameter*> targetAParameters;
    std::vector<juce::RangedAudioParameter*> targetAEnabledParameters;
    juce::RangedAudioParameter* modeParameter = nullptr;
    bool dragHighlighted = false;
    Content content;
    juce::Viewport viewport;
};

class SeqwencerAudioProcessorEditor::PresetBrowser final
    : public juce::Component,
      private juce::ListBoxModel
{
public:
    explicit PresetBrowser (SeqwencerAudioProcessor& audioProcessor)
        : processor (audioProcessor), listBox ("Seqwencer Presets", this)
    {
        title.setText ("SEQWENCER PRESETS", juce::dontSendNotification);
        title.setFont (juce::FontOptions { 18.0f, juce::Font::bold });
        title.setColour (juce::Label::textColourId, juce::Colour (text));

        searchBox.setTextToShowWhenEmpty ("Search presets...",
                                          juce::Colour (mutedText));
        nameBox.setTextToShowWhenEmpty ("Preset name...",
                                        juce::Colour (mutedText));
        for (auto* editor : { &searchBox, &nameBox })
        {
            editor->setColour (juce::TextEditor::backgroundColourId,
                               juce::Colour (0xff111820));
            editor->setColour (juce::TextEditor::textColourId,
                               juce::Colour (text));
            editor->setColour (juce::TextEditor::outlineColourId,
                               juce::Colour (panelOutline));
            addAndMakeVisible (*editor);
        }
        searchBox.onTextChange = [this] { rebuildFilter(); };

        listBox.setRowHeight (28);
        listBox.setColour (juce::ListBox::backgroundColourId,
                           juce::Colour (0xff111820));
        listBox.setColour (juce::ListBox::outlineColourId,
                           juce::Colour (panelOutline));
        listBox.setOutlineThickness (1);
        addAndMakeVisible (listBox);

        loadButton.onClick = [this] { loadSelected(); };
        saveButton.onClick = [this] { saveNamedPreset(); };
        deleteButton.onClick = [this] { confirmDelete(); };
        initialButton.onClick = [this]
        {
            processor.resetToInitialPreset();
            nameBox.setText ("INITIAL", juce::dontSendNotification);
        };
        closeButton.onClick = [this]
        {
            if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
                window->exitModalState (0);
        };

        for (auto* button : { &loadButton, &saveButton, &deleteButton,
                              &initialButton, &closeButton })
        {
            button->setColour (juce::TextButton::buttonColourId,
                               juce::Colour (0xff202a33));
            button->setColour (juce::TextButton::textColourOffId,
                               juce::Colour (text));
            addAndMakeVisible (*button);
        }

        nameBox.setText (processor.getCurrentPresetName(), false);
        refreshFiles();
        setSize (560, 410);
    }

    void paint (juce::Graphics& graphics) override
    {
        graphics.fillAll (juce::Colour (background));
        graphics.setColour (juce::Colour (globalAccent));
        graphics.drawRoundedRectangle (
            getLocalBounds().toFloat().reduced (0.5f), 7.0f, 1.0f);
        graphics.setColour (juce::Colour (mutedText));
        graphics.setFont (juce::FontOptions { 10.0f });
        graphics.drawText (
            "Portable files: Data/Presets",
            18, getHeight() - 54, getWidth() - 36, 16,
            juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        title.setBounds (18, 12, getWidth() - 36, 28);
        searchBox.setBounds (18, 48, getWidth() - 36, 28);
        listBox.setBounds (18, 84, getWidth() - 36, getHeight() - 164);
        nameBox.setBounds (18, getHeight() - 70, 194, 28);
        loadButton.setBounds (220, getHeight() - 70, 58, 28);
        saveButton.setBounds (284, getHeight() - 70, 58, 28);
        deleteButton.setBounds (348, getHeight() - 70, 64, 28);
        initialButton.setBounds (418, getHeight() - 70, 58, 28);
        closeButton.setBounds (482, getHeight() - 70, 60, 28);
    }

private:
    int getNumRows() override
    {
        return static_cast<int> (filteredFiles.size());
    }

    void paintListBoxItem (int rowNumber,
                           juce::Graphics& graphics,
                           int width, int height,
                           bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            graphics.setColour (juce::Colour (globalAccent).withAlpha (0.24f));
            graphics.fillRect (0, 0, width, height);
        }
        if (rowNumber < 0
            || rowNumber >= static_cast<int> (filteredFiles.size()))
            return;
        graphics.setColour (rowIsSelected ? juce::Colour (text)
                                          : juce::Colour (mutedText));
        graphics.setFont (juce::FontOptions { 11.0f,
                                              rowIsSelected
                                                  ? juce::Font::bold : 0 });
        graphics.drawText (
            filteredFiles[static_cast<std::size_t> (rowNumber)]
                .getFileNameWithoutExtension(),
            10, 0, width - 20, height,
            juce::Justification::centredLeft, false);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (lastRowSelected >= 0
            && lastRowSelected < static_cast<int> (filteredFiles.size()))
        {
            nameBox.setText (
                filteredFiles[static_cast<std::size_t> (lastRowSelected)]
                    .getFileNameWithoutExtension(),
                juce::dontSendNotification);
        }
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        listBox.selectRow (row);
        loadSelected();
    }

    static juce::String safeFileName (const juce::String& proposedName)
    {
        const juce::String forbidden { "<>:\"/\\|?*" };
        juce::String result;
        for (const auto character : proposedName.trim())
            result += character < 32 || forbidden.containsChar (character)
                ? '_' : character;
        while (result.endsWithChar ('.') || result.endsWithChar (' '))
            result = result.dropLastCharacters (1);
        return result.substring (0, juce::jmin (48, result.length()));
    }

    void refreshFiles()
    {
        allFiles.clear();
        const auto directory = SeqwencerAudioProcessor::getPortablePresetDirectory();
        directory.createDirectory();
        for (const auto& entry : juce::RangedDirectoryIterator (
                 directory, false, "*.ini", juce::File::findFiles))
            allFiles.push_back (entry.getFile());
        std::sort (allFiles.begin(), allFiles.end(), [] (const auto& left,
                                                         const auto& right)
        {
            return left.getFileName().compareNatural (right.getFileName()) < 0;
        });
        rebuildFilter();
    }

    void rebuildFilter()
    {
        filteredFiles.clear();
        const auto filter = searchBox.getText().trim();
        for (const auto& file : allFiles)
            if (filter.isEmpty()
                || file.getFileNameWithoutExtension().containsIgnoreCase (filter))
                filteredFiles.push_back (file);
        listBox.updateContent();
        listBox.repaint();
    }

    void loadSelected()
    {
        const auto row = listBox.getSelectedRow();
        if (row < 0 || row >= static_cast<int> (filteredFiles.size()))
            return;
        juce::String error;
        const auto& file = filteredFiles[static_cast<std::size_t> (row)];
        if (! processor.loadPortablePreset (file, error))
            showError ("Preset Not Loaded", error);
        else
            nameBox.setText (processor.getCurrentPresetName(), false);
    }

    void saveNamedPreset()
    {
        const auto name = safeFileName (nameBox.getText());
        if (name.isEmpty())
        {
            showError ("Preset Not Saved", "Please enter a preset name.");
            return;
        }
        const auto file = SeqwencerAudioProcessor::getPortablePresetDirectory()
                              .getChildFile (name + ".ini");

        if (file.existsAsFile())
        {
            juce::Component::SafePointer<PresetBrowser> safeThis (this);
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "Overwrite Preset",
                "A preset named " + name + " already exists.\n\nReplace it?",
                "Overwrite", "Cancel", this,
                juce::ModalCallbackFunction::create (
                    [safeThis, file, name] (int result)
                    {
                        if (safeThis != nullptr && result != 0)
                            safeThis->savePresetToFile (file, name);
                    }));
            return;
        }

        savePresetToFile (file, name);
    }

    void savePresetToFile (const juce::File& file, const juce::String& name)
    {
        juce::String error;
        if (! processor.savePortablePreset (file, name, error))
            showError ("Preset Not Saved", error);
        else
            refreshFiles();
    }

    void confirmDelete()
    {
        const auto row = listBox.getSelectedRow();
        if (row < 0 || row >= static_cast<int> (filteredFiles.size()))
            return;
        const auto file = filteredFiles[static_cast<std::size_t> (row)];
        juce::Component::SafePointer<PresetBrowser> safeThis (this);
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Delete Preset",
            "Delete " + file.getFileNameWithoutExtension() + "?",
            "Delete", "Cancel", this,
            juce::ModalCallbackFunction::create (
                [safeThis, file] (int result)
                {
                    if (safeThis == nullptr || result == 0)
                        return;
                    if (! file.deleteFile())
                        safeThis->showError (
                            "Preset Not Deleted",
                            "Seqwencer could not delete:\n"
                                + file.getFullPathName());
                    safeThis->refreshFiles();
                }));
    }

    void showError (const juce::String& titleText,
                    const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            titleText, message, "OK", this);
    }

    SeqwencerAudioProcessor& processor;
    juce::Label title;
    juce::TextEditor searchBox;
    juce::TextEditor nameBox;
    juce::ListBox listBox;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton deleteButton { "DELETE" };
    juce::TextButton initialButton { "INIT" };
    juce::TextButton closeButton { "CLOSE" };
    std::vector<juce::File> allFiles;
    std::vector<juce::File> filteredFiles;
};

SeqwencerAudioProcessorEditor::SeqwencerAudioProcessorEditor (
    SeqwencerAudioProcessor& audioProcessor)
    : AudioProcessorEditor (&audioProcessor), processor (audioProcessor)
{
    const auto storedSettings = loadStoredEditorSettings();
    laneAColour = laneColourFromHue (0, storedSettings.hueA);
    laneBColour = laneColourFromHue (1, storedSettings.hueB);
    lookAndFeel = std::make_unique<SeqwencerLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);

    gridA = std::make_unique<StepGrid> (
        processor, 0, laneAColour);
    gridB = std::make_unique<StepGrid> (
        processor, 1, laneBColour);
    nudgeControlsA = std::make_unique<PatternNudgeControls> (
        *gridA, laneAColour);
    nudgeControlsB = std::make_unique<PatternNudgeControls> (
        *gridB, laneBColour);
    rangeLinkButton = std::make_unique<RangeLinkButton>();
    content.addAndMakeVisible (*gridA);
    content.addAndMakeVisible (*gridB);
    content.addAndMakeVisible (*nudgeControlsA);
    content.addAndMakeVisible (*nudgeControlsB);

    baseParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateLevel);
    depthParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateDepth);
    shortParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::shortGateLength);
    longParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::longGateLength);
    noiseThresholdParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::noiseGateThreshold, "THRESH");
    noiseAttackParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::noiseGateAttack, "ATTACK");
    noiseHoldParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::noiseGateHold, "HOLD");
    noiseReleaseParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::noiseGateRelease, "RELEASE");
    noiseRangeParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::noiseGateRange, "RANGE");
    delayTimeParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::delayTime);
    delayFeedbackParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::delayFeedback, "FDBK");
    delayMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::delayMix);
    reverbSizeParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::reverbSize);
    reverbDampingParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::reverbDamping);
    reverbWidthParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::reverbWidth);
    reverbMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::reverbMix);
    panPositionParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::panPosition);
    filterCutoffParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::filterCutoff);
    filterResonanceParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::filterResonance, "RESO");
    filterMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::filterMix);
    pitchShiftParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::pitchShift);
    pitchMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::pitchMix);
    distortionDriveParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::distortionDrive);
    distortionToneParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::distortionTone);
    distortionMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::distortionMix);
    grainSizeParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::grainSize);
    grainShiftParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::grainShift);
    grainFeedbackParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::grainFeedback, "FDBK");
    grainMixParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::grainMix);
    compressorThresholdParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorThreshold,
            "THRESH");
    compressorRatioParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorRatio);
    compressorAttackParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorAttack);
    compressorReleaseParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorRelease);
    compressorMakeupParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorMakeup);
    compressorMixParameterLabel =
        std::make_unique<ModulationParameterLabel> (
            processor, seqwencer::ModulationTarget::compressorMix);
    attackAParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerAAttack,
        "ATTACK");
    releaseAParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerARelease,
        "RELEASE");
    attackBParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerBAttack,
        "ATTACK");
    releaseBParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerBRelease,
        "RELEASE");
    startParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerStart,
        "START");
    endParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateSequencerEnd,
        "END");
    targetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            gateTargets.begin(), gateTargets.end()), "playback_mode");
    targetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            gateTargets.begin(), gateTargets.end()), "playback_mode");
    delayTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            delayTargets.begin(), delayTargets.end()), "delay_playback_mode");
    delayTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            delayTargets.begin(), delayTargets.end()), "delay_playback_mode");
    reverbTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            reverbTargets.begin(), reverbTargets.end()), "reverb_playback_mode");
    reverbTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            reverbTargets.begin(), reverbTargets.end()), "reverb_playback_mode");
    panTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            panTargets.begin(), panTargets.end()), "pan_playback_mode");
    panTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            panTargets.begin(), panTargets.end()), "pan_playback_mode");
    filterTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            filterTargets.begin(), filterTargets.end()), "filter_playback_mode");
    filterTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            filterTargets.begin(), filterTargets.end()), "filter_playback_mode");
    pitchTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            pitchTargets.begin(), pitchTargets.end()), "pitch_playback_mode");
    pitchTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            pitchTargets.begin(), pitchTargets.end()), "pitch_playback_mode");
    distortionTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            distortionTargets.begin(), distortionTargets.end()),
        "distortion_playback_mode");
    distortionTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            distortionTargets.begin(), distortionTargets.end()),
        "distortion_playback_mode");
    grainTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            grainTargets.begin(), grainTargets.end()), "grain_playback_mode");
    grainTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            grainTargets.begin(), grainTargets.end()), "grain_playback_mode");
    compressorTargetListA = std::make_unique<TargetList> (
        processor, 0, laneAColour,
        std::vector<seqwencer::ModulationTarget> (
            compressorTargets.begin(), compressorTargets.end()),
        "compressor_playback_mode");
    compressorTargetListB = std::make_unique<TargetList> (
        processor, 1, laneBColour,
        std::vector<seqwencer::ModulationTarget> (
            compressorTargets.begin(), compressorTargets.end()),
        "compressor_playback_mode");
    gateFxButton = std::make_unique<FxSelectorButton> (
        "GATE", juce::Colour (gateAccent),
        static_cast<int> (seqwencer::AudioFxStage::gate));
    delayFxButton = std::make_unique<FxSelectorButton> (
        "DELAY", juce::Colour (delayAccent),
        static_cast<int> (seqwencer::AudioFxStage::delay));
    reverbFxButton = std::make_unique<FxSelectorButton> (
        "REVERB", juce::Colour (reverbAccent),
        static_cast<int> (seqwencer::AudioFxStage::reverb));
    panFxButton = std::make_unique<FxSelectorButton> (
        "PAN", juce::Colour (panAccent),
        static_cast<int> (seqwencer::AudioFxStage::pan));
    filterFxButton = std::make_unique<FxSelectorButton> (
        "FILTER", juce::Colour (filterAccent),
        static_cast<int> (seqwencer::AudioFxStage::filter));
    pitchFxButton = std::make_unique<FxSelectorButton> (
        "PITCH", juce::Colour (pitchAccent),
        static_cast<int> (seqwencer::AudioFxStage::pitch));
    distortionFxButton = std::make_unique<FxSelectorButton> (
        "DIST", juce::Colour (distortionAccent),
        static_cast<int> (seqwencer::AudioFxStage::distortion));
    grainFxButton = std::make_unique<FxSelectorButton> (
        "GRAIN", juce::Colour (grainAccent),
        static_cast<int> (seqwencer::AudioFxStage::grain));
    compressorFxButton = std::make_unique<FxSelectorButton> (
        "COMP", juce::Colour (compressorAccent),
        static_cast<int> (seqwencer::AudioFxStage::compressor));
    phiFxButton = std::make_unique<FxSelectorButton> (
        "PHI", juce::Colour (phiAccent));
    content.addAndMakeVisible (*baseParameterLabel);
    content.addAndMakeVisible (*depthParameterLabel);
    content.addAndMakeVisible (*shortParameterLabel);
    content.addAndMakeVisible (*longParameterLabel);
    content.addAndMakeVisible (*noiseThresholdParameterLabel);
    content.addAndMakeVisible (*noiseAttackParameterLabel);
    content.addAndMakeVisible (*noiseHoldParameterLabel);
    content.addAndMakeVisible (*noiseReleaseParameterLabel);
    content.addAndMakeVisible (*noiseRangeParameterLabel);
    content.addAndMakeVisible (*delayTimeParameterLabel);
    content.addAndMakeVisible (*delayFeedbackParameterLabel);
    content.addAndMakeVisible (*delayMixParameterLabel);
    content.addAndMakeVisible (*reverbSizeParameterLabel);
    content.addAndMakeVisible (*reverbDampingParameterLabel);
    content.addAndMakeVisible (*reverbWidthParameterLabel);
    content.addAndMakeVisible (*reverbMixParameterLabel);
    content.addAndMakeVisible (*panPositionParameterLabel);
    content.addAndMakeVisible (*filterCutoffParameterLabel);
    content.addAndMakeVisible (*filterResonanceParameterLabel);
    content.addAndMakeVisible (*filterMixParameterLabel);
    content.addAndMakeVisible (*pitchShiftParameterLabel);
    content.addAndMakeVisible (*pitchMixParameterLabel);
    content.addAndMakeVisible (*distortionDriveParameterLabel);
    content.addAndMakeVisible (*distortionToneParameterLabel);
    content.addAndMakeVisible (*distortionMixParameterLabel);
    content.addAndMakeVisible (*grainSizeParameterLabel);
    content.addAndMakeVisible (*grainShiftParameterLabel);
    content.addAndMakeVisible (*grainFeedbackParameterLabel);
    content.addAndMakeVisible (*grainMixParameterLabel);
    content.addAndMakeVisible (*compressorThresholdParameterLabel);
    content.addAndMakeVisible (*compressorRatioParameterLabel);
    content.addAndMakeVisible (*compressorAttackParameterLabel);
    content.addAndMakeVisible (*compressorReleaseParameterLabel);
    content.addAndMakeVisible (*compressorMakeupParameterLabel);
    content.addAndMakeVisible (*compressorMixParameterLabel);
    content.addAndMakeVisible (*attackAParameterLabel);
    content.addAndMakeVisible (*releaseAParameterLabel);
    content.addAndMakeVisible (*attackBParameterLabel);
    content.addAndMakeVisible (*releaseBParameterLabel);
    content.addAndMakeVisible (*startParameterLabel);
    content.addAndMakeVisible (*endParameterLabel);
    content.addAndMakeVisible (*rangeLinkButton);
    content.addAndMakeVisible (*targetListA);
    content.addAndMakeVisible (*targetListB);
    content.addAndMakeVisible (*delayTargetListA);
    content.addAndMakeVisible (*delayTargetListB);
    content.addAndMakeVisible (*reverbTargetListA);
    content.addAndMakeVisible (*reverbTargetListB);
    content.addAndMakeVisible (*panTargetListA);
    content.addAndMakeVisible (*panTargetListB);
    content.addAndMakeVisible (*filterTargetListA);
    content.addAndMakeVisible (*filterTargetListB);
    content.addAndMakeVisible (*pitchTargetListA);
    content.addAndMakeVisible (*pitchTargetListB);
    content.addAndMakeVisible (*distortionTargetListA);
    content.addAndMakeVisible (*distortionTargetListB);
    content.addAndMakeVisible (*grainTargetListA);
    content.addAndMakeVisible (*grainTargetListB);
    content.addAndMakeVisible (*compressorTargetListA);
    content.addAndMakeVisible (*compressorTargetListB);
    content.addAndMakeVisible (*gateFxButton);
    content.addAndMakeVisible (*delayFxButton);
    content.addAndMakeVisible (*reverbFxButton);
    content.addAndMakeVisible (*panFxButton);
    content.addAndMakeVisible (*filterFxButton);
    content.addAndMakeVisible (*pitchFxButton);
    content.addAndMakeVisible (*distortionFxButton);
    content.addAndMakeVisible (*grainFxButton);
    content.addAndMakeVisible (*compressorFxButton);
    content.addAndMakeVisible (*phiFxButton);
    content.addAndMakeVisible (presetsButton);
    presetsButton.setTooltip ("Open the portable Seqwencer preset browser");
    presetsButton.onClick = [this] { showPresetBrowser(); };

    modeBox.addItem ("PARALLEL", 1);
    modeBox.addItem ("SERIAL", 2);
    content.addAndMakeVisible (modeBox);
    sequenceModeBox.addItem ("LOOP", 1);
    sequenceModeBox.addItem ("BOUNCE", 2);
    sequenceModeBox.addItem ("REVERSE", 3);
    sequenceModeBox.addItem ("PLAYED", 4);
    sequenceModeBox.addItem ("RANDOM", 5);
    sequenceModeBox.setTooltip (
        "Choose forward looping, end-to-end bouncing, reverse playback, first-note phrase retriggering, or shuffled steps");
    content.addAndMakeVisible (sequenceModeBox);
    filterTypeBox.addItem ("LOW PASS", 1);
    filterTypeBox.addItem ("HIGH PASS", 2);
    filterTypeBox.addItem ("BAND PASS", 3);
    filterTypeBox.addItem ("BAND REJECT", 4);
    filterTypeBox.addItem ("PEAKING", 5);
    filterTypeBox.setTooltip ("Choose the Filter response");
    content.addAndMakeVisible (filterTypeBox);
    distortionTypeBox.addItem ("SOFT CLIP", 1);
    distortionTypeBox.addItem ("HARD CLIP", 2);
    distortionTypeBox.addItem ("TUBE", 3);
    distortionTypeBox.addItem ("FOLDBACK", 4);
    distortionTypeBox.setTooltip ("Choose the Distortion waveshaper");
    content.addAndMakeVisible (distortionTypeBox);
    const auto configureWaveformBox = [this] (juce::ComboBox& box,
                                              const juce::String& laneName)
    {
        box.addItem ("SAW", 1);
        box.addItem ("SAW-DOUBLE", 2);
        box.addItem ("SAW DOWN", 3);
        box.addItem ("SAW DOWN-DOUBLE", 4);
        box.addItem ("SINE", 5);
        box.addItem ("SINE-DOUBLE", 6);
        box.addItem ("TRI", 7);
        box.addItem ("TRI-DOUBLE", 8);
        box.addItem ("PULSE 25", 9);
        box.addItem ("PULSE 25-DOUBLE", 10);
        box.addItem ("SQUARE", 11);
        box.addItem ("SQUARE-DOUBLE", 12);
        box.addItem ("PULSE 75", 13);
        box.addItem ("PULSE 75-DOUBLE", 14);
        box.setTextWhenNothingSelected ("WAVEFORM " + laneName);
        box.setTooltip ("Fill all 32 " + laneName
                        + " steps for the selected FX; Double draws two cycles");
        content.addAndMakeVisible (box);
    };
    configureWaveformBox (waveformABox, "A");
    configureWaveformBox (waveformBBox, "B");
    waveformABox.onChange = [this] { applyWaveformPreset (0); };
    waveformBBox.onChange = [this] { applyWaveformPreset (1); };

    for (auto* button : { &syncButton, &phiTargetButton, &noiseGateButton,
                          &enableAButton, &enableBButton,
                          &bipolarAButton, &bipolarBButton })
        content.addAndMakeVisible (*button);

    syncButton.setColour (juce::ToggleButton::tickColourId,
                          juce::Colour (globalAccent));
    noiseGateButton.setColour (juce::ToggleButton::tickColourId,
                               juce::Colour (gateAccent));
    noiseGateButton.setTooltip (
        "Enable the noise gate after the sequenced Gate Volume stage");
    gateFxButton->setTooltip (
        "Show the Gate controls; drag to change its audio-chain position");
    gateFxButton->getEnableButton().setTooltip (
        "Turn the built-in Gate effect on or off");
    gateFxButton->onClick = [this] { selectFx (SelectedFx::gate); };
    delayFxButton->setTooltip (
        "Show the Delay controls; drag to change its audio-chain position");
    delayFxButton->getEnableButton().setTooltip (
        "Turn the built-in Delay effect on or off");
    delayFxButton->onClick = [this] { selectFx (SelectedFx::delay); };
    reverbFxButton->setTooltip (
        "Show the Reverb controls; drag to change its audio-chain position");
    reverbFxButton->getEnableButton().setTooltip (
        "Turn the built-in Reverb effect on or off");
    reverbFxButton->onClick = [this] { selectFx (SelectedFx::reverb); };
    panFxButton->setTooltip (
        "Show the Pan controls; drag to change its audio-chain position");
    panFxButton->getEnableButton().setTooltip (
        "Turn the built-in Pan effect and its sequencer on or off");
    panFxButton->onClick = [this] { selectFx (SelectedFx::pan); };
    filterFxButton->setTooltip (
        "Show the Filter controls; drag to change its audio-chain position");
    filterFxButton->getEnableButton().setTooltip (
        "Turn the built-in Filter effect and its sequencer on or off");
    filterFxButton->onClick = [this] { selectFx (SelectedFx::filter); };
    pitchFxButton->setTooltip (
        "Show the Pitch controls; drag to change its audio-chain position");
    pitchFxButton->getEnableButton().setTooltip (
        "Turn the built-in Pitch effect and its sequencer on or off");
    pitchFxButton->onClick = [this] { selectFx (SelectedFx::pitch); };
    distortionFxButton->setTooltip (
        "Show the Distortion controls; drag to change its audio-chain position");
    distortionFxButton->getEnableButton().setTooltip (
        "Turn the built-in Distortion effect and its sequencer on or off");
    distortionFxButton->onClick = [this] {
        selectFx (SelectedFx::distortion);
    };
    grainFxButton->setTooltip (
        "Show the Grain Shifter controls; drag to change its audio-chain position");
    grainFxButton->getEnableButton().setTooltip (
        "Turn the built-in Grain Shifter effect and its sequencer on or off");
    grainFxButton->onClick = [this] { selectFx (SelectedFx::grain); };
    compressorFxButton->setTooltip (
        "Show the Compressor controls; drag to change its audio-chain position");
    compressorFxButton->getEnableButton().setTooltip (
        "Turn the built-in Compressor effect and its sequencer on or off");
    compressorFxButton->onClick = [this] {
        selectFx (SelectedFx::compressor);
    };
    phiFxButton->setTooltip (
        "Show the PHI integration controls; PHI remains fixed below the audio chain");
    phiFxButton->getEnableButton().setTooltip (
        "Turn Seqwencer's PHI parameter output on or off");
    phiFxButton->onClick = [this] { selectFx (SelectedFx::phi); };
    phiTargetButton.setColour (juce::ToggleButton::tickColourId,
                               juce::Colour (globalAccent));
    phiTargetButton.setClickingTogglesState (false);
    phiTargetButton.setTooltip (
        "Open PHI's searchable parameter target browser");
    phiTargetButton.onClick = [this]
    {
        processor.requestPhiTargetBrowser();
    };
    enableAButton.setColour (juce::ToggleButton::tickColourId,
                             laneAColour);
    enableBButton.setColour (juce::ToggleButton::tickColourId,
                             laneBColour);
    bipolarAButton.setColour (juce::ToggleButton::tickColourId,
                              laneAColour);
    bipolarBButton.setColour (juce::ToggleButton::tickColourId,
                              laneBColour);
    enableAButton.setClickingTogglesState (true);
    enableBButton.setClickingTogglesState (true);
    bipolarAButton.setClickingTogglesState (true);
    bipolarBButton.setClickingTogglesState (true);
    rangeLinkButton->onClick = [this] { handleRangeLinkButton(); };

    configureStepKnob (startSlider);
    configureStepKnob (endSlider);
    configureRateKnob (rateSlider);
    configureKnob (baseSlider);
    configureKnob (depthSlider);
    configureKnob (shortLengthSlider);
    configureKnob (longLengthSlider);
    configureKnob (noiseThresholdSlider);
    configureKnob (noiseAttackSlider);
    configureKnob (noiseHoldSlider);
    configureKnob (noiseReleaseSlider);
    configureKnob (noiseRangeSlider);
    configureKnob (delayTimeSlider);
    configureKnob (delayFeedbackSlider);
    configureKnob (delayMixSlider);
    configureKnob (reverbSizeSlider);
    configureKnob (reverbDampingSlider);
    configureKnob (reverbWidthSlider);
    configureKnob (reverbMixSlider);
    configureKnob (panPositionSlider);
    configureKnob (filterCutoffSlider);
    configureKnob (filterResonanceSlider);
    configureKnob (filterMixSlider);
    configureKnob (pitchShiftSlider);
    configureKnob (pitchMixSlider);
    configureKnob (distortionDriveSlider);
    configureKnob (distortionToneSlider);
    configureKnob (distortionMixSlider);
    configureKnob (grainSizeSlider);
    configureKnob (grainShiftSlider);
    configureKnob (grainFeedbackSlider);
    configureKnob (grainMixSlider);
    configureKnob (compressorThresholdSlider);
    configureKnob (compressorRatioSlider);
    configureKnob (compressorAttackSlider);
    configureKnob (compressorReleaseSlider);
    configureKnob (compressorMakeupSlider);
    configureKnob (compressorMixSlider);
    panPositionSlider.setRange (-1.0, 1.0, 0.001);
    delayTimeSlider.setTextBoxStyle (
        juce::Slider::TextBoxBelow, false, 64, 15);
    shortLengthSlider.setRange (0.10, 0.60, 0.01);
    longLengthSlider.setRange (0.65, 0.95, 0.01);
    noiseThresholdSlider.setRange (-80.0, 0.0, 0.1);
    noiseAttackSlider.setRange (0.1, 100.0, 0.1);
    noiseHoldSlider.setRange (0.0, 500.0, 1.0);
    noiseReleaseSlider.setRange (5.0, 1000.0, 1.0);
    delayTimeSlider.setRange (10.0, 2000.0, 0.1);
    delayFeedbackSlider.setRange (0.0, 0.95, 0.001);
    filterCutoffSlider.setRange (20.0, 20000.0, 0.1);
    filterResonanceSlider.setRange (0.10, 10.0, 0.001);
    pitchShiftSlider.setRange (-24.0, 24.0, 0.01);
    distortionDriveSlider.setRange (0.0, 36.0, 0.01);
    grainSizeSlider.setRange (10.0, 250.0, 0.1);
    grainShiftSlider.setRange (-24.0, 24.0, 0.01);
    grainFeedbackSlider.setRange (0.0, 0.90, 0.001);
    compressorThresholdSlider.setRange (-60.0, 0.0, 0.1);
    compressorRatioSlider.setRange (1.0, 20.0, 0.1);
    compressorAttackSlider.setRange (0.1, 100.0, 0.1);
    compressorReleaseSlider.setRange (10.0, 1000.0, 1.0);
    compressorMakeupSlider.setRange (0.0, 24.0, 0.1);
    shortLengthSlider.setTooltip (
        "Length of every Gate step set to Short (10-60% of one step)");
    longLengthSlider.setTooltip (
        "Length of every Gate step set to Long (65-95% of one step)");
    noiseThresholdSlider.setTooltip (
        "Post-sequencer audio level above which the noise gate opens");
    noiseAttackSlider.setTooltip (
        "Time taken for the noise gate to open");
    noiseHoldSlider.setTooltip (
        "Time the noise gate remains open after the input falls");
    noiseReleaseSlider.setTooltip (
        "Time taken for the noise gate to close");
    noiseRangeSlider.setTooltip (
        "Amount of attenuation while the noise gate is closed");
    delayTimeSlider.setTooltip ("Delay time from 10 to 2000 milliseconds");
    delayFeedbackSlider.setTooltip ("Amount of delayed signal fed back into the delay");
    delayMixSlider.setTooltip ("Balance between dry input and delayed signal");
    reverbSizeSlider.setTooltip ("Size and decay character of the reverb space");
    reverbDampingSlider.setTooltip ("High-frequency absorption inside the reverb");
    reverbWidthSlider.setTooltip ("Stereo width of the reverb field");
    reverbMixSlider.setTooltip ("Balance between dry input and reverberated signal");
    panPositionSlider.setTooltip (
        "Stereo position from full left through centre to full right");
    filterCutoffSlider.setTooltip (
        "Filter cutoff frequency from 20 Hz to 20 kHz");
    filterResonanceSlider.setTooltip (
        "Emphasis around the Filter cutoff frequency");
    filterMixSlider.setTooltip (
        "Balance between dry input and filtered signal");
    pitchShiftSlider.setTooltip (
        "Transpose the signal from two octaves down to two octaves up");
    pitchMixSlider.setTooltip (
        "Balance between dry input and pitch-shifted signal");
    distortionDriveSlider.setTooltip (
        "Input gain into the Distortion waveshaper from 0 to 36 dB");
    distortionToneSlider.setTooltip (
        "Darken or brighten the distorted signal");
    distortionMixSlider.setTooltip (
        "Balance between dry input and distorted signal");
    grainSizeSlider.setTooltip (
        "Length of each overlapping grain from 10 to 250 milliseconds");
    grainShiftSlider.setTooltip (
        "Transpose the grains from two octaves down to two octaves up");
    grainFeedbackSlider.setTooltip (
        "Amount of the shifted grains fed back into the Grain Shifter");
    grainMixSlider.setTooltip (
        "Balance between dry input and grain-shifted signal");
    compressorThresholdSlider.setTooltip (
        "Level above which the Compressor reduces gain");
    compressorRatioSlider.setTooltip (
        "Amount of gain reduction above the Compressor threshold");
    compressorAttackSlider.setTooltip (
        "Time taken for the Compressor to apply gain reduction");
    compressorReleaseSlider.setTooltip (
        "Time taken for the Compressor gain to recover");
    compressorMakeupSlider.setTooltip (
        "Output gain added after compression");
    compressorMixSlider.setTooltip (
        "Balance between dry input and compressed signal");
    configureKnob (attackASlider);
    configureKnob (releaseASlider);
    configureKnob (attackBSlider);
    configureKnob (releaseBSlider);
    configureColourKnob (colourASlider);
    configureColourKnob (colourBSlider);
    colourASlider.setValue (storedSettings.hueA, juce::dontSendNotification);
    colourBSlider.setValue (storedSettings.hueB, juce::dontSendNotification);
    colourASlider.setDoubleClickReturnValue (
        true, juce::Colour (colourA).getHue());
    colourBSlider.setDoubleClickReturnValue (
        true, juce::Colour (colourB).getHue());
    colourASlider.setTooltip ("Choose Sequencer A's interface colour");
    colourBSlider.setTooltip ("Choose Sequencer B's interface colour");
    colourASlider.onValueChange = [this] { updateLaneColours(); };
    colourBSlider.onValueChange = [this] { updateLaneColours(); };
    content.addAndMakeVisible (colourASlider);
    content.addAndMakeVisible (colourBSlider);

    startSlider.setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (gateAccent));
    endSlider.setColour (juce::Slider::rotarySliderFillColourId,
                         juce::Colour (gateAccent));
    rateSlider.setColour (juce::Slider::rotarySliderFillColourId,
                          juce::Colour (gateAccent));
    baseSlider.setColour (juce::Slider::rotarySliderFillColourId,
                          juce::Colour (gateAccent));
    depthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (gateAccent));
    shortLengthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                                 juce::Colour (gateAccent));
    longLengthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                                juce::Colour (gateAccent));
    for (auto* slider : { &noiseThresholdSlider, &noiseAttackSlider,
                          &noiseHoldSlider, &noiseReleaseSlider,
                          &noiseRangeSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (gateAccent));
    }
    for (auto* slider : { &delayTimeSlider, &delayFeedbackSlider,
                          &delayMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (delayAccent));
    }
    for (auto* slider : { &reverbSizeSlider, &reverbDampingSlider,
                          &reverbWidthSlider, &reverbMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (reverbAccent));
    }
    panPositionSlider.setColour (juce::Slider::rotarySliderFillColourId,
                                 juce::Colour (panAccent));
    for (auto* slider : { &filterCutoffSlider, &filterResonanceSlider,
                          &filterMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (filterAccent));
    }
    for (auto* slider : { &pitchShiftSlider, &pitchMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (pitchAccent));
    }
    for (auto* slider : { &distortionDriveSlider, &distortionToneSlider,
                          &distortionMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (distortionAccent));
    }
    for (auto* slider : { &grainSizeSlider, &grainShiftSlider,
                          &grainFeedbackSlider, &grainMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (grainAccent));
    }
    for (auto* slider : { &compressorThresholdSlider, &compressorRatioSlider,
                          &compressorAttackSlider, &compressorReleaseSlider,
                          &compressorMakeupSlider, &compressorMixSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (compressorAccent));
    }
    attackASlider.setColour (juce::Slider::rotarySliderFillColourId,
                             laneAColour);
    releaseASlider.setColour (juce::Slider::rotarySliderFillColourId,
                              laneAColour);
    attackBSlider.setColour (juce::Slider::rotarySliderFillColourId,
                             laneBColour);
    releaseBSlider.setColour (juce::Slider::rotarySliderFillColourId,
                              laneBColour);

    configureLabel (rateLabel, "RATE");
    configureLabel (sequenceModeLabel, "DIRECTION");
    configureLabel (laneATitle, "GATE A");
    configureLabel (laneBTitle, "GATE B");
    configureLabel (colourALabel, "A COLOUR");
    configureLabel (colourBLabel, "B COLOUR");
    configureLabel (filterTypeLabel, "TYPE");
    configureLabel (distortionTypeLabel, "TYPE");
    laneATitle.setColour (juce::Label::textColourId,
                          juce::Colour (gateAccent));
    laneBTitle.setColour (juce::Label::textColourId,
                          juce::Colour (gateAccent));

    auto& state = processor.getParameterState();
    enableAButton.onClick = [this] { handleLaneButton (0); };
    enableBButton.onClick = [this] { handleLaneButton (1); };
    bipolarAButton.onClick = [this] { handleBipolarButton (0); };
    bipolarBButton.onClick = [this] { handleBipolarButton (1); };

    syncAttachment = std::make_unique<ButtonAttachment> (
        state, "sync_to_host", syncButton);
    gateAttachment = std::make_unique<ButtonAttachment> (
        state, "gate_enabled", gateFxButton->getEnableButton());
    delayAttachment = std::make_unique<ButtonAttachment> (
        state, "delay_enabled", delayFxButton->getEnableButton());
    reverbAttachment = std::make_unique<ButtonAttachment> (
        state, "reverb_enabled", reverbFxButton->getEnableButton());
    panAttachment = std::make_unique<ButtonAttachment> (
        state, "pan_enabled", panFxButton->getEnableButton());
    filterAttachment = std::make_unique<ButtonAttachment> (
        state, "filter_enabled", filterFxButton->getEnableButton());
    pitchAttachment = std::make_unique<ButtonAttachment> (
        state, "pitch_enabled", pitchFxButton->getEnableButton());
    distortionAttachment = std::make_unique<ButtonAttachment> (
        state, "distortion_enabled", distortionFxButton->getEnableButton());
    grainAttachment = std::make_unique<ButtonAttachment> (
        state, "grain_enabled", grainFxButton->getEnableButton());
    compressorAttachment = std::make_unique<ButtonAttachment> (
        state, "compressor_enabled", compressorFxButton->getEnableButton());
    phiBridgeAttachment = std::make_unique<ButtonAttachment> (
        state, "phi_bridge_enabled", phiFxButton->getEnableButton());
    noiseGateAttachment = std::make_unique<ButtonAttachment> (
        state, "noise_gate_enabled", noiseGateButton);
    filterTypeAttachment = std::make_unique<ComboAttachment> (
        state, "filter_type", filterTypeBox);
    distortionTypeAttachment = std::make_unique<ComboAttachment> (
        state, "distortion_type", distortionTypeBox);
    baseAttachment = std::make_unique<SliderAttachment> (
        state, "gate_base", baseSlider);
    depthAttachment = std::make_unique<SliderAttachment> (
        state, "gate_depth", depthSlider);
    shortLengthAttachment = std::make_unique<SliderAttachment> (
        state, "gate_short_length", shortLengthSlider);
    longLengthAttachment = std::make_unique<SliderAttachment> (
        state, "gate_long_length", longLengthSlider);
    noiseThresholdAttachment = std::make_unique<SliderAttachment> (
        state, "noise_gate_threshold", noiseThresholdSlider);
    noiseAttackAttachment = std::make_unique<SliderAttachment> (
        state, "noise_gate_attack", noiseAttackSlider);
    noiseHoldAttachment = std::make_unique<SliderAttachment> (
        state, "noise_gate_hold", noiseHoldSlider);
    noiseReleaseAttachment = std::make_unique<SliderAttachment> (
        state, "noise_gate_release", noiseReleaseSlider);
    noiseRangeAttachment = std::make_unique<SliderAttachment> (
        state, "noise_gate_range", noiseRangeSlider);
    delayTimeAttachment = std::make_unique<SliderAttachment> (
        state, "delay_time", delayTimeSlider);
    delayFeedbackAttachment = std::make_unique<SliderAttachment> (
        state, "delay_feedback", delayFeedbackSlider);
    delayMixAttachment = std::make_unique<SliderAttachment> (
        state, "delay_mix", delayMixSlider);
    reverbSizeAttachment = std::make_unique<SliderAttachment> (
        state, "reverb_size", reverbSizeSlider);
    reverbDampingAttachment = std::make_unique<SliderAttachment> (
        state, "reverb_damping", reverbDampingSlider);
    reverbWidthAttachment = std::make_unique<SliderAttachment> (
        state, "reverb_width", reverbWidthSlider);
    reverbMixAttachment = std::make_unique<SliderAttachment> (
        state, "reverb_mix", reverbMixSlider);
    panPositionAttachment = std::make_unique<SliderAttachment> (
        state, "pan_position", panPositionSlider);
    filterCutoffAttachment = std::make_unique<SliderAttachment> (
        state, "filter_cutoff", filterCutoffSlider);
    filterResonanceAttachment = std::make_unique<SliderAttachment> (
        state, "filter_resonance", filterResonanceSlider);
    filterMixAttachment = std::make_unique<SliderAttachment> (
        state, "filter_mix", filterMixSlider);
    pitchShiftAttachment = std::make_unique<SliderAttachment> (
        state, "pitch_shift", pitchShiftSlider);
    pitchMixAttachment = std::make_unique<SliderAttachment> (
        state, "pitch_mix", pitchMixSlider);
    distortionDriveAttachment = std::make_unique<SliderAttachment> (
        state, "distortion_drive", distortionDriveSlider);
    distortionToneAttachment = std::make_unique<SliderAttachment> (
        state, "distortion_tone", distortionToneSlider);
    distortionMixAttachment = std::make_unique<SliderAttachment> (
        state, "distortion_mix", distortionMixSlider);
    grainSizeAttachment = std::make_unique<SliderAttachment> (
        state, "grain_size", grainSizeSlider);
    grainShiftAttachment = std::make_unique<SliderAttachment> (
        state, "grain_shift", grainShiftSlider);
    grainFeedbackAttachment = std::make_unique<SliderAttachment> (
        state, "grain_feedback", grainFeedbackSlider);
    grainMixAttachment = std::make_unique<SliderAttachment> (
        state, "grain_mix", grainMixSlider);
    compressorThresholdAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_threshold", compressorThresholdSlider);
    compressorRatioAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_ratio", compressorRatioSlider);
    compressorAttackAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_attack", compressorAttackSlider);
    compressorReleaseAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_release", compressorReleaseSlider);
    compressorMakeupAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_makeup", compressorMakeupSlider);
    compressorMixAttachment = std::make_unique<SliderAttachment> (
        state, "compressor_mix", compressorMixSlider);
    panPositionSlider.textFromValueFunction = [] (double value)
    {
        if (std::abs (value) < 0.0005)
            return juce::String ("C");
        return juce::String (std::lround (std::abs (value) * 100.0))
             + (value < 0.0 ? " L" : " R");
    };
    filterCutoffSlider.textFromValueFunction = [] (double value)
    {
        return value >= 1000.0
            ? juce::String (value / 1000.0, value < 10000.0 ? 2 : 1) + " kHz"
            : juce::String (value, 0) + " Hz";
    };
    filterResonanceSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 2);
    };
    pitchShiftSlider.textFromValueFunction = [] (double value)
    {
        if (std::abs (value) < 0.005)
            return juce::String ("0.00 st");
        return juce::String (value > 0.0 ? "+" : "")
             + juce::String (value, 2) + " st";
    };
    distortionDriveSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 2) + " dB";
    };
    grainSizeSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, value < 100.0 ? 1 : 0) + " ms";
    };
    grainShiftSlider.textFromValueFunction = [] (double value)
    {
        if (std::abs (value) < 0.005)
            return juce::String ("0.00 st");
        return juce::String (value > 0.0 ? "+" : "")
             + juce::String (value, 2) + " st";
    };
    grainFeedbackSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (std::lround (value * 100.0)) + "%";
    };
    compressorThresholdSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " dB";
    };
    compressorRatioSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + ":1";
    };
    compressorAttackSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " ms";
    };
    compressorReleaseSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 0) + " ms";
    };
    compressorMakeupSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " dB";
    };
    const auto gateLengthText = [] (double value)
    {
        return juce::String (std::lround (value * 100.0)) + "%";
    };
    shortLengthSlider.textFromValueFunction = gateLengthText;
    longLengthSlider.textFromValueFunction = gateLengthText;
    noiseThresholdSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " dB";
    };
    const auto millisecondsText = [] (double value)
    {
        return juce::String (value, value < 10.0 ? 1 : 0) + " ms";
    };
    noiseAttackSlider.textFromValueFunction = millisecondsText;
    noiseHoldSlider.textFromValueFunction = millisecondsText;
    noiseReleaseSlider.textFromValueFunction = millisecondsText;
    delayTimeSlider.textFromValueFunction = millisecondsText;
    delayFeedbackSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (std::lround (value * 100.0)) + "%";
    };
    startSlider.onValueChange = [this]
    {
        if (startStepParameter != nullptr)
            handleRangeControl (true);
    };
    endSlider.onValueChange = [this]
    {
        if (endStepParameter != nullptr && rangeLengthParameter != nullptr)
            handleRangeControl (false);
    };
    startSlider.onDragStart = [this]
    {
        if (startStepParameter != nullptr)
            startStepParameter->beginChangeGesture();
    };
    startSlider.onDragEnd = [this]
    {
        if (startStepParameter != nullptr)
            startStepParameter->endChangeGesture();
    };
    endSlider.onDragStart = [this]
    {
        auto* parameter = rangeLinkButton->getToggleState()
            ? rangeLengthParameter : endStepParameter;
        if (parameter != nullptr)
            parameter->beginChangeGesture();
    };
    endSlider.onDragEnd = [this]
    {
        auto* parameter = rangeLinkButton->getToggleState()
            ? rangeLengthParameter : endStepParameter;
        if (parameter != nullptr)
            parameter->endChangeGesture();
    };

    setResizable (true, true);
    setResizeLimits (960, 510, 1600, 850);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio (
            static_cast<double> (designWidth)
                / static_cast<double> (designHeight));
    setSize (storedSettings.size.x, storedSettings.size.y);
    bindSelectedEngine();
    updateRangeControls();
    updateFxPanel();
    updateLaneColours();
    updateLaneVisuals();
    startTimerHz (30);
}

SeqwencerAudioProcessorEditor::~SeqwencerAudioProcessorEditor()
{
    stopTimer();
    saveStoredEditorSettings (
        getWidth(), getHeight(),
        static_cast<float> (colourASlider.getValue()),
        static_cast<float> (colourBSlider.getValue()));
    if (presetWindow != nullptr)
        presetWindow->exitModalState (0);
    setLookAndFeel (nullptr);
}

void SeqwencerAudioProcessorEditor::showPresetBrowser()
{
    if (presetWindow != nullptr)
    {
        presetWindow->toFront (true);
        return;
    }

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Seqwencer Preset Browser";
    options.content.setOwned (new PresetBrowser (processor));
    options.componentToCentreAround = this;
    options.dialogBackgroundColour = juce::Colour (background);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    presetWindow = options.launchAsync();
}

void SeqwencerAudioProcessorEditor::configureKnob (juce::Slider& slider,
                                                   const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 15);
    slider.textFromValueFunction = [suffix] (double value)
    {
        return juce::String (std::lround (value * 100.0)) + suffix;
    };
    content.addAndMakeVisible (slider);
}

void SeqwencerAudioProcessorEditor::configureColourKnob (
    juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setRange (0.0, 1.0, 0.001);
    // Reserve the same 15-pixel lower strip as every bottom-row knob. The
    // value remains intentionally hidden, but JUCE now gives both sets of
    // rotary dials identical drawing geometry.
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 54, 15);
    slider.setColour (juce::Slider::textBoxTextColourId,
                      juce::Colours::transparentBlack);
    slider.textFromValueFunction = [] (double) { return juce::String {}; };
}

void SeqwencerAudioProcessorEditor::configureStepKnob (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRange (1.0, 64.0, 1.0);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 15);
    slider.textFromValueFunction = [] (double value)
    {
        return juce::String (static_cast<int> (std::lround (value)));
    };
    content.addAndMakeVisible (slider);
}

void SeqwencerAudioProcessorEditor::configureRateKnob (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 15);
    content.addAndMakeVisible (slider);
}

void SeqwencerAudioProcessorEditor::configureLabel (juce::Label& label,
                                                    const juce::String& labelText)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    content.addAndMakeVisible (label);
}

void SeqwencerAudioProcessorEditor::handleRangeControl (bool movingStart)
{
    if (updatingRangeControls)
        return;

    auto* parameter = movingStart
        ? startStepParameter
        : (rangeLinkButton->getToggleState()
               ? rangeLengthParameter : endStepParameter);
    if (parameter == nullptr)
        return;

    const auto value = movingStart ? startSlider.getValue()
                                   : endSlider.getValue();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (
        static_cast<float> (std::lround (value))));
    updateRangeControls();
}

void SeqwencerAudioProcessorEditor::handleRangeLinkButton()
{
    const auto readActual = [] (const juce::RangedAudioParameter* parameter,
                                float fallback)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : fallback;
    };
    const auto setActual = [] (juce::RangedAudioParameter* parameter,
                               float value)
    {
        if (parameter == nullptr)
            return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    };

    const auto serial = readActual (modeParameter, 0.0f) >= 0.5f;
    const auto maximumStep = serial ? seqwencer::linkedStepCount
                                    : seqwencer::stepsPerBank;
    const auto start = static_cast<int> (std::lround (
        readActual (startStepParameter, 1.0f)));

    if (rangeLinkButton->getToggleState())
    {
        const auto range = seqwencer::makeStepRange (
            start,
            static_cast<int> (std::lround (
                readActual (endStepParameter, 64.0f))),
            maximumStep);
        setActual (rangeLengthParameter, static_cast<float> (range.length()));
    }
    else
    {
        const auto range = seqwencer::makeLengthStepRange (
            start,
            static_cast<int> (std::lround (
                readActual (rangeLengthParameter, 64.0f))),
            maximumStep);
        setActual (endStepParameter, static_cast<float> (range.last + 1));
    }
    updateRangeControls();
}

void SeqwencerAudioProcessorEditor::updateRangeControls()
{
    const auto readActual = [] (const juce::RangedAudioParameter* parameter,
                                float fallback)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : fallback;
    };

    const auto serial = readActual (modeParameter, 0.0f) >= 0.5f;
    const auto maximumStep = serial ? seqwencer::linkedStepCount
                                    : seqwencer::stepsPerBank;
    const auto linked = rangeLinkButton != nullptr
                     && rangeLinkButton->getToggleState();
    const auto start = static_cast<int> (std::lround (
        readActual (startStepParameter, 1.0f)));
    const auto range = linked
        ? seqwencer::makeLengthStepRange (
            start,
            static_cast<int> (std::lround (
                readActual (rangeLengthParameter, 64.0f))),
            maximumStep)
        : seqwencer::makeStepRange (
            start,
            static_cast<int> (std::lround (
                readActual (endStepParameter, 64.0f))),
            maximumStep);

    const juce::ScopedValueSetter<bool> guard (updatingRangeControls, true);
    startSlider.setRange (
        1.0,
        static_cast<double> (linked
            ? juce::jmax (1, maximumStep - range.length() + 1)
            : juce::jmin (range.last + 1, maximumStep - 1)),
        1.0);
    endSlider.setRange (
        static_cast<double> (linked ? 2 : juce::jmax (2, range.first + 1)),
        static_cast<double> (linked ? maximumStep - range.first : maximumStep),
        1.0);
    startSlider.setValue (static_cast<double> (range.first + 1),
                          juce::dontSendNotification);
    endSlider.setValue (static_cast<double> (
                            linked ? range.length() : range.last + 1),
                        juce::dontSendNotification);

    if (rangeLabelShowsLength != linked)
    {
        rangeLabelShowsLength = linked;
        endParameterLabel->setTarget (
            boundRangeTargets[static_cast<std::size_t> (linked ? 2 : 1)],
            linked ? "LENGTH" : "END");
    }
}

void SeqwencerAudioProcessorEditor::handleLaneButton (int lane)
{
    const auto readActual = [] (const juce::RangedAudioParameter* parameter)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : 0.0f;
    };
    const auto setActual = [] (juce::RangedAudioParameter* parameter, float value)
    {
        if (parameter == nullptr)
            return;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    };

    const auto serial = readActual (modeParameter) >= 0.5f;
    if (serial)
    {
        setActual (serialProfileParameter, lane == 0 ? 0.0f : 1.0f);
        enableAButton.setToggleState (lane == 0, juce::dontSendNotification);
        enableBButton.setToggleState (lane == 1, juce::dontSendNotification);
    }
    else
    {
        auto& button = lane == 0 ? enableAButton : enableBButton;
        auto* parameter = lane == 0 ? seqAEnabledParameter : seqBEnabledParameter;
        setActual (parameter, button.getToggleState() ? 1.0f : 0.0f);
    }

    updateLaneVisuals();
}

void SeqwencerAudioProcessorEditor::handleBipolarButton (int lane)
{
    auto* parameter = lane == 0 ? bipolarAParameter : bipolarBParameter;
    auto& button = lane == 0 ? bipolarAButton : bipolarBButton;
    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (
        button.getToggleState() ? 1.0f : 0.0f));
    parameter->endChangeGesture();
    updateLaneVisuals();
}

SeqwencerAudioProcessorEditor::FxSelectorButton*
SeqwencerAudioProcessorEditor::buttonForAudioFxStage (
    seqwencer::AudioFxStage stage) const noexcept
{
    switch (stage)
    {
        case seqwencer::AudioFxStage::gate:       return gateFxButton.get();
        case seqwencer::AudioFxStage::delay:      return delayFxButton.get();
        case seqwencer::AudioFxStage::reverb:     return reverbFxButton.get();
        case seqwencer::AudioFxStage::pan:        return panFxButton.get();
        case seqwencer::AudioFxStage::filter:     return filterFxButton.get();
        case seqwencer::AudioFxStage::pitch:      return pitchFxButton.get();
        case seqwencer::AudioFxStage::distortion: return distortionFxButton.get();
        case seqwencer::AudioFxStage::grain:      return grainFxButton.get();
        case seqwencer::AudioFxStage::compressor: return compressorFxButton.get();
    }
    return nullptr;
}

void SeqwencerAudioProcessorEditor::updateFxSelectorBounds()
{
    displayedAudioFxOrder = processor.getAudioFxOrder();
    constexpr auto buttonTop = 182;
    constexpr auto buttonSpacing = 39;
    for (int index = 0; index < seqwencer::audioFxStageCount; ++index)
        if (auto* button = buttonForAudioFxStage (
                displayedAudioFxOrder[static_cast<std::size_t> (index)]))
            button->setBounds (
                20, buttonTop + buttonSpacing * index, 74, 31);
    phiFxButton->setBounds (20, 533, 74, 31);
}

bool SeqwencerAudioProcessorEditor::isInterestedInDragSource (
    const juce::DragAndDropTarget::SourceDetails& details)
{
    const auto description = details.description.toString();
    if (! description.startsWith ("seqwencer-fx:"))
        return false;
    const auto stage = description.fromFirstOccurrenceOf (
        ":", false, false).getIntValue();
    return stage >= 0 && stage < seqwencer::audioFxStageCount;
}

void SeqwencerAudioProcessorEditor::itemDragMove (
    const juce::DragAndDropTarget::SourceDetails& details)
{
    if (! isInterestedInDragSource (details))
        return;

    const auto designPosition = content.getLocalPoint (
        this, details.localPosition);
    auto newDropIndex = -1;
    if (designPosition.x >= 8 && designPosition.x <= 106
        && designPosition.y >= 166 && designPosition.y <= 529)
    {
        newDropIndex = juce::jlimit (
            0, seqwencer::audioFxStageCount - 1,
            static_cast<int> (std::lround (
                (static_cast<double> (designPosition.y) - 182.0) / 39.0)));
    }

    if (newDropIndex != fxDropIndex)
    {
        fxDropIndex = newDropIndex;
        repaint();
    }
}

void SeqwencerAudioProcessorEditor::itemDragExit (
    const juce::DragAndDropTarget::SourceDetails&)
{
    if (fxDropIndex >= 0)
    {
        fxDropIndex = -1;
        repaint();
    }
}

void SeqwencerAudioProcessorEditor::itemDropped (
    const juce::DragAndDropTarget::SourceDetails& details)
{
    const auto destination = fxDropIndex;
    fxDropIndex = -1;
    if (! isInterestedInDragSource (details) || destination < 0)
    {
        repaint();
        return;
    }

    const auto stageValue = details.description.toString()
        .fromFirstOccurrenceOf (":", false, false).getIntValue();
    processor.moveAudioFxStage (
        static_cast<seqwencer::AudioFxStage> (stageValue), destination);
    updateFxSelectorBounds();
    repaint();
}

void SeqwencerAudioProcessorEditor::selectFx (SelectedFx fx)
{
    if (fx == SelectedFx::phi && ! processor.isPhiHostPresent())
        return;

    if (selectedFx == fx)
        return;

    selectedFx = fx;
    bindSelectedEngine();
    updateFxPanel();
    resized();
    repaint();
}

void SeqwencerAudioProcessorEditor::bindSelectedEngine()
{
    if (bindingsInitialised && boundFx == selectedFx)
        return;

    auto& state = processor.getParameterState();
    const auto parameterID = [this] (const juce::String& gateID)
    {
        if (selectedFx == SelectedFx::phi)
            return "phi_" + gateID;
        if (selectedFx == SelectedFx::delay)
            return "delay_" + gateID;
        if (selectedFx == SelectedFx::reverb)
            return "reverb_" + gateID;
        if (selectedFx == SelectedFx::pan)
            return "pan_" + gateID;
        if (selectedFx == SelectedFx::filter)
            return "filter_" + gateID;
        if (selectedFx == SelectedFx::pitch)
            return "pitch_" + gateID;
        if (selectedFx == SelectedFx::distortion)
            return "distortion_" + gateID;
        if (selectedFx == SelectedFx::grain)
            return "grain_" + gateID;
        if (selectedFx == SelectedFx::compressor)
            return "compressor_" + gateID;
        return gateID;
    };

    modeAttachment.reset();
    sequenceModeAttachment.reset();
    rateAttachment.reset();
    rangeLinkAttachment.reset();
    attackAAttachment.reset();
    releaseAAttachment.reset();
    attackBAttachment.reset();
    releaseBAttachment.reset();

    modeParameter = state.getParameter (parameterID ("playback_mode"));
    seqAEnabledParameter = state.getParameter (parameterID ("seq_a_enabled"));
    seqBEnabledParameter = state.getParameter (parameterID ("seq_b_enabled"));
    serialProfileParameter = state.getParameter (parameterID ("serial_profile"));
    startStepParameter = state.getParameter (parameterID ("start_step"));
    endStepParameter = state.getParameter (parameterID ("end_step"));
    rangeLengthParameter = state.getParameter (parameterID ("range_length"));
    rangeLinkParameter = state.getParameter (parameterID ("range_link"));
    bipolarAParameter = state.getParameter (parameterID ("seq_a_bipolar"));
    bipolarBParameter = state.getParameter (parameterID ("seq_b_bipolar"));

    modeAttachment = std::make_unique<ComboAttachment> (
        state, parameterID ("playback_mode"), modeBox);
    sequenceModeAttachment = std::make_unique<ComboAttachment> (
        state, parameterID ("sequence_mode"), sequenceModeBox);
    rateAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("rate"), rateSlider);
    rangeLinkAttachment = std::make_unique<ButtonAttachment> (
        state, parameterID ("range_link"), *rangeLinkButton);
    attackAAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_a_attack"), attackASlider);
    releaseAAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_a_release"), releaseASlider);
    attackBAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_b_attack"), attackBSlider);
    releaseBAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_b_release"), releaseBSlider);

    const auto engine = selectedFx == SelectedFx::phi
        ? seqwencer::SequencerEngine::phi
        : selectedFx == SelectedFx::delay
            ? seqwencer::SequencerEngine::delay
            : selectedFx == SelectedFx::reverb
                ? seqwencer::SequencerEngine::reverb
            : selectedFx == SelectedFx::pan
                ? seqwencer::SequencerEngine::pan
            : selectedFx == SelectedFx::filter
                ? seqwencer::SequencerEngine::filter
            : selectedFx == SelectedFx::pitch
                ? seqwencer::SequencerEngine::pitch
            : selectedFx == SelectedFx::distortion
                ? seqwencer::SequencerEngine::distortion
            : selectedFx == SelectedFx::grain
                ? seqwencer::SequencerEngine::grain
            : selectedFx == SelectedFx::compressor
                ? seqwencer::SequencerEngine::compressor
                : seqwencer::SequencerEngine::gate;
    const auto envelopeTargets = seqwencer::sequencerEnvelopeTargets (engine);
    boundRangeTargets = seqwencer::sequencerRangeTargets (engine);
    attackAParameterLabel->setTarget (envelopeTargets[0], "ATTACK");
    releaseAParameterLabel->setTarget (envelopeTargets[1], "RELEASE");
    attackBParameterLabel->setTarget (envelopeTargets[2], "ATTACK");
    releaseBParameterLabel->setTarget (envelopeTargets[3], "RELEASE");
    startParameterLabel->setTarget (boundRangeTargets[0], "START");
    rangeLabelShowsLength = rangeLinkButton->getToggleState();
    endParameterLabel->setTarget (
        boundRangeTargets[static_cast<std::size_t> (
            rangeLabelShowsLength ? 2 : 1)],
        rangeLabelShowsLength ? "LENGTH" : "END");
    gridA->setEngine (engine);
    gridB->setEngine (engine);
    boundFx = selectedFx;
    bindingsInitialised = true;
    updateRangeControls();
    updateLaneVisuals();
}

void SeqwencerAudioProcessorEditor::updateFxPanel()
{
    const auto phiAvailable = processor.isPhiHostPresent();
    if (! phiAvailable && selectedFx == SelectedFx::phi)
    {
        selectedFx = SelectedFx::gate;
        bindSelectedEngine();
        resized();
    }

    const auto gateSelected = selectedFx == SelectedFx::gate;
    const auto delaySelected = selectedFx == SelectedFx::delay;
    const auto reverbSelected = selectedFx == SelectedFx::reverb;
    const auto panSelected = selectedFx == SelectedFx::pan;
    const auto filterSelected = selectedFx == SelectedFx::filter;
    const auto pitchSelected = selectedFx == SelectedFx::pitch;
    const auto distortionSelected = selectedFx == SelectedFx::distortion;
    const auto grainSelected = selectedFx == SelectedFx::grain;
    const auto compressorSelected = selectedFx == SelectedFx::compressor;
    const auto phiSelected = selectedFx == SelectedFx::phi;
    gateFxButton->setSelected (gateSelected);
    delayFxButton->setSelected (delaySelected);
    reverbFxButton->setSelected (reverbSelected);
    panFxButton->setSelected (panSelected);
    filterFxButton->setSelected (filterSelected);
    pitchFxButton->setSelected (pitchSelected);
    distortionFxButton->setSelected (distortionSelected);
    grainFxButton->setSelected (grainSelected);
    compressorFxButton->setSelected (compressorSelected);
    phiFxButton->setSelected (phiSelected);
    phiFxButton->setVisible (phiAvailable);

    baseParameterLabel->setVisible (gateSelected);
    baseSlider.setVisible (gateSelected);
    depthParameterLabel->setVisible (gateSelected);
    depthSlider.setVisible (gateSelected);
    shortParameterLabel->setVisible (gateSelected);
    shortLengthSlider.setVisible (gateSelected);
    longParameterLabel->setVisible (gateSelected);
    longLengthSlider.setVisible (gateSelected);
    noiseGateButton.setVisible (gateSelected);
    noiseThresholdParameterLabel->setVisible (gateSelected);
    noiseThresholdSlider.setVisible (gateSelected);
    noiseAttackParameterLabel->setVisible (gateSelected);
    noiseAttackSlider.setVisible (gateSelected);
    noiseHoldParameterLabel->setVisible (gateSelected);
    noiseHoldSlider.setVisible (gateSelected);
    noiseReleaseParameterLabel->setVisible (gateSelected);
    noiseReleaseSlider.setVisible (gateSelected);
    noiseRangeParameterLabel->setVisible (gateSelected);
    noiseRangeSlider.setVisible (gateSelected);
    targetListA->setVisible (gateSelected);
    targetListB->setVisible (gateSelected);
    delayTimeParameterLabel->setVisible (delaySelected);
    delayTimeSlider.setVisible (delaySelected);
    delayFeedbackParameterLabel->setVisible (delaySelected);
    delayFeedbackSlider.setVisible (delaySelected);
    delayMixParameterLabel->setVisible (delaySelected);
    delayMixSlider.setVisible (delaySelected);
    delayTargetListA->setVisible (delaySelected);
    delayTargetListB->setVisible (delaySelected);
    reverbSizeParameterLabel->setVisible (reverbSelected);
    reverbSizeSlider.setVisible (reverbSelected);
    reverbDampingParameterLabel->setVisible (reverbSelected);
    reverbDampingSlider.setVisible (reverbSelected);
    reverbWidthParameterLabel->setVisible (reverbSelected);
    reverbWidthSlider.setVisible (reverbSelected);
    reverbMixParameterLabel->setVisible (reverbSelected);
    reverbMixSlider.setVisible (reverbSelected);
    reverbTargetListA->setVisible (reverbSelected);
    reverbTargetListB->setVisible (reverbSelected);
    panPositionParameterLabel->setVisible (panSelected);
    panPositionSlider.setVisible (panSelected);
    panTargetListA->setVisible (panSelected);
    panTargetListB->setVisible (panSelected);
    filterTypeLabel.setVisible (filterSelected);
    filterTypeBox.setVisible (filterSelected);
    filterCutoffParameterLabel->setVisible (filterSelected);
    filterCutoffSlider.setVisible (filterSelected);
    filterResonanceParameterLabel->setVisible (filterSelected);
    filterResonanceSlider.setVisible (filterSelected);
    filterMixParameterLabel->setVisible (filterSelected);
    filterMixSlider.setVisible (filterSelected);
    filterTargetListA->setVisible (filterSelected);
    filterTargetListB->setVisible (filterSelected);
    pitchShiftParameterLabel->setVisible (pitchSelected);
    pitchShiftSlider.setVisible (pitchSelected);
    pitchMixParameterLabel->setVisible (pitchSelected);
    pitchMixSlider.setVisible (pitchSelected);
    pitchTargetListA->setVisible (pitchSelected);
    pitchTargetListB->setVisible (pitchSelected);
    distortionTypeLabel.setVisible (distortionSelected);
    distortionTypeBox.setVisible (distortionSelected);
    distortionDriveParameterLabel->setVisible (distortionSelected);
    distortionDriveSlider.setVisible (distortionSelected);
    distortionToneParameterLabel->setVisible (distortionSelected);
    distortionToneSlider.setVisible (distortionSelected);
    distortionMixParameterLabel->setVisible (distortionSelected);
    distortionMixSlider.setVisible (distortionSelected);
    distortionTargetListA->setVisible (distortionSelected);
    distortionTargetListB->setVisible (distortionSelected);
    grainSizeParameterLabel->setVisible (grainSelected);
    grainSizeSlider.setVisible (grainSelected);
    grainShiftParameterLabel->setVisible (grainSelected);
    grainShiftSlider.setVisible (grainSelected);
    grainFeedbackParameterLabel->setVisible (grainSelected);
    grainFeedbackSlider.setVisible (grainSelected);
    grainMixParameterLabel->setVisible (grainSelected);
    grainMixSlider.setVisible (grainSelected);
    grainTargetListA->setVisible (grainSelected);
    grainTargetListB->setVisible (grainSelected);
    compressorThresholdParameterLabel->setVisible (compressorSelected);
    compressorThresholdSlider.setVisible (compressorSelected);
    compressorRatioParameterLabel->setVisible (compressorSelected);
    compressorRatioSlider.setVisible (compressorSelected);
    compressorAttackParameterLabel->setVisible (compressorSelected);
    compressorAttackSlider.setVisible (compressorSelected);
    compressorReleaseParameterLabel->setVisible (compressorSelected);
    compressorReleaseSlider.setVisible (compressorSelected);
    compressorMakeupParameterLabel->setVisible (compressorSelected);
    compressorMakeupSlider.setVisible (compressorSelected);
    compressorMixParameterLabel->setVisible (compressorSelected);
    compressorMixSlider.setVisible (compressorSelected);
    compressorTargetListA->setVisible (compressorSelected);
    compressorTargetListB->setVisible (compressorSelected);
    phiTargetButton.setVisible (phiAvailable && phiSelected);
    nudgeControlsA->setGateVisible (gateSelected);
    nudgeControlsB->setGateVisible (gateSelected);

    const auto laneColour = juce::Colour (gateSelected ? gateAccent
        : delaySelected ? delayAccent
        : reverbSelected ? reverbAccent
        : panSelected ? panAccent
        : filterSelected ? filterAccent
        : pitchSelected ? pitchAccent
        : distortionSelected ? distortionAccent
        : grainSelected ? grainAccent
        : compressorSelected ? compressorAccent : phiAccent);
    rangeLinkButton->setAccentColour (laneColour);
    const juce::String fxName = gateSelected ? "GATE"
                              : delaySelected ? "DELAY"
                              : reverbSelected ? "REVERB"
                              : panSelected ? "PAN"
                              : filterSelected ? "FILTER"
                              : pitchSelected ? "PITCH"
                              : distortionSelected ? "DISTORTION"
                              : grainSelected ? "GRAIN"
                              : compressorSelected ? "COMPRESSOR" : "PHI";
    laneATitle.setText (fxName + " A",
                        juce::dontSendNotification);
    laneBTitle.setText (fxName + " B",
                        juce::dontSendNotification);
    laneATitle.setColour (juce::Label::textColourId, laneColour);
    laneBTitle.setColour (juce::Label::textColourId, laneColour);

    for (auto* slider : { &rateSlider, &startSlider, &endSlider })
        slider->setColour (juce::Slider::rotarySliderFillColourId, laneColour);
    if (gateSelected)
    {
        for (auto* slider : { &baseSlider, &depthSlider,
                              &shortLengthSlider, &longLengthSlider,
                              &noiseThresholdSlider, &noiseAttackSlider,
                              &noiseHoldSlider, &noiseReleaseSlider,
                              &noiseRangeSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (delaySelected)
    {
        for (auto* slider : { &delayTimeSlider, &delayFeedbackSlider,
                              &delayMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (reverbSelected)
    {
        for (auto* slider : { &reverbSizeSlider, &reverbDampingSlider,
                              &reverbWidthSlider, &reverbMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (panSelected)
    {
        panPositionSlider.setColour (
            juce::Slider::rotarySliderFillColourId, laneColour);
    }
    else if (filterSelected)
    {
        for (auto* slider : { &filterCutoffSlider, &filterResonanceSlider,
                              &filterMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (pitchSelected)
    {
        for (auto* slider : { &pitchShiftSlider, &pitchMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (distortionSelected)
    {
        for (auto* slider : { &distortionDriveSlider, &distortionToneSlider,
                              &distortionMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (grainSelected)
    {
        for (auto* slider : { &grainSizeSlider, &grainShiftSlider,
                              &grainFeedbackSlider, &grainMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }
    else if (compressorSelected)
    {
        for (auto* slider : { &compressorThresholdSlider,
                              &compressorRatioSlider,
                              &compressorAttackSlider,
                              &compressorReleaseSlider,
                              &compressorMakeupSlider,
                              &compressorMixSlider })
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId,
                               laneColour);
        }
    }

    if (lastPhiAvailability != phiAvailable)
    {
        lastPhiAvailability = phiAvailable;
        repaint();
    }
}

void SeqwencerAudioProcessorEditor::updateLaneColours()
{
    laneAColour = laneColourFromHue (
        0, static_cast<float> (colourASlider.getValue()));
    laneBColour = laneColourFromHue (
        1, static_cast<float> (colourBSlider.getValue()));
    const auto combinedHue = seqwencer::combinedTargetHue (
        laneAColour.getHue(), laneBColour.getHue());
    const auto combinedSaturation = juce::jlimit (
        0.55f, 1.0f,
        0.5f * (laneAColour.getSaturation()
                + laneBColour.getSaturation()));
    const auto combinedBrightness = juce::jlimit (
        0.80f, 1.0f,
        juce::jmax (laneAColour.getBrightness(),
                    laneBColour.getBrightness()));
    const auto combinedTargetColour = juce::Colour::fromHSV (
        combinedHue, combinedSaturation, combinedBrightness, 1.0f);

    for (auto* label : {
             baseParameterLabel.get(), depthParameterLabel.get(),
             shortParameterLabel.get(), longParameterLabel.get(),
             noiseThresholdParameterLabel.get(),
             noiseAttackParameterLabel.get(), noiseHoldParameterLabel.get(),
             noiseReleaseParameterLabel.get(), noiseRangeParameterLabel.get(),
             delayTimeParameterLabel.get(), delayFeedbackParameterLabel.get(),
             delayMixParameterLabel.get(), reverbSizeParameterLabel.get(),
             reverbDampingParameterLabel.get(), reverbWidthParameterLabel.get(),
             reverbMixParameterLabel.get(), panPositionParameterLabel.get(),
             filterCutoffParameterLabel.get(),
             filterResonanceParameterLabel.get(), filterMixParameterLabel.get(),
             pitchShiftParameterLabel.get(), pitchMixParameterLabel.get(),
             distortionDriveParameterLabel.get(),
             distortionToneParameterLabel.get(),
             distortionMixParameterLabel.get(), grainSizeParameterLabel.get(),
             grainShiftParameterLabel.get(), grainFeedbackParameterLabel.get(),
             grainMixParameterLabel.get(),
             compressorThresholdParameterLabel.get(),
             compressorRatioParameterLabel.get(),
             compressorAttackParameterLabel.get(),
             compressorReleaseParameterLabel.get(),
             compressorMakeupParameterLabel.get(),
             compressorMixParameterLabel.get(), attackAParameterLabel.get(),
             releaseAParameterLabel.get(), attackBParameterLabel.get(),
             releaseBParameterLabel.get(), startParameterLabel.get(),
             endParameterLabel.get() })
    {
        label->setAssignmentColours (
            laneAColour, laneBColour, combinedTargetColour);
    }

    gridA->setAccentColour (laneAColour);
    gridB->setAccentColour (laneBColour);
    nudgeControlsA->setAccentColour (laneAColour);
    nudgeControlsB->setAccentColour (laneBColour);
    targetListA->setAccentColour (laneAColour);
    targetListB->setAccentColour (laneBColour);
    delayTargetListA->setAccentColour (laneAColour);
    delayTargetListB->setAccentColour (laneBColour);
    reverbTargetListA->setAccentColour (laneAColour);
    reverbTargetListB->setAccentColour (laneBColour);
    panTargetListA->setAccentColour (laneAColour);
    panTargetListB->setAccentColour (laneBColour);
    filterTargetListA->setAccentColour (laneAColour);
    filterTargetListB->setAccentColour (laneBColour);
    pitchTargetListA->setAccentColour (laneAColour);
    pitchTargetListB->setAccentColour (laneBColour);
    distortionTargetListA->setAccentColour (laneAColour);
    distortionTargetListB->setAccentColour (laneBColour);
    grainTargetListA->setAccentColour (laneAColour);
    grainTargetListB->setAccentColour (laneBColour);
    compressorTargetListA->setAccentColour (laneAColour);
    compressorTargetListB->setAccentColour (laneBColour);

    for (auto* button : { &enableAButton, &bipolarAButton })
        button->setColour (juce::ToggleButton::tickColourId, laneAColour);
    for (auto* button : { &enableBButton, &bipolarBButton })
        button->setColour (juce::ToggleButton::tickColourId, laneBColour);
    for (auto* slider : { &attackASlider, &releaseASlider })
        slider->setColour (juce::Slider::rotarySliderFillColourId, laneAColour);
    for (auto* slider : { &attackBSlider, &releaseBSlider })
        slider->setColour (juce::Slider::rotarySliderFillColourId, laneBColour);

    colourASlider.setColour (juce::Slider::rotarySliderFillColourId,
                             laneAColour);
    colourBSlider.setColour (juce::Slider::rotarySliderFillColourId,
                             laneBColour);
    colourALabel.setColour (juce::Label::textColourId, laneAColour);
    colourBLabel.setColour (juce::Label::textColourId, laneBColour);
    repaint();
}

void SeqwencerAudioProcessorEditor::applyWaveformPreset (int bank)
{
    auto& box = bank == 0 ? waveformABox : waveformBBox;
    const auto index = box.getSelectedItemIndex();
    if (index < 0)
        return;

    const auto preset = static_cast<seqwencer::WaveformPreset> (index);
    if (bank == 0)
        gridA->applyWaveform (preset);
    else
        gridB->applyWaveform (preset);
    box.setSelectedId (0, juce::dontSendNotification);
}

void SeqwencerAudioProcessorEditor::updateLaneVisuals()
{
    const auto readActual = [] (const juce::RangedAudioParameter* parameter)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : 0.0f;
    };

    auto* selectedFxEnabledParameter = processor.getParameterState().getParameter (
        selectedFx == SelectedFx::gate ? "gate_enabled"
            : selectedFx == SelectedFx::delay ? "delay_enabled"
            : selectedFx == SelectedFx::reverb ? "reverb_enabled"
            : selectedFx == SelectedFx::pan ? "pan_enabled"
            : selectedFx == SelectedFx::filter ? "filter_enabled"
            : selectedFx == SelectedFx::pitch ? "pitch_enabled"
            : selectedFx == SelectedFx::distortion ? "distortion_enabled"
            : selectedFx == SelectedFx::grain ? "grain_enabled"
            : selectedFx == SelectedFx::compressor ? "compressor_enabled"
                                               : "phi_bridge_enabled");
    auto* noiseGateEnabledParameter =
        processor.getParameterState().getParameter ("noise_gate_enabled");
    const auto serial = readActual (modeParameter) >= 0.5f;
    const auto profileB = readActual (serialProfileParameter) >= 0.5f;
    const auto laneAActive = readActual (seqAEnabledParameter) >= 0.5f;
    const auto laneBActive = readActual (seqBEnabledParameter) >= 0.5f;
    const auto bipolarA = readActual (bipolarAParameter) >= 0.5f;
    const auto bipolarB = readActual (bipolarBParameter) >= 0.5f;
    const auto gateControlsAlpha = readActual (selectedFxEnabledParameter) >= 0.5f
        ? 1.0f : 0.30f;
    const auto fxIsEnabled = gateControlsAlpha >= 0.99f;

    baseParameterLabel->setAlpha (gateControlsAlpha);
    baseSlider.setAlpha (gateControlsAlpha);
    depthParameterLabel->setAlpha (gateControlsAlpha);
    depthSlider.setAlpha (gateControlsAlpha);
    shortParameterLabel->setAlpha (gateControlsAlpha);
    shortLengthSlider.setAlpha (gateControlsAlpha);
    longParameterLabel->setAlpha (gateControlsAlpha);
    longLengthSlider.setAlpha (gateControlsAlpha);
    const auto noiseControlsAlpha = gateControlsAlpha
        * (readActual (noiseGateEnabledParameter) >= 0.5f ? 1.0f : 0.30f);
    noiseGateButton.setAlpha (gateControlsAlpha);
    noiseThresholdParameterLabel->setAlpha (noiseControlsAlpha);
    noiseThresholdSlider.setAlpha (noiseControlsAlpha);
    noiseAttackParameterLabel->setAlpha (noiseControlsAlpha);
    noiseAttackSlider.setAlpha (noiseControlsAlpha);
    noiseHoldParameterLabel->setAlpha (noiseControlsAlpha);
    noiseHoldSlider.setAlpha (noiseControlsAlpha);
    noiseReleaseParameterLabel->setAlpha (noiseControlsAlpha);
    noiseReleaseSlider.setAlpha (noiseControlsAlpha);
    noiseRangeParameterLabel->setAlpha (noiseControlsAlpha);
    noiseRangeSlider.setAlpha (noiseControlsAlpha);
    delayTimeParameterLabel->setAlpha (gateControlsAlpha);
    delayTimeSlider.setAlpha (gateControlsAlpha);
    delayFeedbackParameterLabel->setAlpha (gateControlsAlpha);
    delayFeedbackSlider.setAlpha (gateControlsAlpha);
    delayMixParameterLabel->setAlpha (gateControlsAlpha);
    delayMixSlider.setAlpha (gateControlsAlpha);
    reverbSizeParameterLabel->setAlpha (gateControlsAlpha);
    reverbSizeSlider.setAlpha (gateControlsAlpha);
    reverbDampingParameterLabel->setAlpha (gateControlsAlpha);
    reverbDampingSlider.setAlpha (gateControlsAlpha);
    reverbWidthParameterLabel->setAlpha (gateControlsAlpha);
    reverbWidthSlider.setAlpha (gateControlsAlpha);
    reverbMixParameterLabel->setAlpha (gateControlsAlpha);
    reverbMixSlider.setAlpha (gateControlsAlpha);
    panPositionParameterLabel->setAlpha (gateControlsAlpha);
    panPositionSlider.setAlpha (gateControlsAlpha);
    filterTypeLabel.setAlpha (gateControlsAlpha);
    filterTypeBox.setAlpha (gateControlsAlpha);
    filterCutoffParameterLabel->setAlpha (gateControlsAlpha);
    filterCutoffSlider.setAlpha (gateControlsAlpha);
    filterResonanceParameterLabel->setAlpha (gateControlsAlpha);
    filterResonanceSlider.setAlpha (gateControlsAlpha);
    filterMixParameterLabel->setAlpha (gateControlsAlpha);
    filterMixSlider.setAlpha (gateControlsAlpha);
    pitchShiftParameterLabel->setAlpha (gateControlsAlpha);
    pitchShiftSlider.setAlpha (gateControlsAlpha);
    pitchMixParameterLabel->setAlpha (gateControlsAlpha);
    pitchMixSlider.setAlpha (gateControlsAlpha);
    distortionTypeLabel.setAlpha (gateControlsAlpha);
    distortionTypeBox.setAlpha (gateControlsAlpha);
    distortionDriveParameterLabel->setAlpha (gateControlsAlpha);
    distortionDriveSlider.setAlpha (gateControlsAlpha);
    distortionToneParameterLabel->setAlpha (gateControlsAlpha);
    distortionToneSlider.setAlpha (gateControlsAlpha);
    distortionMixParameterLabel->setAlpha (gateControlsAlpha);
    distortionMixSlider.setAlpha (gateControlsAlpha);
    grainSizeParameterLabel->setAlpha (gateControlsAlpha);
    grainSizeSlider.setAlpha (gateControlsAlpha);
    grainShiftParameterLabel->setAlpha (gateControlsAlpha);
    grainShiftSlider.setAlpha (gateControlsAlpha);
    grainFeedbackParameterLabel->setAlpha (gateControlsAlpha);
    grainFeedbackSlider.setAlpha (gateControlsAlpha);
    grainMixParameterLabel->setAlpha (gateControlsAlpha);
    grainMixSlider.setAlpha (gateControlsAlpha);
    compressorThresholdParameterLabel->setAlpha (gateControlsAlpha);
    compressorThresholdSlider.setAlpha (gateControlsAlpha);
    compressorRatioParameterLabel->setAlpha (gateControlsAlpha);
    compressorRatioSlider.setAlpha (gateControlsAlpha);
    compressorAttackParameterLabel->setAlpha (gateControlsAlpha);
    compressorAttackSlider.setAlpha (gateControlsAlpha);
    compressorReleaseParameterLabel->setAlpha (gateControlsAlpha);
    compressorReleaseSlider.setAlpha (gateControlsAlpha);
    compressorMakeupParameterLabel->setAlpha (gateControlsAlpha);
    compressorMakeupSlider.setAlpha (gateControlsAlpha);
    compressorMixParameterLabel->setAlpha (gateControlsAlpha);
    compressorMixSlider.setAlpha (gateControlsAlpha);

    enableAButton.setEnabled (true);
    enableBButton.setEnabled (true);
    enableAButton.setToggleState (serial ? ! profileB : laneAActive,
                                  juce::dontSendNotification);
    enableBButton.setToggleState (serial ? profileB : laneBActive,
                                  juce::dontSendNotification);
    bipolarAButton.setToggleState (bipolarA, juce::dontSendNotification);
    bipolarBButton.setToggleState (bipolarB, juce::dontSendNotification);

    const auto setControlAlpha = [] (float alpha,
                                     juce::Slider& attack,
                                     juce::Component& attackLabel,
                                     juce::Slider& release,
                                     juce::Component& releaseLabel)
    {
        attack.setAlpha (alpha);
        attackLabel.setAlpha (alpha);
        release.setAlpha (alpha);
        releaseLabel.setAlpha (alpha);
    };

    if (serial)
    {
        gridA->setAlpha (gateControlsAlpha);
        gridB->setAlpha (gateControlsAlpha);
        nudgeControlsA->setAlpha (gateControlsAlpha);
        nudgeControlsB->setAlpha (gateControlsAlpha);
        targetListA->setAlpha (gateControlsAlpha);
        targetListB->setAlpha (gateControlsAlpha);
        delayTargetListA->setAlpha (gateControlsAlpha);
        delayTargetListB->setAlpha (gateControlsAlpha);
        reverbTargetListA->setAlpha (gateControlsAlpha);
        reverbTargetListB->setAlpha (gateControlsAlpha);
        panTargetListA->setAlpha (gateControlsAlpha);
        panTargetListB->setAlpha (gateControlsAlpha);
        filterTargetListA->setAlpha (gateControlsAlpha);
        filterTargetListB->setAlpha (gateControlsAlpha);
        pitchTargetListA->setAlpha (gateControlsAlpha);
        pitchTargetListB->setAlpha (gateControlsAlpha);
        distortionTargetListA->setAlpha (gateControlsAlpha);
        distortionTargetListB->setAlpha (gateControlsAlpha);
        grainTargetListA->setAlpha (gateControlsAlpha);
        grainTargetListB->setAlpha (gateControlsAlpha);
        compressorTargetListA->setAlpha (gateControlsAlpha);
        compressorTargetListB->setAlpha (gateControlsAlpha);
        setControlAlpha (gateControlsAlpha * (profileB ? 0.30f : 1.0f),
                         attackASlider, *attackAParameterLabel,
                         releaseASlider, *releaseAParameterLabel);
        setControlAlpha (gateControlsAlpha * (profileB ? 1.0f : 0.30f),
                         attackBSlider, *attackBParameterLabel,
                         releaseBSlider, *releaseBParameterLabel);
        bipolarAButton.setAlpha (
            gateControlsAlpha * (profileB ? 0.30f : 1.0f));
        bipolarBButton.setAlpha (
            gateControlsAlpha * (profileB ? 1.0f : 0.30f));
        laneATitle.setAlpha (
            gateControlsAlpha * (profileB ? 0.30f : 1.0f));
        laneBTitle.setAlpha (
            gateControlsAlpha * (profileB ? 1.0f : 0.30f));
    }
    else
    {
        const auto alphaA = fxIsEnabled
            ? (laneAActive ? 1.0f : 0.30f) : 0.30f;
        const auto alphaB = fxIsEnabled
            ? (laneBActive ? 1.0f : 0.30f) : 0.30f;
        gridA->setAlpha (alphaA);
        gridB->setAlpha (alphaB);
        nudgeControlsA->setAlpha (alphaA);
        nudgeControlsB->setAlpha (alphaB);
        targetListA->setAlpha (alphaA);
        targetListB->setAlpha (alphaB);
        delayTargetListA->setAlpha (alphaA);
        delayTargetListB->setAlpha (alphaB);
        reverbTargetListA->setAlpha (alphaA);
        reverbTargetListB->setAlpha (alphaB);
        panTargetListA->setAlpha (alphaA);
        panTargetListB->setAlpha (alphaB);
        filterTargetListA->setAlpha (alphaA);
        filterTargetListB->setAlpha (alphaB);
        pitchTargetListA->setAlpha (alphaA);
        pitchTargetListB->setAlpha (alphaB);
        distortionTargetListA->setAlpha (alphaA);
        distortionTargetListB->setAlpha (alphaB);
        grainTargetListA->setAlpha (alphaA);
        grainTargetListB->setAlpha (alphaB);
        compressorTargetListA->setAlpha (alphaA);
        compressorTargetListB->setAlpha (alphaB);
        setControlAlpha (alphaA, attackASlider, *attackAParameterLabel,
                         releaseASlider, *releaseAParameterLabel);
        setControlAlpha (alphaB, attackBSlider, *attackBParameterLabel,
                         releaseBSlider, *releaseBParameterLabel);
        bipolarAButton.setAlpha (alphaA);
        bipolarBButton.setAlpha (alphaB);
        laneATitle.setAlpha (alphaA);
        laneBTitle.setAlpha (alphaB);
    }

    repaint();
}

void SeqwencerAudioProcessorEditor::paint (juce::Graphics& graphics)
{
    graphics.fillAll (juce::Colour (background));

    const auto scale = juce::jmin (
        static_cast<float> (getWidth()) / static_cast<float> (designWidth),
        static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const auto offsetX = 0.5f * (static_cast<float> (getWidth())
        - static_cast<float> (designWidth) * scale);
    const auto offsetY = 0.5f * (static_cast<float> (getHeight())
        - static_cast<float> (designHeight) * scale);
    graphics.saveState();
    graphics.addTransform (
        juce::AffineTransform::scale (scale).translated (offsetX, offsetY));

    juce::ColourGradient headerGradient (
        juce::Colour (0xff263746), 0.0f, 0.0f,
        juce::Colour (0xff151b21), static_cast<float> (designWidth), 52.0f, false);
    headerGradient.addColour (0.55, juce::Colour (0xff202b33));
    graphics.setGradientFill (headerGradient);
    graphics.fillRect (0, 0, designWidth, 52);

    graphics.setColour (juce::Colour (text));
    graphics.setFont (juce::FontOptions { 25.0f, juce::Font::bold });
    graphics.drawText ("SEQWENCER", 18, 7, 220, 28,
                       juce::Justification::centredLeft, false);
    graphics.setColour (juce::Colour (globalAccent));
    graphics.setFont (juce::FontOptions { 10.5f, juce::Font::bold });
    graphics.drawText ("DUAL STEP MODULATION & FX  |  v1.3.19.0",
                       20, 33, 320, 13,
                       juce::Justification::centredLeft, false);

    const auto topPanel = juce::Rectangle<float> (
        10.0f, 62.0f, static_cast<float> (designWidth - 20), 78.0f);
    graphics.setColour (juce::Colour (panel));
    graphics.fillRoundedRectangle (topPanel, 8.0f);
    graphics.setColour (juce::Colour (panelOutline));
    graphics.drawRoundedRectangle (topPanel, 8.0f, 1.0f);

    const auto bottomPanelY = static_cast<float> (designHeight - 88);
    const auto bottomPanel = juce::Rectangle<float> (
        10.0f, bottomPanelY, static_cast<float> (designWidth - 20), 78.0f);
    graphics.setColour (juce::Colour (panel));
    graphics.fillRoundedRectangle (bottomPanel, 8.0f);
    graphics.setColour (juce::Colour (panelOutline));
    graphics.drawRoundedRectangle (bottomPanel, 8.0f, 1.0f);

    graphics.setColour (juce::Colour (globalAccent));
    graphics.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    graphics.drawText (selectedFx == SelectedFx::gate ? "GATE CONTROLS"
                           : selectedFx == SelectedFx::delay
                               ? "DELAY CONTROLS"
                           : selectedFx == SelectedFx::reverb
                               ? "REVERB CONTROLS"
                           : selectedFx == SelectedFx::pan
                               ? "PAN CONTROLS"
                           : selectedFx == SelectedFx::filter
                               ? "FILTER CONTROLS"
                           : selectedFx == SelectedFx::pitch
                               ? "PITCH CONTROLS"
                           : selectedFx == SelectedFx::distortion
                               ? "DISTORTION CONTROLS"
                           : selectedFx == SelectedFx::grain
                               ? "GRAIN CONTROLS"
                           : selectedFx == SelectedFx::compressor
                               ? "COMPRESSOR CONTROLS" : "PHI CONTROLS",
                       20, static_cast<int> (bottomPanelY) + 8,
                       92, 15, juce::Justification::centredLeft, false);

    constexpr auto laneTop = 150.0f;
    constexpr auto laneGap = 10.0f;
    const auto laneAreaBottom = bottomPanelY - laneGap;
    const auto laneHeight = (laneAreaBottom - laneTop - laneGap) * 0.5f;
    const auto fxRail = juce::Rectangle<float> (
        10.0f, laneTop, 94.0f, laneAreaBottom - laneTop);
    graphics.setColour (juce::Colour (panel));
    graphics.fillRoundedRectangle (fxRail, 8.0f);
    graphics.setColour (juce::Colour (panelOutline));
    graphics.drawRoundedRectangle (fxRail, 8.0f, 1.0f);
    graphics.setColour (juce::Colour (globalAccent));
    graphics.setFont (juce::FontOptions { 9.0f, juce::Font::bold });
    graphics.drawText ("FX  TOP-DOWN", fxRail.toNearestInt().withHeight (27),
                       juce::Justification::centred, false);
    if (fxDropIndex >= 0)
    {
        const auto targetY = 182.0f + 39.0f * static_cast<float> (fxDropIndex);
        graphics.setColour (juce::Colour (globalAccent).withAlpha (0.75f));
        graphics.drawRoundedRectangle (
            juce::Rectangle<float> (15.0f, targetY - 3.0f, 84.0f, 37.0f),
            6.0f, 2.0f);
    }

    const auto laneX = 114.0f;
    const auto laneWidth = static_cast<float> (designWidth) - laneX - 10.0f;
    const auto serial = modeBox.getSelectedItemIndex() == 1;
    for (int lane = 0; lane < 2; ++lane)
    {
        const auto y = laneTop
                     + (laneHeight + laneGap) * static_cast<float> (lane);
        const auto bounds = juce::Rectangle<float> (
            laneX, y, laneWidth, laneHeight);
        graphics.setColour (juce::Colour (panel));
        graphics.fillRoundedRectangle (bounds, 8.0f);
        const auto laneIsActive = serial
            || (lane == 0 ? enableAButton.getToggleState()
                          : enableBButton.getToggleState());
        const auto outlineAlpha = laneIsActive ? 0.65f : 0.18f;
        const auto moduleAccent = juce::Colour (
            selectedFx == SelectedFx::gate ? gateAccent
                : selectedFx == SelectedFx::delay ? delayAccent
                : selectedFx == SelectedFx::reverb ? reverbAccent
                : selectedFx == SelectedFx::pan ? panAccent
                : selectedFx == SelectedFx::filter ? filterAccent
                : selectedFx == SelectedFx::pitch ? pitchAccent
                : selectedFx == SelectedFx::distortion
                    ? distortionAccent
                : selectedFx == SelectedFx::grain
                    ? grainAccent
                : selectedFx == SelectedFx::compressor
                    ? compressorAccent : phiAccent);
        graphics.setColour (moduleAccent.withAlpha (outlineAlpha));
        graphics.drawRoundedRectangle (bounds, 8.0f, 1.2f);
    }
    graphics.restoreState();
}

void SeqwencerAudioProcessorEditor::resized()
{
    content.setTransform (juce::AffineTransform());
    content.setBounds (0, 0, designWidth, designHeight);
    const auto scale = juce::jmin (
        static_cast<float> (getWidth()) / static_cast<float> (designWidth),
        static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const auto offsetX = 0.5f * (static_cast<float> (getWidth())
        - static_cast<float> (designWidth) * scale);
    const auto offsetY = 0.5f * (static_cast<float> (getHeight())
        - static_cast<float> (designHeight) * scale);
    content.setTransform (
        juce::AffineTransform::scale (scale).translated (offsetX, offsetY));

    syncButton.setBounds (22, 85, 104, 31);
    presetsButton.setBounds (138, 85, 104, 31);
    colourALabel.setBounds (250, 66, 64, 14);
    colourASlider.setBounds (250, 77, 64, 59);
    colourBLabel.setBounds (322, 66, 64, 14);
    colourBSlider.setBounds (322, 77, 64, 59);
    waveformABox.setBounds (404, 85, 166, 31);
    waveformBBox.setBounds (582, 85, 166, 31);

    const auto bottomPanelY = designHeight - 88;
    updateFxSelectorBounds();
    modeBox.setBounds (20, bottomPanelY + 28, 92, 28);
    rateLabel.setBounds (122, bottomPanelY + 4, 64, 14);
    rateSlider.setBounds (122, bottomPanelY + 15, 64, 59);
    startParameterLabel->setBounds (194, bottomPanelY + 4, 64, 14);
    startSlider.setBounds (194, bottomPanelY + 15, 64, 59);
    endParameterLabel->setBounds (266, bottomPanelY + 4, 64, 14);
    endSlider.setBounds (266, bottomPanelY + 15, 64, 59);
    rangeLinkButton->setBounds (253, bottomPanelY + 27, 22, 20);
    rangeLinkButton->toFront (false);
    sequenceModeLabel.setBounds (338, bottomPanelY + 4, 96, 14);
    sequenceModeBox.setBounds (338, bottomPanelY + 28, 96, 28);

    phiTargetButton.setBounds (448, bottomPanelY + 27, 96, 31);
    baseParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    baseSlider.setBounds (448, bottomPanelY + 15, 64, 59);
    depthParameterLabel->setBounds (526, bottomPanelY + 4, 88, 14);
    depthSlider.setBounds (538, bottomPanelY + 15, 64, 59);
    shortParameterLabel->setBounds (615, bottomPanelY + 4, 90, 14);
    shortLengthSlider.setBounds (628, bottomPanelY + 15, 64, 59);
    longParameterLabel->setBounds (705, bottomPanelY + 4, 90, 14);
    longLengthSlider.setBounds (718, bottomPanelY + 15, 64, 59);

    noiseGateButton.setBounds (798, bottomPanelY + 27, 90, 31);
    noiseThresholdParameterLabel->setBounds (896, bottomPanelY + 4, 72, 14);
    noiseThresholdSlider.setBounds (900, bottomPanelY + 15, 64, 59);
    noiseAttackParameterLabel->setBounds (968, bottomPanelY + 4, 72, 14);
    noiseAttackSlider.setBounds (972, bottomPanelY + 15, 64, 59);
    noiseHoldParameterLabel->setBounds (1040, bottomPanelY + 4, 72, 14);
    noiseHoldSlider.setBounds (1044, bottomPanelY + 15, 64, 59);
    noiseReleaseParameterLabel->setBounds (1112, bottomPanelY + 4, 72, 14);
    noiseReleaseSlider.setBounds (1116, bottomPanelY + 15, 64, 59);
    noiseRangeParameterLabel->setBounds (1184, bottomPanelY + 4, 72, 14);
    noiseRangeSlider.setBounds (1188, bottomPanelY + 15, 64, 59);

    delayTimeParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    delayTimeSlider.setBounds (448, bottomPanelY + 15, 64, 59);
    delayFeedbackParameterLabel->setBounds (520, bottomPanelY + 4, 104, 14);
    delayFeedbackSlider.setBounds (540, bottomPanelY + 15, 64, 59);
    delayMixParameterLabel->setBounds (616, bottomPanelY + 4, 88, 14);
    delayMixSlider.setBounds (628, bottomPanelY + 15, 64, 59);

    reverbSizeParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    reverbSizeSlider.setBounds (448, bottomPanelY + 15, 64, 59);
    reverbDampingParameterLabel->setBounds (526, bottomPanelY + 4, 88, 14);
    reverbDampingSlider.setBounds (538, bottomPanelY + 15, 64, 59);
    reverbWidthParameterLabel->setBounds (616, bottomPanelY + 4, 88, 14);
    reverbWidthSlider.setBounds (628, bottomPanelY + 15, 64, 59);
    reverbMixParameterLabel->setBounds (706, bottomPanelY + 4, 88, 14);
    reverbMixSlider.setBounds (718, bottomPanelY + 15, 64, 59);

    panPositionParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    panPositionSlider.setBounds (448, bottomPanelY + 15, 64, 59);

    filterTypeLabel.setBounds (448, bottomPanelY + 4, 122, 14);
    filterTypeBox.setBounds (448, bottomPanelY + 28, 122, 28);
    filterCutoffParameterLabel->setBounds (578, bottomPanelY + 4, 88, 14);
    filterCutoffSlider.setBounds (590, bottomPanelY + 15, 64, 59);
    filterResonanceParameterLabel->setBounds (666, bottomPanelY + 4, 92, 14);
    filterResonanceSlider.setBounds (680, bottomPanelY + 15, 64, 59);
    filterMixParameterLabel->setBounds (756, bottomPanelY + 4, 88, 14);
    filterMixSlider.setBounds (768, bottomPanelY + 15, 64, 59);

    pitchShiftParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    pitchShiftSlider.setBounds (448, bottomPanelY + 15, 64, 59);
    pitchMixParameterLabel->setBounds (526, bottomPanelY + 4, 88, 14);
    pitchMixSlider.setBounds (538, bottomPanelY + 15, 64, 59);

    distortionTypeLabel.setBounds (448, bottomPanelY + 4, 122, 14);
    distortionTypeBox.setBounds (448, bottomPanelY + 28, 122, 28);
    distortionDriveParameterLabel->setBounds (
        578, bottomPanelY + 4, 88, 14);
    distortionDriveSlider.setBounds (590, bottomPanelY + 15, 64, 59);
    distortionToneParameterLabel->setBounds (
        666, bottomPanelY + 4, 92, 14);
    distortionToneSlider.setBounds (680, bottomPanelY + 15, 64, 59);
    distortionMixParameterLabel->setBounds (
        756, bottomPanelY + 4, 88, 14);
    distortionMixSlider.setBounds (768, bottomPanelY + 15, 64, 59);

    grainSizeParameterLabel->setBounds (436, bottomPanelY + 4, 88, 14);
    grainSizeSlider.setBounds (448, bottomPanelY + 15, 64, 59);
    grainShiftParameterLabel->setBounds (526, bottomPanelY + 4, 88, 14);
    grainShiftSlider.setBounds (538, bottomPanelY + 15, 64, 59);
    grainFeedbackParameterLabel->setBounds (
        616, bottomPanelY + 4, 88, 14);
    grainFeedbackSlider.setBounds (628, bottomPanelY + 15, 64, 59);
    grainMixParameterLabel->setBounds (706, bottomPanelY + 4, 88, 14);
    grainMixSlider.setBounds (718, bottomPanelY + 15, 64, 59);

    compressorThresholdParameterLabel->setBounds (
        434, bottomPanelY + 4, 76, 14);
    compressorThresholdSlider.setBounds (440, bottomPanelY + 15, 64, 59);
    compressorRatioParameterLabel->setBounds (520, bottomPanelY + 4, 76, 14);
    compressorRatioSlider.setBounds (526, bottomPanelY + 15, 64, 59);
    compressorAttackParameterLabel->setBounds (
        606, bottomPanelY + 4, 76, 14);
    compressorAttackSlider.setBounds (612, bottomPanelY + 15, 64, 59);
    compressorReleaseParameterLabel->setBounds (
        692, bottomPanelY + 4, 76, 14);
    compressorReleaseSlider.setBounds (698, bottomPanelY + 15, 64, 59);
    compressorMakeupParameterLabel->setBounds (
        778, bottomPanelY + 4, 76, 14);
    compressorMakeupSlider.setBounds (784, bottomPanelY + 15, 64, 59);
    compressorMixParameterLabel->setBounds (864, bottomPanelY + 4, 76, 14);
    compressorMixSlider.setBounds (870, bottomPanelY + 15, 64, 59);

    constexpr int laneTop = 150;
    constexpr int laneGap = 10;
    const auto laneAreaBottom = bottomPanelY - laneGap;
    const auto totalLaneHeight = laneAreaBottom - laneTop - laneGap;
    const auto laneAHeight = totalLaneHeight / 2;
    const auto laneBHeight = totalLaneHeight - laneAHeight;
    constexpr int laneX = 114;
    constexpr int controlsWidth = 150;
    constexpr int targetWidth = 166;
    constexpr int componentGap = 10;
    constexpr int innerMargin = 10;
    const auto placeLane = [&] (int lane,
                                juce::ToggleButton& enabled,
                                juce::Slider& attack,
                                juce::Component& attackLabel,
                                juce::Slider& release,
                                juce::Component& releaseLabel,
                                PatternNudgeControls& nudgeControls,
                                StepGrid& grid)
    {
        const auto laneHeight = lane == 0 ? laneAHeight : laneBHeight;
        const auto y = lane == 0
            ? laneTop : laneTop + laneAHeight + laneGap;
        auto& laneTitle = lane == 0 ? laneATitle : laneBTitle;
        laneTitle.setBounds (laneX + 10, y + 7, 128, 14);
        enabled.setBounds (laneX + 10, y + 26, 34, 27);
        auto& bipolarButton = lane == 0 ? bipolarAButton : bipolarBButton;
        bipolarButton.setBounds (laneX + 52, y + 26, 86, 27);
        attackLabel.setBounds (laneX + 13, y + 61, 60, 13);
        attack.setBounds (laneX + 13, y + 72, 60,
                          juce::jmax (35, juce::jmin (70, laneHeight - 84)));
        releaseLabel.setBounds (laneX + 78, y + 61, 60, 13);
        release.setBounds (laneX + 78, y + 72, 60,
                           juce::jmax (35, juce::jmin (70, laneHeight - 84)));
        nudgeControls.setBounds (laneX + 13, y + 149, 125, 44);

        const auto gateSelected = selectedFx == SelectedFx::gate;
        const auto delaySelected = selectedFx == SelectedFx::delay;
        const auto reverbSelected = selectedFx == SelectedFx::reverb;
        const auto panSelected = selectedFx == SelectedFx::pan;
        const auto filterSelected = selectedFx == SelectedFx::filter;
        const auto pitchSelected = selectedFx == SelectedFx::pitch;
        const auto distortionSelected = selectedFx == SelectedFx::distortion;
        const auto grainSelected = selectedFx == SelectedFx::grain;
        const auto compressorSelected = selectedFx == SelectedFx::compressor;
        const auto hasInternalTargets = gateSelected || delaySelected
                                     || reverbSelected || panSelected
                                     || filterSelected || pitchSelected
                                     || distortionSelected || grainSelected
                                     || compressorSelected;
        const auto targetX = hasInternalTargets
            ? designWidth - 10 - innerMargin - targetWidth
            : designWidth - 10 - innerMargin;
        auto& gateTargetList = lane == 0 ? *targetListA : *targetListB;
        auto& selectedDelayTargetList = lane == 0
            ? *delayTargetListA : *delayTargetListB;
        auto& selectedReverbTargetList = lane == 0
            ? *reverbTargetListA : *reverbTargetListB;
        auto& selectedPanTargetList = lane == 0
            ? *panTargetListA : *panTargetListB;
        auto& selectedFilterTargetList = lane == 0
            ? *filterTargetListA : *filterTargetListB;
        auto& selectedPitchTargetList = lane == 0
            ? *pitchTargetListA : *pitchTargetListB;
        auto& selectedDistortionTargetList = lane == 0
            ? *distortionTargetListA : *distortionTargetListB;
        auto& selectedGrainTargetList = lane == 0
            ? *grainTargetListA : *grainTargetListB;
        auto& selectedCompressorTargetList = lane == 0
            ? *compressorTargetListA : *compressorTargetListB;
        gateTargetList.setBounds (targetX, y + innerMargin,
                                  gateSelected ? targetWidth : 0,
                                  laneHeight - 2 * innerMargin);
        selectedDelayTargetList.setBounds (targetX, y + innerMargin,
                                           delaySelected ? targetWidth : 0,
                                           laneHeight - 2 * innerMargin);
        selectedReverbTargetList.setBounds (targetX, y + innerMargin,
                                            reverbSelected ? targetWidth : 0,
                                            laneHeight - 2 * innerMargin);
        selectedPanTargetList.setBounds (targetX, y + innerMargin,
                                         panSelected ? targetWidth : 0,
                                         laneHeight - 2 * innerMargin);
        selectedFilterTargetList.setBounds (targetX, y + innerMargin,
                                            filterSelected ? targetWidth : 0,
                                            laneHeight - 2 * innerMargin);
        selectedPitchTargetList.setBounds (targetX, y + innerMargin,
                                           pitchSelected ? targetWidth : 0,
                                           laneHeight - 2 * innerMargin);
        selectedDistortionTargetList.setBounds (
            targetX, y + innerMargin,
            distortionSelected ? targetWidth : 0,
            laneHeight - 2 * innerMargin);
        selectedGrainTargetList.setBounds (
            targetX, y + innerMargin,
            grainSelected ? targetWidth : 0,
            laneHeight - 2 * innerMargin);
        selectedCompressorTargetList.setBounds (
            targetX, y + innerMargin,
            compressorSelected ? targetWidth : 0,
            laneHeight - 2 * innerMargin);
        const auto gridX = laneX + controlsWidth + innerMargin;
        grid.setBounds (gridX, y + innerMargin,
                        juce::jmax (128, targetX - componentGap - gridX),
                        laneHeight - 2 * innerMargin);
    };

    placeLane (0, enableAButton, attackASlider, *attackAParameterLabel,
               releaseASlider, *releaseAParameterLabel,
               *nudgeControlsA, *gridA);
    placeLane (1, enableBButton, attackBSlider, *attackBParameterLabel,
               releaseBSlider, *releaseBParameterLabel,
               *nudgeControlsB, *gridB);
}

void SeqwencerAudioProcessorEditor::timerCallback()
{
    if (processor.getAudioFxOrder() != displayedAudioFxOrder)
        updateFxSelectorBounds();
    updateRangeControls();
    updateFxPanel();
    updateLaneVisuals();
    baseParameterLabel->repaint();
    depthParameterLabel->repaint();
    shortParameterLabel->repaint();
    longParameterLabel->repaint();
    noiseThresholdParameterLabel->repaint();
    noiseAttackParameterLabel->repaint();
    noiseHoldParameterLabel->repaint();
    noiseReleaseParameterLabel->repaint();
    noiseRangeParameterLabel->repaint();
    delayTimeParameterLabel->repaint();
    delayFeedbackParameterLabel->repaint();
    delayMixParameterLabel->repaint();
    reverbSizeParameterLabel->repaint();
    reverbDampingParameterLabel->repaint();
    reverbWidthParameterLabel->repaint();
    reverbMixParameterLabel->repaint();
    panPositionParameterLabel->repaint();
    filterCutoffParameterLabel->repaint();
    filterResonanceParameterLabel->repaint();
    filterMixParameterLabel->repaint();
    pitchShiftParameterLabel->repaint();
    pitchMixParameterLabel->repaint();
    distortionDriveParameterLabel->repaint();
    distortionToneParameterLabel->repaint();
    distortionMixParameterLabel->repaint();
    grainSizeParameterLabel->repaint();
    grainShiftParameterLabel->repaint();
    grainFeedbackParameterLabel->repaint();
    grainMixParameterLabel->repaint();
    compressorThresholdParameterLabel->repaint();
    compressorRatioParameterLabel->repaint();
    compressorAttackParameterLabel->repaint();
    compressorReleaseParameterLabel->repaint();
    compressorMakeupParameterLabel->repaint();
    compressorMixParameterLabel->repaint();
    attackAParameterLabel->repaint();
    releaseAParameterLabel->repaint();
    attackBParameterLabel->repaint();
    releaseBParameterLabel->repaint();
    startParameterLabel->repaint();
    endParameterLabel->repaint();
    rangeLinkButton->repaint();
    targetListA->refresh();
    targetListB->refresh();
    delayTargetListA->refresh();
    delayTargetListB->refresh();
    reverbTargetListA->refresh();
    reverbTargetListB->refresh();
    panTargetListA->refresh();
    panTargetListB->refresh();
    filterTargetListA->refresh();
    filterTargetListB->refresh();
    pitchTargetListA->refresh();
    pitchTargetListB->refresh();
    distortionTargetListA->refresh();
    distortionTargetListB->refresh();
    grainTargetListA->refresh();
    grainTargetListB->refresh();
    compressorTargetListA->refresh();
    compressorTargetListB->refresh();
    gridA->repaint();
    gridB->repaint();
}
