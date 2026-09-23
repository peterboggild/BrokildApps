
"use strict";

/* ══════════════════════════════════════════════════════════════════════════
   0 · the bridge (PROTOCOL.md). Sends are batched into one emitEvent per
   microtask; consecutive "p" writes to the same id coalesce, so a drag is
   ONE message however many pointermove events it took.
   ══════════════════════════════════════════════════════════════════════════ */
var NB = (function () {
  var q = [], sched = false;
  function flush() {
    sched = false; var b = q; q = [];
    if (!b.length) return;
    try { window.__JUCE__.backend.emitEvent("tty", b.length === 1 ? b[0] : { b: b }); } catch (e) {}
  }
  return {
    send: function (m) {
      if (m && m.k === "p") {
        for (var i = q.length - 1; i >= 0; i--) {
          if (q[i].k === "p" && q[i].id === m.id) { q[i].v = m.v; return; }
          if (q[i].k !== "p") break;
        }
      }
      q.push(m);
      if (!sched) { sched = true; (window.queueMicrotask || function (f) { Promise.resolve().then(f); })(flush); }
    },
    on: function (n, f) { try { window.__JUCE__.backend.addEventListener(n, f); } catch (e) {} },
    live: function () { return !!(window.__JUCE__ && window.__JUCE__.backend); }
  };
})();

var $ = function (s) { return document.querySelector(s); };
function el(t, c, p, txt) {
  var e = document.createElement(t);
  if (c) e.className = c;
  if (txt !== undefined) e.textContent = txt;
  if (p) p.appendChild(e);
  return e;
}
function clamp(v, a, b) { return v < a ? a : (v > b ? b : v); }
function clamp01(v) { return clamp(v, 0, 1); }
function xmap(t, lo, hi) { return lo * Math.pow(hi / lo, clamp01(t)); }
function sgn(n) { return (n > 0 ? "+" : "") + n; }

NB.send({ k: "hello" });     /* PROTOCOL: before anything else, the rack included */

/*  BROKILD WORLD FX: the shared rack overlay. The fragment owns bwfx_macro1..5
    and its own state; this page gives it one button and a pipe. */
if (window.BWFX) {
  BWFX.attach({ send: function (o) { var m = { k: "bwfx" }; for (var kk in o) m[kk] = o[kk]; NB.send(m); } });
  NB.on("bwfx", function (p) { try { BWFX.onState(p); } catch (e) { window.__TTYERR.push("bwfx: " + e); } });
}

/* ══════════════════════════════════════════════════════════════════════════
   1 · the model. Everything here arrives in initialState; nothing about the
   parameter list is written into this page.
   ══════════════════════════════════════════════════════════════════════════ */
var V = {}, DEF = {}, HI = {}, KIND = {}, LO = {}, FHI = {}, FLAG = {}, NAMES = {}, PNAME = {};
var IDX = {}, ORDER = [];            /* parameter order, for the eff array */
var CTL = {};                        /* id -> the one control element */
var EFFV = [], EFFP = [];            /* effective values, and what was drawn */
var PRESETS = [], SOURCES = ["—"], MACN = [];
var BUILD = "—", SR = 48000, LATENCY = 0, MEMLAT = 0;
var MAPS = [[], [], [], [], [], [], [], []];
var SLOTS = [], SHAPES = [], SCENES = { set: [0, 0, 0, 0], names: ["", "", "", ""] };
var TREE = { folder: "", exists: false, items: [] };
var READY = false;

var M = { out: 0, peak: 0, lim: 0, loop: 0, space: 0, strata: [0, 0, 0, 0],
          notes: [-1,-1,-1,-1,-1,-1,-1,-1], levels: [0,0,0,0,0,0,0,0], held: [],
          history: 0, grains: 0, memAct: 0, erosion: 0, voices: 0,
          spectrum: [], scope: [],
          mods: { lfo: [], env: [], shape: [], rnd: [], fol: [], evt: [], net: { e: [], b: [], t: [] } },
          fracture: false, capturing: false, capLen: 0, hasCapture: false,
          hasImport: false, importName: "", stopped: false, bend: 0, wheel: 0, at: 0 };
var HAVEMETER = false;
var DRAWS = { rec: 0, scope: 0, meter: 0, eff: 0 };

var K = { PCT:0, SW:1, HZ:2, MS:3, LIST:4, SEMI:5, CENT:6, CENTU:7, BIPOL:8, GLIDE:9,
          VOL:10, PW:11, INT:12, SEC:13, SHZ:14, DB:15, NOTE:16 };
var NOTEN = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];

/* ---- the formatting laws, straight out of PROTOCOL.md. ONE place: every
   readout, every tooltip and the notice line all come through here. ------- */
function hzTxt(x) {
  if (!isFinite(x)) return "—";
  if (x < 1) return x.toFixed(3) + " Hz";
  if (x < 100) return x.toFixed(2) + " Hz";
  if (x < 1000) return Math.round(x) + " Hz";
  return (x / 1000).toFixed(2) + " kHz";
}
function msTxt(x) {
  if (!isFinite(x)) return "—";
  if (x < 100) return x.toFixed(1) + " ms";
  if (x < 1000) return Math.round(x) + " ms";
  return (x / 1000).toFixed(2) + " s";
}
function secTxt(x) {
  if (!isFinite(x)) return "—";
  if (x < 60) return x.toFixed(2) + " s";
  var m = Math.floor(x / 60), s = Math.round(x - 60 * m);
  if (s === 60) { s = 0; m++; }
  return m + " m " + s + " s";
}
function noteTxt(n) {
  n = Math.round(n);
  return NOTEN[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);
}
function fmt(id, v) {
  var k = KIND[id], lo = LO[id], fhi = FHI[id];
  if (k === undefined) return String(v);
  switch (k) {
    case K.PCT:   return Math.round(v * 100) + " %";
    case K.SW:    return v >= 0.5 ? "ON" : "OFF";
    case K.HZ:    return hzTxt(xmap(v, lo, fhi));
    case K.MS:    return msTxt(xmap(v, lo, fhi));
    case K.LIST:  { var n = NAMES[id] || [], i = Math.round(v);
                    return n[i] !== undefined ? n[i] : String(i); }
    case K.SEMI:  { var s = (v - 0.5) * lo; return (s > 0 ? "+" : "") + s.toFixed(1) + " st"; }
    case K.CENT:  return sgn(Math.round((v - 0.5) * lo)) + " c";
    case K.CENTU: return Math.round(v * lo) + " c";
    case K.BIPOL: return sgn(Math.round((v - 0.5) * 200)) + " %";
    case K.GLIDE: return v < 0.01 ? "OFF" : msTxt(xmap(v, lo, fhi));
    case K.VOL:   { var g = 2 * v * v;
                    return g <= 1e-9 ? "-INF dB" : (20 * Math.log10(g)).toFixed(1) + " dB"; }
    case K.PW:    return Math.round(50 + 45 * v) + " %";
    case K.INT:   return String(Math.round(v));
    case K.SEC:   return secTxt(xmap(v, lo, fhi));
    case K.SHZ:   { var x = 2 * v - 1, y = (x < 0 ? -1 : 1) * x * x * x * lo;
                    return (y > 0 ? "+" : "") + y.toFixed(1) + " Hz"; }
    case K.DB:    { var d = lo + v * (fhi - lo); return (d > 0 ? "+" : "") + d.toFixed(1) + " dB"; }
    case K.NOTE:  return noteTxt(v);
  }
  return String(v);
}
function isStepped(id) { var k = KIND[id];
  return k === K.LIST || k === K.INT || k === K.NOTE || k === K.SW; }
function maxOf(id) { var h = HI[id]; return (h === undefined || h <= 0) ? 1 : h; }
function toT(id, v) { return clamp01(v / maxOf(id)); }
function fromT(id, t) { var v = clamp01(t) * maxOf(id); return isStepped(id) ? Math.round(v) : v; }
function effOf(id) { var i = IDX[id]; return (i === undefined) ? undefined : EFFV[i]; }
function modulatable(id) { return !(FLAG[id] & 4) && KIND[id] !== K.SW && KIND[id] !== K.LIST; }

/* ══════════════════════════════════════════════════════════════════════════
   2 · hints. Every control has one, in plain language: what it is and how it
   is used. Families (channel strips, LFOs, envelopes, oscillators) are
   written once and the number is substituted, so nothing falls back to a
   generic line — the probe fails if anything does.
   ══════════════════════════════════════════════════════════════════════════ */
var HINT = {
volume:"The final output level, after the limiter. Set the instrument's place in a mix here, not on the strata.",
ceiling:"The limiter's ceiling. Everything is held under this; lower it if the loudest scene is clipping downstream.",
quality:"How hard the engine works. LIVE keeps the load low for playing, STUDIO is the everyday setting, RENDER is the best it can do for a bounce.",
bassmono:"Everything below this frequency is folded to the centre. The low end of a wide drone stops wandering between the speakers.",
bassmono_on:"Switches the bass-mono fold on. Leave it on unless you are deliberately working in a very wide low end.",
outwidth:"The stereo width of the finished output. Below 100 % the image narrows; at 0 the output is mono.",
vmode:"How keys are allocated. POLY plays chords, MONO is one voice retriggered, LEGATO is one voice that only glides.",
voices:"How many voices may sound at once. Fewer voices costs less and makes a dense patch behave more like one instrument.",
glide:"How long a voice takes to slide to a new note. OFF is an instant jump; long settings turn every move into a slow bend.",
tune:"Transposes the whole instrument in semitones. Use it to sit with a track without moving the drone root.",
fine:"Fine tuning in cents. A few cents against a recording is the difference between beating and blending.",
bend:"How far the pitch wheel reaches, in semitones. Two is the usual; set it wide for a collapse.",
scale:"Quantises played and drone notes to a scale. SCALA FILE uses the .scl you imported. FREE does not quantise at all.",
rootlock:"Keeps the drone root inside the chosen scale when it is moved. Switch it off to place the root anywhere.",
mpe:"Accepts per-note pressure, slide and pitch from an MPE controller. Leave it off for an ordinary keyboard.",
drone:"The explicit drone switch. On, the instrument sounds without a note being held; off, it waits for the keyboard.",
drone_root:"The note the drone is built on, as a name. Everything harmonic in the patch is measured from here.",
drone_chord:"What the drone holds above the root, from a bare unison to an open cluster. A wider chord fills more of the spectrum.",
drone_latch:"Holds the drone once it starts, so it keeps sounding without a key. Off, it follows what you play.",
drone_spread:"How far the drone's notes are spread across the stereo field. Up is wide and architectural, down is a single point.",
drone_vel:"How hard the drone strikes and holds the strata. It is the drone's velocity: low is a breath, high is a blow.",
ext_mode:"What the AUX input does. EXCITE feeds it into the strata as an exciter, DUCK makes the instrument step back for it, or both.",
ext_gain:"Trims the AUX input before it is used. Set it so the excitation is strong without pinning the strata.",
duck_amt:"How far the instrument steps back when the AUX input is present. This is how it sits under a voice.",
duck_rel:"How long the duck takes to let go once the AUX input stops. Longer is smoother and less audible as a pump.",
seed:"The number every random decision in the patch grows from. The same seed is the same instrument, always.",
determin:"Makes the random sources restart from the seed at every note and transport start, so a bounce is repeatable.",
patch:"Which factory patch is on the dial. The name and its note are shown beside it.",

m_sync:"Restarts oscillator 2 on every cycle of oscillator 1. With the two detuned this is the hard, tearing sync sound.",
m_beat:"Sets the beat between the two oscillators directly in hertz instead of in cents. Slow beats are the whole point of a drone.",
m_unison:"How many copies of MASS are stacked per voice. Two or three make it very wide and very heavy at a real cost in voices.",
m_unidet:"How far the unison copies are detuned from one another, in cents. Small is thickness, large is a chorus.",
m_sub:"Adds an octave or two-octave sub beneath MASS. This is where physical weight in a room comes from.",
m_sublvl:"How loud the sub is. Watch the output meter: the sub is the part you feel before you hear it.",
m_subwave:"The sub's wave. SINE is pure weight, SQUARE brings harmonics up with it and cuts through more.",
m_reinf:"Feeds MASS back into itself a little, reinforcing its own low end. A small amount makes it lean forward.",
m_drift:"How far the oscillators wander out of tune on their own, in cents. Nothing in this instrument is meant to be perfectly stable.",
m_drifttime:"How long the drift takes to move from one place to another. Long settings are geological; short ones are nervous.",
m_pwdrift:"How far the pulse width wanders on its own. A slow width drift is a shimmer no LFO can imitate.",
m_ampvar:"How much the level of each oscillator wanders. It stops a held drone from sounding like a sample.",
m_fmode:"The filter circuit. LADDER is the warm four-pole; the SVF modes are cleaner and give you band, high and notch.",
m_cut:"The filter cut-off: the brightness of MASS. The filter envelope and every modulation move out from here.",
m_res:"Resonance at the cut-off. A little lifts the corner; high settings whistle and thin the body out.",
m_fdrive:"How hard the signal is pushed into the filter. It saturates and thickens before it distorts.",
m_fenv:"How far the filter envelope moves the cut-off, and in which direction. Negative closes the filter as the note starts.",
m_ftrack:"How much the cut-off follows the note played. Full tracking keeps the timbre constant up the keyboard.",

s_pmratio:"The frequency of the modulator against the carrier. Whole numbers stay harmonic; the odd ratios go metallic. FIXED uses the hertz below.",
s_pmfixed:"The modulator's frequency when the ratio is FIXED. It does not follow the note, so low notes get a formant rather than a timbre.",
s_pmidx:"How deeply the modulator bends the carrier's phase. Up from nothing it goes hollow, then bright, then noisy.",
s_pmfb:"Feeds the carrier back into its own phase. Small amounts add a saw-like edge; large amounts break into noise.",
s_ring:"Multiplies table 1 by table 2. The result has neither one's pitch: it is the sum and difference of everything in both.",
s_addlvl:"How loud the 64-partial additive bank is against the tables. It is a whole second oscillator in here.",
s_addn:"How many partials the bank uses. Few partials is a thin, glassy tone; all 64 is a full spectrum you can carve.",
s_addspread:"Stretches or squeezes the partials away from the harmonic series. Off-harmonic spacing is what makes it sound inhuman.",
s_addodd:"Tilts the balance between odd and even partials. Odd only is hollow and clarinet-like; even only is bright and open.",
s_addtilt:"Tilts the level of the partials from low to high. Down is dark and heavy, up is thin and present.",
s_addcluster:"Pulls the partials together into clusters instead of spreading them evenly. Clusters beat against each other.",
s_addgaps:"Silences partials in a pattern, leaving holes in the spectrum. A comb you can hear through rather than a filter.",
s_addmotion:"How much the partials drift in level and frequency on their own. It keeps the bank from sounding frozen.",
s_addfund:"The level of the fundamental itself. Pull it down and the ear hears the partials as the note instead.",
s_shift:"Shifts every frequency by the same number of hertz, up or down. Harmonic relationships are destroyed, which is the point.",
s_pshift:"Transposes SIGNAL in semitones while keeping its formants. Use it against MASS to get a second register.",
s_interf:"How much the two tables interfere with one another rather than simply mixing. The result is a third thing neither of them contains.",
s_fmode:"SIGNAL's filter circuit. OFF leaves the spectrum alone; the SVF modes let you take out a band or keep only one.",
s_cut:"SIGNAL's filter cut-off. Most of what makes SIGNAL sound artificial lives above 2 kHz; this is where you let it in.",
s_res:"Resonance at SIGNAL's cut-off. It makes the filter itself audible as a pitch inside the spectrum.",
s_fenv:"How far SIGNAL's filter envelope moves its cut-off, and which way.",

mem_src:"What MEMORY reads from: procedural material, the 30-second capture, or an imported file. CAPTURE and IMPORT are your own world.",
mem_mode:"Whether MEMORY reads with grains, with a spectral resynthesis, or both at once. GRAINS keeps transients, SPECTRAL smooths them into air.",
mem_pos:"Where in the material the reader sits. Everything else here happens around this point.",
mem_scan:"How fast the position moves on its own, and in which direction. Slow negative scanning is the sound of remembering backwards.",
mem_region:"How much material either side of the position is in play. A narrow region loops on a single moment.",
mem_pitch:"Transposes MEMORY in semitones without moving the reader. Down two octaves is where a recording stops being a recording.",
mem_pfine:"Fine tuning for MEMORY in cents, so it can sit with the other strata or beat against them.",
mem_keyfollow:"Lets the keyboard transpose MEMORY. Off, MEMORY stays where it is however you play, which is often what you want.",
mem_dur:"How long each grain is. Short grains are a texture, long ones are recognisable fragments of the source.",
mem_dens:"How many grains are started per second. Low is a scatter of events, high is a continuous cloud.",
mem_win:"The shape each grain fades in and out with. HANN is smooth, the exponential windows are percussive, SOFT RECT is the roughest.",
mem_jit:"How irregularly the grains are placed in time. Without jitter a dense cloud develops an audible pitch of its own.",
mem_pspread:"How far grains are detuned from one another, in cents. Wide spread turns a fragment into a chord.",
mem_scatter:"How far grains are scattered around the reading position. It blurs the moment being remembered.",
mem_rev:"What proportion of grains play backwards. Reversed grains have no attack, so the cloud loses its edges.",
mem_sched:"How grains are timed. CLOCKED is even, IRREGULAR is human, CLUSTERED arrives in bursts with silence between.",
mem_freeze:"Holds the spectral frame where it is. The sound stops moving forward and becomes a standing texture.",
mem_smear:"Blurs the spectral frames into one another over time. Transients disappear and only the air is left.",
mem_stilt:"Tilts MEMORY's spectrum, dark to bright. It is how far away the memory is more than how bright it is.",
mem_thin:"Removes the quietest partials from each spectral frame. What is left is a sketch of the original.",
mem_disp:"Displaces the whole spectrum by a fixed number of hertz. A voice treated this way stops being a voice.",
mem_evolve:"How much the spectral resynthesis is allowed to develop away from what it read. At zero it is faithful.",
mem_erosion:"The single control over decay. Up, MEMORY loses bandwidth, drops out, fragments and simplifies, together.",
mem_ero_bw:"How much of EROSION goes into losing bandwidth. High settings make the memory dull before anything else happens.",
mem_ero_drop:"How much of EROSION goes into dropouts. The material goes missing in pieces, like damaged tape.",
mem_ero_frag:"How much of EROSION goes into fragmentation. Continuous material breaks into disconnected shards.",
mem_ero_simp:"How much of EROSION goes into simplification. Detail is thrown away until only the gesture remains.",
mem_capsrc:"Where the 30-second capture ring records from: before the loop, after the space, or the AUX input.",
mem_atk:"How long MEMORY takes to come up when it is triggered. Long attacks make it arrive rather than start.",
mem_rel:"How long MEMORY takes to fade once released. Long releases leave a memory hanging over the next event.",

st_model:"Which resonant body is being played. PLATE and GLASS ring, CABLE and BEAM are pitched, CAVITY and TUBE are air, COMB is a grid of them.",
st_pitch:"Transposes the body in semitones. A body has a natural size; moving it far is how you get something impossible.",
st_fine:"Fine tuning for the body in cents, for beating against MASS or settling with it.",
st_keyfollow:"Lets the keyboard set the body's pitch. Off, the structure has its own fixed size whatever you play.",
st_stiff:"How stiff the material is. Stiffness pushes the partials away from a harmonic series, towards bell and bar.",
st_damp:"How quickly the body's energy is lost. Low damping rings for a long time; high damping is a knock in a dead room.",
st_dens:"How many modes the body has. Few modes is a clear pitch, many is a wash you can still hear a note inside.",
st_expos:"Where the body is struck or driven. Away from the centre, more modes are excited and it sounds harder.",
st_pickup:"Where the body is listened to. Moving it changes which modes you hear without changing the body at all.",
st_material:"The character of the material, from soft and lossy to hard and metallic. It moves the damping of the high modes most.",
st_couple:"How much the modes feed each other. Coupled modes exchange energy, so the tone keeps changing as it rings.",
st_drive:"How hard the body is driven. Past a point it stops being linear and starts to rattle and buzz.",
st_stress:"How much load the structure is under. Up, it detunes, then groans, then fractures audibly.",
st_exc:"What excites the body. IMPULSE and NOISE BURST are strikes, FRICTION is a bow, and the rest feed it another stratum.",
st_exclvl:"How strongly the exciter drives the body. This is the loudness of the gesture, not of the result.",
st_sustain:"How much energy is fed in continuously rather than as a strike. It is what turns a struck body into a sustaining one.",
st_bowforce:"How hard the friction exciter presses. Too little and it will not catch; too much and it scrapes instead of singing.",
st_bowvel:"How fast the friction exciter moves. Slow and heavy is a groan, fast and light is a whistle.",
st_strikeon:"Strikes the body whenever a note starts. Switch it off if you only want STRIKE and the event lanes to hit it.",
st_strikelvl:"How hard STRIKE hits. The STRIKE button and the event lanes both use this.",
st_atk:"How long STRUCTURE takes to come up when it is triggered.",
st_rel:"How long STRUCTURE takes to fade once released. The body's own damping still decides how it rings.",

e_sat_drive:"How hard the signal is pushed into the valve. Low is thickening, high is the sound of something overloaded.",
e_sat_asym:"How lopsided the saturation is. Symmetric gives odd harmonics only; asymmetry brings the even ones and a second flavour.",
e_sat_tone:"Tilts the tone inside the saturator, so it distorts the dark or the bright end of the signal.",
e_sat_mix:"How much of the saturated signal is kept against the clean one.",
e_fold_amt:"How far the waveform is folded back on itself. It adds harmonics that have nothing to do with the original spectrum.",
e_fold_sym:"How symmetrical the fold is. Off centre, the fold is different on each half of the wave and the result is harsher.",
e_fold_mix:"How much of the folded signal is kept against the clean one.",
e_mb_lo:"Where the low band ends. Set it under the material you want to keep clean.",
e_mb_hi:"Where the high band begins. Above it you are working on air and detail rather than body.",
e_mb_low:"How hard the low band is driven. A little here is weight; a lot is a low end that will not sit still.",
e_mb_mid:"How hard the middle band is driven. This is where the sound's identity is, so it changes most here.",
e_mb_high:"How hard the high band is driven. It buys presence and grit without touching the low end.",
e_mb_mix:"How much of the three driven bands are kept against the clean signal.",
e_sh_hz:"Shifts every frequency by the same number of hertz. Small amounts beat; large amounts destroy every harmonic relationship.",
e_sh_mix:"How much of the shifted signal is kept. At 50 % the original and the shifted copy interfere with each other.",
e_sh_fb:"Feeds the shifted output back in, so it is shifted again and again into a rising or falling staircase.",
e_dl_time:"How long the delay is. Short times are a resonance, long ones are a memory of the last phrase.",
e_dl_fb:"How much of the delay returns to its own input. Near the top it never quite decays.",
e_dl_mod:"How much the delay time wanders. This is tape wow: it detunes every repeat a little differently.",
e_dl_rate:"How fast the delay time wanders. Slow is a tape machine; fast is a vibrato on the repeats.",
e_dl_mode:"TAPE loses high end and saturates a little on every pass. CLEAN repeats what it was given.",
e_dl_tone:"Tilts the tone of the repeats. Darker repeats recede into the distance; brighter ones stay in the room.",
e_dl_mix:"How much delay is heard against the dry signal.",
e_cb_pitch:"The pitch of the comb, in semitones. A comb is a very short delay, so its length is a note.",
e_cb_fb:"How much the comb feeds itself. Up, it rings on a pitch of its own whatever you put into it.",
e_cb_damp:"How quickly the comb's high end is lost as it rings. It is the difference between metal and wood.",
e_cb_diff:"How much the comb's reflections are spread out. Diffusion turns a clear pitch into a resonant cloud.",
e_cb_mix:"How much of the comb is heard against the dry signal.",
e_cr_bits:"How far the signal is reduced in bit depth. The quantisation noise follows the signal, so quiet parts suffer most.",
e_cr_rate:"The sample rate the signal is reduced to. Lower rates fold the high end back down into the audible band.",
e_cr_aa:"Filters before and after the rate reduction, so you get the grit without the aliasing. Switch it off for the full damage.",
e_cr_mix:"How much of the crushed signal is kept against the clean one.",
e_rv_mix:"How much space is heard. This is amount only: the size of the place is set separately.",
e_rv_pre:"How long before the space answers. A long pre-delay puts the source in front of the room rather than inside it.",
e_rv_size:"The size of the place. It moves the early reflections and the network together, so the room changes shape.",
e_rv_decay:"How long the space takes to fall silent. Up to a minute, which stops being a room and becomes a condition.",
e_rv_diff:"How scattered the reflections are. High diffusion is a smooth tail; low leaves individual reflections audible.",
e_rv_damp:"How fast the high end is lost in the tail. It is the difference between tiled concrete and a ruin full of dust.",
e_rv_low:"Whether the low end rings for longer or shorter than the rest. Long low decay is the weight of a very large space.",
e_rv_mod:"How much the space moves. A little keeps the tail alive; a lot detunes it into something that is not a room.",
e_rv_freeze:"Holds the space's energy so it never decays. The tail becomes a standing chord you can play over.",
e_rv_early:"The balance between the early reflections and the late tail. Early is the walls, late is the volume of air.",
e_scale:"How large the place seems, independent of its decay. It moves the reflection pattern, which is what the ear reads as size.",
e_distance:"How far away the instrument seems. It trades direct sound for reflected, and dulls the high end with the air.",
e_fb_send:"How much of the mix is sent into the feedback loop. This is the loop's input, before anything in it.",
e_fb_ret:"How much of the loop comes back. Together with SEND this decides whether the loop decays or grows.",
e_fb_delay:"How long the loop takes to come round. Short loops ring on a pitch; long ones build a slow accumulation.",
e_fb_hp:"Removes low end inside the loop. Without this the loop accumulates weight until nothing else is audible.",
e_fb_lp:"Removes high end inside the loop. It is what keeps the accumulation from turning into hiss.",
e_fb_shift:"Shifts the loop's frequencies a little on every pass, so the accumulation climbs or falls instead of ringing.",
e_fb_sat:"Saturates inside the loop. It limits the growth by itself, so the loop can be run near unity without exploding.",
e_fb_damp:"Damps the loop's energy. The plain safety control: turn it up if the loop is running away from you.",
e_fb_to:"Where the loop returns. Into MIX it is an echo; into a stratum it becomes an exciter and the instrument plays itself.",

l_autonomy:"How much the LIFE network acts on its own rather than waiting to be modulated. Up, the instrument has opinions.",
l_coupling:"How strongly the detectors feed the sources. Coupling is what makes one stratum answer another.",
l_recovery:"How quickly the network returns to rest after it has been disturbed. Slow recovery leaves a long aftermath.",
l_smooth:"How fast the network is allowed to change. Long smoothing makes its decisions geological rather than nervous.",
l_hyst:"How far the network must be pushed before it changes its mind. Hysteresis stops it from flickering on a threshold.",
l_hold:"Freezes the LIFE network where it is. Everything keeps sounding but nothing develops. The companion to HOLD HISTORY.",

h_on:"Switches HISTORY on. Off, the scenes are still stored but the instrument stays where you set it.",
h_pos:"Where you are between the scenes. This is the one control the whole piece travels on.",
h_hold:"Freezes HISTORY where it is, even in AUTO. Use it to sit inside a moment for as long as you like.",
h_mode:"Who moves HISTORY. MANUAL is you, AUTO runs on its own clock, AUTO SYNC runs on the host's bars.",
h_dur:"How long AUTO takes to travel the whole way, up to thirty minutes.",
h_bars:"How many host bars AUTO SYNC takes to travel the whole way.",
h_loop:"What happens at the end. ONCE stops, LOOP begins again, PING-PONG turns round and travels back.",
h_sc2pos:"Where scene 2 sits along the travel. Move it to spend longer in the first part of the piece.",
h_sc3pos:"Where scene 3 sits along the travel. Together with scene 2 it sets the shape of the whole arc.",

mac_mass:"Weight and physical presence. It pushes the low end, the sub and the body of everything forward.",
mac_dread:"Suspense and unresolved tension: the sound leans somewhere without arriving.",
mac_violence:"Abrasion and force. Drive, stress and saturation together.",
mac_instab:"Loss of equilibrium. Drift, wander and the network's autonomy rise together.",
mac_contam:"Layers invading one another. The strata stop being separate and start to excite each other.",
mac_distance:"From near presence to remote trace: space, damping and the loss of detail.",
mac_life:"Environmental activity. How much the instrument moves when you are not touching it.",
mac_humanity:"Recognisable vulnerability. What is left in the sound that a person could have made."
};

/*  the families, written once ------------------------------------------- */
var STRN = { m:"MASS", s:"SIGNAL", mem:"MEMORY", st:"STRUCTURE" };
var CHH = {
on:"Switches $ on. Off, the stratum costs nothing and is silent.",
gain:"How loud $ is in the mix. The four strata are balanced here, before the space and the loop.",
pan:"Where $ sits between the speakers.",
width:"How wide $ is. Narrow keeps it as one object; wide spreads its voices across the field.",
hp:"Removes low end from $ before the mix. Use it to keep the four strata out of each other's way.",
lp:"Removes high end from $ before the mix. It is how you push a stratum behind the others.",
send:"How much of $ is sent to the SPACE.",
loop:"How much of $ is sent into the FEEDBACK LOOP.",
mute:"Silences $ without changing anything else. Not stored in a scene.",
solo:"Hears $ alone. Not stored in a scene.",
drone:"Whether $ sounds when DRONE is on.",
keys:"Whether $ sounds when you play the keyboard."
};
var ADSRH = {
atk:"How long $ takes to reach its top after a note starts.",
dec:"How long $ takes to fall from its top to its sustain level.",
sus:"The level $ holds at while the note is held.",
rel:"How long $ takes to fall to silence once the note is let go."
};
function hintFor(id) {
  if (HINT[id]) return HINT[id];
  var m;
  /* channel strips */
  if ((m = /^(m|s|mem|st)_(on|gain|pan|width|hp|lp|send|loop|mute|solo|drone|keys)$/.exec(id)))
    return CHH[m[2]].replace("$", STRN[m[1]]);
  /* the two stratum ADSRs */
  if ((m = /^(m|s)_(f|a)_(atk|dec|sus|rel)$/.exec(id)))
    return ADSRH[m[3]].replace("$", STRN[m[1]] + (m[2] === "f" ? "'s filter envelope" : "'s amplitude envelope"));
  /* MASS oscillators */
  if ((m = /^m_o([12])(wave|oct|semi|fine|pw|lvl)$/.exec(id))) {
    var o = "MASS oscillator " + m[1];
    return { wave:"The wave " + o + " produces. SINE is weight, TRIANGLE is soft, SAW is bright and full, PULSE is hollow.",
             oct:"The octave " + o + " plays in, from two below to two above.",
             semi:"Transposes " + o + " in semitones. A fifth between the two oscillators is the classic stacked drone.",
             fine:"Detunes " + o + " in cents. A few cents against the other oscillator is what makes one big sound.",
             pw:"The width of " + o + "'s pulse wave, from square and hollow to a thin reedy sliver. Only heard on PULSE.",
             lvl:"How much " + o + " contributes." }[m[2]];
  }
  /* SIGNAL wavetables */
  if ((m = /^s_wt([12])(tab|pos|oct|semi|fine|lvl)$/.exec(id))) {
    var t = "table " + m[1];
    return { tab:"Which wavetable " + t + " reads. Each one is a different family of machine spectra.",
             pos:"Where in " + t + " the oscillator reads. Sweeping this is the wavetable's own kind of movement.",
             oct:"The octave " + t + " plays in, from two below to two above.",
             semi:"Transposes " + t + " in semitones.",
             fine:"Detunes " + t + " in cents, for beating against the other table.",
             lvl:"How much " + t + " contributes." }[m[2]];
  }
  /* LFOs */
  if ((m = /^l([1-8])_(wave|rate|sync|phase|scope)$/.exec(id))) {
    var n = m[1];
    return { wave:"The shape LFO " + n + " runs through. The two random shapes are the useful ones for a drone.",
             rate:"How fast LFO " + n + " runs, from once every twenty minutes to audio rate.",
             sync:"Locks LFO " + n + " to the host's bars instead of its own rate. FREE ignores the host.",
             phase:"Where LFO " + n + " starts. Offsetting several LFOs is how you stop them arriving together.",
             scope:"Whether LFO " + n + " is one global shape or a separate one per voice." }[m[2]];
  }
  /* envelopes */
  if ((m = /^e([1-4])_(atk|dec|sus|rel|trig)$/.exec(id))) {
    if (m[2] === "trig") return "What starts envelope " + m[1] + ": a note, the drone beginning, or an event lane.";
    return ADSRH[m[2]].replace("$", "envelope " + m[1]);
  }
  /* random sources */
  if ((m = /^r([1-4])_(type|rate|amt|restore|smooth)$/.exec(id))) {
    var r = "random source " + m[1];
    return { type:"What kind of randomness " + r + " makes: a slow wander, a stepped hold, a burst, or a bounded chaotic trajectory. The names are on the control.",
             rate:"How often " + r + " produces a new value.",
             amt:"How far " + r + " is allowed to stray from the centre.",
             restore:"How strongly " + r + " is pulled back to the centre. At zero a walk never comes home.",
             smooth:"How much " + r + " is smoothed before it is used. Smoothing turns steps into slopes." }[m[2]];
  }
  /* followers */
  if ((m = /^f([12])_(src|atk|rel)$/.exec(id))) {
    var f = "follower " + m[1];
    return { src:"What " + f + " listens to. It turns that signal's loudness into a modulation source.",
             atk:"How quickly " + f + " rises when what it is listening to gets louder.",
             rel:"How slowly " + f + " falls when what it is listening to gets quieter." }[m[2]];
  }
  /* event lanes */
  if ((m = /^v([1-4])_(prob|rate|target|refract|amt)$/.exec(id))) {
    var v = "event lane " + m[1];
    return { prob:"How likely " + v + " is to fire when its moment comes. At zero the lane is silent.",
             rate:"How often " + v + " considers firing.",
             target:"What " + v + " does when it fires, from striking the structure to nudging the scene.",
             refract:"How long " + v + " must wait after firing before it can fire again.",
             amt:"How strong " + v + "'s action is when it fires." }[m[2]];
  }
  /* the two insert lanes */
  if ((m = /^e_([ab])([123])$/.exec(id)))
    return "Slot " + m[2] + " of insert lane " + m[1].toUpperCase() + ". Choose a processor, or leave it empty to pass through. " +
           "The three slots run in order.";
  return null;
}

/* ══════════════════════════════════════════════════════════════════════════
   3 · building the controls. The type comes from the KIND that arrived,
   never from a list written here. A control is created ONCE; the views move
   the same node between them, so no parameter ever has two controls.
   ══════════════════════════════════════════════════════════════════════════ */
var TRAVEL = 200;                  /* px of pointer for the whole range */
var FADERID = { volume: 1, m_gain: 1, s_gain: 1, mem_gain: 1, st_gain: 1 };
var HFID = { h_pos: 1 };

function typeOf(id, opt) {
  if (opt && opt.t) return opt.t;
  var k = KIND[id];
  if (k === K.SW) return "sw";
  if (k === K.LIST) return "sel";
  if (k === K.INT || k === K.NOTE) return "num";
  if (HFID[id]) return "hf";
  if (FADERID[id]) return "fader";
  return "knob";
}

function buildCtl(id, label, opt) {
  opt = opt || {};
  var t = typeOf(id, opt);
  var c = el("div", "ctl " + t + (opt.cls ? " " + opt.cls : ""));
  c.dataset.id = id;
  c.dataset.type = t;
  c.tabIndex = 0;
  var h = hintFor(id);
  if (!h) { h = (PNAME[id] || id) + " — no hint has been written for this control."; c.dataset.hintAuto = "1"; }
  c.setAttribute("data-hint", h);

  if (t === "knob") {
    var kn = el("div", "kn", c);
    var knr = el("div", "knr", kn); el("div", "ptr", knr);
    c._knr = knr; c._em = el("div", "em", kn);
  } else if (t === "fader") {
    var trk = el("div", "trk", c);
    el("div", "slot", trk);
    c._cap = el("div", "cap", trk);
    c._em = el("div", "em", trk);
    c._trk = trk;
  } else if (t === "hf") {
    var w = el("div", "hfw", c);
    el("div", "hslot", w);
    c._fill = el("div", "hfill", w);
    c._cap = el("div", "hcap", w);
    c._em = el("div", "em", w);
    c._hfw = w;
  } else if (t === "sw") {
    el("div", "pill", c);
    c._em = el("div", "emd", c);
  } else {                                   /* sel and num */
    c._em = el("div", "emd", c);
  }

  var lab = el("div", "lab", c); lab.textContent = label === undefined ? (PNAME[id] || id) : label;
  var val = el("div", "val", c);
  c._val = val;
  if (t === "hf") { c.appendChild(c._hfw); c.appendChild(val); c.appendChild(lab); }
  bindCtl(c, id, t);
  CTL[id] = c;
  return c;
}

/* ---- interaction ------------------------------------------------------- */
function bindCtl(c, id, t) {
  var down = false, p0 = 0, start = 0, moved = false;
  var horiz = (t === "hf");
  var toggles = (t === "sw");
  var cycles = (t === "sel");

  c.addEventListener("pointerdown", function (e) {
    if (e.button !== 0) return;
    down = true; moved = false;
    p0 = horiz ? e.clientX : e.clientY;
    start = V[id] || 0;
    c.classList.add("drag");
    try { c.setPointerCapture(e.pointerId); } catch (x) {}
    showTip(c, true);
    e.preventDefault(); e.stopPropagation();
  });
  c.addEventListener("pointermove", function (e) {
    if (!down) return;
    var now = horiz ? e.clientX : e.clientY;
    var d = (horiz ? (now - p0) : (p0 - now)) / DECK_S;
    if (Math.abs(now - p0) > 2) moved = true;
    if (toggles) return;
    setP(id, fromT(id, toT(id, start) + (e.shiftKey ? 0.12 : 1) * d / TRAVEL));
    showTip(c, true);
  });
  var up = function (e) {
    if (!down) return;
    down = false; c.classList.remove("drag");
    try { c.releasePointerCapture(e.pointerId); } catch (x) {}
    if (!moved) {
      if (toggles) setP(id, (V[id] >= 0.5) ? 0 : 1);
      else if (cycles) {
        if (e.shiftKey || (NAMES[id] || []).length > 6) openSelMenu(c, id);
        else setP(id, (Math.round(V[id] || 0) + 1) % (maxOf(id) + 1));
      }
    }
    if (!c.matches(":hover")) hideTip();
  };
  c.addEventListener("pointerup", up);
  c.addEventListener("pointercancel", up);

  c.addEventListener("wheel", function (e) {
    nudge(id, e.deltaY < 0 ? 1 : -1, e.shiftKey);
    showTip(c, true); e.preventDefault();
  }, { passive: false });

  /* double-click types a value; alt-click resets to the default */
  c.addEventListener("dblclick", function (e) { e.preventDefault(); openEntry(c, id); });
  c.addEventListener("click", function (e) {
    if (e.altKey && DEF[id] !== undefined) { setP(id, DEF[id]); showTip(c, true); }
  });
  c.addEventListener("keydown", function (e) {
    var kk = e.key;
    if (kk === "ArrowUp" || kk === "ArrowRight") { nudge(id, 1, e.shiftKey); e.preventDefault(); }
    else if (kk === "ArrowDown" || kk === "ArrowLeft") { nudge(id, -1, e.shiftKey); e.preventDefault(); }
    else if (kk === "Enter") { openEntry(c, id); e.preventDefault(); }
    else if (kk === "Home" && DEF[id] !== undefined) { setP(id, DEF[id]); e.preventDefault(); }
    else return;
    showTip(c, false);
  });
  c.addEventListener("focus", function () { showTip(c, false); });
  c.addEventListener("blur", function () { if (!c.classList.contains("drag")) hideTip(); });
  c.addEventListener("mouseenter", function (e) { HOVERC = c; CTRLHOVER = !!(e && e.ctrlKey); showTip(c, false); });
  c.addEventListener("mouseleave", function () { if (HOVERC === c) HOVERC = null; if (!c.classList.contains("drag")) hideTip(); });
}

function nudge(id, dir, fine) {
  if (isStepped(id)) setP(id, clamp(Math.round((V[id] || 0) + dir), 0, maxOf(id)));
  else setP(id, clamp01((V[id] || 0) + dir * (fine ? 0.002 : 0.02)));
}

/*  ONE place writes a value: store it, redraw everything that shows it, then
    tell the native side. Nothing is ever sent back from eff or hostParam. */
function setP(id, v) {
  if (KIND[id] === undefined) return;
  v = isStepped(id) ? clamp(Math.round(v), 0, maxOf(id)) : clamp01(v);
  if (V[id] === v) { refresh(id); return; }
  V[id] = v;
  refresh(id);
  NB.send({ k: "p", id: id, v: v });
  say((PNAME[id] || id) + "   " + fmt(id, v));
}

/*  Everything that displays a value redraws here. A control can be working
    and still look broken; this is the function that stops that. */
function refresh(id) {
  var c = CTL[id], v = V[id];
  if (c && v !== undefined) {
    var t = c.dataset.type, tt = toT(id, v);
    if (t === "knob") c._knr.style.transform = "rotate(" + (-140 + 280 * tt) + "deg)";
    else if (t === "fader") c._cap.style.bottom = (tt * 33) + "px";
    else if (t === "hf") {
      var w = c._hfw.clientWidth || 300;
      c._cap.style.left = (5 + tt * (w - 21)) + "px";
      c._fill.style.width = Math.max(0, tt * (w - 21) + 5) + "px";
    } else if (t === "sw") c.classList.toggle("on", v >= 0.5);
    c._val.textContent = fmt(id, v);
  }
  var d = DEPS[id]; if (d) d();
  if (tipFor && tipFor.dataset.id === id) showTip(tipFor, tipFor.classList.contains("drag"));
}

/*  Consumers that are not the control itself. Every one of these is a place a
    value is shown twice; the Photo-Synth Q-factor lesson, in a table. */
var DEPS = {
  drone:      function () { syncDrone(); },
  drone_root: function () { syncDrone(); },
  h_pos:      function () { syncHist(); },
  h_mode:     function () { syncHist(); },
  h_on:       function () { syncHist(); },
  h_sc2pos:   function () { syncHist(); },
  h_sc3pos:   function () { syncHist(); },
  mem_src:    function () { syncMem(); },
  quality:    function () { /* the header selector is the control itself */ },
  patch:      function () { syncPatch(); },
  st_exc:     function () { syncStruct(); },
  scale:      function () { syncScale(); }
};
function refreshAll() { Object.keys(CTL).forEach(refresh); drawEff(true); }

/* ---- the tooltip: beside the control, never over it -------------------- */
/*  Tooltips are OFF on a fresh instance: they sit beside a control, and with
    the deck this dense that is often over the NEXT control. Holding CTRL shows
    one without switching them on; the switch itself is remembered per machine. */
var tipEl = null, tipFor = null, HOVERC = null, CTRLHOVER = false;
var HINTS_ON = (function () {
  try { return localStorage.getItem("tty.tooltips") === "1"; } catch (e) { return false; }
})();
function setTooltips(on) {
  HINTS_ON = !!on;
  try { localStorage.setItem("tty.tooltips", HINTS_ON ? "1" : "0"); } catch (e) {}
  var b = $("#b-hints"); if (b) b.classList.toggle("on", HINTS_ON);
  if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
  else if (HINTS_ON && HOVERC) showTip(HOVERC, false);
}
function showTip(c, dragging) {
  if (!HINTS_ON && !CTRLHOVER && !dragging) { hideTip(); return; }
  if (!tipEl) tipEl = $("#tip");
  var id = c.dataset.id;
  tipFor = c;
  tipEl.textContent = "";
  el("div", "tn", tipEl, PNAME[id] || id);
  el("div", "tv", tipEl, fmt(id, V[id]));
  var ev = effOf(id);
  if (ev !== undefined && Math.abs(ev - (V[id] || 0)) > 0.002)
    el("div", "tv", tipEl, "NOW  " + fmt(id, ev)).style.color = "var(--cyan)";
  el("div", "tt", tipEl, c.getAttribute("data-hint") || "");
  if (!HINTS_ON) el("div", "tk", tipEl, "HOLD CTRL FOR THIS \u00b7 TOOLTIPS TURNS THEM ON");
  tipEl.classList.add("show");
  var r = c.getBoundingClientRect(), tr = tipEl.getBoundingClientRect();
  var x = r.right + 10, y = r.top + r.height / 2 - tr.height / 2;
  if (x + tr.width > innerWidth - 6) x = r.left - tr.width - 10;
  if (x < 6) {
    x = clamp(r.left + r.width / 2 - tr.width / 2, 6, Math.max(6, innerWidth - tr.width - 6));
    y = (r.top > innerHeight / 2) ? r.top - tr.height - 10 : r.bottom + 10;
  }
  tipEl.style.left = Math.round(clamp(x, 4, Math.max(4, innerWidth - tr.width - 4))) + "px";
  tipEl.style.top = Math.round(clamp(y, 4, Math.max(4, innerHeight - tr.height - 4))) + "px";
}
function hideTip() { if (tipEl) tipEl.classList.remove("show"); tipFor = null; }

/* ---- the effective value, drawn as a second mark ----------------------- */
function drawEff(force) {
  for (var i = 0; i < ORDER.length; i++) {
    var e = EFFV[i]; if (e === undefined) continue;
    if (!force && Math.abs(e - EFFP[i]) < 0.0015) continue;
    EFFP[i] = e;
    var id = ORDER[i], c = CTL[id]; if (!c) continue;
    var live = Math.abs(e - (V[id] || 0)) > 0.004;
    var t = c.dataset.type, m = c._em;
    if (t === "knob") m.style.transform = "rotate(" + (-140 + 280 * toT(id, e)) + "deg)";
    else if (t === "fader") m.style.bottom = (4 + toT(id, e) * 36) + "px";
    else if (t === "hf") m.style.left = (5 + toT(id, e) * ((c._hfw.clientWidth || 300) - 10)) + "px";
    m.classList.toggle("live", live);
    c.classList.toggle("mod", live);
  }
  DRAWS.eff++;
}

/* ══════════════════════════════════════════════════════════════════════════
   4 · WHERE each id is drawn. This is placement, not the parameter list: an
   id that is named nowhere here still gets a control, in the MORE strip.

   A view is a row of columns, or rows of rows of columns. A control named by
   two views is the SAME node, moved when the view changes — the views are
   mutually exclusive, so nothing is ever left with an empty socket.
   ══════════════════════════════════════════════════════════════════════════ */
var W86 = { w: 86 }, W96 = { w: 96 }, W104 = { w: 104 };
function A(id, lab, o) { return o ? [id, lab, o] : [id, lab]; }

var STRATA = [
  { p: "m",   nm: "MASS",      sb: "weight",         c: [A("m_gain","LEVEL"),A("m_cut","CUTOFF"),A("m_res","RES"),A("m_fmode","FILTER",W86),
        A("m_o1lvl","OSC 1"),A("m_o2lvl","OSC 2"),A("m_sublvl","SUB"),A("m_beat","BEAT"),A("m_drift","DRIFT")] },
  { p: "s",   nm: "SIGNAL",    sb: "artificial",     c: [A("s_gain","LEVEL"),A("s_cut","CUTOFF"),A("s_res","RES"),A("s_wt1pos","TABLE 1"),A("s_wt2pos","TABLE 2"),
        A("s_pmidx","PM INDEX"),A("s_addlvl","PARTIALS"),A("s_shift","SHIFT"),A("s_interf","INTERFERE")] },
  { p: "mem", nm: "MEMORY",    sb: "traces",         c: [A("mem_gain","LEVEL"),A("mem_src","SOURCE",W86),A("mem_mode","MODE",W86),A("mem_pos","POSITION"),
        A("mem_dens","DENSITY"),A("mem_dur","LENGTH"),A("mem_jit","JITTER"),A("mem_scatter","SCATTER"),A("mem_erosion","EROSION")] },
  { p: "st",  nm: "STRUCTURE", sb: "architecture",   c: [A("st_gain","LEVEL"),A("st_model","BODY",W86),A("st_exc","EXCITER",W86),A("st_damp","DAMPING"),
        A("st_stress","STRESS"),A("st_material","MATERIAL"),A("st_couple","COUPLING"),A("st_drive","DRIVE"),A("st_strikelvl","STRIKE")] }
];
var CHIPS = ["on","drone","keys","mute","solo"];
var CHIPLAB = { on:"ON", drone:"DRONE", keys:"KEYS", mute:"MUTE", solo:"SOLO" };

function stripOf(p) {
  return [A(p+"_gain","LEVEL"),A(p+"_pan","PAN"),A(p+"_width","WIDTH"),A(p+"_hp","HIGH CUT"),
          A(p+"_lp","LOW CUT"),A(p+"_send","SPACE"),A(p+"_loop","LOOP"),A(p+"_on","ON"),
          A(p+"_drone","IN DRONE"),A(p+"_keys","ON KEYS"),A(p+"_mute","MUTE"),A(p+"_solo","SOLO")];
}

var VIEWS = [
{ k:"main", tab:"MAIN", rows:[
  { h:294, cols:[{ w:350, sp:"strata", si:0 },{ w:350, sp:"strata", si:1 },
                 { w:350, sp:"strata", si:2 },{ w:350, sp:"strata", si:3 }] },
  { h:168, cols:[{ w:884, p:[{ t:"HISTORY", sp:"hsc", note:"four scenes, one journey" }] },
                 { w:0, flex:1, p:[{ t:"SPECTRAL RECORD", cy:1, sp:"rec", note:"48 bands · the strata of what has been played" }] }] }
]},

{ k:"sources", tab:"SOURCES", pages:[
  { k:"mass", tab:"MASS", cols:[
    { w:372, p:[
      { t:"OSCILLATOR 1", c:[A("m_o1wave","WAVE",W86),A("m_o1oct","OCTAVE",W86),A("m_o1semi","SEMI"),A("m_o1fine","FINE"),A("m_o1pw","WIDTH"),A("m_o1lvl","LEVEL")] },
      { t:"OSCILLATOR 2", c:[A("m_o2wave","WAVE",W86),A("m_o2oct","OCTAVE",W86),A("m_o2semi","SEMI"),A("m_o2fine","FINE"),A("m_o2pw","WIDTH"),A("m_o2lvl","LEVEL"),A("m_sync","SYNC 2>1"),A("m_beat","BEAT HZ")] }] },
    { w:322, p:[
      { t:"WEIGHT", c:[A("m_unison","UNISON",W86),A("m_unidet","DETUNE"),A("m_sub","SUB",W86),A("m_sublvl","SUB LEVEL"),A("m_subwave","SUB WAVE",W86),A("m_reinf","REINFORCE")] },
      { t:"INSTABILITY", c:[A("m_drift","DRIFT"),A("m_drifttime","DRIFT TIME"),A("m_pwdrift","WIDTH DRIFT"),A("m_ampvar","LEVEL DRIFT")] }] },
    { w:326, p:[
      { t:"FILTER", c:[A("m_fmode","CIRCUIT",W96),A("m_cut","CUTOFF"),A("m_res","RESONANCE"),A("m_fdrive","DRIVE"),A("m_fenv","ENV AMOUNT"),A("m_ftrack","KEY TRACK")] },
      { t:"FILTER ENVELOPE", c:[A("m_f_atk","ATTACK"),A("m_f_dec","DECAY"),A("m_f_sus","SUSTAIN"),A("m_f_rel","RELEASE")] }] },
    { w:278, p:[
      { t:"AMPLITUDE ENVELOPE", c:[A("m_a_atk","ATTACK"),A("m_a_dec","DECAY"),A("m_a_sus","SUSTAIN"),A("m_a_rel","RELEASE")] },
      { t:"MASS", sp:"note", txt:"Two analogue oscillators, a sub and a beat measured in hertz. The weight the rest of the instrument is hung on." }] }
  ]},
  { k:"signal", tab:"SIGNAL", cols:[
    { w:360, p:[
      { t:"TABLE 1", c:[A("s_wt1tab","TABLE",W104),A("s_wt1pos","POSITION"),A("s_wt1oct","OCTAVE",W86),A("s_wt1semi","SEMI"),A("s_wt1fine","FINE"),A("s_wt1lvl","LEVEL")] },
      { t:"TABLE 2", c:[A("s_wt2tab","TABLE",W104),A("s_wt2pos","POSITION"),A("s_wt2oct","OCTAVE",W86),A("s_wt2semi","SEMI"),A("s_wt2fine","FINE"),A("s_wt2lvl","LEVEL")] }] },
    { w:348, p:[
      { t:"PHASE MODULATION", c:[A("s_pmratio","RATIO",W86),A("s_pmfixed","FIXED HZ"),A("s_pmidx","INDEX"),A("s_pmfb","FEEDBACK"),A("s_ring","RING 1×2")] },
      { t:"PARTIALS", c:[A("s_addlvl","LEVEL"),A("s_addn","COUNT"),A("s_addspread","SPREAD"),A("s_addodd","ODD/EVEN"),A("s_addtilt","TILT"),A("s_addcluster","CLUSTER"),A("s_addgaps","GAPS"),A("s_addmotion","MOTION"),A("s_addfund","FUNDAMENTAL")] }] },
    { w:340, p:[
      { t:"DISPLACEMENT", c:[A("s_shift","FREQ SHIFT"),A("s_pshift","PITCH SHIFT"),A("s_interf","INTERFERENCE")] },
      { t:"FILTER", c:[A("s_fmode","CIRCUIT",W96),A("s_cut","CUTOFF"),A("s_res","RESONANCE"),A("s_fenv","ENV AMOUNT")] }] },
    { w:340, p:[
      { t:"FILTER ENVELOPE", c:[A("s_f_atk","ATTACK"),A("s_f_dec","DECAY"),A("s_f_sus","SUSTAIN"),A("s_f_rel","RELEASE")] },
      { t:"AMPLITUDE ENVELOPE", c:[A("s_a_atk","ATTACK"),A("s_a_dec","DECAY"),A("s_a_sus","SUSTAIN"),A("s_a_rel","RELEASE")] }] }
  ]},
  { k:"memory", tab:"MEMORY", cols:[
    { w:336, p:[
      { t:"SOURCE", c:[A("mem_src","MATERIAL",W104),A("mem_mode","MODE",W86),A("mem_capsrc","CAPTURE FROM",W104)], sp:"capture" }] },
    { w:340, p:[
      { t:"READER", c:[A("mem_pos","POSITION"),A("mem_scan","SCAN"),A("mem_region","REGION"),A("mem_pitch","PITCH"),A("mem_pfine","FINE"),A("mem_keyfollow","KEY FOLLOW")] },
      { t:"ENVELOPE", c:[A("mem_atk","ATTACK"),A("mem_rel","RELEASE")] }] },
    { w:356, p:[
      { t:"GRAINS", c:[A("mem_dur","LENGTH"),A("mem_dens","DENSITY"),A("mem_win","WINDOW",W96),A("mem_jit","JITTER"),A("mem_pspread","PITCH SPREAD"),A("mem_scatter","SCATTER"),A("mem_rev","REVERSE"),A("mem_sched","SCHEDULE",W96)] }] },
    { w:356, p:[
      { t:"SPECTRAL", c:[A("mem_freeze","FREEZE"),A("mem_smear","SMEAR"),A("mem_stilt","TILT"),A("mem_thin","THIN"),A("mem_disp","DISPLACE"),A("mem_evolve","EVOLVE")] },
      { t:"EROSION", amb:1, c:[A("mem_erosion","EROSION"),A("mem_ero_bw","BAND"),A("mem_ero_drop","DROPOUT"),A("mem_ero_frag","FRAGMENT"),A("mem_ero_simp","SIMPLIFY")] }] }
  ]},
  { k:"structure", tab:"STRUCTURE", cols:[
    { w:356, p:[
      { t:"BODY", c:[A("st_model","MODEL",W96),A("st_pitch","PITCH"),A("st_fine","FINE"),A("st_keyfollow","KEY FOLLOW"),A("st_stiff","STIFFNESS"),A("st_damp","DAMPING"),A("st_dens","DENSITY"),A("st_material","MATERIAL")] }] },
    { w:330, p:[
      { t:"GEOMETRY", c:[A("st_expos","POSITION"),A("st_pickup","PICKUP"),A("st_couple","COUPLING")] },
      { t:"STRESS", amb:1, c:[A("st_stress","STRESS"),A("st_drive","DRIVE")], sp:"fract" }] },
    { w:356, p:[
      { t:"EXCITER", c:[A("st_exc","EXCITER",W104),A("st_exclvl","LEVEL"),A("st_sustain","SUSTAIN"),A("st_bowforce","BOW FORCE"),A("st_bowvel","BOW SPEED")] },
      { t:"ENVELOPE", c:[A("st_atk","ATTACK"),A("st_rel","RELEASE")] }] },
    { w:330, p:[
      { t:"STRIKE", c:[A("st_strikeon","ON NOTE"),A("st_strikelvl","FORCE")], sp:"strike" },
      { t:"STRUCTURE", sp:"note", txt:"Seven resonant bodies under load. Raise STRESS until the structure detunes, groans and finally fractures." }] }
  ]}
]},

{ k:"life", tab:"LIFE", pages:[
  { k:"lfo",  tab:"LFO",              cols:[{ w:0, flex:1, p:[{ t:"LOW FREQUENCY OSCILLATORS", sp:"lfo" }] }] },
  { k:"env",  tab:"ENVELOPES & SHAPES", cols:[
      { w:560, p:[{ t:"ENVELOPES", sp:"env" }] },
      { w:0, flex:1, p:[{ t:"SHAPES", cy:1, sp:"shapes", note:"drawable · click to add, drag to move, alt-click to remove" }] }] },
  { k:"rnd",  tab:"RANDOM & FOLLOW",  cols:[
      { w:700, p:[{ t:"RANDOM SOURCES", sp:"rnd" }] },
      { w:432, p:[{ t:"FOLLOWERS", sp:"fol" }] },
      { w:0, flex:1, p:[{ t:"MODULATION SOURCES", cy:1, sp:"srcpal", note:"drag a source onto any control" }] }] },
  { k:"evt",  tab:"EVENTS & NETWORK", cols:[
      { w:700, p:[{ t:"EVENT LANES", sp:"evt" }] },
      { w:430, p:[{ t:"THE LIFE NETWORK", amb:1, c:[A("l_autonomy","AUTONOMY"),A("l_coupling","COUPLING"),A("l_recovery","RECOVERY"),A("l_smooth","SMOOTH"),A("l_hyst","HYSTERESIS"),A("l_hold","HOLD LIFE")] }] },
      { w:0, flex:1, p:[{ t:"DETECTORS", cy:1, sp:"net", note:"what the network is hearing" }] }] },
  { k:"matrix", tab:"MATRIX",         cols:[{ w:0, flex:1, p:[{ t:"MODULATION MATRIX", sp:"matrix", note:"32 slots" }] }] }
]},

{ k:"history", tab:"HISTORY", rows:[
  { h:238, cols:[{ w:0, flex:1, p:[{ t:"SCENES", sp:"scenebig", note:"store the instrument into a scene; HISTORY travels between them" }] },
                 { w:186, p:[{ t:"DETERMINISM", c:[A("seed","SEED"),A("determin","LOCKED")] }] }] },
  { h:224, cols:[{ w:700, p:[{ t:"TRAVEL", amb:1, sp:"travel" }] },
                 { w:0, flex:1, p:[{ t:"SPECTRAL RECORD", cy:1, sp:"recbig" }] }] }
]},

{ k:"env", tab:"ENVIRONMENT", pages:[
  { k:"lanes", tab:"INSERT LANES", cols:[
    { w:280, p:[
      { t:"LANE A", c:[A("e_a1","SLOT 1",W96),A("e_a2","SLOT 2",W96),A("e_a3","SLOT 3",W96)] },
      { t:"LANE B", c:[A("e_b1","SLOT 1",W96),A("e_b2","SLOT 2",W96),A("e_b3","SLOT 3",W96)] }] },
    { w:280, p:[
      { t:"SATURATE", c:[A("e_sat_drive","DRIVE"),A("e_sat_asym","ASYMMETRY"),A("e_sat_tone","TONE"),A("e_sat_mix","MIX")] },
      { t:"FOLD", c:[A("e_fold_amt","FOLD"),A("e_fold_sym","SYMMETRY"),A("e_fold_mix","MIX")] }] },
    { w:280, p:[
      { t:"MULTIBAND", c:[A("e_mb_lo","LOW ×"),A("e_mb_hi","HIGH ×"),A("e_mb_low","LOW"),A("e_mb_mid","MID"),A("e_mb_high","HIGH"),A("e_mb_mix","MIX")] },
      { t:"SHIFT", c:[A("e_sh_hz","HERTZ"),A("e_sh_mix","MIX"),A("e_sh_fb","FEEDBACK")] }] },
    { w:280, p:[
      { t:"DELAY", c:[A("e_dl_time","TIME"),A("e_dl_fb","FEEDBACK"),A("e_dl_mod","WOW"),A("e_dl_rate","WOW RATE"),A("e_dl_mode","MODE",W86),A("e_dl_tone","TONE"),A("e_dl_mix","MIX")] }] },
    { w:248, p:[
      { t:"COMB", c:[A("e_cb_pitch","PITCH"),A("e_cb_fb","FEEDBACK"),A("e_cb_damp","DAMPING"),A("e_cb_diff","DIFFUSION"),A("e_cb_mix","MIX")] },
      { t:"CRUSH", c:[A("e_cr_bits","BITS"),A("e_cr_rate","RATE"),A("e_cr_aa","ANTI-ALIAS"),A("e_cr_mix","MIX")] }] }
  ]},
  { k:"space", tab:"SPACE & LOOP", cols:[
    { w:520, p:[
      { t:"SPACE", cy:1, c:[A("e_rv_mix","AMOUNT"),A("e_rv_pre","PRE-DELAY"),A("e_rv_size","SIZE"),A("e_rv_decay","DECAY"),A("e_rv_diff","DIFFUSION"),A("e_rv_damp","HIGH DAMP"),A("e_rv_low","LOW DECAY"),A("e_rv_mod","MOTION"),A("e_rv_early","EARLY/LATE"),A("e_rv_freeze","FREEZE")] }] },
    { w:340, p:[
      { t:"PERSPECTIVE", c:[A("e_scale","APPARENT SCALE"),A("e_distance","DISTANCE")], sp:"spaceE" }] },
    { w:0, flex:1, p:[
      { t:"FEEDBACK LOOP", amb:1, c:[A("e_fb_send","SEND"),A("e_fb_ret","RETURN"),A("e_fb_delay","DELAY"),A("e_fb_hp","HIGH-PASS"),A("e_fb_lp","LOW-PASS"),A("e_fb_shift","SHIFT"),A("e_fb_sat","SATURATION"),A("e_fb_damp","DAMPING"),A("e_fb_to","RETURNS TO",W104)], sp:"loopE" }] }
  ]}
]},

{ k:"mix", tab:"MIX", cols:[
  { w:266, p:[{ t:"MASS",      c:stripOf("m") }] },
  { w:266, p:[{ t:"SIGNAL",    c:stripOf("s") }] },
  { w:266, p:[{ t:"MEMORY",    c:stripOf("mem") }] },
  { w:266, p:[{ t:"STRUCTURE", c:stripOf("st") }] },
  { w:0, flex:1, p:[
    { t:"OUTPUT", amb:1, c:[A("outwidth","WIDTH"),A("ext_mode","AUX INPUT",W104),A("ext_gain","AUX GAIN"),A("duck_amt","DUCK"),A("duck_rel","DUCK REL")] },
    { t:"TUNING & VOICES", c:[A("tune","TUNE"),A("fine","FINE"),A("bend","BEND RANGE"),A("voices","VOICES"),A("scale","SCALE",W104),A("rootlock","ROOT LOCK"),A("mpe","MPE")] },
    { t:"MORE", id:"more", sp:"more", note:"anything this panel has no home for" }] }
]}
];

/*  the always-visible strips: header, macros, foot. These own their ids
    outright and are never borrowed, so nothing here can go missing. */
var FOOTP = [
  { host:"#p-drone", t:"DRONE", amb:1, w:470, c:[A("drone","DRONE",{cls:"big"}),A("drone_root","ROOT"),A("drone_chord","CHORD",W86),
      A("drone_latch","LATCH"),A("drone_spread","SPREAD"),A("drone_vel","FORCE")] },
  { host:"#p-play", t:"PLAY", w:166, c:[A("vmode","MODE",W86),A("glide","GLIDE")] },
  { host:"#p-out", t:"OUTPUT", w:276, c:[A("volume","VOLUME"),A("ceiling","CEILING"),A("bassmono","MONO BELOW"),A("bassmono_on","BASS MONO")] }
];

/* ══════════════════════════════════════════════════════════════════════════
   5 · the shell. Built once at boot from the tables above; the controls
   themselves are built when initialState arrives, because their type is the
   kind that arrived and nothing else.
   ══════════════════════════════════════════════════════════════════════════ */
var TRAVELC = function (w) {
  return [[ "h_pos", "HISTORY", { w: w } ], A("h_mode","DRIVE",W86), A("h_dur","DURATION"),
          A("h_bars","BARS",W86), A("h_loop","AT THE END",W96)];
};
var TRAVELCH = [A("h_on","HISTORY",{cls:"chip"}), A("h_hold","HOLD HIST",{cls:"chip"}), A("l_hold","HOLD LIFE",{cls:"chip"})];

var VE = {}, PANELS = [], CURV = "main", CURP = {}, PLACED = {}, SP = {};
var SREC = [], SBTN = [], SLAMP = [];       /* scene tiles, in both homes */

function mkPanel(host, d) {
  var p = el("div", "panel", host);
  p.dataset.panel = d.t || "";
  if (d.id) p.id = d.id;
  if (d.h) p.style.height = d.h + "px";
  if (d.flexp) { p.style.flex = "1"; p.style.minHeight = "0"; }
  var ti = el("div", "pt" + (d.amb ? " amb" : "") + (d.cy ? " cy" : ""), p);
  el("span", "", ti, d.t || "");
  if (d.note) el("i", "", ti, d.note);
  p._ti = ti;
  var body = el("div", "row", p);
  p._row = body; p._want = (d.c || []).slice();
  if (d.sp && SP[d.sp]) SP[d.sp](p, d);
  PANELS.push(p);
  return p;
}
function mkCols(host, cols) {
  cols.forEach(function (cd) {
    var c = el("div", "col", host);
    if (cd.flex) { c.style.flex = "1"; c.style.minWidth = "0"; }
    else c.style.width = cd.w + "px", c.style.flex = "none";
    if (cd.sp && SP[cd.sp]) { SP[cd.sp](c, cd); return; }
    (cd.p || []).forEach(function (pd, i) {
      var last = (i === cd.p.length - 1);
      mkPanel(c, pd);
      if (last && !pd.h) { c.lastChild.style.flex = "1"; c.lastChild.style.minHeight = "0"; }
    });
  });
}
function buildStage() {
  var st = $("#stage");
  VIEWS.forEach(function (Vw) {
    var v = el("div", "view", st); v.dataset.view = Vw.k; v.hidden = (Vw.k !== "main");
    var rec = { el: v, pages: {}, order: [] };
    VE[Vw.k] = rec;
    var pages = Vw.pages || [{ k: "_", rows: Vw.rows, cols: Vw.cols }];
    if (Vw.pages) {
      var sb = el("div", "subs", v);
      Vw.pages.forEach(function (pg, i) {
        var b = el("div", "sub" + (i ? "" : " on"), sb, pg.tab);
        b.dataset.sub = pg.k;
        b.addEventListener("click", function () { setView(Vw.k, pg.k); });
      });
    }
    pages.forEach(function (pg, i) {
      var pe = el("div", "page", v); pe.dataset.page = pg.k; pe.hidden = (i !== 0);
      if (pg.rows) { pe.style.flexDirection = "column";
        pg.rows.forEach(function (rw) {
          var re = el("div", "page", pe); re.style.flex = "none"; re.style.height = rw.h + "px";
          mkCols(re, rw.cols);
        });
      } else mkCols(pe, pg.cols);
      rec.pages[pg.k] = pe; rec.order.push(pg.k);
      if (i === 0) CURP[Vw.k] = pg.k;
    });
  });
}
/*  Which ids have a home. It has to be counted after EVERY panel exists -
    the views, the header, the foot and the macros - or the strips' own
    controls look unplaced and the spare box takes them away from them. */
function countPlaced() {
  PLACED = {};
  PANELS.forEach(function (p) { (p._want || []).forEach(function (w) { PLACED[w[0]] = 1; }); });
}

/*  Activating a view re-appends every control its panels declare, in order.
    appendChild MOVES a node, so the listeners, the value and the id survive;
    borrowing only ever happens between views that are never both on screen. */
function place(scope) {
  scope.querySelectorAll("[data-panel]").forEach(function (p) {
    if (!p._want || !p._want.length) return;
    p._want.forEach(function (w) {
      var c = CTL[w[0]]; if (!c) return;
      if (w[2] && w[2].w) c.style.width = w[2].w + "px"; else c.style.width = "";
      /*  A control is MOVED between views, so every class a view lends it has
          to be taken back when another view borrows it. classList.add refuses a
          token with a space in it (and throws), so the string is split. */
      ["chip", "big", "mu", "sm"].forEach(function (k) { c.classList.remove(k); });
      if (w[2] && w[2].cls) String(w[2].cls).split(/\s+/).forEach(function (k) { if (k) c.classList.add(k); });
      if (w[1] !== undefined) c.querySelector(".lab").textContent = w[1];
      p._row.appendChild(c);
    });
  });
}
function setView(vk, pk) {
  if (!VE[vk]) return;
  CURV = vk; if (pk) CURP[vk] = pk;
  Object.keys(VE).forEach(function (k) { VE[k].el.hidden = (k !== vk); });
  var rec = VE[vk];
  rec.order.forEach(function (k) { rec.pages[k].hidden = (k !== CURP[vk]); });
  rec.el.querySelectorAll(".sub").forEach(function (b) { b.classList.toggle("on", b.dataset.sub === CURP[vk]); });
  document.querySelectorAll("#tabs .tab").forEach(function (t) { t.classList.toggle("on", t.dataset.tab === vk); });
  place(rec.pages[CURP[vk]]);
  refreshAll();
  return true;
}

/* ══════════════════════════════════════════════════════════════════════════
   6 · the special panels
   ══════════════════════════════════════════════════════════════════════════ */
var STLAMP = [], STBAR = [], STFR = null;

SP.strata = function (col, cd) {
  var S = STRATA[cd.si];
  var hd = el("div", "sthead", col);
  el("div", "nm", hd, S.nm);
  el("div", "sb", hd, S.sb);
  var lp = el("div", "lamp", hd); STLAMP[cd.si] = lp;
  var bar = el("div", "actbar", col); STBAR[cd.si] = el("i", "", bar);
  var ch = el("div", "chips", col);
  var want = [];
  CHIPS.forEach(function (sfx) { want.push([S.p + "_" + sfx, CHIPLAB[sfx], { cls: "chip" + (sfx === "mute" || sfx === "solo" ? " mu" : "") }]); });
  var chp = el("div", "panel", col); chp.dataset.panel = S.nm + " SWITCHES";
  chp.style.cssText = "flex:none;padding:0;border:none;background:none;box-shadow:none";
  chp._row = ch; chp._want = want; PANELS.push(chp);
  var p = mkPanel(col, { t: S.nm + " — " + S.sb, c: S.c });
  p.style.flex = "1"; p.style.minHeight = "0";
  if (cd.si === 3) {
    var bw = el("div", "row", p); bw.style.marginTop = "3px";
    var b = el("div", "btn warn", bw, "STRIKE");
    b.addEventListener("click", function () { NB.send({ k: "strike", v: V.st_strikelvl === undefined ? 0.7 : V.st_strikelvl }); say("STRIKE"); });
    STFR = p;
  }
};

SP.note = function (p, d) { el("div", "ro", p._row, d.txt).style.whiteSpace = "normal"; };

SP.rec = function (p) {
  p.style.display = "flex"; p.style.flexDirection = "column";
  p._row.style.flex = "none";
  p._recwrap = el("div", "cvwrap", p);
  RECW.push(p._recwrap);
};
SP.recbig = SP.rec;

SP.capture = function (p) {
  var r = el("div", "row", p._row.parentNode); r.style.marginTop = "4px";
  var b1 = el("div", "btn", r, "CAPTURE");
  var b2 = el("div", "btn", r, "REMEMBER");
  var b3 = el("div", "btn", r, "IMPORT WAV");
  var b4 = el("div", "btn", r, "SCALA");
  b1.addEventListener("click", function () { CAPON = !CAPON; NB.send({ k: "capture", on: CAPON }); b1.classList.toggle("on", CAPON); });
  b2.addEventListener("click", function () { NB.send({ k: "remember" }); say("THE CAPTURE IS NOW PART OF THE PATCH"); });
  b3.addEventListener("click", function () { NB.send({ k: "import" }); });
  b4.addEventListener("click", function () { NB.send({ k: "scala" }); });
  CAPBTN = b1;
  CAPTXT = el("div", "ro", p._row.parentNode, "NO CAPTURE");
  CAPTXT.style.marginTop = "4px"; CAPTXT.style.whiteSpace = "normal";
};

SP.strike = function (p) {
  var r = el("div", "row", p._row.parentNode); r.style.marginTop = "4px";
  var b = el("div", "btn warn", r, "STRIKE NOW");
  b.addEventListener("click", function () { NB.send({ k: "strike", v: V.st_strikelvl === undefined ? 0.7 : V.st_strikelvl }); say("STRIKE"); });
};
SP.fract = function (p) {
  var r = el("div", "row", p._row.parentNode); r.style.marginTop = "4px";
  el("span", "ro", r, "FRACTURE");
  FRLAMP = el("span", "lamp vm", r);
};
SP.loopE = function (p) {
  var r = el("div", "row", p._row.parentNode); r.style.marginTop = "4px"; r.style.alignItems = "center";
  el("span", "ro", r, "LOOP ENERGY");
  var b = el("div", "bar a", r); b.style.flex = "1"; LOOPB2 = el("i", "", b);
};
SP.spaceE = function (p) {
  var r = el("div", "row", p._row.parentNode); r.style.marginTop = "4px"; r.style.alignItems = "center";
  el("span", "ro", r, "SPACE ENERGY");
  var b = el("div", "bar", r); b.style.flex = "1"; SPACEB2 = el("i", "", b);
};
SP.more = function (p) { MOREP = p; p.hidden = true; };

/* ---- the history strip, and the same scenes shown large ---------------- */
function sceneTile(host, i, big) {
  var t = el("div", "scene", host);
  if (!big) t.style.width = "214px"; else t.style.flex = "1";
  var nm = el("div", "sn", t, "");
  nm.contentEditable = "true"; nm.spellcheck = false;
  nm.addEventListener("keydown", function (e) {
    if (e.key === "Enter") { e.preventDefault(); nm.blur(); }
    e.stopPropagation();
  });
  nm.addEventListener("blur", function () {
    var s = nm.textContent.replace(/\s+/g, " ").trim().slice(0, 28).toUpperCase();
    nm.textContent = s;
    SCENES.names[i] = s;
    NB.send({ k: "scene", op: "name", i: i, name: s });
  });
  var inf = el("div", "si", t, "");
  var b = el("div", "sbtns", t);
  var lp = el("span", "lamp", b);
  var st = el("div", "btn sm", b, "STORE");
  var rc = el("div", "btn sm", b, "RECALL");
  var cl = el("div", "btn sm", b, "CLEAR");
  st.addEventListener("click", function () { NB.send({ k: "scene", op: "store", i: i }); SCENES.set[i] = 1; syncScenes(); say("SCENE " + (i + 1) + " STORED"); });
  rc.addEventListener("click", function () { NB.send({ k: "scene", op: "recall", i: i }); say("SCENE " + (i + 1) + " RECALLED"); });
  cl.addEventListener("click", function () { NB.send({ k: "scene", op: "clear", i: i }); SCENES.set[i] = 0; syncScenes(); });
  (big ? SBTN : SREC).push({ t: t, nm: nm, inf: inf, lp: lp });
  return t;
}
SP.hsc = function (p) {
  var sc = el("div", "row", p._row); sc.style.gap = "4px"; sc.style.height = "62px";
  for (var i = 0; i < 4; i++) sceneTile(sc, i, false);
  var tr = el("div", "row", p._row.parentNode);
  tr.style.marginTop = "4px"; tr.style.alignItems = "flex-start"; tr.style.gap = "4px";
  var pw = el("div", "panel", tr);
  pw.dataset.panel = "TRAVEL SMALL"; pw.style.cssText = "flex:1;padding:0;border:none;background:none;box-shadow:none";
  pw._row = el("div", "row", pw); pw._want = TRAVELC(360); PANELS.push(pw);
  var cw = el("div", "panel", tr);
  cw.dataset.panel = "HOLDS"; cw.style.cssText = "flex:none;width:80px;padding:0;border:none;background:none;box-shadow:none";
  cw._row = el("div", "col", cw); cw._row.style.gap = "2px"; cw._want = TRAVELCH; PANELS.push(cw);
};
SP.travel = function (p) {
  p._want = TRAVELC(620);
  var cw = el("div", "row", p._row.parentNode); cw.style.marginTop = "4px"; cw.style.gap = "3px";
  var cp = el("div", "panel", cw);
  cp.dataset.panel = "HOLDS BIG"; cp.style.cssText = "flex:1;padding:0;border:none;background:none;box-shadow:none";
  cp._row = el("div", "row", cp); cp._row.style.gap = "3px"; cp._want = TRAVELCH; PANELS.push(cp);
  HTL = el("canvas", "scr", p._row.parentNode); HTL.width = 660; HTL.height = 38;
  HTL.style.cssText = "width:100%;height:38px;margin-top:4px";
};
SP.scenebig = function (p) {
  var r = el("div", "row", p._row); r.style.gap = "6px"; r.style.height = "142px";
  for (var i = 0; i < 4; i++) {
    var t = sceneTile(r, i, true);
    if (i === 1 || i === 2) {
      var pw = el("div", "panel", t);
      pw.dataset.panel = "SCENE " + (i + 1) + " AT";
      pw.style.cssText = "padding:0;border:none;background:none;box-shadow:none;flex:none";
      pw._row = el("div", "row", pw); pw._want = [A("h_sc" + (i + 1) + "pos", "AT")]; PANELS.push(pw);
      t.insertBefore(pw, t.lastChild);
    }
  }
};

/* ---- the LIFE rows, each with the live value beside it ----------------- */
var MODBAR = { lfo: [], env: [], shape: [], rnd: [], fol: [], evt: [] };
function rowBlock(host, tag, want, key, idx) {
  var r = el("div", "modrow", host);
  el("div", "tagn", r, tag);
  var pw = el("div", "panel", r);
  pw.dataset.panel = tag;
  pw.style.cssText = "flex:1;padding:0;border:none;background:none;box-shadow:none;overflow:hidden";
  pw._row = el("div", "row", pw); pw._want = want; PANELS.push(pw);
  var mb = el("div", "mb", r);
  var b = el("div", "bar", mb); b.style.width = "34px";
  MODBAR[key][idx] = el("i", "", b);
  el("div", "ro", mb, "NOW").style.fontSize = "6px";
  return r;
}
SP.lfo = function (p) {
  var wrap = el("div", "row", p._row); wrap.style.gap = "6px";
  for (var col = 0; col < 2; col++) {
    var c = el("div", "col", wrap); c.style.flex = "1"; c.style.gap = "0";
    for (var k = 0; k < 4; k++) {
      var n = col * 4 + k + 1;
      rowBlock(c, "LFO " + n, [A("l"+n+"_wave","SHAPE",W104),A("l"+n+"_rate","RATE"),
        A("l"+n+"_sync","SYNC",W86),A("l"+n+"_phase","PHASE"),A("l"+n+"_scope","SCOPE",W86)], "lfo", n - 1);
    }
  }
};
SP.env = function (p) {
  for (var n = 1; n <= 4; n++)
    rowBlock(p._row, "ENV " + n, [A("e"+n+"_atk","ATTACK"),A("e"+n+"_dec","DECAY"),
      A("e"+n+"_sus","SUSTAIN"),A("e"+n+"_rel","RELEASE"),A("e"+n+"_trig","TRIGGER",W96)], "env", n - 1);
  p._row.style.flexDirection = "column";
};
SP.rnd = function (p) {
  for (var n = 1; n <= 4; n++)
    rowBlock(p._row, "RND " + n, [A("r"+n+"_type","KIND",W104),A("r"+n+"_rate","RATE"),
      A("r"+n+"_amt","SPREAD"),A("r"+n+"_restore","RESTORE"),A("r"+n+"_smooth","SMOOTH")], "rnd", n - 1);
  p._row.style.flexDirection = "column";
};
SP.fol = function (p) {
  for (var n = 1; n <= 2; n++)
    rowBlock(p._row, "FOL " + n, [A("f"+n+"_src","LISTENS TO",W104),A("f"+n+"_atk","ATTACK"),
      A("f"+n+"_rel","RELEASE")], "fol", n - 1);
  p._row.style.flexDirection = "column";
};
SP.evt = function (p) {
  for (var n = 1; n <= 4; n++)
    rowBlock(p._row, "EVT " + n, [A("v"+n+"_prob","CHANCE"),A("v"+n+"_rate","RATE"),
      A("v"+n+"_target","TARGET",{w:112}),A("v"+n+"_refract","REFRACT"),A("v"+n+"_amt","AMOUNT")], "evt", n - 1);
  p._row.style.flexDirection = "column";
};
var NETB = [];

/* ---- the network's detectors: five rows of three bars, and the erosion ---- */
SP.net = function (p) {
  p._row.style.flexDirection = "column"; p._row.style.gap = "3px";
  ["MASS","SIGNAL","MEMORY","STRUCTURE","MIX"].forEach(function (nm, i) {
    var r = el("div", "ro", p._row); r.style.cssText = "display:flex;gap:4px;align-items:center";
    el("span", "", r, nm).style.width = "58px";
    var bars = [];
    ["E","B","T"].forEach(function (t) {
      el("span", "", r, t).style.color = "var(--oxide2)";
      var b = el("div", "bar", r); b.style.flex = "1"; bars.push(el("i", "", b));
    });
    NETB[i] = bars;
  });
  var r2 = el("div", "ro", p._row); r2.style.cssText = "display:flex;gap:4px;align-items:center;margin-top:4px";
  el("span", "", r2, "EROSION").style.width = "58px";
  var b2 = el("div", "bar a", r2); b2.style.flex = "1"; NETB.ero = el("i", "", b2);
  NETB.txt = el("span", "", r2, "0 %");
};

/* ══════════════════════════════════════════════════════════════════════════
   7 · popups: one shell for menus, entry boxes, the matrix choosers and the
   macro map editor. It closes on Escape and on a pointerdown OUTSIDE it, armed
   a tick late so it never eats the click that opened it.
   ══════════════════════════════════════════════════════════════════════════ */
var POPS = [];
function pop(title, w, h, x, y) {
  var e = el("div", "pop", document.body);
  var ph = el("div", "ph", e); el("span", "", ph, title);
  var xb = el("span", "x", ph, "×");
  var pb = el("div", "pb", e);
  e.style.width = w + "px"; e.style.maxHeight = h + "px";
  var rec = { el: e, body: pb, close: function () { var i = POPS.indexOf(rec); if (i >= 0) POPS.splice(i, 1); if (e.parentNode) e.parentNode.removeChild(e); } };
  xb.addEventListener("click", rec.close);
  POPS.push(rec);
  requestAnimationFrame(function () {
    var r = e.getBoundingClientRect();
    e.style.left = Math.round(clamp(x, 4, Math.max(4, innerWidth - r.width - 4))) + "px";
    e.style.top = Math.round(clamp(y, 4, Math.max(4, innerHeight - r.height - 4))) + "px";
  });
  e.style.left = x + "px"; e.style.top = y + "px";
  setTimeout(function () {
    var closer = function (ev) { if (!e.contains(ev.target)) { rec.close(); document.removeEventListener("pointerdown", closer, true); } };
    document.addEventListener("pointerdown", closer, true);
    rec._closer = closer;
  }, 0);
  return rec;
}
function closePops() { POPS.slice().forEach(function (p) { if (p._closer) document.removeEventListener("pointerdown", p._closer, true); p.close(); }); }
document.addEventListener("keydown", function (e) { if (e.key === "Escape" && POPS.length) { closePops(); e.stopPropagation(); } }, true);

function openSelMenu(c, id) {
  var r = c.getBoundingClientRect();
  var pk = pop(PNAME[id] || id, 180, 300, r.left, r.bottom + 4);
  (NAMES[id] || []).forEach(function (nm, i) {
    var it = el("div", "pi" + (Math.round(V[id]) === i ? " on" : ""), pk.body, nm);
    it.addEventListener("click", function () { setP(id, i); pk.close(); });
  });
}

/*  Typing a value, in the parameter's own units where the law allows it. */
function parseEntry(id, s) {
  s = String(s).trim().toUpperCase().replace(",", ".");
  var k = KIND[id], lo = LO[id], fhi = FHI[id];
  if (k === K.SW) return /^(1|ON|YES|TRUE)$/.test(s) ? 1 : 0;
  if (k === K.LIST) { var n = (NAMES[id] || []); for (var i = 0; i < n.length; i++) if (n[i].toUpperCase() === s) return i; var q = parseInt(s, 10); return isNaN(q) ? undefined : q; }
  if (k === K.NOTE) { var m = /^([A-G])(#?)(-?\d)$/.exec(s); if (m) { var pc = NOTEN.indexOf(m[1] + m[2]); if (pc >= 0) return (parseInt(m[3], 10) + 1) * 12 + pc; } }
  var x = parseFloat(s); if (isNaN(x)) return undefined;
  switch (k) {
    case K.PCT:   return x > 1.0001 ? x / 100 : x;
    case K.BIPOL: return Math.abs(x) > 1.0001 || /%/.test(s) ? x / 200 + 0.5 : x;
    case K.HZ: case K.MS: case K.SEC: case K.GLIDE:
      if (k === K.SEC && /M/.test(s) && !/MS/.test(s)) x *= 60;
      if (k === K.MS && /\dS$/.test(s) && !/MS$/.test(s)) x *= 1000;
      if (k === K.HZ && /K/.test(s)) x *= 1000;
      return x <= 0 ? 0 : Math.log(clamp(x, lo, fhi) / lo) / Math.log(fhi / lo);
    case K.SEMI: case K.CENT: return x / lo + 0.5;
    case K.CENTU: return x / lo;
    case K.VOL:  { if (!/DB/.test(s) && x >= 0 && x <= 1) return x; var g = Math.pow(10, x / 20); return Math.sqrt(g / 2); }
    case K.PW:   return (x - 50) / 45;
    case K.SHZ:  { var a = Math.abs(x / lo); return 0.5 + 0.5 * (x < 0 ? -1 : 1) * Math.pow(a, 1 / 3); }
    case K.DB:   return (x - lo) / (fhi - lo);
    case K.INT: case K.NOTE: return x;
  }
  return x;
}
function openEntry(c, id) {
  var r = c.getBoundingClientRect();
  var pk = pop(PNAME[id] || id, 170, 80, r.left, r.bottom + 4);
  var inp = el("input", "psearch", pk.body); inp.value = fmt(id, V[id]); inp.spellcheck = false;
  el("div", "ro", pk.body, "type a value in its own units, Enter to set").style.whiteSpace = "normal";
  inp.addEventListener("keydown", function (e) {
    e.stopPropagation();
    if (e.key === "Enter") { var v = parseEntry(id, inp.value); if (v !== undefined) setP(id, v); pk.close(); }
    if (e.key === "Escape") pk.close();
  });
  setTimeout(function () { inp.focus(); inp.select(); }, 20);
}

/*  A searchable chooser of parameter ids (the matrix destination, the macro maps). */
function openDstChooser(anchor, current, onPick) {
  var r = anchor.getBoundingClientRect();
  var pk = pop("DESTINATION", 250, 360, r.left, r.bottom + 4);
  var inp = el("input", "psearch", pk.body); inp.placeholder = "search id or name"; inp.spellcheck = false;
  var list = el("div", "", pk.body);
  function fill(q) {
    list.textContent = "";
    var none = el("div", "pi", list, "— none"); none.addEventListener("click", function () { onPick(""); pk.close(); });
    q = (q || "").toUpperCase();
    var n = 0;
    ORDER.forEach(function (id) {
      if (!modulatable(id) || /^bwfx_/.test(id)) return;
      var nm = PNAME[id] || id;
      if (q && id.toUpperCase().indexOf(q) < 0 && nm.toUpperCase().indexOf(q) < 0) return;
      if (++n > 120) return;
      var it = el("div", "pi" + (id === current ? " on" : ""), list, nm);
      el("span", "sub2", it, "  " + id);
      it.addEventListener("click", function () { onPick(id); pk.close(); });
    });
  }
  fill("");
  inp.addEventListener("input", function () { fill(inp.value); });
  inp.addEventListener("keydown", function (e) { e.stopPropagation(); if (e.key === "Escape") pk.close(); });
  setTimeout(function () { inp.focus(); }, 20);
}
function openListChooser(anchor, title, names, current, onPick) {
  var r = anchor.getBoundingClientRect();
  var pk = pop(title, 220, 340, r.left, r.bottom + 4);
  names.forEach(function (nm, i) {
    var it = el("div", "pi" + (i === current ? " on" : ""), pk.body, nm);
    it.addEventListener("click", function () { onPick(i); pk.close(); });
  });
}

/* ══════════════════════════════════════════════════════════════════════════
   8 · the matrix. 32 rows built once; each cell writes the slot and sends
   the WHOLE slot (the protocol's one message), then the native echo redraws.
   ══════════════════════════════════════════════════════════════════════════ */
var MXROWS = [];
var CURVEN = ["LINEAR", "EXP", "LOG", "S"];
function blankSlot(i) { return { i: i, src: 0, via: 0, dst: "", depth: 0, offset: 0, curve: 0, slew: 0, lo: -1, hi: 1, on: false }; }
function sendSlot(i) { var s = SLOTS[i]; NB.send({ k: "slot", i: i, src: s.src, via: s.via, dst: s.dst, depth: s.depth, offset: s.offset, curve: s.curve, slew: s.slew, lo: s.lo, hi: s.hi, on: !!s.on }); }
function numCell(host, get, set, min, max, step, fmtf) {
  var c = el("div", "mf num", host);
  var down = false, y0 = 0, v0 = 0;
  function draw() { c.textContent = fmtf(get()); }
  c.addEventListener("pointerdown", function (e) { down = true; y0 = e.clientY; v0 = get(); try { c.setPointerCapture(e.pointerId); } catch (x) {} e.preventDefault(); });
  c.addEventListener("pointermove", function (e) { if (!down) return; var v = clamp(v0 + (y0 - e.clientY) / DECK_S / 100 * (max - min) * (e.shiftKey ? 0.1 : 1), min, max); if (step) v = Math.round(v / step) * step; set(v); draw(); });
  c.addEventListener("pointerup", function () { if (down) { down = false; set(get(), true); } });
  c.addEventListener("dblclick", function () { var p2 = pop("VALUE", 140, 60, 0, 0); var inp = el("input", "psearch", p2.body); inp.value = String(get()); inp.addEventListener("keydown", function (e) { e.stopPropagation(); if (e.key === "Enter") { var x = parseFloat(inp.value); if (!isNaN(x)) { set(clamp(x, min, max), true); draw(); } p2.close(); } }); var r = c.getBoundingClientRect(); p2.el.style.left = r.left + "px"; p2.el.style.top = r.bottom + "px"; setTimeout(function () { inp.focus(); inp.select(); }, 20); });
  c._draw = draw; draw();
  return c;
}
SP.matrix = function (p) {
  p._row.style.flexDirection = "column"; p._row.style.gap = "0"; p._row.style.overflow = "auto"; p._row.style.flex = "1"; p._row.style.minHeight = "0";
  p.style.display = "flex"; p.style.flexDirection = "column";
  var hd = el("div", "mxh", p._row);
  ["#", "ON", "SOURCE", "VIA", "DESTINATION", "DEPTH", "OFFSET", "CURVE", "SLEW", "LO", "HI"].forEach(function (t) { el("div", "", hd, t); });
  for (var i = 0; i < 32; i++) (function (i) {
    if (!SLOTS[i]) SLOTS[i] = blankSlot(i);
    var r = el("div", "mxr", p._row);
    var row = { el: r };
    el("div", "n", r, String(i + 1));
    row.on = el("div", "mf sw", r, "●");
    row.on.addEventListener("click", function () { SLOTS[i].on = !SLOTS[i].on; sendSlot(i); drawSlot(i); });
    row.src = el("div", "mf", r, "—");
    row.src.addEventListener("click", function () { openListChooser(row.src, "SOURCE", SOURCES, SLOTS[i].src, function (k) { SLOTS[i].src = k; if (k && SLOTS[i].dst && SLOTS[i].depth === 0) SLOTS[i].depth = 0.3; if (k && SLOTS[i].dst) SLOTS[i].on = true; sendSlot(i); drawSlot(i); }); });
    row.via = el("div", "mf", r, "—");
    row.via.addEventListener("click", function () { openListChooser(row.via, "VIA (multiplies)", SOURCES, SLOTS[i].via, function (k) { SLOTS[i].via = k; sendSlot(i); drawSlot(i); }); });
    row.dst = el("div", "mf dst", r, "—");
    row.dst.addEventListener("click", function () { openDstChooser(row.dst, SLOTS[i].dst, function (id) { SLOTS[i].dst = id; if (id && SLOTS[i].src && SLOTS[i].depth === 0) SLOTS[i].depth = 0.3; if (id && SLOTS[i].src) SLOTS[i].on = true; sendSlot(i); drawSlot(i); }); });
    row.depth = numCell(r, function () { return SLOTS[i].depth; }, function (v, fin) { SLOTS[i].depth = v; if (fin) sendSlot(i); }, -1, 1, 0.01, function (v) { return sgn(Math.round(v * 100)) + " %"; });
    row.offset = numCell(r, function () { return SLOTS[i].offset; }, function (v, fin) { SLOTS[i].offset = v; if (fin) sendSlot(i); }, -1, 1, 0.01, function (v) { return sgn(Math.round(v * 100)) + " %"; });
    row.curve = el("div", "mf", r, "LINEAR");
    row.curve.addEventListener("click", function () { SLOTS[i].curve = (SLOTS[i].curve + 1) % 4; sendSlot(i); drawSlot(i); });
    row.slew = numCell(r, function () { return SLOTS[i].slew; }, function (v, fin) { SLOTS[i].slew = v; if (fin) sendSlot(i); }, 0, 30, 0.05, function (v) { return v < 0.01 ? "0" : v.toFixed(2) + " s"; });
    row.lo = numCell(r, function () { return SLOTS[i].lo; }, function (v, fin) { SLOTS[i].lo = v; if (fin) sendSlot(i); }, -1, 1, 0.05, function (v) { return v.toFixed(2); });
    row.hi = numCell(r, function () { return SLOTS[i].hi; }, function (v, fin) { SLOTS[i].hi = v; if (fin) sendSlot(i); }, -1, 1, 0.05, function (v) { return v.toFixed(2); });
    MXROWS[i] = row;
  })(i);
};
function drawSlot(i) {
  var row = MXROWS[i], s = SLOTS[i]; if (!row || !s) return;
  row.el.classList.toggle("act", !!s.on && s.src > 0 && !!s.dst);
  row.on.classList.toggle("on", !!s.on);
  row.src.textContent = SOURCES[s.src] || "—";
  row.via.textContent = s.via ? (SOURCES[s.via] || "?") : "—";
  row.dst.textContent = s.dst ? (PNAME[s.dst] || s.dst) : "—";
  row.curve.textContent = CURVEN[s.curve] || "LINEAR";
  [row.depth, row.offset, row.slew, row.lo, row.hi].forEach(function (c) { c._draw(); });
}
function drawSlots() { for (var i = 0; i < 32; i++) drawSlot(i); }
function firstFreeSlot() { for (var i = 0; i < 32; i++) if (!SLOTS[i] || (!SLOTS[i].src && !SLOTS[i].dst)) return i; return -1; }

/* ---- the source palette: drag a chip onto any control --------------------- */
var DRAG = null, GHOST = null;
SP.srcpal = function (p) {
  p._row.style.overflow = "auto"; p._row.style.flex = "1"; p._row.style.minHeight = "0"; p._row.style.alignContent = "flex-start";
  p.style.display = "flex"; p.style.flexDirection = "column";
  p._pal = p._row;
};
function buildPalette() {
  var host = null; PANELS.forEach(function (p) { if (p._pal) host = p._pal; });
  if (!host) return;
  host.textContent = "";
  SOURCES.forEach(function (nm, i) {
    if (!i) return;
    var ch = el("div", "chipp", host, nm); ch.dataset.src = i;
    ch.addEventListener("pointerdown", function (e) {
      DRAG = { src: i, name: nm }; ch.classList.add("drg");
      if (!GHOST) GHOST = $("#drghost");
      GHOST.textContent = nm; GHOST.style.display = "block";
      moveGhost(e); e.preventDefault();
    });
  });
}
function moveGhost(e) { if (GHOST) { GHOST.style.left = (e.clientX + 12) + "px"; GHOST.style.top = (e.clientY - 8) + "px"; } }
document.addEventListener("pointermove", function (e) {
  if (!DRAG) return;
  moveGhost(e);
  var t = document.elementFromPoint(e.clientX, e.clientY);
  var c = t && t.closest ? t.closest(".ctl") : null;
  document.querySelectorAll(".ctl.dtgt").forEach(function (x) { if (x !== c) x.classList.remove("dtgt"); });
  if (c && modulatable(c.dataset.id)) c.classList.add("dtgt");
});
document.addEventListener("pointerup", function (e) {
  if (!DRAG) return;
  var d = DRAG; DRAG = null;
  document.querySelectorAll(".chipp.drg").forEach(function (x) { x.classList.remove("drg"); });
  document.querySelectorAll(".ctl.dtgt").forEach(function (x) { x.classList.remove("dtgt"); });
  if (GHOST) GHOST.style.display = "none";
  var t = document.elementFromPoint(e.clientX, e.clientY);
  var c = t && t.closest ? t.closest(".ctl") : null;
  if (!c || !modulatable(c.dataset.id)) return;
  var i = firstFreeSlot(); if (i < 0) { say("THE MATRIX IS FULL"); return; }
  SLOTS[i] = blankSlot(i); SLOTS[i].src = d.src; SLOTS[i].dst = c.dataset.id; SLOTS[i].depth = 0.3; SLOTS[i].on = true;
  sendSlot(i); drawSlot(i);
  say("SLOT " + (i + 1) + "   " + d.name + " → " + (PNAME[c.dataset.id] || c.dataset.id) + "   +30 %");
});

/* ══════════════════════════════════════════════════════════════════════════
   9 · the shapes: four drawable envelopes. Click adds a point, drag moves it,
   alt-click removes it, the wheel bends the segment's curve.
   ══════════════════════════════════════════════════════════════════════════ */
var SHPED = [];
var TRIGN = ["NOTE", "DRONE START", "EVENT LANE"];
SP.shapes = function (p) {
  p._row.style.gap = "6px";
  for (var i = 0; i < 4; i++) (function (i) {
    if (!SHAPES[i]) SHAPES[i] = { trig: 0, loop: false, pts: [{ t: 0, l: 0, c: 0 }, { t: 0.5, l: 1, c: 0 }, { t: 2, l: 0.4, c: 0 }, { t: 4, l: 0, c: 0 }] };
    var w = el("div", "shp", p._row); w.style.width = "308px";
    var h = el("div", "sh", w);
    el("div", "t", h, "SHAPE " + (i + 1));
    var trig = el("div", "btn sm", h, TRIGN[0]);
    var loop = el("div", "btn sm", h, "LOOP");
    var cv = el("canvas", "scr", w); cv.width = 308; cv.height = 128; cv.style.cssText = "width:308px;height:128px;cursor:crosshair";
    var ed = { cv: cv, trig: trig, loop: loop, i: i, drag: -1 };
    trig.addEventListener("click", function () { SHAPES[i].trig = (SHAPES[i].trig + 1) % 3; sendShape(i); drawShape(i); });
    loop.addEventListener("click", function () { SHAPES[i].loop = !SHAPES[i].loop; sendShape(i); drawShape(i); });
    function xy(e) { var r = cv.getBoundingClientRect(); return { x: (e.clientX - r.left) / r.width, y: 1 - (e.clientY - r.top) / r.height }; }
    function tmax() { var s = SHAPES[i]; return Math.max(1, s.pts[s.pts.length - 1].t) * 1.08; }
    function hit(q) { var s = SHAPES[i], best = -1, bd = 0.04; s.pts.forEach(function (pt, k) { var d = Math.hypot(pt.t / tmax() - q.x, pt.l - q.y); if (d < bd) { bd = d; best = k; } }); return best; }
    cv.addEventListener("pointerdown", function (e) {
      var q = xy(e), s = SHAPES[i], k = hit(q);
      if (e.altKey) { if (k > 0 && s.pts.length > 2) { s.pts.splice(k, 1); sendShape(i); drawShape(i); } return; }
      if (k < 0) {
        if (s.pts.length >= 8) return;
        var t = clamp(q.x * tmax(), 0, 1800), l = clamp01(q.y);
        s.pts.push({ t: t, l: l, c: 0 }); s.pts.sort(function (a, b) { return a.t - b.t; });
        k = s.pts.findIndex(function (pt) { return pt.t === t && pt.l === l; });
      }
      ed.drag = k; try { cv.setPointerCapture(e.pointerId); } catch (x) {}
      e.preventDefault();
    });
    cv.addEventListener("pointermove", function (e) {
      if (ed.drag < 0) return;
      var q = xy(e), s = SHAPES[i], k = ed.drag, pt = s.pts[k];
      pt.l = clamp01(q.y);
      if (k > 0) { var lo = s.pts[k - 1].t + 0.001, hi = k + 1 < s.pts.length ? s.pts[k + 1].t - 0.001 : 1800; pt.t = clamp(q.x * tmax(), lo, Math.max(lo, hi)); }
      drawShape(i);
    });
    var up = function () { if (ed.drag >= 0) { ed.drag = -1; sendShape(i); } };
    cv.addEventListener("pointerup", up); cv.addEventListener("pointercancel", up);
    cv.addEventListener("wheel", function (e) {
      var q = xy(e), s = SHAPES[i];
      for (var k = 1; k < s.pts.length; k++) if (q.x * tmax() <= s.pts[k].t) { s.pts[k].c = clamp((s.pts[k].c || 0) + (e.deltaY < 0 ? 0.1 : -0.1), -1, 1); break; }
      sendShape(i); drawShape(i); e.preventDefault();
    }, { passive: false });
    SHPED[i] = ed;
    drawShape(i);
  })(i);
};
function sendShape(i) { var s = SHAPES[i]; NB.send({ k: "mseg", i: i, trig: s.trig, loop: !!s.loop, pts: s.pts.map(function (p) { return { t: p.t, l: p.l, c: p.c || 0 }; }) }); }
function shapeAt(s, t) {
  var v = s.pts[0].l;
  for (var k = 0; k + 1 < s.pts.length; k++) {
    var a = s.pts[k], b = s.pts[k + 1];
    if (t >= a.t && t <= b.t) { var u = b.t > a.t ? (t - a.t) / (b.t - a.t) : 1, c = b.c || 0; if (c > 0.01) u = Math.pow(u, 1 + 3 * c); else if (c < -0.01) u = 1 - Math.pow(1 - u, 1 - 3 * c); return a.l + (b.l - a.l) * u; }
    v = b.l;
  }
  return v;
}
function drawShape(i) {
  var ed = SHPED[i]; if (!ed) return;
  var s = SHAPES[i], g = ed.cv.getContext("2d"), W = ed.cv.width, H = ed.cv.height;
  var tmax = Math.max(1, s.pts[s.pts.length - 1].t) * 1.08;
  g.fillStyle = "#0a0c0e"; g.fillRect(0, 0, W, H);
  g.strokeStyle = "rgba(231,227,217,.07)"; g.lineWidth = 1;
  for (var k = 1; k < 4; k++) { g.beginPath(); g.moveTo(0, k * H / 4); g.lineTo(W, k * H / 4); g.stroke(); }
  g.strokeStyle = "#d99a3a"; g.lineWidth = 1.5; g.beginPath();
  for (var x = 0; x <= W; x++) { var y = H - 4 - shapeAt(s, x / W * tmax) * (H - 8); if (x === 0) g.moveTo(x, y); else g.lineTo(x, y); }
  g.stroke();
  s.pts.forEach(function (pt, k) { g.fillStyle = k === ed.drag ? "#fff" : "#e7e3d9"; g.beginPath(); g.arc(pt.t / tmax * W, H - 4 - pt.l * (H - 8), 3, 0, 6.283); g.fill(); });
  var live = (M.mods.shape || [])[i];
  if (HAVEMETER && live !== undefined) { g.fillStyle = "#5fb8c8"; g.fillRect(2, H - 4 - clamp01(live) * (H - 8) - 1, 5, 2); }
  g.fillStyle = "rgba(231,227,217,.5)"; g.font = "8px Consolas,monospace";
  g.fillText(tmax.toFixed(1) + " s", W - 30, H - 6);
  ed.trig.textContent = TRIGN[s.trig] || TRIGN[0];
  ed.loop.classList.toggle("on", !!s.loop);
}

/* ══════════════════════════════════════════════════════════════════════════
   10 · the eight macros and their map editor
   ══════════════════════════════════════════════════════════════════════════ */
var MACSUB = ["weight and physical presence", "suspense and unresolved tension", "abrasion and force", "loss of equilibrium",
              "layers invading one another", "near presence to remote trace", "environmental activity", "recognisable vulnerability"];
var MACID = ["mac_mass", "mac_dread", "mac_violence", "mac_instab", "mac_contam", "mac_distance", "mac_life", "mac_humanity"];
var MACEL = [];
function buildMacros() {
  var host = $("#macros"); host.textContent = ""; MACEL = [];
  MACID.forEach(function (id, m) {
    var w = el("div", "mac", host);
    var nm = el("div", "mn", w, (MACN[m] || PNAME[id] || id));
    nm.addEventListener("click", function () { openMapEditor(m, nm); });
    var pw = el("div", "panel", w); pw.dataset.panel = "MACRO " + (m + 1);
    pw.style.cssText = "flex:none;padding:0;border:none;background:none;box-shadow:none";
    pw._row = el("div", "row", pw); pw._row.style.justifyContent = "center"; pw._want = [[id, ""]]; PANELS.push(pw);
    var sub = el("div", "ms", w, MACSUB[m]);
    var nas = el("div", "nas", w, "—");
    MACEL[m] = { w: w, nas: nas, pw: pw };
  });
}
function drawMacros() {
  MACEL.forEach(function (me, m) {
    var maps = MAPS[m] || [], n = 0, first = "";
    maps.forEach(function (d) { if (d && d.id) { n++; if (!first) first = PNAME[d.id] || d.id; } });
    me.nas.textContent = n ? (n + " → " + first + (n > 1 ? " +" + (n - 1) : "")) : "unmapped";
    me.w.classList.toggle("mapped", n > 0);
  });
}
function openMapEditor(m, anchor) {
  var r = anchor.getBoundingClientRect();
  var pk = pop((MACN[m] || "MACRO") + " — " + MACSUB[m], 330, 300, r.left - 60, r.top - 260);
  var hd = el("div", "maprow", pk.body); ["#", "DESTINATION", "DEPTH", ""].forEach(function (t) { el("div", "ro", hd, t); });
  for (var d = 0; d < 8; d++) (function (d) {
    var row = el("div", "maprow", pk.body);
    el("div", "n", row, String(d + 1));
    var m0 = (MAPS[m] || [])[d] || { id: "", depth: 0 };
    var dst = el("div", "mf dst", row, m0.id ? (PNAME[m0.id] || m0.id) : "—");
    dst.addEventListener("click", function () { openDstChooser(dst, m0.id, function (id) { m0.id = id; if (id && !m0.depth) m0.depth = 0.4; MAPS[m][d] = m0; NB.send({ k: "macro", op: "set", m: m, d: d, id: id, depth: m0.depth }); dst.textContent = id ? (PNAME[id] || id) : "—"; dep._draw(); drawMacros(); }); });
    var dep = numCell(row, function () { return m0.depth || 0; }, function (v, fin) { m0.depth = v; if (fin) { MAPS[m][d] = m0; NB.send({ k: "macro", op: "set", m: m, d: d, id: m0.id, depth: v }); drawMacros(); } }, -1, 1, 0.01, function (v) { return sgn(Math.round(v * 100)) + " %"; });
    var x = el("div", "btn sm", row, "×");
    x.addEventListener("click", function () { m0.id = ""; m0.depth = 0; MAPS[m][d] = m0; NB.send({ k: "macro", op: "set", m: m, d: d, id: "", depth: 0 }); dst.textContent = "—"; dep._draw(); drawMacros(); });
  })(d);
  var fr = el("div", "row", pk.body); fr.style.marginTop = "4px";
  var clr = el("div", "btn sm warn", fr, "CLEAR ALL");
  clr.addEventListener("click", function () { NB.send({ k: "macro", op: "clear", m: m }); MAPS[m] = []; drawMacros(); pk.close(); });
}

/* ══════════════════════════════════════════════════════════════════════════
   11 · the keyboard, the wheels, the foot
   ══════════════════════════════════════════════════════════════════════════ */
var OCT = 3, KEYS = [], KDOWN = {}, VL = [];
var QW = { a:0, w:1, s:2, e:3, d:4, f:5, t:6, g:7, y:8, h:9, u:10, j:11, k:12, o:13, l:14, p:15, ";":16 };
function buildKeyboard() {
  var kb = $("#kb"); kb.textContent = ""; KEYS = [];
  var base = (OCT + 1) * 12, whites = 0;
  for (var i = 0; i < 25; i++) { var pc = i % 12; if ([1,3,6,8,10].indexOf(pc) < 0) whites++; }
  var ww = 100 / whites, wi = 0;
  for (var i2 = 0; i2 < 25; i2++) {
    var pc2 = i2 % 12, black = [1,3,6,8,10].indexOf(pc2) >= 0, n = base + i2;
    var k = el("div", black ? "bk" : "wk", kb);
    if (black) { k.style.left = (wi * ww - ww * 0.3) + "%"; k.style.width = (ww * 0.6) + "%"; }
    else { k.style.left = (wi * ww) + "%"; k.style.width = ww + "%"; wi++; }
    k.dataset.note = n; KEYS[n] = k;
    (function (n, k) {
      k.addEventListener("pointerdown", function (e) { noteOn(n, 0.8); try { k.setPointerCapture(e.pointerId); } catch (x) {} e.preventDefault(); });
      k.addEventListener("pointerup", function () { noteOff(n); });
      k.addEventListener("pointercancel", function () { noteOff(n); });
    })(n, k);
  }
  $("#kbrange").textContent = "  " + noteTxt(base) + " – " + noteTxt(base + 24) + "   A W S E D F T G Y H U J K = keys, Z / X = octave, Esc = all off";
}
function noteOn(n, v) { if (KDOWN[n]) return; KDOWN[n] = 1; NB.send({ k: "note", n: n, on: true, v: v }); if (KEYS[n]) KEYS[n].classList.add("dn"); drawHeld(); }
function noteOff(n) { if (!KDOWN[n]) return; delete KDOWN[n]; NB.send({ k: "note", n: n, on: false, v: 0 }); if (KEYS[n]) KEYS[n].classList.remove("dn"); drawHeld(); }
function allOff() { Object.keys(KDOWN).forEach(function (n) { noteOff(+n); }); NB.send({ k: "alloff" }); }
function drawHeld() {
  var held = Object.keys(KDOWN).map(Number).concat(M.held || []);
  var u = {}; held.forEach(function (n) { u[n] = 1; });
  var arr = Object.keys(u).map(Number).sort(function (a, b) { return a - b; });
  $("#heldtxt").textContent = arr.length ? arr.map(noteTxt).join(" ") : "—";
  for (var n in KEYS) KEYS[n].classList.toggle("dn", !!u[n]);
}
function buildVoiceLamps() { var h = $("#vlamps"); h.textContent = ""; VL = []; for (var i = 0; i < 8; i++) VL.push(el("span", "lamp", h)); }
function bindWheel(sel, spring, send, get) {
  var w = $(sel), i = w.querySelector("i"), down = false, y0 = 0, v0 = 0, val = get();
  function draw() { var t = spring ? (0.5 + 0.5 * val) : val; i.style.top = (2 + (1 - t) * (w.clientHeight - 12)) + "px"; }
  w.addEventListener("pointerdown", function (e) { down = true; y0 = e.clientY; v0 = val; try { w.setPointerCapture(e.pointerId); } catch (x) {} e.preventDefault(); });
  w.addEventListener("pointermove", function (e) { if (!down) return; val = clamp(v0 + (y0 - e.clientY) / DECK_S / 60 * (spring ? 2 : 1), spring ? -1 : 0, 1); send(val); draw(); });
  var up = function () { if (!down) return; down = false; if (spring) { val = 0; send(0); draw(); } };
  w.addEventListener("pointerup", up); w.addEventListener("pointercancel", up);
  w._draw = draw; w._set = function (v) { if (!down) { val = v; draw(); } };
  setTimeout(draw, 50);
}
function hostPanel(sel, d) {
  var p = $(sel); p.dataset.panel = d.t;
  if (d.w) { p.style.width = d.w + "px"; p.style.flex = "none"; } else { p.style.flex = "1"; p.style.minWidth = "0"; }
  var ti = el("div", "pt" + (d.amb ? " amb" : ""), p); el("span", "", ti, d.t);
  p._row = el("div", "row", p); p._want = d.c.slice(); PANELS.push(p);
  return p;
}

/* ══════════════════════════════════════════════════════════════════════════
   12 · the patch menu and the user folder
   ══════════════════════════════════════════════════════════════════════════ */
var CURPATCH = { i: 0, name: "", cat: "", note: "" };
function openPatchMenu() {
  var r = $("#pname").getBoundingClientRect();
  var pk = pop("FACTORY PRESETS", 360, 520, r.left, r.bottom + 6);
  var inp = el("input", "psearch", pk.body); inp.placeholder = "search"; inp.spellcheck = false;
  var list = el("div", "", pk.body);
  function fill(q) {
    list.textContent = ""; q = (q || "").toUpperCase(); var cat = null;
    PRESETS.forEach(function (p) {
      if (q && p.name.toUpperCase().indexOf(q) < 0 && p.cat.toUpperCase().indexOf(q) < 0) return;
      if (p.cat !== cat) { cat = p.cat; el("div", "pcat", list, cat); }
      var it = el("div", "pi" + (p.n === CURPATCH.i ? " on" : ""), list, (p.n + 1) + "  " + p.name);
      it.title = p.note || "";
      it.addEventListener("click", function () { loadPatch(p.n); pk.close(); });
    });
  }
  fill(""); inp.addEventListener("input", function () { fill(inp.value); });
  inp.addEventListener("keydown", function (e) { e.stopPropagation(); if (e.key === "Escape") pk.close(); });
  setTimeout(function () { inp.focus(); }, 20);
}
function loadPatch(i) { i = ((i % PRESETS.length) + PRESETS.length) % PRESETS.length; NB.send({ k: "patch", i: i }); }
function openUserMenu() {
  NB.send({ k: "presetScan" });
  var r = $("#b-user").getBoundingClientRect();
  var pk = pop("USER PATCHES", 360, 520, r.left - 200, r.bottom + 6);
  var top = el("div", "row", pk.body); top.style.marginBottom = "4px";
  var fb = el("div", "btn sm", top, "CHOOSE FOLDER…"); fb.addEventListener("click", function () { NB.send({ k: "presetFolder" }); });
  var fo = el("div", "ro", pk.body, TREE.folder || "—"); fo.style.cssText = "white-space:normal;margin-bottom:4px";
  var list = el("div", "", pk.body);
  function walk(items, host, depth) {
    items.forEach(function (it) {
      if (it.d) { el("div", "pcat", host, it.n); walk(it.i || [], host, depth + 1); return; }
      var e = el("div", "pi", host, it.n); e.style.paddingLeft = (7 + depth * 10) + "px";
      e.addEventListener("click", function () { NB.send({ k: "presetLoad", path: it.p }); pk.close(); });
    });
  }
  function draw() { fo.textContent = TREE.folder || "—"; list.textContent = ""; if (!TREE.exists) el("div", "ro", list, "the folder does not exist yet: SAVE creates it"); walk(TREE.items || [], list, 0); }
  draw(); pk._redraw = draw;
}

/* ══════════════════════════════════════════════════════════════════════════
   13 · the sync functions: every value shown twice, redrawn from one place
   ══════════════════════════════════════════════════════════════════════════ */
function say(msg) { var n = $("#notice"); if (n) n.textContent = String(msg); }
function syncDrone() { var on = V.drone >= 0.5; document.body.classList.toggle("droning", on); var c = CTL.drone; if (c) c.classList.toggle("on", on); }
function syncHist() {
  var pos = HAVEMETER ? M.history : (V.h_pos || 0);
  var here = -1, sp = [0, V.h_sc2pos === undefined ? 0.333 : V.h_sc2pos, V.h_sc3pos === undefined ? 0.667 : V.h_sc3pos, 1];
  for (var i = 0; i < 4; i++) if (Math.abs(pos - sp[i]) < 0.03) here = i;
  [SREC, SBTN].forEach(function (arr) { arr.forEach(function (s, i) { s.t.classList.toggle("here", i === here && V.h_on >= 0.5); }); });
  drawTravel(pos, sp);
}
function syncScenes() {
  [SREC, SBTN].forEach(function (arr) { arr.forEach(function (s, i) {
    s.nm.textContent = SCENES.names[i] || ["AWAKENING","OCCUPATION","COLLAPSE","AFTERMATH"][i];
    s.lp.classList.toggle("on", !!SCENES.set[i]);
    s.inf.textContent = SCENES.set[i] ? "STORED" : "EMPTY · STORE TO FILL";
  }); });
}
function syncMem() { if (!CAPTXT) return; var s = "SOURCE " + fmt("mem_src", V.mem_src || 0); if (M.capturing) s += "  ·  CAPTURING " + M.capLen.toFixed(1) + " s"; else if (M.hasCapture) s += "  ·  CAPTURE HELD" + (M.remembered ? " (REMEMBERED)" : " — REMEMBER TO KEEP IT"); if (M.hasImport) s += "  ·  IMPORT: " + M.importName; CAPTXT.textContent = s; if (CAPBTN) CAPBTN.classList.toggle("on", !!M.capturing); }
function syncPatch() {}
function syncStruct() {}
function syncScale() {}
function drawTravel(pos, sp) {
  if (!HTL) return;
  var g = HTL.getContext("2d"), W = HTL.width, H = HTL.height;
  g.fillStyle = "#0a0c0e"; g.fillRect(0, 0, W, H);
  g.strokeStyle = "rgba(231,227,217,.18)"; g.beginPath(); g.moveTo(8, H / 2); g.lineTo(W - 8, H / 2); g.stroke();
  sp.forEach(function (x, i) { var px = 8 + x * (W - 16); g.fillStyle = SCENES.set[i] ? "#d99a3a" : "#3a342c"; g.fillRect(px - 2, H / 2 - 9, 4, 18); g.fillStyle = "rgba(231,227,217,.55)"; g.font = "7px Segoe UI,sans-serif"; g.textAlign = i === 0 ? "left" : (i === 3 ? "right" : "center"); g.fillText((SCENES.names[i] || "").slice(0, 12), px, H - 4); });
  var cx = 8 + clamp01(pos) * (W - 16);
  g.fillStyle = "#5fb8c8"; g.beginPath(); g.arc(cx, H / 2, 4, 0, 6.283); g.fill();
}

/* ══════════════════════════════════════════════════════════════════════════
   14 · meters and the spectral record. Nothing draws until a meter arrives.
   ══════════════════════════════════════════════════════════════════════════ */
var RECW = [], RECC = [], MOTION = true, CAPON = false, CAPBTN = null, CAPTXT = null, FRLAMP = null, LOOPB2 = null, SPACEB2 = null, MOREP = null, HTL = null;
var FRFLASH = 0;
/*  The record is a picture of what has been played, so it is drawn at the size
    it is shown: a 200x48 bitmap stretched over the box read as a smear. The
    bitmap is sized from the wrapper the first time the wrapper has a size, and
    again if the window is resized past a threshold. */
function recCanvases() {
  RECW.forEach(function (w, i) {
    if (!RECC[i]) {
      var c = el("canvas", "", w); c.id = i === 0 ? "rec" : "";
      c.style.cssText = "display:block;width:100%;height:100%";
      c.width = 2; c.height = 2; RECC[i] = c;
    }
    var cv = RECC[i], ww = w.clientWidth, hh = w.clientHeight;
    if (ww < 8 || hh < 8) return;
    if (ww > 8 && hh > 8 && (Math.abs(cv.width - ww) > 8 || Math.abs(cv.height - hh) > 8)) {
      cv.width = ww; cv.height = hh;
      var g = cv.getContext("2d"); g.fillStyle = "#0a0c0e"; g.fillRect(0, 0, ww, hh);
    }
  });
}
function drawRec() {
  recCanvases();
  var sp = M.spectrum; if (!sp || !sp.length) return;
  RECC.forEach(function (c) {
    if (c.width < 8 || (c.parentNode && c.parentNode.offsetParent === null)) return;
    var g = c.getContext("2d"), W = c.width, H = c.height;
    if (MOTION) g.drawImage(c, -1, 0);
    var bh = H / 48;
    g.fillStyle = "#0a0c0e"; g.fillRect(W - 1, 0, 1, H);
    for (var b = 0; b < 48; b++) {
      var v = clamp01(sp[b]);
      if (v < 0.02) continue;
      g.fillStyle = "rgb(" + Math.round(8 + 88 * v) + "," + Math.round(12 + 172 * v) + "," + Math.round(16 + 188 * v) + ")";
      g.fillRect(W - 1, H - (b + 1) * bh, 1, Math.ceil(bh));
    }
  });
  DRAWS.rec++;
}
function drawScope() {}
function drawMeter() {
  var pk = clamp01(M.peak), o = clamp01(M.out * 1.6);
  $("#vu-out").querySelector("i").style.width = (o * 100) + "%";
  $("#vu-out").querySelector("u").style.left = (pk * 100) + "%";
  $("#vu-lim").querySelector("i").style.width = (clamp01(M.lim * 3) * 100) + "%";
  $("#vutxt").textContent = M.out > 1e-5 ? (20 * Math.log10(M.out)).toFixed(1) + " dB" : "-INF";
  $("#lamp-loop").classList.toggle("on", M.loop > 0.02); $("#bar-loop").style.width = (clamp01(M.loop * 2) * 100) + "%";
  $("#lamp-space").classList.toggle("on", M.space > 0.01); $("#bar-space").style.width = (clamp01(M.space * 3) * 100) + "%";
  if (LOOPB2) LOOPB2.style.width = (clamp01(M.loop * 2) * 100) + "%";
  if (SPACEB2) SPACEB2.style.width = (clamp01(M.space * 3) * 100) + "%";
  $("#ro-voices").innerHTML = "VOICES <b>" + (M.voices | 0) + "</b> / " + (V.voices === undefined ? 8 : Math.round(V.voices));
  $("#ro-grains").innerHTML = "GRAINS <b>" + (M.grains | 0) + "</b> &#183; MEMORY <b>" + Math.round(clamp01(M.memAct * 4) * 100) + "</b>";
  $("#stopban").classList.toggle("on", !!M.stopped);
  for (var i = 0; i < 4; i++) { if (STLAMP[i]) STLAMP[i].classList.toggle("on", M.strata[i] > 0.02); if (STBAR[i]) STBAR[i].style.width = (clamp01(M.strata[i]) * 100) + "%"; }
  if (M.fracture) FRFLASH = 6;
  if (FRLAMP) FRLAMP.classList.toggle("on", FRFLASH > 0);
  if (STFR) STFR.style.boxShadow = FRFLASH > 0 ? "inset 0 0 0 1px var(--verm)" : "";
  if (FRFLASH > 0) FRFLASH--;
  for (var v = 0; v < 8 && v < VL.length; v++) VL[v].classList.toggle("on", M.notes[v] >= 0 && M.levels[v] > 0.02);
  ["lfo","env","shape","rnd","fol","evt"].forEach(function (key) {
    var arr = M.mods[key] || []; (MODBAR[key] || []).forEach(function (b, i) { if (!b) return; var x = arr[i]; if (x === undefined) return; b.style.width = ((key === "lfo" || key === "rnd") ? (50 + 50 * clamp(x, -1, 1)) : (100 * clamp01(x))) + "%"; });
  });
  if (NETB.length) { var ne = M.mods.net || {}; for (var k = 0; k < 5; k++) if (NETB[k]) { NETB[k][0].style.width = (clamp01((ne.e || [])[k]) * 100) + "%"; NETB[k][1].style.width = (clamp01((ne.b || [])[k]) * 100) + "%"; NETB[k][2].style.width = (clamp01((ne.t || [])[k]) * 100) + "%"; } if (NETB.ero) { NETB.ero.style.width = (clamp01(M.erosion) * 100) + "%"; NETB.txt.textContent = Math.round(clamp01(M.erosion) * 100) + " %"; } }
  for (var s = 0; s < 4; s++) if (SHPED[s] && CURV === "life") drawShape(s);
  drawHeld(); syncMem(); syncHist();
  var wb = $("#w-bend"), wm = $("#w-mod"); if (wb && wb._set) wb._set(M.bend || 0); if (wm && wm._set) wm._set(M.wheel || 0);
  if (MOTION || DRAWS.rec === 0) drawRec();
  DRAWS.meter++;
}

/* ══════════════════════════════════════════════════════════════════════════
   15 · scale to fit, decals, header buttons, shortcuts
   ══════════════════════════════════════════════════════════════════════════ */
var DECK_S = 1;
function fitDeck() {
  var s = Math.min(innerWidth / 1440, innerHeight / 900);
  DECK_S = s;
  var d = $("#deck"); d.style.transform = "scale(" + s + ")";
  d.style.left = Math.round((innerWidth - 1440 * s) / 2) + "px";
  d.style.top = Math.round((innerHeight - 900 * s) / 2) + "px";
}
window.addEventListener("resize", fitDeck);
function probeDecals() {
  ["panel","knob","glass","wordmark","screw","plate"].forEach(function (nm) {
    var img = new Image();
    img.onload = function () { if (img.naturalWidth > 8) { document.documentElement.style.setProperty("--decal-" + nm, "url(assets/decals/" + nm + ".png)"); document.documentElement.classList.add("d-" + nm); } };
    img.onerror = function () {};
    img.src = "assets/decals/" + nm + ".png";
  });
}
var MUT = 0.3, LOCK = 64;
function buildHeader() {
  $("#b-prev").addEventListener("click", function () { loadPatch(CURPATCH.i - 1); });
  $("#b-next").addEventListener("click", function () { loadPatch(CURPATCH.i + 1); });
  $("#pname").addEventListener("click", openPatchMenu);
  $("#b-patch").addEventListener("click", openPatchMenu);
  $("#b-user").addEventListener("click", openUserMenu);
  $("#b-save").addEventListener("click", function () { NB.send({ k: "save" }); });
  $("#b-open").addEventListener("click", function () { NB.send({ k: "open" }); });
  var abMode = "recall";
  $("#b-store").addEventListener("click", function () { abMode = abMode === "recall" ? "store" : "recall"; $("#b-store").classList.toggle("on", abMode === "store"); say(abMode === "store" ? "NOW CLICK A OR B TO STORE INTO IT" : "A / B RECALL"); });
  ["A", "B"].forEach(function (s) { $("#b-" + s.toLowerCase()).addEventListener("click", function () { NB.send({ k: "ab", op: abMode, slot: s }); if (abMode === "store") { abMode = "recall"; $("#b-store").classList.remove("on"); } }); });
  $("#b-undo").addEventListener("click", function () { NB.send({ k: "undo" }); });
  $("#b-mutate").addEventListener("click", function () { NB.send({ k: "mutate", amount: MUT, lock: LOCK }); });
  var ma = $("#mutamt"), mi = ma.querySelector("i"), md = false;
  function drawMut() { mi.style.width = (MUT * 100) + "%"; ma.title = "mutation amount " + Math.round(MUT * 100) + " %"; }
  ma.addEventListener("pointerdown", function (e) { md = true; try { ma.setPointerCapture(e.pointerId); } catch (x) {} var r = ma.getBoundingClientRect(); MUT = clamp((e.clientX - r.left) / r.width, 0.05, 1); drawMut(); });
  ma.addEventListener("pointermove", function (e) { if (!md) return; var r = ma.getBoundingClientRect(); MUT = clamp((e.clientX - r.left) / r.width, 0.05, 1); drawMut(); });
  ma.addEventListener("pointerup", function () { md = false; });
  drawMut();
  document.querySelectorAll(".lockchip").forEach(function (c) { c.addEventListener("click", function () { var b = +c.dataset.lock; LOCK ^= b; c.classList.toggle("on", !!(LOCK & b)); }); });
  $("#b-assign").addEventListener("click", function () { setView("life", "matrix"); });
  $("#b-motion").addEventListener("click", function () { MOTION = !MOTION; $("#b-motion").classList.toggle("on", !MOTION); $("#b-motion").textContent = MOTION ? "MOTION" : "STILL"; });
  $("#b-hints").addEventListener("click", function () { setTooltips(!HINTS_ON); });
  setTooltips(HINTS_ON);
  $("#b-panic").addEventListener("click", function () { allOff(); NB.send({ k: "panic" }); say("PANIC — STOPPED"); });
  document.querySelectorAll("#tabs .tab").forEach(function (t) { t.addEventListener("click", function () { setView(t.dataset.tab); }); });
  $("#b-octdn").addEventListener("click", function () { allOff(); OCT = Math.max(0, OCT - 1); buildKeyboard(); });
  $("#b-octup").addEventListener("click", function () { allOff(); OCT = Math.min(6, OCT + 1); buildKeyboard(); });
  bindWheel("#w-bend", true, function (v) { NB.send({ k: "bend", v: v }); }, function () { return 0; });
  bindWheel("#w-mod", false, function (v) { NB.send({ k: "wheel", v: v }); }, function () { return 0; });
  var qp = el("div", "panel", $("#qslot")); qp.dataset.panel = "QUALITY"; qp.style.cssText = "padding:0;border:none;background:none;box-shadow:none";
  qp._row = el("div", "row", qp); qp._want = [["quality", "QUALITY", { w: 86 }]]; PANELS.push(qp);
}
/*  CTRL is the tooltip shortcut: hold it and whatever the pointer is over
    explains itself, without turning them on for good. */
document.addEventListener("keydown", function (e) {
  if (e.key === "Control" && !CTRLHOVER) { CTRLHOVER = true; if (HOVERC) showTip(HOVERC, false); }
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  if (e.repeat) return;
  var k = e.key.toLowerCase();
  if (k === "escape") { allOff(); return; }
  if (k === "z") { allOff(); OCT = Math.max(0, OCT - 1); buildKeyboard(); return; }
  if (k === "x") { allOff(); OCT = Math.min(6, OCT + 1); buildKeyboard(); return; }
  if (QW[k] !== undefined && !e.ctrlKey && !e.altKey && !e.metaKey) { noteOn((OCT + 1) * 12 + QW[k], 0.8); e.preventDefault(); }
});
document.addEventListener("keyup", function (e) {
  if (e.key === "Control") {
    CTRLHOVER = false;
    if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
  }
  if (e.target && (e.target.tagName === "INPUT" || e.target.isContentEditable)) return;
  var k = e.key.toLowerCase(); if (QW[k] !== undefined) noteOff((OCT + 1) * 12 + QW[k]);
});
/*  A window that loses focus never sees the keyup, so CTRL would stay stuck on. */
window.addEventListener("blur", function () {
  CTRLHOVER = false;
  if (!HINTS_ON && tipFor && !tipFor.classList.contains("drag")) hideTip();
});

/* ══════════════════════════════════════════════════════════════════════════
   16 · the events from the native side
   ══════════════════════════════════════════════════════════════════════════ */
function applyInitial(d) {
  BUILD = d.build || "—"; SR = d.sr || 48000; LATENCY = d.latency || 0; MEMLAT = d.memLatency || 0;
  PRESETS = d.presets || []; SOURCES = d.sources || ["—"]; MACN = d.macroNames || [];
  ORDER = []; IDX = {};
  (d.params || []).forEach(function (p, i) {
    V[p.id] = p.v; DEF[p.id] = p.def; HI[p.id] = p.hi; KIND[p.id] = p.kind; LO[p.id] = p.lo; FHI[p.id] = p.fhi; FLAG[p.id] = p.flags || 0; PNAME[p.id] = p.n;
    if (p.names) NAMES[p.id] = p.names;
    ORDER.push(p.id); IDX[p.id] = i;
  });
  EFFV = ORDER.map(function (id) { return V[id]; }); EFFP = EFFV.slice();
  if (!READY) {
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !CTL[id]) buildCtl(id); });
    /*  Every panel that can own an id must exist before the spare list is
        counted - the views, the header, the foot AND the macro rail - or the
        spare box takes a control out of a strip the moment a view opens. */
    buildMacros(); buildPalette();
    countPlaced();
    /*  Presented elsewhere by something that is not a .ctl: the rack fragment
        owns its five macros, and the header's preset window IS the patch dial. */
    var OWNED = { patch: 1 };
    var spare = [];
    ORDER.forEach(function (id) { if (!/^bwfx_/.test(id) && !PLACED[id] && !OWNED[id]) spare.push([id, PNAME[id] || id]); });
    if (MOREP) { MOREP._want = spare; MOREP.hidden = spare.length === 0; MOREP.parentNode && (MOREP.parentNode.style.display = ""); }
    document.querySelectorAll("#head, #foot, #macros").forEach(place);
    setView("main");
    READY = true;
  } else refreshAll();
  $("#binfo").textContent = "BUILD " + BUILD + " · " + ORDER.length + " PARAMETERS · " + Math.round(LATENCY) + " SAMPLES LATENCY";
  NB.send({ k: "ack" });
  requestAnimationFrame(function () { fitDeck(); NB.send({ k: "ready" }); });
}
NB.on("initialState", function (d) { try { applyInitial(d); } catch (e) { window.__TTYERR.push("initialState: " + (e && e.stack || e)); } });
NB.on("hostParam", function (d) { ((d && d.p) || []).forEach(function (q) { if (KIND[q.id] === undefined) return; V[q.id] = q.v; refresh(q.id); }); });
NB.on("eff", function (d) { if (d && d.v && d.v.length) { EFFV = d.v; drawEff(false); } });
NB.on("meter", function (d) { if (!d) return; for (var k in d) M[k] = d[k]; HAVEMETER = true; try { drawMeter(); } catch (e) { window.__TTYERR.push("meter: " + (e && e.stack || e)); } });
NB.on("notice", function (d) { if (d && d.msg) say(d.msg); });
NB.on("patchinfo", function (d) {
  if (!d) return; CURPATCH = { i: d.i, name: d.name || "", cat: d.cat || "", note: d.note || "" };
  $("#pname").textContent = d.name || "—"; $("#pcat").textContent = d.cat || ""; $("#pnote").textContent = d.note || "";
});
NB.on("macros", function (d) { if (d && d.maps) { MAPS = d.maps.map(function (m) { return (m || []).map(function (x) { return { id: x.id || "", depth: +x.depth || 0 }; }); }); while (MAPS.length < 8) MAPS.push([]); drawMacros(); } });
NB.on("slots", function (d) { if (d && d.slots) { d.slots.forEach(function (s) { if (s.i >= 0 && s.i < 32) SLOTS[s.i] = { i: s.i, src: s.src | 0, via: s.via | 0, dst: s.dst || "", depth: +s.depth || 0, offset: +s.offset || 0, curve: s.curve | 0, slew: +s.slew || 0, lo: s.lo === undefined ? -1 : +s.lo, hi: s.hi === undefined ? 1 : +s.hi, on: !!s.on }; }); drawSlots(); } });
NB.on("mseg", function (d) { if (d && d.shapes) { d.shapes.forEach(function (s, i) { if (i < 4) SHAPES[i] = { trig: s.trig | 0, loop: !!s.loop, pts: (s.pts || []).map(function (p) { return { t: +p.t, l: +p.l, c: +p.c || 0 }; }) }; if (i < 4 && SHAPES[i].pts.length < 2) SHAPES[i].pts = [{ t: 0, l: 0, c: 0 }, { t: 1, l: 1, c: 0 }]; }); for (var i = 0; i < 4; i++) drawShape(i); } });
NB.on("scenes", function (d) { if (d) { SCENES.set = (d.set || []).map(function (x) { return x ? 1 : 0; }); SCENES.names = d.names || SCENES.names; syncScenes(); syncHist(); } });
NB.on("presetTree", function (d) { if (d) { TREE = d; POPS.forEach(function (p) { if (p._redraw) p._redraw(); }); } });

/* ══════════════════════════════════════════════════════════════════════════
   17 · the debug hook: the model lives in a closure and a probe has no other
   way to steer or read it (the BWFX.debug() argument).
   ══════════════════════════════════════════════════════════════════════════ */
window.__TTY = {
  version: "260923.1",
  get: function (id) { return V[id]; },
  set: function (id, v) { setP(id, v); },
  eff: function (id) { return effOf(id); },
  state: function () { return { values: V, build: BUILD, patch: CURPATCH, view: CURV, page: CURP[CURV], slots: SLOTS, maps: MAPS, shapes: SHAPES, scenes: SCENES, order: ORDER.length, draws: DRAWS, placed: Object.keys(CTL).length }; },
  meter: function () { return M; },
  /*  A control belonging to a view that is not showing is DETACHED (the views
      move the same node), so a probe cannot reach it with querySelector. */
  ctl: function (id) { return CTL[id]; },
  text: function (id) { var c = CTL[id]; return c ? c.querySelector(".val").textContent.trim() : null; },
  view: function (n, p) { return setView(n, p); },
  patch: function (i) { loadPatch(i); },
  errors: function () { return window.__TTYERR; }
};

/* ══════════════════════════════════════════════════════════════════════════
   18 · boot
   ══════════════════════════════════════════════════════════════════════════ */
buildStage();
FOOTP.forEach(function (d) { hostPanel(d.host, d); });
buildHeader(); buildKeyboard(); buildVoiceLamps(); probeDecals(); fitDeck();
say("THE CONSOLE IS WAKING");
