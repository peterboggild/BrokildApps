/* Headless checks of the pure modules (no browser): schema, migration, palettes, evaluator determinism.
   Run with: npm run check */
const assert = require('assert');
const Formulas = require('../js/formulas.js');
const Pal = require('../js/palettes.js');
const Schema = require('../js/schema.js');
const Ev = require('../js/evaluator.js');

let passed = 0;
function test(name, fn) {
  try {
    fn();
    passed += 1;
    console.log(`ok   ${name}`);
  } catch (err) {
    console.error(`FAIL ${name}\n     ${err.message}`);
    process.exitCode = 1;
  }
}

const finite = (v) => (typeof v === 'number' ? Number.isFinite(v)
  : Array.isArray(v) || ArrayBuffer.isView(v) ? Array.from(v).every(finite)
    : v && typeof v === 'object' ? Object.values(v).every(finite) : true);

test('default project normalises to itself', () => {
  const p = Schema.defaultProject();
  const n = Schema.normalizeProject(JSON.parse(JSON.stringify(p)));
  assert.deepStrictEqual(n, Schema.normalizeProject(JSON.parse(JSON.stringify(n))));
  assert.strictEqual(n.scenes.length, 3);
});

test('garbage input gives a valid project', () => {
  for (const g of [null, 42, 'x', {}, { version: 2 }, { version: 2, scenes: [{ formula: 'nope', params: { power: 'NaN' } }], sections: [{ length: -5 }] }]) {
    const p = Schema.normalizeProject(g);
    assert.ok(p.scenes.length >= 1 && p.sections.length >= 1);
    assert.ok(p.sections.every((s) => s.length >= 1 && p.scenes.some((sc) => sc.id === s.sceneId)));
  }
});

test('v1 prototype project migrates', () => {
  const v1 = {
    name: 'Draft mix', bpm: 128,
    scenes: [{ id: 'scene-a', name: 'Scene A', formula: 'tricorn' }, { id: 'scene-b', name: 'Scene B', formula: 'hybrid' }],
    sections: [{ id: 's2', name: 'Verse', start: 16, length: 16, sceneId: 'scene-b' }, { id: 's1', name: 'Intro', start: 0, length: 16, sceneId: 'scene-a' }],
    modMatrix: [{ source: 'KICK', target: 'camera', amount: 0.7 }]
  };
  const p = Schema.normalizeProject(v1);
  assert.strictEqual(p.version, 2);
  assert.strictEqual(p.bpm, 128);
  assert.deepStrictEqual(p.sections.map((s) => s.name), ['Intro', 'Verse']);
  assert.strictEqual(p.scenes[0].formula, 'julia');
});

test('every formula packs 8 finite params and two rotation matrices', () => {
  Formulas.FORMULAS.forEach((f) => {
    const pk = Formulas.pack(f.id, Formulas.defaults(f.id));
    assert.strictEqual(pk.p.length, 8, f.id);
    assert.ok(finite(pk.p) && pk.rotA.length === 9 && pk.rotB.length === 9, f.id);
    f.params.forEach((p) => assert.ok(p.def >= p.min && p.def <= p.max, `${f.id}.${p.key} default out of range`));
  });
});

test('rotation matrices are orthonormal', () => {
  const m = Formulas.rotMat3([30, -45, 60]);
  const col = (i) => [m[i * 3], m[i * 3 + 1], m[i * 3 + 2]];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  for (let i = 0; i < 3; i += 1) for (let j = 0; j < 3; j += 1) assert.ok(Math.abs(dot(col(i), col(j)) - (i === j ? 1 : 0)) < 1e-9);
});

test('palette colour round trips', () => {
  ['#000000', '#ffffff', '#ff0000', '#6cc4ff', '#123456'].forEach((hex) => {
    const back = Pal.rgbToHex(Pal.linearToSrgb(Pal.oklabToLinear(Pal.hexToOklab(hex))));
    assert.strictEqual(back, hex);
  });
});

test('palette tools keep 2..8 sorted stops', () => {
  const s = Pal.PRESET_BY_ID.viridis.stops;
  [Pal.reverse(s), Pal.spread(s), Pal.resample(s, 5, 'oklab'), Pal.adjust(s, { hue: 40, chroma: 1.3 }), Pal.harmony('#ff8800', 'triadic', 7), Pal.random(7), Pal.blend(s, 'oklab', Pal.PRESET_BY_ID.magma.stops, 'rgb', 0.3)]
    .forEach((st) => {
      assert.ok(st.length >= 2 && st.length <= 8);
      for (let i = 1; i < st.length; i += 1) assert.ok(st[i].p >= st[i - 1].p);
      st.forEach((x) => assert.ok(/^#[0-9a-f]{6}$/.test(x.c)));
    });
  const gpu = Pal.toGPU(s, 'oklab');
  assert.strictEqual(gpu.pos.length, 8);
  assert.strictEqual(gpu.col.length, 24);
});

test('look presets reference existing palettes', () => {
  Schema.LOOK_PRESETS.forEach((p) => assert.ok(Pal.PRESET_BY_ID[p.palette], p.id));
});

// fake analysis with a deterministic kick track
function fakeAnalysis(seconds) {
  const rate = 100;
  const n = seconds * rate;
  const env = new Float32Array(n);
  for (let i = 0; i < n; i += 1) env[i] = Math.exp(-((i % 50) / 8));
  return { rate, duration: seconds, tracks: { 'master.low': env, 'master.rms': env, 'stem:k.env': env, 'stem:k.onset': env } };
}

test('evaluate is deterministic and finite across the song', () => {
  const p = Schema.defaultProject();
  p.stems = [{ id: 'k', name: 'kick.wav', key: '', role: 'KICK' }];
  p.mod.push({ id: 'm', on: true, source: 'stem:k.onset', dest: 'shape.power', depth: 0.1, attack: 0, release: 0.2, lead: 1 });
  const a = fakeAnalysis(90);
  const dur = Ev.songDuration(p, a);
  for (let t = 0; t < dur; t += 0.37) {
    const f1 = Ev.evaluate(p, a, t);
    const f2 = Ev.evaluate(p, a, t);
    assert.ok(finite(f1.gpu) && finite(f1.post), `non-finite at ${t}`);
    assert.deepStrictEqual(JSON.stringify(f1), JSON.stringify(f2));
  }
});

test('stem route moves the formula parameter', () => {
  const p = Schema.defaultProject();
  p.mod = [{ id: 'm', on: true, source: 'stem:k.env', dest: 'shape.power', depth: 0.2, attack: 0, release: 0, lead: 0 }];
  const a = fakeAnalysis(60);
  const sid = p.scenes[0].id; // mandelbulb
  const quiet = Ev.evaluate(p, null, 0.05, { sceneId: sid });
  const hit = Ev.evaluate(p, a, 0.0, { sceneId: sid });
  assert.ok(hit.gpu.p[0] > quiet.gpu.p[0] + 1, `power ${quiet.gpu.p[0]} -> ${hit.gpu.p[0]}`);
});

// ---------- live input (microphone) ----------
global.window = global.window || global;
require('../js/audio.js');
const LiveAudio = global.window.FV.Audio;

// 10 ms frames [rms, low, mid, high]: a kick (loud lows) every 50 frames over a quiet bed
function feedKicks(live, seconds) {
  const kicks = [];
  for (let i = 0; i < seconds * 100; i += 1) {
    const k = i % 50 < 3;
    live._frame(k ? [0.5, 0.45, 0.1, 0.02] : [0.02, 0.01, 0.015, 0.01]);
    kicks.push({ i, k, onset: live.raw['master.onset'], low: live.raw['master.low'] });
  }
  return kicks;
}

test('live input: kicks give onset pulses and lows, everything stays in 0..1', () => {
  const live = new LiveAudio.LiveInput();
  live.active = true; // as after start(), without a microphone
  const fr = feedKicks(live, 6);
  const later = fr.filter((f) => f.i >= 100);
  const peaks = later.filter((f, j) => j > 0 && f.onset > 0.5 && later[j - 1].onset <= 0.5);
  assert.ok(peaks.length >= 8 && peaks.length <= 10, `${peaks.length} onsets for 10 kicks`);
  peaks.forEach((p) => assert.ok(p.i % 50 >= 2 && p.i % 50 <= 6, `onset ${p.i % 50} frames after the kick`));
  assert.ok(later.filter((f) => f.k).every((f) => f.low > 0.8), 'lows near full scale on a kick');
  assert.ok(later.filter((f) => f.i % 50 === 30).every((f) => f.low < 0.1 && f.onset < 0.2), 'quiet between kicks');
  Object.values(live.raw).forEach((v) => assert.ok(v >= 0 && v <= 1, `value ${v}`));
});

test('live input: a quiet room is not blown up to full scale', () => {
  const live = new LiveAudio.LiveInput();
  live.active = true;
  for (let i = 0; i < 1000; i += 1) live._frame([0.0008, 0.0005, 0.0004, 0.0003]);
  assert.ok(live.raw['master.rms'] < 0.3, `noise floor reads ${live.raw['master.rms']}`);
});

test('live input drives master routes in the evaluator; movies (live: null) ignore it', () => {
  const p = Schema.defaultProject();
  p.mod = [{ id: 'm', on: true, source: 'master.low', dest: 'shape.power', depth: 0.2, attack: 0, release: 0.1, lead: 0 }];
  const sid = p.scenes[0].id;
  const live = new LiveAudio.LiveInput();
  const an = { rate: 100, duration: 0, tracks: {}, master: null, stems: {}, live };
  const off = Ev.evaluate(p, an, 1, { sceneId: sid });
  live.active = true;
  Ev.evaluate(p, an, 1, { sceneId: sid }); // registers the route's smoothing
  for (let i = 0; i < 30; i += 1) live._frame([0.5, 0.5, 0.1, 0.05]);
  const loud = Ev.evaluate(p, an, 1, { sceneId: sid });
  assert.ok(loud.gpu.p[0] > off.gpu.p[0] + 1, `power ${off.gpu.p[0]} -> ${loud.gpu.p[0]}`);
  const file = Ev.evaluate(p, Object.assign({}, an, { live: null }), 1, { sceneId: sid });
  assert.strictEqual(file.gpu.p[0], off.gpu.p[0]);
  // release: after the sound stops the route falls back smoothly, not at once
  for (let i = 0; i < 3; i += 1) live._frame([0, 0, 0, 0]);
  const fading = Ev.evaluate(p, an, 1, { sceneId: sid });
  assert.ok(fading.gpu.p[0] > off.gpu.p[0] + 0.5 && fading.gpu.p[0] < loud.gpu.p[0], 'release is applied');
  live.stop();
  assert.strictEqual(Ev.evaluate(p, an, 1, { sceneId: sid }).gpu.p[0], off.gpu.p[0], 'stopped = no effect');
});

// ---------- VR mode ----------
const VrMode = require('../js/vr-mode.js');
const { Rig, Perf } = VrMode;
const VM = VrMode.math;
const near = (a, b, eps, what) => a.forEach((v, i) => assert.ok(Math.abs(v - b[i]) < (eps || 1e-6), `${what || ''} [${a.map((x) => x.toFixed(4))}] vs [${b.map((x) => x.toFixed(4))}]`));
const roomOf = (rig, w) => VM.mul(VM.mv(VM.mT(rig.R), VM.sub(w, rig.O)), 1 / rig.S);
const g0 = { camPos: [0, 0.5, 3], camFwd: VM.norm([0, -0.5, -3]), camUp: [0, 1, 0], camRight: [1, 0, 0] };

/* The VR panel's tab grid.

   The panel is a canvas in the headset, so nothing here can be checked by eye without putting on
   a Quest; what can be checked is that the arithmetic keeps every tile on the panel and clear of
   the action bar, for every tab, at the sizes the panel actually uses. A tile that lands under the
   bar is the failure that matters: it still takes the trigger press, but you cannot see it. */
const PANEL = { cw: 1280, ch: 900, top: 160, bar: 900 - 110 };

function tabContents() {
  const byCat = {};
  Formulas.CATEGORIES.forEach((c) => { byCat[c.label] = Formulas.FORMULAS.filter((f) => f.cat === c.id).length; });
  return byCat;
}

test('VR panel: every family fits above the action bar, at a size worth reading', () => {
  const counts = tabContents();
  Object.keys(counts).forEach((label) => {
    const rects = VrMode.gridRects(counts[label], 3, 14, 170, PANEL.top, PANEL.bar - 16, PANEL.cw);
    assert.ok(rects.length === counts[label], label);
    rects.forEach((r) => {
      assert.ok(r.x >= 0 && r.x + r.w <= PANEL.cw, `${label}: tile runs off the side`);
      assert.ok(r.y >= PANEL.top - 1e-9, `${label}: tile above the body`);
      assert.ok(r.y + r.h <= PANEL.bar - 16 + 1e-9, `${label}: tile reaches under the action bar`);
    });
    // 32px type needs the tile to be tall enough to hold it with room to spare
    assert.ok(rects[0].h >= 60, `${label}: tiles only ${rects[0].h.toFixed(0)}px tall`);
  });
});

test('VR panel: the looks and the palettes both fit, and the palettes stay legible', () => {
  const looks = VrMode.gridRects(Schema.LOOK_PRESETS.length, 4, 12, 112, PANEL.top, PANEL.bar - 16, PANEL.cw);
  looks.forEach((r) => assert.ok(r.y + r.h <= PANEL.bar - 16 + 1e-9, 'a look tile reaches under the action bar'));
  assert.ok(looks[0].h >= 60, `look tiles only ${looks[0].h.toFixed(0)}px tall`);

  const pals = VrMode.gridRects(Pal.PRESETS.length, 6, 8, 66, PANEL.top, PANEL.bar - 16, PANEL.cw);
  pals.forEach((r) => assert.ok(r.y + r.h <= PANEL.bar - 16 + 1e-9, 'a palette tile reaches under the action bar'));
  // the names are small here on purpose; they still need a line of type to sit on
  assert.ok(pals[0].h >= 34, `palette tiles only ${pals[0].h.toFixed(0)}px tall`);
  assert.ok(pals[0].w >= 150, `palette tiles only ${pals[0].w.toFixed(0)}px wide for a name`);
});

test('VR panel: no family has grown past what three columns can show', () => {
  const counts = tabContents();
  Object.keys(counts).forEach((label) => {
    assert.ok(counts[label] <= 12, `${label} has ${counts[label]} fractals; past 12 the tabs need a second page`);
  });
});

test('VR rig: the ride rig sits at the scene camera, level, facing the target', () => {
  const rig = Rig.fromCamera(g0, 2);
  near(Rig.toWorld(rig, [0, 0, 0]), g0.camPos, 1e-9, 'origin');
  const fwd = VM.mv(rig.R, [0, 0, -1]);
  assert.ok(Math.abs(fwd[1]) < 1e-9 && fwd[2] < -0.99, 'looks along -z, level');
});

test('VR rig: pulling the hands apart makes the fractal bigger about the point between the hands', () => {
  const rig = Rig.fromCamera(g0, 2);
  const a = [-0.1, 1.2, -0.4], b = [0.1, 1.2, -0.4];
  const st = Rig.pinchStart(rig, a, b);
  const r2 = Rig.pinch(st, [-0.2, 1.2, -0.4], [0.2, 1.2, -0.4], 1e-4, 100);
  assert.ok(Math.abs(r2.S - rig.S / 2) < 1e-9, `scale ${r2.S}`);
  near(Rig.toWorld(r2, [0, 1.2, -0.4]), Rig.toWorld(rig, [0, 1.2, -0.4]), 1e-9, 'midpoint stays');
  // a world point 1 room-metre right of the midpoint now appears 2 m away: twice as big
  const w = Rig.toWorld(rig, [1, 1.2, -0.4]);
  near(roomOf(r2, w), [2, 1.2, -0.4], 1e-9, 'doubled');
});

test('VR rig: a grip twist turns the fractal with the hand, about its target', () => {
  const rig = Rig.fromCamera(g0, 2);
  const pivot = [0, 0, 0];
  const h0 = VM.axisAngle([0, 0, 1], 0.3);
  const st = Rig.twistStart(rig, h0, pivot);
  const D = VM.axisAngle([0, 1, 0], Math.PI / 2);
  const r2 = Rig.twist(st, VM.mm(D, h0));
  near(roomOf(r2, pivot), roomOf(rig, pivot), 1e-9, 'pivot stays put');
  const w = [0.5, 0, 0];
  const before = VM.sub(roomOf(rig, w), roomOf(rig, pivot));
  const after = VM.sub(roomOf(r2, w), roomOf(r2, pivot));
  near(after, VM.mv(D, before), 1e-9, 'offset rotated by the hand');
});

test('VR rig: turning keeps the head in place, flying moves by S x the room step', () => {
  const rig = Rig.fromCamera(g0, 2);
  const head = [0.1, 1.6, 0.2];
  const r2 = Rig.turn(rig, head, 0.7);
  near(Rig.toWorld(r2, head), Rig.toWorld(rig, head), 1e-9, 'head');
  const r3 = Rig.fly(rig, [0, 0, -1]);
  near(VM.sub(r3.O, rig.O), VM.mul(VM.mv(rig.R, [0, 0, -1]), 2), 1e-9, 'fly');
  const r4 = Rig.pulse(rig, [0, 0, 0], 1.5);
  near(Rig.toWorld(r4, [0, 0, 0]), VM.mul(rig.O, 1.5), 1e-9, 'pulse scales about the target');
});

test('VR frame-rate guard: drops at once on late frames, climbs back slowly, offers a way out', () => {
  const p = new Perf(72, { start: 0.8, min: 0.3 });
  for (let i = 0; i < 100; i += 1) p.frame(13.9);
  const ok = p.res;
  assert.ok(ok >= 0.8, `steady at ${ok}`);
  p.frame(60); p.frame(60);
  assert.ok(p.res <= ok * 0.61, `two very late frames: ${ok} -> ${p.res}`);
  for (let i = 0; i < 400; i += 1) p.frame(13.9);
  assert.ok(p.res > ok * 0.61 && p.res <= 1, `recovers to ${p.res}`);
  const q = new Perf(72, { start: 0.8, min: 0.3 });
  let at = -1;
  for (let i = 0; i < 200 && at < 0; i += 1) { q.frame(70); if (q.heavy) at = i; }
  assert.ok(Math.abs(q.res - 0.3) < 1e-9, `at the floor: ${q.res}`);
  assert.ok(at > 0 && at < 30, `heavy after ${at} frames of 14 fps`);
  const r = new Perf(72, { start: 0.8, min: 0.3 });
  for (let i = 0; i < 400; i += 1) r.frame(20);
  assert.ok(!r.heavy, '50 fps is not heavy');
});

test('VR sound recipes: a tile adds a route for this scene only and toggles it off and on', () => {
  const p = Schema.defaultProject();
  const sid = p.scenes[0].id;
  const r = VrMode.RECIPES.find((x) => x.id === 'mids');
  const n = p.mod.length;
  VrMode.toggleRecipe(p, sid, r, 2, Schema.uid);
  assert.strictEqual(p.mod.length, n + 1);
  const m = p.mod[p.mod.length - 1];
  assert.ok(VrMode.recipeOn(p, sid, r) && !VrMode.recipeOn(p, p.scenes[1].id, r), 'only in this scene');
  assert.ok(Math.abs(m.depth - r.depth * 2) < 1e-12, 'wild doubles the depth');
  VrMode.setSensitivity(p, sid, 0.5);
  assert.ok(Math.abs(m.depth - r.depth * 0.5) < 1e-12, 'calm halves it');
  VrMode.toggleRecipe(p, sid, r, 1, Schema.uid);
  assert.ok(!VrMode.recipeOn(p, sid, r) && p.mod.length === n + 1, 'off again, route kept');
  const q = Schema.normalizeProject(JSON.parse(JSON.stringify(p)));
  assert.strictEqual(q.mod.length, p.mod.length, 'survives a save');
  // the default project's bass route counts as the Bass recipe
  assert.ok(VrMode.recipeOn(p, sid, VrMode.RECIPES[0]), 'default bass route is on');
});

test('VR way out: fastest() prefers measured speed, then the prior, and never offers the current one', () => {
  const ids = Formulas.FORMULAS.map((f) => ({ id: f.id }));
  const a = VrMode.fastest(ids, {}, 'sierpinski', 3);
  assert.ok(!a.includes('sierpinski') && a.length === 3, a.join());
  const b = VrMode.fastest(ids, { ridges: 5 }, 'mandelbulb', 3);
  assert.strictEqual(b[0], 'ridges');
});

test('transitions blend into the next scene and land on the downbeat', () => {
  const p = Schema.defaultProject();
  const starts = Ev.sectionStarts(p);
  const s1 = starts[1];
  const tr = p.sections[1].transition;
  const before = Ev.evaluate(p, null, Ev.timeAtBeat(p, s1 - tr - 0.01), { modulate: false });
  const mid = Ev.evaluate(p, null, Ev.timeAtBeat(p, s1 - tr / 2), { modulate: false });
  const at = Ev.evaluate(p, null, Ev.timeAtBeat(p, s1 + 0.001), { modulate: false });
  assert.strictEqual(before.blend, 0);
  assert.ok(mid.blend > 0.3 && mid.blend < 0.7);
  assert.strictEqual(at.sceneId, p.sections[1].sceneId);
  assert.strictEqual(at.blend, 0);
});

test('a route switched off in a scene does nothing there, and fades with the transition', () => {
  const p = Schema.defaultProject();
  const [A, B] = [p.sections[0].sceneId, p.sections[1].sceneId];
  assert.notStrictEqual(A, B);
  p.mod = [{ id: 'm', off: [B], source: 'grid.beatPulse', dest: 'look.exposure', depth: 0.2, attack: 0, release: 0, lead: 0 }];
  const t = 0;
  const glow = (proj, opts) => Ev.evaluate(proj, null, t, opts).post.exposure;
  const bare = (sid) => glow(Object.assign({}, p, { mod: [] }), { sceneId: sid });
  assert.ok(glow(p, { sceneId: A }) > bare(A) + 1e-6, 'on in A');
  assert.strictEqual(glow(p, { sceneId: B }), bare(B), 'off in B');
  // across the A -> B transition the route's share follows the blend weight
  const s1 = Ev.sectionStarts(p)[1];
  const tr = p.sections[1].transition;
  const tm = Ev.timeAtBeat(p, s1 - tr / 2);
  const on = Ev.evaluate(p, null, tm);
  const allOn = Ev.evaluate(Object.assign({}, p, { mod: [Object.assign({}, p.mod[0], { off: [] })] }), null, tm);
  const none = Ev.evaluate(Object.assign({}, p, { mod: [] }), null, tm);
  const share = (on.post.exposure - none.post.exposure) / Math.max(1e-9, allOn.post.exposure - none.post.exposure);
  if (allOn.post.exposure - none.post.exposure > 1e-6) assert.ok(Math.abs(share - (1 - on.blend)) < 0.02, `share ${share} vs ${1 - on.blend}`);
});

test('old global on/off migrates to per-scene off', () => {
  const raw = JSON.parse(JSON.stringify(Schema.defaultProject()));
  raw.mod = [{ id: 'a', on: false, source: 'grid.beatPulse', dest: 'look.glow', depth: 0.1 },
    { id: 'b', on: true, source: 'grid.beatPulse', dest: 'look.glow', depth: 0.1 },
    { id: 'c', off: ['nope', raw.scenes[1].id, raw.scenes[1].id], source: 'grid.beatPulse', dest: 'look.glow', depth: 0.1 }];
  const n = Schema.normalizeProject(raw);
  assert.deepStrictEqual(n.mod[0].off.slice().sort(), n.scenes.map((s) => s.id).sort());
  assert.deepStrictEqual(n.mod[1].off, []);
  assert.deepStrictEqual(n.mod[2].off, [raw.scenes[1].id]);
});

test('keyframes set the value, ease between keys, hold outside, and act only in their scenes', () => {
  const p = Schema.defaultProject();
  const [s0, s1] = p.sections;
  const A = s0.sceneId;
  const B = s1.sceneId;
  const base = p.scenes.find((s) => s.id === A).look.exposure;
  p.mod = [];
  p.keys = [{ id: 'k', dest: 'look.exposure', interp: 'linear', off: [B, p.sections[2].sceneId], pts: [{ sec: s0.id, b: 4, v: 1 }, { sec: s0.id, b: 12, v: 2 }] }];
  const at = (beat, opts) => Ev.evaluate(p, null, Ev.timeAtBeat(p, beat), opts).post.exposure;
  assert.ok(Math.abs(at(0) - 1) < 1e-9, `held before first key: ${at(0)}`);
  assert.ok(Math.abs(at(8) - 1.5) < 1e-9, `linear midpoint: ${at(8)}`);
  assert.ok(Math.abs(at(20) - 2) < 1e-9, 'held after last key');
  p.keys[0].interp = 'smooth';
  assert.ok(Math.abs(at(5) - (1 + (0.125 * 0.125 * (3 - 0.25)))) < 1e-9, 'smoothstep');
  p.keys[0].interp = 'step';
  assert.strictEqual(at(11.9), 1);
  // scene B's own value is untouched (the lane is off there), in timeline and scene views
  const baseB = p.scenes.find((s) => s.id === B).look.exposure;
  assert.strictEqual(at(Ev.sectionStarts(p)[1] + 10), baseB);
  assert.strictEqual(Ev.evaluate(p, null, 0, { sceneId: B }).post.exposure, baseB);
  assert.strictEqual(Ev.evaluate(p, null, 0, { sceneId: A, keys: false }).post.exposure, base);
  // routes still add on top of a keyed value
  p.keys[0].interp = 'linear';
  p.mod = [{ id: 'm', off: [], source: 'grid.lfo16', dest: 'look.exposure', depth: 0.1, attack: 0, release: 0, lead: 0 }];
  assert.ok(at(8) > 1.5, 'route adds on top of the key');
});

test('keys move with their section and keep their place when sections split or merge', () => {
  const p = Schema.defaultProject();
  p.mod = [];
  p.keys = [{ id: 'k', dest: 'look.exposure', interp: 'linear', off: [], pts: [{ sec: p.sections[1].id, b: 8, v: 3 }, { sec: p.sections[1].id, b: 40, v: 1 }] }];
  const beats = () => Ev.keyPoints(p, p.keys[0]).map((x) => x.beat);
  const before = beats();
  p.sections[0].length += 8; // lengthen the section before: keys follow
  assert.deepStrictEqual(beats(), before.map((b) => b + 8));
  // keyframe helpers (loaded the same way the page does)
  global.window = { FV: { ui: { h: () => ({}) } }, FVSchema: Schema, FVEvaluator: Ev };
  require('../js/keys.js');
  const K = global.window.FV.Keys;
  const ref = beats();
  const sec = p.sections[1];
  const second = { id: 'sec-b', name: 'b', length: sec.length - 16, sceneId: sec.sceneId, transition: 0 };
  sec.length = 16;
  p.sections.splice(2, 0, second);
  K.sectionSplit(p, sec.id, second.id, 16);
  assert.deepStrictEqual(beats(), ref, 'split keeps absolute positions');
  assert.strictEqual(p.keys[0].pts[1].sec, 'sec-b');
  const removed = p.sections.splice(1, 1)[0];
  const into = p.sections[0];
  K.sectionMerged(p, removed.id, removed.length, into.id, into.length, true);
  into.length += removed.length;
  assert.deepStrictEqual(beats(), ref, 'merge keeps absolute positions');
  const n = Schema.normalizeProject(JSON.parse(JSON.stringify(p)));
  assert.strictEqual(n.keys[0].pts.length, 2, 'survives save/load');
  delete global.window;
});

test('camera keys: short way round, log zoom, orbit on top, cut with a formula change', () => {
  const p = Schema.defaultProject();
  p.mod = [];
  const s0 = p.sections[0];
  const sc = p.scenes.find((s) => s.id === s0.sceneId);
  sc.camera.orbit = 0;
  const lane = (dest, a, b) => ({ id: dest, dest, interp: 'linear', off: [], pts: [{ sec: s0.id, b: 0, v: a }, { sec: s0.id, b: 8, v: b }] });
  p.keys = [lane('cam.yaw', 170, -170), lane('cam.dist', 1, 4)];
  const n = Schema.normalizeProject(JSON.parse(JSON.stringify(p)));
  assert.strictEqual(n.keys.length, 2, 'camera lanes survive normalisation');
  const at = (beat, q) => Ev.evaluate(q || p, null, Ev.timeAtBeat(p, beat)).camera;
  const mid = at(4);
  assert.ok(Math.abs(Math.abs(mid.yaw) - 180) < 1e-6, `yaw goes through 180, not 0 (got ${mid.yaw})`);
  assert.ok(Math.abs(mid.dist - 2) < 1e-6, `distance halfway in log is 2 (got ${mid.dist})`);
  sc.camera.orbit = 10; // degrees per bar, added to the keyed yaw
  const bars = 4 / (p.beatsPerBar || 4);
  const yawOrb = at(4).yaw;
  const dd = ((((yawOrb - (180 + 10 * bars)) % 360) + 540) % 360) - 180;
  assert.ok(Math.abs(dd) < 1e-6, `auto-orbit adds to a keyed yaw (got ${yawOrb})`);
  sc.camera.orbit = 0;
  // a lane on only in scene A: across a formula change it cuts at the midpoint instead of fading
  const s1 = p.sections[1];
  const B = p.scenes.find((s) => s.id === s1.sceneId);
  if (B && B.id !== sc.id) {
    B.formula = sc.formula === 'mandelbox' ? 'mandelbulb' : 'mandelbox';
    B.camera = Schema.defaultCamera(B.formula);
    s1.transition = 4;
    p.keys = [{ id: 'y', dest: 'cam.pitch', interp: 'step', off: [B.id], pts: [{ sec: s0.id, b: 0, v: 60 }] }];
    const end = Ev.sectionStarts(p)[1];
    const before = Ev.evaluate(p, null, Ev.timeAtBeat(p, end - 2.5)).camera.pitch; // the cut is halfway through the transition
    const after = Ev.evaluate(p, null, Ev.timeAtBeat(p, end - 1.5)).camera.pitch;
    assert.ok(Math.abs(before - 60) < 1e-6, `scene A keeps its key up to the cut (got ${before})`);
    assert.ok(Math.abs(after - B.camera.pitch) < 1e-6, `scene B has its own camera after the cut (got ${after})`);
  }
});

test('render plan covers the song', () => {
  const p = Schema.defaultProject();
  const plan = Ev.renderPlan(p, null, { fps: 25 });
  assert.strictEqual(plan.frames, Math.ceil(Ev.songDuration(p, null) * 25));
});

// ---------- auto-flight (async) ----------
const Flight = require('../js/flight.js');
const atest = async (name, fn) => {
  try { await fn(); passed += 1; console.log(`ok   ${name}`); } catch (err) { console.error(`FAIL ${name}\n     ${err.message}`); process.exitCode = 1; }
};
// An exact probe for a signed distance function, marching rays the way the GPU probe does.
const sdfProbe = (sdf, grad) => (frame, pos, dirs, maxRay, eps) => {
  const rays = dirs.map((d) => {
    let t = 0;
    for (let i = 0; i < 160; i += 1) {
      const h = sdf([pos[0] + d[0] * t, pos[1] + d[1] * t, pos[2] + d[2] * t]);
      if (h < eps * 0.5) break;
      t += h;
      if (t >= maxRay) { t = maxRay; break; }
    }
    return Math.min(t, maxRay);
  });
  return { d: sdf(pos), grad: grad(pos), rays };
};
function flightProject(inertia) {
  const p = Schema.defaultProject();
  p.flight.on = [p.sections[0].sceneId];
  p.flight.settings.inertia = inertia || 0; // 0 = the drone the first tests were written for
  return p;
}
// Largest second difference of the height between stored samples: how hard the flight changes its climb.
const jerkY = (pts) => { let m = 0; for (let i = 2; i < pts.length; i += 1) m = Math.max(m, Math.abs(pts[i][1] - 2 * pts[i - 1][1] + pts[i - 2][1])); return m; };
// Samples the playback actually shows: up to the cut into the next (unflown, other-formula) scene.
const samples = (track, p) => {
  const cut = Ev.sectionStarts(p)[1] - p.sections[1].transition / 2;
  const out = [];
  track.runs.forEach((r) => { for (let i = 0; i < r.pts.length; i += 6) if (r.b0 + (i / 6) * track.step < cut) out.push(r.pts.slice(i, i + 6)); });
  return out;
};

(async () => {
  await atest('auto-flight skims a ground plane at the chosen height and never touches it', async () => {
    const p = flightProject();
    const alt = p.flight.settings.height * Formulas.get(p.scenes.find((s) => s.id === p.flight.on[0]).formula).size;
    const res = await Flight.compute(p, null, sdfProbe((q) => q[1], () => [0, 1, 0]), { yieldEvery: 0 });
    assert.ok(res.track && res.track.runs.length === 1, 'one run for the one flown section');
    const pts = samples(res.track, p);
    const hs = pts.map((s) => s[1]);
    assert.ok(Math.min(...hs) >= 0.25 * alt, `lowest point ${Math.min(...hs).toFixed(4)} vs margin ${(0.25 * alt).toFixed(4)}`);
    const late = hs.slice(Math.floor(hs.length / 2));
    const mean = late.reduce((a, b) => a + b, 0) / late.length;
    assert.ok(mean > 0.6 * alt && mean < 1.6 * alt, `cruise height ${mean.toFixed(4)} vs ${alt.toFixed(4)}`);
    // it actually travels
    const a = pts[0], b = pts[pts.length - 1];
    assert.ok(Math.hypot(b[0] - a[0], b[2] - a[2]) > 1, 'flew some distance');
    // playback: the evaluator puts the camera on the stored track, only in the flown section, not in scene view
    p.flight.track = res.track;
    const r0 = res.track.runs[0];
    const beat = r0.b0 + 8;
    const fr = Ev.evaluate(p, null, Ev.timeAtBeat(p, beat));
    const cam = Ev.cameraBasis(fr.camera);
    const k = (beat - r0.b0) / res.track.step;
    assert.ok(Math.hypot(cam.pos[0] - r0.pts[k * 6], cam.pos[1] - r0.pts[k * 6 + 1], cam.pos[2] - r0.pts[k * 6 + 2]) < 1e-6, 'camera on the track');
    const sv = Ev.evaluate(p, null, Ev.timeAtBeat(p, beat), { sceneId: p.flight.on[0] });
    assert.ok(Math.abs(sv.camera.dist - p.scenes[0].camera.dist) < 1e-9 || sv.camera.dist > 0.5, 'scene view keeps the scene camera');
    const other = Ev.evaluate(p, null, Ev.timeAtBeat(p, Ev.sectionStarts(p)[1] + 20));
    assert.ok(other.camera.dist > 0.5, 'unflown section keeps its own camera');
    // up to date until something it depends on changes
    assert.strictEqual(res.track.sig, Ev.flightSig(p));
    p.scenes[0].params[Object.keys(p.scenes[0].params)[0]] += 0.1;
    assert.notStrictEqual(res.track.sig, Ev.flightSig(p));
    // and the stored track survives save/load
    p.flight.track = res.track;
    const n = Schema.normalizeProject(JSON.parse(JSON.stringify(p)));
    assert.strictEqual(n.flight.track.runs[0].pts.length, r0.pts.length);
  });

  await atest('auto-flight approaches a sphere from outside and circles it without entering', async () => {
    const p = flightProject();
    const alt = p.flight.settings.height * Formulas.get(p.scenes[0].formula).size;
    const R = 1;
    const res = await Flight.compute(p, null, sdfProbe((q) => Math.hypot(q[0], q[1], q[2]) - R, (q) => q), { yieldEvery: 0 });
    const ds = samples(res.track, p).map((s) => Math.hypot(s[0], s[1], s[2]) - R);
    assert.ok(Math.min(...ds) >= 0.25 * alt, `closest ${Math.min(...ds).toFixed(4)}`);
    const late = ds.slice(Math.floor(ds.length / 2));
    assert.ok(Math.max(...late) < 3 * alt, `stays near the surface (furthest late ${Math.max(...late).toFixed(3)})`);
    assert.strictEqual(res.stats.restarts, 1, 'restarts once at the cut to another formula');
  });

  await atest('cinematic flight (inertia): settles to the chosen height, never touches, and moves more smoothly than the drone', async () => {
    const plane = sdfProbe((q) => q[1], () => [0, 1, 0]);
    const pc = flightProject(0.7);
    const alt = pc.flight.settings.height * Formulas.get(pc.scenes[0].formula).size;
    const cin = samples((await Flight.compute(pc, null, plane, { yieldEvery: 0 })).track, pc);
    const pd = flightProject(0);
    const dro = samples((await Flight.compute(pd, null, plane, { yieldEvery: 0 })).track, pd);
    const hs = cin.map((x) => x[1]);
    assert.ok(Math.min(...hs) >= 0.25 * alt, `lowest ${Math.min(...hs).toFixed(4)}`);
    const late = hs.slice(Math.floor(hs.length * 0.6));
    const mean = late.reduce((a, b) => a + b, 0) / late.length;
    assert.ok(mean > 0.6 * alt && mean < 1.6 * alt, `cruise ${mean.toFixed(4)} vs ${alt.toFixed(4)}`);
    assert.ok(jerkY(cin) < 0.6 * jerkY(dro), `climb changes ${jerkY(cin).toFixed(5)} vs drone ${jerkY(dro).toFixed(5)}`);
  });

  await atest('cinematic flight reads the ground ahead: it is already climbing when it reaches a ridge, and clears it', async () => {
    // a ring-shaped ridge round the origin, right across the path of a camera that starts outside it looking in
    const Rr = 1.4, Hr = 0.55, Wr = 0.22;
    const ground = (q) => Hr * Math.exp(-Math.pow((Math.hypot(q[0], q[2]) - Rr) / Wr, 2));
    const sdf = (q) => (q[1] - ground(q)) * 0.4;
    const grad = (q) => { const e = 1e-4; return [(sdf([q[0] + e, q[1], q[2]]) - sdf([q[0] - e, q[1], q[2]])) / (2 * e), (sdf([q[0], q[1] + e, q[2]]) - sdf([q[0], q[1] - e, q[2]])) / (2 * e), (sdf([q[0], q[1], q[2] + e]) - sdf([q[0], q[1], q[2] - e])) / (2 * e)]; };
    const probe = sdfProbe(sdf, grad);
    const run = async (ia) => {
      const p = flightProject(ia);
      p.flight.settings.wander = 0;
      p.scenes[0].camera.dist = 4.2; // start well outside the ring
      return { p, pts: samples((await Flight.compute(p, null, probe, { yieldEvery: 0 })).track, p) };
    };
    const c = await run(0.7), d = await run(0);
    const alt = c.p.flight.settings.height * Formulas.get(c.p.scenes[0].formula).size;
    for (const [name, r] of [['cinematic', c], ['drone', d]]) {
      const clear = Math.min(...r.pts.map((x) => x[1] - ground(x)));
      assert.ok(clear >= 0.25 * alt * 0.4, `${name} clearance ${clear.toFixed(4)}`);
    }
    // height above the local ground when first entering the ridge's flank
    const atFlank = (pts) => { const k = pts.findIndex((x) => Math.hypot(x[0], x[2]) < Rr + 2.2 * Wr); return k < 0 ? null : pts[k][1] - ground(pts[k]); };
    const hc = atFlank(c.pts), hd = atFlank(d.pts);
    assert.ok(hc != null && hd != null, 'both reach the ridge');
    assert.ok(hc > hd * 1.3, `climbing early: ${hc.toFixed(3)} vs drone ${hd.toFixed(3)} above the ground at the flank`);
    assert.ok(jerkY(c.pts) < jerkY(d.pts), `smoother over the ridge: ${jerkY(c.pts).toFixed(5)} vs ${jerkY(d.pts).toFixed(5)}`);
  });

  console.log(`\n${passed} passed${process.exitCode ? ', some FAILED' : ''}`);
})();
