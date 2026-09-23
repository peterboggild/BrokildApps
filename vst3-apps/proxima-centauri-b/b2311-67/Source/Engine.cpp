#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace ax
{
int dbgMode = 0;   // 0 normal, 1 no resample, 2 no ground spring, 3 no material update

using ab::SQRT2; using ab::SILVER; using ab::PI; using ab::APO; using ab::PELL;

//==============================================================================
namespace
{
    inline float  clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
    inline double clampd (double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
    inline double xmapd (double t, double lo, double hi) { return lo * std::pow (hi / lo, clampd (t, 0.0, 1.0)); }

    // one shared sine table; 192 partials x 8 voices cannot afford std::sin
    constexpr int STAB = 4096;
    struct SineTable
    {
        float t[STAB + 1];
        SineTable() { for (int i = 0; i <= STAB; ++i) t[i] = (float) std::sin (2.0 * PI * (double) i / (double) STAB); }
    };
    const SineTable SINE;
    inline float fsin (float phase)                    // phase in turns, 0..1
    {
        float x = phase - std::floor (phase);
        const float f = x * (float) STAB;
        const int i = (int) f;
        const float a = f - (float) i;
        return SINE.t[i] + a * (SINE.t[i + 1] - SINE.t[i]);
    }

    inline uint32_t xr (uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float noise (uint32_t& s) { return (float) ((int32_t) xr (s)) * 4.6566129e-10f; }

    // the silver word of length 17: sigma(L)=LLS, sigma(S)=L, applied four
    // times. Twelve L and five S, in the ratio 1+sqrt2 — this tiling's own
    // sequence, and therefore the artefact's own scale.
    const int SILVER_WORD[GRADE] = { 0,0,1, 0,0,1, 0, 0,0,1, 0,0,1, 0, 0,0,1 };
}

//==============================================================================
// The parameter table. Everything walks this.
#define P(field) [] (Params& q) -> float& { return q.field; }

static const PSpec SPECS[] =
{
    // id          name              gloss (the collector's annotation)                      def   kind      lo     hi     accessor
    { "level",    "LEVEL",          "output",                                               0.62f, KP_VOL,   0,     0,     P(level) },
    { "voices",   "APERTURES",      "how many notes the body will hold at once",            4.0f,  KP_INT,   1,     8,     P(voicesN) },
    { "glide",    "GLIDE",          "time to travel between pitches",                       0.0f,  KP_PCT,   0,     0,     P(glide) },
    { "bendr",    "BEND RANGE",     "pitch wheel reach, in semitones",                      2.0f,  KP_INT,   0,     24,    P(bendRange) },

    { "habit",    "HABIT",          "which specimen: a colouring of the fourth dimension",  0.0f,  KP_INT,   0,     255,   P(habit) },
    { "temp",     "TEMPERATURE",    "how much the specimen's own wiring stirs it",          0.298755f, KP_KELVIN, 77, 800, P(temp) },

    { "travel",   "TRAVERSE",       "depth of the cut in w — the wheel drags the body through", 0.5f, KP_BIPOL, -12, 12,  P(travel) },
    { "bearing",  "BEARING",        "the heading of travel within the unseen plane",        0.0f,  KP_PCT,   0,     0,     P(bearing) },
    { "tilt4",    "OBLIQUITY",      "tilt of the cut against the lattice",                  0.0f,  KP_PCT,   0,     0,     P(tilt4) },

    { "cutang",   "CUT BEARING",    "which way the sounding line lies across the body",     0.0f,  KP_PCT,   0,     0,     P(cutang) },
    { "cutpos",   "CUT OFFSET",     "how far the sounding line is set from the centre",     0.5f,  KP_BIPOL, -8,    8,     P(cutpos) },
    { "extent",   "EXTENT",         "how much of the body sounds — a Pell number of sites", 3.0f,  KP_LIST,  0,     5,     P(extent) },

    { "aperture", "APERTURE",       "size of the acceptance window: how dense the matter",  0.5f,  KP_PCT,   0,     0,     P(aperture) },
    { "rim",      "RIM",            "how gradually matter leaves existence at the window",  0.22f, KP_PCT,   0,     0,     P(rim) },

    { "contrast", "CONTRAST",       "heavy-to-light mass ratio across the body",            0.45f, KP_PCT,   0,     0,     P(contrast) },
    { "bondlaw",  "BOND LAW",       "how much a short bond is stiffer than a long one",     0.5f,  KP_PCT,   0,     0,     P(bondlaw) },
    { "damp",     "LOSS",           "how fast the body forgets",                            0.30f, KP_PCT,   0,     0,     P(damp) },
    { "damptilt", "LOSS TILT",      "how much more the top of the spectrum is lost",        0.30f, KP_PCT,   0,     0,     P(damptilt) },
    { "nonlin",   "STRAIN",         "stiffening under displacement — the body bends hard",  0.12f, KP_PCT,   0,     0,     P(nonlin) },

    { "strike",   "INCIDENCE",      "broad and soft, or point and hard",                    0.40f, KP_PCT,   0,     0,     P(strike) },
    { "place",    "STATION",        "where along the cut a note enters the body",           0.5f,  KP_PCT,   0,     0,     P(place) },
    { "follow",   "WALK",           "how far up the body the keyboard walks",               0.35f, KP_PCT,   0,     0,     P(follow) },
    { "drive",    "SUSTAINED FORCE","continuous excitation while the note is held",         0.0f,  KP_PCT,   0,     0,     P(drive) },
    { "drivecol", "FORCE COLOUR",   "spectrum of that excitation",                          0.5f,  KP_PCT,   0,     0,     P(drivecol) },
    { "spread",   "SEPARATION",     "distance between the two listening points",            0.55f, KP_PCT,   0,     0,     P(spread) },

    { "blend",    "ASPECT",         "direct space (the chain) against reciprocal (the star)", 0.45f, KP_PCT, 0,     0,     P(blend) },
    { "peaks",    "ORDERS",         "how many diffraction orders are heard",                72.0f, KP_INT,   8,     192,   P(peaks) },
    { "starwid",  "WINDOW",         "width of the conjugate window: which orders survive",  0.42f, KP_PCT,   0,     0,     P(starwid) },
    { "startilt", "EXTINCTION",     "how fast a distant shadow is extinguished",            0.30f, KP_PCT,   0,     0,     P(startilt) },
    { "drift",    "PRECESSION",     "standing drift in w — each order detunes by its shadow", 0.0f, KP_PCT,   0,     0,     P(drift) },
    { "stardec",  "ORDER DECAY",    "how much faster a high order dies",                    0.45f, KP_PCT,   0,     0,     P(stardec) },

    { "attack",   "ONSET",          "how quickly force is applied",                         0.02f, KP_PCT,   0,     0,     P(attack) },
    { "decay",    "FALL",           "how the applied force falls back",                     0.55f, KP_PCT,   0,     0,     P(decay) },
    { "sustain",  "HOLD",           "force remaining while a note is held",                 0.35f, KP_PCT,   0,     0,     P(sustain) },
    { "release",  "QUENCH",         "extra loss applied once the note is let go",           0.30f, KP_PCT,   0,     0,     P(release) },

    { "conform",  "CONFORMANCE",    "the artefact's own scale, bent toward ours",           0.0f,  KP_PCT,   0,     0,     P(conform) },
    { "ref",      "REFERENCE",      "concert pitch, plus or minus fifty cents",             0.5f,  KP_CENT,  100,   0,     P(ref) },
    { "octave",   "REGISTER",       "transpose in octaves",                                 0.5f,  KP_SEMI,  0,     0,     P(octave) },

    { "cavity",   "CAVITY",         "how much every note hears the others in the one body", 0.18f, KP_PCT,   0,     0,     P(cavity) },
    { "cavtone",  "CAVITY COLOUR",  "colour of that shared coupling",                       0.5f,  KP_PCT,   0,     0,     P(cavtone) },
    { "sat",      "CEILING",        "the soft limit the artefact will not exceed",          0.35f, KP_PCT,   0,     0,     P(sat) },
    { "air",      "AIR",            "top-end lift on the way out",                          0.35f, KP_PCT,   0,     0,     P(air) },
};
#undef P

int          numParams()          { return (int) (sizeof (SPECS) / sizeof (SPECS[0])); }
const PSpec& paramSpec (int i)    { return SPECS[i]; }

float paramMax (const PSpec& s)
{
    if (s.kind == KP_INT)  return s.hi;
    if (s.kind == KP_LIST) return s.hi;
    if (s.kind == KP_SW)   return 1.0f;
    return 1.0f;
}

static const char* EXTENT_NAMES[6] = { "5", "12", "29", "70", "169", "408" };

const char* const* listNames (const char* id, int& n)
{
    if (std::strcmp (id, "extent") == 0) { n = 6; return EXTENT_NAMES; }
    n = 0; return nullptr;
}

//==============================================================================
// habit -> panel
namespace
{
    inline float inv_xmap (double v, double lo, double hi) { return (float) clampd (std::log (v / lo) / std::log (hi / lo), 0.0, 1.0); }
    inline float inv_lin  (double v, double lo, double hi) { return (float) clampd ((v - lo) / (hi - lo), 0.0, 1.0); }
}

int habitCount() { return ab::habitCount(); }

void applyHabit (int index, Params& p)
{
    const ab::Habit h = ab::habitOf (index);
    p.habit    = (float) index;
    p.contrast = inv_xmap (h.contrast, 1.0, 12.0);
    p.bondlaw  = inv_lin  (h.bondExp, -0.5, 3.0);
    p.damp     = inv_xmap (h.damp, 5.0e-5, 2.0e-2);
    p.damptilt = (float) clampd (h.dampTilt, 0.0, 1.0);
    p.nonlin   = (float) clampd (h.nonlin * 1.6, 0.0, 1.0);
    p.extent   = (float) h.extent;
    p.cutang   = (float) h.cutIndex / 8.0f;
    p.blend    = (float) h.blend;
    p.starwid  = inv_lin  (h.starWid, 0.2, 4.0);
    p.startilt = inv_lin  (h.starTilt, 0.0, 1.4);
    p.strike   = (float) h.strike;
    p.drive    = (float) h.drive;
}

/*  A habit is not named, it is catalogued. Its signature is a true structural
    invariant: the two strongest wavevectors of its field over the acceptance
    window, as an eightfold direction and an integer magnitude. */
void habitSignature (int index, int& gapA, int& gapB, float& hueA, float& hueB)
{
    const ab::Habit h = ab::habitOf (index);
    int i0 = 0, i1 = 1;
    if (h.waves >= 2 && h.kamp[1] > h.kamp[0]) { i0 = 1; i1 = 0; }
    gapA = (h.kdir[i0] & 7) * 10 + (int) std::lround (h.kmag[i0]);
    gapB = (h.kdir[i1] & 7) * 10 + (int) std::lround (h.kmag[i1]);
    // thin-film hue at the two ends of the field (see the shader for the real
    // reflectance; this is only for the catalogue swatch)
    hueA = (float) std::fmod (h.filmBase / 700.0, 1.0);
    hueB = (float) std::fmod (std::abs (h.filmBase + h.filmSpan) / 700.0, 1.0);
}

//==============================================================================
Engine::Engine()
{
    // build the grade: 17 unequal steps spanning the silver ratio
    const double lnS = std::log (SILVER);
    const double Sst = lnS / (12.0 * SILVER + 5.0);
    const double Lst = Sst * SILVER;
    double acc = 0.0;
    for (int i = 0; i < GRADE; ++i)
    {
        gradeCents[i] = acc * 1731.2340490667560888;      // ln -> cents
        acc += (SILVER_WORD[i] == 0 ? Lst : Sst);
    }
    gradeCents[GRADE] = acc * 1731.2340490667560888;      // == 1200*log2(silver)

    hab = ab::habitOf (0);
    reset();
}

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate > 8000.0 ? sampleRate : 48000.0;
    dyn.prepare (sr);
    reset();
    service();
}

void Engine::reset()
{
    for (auto& v : voices) { v = Voice(); v.rng = 0x2311u + (uint32_t) (&v - voices.data()) * 7919u; }
    cavZ = cavSum = 0.0f; meter = 0.0f; dyn.reset();
    for (auto& e : siteE) e = 0.0f;
}

//==============================================================================
float Engine::noteHz (int note) const
{
    const double refCents = (double) (p.ref - 0.5f) * 100.0;
    const double oct = std::round ((double) (p.octave - 0.5f) * 6.0) * 12.0;

    // the artefact's own grade: 17 unequal steps per silver octave, in the
    // silver word's order. CONFORMANCE bends it toward twelve equal steps.
    const double n = (double) note - 60.0;
    int step = (int) std::floor (n / (double) GRADE);
    int idx  = (int) (n - (double) step * (double) GRADE);
    if (idx < 0) { idx += GRADE; --step; }
    const double alien = (double) step * gradeCents[GRADE] + gradeCents[idx];
    const double human = n * 100.0;
    const double cents = alien + (human - alien) * (double) clampf (p.conform, 0.0f, 1.0f);

    const double semis = (double) bend * (double) std::round (p.bendRange);
    return (float) (440.0 * std::pow (2.0, (cents + refCents + oct) / 1200.0 + (semis - 9.0) / 12.0));
}

//==============================================================================
/*  Message-thread work. The body is rebuilt from the parameters: the window,
    the cut, the masses, the tuning anchor and the diffraction star. */
void Engine::service()
{
    const int hi = (int) std::lround (clampf (p.habit, 0.0f, 255.0f));
    if (hi != lastHabit)
    {
        hab = ab::habitOf (hi); lastHabit = hi;
        /*  EVERY SPECIMEN AT A WORKABLE LEVEL.

            The catalogue was not level. Habit 240 was under gain reduction in
            all 300 blocks of a three-second note, averaging 2.5 dB and
            reaching 7.3; habit 96 never touched the limiter at all. Twenty
            decibels apart, and the limiter riding that hard on a dense
            inharmonic signal is heard as fast clicks -- which is what Peter
            heard, on the lower-left swatch of the catalogue, which is 240.

            The trim is MEASURED, not modelled: every habit was rendered and
            its pre-limiter peak recorded, exactly the way Martian Gain
            measures its auto-gain instead of predicting it. It only ever
            reduces, so a quiet specimen stays quiet and keeps its character;
            it is applied at the output multiply, after every nonlinearity, so
            the calibration is exactly linear and one pass is enough. */
        habTrim = ab::habitTrim (hi);
    }

    // the habit's field is the material; the panel's knobs override its scalars
    hab.contrast = xmapd (p.contrast, 1.0, 12.0);
    hab.bondExp  = -0.5 + 3.5 * (double) p.bondlaw;
    hab.damp     = xmapd (p.damp, 5.0e-5, 2.0e-2);
    hab.dampTilt = p.damptilt;
    hab.nonlin   = 0.62 * (double) p.nonlin;
    hab.starWid  = 0.2 + 3.8 * (double) p.starwid;
    hab.starTilt = 1.4 * (double) p.startilt;

    // the fourth dimension: depth tau, along a bearing in the unseen plane
    /*  TRAVERSE is a host parameter and therefore lives on nought to one;
        its declared range of plus or minus twelve is what the panel and the
        host READ OUT. Taking the raw normalised value as the depth — which is
        what this did at first — drags the body one unit instead of
        twenty-four, so the wheel barely stirred it and travelling looked like
        it did nothing. The mapping has to be the same on both sides. */
    /*  THE SITE: cold and close, the section rocks through w in the bench's
        time — a quarter of a unit deep at full pull, which is enough to set
        the rim flipping and the star's shimmer breathing, and nothing like a
        traversal (the neighbours in the field are fourteen units away). The
        negotiated phase arrives from the processor every tick; at pull 0 the
        rock is exactly 0.0 and this line is the line it always was. */
    {
        const float sp = sitePull.load();
        siteTau = sp > 0.0f ? (double) sp * 0.25 * std::sin (6.283185307 * (double) sitePhaseIn.load()) : 0.0;
    }
    tau = ((double) p.travel - 0.5) * 24.0 + tauUi.load() + siteTau;
    const double bg = (double) p.bearing * 2.0 * PI;
    gx = tau * std::cos (bg);
    gy = tau * std::sin (bg);

    win.scale = 0.55 + 1.05 * (double) p.aperture;
    /*  A narrow rim is enough now that the energy rule handles the joins: a
        wide one puts a third of the chain into a damped transition and the
        filament goes quiet. */
    win.soft  = 0.01 + 0.16 * (double) p.rim;

    /*  ARRESTS. A pinned site must not be allowed to leave, so the window
        bulges to keep holding it — and the bulge brings other matter with it.
        The player deforms the four-dimensional body, and every slice of it,
        for ever after, is different. This is a pinned phason strain. */
    for (int j = 0; j < 8; ++j) win.bulge[j] = 0.0;

    /*  A pin can only be dragged so far. The bulge is capped — an uncapped one
        would swell the window until the tiling degenerated — so past a certain
        depth in w the strain exceeds what the body will carry and the pin TEARS
        LOOSE. That is right for a pinned phason strain, and it has to be
        visible: strain is published per pin so the panel can show it pulling,
        and the pin is released rather than silently failing to hold, which is
        what it did before and which made the whole gesture look inert. */
    const double cap = 2.0 * APO;
    arrestStrain.assign (arrests.size(), 0.0f);
    torn = false;
    for (size_t i = 0; i < arrests.size(); )
    {
        const double qx = arrests[i].first - gx, qy = arrests[i].second - gy;
        double worst = 0.0;
        for (int j = 0; j < 8; ++j)
        {
            double ux, uy; ab::Window::dir (j, ux, uy);
            const double need = qx * ux + qy * uy - APO * win.scale + 0.02;
            if (need > worst) worst = need;
        }
        if (worst > cap) { arrests.erase (arrests.begin() + (long) i); arrestStrain.erase (arrestStrain.begin() + (long) i); torn = true; continue; }
        arrestStrain[i] = (float) std::max (0.0, worst / cap);
        for (int j = 0; j < 8; ++j)
        {
            double ux, uy; ab::Window::dir (j, ux, uy);
            const double need = qx * ux + qy * uy - APO * win.scale + 0.02;
            if (need > win.bulge[j]) win.bulge[j] = need;
        }
        ++i;
    }

    // the tiling the panel is shown — small, and only what fits the aperture
    ab::buildPatch (vis, win, hab, gx, gy, 13.5, 2);

    rebuildChain();

    /*  WHEN IS THE STAR STALE?

        What was here was a list of the parameters someone remembered feeding
        it, guarded by a comment claiming the peak positions never move. That
        comment described the old star, which read its ratios off an assumed
        module. This one reads them off the chain. And the list never mentioned
        CUT BEARING, CUT OFFSET, APERTURE, RIM, CONTRAST, or the arrests --
        every one of which reshapes the chain, and every one of which is a
        thing the player does to the body ON SCREEN.

        The consequence was measured before it was fixed. Moving the sounding
        line while a note was held changed the sustained sound by exactly zero:
        the chain was rebuilt on every tick, correctly, and the star went on
        being the diffraction of a filament that no longer existed. Since the
        sustain is carried almost entirely by the star -- the filament is
        struck and dies, three orders of magnitude down by two seconds -- the
        note simply did not hear the control. Twenty-eight of forty-one
        controls were deaf to a held note; this was the largest single cause.

        So the question is no longer "which parameters did I remember". The
        star is the diffraction of the chain, therefore the star is stale when
        the chain it was made from is not the chain that is sounding. Ask the
        chain. A parameter added in five years' time is covered by construction,
        which the list could never be. */
    const ChainData& cc = chains[chainCur.load()];
    double chainKey = (double) cc.n;
    for (int i = 0; i < cc.n; ++i)
        chainKey += (double) cc.s[i] * (double) (i + 1) * 0.5
                  + (double) cc.m[i] * (double) (i + 7) * 0.25;

    const bool starStale = (lastHabit != starHabit) || std::abs (tau - starTau) > 0.11
                        || std::abs ((double) p.peaks - starPeaks) > 0.5
                        || std::abs ((double) p.starwid - starWid) > 1.0e-4
                        || std::abs ((double) p.startilt - starTilt) > 1.0e-4
                        || std::abs ((double) p.extent - starExtent) > 1.0e-4
                        || std::abs ((double) p.tilt4 - starTilt4) > 1.0e-4
                        || std::abs (chainKey - starChainKey) > 1.0e-9 * (1.0 + std::abs (chainKey));
    if (starStale)
    {
        starHabit = lastHabit; starTau = tau; starPeaks = p.peaks;
        starWid = p.starwid; starTilt = p.startilt; starExtent = p.extent;
        starTilt4 = p.tilt4; starChainKey = chainKey;
        rebuildStar();
    }
}

void Engine::rebuildChain()
{
    const int ei = (int) clampf (std::round (p.extent), 0.0f, 5.0f);
    const int want = std::min (MAXN, PELL[ei]);
    const double bearing = (double) p.cutang * PI;            // a line, so pi is a full turn
    const double offset  = (double) (p.cutpos - 0.5f) * 16.0;

    /*  OBLIQUITY, at last connected to something. It reaches a strain of 0.06,
        which over a 169-site filament walks the acceptance window some eight
        units through the unseen plane -- about three window widths, enough to
        pass several commensurate slopes on the way. Beyond that the search box
        grows faster than the sound changes: the rebuild is 20 ms at rest and
        27 ms at full, and it is not worth more. */
    const double strain = (double) clampf (p.tilt4, 0.0f, 1.0f) * 0.06;
    ab::buildChain (cut, win, hab, gx, gy, bearing, offset, want, 0.62, strain);

    const int next = (chainCur.load() + 1) % 3;
    ChainData& c = chains[next];
    const int n = std::min ((int) cut.size(), MAXN);
    c.n = n;
    c.gen = ++chainGen;
    for (int i = 0; i < n; ++i) c.s[i] = (float) cut[(size_t) i].s;
    if (n < 3) { c.w1 = 0.05; c.wmax = 2.0; chainCur.store (next); chainN.store (n); return; }

    // spacings -> bonds; the field -> masses; the rim -> a fade out of being
    double mean = 0.0;
    for (int i = 0; i + 1 < n; ++i) mean += cut[(size_t) (i + 1)].s - cut[(size_t) i].s;
    mean /= (double) (n - 1);
    if (mean < 1.0e-6) mean = 1.0;

    double msum = 0.0, ksum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double f = cut[(size_t) i].field;
        const double m = std::pow (hab.contrast, f - 0.5);
        c.m[i]   = (float) m;
        c.occ[i] = (float) cut[(size_t) i].occ;
        c.fld[i] = (float) f;
        msum += m;
    }
    /*  Bonds from spacings — and the normalisation must be taken on the RAW
        stiffnesses, before the occupancies fade them. Normalising the faded
        values means that as matter leaves the slab the survivors are scaled
        UP to compensate, so travelling through w silently stiffened the whole
        body: the pitch swung and the timestep bound was chasing a moving
        target. That was the last of the travel roughness. */
    static thread_local double kraw[MAXN + 1];
    for (int i = 0; i <= n; ++i)
    {
        double d = mean;
        if (i > 0 && i < n) d = cut[(size_t) i].s - cut[(size_t) (i - 1)].s;
        d = std::max (1.0e-3, d / mean);
        kraw[i] = std::pow (d, -hab.bondExp);
        ksum += kraw[i];
    }
    // normalise so the material's scale is fixed and the tuning anchor rules
    const double mnorm = (double) n / std::max (1.0e-9, msum);
    const double knorm = (double) (n + 1) / std::max (1.0e-9, ksum);   // on the RAW sum
    c.mMin = 1.0e9f;
    for (int i = 0; i < n; ++i)
    {
        c.m[i] = (float) std::max (0.02, c.m[i] * mnorm);
        c.im[i] = 1.0f / c.m[i];
        if (c.m[i] < c.mMin) c.mMin = c.m[i];
    }
    for (int i = 0; i <= n; ++i)
    {
        // a departing site lets go of its bonds — applied AFTER the scale is fixed
        const double oL = (i > 0) ? cut[(size_t) (i - 1)].occ : cut[0].occ;
        const double oR = (i < n) ? cut[(size_t) i].occ       : cut[(size_t) (n - 1)].occ;
        c.k[i] = (float) std::max (0.0, kraw[i] * knorm * (dbgMode == 4 ? 1.0 : oL * oR));
        /*  As a bond fades it also gets STICKY. That is what stops matter
            arriving in our slice from injecting energy: by the time the bond
            is stiff again the two sites are moving together, so there is no
            strain to pay for. Bond damping is symmetric — equal and opposite
            on the two ends — so it can only ever take energy out. */
        const double faint = 1.0 - std::min (oL, oR);
        c.cf[i] = (float) (faint * faint * std::sqrt (faint));
    }

    /*  z is a RELATIVE loss factor per site; the absolute decay time is set at
        render, where the timestep is known (see renderVoiceBlock).

        g is the spring that draws a departing site back to rest. It is there
        for a physical reason and a numerical one. Physically: a bubble
        crossing a plane shrinks to a point, it does not let go and drift.
        Numerically: without it a site whose two bonds have both faded to
        nothing is a FREE MASS, the stiffness matrix is singular, the lowest
        eigenvalue goes to zero, and the tuning anchor asks for an infinite
        timestep. That is what the travel test caught as a 1.18 splice. */
    for (int i = 0; i < n; ++i)
    {
        c.z[i] = 1.0f + 1.6f * c.fld[i];
        const float leaving = 1.0f - c.occ[i];
        c.g[i] = 0.6f * leaving * leaving;
    }
    c.span = (float) (cut[(size_t) (n - 1)].s - cut[0].s);

    spectralAnchor (c);
    chainCur.store (next);
    chainN.store (n);
    lowHz.store ((float) c.w1);
}

/*  The lowest eigenfrequency of the chain, exactly, by inverse iteration on
    the tridiagonal generalised problem K x = w^2 M x — and a Gershgorin bound
    on the highest, for stability. The first keeps the artefact in tune while
    its material changes underneath; the second sets the substep count. */
void Engine::spectralAnchor (ChainData& c)
{
    const int n = c.n;
    static thread_local double dg[MAXN], sub[MAXN], x[MAXN], y[MAXN], cp[MAXN];

    double wmax2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        // g regularises K so inverse iteration has something definite to work
        // on when a site's bonds have faded away; it is not a force any more
        dg[i]  = (double) c.k[i] + (double) c.k[i + 1] + (double) c.g[i];
        sub[i] = (i > 0) ? -(double) c.k[i] : 0.0;
        const double g = (2.0 * ((double) c.k[i] + (double) c.k[i + 1]) + (double) c.g[i]) / (double) c.m[i];
        if (g > wmax2) wmax2 = g;
    }
    c.wmax = std::sqrt (std::max (1.0e-9, wmax2));

    /*  What the note is anchored to is the BODY'S SIZE — the fundamental a
        chain of this many sites would have if it were uniform — not the true
        lowest eigenvalue. That was the first design and it was wrong twice
        over: at high mass contrast the lowest mode is LOCALISED, so the note
        was tuned to something inaudible; and because that eigenvalue moves as
        the material changes, dragging the body through w slid the pitch. The
        wheel must change the matter, not the tuning. The true w1 is still
        computed below — it is the artefact's own lowest note, and the panel
        shows it. */
    c.wRef = 2.0 * std::sin (PI / (2.0 * (double) (n + 1)));

    // Thomas factorisation of K (symmetric positive definite tridiagonal)
    const double eps = 1.0e-12 * (dg[0] + 1.0);
    cp[0] = (n > 1 ? sub[1] : 0.0) / (dg[0] + eps);
    for (int i = 1; i < n; ++i)
    {
        const double den = dg[i] - sub[i] * cp[i - 1];
        cp[i] = ((i + 1 < n) ? sub[i + 1] : 0.0) / (std::abs (den) < eps ? eps : den);
    }
    auto solve = [&] (const double* rhs, double* out)
    {
        static thread_local double d2[MAXN];
        d2[0] = rhs[0] / (dg[0] + eps);
        for (int i = 1; i < n; ++i)
        {
            const double den = dg[i] - sub[i] * cp[i - 1];
            d2[i] = (rhs[i] - sub[i] * d2[i - 1]) / (std::abs (den) < eps ? eps : den);
        }
        out[n - 1] = d2[n - 1];
        for (int i = n - 2; i >= 0; --i) out[i] = d2[i] - cp[i] * out[i + 1];
    };

    for (int i = 0; i < n; ++i) x[i] = std::sin (PI * (double) (i + 1) / (double) (n + 1));
    for (int it = 0; it < 26; ++it)
    {
        for (int i = 0; i < n; ++i) y[i] = (double) c.m[i] * x[i];
        solve (y, x);
        double nn = 0.0;
        for (int i = 0; i < n; ++i) nn += (double) c.m[i] * x[i] * x[i];
        nn = std::sqrt (std::max (1.0e-300, nn));
        for (int i = 0; i < n; ++i) x[i] /= nn;
    }
    // Rayleigh quotient
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double kx = dg[i] * x[i] + (i > 0 ? sub[i] * x[i - 1] : 0.0) + (i + 1 < n ? sub[i + 1] * x[i + 1] : 0.0);
        num += x[i] * kx;
        den += (double) c.m[i] * x[i] * x[i];
    }
    c.w1 = std::sqrt (std::max (1.0e-9, num / std::max (1.0e-12, den)));
}

void Engine::rebuildStar()
{
    ++starGen;                     // the panel draws the star; tell it when it moved
    const int want = std::min (MAXPEAK, (int) std::lround (clampf (p.peaks, 8.0f, (float) MAXPEAK)));
    ab::buildStar (peaks, hab, want, &cut, hab.contrast);

    const int next = (starCur.load() + 1) % 3;
    StarData& s = stars[next];
    s.n = (int) std::min ((size_t) MAXPEAK, peaks.size());
    for (int i = 0; i < s.n; ++i)
    {
        s.lam[i]  = (float) peaks[(size_t) i].lam;
        s.conj[i] = (float) peaks[(size_t) i].conj;
        s.amp[i]  = (float) peaks[(size_t) i].amp;
        s.rate[i] = (float) std::pow (peaks[(size_t) i].lam, 0.6);   // a high order dies faster
    }
    starCur.store (next);
    starN.store (s.n);
}

//==============================================================================
void Engine::arrest (double px, double py, bool on)
{
    if (! on)
    {
        double best = 1.0e9; int bi = -1;
        for (int i = 0; i < (int) arrests.size(); ++i)
        {
            const double d = (arrests[(size_t) i].first - px) * (arrests[(size_t) i].first - px)
                           + (arrests[(size_t) i].second - py) * (arrests[(size_t) i].second - py);
            if (d < best) { best = d; bi = i; }
        }
        if (bi >= 0 && best < 0.09) arrests.erase (arrests.begin() + bi);
        return;
    }
    if (arrests.size() >= 12) arrests.erase (arrests.begin());
    arrests.push_back ({ px, py });
}

void Engine::clearArrests() { arrests.clear(); }

//==============================================================================
int Engine::allocVoice (int note)
{
    const int nv = (int) clampf (std::round (p.voicesN), 1.0f, (float) MAXVOICE);
    for (int i = 0; i < nv; ++i) if (voices[(size_t) i].on && voices[(size_t) i].note == note) return i;
    for (int i = 0; i < nv; ++i) if (! voices[(size_t) i].on) return i;
    int oldest = 0; float best = -1.0f;
    for (int i = 0; i < nv; ++i) if (voices[(size_t) i].age > best) { best = voices[(size_t) i].age; oldest = i; }
    return oldest;
}

//==============================================================================
//  Voices. Rendered a chunk at a time, because everything that is constant
//  over a chunk — envelope coefficients, the timestep, the substep count, the
//  per-order decay — costs a transcendental, and paying for those per sample
//  per order per voice put the whole instrument at 28 % of a core.
//==============================================================================

struct Engine::BlockConst
{
    float dt = 1.0f / 48000.0f;
    float aT = 0.01f, dT = 1.0f, rT = 0.5f, sus = 0.35f;
    float envA = 0.0f, envD = 0.0f, envR = 0.0f;    // one-pole coefficients
    double decaySec = 1.6;                          // the body's own T60-ish
    float  quenchMul = 1.0f;                        // extra loss once let go
    float  beta = 0.0f;                             // strain
    float  tiltX = 4.0f;                            // top-of-band loss multiplier
    float  driveAmt = 0.0f, driveCol = 0.5f;
    float  cavK = 0.0f;              // the shared body, as a spring constant
    float  gStar = 1.0f, gFil = 1.0f;
    float  starFloor = 0.5f;
    float  envK[MAXPEAK] {};                        // per-order envelope, per 16 samples
    int    nStar = 0;
};

//==============================================================================
int Engine::nearestIndex (const ChainData& c, double s)
{
    if (c.n <= 1) return 0;
    int lo = 0, hi = c.n - 1;
    while (lo < hi) { const int mid = (lo + hi) >> 1; if ((double) c.s[mid] < s) lo = mid + 1; else hi = mid; }
    if (lo > 0 && std::abs ((double) c.s[lo - 1] - s) < std::abs ((double) c.s[lo] - s)) --lo;
    return lo;
}

// the same, but keeping the fraction: a listening point is a PLACE on the body,
// not a site. Snapping it to the nearest site means that when the body is
// dragged through w and the sites shuffle by one, the output jumps from one
// site's velocity to its neighbour's — a full-scale step, which is what the
// first travel test caught (0.95 against a settled 0.015).
static inline float placeOf (const ChainData& c, double s, int& i0)
{
    if (c.n <= 1) { i0 = 0; return 0.0f; }
    int lo = 0, hi = c.n - 1;
    while (lo < hi) { const int mid = (lo + hi) >> 1; if ((double) c.s[mid] <= s) lo = mid + 1; else hi = mid; }
    i0 = std::max (0, std::min (c.n - 2, lo - 1));
    const double a = c.s[i0], b = c.s[i0 + 1];
    return (float) std::max (0.0, std::min (1.0, (b - a) > 1e-9 ? (s - a) / (b - a) : 0.0));
}

/*  The energy the filament is carrying, on whatever material it is currently
    sitting on: kinetic plus the strain in every bond. */
static double chainEnergy (const float* u, const float* v, const ChainData& c)
{
    double e = 0.0;
    for (int i = 0; i < c.n; ++i) e += 0.5 * (double) c.m[i] * (double) v[i] * (double) v[i];
    for (int i = 0; i <= c.n; ++i)
    {
        const double a = (i > 0)   ? (double) u[i - 1] : 0.0;
        const double b = (i < c.n) ? (double) u[i]     : 0.0;
        e += 0.5 * (double) c.k[i] * (b - a) * (b - a);
    }
    return e;
}

/*  The wave lives on the body, not on an array index. When the cut is rebuilt
    — the wheel moved, or a site was arrested — the displacement field is
    carried onto the new sites by interpolation in position. That is what makes
    travelling through the fourth dimension continuous rather than a sequence
    of splices. */
void Engine::resampleVoice (Voice& vc, const ChainData& c)
{
    static thread_local float nu[MAXN], nv[MAXN];
    const int n = c.n, o = vc.nOld;
    if (o < 2)
    {
        for (int i = 0; i < MAXN; ++i) { vc.u[i] = 0.0f; vc.v[i] = 0.0f; }
    }
    else
    {
        int j = 0;
        for (int i = 0; i < n; ++i)
        {
            const float s = c.s[i];
            while (j + 2 < o && vc.sOld[j + 1] < s) ++j;
            if (s <= vc.sOld[0] || s >= vc.sOld[o - 1]) { nu[i] = 0.0f; nv[i] = 0.0f; continue; }
            const float d = vc.sOld[j + 1] - vc.sOld[j];
            const float t = d > 1.0e-9f ? (s - vc.sOld[j]) / d : 0.0f;
            nu[i] = vc.u[j] + t * (vc.u[j + 1] - vc.u[j]);
            nv[i] = vc.v[j] + t * (vc.v[j + 1] - vc.v[j]);
        }
        for (int i = 0; i < n; ++i) { vc.u[i] = nu[i]; vc.v[i] = nv[i]; }
        for (int i = n; i < MAXN; ++i) { vc.u[i] = 0.0f; vc.v[i] = 0.0f; }
    }
    vc.nOld = n; vc.gen = c.gen;
    for (int i = 0; i < n; ++i) vc.sOld[i] = c.s[i];

    /*  THE RULE THAT MAKES TRAVEL SAFE, and it is a physical statement rather
        than a patch: dragging the artefact through our slice may take energy
        OUT of the sound, and may never put energy in.

        Every individual route into the field was chased and fixed in turn —
        the normalisation, the grounding spring, the arriving site's velocity —
        and each time a little less energy appeared and the sound still grew.
        It had to: ANY change to the masses or the bonds at fixed displacement
        changes the stored energy, and there are as many routes as there are
        terms. So the constraint is imposed on the total, once, here. */
    const double e = chainEnergy (vc.u, vc.v, c);
    if (vc.energyPrev > 1.0e-12 && e > vc.energyPrev)
    {
        const float g = (float) std::sqrt (vc.energyPrev / e);
        for (int i = 0; i < n; ++i) { vc.u[i] *= g; vc.v[i] *= g; }
    }
    else vc.energyPrev = e;
}

//==============================================================================
void Engine::startVoice (Voice& vc, int note, float vel)
{
    const ChainData& c = chains[chainCur.load()];
    const StarData&  s = stars[starCur.load()];
    const int n = std::max (1, c.n);

    const bool fresh = ! vc.on || vc.env < 1.0e-4f;
    vc.on = vc.held = true;
    vc.note = note; vc.vel = clampf (vel, 0.02f, 1.0f);
    vc.gate = 1.0f; vc.age = 0.0f;
    vc.f0Target = noteHz (note);
    if (fresh || p.glide <= 0.0f) vc.f0 = vc.f0Target;
    if (fresh) { vc.gen = c.gen; vc.nOld = n; for (int i = 0; i < n; ++i) vc.sOld[i] = c.s[i]; }

    /*  Where the note enters the body. WALK moves that station further along
        the cut as you climb the keyboard, so playing higher is also playing
        FURTHER ALONG — and because the body is quasiperiodic the timbre almost
        repeats and never does. */
    const double walk = (double) p.follow * (double) (note - 60) * 0.5;
    double stn = (double) p.place * (double) (n - 1) + walk;
    stn = std::fmod (stn, (double) n); if (stn < 0) stn += (double) n;
    const int si = (int) clampd (stn, 0.0, (double) (n - 1));

    const int half = std::max (1, (int) ((double) p.spread * 0.45 * (double) n));
    vc.sStation = c.s[si];
    vc.sL = c.s[(si - half + 4 * n) % n];
    vc.sR = c.s[(si + half) % n];

    /*  The strike. INCIDENCE runs from a broad soft push over many sites to a
        point impulse on one: the difference between being leaned on and being
        struck. */
    const double hard = clampd (p.strike, 0.0, 1.0);
    const int wdt = std::max (1, (int) std::lround ((1.0 - hard) * 0.09 * (double) n) + 1);
    const double amp = 0.75 * (double) vc.vel * (0.30 + 0.70 * hard);
    for (int d = -wdt; d <= wdt; ++d)
    {
        const int i = si + d;
        if (i < 0 || i >= n) continue;
        const double wgt = 0.5 * (1.0 + std::cos (PI * (double) d / (double) (wdt + 1)));
        vc.v[i] += (float) (amp * wgt * (double) c.im[i]);
    }

    // a strike is an INPUT, so it resets the energy ceiling
    vc.energyPrev = chainEnergy (vc.u, vc.v, c);

    // the star is re-struck: every order restarts at its own intensity
    for (int i = 0; i < s.n; ++i)
    {
        vc.pa[i] = s.amp[i] * vc.vel;
        if (fresh) vc.ph[i] = (float) std::fmod ((double) i * 0.6180339887498949, 1.0);   // decorrelated, not random
    }
}

void Engine::noteOn (int note, float vel)
{
    if (vel <= 0.0f) { noteOff (note); return; }
    startVoice (voices[(size_t) allocVoice (note)], note, vel);
}

void Engine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.on && v.note == note) { v.held = false; if (! sustainPedal) v.gate = 0.0f; }
}

void Engine::allNotesOff()
{
    sustainPedal = false;   // a panic lets go of the pedal, or it holds the next note
    for (auto& v : voices) { v.held = false; v.gate = 0.0f; }
}

void Engine::setSustainPedal (bool on)
{
    sustainPedal = on;
    if (! on) for (auto& v : voices) if (! v.held) v.gate = 0.0f;
}

//==============================================================================
void Engine::renderVoiceBlock (Voice& vc, const BlockConst& b, float* aL, float* aR, int nS, float cav)
{
    const ChainData& c = chains[chainCur.load()];
    const StarData&  s = stars[starCur.load()];
    const int n = c.n;
    if (n < 3) return;
    if (vc.gen != c.gen && dbgMode != 1) resampleVoice (vc, c);
    if (dbgMode == 1) vc.gen = c.gen;

    // ---- pitch, timestep, substeps ---------------------------------------
    if (p.glide > 0.0f)
    {
        const double g = 1.0 - std::exp (-(double) nS * (double) b.dt / xmapd (p.glide, 0.004, 3.0));
        vc.f0 += (vc.f0Target - vc.f0) * g;
    }
    else vc.f0 = vc.f0Target;

    int i0L = 0, i0R = 0, i0S = 0;
    const float fL = placeOf (c, vc.sL, i0L);
    const float fR = placeOf (c, vc.sR, i0R);
    const float fS = placeOf (c, vc.sStation, i0S);

    const double hWant = 2.0 * PI * vc.f0 / (std::max (1.0e-6, c.wRef) * sr);
    // the cavity spring stiffens the station site, so it belongs in the bound
    const double wmaxEff = std::sqrt (c.wmax * c.wmax + 2.0 * (double) b.cavK * (double) c.im[i0S]);
    const double hSafe = 1.55 / std::max (1.0e-6, wmaxEff);
    int sub = (int) std::ceil (hWant / hSafe);
    sub = std::max (1, std::min (8, sub));
    /*  Above eight substeps the note is allowed to run flat rather than the
        integrator allowed to explode. Stability is never a matter of trusting
        an earlier calculation. */
    const float h = (float) std::min (hWant / (double) sub, hSafe);
    const double hs = (double) h * (double) sub * sr;       // chain-time per second

    /*  LOSS is a decay time, not a coefficient. Written as a raw per-step
        number it depended on the timestep, so the artefact's memory changed
        with the note played, and the default worked out at ninety
        milliseconds — the bench measured a fundamental that had died before
        the window opened. */
    const float zBase   = (float) (1.0 / (b.decaySec * hs));
    const float zLeave  = (float) (1.0 / (0.012 * hs));      // gone from the slab in ~12 ms
    const float zQuench = (vc.gate > 0.5f) ? 0.0f : (float) (b.quenchMul / hs);

    // viscous (Kelvin-Voigt) loss, set so the top of the band decays tiltX
    // times faster than the bottom, whatever the note or the material
    const float nuV = (float) (2.0 * (double) (b.tiltX - 1.0f) / (b.decaySec * hs * std::max (1.0e-6, c.wmax * c.wmax)));

    const float beta = b.beta;

    /*  The gain on the fade damping, bounded so the explicit term can never
        run away: two such bonds meet at a site, so h*2*cfG/m stays at 0.7.

        This replaces an earlier attempt that RELAXED a faint site toward the
        average of its neighbours. That looks dissipative and is not: smoothing
        only lowers the UNWEIGHTED norm, and with a heavy site between two
        light ones, pulling it toward their mean adds kinetic energy. Two
        habits in the catalogue rang up a hundredfold on that alone. */
    const float cfG = 0.35f * c.mMin / std::max (1.0e-6f, h);

    for (int t = 0; t < nS; ++t)
    {
        // ---- the applied force's envelope ---------------------------------
        if (vc.gate > 0.5f) vc.env += ((vc.age < b.aT ? 1.0f : b.sus) - vc.env) * (vc.age < b.aT ? b.envA : b.envD);
        else                vc.env += (0.0f - vc.env) * b.envR;
        vc.age += b.dt;

        /*  THE CAVITY. Every note sounds into one body and hears the others.
            It is a SPRING to a shared node — a restoring force toward the
            common displacement — and not an injection of the bus signal.

            The first version injected the bus as a force while listening at a
            point seventeen sites away. That is non-collocated VELOCITY
            feedback, which is not dissipative: it pumps wherever the transfer
            phase suits it, and in a chain whose Q is around nine hundred it
            reached a hundred and forty times full scale. Measured: with the
            cavity at zero the same patch peaks at 0.126, with it at the
            default, 147. A spring is what a shared body actually is, and a
            symmetric stiffness cannot create energy. */
        float fIn = 0.0f;
        if (b.driveAmt > 0.0f)
        {
            const float nz = noise (vc.rng);
            vc.drivez += (nz - vc.drivez) * (0.02f + 0.95f * b.driveCol * b.driveCol);
            fIn = 0.55f * b.driveAmt * vc.env * vc.vel * vc.drivez;
        }

        // ---- the filament: the wave equation on the quasiperiodic chain ----
        for (int st = 0; st < sub; ++st)
        {
            float uPrev = 0.0f, uCur = vc.u[0], vPrevOld = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float uNext = (i + 1 < n) ? vc.u[i + 1] : 0.0f;
                const float vNext = (i + 1 < n) ? vc.v[i + 1] : 0.0f;
                const float kA = c.k[i], kB = c.k[i + 1];
                const float dA = uPrev - uCur, dB = uNext - uCur;
                const float vi = vc.v[i];
                float f = kA * dA + kB * dB;
                if (beta > 0.0f)
                {
                    // clamped before cubing: the stability bound above knows
                    // about the linear stiffness, not about a cubic that grows
                    // without limit under a hard strike
                    const float ca = dA < -1.5f ? -1.5f : (dA > 1.5f ? 1.5f : dA);
                    const float cb = dB < -1.5f ? -1.5f : (dB > 1.5f ? 1.5f : dB);
                    f += beta * (kA * ca * ca * ca + kB * cb * cb * cb);
                }
                f += nuV * (kA * (vPrevOld - vi) + kB * (vNext - vi));
                f += cfG * (c.cf[i] * (vPrevOld - vi) + c.cf[i + 1] * (vNext - vi));
                /*  The station: where the note enters, and where this voice is
                    tied to the shared body.

                    The cavity spring must read the displacement of THIS
                    substep. Written against last sample's value it is a
                    delayed spring, and a delayed spring is a NEGATIVE
                    stiffness at the top of the band — habits with a long decay
                    rang up to ten kilohertz after the note had been let go. */
                if (i == i0S)          f += (fIn + b.cavK * (cav - uCur)) * (1.0f - fS);
                else if (i == i0S + 1) f += (fIn + b.cavK * (cav - uCur)) * fS;
                const float occLeave = (dbgMode == 6) ? 0.0f : 1.0f - c.occ[i];
                const float z = zBase * c.z[i] + zQuench + zLeave * occLeave * occLeave;
                vc.v[i] = vi + h * (f * c.im[i] - z * vi);
                vc.u[i] = uCur + h * vc.v[i];
                uPrev = uCur; uCur = uNext; vPrevOld = vi;
            }
        }
        float yL = vc.v[i0L] + fL * (vc.v[i0L + 1] - vc.v[i0L]);
        float yR = vc.v[i0R] + fR * (vc.v[i0R + 1] - vc.v[i0R]);
        yL *= b.gFil; yR *= b.gFil;

        // ---- the star: the diffraction of the same chain -------------------
        if (b.nStar > 0)
        {
            const float base = (float) (vc.f0 / sr);
            const float dr = starRate;
            const bool  upd = ((vc.envTick++ & 15) == 0);
            float acc = 0.0f;
            for (int i = 0; i < b.nStar; ++i)
            {
                /*  f = f0*(p + q*sqrt2) + rate*(p - q*sqrt2).
                    Travelling in the fourth dimension adds the conjugate. */
                /*  A NEGATIVE increment is not an error: an order whose shadow
                    has carried it below zero simply runs its phase backwards,
                    and sounds identical. Dropping those made loud orders wink
                    out mid-travel. Only fold away what is above Nyquist. */
                const float inc = base * s.lam[i] + dr * s.conj[i];
                if (inc > 0.47f || inc < -0.47f) continue;
                float ph = vc.ph[i] + inc;
                if (ph >= 1.0f) ph -= 1.0f; else if (ph < 0.0f) ph += 1.0f;
                vc.ph[i] = ph;
                if (upd) vc.pa[i] += (vc.env * s.amp[i] * b.starFloor * vc.vel - vc.pa[i]) * b.envK[i];
                acc += vc.pa[i] * fsin (ph);
            }
            acc *= b.gStar;
            yL += acc; yR += acc * 0.97f;
        }

        // per-voice dc block: a chain can carry a slow drift
        vc.hpx += 0.0006f * (yL - vc.hpx); yL -= vc.hpx;
        vc.hpy += 0.0006f * (yR - vc.hpy); yR -= vc.hpy;

        vc.own = vc.u[i0S];          // this voice's contribution to the shared node
        aL[t] += yL; aR[t] += yR;
    }

    // what the field is carrying now, on this material — the ceiling that the
    // next rebuild is not allowed to exceed
    vc.energyPrev = chainEnergy (vc.u, vc.v, c);

    /*  The artefact will not tear itself apart. Three feedback paths meet in
        this chain — the cavity, the strain nonlinearity and the driven force —
        and a bound that has been reasoned about is not a bound that has been
        enforced. If the field ever runs away, scale it back rather than let
        the ceiling turn it into a square wave. Counted, so the bench sees it. */
    {
        float pk = 0.0f;
        for (int i = 0; i < n; i += 3) { const float a = std::abs (vc.u[i]); if (a > pk) pk = a; }
        if (pk > 600.0f)
        {
            const float g = 600.0f / pk;
            for (int i = 0; i < n; ++i) { vc.u[i] *= g; vc.v[i] *= g; }
            runaways.fetch_add (1);
        }
    }

    if (! std::isfinite (vc.u[0]) || ! std::isfinite (vc.v[0]))
    {
        for (int i = 0; i < MAXN; ++i) { vc.u[i] = 0.0f; vc.v[i] = 0.0f; }
        for (int i = 0; i < MAXPEAK; ++i) vc.pa[i] = 0.0f;
        vc.on = false;
    }

    {
        float vm = 0, um = 0, km = 0;
        for (int i = 0; i < n; ++i) { vm = std::max (vm, std::abs (vc.v[i])); um = std::max (um, std::abs (vc.u[i])); }
        for (int i = 0; i <= n; ++i) km = std::max (km, c.k[i] * c.im[std::min (i, n - 1)]);
        dbgH.store (h); dbgSub.store (sub); dbgWmax.store ((float) c.wmax);
        dbgVmax.store (vm); dbgUmax.store (um); dbgKmax.store (km);
    }

    if (vc.gate < 0.5f && vc.env < 1.0e-5f)
    {
        float e = 0.0f;
        for (int i = 0; i < n; i += 4) e += std::abs (vc.v[i]);
        float sa = 0.0f;
        for (int i = 0; i < b.nStar; i += 4) sa += vc.pa[i];
        if (e < 1.0e-5f && sa < 1.0e-6f) vc.on = false;
    }
}

//==============================================================================
void Engine::process (float* L, float* R, int nS)
{
    const int nv = (int) clampf (std::round (p.voicesN), 1.0f, (float) MAXVOICE);
    const StarData& s = stars[starCur.load()];

    /*  How fast the body is being dragged through the cut. This is what makes
        every diffraction order detune by its own shadow: at rest PRECESSION
        supplies a standing drift, and while the wheel is turning the travel
        itself supplies it, so scrolling shears the whole spectrum. */
    const double d = tau - tauPrev;
    tauPrev = tau;
    travelRate += (d * sr / (double) std::max (1, nS) - travelRate) * 0.25;
    starRate = (float) ((6.0 * (double) p.drift * (double) p.drift + 4.0 * travelRate) / sr);

    BlockConst b;
    b.dt  = (float) (1.0 / sr);
    b.aT  = (float) xmapd (p.attack,  0.0006, 1.2);
    b.dT  = (float) xmapd (p.decay,   0.010,  6.0);
    b.rT  = (float) xmapd (p.release, 0.010,  3.0);
    b.sus = clampf (p.sustain, 0.0f, 1.0f);
    b.envA = 1.0f - std::exp (-b.dt / std::max (1.0e-4f, b.aT * 0.35f));
    b.envD = 1.0f - std::exp (-b.dt / std::max (1.0e-4f, b.dT * 0.35f));
    b.envR = 1.0f - std::exp (-b.dt / std::max (1.0e-4f, b.rT * 0.35f));
    b.decaySec  = xmapd (1.0 - (double) p.damp, 0.02, 14.0);
    b.quenchMul = 1.0f / (float) std::max (0.008, xmapd (1.0 - (double) p.release, 0.008, 2.5));
    b.beta      = (float) hab.nonlin;
    b.tiltX     = 1.0f + 60.0f * clampf (p.damptilt, 0.0f, 1.0f);
    b.driveAmt  = clampf (p.drive, 0.0f, 1.0f);
    b.driveCol  = clampf (p.drivecol, 0.0f, 1.0f);
    const float bl = clampf (p.blend, 0.0f, 1.0f);
    b.gStar   = 1.10f * bl;
    b.gFil    = 7.0f * (1.0f - bl);
    b.starFloor = 0.10f + 0.9f * b.sus;
    b.nStar   = s.n;
    {
        const float dec = 0.6f + 7.0f * clampf (p.stardec, 0.0f, 1.0f);
        for (int i = 0; i < s.n; ++i) b.envK[i] = 1.0f - std::exp (-16.0f * b.dt * dec * s.rate[i]);
    }

    /*  THE FRAGMENT HAS FACES, and past them the plane cuts nothing.

        An ideal cut-and-project tiling is infinite and dense: the acceptance
        window holds points at every depth, so no amount of TRAVERSE would
        ever empty the section. That is true of the mathematics and false of
        the object — B2311.67 came out of a vault as a fragment, and a
        fragment ends. The tissue occupies the middle of the traverse and the
        outer reaches are the vault: wind the cut far enough either way and
        there is no cross-section, and therefore nothing to hear.

        The face is a raised cosine so the object thins out rather than
        switching off, and it is exactly zero beyond it. */
    const double tauAbs = std::abs (tau);
    auto face = [] (double d, double in, double out)
    {
        d = std::abs (d);
        if (d <= in)  return 1.0;
        if (d >= out) return 0.0;
        return 0.5 + 0.5 * std::cos (PI * (d - in) / (out - in));
    };
    double presence = face (tauAbs, 8.5, 10.4);
    /*  THE FIELD. Sixty-seven vaults were found at the Sabik Terminator, each
        at the centre of its own void, and the field is not only a thing on the
        ground: carry the cut far enough along w and it meets a neighbour. They
        are narrow and quieter than home and nothing says where they are. */
    presence = std::max (presence, 0.55 * face (tauAbs -  67.0, 2.2, 4.2));
    presence = std::max (presence, 0.30 * face (tauAbs - 134.0, 1.4, 2.8));
    presencePub.store ((float) presence, std::memory_order_relaxed);

    const float lvl    = (float) (2.2 * (double) p.level * (double) p.level
                                  * presence) * habTrim;
    // (the cavity coupling gain lives in BlockConst, applied per voice)
    b.cavK = 0.35f * clampf (p.cavity, 0.0f, 1.0f) * clampf (p.cavity, 0.0f, 1.0f);
    // the shared body couples notes at LOW frequencies: a big body does, and a
    // one-chunk-delayed coupling can only be trusted well below the chunk rate
    const float cavA   = (float) xmapd (p.cavtone, 0.0018, 0.10);
    /*  CEILING is the level the artefact will not exceed, and it is now
        enforced by reducing gain rather than by bending the waveform. The
        compressor works a little below it so the brickwall has almost nothing
        left to catch. */
    const float ceilg  = clampf (0.35f + 0.9f * (1.0f - clampf (p.sat, 0.0f, 1.0f)), 0.10f, 0.94f);
    dyn.thr = ceilg * 0.55f;
    const float air    = clampf (p.air, 0.0f, 1.0f) * 0.8f;

    constexpr int CH = 32;
    static thread_local float bufL[CH], bufR[CH];

    for (int off = 0; off < nS; off += CH)
    {
        const int m = std::min (CH, nS - off);
        for (int i = 0; i < m; ++i) { bufL[i] = 0.0f; bufR[i] = 0.0f; }

        cavZ += (cavSum - cavZ) * cavA;
        const float cav = cavZ;                 // the bus; the coupling gain is per voice

        for (int i = 0; i < nv; ++i)
        {
            auto& vc = voices[(size_t) i];
            if (vc.on) renderVoiceBlock (vc, b, bufL, bufR, m, cav);
        }

        float last = 0.0f; (void) last;
        for (int i = 0; i < m; ++i)
        {
            float x = bufL[i], y = bufR[i];
            // AIR first: a gentle first-order lift so the top of the Cantor
            // set is heard...
            airL += 0.30f * (x - airL); airR += 0.30f * (y - airR);
            x += air * (x - airL); y += air * (y - airR);
            // ...then the level, and the output stage LAST, or the lift
            // overshoots it (an early version peaked at 1.30 against a ceiling
            // of 0.94).
            x *= lvl; y *= lvl;
            {   // what the output stage was handed, before it acted
                const float a = std::max (std::abs (x), std::abs (y));
                if (a > preLimitPeak.load (std::memory_order_relaxed))
                    preLimitPeak.store (a, std::memory_order_relaxed);
            }
            dyn.process (x, y, ceilg);
            L[off + i] = x; R[off + i] = y;
            last = 0.5f * (x + y);
            meter += ((std::abs (x) + std::abs (y)) - meter) * 0.001f;
        }
        cavSum = 0.0f;
        { int act = 0; for (int i = 0; i < nv; ++i) if (voices[(size_t) i].on) { cavSum += voices[(size_t) i].own; ++act; }
          if (act > 1) cavSum /= (float) act; }
    }
    outLevel.store (meter);
    limitGR.store (dyn.gr);
    dyn.gr = 1.0f;

    // stream the body's own displacement back to the panel
    const ChainData& c = chains[chainCur.load()];
    for (int i = 0; i < c.n && i < MAXN; ++i)
    {
        float e = 0.0f;
        for (int vI = 0; vI < nv; ++vI) if (voices[(size_t) vI].on) e += std::abs (voices[(size_t) vI].u[i]);
        siteE[i] += (e - siteE[i]) * 0.15f;
    }
}

void Engine::visualState (float* out, int maxSites, int& nOut) const
{
    const ChainData& c = chains[chainCur.load()];
    nOut = std::min (c.n, maxSites);
    for (int i = 0; i < nOut; ++i) out[i] = siteE[i];
}

} // namespace ax
