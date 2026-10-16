#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed  hw;
Oscillator osc;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float s   = osc.Process();
        out[0][i] = in[0][i] + s;
        out[1][i] = in[1][i] + s;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    osc.Init(hw.AudioSampleRate());
    osc.SetWaveform(Oscillator::WAVE_SIN);
    osc.SetFreq(220.f);
    osc.SetAmp(0.1f);
    hw.StartAudio(AudioCallback);
    while (1) {}
}
