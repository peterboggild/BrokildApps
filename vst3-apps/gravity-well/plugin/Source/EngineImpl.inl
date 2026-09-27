/*  GRAVITY WELL - the engine's implementation.
    Included from Engine.cpp; not a translation unit of its own.            */

namespace gw {

enum Src { S_OFF, S_ENV1, S_ENV2, S_ENV3, S_LFO1, S_LFO2, S_LFO3, S_RND,
           S_CHIRP, S_MASS, S_VEL, S_KEY, S_WHEEL, S_PRESS, S_ACCENT, S_SEQ, N_SRC };
enum Dst { D_OFF, D_PITCH, D_ASHAPE, D_BSHAPE, D_AWIDTH, D_PM, D_RING, D_SUB, D_NOISE,
           D_ACUT, D_BCUT, D_ARES, D_BRES, D_SPLIT, D_BLEND, D_DISK, D_DRIVE,
           D_MASS, D_REDSHIFT, D_RINGDOWN, D_PAN, D_LEVEL, N_DST };

static const int OS = 4;                 // everything nonlinear runs here
static const int CTRL = 16;              // control rate, in oversampled samples

// --------------------------------------------------------------- the drive
/*  Four shapers.  Each is made POWER NEUTRAL by a trim MEASURED at prepare:
    a table written by hand goes stale the first time a curve is retuned, and
    then turning DRIVE up quietly turns the instrument up, which is the thing
    Peter asked to avoid.  (Battlestar's rule, and its measurement.)         */
static inline float shape (int type, float x, float k) {
    switch (type) {
        case 1:  return fastTanh (x * (1.0f + 7.0f * k));                       // ION
        case 2: { float d = x * (1.0f + 11.0f * k);                             // PLASMA
                  return fastTanh (d + 0.35f * d * d / (1.0f + std::fabs (d))); }
        case 3: { float d = x * (1.0f + 16.0f * k);                             // COLLAPSE
                  float f = std::sin (d * 1.4f);
                  return lerpf (fastTanh (d), f, clampf (k, 0.f, 1.f)); }
        default: { float d = x * (1.0f + 2.2f * k);                             // IDLE BURN
                  return d / (1.0f + 0.32f * d * d); }
    }
}

// ------------------------------------------------------------------- a voice
struct Engine::Voice {
    bool  on = false, held = false, sliding = false;
    int   note = 36;
    float vel = 0.8f, keyNorm = 0.3f;
    float targetHz = 55.f, curHz = 55.f, glideK = 1.f;

    static const int MAXU = 8;
    Osc  oa[MAXU], ob[MAXU], osub;
    float uDet[MAXU] {}, uPan[MAXU] {}, uPhase[MAXU] {};
    float syncPh[MAXU] {};                //  the hidden master clock for SYNC
    float e2Prev = 0.f, e2Cur = 0.f;      //  this voice's own filter envelope, per control block
    float orbPh[MAXU] {};                 //  DISK ORBIT: each voice's own orbit
    float nzLp = 0.f, nzHp = 0.f;         //  NOISE COLOUR
    float drP = 0.f, drC = 0.f;           //  VINTAGE: slow drift, pitch and cutoff
    Rng   drng;                           //  its own dice, so noise patches keep theirs

    Env  e1, e2, e3;
    Ladder ladA, ladB;
    K35    k35A, k35B;
    SVF    svfA, svfB;
    Diode  diA, diB;
    Comb   cmA, cmB;
    Formant fmA, fmB;
    SVF    xLowA, xHighA;                 // the SPLIT crossover
    SVF    singLP;
    Osc    singOsc;
    Ringdown ring;
    Rng    rng;
    float  lastOut = 0.f, ringExcite = 0.f;
    int    tailN = -1;                     //  samples into a ringdown tail; -1 = not in one
    float  tailPk = 0.f, tailAmt = 0.f;    //  the tail's recent peak, and the amount it rings at

    void prepare (double sr) {
        cmA.prepare (sr); cmB.prepare (sr);
        //  ENV1 is ticked every oversampled sample.  ENV2 and ENV3 are ticked
        //  once per CTRL samples, in the control tick - timed at the sample
        //  rate, every decay on them ran CTRL (16x) too slow: the filter
        //  envelope of every acid patch, and ENV3's sustain never reached.
        e1.sr = sr;
        e2.sr = e3.sr = sr / CTRL;
        reset();
    }
    void reset() {
        for (int i = 0; i < MAXU; ++i) { oa[i].ph = rng.uni(); ob[i].ph = rng.uni(); }
        osub.ph = singOsc.ph = 0.f;
        for (int i = 0; i < MAXU; ++i) { syncPh[i] = 0.f; orbPh[i] = 0.f; }
        nzLp = nzHp = 0.f;
        ladA.reset(); ladB.reset(); k35A.reset(); k35B.reset();
        svfA.reset(); svfB.reset(); diA.reset(); diB.reset();
        cmA.reset(); cmB.reset(); fmA.reset(); fmB.reset();
        xLowA.reset(); xHighA.reset(); singLP.reset(); ring.reset();
        e1.hardReset(); e2.hardReset(); e3.hardReset();
        lastOut = 0.f; ringExcite = 0.f; tailN = -1; tailPk = 0.f; tailAmt = 0.f;
    }
};

// -------------------------------------------------------------------- state
struct Engine::Impl {
    Voice v[2];
    WaveTables wt;
    Decim2 dec1L, dec2L, dec1R, dec2R;
    DCBlock dcL, dcR;
    Rng rng;

    Lfo  lfo[3];
    Lfo  chirp;
    float rndVal = 0.f, rndTarget = 0.f, rndPhase = 0.f;
    Smooth rndSm;

    float mod[N_DST] {};
    float env2Prev = 0.f, env2Cur = 0.f;   //  the filter envelope, interpolated per sample
    float acutPrev = 0.f, acutCur = 0.f;   //  matrix -> A CUTOFF, likewise
    float bcutPrev = 0.f, bcutCur = 0.f;   //  matrix -> B CUTOFF, likewise
    float srcVal[N_SRC] {};

    float wheel = 0.f, pressure = 0.f, bend = 0.f;
    float stopJump = 0.f;                    //  bench: a voice's output at the sample it stopped
    float pitchSemiSm = 0.f, pitchMul = 1.f;   //  bend + TRANSPOSE, smoothed, and its multiplier
    bool  pedal = false;

    //  the sequencer
    Step  pattern[MAX_STEPS];
    int   curStep = -1;
    float stepPhase = 0.f;
    float accentEnv = 0.f;              // the 303's accumulating accent circuit
    int   ratchetIdx = 0;
    bool  seqGateOpen = false;
    int   lastSeqNote = 0;

    //  the well
    float energy = 0.f;
    Smooth massFollow, rsSmooth;

    //  drive trim, measured
    float trim[2][4][17] {};             //  [PRE, POST][type][amount]

    //  stereo widening, above the crossover only - the sub stays mono
    SVF wLow, wHigh;
    float apL = 0.f, apR = 0.f;
    //  SPACE: two short taps on the high band make a real stereo field; the
    //  side they add cancels in L+R, so the mono sum is untouched
    static const int SPN = 8192;
    float spBuf[SPN] {};
    int   spW = 0, spA = 1, spB = 1;

    int   ctrlCount = 0;
    float outSm = 1.f;
    float peakDecay = 0.f;
};

// ----------------------------------------------------------------- lifecycle
Engine::Engine()  { im = new Impl(); }
Engine::~Engine() { delete im; }

void Engine::prepare (double sampleRate, int maxBlock) {
    (void) maxBlock;
    sr = sampleRate;
    const double osr = sr * OS;

    im->wt.init();
    im->dec1L.init(); im->dec2L.init(); im->dec1R.init(); im->dec2R.init();
    im->dcL.setCorner (sr, 12.0f); im->dcR.setCorner (sr, 12.0f);
    im->massFollow.setTau (sr, 0.020f);
    im->rsSmooth.setTau  (sr, 0.035f);
    im->rndSm.setTau     (sr, 0.010f);
    for (auto& v : im->v) v.prepare (osr);
    im->wLow.set  (sr, 160.f, 0.2f, 0);
    im->spA = std::min ((int) (sr * 0.0071), Impl::SPN - 1);
    im->spB = std::min ((int) (sr * 0.0113), Impl::SPN - 1);
    im->wHigh.set (sr, 160.f, 0.2f, 2);

    /*  MEASURE the drive trims.  Each shaper is run over a reference tone at
        seventeen drive points and its output rms compared with the input's,
        so the make-up is whatever the curve actually does - not whatever it
        did the day somebody typed a number.                                 */
    //  PRE and POST each measured at the level that position is fed
    const int N = 2048;
    const float refAmp[2] = { DRIVE_REF_PRE, DRIVE_REF_POST };
    for (int pos = 0; pos < 2; ++pos)
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k <= 16; ++k) {
            const float amt = (float) k / 16.0f;
            double inSum = 0.0, outSum = 0.0;
            for (int i = 0; i < N; ++i) {
                //  COLLAPSE is the most level-sensitive of the four: its own reference
                float x = refAmp[pos] * (t == 3 ? DRIVE_REF_COLLAPSE : 1.0f) * std::sin (6.2831853f * 220.0f * (float) i / (float) sr);
                float y = shape (t, x, amt);
                inSum += (double) x * x; outSum += (double) y * y;
            }
            float ri = (float) std::sqrt (inSum / N), ro = (float) std::sqrt (outSum / N);
            im->trim[pos][t][k] = ro > 1e-6f ? clampf (ri / ro, 0.05f, 8.0f) : 1.0f;
        }
    reset();
}

float Engine::driveTrim (int type, float amount, bool post) const {
    int t = type < 0 ? 0 : (type > 3 ? 3 : type);
    float x = clampf (amount, 0.f, 1.f) * 16.0f;
    int   i = (int) x; float f = x - (float) i;
    int   j = i + 1 > 16 ? 16 : i + 1;
    const int q = post ? 1 : 0;
    return lerpf (im->trim[q][t][i], im->trim[q][t][j], f);
}

void Engine::reset() {
    for (auto& v : im->v) v.reset();
    im->dec1L.reset(); im->dec2L.reset(); im->dec1R.reset(); im->dec2R.reset();
    im->dcL.reset(); im->dcR.reset();
    im->energy = 0.f; im->massFollow.snap (0.f); im->rsSmooth.snap (0.f);
    im->curStep = -1; im->stepPhase = 0.f; im->accentEnv = 0.f; im->seqGateOpen = false;
    im->pedal = false;                        // a reset lets go of the pedal too
    massSm = rsSm = 0.f; peakHold = 0.f;
    mPkL = mPkR = mMsL = mMsR = mOver = 0.f;
    heavy = 0.f;
    scopeBuf.assign (1024, 0.f); scopeW = 0;
    for (auto& m : modOut) m = 0.f;
    for (auto& f : modFroze) f = false;
}

// -------------------------------------------------------------------- notes
void Engine::noteOn (int midiNote, float velocity) {
    Voice& v = im->v[0];
    bool duo = p.v[P_voicing] > 0.5f;
    Voice& tgt = (duo && im->v[0].on && !im->v[1].on) ? im->v[1] : v;

    bool wasOn = tgt.on && tgt.held;
    tgt.note = midiNote; tgt.vel = clampf (velocity, 0.f, 1.f);
    tgt.keyNorm = clampf ((float) (midiNote - 24) / 60.0f, 0.f, 1.f);
    tgt.targetHz = pitchToHz ((float) midiNote);
    /*  RESET ONLY FROM SILENCE.  noteOff clears `held` but leaves the release
        running, so a note arriving straight after another found wasOn false
        and wiped the voice mid-ring - every filter state zeroed, every
        oscillator phase re-randomised, the envelope dropped to 0 - while the
        previous note was still at full amplitude.  That was the note-on
        click, and it was on every note except the first, which is why a
        single-note probe could never see it and why the sequencer (which
        gates without resetting) was clean.                                  */
    const bool sounding = tgt.on;          // still making sound, release included
    if (!wasOn) {
        if (!sounding) {
            //  a voice arriving from silence must not glide in from a stale pitch
            tgt.curHz = tgt.targetHz;
            tgt.reset();
        }
        //  Env::gate() attacks from the level it is already at, so a
        //  retrigger over a dying note is continuous by construction.
        tgt.e1.gate (true); tgt.e2.gate (true); tgt.e3.gate (true);
        tgt.ringExcite = 1.0f;
        tgt.tailN = -1;                    //  a note over a ringing tail is a note again
    }
    tgt.on = tgt.held = true;
}

void Engine::noteOff (int midiNote) {
    for (auto& v : im->v)
        if (v.on && v.note == midiNote) {
            v.held = false;
            if (!im->pedal) { v.e1.gate (false); v.e2.gate (false); v.e3.gate (false); }
        }
}

void Engine::sustain (bool down) {
    im->pedal = down;
    if (!down)
        for (auto& v : im->v)
            if (v.on && !v.held) { v.e1.gate (false); v.e2.gate (false); v.e3.gate (false); }
}

/*  A PANIC MUST LET GO OF THE PEDAL.  Without it the note ringing at the time
    stops - so the panic looks like it worked - and the NEXT note is held for
    ever by a pedal nobody is pressing.  Six synths in this fleet had exactly
    that bug, and their own PANIC buttons could not clear it either.         */
void Engine::allNotesOff() {
    im->pedal = false;
    for (auto& v : im->v) { v.held = false; v.e1.gate (false); v.e2.gate (false); v.e3.gate (false); }
    im->seqGateOpen = false;
}

float Engine::stopJump () const { return im->stopJump; }
int Engine::soundingVoices () const { int n = 0; for (auto& v : im->v) if (v.on) ++n; return n; }
void Engine::pitchBend (float wheel) { im->bend = clampf (wheel, -1.f, 1.f); }
void Engine::setWheel    (float v01) { im->wheel    = clampf (v01, 0.f, 1.f); }
void Engine::setPressure (float v01) { im->pressure = clampf (v01, 0.f, 1.f); }

void Engine::setStep (int i, const Step& s) { if (i >= 0 && i < MAX_STEPS) im->pattern[i] = s; }
Step Engine::getStep (int i) const { return (i >= 0 && i < MAX_STEPS) ? im->pattern[i] : Step(); }
int   Engine::seqStep()  const { return im->curStep; }
float Engine::seqPhase() const { return im->stepPhase; }
void  Engine::seqReset() { im->curStep = -1; im->stepPhase = 0.f; im->accentEnv = 0.f; }

// ---------------------------------------------------------------- the well
float Engine::dilationAt (float radius01) const {
    float r = R_NEAR + clampf (radius01, 0.f, 1.f) * (R_FAR - R_NEAR);
    if (r <= rsSm) return 0.0f;
    return std::sqrt (std::max (0.0f, 1.0f - rsSm / r));
}
float Engine::redshiftNow() const {
    //  the observer sits deeper as REDSHIFT rises; floored so the formants
    //  sink dramatically without becoming a cartoon
    float amt = clampf (p.v[P_redshift] + im->mod[D_REDSHIFT], 0.f, 1.f);
    float r = R_NEAR + (1.0f - amt) * (R_FAR - R_NEAR);
    if (r <= rsSm) return 0.35f;
    return clampf (std::sqrt (std::max (0.0f, 1.0f - rsSm / r)), 0.35f, 1.0f);
}


// ============================================================== the audio
void Engine::process (float* L, float* R, int n)
{
    Impl& I = *im;
    const double osr = sr * OS;
    const float  P   = 1.0f / (float) osr;

    // ---- things that only change per block -------------------------------
    const int   route   = (int) (p.v[P_route] + 0.5f);
    const int   circA   = (int) (p.v[P_fa_circ] + 0.5f);
    const int   circB   = (int) (p.v[P_fb_circ] + 0.5f);
    const int   modeA   = (int) (p.v[P_fa_mode] + 0.5f);
    const int   modeB   = (int) (p.v[P_fb_mode] + 0.5f);
    const int   drvType = (int) (p.v[P_drv_type] + 0.5f);
    const int   nU      = std::max (1, std::min ((int) (p.v[P_disk_n] + 0.5f), Voice::MAXU));
    const int   tabA    = (int) (p.v[P_a_table] + 0.5f);
    const int   tabB    = (int) (p.v[P_b_table] + 0.5f);
    const bool  wtA     = p.v[P_a_engine] > 0.5f;
    const bool  wtB     = p.v[P_b_engine] > 0.5f;
    const bool  seqOn   = p.v[P_seq_on] > 0.5f;

    const float macroMass  = p.v[P_macro_mass];
    const float macroDepth = p.v[P_macro_depth];
    const float macroEner  = p.v[P_macro_energy];
    const float macroHor   = p.v[P_macro_horiz];
    //  x1 at noon exactly, x0.25 .. x4 - the same law as the envelopes
    const float macroTime  = std::pow (4.0f, 2.0f * (p.v[P_macro_time] - 0.5f));
    const float macroSpace = p.v[P_macro_space];

    //  the six macros are gestures, so they SUM with the fine controls
    const float driveAmt = clampf (p.v[P_drv_amt] + 0.75f * macroEner, 0.f, 1.f);
    const float widthAmt = clampf (p.v[P_disk_width] * 0.6f + macroSpace, 0.f, 1.f);
    const float singLvl  = clampf (p.v[P_sing_level] + 0.85f * macroDepth, 0.f, 1.f);
    const float outGain  = std::pow (10.0f, p.v[P_out_trim] / 20.0f);
    const float tilt     = clampf (p.v[P_tilt], -1.f, 1.f);

    for (auto& v : I.v) v.glideK = 1.0f - std::exp (-1.0f / (float) (osr * std::max (timeSec (p.v[P_glide]), 1e-4f)));

    /*  THE ENVELOPES.  ta/td/su/tr/curve used to keep the struct defaults for
        ever, so every ADSR on the panel did nothing at all - the probe proved
        it by rendering byte-identical output for a 0.5 ms and a 50 ms attack.

        ATT_FLOOR is not timidity.  timeSec(0) is half a millisecond, and a
        half-millisecond attack on a 41 Hz note is a broadband transient - a
        CLICK - whatever the oscillator does, because the ear hears the
        envelope edge itself.  A real envelope generator cannot slew that fast
        either.  The floor is measured, not guessed: see the bench.            */
    {
        //  TIME is the rate of the ship's clock: up is faster.  4^(-2(t-0.5)) is
        //  EXACTLY 1 at the default 0.5, so no preset moves; it measured 0.000
        //  difference end to end before, because it only reached the LFOs.
        const float tS  = std::pow (4.0f, -2.0f * (p.v[P_macro_time] - 0.5f));
        const float ta1 = std::max (timeSec (p.v[P_e1_a]) * tS, ATT_FLOOR);
        const float ta2 = std::max (timeSec (p.v[P_e2_a]) * tS, ATT_FLOOR);
        const float ta3 = std::max (timeSec (p.v[P_e3_a]) * tS, ATT_FLOOR);
        for (auto& v : I.v) {
            v.e1.ta = ta1; v.e1.td = timeSec (p.v[P_e1_d]) * tS;
            v.e1.su = clampf (p.v[P_e1_s], 0.f, 1.f);
            v.e1.tr = std::max (timeSec (p.v[P_e1_r]) * tS, REL_FLOOR);
            v.e1.curve = p.v[P_e1_curve];
            v.e2.ta = ta2; v.e2.td = timeSec (p.v[P_e2_d]) * tS;
            v.e2.su = clampf (p.v[P_e2_s], 0.f, 1.f);
            v.e2.tr = std::max (timeSec (p.v[P_e2_r]) * tS, REL_FLOOR);
            v.e2.curve = p.v[P_e2_curve];
            v.e3.ta = ta3; v.e3.td = timeSec (p.v[P_e3_d]) * tS;
            v.e3.su = clampf (p.v[P_e3_s], 0.f, 1.f);
            v.e3.tr = std::max (timeSec (p.v[P_e3_r]) * tS, REL_FLOOR);
            v.e3.curve = p.v[P_e3_curve];
        }
    }

    // ---------------------------------------------------------- control tick
    int curSample = 0;                       //  where in the block the tick falls
    auto controlTick = [&] ()
    {
        const float dt = (float) CTRL / (float) osr;

        // --- the pitch wheel and TRANSPOSE, together, gliding over ~6 ms
        {   const float target = I.bend * p.v[P_bend_range] + 12.0f * p.v[P_transpose];
            I.pitchSemiSm += (target - I.pitchSemiSm) * (1.0f - std::exp (-dt / 0.006f));
            if (std::fabs (target - I.pitchSemiSm) < 1e-5f) I.pitchSemiSm = target;
            I.pitchMul = I.pitchSemiSm == 0.f ? 1.0f : std::pow (2.0f, I.pitchSemiSm / 12.0f); }

        // --- the sequencer.  Its clock is the host's, so a pattern locks.
        if (seqOn) {
            static const float DIV[6] = { 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 1.0f/6.0f, 0.125f };
            const int   di   = (int) clampf (p.v[P_seq_div], 0.f, 5.f);
            const float beat = DIV[di];
            const float spb  = 60.0f / (float) std::max (20.0, hostBpm);
            float stepSec = beat * spb;
            //  swing lengthens the odd steps and shortens the even ones
            const int   len = std::max (1, (int) (p.v[P_seq_len] + 0.5f));
            const bool  oddStep = (I.curStep >= 0) && ((I.curStep & 1) != 0);
            const float sw  = clampf (p.v[P_seq_swing], 0.f, 1.f) * 0.42f;
            stepSec *= oddStep ? (1.0f - sw) : (1.0f + sw);

            I.stepPhase += dt / std::max (stepSec, 1e-4f);
            while (I.stepPhase >= 1.0f) {
                I.stepPhase -= 1.0f;
                I.curStep = (I.curStep + 1) % len;
                const Step& s = I.pattern[I.curStep];
                I.ratchetIdx = 0;
                if (s.on) {
                    Voice& v = I.v[0];
                    const int oct = (int) p.v[P_seq_octave];
                    const int nn  = v.note + s.note + 12 * oct;
                    v.targetHz = pitchToHz ((float) nn);
                    I.lastSeqNote = nn;
                    /*  SLIDE is legato: glide to the new pitch and do NOT
                        retrigger.  That, and the accent circuit below, are
                        what a 303 actually is.                              */
                    if (!s.slide) {
                        v.curHz = (v.sliding ? v.curHz : v.targetHz);
                        v.e1.gate (true); v.e2.gate (true); v.e3.gate (true);
                        v.ringExcite = 1.0f;
                    }
                    v.sliding = s.slide != 0;
                    /*  THE ACCENT CIRCUIT ACCUMULATES.  Consecutive accented
                        steps charge it further before it has decayed, and
                        that build-up is the squelch - a per-note accent that
                        simply adds a fixed amount does not sound like a 303. */
                    if (s.accent) I.accentEnv = clampf (I.accentEnv + 0.55f, 0.f, 1.35f);
                    I.seqGateOpen = true;
                } else {
                    I.seqGateOpen = false;
                    if (!I.v[0].held || true) { I.v[0].e1.gate (false); I.v[0].e2.gate (false); }
                }
            }
            //  gate length, and ratchets inside the step
            const Step& cs = I.pattern[std::max (0, I.curStep)];
            const float gl = clampf ((float) cs.gate / 100.0f * (0.2f + 1.6f * p.v[P_seq_gate]), 0.02f, 1.0f);
            const int   rt = std::max<int> (1, cs.ratchet);
            const float sub = I.stepPhase * (float) rt;
            const int   subI = (int) sub;
            if (rt > 1 && subI != I.ratchetIdx && cs.on) {
                I.ratchetIdx = subI;
                I.v[0].e1.gate (true); I.v[0].e2.gate (true);
            }
            if (cs.on && (sub - (float) subI) > gl && I.seqGateOpen) {
                I.v[0].e1.gate (false); I.v[0].e2.gate (false);
                I.seqGateOpen = false;
            }
        }
        //  the accent decays between hits - this is what makes it accumulate
        I.accentEnv *= std::exp (-dt / 0.28f);

        // --- MASS: signal energy + velocity + the manual knob
        float lvl = 0.f;
        for (auto& v : I.v) if (v.on) lvl = std::max (lvl, std::fabs (v.lastOut));
        I.energy += 0.25f * (lvl - I.energy);
        float velPart = 0.f;
        for (auto& v : I.v) if (v.on) velPart = std::max (velPart, v.vel);
        float massRaw = clampf (macroMass
                              + p.v[P_mass_track] * clampf (I.energy * 2.4f, 0.f, 1.f)
                              + p.v[P_mass_vel]   * velPart
                              + I.mod[D_MASS], 0.f, 1.f);
        massSm = I.massFollow.tick (massRaw);
        rsSm   = I.rsSmooth.tick (massSm * clampf (macroHor, 0.f, 1.f) * R_HORIZON_MAX);
        //  MASS used to be heard only through what it slowed down, so on a
        //  plain patch the knob seemed dead.  Past noon it now acts directly:
        //  light cannot climb out of a deep well, so the top sinks, and the
        //  weight gathers in the sub.  At or below noon this is exactly zero,
        //  so every patch that keeps MASS there is untouched.
        {   const float target = clampf ((macroMass + I.mod[D_MASS] - 0.5f) * 2.0f, 0.f, 1.f);
            heavy += 0.02f * (target - heavy);
            if (target == 0.f && heavy < 1e-6f) heavy = 0.f; }

        // --- the modulators, each at its own radius
        const float dilE2 = dilationAt (p.v[P_r_e2]);
        const float dilE3 = dilationAt (p.v[P_r_e3]);
        const float dilL1 = dilationAt (p.v[P_r_l1]);
        const float dilL2 = dilationAt (p.v[P_r_l2]);
        const float dilL3 = dilationAt (p.v[P_r_l3]);
        const float dilRn = dilationAt (p.v[P_r_rnd]);
        const float dilCh = dilationAt (p.v[P_r_chirp]);

        I.lfo[0].shape = (int) p.v[P_l1_shape];
        I.lfo[1].shape = (int) p.v[P_l2_shape];
        I.lfo[2].shape = (int) p.v[P_l3_shape];
        const float beatHz = (float) (hostBpm / 60.0);
        auto rateOf = [&] (int which) {
            const float knob[3] = { p.v[P_l1_rate], p.v[P_l2_rate], p.v[P_l3_rate] };
            const float sync[3] = { p.v[P_l1_sync], p.v[P_l2_sync], p.v[P_l3_sync] };
            float hz = lfoHz (knob[which]) * macroTime;
            if (sync[which] > 0.5f)
                hz = beatHz * LFO_DIV_CPB[(int) clampf (std::round (knob[which] * (N_LFO_DIV - 1)), 0.f, (float) (N_LFO_DIV - 1))];
            return hz;
        };
        const float dilArr[3] = { dilL1, dilL2, dilL3 };
        float lv[3];
        for (int k = 0; k < 3; ++k)
        {
            //  PHASE-LOCKED to the bar while the transport runs, so a synced LFO
            //  lands on the grid rather than merely matching its rate.  Only
            //  outside the well's grip: inside it, dilation slows the clock,
            //  and that is the instrument's whole idea.
            const float knobS[3] = { p.v[P_l1_rate], p.v[P_l2_rate], p.v[P_l3_rate] };
            const float syncS[3] = { p.v[P_l1_sync], p.v[P_l2_sync], p.v[P_l3_sync] };
            if (syncS[k] > 0.5f && hostPlaying && dilArr[k] >= 0.999f) {
                const double ppqNow = hostPpq + (double) curSample / sr * hostBpm / 60.0;
                const int di = (int) clampf (std::round (knobS[k] * (N_LFO_DIV - 1)), 0.f, (float) (N_LFO_DIV - 1));
                const double cyc = ppqNow * (double) LFO_DIV_CPB[di];
                I.lfo[k].ph = (float) (cyc - std::floor (cyc));
                lv[k] = I.lfo[k].tick (osr / CTRL, 0.0f, 1.0f, false);
            } else
                lv[k] = I.lfo[k].tick (osr / CTRL, rateOf (k), dilArr[k], dilArr[k] <= 0.f);
        }

        //  random: a stepped source, smoothed by its own knob
        I.rndPhase += dt * lfoHz (p.v[P_rnd_rate]) * macroTime * std::max (dilRn, 1e-4f);
        if (I.rndPhase >= 1.f) { I.rndPhase -= std::floor (I.rndPhase); I.rndTarget = I.rng.bip(); }
        I.rndSm.setTau (osr / CTRL, 0.002f + 0.30f * p.v[P_rnd_smooth]);
        I.rndVal = (dilRn <= 0.f) ? I.rndVal : I.rndSm.tick (I.rndTarget);

        I.chirp.shape = 4;
        const float chirpV = I.chirp.tick (osr / CTRL, 1.0f / std::max (timeSec (p.v[P_chirp_time]), 0.02f),
                                           dilCh, dilCh <= 0.f);

        Voice& v0 = I.v[0];
        I.srcVal[S_OFF]   = 0.f;
        I.srcVal[S_ENV1]  = v0.e1.y;
        //  EVERY voice runs its own filter envelope: in DUO the second voice's
        //  filter used to follow the first voice's envelope
        for (auto& vv : I.v) { vv.e2Prev = vv.e2Cur; vv.e2Cur = vv.e2.tick (std::max (dilE2, 1e-4f)); }
        I.srcVal[S_ENV2]  = v0.e2Cur;
        I.env2Prev = I.env2Cur; I.env2Cur = I.srcVal[S_ENV2];
        I.srcVal[S_ENV3]  = v0.e3.tick (std::max (dilE3, 1e-4f));
        I.srcVal[S_LFO1]  = lv[0];
        I.srcVal[S_LFO2]  = lv[1];
        I.srcVal[S_LFO3]  = lv[2];
        I.srcVal[S_RND]   = I.rndVal;
        I.srcVal[S_CHIRP] = chirpV;
        I.srcVal[S_MASS]  = massSm;
        I.srcVal[S_VEL]   = v0.vel;
        I.srcVal[S_KEY]   = v0.keyNorm;
        I.srcVal[S_WHEEL] = I.wheel;
        I.srcVal[S_PRESS] = I.pressure;
        I.srcVal[S_ACCENT]= clampf (I.accentEnv, 0.f, 1.f);
        I.srcVal[S_SEQ]   = I.curStep < 0 ? 0.f : (float) I.curStep / 31.0f;

        //  publish what the panel draws and the bench measures
        modOut[0] = I.srcVal[S_ENV1];  modOut[1] = I.srcVal[S_ENV2]; modOut[2] = I.srcVal[S_ENV3];
        modOut[3] = lv[0]; modOut[4] = lv[1]; modOut[5] = lv[2];
        modOut[6] = I.rndVal; modOut[7] = chirpV; modOut[8] = massSm;
        modFroze[1] = dilE2 <= 0.f; modFroze[2] = dilE3 <= 0.f;
        modFroze[3] = dilL1 <= 0.f; modFroze[4] = dilL2 <= 0.f; modFroze[5] = dilL3 <= 0.f;
        modFroze[6] = dilRn <= 0.f; modFroze[7] = dilCh <= 0.f;

        // --- the matrix
        for (auto& m : I.mod) m = 0.f;
        const int sIdx[8] = { P_m1_src, P_m2_src, P_m3_src, P_m4_src, P_m5_src, P_m6_src, P_m7_src, P_m8_src };
        const int dIdx[8] = { P_m1_dst, P_m2_dst, P_m3_dst, P_m4_dst, P_m5_dst, P_m6_dst, P_m7_dst, P_m8_dst };
        const int aIdx[8] = { P_m1_amt, P_m2_amt, P_m3_amt, P_m4_amt, P_m5_amt, P_m6_amt, P_m7_amt, P_m8_amt };
        for (int k = 0; k < 8; ++k) {
            const int s = (int) (p.v[sIdx[k]] + 0.5f), d = (int) (p.v[dIdx[k]] + 0.5f);
            if (s <= 0 || d <= 0 || s >= N_SRC || d >= N_DST) continue;
            I.mod[d] += I.srcVal[s] * p.v[aIdx[k]];
        }
        //  ORBITS: each LFO can carry its own target and amount, set on its own
        //  console - three more routes, drawn in the MATRIX network with the rest
        {   const int oD[3] = { P_l1_dst, P_l2_dst, P_l3_dst }, oA[3] = { P_l1_amt, P_l2_amt, P_l3_amt };
            for (int k = 0; k < 3; ++k) {
                const int d = (int) (p.v[oD[k]] + 0.5f);
                if (d <= 0 || d >= N_DST) continue;
                I.mod[d] += I.srcVal[S_LFO1 + k] * p.v[oA[k]];
            } }
        //  a modulated cutoff read raw moves in a staircase, one step per
        //  control block; interpolated like the filter envelope it is a line
        I.acutPrev = I.acutCur; I.acutCur = I.mod[D_ACUT];
        I.bcutPrev = I.bcutCur; I.bcutCur = I.mod[D_BCUT];
    };

    // ------------------------------------------------------- one voice, one OS sample
    auto voiceSample = [&] (Voice& v) -> float
    {
        v.curHz += (v.targetHz - v.curHz) * v.glideK;
        const float bendMul = I.pitchMul;
        const float pitchMod = std::pow (2.0f, I.mod[D_PITCH] * 2.0f);
        const float base = clampf (v.curHz * bendMul * pitchMod, 8.f, 8000.f);

        const float env1 = v.e1.tick (1.0f);
        if (!v.e1.active() && !v.held) {
            //  The amp has finished, but the RINGDOWN body sits after it and
            //  rings on its own decay.  Stopping the voice here cut the body
            //  mid-ring: the click at the end of RINGDOWN, HAWKING, DOOM WEIGHT.
            //  So a ringing body is left to ring out alone, until it is below
            //  -100 dB; the source and filters are idle, only the modes tick.
            if (v.tailN < 0) {
                v.tailAmt = clampf (p.v[P_ring_amt] + I.mod[D_RINGDOWN], 0.f, 1.f);
                v.tailN = 0; v.tailPk = 0.f;
            }
            const int tailMax = (int) (osr * 60.0), tailFade = (int) (osr * 0.3);
            if (v.tailAmt > 1e-4f && v.tailN < tailMax) {
                float out = v.ring.tick (0.0f) * v.tailAmt;
                if (v.tailN > tailMax - tailFade) {          //  a ceiling, faded not cut
                    const float u = (float) (v.tailN - (tailMax - tailFade)) / (float) tailFade;
                    out *= 0.5f * (1.0f + std::cos (3.14159265f * std::min (u, 1.0f)));
                }
                out *= std::max (0.0f, 1.0f + I.mod[D_LEVEL]);
                v.tailPk = std::max (v.tailPk, std::fabs (out));
                if (++v.tailN % 2048 != 0 || v.tailPk >= 1e-5f) {
                    if (v.tailN % 2048 == 0) v.tailPk = 0.f;
                    v.lastOut = out;
                    return out;
                }
            }
            I.stopJump = std::max (I.stopJump, std::fabs (v.lastOut));
            v.on = false; v.tailN = -1; v.lastOut = 0.f;
            return 0.f;
        }

        //  the accretion disk: Keplerian radii, so the spread is ordered
        const float spread = clampf (p.v[P_disk_spread] + I.mod[D_DISK], 0.f, 1.f);
        float sum = 0.f;
        const float shapeA = clampf (p.v[P_a_shape] + I.mod[D_ASHAPE], 0.f, 1.f);
        const float shapeB = clampf (p.v[P_b_shape] + I.mod[D_BSHAPE], 0.f, 1.f);
        const float widthA = clampf (p.v[P_a_width] + I.mod[D_AWIDTH], 0.03f, 0.97f);
        //  B WIDTH was never read - oscillator B borrowed A's width
        const float widthB = clampf (p.v[P_b_width], 0.03f, 0.97f);
        const float pmAmt  = clampf (p.v[P_pm]      + I.mod[D_PM],   0.f, 1.f);
        const float ringA  = clampf (p.v[P_ringmod] + I.mod[D_RING], 0.f, 1.f);

        //  VINTAGE was never read.  Now: a slow random walk per voice, a few
        //  cents of pitch and a percent or so of cutoff at full, the way an
        //  old voice card wanders.  At 0 both factors are exactly 1.
        const float vint = p.v[P_vintage];
        if (vint > 0.f) {
            v.drP = v.drP * 0.999998f + v.drng.bip() * 0.00006f;
            v.drC = v.drC * 0.999998f + v.drng.bip() * 0.00006f;
        }
        const float vPitch = vint > 0.f ? std::pow (2.0f, v.drP * vint * 200.0f / 1200.0f) : 1.0f;
        const float hzA = base * vPitch * std::pow (2.0f, p.v[P_a_oct] + p.v[P_a_semi] / 12.0f + p.v[P_a_fine] / 1200.0f);
        const float hzB = base * vPitch * std::pow (2.0f, p.v[P_b_oct] + p.v[P_b_semi] / 12.0f + p.v[P_b_fine] / 1200.0f);

        for (int u = 0; u < nU; ++u) {
            //  Omega ~ r^(-3/2): the detune law is the orbit law
            const float rr  = 1.0f + 0.55f * (float) u;
            float det = (u == 0) ? 0.f : spread * 0.035f * std::pow (rr, -1.5f) * ((u & 1) ? 1.f : -1.f) * (float) u;
            //  DISK ORBIT was never read.  Each voice's detune now swings on its
            //  own Keplerian orbit, outer voices slower: the disk moves.  At 0
            //  the factor is exactly 1.
            const float rot = p.v[P_disk_rot];
            if (rot > 0.f && u > 0) {
                v.orbPh[u] += 0.35f * std::pow (rr, -1.5f) * P;
                if (v.orbPh[u] >= 1.f) v.orbPh[u] -= 1.f;
                det *= 1.0f + rot * 0.8f * std::sin (6.2831853f * v.orbPh[u]);
            }
            const float ma  = hzA * (1.0f + det), mb = hzB * (1.0f + det);
            //  SYNC used to reset A on B's wraps, so the pitch jumped to B's
            //  tuning (+1201 cents with A an octave down).  Now a hidden master
            //  at A's OWN pitch resets A while A runs at B's: the note stays
            //  put and B's tuning becomes the sync sweep.  Off, nothing moves.
            const bool syncOn = p.v[P_sync] > 0.5f;
            //  ...and the ratio is FOLDED INTO ONE OCTAVE.  Hard sync at an exact
            //  whole-number ratio IS the slave's pitch - the reset lands on its
            //  own cycle start - so A an octave under B (ratio 2) still jumped an
            //  octave.  Folded, an octave offset gives plain A and every other
            //  interval gives the sync sweep, with the note where it was.
            float syncRatio = ma > 1e-6f ? mb / ma : 1.f;
            while (syncRatio >= 2.f)  syncRatio *= 0.5f;
            while (syncRatio <  1.f)  syncRatio *= 2.f;
            v.oa[u].inc = (syncOn ? ma * syncRatio : ma) * P; v.ob[u].inc = mb * P;

            v.ob[u].advance();
            float b = wtB ? I.wt.read (tabB, v.ob[u].ph, shapeB, (int) (osr * 0.5f / std::max (mb, 1.f)))
                          : v.ob[u].analogue (shapeB, widthB);
            if (syncOn) {
                v.syncPh[u] += ma * P;
                if (v.syncPh[u] >= 1.f) { v.syncPh[u] -= std::floor (v.syncPh[u]); v.oa[u].ph = 0.f; }
            }
            v.oa[u].advance();
            float phA = v.oa[u].ph + pmAmt * 0.5f * b;
            phA -= std::floor (phA);
            float saveA = v.oa[u].ph; v.oa[u].ph = phA;
            float a = wtA ? I.wt.read (tabA, phA, shapeA, (int) (osr * 0.5f / std::max (ma, 1.f)))
                          : v.oa[u].analogue (shapeA, widthA);
            v.oa[u].ph = saveA;

            float mix = a * p.v[P_a_level] + b * p.v[P_b_level];
            mix = lerpf (mix, a * b * 2.0f, ringA);
            sum += mix;
        }
        sum *= 1.0f / std::sqrt ((float) nU);

        //  sub: phase-coherent with the fundamental, or it cancels instead of
        //  reinforcing - the commonest silent fault in a bass synth
        //  SUB OCTAVE counts octaves BELOW oscillator A (1 = one down, 2 = two).
        //  It used to divide the note by the value, so 1 sounded at pitch.
        {   const float aOct = std::round (p.v[P_a_oct]), sOct = std::round (p.v[P_sub_oct]);
            v.osub.inc = (base * std::exp2 (aOct - sOct)) * P; }
        v.osub.advance();
        const float subS = (p.v[P_sub_shape] > 0.5f) ? (v.osub.ph < 0.5f ? 1.f : -1.f)
                                                     : std::sin (6.2831853f * v.osub.ph);
        sum += subS * (clampf (p.v[P_sub_level] + I.mod[D_SUB], 0.f, 1.f) + 0.5f * heavy) * 0.9f;
        {
            //  NOISE COLOUR was never read.  0.5 is the old white noise exactly;
            //  below it darkens towards rumble, above it thins towards hiss.
            float nz = v.rng.bip();
            const float col = p.v[P_noise_col];
            if (col < 0.5f)      { v.nzLp += (0.01f + 1.98f * col) * (nz - v.nzLp); nz = v.nzLp * (1.0f + (0.5f - col) * 4.0f); }
            else if (col > 0.5f) { v.nzHp += (1.0f - (col - 0.5f) * 1.9f) * 0.5f * (nz - v.nzHp); nz = nz - v.nzHp; }
            sum += nz * clampf (p.v[P_noise_level] + I.mod[D_NOISE], 0.f, 1.f) * 0.30f;
        }

        //  THE AMP (VCA) NOW SITS AFTER THE FILTERS AND THE DRIVE.  Applied
        //  before them, a loud strike drove the nonlinear filters harder than
        //  the sustain did and came out darker - 8.3 dB of tone change on the
        //  ladder with no filter envelope at all, which read as the AMP
        //  envelope doing the filter envelope's job.  Now the filters always
        //  see the full signal and the amp envelope is only level.
        const float accBoost = 1.0f + 1.6f * I.accentEnv * p.v[P_seq_accent];
        const float vca = env1 * lerpf (1.0f, v.vel, p.v[P_vel_amt]) * accBoost;

        //  ---- the singularity is taken BEFORE anything can destroy it
        const float singHz = cutoffHz (p.v[P_sing_freq]) * 0.25f;
        v.singLP.set (osr, singHz, 0.2f, 0);
        float core;
        if (p.v[P_sing_src] > 0.5f) { v.singOsc.inc = base * P; v.singOsc.advance();
                                      core = std::sin (6.2831853f * v.singOsc.ph) * env1; }
        //  * env1: the FILTERED LOW core used to skip the amp envelope while
        //  the PURE SINE branch above applied it - so an un-enveloped copy of
        //  the oscillators switched on at full level at every note-on.  That
        //  was the click, and singLvl carries 0.85*DEPTH, so it was on almost
        //  every patch rather than only on the ones that dial SING LEVEL up.
        else                          core = v.singLP.tick (sum * vca) * env1;

        // ---- DRIVE, PRE the filters (the default): the filters shape the
        //  harmonics the drive makes, as on most hardware
        const float dAmt = clampf (driveAmt + I.mod[D_DRIVE], 0.f, 1.f);
        const bool  drivePost = p.v[P_drv_pos] > 0.5f;
        if (!drivePost && dAmt > 1e-4f) sum = shape (drvType, sum, dAmt) * driveTrim (drvType, dAmt, false);

        // ---- the two horizons
        //  The filter envelope is computed once per control block.  Read raw,
        //  a fast attack reached the cutoff in a dozen little steps - zipper
        //  noise at every note, exposed the moment the envelope ran at its
        //  true speed.  Interpolated across the block, it is a straight line.
        const float envFrac = 1.0f - (float) I.ctrlCount / (float) CTRL;
        const float envF = v.e2Prev + (v.e2Cur - v.e2Prev) * envFrac;
        const float accCut = 1.0f + 2.2f * I.accentEnv * p.v[P_seq_accent];
        //  heavy: up to 1.8 octaves off both cutoffs at full MASS; exactly 1 below noon
        const float heavyCut = heavy > 0.f ? std::exp2 (-1.8f * heavy) : 1.0f;
        auto cutFor = [&] (int par, float envDepth, float key, float mo) {
            float x = clampf (p.v[par] + mo + 0.55f * (macroHor - 0.5f), 0.f, 1.f);
            float hz = cutoffHz (x) * accCut * heavyCut;
            hz *= std::pow (2.0f, envDepth * envF * 4.0f);
            hz *= std::pow (2.0f, key * (v.keyNorm - 0.3f) * 3.0f);
            if (vint > 0.f) hz *= 1.0f + v.drC * vint * 1.2f;
            return clampf (hz, 12.f, (float) osr * 0.45f);
        };
        const float cA = cutFor (P_fa_cut, p.v[P_fa_env], p.v[P_fa_key], I.acutPrev + (I.acutCur - I.acutPrev) * envFrac);
        const float cB = cutFor (P_fb_cut, p.v[P_fb_env], p.v[P_fb_key], I.bcutPrev + (I.bcutCur - I.bcutPrev) * envFrac);
        const float rA = clampf (p.v[P_fa_res] + I.mod[D_ARES], 0.f, 1.f);
        const float rB = clampf (p.v[P_fb_res] + I.mod[D_BRES], 0.f, 1.f);

        //  REDSHIFT used to reach only the FORMANT bank.  A cutoff is a spectral
        //  feature like a formant, so it sinks the same way on every circuit -
        //  the comb's teeth included.  Blended by the knob, so REDSHIFT 0 leaves
        //  the other six exactly as they were; FORMANT keeps its own law.
        const float rsAmt = clampf (p.v[P_redshift] + I.mod[D_REDSHIFT], 0.f, 1.f);
        const float redCut = rsAmt > 0.f ? 1.0f - rsAmt * (1.0f - redshiftNow()) : 1.0f;
        auto runFilter = [&] (int circ, int mode, bool isA, float hz0, float res, float drv, float x) -> float {
            const float hz = (circ == 6) ? hz0 : clampf (hz0 * redCut, 12.f, (float) osr * 0.45f);
            switch (circ) {
                case 0: { auto& f = isA ? v.ladA : v.ladB; f.set (osr, hz, res, drv); return f.tick (x); }
                case 1: { auto& f = isA ? v.k35A : v.k35B; f.set (osr, hz, res, false, mode == 2); return f.tick (x); }
                case 2: { auto& f = isA ? v.k35A : v.k35B; f.set (osr, hz, res, true,  mode == 2); return f.tick (x); }
                case 3: { auto& f = isA ? v.svfA : v.svfB; f.set (osr, hz, res, mode); return f.tick (x); }
                case 4: { auto& f = isA ? v.diA  : v.diB;  f.set (osr, hz, res); return f.tick (x); }
                case 5: { auto& f = isA ? v.cmA  : v.cmB;  f.set (osr, hz, res, mode == 3); return f.tick (x); }
                default:{ auto& f = isA ? v.fmA  : v.fmB;
                          f.set (osr, clampf (hz / 2200.f, 0.f, 1.f), res, redshiftNow());
                          return f.tick (x); }
            }
        };

        float wet;
        if (route == 1) {                                   // PARALLEL
            const float bl = clampf (p.v[P_blend] + I.mod[D_BLEND], 0.f, 1.f);
            wet = runFilter (circA, modeA, true,  cA, rA, p.v[P_fa_drive], sum) * (1.0f - bl)
                + runFilter (circB, modeB, false, cB, rB, p.v[P_fb_drive], sum) * bl;
        } else if (route == 2) {                            // SPLIT
            const float xf = cutoffHz (clampf (p.v[P_split] + I.mod[D_SPLIT], 0.f, 1.f));
            v.xLowA.set (osr, xf, 0.2f, 0); v.xHighA.set (osr, xf, 0.2f, 2);
            const float lo = v.xLowA.tick (sum), hi = v.xHighA.tick (sum);
            wet = runFilter (circA, modeA, true,  cA, rA, p.v[P_fa_drive], lo)
                + runFilter (circB, modeB, false, cB, rB, p.v[P_fb_drive], hi);
        } else {                                            // SERIES
            wet = runFilter (circB, modeB, false, cB, rB, p.v[P_fb_drive],
                  runFilter (circA, modeA, true,  cA, rA, p.v[P_fa_drive], sum));
        }

        // ---- DRIVE, POST the filters: the filtered sound is what distorts
        if (drivePost && dAmt > 1e-4f) wet = shape (drvType, wet, dAmt) * driveTrim (drvType, dAmt, true);

        // ---- the AMP
        wet *= vca;

        // ---- the body: quasi-normal modes, deeper and longer with MASS
        const float rAmt = clampf (p.v[P_ring_amt] + I.mod[D_RINGDOWN], 0.f, 1.f);
        if (rAmt > 1e-4f) {
            v.ring.set (osr, 30.0f + 380.0f * p.v[P_ring_tone], p.v[P_ring_decay], massSm);
            float exc = v.ringExcite; v.ringExcite *= 0.86f;
            wet += v.ring.tick (wet * 0.25f + exc) * rAmt;
        }

        wet += core * singLvl * 1.1f;
        //  LEVEL: a matrix destination that nothing used to read
        wet *= std::max (0.0f, 1.0f + I.mod[D_LEVEL]);
        v.lastOut = wet;
        return wet;
    };

    // ------------------------------------------------------------- the block
    for (int i = 0; i < n; ++i)
    {
        curSample = i;
        float os[OS];
        for (int k = 0; k < OS; ++k) {
            if (--I.ctrlCount <= 0) { I.ctrlCount = CTRL; controlTick(); }
            float s = 0.f;
            for (auto& v : I.v) if (v.on) s += voiceSample (v);
            os[k] = s;
        }
        const float d0 = I.dec1L.tick (os[0], os[1]);
        const float d1 = I.dec1L.tick (os[2], os[3]);
        float mono = I.dec2L.tick (d0, d1);

        //  TONE TILT, one shelf pair, cheap and useful on a bass
        if (std::fabs (tilt) > 1e-3f) {
            const float lo = I.wLow.tick (mono);
            mono += lo * tilt * 0.8f - (mono - lo) * tilt * 0.5f;
        }

        /*  Width above the crossover ONLY, so L+R is exactly the mono core:
            a bass instrument that collapses on a mono system is useless.    */
        const float lo = I.wHigh.tick (mono) * 0.0f + I.wLow.tick (mono);
        const float hi = mono - lo;
        I.apL = 0.62f * (hi - I.apL) + I.apL;
        I.apR = 0.41f * (hi - I.apR) + I.apR;
        I.spBuf[I.spW] = hi;
        const float tA = I.spBuf[(I.spW - I.spA + Impl::SPN) % Impl::SPN];
        const float tB = I.spBuf[(I.spW - I.spB + Impl::SPN) % Impl::SPN];
        I.spW = (I.spW + 1) % Impl::SPN;
        const float side = (I.apL - I.apR) * widthAmt * 1.4f + (tA - tB) * macroSpace * 0.3f;

        float outL = (lo + hi + side) * outGain;
        float outR = (lo + hi - side) * outGain;
        //  PAN: a matrix destination that nothing used to read.  A balance law,
        //  so the centre is exactly unity and a patch without it is untouched.
        const float pan = clampf (I.mod[D_PAN], -1.f, 1.f);
        if (pan > 0.f) outL *= 1.0f - pan; else if (pan < 0.f) outR *= 1.0f + pan;

        //  how far into the soft ceiling the signal goes, for the meter: the
        //  curve bends noticeably from about 0.9, so that is where amber starts
        {   const float pre = std::max (std::fabs (outL), std::fabs (outR));
            const float ov  = clampf ((pre - 0.9f) * 2.5f, 0.f, 1.f);
            mOver = std::max (ov, mOver * 0.99985f); }
        //  the hard bound is LAST, and DC is blocked after the ceiling
        outL = I.dcL.tick (fastTanh (outL * 0.92f) * 1.08f);
        outR = I.dcR.tick (fastTanh (outR * 0.92f) * 1.08f);
        outL = clampf (outL, -1.f, 1.f);
        outR = clampf (outR, -1.f, 1.f);

        L[i] = outL; R[i] = outR;

        const float pk = std::max (std::fabs (outL), std::fabs (outR));
        peakHold = std::max (pk, peakHold * 0.9997f);
        mPkL = std::max (std::fabs (outL), mPkL * 0.9997f);
        mPkR = std::max (std::fabs (outR), mPkR * 0.9997f);
        mMsL += 0.0004f * (outL * outL - mMsL);
        mMsR += 0.0004f * (outR * outR - mMsR);
        scopeBuf[(size_t) scopeW] = outL;
        scopeW = (scopeW + 1) % (int) scopeBuf.size();
    }
}

} // namespace gw
