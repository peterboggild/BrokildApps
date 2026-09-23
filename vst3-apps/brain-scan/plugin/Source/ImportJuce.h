#pragma once

/*  The thin JUCE layer over Import.cpp: get bytes off disk and hand them to a
    reader that knows nothing about files. Everything decided here is a
    question of WHICH reader, never of how a volume is built.

    What a person is allowed to point at:

      a .nii or .nii.gz        the file itself
      a folder of DICOM        the folder, or any one slice in it — a single
                               slice is not a volume, so its folder is used
      a folder of PNG or JPEG  the folder, or any one image in it

    Anything else says what it is and what to do about it. A reader that
    guesses is worse than one that refuses.
*/

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "Import.h"

namespace bsjuce
{

inline bool looksGzipped (const juce::MemoryBlock& mb)
{
    return mb.getSize() > 2 && ((const juce::uint8*) mb.getData())[0] == 0x1f
                            && ((const juce::uint8*) mb.getData())[1] == 0x8b;
}

inline bool gunzip (const juce::MemoryBlock& in, juce::MemoryBlock& out)
{
    juce::MemoryInputStream src (in, false);
    juce::GZIPDecompressorInputStream gz (&src, false, juce::GZIPDecompressorInputStream::gzipFormat);
    juce::MemoryOutputStream dst;
    if (dst.writeFromInputStream (gz, -1) <= 0) return false;
    out = dst.getMemoryBlock();
    return out.getSize() > 0;
}

inline bool isImageFile (const juce::File& f)
{
    return f.hasFileExtension ("png;jpg;jpeg;bmp;gif");
}

inline bool isNiftiFile (const juce::File& f)
{
    return f.hasFileExtension ("nii") || f.getFileName().endsWithIgnoreCase (".nii.gz");
}

//==============================================================================
inline bool loadNifti (const juce::File& f, bs::SrcVolume& out, juce::String& err)
{
    juce::MemoryBlock mb;
    if (! f.loadFileAsData (mb)) { err = "could not read " + f.getFileName(); return false; }
    if (looksGzipped (mb))
    {
        juce::MemoryBlock un;
        if (! gunzip (mb, un)) { err = "this .gz could not be decompressed."; return false; }
        mb = un;
    }
    std::string e;
    if (! bs::readNifti ((const juce::uint8*) mb.getData(), mb.getSize(), out, e))
    { err = juce::String (juce::CharPointer_UTF8 (e.c_str())); return false; }
    return true;
}

//==============================================================================
inline bool loadDicomFolder (const juce::File& dir, bs::SrcVolume& out, juce::String& err)
{
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false);
    if (files.isEmpty()) { err = "no files in " + dir.getFileName(); return false; }
    if (files.size() > 4000) { err = "that folder holds " + juce::String (files.size())
        + " files; point at a single series."; return false; }

    std::vector<bs::DicomSlice> slices;
    juce::String firstRefusal;
    int tried = 0;
    for (const auto& f : files)
    {
        if (f.getFileName().startsWithIgnoreCase ("DICOMDIR")) continue;
        if (isImageFile (f) || isNiftiFile (f)) continue;
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb) || mb.getSize() < 132) continue;
        ++tried;
        bs::DicomSlice s;
        std::string e;
        if (bs::readDicomFile ((const juce::uint8*) mb.getData(), mb.getSize(), s, e))
            slices.push_back (std::move (s));
        else if (firstRefusal.isEmpty() && e.find ("compressed") != std::string::npos)
            firstRefusal = juce::String (juce::CharPointer_UTF8 (e.c_str()));
    }
    if (slices.size() < 2)
    {
        if (firstRefusal.isNotEmpty()) { err = firstRefusal; return false; }
        err = tried == 0 ? juce::String ("nothing in that folder looks like DICOM.")
                         : juce::String ("only ") + juce::String ((int) slices.size())
                           + " of " + juce::String (tried) + " files parsed as DICOM slices.";
        return false;
    }
    std::string e;
    if (! bs::assembleDicom (slices, out, e))
    { err = juce::String (juce::CharPointer_UTF8 (e.c_str())); return false; }
    return true;
}

//==============================================================================
inline bool loadImageStack (const juce::File& dir, bs::SrcVolume& out, juce::String& err)
{
    juce::Array<juce::File> files;
    dir.findChildFiles (files, juce::File::findFiles, false);
    juce::Array<juce::File> imgs;
    for (const auto& f : files) if (isImageFile (f)) imgs.add (f);
    if (imgs.size() < 2) { err = "a stack needs at least two images; " + juce::String (imgs.size())
        + " found."; return false; }
    if (imgs.size() > 2048) { err = "that is more than 2048 images."; return false; }

    /*  Natural order, so slice 2 comes before slice 10 — the whole stack is
        mirrored or shuffled otherwise, with no other symptom. */
    struct Nat { static int compareElements (const juce::File& a, const juce::File& b)
        { return a.getFileName().compareNatural (b.getFileName()); } };
    Nat nat; imgs.sort (nat);

    std::vector<std::vector<float>> slices;
    int w = 0, h = 0;
    for (const auto& f : imgs)
    {
        auto img = juce::ImageFileFormat::loadFrom (f);
        if (! img.isValid()) { err = "could not decode " + f.getFileName(); return false; }
        if (w == 0) { w = img.getWidth(); h = img.getHeight(); }
        else if (img.getWidth() != w || img.getHeight() != h)
        { err = "the images are not all the same size (" + f.getFileName() + ")."; return false; }

        std::vector<float> plane ((size_t) w * (size_t) h);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = bd.getPixelColour (x, y);
                plane[(size_t) y * (size_t) w + (size_t) x] = (float) c.getBrightness();
            }
        slices.push_back (std::move (plane));
    }
    const float sp[3] { 1.0f, 1.0f, 1.0f };
    std::string e;
    if (! bs::assembleStack (slices, w, h, sp, out, e))
    { err = juce::String (juce::CharPointer_UTF8 (e.c_str())); return false; }
    return true;
}

//==============================================================================
inline bool loadVolume (const juce::File& f, bs::SrcVolume& out, juce::String& err)
{
    if (! f.exists()) { err = "that file is gone."; return false; }

    if (f.isDirectory())
    {
        juce::Array<juce::File> imgs;
        f.findChildFiles (imgs, juce::File::findFiles, false, "*.png;*.jpg;*.jpeg");
        if (imgs.size() >= 2) return loadImageStack (f, out, err);
        return loadDicomFolder (f, out, err);
    }

    if (isNiftiFile (f)) return loadNifti (f, out, err);
    if (isImageFile (f)) return loadImageStack (f.getParentDirectory(), out, err);

    //  one DICOM slice is not a volume, so its folder is what was meant
    juce::MemoryBlock mb;
    if (f.loadFileAsData (mb) && mb.getSize() > 132)
    {
        bs::DicomSlice probe;
        std::string e;
        if (bs::readDicomFile ((const juce::uint8*) mb.getData(), mb.getSize(), probe, e))
            return loadDicomFolder (f.getParentDirectory(), out, err);
        if (e.find ("compressed") != std::string::npos)
        { err = juce::String (juce::CharPointer_UTF8 (e.c_str())); return false; }
    }

    //  a headerless .nii is still worth a try before giving up
    if (looksGzipped (mb) || mb.getSize() > 352)
    {
        bs::SrcVolume v;
        std::string e;
        juce::MemoryBlock body = mb;
        if (looksGzipped (mb)) { juce::MemoryBlock un; if (gunzip (mb, un)) body = un; }
        if (bs::readNifti ((const juce::uint8*) body.getData(), body.getSize(), v, e))
        { out = std::move (v); return true; }
    }

    err = "that is not a NIfTI file, a DICOM series or a stack of images.";
    return false;
}

} // namespace bsjuce
