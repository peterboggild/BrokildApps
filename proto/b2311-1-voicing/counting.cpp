/*  counting.cpp — the B2311.1 mechanism in continuous time, so that its
    thesis can be tested rather than asserted.
    ==========================================================================

    THE COMPLAINT THIS ANSWERS

    Peter, having listened to the shipped object: "the sounds are relatively
    uniformly tiny noise pops". He is right, and the reason is structural. The
    design document claims that rhythm, pitch and colour are one quantity — the
    rate of events — heard at three scales. The shipped instrument only ever
    occupies the first of the three, for three reasons that compound:

      * unit rates top out around 40 Hz, so the pitch scale is unreachable;
      * a cascade resolves inside a fraction of one lattice step (about 1.1 ms)
        whether it involves five units or all 9216, so a large event is not
        LONGER than a small one, only denser — which is a click either way;
      * one two-pole around 1-3 kHz shapes every event identically, so 256
        specimens differ in timing and barely at all in tone.

    WHAT IS DIFFERENT HERE

    1. CONTINUOUS TIME. The shipped engine advances a lattice in fixed steps
       and resolves each cascade inside one of them. That step is the reason
       every event is the same length, and no amount of parameter range fixes
       it. Here a unit's firing is an instant with a real time attached, held
       in a queue, and the sample grid is only where the sound is finally
       written down. Rates may then go as high as the mechanism can afford
       rather than as high as a step rate allows.

    2. CONDUCTION HAS A SPEED. A firing does not shove its neighbours at the
       same instant; the shove arrives one delay later. This is the change that
       makes size and duration the same quantity: a cascade that reaches d hops
       away takes d delays to do it, so a whole-lattice discharge unfolds over
       a hundred milliseconds and a small one stays a tick. Nobody chooses the
       lengths — the same claim the original made about cascade SIZE now also
       holds for cascade DURATION.

    3. A KERNEL PER UNIT. A unit's voice is its position in the unseen axes,
       so a cascade travelling across the body sweeps colour as it goes, and a
       large event does not merely last longer, it CHANGES while it happens.
       Implemented as a bank of resonators with units mapped into it, which
       costs the bank rather than 9216 filters.

    4. THE PARITY SIGN IS A SWITCH, NOT A LAW. It is suspected of cancelling
       exactly the coherent structure that would give a cascade a note. That is
       measurable, so it is measured (--parity 0|1) rather than argued about.

    Build:  g++ -O2 -o counting counting.cpp -lm
    Run:    ./counting --help
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <queue>
#include <algorithm>
#include <ctime>

// ---------------------------------------------------------------------------
//  The body: 32 x 32 seen, 3 x 3 unseen. 9216 units, eight neighbours each,
//  which is the four-dimensional von Neumann neighbourhood the design uses.
//  The seen face is what the panel draws as 1024 dials.
// ---------------------------------------------------------------------------
static const int NX = 32, NY = 32, NZ = 3, NW = 3;
static const int N  = NX * NY * NZ * NW;

static inline int idx (int x, int y, int z, int w) {
    return ((w * NZ + z) * NY + y) * NX + x;
}

struct Params {
    // THE COUNTING
    //  These defaults are CALIBRATED, not guessed: they put the prototype in
    //  the regime the shipped object was measured in — around 11 000 firings a
    //  second free-running, cascades running from a single unit to all 9216,
    //  and most of the take silent. See the head of ARTEFACT-B2311-1-VOICING.md
    //  for what that calibration is and is not evidence of.
    double rateLo   = 0.02;     // Hz, the slowest unit
    double rateHi   = 1.5;      // Hz, the fastest
    double couple   = 0.40;     // shove strength, in the concave state variable
    double dead     = 0.004;    // s, refractory: a unit ignores shoves after firing
    double leak     = 2.2;      // concavity of the rise; 0 is linear and cannot entrain
    double speed    = 0.0;      // s per hop of conduction. 0 = the shipped behaviour

    // THE IMPOSED PULSE
    double pulseHz  = 0.0;      // 0 = free running
    double grip     = 0.20;     // how hard the pulse shoves
    double reach    = 0.25;     // fraction of the body the pulse reaches

    // THE BODY
    double kernelLo = 1000.0;   // Hz, lowest kernel — SHIPPED range is ~100..3k
    double kernelHi = 3000.0;
    double damp     = 0.006;    // resonator decay time constant, seconds
    double sat      = 1.0;
    int    parity   = 1;        // the sign flip: 1 = as shipped, 0 = off

    double seconds  = 8.0;
    int    specimen = 0;
    int    sr       = 48000;
    int    bins     = 48;       // resolution of the kernel bank
    std::string wav;
};

// ---------------------------------------------------------------------------
//  Mirollo-Strogatz. A unit's phase rises linearly in time; its STATE is a
//  concave function of that phase, and a shove adds to the state. Concavity is
//  the whole reason a population of these pulls together, and therefore the
//  reason the object leans towards an imposed pulse at all: with leak = 0 the
//  map is linear and nothing entrains.
// ---------------------------------------------------------------------------
struct Rise {
    double b, eb1;
    void set (double bb) { b = bb < 1e-6 ? 1e-6 : bb; eb1 = std::expm1(b); }
    double f    (double phi) const { return std::log1p(eb1 * phi) / b; }
    double finv (double u)   const { return std::expm1(b * u) / eb1; }
};

// ---------------------------------------------------------------------------
//  An indexed binary heap over each unit's next firing time. Indexed because a
//  shove moves one known unit's key and we must not search for it: at audio
//  rates this queue turns over millions of times a second and a linear scan
//  would decide the architecture all by itself.
// ---------------------------------------------------------------------------
struct FireHeap {
    std::vector<int>    heap, pos;
    std::vector<double> key;

    void init (int n, const std::vector<double>& k) {
        key = k; heap.resize(n); pos.resize(n);
        for (int i = 0; i < n; i++) heap[i] = i;
        std::sort(heap.begin(), heap.end(), [&](int a, int b){ return key[a] < key[b]; });
        for (int i = 0; i < n; i++) pos[heap[i]] = i;
    }
    void swapAt (int i, int j) {
        std::swap(heap[i], heap[j]); pos[heap[i]] = i; pos[heap[j]] = j;
    }
    void up (int i) {
        while (i > 0) { int p = (i - 1) / 2;
            if (key[heap[p]] <= key[heap[i]]) break; swapAt(i, p); i = p; }
    }
    void down (int i) {
        int n = (int) heap.size();
        for (;;) { int l = 2*i+1, r = l+1, m = i;
            if (l < n && key[heap[l]] < key[heap[m]]) m = l;
            if (r < n && key[heap[r]] < key[heap[m]]) m = r;
            if (m == i) break; swapAt(i, m); i = m; }
    }
    void set (int u, double k) {
        double old = key[u]; key[u] = k;
        if (k < old) up(pos[u]); else if (k > old) down(pos[u]);
    }
    int    top    () const { return heap[0]; }
    double topKey () const { return key[heap[0]]; }
};

//  A shove in flight: conduction has a speed, so it arrives later than it left.
struct Shove {
    double t; int target; uint32_t cascade;
    bool operator< (const Shove& o) const { return t > o.t; }   // min-heap
};

struct Cascade { double t0, t1; uint32_t size; };

// ---------------------------------------------------------------------------
//  Two poles, struck. The impulse response is a decaying sinusoid, which is
//  what a kernel IS here: a unit does not oscillate, it is hit, and this is
//  the shape of the hit. Gain is normalised by sin(w0) so a low kernel is not
//  simply louder than a high one — without that the deep register wins every
//  measurement for the wrong reason.
// ---------------------------------------------------------------------------
struct TwoPole {
    double a1 = 0, a2 = 0, g = 1, y1 = 0, y2 = 0;
    void set (double hz, double tau, double sr) {
        double w = 2.0 * M_PI * hz / sr;
        double r = std::exp(-1.0 / (tau * sr));
        if (r > 0.99999) r = 0.99999;
        a1 = 2.0 * r * std::cos(w); a2 = -r * r; g = std::sin(w);
    }
    inline double step (double x) {
        double y = g * x + a1 * y1 + a2 * y2;
        y2 = y1; y1 = y; return y;
    }
};

// ---------------------------------------------------------------------------
//  A specimen is a seed. Everything a specimen owns — the spread of its rates,
//  where each unit sits in the kernel bank, its amplitude and its pan — comes
//  out of this and nothing else, so a specimen number reproduces exactly.
// ---------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    explicit Rng (uint64_t seed) : s(seed * 6364136223846793005ULL + 1442695040888963407ULL) {}
    uint32_t u32 () { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return (uint32_t)((s * 2685821657736338717ULL) >> 32); }
    double   uni () { return u32() / 4294967296.0; }
};

static void writeWav (const std::string& path, const std::vector<float>& l,
                      const std::vector<float>& r, int sr)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); return; }
    uint32_t frames = (uint32_t) l.size(), dataBytes = frames * 4;
    auto u32 = [&](uint32_t v){ fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v){ fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); u32(36 + dataBytes); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(2); u32((uint32_t)sr);
    u32((uint32_t)sr * 4); u16(4); u16(16);
    fwrite("data", 1, 4, f); u32(dataBytes);
    for (uint32_t i = 0; i < frames; i++) {
        auto q = [](float v){ v = v < -1 ? -1 : (v > 1 ? 1 : v); return (int16_t) lrintf(v * 32767.0f); };
        int16_t a = q(l[i]), b = q(r[i]); fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
    }
    fclose(f);
}

//  Radix-2, in place. Only wanted so the pitch claim can be checked against a
//  spectrum rather than against an opinion.
static void fft (std::vector<double>& re, std::vector<double>& im)
{
    const int n = (int) re.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        double wr = std::cos(ang), wi = std::sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                double ur = re[i+k],           ui = im[i+k];
                double vr = re[i+k+len/2]*cr - im[i+k+len/2]*ci;
                double vi = re[i+k+len/2]*ci + im[i+k+len/2]*cr;
                re[i+k] = ur + vr;       im[i+k] = ui + vi;
                re[i+k+len/2] = ur - vr; im[i+k+len/2] = ui - vi;
                double ncr = cr*wr - ci*wi; ci = cr*wi + ci*wr; cr = ncr;
            }
        }
    }
}

struct Result {
    double firingsPerSec = 0, cascadesPerSec = 0;
    double medianSize = 0, meanSize = 0; uint32_t maxSize = 0;
    double medianDurMs = 0, maxDurMs = 0, durOfLargestMs = 0;
    //  duration by size band: 1, 2-9, 10-99, 100-999, 1000+
    double bandDur[5] = {0,0,0,0,0}; long bandN[5] = {0,0,0,0,0};
    double silentFraction = 0, peak = 0, rms = 0, crest = 0;
    double R = 0, Rfirst = 0, Rlast = 0;
    double centroidHz = 0;
    double eventPeakHz = 0, eventProminence = 0;   // of the POINT PROCESS itself
    double eventsPerSec = 0, realtimeFactor = 0;
    uint64_t events = 0;
};

static Result run (const Params& P, std::vector<float>* outL = nullptr,
                                    std::vector<float>* outR = nullptr)
{
    Result R{};
    Rise rise; rise.set(P.leak);

    //  THE ONE LAW THIS PROTOTYPE FOUND. Conduction with a delay closes a loop:
    //  a shove leaves, travels, and something comes back. If a unit is ready to
    //  fire again before the return arrives, the body never stops — it stops
    //  being an instrument and becomes a boil, at a million and a half firings
    //  a second. Measured, the threshold is 3.05 +/- 0.05 conduction delays,
    //  and it sits in the same place for delays of 1, 2 and 4 ms — so it is a
    //  property of the lattice's loops, not of any one setting. The refractory
    //  is therefore NOT an independent control: it has a floor, and the floor
    //  is set by the conduction speed.
    const double dead = std::max(P.dead, 3.5 * P.speed);
    Rng rng(P.specimen * 2654435761u + 12345u);

    // ---- the body -------------------------------------------------------
    std::vector<double> rate(N), tNext(N), refracEnd(N, -1.0);
    std::vector<int>    bin(N);
    std::vector<float>  amp(N), pan(N), sgn(N);

    //  Rates are spread logarithmically, so "several layers keeping their own
    //  timings" is a property of the body rather than a mode it can be put in.
    //  Neighbours in space are given neighbouring rates (a smooth field plus
    //  scatter), which is what makes the face band rather than fizz.
    std::vector<double> field(N);
    {
        double ph[4] = { rng.uni()*6.283, rng.uni()*6.283, rng.uni()*6.283, rng.uni()*6.283 };
        double kx = 1 + (int)(rng.uni()*3), ky = 1 + (int)(rng.uni()*3);
        for (int w = 0; w < NW; w++) for (int z = 0; z < NZ; z++)
        for (int y = 0; y < NY; y++) for (int x = 0; x < NX; x++) {
            double v = 0.5 + 0.25 * std::sin(2*M_PI*kx*x/NX + ph[0])
                            + 0.25 * std::sin(2*M_PI*ky*y/NY + ph[1])
                            + 0.15 * std::sin(2*M_PI*z/NZ + ph[2])
                            + 0.15 * std::sin(2*M_PI*w/NW + ph[3]);
            field[idx(x,y,z,w)] = v;
        }
    }
    const double lo = std::log(P.rateLo), hi = std::log(P.rateHi);
    for (int w = 0; w < NW; w++) for (int z = 0; z < NZ; z++)
    for (int y = 0; y < NY; y++) for (int x = 0; x < NX; x++) {
        int i = idx(x,y,z,w);
        double t = field[i] * 0.82 + rng.uni() * 0.18;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        rate[i] = std::exp(lo + (hi - lo) * t);

        //  THE KERNEL IS A PROPERTY OF THE UNIT. Led by the unseen axes — the
        //  two the section does not show — so two units the panel draws side
        //  by side can sound nothing alike, and a cascade crossing the body
        //  sweeps colour as it travels.
        double kq = (z + w * NZ) / (double)(NZ * NW - 1);
        kq = 0.72 * kq + 0.28 * t;                    // rate tilts it a little
        bin[i] = (int) std::lround(kq * (P.bins - 1));

        amp[i] = (float)(0.55 + 0.45 * ((z + 0.5) / NZ));
        pan[i] = (float)((x + 0.5) / NX * 2.0 - 1.0);
        sgn[i] = P.parity ? (float)(((x + y + z + w) & 1) ? -1.0 : 1.0) : 1.0f;

        tNext[i] = rng.uni() / rate[i];               // scattered, never in step
    }

    FireHeap fq; fq.init(N, tNext);
    std::priority_queue<Shove> sq;

    //  the pulse reaches a fixed, arbitrary subset — the same one every time,
    //  so a measurement can be repeated
    std::vector<int> reached;
    for (int i = 0; i < N; i++) if (rng.uni() < P.reach) reached.push_back(i);

    // ---- output ---------------------------------------------------------
    const int    sr      = P.sr;
    const size_t nFrames = (size_t)(P.seconds * sr);
    std::vector<std::vector<std::pair<double,float>>> hitsL(P.bins), hitsR(P.bins);
    //  The bare list of moments, with no voice on it at all. This is what the
    //  thesis is a claim about: if the COUNTING has become periodic at an audio
    //  rate then the object has crossed from rhythm into pitch. Measuring the
    //  finished audio instead would only rediscover the resonator's own note,
    //  which is there whatever the counting does.
    std::vector<float> train(nFrames, 0.0f);

    std::vector<Cascade> casc;
    std::vector<double>  fireTimes;             // for R, subsampled
    uint64_t events = 0, firings = 0;

    clock_t cpu0 = clock();
    double t = 0.0;
    double nextBeat = (P.pulseHz > 0) ? 0.0 : 1e30;
    const double beatDt = (P.pulseHz > 0) ? 1.0 / P.pulseHz : 1e30;

    //  One firing, wherever it came from. Returns the shoves it sends.
    auto fire = [&](int u, double now, uint32_t cid) {
        firings++;
        if (cid < casc.size()) { casc[cid].size++; casc[cid].t1 = now; }
        if ((firings & 3) == 0) fireTimes.push_back(now);

        //  where the moment lands, and how loud, and where
        double a = amp[u] * sgn[u];
        double p = pan[u];
        float gl = (float)(a * 0.5 * (1.0 - p)), gr = (float)(a * 0.5 * (1.0 + p));
        double s = now * sr;
        if (s >= 0 && s < (double) nFrames - 1) {
            hitsL[bin[u]].push_back({ s, gl });
            hitsR[bin[u]].push_back({ s, gr });
            size_t si = (size_t) s; float fr = (float)(s - si);
            train[si] += 1.0f - fr; train[si+1] += fr;
        }

        refracEnd[u] = now + dead;
        fq.set(u, now + 1.0 / rate[u]);

        int x =  u % NX, y = (u / NX) % NY, z = (u / (NX*NY)) % NZ, w = u / (NX*NY*NZ);
        const int dx[8] = {1,-1,0,0,0,0,0,0}, dy[8] = {0,0,1,-1,0,0,0,0};
        const int dz[8] = {0,0,0,0,1,-1,0,0}, dw[8] = {0,0,0,0,0,0,1,-1};
        for (int k = 0; k < 8; k++) {
            int nx = (x+dx[k]+NX)%NX, ny = (y+dy[k]+NY)%NY;
            int nz = (z+dz[k]+NZ)%NZ, nw = (w+dw[k]+NW)%NW;
            sq.push({ now + P.speed, idx(nx,ny,nz,nw), cid });
        }
    };

    //  A shove landing on a unit. Either it carries the unit over — and the
    //  cascade continues, at this instant plus one more conduction delay — or
    //  it moves it closer and the queue is told.
    auto shove = [&](int u, double now, double eps, uint32_t cid) {
        if (now < refracEnd[u]) return;
        double phi = 1.0 - (fq.key[u] - now) * rate[u];
        if (phi < 0.0) phi = 0.0;
        double uu = rise.f(phi) + eps;
        if (uu >= 1.0) fire(u, now, cid);
        else fq.set(u, now + (1.0 - rise.finv(uu)) / rate[u]);
    };

    while (t < P.seconds) {
        double tf = fq.topKey();
        double ts = sq.empty() ? 1e30 : sq.top().t;
        double tb = nextBeat;
        double tn = std::min(std::min(tf, ts), tb);
        if (tn >= P.seconds) break;
        t = tn; events++;

        if (tb <= tf && tb <= ts) {
            //  THE IMPOSED PULSE. Not a sound — a shove, delivered to part of
            //  the body, which is the only way anything outside reaches it.
            nextBeat += beatDt;
            for (int u : reached) shove(u, t, P.grip, 0xFFFFFFFFu);
        } else if (ts <= tf) {
            Shove s = sq.top(); sq.pop();
            shove(s.target, s.t, P.couple, s.cascade);
        } else {
            //  a unit reaching its own top, unaided: a NEW cascade starts here
            int u = fq.top();
            uint32_t cid = (uint32_t) casc.size();
            casc.push_back({ t, t, 0 });
            fire(u, t, cid);
        }
    }
    double cpuSec = (double)(clock() - cpu0) / CLOCKS_PER_SEC;

    // ---- the sound ------------------------------------------------------
    std::vector<float> L(nFrames, 0.0f), Rr(nFrames, 0.0f);
    for (int b = 0; b < P.bins; b++) {
        if (hitsL[b].empty()) continue;
        double q  = P.bins > 1 ? b / (double)(P.bins - 1) : 0.0;
        double hz = P.kernelLo * std::pow(P.kernelHi / P.kernelLo, q);
        //  a low kernel rings longer than a high one, in proportion — a struck
        //  body does, and a fixed decay makes every register sound the same age
        double tau = P.damp * std::pow(P.kernelLo * 4.0 / hz, 0.5);

        std::vector<float> x(nFrames, 0.0f);
        for (auto& h : hitsL[b]) {
            size_t i = (size_t) h.first; float fr = (float)(h.first - i);
            x[i] += h.second * (1.0f - fr); x[i+1] += h.second * fr;
        }
        TwoPole f; f.set(hz, tau, sr);
        for (size_t i = 0; i < nFrames; i++) L[i] += (float) f.step(x[i]);

        std::fill(x.begin(), x.end(), 0.0f);
        for (auto& h : hitsR[b]) {
            size_t i = (size_t) h.first; float fr = (float)(h.first - i);
            x[i] += h.second * (1.0f - fr); x[i+1] += h.second * fr;
        }
        TwoPole g; g.set(hz, tau, sr);
        for (size_t i = 0; i < nFrames; i++) Rr[i] += (float) g.step(x[i]);
    }

    std::vector<float> rawL = L;                //  before any gain is applied
    double peak = 0;
    for (size_t i = 0; i < nFrames; i++) peak = std::max(peak, (double) std::max(std::fabs(L[i]), std::fabs(Rr[i])));
    double norm = peak > 1e-9 ? 0.89 / peak : 0.0;
    for (size_t i = 0; i < nFrames; i++) {
        L[i]  = (float) std::tanh(L[i]  * norm * P.sat) / (float) std::tanh(P.sat);
        Rr[i] = (float) std::tanh(Rr[i] * norm * P.sat) / (float) std::tanh(P.sat);
    }
    R.peak = peak;

    // ---- what it did ----------------------------------------------------
    R.events         = events;
    R.firingsPerSec  = firings / P.seconds;
    R.eventsPerSec   = events  / P.seconds;
    R.realtimeFactor = cpuSec > 0 ? P.seconds / cpuSec : 0;

    std::vector<double> sizes, durs;
    uint32_t big = 0; double bigDur = 0;
    for (auto& c : casc) {
        if (c.size == 0) continue;
        sizes.push_back(c.size);
        durs.push_back((c.t1 - c.t0) * 1000.0);
        if (c.size > big) { big = c.size; bigDur = (c.t1 - c.t0) * 1000.0; }
    }

    auto median = [](std::vector<double> v) {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    R.cascadesPerSec = sizes.size() / P.seconds;
    R.maxSize        = big;
    R.medianSize     = median(sizes);
    R.medianDurMs    = median(durs);
    R.durOfLargestMs = bigDur;
    for (double d : durs) R.maxDurMs = std::max(R.maxDurMs, d);
    double sum = 0; for (double v : sizes) sum += v;
    R.meanSize = sizes.empty() ? 0 : sum / sizes.size();

    //  THE CLAIM OF THE REBUILD, as a table. If duration does not climb with
    //  size these bands all read the same, which is exactly what the shipped
    //  object does: every event, from one unit to nine thousand, inside the
    //  same 1.1 ms.
    {
        std::vector<std::vector<double>> band(5);
        for (auto& c : casc) {
            if (c.size == 0) continue;
            int b = c.size == 1 ? 0 : c.size < 10 ? 1 : c.size < 100 ? 2 : c.size < 1000 ? 3 : 4;
            band[b].push_back((c.t1 - c.t0) * 1000.0);
        }
        for (int b = 0; b < 5; b++) { R.bandN[b] = (long) band[b].size(); R.bandDur[b] = median(band[b]); }
    }

    //  LEVEL AND SHAPE, measured BEFORE the normalisation, because the
    //  normalisation is what hides the difference between the two things the
    //  ear actually distinguishes here. Crest factor — peak against RMS — is
    //  the honest one: a take of uniform hash has a low crest, and a take of
    //  rare enormous events has a high one. "Tiny noise pops" is a complaint
    //  about a low crest, and no amount of gain fixes it.
    {
        double sum2 = 0;
        for (size_t i = 0; i < nFrames; i++) sum2 += rawL[i] * (double) rawL[i];
        R.rms   = std::sqrt(sum2 / nFrames);
        R.crest = R.rms > 1e-12 ? R.peak / R.rms : 0;
    }

    //  SILENCE, on 10 ms windows, against the take's own loud windows rather
    //  than against its single loudest sample — one enormous event otherwise
    //  drags every other window under a fixed threshold and the take reads as
    //  silent when it is merely uneven.
    {
        const size_t win = (size_t)(0.010 * sr);
        std::vector<double> wp;
        for (size_t i = 0; i + win < nFrames; i += win) {
            double m = 0;
            for (size_t k = 0; k < win; k++) m = std::max(m, (double) std::fabs(rawL[i+k]));
            wp.push_back(m);
        }
        if (!wp.empty()) {
            std::vector<double> sorted = wp;
            std::sort(sorted.begin(), sorted.end());
            double p95 = sorted[(size_t)(sorted.size() * 0.95)];
            double thr = p95 * 0.01;
            size_t quiet = 0;
            for (double m : wp) if (m < thr) quiet++;
            R.silentFraction = quiet / (double) wp.size();
        }
    }

    //  GATHERING ONTO THE BEAT. Every firing's phase against the imposed pulse,
    //  averaged as unit vectors. 0 is indifference, 1 is a drum machine. Split
    //  first third against last third, because the finding that matters is that
    //  the relationship BUILDS across a take.
    if (P.pulseHz > 0 && !fireTimes.empty()) {
        auto conc = [&](size_t a, size_t b) {
            double cr = 0, ci = 0; size_t n = 0;
            for (size_t i = a; i < b; i++) {
                double ph = std::fmod(fireTimes[i] * P.pulseHz, 1.0) * 2 * M_PI;
                cr += std::cos(ph); ci += std::sin(ph); n++;
            }
            return n ? std::sqrt(cr*cr + ci*ci) / n : 0.0;
        };
        size_t n = fireTimes.size();
        R.R      = conc(0, n);
        R.Rfirst = conc(0, n / 3);
        R.Rlast  = conc(n - n / 3, n);
    }

    //  THE SPECTRUM. Two of them, and the distinction is the whole point.
    //  The audio's centroid says where the voicing has put its energy. The
    //  POINT PROCESS's spectrum says whether the counting itself has become
    //  periodic — which is the thesis, and the only one of the two that can
    //  tell rhythm from pitch. Prominence is the peak against the median bin:
    //  a periodic train stands far above it, a random one not at all.
    {
        const int NFFT = 1 << 15;
        if (nFrames > (size_t) NFFT) {
            size_t off = nFrames / 2;
            std::vector<double> re(NFFT), im(NFFT, 0.0);
            for (int i = 0; i < NFFT; i++) {
                double wnd = 0.5 - 0.5 * std::cos(2 * M_PI * i / (NFFT - 1));
                re[i] = 0.5 * (L[off + i] + Rr[off + i]) * wnd;
            }
            fft(re, im);
            double num = 0, den = 0;
            for (int i = 1; i < NFFT / 2; i++) {
                double m = std::sqrt(re[i]*re[i] + im[i]*im[i]);
                num += i * (double) sr / NFFT * m; den += m;
            }
            R.centroidHz = den > 0 ? num / den : 0;

            std::fill(im.begin(), im.end(), 0.0);
            double mean = 0;
            for (int i = 0; i < NFFT; i++) mean += train[off + i];
            mean /= NFFT;
            for (int i = 0; i < NFFT; i++) {
                double wnd = 0.5 - 0.5 * std::cos(2 * M_PI * i / (NFFT - 1));
                re[i] = (train[off + i] - mean) * wnd;   // DC out, or it wins
            }
            fft(re, im);
            const int lo = (int) std::ceil(30.0 * NFFT / sr);
            std::vector<double> mag(NFFT / 2, 0.0);
            int best = lo; double bv = 0;
            for (int i = lo; i < NFFT / 2; i++) {
                mag[i] = std::sqrt(re[i]*re[i] + im[i]*im[i]);
                if (mag[i] > bv) { bv = mag[i]; best = i; }
            }
            R.eventPeakHz = best * (double) sr / NFFT;
            std::vector<double> sorted(mag.begin() + lo, mag.end());
            std::sort(sorted.begin(), sorted.end());
            double med = sorted.empty() ? 0 : sorted[sorted.size() / 2];
            R.eventProminence = med > 1e-12 ? bv / med : 0;
        }
    }

    if (outL) *outL = L;
    if (outR) *outR = Rr;
    return R;
}

// ---------------------------------------------------------------------------

static void report (const char* label, const Params& P, const Result& R)
{
    printf("%-26s  firings/s %8.0f   cascades/s %7.0f   median %5.0f  max %5u\n",
           label, R.firingsPerSec, R.cascadesPerSec, R.medianSize, R.maxSize);
    printf("%-26s  cascade ms: median %6.2f  longest %7.2f  of the largest %7.2f\n",
           "", R.medianDurMs, R.maxDurMs, R.durOfLargestMs);
    {
        static const char* bn[5] = { "1 unit", "2-9", "10-99", "100-999", "1000+" };
        printf("%-26s  duration by size: ", "");
        for (int b = 0; b < 5; b++)
            if (R.bandN[b]) printf("%s %.1f ms (n=%ld)  ", bn[b], R.bandDur[b], R.bandN[b]);
        printf("\n");
    }
    printf("%-26s  silent %5.1f %%   crest %6.1f   centroid %6.0f Hz   raw peak %9.2f\n",
           "", R.silentFraction * 100, R.crest, R.centroidHz, R.peak);
    printf("%-26s  the counting itself: strongest period %7.1f Hz, %6.1f x the median bin\n",
           "", R.eventPeakHz, R.eventProminence);
    if (P.pulseHz > 0)
        printf("%-26s  gathering R %5.3f   first third %5.3f -> last third %5.3f\n",
               "", R.R, R.Rfirst, R.Rlast);
    printf("%-26s  cost: %.2f M events/s, %5.1f x realtime on one core\n\n",
           "", R.eventsPerSec / 1e6, R.realtimeFactor);
}

static bool arg (int c, char** v, const char* name, double& out)
{
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], name)) { out = atof(v[i+1]); return true; }
    return false;
}
static bool argS (int c, char** v, const char* name, std::string& out)
{
    for (int i = 1; i < c - 1; i++) if (!strcmp(v[i], name)) { out = v[i+1]; return true; }
    return false;
}
static bool has (int c, char** v, const char* name)
{
    for (int i = 1; i < c; i++) if (!strcmp(v[i], name)) return true;
    return false;
}

int main (int argc, char** argv)
{
    if (has(argc, argv, "--help")) {
        printf(
        "counting — the B2311.1 mechanism in continuous time\n\n"
        "  --seconds N     --specimen N   --sr N        --bins N\n"
        "  --ratelo HZ     --ratehi HZ    --couple X    --dead S     --leak X\n"
        "  --speed S       seconds per hop of conduction; 0 is the shipped behaviour\n"
        "  --pulse HZ      --grip X       --reach X\n"
        "  --kernello HZ   --kernelhi HZ  --damp S      --sat X      --parity 0|1\n"
        "  --wav FILE      write the take\n\n"
        "  --suite         run every measurement this prototype exists to make\n");
        return 0;
    }

    Params P;
    arg(argc, argv, "--seconds", P.seconds);   arg(argc, argv, "--couple", P.couple);
    arg(argc, argv, "--ratelo",  P.rateLo);    arg(argc, argv, "--ratehi", P.rateHi);
    arg(argc, argv, "--dead",    P.dead);      arg(argc, argv, "--leak",   P.leak);
    arg(argc, argv, "--speed",   P.speed);     arg(argc, argv, "--pulse",  P.pulseHz);
    arg(argc, argv, "--grip",    P.grip);      arg(argc, argv, "--reach",  P.reach);
    arg(argc, argv, "--kernello",P.kernelLo);  arg(argc, argv, "--kernelhi",P.kernelHi);
    arg(argc, argv, "--damp",    P.damp);      arg(argc, argv, "--sat",    P.sat);
    double d;
    if (arg(argc, argv, "--specimen", d)) P.specimen = (int) d;
    if (arg(argc, argv, "--sr",       d)) P.sr       = (int) d;
    if (arg(argc, argv, "--bins",     d)) P.bins     = (int) d;
    if (arg(argc, argv, "--parity",   d)) P.parity   = (int) d;
    argS(argc, argv, "--wav", P.wav);

    if (!has(argc, argv, "--suite")) {
        std::vector<float> L, R2;
        Result R = run(P, &L, &R2);
        report("take", P, R);
        if (!P.wav.empty()) { writeWav(P.wav, L, R2, P.sr); printf("wrote %s\n", P.wav.c_str()); }
        return 0;
    }

    // -----------------------------------------------------------------------
    //  The suite. Each block answers one of the four complaints, and the
    //  comparison in each is against the SHIPPED setting, not against nothing.
    // -----------------------------------------------------------------------
    printf("\n==== 1. DOES CASCADE DURATION FOLLOW CASCADE SIZE? ====\n");
    printf("Conduction delay per hop. At 0 a cascade resolves in an instant, which\n"
           "is the shipped behaviour and the reason every event is the same length.\n\n");
    for (double sp : { 0.0, 0.0005, 0.002, 0.006 }) {
        Params Q = P; Q.speed = sp; Q.seconds = 8;
        char lab[64]; snprintf(lab, sizeof lab, "hop %.1f ms", sp * 1000);
        report(lab, Q, run(Q));
    }

    printf("\n==== 2. DOES THE RATE RANGE REACH PITCH? ====\n");
    printf("The shipped ceiling is 40 Hz. A tone needs the body's events to recur\n"
           "hundreds of times a second, so this is the claim the thesis rests on.\n\n");
    for (double rh : { 40.0, 120.0, 400.0, 1200.0, 3000.0 }) {
        Params Q = P; Q.rateHi = rh; Q.speed = 0.002; Q.seconds = 4;
        Q.kernelLo = 60; Q.kernelHi = 4000;
        char lab[64]; snprintf(lab, sizeof lab, "rates to %.0f Hz", rh);
        report(lab, Q, run(Q));
    }

    printf("\n==== 3. THE PARITY SIGN FLIP ====\n");
    printf("Suspected of cancelling the coherent structure that would give a\n"
           "cascade a note. Same body, same seed, sign on and off.\n\n");
    for (int par : { 1, 0 }) {
        Params Q = P; Q.parity = par; Q.speed = 0.002; Q.rateHi = 400; Q.seconds = 4;
        Q.kernelLo = 60; Q.kernelHi = 4000;
        report(par ? "parity on (as shipped)" : "parity off", Q, run(Q));
    }

    printf("\n==== 4. THE DEEP REGISTER ====\n");
    printf("The shipped kernel range bottoms out near 100 Hz.\n\n");
    for (double kl : { 1000.0, 300.0, 90.0, 28.0 }) {
        Params Q = P; Q.kernelLo = kl; Q.kernelHi = std::max(3000.0, kl * 4);
        Q.speed = 0.002; Q.seconds = 6;
        char lab[64]; snprintf(lab, sizeof lab, "kernel from %.0f Hz", kl);
        report(lab, Q, run(Q));
    }

    printf("\n==== 5. IT CAN BE LED, AND NOT SHOVED ====\n");
    printf("The finding the shipped bench kept by being left failing. It has to\n"
           "survive the rebuild, or the rebuild has broken the object.\n\n");
    for (double g : { 0.05, 0.20, 0.55 }) {
        Params Q = P; Q.grip = g; Q.pulseHz = 3.0; Q.speed = 0.002; Q.seconds = 8;
        char lab[64]; snprintf(lab, sizeof lab, "grip %.2f", g);
        report(lab, Q, run(Q));
    }
    return 0;
}
