#include <JuceHeader.h>

#include "../EditorLifecycleTrace.h"
#include "../PluginProcessor.h"

#include <iostream>
#include <memory>

namespace
{
bool createAndDestroyEditor (SeqwencerAudioProcessor& processor, int pass)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor (
        processor.createEditorAndMakeActive());

    if (editor == nullptr)
    {
        std::cout << "FAIL: editor pass " << pass << " returned null\n";
        return false;
    }

    processor.editorBeingDeleted (editor.get());
    editor.reset();
    std::cout << "PASS: editor pass " << pass
              << " constructed and destroyed safely\n";
    return true;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    SeqwencerAudioProcessor processor;

    if (! processor.hasEditor())
    {
        std::cout << "FAIL: processor reported no editor\n";
        return 1;
    }

    if (! createAndDestroyEditor (processor, 1)
        || ! createAndDestroyEditor (processor, 2))
        return 1;

    std::cout << "All Seqwencer editor lifecycle tests passed\n";
    return 0;
}
