#pragma once

/*  BRAIN SCAN — importing a real volume.

    Everything in here is plain C++ with no JUCE, so the bench can measure it:
    the format readers take a byte buffer, and the resampler takes a source
    volume and hands back the VN^3 cube the engine reads. The JUCE side does
    only what it must — file chooser, directory scan, gunzip, PNG decode.

    THREE THINGS DECIDE WHETHER AN IMPORT IS ANY GOOD, and none of them is the
    file format.

    1.  ANISOTROPY. A CT is typically 0.4-0.6 mm in plane and 1-5 mm between
        slices. Resampling by INDEX rather than by physical extent squashes the
        body along the slice axis by whatever that ratio happens to be. Every
        reader therefore reports spacing, and the resampler works in
        millimetres, not indices.

    2.  DECIMATION. 512x512x300 into 64^3 is roughly 8x8x5 source voxels per
        destination voxel. Point-sampling that is aliasing, and it would
        undermine the mip pyramid at the source, where no later filtering can
        rescue it. So each axis is resampled with a tent whose width follows
        that axis's own ratio — which is also why it has to be per axis.

    3.  OUTLIERS. One metal clip at 3000 HU will crush every brain voxel into
        the bottom two per cent of the range if the window is taken from the
        extremes. The default window is a PERCENTILE of the data, and the
        values it chose are reported so they can be read in Hounsfield units.

    A fourth thing decides whether it is HONEST: a reader must either be right
    or refuse out loud. Anything this cannot read — a compressed DICOM transfer
    syntax, a colour image, NIfTI-2 — says what it is and what to do about it,
    rather than producing a volume that is quietly wrong.
*/

#include <cstdint>
#include <string>
#include <vector>

namespace bs
{

//==============================================================================
/*  A source volume as it came off disk: values in their native units (Hounsfield
    units for CT), the size in voxels, the spacing in millimetres, and a
    direction matrix whose column j is the patient-space direction of axis j. */
struct SrcVolume
{
    std::vector<float> v;
    int   n[3] { 0, 0, 0 };
    float spacing[3] { 1.0f, 1.0f, 1.0f };
    float dir[9] { 1,0,0, 0,1,0, 0,0,1 };
    std::string note;                       // what was read, for the panel

    size_t count() const { return (size_t) n[0] * (size_t) n[1] * (size_t) n[2]; }
    bool   valid() const { return n[0] > 1 && n[1] > 1 && n[2] > 1 && v.size() == count(); }
    float  at (int i, int j, int k) const { return v[((size_t) k * n[1] + (size_t) j) * n[0] + (size_t) i]; }
};

//==============================================================================
struct ImportOpts
{
    int   phaseAxis = 0;        // which SOURCE axis becomes x, the phase axis
    bool  stretch = false;      // fill the cube, or keep the body's proportions
    bool  reorient = true;      // put the axes in a canonical order from `dir`
    float loPct = 0.005f;       // the window, as percentiles of the data
    float hiPct = 0.995f;
    float loValue = 0.0f;       // ... or an explicit window, if useValues
    float hiValue = 0.0f;
    bool  useValues = false;
};

struct ImportReport
{
    int   srcN[3] { 0, 0, 0 };
    float srcSpacing[3] { 0, 0, 0 };
    float extentMm[3] { 0, 0, 0 };
    int   filled[3] { 0, 0, 0 };            // voxels of the cube the body occupies
    float loValue = 0, hiValue = 0;         // the window actually used, native units
    float airFraction = 0;                  // how much of the cube is padding
    int   axisOrder[3] { 0, 1, 2 };         // source axis -> cube axis
    bool  flipped[3] { false, false, false };
    std::string note;
};

//==============================================================================
/*  Readers. Each returns false and fills `err` with something a person can act
    on. None of them ever returns a volume it is not sure about. */
bool readNifti (const uint8_t* data, size_t bytes, SrcVolume& out, std::string& err);

/*  One DICOM file. Only the uncompressed transfer syntaxes — implicit VR LE,
    explicit VR LE and explicit VR BE — which is most of what an archive holds;
    anything encapsulated says so and names dcm2niix. */
struct DicomSlice
{
    int   rows = 0, cols = 0;
    float pixelSpacing[2] { 1.0f, 1.0f };
    float thickness = 0.0f;
    float pos[3] { 0, 0, 0 };
    float orient[6] { 1,0,0, 0,1,0 };
    float slope = 1.0f, intercept = 0.0f;
    int   instance = 0;
    std::string seriesUid;
    std::vector<float> pix;                 // rows*cols, already in native units
};
bool readDicomFile (const uint8_t* data, size_t bytes, DicomSlice& out, std::string& err);

/*  Assemble a series. Slices are ordered by their position along the slice
    NORMAL, never by filename — a directory listing is not slice order, and
    getting that wrong mirrors or shuffles the body without any other symptom. */
bool assembleDicom (std::vector<DicomSlice>& slices, SrcVolume& out, std::string& err);

/*  A stack of decoded greyscale images, all the same size, bottom slice first.
    No physical spacing exists, so the caller supplies it (1,1,1 by default). */
bool assembleStack (const std::vector<std::vector<float>>& slices, int w, int h,
                    const float spacing[3], SrcVolume& out, std::string& err);

//==============================================================================
/*  The resampler: source volume in, VN^3 floats in [0,1] out. `dstN` is the
    cube's side. This is where anisotropy, decimation and the window are dealt
    with; the bench measures each of them separately. */
bool resampleToCube (const SrcVolume& src, int dstN, const ImportOpts& opts,
                     float* out, ImportReport& rep, std::string& err);

/*  The window the default percentiles would choose — exposed so the panel can
    show it before anything is committed. */
void percentileWindow (const std::vector<float>& v, float loPct, float hiPct,
                       float& lo, float& hi);

} // namespace bs
