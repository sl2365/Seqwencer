# Changelog

Seqwencer development history, regression notes and planned work.

## v1.3.18.1 fixes and features

- Replaced the subdivision Off character with a directly drawn centred dot so
  it renders correctly and consistently at every GUI scale
- Increased the `H`, `2` and `3` subdivision labels from 9 to 10 pixels
- Added a right-click menu to the subdivision row with Off, H, 2 and 3 commands
  that set all 32 steps in the selected lane
- Newly activated subdivision segments inherit their main step's current
  height, while heights belonging to segments already in use are preserved

## v1.3.18.0 features

- Added a compact subdivision button beneath every main step on every FX and
  PHI sequencer page
- The `•` mode keeps the original full-width step, `H` uses the first half and
  clears the second, `2` creates two equal segments, and `3` creates three
- Every active segment has an independently drawable height while continuing
  to use its lane's existing Attack and Release controls
- Rate, Start, End and linked Length still count the original main steps;
  subdivision happens only inside each step's existing duration
- Subdivisions follow Loop, Bounce, Reverse, Played and Random traversal and
  work across the A-to-B boundary in Serial mode
- Step Copy/Paste, Reset, left/right nudge, waveform application and bulk value
  commands now include subdivision modes and segment heights
- Subdivision data is stored in DAW projects and portable presets without
  adding thousands of entries to the host automation list
- Older projects and presets default every step to `•`, retaining their exact
  previous behaviour
- Removed the `Coming soon...` heading from README and shortened its opening
  description as requested

## v1.3.17.1 fixes

- Increased the gap between Direction and Type on the Filter and Distortion
  pages to match the spacing used by PHI's adjacent control
- Shifted the remaining Filter and Distortion controls by the same amount so
  their established internal spacing is preserved
- Renamed PHI's `TARGET` button to `TARGETS` to better describe the list it
  opens

## v1.3.17.0 features

- Added a chain-link control between the existing Start and End knobs without
  changing either knob's position
- Link off preserves the original independent Start/End range workflow
- Link on changes the second knob and caption from End to Length; moving Start
  then slides the same inclusive step count through the available range
- Enabling Link converts the current Start/End span into its matching Length,
  while disabling Link converts Start/Length back into the matching End point
- Every Gate, Delay, Reverb, Pan, Filter, Pitch, Distortion, Grain Shifter and
  Compressor sequencer now exposes separate Start, End and Length targets to
  both source lanes
- Start targets remain active in both range modes; End targets apply with Link
  off and Length targets apply with Link on
- PHI receives the same saved manual Link/Length workflow but keeps its range
  captions non-draggable because PHI destinations are selected externally
- Host-synchronised and free-running phase now retain the unwrapped timeline so
  a modulated range length can change without resetting to the old cycle length
- Core tests cover linked movement, conversion helpers, Start/End/Length target
  identity and modulated range clamping; the VST3 probe verifies all Link,
  Length and range-target parameters

## v1.3.16.0 features

- Draggable parameter captions assigned to Sequencer A now use the live colour
  selected by the A Colour knob instead of the original fixed turquoise
- Captions assigned to Sequencer B likewise follow the live B Colour setting
- A caption assigned to both sequencers uses a dynamically calculated third
  colour based on the current A/B hues
- The dual-assignment colour is the complement of the circular A/B hue
  midpoint, keeping it distinguishable even when A and B use the same colour
- Opposing A/B hues use a stable quarter-turn fallback rather than producing
  an undefined or flickering midpoint colour
- The drag handle and clear cross follow the same live assignment colour as
  the caption
- Core tests cover identical, wraparound and opposing A/B colour combinations

## v1.3.15.0 features and fixes

- Every internal FX page now exposes its Sequencer A and Sequencer B Attack and
  Release captions as drag targets
- Either lane can control A Attack, A Release, B Attack or B Release, including
  assigning the same envelope control to both lanes
- Envelope targets follow the same temporary enable, remove, Parallel, Serial,
  Unipolar and Bipolar behaviour as existing parameter targets
- PHI Attack and Release remain local controls because PHI targets parameters
  in its external host rather than Seqwencer's internal FX engines
- Preset and DAW-state compatibility is preserved by appending the new target
  IDs after all existing modulation targets
- Fixed the independent probe's Reverse and Played checks: after Random became
  the fifth Direction choice, the probe still used normalized fractions from
  the previous four-choice list and accidentally selected other modes
- Direction probe choices are now selected by index and parameter step count,
  so another appended Direction mode cannot silently retarget these checks
- Core tests cover envelope-target identity, ownership, lane mapping and
  Bipolar support; the VST3 probe verifies every internal FX exposes all four
  envelope destinations to both source lanes

## v1.3.14.0 features

- Added Random as a fifth Direction choice for Gate, PHI, Delay, Reverb, Pan,
  Filter, Pitch, Distortion, Grain Shifter and Compressor
- Random traverses the selected Start/End range in shuffled passes, visiting
  every selected step once before beginning the next shuffled pass
- The shuffled traversal is deterministic so DAW projects and portable presets
  recall consistently, while each of eight successive passes changes order
- Existing Direction values remain compatible: Loop, Bounce, Reverse and
  Played retain their original choice indices
- Core tests now verify Random range safety, complete shuffled passes and
  repeatability; the VST3 probe verifies all ten Direction parameters

## Stage 3.13.1 fixes

- Step-pattern Copy/Paste now works between any two sequencers, including A/B
  lanes belonging to different FX
- Corrected the independent VST3 probe so it reads and edits Seqwencer's
  processor state inside JUCE's outer `VST3PluginState` wrapper
- The routing test now checks the same embedded state that a VST3 host actually
  saves and restores

## Stage 3.13.0 features and fixes

- Right-clicking a step lane now opens Zero, Max, Min, Random, Reset, Copy and
  Paste commands for all 32 values in that lane
- Right-clicking the Gate mode row now opens Short, Long, Random, Reset, Copy
  and Paste commands for all 32 Gate modes in that lane
- Copy/Paste transfers the selected FX pattern or Gate-mode row between A and B
  without changing any other sequencer data
- Reset reloads only the selected step lane or Gate-mode row from the current
  portable preset; INITIAL uses the parameter defaults
- Added `STEPS < >` beneath both lanes for one-step rotation with wraparound
- Added independent `GATE < >` rotation controls that appear only on the Gate
  page
- The plug-in state writer now always injects the saved audio FX order into the
  emitted state, fixing the independent VST3 routing assertion on hosts that
  rebuild the initial parameter tree after construction
- A failed VST3 probe now produces one `FAIL` summary line instead of reporting
  the same exit condition twice
- Core tests verify left and right pattern rotation and wraparound

## Stage 3.12.0 features

- Internal FX selectors from Gate through Compressor can be dragged vertically
  to change the real top-to-bottom audio-processing order
- The left rail immediately follows the new order while the selected page,
  enabled states, patterns, controls and effect tails remain intact
- PHI remains fixed at the bottom and is deliberately excluded from audio-chain
  routing because it sends modulation to the host rather than processing audio
- FX order is saved with DAW plug-in state and in a portable preset's new
  `[Routing]` section
- Existing DAW projects and older portable presets without routing data retain
  the original Gate, Delay, Reverb, Pan, Filter, Pitch, Distortion, Grain and
  Compressor order
- Core tests verify moving and repairing FX orders; the independent VST3 probe
  verifies saved routing and an audible Gate/Distortion order change

## Stage 3.11.0 features

- Added a periwinkle Compressor selector with its own independent A/B
  sequencer page
- Threshold covers -60-0 dB, Ratio covers 1:1-20:1, Attack covers 0.1-100 ms,
  Release covers 10-1000 ms, Makeup covers 0-24 dB, and Mix blends dry and wet
- A stereo-linked peak detector applies identical gain reduction to both
  channels so the stereo image does not move during compression
- Threshold, Ratio, Attack, Release, Makeup and Mix can each be dragged to A,
  B or both Compressor target lists
- Compressor has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- Compressor is disabled by default; disabling it bypasses audio processing,
  resets gain reduction and pauses its private sequencer phase
- Compressor state is stored in its own `[Compressor]` section in portable
  presets
- The release probe verifies all six controls and targets, independent timing
  and pattern data, stereo-linked gain reduction and clean disabled pass-through

## Stage 3.10.0 features

- Added a lime-green Grain Shifter selector with its own independent A/B
  sequencer page
- Grain covers 10-250 ms, Shift covers -24 to +24 semitones, Feedback covers
  0-90%, and Mix blends dry and shifted audio
- Two overlapping Hann-windowed read heads keep the granular transposition
  continuous while the circular buffer supports controlled feedback
- Grain, Shift, Feedback and Mix can each be dragged to A, B or both Grain
  Shifter target lists
- Grain Shifter has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- Grain Shifter is disabled by default; disabling it bypasses audio processing,
  clears its working buffer and pauses its private sequencer phase
- Grain Shifter state is stored in its own `[GrainShifter]` section in portable
  presets
- The release probe uses realistic 480-sample host blocks to verify octave-up
  Grain Shift processing, target independence and clean disabled pass-through

## Stage 3.9.1 fixes

- Shortened the Distortion selector label from DISTORT to DIST
- Corrected the independent VST3 probe's Pitch test so its one-second signal
  is processed in realistic 480-sample host blocks instead of passing a
  48,000-sample buffer after declaring a 480-sample maximum block size
- This removes the Windows `0xC0000005` access violation that appeared after
  the Filter checks; the two final FAIL lines were duplicate reports of that
  one probe crash

## Stage 3.9.0 features

- Added a red-orange Distortion FX selector with an independent A/B sequencer
  page
- Distortion Type offers Soft Clip, Hard Clip, Tube and Foldback shaping
- Drive covers 0-36 dB; Tone runs from dark to bright; Mix blends dry and wet
- Drive, Tone and Mix can each be dragged to A, B or both Distortion target
  lists
- Distortion has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- Distortion is disabled by default; disabling it bypasses audio processing,
  resets its Tone filter and pauses its private sequencer phase
- Distortion state is stored in its own `[Distortion]` section in portable
  presets
- The VST3 release probe verifies all four shapers, Tone response, targets,
  independent timing/pattern data and clean disabled pass-through

## Stage 3.8.0 features

- Added a cyan Pitch FX selector with its own independent A/B sequencer page
- Shift covers -24 to +24 semitones and Mix blends dry and shifted audio
- Shift and Mix can each be dragged to A, B or both Pitch target lists
- Pitch has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- A dual-window time-domain shifter crossfades overlapping read heads to avoid
  the hard discontinuities of a single moving delay tap
- Zero semitones passes the wet signal without unnecessary delay or colouring
- Pitch is disabled by default; disabling it bypasses audio processing, clears
  its working buffer and pauses its private sequencer phase
- Pitch state is stored in its own `[Pitch]` section in portable presets
- The VST3 release probe verifies Pitch parameters and targets, independent
  timing/pattern data, octave-up processing and clean disabled pass-through

## Stage 3.7.0 features

- Added a mint-green Filter FX selector with its own independent A/B
  sequencer page
- Filter Type offers Low Pass, High Pass, Band Pass, Band Reject and Peaking
- Cutoff covers 20 Hz to 20 kHz; Resonance and Mix are available alongside it
- Cutoff, Resonance and Mix can each be dragged to A, B or both Filter target
  lists
- Filter has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- Filter is disabled by default; disabling it bypasses its audio processing and
  pauses its private sequencer phase
- Filter state is stored in its own `[Filter]` section in portable presets
- The VST3 release probe verifies all five responses, all Filter targets,
  independent timing/pattern data, audio processing and clean disabled
  pass-through

## Stage 3.6.1 fixes

- Corrected the independent VST3 Reverb probe so JUCE's intentional 10 ms
  wet/dry gain slew is not mistaken for a failed audio tail
- The probe still requires a genuine delayed Reverb tail and clean disabled
  pass-through; no Reverb or Pan audio behaviour was changed

## Stage 3.6.0 features

- Added a gold Pan FX selector and widened every FX selector for clearer labels
- Pan has its own Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- The Pan knob works as a static stereo position and can be dragged to A, B or
  both target lists for modulation around its current value
- Centre leaves both channels unchanged; hard left or right attenuates only the
  opposite channel without adding gain
- Disabled internal effects stop their private sequencer phase and per-sample
  sequence evaluation as well as bypassing their audio processor
- Pan state is stored in a separate `[Pan]` portable-preset section

## Stage 3.5.0 features

- Added a rose Reverb FX selector with its own independent A/B sequencer page
- Reverb has independent Mode, Rate, Start, End, Direction, patterns,
  Attack/Release, Bipolar state and SERIAL control profile
- Size, Damping, Width and Mix can each be dragged to A, B or both target lists
- Reverb is disabled by default and processes after Delay when enabled
- Reverb state is stored in its own `[Reverb]` section in portable presets
- The VST3 release probe verifies all eight Reverb target switches, a real wet
  audio tail and clean pass-through when Reverb is disabled

## Stage 3.4.1 features

- Every bottom-panel parameter caption now uses the same 10-pixel bold font
- Gate uses the complete VOLUME, DEPTH, ATTACK, RELEASE and RANGE captions;
  Threshold is intentionally shortened to THRESH
- Delay Feedback is intentionally shortened to FDBK
- Knob value text uses one fixed font scale, and Delay Time has enough width
  for its complete millisecond value without shrinking or truncation

## Stage 3.4.0 features

- Added a purple Delay FX page with an independent A/B sequencer engine
- Delay has its own Mode, Rate, Start, End, Direction, envelopes, polarity,
  patterns and SERIAL profile
- Delay Time covers 10-2000 ms; smooth fractional-delay interpolation gives
  tape-style pitch movement without stepped zipper noise
- Time, Feedback and Mix can each be dragged to A, B or both Delay target lists
- Threshold, Noise Attack, Hold, Noise Release and Range can now be dragged to
  either Gate target list and respond to Unipolar or Bipolar patterns
- Gate, Delay and PHI pattern/timing data are independently regression-tested
- A/B Colour dials now reserve the same lower value area as bottom-row knobs,
  making their actual rotary circles the same size
- Portable preset INI files now include a separate Delay section

- Added separate Waveform A and Waveform B menus with Saw, Saw Down, Sine,
  Triangle, Pulse 25, Square and Pulse 75 patterns, plus two-cycle Double variants
- Each waveform menu fills only its own 32-step bank for the currently selected FX
- Waveforms use the complete visible height in both Unipolar and Bipolar mode
- Added portable A Colour and B Colour knobs to the global top panel
- The top panel now matches the bottom panel's height, and its colour knobs
  match the bottom-row rotary controls in size
- Colour knobs stop at their minimum and maximum and double-click back to the
  original teal/orange defaults
- Lane colours now cover step bars, Gate-mode cells, A/B controls and target
  lists while the selected FX retains ownership of lane titles and borders
- Bottom-row rotary controls now use the currently selected FX colour
- Sequencer colours are retained in `Data/Settings.ini`

- Noise Gate now detects the signal after Gate Volume/step processing, so
  quieter sequenced steps can fall below Threshold while loud steps pass
- The ordinary rhythmic Gate and Noise Gate work independently or in series
- Removed the redundant Mix control; Depth remains the rhythmic strength control
- Returned every bottom-panel control to one compact row
- Mode now sits below the current FX-controls heading
- Removed the extra Noise Gate heading because its button identifies the group
- Portable preset INI files now group values into Global, Gate, PHI and Delay sections

- Gate-mode cells now shape Gate timing even when Volume is not assigned to a
  sequencer; assigning Volume independently enables the drawn bar levels
- Added an optional input-level noise gate with Threshold, Attack, Hold,
  Release and Range controls
- Noise Gate is disabled by default and uses a stereo-linked detector
- Range controls closed-gate attenuation; it is separate from the rhythmic
  sequencer Depth control
- Presets moved into the global top section
- Bottom-panel knobs use an even horizontal spacing grid

- Gate and PHI have independent A/B patterns and playback engines
- Selecting an FX swaps the editor onto that FX's saved sequencer state
- Mode, Rate, Start, End and Direction are per-FX controls at the left of the
  bottom panel
- Gate-only Volume, Depth, Short Step and Long Step follow the common
  controls; PHI instead shows Target
- Dynamic lane titles identify the selected FX above each lane control group
- Gate uses a green selector/border accent and PHI uses blue; step bars default
  to the established teal A and orange B colours and can now be recoloured
- Gate-mode cells and internal target lists appear only on the Gate page
- Changing the visible FX page never interrupts enabled playback; switching an
  FX off pauses its private sequencer and saves its pattern for later
- The independent VST3 probe verifies that Gate and PHI timing and step data
  cannot change one another

- Per-FX **DIRECTION** offers LOOP, BOUNCE, REVERSE and PLAYED traversal
- PLAYED retriggers only on the 0-to-1 held-note transition
- Additional held notes do not restart chords or legato phrases
- After every note is released, the next note starts a new phrase at Start
- Volume, Depth, Short Step and Long Step can each be assigned to A, B or both
- Each internal target has its own temporary enable checkbox and remove cross
- Portable preset browser saves the complete musical state to `Data/Presets`
- Saving over an existing preset requires explicit confirmation
- The resized window is remembered in `Data/Settings.ini`
- BOUNCE changes direction without playing either endpoint twice
- All traversal modes respect Start/End, Parallel/SERIAL, Gate modes and PHI targets
- The independent VST3 probe now reads hosted proxy parameters correctly instead
  of requiring the plug-in's internal parameter class

- Seqwencer's **TARGET** button opens PHI's integrated Macro Mappings view
- PHI always lists every automatable hosted parameter in slim sortable rows
- The view uses Tab, Plugin, Parameter, Mapped, optional Targets and Macro columns
- The Targets A/B column appears only while Seqwencer is loaded in PHI
- Mapped assigns the next free Macro, or pauses/resumes an existing Macro
- Pausing retains the Macro number and both Seqwencer assignments
- Replace keeps the Macro number and changes its destination to the last touched parameter
- X permanently deletes the Macro and its A/B assignments after confirmation
- Column sorting changes display order only and never reassigns a Macro
- Assigned Only toggles between every parameter and parameters that already
  have a Macro number, including paused mappings
- Text in the first three columns uses a fixed 25-colour tab palette that
  repeats after Tab 25; row backgrounds and mapping controls remain unchanged
- Targets is completely removed whenever Seqwencer is not loaded
- Macro retains a defined left divider with or without the Targets column
- Macro drag-to-reorder has been removed
- PHI snapshots the outer DAW timeline once per audio block and forwards a stable
  playhead to every hosted plug-in
- PHI derives PPQ from the outer sample/second timeline when the DAW omits PPQ
- HOST SYNC detects a moving nested-host timeline even if its play flag is wrong
- Inside PHI, a stalled nested timeline uses a continuous tempo-synchronised
  clock instead of repeatedly returning to the same position
- If the entire host chain omits transport-position fields, HOST SYNC safely
  falls back to tempo-synchronised free-run instead of freezing

- FX button bodies select which controls appear in the bottom panel
- Each FX button has a separate LED that turns the effect on or off
- Gate selection shows Volume, Depth, Short Step and Long Step in the bottom panel
- PHI selection shows **TARGET** in the bottom panel
- The PHI selector is hidden outside PHI and revealed by a private PHI presence
  message when Seqwencer is loaded there
- Clicking A/B in the target browser updates only the browser table, avoiding
  a complete PHI interface refresh and its associated flicker
- Unmapped Macro cells are blank rather than displaying a malformed dash
- **TARGET** opens PHI's integrated sortable Macro Mappings table
- Table columns: Tab, Plugin, Parameter, Mapped, Targets and Macro
- Click any header to sort the view without changing a setting
- Search by PHI tab name, plug-in name or parameter name
- Tick A or B to route a parameter to either sequencer
- Checking an unmapped row automatically uses the next free PHI macro
- Unticking both lanes pauses Seqwencer control but preserves the macro mapping
- Click the visible cross beside a mapped Macro number to delete it after
  confirmation
- Targets and Macro use a clearly defined column divider
- Seqwencer is excluded from its own target list to prevent feedback
- Parallel A/B assignments and values are independent
- If both lanes address one Parallel target, the value furthest from zero wins;
  exact magnitude ties use A
- SERIAL mirrors A and B as one shared 64-step target while preserving the
  separate B Parallel assignment
- High-resolution 14-bit values drive every assigned PHI macro
- Gate-mode cells affect only the audio Gate and never alter PHI modulation
- Bridge output is consumed privately by PHI rather than forwarded as MIDI
- Bridge-driven changes do not replace PHI's last-touched parameter or mark the
  preset dirty on every audio block
- Independent VST3 probe checks the emitted lane state, 75% test value and
  nested-host timing fallback directly

- Two visible 32-step sequencers, A and B
- Paint a curve across many steps with one left-mouse drag
- Independent Attack and Release transition controls per sequencer
- Curve preview and active-step indication
- Compact Gate-mode cell above every step, aligned to the bar width and only
  68% as tall
- Four Gate states per step: Off, Short, Long and Link
- Short length is adjustable from 10-60% and defaults to 50%
- Long length is adjustable from 65-95% and defaults to 90%
- Link crosses directly into the following step without a forced closure
- Equal adjacent Link steps form one continuous held level without a boundary dip
- Gate-mode cells process every left press without waiting for the system
  double-click interval
- Gate modes affect only the built-in audio Gate and do not alter PHI targets
- Gate controls and mode cells dim when Gate is disabled; stored modes remain intact
- Host-synchronised rates from 1/128 to 1/1
- Triplet rates: 1/64T, 1/32T, 1/16T, 1/8T, 1/4T and 1/2T
- Stepped Rate knob with the selected timing shown beneath it
- Free-running fallback when Host Sync is switched off
- Per-FX Start and End controls
- Parallel ranges address steps 1-32 in both banks
- SERIAL ranges address A1-A32 as 1-32 and B1-B32 as 33-64
- Steps outside the active range remain editable but are visibly shaded
- The transition into Start correctly comes from End for a seamless loop
- Compact `A` and `B` lane buttons
- Disabled Parallel lanes dim their controls, target list and step grid
- Per-lane Bipolar switches and centred bipolar grid displays
- One permanent signed value per step, retained across polarity switches
- Unipolar shows and plays negative Bipolar steps as 0% without deleting them
- Switching back to Bipolar restores every hidden negative step exactly
- Positive values keep the same musical percentage in either view
- Bipolar positive steps drive Gate at their stated percentage, including when
  Volume is 100%; negative steps produce a 0% Gate level
- In SERIAL, A/B selects which Attack/Release/Bipolar profile shapes the
  complete range; both step grids remain active
- Gate moved into a dedicated left-side FX rail
- Gate Volume and Depth controls in a full-width bottom panel
- Removed the unnecessary status bar and reclaimed its height
- Consistent ten-pixel gaps between the major interface sections
- Fixed-aspect-ratio whole-interface zoom: panels, controls, text, borders,
  lists, grids and mouse areas all scale together
- Four-dot marker on modulation-capable parameter names
- Parameter captions remain centred whether or not their remove cross is shown
- Ordinary left-drag knob operation, with no special right-click gesture
- Drag a modulation-capable parameter name onto either sequencer
- Scrollable target list on the right of each sequencer
- Per-target checkbox for temporary bypass without deletion
- Target checkboxes remain independent between A and B in Parallel mode
- Per-target cross for removal from only that sequencer
- Cross beside a parameter name for clearing all of its assignments
- Assignment colour feedback: teal for A, orange for B and violet for both
- Parallel mode: each active lane controls its own assigned targets
- Parallel lanes assigned to Volume multiply together
- SERIAL mode: A supplies steps 1-32, B supplies steps 33-64 and both lists
  edit A's shared destination
- Complete parameter, target-enable and pattern restoration with the host or
  Seqwencer's portable preset browser
- Mono and stereo audio support
- Independent JUCE VST3 editor probe matching PHI's editor check

The current Gate is a trance-gate-style volume effect: it rhythmically shapes
audio gain rather than acting as a general volume fader. It remains off by
default, so inserting Seqwencer does not initially alter the audio.

Every step has a separate Gate-mode cell above its volume bar. Click a cell to
cycle `Long -> Link -> Off -> Short -> Long`. Off closes the Gate for the step,
Short and Long close at their adjustable Gate-panel lengths, while Link remains open through
the boundary and transitions directly to the following step value. Gate modes
are ignored when Gate is disabled. They do not change PHI-controlled parameters
or the Pitch, Pan, Filter and other internal modulation targets. Gate-mode timing remains active
without a Volume target; assigning Volume additionally makes the drawn bar
heights control the audio level.

Gate is a unipolar volume destination, so positive Bipolar steps retain their
ordinary Gate percentages: +100%, +75%, +50% and +25% produce the same levels
as 100%, 75%, 50% and 25% in Unipolar. Negative steps produce 0% for Gate
because volume has no negative range. Their original signed values remain
stored and remain available to bipolar-capable destinations such as Pan or
Pitch. Two Parallel lanes assigned to Gate continue to multiply, and
the final Gate result always stays inside its valid range.

## Stage 3.6.0 test

1. Load a synth in PHI, followed by Seqwencer, then start playback.
   Select Pan, enable its LED and move PAN left and right. Confirm centre leaves
   the stereo signal unchanged and the extremes mute only the opposite channel.
   Drag PAN to A or B, draw a bipolar pattern and confirm the stereo position
   follows that lane. Switch Pan off and confirm clean passthrough.
2. Confirm the top panel retains the familiar styling and that the target lists
   sit to the right of their grids.
3. Confirm Rate is now a knob and its selected timing is shown underneath it.
4. Turn Rate through every choice and confirm the six triplets appear between
   their neighbouring straight divisions.
5. Confirm every bar has a compact Gate-mode cell directly above it and that
   each cell is shorter than it is wide.
6. Switch Gate on and click a mode cell repeatedly. Confirm it cycles through
   Long, Link, Off and Short using the full fill, arrow, empty and half-fill
   appearances.
7. Set active steps to 100%. Confirm SHORT STEP changes the audible Short-cell
   length from 10-60%, and LONG STEP changes Long cells from 65-95%. Their mode
   squares must retain the same fixed appearance. Confirm Off remains silent
   and Link remains open into the next step.
8. Set two or more neighbouring steps to Link at the same value and confirm
   they sound like one held section without boundary dips.
9. In SERIAL, confirm a Link on A32 passes continuously into B1. Use a smaller
   Start/End range and confirm an End Link continues into Start.
10. Turn Gate off and confirm Volume, Depth and both mode rows dim, the audio
    passes normally, and switching Gate on restores the stored modes.
11. Confirm straight and triplet rates both sequence the synth.
12. Drag the Volume name to A, B and both lists; confirm teal, orange and violet
   assignment feedback.
13. In Parallel, untick each target and confirm only that lane's checkbox and
   modulation are disabled. In SERIAL, confirm the two shared lists mirror the
   same continuous destination.
14. Re-tick it, then use the row cross and the Volume-name cross to verify the two
   different removal scopes.
15. Confirm the Volume knob still responds to an ordinary left drag.
16. In Parallel, switch each lane off and confirm its controls, targets and grid
   dim independently.
17. In SERIAL, click A and B and confirm the selected Attack/Release/Bipolar
   profile alone remains bright and shapes both grids while playback continues.
18. In Parallel, set Start/End to a smaller range and confirm both banks loop
    that range and the unused steps are shaded.
19. Select SERIAL and confirm End can reach 64, ranges can cross the A/B
    boundary, and the active step returns from End to Start.
20. Enable Bipolar in each Parallel lane and confirm the centre line represents
    signed 0%. In SERIAL, confirm the selected A/B profile also selects Bipolar.
21. With Volume at 100%, draw Unipolar steps at 100%, 75%, 50% and 25%, then
    enable Bipolar. Confirm they read +100%, +75%, +50% and +25% and that Gate
    continues producing those same four audible levels rather than bypassing.
22. Draw several negative Bipolar steps, change to Unipolar and confirm they
    display and play as 0%. Change back to Bipolar and confirm the exact
    negative pattern returns.
23. Double-click one step and confirm only that step becomes 0%. Confirm
    right-click opens the complete-lane editing menu, and Bipolar middle-click
    sets one step to -100%.
24. Save and reopen the PHI project and confirm Gate modes, Rate, ranges,
    polarity, patterns, assignments, checkbox states and the SERIAL profile return.
25. Resize from a corner and confirm every interface element and mouse area
    zooms uniformly, with no fixed-size controls left behind.
26. Click Reverb in the FX rail and confirm the lane headings, borders and
    bottom-row controls change to the rose Reverb page.
27. Toggle the Reverb LED and confirm disabled Reverb passes the source cleanly,
    while enabled Reverb produces an audible tail.
28. Adjust Size, Damping, Width and Mix and confirm each changes the Reverb
    without altering the saved Gate, Delay or PHI page.
29. Drag each Reverb caption to A, B and both target lists. Confirm independent
    checkboxes, remove crosses and teal/orange/violet assignment feedback.
30. Give Reverb a different Rate, range, Direction and A/B pattern, switch among
    all FX pages, and confirm every page retains its own complete settings.
31. Save a Seqwencer preset and confirm it contains a `[Reverb]` section; reload
    it and confirm the Reverb controls, patterns and targets are restored.
26. Remove the Volume target, then confirm Off, Short, Long and Link still shape
    Gate timing while the bar heights no longer change Volume.
27. Assign Volume, create repeating 100%, 25%, 25%, 25% steps, then place
    Threshold between those two audible levels. Confirm the first step passes
    and the three quieter steps are closed. Disable Noise Gate and confirm the
    ordinary 100%/25% Gate pattern remains.
28. Confirm Presets is in the global top section and every rotary control in
    the single bottom-panel row is evenly spaced. Save a preset and confirm its
    INI file contains separate Global, Gate and PHI sections.
26. Click one Gate-mode cell repeatedly and quickly. Confirm every click cycles
    it immediately through Long, Link, Off and Short without requiring a pause.
27. Confirm the PHI FX button is visible when Seqwencer runs in PHI. Click its
    body and confirm the bottom panel changes from Gate controls to PHI controls,
    then click **TARGET** and confirm PHI opens its integrated Macro Mappings
    view with Tab, Plugin, Parameter, Mapped, Targets and Macro columns.
28. Click each header and confirm it only sorts the rows without changing Macro
    numbers. Search for an obvious synth parameter and tick Mapped; confirm the
    next free three-digit Macro number appears. Untick and re-tick Mapped and
    confirm the same number is paused and resumed.
29. Click Assigned Only and confirm unmapped rows disappear while mapped and
    paused rows remain. Confirm only the text in Tab, Plugin and Parameter uses
    each tab's fixed colour; row backgrounds must retain the normal dark style.
30. Tick A for that parameter and confirm Seqwencer controls it. Confirm the
    Targets column disappears if Seqwencer is removed from PHI and returns when
    it is loaded again.
31. In Parallel, tick B as well, then untick A. Confirm B remains checked and
    continues controlling the parameter.
32. Assign different parameters to A and B and confirm both follow their own
    patterns. Assign one parameter to both, enable Bipolar, and confirm the
    current value furthest from zero wins; use equal magnitudes to confirm A
    wins the tie.
33. Select SERIAL and reopen TARGET. Confirm A and B mirror the same shared
    target, then return to Parallel and confirm B's earlier assignment returns.
34. Untick both boxes and confirm Seqwencer stops controlling the parameter but
    its Macro number remains. Re-tick one box and confirm control resumes.
35. Touch another plug-in parameter and click Replace beside the Macro. Confirm
    its number stays fixed while its destination changes.
36. Click the cross beside a mapped Macro number and cancel once. Click it
    again and confirm deletion; verify the Macro cell and both boxes clear.
37. Give Gate and PHI visibly different A/B patterns, Rates, Start/End ranges
    and Directions. Switch between the green Gate and blue PHI selectors and
    confirm every page recalls its own values without altering the other.
38. Confirm the common controls appear first in the bottom panel, followed by
    Gate's Volume, Depth, Short Step and Long Step controls, while the PHI
    page shows Target instead of Gate-only controls.
39. Confirm the lane titles change between GATE A/B and PHI A/B, the two outer
    lane borders match the selected FX colour, and the teal/orange step bars do
    not change colour.
40. Leave Gate audible, select PHI and confirm Gate continues running. Then
    leave PHI controlling a macro, select Gate and confirm PHI continues running.
37. Switch the **PHI LED** off and confirm mapped parameters stop following Seqwencer
    while its ordinary Gate behaviour remains independent.
38. Click the Gate and PHI button bodies and confirm they change only the bottom
    panel. Click each internal LED and confirm it changes only the FX on/off state.
39. Click A and B repeatedly in Macro Mappings and confirm the PHI interface
    no longer flashes or rebuilds. Confirm unmapped Macro cells are blank and
    a clear divider separates Targets from Macro.
40. Load PHI VST3 in MuLab, leave HOST SYNC on, and start MuLab playback. Confirm
    Seqwencer follows MuLab's tempo and timeline rather than freezing. Turn HOST
    SYNC off and confirm it deliberately free-runs at the current tempo.
41. Load Seqwencer in another VST3 host and confirm the PHI selector is hidden.
42. Select every Waveform A entry and confirm it redraws all 32 A steps without
    changing B. Repeat with Waveform B and confirm A remains unchanged. Confirm
    Saw Down descends and each Double entry contains two complete cycles.
43. Repeat a waveform selection in Unipolar and Bipolar mode and confirm it uses
    the complete visible grid height in both cases.
44. Turn A Colour and B Colour through their ranges. Confirm their grids,
    Gate-mode cells, Attack/Release knobs, A/B and Bipolar buttons, and target
    lists follow independently. Confirm FX titles and lane borders retain the
    selected Gate or PHI colour.
45. Switch between Gate and PHI and confirm the bottom-row knobs change to the
    selected FX colour. Close and reopen Seqwencer and confirm both chosen lane
    colours return from `Data/Settings.ini`.
46. Confirm the top and bottom panels are the same height, the A/B colour knobs
    match the bottom-row knob size, each colour knob stops at both ends, and a
    double-click restores its original teal or orange default.
42. Open Plugin Diagnostics for Seqwencer and confirm **Seqwencer Bridge Packets
    Received** is greater than zero after the test. The processor section also
    reports whether MuLab supplied an outer playhead, position, tempo, PPQ,
    sample time, play state, and observable timeline motion.
43. Restrict Start/End to a clearly audible range. Confirm LOOP runs Start to
    End, BOUNCE runs Start to End to Start without repeating either endpoint,
    and REVERSE runs End to Start before restarting at End. Repeat in SERIAL
    across the A/B boundary and confirm PHI targets follow the same order.
44. Compare the A/B Colour knobs with the bottom-row knobs and confirm their
    rotary circles now have the same diameter.
45. Select Delay, switch its LED on, and confirm Time, Feedback and Mix produce
    a normal delay even before any target is assigned. Move Time while echoes
    are sounding and confirm it glides with tape-style pitch movement rather
    than producing stepped clicks or buzz.
46. Give Delay visibly different A/B patterns and timing settings from Gate and
    PHI. Switch among all three FX pages and confirm each recalls its own state
    while the enabled hidden engines continue running.
47. Drag Time, Feedback and Mix to Delay A, B and both target lists. Confirm
    their checkboxes, crosses, Parallel independence and SERIAL sharing behave
    like the existing Gate target lists.
48. Return to Gate and drag Threshold, Attack, Hold, Release and Range to its A
    and B lists. Confirm each assigned Noise Gate parameter follows the Gate
    pattern, while disabling Noise Gate stops its audio processing without
    deleting those assignments.
49. Save and reload a portable preset. Confirm its INI file has a Delay section
    and restores Delay patterns, timing, controls and target assignments.
50. Confirm every Gate and Delay parameter caption uses the same size, with
    VOLUME, DEPTH, ATTACK, RELEASE and RANGE written in full, THRESH and FDBK
    intentionally shortened, and Delay Time showing its complete value.

## Planned development

Further requested sequencer workflow work will continue in subsequent builds.
