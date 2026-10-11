#pragma once
// Magneto Transport - the parameter surface, in one table.
//
// Every host parameter is one row with a pointer to its Params member. The
// APVTS layout, the processor's read, the panel's initial state and the bench
// all walk this table, so there is no read order to get wrong (the Hairfryer
// pattern) and a row may be added anywhere. No JUCE in here: the bench uses it.
//
// Values are stored in PHYSICAL units (cm, cm/s, m, dB, index), which is also
// what JUCE's getRawParameterValue hands back for a float parameter.

namespace mt
{

struct Params
{
    float mode      = 0.0f;   // 0 REEL, 1 LOOP
    float loopLen   = 2.0f;   // m
    float speed     = 19.0f;  // cm/s
    float sync      = 0.0f;   // 0 off, else index into SYNC_BEATS
    float syncHead  = 0.0f;   // which playback head is the tempo head

    float condition = 15.0f;  // TRANSPORT CONDITION, %: 0 ideal, 15 freshly serviced, 100 worn out
    float capstan   = 100.0f; // the causes' balance, %: capstan assembly (flutter)
    float reels     = 100.0f; //                         reels and tension (wow)
    float scrape    = 100.0f; //                         head friction (scrape flutter)
    float motor     = 0.0f;   // 0 AC synchronous, 1 DC servo
    float broken    = 0.0f;   // OK -> BROKEN, %: the machine's causes moved towards a wreck

    float h1Pos = 4.0f,  h2Pos = 8.0f,  h3Pos = 12.0f;   // cm downstream of the record head
    float h1On  = 1.0f,  h2On  = 0.0f,  h3On  = 0.0f;
    float h1Lvl = 0.0f,  h2Lvl = 0.0f,  h3Lvl = 0.0f;    // dB
    float h1Pan = 0.0f,  h2Pan = 0.0f,  h3Pan = 0.0f;    // -1..1

    float feedback  = 0.35f;  // 0..1, the playback -> record wire
    float bass      = 0.0f;   // dB, in the feedback path
    float treble    = 0.0f;   // dB, in the feedback path

    float drive     = 0.0f;   // dB, input amplifier
    float recLevel  = 0.0f;   // dB, into the record head
    float bias      = 0.0f;   // dB re the operating bias (the 456 alignment)
    float mix       = 0.35f;  // 0 dry .. 1 wet, equal power
    float killDry   = 0.0f;
    float repro     = 0.0f;   // a tape machine: head 1 replays, its delay is reported to the host
    float output    = 0.0f;   // dB
    float inMode    = 0.0f;   // 0 STEREO, 1 MONO (sum the input)
    float eqStd     = 0.0f;   // 0 IEC, 1 NAB: the record and replay equalisation standard

    float stock     = 0.0f;   // 0 STUDIO, 1 VINTAGE, 2 SHEDDING, 3 BUDGET
    float particles = 1.0f;   // particle count relative to the stock's own; 1000 = a continuum, silent
    float wear      = 0.0f;   // %
    float accumulate = 0.0f;  // a loop wears on every pass

    float erase     = 100.0f; // FLUX MEMORY: the erase head's field in DUB, % (100 full, 0 off)
    float looper    = 1.0f;   // LOOP STATE: 0 REC, 1 DUB, 2 PLAY
    float run       = 1.0f;   // the capstan motor: 1 running, 0 off (it coasts down)
    float clear     = 0.0f;   // LOOP: a rising edge is one full pass of the erase head
    float bars      = 0.0f;   // LOOP with SYNC: the loop cut to N bars (0: LOOP LENGTH)
};

enum class Kind { Lin, Log, Choice, Toggle };

struct Spec
{
    const char* id;
    const char* name;
    Kind        kind;
    float       lo, hi, def;
    const char* unit;
    const char* choices;      // pipe separated, Choice only
    float Params::* member;
    const char* hint;
};

// Note values for SYNC, in beats. Index 0 is OFF.
constexpr int    NUM_SYNC = 13;
constexpr double SYNC_BEATS[NUM_SYNC] =
    { 0.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 3.0, 4.0, 8.0 };
#define MT_SYNC_NAMES "OFF|1/16|1/16D|1/8T|1/8|1/8D|1/4T|1/4|1/4D|1/2|1/2D|1/1|2/1"

inline const Spec SPECS[] =
{
    { "mode",     "TRANSPORT",  Kind::Choice, 0, 1, 0,   "", "REEL|LOOP", &Params::mode,
      "REEL plays an open reel: fresh tape arrives, recordings leave on the take-up reel, and the LOOP panel does nothing. LOOP runs a closed loop of tape round and round, and the LOOP panel decides what the heads do to it." },
    { "looplen",  "LOOP LENGTH", Kind::Lin,   0.5f, 6.0f, 2.0f, "m", nullptr, &Params::loopLen,
      "LOOP: the physical length of the tape loop, cut to 5 cm steps; a change splices in a new loop. With SYNC on and LOOP BARS set, the bars cut the loop instead." },
    { "speed",    "TAPE SPEED", Kind::Log,    4.75f, 76.0f, 19.0f, "cm/s", nullptr, &Params::speed,
      "Capstan speed. Delay = head distance / speed. Changes glide with the motor's inertia, so echoes on the tape bend in pitch." },
    { "sync",     "SYNC",       Kind::Choice, 0, NUM_SYNC - 1, 0, "", MT_SYNC_NAMES, &Params::sync,
      "Sets the motor speed so the tempo head lands on this note value at the host tempo." },
    { "synchead", "TEMPO HEAD", Kind::Choice, 0, 2, 0,   "", "HEAD 1|HEAD 2|HEAD 3", &Params::syncHead,
      "The playback head SYNC puts on the beat. The others land where their spacing puts them." },
    { "condition", "TRANSPORT CONDITION", Kind::Lin, 0.0f, 100.0f, 15.0f, "%", nullptr, &Params::condition,
      "How worn the transport is. 0 is an ideal machine, 15 a freshly serviced one, 100 worn out. It sets physical causes - runout, rough bearings, tension variation, friction - and the wow and flutter are what they do." },
    { "capstan", "CAPSTAN", Kind::Lin, 0.0f, 200.0f, 100.0f, "%", nullptr, &Params::capstan,
      "The capstan assembly's share of the wear: shaft and pinch-roller runout, bearing roughness, motor ripple. Flutter." },
    { "reels", "REELS", Kind::Lin, 0.0f, 200.0f, 100.0f, "%", nullptr, &Params::reels,
      "The reels' share: eccentricity and holdback friction vary the tension, and tension stretches the tape. Wow." },
    { "scrape", "SCRAPE", Kind::Lin, 0.0f, 200.0f, 100.0f, "%", nullptr, &Params::scrape,
      "Friction at the heads: the tape sticks and slips and rings at a few kHz. Heard as fuzz around a tone, not as wow." },
    { "motor", "MOTOR", Kind::Choice, 0, 1, 0, "", "AC SYNC|DC SERVO", &Params::motor,
      "AC SYNC: a synchronous capstan motor, hunting gently when the speed changes. DC SERVO: a tachometer-controlled motor, quicker and firmer, with its own slot ripple." },
    { "broken", "OK / BROKEN", Kind::Lin, 0.0f, 100.0f, 0.0f, "%", nullptr, &Params::broken,
      "A neglected machine, in one control. It moves the real causes from where the knobs are towards a wreck - CONDITION, CAPSTAN, REELS, SCRAPE and WEAR up, the bias oscillator sagging, the erase head weakening on a loop - and the knobs show where it has taken them. Past the middle three faults appear: oxide clogging a head (the treble drops away for seconds, then the clog breaks loose), the motor's speed control hunting (slow pitch swings), and a crinkled stretch of tape (a burst of dropouts and flutter, once a loop). OK is the knobs exactly." },

    { "h1pos", "HEAD 1 POSITION", Kind::Lin, 1.5f, 24.0f, 4.0f,  "cm", nullptr, &Params::h1Pos, "Distance from the record head along the rail." },
    { "h2pos", "HEAD 2 POSITION", Kind::Lin, 1.5f, 24.0f, 8.0f,  "cm", nullptr, &Params::h2Pos, "Distance from the record head along the rail." },
    { "h3pos", "HEAD 3 POSITION", Kind::Lin, 1.5f, 24.0f, 12.0f, "cm", nullptr, &Params::h3Pos, "Distance from the record head along the rail." },
    { "h1on",  "HEAD 1",  Kind::Toggle, 0, 1, 1, "", nullptr, &Params::h1On, "Playback head 1 on or off." },
    { "h2on",  "HEAD 2",  Kind::Toggle, 0, 1, 0, "", nullptr, &Params::h2On, "Playback head 2 on or off." },
    { "h3on",  "HEAD 3",  Kind::Toggle, 0, 1, 0, "", nullptr, &Params::h3On, "Playback head 3 on or off." },
    { "h1lvl", "HEAD 1 LEVEL", Kind::Lin, -36.0f, 6.0f, 0.0f, "dB", nullptr, &Params::h1Lvl, "Playback level of head 1." },
    { "h2lvl", "HEAD 2 LEVEL", Kind::Lin, -36.0f, 6.0f, 0.0f, "dB", nullptr, &Params::h2Lvl, "Playback level of head 2." },
    { "h3lvl", "HEAD 3 LEVEL", Kind::Lin, -36.0f, 6.0f, 0.0f, "dB", nullptr, &Params::h3Lvl, "Playback level of head 3." },
    { "h1pan", "HEAD 1 PAN", Kind::Lin, -1.0f, 1.0f, 0.0f, "", nullptr, &Params::h1Pan, "Where head 1 sits in the stereo field." },
    { "h2pan", "HEAD 2 PAN", Kind::Lin, -1.0f, 1.0f, 0.0f, "", nullptr, &Params::h2Pan, "Where head 2 sits in the stereo field." },
    { "h3pan", "HEAD 3 PAN", Kind::Lin, -1.0f, 1.0f, 0.0f, "", nullptr, &Params::h3Pan, "Where head 3 sits in the stereo field." },

    { "feedback", "FEEDBACK", Kind::Lin, 0.0f, 1.0f, 0.35f, "", nullptr, &Params::feedback,
      "How much of the playback is wired back into the record amplifier." },
    { "bass",     "BASS",     Kind::Lin, -12.0f, 12.0f, 0.0f, "dB", nullptr, &Params::bass,   "Low shelf in the feedback path." },
    { "treble",   "TREBLE",   Kind::Lin, -12.0f, 12.0f, 0.0f, "dB", nullptr, &Params::treble, "High shelf in the feedback path." },

    { "drive",    "INPUT DRIVE",  Kind::Lin, -24.0f, 24.0f, 0.0f, "dB", nullptr, &Params::drive,    "Gain of the input amplifier." },
    { "reclevel", "RECORD LEVEL", Kind::Lin, -24.0f, 12.0f, 0.0f, "dB", nullptr, &Params::recLevel, "How hard the record head drives the tape. 0 dB puts a -18 dBFS tone at 250 nWb/m." },
    { "bias",     "BIAS",     Kind::Lin, -10.0f, 10.0f, 0.0f, "dB", nullptr, &Params::bias,
      "Bias current, relative to the alignment point (10 kHz 3 dB past its peak). Less bias: brighter, louder, dirtier. More: cleaner and darker." },
    { "mix",      "MIX",      Kind::Lin, 0.0f, 1.0f, 0.35f, "", nullptr, &Params::mix,     "Dry to wet, equal power." },
    { "killdry",  "KILL DRY", Kind::Toggle, 0, 1, 0, "", nullptr, &Params::killDry,         "Removes the dry signal entirely, for use on a send." },
    { "repro",    "REPRO",    Kind::Toggle, 0, 1, 0, "", nullptr, &Params::repro,
      "A tape machine on an insert: head 1 is the replay head, heads 2 and 3 and FEEDBACK are off, and head 1's delay is reported to the host so the track stays in time. MIX blends the tape with the dry signal, delayed to match. Moving head 1 or changing the speed changes the delay the host has to make up; some hosts click when it does." },
    { "output",   "OUTPUT",   Kind::Lin, -24.0f, 12.0f, 0.0f, "dB", nullptr, &Params::output, "Output level." },
    { "eq",       "EQUALISATION", Kind::Choice, 0, 1, 0, "", "IEC|NAB", &Params::eqStd,
      "The record and replay equalisation standard. IEC (the default) lifts the treble on replay; NAB also boosts the bass on record, so lows saturate a little sooner." },
    { "inmode",   "INPUT",    Kind::Choice, 0, 1, 0, "", "STEREO|MONO", &Params::inMode,
      "STEREO records left and right on their own tracks. MONO records the sum on both, for mono-to-stereo use." },

    { "stock", "TAPE STOCK", Kind::Choice, 0, 3, 0, "", "STUDIO|VINTAGE|SHEDDING|BUDGET", &Params::stock,
      "The tape on the machine. STUDIO: a 1970s high-output studio tape (456 class). VINTAGE: a 1960s low-output tape, less headroom and more hiss. SHEDDING: the studio tape decayed - oxide lost, dropouts, a sticky binder. BUDGET: cheap echo-machine tape. The machine is aligned to each fresh stock; changing it threads a new reel." },
    { "particles", "PARTICLES", Kind::Log, 0.1f, 1000.0f, 1.0f, "x", nullptr, &Params::particles,
      "Particle count against the stock's own. Hiss is the statistics of the particles: ten times as many, 10 dB less hiss. At the top the coating is a continuum and the tape is silent." },
    { "wear", "WEAR", Kind::Lin, 0.0f, 100.0f, 0.0f, "%", nullptr, &Params::wear,
      "How worn the tape is: oxide lost, a rougher surface, more dropouts, debris lifting it off the heads." },
    { "erase", "ERASE", Kind::Lin, 0.0f, 100.0f, 100.0f, "%", nullptr, &Params::erase,
      "FLUX MEMORY, in DUB: the erase head's field. 100 % wipes the loop before the record head writes it again - a clean echo. Turned down, earlier passes survive as magnetisation and come round every loop period: the knob walks from a pass losing 60 dB (90 %) to one losing under a decibel, and 0 switches the head off. The record head's bias still takes some off on every pass: the less BIAS, the longer they live." },
    { "looper", "LOOP STATE", Kind::Choice, 0, 2, 1, "", "REC|DUB|PLAY", &Params::looper,
      "LOOP: what the heads do to the loop. REC wipes it with the erase head at full and records a fresh take every pass. DUB records on top, with the erase head at the ERASE knob (at 100 % that is the plain echo). PLAY turns record and bias off: the loop plays what is on it, unchanged, and FEEDBACK has nothing to write. Tape has no undo." },
    { "run", "MOTOR RUN", Kind::Toggle, 0, 1, 1, "", nullptr, &Params::run,
      "Starts and stops the capstan motor. Stopped, it coasts down and everything on the tape dives in pitch; started, it pulls back up to speed." },
    { "clear", "CLEAR", Kind::Toggle, 0, 1, 0, "", nullptr, &Params::clear,
      "LOOP: one full pass of the erase head over the loop, record off. Then the loop is blank." },
    { "bars", "LOOP BARS", Kind::Lin, 0.0f, 16.0f, 0.0f, "", nullptr, &Params::bars,
      "LOOP with SYNC on: the loop is cut to this many 4/4 bars at the tempo head's note value (a new loop, so a new tape), and then it is N bars at any tempo. Dragging the tempo head afterwards changes the speed, so the loop stops being N bars until BARS is set again. FREE uses LOOP LENGTH." },
    { "accumulate", "ACCUMULATE", Kind::Toggle, 0, 1, 0, "", nullptr, &Params::accumulate,
      "A loop wears a little on every pass over the heads, and keeps its wear with the project. Off, the tape stays as WEAR sets it." },
};
inline constexpr int NUM_SPECS = (int) (sizeof (SPECS) / sizeof (SPECS[0]));

} // namespace mt
