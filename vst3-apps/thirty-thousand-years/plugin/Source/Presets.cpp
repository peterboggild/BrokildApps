#include "Presets.h"
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>

namespace tty
{

// ---- the DSL parser ----------------------------------------------------------
static int sourceByName (const std::string& s)
{
    if (! s.empty() && (std::isdigit ((unsigned char) s[0]))) return std::atoi (s.c_str());
    static const char* N[NUM_MODSRC] = {
        "NONE", "LFO1", "LFO2", "LFO3", "LFO4", "LFO5", "LFO6", "LFO7", "LFO8",
        "ENV1", "ENV2", "ENV3", "ENV4", "SHAPE1", "SHAPE2", "SHAPE3", "SHAPE4",
        "RND1", "RND2", "RND3", "RND4", "FOL1", "FOL2", "EVT1", "EVT2", "EVT3", "EVT4",
        "NE_MASS", "NE_SIGNAL", "NE_MEMORY", "NE_STRUCT", "NE_MIX",
        "NB_MASS", "NB_SIGNAL", "NB_MEMORY", "NB_STRUCT", "NB_MIX",
        "NT_MASS", "NT_SIGNAL", "NT_MEMORY", "NT_STRUCT", "NT_MIX",
        "VEL", "PRESS", "KEY", "WHEEL", "BEND",
        "MAC1", "MAC2", "MAC3", "MAC4", "MAC5", "MAC6", "MAC7", "MAC8",
        "HIST", "VRND", "EROS", "CONST" };
    for (int i = 0; i < NUM_MODSRC; ++i) if (s == N[i]) return i;
    return 0;
}

static int macroByName (const std::string& s)
{
    static const char* N[NUM_MACROS] = { "MASS", "DREAD", "VIOLENCE", "INSTABILITY", "CONTAMINATION", "DISTANCE", "LIFE", "HUMANITY" };
    for (int i = 0; i < NUM_MACROS; ++i) if (s == N[i]) return i;
    return -1;
}

void defaultMacros (MacroDest macro[NUM_MACROS][MACRO_DESTS])
{
    Engine tmp; for (int m = 0; m < NUM_MACROS; ++m) for (int d = 0; d < MACRO_DESTS; ++d) macro[m][d] = tmp.macro[m][d];
}

int applyPresetBody (const char* body, Params& base, MacroDest macro[NUM_MACROS][MACRO_DESTS],
                     Params scene[NUM_SCENES], bool sceneSet[NUM_SCENES], Slot slots[NUM_SLOTS], Mseg mseg[4])
{
    int count = 0, nextSlot = 0;
    std::istringstream all (body);
    std::string line;
    // scenes are the base with overrides; collect the overrides then build after the base is known
    std::string sceneText[NUM_SCENES]; bool sceneGiven[NUM_SCENES] = { false, false, false, false };
    for (int i = 0; i < NUM_SLOTS; ++i) slots[i] = Slot();
    for (int i = 0; i < 4; ++i) { mseg[i].defaults(); }
    auto assign = [&] (Params& tgt, const std::string& tok)
    {
        const size_t eq = tok.find ('=');
        if (eq == std::string::npos) return;
        const int idx = paramIndex (tok.substr (0, eq).c_str());
        if (idx < 0) return;
        tgt.v[idx] = clampf ((float) std::atof (tok.c_str() + eq + 1), 0.0f, paramMax (paramSpec (idx)));
        ++count;
    };
    while (std::getline (all, line))
    {
        std::istringstream ls (line);
        std::string tok; ls >> tok;
        if (tok.empty()) continue;
        if (tok == "#MACRO")
        {
            std::string nm; ls >> nm; const int m = macroByName (nm); if (m < 0) continue;
            for (int d = 0; d < MACRO_DESTS; ++d) macro[m][d] = MacroDest();
            int d = 0;
            while (ls >> tok && d < MACRO_DESTS)
            {
                const size_t c = tok.find (':'); if (c == std::string::npos) continue;
                const int idx = paramIndex (tok.substr (0, c).c_str()); if (idx < 0) continue;
                macro[m][d].dst = idx; macro[m][d].depth = (float) std::atof (tok.c_str() + c + 1); ++d; ++count;
            }
        }
        else if (tok == "#SCENE")
        {
            int n = 0; ls >> n; if (n < 1 || n > NUM_SCENES) continue;
            std::string rest; std::getline (ls, rest); sceneText[n - 1] = rest; sceneGiven[n - 1] = true;
        }
        else if (tok == "#SLOT")
        {
            if (nextSlot >= NUM_SLOTS) continue;
            std::string src, dst; float depth = 0; ls >> src >> dst >> depth;
            Slot& s = slots[nextSlot];
            s.src = sourceByName (src); s.dst = paramIndex (dst.c_str()); s.depth = depth; s.on = s.dst >= 0 && s.src > 0;
            std::string via; if (ls >> via) s.via = sourceByName (via);
            int curve = 0; if (ls >> curve) s.curve = curve;
            float slew = 0; if (ls >> slew) s.slew = slew;
            float lo = -1, hi = 1; if (ls >> lo >> hi) { s.lo = lo; s.hi = hi; }
            ++nextSlot; ++count;
        }
        else if (tok == "#SHAPE")
        {
            int n = 0; ls >> n; if (n < 1 || n > 4) continue;
            Mseg& m = mseg[n - 1]; int trig = 0, loop = 0; ls >> trig >> loop; m.trig = trig; m.loop = loop != 0; m.n = 0;
            float t, l, c;
            while (m.n < MSEG_MAX && (ls >> t >> l >> c)) { m.pts[m.n] = { t, l, c }; ++m.n; }
            if (m.n < 2) m.defaults();
            ++count;
        }
        else
        {
            assign (base, tok);
            while (ls >> tok) assign (base, tok);
        }
    }
    bool any = false; for (int i = 0; i < NUM_SCENES; ++i) any |= sceneGiven[i];
    for (int i = 0; i < NUM_SCENES; ++i)
    {
        scene[i] = base; sceneSet[i] = any;      // if any scene is written, all four exist (unwritten ones = the base)
        if (! sceneGiven[i]) continue;
        std::istringstream ss (sceneText[i]);
        std::string t; while (ss >> t) assign (scene[i], t);
    }
    return count;
}

// ---- mutate ------------------------------------------------------------------
static int groupOf (const char* id)
{
    if (! std::strncmp (id, "m_", 2)) return 1;
    if (! std::strncmp (id, "s_", 2)) return 2;
    if (! std::strncmp (id, "mem_", 4)) return 4;
    if (! std::strncmp (id, "st_", 3)) return 8;
    if (! std::strncmp (id, "e_", 2)) return 16;
    if (! std::strncmp (id, "l", 1) || ! std::strncmp (id, "r", 1) || ! std::strncmp (id, "f", 1) || ! std::strncmp (id, "v", 1) || ! std::strncmp (id, "e", 1)) return 32;
    return 64;   // global
}

void mutatePatch (uint32_t seed, float amount, int lockMask, Params& p)
{
    Rng r; r.seed (seed * 977u + 11u);
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        const PSpec& s = paramSpec (i);
        const std::string id (s.id);
        // never: levels, mute/solo, the drone switch, quality, seeds, history, macros, the root, panic-adjacent
        if (s.flags & F_NS) continue;
        if (id.find ("_gain") != std::string::npos || id.find ("_mute") != std::string::npos || id.find ("_solo") != std::string::npos) continue;
        if (id.find ("_on") != std::string::npos || id == "drone_root" || id == "drone_chord" || id.rfind ("mac_", 0) == 0 || id.rfind ("h_", 0) == 0) continue;
        if (id == "e_fb_ret" || id == "e_fb_send" || id.find ("_loop") != std::string::npos) continue;   // feedback stays where the designer put it
        if (groupOf (s.id) & lockMask) continue;
        if (paramStepped (s))
        {
            if (r.uni() < amount * 0.15f) p.v[i] = (float) clampi ((int) std::lround (p.v[i]) + (r.uni() < 0.5f ? -1 : 1), 0, (int) paramMax (s));
        }
        else if (r.uni() < 0.6f)
            p.v[i] = clampf (p.v[i] + r.gauss() * 0.18f * amount, 0.0f, 1.0f);
    }
}

// ---- the bank ------------------------------------------------------------------
/*  Values are NORMALISED. Reminders: Hz/ms/s parameters are log-mapped, so
    0.5 is the geometric middle of the range. LIST parameters take the index.
    Every patch starts from the defaults and the default macro maps. */

static const PresetDef BANK[] =
{
{ "THE MACHINES KEPT WORKING", "EVOLVING WORLD",
  "A steady industrial process that gradually organises itself. Hold the root; raise LIFE and watch STRUCTURE begin to answer MASS.",
  "h_on=1 outtrim=0.5715 drone_root=33 drone_chord=2 m_o1wave=2 m_o2wave=3 m_o2pw=0.3 m_o2oct=2 m_o2semi=0.5 m_o2fine=0.55 m_beat=0.45 m_sub=1 m_sublvl=0.6 m_reinf=0.3\n"
  "m_fmode=0 m_cut=0.42 m_res=0.35 m_fdrive=0.5 m_fenv=0.55 m_a_atk=0.55 m_a_rel=0.7 m_drift=0.2 m_drifttime=0.5 m_send=0.2 m_loop=0.1\n"
  "s_on=1 s_gain=0.32 s_wt1tab=1 s_wt1pos=0.3 s_wt1oct=1 s_wt2tab=4 s_wt2pos=0.2 s_wt2lvl=0.4 s_wt2oct=2 s_pmidx=0.12 s_fmode=1 s_cut=0.5 s_res=0.3 s_a_atk=0.6 s_a_rel=0.7 s_send=0.25\n"
  "st_on=1 st_gain=0.42 st_model=2 st_exc=3 st_exclvl=0.5 st_damp=0.45 st_dens=0.5 st_material=0.7 st_couple=0.2 st_pitch=0.6 st_send=0.4\n"
  "e_a1=1 e_sat_drive=0.25 e_a2=3 e_mb_mid=0.2 e_mb_high=0.15 e_rv_mix=0.3 e_rv_decay=0.6 e_rv_size=0.6 e_scale=0.55 e_fb_send=0.08 e_fb_ret=0.45 e_fb_delay=0.72 e_fb_to=1 e_fb_damp=0.5\n"
  "l1_wave=5 l1_rate=0.3 l2_wave=0 l2_rate=0.22 r1_type=4 r1_rate=0.55 v1_prob=0.25 v1_rate=0.45 v1_target=0 v1_refract=0.55 l_coupling=0.45 l_autonomy=0.35\n"
  "#SLOT LFO1 m_cut 0.12\n#SLOT LFO2 s_wt1pos 0.25\n#SLOT RND1 st_exclvl 0.3\n#SLOT NE_STRUCT mem_dens 0.2\n"
  "#MACRO HUMANITY mem_on:1 mem_gain:0.25 s_addfund:0.3 st_stress:-0.3 m_res:-0.15\n"
  "#SCENE 1 l_coupling=0.2 st_couple=0.05 e_fb_send=0.0\n#SCENE 2 l_coupling=0.5 st_couple=0.3 e_fb_send=0.12 v1_prob=0.4\n#SCENE 3 st_stress=0.55 e_mb_mid=0.5 e_fb_send=0.25 m_drift=0.5\n#SCENE 4 st_stress=0.2 l_coupling=0.7 e_fb_send=0.15 mem_on=1 mem_gain=0.3 m_cut=0.3\n" },

{ "A CITY WITHOUT WITNESSES", "RESTRAINED BED",
  "Distant structures, air, rare resonant movement. Nothing here wants attention; leave it running under a scene.",
  "outtrim=0.8924 drone_root=36 drone_chord=9 drone_spread=0.7 m_o1wave=1 m_o2wave=0 m_o2oct=3 m_o2lvl=0.35 m_o2fine=0.52 m_sub=1 m_sublvl=0.35 m_gain=0.42 m_width=0.9\n"
  "m_fmode=1 m_cut=0.36 m_res=0.1 m_a_atk=0.8 m_a_rel=0.85 m_drift=0.25 m_drifttime=0.7 m_send=0.6\n"
  "st_on=1 st_gain=0.3 st_model=3 st_exc=1 st_exclvl=0.15 st_sustain=0.2 st_damp=0.25 st_dens=0.8 st_material=0.4 st_pitch=0.62 st_send=0.7 st_width=1.0\n"
  "mem_on=1 mem_gain=0.22 mem_src=1 mem_mode=1 mem_pitch=0.35 mem_smear=0.6 mem_thin=0.4 mem_evolve=0.6 mem_erosion=0.5 mem_send=0.8 mem_lp=0.55\n"
  "e_rv_mix=0.55 e_rv_decay=0.8 e_rv_size=0.85 e_rv_damp=0.55 e_rv_early=0.3 e_scale=0.85 e_distance=0.55 e_rv_mod=0.3\n"
  "v1_prob=0.12 v1_rate=0.25 v1_target=0 v1_refract=0.8 v1_amt=0.4 l1_wave=5 l1_rate=0.18 l_coupling=0.25\n"
  "#SLOT LFO1 st_pitch 0.04\n#SLOT LFO1 e_scale 0.1\n"
  "#MACRO HUMANITY mem_erosion:-0.5 mem_gain:0.25 mem_thin:-0.4 m_cut:0.1\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },

{ "THE LAST HUMAN FREQUENCY", "TRANSITIONAL TEXTURE",
  "A fragile formant survives inside hostile spectra. HUMANITY pulls the voice forward; VIOLENCE lets the machines have it.",
  "outtrim=0.9375 drone_root=45 drone_chord=0 m_on=1 m_gain=0.3 m_o1wave=2 m_o2wave=3 m_o2pw=0.6 m_fmode=2 m_cut=0.55 m_res=0.5 m_a_atk=0.5\n"
  "s_on=1 s_gain=0.4 s_wt1tab=2 s_wt1pos=0.45 s_wt2tab=6 s_wt2lvl=0.35 s_wt2oct=2 s_wt2fine=0.56 s_ring=0.15 s_interf=0.2 s_shift=0.52 s_fmode=2 s_cut=0.62 s_res=0.4\n"
  "mem_on=1 mem_gain=0.5 mem_src=0 mem_mode=2 mem_dur=0.62 mem_dens=0.55 mem_jit=0.3 mem_scan=0.55 mem_region=0.7 mem_erosion=0.35 mem_ero_bw=0.7 mem_ero_drop=0.4 mem_send=0.4\n"
  "e_a1=3 e_mb_low=0.05 e_mb_mid=0.55 e_mb_high=0.5 e_a2=4 e_sh_hz=0.53 e_sh_mix=0.3 e_rv_mix=0.35 e_rv_decay=0.55 e_scale=0.5\n"
  "l1_wave=5 l1_rate=0.3 r1_type=0 r1_rate=0.35 l_coupling=0.4\n"
  "#SLOT LFO1 mem_pos 0.15\n#SLOT RND1 s_shift 0.03\n#SLOT NE_SIGNAL mem_erosion 0.35\n"
  "#MACRO HUMANITY mem_gain:0.3 mem_erosion:-0.35 s_interf:-0.2 e_mb_mid:-0.4 e_sh_mix:-0.3 s_wt1pos:-0.3\n"
  "#MACRO VIOLENCE e_mb_mid:0.45 e_mb_high:0.5 s_interf:0.4 mem_erosion:0.4 e_sh_mix:0.3\n" },

{ "BENEATH THIRTY KILOMETRES OF CONCRETE", "DRONE",
  "Coherent sub pressure with immense upper resonances. The sub is the anchor; everything else is the weight above it.",
  "outtrim=0.5506 drone_root=24 drone_chord=1 m_o1wave=2 m_o1oct=2 m_o2wave=1 m_o2oct=3 m_o2lvl=0.4 m_o2fine=0.51 m_sub=2 m_sublvl=0.85 m_reinf=0.6 m_gain=0.55\n"
  "m_fmode=0 m_cut=0.3 m_res=0.25 m_fdrive=0.6 m_a_atk=0.7 m_a_rel=0.8 m_drift=0.1 m_send=0.15\n"
  "st_on=1 st_gain=0.37 st_model=0 st_exc=3 st_exclvl=0.6 st_damp=0.35 st_dens=0.9 st_material=0.6 st_pitch=0.68 st_stiff=0.3 st_couple=0.15 st_send=0.5 st_hp=0.35\n"
  "e_a1=3 e_mb_low=0.1 e_mb_mid=0.35 e_mb_high=0.3 e_mb_lo=0.35 e_rv_mix=0.3 e_rv_decay=0.7 e_rv_size=0.9 e_scale=0.9 e_rv_low=0.6 bassmono=0.4\n"
  "l1_wave=0 l1_rate=0.15 l_coupling=0.3\n"
  "#SLOT LFO1 st_pitch 0.03\n#SLOT LFO1 m_cut 0.06\n"
  "#MACRO MASS m_sublvl:0.15 m_reinf:0.4 m_gain:0.1 st_exclvl:0.3 e_rv_low:0.3\n"
  "#MACRO HUMANITY s_on:1 s_gain:0.3 s_addlvl:0.6 s_addfund:0.3 s_addtilt:0.3\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "NO ONE ANSWERS THE BROADCAST", "EVOLVING WORLD",
  "Repeated synthetic messages degrade into fragments. Run HISTORY over ten minutes and let EROSION take the signal apart.",
  "h_on=1 outtrim=0.9375 drone_root=48 m_on=0 mem_on=1 mem_gain=0.55 mem_src=1 mem_mode=0 mem_dur=0.55 mem_dens=0.5 mem_sched=0 mem_win=5 mem_scan=0.53 mem_region=0.6 mem_scatter=0.3 mem_keyfollow=0\n"
  "mem_erosion=0.1 mem_ero_bw=0.6 mem_ero_drop=0.7 mem_ero_frag=0.8 mem_ero_simp=0.4 mem_send=0.5\n"
  "s_on=1 s_gain=0.22 s_wt1tab=6 s_wt1pos=0.2 s_wt1oct=3 s_wt2lvl=0 s_fmode=2 s_cut=0.7 s_res=0.2 s_a_atk=0.7 s_a_rel=0.75\n"
  "e_a1=5 e_dl_time=0.78 e_dl_fb=0.5 e_dl_mode=0 e_dl_mod=0.25 e_dl_tone=0.35 e_dl_mix=0.3 e_a2=7 e_cr_bits=0.35 e_cr_rate=0.7 e_cr_mix=0.5 e_rv_mix=0.3 e_rv_decay=0.6 e_scale=0.6 e_distance=0.3\n"
  "r1_type=3 r1_rate=0.4 r1_amt=0.6 v1_prob=0.3 v1_rate=0.4 v1_target=1 v1_refract=0.5 l_coupling=0.35 l_recovery=0.3\n"
  "#SLOT RND1 mem_pos 0.2\n#SLOT HIST mem_erosion 0.8\n#SLOT HIST e_cr_bits 0.4\n#SLOT EROS e_dl_fb 0.25\n"
  "#MACRO HUMANITY mem_erosion:-0.6 e_cr_mix:-0.5 e_dl_mix:-0.2 mem_ero_drop:-0.4\n"
  "#SCENE 1 mem_erosion=0.05\n#SCENE 2 mem_erosion=0.3 e_dl_fb=0.6\n#SCENE 3 mem_erosion=0.7 e_cr_bits=0.6 e_dl_fb=0.75\n#SCENE 4 mem_erosion=0.9 mem_dens=0.3 e_dl_mix=0.5 s_gain=0.1\n" },

{ "AN INTELLIGENCE IN THE WALLS", "INDUSTRIAL PULSE",
  "Localised pulses develop coordinated responses. The event lanes are the intelligence; COUPLING is how much it listens.",
  "h_on=1 drone_root=38 drone_chord=8 m_on=1 m_gain=0.35 m_o1wave=3 m_o1pw=0.7 m_o2wave=2 m_o2oct=1 m_o2lvl=0.5 m_fmode=0 m_cut=0.38 m_res=0.45 m_a_atk=0.35 m_a_rel=0.6\n"
  "s_on=1 s_gain=0.42 s_wt1tab=4 s_wt1pos=0.6 s_wt2tab=7 s_wt2pos=0.4 s_wt2lvl=0.5 s_wt2oct=3 s_pmratio=4 s_pmidx=0.25 s_fmode=1 s_cut=0.58 s_res=0.5 s_a_atk=0.2 s_a_rel=0.55 s_send=0.3\n"
  "st_on=1 st_gain=0.4 st_model=5 st_exc=4 st_exclvl=0.5 st_damp=0.5 st_material=0.6 st_pitch=0.55 st_couple=0.3 st_send=0.35\n"
  "e_a1=1 e_sat_drive=0.35 e_sat_asym=0.65 e_a2=6 e_cb_pitch=0.45 e_cb_fb=0.6 e_cb_mix=0.3 e_rv_mix=0.25 e_rv_decay=0.45 e_scale=0.4 e_fb_send=0.1 e_fb_ret=0.5 e_fb_delay=0.55 e_fb_to=3 e_fb_shift=0.52\n"
  "l1_wave=3 l1_rate=0.62 l2_wave=4 l2_rate=0.58 r1_type=4 r1_rate=0.6 r1_amt=0.7 v1_prob=0.5 v1_rate=0.6 v1_target=5 v1_refract=0.45 v2_prob=0.35 v2_rate=0.5 v2_target=0 v2_refract=0.5 l_coupling=0.6 l_autonomy=0.4 l_hyst=0.4\n"
  "#SLOT LFO1 s_gain 0.35\n#SLOT LFO2 s_wt1pos 0.3\n#SLOT RND1 s_pmidx 0.3\n#SLOT NT_MASS st_exclvl 0.4\n#SLOT NE_SIGNAL m_cut -0.2\n"
  "#SCENE 1 v1_prob=0.15 l_coupling=0.3\n#SCENE 2 v1_prob=0.5 l_coupling=0.6\n#SCENE 3 v1_prob=0.8 v2_prob=0.6 s_pmidx=0.5 e_fb_send=0.25\n#SCENE 4 v1_prob=0.4 l_coupling=0.85 st_couple=0.6\n" },

{ "THE SEA IS MADE OF IRON", "DRONE",
  "Broad metallic motion with a slow physical undertow. A bowed plate over a sub; the bow speed breathes.",
  "outtrim=0.4175 drone_root=31 drone_chord=2 m_on=1 m_gain=0.42 m_o1wave=0 m_o2wave=1 m_o2oct=1 m_o2lvl=0.5 m_sub=1 m_sublvl=0.6 m_reinf=0.3 m_fmode=1 m_cut=0.3 m_a_atk=0.75 m_a_rel=0.8\n"
  "st_on=1 st_gain=0.47 st_model=0 st_exc=2 st_exclvl=0.6 st_sustain=0.7 st_bowforce=0.55 st_bowvel=0.4 st_damp=0.45 st_dens=0.8 st_material=0.65 st_stiff=0.25 st_pitch=0.58 st_couple=0.25 st_send=0.6 st_loop=0.2 st_width=0.9\n"
  "e_a1=1 e_sat_drive=0.3 e_rv_mix=0.4 e_rv_decay=0.7 e_rv_size=0.8 e_scale=0.75 e_rv_low=0.55 e_fb_send=0.0 e_fb_ret=0.4 e_fb_delay=0.8 e_fb_to=0 e_fb_shift=0.49\n"
  "l1_wave=0 l1_rate=0.22 l2_wave=5 l2_rate=0.28 l_coupling=0.4\n"
  "#SLOT LFO1 st_bowvel 0.3\n#SLOT LFO2 st_bowforce 0.2\n#SLOT LFO1 st_expos 0.2\n"
  "#MACRO VIOLENCE st_stress:0.6 st_bowforce:0.4 e_sat_drive:0.4 st_drive:0.5\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "WE WERE PROMISED A FUTURE", "TRANSITIONAL TEXTURE",
  "A mournful chord becomes mechanically constrained. Play it as a pad; the scenes take it into the machine.",
  "h_on=1 outtrim=0.8394 drone_root=45 drone_chord=6 drone_spread=0.6 m_o1wave=2 m_o2wave=2 m_o2fine=0.56 m_unison=2 m_unidet=0.25 m_sub=1 m_sublvl=0.3 m_gain=0.45 m_width=0.85\n"
  "m_fmode=0 m_cut=0.5 m_res=0.2 m_fenv=0.6 m_f_atk=0.7 m_f_dec=0.7 m_f_sus=0.4 m_a_atk=0.65 m_a_rel=0.78 m_drift=0.15 m_send=0.5\n"
  "s_on=1 s_gain=0.25 s_wt1tab=3 s_wt1pos=0.3 s_wt1oct=3 s_fmode=2 s_cut=0.55 s_a_atk=0.75 s_a_rel=0.8 s_send=0.5\n"
  "e_a1=3 e_mb_mid=0.1 e_mb_high=0.1 e_a2=7 e_cr_bits=0.1 e_cr_mix=0.0 e_rv_mix=0.4 e_rv_decay=0.65 e_scale=0.6 e_rv_early=0.3\n"
  "l1_wave=0 l1_rate=0.26 l_coupling=0.3\n"
  "#SLOT LFO1 m_cut 0.08\n#SLOT HIST e_cr_mix 0.7\n#SLOT HIST e_mb_mid 0.5\n#SLOT HIST m_pwdrift 0.4\n"
  "#SCENE 1 e_cr_mix=0 e_mb_mid=0.1\n#SCENE 2 e_mb_mid=0.3 m_res=0.35\n#SCENE 3 e_cr_mix=0.6 e_cr_bits=0.4 e_mb_mid=0.55 m_cut=0.42\n#SCENE 4 e_cr_mix=0.4 e_cr_bits=0.6 m_cut=0.34 s_gain=0.1 e_rv_mix=0.55\n" },

{ "POPULATION: ZERO", "RESTRAINED BED",
  "Almost silence, distance, and rare signs of operation. Turn it up; the quiet is the point.",
  "outtrim=0.9119 drone_root=36 drone_chord=2 m_o1wave=0 m_o2wave=0 m_o2oct=3 m_o2lvl=0.2 m_o2fine=0.505 m_sub=1 m_sublvl=0.3 m_gain=0.42 m_width=0.7 m_fmode=1 m_cut=0.3 m_a_atk=0.85 m_a_rel=0.9 m_send=0.7\n"
  "st_on=1 st_gain=0.25 st_model=4 st_exc=0 st_damp=0.15 st_dens=0.6 st_material=0.85 st_pitch=0.7 st_strikelvl=0.4 st_send=0.9\n"
  "e_rv_mix=0.6 e_rv_decay=0.85 e_rv_size=0.9 e_rv_damp=0.6 e_scale=0.95 e_distance=0.75 e_rv_early=0.2\n"
  "v1_prob=0.08 v1_rate=0.3 v1_target=0 v1_refract=0.85 v1_amt=0.3 l_coupling=0.15\n"
  "#MACRO VIOLENCE st_strikelvl:0.4 v1_prob:0.4 e_sat_drive:0.2\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },

{ "SOMETHING LEARNED TO BREATHE", "INDUSTRIAL PULSE",
  "Asymmetric, adapting pulses emerge from noise. The shape envelope is the breath; the network decides how deep.",
  "outtrim=0.8192 drone_root=40 drone_chord=0 m_on=1 m_gain=0.3 m_o1wave=3 m_o1pw=0.5 m_o2wave=2 m_o2oct=1 m_o2lvl=0.3 m_sub=1 m_sublvl=0.4 m_fmode=0 m_cut=0.35 m_res=0.5 m_fdrive=0.4 m_a_atk=0.3 m_a_rel=0.55\n"
  "s_on=1 s_gain=0.35 s_wt1tab=7 s_wt1pos=0.5 s_wt2tab=1 s_wt2pos=0.7 s_wt2lvl=0.4 s_fmode=2 s_cut=0.45 s_res=0.55 s_a_atk=0.2 s_a_rel=0.5\n"
  "st_on=1 st_gain=0.3 st_model=3 st_exc=1 st_exclvl=0.4 st_sustain=0.3 st_damp=0.5 st_dens=0.4 st_send=0.5\n"
  "e_a1=7 e_cr_bits=0.3 e_cr_rate=0.55 e_cr_mix=0.35 e_a2=1 e_sat_drive=0.3 e_rv_mix=0.3 e_rv_decay=0.5 e_scale=0.45\n"
  "r1_type=4 r1_rate=0.5 r1_amt=0.6 r2_type=5 r2_rate=0.4 r2_amt=0.5 l_coupling=0.55 l_autonomy=0.45 l_recovery=0.4\n"
  "#SHAPE 1 1 1 0.0 0.0 0.0 0.35 1.0 0.6 0.9 0.35 -0.5 2.6 0.0 0.3\n"
  "#SLOT SHAPE1 m_gain 0.4\n#SLOT SHAPE1 s_cut 0.3\n#SLOT SHAPE1 st_exclvl 0.5\n#SLOT RND2 s_wt1pos 0.3\n#SLOT NE_MIX e_cr_bits 0.3\n" },

{ "ORBITAL DEBRIS CHOIR", "DRONE",
  "Slowly moving inharmonic tones over a restrained tonal anchor. The fundamental holds while the rest drifts out of tune.",
  "outtrim=0.8826 drone_root=43 drone_chord=2 m_on=1 m_gain=0.3 m_o1wave=0 m_o2wave=0 m_o2oct=1 m_o2lvl=0.5 m_sub=1 m_sublvl=0.3 m_fmode=1 m_cut=0.35 m_a_atk=0.8 m_a_rel=0.85\n"
  "s_on=1 s_gain=0.45 s_wt1lvl=0 s_wt2lvl=0 s_addlvl=0.9 s_addn=40 s_addspread=0.62 s_addtilt=0.6 s_addmotion=0.35 s_addfund=1.0 s_addgaps=0.2 s_fmode=0 s_a_atk=0.8 s_a_rel=0.85 s_width=0.9 s_send=0.6\n"
  "mem_on=1 mem_gain=0.25 mem_src=2 mem_mode=1 mem_freeze=0 mem_smear=0.7 mem_evolve=0.4 mem_pitch=0.5 mem_send=0.7\n"
  "e_rv_mix=0.45 e_rv_decay=0.75 e_rv_size=0.85 e_scale=0.8 e_rv_mod=0.35 e_distance=0.35\n"
  "l1_wave=0 l1_rate=0.2 l2_wave=5 l2_rate=0.25 l_coupling=0.3\n"
  "#SLOT LFO1 s_addspread 0.08\n#SLOT LFO2 s_addtilt 0.15\n#SLOT LFO2 mem_stilt 0.2\n"
  "#MACRO INSTABILITY s_addspread:0.3 s_addmotion:0.5 m_drift:0.4 s_addfund:-0.4\n#MACRO HUMANITY s_addfund:0.2 s_addspread:-0.3 mem_gain:0.3 mem_smear:-0.3\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "THE FACTORY DREAMED OF FLESH", "EVOLVING WORLD",
  "Industrial resonances acquire vulnerable formants. HUMANITY is the dream; at zero it is only the factory.",
  "h_on=1 outtrim=0.5405 drone_root=33 drone_chord=2 m_on=1 m_gain=0.35 m_o1wave=2 m_o2wave=3 m_o2pw=0.4 m_o2oct=2 m_sub=1 m_sublvl=0.5 m_fmode=0 m_cut=0.36 m_res=0.35 m_fdrive=0.5 m_a_atk=0.5 m_a_rel=0.65\n"
  "s_on=1 s_gain=0.0 s_wt1tab=2 s_wt1pos=0.35 s_wt1oct=3 s_fmode=2 s_cut=0.6 s_res=0.5 s_a_atk=0.65 s_a_rel=0.75 s_send=0.4\n"
  "mem_on=1 mem_gain=0.3 mem_src=3 mem_mode=0 mem_dur=0.5 mem_dens=0.55 mem_sched=0 mem_keyfollow=1 mem_pitch=0.45 mem_send=0.2\n"
  "st_on=1 st_gain=0.45 st_model=2 st_exc=5 st_exclvl=0.6 st_damp=0.4 st_dens=0.6 st_material=0.75 st_pitch=0.55 st_couple=0.2 st_send=0.4\n"
  "e_a1=1 e_sat_drive=0.4 e_sat_asym=0.6 e_a2=3 e_mb_mid=0.3 e_mb_high=0.25 e_rv_mix=0.3 e_rv_decay=0.55 e_scale=0.5\n"
  "l1_wave=5 l1_rate=0.28 v1_prob=0.3 v1_rate=0.5 v1_target=0 v1_refract=0.5 l_coupling=0.45\n"
  "#SLOT LFO1 s_wt1pos 0.3\n#SLOT LFO1 mem_pos 0.2\n"
  "#MACRO HUMANITY s_gain:0.45 s_wt1pos:0.2 e_sat_drive:-0.25 e_mb_mid:-0.2 mem_erosion:-0.3 st_material:-0.3\n"
  "#SCENE 1 s_gain=0.0\n#SCENE 2 s_gain=0.2 st_couple=0.3\n#SCENE 3 s_gain=0.35 e_mb_mid=0.5 st_stress=0.4\n#SCENE 4 s_gain=0.45 e_sat_drive=0.15 st_stress=0.1 m_cut=0.3\n" },

{ "AFTER THE FINAL TRANSMISSION", "RESONANT GESTURE",
  "A short source leaves a long, evolving residue. Strike once (a key, or STRIKE) and listen to the loop take it.",
  "outtrim=0.8571 drone_root=48 m_on=0 s_on=0 st_on=1 st_gain=0.5 st_model=4 st_exc=0 st_damp=0.35 st_dens=0.7 st_material=0.7 st_pitch=0.6 st_strikelvl=0.9 st_rel=0.85 st_loop=0.6 st_send=0.3\n"
  "mem_on=1 mem_gain=0.25 mem_src=3 mem_mode=0 mem_dur=0.6 mem_dens=0.3 mem_loop=0.5 mem_capsrc=0 mem_scan=0.5 mem_rel=0.9\n"
  "e_fb_send=0.2 e_fb_ret=0.85 e_fb_delay=0.82 e_fb_shift=0.515 e_fb_sat=0.4 e_fb_damp=0.45 e_fb_to=2 e_fb_lp=0.65\n"
  "e_a1=4 e_sh_hz=0.503 e_sh_mix=0.25 e_rv_mix=0.5 e_rv_decay=0.8 e_rv_size=0.8 e_scale=0.7 e_rv_early=0.3\n"
  "l_coupling=0.3\n"
  "#MACRO CONTAMINATION e_fb_ret:0.15 e_fb_shift:0.02 e_fb_sat:0.4 st_couple:0.4\n" },

{ "THE SUN BEHIND THE ASH", "RESTRAINED BED",
  "Softness and obscured harmonic light. HUMANITY lifts the ash a little; nothing here is loud.",
  "outtrim=0.9159 drone_root=48 drone_chord=4 drone_spread=0.5 m_o1wave=1 m_o2wave=0 m_o2oct=3 m_o2lvl=0.4 m_o2fine=0.515 m_sub=1 m_sublvl=0.3 m_gain=0.42 m_width=0.8\n"
  "m_fmode=1 m_cut=0.33 m_res=0.05 m_a_atk=0.85 m_a_rel=0.9 m_drift=0.2 m_drifttime=0.8 m_send=0.6\n"
  "s_on=1 s_gain=0.28 s_wt1tab=3 s_wt1pos=0.2 s_wt1oct=2 s_wt2tab=0 s_wt2pos=0.05 s_wt2lvl=0.4 s_wt2oct=3 s_fmode=2 s_cut=0.4 s_a_atk=0.85 s_a_rel=0.9 s_send=0.6\n"
  "e_rv_mix=0.5 e_rv_decay=0.75 e_rv_size=0.75 e_rv_damp=0.6 e_scale=0.7 e_distance=0.4 e_rv_mod=0.25\n"
  "l1_wave=0 l1_rate=0.2 l_coupling=0.2\n"
  "#SLOT LFO1 s_wt1pos 0.2\n#SLOT LFO1 m_cut 0.05\n"
  "#MACRO HUMANITY m_cut:0.2 s_cut:0.25 e_distance:-0.3 s_wt2lvl:0.3 m_res:-0.05\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },

{ "A WEAPON WAITING FOR A WAR", "INDUSTRIAL PULSE",
  "Contained energy, repetition, an abrupt controlled release. LFO 1 gates the pulse; VIOLENCE is the trigger.",
  "outtrim=0.6582 drone_root=31 drone_chord=0 m_on=1 m_gain=0.45 m_o1wave=3 m_o1pw=0.2 m_o2wave=2 m_o2oct=1 m_o2lvl=0.6 m_sub=1 m_sublvl=0.5 m_fmode=0 m_cut=0.33 m_res=0.55 m_fdrive=0.7 m_fenv=0.7 m_f_atk=0.1 m_f_dec=0.45 m_f_sus=0.2 m_a_atk=0.1 m_a_rel=0.5\n"
  "st_on=1 st_gain=0.35 st_model=1 st_exc=3 st_exclvl=0.5 st_damp=0.6 st_material=0.8 st_pitch=0.6 st_send=0.2\n"
  "e_a1=3 e_mb_low=0.15 e_mb_mid=0.35 e_mb_high=0.3 e_a2=2 e_fold_amt=0.0 e_fold_mix=1 e_rv_mix=0.2 e_rv_decay=0.4 e_scale=0.35 e_distance=0.1\n"
  "l1_wave=3 l1_rate=0.6 l1_sync=6 l2_wave=1 l2_rate=0.35 l_coupling=0.3\n"
  "#SLOT LFO1 m_gain 0.5\n#SLOT LFO1 st_exclvl 0.5\n#SLOT LFO2 m_cut 0.1\n"
  "#MACRO VIOLENCE e_fold_amt:0.7 e_mb_mid:0.5 e_mb_high:0.5 st_stress:0.6 m_cut:0.2 st_exclvl:0.3\n" },

{ "THIRTY THOUSAND YEARS", "EVOLVING WORLD",
  "The flagship journey: order, occupation, collapse, altered life. Switch HISTORY on, set it to thirty minutes, and leave.",
  "h_on=1 outtrim=0.5840 drone_root=33 drone_chord=6 drone_spread=0.55 m_o1wave=2 m_o2wave=3 m_o2pw=0.3 m_o2fine=0.54 m_beat=0.35 m_unison=1 m_unidet=0.2 m_sub=1 m_sublvl=0.55 m_reinf=0.3 m_gain=0.45 m_width=0.8\n"
  "m_fmode=0 m_cut=0.42 m_res=0.3 m_fdrive=0.4 m_a_atk=0.7 m_a_rel=0.8 m_drift=0.2 m_drifttime=0.6 m_send=0.4 m_loop=0.05\n"
  "s_on=1 s_gain=0.3 s_wt1tab=3 s_wt1pos=0.25 s_wt1oct=2 s_wt2tab=1 s_wt2pos=0.3 s_wt2lvl=0.3 s_wt2oct=3 s_pmidx=0.05 s_addlvl=0.3 s_addspread=0.55 s_addfund=1 s_fmode=2 s_cut=0.55 s_res=0.3 s_a_atk=0.75 s_a_rel=0.8 s_send=0.45\n"
  "mem_on=1 mem_gain=0.3 mem_src=0 mem_mode=2 mem_dur=0.6 mem_dens=0.5 mem_sched=1 mem_smear=0.4 mem_erosion=0.15 mem_send=0.6 mem_lp=0.7\n"
  "st_on=1 st_gain=0.4 st_model=0 st_exc=3 st_exclvl=0.45 st_damp=0.35 st_dens=0.7 st_material=0.65 st_pitch=0.6 st_couple=0.15 st_send=0.5\n"
  "e_a1=1 e_sat_drive=0.25 e_a2=3 e_mb_mid=0.15 e_mb_high=0.15 e_b1=4 e_sh_hz=0.51 e_sh_mix=0.2 e_rv_mix=0.4 e_rv_decay=0.7 e_rv_size=0.8 e_scale=0.7 e_distance=0.3 e_fb_send=0.06 e_fb_ret=0.5 e_fb_delay=0.75 e_fb_to=1 e_fb_damp=0.5\n"
  "l1_wave=0 l1_rate=0.2 l2_wave=5 l2_rate=0.26 r1_type=1 r1_rate=0.3 r1_amt=0.5 v1_prob=0.2 v1_rate=0.4 v1_target=0 v1_refract=0.6 v2_prob=0.15 v2_rate=0.35 v2_target=1 v2_refract=0.6 l_coupling=0.45 l_autonomy=0.4 l_recovery=0.5 h_dur=0.9\n"
  "#SLOT LFO1 m_cut 0.08\n#SLOT LFO2 s_wt1pos 0.3\n#SLOT RND1 mem_pos 0.3\n#SLOT NE_STRUCT mem_dens 0.25\n#SLOT EROS st_exclvl -0.3\n"
  "#SCENE 1 mem_erosion=0.05 st_stress=0.0 e_mb_mid=0.1 l_coupling=0.3 s_gain=0.2 e_fb_send=0.0\n"
  "#SCENE 2 mem_erosion=0.25 st_couple=0.35 e_mb_mid=0.3 l_coupling=0.55 s_pmidx=0.2 e_fb_send=0.1 v1_prob=0.4\n"
  "#SCENE 3 mem_erosion=0.7 st_stress=0.6 e_mb_mid=0.6 e_mb_high=0.5 e_sh_mix=0.5 s_interf=0.5 m_drift=0.6 e_fb_send=0.3 l_coupling=0.7 mem_dens=0.7\n"
  "#SCENE 4 mem_erosion=0.5 st_stress=0.1 e_mb_mid=0.15 m_cut=0.3 s_gain=0.15 mem_gain=0.4 mem_smear=0.8 e_rv_mix=0.6 e_distance=0.6 l_coupling=0.8 e_fb_send=0.12\n" },

// ---- the bank beyond the heroes ---------------------------------------------
{ "CONCRETE BASS", "PLAYABLE BASS",
  "A dry, playable sub bass with a ladder growl. Mono, legato, no space.",
  "outtrim=0.8123 vmode=2 glide=0.35 m_o1wave=2 m_o2wave=3 m_o2pw=0.4 m_o2fine=0.52 m_sub=1 m_sublvl=0.7 m_reinf=0.35 m_gain=0.55 m_width=0.2\n"
  "m_fmode=0 m_cut=0.34 m_res=0.35 m_fdrive=0.6 m_fenv=0.68 m_f_atk=0.05 m_f_dec=0.5 m_f_sus=0.25 m_a_atk=0.05 m_a_dec=0.5 m_a_sus=0.9 m_a_rel=0.4 m_send=0.0 m_drift=0.05\n"
  "e_a1=1 e_sat_drive=0.3 e_rv_mix=0.0 e_distance=0.0 bassmono=0.5\n" },

{ "IRON BASS", "PLAYABLE BASS",
  "A bass whose body is a struck cable. Velocity is the hammer.",
  "outtrim=0.7805 vmode=1 m_o1wave=2 m_o2wave=2 m_o2oct=1 m_o2lvl=0.4 m_sub=1 m_sublvl=0.6 m_gain=0.5 m_width=0.3 m_fmode=0 m_cut=0.36 m_res=0.3 m_fdrive=0.5 m_fenv=0.6 m_f_dec=0.45 m_f_sus=0.3 m_a_atk=0.02 m_a_rel=0.45 m_send=0.05\n"
  "st_on=1 st_gain=0.42 st_model=1 st_exc=3 st_exclvl=0.5 st_damp=0.7 st_material=0.65 st_stiff=0.15 st_pitch=0.5 st_strikelvl=0.8 st_rel=0.5 st_send=0.15\n"
  "e_a1=3 e_mb_low=0.1 e_mb_mid=0.35 e_mb_high=0.3 e_rv_mix=0.1 e_rv_decay=0.35 e_scale=0.3\n"
  "#SLOT VEL st_strikelvl 0.5\n#SLOT VEL m_cut 0.2\n" },

{ "CABLE BASS", "PLAYABLE BASS",
  "A bowed cable under a sine. Slow attack, long sustain; aftertouch presses the bow.",
  "outtrim=0.5347 vmode=0 voices=4 m_o1wave=0 m_o2wave=1 m_o2oct=1 m_o2lvl=0.4 m_sub=1 m_sublvl=0.6 m_gain=0.45 m_fmode=1 m_cut=0.3 m_a_atk=0.5 m_a_rel=0.6\n"
  "st_on=1 st_gain=0.42 st_model=1 st_exc=2 st_exclvl=0.5 st_sustain=0.6 st_bowforce=0.5 st_bowvel=0.35 st_damp=0.55 st_material=0.55 st_stiff=0.1 st_pitch=0.5 st_atk=0.4 st_rel=0.6 st_send=0.25\n"
  "e_rv_mix=0.2 e_rv_decay=0.45 e_scale=0.4\n"
  "#SLOT PRESS st_bowforce 0.4\n#SLOT PRESS st_bowvel 0.25\n" },

{ "STEEL CATHEDRAL", "DRONE",
  "A plate the size of a building, bowed slowly. Everything is resonance.",
  "outtrim=0.4995 drone_root=36 drone_chord=2 m_on=0 st_on=1 st_gain=0.47 st_model=0 st_exc=2 st_exclvl=0.55 st_sustain=0.65 st_bowforce=0.5 st_bowvel=0.3 st_damp=0.35 st_dens=1.0 st_material=0.65 st_stiff=0.2 st_pitch=0.47 st_couple=0.3 st_send=0.7 st_width=1.0\n"
  "e_rv_mix=0.5 e_rv_decay=0.85 e_rv_size=0.95 e_scale=0.95 e_rv_low=0.6 e_distance=0.3\n"
  "l1_wave=0 l1_rate=0.18 l2_wave=5 l2_rate=0.25\n#SLOT LFO1 st_bowvel 0.25\n#SLOT LFO2 st_expos 0.3\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "HOSTILE COMPUTATION", "INDUSTRIAL PULSE",
  "Stepped tables through PM, clocked events resetting the signal. A machine thinking out loud.",
  "outtrim=0.9075 drone_root=45 drone_chord=8 m_on=0 s_on=1 s_gain=0.5 s_wt1tab=4 s_wt1pos=0.5 s_wt2tab=5 s_wt2pos=0.4 s_wt2lvl=0.5 s_wt2oct=3 s_pmratio=5 s_pmidx=0.35 s_pmfb=0.3 s_ring=0.2 s_fmode=1 s_cut=0.6 s_res=0.5 s_a_atk=0.1 s_a_rel=0.5\n"
  "e_a1=7 e_cr_bits=0.4 e_cr_rate=0.6 e_cr_mix=0.5 e_a2=6 e_cb_pitch=0.55 e_cb_fb=0.7 e_cb_mix=0.35 e_rv_mix=0.2 e_rv_decay=0.4 e_scale=0.35\n"
  "l1_wave=4 l1_rate=0.64 l1_sync=7 l2_wave=3 l2_rate=0.6 l2_sync=6 v1_prob=0.6 v1_rate=0.62 v1_target=5 v1_refract=0.35 r1_type=2 r1_rate=0.62 r1_amt=0.7\n"
  "#SLOT LFO1 s_wt1pos 0.4\n#SLOT LFO2 s_gain 0.3\n#SLOT RND1 s_pmidx 0.35\n#SLOT RND1 e_cb_pitch 0.2\n" },

{ "SIRENS OVER THE DISTRICT", "TRANSITIONAL TEXTURE",
  "Siren tables sweeping through a shifter, far away. DISTANCE brings them over the rooftops.",
  "outtrim=0.9375 drone_root=52 drone_chord=8 m_on=0 s_on=1 s_gain=0.4 s_wt1tab=6 s_wt1pos=0.3 s_wt2tab=6 s_wt2pos=0.6 s_wt2lvl=0.4 s_wt2fine=0.6 s_fmode=2 s_cut=0.7 s_a_atk=0.7 s_a_rel=0.8 s_send=0.7\n"
  "e_a1=4 e_sh_hz=0.55 e_sh_mix=0.4 e_sh_fb=0.3 e_a2=5 e_dl_time=0.8 e_dl_fb=0.55 e_dl_mode=0 e_dl_mix=0.35 e_rv_mix=0.5 e_rv_decay=0.7 e_rv_size=0.85 e_scale=0.85 e_distance=0.6\n"
  "l1_wave=1 l1_rate=0.35 l2_wave=0 l2_rate=0.3\n#SLOT LFO1 s_wt1pos 0.45\n#SLOT LFO2 s_pshift 0.08\n" },

{ "GRANITE PAD", "RESTRAINED BED",
  "A dry, wide chord pad with no space at all. Sits under drums.",
  "outtrim=0.8874 drone_root=45 drone_chord=3 drone_spread=0.8 m_o1wave=2 m_o2wave=2 m_o2fine=0.55 m_unison=2 m_unidet=0.3 m_sub=1 m_sublvl=0.3 m_gain=0.42 m_width=1.0 m_fmode=0 m_cut=0.5 m_res=0.15 m_a_atk=0.6 m_a_rel=0.7 m_send=0 m_hp=0.3\n"
  "e_rv_mix=0.0 e_distance=0.0\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },

{ "THE ROOM IS LISTENING", "RESONANT GESTURE",
  "Every note strikes a glass body and the loop keeps it. Play sparsely.",
  "outtrim=0.6702 vmode=0 m_on=0 st_on=1 st_gain=0.5 st_model=4 st_exc=0 st_damp=0.3 st_dens=0.6 st_material=0.75 st_pitch=0.55 st_strikelvl=0.85 st_rel=0.8 st_loop=0.4 st_send=0.4\n"
  "e_fb_send=0.1 e_fb_ret=0.7 e_fb_delay=0.7 e_fb_shift=0.51 e_fb_sat=0.3 e_fb_damp=0.5 e_fb_to=0 e_rv_mix=0.35 e_rv_decay=0.65 e_scale=0.6\n"
  "#SLOT VEL st_strikelvl 0.4\n" },

{ "COLD START", "DRY",
  "The naked MASS oscillators, no filter movement, no space. A starting point.",
  "outtrim=0.8724 m_o1wave=2 m_o2wave=2 m_o2fine=0.53 m_sub=1 m_sublvl=0.4 m_gain=0.45 m_fmode=0 m_cut=0.75 m_res=0.1 m_a_atk=0.2 m_a_rel=0.45 m_send=0 e_rv_mix=0 e_distance=0\n" },

{ "NAKED SIGNAL", "DRY",
  "Only the SIGNAL tables, clean. For building.",
  "outtrim=0.8434 m_on=0 s_on=1 s_gain=0.45 s_wt1tab=0 s_wt1pos=0.5 s_wt2tab=3 s_wt2pos=0.3 s_wt2lvl=0.3 s_fmode=0 s_a_atk=0.2 s_a_rel=0.45 s_send=0 e_rv_mix=0\n" },

{ "NAKED STRUCTURE", "DRY",
  "A struck plate and nothing else. For learning the body controls.",
  "m_on=0 st_on=1 st_gain=0.5 st_model=0 st_exc=0 st_damp=0.4 st_dens=0.7 st_material=0.6 st_pitch=0.5 st_strikelvl=0.8 st_send=0 e_rv_mix=0\n" },

{ "NAKED MEMORY", "DRY",
  "The voice source through grains, dry. Import your own material here.",
  "outtrim=0.9375 m_on=0 mem_on=1 mem_gain=0.5 mem_src=0 mem_mode=0 mem_dur=0.55 mem_dens=0.5 mem_jit=0.2 mem_scatter=0.4 mem_send=0 e_rv_mix=0\n" },

{ "ABANDONED SUBSTATION", "DRONE",
  "A mains hum with harmonics, a transformer body, and the loop keeping it warm.",
  "outtrim=0.9375 drone_root=35 drone_chord=0 m_o1wave=3 m_o1pw=0.9 m_o2wave=2 m_o2oct=3 m_o2lvl=0.25 m_o2fine=0.52 m_sub=1 m_sublvl=0.6 m_reinf=0.5 m_gain=0.45 m_fmode=0 m_cut=0.38 m_res=0.3 m_fdrive=0.7 m_a_atk=0.6 m_a_rel=0.7 m_loop=0.15 m_send=0.2\n"
  "st_on=1 st_gain=0.35 st_model=2 st_exc=3 st_exclvl=0.5 st_damp=0.55 st_material=0.85 st_pitch=0.65 st_send=0.3\n"
  "e_a1=1 e_sat_drive=0.45 e_sat_asym=0.7 e_fb_send=0.08 e_fb_ret=0.55 e_fb_delay=0.6 e_fb_to=0 e_fb_damp=0.4 e_rv_mix=0.2 e_rv_decay=0.5 e_scale=0.5\n"
  "l1_wave=5 l1_rate=0.3\n#SLOT LFO1 m_o1pw 0.15\n#SLOT LFO1 e_fb_ret 0.1\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "THE ARCHIVE BURNS", "TRANSITIONAL TEXTURE",
  "The choir memory frozen and slowly displaced while the crusher eats it. Ten minutes of HISTORY.",
  "h_on=1 outtrim=0.9375 drone_root=48 m_on=0 mem_on=1 mem_gain=0.5 mem_src=2 mem_mode=1 mem_freeze=1 mem_smear=0.5 mem_disp=0.5 mem_evolve=0.3 mem_thin=0.1 mem_send=0.5 mem_keyfollow=1\n"
  "e_a1=7 e_cr_bits=0.15 e_cr_rate=0.75 e_cr_mix=0.0 e_a2=1 e_sat_drive=0.2 e_rv_mix=0.45 e_rv_decay=0.75 e_scale=0.7 e_distance=0.3\n"
  "#SLOT HIST mem_disp 0.1\n#SLOT HIST e_cr_mix 0.8\n#SLOT HIST e_cr_bits 0.5\n#SLOT HIST mem_thin 0.5\n"
  "#SCENE 1 e_cr_mix=0\n#SCENE 2 e_cr_mix=0.3\n#SCENE 3 e_cr_mix=0.7 e_cr_bits=0.5 mem_thin=0.5\n#SCENE 4 e_cr_mix=0.5 mem_thin=0.8 mem_gain=0.3 e_rv_mix=0.7\n" },

{ "STRESSED PLATE", "RESONANT GESTURE",
  "A plate under rising stress: strike it and it fractures. STRESS on the macro VIOLENCE.",
  "outtrim=0.5021 vmode=0 m_on=0 st_on=1 st_gain=0.47 st_model=0 st_exc=0 st_damp=0.45 st_dens=0.8 st_material=0.55 st_pitch=0.42 st_stress=0.4 st_couple=0.3 st_drive=0.3 st_strikelvl=0.9 st_rel=0.8 st_send=0.3\n"
  "e_rv_mix=0.3 e_rv_decay=0.55 e_scale=0.5\n#MACRO VIOLENCE st_stress:0.55 st_drive:0.5 st_couple:0.4\n" },

{ "BREATH OF THE GRID", "INDUSTRIAL PULSE",
  "A synced square gate over a sub drone and a cavity. Tempo-locked.",
  "outtrim=0.6490 drone_root=36 drone_chord=2 m_o1wave=2 m_o2wave=3 m_o2pw=0.3 m_sub=1 m_sublvl=0.6 m_gain=0.45 m_fmode=0 m_cut=0.36 m_res=0.4 m_fdrive=0.4 m_a_atk=0.1 m_a_rel=0.4\n"
  "st_on=1 st_gain=0.3 st_model=3 st_exc=3 st_exclvl=0.4 st_damp=0.5 st_pitch=0.6 st_send=0.4\n"
  "e_a1=3 e_mb_mid=0.3 e_mb_high=0.2 e_rv_mix=0.25 e_rv_decay=0.4 e_scale=0.4\n"
  "l1_wave=3 l1_sync=6 l2_wave=2 l2_sync=4\n#SLOT LFO1 m_gain 0.45\n#SLOT LFO2 m_cut 0.2\n" },

{ "VOICES UNDER THE ICE", "RESTRAINED BED",
  "The voice source, spectral, frozen and smeared, under a sine. Distant and slow.",
  "outtrim=0.9375 drone_root=43 drone_chord=2 m_o1wave=0 m_o2wave=0 m_o2oct=1 m_o2lvl=0.4 m_sub=0 m_gain=0.3 m_fmode=1 m_cut=0.3 m_a_atk=0.85 m_a_rel=0.9 m_send=0.5\n"
  "mem_on=1 mem_gain=0.4 mem_src=0 mem_mode=1 mem_smear=0.8 mem_evolve=0.5 mem_thin=0.3 mem_stilt=0.4 mem_pitch=0.42 mem_send=0.8 mem_lp=0.6\n"
  "e_rv_mix=0.55 e_rv_decay=0.85 e_rv_size=0.9 e_scale=0.9 e_distance=0.6 e_rv_damp=0.6\n"
  "#MACRO HUMANITY mem_thin:-0.3 mem_smear:-0.4 mem_lp:0.3 e_distance:-0.4\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },

{ "SLOW COLLAPSE", "TRANSITIONAL TEXTURE",
  "Everything present, sinking. HISTORY from a clean chord to rubble in four minutes.",
  "h_on=1 outtrim=0.7799 drone_root=40 drone_chord=3 m_o1wave=2 m_o2wave=2 m_o2fine=0.54 m_sub=1 m_sublvl=0.5 m_gain=0.42 m_fmode=0 m_cut=0.5 m_res=0.2 m_a_atk=0.6 m_a_rel=0.75 m_send=0.4\n"
  "s_on=1 s_gain=0.25 s_wt1tab=3 s_wt1pos=0.2 s_fmode=2 s_cut=0.55 s_a_atk=0.65 s_a_rel=0.75 s_send=0.4\n"
  "st_on=1 st_gain=0.35 st_model=0 st_exc=3 st_exclvl=0.4 st_damp=0.4 st_material=0.7 st_pitch=0.6 st_send=0.4\n"
  "e_a1=3 e_mb_mid=0.1 e_mb_high=0.1 e_a2=4 e_sh_hz=0.5 e_sh_mix=0.0 e_rv_mix=0.4 e_rv_decay=0.65 e_scale=0.6 h_dur=0.65\n"
  "#SCENE 1 m_drift=0.1 s_interf=0 e_sh_mix=0 st_stress=0\n#SCENE 2 m_drift=0.3 s_interf=0.2 e_mb_mid=0.3\n#SCENE 3 m_drift=0.7 s_interf=0.6 e_sh_mix=0.5 e_sh_hz=0.53 st_stress=0.6 e_mb_mid=0.6\n#SCENE 4 m_drift=0.4 s_interf=0.3 e_sh_mix=0.3 st_stress=0.2 m_cut=0.3 e_rv_mix=0.6 e_distance=0.6 s_gain=0.1\n" },

{ "HOLLOW EARTH", "DRONE",
  "Hollow tables in a cavity, low. The kind of note a mountain would hold.",
  "outtrim=0.6748 drone_root=29 drone_chord=1 m_o1wave=1 m_o2wave=0 m_o2oct=1 m_o2lvl=0.5 m_sub=2 m_sublvl=0.7 m_reinf=0.4 m_gain=0.45 m_fmode=1 m_cut=0.3 m_a_atk=0.75 m_a_rel=0.85\n"
  "s_on=1 s_gain=0.3 s_wt1tab=3 s_wt1pos=0.6 s_wt1oct=2 s_fmode=2 s_cut=0.42 s_a_atk=0.75 s_a_rel=0.85 s_send=0.4\n"
  "st_on=1 st_gain=0.3 st_model=3 st_exc=4 st_exclvl=0.4 st_damp=0.25 st_dens=0.7 st_material=0.5 st_pitch=0.5 st_send=0.6\n"
  "e_rv_mix=0.4 e_rv_decay=0.75 e_rv_size=0.9 e_scale=0.9 e_rv_low=0.65 bassmono=0.45\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },

{ "LAST LIGHT IN THE TOWER", "RESTRAINED BED",
  "A single fragile sine tower, glass modes, and a shifter breathing at half a hertz.",
  "outtrim=0.9286 drone_root=55 drone_chord=0 m_o1wave=0 m_o2wave=0 m_o2oct=1 m_o2lvl=0.3 m_o2fine=0.502 m_sub=0 m_gain=0.35 m_fmode=1 m_cut=0.5 m_a_atk=0.8 m_a_rel=0.9 m_send=0.6\n"
  "st_on=1 st_gain=0.25 st_model=4 st_exc=3 st_exclvl=0.3 st_damp=0.15 st_material=0.9 st_pitch=0.55 st_send=0.8\n"
  "e_a1=4 e_sh_hz=0.5 e_sh_mix=0.3 e_rv_mix=0.5 e_rv_decay=0.8 e_rv_size=0.8 e_scale=0.75 e_distance=0.45\n"
  "l1_wave=0 l1_rate=0.55\n#SLOT LFO1 e_sh_hz 0.006\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },
{ "COOLING TOWERS", "DRONE",
  "Concrete cylinders breathing. A cavity over a sub, the cutoff drifting on a slow sine, wide.",
  "drone_root=31 drone_chord=9 drone_spread=0.8 m_o1wave=1 m_o2wave=2 m_o2oct=1 m_o2lvl=0.4 m_o2fine=0.515 m_sub=1 m_sublvl=0.6 m_reinf=0.3 m_gain=0.5 m_width=1.0\n"
  "m_fmode=0 m_cut=0.38 m_res=0.3 m_fdrive=0.3 m_a_atk=0.75 m_a_rel=0.85 m_drift=0.2 m_send=0.5\n"
  "st_on=1 st_gain=0.35 st_model=3 st_exc=3 st_exclvl=0.45 st_damp=0.3 st_dens=0.8 st_material=0.4 st_pitch=0.55 st_send=0.6 st_width=1.0\n"
  "e_rv_mix=0.4 e_rv_decay=0.75 e_rv_size=0.9 e_scale=0.85 e_rv_low=0.6 e_distance=0.3\n"
  "l1_wave=0 l1_rate=0.19 l2_wave=5 l2_rate=0.24\n"
  "#SLOT LFO1 m_cut 0.12\n"
  "#SLOT LFO2 st_pitch 0.03\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },
{ "SERVO BASS", "PLAYABLE BASS",
  "A machine-table bass with a snapping filter and a comb in the lane. Mono.",
  "outtrim=0.9375 vmode=1 glide=0.25 m_o1wave=2 m_o2wave=3 m_o2pw=0.5 m_o2oct=2 m_o2fine=0.52 m_sub=1 m_sublvl=0.6 m_gain=0.5 m_width=0.2\n"
  "m_fmode=0 m_cut=0.3 m_res=0.45 m_fdrive=0.6 m_fenv=0.75 m_f_atk=0.02 m_f_dec=0.4 m_f_sus=0.15 m_a_atk=0.03 m_a_rel=0.4 m_send=0.05\n"
  "s_on=1 s_gain=0.35 s_wt1tab=1 s_wt1pos=0.6 s_wt1oct=2 s_fmode=1 s_cut=0.4 s_res=0.4 s_fenv=0.7 s_f_atk=0.02 s_f_dec=0.4 s_f_sus=0.1 s_a_atk=0.03 s_a_rel=0.4 s_send=0.05 s_width=0.2\n"
  "e_a1=6 e_cb_pitch=0.42 e_cb_fb=0.55 e_cb_damp=0.5 e_cb_mix=0.25 e_a2=1 e_sat_drive=0.3 e_rv_mix=0.05 bassmono=0.5\n"
  "#SLOT VEL m_cut 0.25\n"
  "#SLOT VEL s_cut 0.2\n" },
{ "GLASS RAIN", "RESONANT GESTURE",
  "Noise bursts on glass modes, scattered by an event lane, in a large space.",
  "outtrim=0.8898 drone_root=60 drone_chord=7 m_on=0 st_on=1 st_gain=0.37 st_model=4 st_exc=1 st_exclvl=0.3 st_sustain=0.15 st_damp=0.35 st_dens=0.7 st_material=0.7 st_pitch=0.6 st_strikelvl=0.5 st_send=0.7 st_width=1.0\n"
  "e_rv_mix=0.5 e_rv_decay=0.7 e_rv_size=0.8 e_scale=0.7 e_distance=0.3 e_rv_early=0.35\n"
  "v1_prob=0.6 v1_rate=0.55 v1_target=0 v1_refract=0.3 v1_amt=0.5 r1_type=4 r1_rate=0.5 r1_amt=0.6\n"
  "#SLOT RND1 st_expos 0.4\n"
  "#SLOT RND1 st_pitch 0.05\n"
  "#SLOT EVT1 st_exclvl 0.3\n" },
{ "TELEMETRY", "INDUSTRIAL PULSE",
  "Stepped tables on a synced sample-and-hold, crushed, with a siren far behind.",
  "outtrim=0.6737 drone_root=57 drone_chord=1 m_on=0 s_on=1 s_gain=0.45 s_wt1tab=4 s_wt1pos=0.4 s_wt2tab=6 s_wt2pos=0.3 s_wt2lvl=0.25 s_wt2oct=3 s_fmode=2 s_cut=0.62 s_res=0.35 s_a_atk=0.1 s_a_rel=0.45 s_send=0.35\n"
  "e_a1=7 e_cr_bits=0.45 e_cr_rate=0.55 e_cr_mix=0.6 e_a2=5 e_dl_time=0.72 e_dl_fb=0.45 e_dl_mode=1 e_dl_mix=0.3 e_rv_mix=0.3 e_rv_decay=0.5 e_scale=0.6 e_distance=0.4\n"
  "l1_wave=4 l1_sync=7 l2_wave=3 l2_sync=6 r1_type=2 r1_rate=0.6 r1_amt=0.8\n"
  "#SLOT LFO1 s_wt1pos 0.45\n"
  "#SLOT LFO2 s_gain 0.3\n"
  "#SLOT RND1 s_wt2pos 0.4\n"
  "#SLOT LFO1 e_cr_bits 0.2\n" },
{ "THE DEEP FIELD", "RESTRAINED BED",
  "Slightly inharmonic partials rising very slowly, very far away.",
  "outtrim=0.9375 drone_root=48 drone_chord=4 drone_spread=0.6 m_on=0 s_on=1 s_gain=0.4 s_wt1lvl=0 s_wt2lvl=0 s_addlvl=0.9 s_addn=24 s_addspread=0.54 s_addtilt=0.65 s_addmotion=0.15 s_addfund=1 s_fmode=2 s_cut=0.45 s_a_atk=0.85 s_a_rel=0.9 s_width=1.0 s_send=0.7\n"
  "e_rv_mix=0.5 e_rv_decay=0.85 e_rv_size=0.9 e_rv_damp=0.6 e_scale=0.9 e_distance=0.7 e_rv_mod=0.3\n"
  "l1_wave=0 l1_rate=0.17\n"
  "#SLOT LFO1 s_addtilt 0.15\n"
  "#SLOT LFO1 s_cut 0.08\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 l_coupling=0.15 e_rv_mod=0.1 mem_erosion=0.05\n#SCENE 2 l_coupling=0.4 e_rv_mod=0.3 m_drift=0.3\n#SCENE 3 mem_on=1 mem_gain=0.18 e_distance=0.5 l_coupling=0.55\n#SCENE 4 mem_erosion=0.5 e_scale=0.75 l_coupling=0.7 e_distance=0.35\n" },
{ "FURNACE", "EVOLVING WORLD",
  "Heat rising through a beam. The scenes stoke it; VIOLENCE opens the door.",
  "h_on=1 outtrim=0.5543 drone_root=36 drone_chord=2 m_o1wave=2 m_o2wave=3 m_o2pw=0.4 m_o2fine=0.54 m_sub=1 m_sublvl=0.6 m_reinf=0.3 m_gain=0.5 m_fmode=0 m_cut=0.4 m_res=0.35 m_fdrive=0.6 m_a_atk=0.5 m_a_rel=0.7 m_send=0.25\n"
  "st_on=1 st_gain=0.45 st_model=2 st_exc=3 st_exclvl=0.5 st_damp=0.45 st_material=0.75 st_pitch=0.6 st_couple=0.2 st_stress=0.1 st_send=0.4\n"
  "e_a1=3 e_mb_low=0.1 e_mb_mid=0.3 e_mb_high=0.25 e_a2=1 e_sat_drive=0.3 e_sat_asym=0.6 e_rv_mix=0.3 e_rv_decay=0.55 e_scale=0.5 e_fb_send=0.05 e_fb_ret=0.5 e_fb_delay=0.6 e_fb_to=1\n"
  "v1_prob=0.3 v1_rate=0.45 v1_target=0 v1_refract=0.5 l1_wave=5 l1_rate=0.3 l_coupling=0.5 l_autonomy=0.4\n"
  "#SLOT LFO1 m_cut 0.1\n"
  "#SLOT NE_MASS st_stress 0.3\n"
  "#SCENE 1 e_mb_mid=0.1 st_stress=0.0 e_fb_send=0.0\n"
  "#SCENE 2 e_mb_mid=0.35 st_stress=0.2 e_fb_send=0.08\n"
  "#SCENE 3 e_mb_mid=0.65 e_mb_high=0.5 st_stress=0.6 e_fb_send=0.2 m_fdrive=0.9\n"
  "#SCENE 4 e_mb_mid=0.25 st_stress=0.3 e_fb_send=0.1 m_cut=0.32 e_rv_mix=0.5\n" },
{ "PLATE TECTONICS", "RESONANT GESTURE",
  "A plate bowed so slowly it creaks; the stress builds until it gives.",
  "outtrim=0.4905 drone_root=33 drone_chord=0 m_on=0 st_on=1 st_gain=0.47 st_model=0 st_exc=2 st_exclvl=0.6 st_sustain=0.8 st_bowforce=0.7 st_bowvel=0.2 st_damp=0.4 st_dens=0.9 st_material=0.45 st_stiff=0.35 st_pitch=0.42 st_couple=0.35 st_drive=0.2 st_stress=0.5 st_send=0.5 st_width=0.9\n"
  "e_rv_mix=0.35 e_rv_decay=0.7 e_rv_size=0.85 e_scale=0.8 e_rv_low=0.6\n"
  "l1_wave=5 l1_rate=0.22 l2_wave=0 l2_rate=0.15\n"
  "#SLOT LFO1 st_bowvel 0.15\n"
  "#SLOT LFO2 st_bowforce 0.25\n"
  "#SLOT LFO1 st_expos 0.3\n"
  "#MACRO VIOLENCE st_stress:0.45 st_bowforce:0.3 st_drive:0.5\n" },
{ "RADIO SILENCE", "TRANSITIONAL TEXTURE",
  "The broadcast, frozen by an event lane and shifted a few hertz, thins to nothing.",
  "h_on=1 outtrim=0.9375 drone_root=52 m_on=0 mem_on=1 mem_gain=0.5 mem_src=1 mem_mode=1 mem_smear=0.4 mem_evolve=0.7 mem_disp=0.51 mem_thin=0.15 mem_erosion=0.2 mem_ero_simp=0.8 mem_send=0.6 mem_keyfollow=0\n"
  "e_a1=4 e_sh_hz=0.505 e_sh_mix=0.35 e_rv_mix=0.4 e_rv_decay=0.7 e_scale=0.7 e_distance=0.45\n"
  "v1_prob=0.35 v1_rate=0.35 v1_target=2 v1_refract=0.7 v1_amt=0.6 l1_wave=5 l1_rate=0.25\n"
  "#SLOT LFO1 mem_disp 0.02\n"
  "#SLOT HIST mem_thin 0.7\n"
  "#SLOT HIST mem_erosion 0.6\n"
  "#SCENE 1 mem_thin=0.1\n"
  "#SCENE 2 mem_thin=0.3 e_sh_mix=0.5\n"
  "#SCENE 3 mem_thin=0.6 mem_erosion=0.6\n"
  "#SCENE 4 mem_thin=0.9 mem_gain=0.35 e_rv_mix=0.6\n" },
{ "MONOLITH", "DRONE",
  "One enormous square sub with its harmonics reinforced and a hollow table above it. Nothing moves.",
  "outtrim=0.6280 drone_root=29 drone_chord=0 m_o1wave=1 m_o1lvl=0.5 m_o2lvl=0 m_sub=2 m_subwave=1 m_sublvl=0.9 m_reinf=0.5 m_gain=0.55 m_fmode=1 m_cut=0.28 m_a_atk=0.7 m_a_rel=0.85 m_drift=0.05 m_send=0.1\n"
  "s_on=1 s_gain=0.3 s_wt1tab=3 s_wt1pos=0.1 s_wt1oct=3 s_fmode=2 s_cut=0.4 s_a_atk=0.7 s_a_rel=0.85 s_send=0.3\n"
  "e_a1=1 e_sat_drive=0.2 e_rv_mix=0.2 e_rv_decay=0.6 e_scale=0.7 e_rv_low=0.7 bassmono=0.45\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 e_fb_send=0.0 mem_erosion=0.05 l_coupling=0.2\n#SCENE 2 e_fb_send=0.1 l_coupling=0.4 m_drift=0.35\n#SCENE 3 e_fb_send=0.2 mem_on=1 mem_gain=0.22 mem_erosion=0.4 e_distance=0.45\n#SCENE 4 e_fb_send=0.12 mem_erosion=0.7 e_scale=0.8 l_coupling=0.65\n" },
{ "SWARM", "EVOLVING WORLD",
  "Organism tables through phase-modulation feedback, a random walk on the pitch, choir grains, the network wide awake.",
  "outtrim=0.8022 drone_root=45 drone_chord=3 m_on=0 s_on=1 s_gain=0.4 s_wt1tab=7 s_wt1pos=0.5 s_wt2tab=7 s_wt2pos=0.8 s_wt2lvl=0.4 s_wt2fine=0.58 s_pmratio=2 s_pmidx=0.3 s_pmfb=0.45 s_fmode=2 s_cut=0.55 s_res=0.4 s_a_atk=0.5 s_a_rel=0.7 s_send=0.4\n"
  "mem_on=1 mem_gain=0.3 mem_src=2 mem_mode=0 mem_dur=0.4 mem_dens=0.65 mem_sched=2 mem_pspread=0.15 mem_scatter=0.7 mem_send=0.5\n"
  "e_a1=1 e_sat_drive=0.25 e_rv_mix=0.35 e_rv_decay=0.6 e_scale=0.6\n"
  "r1_type=1 r1_rate=0.4 r1_amt=0.6 r1_restore=0.4 r2_type=5 r2_rate=0.35 r2_amt=0.5 l_coupling=0.7 l_autonomy=0.5 l_recovery=0.3\n"
  "#SLOT RND1 s_wt1fine 0.15\n"
  "#SLOT RND2 s_pmidx 0.35\n"
  "#SLOT NE_MEMORY s_pmfb 0.3\n"
  "#SLOT NE_SIGNAL mem_dens 0.3\n"
  "h_on=1 h_dur=0.8\n"
  "#SCENE 1 v1_prob=0.15 l_coupling=0.3\n#SCENE 2 v1_prob=0.45 l_coupling=0.55 m_drift=0.4\n#SCENE 3 v1_prob=0.75 e_fb_send=0.2 st_couple=0.4\n#SCENE 4 v1_prob=0.4 l_coupling=0.8 mem_on=1 mem_gain=0.25\n" },
{ "THIN AIR", "DRY",
  "A sine and a triangle, slowly, and nothing else at all. The quiet tone.",
  "outtrim=0.6959 drone_root=48 drone_chord=2 m_o1wave=0 m_o2wave=1 m_o2lvl=0.35 m_o2fine=0.505 m_sub=0 m_gain=0.5 m_fmode=1 m_cut=0.5 m_a_atk=0.8 m_a_rel=0.85 m_drift=0.1 m_drifttime=0.8 m_send=0 e_rv_mix=0 e_distance=0\n" },
{ "THE LAST GENERATOR", "INDUSTRIAL PULSE",
  "A square gate over a machine table and a sub, a tape delay behind, the loop feeding the structure.",
  "outtrim=0.6369 drone_root=33 drone_chord=0 m_o1wave=3 m_o1pw=0.6 m_o2wave=2 m_o2oct=1 m_o2lvl=0.5 m_sub=1 m_sublvl=0.6 m_gain=0.45 m_fmode=0 m_cut=0.34 m_res=0.4 m_fdrive=0.5 m_a_atk=0.1 m_a_rel=0.45\n"
  "s_on=1 s_gain=0.3 s_wt1tab=1 s_wt1pos=0.7 s_wt1oct=2 s_fmode=1 s_cut=0.45 s_res=0.3 s_a_atk=0.1 s_a_rel=0.45\n"
  "st_on=1 st_gain=0.35 st_model=2 st_exc=6 st_exclvl=0.6 st_damp=0.5 st_material=0.8 st_pitch=0.6 st_send=0.3\n"
  "e_a1=5 e_dl_time=0.75 e_dl_fb=0.45 e_dl_mode=0 e_dl_mod=0.2 e_dl_mix=0.3 e_a2=3 e_mb_mid=0.3 e_mb_high=0.2 e_rv_mix=0.25 e_rv_decay=0.5 e_scale=0.45 e_fb_send=0.12 e_fb_ret=0.5 e_fb_delay=0.62 e_fb_to=1 e_fb_damp=0.5\n"
  "l1_wave=3 l1_sync=6 l2_wave=3 l2_sync=7\n"
  "#SLOT LFO1 m_gain 0.5\n"
  "#SLOT LFO1 s_gain 0.5\n"
  "#SLOT LFO2 m_cut 0.15\n" },
};

int numPresets() { return (int) (sizeof (BANK) / sizeof (BANK[0])); }
const PresetDef& preset (int i) { return BANK[i < 0 ? 0 : (i >= numPresets() ? numPresets() - 1 : i)]; }

void applyPreset (int i, Params& base, MacroDest macro[NUM_MACROS][MACRO_DESTS],
                  Params scene[NUM_SCENES], bool sceneSet[NUM_SCENES], Slot slots[NUM_SLOTS], Mseg mseg[4])
{
    base = Params();
    defaultMacros (macro);
    applyPresetBody (preset (i).body, base, macro, scene, sceneSet, slots, mseg);
    base.v[P_patch] = (float) clampi (i, 0, numPresets() - 1);
}

} // namespace tty
