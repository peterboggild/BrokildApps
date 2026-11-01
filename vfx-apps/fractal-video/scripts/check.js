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

console.log(`\n${passed} passed${process.exitCode ? ', some FAILED' : ''}`);
