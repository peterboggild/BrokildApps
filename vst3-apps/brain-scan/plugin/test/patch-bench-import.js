/*  bench.cpp — the import section.

    The resampler's correctness IS the feature, so every claim Import.h makes
    gets a number here: physical aspect (not index aspect), decimation that
    averages rather than point-samples, no half-voxel shift, a window robust to
    a metal clip, both readers round-tripped from bytes synthesised in memory,
    slice order taken from position rather than filename, and every refusal
    refusing.

    node test/patch-bench-import.js
*/
const fs = require("fs"), path = require("path");
const FILE = path.resolve(__dirname, "bench.cpp");
let s = fs.readFileSync(FILE, "utf8");
const NL = s.indexOf("\r\n") >= 0 ? "\r\n" : "\n";
const miss = [];
function rep(name, from, to){
  from = from.split("\n").join(NL); to = to.split("\n").join(NL);
  const n = s.split(from).length - 1;
  if (n !== 1){ miss.push(name + " (found " + n + ")"); return; }
  s = s.split(from).join(to);
}

rep("include", `#include "../Source/Engine.h"`, `#include "../Source/Engine.h"\n#include "../Source/Import.h"`);

/* ---- helpers, before main ------------------------------------------------ */
rep("helpers",
`static const double SR = 48000.0;
static const int BLK = 256;`,
`static const double SR = 48000.0;
static const int BLK = 256;

//==============================================================================
//  synthesised files, so the readers are measured on bytes and not on a mock
static void put16 (std::vector<uint8_t>& b, size_t at, uint16_t v)
{ b[at] = (uint8_t) (v & 0xff); b[at + 1] = (uint8_t) (v >> 8); }
static void put32 (std::vector<uint8_t>& b, size_t at, uint32_t v)
{ for (int i = 0; i < 4; ++i) b[at + (size_t) i] = (uint8_t) ((v >> (8 * i)) & 0xff); }
static void putF32 (std::vector<uint8_t>& b, size_t at, float f)
{ uint32_t u; std::memcpy (&u, &f, 4); put32 (b, at, u); }

/*  A NIfTI-1 single file: 348-byte header, magic at 344, voxels at 352. */
static std::vector<uint8_t> makeNifti (int nx, int ny, int nz,
                                       float sx, float sy, float sz,
                                       float slope, float inter,
                                       const std::vector<int16_t>& vox,
                                       const char* magic = "n+1")
{
    std::vector<uint8_t> b (352 + vox.size() * 2, 0);
    put32 (b, 0, 348);
    put16 (b, 40, 3);
    put16 (b, 42, (uint16_t) nx); put16 (b, 44, (uint16_t) ny); put16 (b, 46, (uint16_t) nz);
    put16 (b, 70, 4);                       // int16
    put16 (b, 72, 16);
    putF32 (b, 80, sx); putF32 (b, 84, sy); putF32 (b, 88, sz);
    putF32 (b, 108, 352.0f);
    putF32 (b, 112, slope); putF32 (b, 116, inter);
    put16 (b, 254, 1);                      // sform_code
    putF32 (b, 280, sx); putF32 (b, 296 + 4, sy); putF32 (b, 312 + 8, sz);
    std::memcpy (b.data() + 344, magic, 3);
    for (size_t i = 0; i < vox.size(); ++i)
        put16 (b, 352 + i * 2, (uint16_t) (uint16_t) vox[i]);
    return b;
}

static void dcmElem (std::vector<uint8_t>& b, uint16_t g, uint16_t e, const char* vr,
                     const void* data, size_t len)
{
    const size_t at = b.size();
    const bool longForm = ! std::strncmp (vr, "OB", 2) || ! std::strncmp (vr, "OW", 2)
                       || ! std::strncmp (vr, "SQ", 2) || ! std::strncmp (vr, "UN", 2);
    b.resize (at + (longForm ? 12 : 8) + len, 0);
    put16 (b, at, g); put16 (b, at + 2, e);
    b[at + 4] = (uint8_t) vr[0]; b[at + 5] = (uint8_t) vr[1];
    if (longForm) { put32 (b, at + 8, (uint32_t) len); std::memcpy (b.data() + at + 12, data, len); }
    else          { put16 (b, at + 6, (uint16_t) len); std::memcpy (b.data() + at + 8, data, len); }
}
static void dcmStr (std::vector<uint8_t>& b, uint16_t g, uint16_t e, const char* vr, std::string v)
{ if (v.size() & 1) v += ' '; dcmElem (b, g, e, vr, v.data(), v.size()); }
static void dcmU16 (std::vector<uint8_t>& b, uint16_t g, uint16_t e, uint16_t v)
{ uint8_t t[2] { (uint8_t) (v & 0xff), (uint8_t) (v >> 8) }; dcmElem (b, g, e, "US", t, 2); }

/*  One explicit-VR little-endian CT slice. */
static std::vector<uint8_t> makeDicom (int rows, int cols, float px, float py, float thick,
                                       float z, float slope, float inter,
                                       const std::vector<int16_t>& pix,
                                       const char* ts = "1.2.840.10008.1.2.1")
{
    std::vector<uint8_t> b (132, 0);
    std::memcpy (b.data() + 128, "DICM", 4);
    dcmStr (b, 0x0002, 0x0010, "UI", ts);
    dcmStr (b, 0x0018, 0x0050, "DS", std::to_string ((int) thick));
    dcmStr (b, 0x0020, 0x000E, "UI", "1.2.3.4.5");
    dcmStr (b, 0x0020, 0x0032, "DS", "0\\\\0\\\\" + std::to_string ((int) z));
    dcmStr (b, 0x0020, 0x0037, "DS", "1\\\\0\\\\0\\\\0\\\\1\\\\0");
    dcmU16 (b, 0x0028, 0x0002, 1);
    dcmU16 (b, 0x0028, 0x0010, (uint16_t) rows);
    dcmU16 (b, 0x0028, 0x0011, (uint16_t) cols);
    { char t[64]; std::snprintf (t, sizeof t, "%g\\\\%g", py, px); dcmStr (b, 0x0028, 0x0030, "DS", t); }
    dcmU16 (b, 0x0028, 0x0100, 16);
    dcmU16 (b, 0x0028, 0x0101, 16);
    dcmU16 (b, 0x0028, 0x0103, 1);
    dcmStr (b, 0x0028, 0x1052, "DS", std::to_string ((int) inter));
    dcmStr (b, 0x0028, 0x1053, "DS", std::to_string ((int) slope));
    std::vector<uint8_t> raw (pix.size() * 2);
    for (size_t i = 0; i < pix.size(); ++i)
    { const uint16_t u = (uint16_t) pix[i]; raw[i * 2] = (uint8_t) (u & 0xff); raw[i * 2 + 1] = (uint8_t) (u >> 8); }
    dcmElem (b, 0x7FE0, 0x0010, "OW", raw.data(), raw.size());
    return b;
}

//  extent of the values above a threshold, per axis, in voxels
static void cubeExtent (const std::vector<float>& c, int n, float th, int* ext)
{
    int lo[3] { n, n, n }, hi[3] { -1, -1, -1 };
    for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) for (int i = 0; i < n; ++i)
        if (c[((size_t) k * n + j) * n + i] > th)
        {
            const int p[3] { i, j, k };
            for (int a = 0; a < 3; ++a) { if (p[a] < lo[a]) lo[a] = p[a]; if (p[a] > hi[a]) hi[a] = p[a]; }
        }
    for (int a = 0; a < 3; ++a) ext[a] = hi[a] >= lo[a] ? hi[a] - lo[a] + 1 : 0;
}`);

/* ---- the section itself, before the summary ------------------------------ */
rep("section",
`    std::printf ("\\n%d checks, %d failed  -  %s\\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`,
`    //==========================================================================
    head ("8 - importing a real volume");
    {
        //  ---- anisotropy: physical extent, not index extent ---------------
        {
            /*  80 x 80 x 20 voxels at 0.5 x 0.5 x 2.0 mm is PHYSICALLY a cube,
                40 mm on a side, holding a 15 mm sphere. Resampled by index it
                comes out squashed 4x in z; resampled in millimetres it is
                still a sphere. */
            SrcVolume v;
            v.n[0] = 80; v.n[1] = 80; v.n[2] = 20;
            v.spacing[0] = 0.5f; v.spacing[1] = 0.5f; v.spacing[2] = 2.0f;
            v.v.assign (v.count(), -1000.0f);
            for (int k = 0; k < 20; ++k) for (int j = 0; j < 80; ++j) for (int i = 0; i < 80; ++i)
            {
                const float x = ((float) i + 0.5f) * 0.5f - 20.0f;
                const float y = ((float) j + 0.5f) * 0.5f - 20.0f;
                const float z = ((float) k + 0.5f) * 2.0f - 20.0f;
                if (std::sqrt (x * x + y * y + z * z) < 15.0f)
                    v.v[((size_t) k * 80 + j) * 80 + i] = 1000.0f;
            }
            ImportOpts o; ImportReport rep8; std::string err;
            std::vector<float> cube ((size_t) VN * VN * VN);
            const bool okr = resampleToCube (v, VN, o, cube.data(), rep8, err);
            ok (okr, "an anisotropic volume resamples", err.c_str());
            char d[160];
            std::snprintf (d, sizeof d, "filled %d x %d x %d of %d (extents %.0f x %.0f x %.0f mm)",
                           rep8.filled[0], rep8.filled[1], rep8.filled[2], VN,
                           rep8.extentMm[0], rep8.extentMm[1], rep8.extentMm[2]);
            std::printf ("  %s\\n", d);
            ok (rep8.filled[0] == VN && rep8.filled[1] == VN && rep8.filled[2] == VN,
                "a physically cubic volume fills the cube whatever its voxel counts say", d);
            int ext[3]; cubeExtent (cube, VN, 0.5f, ext);
            std::snprintf (d, sizeof d, "sphere reads %d x %d x %d voxels (want ~48 each)", ext[0], ext[1], ext[2]);
            std::printf ("  %s\\n", d);
            ok (std::abs (ext[0] - ext[2]) <= 3 && std::abs (ext[1] - ext[2]) <= 3,
                "and a sphere in it is still a sphere - THE anisotropy check", d);
        }

        //  ---- decimation averages, it does not point-sample ---------------
        {
            /*  A one-voxel checkerboard has no low frequencies at all: area
                averaged 4:1 it must collapse to a flat field. Point sampling
                would keep the pattern, and no later mip could undo it. */
            SrcVolume v;
            v.n[0] = v.n[1] = v.n[2] = 128;
            v.v.resize (v.count());
            for (int k = 0; k < 128; ++k) for (int j = 0; j < 128; ++j) for (int i = 0; i < 128; ++i)
                v.v[((size_t) k * 128 + j) * 128 + i] = ((i + j + k) & 1) ? 1.0f : 0.0f;
            ImportOpts o; o.stretch = true;
            ImportReport rp; std::string err;
            std::vector<float> c (32 * 32 * 32);
            resampleToCube (v, 32, o, c.data(), rp, err);
            double m = 0; for (float f : c) m += f; m /= (double) c.size();
            double sd = 0; for (float f : c) sd += (f - m) * (f - m); sd = std::sqrt (sd / (double) c.size());
            char d[128]; std::snprintf (d, sizeof d, "mean %.4f, sd %.5f (point sampling gives sd ~0.5)", m, sd);
            std::printf ("  %s\\n", d);
            ok (sd < 0.02, "4:1 decimation AVERAGES - a one-voxel checkerboard flattens", d);
        }

        //  ---- no half-voxel shift ------------------------------------------
        {
            SrcVolume v;
            v.n[0] = 64; v.n[1] = 16; v.n[2] = 16;
            v.v.resize (v.count());
            for (int k = 0; k < 16; ++k) for (int j = 0; j < 16; ++j) for (int i = 0; i < 64; ++i)
                v.v[((size_t) k * 16 + j) * 64 + i] = (float) i / 63.0f;
            ImportOpts o; o.stretch = true;
            ImportReport rp; std::string err;
            std::vector<float> c ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, c.data(), rp, err);
            double worst = 0;
            for (int i = VN / 8; i < VN - VN / 8; ++i)
            {
                const float got = c[(((size_t) VN / 2) * VN + VN / 2) * VN + (size_t) i];
                const double want = ((double) i + 0.5) / (double) VN;
                worst = std::max (worst, std::fabs ((double) got - want));
            }
            char d[96]; std::snprintf (d, sizeof d, "worst departure %.4f", worst);
            std::printf ("  %s\\n", d);
            ok (worst < 0.02, "a ramp resamples to a ramp - no half-voxel shift", d);
        }

        //  ---- one metal clip must not crush the tissue ---------------------
        {
            SrcVolume v;
            v.n[0] = v.n[1] = v.n[2] = 32;
            v.v.resize (v.count());
            for (size_t i = 0; i < v.count(); ++i) v.v[i] = (float) (i % 101);   // 0..100
            v.v[v.count() / 2] = 30000.0f;                                       // the clip
            float lo = 0, hi = 0;
            percentileWindow (v.v, 0.005f, 0.995f, lo, hi);
            char d[128]; std::snprintf (d, sizeof d, "window %.0f .. %.0f (full range 0 .. 30000)", lo, hi);
            std::printf ("  %s\\n", d);
            ok (hi < 300.0f, "the percentile window ignores an outlier", d);
            ImportOpts o; o.stretch = true; ImportReport rp; std::string err;
            std::vector<float> c ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, c.data(), rp, err);
            double m = 0; for (float f : c) m += f; m /= (double) c.size();
            std::snprintf (d, sizeof d, "mean of the cube %.3f", m);
            ok (m > 0.2, "so the tissue still uses the range", d);
        }

        //  ---- the NIfTI reader, on real bytes ------------------------------
        {
            std::vector<int16_t> vox ((size_t) 8 * 6 * 4);
            for (size_t i = 0; i < vox.size(); ++i) vox[i] = (int16_t) (i * 3);
            auto f = makeNifti (8, 6, 4, 0.5f, 1.0f, 2.5f, 2.0f, -100.0f, vox);
            SrcVolume v; std::string err;
            const bool okr = readNifti (f.data(), f.size(), v, err);
            ok (okr, "a NIfTI-1 file reads", err.c_str());
            if (okr)
            {
                char d[160];
                std::snprintf (d, sizeof d, "%d x %d x %d at %.2f x %.2f x %.2f mm, first %.1f",
                               v.n[0], v.n[1], v.n[2], v.spacing[0], v.spacing[1], v.spacing[2], v.v[0]);
                std::printf ("  %s\\n", d);
                ok (v.n[0] == 8 && v.n[1] == 6 && v.n[2] == 4, "its dimensions", d);
                ok (std::fabs (v.spacing[0] - 0.5f) < 1e-6f && std::fabs (v.spacing[2] - 2.5f) < 1e-6f,
                    "its spacing, which is what stops the body being squashed", d);
                ok (std::fabs (v.v[0] - (-100.0f)) < 1e-4f && std::fabs (v.v[1] - (3 * 2.0f - 100.0f)) < 1e-4f,
                    "and scl_slope / scl_inter applied", d);
            }
            //  refusals
            auto f2 = makeNifti (8, 6, 4, 1, 1, 1, 1, 0, vox, "n+2");
            put32 (f2, 0, 540);
            SrcVolume v2; std::string e2;
            ok (! readNifti (f2.data(), f2.size(), v2, e2)
                && e2.find ("NIfTI-2") != std::string::npos, "NIfTI-2 is refused by name, not misread", e2.c_str());
            std::vector<uint8_t> trunc (f.begin(), f.begin() + 400);
            SrcVolume v3; std::string e3;
            ok (! readNifti (trunc.data(), trunc.size(), v3, e3), "a truncated file is refused", e3.c_str());
        }

        //  ---- the DICOM reader, and slice ORDER ----------------------------
        {
            const int R = 6, C = 8;
            std::vector<DicomSlice> sl;
            //  positions deliberately out of order: a directory listing is not
            //  slice order, and sorting by name would keep this wrong
            const float zs[5] { 8.0f, 0.0f, 4.0f, 12.0f, 16.0f };
            std::string err;
            for (int q = 0; q < 5; ++q)
            {
                std::vector<int16_t> pix ((size_t) R * C);
                for (size_t i = 0; i < pix.size(); ++i) pix[i] = (int16_t) (100 * (int) zs[q] + (int) i);
                auto f = makeDicom (R, C, 0.5f, 0.75f, 4.0f, zs[q], 1.0f, -1024.0f, pix);
                DicomSlice s;
                const bool okr = readDicomFile (f.data(), f.size(), s, err);
                if (q == 0) ok (okr, "a DICOM slice reads", err.c_str());
                if (okr) sl.push_back (std::move (s));
            }
            ok (sl.size() == 5, "all five slices read");
            if (sl.size() == 5)
            {
                char d[160];
                std::snprintf (d, sizeof d, "%d x %d at %.2f x %.2f mm, first value %.0f HU",
                               sl[0].cols, sl[0].rows, sl[0].pixelSpacing[1], sl[0].pixelSpacing[0], sl[0].pix[0]);
                std::printf ("  %s\\n", d);
                ok (sl[0].rows == R && sl[0].cols == C, "its dimensions", d);
                ok (std::fabs (sl[0].pix[0] - (-1024.0f)) < 1e-3f,
                    "rescale slope and intercept give Hounsfield units", d);
                SrcVolume v;
                const bool okr = assembleDicom (sl, v, err);
                ok (okr, "the series assembles", err.c_str());
                if (okr)
                {
                    std::snprintf (d, sizeof d, "%d x %d x %d, slice spacing %.2f mm",
                                   v.n[0], v.n[1], v.n[2], v.spacing[2]);
                    std::printf ("  %s\\n", d);
                    ok (std::fabs (v.spacing[2] - 4.0f) < 1e-3f,
                        "slice spacing comes from the positions, not the tag", d);
                    //  slice k must be the one whose z was 4k
                    bool ordered = true;
                    for (int k = 0; k < 5; ++k)
                    {
                        const float got = v.at (0, 0, k);
                        const float want = 100.0f * (float) (4 * k) - 1024.0f;
                        if (std::fabs (got - want) > 1e-3f) ordered = false;
                    }
                    ok (ordered, "and the slices are ORDERED BY POSITION, not by the order they arrived");
                }
            }
            //  a compressed transfer syntax must refuse, by name
            std::vector<int16_t> pix ((size_t) R * C, 0);
            auto fz = makeDicom (R, C, 1, 1, 1, 0, 1, 0, pix, "1.2.840.10008.1.2.4.90");
            DicomSlice s2; std::string e2;
            ok (! readDicomFile (fz.data(), fz.size(), s2, e2)
                && e2.find ("dcm2niix") != std::string::npos,
                "a compressed DICOM refuses and says what to do", e2.c_str());
        }

        //  ---- determinism, and that an import actually plays ---------------
        {
            SrcVolume v;
            v.n[0] = 40; v.n[1] = 40; v.n[2] = 40;
            v.v.resize (v.count());
            for (int k = 0; k < 40; ++k) for (int j = 0; j < 40; ++j) for (int i = 0; i < 40; ++i)
            {
                const float x = (float) i / 39.0f - 0.5f, y = (float) j / 39.0f - 0.5f, z = (float) k / 39.0f - 0.5f;
                const float r = std::sqrt (x * x + y * y + z * z);
                v.v[((size_t) k * 40 + j) * 40 + i] = std::sin (28.0f * r) * (r < 0.45f ? 1.0f : 0.0f);
            }
            ImportOpts o; ImportReport rp; std::string err;
            std::vector<float> a ((size_t) VN * VN * VN), b ((size_t) VN * VN * VN);
            resampleToCube (v, VN, o, a.data(), rp, err);
            resampleToCube (v, VN, o, b.data(), rp, err);
            ok (std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0,
                "the same source resamples to the same cube, exactly");

            Line six[NLINES]; Params p; factory (0).build (six, p);
            Engine e1; e1.p = p; e1.prepare (SR, BLK); e1.setLines (six); e1.service();
            e1.setImported (a.data());
            ok (e1.importedActive() && e1.specimenLoaded() == SPEC_IMPORTED,
                "the engine reads the imported volume, and the specimen dial is untouched");
            e1.noteOn (52, 0.9f);
            std::vector<float> L (BLK), R (BLK); std::vector<float> out;
            for (int q = 0; q < 120; ++q) { e1.process (L.data(), R.data(), BLK);
                out.insert (out.end(), L.begin(), L.end()); }
            double pk = 0, rr = 0; bool fin = true;
            for (float f : out) { if (! std::isfinite (f)) fin = false; pk = std::max (pk, (double) std::fabs (f)); rr += (double) f * f; }
            rr = std::sqrt (rr / (double) out.size());
            char d[96]; std::snprintf (d, sizeof d, "peak %.3f rms %.3f", pk, rr);
            std::printf ("  %s\\n", d);
            ok (fin && pk <= 1.0 && rr > 0.005, "an imported volume sounds, and stays bounded", d);

            //  clearing it must leave no residue at all
            e1.allNotesOff();
            for (int q = 0; q < 40; ++q) e1.process (L.data(), R.data(), BLK);
            e1.clearImported();
            ok (! e1.importedActive() && e1.specimenLoaded() != SPEC_IMPORTED,
                "clearing the import gives the specimen dial back");
            Engine e2; e2.p = p; e2.prepare (SR, BLK); e2.setLines (six); e2.service();
            Engine e3; e3.p = p; e3.prepare (SR, BLK); e3.setLines (six); e3.service();
            e3.setImported (a.data()); e3.clearImported();
            e2.noteOn (48, 0.8f); e3.noteOn (48, 0.8f);
            std::vector<float> o2, o3;
            for (int q = 0; q < 60; ++q)
            {
                e2.process (L.data(), R.data(), BLK); o2.insert (o2.end(), L.begin(), L.end());
                e3.process (L.data(), R.data(), BLK); o3.insert (o3.end(), L.begin(), L.end());
            }
            ok (std::memcmp (o2.data(), o3.data(), o2.size() * sizeof (float)) == 0,
                "and an engine that imported and cleared is bit-identical to one that never did");
        }
    }

    std::printf ("\\n%d checks, %d failed  -  %s\\n", checks, fails, fails == 0 ? "ALL CLEAR" : "SEE ABOVE");`);

if (miss.length){ console.error("MISSED:\\n  " + miss.join("\\n  ")); process.exit(1); }
fs.writeFileSync(FILE, s);
console.log("patched bench.cpp");
