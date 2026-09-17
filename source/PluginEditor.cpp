#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr auto colourBoth = 0xffb98cff;
constexpr auto globalAccent = 0xff8aa3b5;
constexpr auto gateAccent = 0xff62cf8a;
constexpr auto phiAccent = 0xff68a8ff;
constexpr auto targetDragPrefix = "seqwencer-target:";
constexpr int designWidth = 1280;
constexpr int designHeight = 650;

constexpr std::array<seqwencer::ModulationTarget,
                     seqwencer::gateModulationTargetCount> gateTargets {
    seqwencer::ModulationTarget::gateLevel,
    seqwencer::ModulationTarget::gateDepth,
    seqwencer::ModulationTarget::shortGateLength,
    seqwencer::ModulationTarget::longGateLength
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

juce::Point<int> loadStoredEditorSize()
{
    auto size = juce::Point<int> { designWidth, designHeight };
    const auto file = SeqwencerAudioProcessor::getPortableDataDirectory()
                          .getChildFile ("Settings.ini");
    if (! file.existsAsFile())
        return size;

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
        const auto value = line.substring (separator + 1).getIntValue();
        if (key.equalsIgnoreCase ("Width"))
            size.x = value;
        else if (key.equalsIgnoreCase ("Height"))
            size.y = value;
    }
    size.x = juce::jlimit (960, 1600, size.x);
    size.y = static_cast<int> (std::lround (
        static_cast<double> (size.x) * designHeight / designWidth));
    return size;
}

void saveStoredEditorSize (int width, int height)
{
    const auto dataDirectory = SeqwencerAudioProcessor::getPortableDataDirectory();
    if (dataDirectory.createDirectory().failed())
        return;
    juce::String contents { "; Seqwencer portable settings\r\n" };
    contents << "[Interface]\r\nWidth=" << width
             << "\r\nHeight=" << height << "\r\n";
    dataDirectory.getChildFile ("Settings.ini").replaceWithText (contents);
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
    FxSelectorButton (const juce::String& name, juce::Colour accentColour)
        : juce::Button (name), accent (accentColour), ledButton (accentColour)
    {
        addAndMakeVisible (ledButton);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
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
    bool selected = false;
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
        setPhiEngine (false);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    }

    ~StepGrid() override
    {
        endGestures();
    }

    void setPhiEngine (bool shouldUsePhi)
    {
        if (phiEngine == shouldUsePhi && stepParameters[0] != nullptr)
            return;

        endGestures();
        phiEngine = shouldUsePhi;
        auto& state = processor.getParameterState();
        const auto parameterID = [shouldUsePhi] (const juce::String& gateID)
        {
            return shouldUsePhi ? "phi_" + gateID : gateID;
        };

        for (int step = 0; step < seqwencer::stepsPerBank; ++step)
        {
            const auto index = static_cast<std::size_t> (step);
            stepParameters[index] = state.getParameter (
                shouldUsePhi
                    ? SeqwencerAudioProcessor::phiStepParameterID (bank, step)
                    : SeqwencerAudioProcessor::stepParameterID (bank, step));
            otherBankStepParameters[index] = state.getParameter (
                shouldUsePhi
                    ? SeqwencerAudioProcessor::phiStepParameterID (
                        bank == 0 ? 1 : 0, step)
                    : SeqwencerAudioProcessor::stepParameterID (
                        bank == 0 ? 1 : 0, step));
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
        const auto bipolar = usesBipolarDisplay();
        const auto activeStep = bank == 0
            ? processor.getActiveStepA (phiEngine)
            : processor.getActiveStepB (phiEngine);

        if (! phiEngine)
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

            const auto value = values[static_cast<std::size_t> (step)];
            const auto height = graph.getHeight() * value;
            graphics.setColour (accent.withAlpha (step == activeStep ? 0.92f : 0.68f));
            if (bipolar)
            {
                const auto valueY = graph.getBottom() - height;
                const auto centreY = graph.getCentreY();
                graphics.fillRect (left + 1.0f,
                                   juce::jmin (valueY, centreY),
                                   juce::jmax (1.0f, columnWidth - 2.0f),
                                   std::abs (valueY - centreY));
            }
            else
            {
                graphics.fillRect (left + 1.0f,
                                   graph.getBottom() - height,
                                   juce::jmax (1.0f, columnWidth - 2.0f), height);
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
                             - graph.getHeight() * displayedValueAt (values, step, position);
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
        const auto inGateModeRow = ! phiEngine
                                && getGateModeBounds().contains (event.position);
        const auto inGraph = getGraphBounds().contains (event.position);
        if (! inGateModeRow && ! inGraph)
            return;

        const auto step = stepAtPosition (event.position);
        if (inGateModeRow)
        {
            if (event.mods.isLeftButtonDown())
                cycleGateMode (step);
            return;
        }

        if (event.getNumberOfClicks() > 1)
            return;

        if (event.mods.isRightButtonDown())
        {
            setStoredStepValueOnce (step, 1.0f);
            return;
        }

        if (event.mods.isMiddleButtonDown())
        {
            setStoredStepValueOnce (step, usesBipolarDisplay() ? 0.0f : 0.5f);
            return;
        }

        if (! event.mods.isLeftButtonDown())
            return;

        dragging = true;
        lastEditedStep = -1;
        updateFromMouse (event.position);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragging)
            updateFromMouse (event.position);
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
        setStoredStepValueOnce (stepAtPosition (event.position), 0.5f);
    }

private:
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

    juce::Rectangle<float> getGraphBounds() const
    {
        const auto inner = getLocalBounds().toFloat().reduced (7.0f);
        if (phiEngine)
        {
            const auto graphTop = inner.getY() + 9.0f;
            return { inner.getX(), graphTop, inner.getWidth(),
                     juce::jmax (1.0f, inner.getBottom() - graphTop) };
        }
        const auto modeRow = getGateModeBounds();
        const auto graphTop = modeRow.getBottom() + 4.0f;
        return { inner.getX(), graphTop, inner.getWidth(),
                 juce::jmax (1.0f, inner.getBottom() - graphTop) };
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
        return seqwencer::makeStepRange (
            static_cast<int> (std::lround (readParameter (startStepParameter, 1.0f))),
            static_cast<int> (std::lround (readParameter (endStepParameter, 64.0f))),
            isSerial() ? seqwencer::linkedStepCount : seqwencer::stepsPerBank);
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

    float displayedValueAt (const seqwencer::Pattern& values,
                            int step,
                            float position) const
    {
        const auto linked = isSerial();
        const auto range = getPlaybackRange();
        float previous = 1.0f;
        if (linked)
        {
            const auto globalStep = bank * seqwencer::stepsPerBank + step;
            const auto previousGlobalStep = globalStep == range.first
                ? range.last
                : (globalStep + seqwencer::linkedStepCount - 1)
                    % seqwencer::linkedStepCount;
            previous = valueAtGlobalStep (values, previousGlobalStep);
        }
        else
        {
            const auto previousStep = step == range.first
                ? range.last
                : (step + seqwencer::stepsPerBank - 1)
                    % seqwencer::stepsPerBank;
            previous = values[static_cast<std::size_t> (previousStep)];
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
        return seqwencer::transitionValue (
            previous, values[static_cast<std::size_t> (step)],
            position, attack, release);
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

    void setStoredStepValueOnce (int step, float canonical)
    {
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
        const auto value = juce::jlimit (
            0.0f, 1.0f, (graph.getBottom() - y) / graph.getHeight());

        if (lastEditedStep < 0 || lastEditedStep == step)
        {
            setStepValue (step, value);
        }
        else
        {
            const auto direction = step > lastEditedStep ? 1 : -1;
            const auto distance = std::abs (step - lastEditedStep);
            for (int offset = 1; offset <= distance; ++offset)
            {
                const auto interpolation = static_cast<float> (offset)
                                         / static_cast<float> (distance);
                setStepValue (lastEditedStep + direction * offset,
                              lastEditedValue
                                  + interpolation * (value - lastEditedValue));
            }
        }

        lastEditedStep = step;
        lastEditedValue = value;
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
        lastEditedStep = -1;
    }

    SeqwencerAudioProcessor& processor;
    int bank = 0;
    juce::Colour accent;
    bool phiEngine = false;
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
    juce::RangedAudioParameter* bipolarAParameter = nullptr;
    juce::RangedAudioParameter* bipolarBParameter = nullptr;
    juce::RangedAudioParameter* gateEnabledParameter = nullptr;
    bool dragging = false;
    int lastEditedStep = -1;
    float lastEditedValue = 1.0f;
};

class SeqwencerAudioProcessorEditor::ModulationParameterLabel final
    : public juce::Component,
      public juce::SettableTooltipClient
{
public:
    ModulationParameterLabel (SeqwencerAudioProcessor& audioProcessor,
                              seqwencer::ModulationTarget modulationTarget)
        : target (modulationTarget),
          caption (SeqwencerAudioProcessor::targetDisplayName (target))
    {
        auto& state = audioProcessor.getParameterState();
        targetAParameter = state.getParameter (
            SeqwencerAudioProcessor::targetAssignedParameterID (0, target));
        targetBParameter = state.getParameter (
            SeqwencerAudioProcessor::targetAssignedParameterID (1, target));
        jassert (targetAParameter != nullptr);
        jassert (targetBParameter != nullptr);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag " + caption
                    + " to a sequencer. Click the cross to clear both assignments.");
    }

    void paint (juce::Graphics& graphics) override
    {
        const auto assignedA = hasAssignment (targetAParameter);
        const auto assignedB = hasAssignment (targetBParameter);
        const auto assignmentColour = assignedA && assignedB
            ? juce::Colour (colourBoth)
            : assignedA ? juce::Colour (colourA)
                        : assignedB ? juce::Colour (colourB)
                                    : juce::Colour (text);
        const auto layout = getItemLayout (assignedA || assignedB);

        drawDragHandle (graphics, layout.handle, assignmentColour);
        graphics.setColour (assignmentColour);
        graphics.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
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
        clearPressed = event.mods.isLeftButtonDown()
                    && getItemLayout (hasAnyAssignment()).clear.contains (
                           event.getPosition());
        dragStarted = false;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (clearPressed || dragStarted || ! event.mods.isLeftButtonDown()
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
        if (clearPressed && getItemLayout (hasAnyAssignment()).clear.contains (
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
        constexpr int handleWidth = 10;
        const auto captionWidth = juce::jlimit (
            38, juce::jmax (38, getWidth() - 25), caption.length() * 6 + 5);
        constexpr int clearWidth = 11;
        constexpr int gap = 2;
        const auto captionX = (getWidth() - captionWidth) / 2;
        const auto centreY = getHeight() / 2;

        ItemLayout result;
        result.handle = { captionX - gap - handleWidth,
                          centreY - 5, handleWidth, 10 };
        result.caption = { captionX, 0, captionWidth, getHeight() };
        result.clear = showClear
            ? juce::Rectangle<int> { result.caption.getRight() + gap,
                                     0, clearWidth, getHeight() }
            : juce::Rectangle<int>();
        return result;
    }

    static void drawDragHandle (juce::Graphics& graphics,
                                juce::Rectangle<int> bounds,
                                juce::Colour colour)
    {
        graphics.setColour (colour.withAlpha (0.82f));
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 2; ++column)
                graphics.fillEllipse (static_cast<float> (bounds.getX() + column * 5),
                                      static_cast<float> (bounds.getY() + row * 5),
                                      2.5f, 2.5f);
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

    juce::RangedAudioParameter* targetAParameter = nullptr;
    juce::RangedAudioParameter* targetBParameter = nullptr;
    seqwencer::ModulationTarget target = seqwencer::ModulationTarget::none;
    juce::String caption;
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
                juce::Colour accentColour)
        : bank (bankIndex), accent (accentColour), content (*this)
    {
        auto& state = audioProcessor.getParameterState();
        for (int index = 0; index < seqwencer::gateModulationTargetCount; ++index)
        {
            const auto target = gateTargets[static_cast<std::size_t> (index)];
            ownTargetParameters[static_cast<std::size_t> (index)] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetAssignedParameterID (
                        bank, target));
            ownTargetEnabledParameters[static_cast<std::size_t> (index)] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetEnabledParameterID (
                        bank, target));
            targetAParameters[static_cast<std::size_t> (index)] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetAssignedParameterID (
                        0, target));
            targetAEnabledParameters[static_cast<std::size_t> (index)] =
                state.getParameter (
                    SeqwencerAudioProcessor::targetEnabledParameterID (
                        0, target));
        }
        modeParameter = state.getParameter ("playback_mode");
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
        return targetFromDragID (details.description.toString())
            != seqwencer::ModulationTarget::none;
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
        return juce::jlimit (0, seqwencer::gateModulationTargetCount - 1,
                             static_cast<int> (target) - 1);
    }

    juce::RangedAudioParameter* targetParameterToEdit (
        seqwencer::ModulationTarget target) const
    {
        const auto index = static_cast<std::size_t> (targetIndex (target));
        return isShared() ? targetAParameters[index] : ownTargetParameters[index];
    }

    juce::RangedAudioParameter* enabledParameterToEdit (
        seqwencer::ModulationTarget target) const
    {
        const auto index = static_cast<std::size_t> (targetIndex (target));
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
        for (const auto target : gateTargets)
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
    std::array<juce::RangedAudioParameter*, seqwencer::gateModulationTargetCount>
        ownTargetParameters {};
    std::array<juce::RangedAudioParameter*, seqwencer::gateModulationTargetCount>
        ownTargetEnabledParameters {};
    std::array<juce::RangedAudioParameter*, seqwencer::gateModulationTargetCount>
        targetAParameters {};
    std::array<juce::RangedAudioParameter*, seqwencer::gateModulationTargetCount>
        targetAEnabledParameters {};
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
    lookAndFeel = std::make_unique<SeqwencerLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);
    addAndMakeVisible (content);
    content.setInterceptsMouseClicks (false, true);

    gridA = std::make_unique<StepGrid> (
        processor, 0, juce::Colour (colourA));
    gridB = std::make_unique<StepGrid> (
        processor, 1, juce::Colour (colourB));
    content.addAndMakeVisible (*gridA);
    content.addAndMakeVisible (*gridB);

    baseParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateLevel);
    depthParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::gateDepth);
    shortParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::shortGateLength);
    longParameterLabel = std::make_unique<ModulationParameterLabel> (
        processor, seqwencer::ModulationTarget::longGateLength);
    targetListA = std::make_unique<TargetList> (
        processor, 0, juce::Colour (colourA));
    targetListB = std::make_unique<TargetList> (
        processor, 1, juce::Colour (colourB));
    gateFxButton = std::make_unique<FxSelectorButton> (
        "GATE", juce::Colour (gateAccent));
    phiFxButton = std::make_unique<FxSelectorButton> (
        "PHI", juce::Colour (phiAccent));
    content.addAndMakeVisible (*baseParameterLabel);
    content.addAndMakeVisible (*depthParameterLabel);
    content.addAndMakeVisible (*shortParameterLabel);
    content.addAndMakeVisible (*longParameterLabel);
    content.addAndMakeVisible (*targetListA);
    content.addAndMakeVisible (*targetListB);
    content.addAndMakeVisible (*gateFxButton);
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
    sequenceModeBox.setTooltip (
        "Choose forward looping, end-to-end bouncing, reverse playback, or first-note phrase retriggering");
    content.addAndMakeVisible (sequenceModeBox);

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
    gateFxButton->setTooltip ("Show the Gate controls");
    gateFxButton->getEnableButton().setTooltip (
        "Turn the built-in Gate effect on or off");
    gateFxButton->onClick = [this] { selectFx (SelectedFx::gate); };
    phiFxButton->setTooltip ("Show the PHI integration controls");
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
                             juce::Colour (colourA));
    enableBButton.setColour (juce::ToggleButton::tickColourId,
                             juce::Colour (colourB));
    bipolarAButton.setColour (juce::ToggleButton::tickColourId,
                              juce::Colour (colourA));
    bipolarBButton.setColour (juce::ToggleButton::tickColourId,
                              juce::Colour (colourB));
    enableAButton.setClickingTogglesState (true);
    enableBButton.setClickingTogglesState (true);
    bipolarAButton.setClickingTogglesState (true);
    bipolarBButton.setClickingTogglesState (true);

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
    shortLengthSlider.setRange (0.10, 0.60, 0.01);
    longLengthSlider.setRange (0.65, 0.95, 0.01);
    noiseThresholdSlider.setRange (-80.0, 0.0, 0.1);
    noiseAttackSlider.setRange (0.1, 100.0, 0.1);
    noiseHoldSlider.setRange (0.0, 500.0, 1.0);
    noiseReleaseSlider.setRange (5.0, 1000.0, 1.0);
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
    configureKnob (attackASlider);
    configureKnob (releaseASlider);
    configureKnob (attackBSlider);
    configureKnob (releaseBSlider);

    startSlider.setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (globalAccent));
    endSlider.setColour (juce::Slider::rotarySliderFillColourId,
                         juce::Colour (globalAccent));
    rateSlider.setColour (juce::Slider::rotarySliderFillColourId,
                          juce::Colour (globalAccent));
    baseSlider.setColour (juce::Slider::rotarySliderFillColourId,
                          juce::Colour (globalAccent));
    depthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (globalAccent));
    shortLengthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                                 juce::Colour (globalAccent));
    longLengthSlider.setColour (juce::Slider::rotarySliderFillColourId,
                                juce::Colour (globalAccent));
    for (auto* slider : { &noiseThresholdSlider, &noiseAttackSlider,
                          &noiseHoldSlider, &noiseReleaseSlider,
                          &noiseRangeSlider })
    {
        slider->setColour (juce::Slider::rotarySliderFillColourId,
                           juce::Colour (gateAccent));
    }
    attackASlider.setColour (juce::Slider::rotarySliderFillColourId,
                             juce::Colour (colourA));
    releaseASlider.setColour (juce::Slider::rotarySliderFillColourId,
                              juce::Colour (colourA));
    attackBSlider.setColour (juce::Slider::rotarySliderFillColourId,
                             juce::Colour (colourB));
    releaseBSlider.setColour (juce::Slider::rotarySliderFillColourId,
                              juce::Colour (colourB));

    configureLabel (rateLabel, "RATE");
    configureLabel (sequenceModeLabel, "DIRECTION");
    configureLabel (startLabel, "START");
    configureLabel (endLabel, "END");
    configureLabel (noiseThresholdLabel, "THRESHOLD");
    configureLabel (noiseAttackLabel, "ATTACK");
    configureLabel (noiseHoldLabel, "HOLD");
    configureLabel (noiseReleaseLabel, "RELEASE");
    configureLabel (noiseRangeLabel, "RANGE");
    configureLabel (attackALabel, "ATTACK");
    configureLabel (releaseALabel, "RELEASE");
    configureLabel (attackBLabel, "ATTACK");
    configureLabel (releaseBLabel, "RELEASE");
    configureLabel (laneATitle, "GATE A");
    configureLabel (laneBTitle, "GATE B");
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
    phiBridgeAttachment = std::make_unique<ButtonAttachment> (
        state, "phi_bridge_enabled", phiFxButton->getEnableButton());
    noiseGateAttachment = std::make_unique<ButtonAttachment> (
        state, "noise_gate_enabled", noiseGateButton);
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
    startSlider.onValueChange = [this]
    {
        if (startStepParameter != nullptr)
            handleRangeControl (startSlider, *startStepParameter);
    };
    endSlider.onValueChange = [this]
    {
        if (endStepParameter != nullptr)
            handleRangeControl (endSlider, *endStepParameter);
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
        if (endStepParameter != nullptr)
            endStepParameter->beginChangeGesture();
    };
    endSlider.onDragEnd = [this]
    {
        if (endStepParameter != nullptr)
            endStepParameter->endChangeGesture();
    };

    setResizable (true, true);
    setResizeLimits (960, 488, 1600, 813);
    if (auto* editorConstrainer = getConstrainer())
        editorConstrainer->setFixedAspectRatio (
            static_cast<double> (designWidth)
                / static_cast<double> (designHeight));
    const auto storedSize = loadStoredEditorSize();
    setSize (storedSize.x, storedSize.y);
    bindSelectedEngine();
    updateRangeControls();
    updateFxPanel();
    updateLaneVisuals();
    startTimerHz (30);
}

SeqwencerAudioProcessorEditor::~SeqwencerAudioProcessorEditor()
{
    stopTimer();
    saveStoredEditorSize (getWidth(), getHeight());
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

void SeqwencerAudioProcessorEditor::handleRangeControl (
    juce::Slider& slider,
    juce::RangedAudioParameter& parameter)
{
    if (updatingRangeControls)
        return;

    parameter.setValueNotifyingHost (parameter.convertTo0to1 (
        static_cast<float> (std::lround (slider.getValue()))));
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
    const auto range = seqwencer::makeStepRange (
        static_cast<int> (std::lround (readActual (startStepParameter, 1.0f))),
        static_cast<int> (std::lround (readActual (endStepParameter, 64.0f))),
        maximumStep);

    const juce::ScopedValueSetter<bool> guard (updatingRangeControls, true);
    startSlider.setRange (1.0,
                          static_cast<double> (juce::jmin (
                              range.last + 1, maximumStep - 1)),
                          1.0);
    endSlider.setRange (static_cast<double> (juce::jmax (2, range.first + 1)),
                        static_cast<double> (maximumStep), 1.0);
    startSlider.setValue (static_cast<double> (range.first + 1),
                          juce::dontSendNotification);
    endSlider.setValue (static_cast<double> (range.last + 1),
                        juce::dontSendNotification);
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
    const auto phiSelected = selectedFx == SelectedFx::phi;
    const auto parameterID = [phiSelected] (const juce::String& gateID)
    {
        return phiSelected ? "phi_" + gateID : gateID;
    };

    modeAttachment.reset();
    sequenceModeAttachment.reset();
    rateAttachment.reset();
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
    bipolarAParameter = state.getParameter (parameterID ("seq_a_bipolar"));
    bipolarBParameter = state.getParameter (parameterID ("seq_b_bipolar"));

    modeAttachment = std::make_unique<ComboAttachment> (
        state, parameterID ("playback_mode"), modeBox);
    sequenceModeAttachment = std::make_unique<ComboAttachment> (
        state, parameterID ("sequence_mode"), sequenceModeBox);
    rateAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("rate"), rateSlider);
    attackAAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_a_attack"), attackASlider);
    releaseAAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_a_release"), releaseASlider);
    attackBAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_b_attack"), attackBSlider);
    releaseBAttachment = std::make_unique<SliderAttachment> (
        state, parameterID ("seq_b_release"), releaseBSlider);

    gridA->setPhiEngine (phiSelected);
    gridB->setPhiEngine (phiSelected);
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
    gateFxButton->setSelected (gateSelected);
    phiFxButton->setSelected (! gateSelected);
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
    noiseThresholdLabel.setVisible (gateSelected);
    noiseThresholdSlider.setVisible (gateSelected);
    noiseAttackLabel.setVisible (gateSelected);
    noiseAttackSlider.setVisible (gateSelected);
    noiseHoldLabel.setVisible (gateSelected);
    noiseHoldSlider.setVisible (gateSelected);
    noiseReleaseLabel.setVisible (gateSelected);
    noiseReleaseSlider.setVisible (gateSelected);
    noiseRangeLabel.setVisible (gateSelected);
    noiseRangeSlider.setVisible (gateSelected);
    targetListA->setVisible (gateSelected);
    targetListB->setVisible (gateSelected);
    phiTargetButton.setVisible (phiAvailable && ! gateSelected);

    const auto laneColour = juce::Colour (
        gateSelected ? gateAccent : phiAccent);
    laneATitle.setText (gateSelected ? "GATE A" : "PHI A",
                        juce::dontSendNotification);
    laneBTitle.setText (gateSelected ? "GATE B" : "PHI B",
                        juce::dontSendNotification);
    laneATitle.setColour (juce::Label::textColourId, laneColour);
    laneBTitle.setColour (juce::Label::textColourId, laneColour);

    if (lastPhiAvailability != phiAvailable)
    {
        lastPhiAvailability = phiAvailable;
        repaint();
    }
}

void SeqwencerAudioProcessorEditor::updateLaneVisuals()
{
    const auto readActual = [] (const juce::RangedAudioParameter* parameter)
    {
        return parameter != nullptr
            ? parameter->convertFrom0to1 (parameter->getValue()) : 0.0f;
    };

    auto* selectedFxEnabledParameter = processor.getParameterState().getParameter (
        selectedFx == SelectedFx::gate ? "gate_enabled" : "phi_bridge_enabled");
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
    noiseThresholdLabel.setAlpha (noiseControlsAlpha);
    noiseThresholdSlider.setAlpha (noiseControlsAlpha);
    noiseAttackLabel.setAlpha (noiseControlsAlpha);
    noiseAttackSlider.setAlpha (noiseControlsAlpha);
    noiseHoldLabel.setAlpha (noiseControlsAlpha);
    noiseHoldSlider.setAlpha (noiseControlsAlpha);
    noiseReleaseLabel.setAlpha (noiseControlsAlpha);
    noiseReleaseSlider.setAlpha (noiseControlsAlpha);
    noiseRangeLabel.setAlpha (noiseControlsAlpha);
    noiseRangeSlider.setAlpha (noiseControlsAlpha);

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
                                     juce::Label& attackLabel,
                                     juce::Slider& release,
                                     juce::Label& releaseLabel)
    {
        attack.setAlpha (alpha);
        attackLabel.setAlpha (alpha);
        release.setAlpha (alpha);
        releaseLabel.setAlpha (alpha);
    };

    if (serial)
    {
        gridA->setAlpha (1.0f);
        gridB->setAlpha (1.0f);
        targetListA->setAlpha (1.0f);
        targetListB->setAlpha (1.0f);
        setControlAlpha (profileB ? 0.30f : 1.0f,
                         attackASlider, attackALabel,
                         releaseASlider, releaseALabel);
        setControlAlpha (profileB ? 1.0f : 0.30f,
                         attackBSlider, attackBLabel,
                         releaseBSlider, releaseBLabel);
        bipolarAButton.setAlpha (profileB ? 0.30f : 1.0f);
        bipolarBButton.setAlpha (profileB ? 1.0f : 0.30f);
        laneATitle.setAlpha (profileB ? 0.30f : 1.0f);
        laneBTitle.setAlpha (profileB ? 1.0f : 0.30f);
    }
    else
    {
        const auto alphaA = laneAActive ? 1.0f : 0.30f;
        const auto alphaB = laneBActive ? 1.0f : 0.30f;
        gridA->setAlpha (alphaA);
        gridB->setAlpha (alphaB);
        targetListA->setAlpha (alphaA);
        targetListB->setAlpha (alphaB);
        setControlAlpha (alphaA, attackASlider, attackALabel,
                         releaseASlider, releaseALabel);
        setControlAlpha (alphaB, attackBSlider, attackBLabel,
                         releaseBSlider, releaseBLabel);
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
    graphics.drawText ("DUAL STEP MODULATION & FX  |  STAGE 3.2.2",
                       20, 33, 320, 13,
                       juce::Justification::centredLeft, false);

    const auto topPanel = juce::Rectangle<float> (
        10.0f, 62.0f, static_cast<float> (designWidth - 20), 48.0f);
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
    graphics.drawText (selectedFx == SelectedFx::gate
                           ? "GATE CONTROLS" : "PHI CONTROLS",
                       20, static_cast<int> (bottomPanelY) + 8,
                       92, 15, juce::Justification::centredLeft, false);

    constexpr auto laneTop = 120.0f;
    constexpr auto laneGap = 10.0f;
    const auto laneAreaBottom = bottomPanelY - laneGap;
    const auto laneHeight = (laneAreaBottom - laneTop - laneGap) * 0.5f;
    const auto fxRail = juce::Rectangle<float> (
        10.0f, laneTop, 84.0f, laneAreaBottom - laneTop);
    graphics.setColour (juce::Colour (panel));
    graphics.fillRoundedRectangle (fxRail, 8.0f);
    graphics.setColour (juce::Colour (panelOutline));
    graphics.drawRoundedRectangle (fxRail, 8.0f, 1.0f);
    graphics.setColour (juce::Colour (globalAccent));
    graphics.setFont (juce::FontOptions { 10.0f, juce::Font::bold });
    graphics.drawText ("FX", fxRail.toNearestInt().withHeight (27),
                       juce::Justification::centred, false);

    const auto laneX = 104.0f;
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
            selectedFx == SelectedFx::gate ? gateAccent : phiAccent);
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

    syncButton.setBounds (22, 70, 104, 31);
    presetsButton.setBounds (138, 70, 104, 31);

    const auto bottomPanelY = designHeight - 88;
    gateFxButton->setBounds (20, 182, 64, 31);
    phiFxButton->setBounds (20, 221, 64, 31);
    modeBox.setBounds (20, bottomPanelY + 28, 92, 28);
    rateLabel.setBounds (122, bottomPanelY + 4, 64, 14);
    rateSlider.setBounds (122, bottomPanelY + 15, 64, 59);
    startLabel.setBounds (194, bottomPanelY + 4, 64, 14);
    startSlider.setBounds (194, bottomPanelY + 15, 64, 59);
    endLabel.setBounds (266, bottomPanelY + 4, 64, 14);
    endSlider.setBounds (266, bottomPanelY + 15, 64, 59);
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
    noiseThresholdLabel.setBounds (892, bottomPanelY + 4, 80, 14);
    noiseThresholdSlider.setBounds (900, bottomPanelY + 15, 64, 59);
    noiseAttackLabel.setBounds (972, bottomPanelY + 4, 64, 14);
    noiseAttackSlider.setBounds (972, bottomPanelY + 15, 64, 59);
    noiseHoldLabel.setBounds (1044, bottomPanelY + 4, 64, 14);
    noiseHoldSlider.setBounds (1044, bottomPanelY + 15, 64, 59);
    noiseReleaseLabel.setBounds (1116, bottomPanelY + 4, 64, 14);
    noiseReleaseSlider.setBounds (1116, bottomPanelY + 15, 64, 59);
    noiseRangeLabel.setBounds (1188, bottomPanelY + 4, 64, 14);
    noiseRangeSlider.setBounds (1188, bottomPanelY + 15, 64, 59);

    constexpr int laneTop = 120;
    constexpr int laneGap = 10;
    const auto laneAreaBottom = bottomPanelY - laneGap;
    const auto totalLaneHeight = laneAreaBottom - laneTop - laneGap;
    const auto laneAHeight = totalLaneHeight / 2;
    const auto laneBHeight = totalLaneHeight - laneAHeight;
    constexpr int laneX = 104;
    constexpr int controlsWidth = 150;
    constexpr int targetWidth = 166;
    constexpr int componentGap = 10;
    constexpr int innerMargin = 10;
    const auto placeLane = [&] (int lane,
                                juce::ToggleButton& enabled,
                                juce::Slider& attack,
                                juce::Label& attackLabel,
                                juce::Slider& release,
                                juce::Label& releaseLabel,
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

        auto& targetList = lane == 0 ? *targetListA : *targetListB;
        const auto gateSelected = selectedFx == SelectedFx::gate;
        const auto targetX = gateSelected
            ? designWidth - 10 - innerMargin - targetWidth
            : designWidth - 10 - innerMargin;
        targetList.setBounds (targetX, y + innerMargin,
                              gateSelected ? targetWidth : 0,
                              laneHeight - 2 * innerMargin);
        const auto gridX = laneX + controlsWidth + innerMargin;
        grid.setBounds (gridX, y + innerMargin,
                        juce::jmax (128, targetX - componentGap - gridX),
                        laneHeight - 2 * innerMargin);
    };

    placeLane (0, enableAButton, attackASlider, attackALabel,
               releaseASlider, releaseALabel, *gridA);
    placeLane (1, enableBButton, attackBSlider, attackBLabel,
               releaseBSlider, releaseBLabel, *gridB);
}

void SeqwencerAudioProcessorEditor::timerCallback()
{
    updateRangeControls();
    updateFxPanel();
    updateLaneVisuals();
    baseParameterLabel->repaint();
    depthParameterLabel->repaint();
    shortParameterLabel->repaint();
    longParameterLabel->repaint();
    targetListA->refresh();
    targetListB->refresh();
    gridA->repaint();
    gridB->repaint();
}
