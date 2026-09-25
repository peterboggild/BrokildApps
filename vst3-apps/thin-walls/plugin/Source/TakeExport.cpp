#include "TakeExport.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <mfapi.h>
 #include <mfidl.h>
 #include <mfreadwrite.h>
 #include <mferror.h>
 #include <codecapi.h>
#endif

namespace tw
{

#if JUCE_WINDOWS
namespace
{
    template <typename T> void release (T*& p) { if (p != nullptr) { p->Release(); p = nullptr; } }
    juce::String hrText (const char* what, HRESULT hr)
    {
        return juce::String (what) + " failed (0x" + juce::String::toHexString ((juce::int64) (uint32_t) hr) + ")";
    }
}

struct Mp4Writer::Impl
{
    IMFSinkWriter* w = nullptr;
    DWORD vs = 0, as = 0;
    int W = 0, H = 0, fps = 30, arate = 48000;
    juce::int64 frames = 0, aSamples = 0;
    bool mfStarted = false, writing = false;
    juce::File file;

    bool build (bool hardware, int kbps, juce::String& err)
    {
        release (w);
        IMFAttributes* attr = nullptr;
        HRESULT hr = MFCreateAttributes (&attr, 3);
        if (FAILED (hr)) { err = hrText ("MFCreateAttributes", hr); return false; }
        attr->SetUINT32 (MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, hardware ? TRUE : FALSE);
        attr->SetUINT32 (MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
        attr->SetGUID (MF_TRANSCODE_CONTAINERTYPE, MFTranscodeContainerType_MPEG4);
        hr = MFCreateSinkWriterFromURL (file.getFullPathName().toWideCharPointer(), nullptr, attr, &w);
        release (attr);
        if (FAILED (hr)) { err = hrText ("creating the MP4 file", hr); return false; }

        IMFMediaType* t = nullptr;
        // ---- video: H.264 out, RGB32 in (the sink writer converts)
        MFCreateMediaType (&t);
        t->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        t->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_H264);
        t->SetUINT32 (MF_MT_AVG_BITRATE, (UINT32) kbps * 1000u);
        t->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        t->SetUINT32 (MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
        MFSetAttributeSize (t, MF_MT_FRAME_SIZE, (UINT32) W, (UINT32) H);
        MFSetAttributeRatio (t, MF_MT_FRAME_RATE, (UINT32) fps, 1);
        MFSetAttributeRatio (t, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = w->AddStream (t, &vs);
        release (t);
        if (FAILED (hr)) { err = hrText ("adding the H.264 stream", hr); return false; }

        MFCreateMediaType (&t);
        t->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
        t->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        t->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        t->SetUINT32 (MF_MT_DEFAULT_STRIDE, (UINT32) (W * 4));      // positive: top-down rows
        MFSetAttributeSize (t, MF_MT_FRAME_SIZE, (UINT32) W, (UINT32) H);
        MFSetAttributeRatio (t, MF_MT_FRAME_RATE, (UINT32) fps, 1);
        MFSetAttributeRatio (t, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = w->SetInputMediaType (vs, t, nullptr);
        release (t);
        if (FAILED (hr)) { err = hrText ("the video encoder's input", hr); return false; }

        // ---- audio: AAC 192 kbit/s out, 16-bit stereo PCM in
        MFCreateMediaType (&t);
        t->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        t->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
        t->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        t->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) arate);
        t->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        t->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000);
        hr = w->AddStream (t, &as);
        release (t);
        if (FAILED (hr)) { err = hrText ("adding the AAC stream", hr); return false; }

        MFCreateMediaType (&t);
        t->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        t->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
        t->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        t->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) arate);
        t->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        t->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
        t->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32) arate * 4u);
        hr = w->SetInputMediaType (as, t, nullptr);
        release (t);
        if (FAILED (hr)) { err = hrText ("the audio encoder's input", hr); return false; }

        hr = w->BeginWriting();
        if (FAILED (hr)) { err = hrText ("starting the MP4", hr); return false; }
        return true;
    }
};

Mp4Writer::Mp4Writer() : impl (std::make_unique<Impl>()) {}
Mp4Writer::~Mp4Writer() { abandon(); }

bool Mp4Writer::isOpen() const { return impl->writing; }
double Mp4Writer::videoSeconds() const { return impl->fps > 0 ? (double) impl->frames / impl->fps : 0.0; }

bool Mp4Writer::open (const juce::File& f, int w, int h, int fps, int kbps, int audioRate, juce::String& err)
{
    abandon();
    auto& m = *impl;
    m.W = w & ~1; m.H = h & ~1; m.fps = juce::jlimit (1, 120, fps); m.arate = audioRate;
    m.frames = 0; m.aSamples = 0; m.file = f;
    HRESULT hr = MFStartup (MF_VERSION, MFSTARTUP_LITE);
    if (FAILED (hr)) { err = hrText ("starting Media Foundation", hr); return false; }
    m.mfStarted = true;
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    // hardware encoder first (NVENC and the like), the software one if that refuses
    juce::String e1;
    if (! m.build (true, kbps, e1))
    {
        f.deleteFile();
        if (! m.build (false, kbps, err)) { err = e1 + "; software: " + err; abandon(); return false; }
    }
    m.writing = true;
    return true;
}

bool Mp4Writer::writeFrame (const juce::Image& src, juce::String& err)
{
    auto& m = *impl;
    if (! m.writing) { err = "no MP4 open"; return false; }
    juce::Image img = src;
    if (img.getWidth() != m.W || img.getHeight() != m.H) img = img.rescaled (m.W, m.H, juce::Graphics::highResamplingQuality);
    if (img.getFormat() != juce::Image::ARGB) img = img.convertedToFormat (juce::Image::ARGB);

    const DWORD bytes = (DWORD) (m.W * m.H * 4);
    IMFMediaBuffer* buf = nullptr;
    HRESULT hr = MFCreateMemoryBuffer (bytes, &buf);
    if (FAILED (hr)) { err = hrText ("a frame buffer", hr); return false; }
    BYTE* d = nullptr;
    buf->Lock (&d, nullptr, nullptr);
    {
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < m.H; ++y)
        {
            // JUCE's ARGB is B, G, R, A in memory on Windows: exactly RGB32's B, G, R, X
            const juce::uint8* row = bd.getLinePointer (y);
            if (bd.pixelStride == 4) std::memcpy (d + (size_t) y * (size_t) m.W * 4, row, (size_t) m.W * 4);
            else
                for (int x = 0; x < m.W; ++x)
                {
                    const juce::Colour c = bd.getPixelColour (x, y);
                    BYTE* p = d + ((size_t) y * (size_t) m.W + (size_t) x) * 4;
                    p[0] = c.getBlue(); p[1] = c.getGreen(); p[2] = c.getRed(); p[3] = 255;
                }
        }
    }
    buf->Unlock();
    buf->SetCurrentLength (bytes);
    IMFSample* s = nullptr;
    MFCreateSample (&s);
    s->AddBuffer (buf);
    const LONGLONG t0 = (LONGLONG) (m.frames * 10000000LL / m.fps);
    const LONGLONG t1 = (LONGLONG) ((m.frames + 1) * 10000000LL / m.fps);
    s->SetSampleTime (t0);
    s->SetSampleDuration (t1 - t0);
    hr = m.w->WriteSample (m.vs, s);
    release (s); release (buf);
    if (FAILED (hr)) { err = hrText ("writing a video frame", hr); return false; }
    ++m.frames;
    return true;
}

bool Mp4Writer::writeAudio (const int16_t* pcm, int frames, juce::String& err)
{
    auto& m = *impl;
    if (! m.writing) { err = "no MP4 open"; return false; }
    if (frames <= 0) return true;
    const DWORD bytes = (DWORD) frames * 4u;
    IMFMediaBuffer* buf = nullptr;
    HRESULT hr = MFCreateMemoryBuffer (bytes, &buf);
    if (FAILED (hr)) { err = hrText ("an audio buffer", hr); return false; }
    BYTE* d = nullptr;
    buf->Lock (&d, nullptr, nullptr);
    std::memcpy (d, pcm, bytes);
    buf->Unlock();
    buf->SetCurrentLength (bytes);
    IMFSample* s = nullptr;
    MFCreateSample (&s);
    s->AddBuffer (buf);
    const LONGLONG t0 = (LONGLONG) (m.aSamples * 10000000LL / m.arate);
    const LONGLONG t1 = (LONGLONG) ((m.aSamples + frames) * 10000000LL / m.arate);
    s->SetSampleTime (t0);
    s->SetSampleDuration (t1 - t0);
    hr = m.w->WriteSample (m.as, s);
    release (s); release (buf);
    if (FAILED (hr)) { err = hrText ("writing audio", hr); return false; }
    m.aSamples += frames;
    return true;
}

bool Mp4Writer::finish (juce::String& err)
{
    auto& m = *impl;
    if (! m.writing) { err = "no MP4 open"; return false; }
    const HRESULT hr = m.w->Finalize();
    release (m.w);
    m.writing = false;
    if (m.mfStarted) { MFShutdown(); m.mfStarted = false; }
    if (FAILED (hr)) { err = hrText ("finishing the MP4", hr); m.file.deleteFile(); return false; }
    return true;
}

void Mp4Writer::abandon()
{
    auto& m = *impl;
    const bool had = m.writing || m.w != nullptr;
    release (m.w);
    m.writing = false;
    if (m.mfStarted) { MFShutdown(); m.mfStarted = false; }
    if (had && m.file.existsAsFile()) m.file.deleteFile();
}

#else   // no Media Foundation: the export says so rather than pretending

struct Mp4Writer::Impl { };
Mp4Writer::Mp4Writer() : impl (std::make_unique<Impl>()) {}
Mp4Writer::~Mp4Writer() {}
bool Mp4Writer::isOpen() const { return false; }
double Mp4Writer::videoSeconds() const { return 0; }
bool Mp4Writer::open (const juce::File&, int, int, int, int, int, juce::String& err) { err = "MP4 export needs Windows"; return false; }
bool Mp4Writer::writeFrame (const juce::Image&, juce::String& err) { err = "MP4 export needs Windows"; return false; }
bool Mp4Writer::writeAudio (const int16_t*, int, juce::String& err) { err = "MP4 export needs Windows"; return false; }
bool Mp4Writer::finish (juce::String& err) { err = "MP4 export needs Windows"; return false; }
void Mp4Writer::abandon() {}

#endif

} // namespace tw
