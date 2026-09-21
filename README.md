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
- Filter
- Pitch
- Distortion
- Grain Shifter
- Compressor

## Audio routing

Audio flows through the internal FX from top to bottom in the left rail. Drag
the body of any Gate-through-Compressor selector up or down to change its
position in the chain. Its small LED remains the independent on/off control.
The order can therefore be changed between arrangements such as Gate into
Compressor and Compressor into Gate without changing either effect's settings.

PHI remains fixed at the bottom and cannot be dragged because it sends parameter
modulation rather than processing audio inside Seqwencer. The chosen internal
FX order is stored in DAW projects and portable presets. Older projects and
presets that contain no routing order use the original Gate-to-Compressor order.

## Sequencers

- Two 32-step lanes: A and B
- Parallel mode for independent 32-step patterns
- Serial mode for one continuous 64-step pattern
- Unipolar and Bipolar values
- Independent Rate, Start/End or linked Start/Length, Direction, Attack and
  Release for every effect
- Draggable A/B Attack and Release targets on every internal FX page
- Draggable Start, End and Length targets on every internal FX page
- Loop, Bounce, Reverse, Played and Random directions
- Straight and triplet rates from 1/128 through 1/1
- Waveform drawing presets for each lane
- Right-click bulk menus for step values and Gate modes
- Independent left/right nudging for step values and Gate modes
- Per-lane target lists with temporary enable checkboxes and remove buttons
- Target captions that follow the adjustable A/B lane colours, with a distinct
  derived colour when a parameter is assigned to both lanes
- Host synchronisation and note-triggered playback

Random uses repeatable shuffled passes: every selected step is visited once per
pass, the order changes between passes, and saved projects recall the same
sequence reliably.

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
- Right-click: open Zero, Max, Min, Random, Reset, Copy and Paste for the
  complete 32-step lane
- Middle-click: set that step to -100% in Bipolar mode, or 0% in Unipolar mode

`Reset` reloads only the right-clicked lane from the current portable preset;
all other lanes and controls remain untouched. `Copy` and `Paste` transfer the
complete lane between any A or B sequencers, including sequencers belonging to
different FX.

The small `STEPS < >` controls beneath each lane's Attack/Release knobs rotate
the whole 32-step pattern left or right, including wraparound.

On every internal FX page, the `ATTACK` and `RELEASE` captions are also
draggable targets. Drag A Attack, A Release, B Attack or B Release into either
target list to sequence that envelope from lane A, lane B or both. PHI keeps
plain Attack/Release captions because its modulation destinations are selected
through PHI's external target browser.

The chain icon between `START` and `END` controls how the playback range is
described. With the icon off, Start and End retain their original independent
end-point behaviour. Switching it on converts the current inclusive span to a
fixed Length: the `END` caption becomes `LENGTH`, and moving Start slides that
same-size window through the 32-step Parallel or 64-step Serial range. Switching
the icon off converts Start plus Length back to the matching End point. The Link
state and Length are saved independently for every FX and PHI page.

Start, End and Length are separate modulation destinations on every internal
FX. End modulation is active while the icon is off; Length modulation is active
while it is on. Start remains available in both states, so a sequencer can move
a fixed-length playback window without changing its number of steps. PHI shows
the same manual range workflow but keeps range captions local, like its envelope
controls.

## Gate modes

Each Gate step has a small mode cell above its value bar:

- Short: opens for the duration selected by Short Step
- Long: opens for the duration selected by Long Step
- Link: remains open continuously into the next step
- Off: silent for that step

Short Step covers 10-60% of a step and Long Step covers 65-95%. Their knobs
change the audio timing without changing the fixed cell symbols.

Right-click any Gate mode cell for Short, Long, Random, Reset, Copy and Paste
commands covering that complete A or B Gate-mode row. The `GATE < >` controls
shown only on the Gate page rotate all Gate modes independently of the values.

## Pan

Pan uses a centre-preserving stereo balance. Centre leaves both input channels
unchanged; moving left or right attenuates only the opposite channel and does
not add gain. Pan lanes default to Bipolar so negative values move left,
positive values move right and the centre line represents the Pan knob value.

## Filter

Filter Type offers Low Pass, High Pass, Band Pass, Band Reject and Peaking.
Cutoff, Resonance and Mix can each be sequenced from A, B or both lanes. The
Filter has its own patterns, timing, range, direction and envelopes, and its
private sequencer pauses when the Filter is disabled.

## Pitch

Pitch Shift transposes the signal continuously from -24 to +24 semitones, and
Mix blends the shifted signal with the dry input. Shift and Mix can each be
sequenced from A, B or both lanes. Pitch has its own patterns, timing, range,
direction and envelopes; disabling it bypasses the audio processor and pauses
its private sequencer.

## Distortion

Distortion Type offers Soft Clip, Hard Clip, Tube and Foldback shaping. Drive
covers 0-36 dB, Tone darkens or brightens the wet signal, and Mix blends it
with the dry input. Drive, Tone and Mix can each be sequenced from A, B or both
lanes. Distortion has its own patterns, timing, range, direction and envelopes;
disabling it bypasses processing, resets its Tone filter and pauses its private
sequencer.

## Grain Shifter

Grain Shifter uses two overlapping, windowed grains for continuous pitch
movement. Grain covers 10-250 ms, Shift covers -24 to +24 semitones, Feedback
runs from 0-90%, and Mix blends dry and shifted audio. All four controls can be
sequenced from A, B or both lanes. Grain Shifter has independent patterns,
timing, range, direction and envelopes; disabling it clears its working buffer,
bypasses processing and pauses its private sequencer.

## Compressor

Compressor uses a stereo-linked peak detector so both channels receive the
same gain reduction and the stereo image remains stable. Threshold covers
-60-0 dB, Ratio covers 1:1-20:1, Attack covers 0.1-100 ms, Release covers
10-1000 ms, Makeup covers 0-24 dB, and Mix blends dry and compressed audio.
All six controls can be sequenced from A, B or both lanes.

Sequencing Threshold or Mix can create rhythmic, sidechain-style pumping when
used with Gate, even though the Compressor does not require an external
sidechain input. Compressor has independent patterns, timing, range, direction
and envelopes; disabling it bypasses processing, resets its detector gain and
pauses its private sequencer. Drag-to-reorder routing allows positions such as
Gate into Compressor and Compressor into Gate.

## PHI parameter control

1. Load the target synth or effect in PHI.
2. Load Seqwencer as an FX tab after it.
3. Select Seqwencer's PHI page and enable its LED.
4. Click `TARGETS` to open PHI's integrated Macro Mappings view.
5. Find the required plug-in parameter and tick A, B or both in the Targets
   column. PHI assigns a Macro automatically when required.
6. Return to Seqwencer and draw the PHI A/B patterns.

In Parallel, A and B are independent. In Serial, they share one 64-step target.
Unticking a target temporarily stops Seqwencer control while preserving its PHI
Macro. Removing Seqwencer from PHI hides the Targets column but does not delete
the saved Macro mappings.

The PHI selector and TARGETS control are hidden when Seqwencer is used in a host
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

The A Colour and B Colour knobs also control the colour of any draggable
parameter caption assigned to that lane. A caption assigned to both lanes uses
a contrasting colour calculated from the two current lane colours, so dual
assignments remain visible when either colour knob is changed.

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
