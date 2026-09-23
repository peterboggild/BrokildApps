#pragma once

/*  A 16-bit greyscale PNG writer and reader for the terrain.

    JUCE's Image is 8-bit, and eight bits over four units of height is a
    staircase a ball can hear; sixteen bits are the floor. PNG is zlib plus a
    CRC, both of which JUCE has, so this is forty lines rather than a library.
    Reader: accepts 16-bit and 8-bit greyscale, non-interlaced, any size
    (the caller resamples to NX x NZ).
*/

#include <JuceHeader.h>
#include <vector>
#include <cstdint>

namespace png16
{

inline uint32_t crc32 (const uint8_t* d, size_t n, uint32_t c = 0xffffffffu)
{
    static uint32_t table[256]; static bool init = false;
    if (! init) { for (uint32_t i = 0; i < 256; ++i) { uint32_t v = i; for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1; table[i] = v; } init = true; }
    for (size_t i = 0; i < n; ++i) c = table[(c ^ d[i]) & 0xff] ^ (c >> 8);
    return c;
}

inline void be32 (std::vector<uint8_t>& o, uint32_t v) { o.push_back ((uint8_t) (v >> 24)); o.push_back ((uint8_t) (v >> 16)); o.push_back ((uint8_t) (v >> 8)); o.push_back ((uint8_t) v); }

inline void chunk (std::vector<uint8_t>& o, const char* type, const std::vector<uint8_t>& data)
{
    be32 (o, (uint32_t) data.size());
    std::vector<uint8_t> td; td.insert (td.end(), type, type + 4); td.insert (td.end(), data.begin(), data.end());
    o.insert (o.end(), td.begin(), td.end());
    be32 (o, crc32 (td.data(), td.size()) ^ 0xffffffffu);
}

/*  values: w*h floats, row-major; scaled by 1/umax into 0..65535. */
inline juce::MemoryBlock write (const float* values, int w, int h, float umax)
{
    std::vector<uint8_t> raw; raw.reserve ((size_t) h * (1 + 2 * (size_t) w));
    for (int y = 0; y < h; ++y)
    {
        raw.push_back (0);   // filter: none
        for (int x = 0; x < w; ++x)
        {
            float v = values[(size_t) y * w + x] / umax; v = v < 0 ? 0 : (v > 1 ? 1 : v);
            const uint16_t q = (uint16_t) std::lround (v * 65535.0f);
            raw.push_back ((uint8_t) (q >> 8)); raw.push_back ((uint8_t) q);
        }
    }
    juce::MemoryOutputStream z;
    {
        juce::GZIPCompressorOutputStream gz (z, 9, 15);   // windowBits 15 = zlib format
        gz.write (raw.data(), raw.size());
        gz.flush();
    }
    std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a };
    std::vector<uint8_t> ihdr;
    be32 (ihdr, (uint32_t) w); be32 (ihdr, (uint32_t) h);
    ihdr.push_back (16); ihdr.push_back (0); ihdr.push_back (0); ihdr.push_back (0); ihdr.push_back (0);
    chunk (out, "IHDR", ihdr);
    std::vector<uint8_t> idat ((const uint8_t*) z.getData(), (const uint8_t*) z.getData() + z.getDataSize());
    chunk (out, "IDAT", idat);
    chunk (out, "IEND", {});
    return juce::MemoryBlock (out.data(), out.size());
}

/*  Reads greyscale 8/16-bit (colour type 0) or, as a courtesy, RGB/RGBA
    by luminance. Returns false if it is not a PNG we understand. */
inline bool read (const void* data, size_t size, std::vector<float>& values, int& w, int& h, float umax)
{
    const uint8_t* d = (const uint8_t*) data;
    if (size < 33 || std::memcmp (d, "\x89PNG\r\n\x1a\n", 8) != 0) return false;
    size_t p = 8; int depth = 0, ctype = 0; w = h = 0;
    std::vector<uint8_t> zdata;
    while (p + 8 <= size)
    {
        const uint32_t len = ((uint32_t) d[p] << 24) | ((uint32_t) d[p + 1] << 16) | ((uint32_t) d[p + 2] << 8) | d[p + 3];
        const char* type = (const char*) d + p + 4;
        if (p + 12 + len > size) return false;
        const uint8_t* body = d + p + 8;
        if (std::memcmp (type, "IHDR", 4) == 0)
        {
            w = (int) (((uint32_t) body[0] << 24) | ((uint32_t) body[1] << 16) | ((uint32_t) body[2] << 8) | body[3]);
            h = (int) (((uint32_t) body[4] << 24) | ((uint32_t) body[5] << 16) | ((uint32_t) body[6] << 8) | body[7]);
            depth = body[8]; ctype = body[9];
            if (body[12] != 0) return false;   // interlaced: no
        }
        else if (std::memcmp (type, "IDAT", 4) == 0) zdata.insert (zdata.end(), body, body + len);
        else if (std::memcmp (type, "IEND", 4) == 0) break;
        p += 12 + len;
    }
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (depth != 8 && depth != 16)) return false;
    int chans = 0;
    switch (ctype) { case 0: chans = 1; break; case 2: chans = 3; break; case 4: chans = 2; break; case 6: chans = 4; break; default: return false; }
    const size_t bpp = (size_t) chans * (depth / 8), stride = 1 + bpp * (size_t) w;
    juce::MemoryInputStream mi (zdata.data(), zdata.size(), false);
    juce::GZIPDecompressorInputStream gz (&mi, false, juce::GZIPDecompressorInputStream::zlibFormat, (juce::int64) (stride * (size_t) h));
    std::vector<uint8_t> raw (stride * (size_t) h);
    if (gz.read (raw.data(), (int) raw.size()) != (int) raw.size()) return false;
    //  unfilter (the five PNG filters), then convert
    std::vector<uint8_t> prev (stride, 0);
    for (int y = 0; y < h; ++y)
    {
        uint8_t* row = raw.data() + (size_t) y * stride;
        const int f = row[0];
        for (size_t i = 1; i < stride; ++i)
        {
            const int a = i > bpp ? row[i - bpp] : 0, b = prev[i], c = i > bpp ? prev[i - bpp] : 0;
            int pr = 0;
            switch (f)
            {
                case 1: pr = a; break;
                case 2: pr = b; break;
                case 3: pr = (a + b) / 2; break;
                case 4: { const int pp = a + b - c, pa = std::abs (pp - a), pb = std::abs (pp - b), pc = std::abs (pp - c);
                          pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
                default: break;
            }
            row[i] = (uint8_t) (row[i] + pr);
        }
        std::memcpy (prev.data(), row, stride);
    }
    values.assign ((size_t) w * h, 0.0f);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const uint8_t* px = raw.data() + (size_t) y * stride + 1 + (size_t) x * bpp;
            float lum = 0;
            if (chans == 1 || chans == 2) lum = depth == 16 ? (float) ((px[0] << 8) | px[1]) / 65535.0f : (float) px[0] / 255.0f;
            else
            {
                float r, g, b;
                if (depth == 16) { r = (float) ((px[0] << 8) | px[1]) / 65535.0f; g = (float) ((px[2] << 8) | px[3]) / 65535.0f; b = (float) ((px[4] << 8) | px[5]) / 65535.0f; }
                else { r = px[0] / 255.0f; g = px[1] / 255.0f; b = px[2] / 255.0f; }
                lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            }
            values[(size_t) y * w + x] = lum * umax;
        }
    return true;
}

} // namespace png16
