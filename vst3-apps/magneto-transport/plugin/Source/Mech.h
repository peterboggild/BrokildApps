#pragma once
/*  Magneto Transport - MECH SOUNDS: what the machine itself sounds like.

    Every sound here is driven by the transport the engine simulates, so it moves
    when the machine moves: the capstan's rumble turns at the capstan's own rate
    (v / 2 pi r, 6.1 Hz at 19 cm/s), each reel's bearing at that reel's rate (which
    changes as the pack winds across), the loop's splice ticks once per pass of the
    loop, a carriage whines only while it travels, and the solenoids clunk when the
    motor is started or stopped and when the transport changes. An AC synchronous
    motor hums at twice the mains (100 Hz, and its harmonics) while it is powered; a
    DC servo motor has no hum but whines at its slot rate, which follows its speed.

    Added to the main output after OUTPUT, at its own level (MECH SOUNDS, off by
    default). At 0 the engine does not call this at all, so the output is bit for bit
    the machine without it. Plain C++, no JUCE: the bench drives it. Host rate. */

#include <cmath>
#include <cstdint>

namespace mt
{

class Mech
{
public:
    struct In
    {
        double level = 0.0;        // MECH SOUNDS, 0..1 (0 = off)
        double vCms = 0.0;         // the tape's speed now
        bool   powered = false;    // RUN: the capstan motor has power
        bool   dcServo = false;    // MOTOR: DC SERVO (else AC SYNC)
        int    mode = 0;           // 0 REEL, 1 LOOP
        int    looper = 0;         // LOOP STATE (only its changes matter)
        double supplyR = 0.1, takeupR = 0.1;   // m
        double loopM = 2.0;
        double headCm[3] {}, headVelCms[3] {};
        double cond = 0.15;        // TRANSPORT CONDITION 0..1 (after BROKEN)
        double capstan = 1.0, reels = 1.0, scrape = 1.0;   // the causes' balance, 0..2
        bool   splice = false;     // the transport is splicing (a new tape or loop): it clunks once
    };

    static constexpr double CAPSTAN_R = 0.005;   // m, as the engine's
    static constexpr double REF_CMS = 19.05;
    static constexpr double CARRIAGE_MAX = 12.0;  // cm/s, as the engine's
    static constexpr double TOP = 6.0;            // the top of the knob: about -35 dBFS rms for a 19 cm/s reel

    void prepare (double fsIn, uint64_t seedIn)
    {
        fs = fsIn; inv = 1.0 / fs;
        rng = (uint32_t) (seedIn ^ (seedIn >> 32)) | 1u;
        aSm = 1.0 - std::exp (-1.0 / (fs * 0.012));
        aPow = 1.0 - std::exp (-1.0 / (fs * 0.15));    // a motor's power ramp
        rumbleLp.set (fs, 260.0, 0.6); rumbleHp.set (fs, 35.0, 0.7);
        for (auto& f : reelBp) f.set (fs, 380.0, 0.9);
        tankBp.set (fs, 1700.0, 0.8);
        scrapeBp.set (fs, 4200.0, 1.3);
        tickBp.set (fs, 2600.0, 1.6);
        clickBp.set (fs, 1500.0, 1.1);
        for (auto& f : carBp) f.set (fs, 950.0, 1.0);
        setDecays();
        reset();
    }

    void reset()
    {
        gKnob = 0.0; gKnobT = 0.0; pow = 0.0;
        phMains = phCap = phSup = phTak = phDc = 0.0;
        for (double& p : phCar) p = 0.0;
        for (double& a : amp) a = 0.0;
        for (double& a : ampT) a = 0.0;
        spliceAcc = 0.0; tickEnv = thumpEnv = clunkEnv = clickEnv = 0.0; thumpPh = clunkPh = 0.0;
        first = true;
        rumbleLp.reset(); rumbleHp.reset(); for (auto& f : reelBp) f.reset(); tankBp.reset();
        scrapeBp.reset(); tickBp.reset(); clickBp.reset(); for (auto& f : carBp) f.reset();
    }

    /*  Once per block, before the samples. */
    void block (const In& in)
    {
        // the knob: 0 is off; above, -36 dB at the bottom of the travel to 0 dB at the top
        gKnobT = in.level <= 0.0 ? 0.0 : std::pow (10.0, -36.0 * (1.0 - std::fmin (1.0, in.level)) / 20.0);
        const double v = std::fmax (0.0, in.vCms), vr = v / REF_CMS, vm = v * 0.01;
        x = in;
        fCap = vm / (2.0 * M_PI * CAPSTAN_R);
        fSup = vm / (2.0 * M_PI * std::fmax (0.03, in.supplyR));
        fTak = vm / (2.0 * M_PI * std::fmax (0.03, in.takeupR));
        // a worn transport is a louder one: rough bearings, a slipping clutch
        const double wear = 0.55 + 1.6 * in.cond;
        ampT[MOTOR]  = in.dcServo ? 0.0 : 1.0;                             // scaled by the power ramp
        ampT[DC]     = in.dcServo ? std::fmin (1.0, vr) : 0.0;
        ampT[RUMBLE] = std::pow (vr, 0.7) * wear * (0.35 + 0.65 * in.capstan);
        ampT[REELS]  = in.mode == 0 ? vr * wear * (0.35 + 0.65 * in.reels) : 0.0;
        ampT[TANK]   = in.mode == 1 ? vr * (0.6 + 0.8 * in.cond) : 0.0;
        ampT[SCRAPE] = vr * (0.3 + 0.7 * in.scrape) * (0.7 + 0.6 * in.cond);
        for (int i = 0; i < 3; ++i) carT[i] = std::fmin (1.0, std::fabs (in.headVelCms[i]) / CARRIAGE_MAX);

        // events: a clunk when the motor is started or stopped and when the transport changes
        if (! first)
        {
            if (in.powered != lastPowered) { clunkEnv = 1.0; clunkPh = 0.0; clickEnv = 1.0; clunkPan = PAN_CAPSTAN; }
            if (in.mode != lastMode || (in.mode == 1 && in.looper != lastLooper) || (in.splice && ! lastSplice))
                { clunkEnv = std::fmax (clunkEnv, 0.8); clunkPh = 0.0; clickEnv = 1.0; clunkPan = 0.0; }
        }
        if (in.mode != lastMode || (in.splice && ! lastSplice)) spliceAcc = 0.0;
        lastPowered = in.powered; lastMode = in.mode; lastLooper = in.looper; lastSplice = in.splice; first = false;
    }

    /*  True while anything can still be heard (the knob or its fade). */
    bool active() const { return gKnobT > 0.0 || gKnob > 1.0e-7; }

    void tick (float& outL, float& outR)
    {
        gKnob += (gKnobT - gKnob) * aSm;
        pow += ((x.powered ? 1.0 : 0.0) - pow) * aPow;
        for (int i = 0; i < NAMP; ++i) amp[i] += (ampT[i] - amp[i]) * aSm;
        for (int i = 0; i < 3; ++i) car[i] += (carT[i] - car[i]) * aSm;
        double L = 0.0, R = 0.0;
        auto put = [&L, &R] (double s, double pan) {
            const double a = (pan + 1.0) * (M_PI / 4.0);
            L += s * std::cos (a); R += s * std::sin (a);
        };
        const double tw = 2.0 * M_PI;

        // the AC motor: magnetostriction at twice the 50 Hz mains, with its harmonics
        phMains += 100.0 * inv; if (phMains >= 1.0) phMains -= 1.0;
        const double hum = std::sin (tw * phMains) + 0.45 * std::sin (2.0 * tw * phMains) + 0.2 * std::sin (3.0 * tw * phMains);
        put (0.0030 * amp[MOTOR] * pow * hum, PAN_CAPSTAN);

        // the capstan turns: its rumble is modulated once (runout) and twice (ovality) per turn
        phCap += fCap * inv; if (phCap >= 1.0) phCap -= 1.0;
        const double turn = 1.0 + 0.55 * std::sin (tw * phCap) + 0.25 * std::sin (2.0 * tw * phCap + 0.7);
        // the DC servo: 24 slots, a whine at 24 x the rotor (here direct drive, the capstan)
        phDc += 24.0 * fCap * inv; if (phDc >= 1.0) phDc -= 1.0;
        put (0.0060 * amp[DC] * (std::sin (tw * phDc) + 0.3 * std::sin (2.0 * tw * phDc)), PAN_CAPSTAN);
        const double rum = rumbleHp.hp (rumbleLp.lp (white()));
        put (0.030 * amp[RUMBLE] * turn * rum, PAN_CAPSTAN);

        // the reels: each bearing, and the pack's eccentricity, once per turn of that reel
        phSup += fSup * inv; if (phSup >= 1.0) phSup -= 1.0;
        phTak += fTak * inv; if (phTak >= 1.0) phTak -= 1.0;
        if (amp[REELS] > 1.0e-6)
        {
            put (0.012 * amp[REELS] * (1.0 + 0.6 * std::sin (tw * phSup)) * reelBp[0].bp (white()), PAN_SUPPLY);
            put (0.012 * amp[REELS] * (1.0 + 0.6 * std::sin (tw * phTak)) * reelBp[1].bp (white()), PAN_TAKEUP);
        }
        // the loop: tape folding and sliding in the tank
        if (amp[TANK] > 1.0e-6) put (0.006 * amp[TANK] * tankBp.bp (white()), PAN_TANK);
        // tape over the heads and guides
        put (0.0035 * amp[SCRAPE] * scrapeBp.bp (white()), PAN_HEADS);

        // the splice passes the heads once per loop
        if (x.mode == 1 && x.loopM > 0.1)
        {
            spliceAcc += std::fmax (0.0, x.vCms) * 0.01 * inv;
            if (spliceAcc >= x.loopM) { spliceAcc -= x.loopM; tickEnv = 1.0; thumpEnv = 1.0; thumpPh = 0.0; }
        }
        if (tickEnv > 1.0e-5 || thumpEnv > 1.0e-5)
        {
            const double s = 0.10 * tickEnv * tickBp.bp (white()) + 0.012 * thumpEnv * std::sin (tw * thumpPh);
            thumpPh += 140.0 * inv;
            tickEnv *= kTick; thumpEnv *= kThump;
            put (s, PAN_HEADS);
        }

        // the carriages: a small geared motor and a slide, while they travel
        for (int i = 0; i < 3; ++i)
        {
            if (car[i] < 1.0e-5) continue;
            const double f = 160.0 + 420.0 * car[i];
            phCar[i] += f * inv; if (phCar[i] >= 1.0) phCar[i] -= 1.0;
            const double s = std::sin (tw * phCar[i]) + 0.35 * std::sin (3.0 * tw * phCar[i]);
            const double pan = std::fmax (-1.0, std::fmin (1.0, (600.0 + 15.0 * x.headCm[i] - 800.0) / 800.0));
            put (car[i] * (0.012 * s + 0.025 * carBp[i].bp (white())), pan);
        }

        // the solenoids: a thump of the armature and the click of its stop
        if (clunkEnv > 1.0e-5 || clickEnv > 1.0e-5)
        {
            const double s = 0.060 * clunkEnv * std::sin (tw * clunkPh) + 0.05 * clickEnv * clickBp.bp (white());
            clunkPh += 72.0 * inv;
            clunkEnv *= kClunk; clickEnv *= kClick;
            put (s, clunkPan);
        }

        outL = (float) (L * gKnob * TOP);
        outR = (float) (R * gKnob * TOP);
    }

private:
    struct Svf   // TPT state variable filter
    {
        double g = 0.0, k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, s1 = 0.0, s2 = 0.0;
        void set (double fs, double f, double q)
        {
            g = std::tan (M_PI * f / fs); k = 1.0 / q;
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        void reset() { s1 = s2 = 0.0; }
        void run (double x, double& lp, double& bp, double& hp)
        {
            const double v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0 * v1 - s1; s2 = 2.0 * v2 - s2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        double lp (double x) { double l, b, h; run (x, l, b, h); return l; }
        double bp (double x) { double l, b, h; run (x, l, b, h); return b * k; }   // unity at the centre
        double hp (double x) { double l, b, h; run (x, l, b, h); return h; }
    };

    double white()   // uniform, unit variance
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return ((double) rng * (1.0 / 4294967296.0) - 0.5) * 3.4641016151377544;
    }

    enum { MOTOR, DC, RUMBLE, REELS, TANK, SCRAPE, NAMP };
    // where each part sits on the deck (the panel's machine: 1600 wide, the centre at 800)
    static constexpr double PAN_CAPSTAN = (1010.0 - 800.0) / 800.0;
    static constexpr double PAN_SUPPLY  = (230.0 - 800.0) / 800.0;
    static constexpr double PAN_TAKEUP  = (1370.0 - 800.0) / 800.0;
    static constexpr double PAN_TANK    = (205.0 - 800.0) / 800.0;
    static constexpr double PAN_HEADS   = (700.0 - 800.0) / 800.0;

    double fs = 48000.0, inv = 1.0 / 48000.0, aSm = 0.0, aPow = 0.0;
    double kTick = 0.0, kThump = 0.0, kClunk = 0.0, kClick = 0.0;
    uint32_t rng = 1;
    In x;
    double gKnob = 0.0, gKnobT = 0.0, pow = 0.0;
    double fCap = 0.0, fSup = 0.0, fTak = 0.0;
    double phMains = 0.0, phCap = 0.0, phSup = 0.0, phTak = 0.0, phDc = 0.0, phCar[3] {};
    double amp[NAMP] {}, ampT[NAMP] {}, car[3] {}, carT[3] {};
    double spliceAcc = 0.0, tickEnv = 0.0, thumpEnv = 0.0, thumpPh = 0.0;
    double clunkEnv = 0.0, clickEnv = 0.0, clunkPh = 0.0, clunkPan = 0.0;
    bool first = true, lastPowered = false, lastSplice = false;
    int lastMode = 0, lastLooper = 0;
    Svf rumbleLp, rumbleHp, reelBp[2], tankBp, scrapeBp, tickBp, clickBp, carBp[3];

public:
    /*  The decays, set once from the sample rate (time constants in seconds). */
    void setDecays()
    {
        kTick  = std::exp (-1.0 / (fs * 0.0022));
        kThump = std::exp (-1.0 / (fs * 0.010));
        kClunk = std::exp (-1.0 / (fs * 0.045));
        kClick = std::exp (-1.0 / (fs * 0.004));
    }
};

} // namespace mt
