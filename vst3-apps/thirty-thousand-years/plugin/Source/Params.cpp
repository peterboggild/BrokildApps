#include "Params.h"
#include <cstring>
#include <string>

namespace tty
{

static const PSpec SPECS[] =
{
   #define TTY_ROW(sym, id, name, def, kind, lo, hi, flags) { id, name, (float) (def), kind, (float) (lo), (float) (hi), flags },
    TTY_PARAMS (TTY_ROW)
   #undef TTY_ROW
};

int numParams() { return NUM_PARAMS; }
const PSpec& paramSpec (int i) { return SPECS[i < 0 ? 0 : (i >= NUM_PARAMS ? NUM_PARAMS - 1 : i)]; }

int paramIndex (const char* id)
{
    for (int i = 0; i < NUM_PARAMS; ++i)
        if (std::strcmp (SPECS[i].id, id) == 0) return i;
    return -1;
}

bool paramStepped (const PSpec& s)
{
    return s.kind == KP_LIST || s.kind == KP_INT || s.kind == KP_SW || s.kind == KP_NOTE;
}

float paramMax (const PSpec& s)
{
    if (s.kind == KP_SW) return 1.0f;
    if (s.kind == KP_LIST || s.kind == KP_INT || s.kind == KP_NOTE) return s.hi;
    return 1.0f;
}

bool paramModulatable (const PSpec& s) { return ! paramStepped (s) && ! (s.flags & F_NM); }
bool paramInScene (const PSpec& s)     { return ! (s.flags & F_NS); }

// ---- list names ----------------------------------------------------------------
static const char* L_QUALITY[]  = { "LIVE", "STUDIO", "RENDER" };
static const char* L_VMODE[]    = { "POLY", "MONO", "LEGATO" };
static const char* L_SCALE[]    = { "FREE", "CHROMATIC", "MINOR", "PHRYGIAN", "LOCRIAN", "HARMONIC MINOR", "WHOLE TONE", "PENTATONIC MINOR", "DORIAN", "SCALA FILE" };
static const char* L_CHORD[]    = { "UNISON", "OCTAVES", "FIFTH", "MINOR", "MAJOR", "SUS2", "MINOR 7", "CLUSTER", "TRITONE", "OPEN FIFTHS" };
static const char* L_EXTMODE[]  = { "OFF", "EXCITE", "DUCK", "EXCITE + DUCK" };
static const char* L_WAVE[]     = { "SINE", "TRIANGLE", "SAW", "PULSE" };
static const char* L_OCT[]      = { "-2", "-1", "0", "+1", "+2" };
static const char* L_UNISON[]   = { "1", "2", "3" };
static const char* L_SUB[]      = { "OFF", "-1 OCT", "-2 OCT" };
static const char* L_SUBWAVE[]  = { "SINE", "SQUARE" };
static const char* L_MFILT[]    = { "LADDER", "SVF LOW", "SVF BAND", "SVF HIGH", "SVF NOTCH" };
static const char* L_SFILT[]    = { "OFF", "LADDER", "SVF LOW", "SVF BAND", "SVF HIGH", "SVF NOTCH" };
static const char* L_TABLE[]    = { "GEOMETRIC", "MACHINE", "FORMANT", "HOLLOW", "STEPPED", "METALLIC", "SIREN", "ORGANISM" };
static const char* L_PMRATIO[]  = { "1/2", "1", "3/2", "2", "3", "4", "5", "7", "FIXED" };
static const char* L_MEMSRC[]   = { "VOICE", "BROADCAST", "CHOIR", "MACHINE", "CAPTURE", "IMPORT" };
static const char* L_MEMMODE[]  = { "GRAINS", "SPECTRAL", "BOTH" };
static const char* L_WIN[]      = { "HANN", "TUKEY", "TRIANGLE", "EXPO", "REVERSE EXPO", "SOFT RECT" };
static const char* L_SCHED[]    = { "CLOCKED", "IRREGULAR", "CLUSTERED" };
static const char* L_CAPSRC[]   = { "PRE-LOOP", "POST-SPACE", "AUX INPUT" };
static const char* L_BODY[]     = { "PLATE", "CABLE", "BEAM", "CAVITY", "GLASS", "TUBE", "COMB" };
static const char* L_EXC[]      = { "IMPULSE", "NOISE BURST", "FRICTION", "MASS", "SIGNAL", "MEMORY", "LOOP RETURN", "AUX INPUT" };
static const char* L_SLOT[]     = { "—", "SATURATE", "FOLD", "MULTIBAND", "SHIFT", "DELAY", "COMB", "CRUSH" };
static const char* L_DLMODE[]   = { "TAPE", "CLEAN" };
static const char* L_FBTO[]     = { "MIX", "STRUCTURE", "MEMORY", "SIGNAL" };
static const char* L_LFOWAVE[]  = { "SINE", "TRIANGLE", "SAW", "SQUARE", "SAMPLE & HOLD", "SMOOTH RANDOM" };
static const char* L_SYNC[]     = { "FREE", "8 BARS", "4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16" };
static const char* L_SCOPE[]    = { "GLOBAL", "PER VOICE" };
static const char* L_ETRIG[]    = { "NOTE", "DRONE START", "EVENT LANE" };
static const char* L_STO[]      = { "SMOOTH NOISE", "RANDOM WALK", "SAMPLE & HOLD", "PROBABILITY", "CLUSTER BURSTS", "CHAOS" };
static const char* L_FOLSRC[]   = { "MASS", "SIGNAL", "MEMORY", "STRUCTURE", "MIX", "AUX INPUT" };
static const char* L_EVTGT[]    = { "STRIKE", "GRAIN BURST", "FREEZE", "EROSION PULSE", "LOOP KICK", "SIGNAL RESET", "SCENE NUDGE" };
static const char* L_HMODE[]    = { "MANUAL", "AUTO", "AUTO SYNC" };
static const char* L_HBARS[]    = { "1", "2", "4", "8", "16", "32", "64", "128" };
static const char* L_HLOOP[]    = { "ONCE", "LOOP", "PING-PONG" };

#define RET(arr) { count = (int) (sizeof (arr) / sizeof (arr[0])); return arr; }

const char* const* listNames (const char* id, int& count)
{
    const std::string s (id);
    auto ends = [&] (const char* suf) { const size_t n = std::strlen (suf); return s.size() >= n && s.compare (s.size() - n, n, suf) == 0; };
    auto starts = [&] (const char* pre) { return s.rfind (pre, 0) == 0; };

    if (s == "quality") RET (L_QUALITY)
    if (s == "vmode") RET (L_VMODE)
    if (s == "scale") RET (L_SCALE)
    if (s == "drone_chord") RET (L_CHORD)
    if (s == "ext_mode") RET (L_EXTMODE)
    if (s == "m_o1wave" || s == "m_o2wave") RET (L_WAVE)
    if (ends ("oct")) RET (L_OCT)
    if (s == "m_unison") RET (L_UNISON)
    if (s == "m_sub") RET (L_SUB)
    if (s == "m_subwave") RET (L_SUBWAVE)
    if (s == "m_fmode") RET (L_MFILT)
    if (s == "s_fmode") RET (L_SFILT)
    if (s == "s_wt1tab" || s == "s_wt2tab") RET (L_TABLE)
    if (s == "s_pmratio") RET (L_PMRATIO)
    if (s == "mem_src") RET (L_MEMSRC)
    if (s == "mem_mode") RET (L_MEMMODE)
    if (s == "mem_win") RET (L_WIN)
    if (s == "mem_sched") RET (L_SCHED)
    if (s == "mem_capsrc") RET (L_CAPSRC)
    if (s == "st_model") RET (L_BODY)
    if (s == "st_exc") RET (L_EXC)
    if (starts ("e_a") || starts ("e_b")) if (s.size() == 4) RET (L_SLOT)
    if (s == "e_dl_mode") RET (L_DLMODE)
    if (s == "e_fb_to") RET (L_FBTO)
    if (starts ("l") && ends ("_wave")) RET (L_LFOWAVE)
    if (starts ("l") && ends ("_sync")) RET (L_SYNC)
    if (starts ("l") && ends ("_scope")) RET (L_SCOPE)
    if (starts ("e") && ends ("_trig")) RET (L_ETRIG)
    if (starts ("r") && ends ("_type")) RET (L_STO)
    if (starts ("f") && ends ("_src")) RET (L_FOLSRC)
    if (starts ("v") && ends ("_target")) RET (L_EVTGT)
    if (s == "h_mode") RET (L_HMODE)
    if (s == "h_bars") RET (L_HBARS)
    if (s == "h_loop") RET (L_HLOOP)
    count = 0;
    return nullptr;
}

} // namespace tty
