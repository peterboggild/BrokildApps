#include "Engine.h"

namespace ps
{

static inline double clampd (double x, double a, double b) { return x < a ? a : (x > b ? b : x); }

//==============================================================================
void Biquad::set (Type t, double freq, double Q, double gainDb, double fs)
{
    type = t;
    // Clamp away from 0 Hz: a biquad at f = 0 degenerates and the signal can
    // vanish. Swept all-pass stages (phasers) hit this at high LFO depth.
    const double f = clampd (freq, 10.0, fs * 0.49);
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    const double cw = std::cos (w), sw = std::sin (w);
    double b0n = 1, b1n = 0, b2n = 0, a0 = 1, a1n = 0, a2n = 0;

    switch (t)
    {
        case lowpass:
        case highpass:
        {
            const double q = std::pow (10.0, Q / 20.0);      // Q is in dB here
            const double alpha = sw / (2.0 * juce::jmax (1e-9, q));
            if (t == lowpass) { b0n = (1 - cw) / 2; b1n = 1 - cw;    b2n = b0n; }
            else              { b0n = (1 + cw) / 2; b1n = -(1 + cw); b2n = b0n; }
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case bandpass:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = alpha; b1n = 0; b2n = -alpha;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case notch:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1; b1n = -2 * cw; b2n = 1;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case allpass:
        {
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1 - alpha; b1n = -2 * cw; b2n = 1 + alpha;
            a0 = 1 + alpha; a1n = -2 * cw; a2n = 1 - alpha;
            break;
        }
        case peaking:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double alpha = sw / (2.0 * juce::jmax (1e-9, Q));
            b0n = 1 + alpha * A; b1n = -2 * cw; b2n = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1n = -2 * cw; a2n = 1 - alpha / A;
            break;
        }
    }
    b0 = b0n / a0; b1 = b1n / a0; b2 = b2n / a0;
    a1 = a1n / a0; a2 = a2n / a0;
}

//==============================================================================
Engine::Engine()
{
    cmdStorage.resize (65536);
}

void Engine::prepare (double sampleRate, int maxBlock)
{
    fsHost = sampleRate;
    maxBlockSize = juce::jmax (32, maxBlock);
    currentFrame = 0;

    if (! everConfigured.load())
    {
        for (auto& p : params) p.init (0.0f);
        params[pMaster].init (1.0f);
        // … initialise your DSP defaults here …

        for (int i = 0; i < numParams; ++i)
            shadowParams[(size_t) i] = params[(size_t) i].current;
        everConfigured = true;
    }
    else
    {
        // Re-prepare (sample-rate change): restore from the mirrors instead of
        // resetting, or the user's patch silently reverts to defaults.
        for (int i = 0; i < numParams; ++i)
            params[(size_t) i].init (shadowParams[(size_t) i].load());
    }

    // Allocate every buffer, delay line and voice here — never in process().
}

//==============================================================================
bool Engine::pushCommand (const Command& c)
{
    // Message thread only. A preset apply can burst thousands of commands;
    // wait briefly for room rather than dropping state on the floor.
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        int s1, n1, s2, n2;
        cmdFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 + n2 >= 1)
        {
            cmdStorage[(size_t) s1] = c;
            cmdFifo.finishedWrite (1);
            return true;
        }
        juce::Thread::sleep (1);
    }
    return false;
}

void Engine::drainCommands()
{
    int s1, n1, s2, n2;
    cmdFifo.prepareToRead (cmdFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) applyCommand (cmdStorage[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) applyCommand (cmdStorage[(size_t) (s2 + i)]);
    cmdFifo.finishedRead (n1 + n2);
}

void Engine::applyCommand (const Command& c)
{
    const double now = (double) currentFrame.load() / fsHost;
    switch (c.type)
    {
        case Command::setParamValue:
            if (c.d1 >= 0 && c.d1 > now)
                params[(size_t) c.param].push ({ c.d1, c.f1, 0, false });
            else
            {
                auto& p = params[(size_t) c.param];
                p.current = p.target = c.f1; p.tau = 0;
            }
            break;

        case Command::setParamTarget:
            if (c.d1 >= 0 && c.d1 > now)
                params[(size_t) c.param].push ({ c.d1, c.f1, c.f2, true });
            else
            {
                auto& p = params[(size_t) c.param];
                p.target = c.f1; p.tau = c.f2;
            }
            break;

        case Command::cancelParam:
            params[(size_t) c.param].cancel();
            break;

        case Command::setVoiceField:
            // voices[c.i1].setField (c.i2, c.f1);
            break;

        case Command::voiceEvents:
            if (c.ptr != nullptr)
            {
                // voices[c.ptr->voice].addEvents (c.ptr->events);
                c.ptr->consumed = true;      // the message thread frees it
            }
            break;

        case Command::voiceClear:
        case Command::voiceReset:
        case Command::setMeta:
            break;
    }
}

//==============================================================================
void Engine::process (float* L, float* R, int n)
{
    drainCommands();

    // Short sub-blocks keep parameter smoothing and scheduled events close to
    // their true time without per-sample overhead.
    int done = 0;
    while (done < n)
    {
        const int m = juce::jmin (32, n - done);
        processSub (L + done, R + done, m);
        done += m;
        currentFrame += m;
    }
}

void Engine::processSub (float* L, float* R, int n)
{
    const double now = (double) currentFrame.load() / fsHost;
    for (auto& p : params) p.tick (now, fsHost, n);

    for (int i = 0; i < n; ++i) { L[i] = 0; R[i] = 0; }

    // … render voices into L/R, then run the effect chain …

    const float master = params[pMaster].current;
    for (int i = 0; i < n; ++i) { L[i] *= master; R[i] *= master; }
}

//==============================================================================
void Engine::hostNoteOn (int, int)   {}
void Engine::hostNoteOff (int, int)  {}
void Engine::hostAllNotesOff()       {}

} // namespace ps
