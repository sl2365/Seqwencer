#pragma once

#include <JuceHeader.h>

namespace seqwencer::editorTrace
{
inline bool isEnabled()
{
    // Temporary Stage 1.3 crash tracing. Keep this unconditional so the
    // independent test host and PHI both record the editor checkpoints.
    return true;
}

inline juce::File getFile()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("Seqwencer-Editor-Diagnostic.log");
}

inline void write (const juce::String& message)
{
    if (! isEnabled())
        return;

    const auto line = juce::Time::getCurrentTime().toString (
        true, true, true, true) + " | " + message + "\r\n";
    getFile().appendText (line, false, false, "\r\n");
}

inline void reset()
{
    if (! isEnabled())
        return;

    getFile().deleteFile();
    write ("00 processor construction entered");
}
}
