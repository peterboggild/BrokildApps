#include "Import.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace bs
{

/*  A source above this is refused rather than allocated: 300 million voxels is
    1.2 GB of float, and past that a machine starts swapping instead of
    importing. */
static const size_t MAXVOX = 300u * 1000u * 1000u;

//==============================================================================
namespace
{
    inline uint16_t rd16 (const uint8_t* p, bool big)
    { return big ? (uint16_t) ((p[0] << 8) | p[1]) : (uint16_t) ((p[1] << 8) | p[0]); }

    inline uint32_t rd32 (const uint8_t* p, bool big)
    {
        return big ? ((uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3])
                   : ((uint32_t) p[3] << 24 | (uint32_t) p[2] << 16 | (uint32_t) p[1] << 8 | p[0]);
    }

    inline float rdF32 (const uint8_t* p, bool big)
    { const uint32_t u = rd32 (p, big); float f; std::memcpy (&f, &u, 4); return f; }

    inline double rdF64 (const uint8_t* p, bool big)
    {
        uint8_t b[8];
        for (int i = 0; i < 8; ++i) b[i] = big ? p[7 - i] : p[i];
        double d; std::memcpy (&d, b, 8); return d;
    }

    //  a decimal string, as DICOM stores numbers; "1.25\\1.25" is two of them
    void parseDs (const uint8_t* p, size_t n, float* out, int want)
    {
        std::string s ((const char*) p, n);
        int got = 0;
        size_t i = 0;
        while (i < s.size() && got < want)
        {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\\')) ++i;
            size_t j = i;
            while (j < s.size() && s[j] != '\\') ++j;
            if (j > i)
            {
                try { out[got++] = std::stof (s.substr (i, j - i)); }
                catch (...) { out[got++] = 0.0f; }
            }
            i = j + 1;
        }
    }

    std::string trimmed (const uint8_t* p, size_t n)
    {
        std::string s ((const char*) p, n);
        while (!s.empty() && (s.back() == ' ' || s.back() == 0)) s.pop_back();
        return s;
    }
}

//==============================================================================
//  NIfTI-1.  Single file (.nii), magic "n+1" at 344.
//==============================================================================
bool readNifti (const uint8_t* d, size_t bytes, SrcVolume& out, std::string& err)
{
    if (bytes < 352) { err = "too short to be a NIfTI file."; return false; }

    const int32_t szA = (int32_t) rd32 (d, false);
    const int32_t szB = (int32_t) rd32 (d, true);
    bool big;
    if (szA == 348)      big = false;
    else if (szB == 348) big = true;
    else
    {
        if (szA == 540 || szB == 540)
        { err = "this is NIfTI-2, which is not read here. Convert it with "
                "'dcm2niix' or save it as NIfTI-1."; return false; }
        err = "not a NIfTI-1 file (its header does not begin with 348)."; return false;
    }

    const std::string magic ((const char*) d + 344, 3);
    if (magic == "ni1")
    { err = "this is a NIfTI .hdr paired with a separate .img. Open the single-file "
            "form (.nii or .nii.gz) instead."; return false; }
    if (magic != "n+1") { err = "not a NIfTI-1 file (bad magic)."; return false; }

    const int ndim = (int) (int16_t) rd16 (d + 40, big);
    int n[3] { 1, 1, 1 };
    for (int i = 0; i < 3; ++i)
        n[i] = (ndim > i) ? (int) (int16_t) rd16 (d + 42 + 2 * i, big) : 1;
    if (n[0] < 2 || n[1] < 2 || n[2] < 2)
    { err = "this volume is flat: " + std::to_string (n[0]) + " x " + std::to_string (n[1])
          + " x " + std::to_string (n[2]) + ". A specimen needs three real dimensions."; return false; }

    const size_t count = (size_t) n[0] * (size_t) n[1] * (size_t) n[2];
    if (count > MAXVOX) { err = "this volume has " + std::to_string (count / 1000000)
        + " million voxels, more than can be held at once."; return false; }

    float pix[3];
    for (int i = 0; i < 3; ++i) pix[i] = rdF32 (d + 80 + 4 * i, big);
    for (int i = 0; i < 3; ++i) if (!(pix[i] > 0.0f) || !std::isfinite (pix[i])) pix[i] = 1.0f;

    const int   dtype = (int) (int16_t) rd16 (d + 70, big);
    const float voxOff = rdF32 (d + 108, big);
    float slope = rdF32 (d + 112, big);
    float inter = rdF32 (d + 116, big);
    if (!std::isfinite (slope) || slope == 0.0f) { slope = 1.0f; inter = 0.0f; }
    if (!std::isfinite (inter)) inter = 0.0f;

    size_t off = (size_t) (voxOff > 352.0f ? voxOff : 352.0f);
    int bytesPer = 0;
    switch (dtype)
    {
        case 2: case 256: bytesPer = 1; break;      // uint8, int8
        case 4: case 512: bytesPer = 2; break;      // int16, uint16
        case 8: case 768: case 16: bytesPer = 4; break; // int32, uint32, float32
        case 64: bytesPer = 8; break;               // float64
        default:
            err = "NIfTI datatype " + std::to_string (dtype) + " is not read here "
                  "(colour and complex volumes have no single density to sound).";
            return false;
    }
    if (off + count * (size_t) bytesPer > bytes)
    { err = "the file is shorter than its header says (truncated download?)."; return false; }

    out.v.resize (count);
    const uint8_t* p = d + off;
    for (size_t i = 0; i < count; ++i)
    {
        const uint8_t* q = p + i * (size_t) bytesPer;
        float raw = 0.0f;
        switch (dtype)
        {
            case 2:   raw = (float) q[0]; break;
            case 256: raw = (float) (int8_t) q[0]; break;
            case 4:   raw = (float) (int16_t) rd16 (q, big); break;
            case 512: raw = (float) rd16 (q, big); break;
            case 8:   raw = (float) (int32_t) rd32 (q, big); break;
            case 768: raw = (float) rd32 (q, big); break;
            case 16:  raw = rdF32 (q, big); break;
            case 64:  raw = (float) rdF64 (q, big); break;
            default: break;
        }
        out.v[i] = raw * slope + inter;
    }

    out.n[0] = n[0]; out.n[1] = n[1]; out.n[2] = n[2];
    for (int i = 0; i < 3; ++i) out.spacing[i] = pix[i];

    /*  Orientation: prefer sform, else qform, else the identity. Only the
        DIRECTION is taken — the translation does not matter to a cube. */
    const int sform = (int) (int16_t) rd16 (d + 254, big);
    float dir[9] = { 1,0,0, 0,1,0, 0,0,1 };
    if (sform > 0)
    {
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                dir[r * 3 + c] = rdF32 (d + 280 + 16 * r + 4 * c, big);
        //  the rows carry the spacing as well; only the direction is wanted
        for (int c = 0; c < 3; ++c)
        {
            const float len = std::sqrt (dir[c] * dir[c] + dir[3 + c] * dir[3 + c] + dir[6 + c] * dir[6 + c]);
            if (len > 1e-8f) { dir[c] /= len; dir[3 + c] /= len; dir[6 + c] /= len; }
        }
    }
    std::memcpy (out.dir, dir, sizeof (dir));

    out.note = "NIfTI-1  " + std::to_string (n[0]) + "x" + std::to_string (n[1]) + "x"
             + std::to_string (n[2]);
    return true;
}

//==============================================================================
//  DICOM.  Uncompressed transfer syntaxes only, and it says so when it is not.
//==============================================================================
namespace
{
    struct Elem { uint16_t group, elem; uint32_t len; const uint8_t* val; char vr[3]; };

    bool isSeqVr (const char* vr)
    {
        return !std::strncmp (vr, "OB", 2) || !std::strncmp (vr, "OW", 2) || !std::strncmp (vr, "OF", 2)
            || !std::strncmp (vr, "SQ", 2) || !std::strncmp (vr, "UT", 2) || !std::strncmp (vr, "UN", 2);
    }

    /*  Walk one element. Returns false at the end of the buffer. `at` is
        advanced past the element, sequences included. */
    bool nextElem (const uint8_t* d, size_t bytes, size_t& at, bool explicitVr, bool big, Elem& e)
    {
        if (at + 8 > bytes) return false;
        e.group = rd16 (d + at, big);
        e.elem  = rd16 (d + at + 2, big);
        e.vr[0] = e.vr[1] = e.vr[2] = 0;
        size_t p = at + 4;

        if (explicitVr && !(e.group == 0xFFFE))
        {
            if (p + 2 > bytes) return false;
            e.vr[0] = (char) d[p]; e.vr[1] = (char) d[p + 1];
            p += 2;
            if (isSeqVr (e.vr))
            {
                if (p + 6 > bytes) return false;
                p += 2;                                   // reserved
                e.len = rd32 (d + p, big); p += 4;
            }
            else { if (p + 2 > bytes) return false; e.len = rd16 (d + p, big); p += 2; }
        }
        else
        {
            if (p + 4 > bytes) return false;
            e.len = rd32 (d + p, big); p += 4;
        }

        e.val = d + p;

        if (e.len == 0xFFFFFFFFu)
        {
            //  undefined length: a sequence, or encapsulated pixel data. Walk
            //  items to its delimiter so the scan can carry on past it.
            size_t q = p;
            int depth = 1;
            while (q + 8 <= bytes && depth > 0)
            {
                const uint16_t g = rd16 (d + q, big), el = rd16 (d + q + 2, big);
                const uint32_t l = rd32 (d + q + 4, big);
                q += 8;
                if (g == 0xFFFE && el == 0xE0DD) { --depth; continue; }         // sequence end
                if (g == 0xFFFE && el == 0xE000) { if (l != 0xFFFFFFFFu) q += l; continue; }
                if (g == 0xFFFE && el == 0xE00D) continue;                       // item end
                if (l == 0xFFFFFFFFu) ++depth; else q += l;
            }
            at = q;
            e.len = 0;
            return true;
        }

        if (p + e.len > bytes) { e.len = (uint32_t) (bytes - p); }
        at = p + e.len;
        return true;
    }
}

bool readDicomFile (const uint8_t* d, size_t bytes, DicomSlice& out, std::string& err)
{
    if (bytes < 132) { err = "too short to be a DICOM file."; return false; }

    size_t at = 0;
    bool explicitVr = false, big = false;
    std::string tsUid;

    if (!std::memcmp (d + 128, "DICM", 4))
    {
        //  the meta group is always explicit VR little endian
        at = 132;
        size_t metaAt = at;
        Elem e;
        while (metaAt < bytes && nextElem (d, bytes, metaAt, true, false, e) && e.group == 0x0002)
        {
            if (e.group == 0x0002 && e.elem == 0x0010) tsUid = trimmed (e.val, e.len);
            at = metaAt;
        }
        if (tsUid == "1.2.840.10008.1.2")        { explicitVr = false; big = false; }
        else if (tsUid == "1.2.840.10008.1.2.1") { explicitVr = true;  big = false; }
        else if (tsUid == "1.2.840.10008.1.2.2") { explicitVr = true;  big = true;  }
        else if (tsUid.empty())                  { explicitVr = true;  big = false; }
        else
        {
            err = "this DICOM is stored with a compressed transfer syntax (" + tsUid
                + "). Convert the folder with 'dcm2niix' and open the .nii it writes.";
            return false;
        }
    }
    else
    {
        //  a bare dataset: guess. If bytes 4-5 look like a VR, it is explicit.
        const bool looksVr = d[4] >= 'A' && d[4] <= 'Z' && d[5] >= 'A' && d[5] <= 'Z';
        explicitVr = looksVr;
    }

    int rows = 0, cols = 0, bitsAlloc = 16, bitsStored = 16, pixRep = 0, samples = 1;
    bool mono1 = false;
    const uint8_t* pixels = nullptr;
    size_t pixLen = 0;
    bool sawPos = false, sawOrient = false, sawSpacing = false;
    float between = 0.0f;

    Elem e;
    while (at < bytes && nextElem (d, bytes, at, explicitVr, big, e))
    {
        if (e.group == 0x0028)
        {
            switch (e.elem)
            {
                case 0x0002: samples = (int) rd16 (e.val, big); break;
                case 0x0004: mono1 = (trimmed (e.val, e.len) == "MONOCHROME1"); break;
                case 0x0010: rows = (int) rd16 (e.val, big); break;
                case 0x0011: cols = (int) rd16 (e.val, big); break;
                case 0x0030: parseDs (e.val, e.len, out.pixelSpacing, 2); sawSpacing = true; break;
                case 0x0100: bitsAlloc = (int) rd16 (e.val, big); break;
                case 0x0101: bitsStored = (int) rd16 (e.val, big); break;
                case 0x0103: pixRep = (int) rd16 (e.val, big); break;
                case 0x1052: { float f[1]; parseDs (e.val, e.len, f, 1); out.intercept = f[0]; } break;
                case 0x1053: { float f[1]; parseDs (e.val, e.len, f, 1); out.slope = f[0]; } break;
                default: break;
            }
        }
        else if (e.group == 0x0018)
        {
            if (e.elem == 0x0050) { float f[1]; parseDs (e.val, e.len, f, 1); out.thickness = f[0]; }
            else if (e.elem == 0x0088) { float f[1]; parseDs (e.val, e.len, f, 1); between = f[0]; }
        }
        else if (e.group == 0x0020)
        {
            if (e.elem == 0x0032) { parseDs (e.val, e.len, out.pos, 3); sawPos = true; }
            else if (e.elem == 0x0037) { parseDs (e.val, e.len, out.orient, 6); sawOrient = true; }
            else if (e.elem == 0x0013) { float f[1]; parseDs (e.val, e.len, f, 1); out.instance = (int) f[0]; }
            else if (e.elem == 0x000E) out.seriesUid = trimmed (e.val, e.len);
        }
        else if (e.group == 0x7FE0 && e.elem == 0x0010)
        {
            if (e.len == 0)
            {
                err = "this DICOM's pixel data is encapsulated, which means it is "
                      "compressed. Convert the folder with 'dcm2niix' and open the .nii.";
                return false;
            }
            pixels = e.val; pixLen = e.len;
            break;                                   // nothing after it is wanted
        }
    }

    if (rows < 2 || cols < 2) { err = "no image dimensions in this file (is it a DICOMDIR?)."; return false; }
    if (samples != 1) { err = "this is a colour image (" + std::to_string (samples)
        + " samples per pixel); a specimen needs one density per voxel."; return false; }
    if (pixels == nullptr) { err = "no pixel data in this file."; return false; }
    if (bitsAlloc != 8 && bitsAlloc != 16)
    { err = std::to_string (bitsAlloc) + " bits per pixel is not read here."; return false; }

    const size_t want = (size_t) rows * (size_t) cols;
    const size_t have = pixLen / (size_t) (bitsAlloc / 8);
    if (have < want) { err = "the pixel data is short: " + std::to_string (have) + " of "
        + std::to_string (want) + " pixels."; return false; }

    if (!std::isfinite (out.slope) || out.slope == 0.0f) out.slope = 1.0f;
    if (!std::isfinite (out.intercept)) out.intercept = 0.0f;
    if (between > 0.0f) out.thickness = between;
    if (!sawSpacing) { out.pixelSpacing[0] = out.pixelSpacing[1] = 1.0f; }
    if (!sawOrient) { const float id[6] { 1,0,0, 0,1,0 }; std::memcpy (out.orient, id, sizeof (id)); }
    if (!sawPos) { out.pos[0] = out.pos[1] = 0; out.pos[2] = (float) out.instance; }

    out.rows = rows; out.cols = cols;
    out.pix.resize (want);
    const int shift = (bitsAlloc == 16 && bitsStored > 0 && bitsStored < 16) ? (16 - bitsStored) : 0;
    for (size_t i = 0; i < want; ++i)
    {
        float raw;
        if (bitsAlloc == 8) raw = pixRep ? (float) (int8_t) pixels[i] : (float) pixels[i];
        else
        {
            uint16_t u = rd16 (pixels + 2 * i, big);
            if (pixRep) { int16_t s = (int16_t) (shift ? (int16_t) (u << shift) >> shift : (int16_t) u); raw = (float) s; }
            else raw = (float) u;
        }
        float hu = raw * out.slope + out.intercept;
        if (mono1) hu = -hu;
        out.pix[i] = hu;
    }
    return true;
}

//==============================================================================
bool assembleDicom (std::vector<DicomSlice>& s, SrcVolume& out, std::string& err)
{
    if (s.size() < 2) { err = "a series needs at least two slices; " + std::to_string (s.size())
        + " were read."; return false; }

    //  keep only the largest series in the folder — archives mix scouts,
    //  dose reports and reformats in with the scan
    if (!s[0].seriesUid.empty())
    {
        std::string best; size_t bestN = 0;
        for (const auto& a : s)
        {
            size_t c = 0;
            for (const auto& b : s) if (b.seriesUid == a.seriesUid) ++c;
            if (c > bestN) { bestN = c; best = a.seriesUid; }
        }
        if (bestN >= 2 && bestN < s.size())
        {
            std::vector<DicomSlice> keep;
            for (auto& a : s) if (a.seriesUid == best) keep.push_back (std::move (a));
            s.swap (keep);
        }
    }

    const int rows = s[0].rows, cols = s[0].cols;
    for (const auto& a : s)
        if (a.rows != rows || a.cols != cols)
        { err = "the slices are not all the same size (" + std::to_string (cols) + "x"
              + std::to_string (rows) + " then " + std::to_string (a.cols) + "x"
              + std::to_string (a.rows) + "). Point this at one series."; return false; }

    /*  ORDER BY POSITION ALONG THE SLICE NORMAL, never by filename. A directory
        listing is not slice order, and getting it wrong shuffles or mirrors the
        body with no other symptom. */
    const float* o = s[0].orient;
    float nrm[3] { o[1] * o[5] - o[2] * o[4],
                   o[2] * o[3] - o[0] * o[5],
                   o[0] * o[4] - o[1] * o[3] };
    const float nl = std::sqrt (nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
    if (nl > 1e-6f) { nrm[0] /= nl; nrm[1] /= nl; nrm[2] /= nl; }
    else { nrm[0] = 0; nrm[1] = 0; nrm[2] = 1; }

    std::stable_sort (s.begin(), s.end(), [&] (const DicomSlice& a, const DicomSlice& b)
    {
        const float da = a.pos[0] * nrm[0] + a.pos[1] * nrm[1] + a.pos[2] * nrm[2];
        const float db = b.pos[0] * nrm[0] + b.pos[1] * nrm[1] + b.pos[2] * nrm[2];
        if (da != db) return da < db;
        return a.instance < b.instance;
    });

    //  slice spacing from the positions themselves — the tag lies often enough
    float dz = 0.0f;
    {
        const float d0 = s.front().pos[0] * nrm[0] + s.front().pos[1] * nrm[1] + s.front().pos[2] * nrm[2];
        const float d1 = s.back().pos[0]  * nrm[0] + s.back().pos[1]  * nrm[1] + s.back().pos[2]  * nrm[2];
        const float span = std::fabs (d1 - d0);
        if (span > 1e-4f) dz = span / (float) (s.size() - 1);
    }
    if (!(dz > 0.0f)) dz = s[0].thickness > 0.0f ? s[0].thickness : 1.0f;

    const size_t count = (size_t) rows * (size_t) cols * s.size();
    if (count > MAXVOX) { err = "this series has " + std::to_string (count / 1000000)
        + " million voxels, more than can be held at once."; return false; }

    out.v.resize (count);
    for (size_t k = 0; k < s.size(); ++k)
        std::memcpy (out.v.data() + k * (size_t) rows * cols, s[k].pix.data(),
                     (size_t) rows * cols * sizeof (float));

    out.n[0] = cols; out.n[1] = rows; out.n[2] = (int) s.size();
    out.spacing[0] = s[0].pixelSpacing[1] > 0 ? s[0].pixelSpacing[1] : 1.0f;   // column spacing
    out.spacing[1] = s[0].pixelSpacing[0] > 0 ? s[0].pixelSpacing[0] : 1.0f;   // row spacing
    out.spacing[2] = dz;

    //  column j of dir is the patient direction of source axis j
    for (int r = 0; r < 3; ++r)
    {
        out.dir[r * 3 + 0] = o[r];
        out.dir[r * 3 + 1] = o[3 + r];
        out.dir[r * 3 + 2] = nrm[r];
    }

    out.note = "DICOM  " + std::to_string (cols) + "x" + std::to_string (rows) + "x"
             + std::to_string (s.size());
    return true;
}

//==============================================================================
bool assembleStack (const std::vector<std::vector<float>>& slices, int w, int h,
                    const float spacing[3], SrcVolume& out, std::string& err)
{
    if (slices.size() < 2) { err = "a stack needs at least two images."; return false; }
    if (w < 2 || h < 2) { err = "the images are too small to be a volume."; return false; }
    const size_t want = (size_t) w * (size_t) h;
    for (const auto& s : slices)
        if (s.size() != want) { err = "the images are not all the same size."; return false; }

    const size_t count = want * slices.size();
    if (count > MAXVOX) { err = "this stack is larger than can be held at once."; return false; }

    out.v.resize (count);
    for (size_t k = 0; k < slices.size(); ++k)
        std::memcpy (out.v.data() + k * want, slices[k].data(), want * sizeof (float));

    out.n[0] = w; out.n[1] = h; out.n[2] = (int) slices.size();
    for (int i = 0; i < 3; ++i) out.spacing[i] = (spacing && spacing[i] > 0) ? spacing[i] : 1.0f;
    const float id[9] { 1,0,0, 0,1,0, 0,0,1 };
    std::memcpy (out.dir, id, sizeof (id));
    out.note = "image stack  " + std::to_string (w) + "x" + std::to_string (h) + "x"
             + std::to_string (slices.size());
    return true;
}

//==============================================================================
void percentileWindow (const std::vector<float>& v, float loPct, float hiPct, float& lo, float& hi)
{
    float mn = std::numeric_limits<float>::infinity();
    float mx = -std::numeric_limits<float>::infinity();
    size_t good = 0;
    for (float f : v) if (std::isfinite (f)) { if (f < mn) mn = f; if (f > mx) mx = f; ++good; }
    if (good == 0 || !(mx > mn)) { lo = 0.0f; hi = 1.0f; return; }

    const int NB = 4096;
    std::vector<uint32_t> h ((size_t) NB, 0u);
    const double s = (double) NB / ((double) mx - (double) mn);
    for (float f : v)
    {
        if (!std::isfinite (f)) continue;
        int b = (int) (((double) f - (double) mn) * s);
        if (b < 0) b = 0; if (b >= NB) b = NB - 1;
        ++h[(size_t) b];
    }
    const double wantLo = (double) good * (double) loPct;
    const double wantHi = (double) good * (double) hiPct;
    double c = 0; int bLo = 0, bHi = NB - 1;
    bool haveLo = false;
    for (int b = 0; b < NB; ++b)
    {
        c += (double) h[(size_t) b];
        if (!haveLo && c >= wantLo) { bLo = b; haveLo = true; }
        if (c >= wantHi) { bHi = b; break; }
    }
    lo = (float) ((double) mn + ((double) bLo) / s);
    hi = (float) ((double) mn + ((double) bHi + 1.0) / s);
    if (!(hi > lo)) { lo = mn; hi = mx; }
}

//==============================================================================
//  The resampler.
//==============================================================================
namespace
{
    /*  Taps for one axis, srcN -> dstN across the FULL extent. Source sample i
        is centred at i + 0.5, so the tent is placed in those coordinates: get
        that wrong by half a voxel and every import is shifted, which is the
        classic way to break a resampler without it looking broken. */
    struct Taps
    {
        std::vector<int>   first;
        std::vector<int>   count;
        std::vector<float> w;
        int maxTaps = 0;
    };

    Taps buildTaps (int srcN, int dstN)
    {
        Taps t;
        t.first.resize ((size_t) dstN);
        t.count.resize ((size_t) dstN);
        const double scale  = (double) srcN / (double) dstN;
        const double fscale = scale > 1.0 ? scale : 1.0;    // widen only when decimating
        std::vector<std::vector<float>> rows ((size_t) dstN);

        for (int j = 0; j < dstN; ++j)
        {
            const double c = ((double) j + 0.5) * scale;
            int a = (int) std::ceil  (c - fscale - 0.5);
            int b = (int) std::floor (c + fscale - 0.5);
            if (b < a) b = a;
            std::vector<float> ws;
            double sum = 0;
            for (int i = a; i <= b; ++i)
            {
                const double d = std::fabs (((double) i + 0.5) - c) / fscale;
                const double w = d < 1.0 ? (1.0 - d) : 0.0;
                ws.push_back ((float) w);
                sum += w;
            }
            if (sum <= 0) { ws.assign (1, 1.0f); a = (int) c; b = a; sum = 1.0; }
            for (auto& w : ws) w = (float) ((double) w / sum);
            t.first[(size_t) j] = a;
            t.count[(size_t) j] = (int) ws.size();
            t.maxTaps = std::max (t.maxTaps, (int) ws.size());
            rows[(size_t) j] = std::move (ws);
        }
        for (int j = 0; j < dstN; ++j)
            for (float w : rows[(size_t) j]) t.w.push_back (w);
        return t;
    }

    inline int clampi (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

bool resampleToCube (const SrcVolume& src, int dstN, const ImportOpts& opts,
                     float* out, ImportReport& rep, std::string& err)
{
    if (!src.valid()) { err = "the source volume is empty or malformed."; return false; }
    if (dstN < 8) { err = "the cube is too small."; return false; }

    //  ---- 1. which source axis becomes which cube axis, and which way round --
    int sAxis[3] { 0, 1, 2 };
    bool flip[3] { false, false, false };
    if (opts.reorient)
    {
        int fromCanon[3] { -1, -1, -1 };
        bool signNeg[3] { false, false, false };
        for (int c = 0; c < 3; ++c)
        {
            int best = 0; float bv = 0;
            for (int r = 0; r < 3; ++r)
            {
                const float a = std::fabs (src.dir[r * 3 + c]);
                if (a > bv) { bv = a; best = r; }
            }
            if (fromCanon[best] < 0) { fromCanon[best] = c; signNeg[best] = src.dir[best * 3 + c] < 0.0f; }
        }
        //  only trust it if it really is a permutation
        if (fromCanon[0] >= 0 && fromCanon[1] >= 0 && fromCanon[2] >= 0)
            for (int r = 0; r < 3; ++r) { sAxis[r] = fromCanon[r]; flip[r] = signNeg[r]; }
    }

    //  the phase axis choice is applied on top, as a rotation of the three
    const int pa = clampi (opts.phaseAxis, 0, 2);
    if (pa != 0)
    {
        int a2[3]; bool f2[3];
        for (int i = 0; i < 3; ++i) { a2[i] = sAxis[(i + pa) % 3]; f2[i] = flip[(i + pa) % 3]; }
        for (int i = 0; i < 3; ++i) { sAxis[i] = a2[i]; flip[i] = f2[i]; }
    }

    int   sn[3];  float sp[3], ext[3];
    for (int a = 0; a < 3; ++a)
    {
        sn[a] = src.n[sAxis[a]];
        sp[a] = src.spacing[sAxis[a]] > 0 ? src.spacing[sAxis[a]] : 1.0f;
        ext[a] = (float) sn[a] * sp[a];
    }

    //  ---- 2. how much of the cube the body fills ----------------------------
    int filled[3];
    const float emax = std::max (ext[0], std::max (ext[1], ext[2]));
    for (int a = 0; a < 3; ++a)
        filled[a] = opts.stretch ? dstN
                                 : clampi ((int) std::lround ((double) dstN * ext[a] / (double) emax), 4, dstN);

    //  ---- 3. the window -----------------------------------------------------
    float lo = opts.loValue, hi = opts.hiValue;
    if (!opts.useValues || !(hi > lo)) percentileWindow (src.v, opts.loPct, opts.hiPct, lo, hi);
    if (!(hi > lo)) { hi = lo + 1.0f; }

    //  ---- 4. three separable passes, in native units ------------------------
    const Taps tx = buildTaps (sn[0], filled[0]);
    const Taps ty = buildTaps (sn[1], filled[1]);
    const Taps tz = buildTaps (sn[2], filled[2]);

    auto srcIndex = [&] (int a, int idx) { return flip[a] ? src.n[sAxis[a]] - 1 - idx : idx; };

    //  pass X: gathers through the permutation, so nothing is copied first
    std::vector<float> b1 ((size_t) filled[0] * (size_t) sn[1] * (size_t) sn[2]);
    {
        int idx[3];
        for (int k = 0; k < sn[2]; ++k)
        {
            idx[sAxis[2]] = srcIndex (2, k);
            for (int j = 0; j < sn[1]; ++j)
            {
                idx[sAxis[1]] = srcIndex (1, j);
                float* dst = b1.data() + ((size_t) k * sn[1] + (size_t) j) * filled[0];
                const float* w = tx.w.data();
                for (int i = 0; i < filled[0]; ++i)
                {
                    const int a = tx.first[(size_t) i], c = tx.count[(size_t) i];
                    float acc = 0;
                    for (int q = 0; q < c; ++q)
                    {
                        idx[sAxis[0]] = srcIndex (0, clampi (a + q, 0, sn[0] - 1));
                        acc += w[q] * src.at (idx[0], idx[1], idx[2]);
                    }
                    w += c;
                    dst[i] = acc;
                }
            }
        }
    }

    //  pass Y
    std::vector<float> b2 ((size_t) filled[0] * (size_t) filled[1] * (size_t) sn[2]);
    for (int k = 0; k < sn[2]; ++k)
    {
        const float* plane = b1.data() + (size_t) k * sn[1] * filled[0];
        float* dstPlane = b2.data() + (size_t) k * filled[1] * filled[0];
        const float* w = ty.w.data();
        for (int j = 0; j < filled[1]; ++j)
        {
            const int a = ty.first[(size_t) j], c = ty.count[(size_t) j];
            float* dst = dstPlane + (size_t) j * filled[0];
            for (int i = 0; i < filled[0]; ++i) dst[i] = 0.0f;
            for (int q = 0; q < c; ++q)
            {
                const float* srcRow = plane + (size_t) clampi (a + q, 0, sn[1] - 1) * filled[0];
                const float ww = w[q];
                for (int i = 0; i < filled[0]; ++i) dst[i] += ww * srcRow[i];
            }
            w += c;
        }
    }
    b1.clear(); b1.shrink_to_fit();

    //  pass Z
    const size_t planeN = (size_t) filled[0] * (size_t) filled[1];
    std::vector<float> b3 (planeN * (size_t) filled[2], 0.0f);
    {
        const float* w = tz.w.data();
        for (int k = 0; k < filled[2]; ++k)
        {
            const int a = tz.first[(size_t) k], c = tz.count[(size_t) k];
            float* dst = b3.data() + (size_t) k * planeN;
            for (int q = 0; q < c; ++q)
            {
                const float* srcPlane = b2.data() + (size_t) clampi (a + q, 0, sn[2] - 1) * planeN;
                const float ww = w[q];
                for (size_t i = 0; i < planeN; ++i) dst[i] += ww * srcPlane[i];
            }
            w += c;
        }
    }
    b2.clear(); b2.shrink_to_fit();

    //  ---- 5. window into [0,1] and place, centred, in the cube --------------
    const size_t cube = (size_t) dstN * (size_t) dstN * (size_t) dstN;
    for (size_t i = 0; i < cube; ++i) out[i] = 0.0f;
    const float inv = 1.0f / (hi - lo);
    int off[3];
    for (int a = 0; a < 3; ++a) off[a] = (dstN - filled[a]) / 2;

    for (int k = 0; k < filled[2]; ++k)
        for (int j = 0; j < filled[1]; ++j)
        {
            const float* s = b3.data() + ((size_t) k * filled[1] + (size_t) j) * filled[0];
            float* d = out + (((size_t) (k + off[2]) * dstN + (size_t) (j + off[1])) * dstN + (size_t) off[0]);
            for (int i = 0; i < filled[0]; ++i)
            {
                float t = std::isfinite (s[i]) ? (s[i] - lo) * inv : 0.0f;
                d[i] = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            }
        }

    //  ---- 6. the report -----------------------------------------------------
    for (int a = 0; a < 3; ++a)
    {
        rep.srcN[a] = sn[a];
        rep.srcSpacing[a] = sp[a];
        rep.extentMm[a] = ext[a];
        rep.filled[a] = filled[a];
        rep.axisOrder[a] = sAxis[a];
        rep.flipped[a] = flip[a];
    }
    rep.loValue = lo; rep.hiValue = hi;
    rep.airFraction = 1.0f - (float) ((double) filled[0] * filled[1] * filled[2] / (double) cube);
    rep.note = src.note;
    return true;
}

} // namespace bs
