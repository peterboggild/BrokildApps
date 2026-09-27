// Twelve more presets (36 -> 48), appended before the bank's closing brace,
// and POPULATION: ZERO lifted a little. Exact-count anchors.
const fs = require("fs");
const path = "C:/Users/peter/b/ThirtyThousandYears/Source/Presets.cpp";
let s = fs.readFileSync(path, "utf8");
const NL = String.fromCharCode(92) + "n";
const Q = String.fromCharCode(34);
function body(lines) { return lines.map(l => "  " + Q + l + NL + Q).join("\n"); }
function preset(name, cat, note, lines) { return "{ " + Q + name + Q + ", " + Q + cat + Q + ",\n  " + Q + note + Q + ",\n" + body(lines) + " },\n"; }

const more =
  preset("COOLING TOWERS", "DRONE", "Concrete cylinders breathing. A cavity over a sub, the cutoff drifting on a slow sine, wide.",
    ["drone_root=31 drone_chord=9 drone_spread=0.8 m_o1wave=1 m_o2wave=2 m_o2oct=1 m_o2lvl=0.4 m_o2fine=0.515 m_sub=1 m_sublvl=0.6 m_reinf=0.3 m_gain=0.5 m_width=1.0",
     "m_fmode=0 m_cut=0.38 m_res=0.3 m_fdrive=0.3 m_a_atk=0.75 m_a_rel=0.85 m_drift=0.2 m_send=0.5",
     "st_on=1 st_gain=0.35 st_model=3 st_exc=3 st_exclvl=0.45 st_damp=0.3 st_dens=0.8 st_material=0.4 st_pitch=0.55 st_send=0.6 st_width=1.0",
     "e_rv_mix=0.4 e_rv_decay=0.75 e_rv_size=0.9 e_scale=0.85 e_rv_low=0.6 e_distance=0.3",
     "l1_wave=0 l1_rate=0.19 l2_wave=5 l2_rate=0.24",
     "#SLOT LFO1 m_cut 0.12", "#SLOT LFO2 st_pitch 0.03"]) +
  preset("SERVO BASS", "PLAYABLE BASS", "A machine-table bass with a snapping filter and a comb in the lane. Mono.",
    ["vmode=1 glide=0.25 m_o1wave=2 m_o2wave=3 m_o2pw=0.5 m_o2oct=2 m_o2fine=0.52 m_sub=1 m_sublvl=0.6 m_gain=0.5 m_width=0.2",
     "m_fmode=0 m_cut=0.3 m_res=0.45 m_fdrive=0.6 m_fenv=0.75 m_f_atk=0.02 m_f_dec=0.4 m_f_sus=0.15 m_a_atk=0.03 m_a_rel=0.4 m_send=0.05",
     "s_on=1 s_gain=0.35 s_wt1tab=1 s_wt1pos=0.6 s_wt1oct=2 s_fmode=1 s_cut=0.4 s_res=0.4 s_fenv=0.7 s_f_atk=0.02 s_f_dec=0.4 s_f_sus=0.1 s_a_atk=0.03 s_a_rel=0.4 s_send=0.05 s_width=0.2",
     "e_a1=6 e_cb_pitch=0.42 e_cb_fb=0.55 e_cb_damp=0.5 e_cb_mix=0.25 e_a2=1 e_sat_drive=0.3 e_rv_mix=0.05 bassmono=0.5",
     "#SLOT VEL m_cut 0.25", "#SLOT VEL s_cut 0.2"]) +
  preset("GLASS RAIN", "RESONANT GESTURE", "Noise bursts on glass modes, scattered by an event lane, in a large space.",
    ["drone_root=60 drone_chord=7 m_on=0 st_on=1 st_gain=0.45 st_model=4 st_exc=1 st_exclvl=0.3 st_sustain=0.15 st_damp=0.2 st_dens=0.7 st_material=0.85 st_pitch=0.6 st_strikelvl=0.5 st_send=0.7 st_width=1.0",
     "e_rv_mix=0.5 e_rv_decay=0.7 e_rv_size=0.8 e_scale=0.7 e_distance=0.3 e_rv_early=0.35",
     "v1_prob=0.6 v1_rate=0.55 v1_target=0 v1_refract=0.3 v1_amt=0.5 r1_type=4 r1_rate=0.5 r1_amt=0.6",
     "#SLOT RND1 st_expos 0.4", "#SLOT RND1 st_pitch 0.05", "#SLOT EVT1 st_exclvl 0.3"]) +
  preset("TELEMETRY", "INDUSTRIAL PULSE", "Stepped tables on a synced sample-and-hold, crushed, with a siren far behind.",
    ["drone_root=57 drone_chord=1 m_on=0 s_on=1 s_gain=0.45 s_wt1tab=4 s_wt1pos=0.4 s_wt2tab=6 s_wt2pos=0.3 s_wt2lvl=0.25 s_wt2oct=3 s_fmode=2 s_cut=0.62 s_res=0.35 s_a_atk=0.1 s_a_rel=0.45 s_send=0.35",
     "e_a1=7 e_cr_bits=0.45 e_cr_rate=0.55 e_cr_mix=0.6 e_a2=5 e_dl_time=0.72 e_dl_fb=0.45 e_dl_mode=1 e_dl_mix=0.3 e_rv_mix=0.3 e_rv_decay=0.5 e_scale=0.6 e_distance=0.4",
     "l1_wave=4 l1_sync=7 l2_wave=3 l2_sync=6 r1_type=2 r1_rate=0.6 r1_amt=0.8",
     "#SLOT LFO1 s_wt1pos 0.45", "#SLOT LFO2 s_gain 0.3", "#SLOT RND1 s_wt2pos 0.4", "#SLOT LFO1 e_cr_bits 0.2"]) +
  preset("THE DEEP FIELD", "RESTRAINED BED", "Slightly inharmonic partials rising very slowly, very far away.",
    ["drone_root=48 drone_chord=4 drone_spread=0.6 m_on=0 s_on=1 s_gain=0.4 s_wt1lvl=0 s_wt2lvl=0 s_addlvl=0.9 s_addn=24 s_addspread=0.54 s_addtilt=0.65 s_addmotion=0.15 s_addfund=1 s_fmode=2 s_cut=0.45 s_a_atk=0.85 s_a_rel=0.9 s_width=1.0 s_send=0.7",
     "e_rv_mix=0.5 e_rv_decay=0.85 e_rv_size=0.9 e_rv_damp=0.6 e_scale=0.9 e_distance=0.7 e_rv_mod=0.3",
     "l1_wave=0 l1_rate=0.17",
     "#SLOT LFO1 s_addtilt 0.15", "#SLOT LFO1 s_cut 0.08"]) +
  preset("FURNACE", "EVOLVING WORLD", "Heat rising through a beam. The scenes stoke it; VIOLENCE opens the door.",
    ["drone_root=36 drone_chord=2 m_o1wave=2 m_o2wave=3 m_o2pw=0.4 m_o2fine=0.54 m_sub=1 m_sublvl=0.6 m_reinf=0.3 m_gain=0.5 m_fmode=0 m_cut=0.4 m_res=0.35 m_fdrive=0.6 m_a_atk=0.5 m_a_rel=0.7 m_send=0.25",
     "st_on=1 st_gain=0.45 st_model=2 st_exc=3 st_exclvl=0.5 st_damp=0.45 st_material=0.75 st_pitch=0.6 st_couple=0.2 st_stress=0.1 st_send=0.4",
     "e_a1=3 e_mb_low=0.1 e_mb_mid=0.3 e_mb_high=0.25 e_a2=1 e_sat_drive=0.3 e_sat_asym=0.6 e_rv_mix=0.3 e_rv_decay=0.55 e_scale=0.5 e_fb_send=0.05 e_fb_ret=0.5 e_fb_delay=0.6 e_fb_to=1",
     "v1_prob=0.3 v1_rate=0.45 v1_target=0 v1_refract=0.5 l1_wave=5 l1_rate=0.3 l_coupling=0.5 l_autonomy=0.4",
     "#SLOT LFO1 m_cut 0.1", "#SLOT NE_MASS st_stress 0.3",
     "#SCENE 1 e_mb_mid=0.1 st_stress=0.0 e_fb_send=0.0", "#SCENE 2 e_mb_mid=0.35 st_stress=0.2 e_fb_send=0.08", "#SCENE 3 e_mb_mid=0.65 e_mb_high=0.5 st_stress=0.6 e_fb_send=0.2 m_fdrive=0.9", "#SCENE 4 e_mb_mid=0.25 st_stress=0.3 e_fb_send=0.1 m_cut=0.32 e_rv_mix=0.5"]) +
  preset("PLATE TECTONICS", "RESONANT GESTURE", "A plate bowed so slowly it creaks; the stress builds until it gives.",
    ["drone_root=33 drone_chord=0 m_on=0 st_on=1 st_gain=0.55 st_model=0 st_exc=2 st_exclvl=0.6 st_sustain=0.8 st_bowforce=0.7 st_bowvel=0.2 st_damp=0.25 st_dens=0.9 st_material=0.6 st_stiff=0.35 st_pitch=0.5 st_couple=0.35 st_drive=0.2 st_stress=0.5 st_send=0.5 st_width=0.9",
     "e_rv_mix=0.35 e_rv_decay=0.7 e_rv_size=0.85 e_scale=0.8 e_rv_low=0.6",
     "l1_wave=5 l1_rate=0.22 l2_wave=0 l2_rate=0.15",
     "#SLOT LFO1 st_bowvel 0.15", "#SLOT LFO2 st_bowforce 0.25", "#SLOT LFO1 st_expos 0.3",
     "#MACRO VIOLENCE st_stress:0.45 st_bowforce:0.3 st_drive:0.5"]) +
  preset("RADIO SILENCE", "TRANSITIONAL TEXTURE", "The broadcast, frozen by an event lane and shifted a few hertz, thins to nothing.",
    ["drone_root=52 m_on=0 mem_on=1 mem_gain=0.5 mem_src=1 mem_mode=1 mem_smear=0.4 mem_evolve=0.7 mem_disp=0.51 mem_thin=0.15 mem_erosion=0.2 mem_ero_simp=0.8 mem_send=0.6 mem_keyfollow=0",
     "e_a1=4 e_sh_hz=0.505 e_sh_mix=0.35 e_rv_mix=0.4 e_rv_decay=0.7 e_scale=0.7 e_distance=0.45",
     "v1_prob=0.35 v1_rate=0.35 v1_target=2 v1_refract=0.7 v1_amt=0.6 l1_wave=5 l1_rate=0.25",
     "#SLOT LFO1 mem_disp 0.02", "#SLOT HIST mem_thin 0.7", "#SLOT HIST mem_erosion 0.6",
     "#SCENE 1 mem_thin=0.1", "#SCENE 2 mem_thin=0.3 e_sh_mix=0.5", "#SCENE 3 mem_thin=0.6 mem_erosion=0.6", "#SCENE 4 mem_thin=0.9 mem_gain=0.35 e_rv_mix=0.6"]) +
  preset("MONOLITH", "DRONE", "One enormous square sub with its harmonics reinforced and a hollow table above it. Nothing moves.",
    ["drone_root=29 drone_chord=0 m_o1wave=1 m_o1lvl=0.5 m_o2lvl=0 m_sub=2 m_subwave=1 m_sublvl=0.9 m_reinf=0.5 m_gain=0.55 m_fmode=1 m_cut=0.28 m_a_atk=0.7 m_a_rel=0.85 m_drift=0.05 m_send=0.1",
     "s_on=1 s_gain=0.3 s_wt1tab=3 s_wt1pos=0.1 s_wt1oct=3 s_fmode=2 s_cut=0.4 s_a_atk=0.7 s_a_rel=0.85 s_send=0.3",
     "e_a1=1 e_sat_drive=0.2 e_rv_mix=0.2 e_rv_decay=0.6 e_scale=0.7 e_rv_low=0.7 bassmono=0.45"]) +
  preset("SWARM", "EVOLVING WORLD", "Organism tables through phase-modulation feedback, a random walk on the pitch, choir grains, the network wide awake.",
    ["drone_root=45 drone_chord=3 m_on=0 s_on=1 s_gain=0.4 s_wt1tab=7 s_wt1pos=0.5 s_wt2tab=7 s_wt2pos=0.8 s_wt2lvl=0.4 s_wt2fine=0.58 s_pmratio=2 s_pmidx=0.3 s_pmfb=0.45 s_fmode=2 s_cut=0.55 s_res=0.4 s_a_atk=0.5 s_a_rel=0.7 s_send=0.4",
     "mem_on=1 mem_gain=0.3 mem_src=2 mem_mode=0 mem_dur=0.4 mem_dens=0.65 mem_sched=2 mem_pspread=0.15 mem_scatter=0.7 mem_send=0.5",
     "e_a1=1 e_sat_drive=0.25 e_rv_mix=0.35 e_rv_decay=0.6 e_scale=0.6",
     "r1_type=1 r1_rate=0.4 r1_amt=0.6 r1_restore=0.4 r2_type=5 r2_rate=0.35 r2_amt=0.5 l_coupling=0.7 l_autonomy=0.5 l_recovery=0.3",
     "#SLOT RND1 s_wt1fine 0.15", "#SLOT RND2 s_pmidx 0.35", "#SLOT NE_MEMORY s_pmfb 0.3", "#SLOT NE_SIGNAL mem_dens 0.3"]) +
  preset("THIN AIR", "DRY", "A sine and a triangle, slowly, and nothing else at all. The quiet tone.",
    ["drone_root=48 drone_chord=2 m_o1wave=0 m_o2wave=1 m_o2lvl=0.35 m_o2fine=0.505 m_sub=0 m_gain=0.5 m_fmode=1 m_cut=0.5 m_a_atk=0.8 m_a_rel=0.85 m_drift=0.1 m_drifttime=0.8 m_send=0 e_rv_mix=0 e_distance=0"]) +
  preset("THE LAST GENERATOR", "INDUSTRIAL PULSE", "A square gate over a machine table and a sub, a tape delay behind, the loop feeding the structure.",
    ["drone_root=33 drone_chord=0 m_o1wave=3 m_o1pw=0.6 m_o2wave=2 m_o2oct=1 m_o2lvl=0.5 m_sub=1 m_sublvl=0.6 m_gain=0.45 m_fmode=0 m_cut=0.34 m_res=0.4 m_fdrive=0.5 m_a_atk=0.1 m_a_rel=0.45",
     "s_on=1 s_gain=0.3 s_wt1tab=1 s_wt1pos=0.7 s_wt1oct=2 s_fmode=1 s_cut=0.45 s_res=0.3 s_a_atk=0.1 s_a_rel=0.45",
     "st_on=1 st_gain=0.35 st_model=2 st_exc=6 st_exclvl=0.6 st_damp=0.5 st_material=0.8 st_pitch=0.6 st_send=0.3",
     "e_a1=5 e_dl_time=0.75 e_dl_fb=0.45 e_dl_mode=0 e_dl_mod=0.2 e_dl_mix=0.3 e_a2=3 e_mb_mid=0.3 e_mb_high=0.2 e_rv_mix=0.25 e_rv_decay=0.5 e_scale=0.45 e_fb_send=0.12 e_fb_ret=0.5 e_fb_delay=0.62 e_fb_to=1 e_fb_damp=0.5",
     "l1_wave=3 l1_sync=6 l2_wave=3 l2_sync=7",
     "#SLOT LFO1 m_gain 0.5", "#SLOT LFO1 s_gain 0.5", "#SLOT LFO2 m_cut 0.15"]);

const end = "  \"l1_wave=0 l1_rate=0.55\\n#SLOT LFO1 e_sh_hz 0.006\\n\" },\n};";
if (s.split(end).length !== 2) { console.log("END ANCHOR MISS"); process.exit(1); }
s = s.replace(end, "  \"l1_wave=0 l1_rate=0.55\\n#SLOT LFO1 e_sh_hz 0.006\\n\" },\n" + more + "};");
// POPULATION: ZERO a little less silent
const pz = "m_sub=1 m_sublvl=0.25 m_gain=0.3 m_width=0.7 m_fmode=1 m_cut=0.28";
if (s.split(pz).length !== 2) { console.log("PZ ANCHOR MISS"); process.exit(1); }
s = s.replace(pz, "m_sub=1 m_sublvl=0.3 m_gain=0.42 m_width=0.7 m_fmode=1 m_cut=0.3");
fs.writeFileSync(path, s);
console.log("presets appended");
