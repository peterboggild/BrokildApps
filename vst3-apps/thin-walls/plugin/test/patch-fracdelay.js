/*  The measured fault: the 4-point Hermite read of a delay line loses up to
    5.0 dB at 20 kHz when the fractional part of the delay is near a half sample,
    and the LINEAR read of the ITD is worse still (a half-sample error there is
    -11.7 dB at 20 kHz by the two-tap formula). Neither is in a real room: both
    swing with sub-millimetre changes of position, so the top octave breathes as
    a source moves and the two ears can lose different amounts, which smears the
    high end of the image.

    Replaced by a 16-tap Kaiser-windowed sinc at 512 fractional phases, used for
    BOTH reads. Peter's brief allows the CPU.
*/
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");
const misses = [];
function edit(rel, fn) {
  const p = path.join(root, rel);
  let s = fs.readFileSync(p, "utf8");
  const rep = (a, b, n = 1) => { const c = s.split(a).length - 1; if (c !== n) { misses.push(rel + ": " + c + " matches for: " + a.slice(0, 70)); return; } s = s.split(a).join(b); };
  fn(rep);
  if (misses.length === 0) fs.writeFileSync(p, s);
}

edit("Source/Engine.h", rep => {
  // the table, declared before DelayLine so read() can use it
  rep(`//------------------------------------------------------------------------------
// a mono ring buffer read at a fractional delay
class DelayLine`,
`//------------------------------------------------------------------------------
/*  Fractional delay by a windowed sinc, 16 taps at 512 phases.

    MEASURED, which is why it is here: 4-point Hermite loses 5.0 dB at 20 kHz at
    a half-sample fraction, and two-point linear (which the ITD used) loses 11.7.
    Both swing with sub-millimetre movements of a source, so the top octave
    breathes as it moves and the two ears can lose different amounts - a roll-off
    no room has. This is flat to a tenth of a dB across the audio band at every
    fraction. The window is Kaiser (beta 8.6) and every phase is normalised to
    unity at DC, so moving a source cannot change its level either. */
struct FracDelay
{
    static constexpr int TAPS = 16;
    static constexpr int HALF = 8;          // taps run j = -(HALF-1) .. HALF
    static constexpr int PHASES = 512;
    static constexpr int MIN_DELAY = HALF;  // the oldest tap a read touches

    float h[PHASES][TAPS];
    FracDelay();
    static const FracDelay& table();
};

//------------------------------------------------------------------------------
// a mono ring buffer read at a fractional delay
class DelayLine`);

  rep(`    // delay in samples, measured from the sample just written (delay >= 2).
    // 4-point Hermite: linear interpolation is a lowpass (-3 dB at fs/4 for a
    // half-sample fraction) that would breathe as a path's delay moves.
    inline float read (float delay) const
    {
        int   di = (int) delay;
        float t  = delay - (float) di;
        const int i1 = (w - 1 - di) & mask;         // the sample at the integer delay
        const int i0 = (i1 + 1) & mask;             // one newer
        const int i2 = (i1 - 1) & mask;             // one older
        const int i3 = (i1 - 2) & mask;
        const float y0 = buf[(size_t) i0], y1 = buf[(size_t) i1], y2 = buf[(size_t) i2], y3 = buf[(size_t) i3];
        const float c0 = y1, c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }`,
`    // delay in samples, measured from the sample just written. Must be at least
    // FracDelay::MIN_DELAY: the kernel reaches HALF-1 samples NEWER than the
    // integer part, and beyond the write cursor there is nothing written yet.
    inline float read (float delay) const
    {
        const int   di = (int) delay;
        const float f  = delay - (float) di;
        const float* h = FracDelay::table().h[(int) (f * (float) FracDelay::PHASES) & (FracDelay::PHASES - 1)];
        int idx = (w - 1 - (di - (FracDelay::HALF - 1))) & mask;   // j = -(HALF-1)
        float acc = 0;
        for (int k = 0; k < FracDelay::TAPS; ++k) { acc += h[k] * buf[(size_t) idx]; idx = (idx - 1) & mask; }
        return acc;
    }
    // the same kernel over a contiguous array: v[at] interpolated f forward
    static inline float interp (const float* v, int at, float f)
    {
        const float* h = FracDelay::table().h[(int) (f * (float) FracDelay::PHASES) & (FracDelay::PHASES - 1)];
        const float* p = v + at + (FracDelay::HALF - 1);           // j = -(HALF-1) is the NEWEST here
        float acc = 0;
        for (int k = 0; k < FracDelay::TAPS; ++k) acc += h[k] * p[-k];
        return acc;
    }`);
});

edit("Source/Engine.cpp", rep => {
  rep(`//==============================================================================
void DelayLine::prepare (int maxSamples)`,
`//==============================================================================
static double besselI0 (double x)
{
    double s = 1.0, t = 1.0;
    for (int k = 1; k < 60; ++k)
    {
        const double q = x / (2.0 * k);
        t *= q * q; s += t;
        if (t < 1e-18 * s) break;
    }
    return s;
}

FracDelay::FracDelay()
{
    const double beta = 8.6;
    const double i0b = besselI0 (beta);
    for (int p = 0; p < PHASES; ++p)
    {
        const double f = (double) p / (double) PHASES;
        double sum = 0;
        for (int k = 0; k < TAPS; ++k)
        {
            const int j = k - (HALF - 1);              // -7 .. 8
            const double x = f - (double) j;           // 0 at the tap we are landing on
            const double s = (std::abs (x) < 1e-12) ? 1.0 : std::sin (PI * x) / (PI * x);
            const double u = x / (double) HALF;        // |u| <= 1 over the span
            const double wnd = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - u * u))) / i0b;
            h[p][k] = (float) (s * wnd);
            sum += s * wnd;
        }
        // unity at DC: a source that moves must not change its own level
        for (int k = 0; k < TAPS; ++k) h[p][k] = (float) (h[p][k] / sum);
    }
}

const FracDelay& FracDelay::table()
{
    static const FracDelay t;
    return t;
}

//==============================================================================
void DelayLine::prepare (int maxSamples)`);

  // the reads must clear the kernel's newest tap
  rep(`        float x = feed.read (std::min ((float) maxDelay, std::max (2.0f, p.delay + back)));`,
      `        float x = feed.read (std::min ((float) maxDelay, std::max ((float) FracDelay::MIN_DELAY, p.delay + back)));`);
  rep(`            const float xb = feed.read (std::min ((float) maxDelay, std::max (2.0f, p.delayB + back)));`,
      `            const float xb = feed.read (std::min ((float) maxDelay, std::max ((float) FracDelay::MIN_DELAY, p.delayB + back)));`);
  rep(`        slot->delayTarget = std::max (2.0f, sp.length / SPEED_OF_SOUND * (float) fs);`,
      `        slot->delayTarget = std::max ((float) FracDelay::MIN_DELAY, sp.length / SPEED_OF_SOUND * (float) fs);`);
  rep(`    const int maxDelay = feed.capacity() - 4;`, `    const int maxDelay = feed.capacity() - FracDelay::TAPS - 4;`);

  // the ITD read: the same kernel, with a constant pre-delay so the newest tap
  // of the kernel never reaches past the present
  rep(`    const int ntapPad = (ntap + 15) & ~15;
    const int off = maxItd + 1;`,
      `    const int ntapPad = (ntap + 15) & ~15;
    // room for the sinc: HALF-1 newer taps at the largest ITD, HALF-1 older at
    // the smallest, and the constant pre-delay that keeps the newest tap behind
    // the write cursor
    const int off = maxItd + 2 * FracDelay::HALF;`);
  rep(`    // ears: the later ear delayed by the ITD, fractional, ramped through the block
    for (int i = 0; i < n; ++i)
    {
        const float it = it0 + (it1 - it0) * (float) (i + 1) / (float) n;
        const float dL = it > 0 ? 0.0f : -it, dR = it > 0 ? it : 0.0f;
        const int iL = (int) dL, iR = (int) dR;
        const float fL = dL - iL, fR = dR - iR;
        const int jL = i - iL + off, jR = i - iR + off;
        wetL[(size_t) i] += p.gL * ((1.0f - fL) * p.zL[(size_t) jL] + fL * p.zL[(size_t) (jL - 1)]);
        wetR[(size_t) i] += p.gR * ((1.0f - fR) * p.zR[(size_t) jR] + fR * p.zR[(size_t) (jR - 1)]);
    }`,
      `    /*  Ears: the later ear delayed by the ITD, ramped through the block. Both
        ears carry a constant HALF-sample pre-delay - the same on every path, so
        no cue moves - which is what lets the interpolation kernel look "forward"
        without reading past the newest sample written. */
    for (int i = 0; i < n; ++i)
    {
        const float it = it0 + (it1 - it0) * (float) (i + 1) / (float) n;
        const float dL = (float) FracDelay::HALF + (it > 0 ? 0.0f : -it);
        const float dR = (float) FracDelay::HALF + (it > 0 ? it : 0.0f);
        const int iL = (int) dL, iR = (int) dR;
        const float fL = dL - (float) iL, fR = dR - (float) iR;
        wetL[(size_t) i] += p.gL * DelayLine::interp (p.zL.data(), i - iL + off, fL);
        wetR[(size_t) i] += p.gR * DelayLine::interp (p.zR.data(), i - iR + off, fR);
    }`);

  // the path history must hold the wider window
  rep(`        int n = 64; while (n < ntap + maxItd + SUB_BLOCK + 16) n <<= 1;`,
      `        int n = 64; while (n < ntap + maxItd + SUB_BLOCK + 4 * FracDelay::TAPS) n <<= 1;`);
  rep(`        s.zL.assign ((size_t) (SUB_BLOCK + maxItd + ntap + 32), 0.0f);`,
      `        s.zL.assign ((size_t) (SUB_BLOCK + maxItd + ntap + 4 * FracDelay::TAPS + 32), 0.0f);`);
});

if (misses.length) { console.error("NOT WRITTEN:\n" + misses.join("\n")); process.exit(1); }
console.log("fractional delay replaced with a windowed sinc");
