# Seqwencer

## Coming soon...

[![Release](https://img.shields.io/github/v/release/sl2365/Seqwencer?style=for-the-badge-square&logo=github&logoColor=white&color=purple)](https://github.com/sl2365/Seqwencer/releases/latest/download/Seqwencer.rar)
[![Release Date](https://img.shields.io/github/release-date/sl2365/Seqwencer?style=for-the-badge-square&logo=github&logoColor=white&color=yellow)](https://github.com/sl2365/Seqwencer/releases)

[![Latest Asset Downloads](https://img.shields.io/github/downloads/sl2365/Seqwencer/latest/Seqwencer.rar?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-latest&displayAssetName=false&color=blue)](https://github.com/sl2365/Seqwencer/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/sl2365/Seqwencer/total?style=for-the-badge-square&logo=github&logoColor=white&label=downloads-total&color=blue)](https://github.com/sl2365/Seqwencer/releases)

[![Commits Since Release](https://img.shields.io/github/commits-since/sl2365/Seqwencer/latest?style=for-the-badge-square&logo=github&logoColor=white&color=green)](https://github.com/sl2365/Seqwencer/activity)
[![Last Commit](https://img.shields.io/github/last-commit/sl2365/Seqwencer?style=for-the-badge-square&logo=github&logoColor=white&color=green)](https://github.com/sl2365/Seqwencer/activity)

![Seqwencer](Resources/Seqwencer1.jpg)

Seqwencer is a Windows x64 VST3 dual step-sequencer and effects plug-in by
**sl23**, designed for fast drawing, flexible modulation and simple routing.
Each internal effect owns an independent pair of 32-step sequencers, so its
pattern, speed, range and direction do not have to match the other effects.

Seqwencer can also integrate directly with
[PolyHostInterface (PHI)](https://github.com/sl2365/PolyHostInterface). In PHI,
its A/B lanes can sequence any automatable parameter exposed by another loaded
plug-in. This includes synth, arp and third-party effect parameters—not only
Seqwencer's built-in effects.

## Built-in effects

The current development build includes:

- Gate and Noise Gate
- Delay
- Reverb
- Pan

Filter, Pitch, Distortion and GrainShifter are planned additions. Reverb is
included because sequenced Mix can create useful rhythmic effects even when
sequencing its tail parameters is less suitable.

## Sequencers

- Two 32-step lanes: A and B
- Parallel mode for independent 32-step patterns
- Serial mode for one continuous 64-step pattern
- Unipolar and Bipolar values
- Independent Rate, Start, End, Direction, Attack and Release for every effect
- Loop, Bounce, Reverse and Played directions
- Straight and triplet rates from 1/128 through 1/1
- Waveform drawing presets for each lane
- Per-lane target lists with temporary enable checkboxes and remove buttons
- Host synchronisation and note-triggered playback

## Basic use

1. Load Seqwencer after a synth or other sound-producing plug-in.
2. Click an FX button in the left rail to display that effect's controls and
   saved sequencer pair.
3. Click the small LED inside the FX button to switch the effect on or off.
   Disabled internal effects also pause their private sequencer processing.
4. Click and drag inside a lane to draw its steps.
5. Drag any parameter caption beginning with `::` into the A or B target list.
6. Tick or untick a target-list checkbox to pause or resume that assignment
   without deleting it. Click its `X` to remove the assignment.

The step shortcuts are:

- Double-click: set that step to 0%
- Right-click: set that step to +100%
- Middle-click: set that step to -100% in Bipolar mode, or 0% in Unipolar mode

## Gate modes

Each Gate step has a small mode cell above its value bar:

- Short: opens for the duration selected by Short Step
- Long: opens for the duration selected by Long Step
- Link: remains open continuously into the next step
- Off: silent for that step

Short Step covers 10-60% of a step and Long Step covers 65-95%. Their knobs
change the audio timing without changing the fixed cell symbols.

## Pan

Pan uses a centre-preserving stereo balance. Centre leaves both input channels
unchanged; moving left or right attenuates only the opposite channel and does
not add gain. Pan lanes default to Bipolar so negative values move left,
positive values move right and the centre line represents the Pan knob value.

## PHI parameter control

1. Load the target synth or effect in PHI.
2. Load Seqwencer as an FX tab after it.
3. Select Seqwencer's PHI page and enable its LED.
4. Click `TARGET` to open PHI's integrated Macro Mappings view.
5. Find the required plug-in parameter and tick A, B or both in the Targets
   column. PHI assigns a Macro automatically when required.
6. Return to Seqwencer and draw the PHI A/B patterns.

In Parallel, A and B are independent. In Serial, they share one 64-step target.
Unticking a target temporarily stops Seqwencer control while preserving its PHI
Macro. Removing Seqwencer from PHI hides the Targets column but does not delete
the saved Macro mappings.

The PHI selector and TARGET control are hidden when Seqwencer is used in a host
that does not support this bridge. The built-in audio effects continue to work
normally in other VST3 hosts.

## Presets and portable data

Seqwencer stores all of its portable data beside the plug-in:

```text
Seqwencer.vst3
Data/
  Settings.ini
  Presets/
    Preset Name.ini
```

`Settings.ini` remembers the editor size and lane colours. Presets store the
complete state of every internal effect, its sequencers and target assignments.
Saving over an existing preset requires confirmation.

## Building from source

The supplied project expects this layout:

```text
_Projects/
  Seqwencer/
    - Build.bat
    source/
  _Tools/
    cmake/
      _4.4.2/
        bin/
          cmake.exe
    JUCE/
      _8.0.15/
        CMakeLists.txt
```

Requirements:

- Windows x64
- Visual Studio Community 2026 with Desktop development with C++
- CMake 4.4.2 in the layout above
- JUCE 8.0.15 in the layout above

To build:

1. Close PHI if it is running.
2. Open the `Seqwencer` project folder.
3. Double-click `- Build.bat`.
4. Watch the live output and wait for all seven summary lines to show `PASS`.
5. Find the finished plug-in at `dist\Seqwencer.vst3`.

The same build and test output is saved to `Results.log`. The script does not
install the plug-in elsewhere.

## Development history

See [Changelog.md](Changelog.md) for stage-by-stage changes, regression checks,
test instructions and planned development work.

## Plug-in identity

- Manufacturer: `sl23`
- Manufacturer code: `sl23`
- Plug-in code: `sqw1`
- Bundle ID: `com.sl23.seqwencer`
