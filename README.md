# SoundChopper

A VST3 / AU audio effect plug-in that rhythmically gates ("chops") any incoming audio signal in sync with your DAW's transport, or in response to incoming MIDI notes.

---

## Features

- **Fixed-Pattern mode** – The gate opens and closes at a selectable subdivision of the DAW tempo (1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T).
- **MIDI Input mode** – A specified MIDI note triggers the gate; a separate bypass note lets audio pass through unaffected.
- **Gate Length** – Controls what fraction of each chop cycle the gate stays open (1 – 100 %).
- **Fade Time** – Adds a linear fade-in and fade-out ramp at the edges of each gate opening to remove clicks (0 – 50 % of gate duration).
- **Dry/Wet** – Blends the chopped ("wet") signal with the untouched ("dry") signal.
- **Future: Filter** – A state-variable TPT filter (Low Pass / High Pass / Band Pass / Notch) can be switched on to process the chopped audio. UI controls are already present.
- **LED indicators** – Visual feedback shows which MIDI notes are held (MIDI, Gate, Bypass).
- **Logic IAC Driver workaround** – A built-in MIDI Source selector lets you open an external MIDI input device directly, so Logic users can route MIDI without an IAC Bus.

---

## Rhythm Subdivisions

| Display name | Cycle length (quarter notes) |
|---|---|
| 1/4  – Quarter note | 1 |
| 1/4T – Quarter-note triplet | 2/3 |
| 1/8  – Eighth note | 1/2 |
| 1/8T – Eighth-note triplet | 1/3 |
| 1/16 – Sixteenth note | 1/4 |
| 1/16T – Sixteenth-note triplet | 1/6 |
| 1/32 – Thirty-second note | 1/8 |
| 1/32T – Thirty-second-note triplet | 1/12 |

---

## Parameters

| Parameter | Range | Default | Description |
|---|---|---|---|
| Rhythm Source | Fixed Pattern / MIDI Input | Fixed Pattern | Selects how the gate is controlled |
| Rhythm Pattern | See table above | 1/8 | Chop rate (Fixed Pattern mode only) |
| Gate Length | 1 – 100 % | 50 % | Fraction of the chop cycle that the gate is open |
| Fade Time | 0 – 50 % | 10 % | Fraction of the gate duration used for each fade ramp |
| Dry/Wet | 0 – 100 % | 100 % | Mix between dry (unprocessed) and wet (chopped) signal |
| Filter On | on / off | off | Enables the optional filter (future feature) |
| Filter Type | Low Pass / High Pass / Band Pass / Notch | Low Pass | Filter mode (future feature) |
| Filter Freq | 20 – 20 000 Hz | 1 000 Hz | Filter cutoff frequency (future feature) |
| Filter Q | 0.1 – 10 | 0.707 | Filter resonance (future feature) |

### MIDI Input mode only

| Parameter | Range | Default | Description |
|---|---|---|---|
| MIDI Source | None / device list | None | Optional: open an external MIDI device directly (Logic IAC Driver workaround) |
| Gate Note | C-1 – G9 | C1 (36) | The MIDI note that triggers a gate opening |
| Bypass Note | C-1 – G9 | D1 (38) | A Note On latches audio at full volume until the next Gate Note On |

---

## How It Works

### Fixed-Pattern Mode

The plug-in reads the DAW playhead position (PPQ) and BPM each audio block, then computes a normalised phase within the selected chop cycle. The `calculateGate` function maps that phase to a gain value:

- **0** when the gate is closed (`phase ≥ gateLength`).
- **Linear ramp 0→1** during the fade-in (`phase < fadeDuration`).
- **1** while the gate is fully open.
- **Linear ramp 1→0** during the fade-out (`phase ≥ gateLength − fadeDuration`).

The Dry/Wet parameter blends the chopped signal back with the original signal:

```
totalGain = (1 − dryWet) + dryWet × wetGain
```

Audio passes through unchanged while the DAW transport is stopped.

### MIDI Input Mode

The gate state machine has four states: **Closed → FadingIn → Open → FadingOut → Closed**.

- A **Gate Note On** event (when the gate is Closed) starts the FadingIn state. The gate then transitions automatically through Open → FadingOut → Closed using the same Gate Length and Fade Time values.
- A **Bypass Note On** event immediately routes audio through at full gain and latches it there; the latch is cleared the next time a Gate Note On is received.
- Note-Off events for the gate note are ignored; the gate envelope always runs to completion.

---

## Building from Source

### Prerequisites

- **CMake** ≥ 3.22
- A C++17-capable compiler (Clang, GCC, MSVC)
- On Linux: `libx11-dev` and `libasound2-dev`

```bash
sudo apt install libx11-dev libasound2-dev   # Linux only
```

JUCE is fetched automatically via CPM on the first configure step (requires internet access).

### Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the VST3 target
cmake --build build --target SoundChopper_VST3 -- -j4

# Build all formats (VST3, AU, Standalone)
cmake --build build -- -j4
```

The built plug-in is copied automatically to your system plug-in folder when `COPY_PLUGIN_AFTER_BUILD` is enabled in CMakeLists.txt.

### Debug build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target SoundChopper_VST3 -- -j4
```

---

## Using the Plug-in in a DAW

### Fixed-Pattern Mode

1. Insert SoundChopper on any audio track.
2. Start the DAW transport.
3. Select the desired **Rhythm Pattern** (e.g. `1/8 – Eighth note`).
4. Adjust **Gate Length** to set how much of each cycle is audible.
5. Use **Fade Time** to smooth the transitions and avoid clicks.
6. Use **Dry/Wet** to blend in the original, unprocessed signal.

### MIDI Input Mode

1. Set **Rhythm Source** to `MIDI Input`.
2. Route a MIDI track to the plug-in's MIDI input (most DAWs support this natively).
   - **Logic Pro users**: Either use the built-in **MIDI Source** selector to pick an IAC Driver Bus, or set up an IAC Bus and route your MIDI track through it.
3. Assign the desired **Gate Note** (default: C1 / kick drum) and **Bypass Note** (default: D1 / snare drum).
4. Play or sequence MIDI notes: the gate note triggers one chop, and the bypass note latches audio through at full gain until the next gate note arrives.

---

## Project Structure

```
sound-chopper/
├── CMakeLists.txt          # Build configuration (JUCE via CPM)
├── cmake/
│   └── CPM.cmake           # CMake Package Manager helper
├── src/
│   ├── PluginProcessor.h   # Processor class, parameter IDs, MIDI gate state
│   ├── PluginProcessor.cpp # Audio processing, parameter layout, rhythm tables
│   ├── PluginEditor.h      # Editor class declaration
│   └── PluginEditor.cpp    # Editor UI layout and controls
└── libs/
    └── juce/               # JUCE source (fetched by CPM on first build)
```

---

## License

© ErwinDB. All rights reserved.
