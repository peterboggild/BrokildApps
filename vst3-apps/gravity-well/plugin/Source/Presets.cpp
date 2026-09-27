/*  GRAVITY WELL - the factory bank.

    Thirty-two, slots 0..31, ordered from the ordinary to the strange, so the
    dial itself tells you how far out you are going.  Slots 32..199 are the
    user's and live on disk.  (Peter, 2026-09-26: thirty-two that are
    genuinely different beat a hundred with filler in them.)

    Each preset is a SPARSE list of (parameter, value in its own units) laid
    over the defaults - the 1984 precedent.  Anything a preset does not name
    is whatever Params() says, so adding a parameter later cannot silently
    change an existing preset.
*/
#include "Engine.h"

namespace gw {

struct PV { int id; float v; };
struct Factory { const char* name; const char* group; const PV* pv; int n; };

#define BANK(sym, ...) static const PV sym[] = { __VA_ARGS__ };
#define ENT(sym, name, group) { name, group, sym, (int) (sizeof (sym) / sizeof (PV)) }

// ------------------------------------------------ FOUNDATION (0-9): the plain ones
BANK(k00, {P_a_shape,0.00f},{P_a_level,0.85f},{P_sub_level,0.70f},{P_fa_cut,0.3243f},
          {P_fa_res,0.12f},{P_e1_d,0.6811f},{P_e1_s,0.85f},{P_macro_mass,0.00f},{P_out_trim,-0.35f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k01, {P_a_shape,0.00f},{P_b_level,0.45f},{P_b_fine,7.0f},{P_fa_cut,0.50f},
          {P_fa_env,0.35f},{P_e2_d,0.6267f},{P_sub_level,0.55f},{P_out_trim,-0.08f})
BANK(k02, {P_a_shape,0.33f},{P_a_width,0.35f},{P_sub_level,0.80f},{P_fa_cut,0.2843f},
          {P_fa_circ,0.f},{P_fa_res,0.25f},{P_macro_depth,0.55f},{P_out_trim,-0.90f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k03, {P_a_shape,0.99f},{P_sub_level,0.70f},{P_sub_shape,0.f},{P_fa_cut,0.1243f},
          {P_glide,0.6975f},{P_e1_a,0.5097f},{P_e1_d,0.7549f},{P_e1_s,0.95f},{P_e1_r,0.7242f},
          {P_macro_depth,0.80f},{P_macro_space,0.10f},{P_out_trim,-3.84f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k04, {P_a_shape,0.00f},{P_fa_cut,0.30f},{P_fa_res,0.42f},{P_fa_env,0.75f},
          {P_e2_d,0.6100f},{P_e2_s,0.f},{P_e1_d,0.6400f},{P_e1_s,0.f},{P_e1_curve,0.f},
          {P_macro_energy,0.30f},{P_out_trim,8.00f})
BANK(k05, {P_a_shape,0.33f},{P_a_width,0.50f},{P_fa_cut,0.3543f},{P_fa_res,0.18f},
          {P_sub_level,0.60f},{P_e1_d,0.648f},{P_e1_s,0.55f},{P_out_trim,-1.20f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k06, {P_a_level,0.f},{P_a_oct,1.f},{P_sub_level,1.00f},{P_sub_shape,0.f},{P_fa_cut,0.0843f},
          {P_macro_depth,0.90f},{P_e1_a,0.3927f},{P_e1_s,1.00f},{P_out_trim,-2.09f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k07, {P_a_shape,0.20f},{P_disk_n,3.f},{P_disk_spread,0.22f},{P_fa_cut,0.3043f},
          {P_macro_space,0.35f},{P_e1_d,0.7117f},{P_e1_s,0.75f},{P_out_trim,1.53f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k08, {P_a_shape,0.00f},{P_fa_cut,0.52f},{P_fa_env,0.55f},{P_e2_d,0.5261f},
          {P_e2_s,0.f},{P_e1_d,0.5529f},{P_e1_s,0.f},{P_drv_amt,0.18f},{P_drv_type,1.f},{P_out_trim,7.50f},{P_drv_pos,1.0f})
BANK(k09, {P_a_shape,0.10f},{P_e1_a,0.6975f},{P_e1_d,0.7713f},{P_e1_s,0.90f},{P_e1_r,0.7456f},
          {P_e1_curve,1.00f},{P_fa_cut,0.2443f},{P_macro_mass,0.30f},{P_macro_depth,0.55f},{P_out_trim,3.19f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})

// ------------------------------------------------ WORKING (10-19): the ones you reach for
BANK(k10, {P_fa_circ,4.f},{P_fa_cut,0.30f},{P_fa_res,0.58f},{P_fa_env,0.70f},
          {P_a_shape,0.00f},{P_sub_level,0.35f},{P_e2_d,0.5835f},{P_e2_s,0.f},
          {P_e1_d,0.6141f},{P_e1_s,0.f},
          {P_drv_amt,0.22f},{P_drv_type,1.f},{P_out_trim,0.86f},{P_drv_pos,1.0f})
BANK(k11, {P_fa_circ,2.f},{P_fa_cut,0.34f},{P_fa_res,0.72f},{P_fa_env,0.80f},
          {P_a_shape,0.33f},{P_e2_d,0.5641f},{P_e2_s,0.f},{P_e1_d,0.5999f},{P_e1_s,0.f},
          {P_drv_amt,0.35f},{P_drv_type,2.f},{P_out_trim,3.45f},{P_drv_pos,1.0f})
BANK(k12, {P_a_shape,0.00f},{P_b_level,0.85f},{P_b_fine,12.f},{P_disk_n,4.f},
          {P_disk_spread,0.45f},{P_fa_cut,0.3443f},{P_fa_res,0.20f},{P_macro_space,0.45f},
          {P_e1_s,0.95f},{P_e1_d,0.7354f},{P_out_trim,-2.15f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k13, {P_fa_circ,6.f},{P_fa_cut,0.3817f},{P_fa_res,0.55f},{P_l1_rate,0.4355f},
          {P_m1_src,4.f},{P_m1_dst,9.f},{P_m1_amt,0.55f},{P_drv_amt,0.40f},{P_drv_type,2.f},
          {P_sub_level,0.55f},{P_macro_energy,0.45f},{P_out_trim,-1.99f},{P_fa_env,0.1f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k14, {P_fa_circ,0.f},{P_fa_cut,0.2617f},{P_fa_res,0.60f},{P_l1_sync,1.f},
          {P_l1_rate,0.40f},{P_m1_src,4.f},{P_m1_dst,9.f},{P_m1_amt,0.80f},
          {P_sub_level,0.70f},{P_route,2.f},{P_split,0.30f},{P_fb_circ,3.f},{P_fb_mode,0.f},{P_out_trim,2.10f},{P_fa_env,0.1f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k15, {P_fa_circ,5.f},{P_fa_cut,0.4043f},{P_fa_res,0.70f},{P_noise_level,0.22f},
          {P_ringmod,0.30f},{P_b_level,0.5f},{P_b_semi,7.f},{P_drv_amt,0.55f},{P_drv_type,3.f},
          {P_macro_energy,0.60f},{P_out_trim,0.66f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k16, {P_a_shape,0.00f},{P_disk_n,5.f},{P_disk_spread,0.55f},{P_fa_cut,0.1643f},
          {P_fa_res,0.30f},{P_drv_amt,0.62f},{P_drv_type,2.f},{P_ring_amt,0.45f},
          {P_ring_decay,0.70f},{P_ring_tone,0.30f},{P_macro_mass,0.55f},{P_macro_depth,0.75f},
          {P_e1_a,0.5835f},{P_e1_d,0.7855f},{P_e1_s,0.85f},{P_e1_r,0.7549f},{P_e1_curve,0.85f},{P_out_trim,-5.61f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k17, {P_a_shape,0.00f},{P_e1_a,0.0f},{P_e1_d,0.6379f},{P_e1_s,0.45f},{P_e1_curve,0.f},
          {P_fa_cut,0.46f},{P_fa_env,0.60f},{P_e2_d,0.4903f},{P_e2_s,0.f},{P_noise_level,0.12f},
          {P_drv_amt,0.25f},{P_drv_type,1.f},{P_out_trim,1.83f},{P_drv_pos,1.0f})
BANK(k18, {P_a_shape,0.66f},{P_fa_circ,1.f},{P_fa_cut,0.36f},{P_fa_res,0.48f},
          {P_fa_env,0.45f},{P_e2_d,0.648f},{P_glide,0.6267f},{P_sub_level,0.65f},{P_out_trim,-9.51f})
BANK(k19, {P_a_engine,1.f},{P_a_table,4.f},{P_a_shape,0.55f},{P_fa_circ,3.f},
          {P_fa_mode,0.f},{P_fa_cut,0.4543f},{P_fa_res,0.25f},{P_sub_level,0.45f},
          {P_macro_space,0.40f},{P_out_trim,-8.42f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})

// ------------------------------------------------ CHARACTER (20-27): the strong ones
BANK(k20, {P_fa_circ,5.f},{P_fa_cut,0.2043f},{P_fa_res,0.80f},{P_fa_mode,3.f},
          {P_a_shape,0.20f},{P_sub_level,0.50f},{P_macro_energy,0.30f},{P_ring_amt,0.20f},{P_out_trim,-11.19f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k21, {P_fa_circ,6.f},{P_fa_cut,0.3617f},{P_fa_res,0.65f},{P_m1_src,5.f},{P_m1_dst,9.f},
          {P_m1_amt,0.60f},{P_l2_rate,0.2355f},{P_drv_amt,0.35f},{P_drv_type,1.f},
          {P_sub_level,0.50f},{P_out_trim,-3.67f},{P_fa_env,0.1f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k22, {P_ringmod,0.75f},{P_b_semi,5.f},{P_b_level,0.8f},{P_fa_circ,0.f},
          {P_fa_cut,0.3543f},{P_fa_res,0.35f},{P_drv_amt,0.45f},{P_drv_type,3.f},
          {P_macro_energy,0.55f},{P_out_trim,-1.51f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k23, {P_m1_src,8.f},{P_m1_dst,1.f},{P_m1_amt,-0.55f},{P_chirp_time,0.6975f},
          {P_fa_cut,0.3043f},{P_fa_res,0.45f},{P_a_shape,0.10f},{P_drv_amt,0.30f},
          {P_macro_mass,0.35f},{P_out_trim,6.85f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k24, {P_disk_n,8.f},{P_disk_spread,0.85f},{P_disk_rot,0.55f},{P_a_engine,1.f},
          {P_a_table,7.f},{P_fa_cut,0.4043f},{P_macro_space,0.70f},{P_e1_s,0.9f},{P_e1_d,0.7549f},{P_out_trim,-2.81f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k25, {P_redshift,0.78f},{P_macro_mass,0.65f},{P_fa_circ,6.f},{P_fa_cut,0.3543f},
          {P_sub_level,0.70f},{P_e1_a,0.5641f},{P_e1_s,0.90f},{P_e1_d,0.7713f},
          {P_macro_depth,0.70f},{P_out_trim,-8.55f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k26, {P_ring_amt,0.80f},{P_ring_decay,0.82f},{P_ring_tone,0.25f},{P_macro_mass,0.70f},
          {P_a_level,0.45f},{P_sub_level,0.60f},{P_fa_cut,0.2543f},{P_e1_d,0.7242f},{P_e1_s,0.35f},{P_out_trim,3.68f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k27, {P_macro_mass,0.95f},{P_macro_horiz,0.95f},{P_r_l1,0.05f},{P_r_l2,0.35f},
          {P_l1_rate,0.5655f},{P_m1_src,4.f},{P_m1_dst,9.f},{P_m1_amt,0.70f},
          {P_m2_src,5.f},{P_m2_dst,11.f},{P_m2_amt,0.40f},{P_fa_cut,0.3617f},{P_fa_res,0.50f},{P_sub_level,0.85f},{P_out_trim,2.32f},{P_fa_env,0.1f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_l2_rate,0.2155f})

// ------------------------------------------------ EVENT HORIZON (28-31): the far end
BANK(k28, {P_macro_mass,1.00f},{P_macro_horiz,1.00f},{P_redshift,0.90f},{P_ring_amt,0.70f},
          {P_r_e2,0.10f},{P_r_l1,0.10f},{P_r_l2,0.20f},{P_r_l3,0.30f},
          {P_fa_circ,6.f},{P_fa_cut,0.3043f},{P_drv_amt,0.50f},{P_drv_type,3.f},
          {P_e1_a,0.6616f},{P_e1_s,0.95f},{P_e1_r,0.7713f},{P_macro_depth,0.85f},{P_out_trim,-8.27f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k29, {P_m1_src,9.f},{P_m1_dst,1.f},{P_m1_amt,0.45f},{P_m2_src,9.f},{P_m2_dst,18.f},
          {P_m2_amt,0.80f},{P_macro_mass,0.60f},{P_mass_track,0.95f},{P_disk_n,6.f},
          {P_disk_spread,0.70f},{P_fa_circ,5.f},{P_fa_cut,0.3243f},{P_fa_res,0.72f},{P_out_trim,-11.39f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})
BANK(k30, {P_sing_level,0.95f},{P_sing_src,1.f},{P_fa_cut,0.5043f},{P_fa_res,0.85f},
          {P_fa_circ,2.f},{P_drv_amt,0.80f},{P_drv_type,3.f},{P_a_shape,0.15f},
          {P_macro_depth,1.00f},{P_macro_energy,0.75f},{P_out_trim,-14.71f},{P_fa_env,0.25f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f},{P_drv_pos,1.0f})
BANK(k31, {P_noise_level,0.55f},{P_noise_col,0.80f},{P_a_level,0.25f},{P_ring_amt,0.65f},
          {P_ring_decay,0.90f},{P_macro_mass,0.80f},{P_redshift,0.60f},{P_fa_circ,5.f},
          {P_fa_cut,0.5117f},{P_fa_res,0.75f},{P_r_rnd,0.15f},{P_m1_src,7.f},{P_m1_dst,9.f},
          {P_m1_amt,0.50f},{P_out_trim,-13.82f},{P_fa_env,0.1f},{P_e2_a,0.0f},{P_e2_s,1.0f},{P_e2_r,1.0f})

static const Factory FAC[NUM_FACTORY] = {
    ENT(k00, "SCHWARZSCHILD", "FOUNDATION"), ENT(k01, "FLAT SPACE",    "FOUNDATION"),
    ENT(k02, "SOLID BODY",    "FOUNDATION"), ENT(k03, "ROUND DUB",     "FOUNDATION"),
    ENT(k04, "TECHNO PLUCK",  "FOUNDATION"), ENT(k05, "SQUARE ONE",    "FOUNDATION"),
    ENT(k06, "SUB ONLY",      "FOUNDATION"), ENT(k07, "SOFT ORBIT",    "FOUNDATION"),
    ENT(k08, "SHORT FALL",    "FOUNDATION"), ENT(k09, "SLOW MASS",     "FOUNDATION"),

    ENT(k10, "ACID 303",      "WORKING"),    ENT(k11, "ACID SCREAM",   "WORKING"),
    ENT(k12, "REESE",         "WORKING"),    ENT(k13, "GROWL",         "WORKING"),
    ENT(k14, "WOBBLE",        "WORKING"),    ENT(k15, "INDUSTRIAL",    "WORKING"),
    ENT(k16, "DOOM WEIGHT",   "WORKING"),    ENT(k17, "PICK ATTACK",   "WORKING"),
    ENT(k18, "RUBBER",        "WORKING"),    ENT(k19, "GLASS BASS",    "WORKING"),

    ENT(k20, "COMB TUNED",    "CHARACTER"),  ENT(k21, "FORMANT TALK",  "CHARACTER"),
    ENT(k22, "RING METAL",    "CHARACTER"),  ENT(k23, "CHIRP DROP",    "CHARACTER"),
    ENT(k24, "SWARM",         "CHARACTER"),  ENT(k25, "REDSHIFT",      "CHARACTER"),
    ENT(k26, "RINGDOWN",      "CHARACTER"),  ENT(k27, "FROZEN CLOCK",  "CHARACTER"),

    ENT(k28, "EVENT HORIZON", "EVENT HORIZON"), ENT(k29, "TIDAL",      "EVENT HORIZON"),
    ENT(k30, "SINGULARITY",   "EVENT HORIZON"), ENT(k31, "HAWKING",    "EVENT HORIZON")
};

int         numFactory()            { return NUM_FACTORY; }
const char* factoryName  (int i)    { return (i >= 0 && i < NUM_FACTORY) ? FAC[i].name  : ""; }
const char* factoryGroup (int i)    { return (i >= 0 && i < NUM_FACTORY) ? FAC[i].group : ""; }

void applyFactory (int i, Params& p)
{
    p = Params();                                  // every value at its default first
    if (i < 0 || i >= NUM_FACTORY) return;
    const Factory& f = FAC[i];
    for (int k = 0; k < f.n; ++k) {
        const int id = f.pv[k].id;
        if (id >= 0 && id < NUM_PARAMS) {
            const PSpec& s = spec (id);
            float v = f.pv[k].v;
            p.v[id] = v < s.lo ? s.lo : (v > s.hi ? s.hi : v);
        }
    }
}

//  The sequencer pattern a preset opens with.  Only the 303-ish ones carry a
//  real one; everything else gets a plain sixteen so turning SEQ on is never
//  a surprise.
void applyFactoryPattern (int i, Step* dst, int n)
{
    for (int k = 0; k < n; ++k) { dst[k] = Step(); dst[k].on = (k % 4 == 0) ? 1 : 0; dst[k].gate = 50; }
    if (i == 10 || i == 11) {
        static const int8_t note[16] = { 0,0,12,0, 3,0,0,10, 0,12,0,0, 5,0,3,0 };
        static const uint8_t acc[16] = { 1,0,0,0, 1,0,0,1, 0,0,0,0, 1,0,0,0 };
        static const uint8_t sld[16] = { 0,0,1,0, 0,0,0,1, 0,1,0,0, 0,0,1,0 };
        for (int k = 0; k < n && k < 16; ++k) {
            dst[k].on = (k == 1 || k == 6 || k == 11) ? 0 : 1;
            dst[k].note = note[k]; dst[k].accent = acc[k]; dst[k].slide = sld[k];
            dst[k].gate = sld[k] ? 95 : 55;
        }
    }
}

} // namespace gw
