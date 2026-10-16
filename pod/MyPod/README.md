# MyPod

## Author

Nick Zappia

## Description

A small mono synth for the Daisy Pod: one oscillator with four waveforms,
followed by a low-pass filter and an envelope. It either drones continuously
or plays notes when you press Button 2.

## Controls

| Control      | Function                                                        |
|--------------|-----------------------------------------------------------------|
| Knob 1       | Pitch: MIDI notes 36–96 (about C2 / 65 Hz up to C7 / 2 kHz)       |
| Knob 2       | Volume: silent to max, on a squared (audio-style) curve         |
| Button 1     | Next waveform: sine → triangle → saw → square → back to sine     |
| Button 2     | In gated mode, plays a note while held                          |
| Encoder turn | Filter cutoff: left = darker, right = brighter (48 clicks end to end; starts fully open) |
| Encoder push | Switch between **drone** (always on, the default) and **gated** (Button 2 plays notes) |
| LED 1        | Current waveform: red = sine, green = triangle, blue = saw, white = square |
| LED 2        | Mode and level: orange = drone, cyan = gated. Lights while sound plays and dims as the filter closes |
| Audio out    | Same signal on left and right                                   |
| USB serial   | Prints `waveform N` and `mode: ...` when they change            |

## How it works

### Audio callback (`AudioCallback`)
Runs every 4 samples (`SetAudioBlockSize(4)`), at 48 kHz by default.

1. `ProcessAllControls()` reads the knobs, buttons and encoder.
2. A Button 1 press (`RisingEdge`) moves to the next waveform. An encoder
   push switches between drone and gated mode.
3. Each encoder click moves `cutoff` (0–1) by 1/48. After smoothing, it maps
   exponentially to 40 Hz to ~20 kHz (`40 × 2^(cutoff × 9)`), so every
   click sounds like the same amount of change.
4. The knob values are smoothed with `fonepole` (a one-pole low-pass filter)
   to remove zipper noise and jumps.
5. Pitch: the knob's 0–1 value maps to MIDI notes 36–96, and `mtof`
   converts the note to a frequency in Hz.
6. Volume: `knob² × kMaxAmp × gains[waveform]`.
7. For each sample: oscillator → `Svf` low-pass filter (resonance 0.2). In
   gated mode the result is multiplied by the ADSR envelope, with Button 2
   as the gate. The sample goes to both output channels.

### Envelope (gated mode)
| Stage   | Setting |
|---------|---------|
| Attack  | 10 ms   |
| Decay   | 100 ms  |
| Sustain | 70%     |
| Release | 300 ms  |

Change these in `main()` (`env.Set...`).

### Main loop (`main`)
Sets up the hardware, oscillator, filter, envelope, ADC and audio. Then every
10 ms it updates LEDs 1 and 2 and prints a log line when the waveform or mode
changes.

## Fix applied: loud, muddy triangle, saw and square

**Symptom:** each waveform after sine sounded louder and muddier than the
last, and some were extremely loud.

**Causes:**
1. **Aliasing.** DaisySP's plain `WAVE_SAW` and `WAVE_SQUARE` are naive
   (not band-limited) waveforms. Their instant jumps make harmonics above
   the Nyquist limit (half the sample rate). Those fold back down as harsh,
   off-pitch noise, and it gets worse the higher you play.
2. **No loudness matching.** Every waveform got the same volume, but saw and
   square carry much more harmonic energy than a sine, so they sound much
   louder at the same setting.
3. **High maximum level.** Max volume was 0.5, which is very loud through
   the Pod's headphone output.

**Changes:**
- Saw and square now use `WAVE_POLYBLEP_SAW` / `WAVE_POLYBLEP_SQUARE`, which
  are band-limited and sound much cleaner. Triangle stays naive because its
  aliasing is mild.
- A `gains[]` table balances the waveforms (sine 1.0, tri 1.0, saw 0.5,
  square 0.5).
- `kMaxAmp` dropped from 0.5 to 0.25, and volume uses a squared curve.
- Both knobs are smoothed with `fonepole`.

**To tune further:** if one waveform still sounds too loud or quiet, change
its entry in `gains[]`. For more or less overall headroom, change `kMaxAmp`.

## Build and flash

```sh
make                 # build to build/MyPod.bin
make program-dfu     # flash over USB (board must be in DFU mode)
```

**DFU mode:** on the Daisy Seed module, hold **BOOT**, tap **RESET**, then
let go of BOOT. `lsusb | grep 0483` should show `df11`.

Ignore these messages from `dfu-util`:
- `Invalid DFU suffix signature`: normal for Daisy builds.
- `Error during download get_status` at the end: the board reset right
  after a successful flash.
