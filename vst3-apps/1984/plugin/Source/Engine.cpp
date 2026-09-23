/*  1984 — engine implementation. The reasoning lives in Engine.h; this file
    is the machinery. */
#include "Engine.h"

namespace n84
{

//==============================================================================
// knob -> value
float glideMs (float v)              { return v < 0.01f ? 0.0f : xmap (v, 5.0f, 5000.0f); }
float msOf (const PSpec& s, float v) { return xmap (v, s.lo, s.hi); }
float hzOf (const PSpec& s, float v) { return xmap (v, s.lo, s.hi); }
float volGain (float v)              { return v < 0.005f ? 0.0f : 2.0f * v * v; }
static inline float semiOf (float v) { return std::round ((v - 0.5f) * 24.0f); }
static inline float bip (float v)    { return (v - 0.5f) * 2.0f; }

//==============================================================================
// The parameter table. Rank rows are generated for I and II from one macro.
#define GROW(id, name, def, kind, lo, hi, field) { id, name, def, kind, lo, hi, -1, &Params::field, nullptr }
#define RROW(r, id, name, def, kind, lo, hi, field) { id, name, def, kind, lo, hi, r, nullptr, &RankParams::field }
#define RANKROWS(r, P, N) \
    RROW (r, P "saw",   N " SAW",         0.8f,  KP_PCT,  0, 0,         saw),   \
    RROW (r, P "pulse", N " PULSE",       0.0f,  KP_PCT,  0, 0,         pulse), \
    RROW (r, P "pw",    N " WIDTH",       0.0f,  KP_PW,   0, 0,         pw),    \
    RROW (r, P "pwm",   N " PWM",         0.0f,  KP_PCT,  0, 0,         pwm),   \
    RROW (r, P "tri",   N " TRIANGLE",    0.0f,  KP_PCT,  0, 0,         tri),   \
    RROW (r, P "sine",  N " SINE",        0.0f,  KP_PCT,  0, 0,         sine),  \
    RROW (r, P "noise", N " NOISE",       0.0f,  KP_PCT,  0, 0,         noise), \
    RROW (r, P "oct",   N " FEET",        2.0f,  KP_LIST, 0, 4,         oct),   \
    RROW (r, P "semi",  N " SEMITONE",    0.5f,  KP_SEMI, 0, 0,         semi),  \
    RROW (r, P "fine",  N " FINE",        0.5f,  KP_CENT, 100, 0,       fine),  \
    RROW (r, P "hpf",   N " HPF",         0.0f,  KP_HZ,   10, 12000,    hpf),   \
    RROW (r, P "hpq",   N " HPF RES",     0.0f,  KP_PCT,  0, 0,         hpq),   \
    RROW (r, P "lpf",   N " LPF",         0.75f, KP_HZ,   20, 20000,    lpf),   \
    RROW (r, P "lpq",   N " LPF RES",     0.15f, KP_PCT,  0, 0,         lpq),   \
    RROW (r, P "fmode", N " FILTER",      0.0f,  KP_LIST, 0, 1,         fmode), \
    RROW (r, P "il",    N " INITIAL LVL", 0.5f,  KP_BIPOL,0, 0,         il),    \
    RROW (r, P "al",    N " ATTACK LVL",  0.75f, KP_BIPOL,0, 0,         al),    \
    RROW (r, P "fa",    N " VCF ATTACK",  0.2f,  KP_MS,   1, 10000,     fa),    \
    RROW (r, P "fd",    N " VCF DECAY",   0.5f,  KP_MS,   2, 20000,     fd),    \
    RROW (r, P "fr",    N " VCF RELEASE", 0.5f,  KP_MS,   2, 20000,     fr),    \
    RROW (r, P "va",    N " VCA ATTACK",  0.15f, KP_MS,   1, 10000,     va),    \
    RROW (r, P "vd",    N " VCA DECAY",   0.5f,  KP_MS,   2, 20000,     vd),    \
    RROW (r, P "vs",    N " VCA SUSTAIN", 0.8f,  KP_PCT,  0, 0,         vs),    \
    RROW (r, P "vr",    N " VCA RELEASE", 0.4f,  KP_MS,   2, 20000,     vr),    \
    RROW (r, P "lvl",   N " LEVEL",       0.8f,  KP_PCT,  0, 0,         lvl),   \
    RROW (r, P "pan",   N " PAN",         0.5f,  KP_BIPOL,0, 0,         pan),   \
    RROW (r, P "vel",   N " VELOCITY",    0.5f,  KP_PCT,  0, 0,         vel),   \
    RROW (r, P "velb",  N " VEL>BRILL",   0.3f,  KP_PCT,  0, 0,         velb)

static const PSpec SPECS[] =
{
    // ---- global / performance ------------------------------------------
    GROW ("mode",    "VOICE MODE",     0.0f,  KP_LIST,  0, 3,      mode),
    GROW ("glide",   "GLIDE",          0.0f,  KP_GLIDE, 5, 5000,   glide),
    GROW ("gliss",   "GLISSANDO",      0.0f,  KP_SW,    0, 0,      gliss),
    GROW ("legato",  "LEGATO",         1.0f,  KP_SW,    0, 0,      legato),
    GROW ("tune",    "MASTER TUNE",    0.5f,  KP_SEMI,  0, 0,      tune),
    GROW ("fine",    "MASTER FINE",    0.5f,  KP_CENT,  100, 0,    fine),
    GROW ("bend",    "BEND RANGE",     2.0f,  KP_INT,   0, 24,     bend),
    GROW ("vintage", "VINTAGE",        0.3f,  KP_PCT,   0, 0,      vintage),
    GROW ("os",      "QUALITY",        1.0f,  KP_LIST,  0, 2,      os),
    GROW ("volume",  "VOLUME",         0.5f,  KP_VOL,   0, 0,      volume),
    GROW ("brill",   "BRILLIANCE",     0.5f,  KP_BIPOL, 0, 0,      brill),
    GROW ("reso",    "RESONANCE",      0.5f,  KP_BIPOL, 0, 0,      reso),
    GROW ("ktrack",  "KEY TRACK",      0.5f,  KP_PCT,   0, 0,      ktrack),
    GROW ("spread",  "STEREO SPREAD",  0.6f,  KP_PCT,   0, 0,      spread),
    GROW ("unidet",  "UNISON DETUNE",  0.3f,  KP_CENTU, 60, 0,     unidet),
    GROW ("patch",   "PATCH",          0.0f,  KP_INT,   0, 39,     patch),

    // ---- rank I and II ---------------------------------------------------
    RANKROWS (0, "a_", "I"),
    RANKROWS (1, "b_", "II"),
    RROW (1, "b_sync", "SYNC I TO II", 0.0f, KP_SW, 0, 0, sync),

    // ---- ring modulator --------------------------------------------------
    GROW ("ring_mode",  "RING MODE",    0.0f,  KP_LIST,  0, 2,      ring_mode),
    GROW ("ring_speed", "RING SPEED",   0.5f,  KP_HZ,    0.1f, 4000, ring_speed),
    GROW ("ring_key",   "RING KEY",     0.0f,  KP_SW,    0, 0,      ring_key),
    GROW ("ring_depth", "RING DEPTH",   0.0f,  KP_PCT,   0, 0,      ring_depth),
    GROW ("ring_a",     "RING ATTACK",  0.2f,  KP_MS,    1, 5000,   ring_a),
    GROW ("ring_d",     "RING DECAY",   0.5f,  KP_MS,    5, 10000,  ring_d),
    GROW ("ring_mod",   "RING MOD",     0.5f,  KP_BIPOL, 0, 0,      ring_mod),

    // ---- poly-mod --------------------------------------------------------
    GROW ("pm_o2pitch", "II>I PITCH",   0.0f,  KP_PCT,   0, 0,      pm_o2pitch),
    GROW ("pm_o2pw",    "II>I WIDTH",   0.0f,  KP_PCT,   0, 0,      pm_o2pw),
    GROW ("pm_o2filt",  "II>FILTER",    0.0f,  KP_PCT,   0, 0,      pm_o2filt),
    GROW ("pm_envpitch","ENV>I PITCH",  0.5f,  KP_BIPOL, 0, 0,      pm_envpitch),
    GROW ("pm_envpw",   "ENV>I WIDTH",  0.5f,  KP_BIPOL, 0, 0,      pm_envpw),

    // ---- sub-oscillator --------------------------------------------------
    GROW ("lfo_wave",  "SUB WAVE",      0.0f,  KP_LIST,  0, 6,      lfo_wave),
    GROW ("lfo_rate",  "SUB RATE",      0.7f,  KP_HZ,    0.05f, 40, lfo_rate),
    GROW ("lfo_pitch", "SUB>PITCH",     0.0f,  KP_PCT,   0, 0,      lfo_pitch),
    GROW ("lfo_pw",    "SUB>WIDTH",     0.0f,  KP_PCT,   0, 0,      lfo_pw),
    GROW ("lfo_vcf",   "SUB>FILTER",    0.0f,  KP_PCT,   0, 0,      lfo_vcf),
    GROW ("lfo_vca",   "SUB>LEVEL",     0.0f,  KP_PCT,   0, 0,      lfo_vca),
    GROW ("lfo_delay", "SUB DELAY",     0.0f,  KP_GLIDE, 10, 5000,  lfo_delay),
    GROW ("lfo_mode",  "SUB PHASE",     0.0f,  KP_LIST,  0, 2,      lfo_mode),

    // ---- wheel and touch -------------------------------------------------
    GROW ("wheel_lfo",   "WHEEL>VIBRATO", 0.4f, KP_PCT,   0, 0,     wheel_lfo),
    GROW ("wheel_brill", "WHEEL>BRILL",   0.5f, KP_BIPOL, 0, 0,     wheel_brill),
    GROW ("wheel_rate",  "WHEEL RATE",    0.5f, KP_HZ,    2, 10,    wheel_rate),
    GROW ("at_pitch",    "TOUCH>PITCH",   0.0f, KP_PCT,   0, 0,     at_pitch),
    GROW ("at_brill",    "TOUCH>BRILL",   0.3f, KP_PCT,   0, 0,     at_brill),
    GROW ("at_lvl",      "TOUCH>LEVEL",   0.0f, KP_PCT,   0, 0,     at_lvl),
    GROW ("at_lfo",      "TOUCH>SUB",     0.3f, KP_PCT,   0, 0,     at_lfo),

    // ---- chain -----------------------------------------------------------
    GROW ("drv_mode",  "DRIVE",          0.0f,  KP_LIST, 0, 4,      drv_mode),
    GROW ("drv_amt",   "DRIVE AMOUNT",   0.3f,  KP_PCT,  0, 0,      drv_amt),
    GROW ("drv_tone",  "DRIVE TONE",     0.5f,  KP_BIPOL,0, 0,      drv_tone),
    GROW ("ens_mode",  "ENSEMBLE",       0.0f,  KP_LIST, 0, 4,      ens_mode),
    GROW ("ens_rate",  "ENSEMBLE RATE",  0.5f,  KP_PCT,  0, 0,      ens_rate),
    GROW ("ens_depth", "ENSEMBLE DEPTH", 0.6f,  KP_PCT,  0, 0,      ens_depth),
    GROW ("ens_mix",   "ENSEMBLE MIX",   0.5f,  KP_PCT,  0, 0,      ens_mix),
    GROW ("choir_mix",   "CHOIR MIX",    0.0f,  KP_PCT,  0, 0,      choir_mix),
    GROW ("choir_vowel", "CHOIR VOWEL",  0.5f,  KP_PCT,  0, 0,      choir_vowel),
    GROW ("choir_reg",   "CHOIR REGISTER",0.5f, KP_BIPOL,0, 0,      choir_reg),
    GROW ("choir_air",   "CHOIR AIR",    0.2f,  KP_PCT,  0, 0,      choir_air),
    GROW ("tape_mode",   "TAPE",         0.0f,  KP_LIST, 0, 2,      tape_mode),
    GROW ("tape_wow",    "WOW",          0.3f,  KP_PCT,  0, 0,      tape_wow),
    GROW ("tape_wowrate","WOW RATE",     0.4f,  KP_HZ,   0.1f, 3,   tape_wowrate),
    GROW ("tape_flut",   "FLUTTER",      0.3f,  KP_PCT,  0, 0,      tape_flut),
    GROW ("tape_sat",    "TAPE SAT",     0.3f,  KP_PCT,  0, 0,      tape_sat),
    GROW ("tape_age",    "TAPE AGE",     0.3f,  KP_PCT,  0, 0,      tape_age),
    GROW ("tape_drop",   "DROPOUTS",     0.0f,  KP_PCT,  0, 0,      tape_drop),
    GROW ("tape_hiss",   "HISS",         0.2f,  KP_PCT,  0, 0,      tape_hiss),
    GROW ("hall_mix",    "HALL MIX",     0.25f, KP_PCT,  0, 0,      hall_mix),
    GROW ("hall_pre",    "HALL PREDELAY",0.45f, KP_MS,   1, 250,    hall_pre),
    GROW ("hall_size",   "HALL SIZE",    0.6f,  KP_PCT,  0, 0,      hall_size),
    GROW ("hall_decay",  "HALL DECAY",   0.45f, KP_SEC,  0.2f, 20,  hall_decay),
    GROW ("hall_damp",   "HALL DAMPING", 0.6f,  KP_HZ,   1000, 16000, hall_damp),
    GROW ("hall_mod",    "HALL MOTION",  0.3f,  KP_PCT,  0, 0,      hall_mod),
    GROW ("hall_shim",   "HALL SHIMMER", 0.0f,  KP_PCT,  0, 0,      hall_shim),
};

int numParams() { return (int) (sizeof (SPECS) / sizeof (SPECS[0])); }
const PSpec& paramSpec (int i) { return SPECS[i < 0 ? 0 : (i >= numParams() ? numParams() - 1 : i)]; }
int paramIndex (const char* id)
{
    for (int i = 0; i < numParams(); ++i) if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}
float paramMax (const PSpec& s) { return (s.kind == KP_LIST || s.kind == KP_INT) ? s.hi : 1.0f; }

const char* const* listNames (const char* id, int& count)
{
    static const char* mode[]  = { "POLY", "DUO", "UNISON", "MONO" };
    static const char* os[]    = { "1X", "2X", "4X" };
    static const char* oct[]   = { "32'", "16'", "8'", "4'", "2'" };
    static const char* fmode[] = { "CS 12 dB", "LADDER 24 dB" };
    static const char* ring[]  = { "OFF", "CARRIER", "RANKS" };
    static const char* lwave[] = { "SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H", "NOISE" };
    static const char* lmode[] = { "FREE", "RESET", "ONE" };
    static const char* drv[]   = { "OFF", "VALVE", "TAPE", "FUZZ", "FOLD" };
    static const char* ens[]   = { "OFF", "CHORUS I", "CHORUS II", "ENSEMBLE", "CATHEDRAL" };
    static const char* tape[]  = { "OFF", "TAPE", "VHS" };
    const std::string s (id);
    if (s == "mode")      { count = 4; return mode; }
    if (s == "os")        { count = 3; return os; }
    if (s == "a_oct" || s == "b_oct")     { count = 5; return oct; }
    if (s == "a_fmode" || s == "b_fmode") { count = 2; return fmode; }
    if (s == "ring_mode") { count = 3; return ring; }
    if (s == "lfo_wave")  { count = 7; return lwave; }
    if (s == "lfo_mode")  { count = 3; return lmode; }
    if (s == "drv_mode")  { count = 5; return drv; }
    if (s == "ens_mode")  { count = 5; return ens; }
    if (s == "tape_mode") { count = 3; return tape; }
    count = 0; return nullptr;
}

//==============================================================================
void Voice::reset()
{
    for (int r = 0; r < NUM_RANKS; ++r) rk[r].reset (rk[r].osc.ph);
    note = -1; gate = false; slot = -1; member = 0; pressure = 0.0f;
    lfoVal = lfoSH = lfoNz = lfoEnv = 0.0f; ringEnv = 0.0f; ringStage = 0; wmGate = 0.0f;
    outDc.reset(); ctrlPhase = 0;
}

//==============================================================================
// Drive
void Drive::prepare (double fs)
{
    tiltL.setHz (1200.0f, fs); tiltR.setHz (1200.0f, fs);
    fuzzHpL.setHz (150.0f, fs); fuzzHpR.setHz (150.0f, fs);
    dcL.setHz (10.0f, fs); dcR.setHz (10.0f, fs);
    // measured trims: a 0.35 sine through each shaper at 17 amounts
    for (int m = 0; m < NMODE; ++m)
        for (int j = 0; j < NPTS; ++j)
        {
            if (m == 0) { trim[(size_t) m][(size_t) j] = 1.0f; continue; }
            const float amt = (float) j / (NPTS - 1);
            const int N = 1024;
            double sumIn = 0, sumOut = 0, mean = 0;
            std::array<float, 1024> y {};
            for (int i = 0; i < N; ++i)
            {
                const float x = 0.35f * std::sin (6.2831853f * (float) i / 64.0f);
                y[(size_t) i] = shape (m, x, amt);
                mean += y[(size_t) i]; sumIn += x * x;
            }
            mean /= N;
            for (int i = 0; i < N; ++i) { const double d = y[(size_t) i] - mean; sumOut += d * d; }
            trim[(size_t) m][(size_t) j] = (float) std::sqrt (sumIn / std::max (1.0e-12, sumOut));
        }
    reset();
}

//==============================================================================
// Ensemble
void EnsembleFx::prepare (double fs)
{
    fsOs = (float) fs;
    dL.prepare ((int) (fs * 0.04) + 16);
    dR.prepare ((int) (fs * 0.04) + 16);
    bbdL.setHz (9000.0f, fs); bbdR.setHz (9000.0f, fs);
    reset();
}

void EnsembleFx::tick (int mode, float rate, float depth, float mix, float& l, float& r)
{
    if (mode <= 0 || mix <= 0.0005f) return;
    const float rateMul = xmap (rate, 0.5f, 2.0f);
    const float slowHz = (mode == 1 ? 0.8f : 0.55f) * rateMul * (mode == 4 ? 0.6f : 1.0f);
    const float fastHz = 6.2f * rateMul;
    phS += slowHz / fsOs; if (phS >= 1.0f) phS -= 1.0f;
    phF += fastHz / fsOs; if (phF >= 1.0f) phF -= 1.0f;
    const float ms = fsOs * 0.001f;
    const float base = 5.5f * ms;
    float dS = depth * 1.7f * ms, dF = depth * 0.28f * ms;
    int taps = 3;
    if (mode == 1)      { dS *= 1.1f; dF = 0.0f; taps = 2; }
    else if (mode == 2) { dS *= 0.25f; dF *= 1.6f; taps = 2; }
    else if (mode == 4) { dS *= 1.6f; dF *= 0.5f; }
    dL.push (l); dR.push (r);
    float wl = 0.0f, wr = 0.0f;
    for (int t = 0; t < taps; ++t)
    {
        const float phL = (float) t / (float) taps;
        const float phR = phL + 0.5f / (float) taps;
        const float offL = base + dS * fsin (phS + phL) + dF * fsin (phF + phL);
        const float offR = base + dS * fsin ((mode == 4 ? -phS : phS) + phR) + dF * fsin (phF + phR);
        wl += dL.readHermite (offL);
        wr += dR.readHermite (offR);
    }
    const float norm = taps == 3 ? 0.44f : 0.59f;      // taps^-0.75: between the amplitude and the power sum
    wl = bbdL.lp (wl * norm); wr = bbdR.lp (wr * norm);
    l = lerp (l, wl, mix); r = lerp (r, wr, mix);
}

//==============================================================================
// Choir — alto formants, U O A E I
namespace
{
    const float VF[5][5]  = { { 325, 700, 2530, 3500, 4950 }, { 450, 800, 2830, 3500, 4950 },
                              { 800, 1150, 2800, 3500, 4950 }, { 400, 1600, 2700, 3300, 4950 },
                              { 350, 1700, 2700, 3700, 4950 } };
    const float VBW[5][5] = { { 50, 60, 170, 180, 200 }, { 70, 80, 100, 130, 135 }, { 80, 90, 120, 130, 140 },
                              { 60, 80, 120, 150, 200 }, { 50, 100, 120, 150, 200 } };
    const float VDB[5][5] = { { 0, -12, -30, -40, -64 }, { 0, -9, -16, -28, -55 }, { 0, -4, -20, -36, -60 },
                              { 0, -24, -30, -35, -60 }, { 0, -20, -30, -36, -60 } };
}

void ChoirFx::prepare (double fs)
{
    fsOs = (float) fs;
    darkL.setHz (9000.0f, fs); darkR.setHz (9000.0f, fs);
    update (0.5f, 0.5f, 0.2f);
    reset();
}

void ChoirFx::update (float vowel, float reg, float air)
{
    const float pos = clamp01 (vowel) * 4.0f;
    const int i = std::min (3, (int) pos);
    const float f = pos - (float) i;
    const float regMul = std::pow (2.0f, (reg - 0.5f) * 1.0f);
    for (int k = 0; k < 5; ++k)
    {
        const float fq = std::exp (lerp (std::log (VF[i][k]), std::log (VF[i + 1][k]), f)) * regMul;
        const float bw = lerp (VBW[i][k], VBW[i + 1][k], f) * regMul;
        const float db = lerp (VDB[i][k], VDB[i + 1][k], f);
        bL[(size_t) k].bandpass (fq, bw, fsOs);
        bR[(size_t) k].bandpass (fq, bw, fsOs);
        amp[(size_t) k] = std::pow (10.0f, db / 20.0f);
    }
    airL.highShelf (6000.0f, air * 9.0f, fsOs);
    airR.highShelf (6000.0f, air * 9.0f, fsOs);
}

//==============================================================================
// Tape
namespace
{
    inline float satShape (float x, float sat)
    {
        const float u = x * (1.0f + 3.0f * sat) + 0.06f * sat;
        return ftanh (u) - ftanh (0.06f * sat);
    }
}

void TapeFx::prepare (double fs)
{
    fsOs = (float) fs;
    dL.prepare ((int) (fs * 0.06) + 16);
    dR.prepare ((int) (fs * 0.06) + 16);
    bumpL.lowShelf (90.0f, 2.0f, fs); bumpR.lowShelf (90.0f, 2.0f, fs);
    vhsPkL.peak (4500.0f, 3.0f, 1.5f, fs); vhsPkR.peak (4500.0f, 3.0f, 1.5f, fs);
    chromaL.bandpass (3000.0f, 3000.0f, fs); chromaR.bandpass (3000.0f, 3000.0f, fs);
    ageL.setHz (18000.0f, fs); ageR.setHz (18000.0f, fs); ageL2.setHz (18000.0f, fs); ageR2.setHz (18000.0f, fs);
    hissLp.setHz (8000.0f, fs);
    envAtk = 1.0f - std::exp (-1.0f / (0.005f * (float) fs));
    envRel = 1.0f - std::exp (-1.0f / (0.6f * (float) fs));
    dropLpL.setHz (1500.0f, fs); dropLpR.setHz (1500.0f, fs);
    wowLp.setHz (1.0f, fs);
    dcL.setHz (10.0f, fs); dcR.setHz (10.0f, fs);
    for (int j = 0; j < 17; ++j)
    {
        const float sat = (float) j / 16.0f;
        double sumIn = 0, sumOut = 0, mean = 0;
        std::array<float, 1024> y {};
        for (int i = 0; i < 1024; ++i)
        {
            const float x = 0.35f * std::sin (6.2831853f * (float) i / 64.0f);
            y[(size_t) i] = satShape (x, sat); mean += y[(size_t) i]; sumIn += x * x;
        }
        mean /= 1024;
        for (int i = 0; i < 1024; ++i) { const double d = y[(size_t) i] - mean; sumOut += d * d; }
        satTrim[j] = (float) std::sqrt (sumIn / std::max (1.0e-12, sumOut));
    }
    reset();
}

void TapeFx::reset()
{
    dL.reset(); dR.reset(); wowPh = 0.0f; walk = walkTgt = walkSm = 0.0f;
    fl1 = fl2 = fl3 = flOu = flOuTgt = 0.0f; vhsPh = 0.0f;
    dropEnv = dropTarget = 0.0f; dropLeft = 0; dropPos = 0.0f; dropLen = 1.0f;
    bumpL.reset(); bumpR.reset(); vhsPkL.reset(); vhsPkR.reset(); chromaL.reset(); chromaR.reset();
    ageL.reset(); ageR.reset(); ageL2.reset(); ageR2.reset(); hissLp.reset(); envF = 0.0f; dropLpL.reset(); dropLpR.reset(); wowLp.reset();
    dcL.reset(); dcR.reset(); tickEnv = 0.0f; uiWow = 0.0f; uiDrop = false;
    rng.seed (0xC0FFEEu);
}

void TapeFx::tick (int mode, float wow, float wowRate, float flut, float sat, float age, float drop, float hiss, float& l, float& r)
{
    if (mode <= 0) { uiWow = 0.0f; uiDrop = false; return; }
    const bool vhs = mode == 2;
    const float ms = fsOs * 0.001f;
    const float inv = 1.0f / fsOs;

    // ---- wow: a sine plus a slewed random walk (a wander never steps)
    wowPh += wowRate * inv; if (wowPh >= 1.0f) wowPh -= 1.0f;
    // the walk is an OU process stepped every 64 samples and smoothed at 1 Hz
    if (++walkCtr >= 64) { walkCtr = 0; walk += rng.bi() * 0.12f - walk * 0.02f; walk = clampf (walk, -1.0f, 1.0f); }
    walkSm = wowLp.lp (walk);
    const float wowAmt = wow * 2.5f * ms;
    const float wowL = wowAmt * (0.7f * fsin (wowPh) + 0.6f * walkSm);
    const float wowR = wowAmt * (0.7f * fsin (wowPh + 0.04f) + 0.6f * walkSm);

    // ---- flutter: three incommensurate sines plus a fast OU; VHS: a 50 Hz tracking saw
    float flutS = 0.0f;
    if (! vhs)
    {
        fl1 += 4.3f * inv; if (fl1 >= 1.0f) fl1 -= 1.0f;
        fl2 += 8.7f * inv; if (fl2 >= 1.0f) fl2 -= 1.0f;
        fl3 += 13.1f * inv; if (fl3 >= 1.0f) fl3 -= 1.0f;
        flOu += (flOuTgt - flOu) * 0.002f;
        if (((int) (fl1 * 1000.0f) & 15) == 0) flOuTgt = rng.bi();
        flutS = flut * 0.06f * ms * ((fsin (fl1) + 0.6f * fsin (fl2) + 0.4f * fsin (fl3)) * 0.5f + 0.4f * flOu);
    }
    else
    {
        vhsPh += 50.0f * inv;
        if (vhsPh >= 1.0f) { vhsPh -= 1.0f; tickEnv = 0.02f * flut; }
        flutS = flut * 0.03f * ms * (vhsPh * 2.0f - 1.0f);
        tickEnv *= 0.995f;
    }

    const float base = 12.0f * ms;
    {   // the follower: hiss belongs to a tape that is playing something
        const float a = std::max (std::abs (l), std::abs (r));
        envF += (a - envF) * (a > envF ? envAtk : envRel);
    }
    dL.push (l); dR.push (r);
    float xl = dL.readHermite (base + wowL + flutS);
    float xr = dR.readHermite (base + wowR + flutS);

    // ---- saturation with a head bump, level-trimmed by a measured table
    {
        const float ti = clamp01 (sat) * 16.0f; const int i = std::min (15, (int) ti); const float f = ti - (float) i;
        const float trim = lerp (satTrim[i], satTrim[i + 1], f);
        xl = dcL (satShape (xl, sat)) * trim;
        xr = dcR (satShape (xr, sat)) * trim;
        xl += sat * (bumpL (xl) - xl);
        xr += sat * (bumpR (xr) - xr);
    }
    // ---- age (corner set from the engine at control rate) and the VHS band
    xl = ageL2.lp (ageL.lp (xl)); xr = ageR2.lp (ageR.lp (xr));
    if (vhs) { xl = vhsPkL (xl); xr = vhsPkR (xr); }

    // ---- dropouts: Poisson events, raised-cosine dips with extra loss
    if (dropLeft <= 0 && drop > 0.0005f)
    {
        const float rate = drop * (vhs ? 4.5f : 1.5f);      // events per second
        if (rng.uni() < rate * inv)
        {
            dropLen = fsOs * (vhs ? (0.01f + 0.05f * rng.uni()) : (0.03f + 0.12f * rng.uni()));
            dropLeft = (int) dropLen; dropPos = 0.0f;
            dropTarget = 0.4f + 0.55f * rng.uni() * drop;
        }
    }
    float dg = 1.0f;
    if (dropLeft > 0)
    {
        --dropLeft; dropPos += 1.0f;
        const float w = 0.5f - 0.5f * std::cos (6.2831853f * dropPos / dropLen);
        dg = 1.0f - dropTarget * w;
        uiDrop = true;
    }
    else uiDrop = false;
    if (dg < 0.9999f)
    {
        xl = xl * dg + (1.0f - dg) * dropLpL.lp (xl) * 0.3f;
        xr = xr * dg + (1.0f - dg) * dropLpR.lp (xr) * 0.3f;
    }
    // ---- hiss, after everything and inside no loop
    const float hissGate = envF < 1.0e-3f ? 0.0f : std::min (1.0f, envF * 40.0f);
    const float hg = hiss * hiss * (0.15f + 0.85f * age) * (vhs ? 0.02f : 0.012f) * hissGate;
    if (hg > 1.0e-7f)
    {
        const float n = hissLp.lp (rng.bi()) * hg;
        if (vhs) { xl += chromaL (n) * 2.0f + n * 0.5f; xr += chromaR (rng.bi() * hg) * 2.0f + n * 0.5f; }
        else     { xl += n; xr += hissLp.lp (rng.bi()) * hg; }
    }
    if (vhs && tickEnv > 1.0e-6f) { const float t = tickEnv * rng.bi(); xl += t; xr += t; }
    uiWow = (wowL + flutS) / std::max (1.0e-3f, 2.5f * ms);
    l = xl; r = xr;
}

//==============================================================================
// Hall
void HallFx::prepare (double sr)
{
    fs = (float) sr;
    srScale = fs / 44100.0f;
    for (int i = 0; i < N; ++i)
    {
        line[i].prepare ((int) (baseLen[i] * 2.2f * srScale) + 64);
        damp[i].setHz (6000.0f, sr);
    }
    apL[0].prepare ((int) (apLen[0] * srScale) + 64); apL[1].prepare ((int) (apLen[1] * srScale) + 64);
    apR[0].prepare ((int) (apLen[2] * srScale) + 64); apR[1].prepare ((int) (apLen[3] * srScale) + 64);
    pre[0].prepare ((int) (sr * 0.26) + 16); pre[1].prepare ((int) (sr * 0.26) + 16);
    shimmer.prepare (sr);
    shHp.setHz (300.0f, sr);
    set (0.6f, 1.6f, 5000.0f, 0.0f);
    reset();
}

void HallFx::reset()
{
    for (auto& l : line) l.reset();
    for (auto& a : apL) a.reset(); for (auto& a : apR) a.reset();
    for (auto& p : pre) p.reset();
    for (auto& d : damp) d.reset();
    shimmer.reset(); shHp.reset(); modPh = 0.0f; mixSm = 0.0f;
}

void HallFx::set (float size, float rt60, float dampHz, float shim)
{
    const float sc = lerp (0.5f, 2.0f, clamp01 (size)) * srScale;
    for (int i = 0; i < N; ++i)
    {
        len[i] = baseLen[i] * sc;
        g[i] = std::pow (10.0f, -3.0f * len[i] / (std::max (0.05f, rt60) * fs));
        g[i] = std::min (g[i], 0.9995f);
        shimG[i] = clamp01 (shim) * (1.0f - g[i]) * 0.9f;
        damp[i].setHz (dampHz, fs);
    }
}

void HallFx::tick (float inL, float inR, float mod, float mix, float& outL, float& outR)
{
    mixSm += (mix - mixSm) * 0.0005f;
    if (mixSm < 0.0005f && mix < 0.0005f) { outL = inL; outR = inR; return; }
    pre[0].push (inL); pre[1].push (inR);
    float xl = pre[0].readInt (preSamples), xr = pre[1].readInt (preSamples);
    // diffusers
    auto ap = [] (Delay& d, float len, float x)
    {
        const float dl = d.readInt ((int) len);
        const float v = x - 0.62f * dl;
        d.push (v);
        return dl + 0.62f * v;
    };
    xl = ap (apL[0], apLen[0] * srScale, xl); xl = ap (apL[1], apLen[1] * srScale, xl);
    xr = ap (apR[0], apLen[2] * srScale, xr); xr = ap (apR[1], apLen[3] * srScale, xr);

    modPh += 0.7f / fs; if (modPh >= 1.0f) modPh -= 1.0f;
    const float m1 = fsin (modPh) * mod * 1.2f, m2 = fsin (modPh + 0.25f) * mod * 1.2f;

    float y[N];
    for (int i = 0; i < N; ++i)
    {
        float v;
        if (i == 1)      v = line[1].readHermite (len[1] + m1);
        else if (i == 4) v = line[4].readHermite (len[4] + m2);
        else             v = line[i].readInt ((int) len[i]);
        y[i] = damp[i].lp (v);
    }
    static const float sgn[N] = { 1, -1, -1, 1, 1, -1, 1, -1 };
    float t[N];
    for (int i = 0; i < 4; ++i) { t[i] = y[i] + y[i + 4]; t[i + 4] = y[i] - y[i + 4]; }
    for (int b = 0; b < 8; b += 4)
        for (int i = 0; i < 2; ++i)
        { const float a = t[b + i], c = t[b + i + 2]; t[b + i] = a + c; t[b + i + 2] = a - c; }
    for (int b = 0; b < 8; b += 2)
    { const float a = t[b], c = t[b + 1]; t[b] = a + c; t[b + 1] = a - c; }
    for (int i = 0; i < N; ++i) t[i] *= 0.35355339f * sgn[i];

    float sh = 0.0f;
    if (shimG[0] > 1.0e-5f) sh = shHp.hp (shimmer.tick ((y[0] + y[3] + y[5] + y[6]) * 0.25f));

    for (int i = 0; i < N; ++i)
        line[i].push (ceilSoft (((i & 1) ? xr : xl) + g[i] * t[i] + shimG[i] * sh, 4.0f));

    const float wl = (y[0] + y[2] + y[4] + y[6]) * 0.4f;
    const float wr = (y[1] + y[3] + y[5] + y[7]) * 0.4f;
    const float a = mixSm * 1.5707963f;
    const float cd = std::cos (a), cw = std::sin (a);
    outL = inL * cd + wl * cw;
    outR = inR * cd + wr * cw;
}

//==============================================================================
// Engine
void Engine::prepare (double sampleRate, int maxBlockIn)
{
    sr = sampleRate;
    maxBlock = std::max (16, maxBlockIn);
    osL.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);
    osR.assign ((size_t) (maxBlock * MAX_OS) + 8, 0.0f);
    const size_t nTicks = (size_t) (maxBlock * MAX_OS / CTRL) + 4;
    bendBuf.assign (nTicks, 0.0f); wheelBuf.assign (nTicks, 0.0f); atBuf.assign (nTicks, 0.0f);
    dnL1.prepare(); dnR1.prepare(); dnL2.prepare(); dnR2.prepare();
    // fixed tolerances: eight machines
    for (int vi = 0; vi < MAX_VOICES; ++vi)
    {
        Voice& v = voices[(size_t) vi];
        v.rng.seed (0x1984u + (uint32_t) vi * 7919u);
        for (int r = 0; r < NUM_RANKS; ++r)
        {
            Rank& k = v.rk[r];
            k.tol.cents = v.rng.bi() * 5.0f;
            k.tol.cut = v.rng.bi() * 0.15f;
            k.tol.env = 1.0f + v.rng.bi() * 0.12f;
            k.tol.driftPh = v.rng.uni();
            k.osc.reset (v.rng.uni());
        }
        v.lfoPh = v.rng.uni();
        v.reset();
    }
    hissRng.seed (0xBEEFu);
    lfoRng.seed (0x1984u);
    hall.prepare (sr);
    outDcL.setHz (5.0f, sr); outDcR.setHz (5.0f, sr);
    lastOsParam = -1;
    configureRate();
    reset();
}

void Engine::configureRate()
{
    const int o = (int) std::round (clampf (p.os, 0.0f, 2.0f));
    osf = o == 0 ? 1 : (o == 1 ? 2 : 4);
    fsOs = sr * osf;
    lastOsParam = o;
    perfK = 1.0f - std::exp (-(float) CTRL / ((float) fsOs * 0.008f));   // 8 ms
    drive.prepare (fsOs);
    ens.prepare (fsOs);
    choir.prepare (fsOs);
    tape.prepare (fsOs);
    dnL1.reset(); dnR1.reset(); dnL2.reset(); dnR2.reset();
    for (auto& v : voices) v.reset();
}

void Engine::reset()
{
    for (auto& v : voices) v.reset();
    heldN = 0; sustain = false; heldKeys.fill (false); susKeys.fill (false); polyAt.fill (0.0f);
    drive.reset(); ens.reset(); choir.reset(); tape.reset(); hall.reset();
    dnL1.reset(); dnR1.reset(); dnL2.reset(); dnR2.reset();
    outDcL.reset(); outDcR.reset();
    scope.fill (0.0f); scopeWrite = 0; scopeHold = 0; scopePrev = 0.0f;
    rmsAcc = peakAcc = 0.0f; rmsN = 0; outRms = outPeak = 0.0f;
    uiNotes.fill (-1); uiLevel.fill (0.0f); heldCount = 0;
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (const auto& v : voices) if (v.sounding()) ++n;
    return n;
}

int Engine::newestVoice() const
{
    int best = 0, order = -1;
    for (int i = 0; i < MAX_VOICES; ++i) if (voices[(size_t) i].order > order) { order = voices[(size_t) i].order; best = i; }
    return best;
}

//==============================================================================
// notes
int Engine::voicesPerNote() const
{
    switch (modeI) { case 1: return 2; case 2: return MAX_VOICES; default: return 1; }
}

void Engine::startVoice (Voice& v, int note, float vel, int slot, int member, int vpn, bool retrigger, float fromPitch)
{
    const bool fresh = ! v.sounding();
    v.note = note; v.vel = vel; v.gate = true; v.order = ++noteSerial; v.slot = slot; v.member = member;
    v.onAt = samplesDone;
    v.pitchTarget = (float) note;
    if (retrigger || fresh)
    {
        v.pitch = glideK >= 1.0f ? (float) note : fromPitch;
        for (int r = 0; r < NUM_RANKS; ++r) { v.rk[r].feg.gate (true); v.rk[r].aeg.gate (true); }
        v.ringEnv = 0.0f; v.ringStage = 1;
        if (lfoMode == 1) v.lfoPh = 0.0f;
        v.lfoEnv = 0.0f;
        if (fresh) { v.wmGate = 0.0f; v.pressure = 0.0f; }
    }
    // the fan of this slot: detune and pan
    const float fan = vpn > 1 ? ((float) member - (float) (vpn - 1) * 0.5f) / ((float) (vpn - 1) * 0.5f) : 0.0f;
    v.detCents = fan * p.unidet * 60.0f;
    const int vi = (int) (&v - &voices[0]);
    const float seat = -1.0f + 2.0f * (float) vi / (float) (MAX_VOICES - 1);
    v.panTarget = clampf (vpn > 1 ? fan * p.spread : seat * p.spread, -1.0f, 1.0f);
    if (fresh) v.pan = v.panTarget;
}

int Engine::allocSlot (int note)
{
    const int vpn = voicesPerNote();
    const int nSlots = MAX_VOICES / vpn;
    // the same key still sounding: take it back
    for (int s = 0; s < nSlots; ++s)
        if (voices[(size_t) (s * vpn)].note == note && voices[(size_t) (s * vpn)].sounding()) return s;
    // a silent slot
    for (int s = 0; s < nSlots; ++s)
    {
        bool silent = true;
        for (int m = 0; m < vpn; ++m) if (voices[(size_t) (s * vpn + m)].sounding()) { silent = false; break; }
        if (silent) return s;
    }
    // a released slot with the lowest level, else the oldest
    int best = -1; float bestLvl = 1.0e9f;
    for (int s = 0; s < nSlots; ++s)
    {
        const Voice& v = voices[(size_t) (s * vpn)];
        if (v.gate) continue;
        const float lv = std::max (v.rk[0].aeg.v, v.rk[1].aeg.v);
        if (lv < bestLvl) { bestLvl = lv; best = s; }
    }
    if (best >= 0) return best;
    int oldest = 0; int lowOrder = 1 << 30;
    for (int s = 0; s < nSlots; ++s)
        if (voices[(size_t) (s * vpn)].order < lowOrder) { lowOrder = voices[(size_t) (s * vpn)].order; oldest = s; }
    return oldest;
}

void Engine::noteOn (int note, float vel)
{
    if (note < 0 || note > 127) return;
    deriveBlock();       // the mode and the glide are decided here, not by the last block
    vel = clampf (vel, 0.02f, 1.0f);
    const bool wasHeld = heldN > 0;
    // the held stack (mono memory)
    for (int i = 0; i < heldN; ++i) if (heldStack[(size_t) i] == note) { for (int j = i; j < heldN - 1; ++j) heldStack[(size_t) j] = heldStack[(size_t) (j + 1)]; --heldN; break; }
    if (heldN < 32) heldStack[(size_t) heldN++] = note;
    heldKeys[(size_t) note] = true; susKeys[(size_t) note] = false;

    const int vpn = voicesPerNote();
    if (modeI == 2 || modeI == 3)     // UNISON, MONO: slot 0 always
    {
        const bool legato = wasHeld && p.legato >= 0.5f && voices[0].sounding();
        const float from = voices[0].sounding() ? voices[0].pitch : lastMonoPitch;
        const int members = modeI == 2 ? MAX_VOICES : 1;
        for (int m = 0; m < members; ++m)
            startVoice (voices[(size_t) m], note, vel, 0, m, members, ! legato, from);
        // in mono the other voices must be silent
        if (modeI == 3) for (int m = 1; m < MAX_VOICES; ++m) if (voices[(size_t) m].sounding()) { voices[(size_t) m].gate = false; voices[(size_t) m].rk[0].aeg.gate (false); voices[(size_t) m].rk[1].aeg.gate (false); voices[(size_t) m].rk[0].feg.gate (false); voices[(size_t) m].rk[1].feg.gate (false); }
    }
    else
    {
        const int slot = allocSlot (note);
        const float from = glideK >= 1.0f ? (float) note : lastMonoPitch;
        for (int m = 0; m < vpn; ++m)
            startVoice (voices[(size_t) (slot * vpn + m)], note, vel, slot, m, vpn, true, from);
    }
    lastMonoPitch = (float) note;
}

void Engine::noteOff (int note)
{
    if (note < 0 || note > 127) return;
    heldKeys[(size_t) note] = false;
    for (int i = 0; i < heldN; ++i) if (heldStack[(size_t) i] == note) { for (int j = i; j < heldN - 1; ++j) heldStack[(size_t) j] = heldStack[(size_t) (j + 1)]; --heldN; break; }
    if (sustain) { susKeys[(size_t) note] = true; return; }

    if (modeI == 2 || modeI == 3)
    {
        if (voices[0].note != note) return;
        if (heldN > 0)   // return to the most recent held key
        {
            const int prev = heldStack[(size_t) (heldN - 1)];
            const bool legato = p.legato >= 0.5f;
            const int members = modeI == 2 ? MAX_VOICES : 1;
            for (int m = 0; m < members; ++m)
                startVoice (voices[(size_t) m], prev, voices[0].vel, 0, m, members, ! legato, voices[0].pitch);
            return;
        }
        for (auto& v : voices) if (v.gate) { v.gate = false; for (auto& k : v.rk) { k.aeg.gate (false); k.feg.gate (false); } }
        return;
    }
    for (auto& v : voices)
        if (v.gate && v.note == note) { v.gate = false; for (auto& k : v.rk) { k.aeg.gate (false); k.feg.gate (false); } }
}

void Engine::allNotesOff()
{
    sustain = false;   // a panic lets go of the pedal, or it holds the next note
    heldN = 0; heldKeys.fill (false); susKeys.fill (false);
    for (auto& v : voices) if (v.gate) { v.gate = false; for (auto& k : v.rk) { k.aeg.gate (false); k.feg.gate (false); } }
}

void Engine::setSustain (bool on)
{
    sustain = on;
    if (! on)
        for (int n = 0; n < 128; ++n)
            if (susKeys[(size_t) n]) { susKeys[(size_t) n] = false; if (! heldKeys[(size_t) n]) noteOff (n); }
}

//==============================================================================
/*  Per-voice control tick: pitch, filter coefficients, amplitudes. Every
    CTRL oversampled samples. */
void Engine::controlTick (Voice& v, int vi, int tick)
{
    const float bendSm = bendBuf[(size_t) tick], wheelSm = wheelBuf[(size_t) tick], atSm = atBuf[(size_t) tick];
    const float dt = (float) CTRL / (float) fsOs;
    // glide
    if (glideK >= 1.0f) v.pitch = v.pitchTarget;
    else v.pitch += (v.pitchTarget - v.pitch) * glideK;
    const float pitchNote = gliss ? std::round (v.pitch) : v.pitch;
    // touch
    const float prTarget = std::max (atSm, polyAt[(size_t) std::max (0, v.note)]);
    v.pressure += (prTarget - v.pressure) * 0.08f;
    v.wmGate += ((v.gate ? 1.0f : 0.0f) - v.wmGate) * 0.03f;
    v.pan += (v.panTarget - v.pan) * 0.05f;

    // ---- the sub-oscillator
    float lfo;
    if (lfoMode == 2) lfo = gLfoVal;
    else
    {
        v.lfoPh += lfoInc * CTRL;
        bool wrapped = false;
        if (v.lfoPh >= 1.0f) { v.lfoPh -= 1.0f; wrapped = true; }
        const float ph = v.lfoPh;
        switch (lfoWave)
        {
            case 0: lfo = fsin (ph); break;
            case 1: lfo = ph < 0.5f ? -1.0f + 4.0f * ph : 3.0f - 4.0f * ph; break;
            case 2: lfo = 1.0f - 2.0f * ph; break;
            case 3: lfo = 2.0f * ph - 1.0f; break;
            case 4: lfo = ph < 0.5f ? 1.0f : -1.0f; break;
            case 5: if (wrapped || v.lfoSH == 0.0f) v.lfoSH = v.rng.bi(); lfo = v.lfoSH; break;
            default: if (wrapped) v.lfoSH = v.rng.bi(); v.lfoNz += (v.lfoSH - v.lfoNz) * std::min (1.0f, lfoInc * CTRL * 6.0f); lfo = v.lfoNz; break;
        }
    }
    v.lfoEnv = std::min (1.0f, v.lfoEnv + lfoDelayK);
    v.lfoVal = lfo;
    const float lfoDepthMul = v.lfoEnv;
    const float vibCents = lfoPitchCents * lfoDepthMul + p.wheel_lfo * wheelSm * 100.0f + p.at_lfo * v.pressure * 100.0f;

    // ---- world mod
    float wmDetFan = 0.0f, wmSagSemis = 0.0f, wmTrem = 1.0f, wmFiltOct = 0.0f, wmPanAdd = 0.0f;
    if (wmActive)
    {
        const float fan = std::fmod ((float) vi * 0.618f + 0.5f, 1.0f) * 2.0f - 1.0f;
        wmDetFan = wmDet * fan;
        wmSagSemis = wmSag * (1.0f - v.wmGate);
        wmPanAdd = wmPan * fan;
        if (wmTremD > 0.0f)
        {
            const float tph = (float) (wmT * wmTremR) + (float) vi * 0.618f;
            wmTrem = 1.0f - wmTremD * 0.5f * (1.0f - fsin (tph));
        }
        wmFiltOct = std::log2 (std::max (0.01f, wmFmul));
    }

    const float semisCommon = pitchNote + tuneSemis + bendSm * bendRange + p.at_pitch * v.pressure * 2.0f
                            + v.lfoVal * vibCents * 0.01f + (v.detCents + wmDetFan) * 0.01f - wmSagSemis;
    const float noteHz = midiHz (semisCommon);
    const float velOct = (v.vel * 2.0f - 1.0f) * 1.5f;
    const float brillCommon = bip (p.brill) * 3.0f + bip (p.wheel_brill) * wheelSm * 2.0f + p.at_brill * v.pressure * 2.0f
                            + p.lfo_vcf * v.lfoVal * 3.0f + wmFiltOct;
    const float keyOct = (pitchNote - 60.0f) / 12.0f;
    const float resoAdd = bip (p.reso) * 0.5f;
    const float pmFilt = p.pm_o2filt * v.rk[1].lastPre * 4.0f;

    for (int r = 0; r < NUM_RANKS; ++r)
    {
        Rank& k = v.rk[r];
        const RankBlock& b = rb[r];
        // drift: an OU walk, cents, tau 3 s, amplitude by VINTAGE
        const float tau = 3.0f;
        k.driftA += v.rng.bi() * std::sqrt (dt / tau) * 6.0f * p.vintage - k.driftA * (dt / tau);
        k.jit = v.rng.bi() * 0.3f * p.vintage;
        const float fegVal = k.feg.v;
        float semis = semisCommon + b.semis + (k.tol.cents * p.vintage + k.driftA + k.jit) * 0.01f;
        if (r == 0) semis += bip (p.pm_envpitch) * fegVal * 24.0f;
        k.pitchNorm = midiHz (semis) * b.octMul / (float) fsOs;
        float pw = b.pw + (b.pwm * v.lfoVal + p.lfo_pw * v.lfoVal) * 0.45f;
        if (r == 0) pw += bip (p.pm_envpw) * fegVal * 0.45f;
        k.pwHeld = clampf (pw, 0.03f, 0.97f);
        // filters
        const float bOct = brillCommon + b.velb * velOct + k.tol.cut * p.vintage;
        const float lpHz = clampf (b.lpfHz * fexp2 (fegVal * 6.0f + p.ktrack * keyOct + bOct + pmFilt), 8.0f, (float) fsOs * 0.45f);
        const float hpHz = clampf (b.hpfHz * fexp2 (fegVal * 4.0f + p.ktrack * keyOct * 0.5f + bOct * 0.5f), 5.0f, (float) fsOs * 0.45f);
        const float gl = std::tan (3.14159265f * lpHz / (float) fsOs);
        const float gh = std::tan (3.14159265f * hpHz / (float) fsOs);
        k.resLp = clampf (b.lpq + resoAdd, 0.0f, 1.0f);
        k.resHp = clampf (b.hpq + resoAdd * 0.5f, 0.0f, 1.0f);
        k.hp.setG (gh, k.resHp);
        if (b.ladder) { k.lad.setG (gl * 0.9965f); k.kLad = 4.3f * std::pow (k.resLp, 0.9f); }
        else          k.lp.setG (gl, k.resLp);
        // amplitude: level, velocity, tremolo, touch, world trem, headroom
        const float lvlVel = lerp (1.0f, v.vel, b.vel);
        const float trem = 1.0f - p.lfo_vca * (0.5f - 0.5f * v.lfoVal) * lfoDepthMul;
        k.ampHeld = b.lvl * lvlVel * trem * (1.0f + p.at_lvl * v.pressure) * wmTrem * 0.4f;
        if (vi == 0 || uiNotes[(size_t) vi] == v.note) { uiCut[(size_t) r] = lpHz; uiFeg[(size_t) r] = fegVal; }
    }
    // ring carrier
    if (ringMode == 1)
    {
        // AD envelope on the carrier frequency
        const float ka = 1.0f - std::exp (-1.466f * dt / std::max (0.001f, msOf (paramSpec (paramIndex ("ring_a")), p.ring_a) * 0.001f));
        const float kd = 1.0f - std::exp (-4.0f * dt / std::max (0.001f, msOf (paramSpec (paramIndex ("ring_d")), p.ring_d) * 0.001f));
        if (v.ringStage == 1) { v.ringEnv += (1.3f - v.ringEnv) * ka; if (v.ringEnv >= 1.0f) { v.ringEnv = 1.0f; v.ringStage = 2; } }
        else if (v.ringStage == 2) { v.ringEnv += (0.0f - v.ringEnv) * kd; if (v.ringEnv < 1.0e-4f) { v.ringEnv = 0.0f; v.ringStage = 0; } }
        const float hz = (p.ring_key >= 0.5f ? noteHz * ringRatio : ringHz) * fexp2 (bip (p.ring_mod) * 4.0f * v.ringEnv);
        ringIncHeld[(size_t) vi] = clampf (hz / (float) fsOs, 0.0f, 0.45f);
    }
    // pan gains
    for (int r = 0; r < NUM_RANKS; ++r)
    {
        const float pn = clampf (v.pan + rb[r].pan + wmPanAdd, -1.0f, 1.0f);
        const float a = (pn + 1.0f) * 0.78539816f;
        panG[(size_t) vi][(size_t) r][0] = std::cos (a);
        panG[(size_t) vi][(size_t) r][1] = std::sin (a);
    }
    if (vi == 0) uiLfo = v.lfoVal;
}

void Engine::renderVoice (Voice& v, int vi, float* outL, float* outR, int nOs)
{
    // envelope coefficients once per block
    for (int r = 0; r < NUM_RANKS; ++r)
    {
        Rank& k = v.rk[r];
        const RankBlock& b = rb[r];
        const float te = k.tol.env;
        k.feg.set (b.il, b.al, b.fa * te, b.fd * te, b.fr * te, fsOs);
        k.aeg.set (b.va * te, b.vd * te, b.vs, b.vr * te, fsOs);
    }
    const float ringDepth = ringMode > 0 ? p.ring_depth : 0.0f;
    const float pmPitch = p.pm_o2pitch, pmPw = p.pm_o2pw;
    const bool  sync = rb[1].sync;

    for (int i = 0; i < nOs; ++i)
    {
        if ((v.ctrlPhase++ & (CTRL - 1)) == 0) controlTick (v, vi, i / CTRL);
        const float nz = v.rng.bi();
        float o[NUM_RANKS];
        float syncF = -1.0f;
        /*  Rank II first: it is the sync MASTER and the poly-mod source. Rank I
            is the slave and the poly-mod destination, so the filter envelope
            sweeps the synced rank - the Prophet's topology, where the audible
            oscillator is the one that is reset. */
        for (int rr = 0; rr < NUM_RANKS; ++rr)
        {
            const int r = NUM_RANKS - 1 - rr;
            Rank& k = v.rk[r];
            const RankBlock& b = rb[r];
            float inc = k.pitchNorm, pw = k.pwHeld;
            if (r == 0)
            {
                const float m2 = v.rk[1].lastPre;
                if (pmPitch > 0.0f) inc *= fexp2 (pmPitch * m2 * 2.0f);
                if (pmPw > 0.0f)    pw = clampf (pw + pmPw * m2 * 0.4f, 0.03f, 0.97f);
            }
            k.osc.tick (inc, pw, r == 0 && sync ? syncF : -1.0f);
            if (r == 1) syncF = k.osc.wrapF;
            const float pre = k.osc.saw * b.saw + k.osc.pulse * b.pulse + k.osc.tri * b.tri + nz * b.noise;
            k.lastPre = pre;
            float y = k.hp.highpass (pre);
            y = b.ladder ? k.lad.tick (y, k.kLad) : k.lp.lowpass (y);
            k.feg.tick();
            const float a = k.aeg.tick();
            const float out = (y + k.osc.sine * b.sine) * a * k.ampHeld;
            k.lastOut = out;
            o[r] = out;
        }
        // ring modulator
        if (ringDepth > 0.0005f)
        {
            if (ringMode == 1)
            {
                v.ringPh += ringIncHeld[(size_t) vi]; if (v.ringPh >= 1.0f) v.ringPh -= 1.0f;
                const float c = fsin (v.ringPh);
                o[0] = lerp (o[0], o[0] * c, ringDepth);
                o[1] = lerp (o[1], o[1] * c, ringDepth);
            }
            else
            {
                const float m = o[0] * o[1] * (1.0f / 0.4f) * 1.5f;
                o[0] = lerp (o[0], m, ringDepth);
                o[1] = o[1] * (1.0f - ringDepth);
            }
        }
        const float sum = v.outDc (o[0] + o[1]);
        (void) sum;
        outL[i] += o[0] * panG[(size_t) vi][0][0] + o[1] * panG[(size_t) vi][1][0];
        outR[i] += o[0] * panG[(size_t) vi][0][1] + o[1] * panG[(size_t) vi][1][1];
    }
    uiLevel[(size_t) vi] = std::max (v.rk[0].aeg.v, v.rk[1].aeg.v);
}

//==============================================================================
void Engine::deriveBlock()
{
    // ---- per-block derived values
    modeI = (int) std::round (clampf (p.mode, 0.0f, 3.0f));
    ringMode = (int) std::round (clampf (p.ring_mode, 0.0f, 2.0f));
    lfoWave = (int) std::round (clampf (p.lfo_wave, 0.0f, 6.0f));
    lfoMode = (int) std::round (clampf (p.lfo_mode, 0.0f, 2.0f));
    gliss = p.gliss >= 0.5f;
    bendRange = std::round (p.bend);
    tuneSemis = semiOf (p.tune) + (p.fine - 0.5f);
    {
        const float gm = glideMs (p.glide);
        glideK = gm <= 0.0f ? 1.0f : 1.0f - std::exp (-(float) CTRL / ((float) fsOs * gm * 0.001f * 0.33f));
        const float lm = glideMs (p.lfo_delay);   // same 0 = off mapping, 10..5000 ms
        const float lmMs = p.lfo_delay < 0.01f ? 0.0f : xmap (p.lfo_delay, 10.0f, 5000.0f);
        lfoDelayK = lmMs <= 0.0f ? 1.0f : (float) CTRL / ((float) fsOs * lmMs * 0.001f);
        (void) lm;
    }
    lfoInc = xmap (p.lfo_rate, 0.05f, 40.0f) / (float) fsOs;
    lfoPitchCents = p.lfo_pitch * p.lfo_pitch * 1200.0f;
    ringHz = xmap (p.ring_speed, 0.1f, 4000.0f);
    ringRatio = xmap (p.ring_speed, 0.25f, 8.0f);
    wheelLfoInc = xmap (p.wheel_rate, 2.0f, 10.0f) / (float) sr;
    for (int r = 0; r < NUM_RANKS; ++r)
    {
        const RankParams& q = p.rk[r];
        RankBlock& b = rb[r];
        b.octMul = std::pow (2.0f, std::round (clampf (q.oct, 0.0f, 4.0f)) - 2.0f);
        b.semis = semiOf (q.semi) + (q.fine - 0.5f);
        b.hpfHz = xmap (q.hpf, 10.0f, 12000.0f); b.lpfHz = xmap (q.lpf, 20.0f, 20000.0f);
        b.hpq = q.hpq; b.lpq = q.lpq; b.ladder = q.fmode >= 0.5f;
        b.il = bip (q.il); b.al = bip (q.al);
        b.fa = xmap (q.fa, 1.0f, 10000.0f); b.fd = xmap (q.fd, 2.0f, 20000.0f); b.fr = xmap (q.fr, 2.0f, 20000.0f);
        b.va = xmap (q.va, 1.0f, 10000.0f); b.vd = xmap (q.vd, 2.0f, 20000.0f); b.vs = q.vs; b.vr = xmap (q.vr, 2.0f, 20000.0f);
        b.lvl = q.lvl; b.pan = bip (q.pan); b.vel = q.vel; b.velb = q.velb; b.sync = q.sync >= 0.5f;
        b.pw = 0.5f + clamp01 (q.pw) * 0.45f; b.pwm = q.pwm;
        b.saw = q.saw; b.pulse = q.pulse; b.tri = q.tri; b.sine = q.sine; b.noise = q.noise;
    }
}

void Engine::process (float* L, float* R, int n)
{
    if (n <= 0) return;
    if ((int) std::round (clampf (p.os, 0.0f, 2.0f)) != lastOsParam) configureRate();
    if (n > maxBlock) { process (L, R, maxBlock); process (L + maxBlock, R + maxBlock, n - maxBlock); return; }
    deriveBlock();
    // world mod
    {
        const float det = wmIn[0].load (std::memory_order_relaxed), pan = wmIn[1].load (std::memory_order_relaxed),
                    td = wmIn[2].load (std::memory_order_relaxed), tr = wmIn[3].load (std::memory_order_relaxed),
                    sag = wmIn[4].load (std::memory_order_relaxed), fm = wmIn[5].load (std::memory_order_relaxed);
        wmActive = ! (det == 0.0f && pan == 0.0f && td == 0.0f && sag == 0.0f && fm == 1.0f);
        wmDet = det; wmPan = pan; wmTremD = td; wmTremR = tr; wmSag = sag; wmFmul = fm;
    }
    // performance controls, smoothed at the control rate (8 ms), one value per tick
    {
        const int nTicks = (n * osf) / CTRL + 1;
        for (int t = 0; t < nTicks; ++t)
        {
            bendSm += (bendIn - bendSm) * perfK;
            wheelSm += (wheelIn - wheelSm) * perfK;
            atSm += (atIn - atSm) * perfK;
            bendBuf[(size_t) t] = bendSm; wheelBuf[(size_t) t] = wheelSm; atBuf[(size_t) t] = atSm;
        }
    }
    // the global sub-oscillator (ONE mode), advanced per block at the control rate
    {
        const float adv = lfoInc * (float) (n * osf);
        gLfoPh += adv; bool wrapped = false; if (gLfoPh >= 1.0f) { gLfoPh -= std::floor (gLfoPh); wrapped = true; }
        switch (lfoWave)
        {
            case 0: gLfoVal = fsin (gLfoPh); break;
            case 1: gLfoVal = gLfoPh < 0.5f ? -1.0f + 4.0f * gLfoPh : 3.0f - 4.0f * gLfoPh; break;
            case 2: gLfoVal = 1.0f - 2.0f * gLfoPh; break;
            case 3: gLfoVal = 2.0f * gLfoPh - 1.0f; break;
            case 4: gLfoVal = gLfoPh < 0.5f ? 1.0f : -1.0f; break;
            case 5: if (wrapped) gLfoSH = lfoRng.bi(); gLfoVal = gLfoSH; break;
            default: if (wrapped) gLfoSH = lfoRng.bi(); gLfoNz += (gLfoSH - gLfoNz) * std::min (1.0f, adv * 6.0f); gLfoVal = gLfoNz; break;
        }
    }
    hall.preSamples = (int) (xmap (p.hall_pre, 1.0f, 250.0f) * 0.001f * (float) sr);
    {
        const float rt = xmap (p.hall_decay, 0.2f, 20.0f), dh = xmap (p.hall_damp, 1000.0f, 16000.0f);
        if (p.hall_size != hallLast[0] || rt != hallLast[1] || dh != hallLast[2] || p.hall_shim != hallLast[3])
        { hall.set (p.hall_size, rt, dh, p.hall_shim); hallLast[0] = p.hall_size; hallLast[1] = rt; hallLast[2] = dh; hallLast[3] = p.hall_shim; }
    }

    // ---- voices, oversampled
    const int nOs = n * osf;
    std::fill (osL.begin(), osL.begin() + nOs, 0.0f);
    std::fill (osR.begin(), osR.begin() + nOs, 0.0f);
    heldCount = 0;
    for (int vi = 0; vi < MAX_VOICES; ++vi)
    {
        Voice& v = voices[(size_t) vi];
        if (v.sounding()) { renderVoice (v, vi, osL.data(), osR.data(), nOs); uiNotes[(size_t) vi] = v.note; if (v.gate) ++heldCount; }
        else { uiNotes[(size_t) vi] = -1; uiLevel[(size_t) vi] = 0.0f; }
    }
    samplesDone += (uint64_t) nOs;
    wmT += (double) n / sr;

    // ---- the chain, oversampled
    const int drvMode = (int) std::round (clampf (p.drv_mode, 0.0f, 4.0f));
    const int ensMode = (int) std::round (clampf (p.ens_mode, 0.0f, 4.0f));
    const int tapeMode = (int) std::round (clampf (p.tape_mode, 0.0f, 2.0f));
    const float wowRate = xmap (p.tape_wowrate, 0.1f, 3.0f);
    for (int i = 0; i < nOs; ++i)
    {
        float l = osL[(size_t) i], r = osR[(size_t) i];
        if ((fxCtr++ & 63) == 0)
        {
            if (p.choir_mix > 0.0005f) choir.update (p.choir_vowel, p.choir_reg, p.choir_air);
            if (tapeMode > 0)
            {
                float hz = xmap (1.0f - p.tape_age, 3500.0f, 20000.0f);
                if (tapeMode == 2) hz = std::min (hz, 6000.0f);
                tape.ageL.setHz (hz, fsOs); tape.ageR.setHz (hz, fsOs); tape.ageL2.setHz (hz, fsOs); tape.ageR2.setHz (hz, fsOs);
            }
        }
        drive.tick (drvMode, p.drv_amt, p.drv_tone, l, r);
        ens.tick (ensMode, p.ens_rate, p.ens_depth, p.ens_mix, l, r);
        choir.tick (p.choir_mix, l, r);
        tape.tick (tapeMode, p.tape_wow, wowRate, p.tape_flut, p.tape_sat, p.tape_age, p.tape_drop, p.tape_hiss, l, r);
        osL[(size_t) i] = l; osR[(size_t) i] = r;
    }
    uiWow = tape.uiWow; uiDrop = tape.uiDrop;

    // ---- decimate
    if (osf == 1)      { for (int i = 0; i < n; ++i) { L[i] = osL[(size_t) i]; R[i] = osR[(size_t) i]; } }
    else if (osf == 2) { for (int i = 0; i < n; ++i) { L[i] = dnL1 (osL[(size_t) (2 * i)], osL[(size_t) (2 * i + 1)]); R[i] = dnR1 (osR[(size_t) (2 * i)], osR[(size_t) (2 * i + 1)]); } }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const float la = dnL1 (osL[(size_t) (4 * i)], osL[(size_t) (4 * i + 1)]), lb = dnL1 (osL[(size_t) (4 * i + 2)], osL[(size_t) (4 * i + 3)]);
            const float ra = dnR1 (osR[(size_t) (4 * i)], osR[(size_t) (4 * i + 1)]), rb2 = dnR1 (osR[(size_t) (4 * i + 2)], osR[(size_t) (4 * i + 3)]);
            L[i] = dnL2 (la, lb); R[i] = dnR2 (ra, rb2);
        }
    }

    // ---- hall, master
    const float vol = volGain (p.volume);
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        hall.tick (l, r, p.hall_mod, p.hall_mix, l, r);
        l = ceilSoft (outDcL (l * vol), 1.0f);
        r = ceilSoft (outDcR (r * vol), 1.0f);
        L[i] = clean (l); R[i] = clean (r);
        // meters and scope
        const float m = 0.5f * (L[i] + R[i]);
        rmsAcc += m * m; peakAcc = std::max (peakAcc, std::abs (m));
        if (++rmsN >= 2048) { outRms = rmsAcc / (float) rmsN; outPeak = peakAcc; rmsAcc = 0.0f; peakAcc = 0.0f; rmsN = 0; }
        scope[(size_t) scopeWrite] = m; scopeWrite = (scopeWrite + 1) & (SCOPE_N - 1);
    }
}

} // namespace n84
