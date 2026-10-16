#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod   pod;
Oscillator osc;
Svf        filt;
Adsr       env;

int           waveform = 0;
const uint8_t waves[]  = {Oscillator::WAVE_SIN,
                          Oscillator::WAVE_TRI,
                          Oscillator::WAVE_POLYBLEP_SAW,
                          Oscillator::WAVE_POLYBLEP_SQUARE};

// Per-waveform gain so all four sound roughly equally loud
// (saw and square carry far more harmonic energy than sine)
const float gains[] = {1.0f, 1.0f, 0.5f, 0.5f};

// Maximum output level (Pod headphone out is hot)
const float kMaxAmp = 0.25f;

// Smoothed knob values (avoid zipper noise)
float pitch_smooth = 0.f;
float vol_smooth   = 0.f;

// Encoder sets filter cutoff (0 = closed, 1 = fully open)
const float kCutoffStep   = 1.f / 48.f; // 48 clicks from closed to open
float       cutoff        = 1.f;
float       cutoff_smooth = 1.f;

// Encoder push toggles play mode:
//   drone = constant tone, gated = Button 2 plays notes through the envelope
bool gated = false;

// Shared with main loop for LED 2
float level = 0.f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    pod.ProcessAllControls();

    // Button 1 cycles the waveform
    if (pod.button1.RisingEdge())
    {
        waveform = (waveform + 1) % 4;
        osc.SetWaveform(waves[waveform]);
    }

    // Encoder push toggles drone / gated
    if (pod.encoder.RisingEdge())
        gated = !gated;

    // Encoder turn = filter cutoff
    cutoff += pod.encoder.Increment() * kCutoffStep;
    cutoff = fclamp(cutoff, 0.f, 1.f);
    fonepole(cutoff_smooth, cutoff, 0.05f);
    // Exponential map: 40 Hz (closed) to ~20 kHz (open)
    filt.SetFreq(40.f * powf(2.f, cutoff_smooth * 9.f));

    bool gate = pod.button2.Pressed();

    // Knob 1 = pitch (MIDI notes 36-96), Knob 2 = volume
    fonepole(pitch_smooth, pod.knob1.Value(), 0.05f);
    fonepole(vol_smooth, pod.knob2.Value(), 0.05f);

    // Squared volume curve feels more natural than linear
    float vol = vol_smooth * vol_smooth;
    osc.SetFreq(mtof(36.f + pitch_smooth * 60.f));
    osc.SetAmp(vol * kMaxAmp * gains[waveform]);

    for (size_t i = 0; i < size; i++)
    {
        float e = env.Process(gate);
        filt.Process(osc.Process());
        float s   = filt.Low() * (gated ? e : 1.f);
        level     = gated ? e : 1.f;
        out[0][i] = s;
        out[1][i] = s;
    }
}

int main(void)
{
    pod.Init();
    pod.SetAudioBlockSize(4);
    pod.seed.StartLog(false);

    osc.Init(pod.AudioSampleRate());
    osc.SetWaveform(waves[waveform]);

    filt.Init(pod.AudioSampleRate());
    filt.SetRes(0.2f);

    env.Init(pod.AudioSampleRate());
    env.SetAttackTime(0.01f);
    env.SetDecayTime(0.1f);
    env.SetSustainLevel(0.7f);
    env.SetReleaseTime(0.3f);

    pod.StartAdc();
    pod.StartAudio(AudioCallback);

    // LED 1 color shows the current waveform: red, green, blue, white
    const float colors[4][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}};
    int         last         = -1;
    int         last_mode    = -1;

    while (1)
    {
        pod.led1.Set(colors[waveform][0], colors[waveform][1], colors[waveform][2]);

        // LED 2: orange = drone, cyan = gated. Brightness follows the sound
        // and dims as the filter closes.
        float b = level * (0.15f + 0.85f * cutoff);
        if (gated)
            pod.led2.Set(0, b, b);
        else
            pod.led2.Set(b, b * 0.4f, 0);
        pod.UpdateLeds();

        if (gated != last_mode)
        {
            pod.seed.PrintLine(gated ? "mode: gated (Button 2 plays)" : "mode: drone");
            last_mode = gated;
        }

        if (waveform != last)
        {
            pod.seed.PrintLine("waveform %d", waveform);
            last = waveform;
        }
        // LEDs use software PWM, which expects UpdateLeds() at ~1 kHz
        System::Delay(1);
    }
}
