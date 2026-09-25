#pragma once
/*  THIN WALLS - recording a take and exporting it as an MP4.

    A TAKE is what the plug-in heard and everything that moved while it heard it:
    the input audio (after the test signal replaced MAIN), every host parameter on
    a fixed 256-sample grid, furniture layouts as they changed, and the page's own
    camera (pitch, field of view). The export replays it through a FRESH engine,
    offline and at full quality, while the page redraws each video frame from the
    same recorded values - so picture and sound come from one timeline and cannot
    drift apart. Frames arrive from the page as JPEGs; Media Foundation (built
    into Windows, no extra libraries) encodes H.264 video and AAC audio into an
    MP4.
*/

#include <JuceHeader.h>
#include "Engine.h"
#include <atomic>
#include <memory>
#include <vector>

namespace tw
{

struct TakeData
{
    static constexpr int PBLOCK = 256;          // the parameter grid, samples

    double rate = 48000.0;
    int capacity = 0;                           // samples
    bool aux = false;                           // the AUX pair was captured too
    juce::AudioBuffer<float> input;             // 2 (main) or 4 (main + aux) channels
    std::atomic<int> length { 0 };              // samples written so far

    int np = 0;                                 // host parameters per grid point
    std::vector<float> params;                  // np raw values per grid point
    std::vector<float> beat;                    // 3 per grid point: ppq, bpm, playing (the host's clock)
    std::atomic<int> nblocks { 0 };

    struct FurnSnap { int sample = 0; int n = 0; FurnItem items[MAX_FURN]; float panelArea[NUM_ROOMS] = { 0, 0, 0 }; };
    std::vector<FurnSnap> furn;                 // preallocated; written by the audio thread
    std::atomic<int> nfurn { 0 };

    struct Cam { float t = 0, pitch = 0, fov = 0; };
    std::vector<Cam> cam;                       // message thread only

    double seconds() const { return rate > 0 ? length.load() / rate : 0.0; }
};

class Mp4Writer
{
public:
    Mp4Writer();
    ~Mp4Writer();
    bool open (const juce::File& f, int w, int h, int fps, int videoKbps, int audioRate, juce::String& err);
    bool writeFrame (const juce::Image& img, juce::String& err);          // exactly w x h
    bool writeAudio (const int16_t* interleavedStereo, int frames, juce::String& err);
    bool finish (juce::String& err);
    void abandon();
    bool isOpen() const;
    double videoSeconds() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace tw
