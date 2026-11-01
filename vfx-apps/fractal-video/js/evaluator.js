/* Evaluator: project + audio analysis + time -> everything the renderer needs for one frame.
   Pure and deterministic (time comes from the caller, never from a clock), so preview,
   still renders, the Node render driver and tests all see the same frame for the same t. */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory(require('./formulas.js'), require('./palettes.js'), require('./schema.js'));
  } else {
    root.FVEvaluator = factory(root.FVFormulas, root.FVPalettes, root.FVSchema);
  }
})(typeof self !== 'undefined' ? self : globalThis, function (Formulas, Palettes, Schema) {
  'use strict';

  const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
  const lerp = (a, b, t) => a + (b - a) * t;
  const fract = (x) => x - Math.floor(x);
  const smooth = (t) => t * t * (3 - 2 * t);
  const DEG = Math.PI / 180;

  // ---------- time grid ----------
  const secPerBeat = (project) => 60 / (project.bpm || 120);
  const beatAt = (project, t) => (t - (project.offset || 0)) / secPerBeat(project);
  const timeAtBeat = (project, beat) => (project.offset || 0) + beat * secPerBeat(project);

  // Sections are contiguous from beat 0; starts are derived from lengths.
  function sectionStarts(project) {
    let acc = 0;
    return project.sections.map((s) => {
      const start = acc;
      acc += s.length;
      return start;
    });
  }

  const totalBeats = (project) => project.sections.reduce((sum, s) => sum + s.length, 0);

  function songDuration(project, analysis) {
    return Math.max(timeAtBeat(project, totalBeats(project)), analysis && analysis.duration ? analysis.duration : 0);
  }

  function sectionIndexAt(project, beat) {
    const starts = sectionStarts(project);
    let idx = 0;
    for (let i = 0; i < starts.length; i += 1) if (beat >= starts[i]) idx = i;
    return idx;
  }

  const sceneById = (project, id) => project.scenes.find((s) => s.id === id) || project.scenes[0];

  // Which scene(s) are visible at a beat. A transition blends into a section over the
  // `transition` beats before its start, so the new scene lands exactly on the downbeat.
  function sceneStateAt(project, beat) {
    const starts = sectionStarts(project);
    const i = sectionIndexAt(project, beat);
    const sec = project.sections[i];
    const progress = clamp((beat - starts[i]) / Math.max(sec.length, 1e-6), 0, 1);
    const next = project.sections[i + 1];
    if (next && next.transition > 0 && next.sceneId !== sec.sceneId) {
      const from = starts[i + 1] - next.transition;
      if (beat >= from) {
        return {
          section: sec, sectionIndex: i, progress,
          a: sceneById(project, sec.sceneId), b: sceneById(project, next.sceneId),
          w: smooth(clamp((beat - from) / next.transition, 0, 1))
        };
      }
    }
    return { section: sec, sectionIndex: i, progress, a: sceneById(project, sec.sceneId), b: null, w: 0 };
  }

  // ---------- scene state (numbers only) ----------
  function sceneState(scene, bars) {
    const f = Formulas.get(scene.formula);
    const cam = Object.assign({}, scene.camera);
    cam.orbitYaw = (cam.orbit || 0) * bars; // kept apart so a keyed yaw still orbits
    cam.yaw += cam.orbitYaw;
    const look = Object.assign({}, scene.look);
    look.palette = scene.look.palette;
    return {
      formula: f.id,
      size: f.size,
      unbounded: !!f.unbounded,
      iter: scene.iter,
      params: Formulas.resolveParams(f.id, scene.params),
      space: Object.assign({}, scene.space),
      camera: cam,
      look,
      stops: scene.look.palette.stops,
      fogBoost: 0
    };
  }

  function lerpAngle(a, b, t) {
    const d = ((((b - a) % 360) + 540) % 360) - 180;
    return a + d * t;
  }

  function blendStates(A, B, w) {
    const same = A.formula === B.formula;
    const shapeFrom = same || w < 0.5 ? A : B;
    const out = {
      formula: shapeFrom.formula,
      size: lerp(A.size, B.size, w),
      unbounded: shapeFrom.unbounded,
      iter: same ? Math.round(lerp(A.iter, B.iter, w)) : shapeFrom.iter,
      params: {},
      space: {},
      camera: {},
      look: {},
      // Different formulas cannot be interpolated: cut at the midpoint, hidden in a fog swell.
      fogBoost: same ? 0 : 1 - Math.abs(2 * w - 1)
    };
    Object.keys(shapeFrom.params).forEach((k) => {
      out.params[k] = same ? lerp(A.params[k], B.params[k], w) : shapeFrom.params[k];
    });
    if (same) {
      Object.keys(A.space).forEach((k) => { out.space[k] = lerp(A.space[k], B.space[k], w); });
      Object.keys(A.camera).forEach((k) => {
        out.camera[k] = (k === 'yaw' || k === 'roll') ? lerpAngle(A.camera[k], B.camera[k], w) : lerp(A.camera[k], B.camera[k], w);
      });
      out.camera.dist = Math.exp(lerp(Math.log(A.camera.dist), Math.log(B.camera.dist), w));
    } else {
      // A camera framed for one formula is meaningless for another: cut it together with the shape.
      out.space = Object.assign({}, shapeFrom.space);
      out.camera = Object.assign({}, shapeFrom.camera);
      out.size = shapeFrom.size;
    }
    Object.keys(Schema.LOOK_NUM).forEach((k) => {
      out.look[k] = k === 'hue' ? lerpAngle(A.look[k], B.look[k], w) : lerp(A.look[k], B.look[k], w);
    });
    Object.keys(Schema.LOOK_COLORS).forEach((k) => { out.look[k] = Palettes.mixHex(A.look[k], B.look[k], w); });
    const discrete = w < 0.5 ? A.look : B.look;
    out.look.fogMatch = discrete.fogMatch;
    out.look.shadows = A.look.shadows || B.look.shadows;
    out.look.tonemap = discrete.tonemap;
    out.look.palette = Object.assign({}, discrete.palette);
    out.stops = Palettes.blend(A.stops, A.look.palette.interp, B.stops, B.look.palette.interp, w);
    if (w > 0 && w < 1 && A.look.palette.interp !== B.look.palette.interp) out.look.palette.interp = 'oklab';
    return out;
  }

  // ---------- keyframes ----------
  // Absolute beat of each point of a lane, sorted. Points past the end of a shortened section sit at its end.
  function keyPoints(project, lane, starts) {
    const st = starts || sectionStarts(project);
    const out = [];
    lane.pts.forEach((q) => {
      const i = project.sections.findIndex((s) => s.id === q.sec);
      if (i >= 0) out.push({ beat: st[i] + Math.min(q.b, project.sections[i].length), v: q.v, q });
    });
    return out.sort((a, b) => a.beat - b.beat);
  }

  // Value of a lane at a beat: held before the first key and after the last, eased/linear/stepped between.
  function keyValueAt(project, lane, beat, starts) {
    const pts = keyPoints(project, lane, starts);
    if (!pts.length) return null;
    if (beat <= pts[0].beat) return pts[0].v;
    const last = pts[pts.length - 1];
    if (beat >= last.beat) return last.v;
    let i = 0;
    while (pts[i + 1].beat < beat) i += 1;
    const a = pts[i];
    const b = pts[i + 1];
    if (lane.interp === 'step') return beat < b.beat ? a.v : b.v;
    let u = b.beat > a.beat ? (beat - a.beat) / (b.beat - a.beat) : 1;
    if (lane.interp !== 'linear') u = u * u * (3 - 2 * u);
    const spec = Schema.keySpec(lane.dest);
    if (spec && spec.angle) return lerpAngle(a.v, b.v, u); // yaw/roll take the short way round
    if (spec && spec.log && a.v > 0 && b.v > 0) return Math.exp(lerp(Math.log(a.v), Math.log(b.v), u)); // zoom at an even rate
    return lerp(a.v, b.v, u);
  }

  // Set a keyed parameter in a frame state, pulled towards the key value by weight g.
  function applyKey(state, dest, v, g) {
    const [group, key] = dest.split('.');
    const spec = Schema.keySpec(dest, group === 'shape' ? state.formula : undefined);
    if (!spec) return;
    const x = clamp(v, spec.min, spec.max);
    if (group === 'shape') { if (key in state.params) state.params[key] = lerp(state.params[key], x, g); }
    else if (group === 'space') state.space[key] = lerp(state.space[key], x, g);
    else if (group === 'look') state.look[key] = lerp(state.look[key], x, g);
    else if (group === 'cam') {
      const c = state.camera;
      if (key === 'yaw') c.yaw = lerpAngle(c.yaw, v + (c.orbitYaw || 0), g);
      else if (key === 'roll') c.roll = lerpAngle(c.roll, v, g);
      else if (key === 'dist') c.dist = Math.exp(lerp(Math.log(c.dist), Math.log(Math.max(x, 1e-6)), g));
      else c[key] = lerp(c[key], x, g);
    }
  }

  // ---------- auto-flight playback ----------
  // A run is a stretch of consecutive flown sections; pts = [x, y, z, yaw, pitch, roll] every `step` beats.
  function flightPose(project, beat) {
    const tr = project.flight && project.flight.track;
    if (!tr || !tr.runs) return null;
    for (const r of tr.runs) {
      if (beat < r.b0 || beat > r.b1 + 1e-9) continue;
      const n = Math.floor(r.pts.length / 6);
      if (n < 1) return null;
      const x = clamp((beat - r.b0) / tr.step, 0, n - 1);
      const i = n > 1 ? Math.min(Math.floor(x), n - 2) : 0;
      const u = n > 1 ? x - i : 0;
      const g = (k, c) => r.pts[clamp(k, 0, n - 1) * 6 + c];
      const cr = (c) => {
        const p0 = g(i - 1, c), p1 = g(i, c), p2 = g(i + 1, c), p3 = g(i + 2, c);
        return 0.5 * (2 * p1 + (p2 - p0) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (3 * p1 - p0 - 3 * p2 + p3) * u * u * u);
      };
      return { pos: [cr(0), cr(1), cr(2)], yaw: lerpAngle(g(i, 3), g(i + 1, 3), u), pitch: lerp(g(i, 4), g(i + 1, 4), u), roll: lerpAngle(g(i, 5), g(i + 1, 5), u) };
    }
    return null;
  }

  // Put a flown pose into the orbit-style camera the renderer understands (target a little ahead).
  function applyFlightPose(cam, fp, size) {
    const D = Math.max(1e-3, 0.1 * size);
    const y = fp.yaw * DEG, p = fp.pitch * DEG;
    const dir = [Math.cos(p) * Math.sin(y), Math.sin(p), Math.cos(p) * Math.cos(y)];
    cam.yaw = fp.yaw; cam.pitch = fp.pitch; cam.roll = fp.roll; cam.dist = D; cam.orbitYaw = 0;
    cam.tx = fp.pos[0] - dir[0] * D; cam.ty = fp.pos[1] - dir[1] * D; cam.tz = fp.pos[2] - dir[2] * D;
  }

  // Everything the flight depends on. A stored path whose signature differs is out of date.
  function flightSig(project) {
    const f = project.flight || {};
    const src = JSON.stringify({
      bpm: project.bpm, off: project.offset, bpb: project.beatsPerBar,
      sec: project.sections.map((s) => [s.length, s.sceneId, s.transition]),
      sc: project.scenes.map((s) => [s.id, s.formula, s.iter, s.params, s.space, s.camera]),
      mod: project.mod, keys: project.keys,
      au: [project.master && project.master.key, (project.stems || []).map((s) => s.key)],
      fl: [f.on, f.settings]
    });
    let h = 2166136261;
    for (let i = 0; i < src.length; i += 1) { h ^= src.charCodeAt(i); h = Math.imul(h, 16777619); }
    return (h >>> 0).toString(16);
  }

  // ---------- modulation signals ----------
  const smoothCache = typeof WeakMap !== 'undefined' ? new WeakMap() : null;

  // Attack/release one-pole over the whole track, computed once per (track, attack, release).
  function smoothedTrack(track, rate, attack, release) {
    if (!(attack > 0) && !(release > 0)) return track;
    const key = `${attack.toFixed(4)}|${release.toFixed(4)}`;
    let byKey = smoothCache && smoothCache.get(track);
    if (!byKey) {
      byKey = new Map();
      if (smoothCache) smoothCache.set(track, byKey);
    }
    if (byKey.has(key)) return byKey.get(key);
    const aa = attack > 0 ? Math.exp(-1 / (attack * rate)) : 0;
    const ar = release > 0 ? Math.exp(-1 / (release * rate)) : 0;
    const out = new Float32Array(track.length);
    let y = 0;
    for (let i = 0; i < track.length; i += 1) {
      const x = track[i];
      const a = x > y ? aa : ar;
      y = a * y + (1 - a) * x;
      out[i] = y;
    }
    if (byKey.size > 64) byKey.clear();
    byKey.set(key, out);
    return out;
  }

  function sampleTrack(track, rate, t) {
    if (!track || !track.length) return 0;
    const x = t * rate;
    if (x <= 0) return track[0];
    const i = Math.floor(x);
    if (i >= track.length - 1) return track[track.length - 1];
    return lerp(track[i], track[i + 1], x - i);
  }

  function gridSignal(project, id, beat, sectionProgress) {
    const bpb = project.beatsPerBar || 4;
    const bars = beat / bpb;
    switch (id) {
      case 'grid.beatPulse': return beat < 0 ? 0 : Math.exp(-fract(beat) * 5);
      case 'grid.barPulse': return beat < 0 ? 0 : Math.exp(-fract(bars) * bpb * 1.2);
      case 'grid.beatSaw': return fract(beat);
      case 'grid.barSaw': return fract(bars);
      case 'grid.section': return sectionProgress;
      case 'grid.lfo4': return 0.5 - 0.5 * Math.cos(2 * Math.PI * bars / 4);
      case 'grid.lfo16': return 0.5 - 0.5 * Math.cos(2 * Math.PI * bars / 16);
      default: return 0;
    }
  }

  // Value 0..1 of a modulation row's source at time t (after smoothing and look-ahead).
  function rowSignal(project, analysis, row, t, sectionProgressAt) {
    const leadSec = (row.lead || 0) * secPerBeat(project);
    const tt = t + leadSec;
    if (Schema.isGridSource(row.source)) {
      const beat = beatAt(project, tt);
      let prog = 0;
      if (row.source === 'grid.section') {
        if (sectionProgressAt) prog = sectionProgressAt(beat);
        else {
          const i = sectionIndexAt(project, beat);
          prog = clamp((beat - sectionStarts(project)[i]) / Math.max(project.sections[i].length, 1e-6), 0, 1);
        }
      }
      return gridSignal(project, row.source, beat, prog);
    }
    if (!analysis || !analysis.tracks) return 0;
    const raw = analysis.tracks[row.source];
    if (!raw) return 0;
    const track = smoothedTrack(raw, analysis.rate, row.attack || 0, row.release || 0);
    return clamp(sampleTrack(track, analysis.rate, tt), 0, 1);
  }

  // Deterministic camera shake: sum of incommensurate sines.
  function shake(t, seed) {
    return Math.sin(t * 13.1 + seed) * 0.5 + Math.sin(t * 7.37 + seed * 2.1) * 0.3 + Math.sin(t * 23.9 + seed * 0.7) * 0.2;
  }

  function applyMod(state, dest, amount, t) {
    const spec = Schema.destSpec(dest);
    if (!spec) return;
    const range = spec.max - spec.min;
    const [group, key] = dest.split('.');
    if (group === 'shape') {
      if (!(key in state.params)) return;
      const p = Formulas.get(state.formula).params.find((x) => x.key === key);
      state.params[key] = clamp(state.params[key] + amount * (p.max - p.min), p.min, p.max);
    } else if (group === 'space') {
      state.space[key] = clamp(state.space[key] + amount * range, Schema.SPACE[key].min, Schema.SPACE[key].max);
    } else if (dest === 'cam.dist') {
      state.camera.dist = Math.max(state.camera.dist * 0.05, state.camera.dist * (1 - 0.6 * amount));
    } else if (dest === 'cam.shake') {
      const a = amount * 10;
      state.camera.yaw += a * shake(t, 1.3);
      state.camera.pitch += a * shake(t, 4.7);
    } else if (group === 'cam') {
      state.camera[key] = clamp(state.camera[key] + amount * range, spec.min, spec.max);
    } else if (group === 'look') {
      if (key === 'palOffset') state.look.palOffset += amount;
      else state.look[key] = clamp(state.look[key] + amount * range, spec.min, spec.max);
    } else if (group === 'bg') {
      state.layers.forEach((L) => { L.bd[key] = clamp(L.bd[key] + amount * range, spec.min, spec.max); });
    }
  }

  // ---------- backdrop ----------
  const fmod = (x, m) => ((x % m) + m) % m;

  /* One backdrop layer for the GPU: texture-space transform of the frame (x in frame heights from the
     centre, y up), its weight, and for movies the media time at t. The frame aspect comes from the
     render size, so the preview and the render crop the backdrop identically. */
  function backdropLayer(bd, weight, t, anchor, frameAspect) {
    const fa = frameAspect;
    const ia = bd.aspect > 0 ? bd.aspect : 16 / 9;
    let sx;
    let sy;
    if (bd.fit === 'stretch') {
      sx = 1 / (fa * bd.zoom);
      sy = 1 / bd.zoom;
    } else {
      const hh = (bd.fit === 'contain' ? Math.min(1, fa / ia) : Math.max(1, fa / ia)) * bd.zoom;
      sx = 1 / (hh * ia);
      sy = 1 / hh;
    }
    const roomU = Math.max(0, 0.5 - fa * sx / 2);
    const roomV = Math.max(0, 0.5 - sy / 2);
    let time = 0;
    if (bd.kind === 'video' && bd.duration > 0) time = fmod((t - anchor) * bd.speed, bd.duration);
    return {
      key: bd.key,
      kind: bd.kind,
      time,
      speed: bd.speed,
      scale: [sx, -sy],
      offset: [0.5 + bd.panX * roomU, 0.5 - bd.panY * roomV],
      amount: clamp(bd.opacity * weight, 0, 1),
      gain: Math.pow(2, bd.brightness),
      sat: bd.saturation,
      lod: bd.blur,
      fogFade: bd.fogFade,
      front: bd.place === 'front' ? 1 : 0,
      mode: (Schema.BACKDROP_BLENDS.find((b) => b.id === bd.blend) || Schema.BACKDROP_BLENDS[0]).code
    };
  }

  // ---------- camera ----------
  function cameraBasis(cam) {
    const y = cam.yaw * DEG;
    const p = clamp(cam.pitch, -89.5, 89.5) * DEG;
    const dir = [Math.cos(p) * Math.sin(y), Math.sin(p), Math.cos(p) * Math.cos(y)];
    const target = [cam.tx, cam.ty, cam.tz];
    const pos = target.map((v, i) => v + dir[i] * cam.dist);
    const fwd = dir.map((v) => -v);
    let right = [fwd[1] * 0 - fwd[2] * 1, fwd[2] * 0 - fwd[0] * 0, fwd[0] * 1 - fwd[1] * 0];
    const rl = Math.hypot(right[0], right[1], right[2]) || 1;
    right = right.map((v) => v / rl);
    let up = [right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2], right[0] * fwd[1] - right[1] * fwd[0]];
    if (cam.roll) {
      const r = cam.roll * DEG, c = Math.cos(r), s = Math.sin(r);
      const nr = right.map((v, i) => v * c + up[i] * s);
      up = up.map((v, i) => v * c - right[i] * s);
      right = nr;
    }
    return { pos, right, up, fwd, fovT: Math.tan(clamp(cam.fov, 1, 170) * DEG / 2), dist: cam.dist, target };
  }

  // ---------- main entry ----------
  /* opts.sceneId: show that scene only (scene-edit view, no transitions).
     opts.modulate: apply the modulation matrix (default true). */
  function evaluate(project, analysis, t, opts) {
    const o = opts || {};
    const time = Number.isFinite(t) ? t : 0;
    const beat = beatAt(project, time);
    const bars = beat / (project.beatsPerBar || 4);
    const ss = sceneStateAt(project, beat);
    let state;
    let sceneA;
    let sceneB = null;
    let w = 0;
    if (o.sceneId) {
      sceneA = sceneById(project, o.sceneId);
      state = sceneState(sceneA, o.animate === false ? 0 : bars);
    } else {
      sceneA = ss.a;
      sceneB = ss.b;
      w = ss.w;
      state = sceneState(sceneA, bars);
      if (sceneB) state = blendStates(state, sceneState(sceneB, bars), w);
    }

    // Backdrops cross-fade with the transition weight. Movie time runs from the song start, or from
    // the start of the section that shows the scene ("restarts each section").
    const starts = sectionStarts(project);
    // A movie that restarts per section starts where the section starts fading in.
    const anchorOf = (scene, secIdx) => {
      if (o.sceneId || (scene.backdrop || {}).sync !== 'section') return 0;
      const i = Math.min(secIdx, starts.length - 1);
      const sec = project.sections[i];
      const fadesIn = i > 0 && sec.transition > 0 && project.sections[i - 1].sceneId !== sec.sceneId;
      return timeAtBeat(project, starts[i] - (fadesIn ? Math.min(sec.transition, project.sections[i - 1].length) : 0));
    };
    const bdOf = (scene) => Object.assign(Schema.defaultBackdrop(), scene.backdrop);
    state.layers = [{ bd: bdOf(sceneA), w: 1 - w, anchor: anchorOf(sceneA, ss.sectionIndex) }];
    if (sceneB) state.layers.push({ bd: bdOf(sceneB), w, anchor: anchorOf(sceneB, ss.sectionIndex + 1) });

    // Keyframes set the value (after the scene blend, before the routes add movement on top).
    // A lane acts only in the scenes it is on in, and fades with them across a transition.
    const sceneW = (row) => (o.sceneId ? (Schema.routeOn(row, sceneA.id) ? 1 : 0)
      : (Schema.routeOn(row, sceneA.id) ? 1 - w : 0) + (sceneB && Schema.routeOn(row, sceneB.id) ? w : 0));
    const camCut = !o.sceneId && sceneB && sceneA.formula !== sceneB.formula;
    if (o.keys !== false) {
      (project.keys || []).forEach((lane) => {
        let g = sceneW(lane);
        // Across a formula change the camera cuts with the shape, so camera keys cut there too.
        if (camCut && lane.dest.startsWith('cam.')) g = w < 0.5 ? (Schema.routeOn(lane, sceneA.id) ? 1 : 0) : (Schema.routeOn(lane, sceneB.id) ? 1 : 0);
        if (g <= 0) return;
        const v = keyValueAt(project, lane, beat, starts);
        if (v !== null) applyKey(state, lane.dest, v, g);
      });
    }

    // Auto-flight replaces the camera pose in flown scenes (timeline only; scene view edits the camera).
    // Across a transition it behaves like the camera already does: blended between two scenes of the same
    // formula, cut at the midpoint when the formula changes.
    if (o.flight !== false && !o.sceneId && project.flight && project.flight.on && project.flight.on.length) {
      const on = (sc) => (sc && project.flight.on.includes(sc.id) ? 1 : 0);
      const g = !sceneB ? on(sceneA) : camCut ? (w < 0.5 ? on(sceneA) : on(sceneB)) : on(sceneA) * (1 - w) + on(sceneB) * w;
      const fp = g > 0 ? flightPose(project, beat) : null;
      if (fp) {
        const c = state.camera;
        const f = Object.assign({}, c);
        applyFlightPose(f, fp, state.size);
        if (g >= 1) Object.assign(c, f);
        else {
          c.yaw = lerpAngle(c.yaw, f.yaw, g); c.roll = lerpAngle(c.roll, f.roll, g); c.pitch = lerp(c.pitch, f.pitch, g);
          c.dist = Math.exp(lerp(Math.log(c.dist), Math.log(f.dist), g));
          c.tx = lerp(c.tx, f.tx, g); c.ty = lerp(c.ty, f.ty, g); c.tz = lerp(c.tz, f.tz, g);
          c.orbitYaw = lerp(c.orbitYaw || 0, 0, g);
        }
      }
    }

    if (o.modulate !== false) {
      const progAt = (b) => {
        const i = sectionIndexAt(project, b);
        return clamp((b - starts[i]) / Math.max(project.sections[i].length, 1e-6), 0, 1);
      };
      (project.mod || []).forEach((row) => {
        if (!row.depth) return;
        // Weight by how much of the frame shows a scene the route is on in, so it fades with the transition.
        const g = sceneW(row);
        if (g <= 0) return;
        const s = rowSignal(project, analysis, row, time, progAt);
        if (s) applyMod(state, row.dest, row.depth * s * g, time);
      });
    }

    const L = state.look;
    const f = Formulas.get(state.formula);
    const packed = Formulas.pack(state.formula, state.params);
    const cam = cameraBasis(state.camera);
    const size = state.size;
    const far = state.unbounded || state.space.repeat > 0 ? cam.dist + size * 16 : cam.dist + size * 4;
    const lin = (hex) => Palettes.srgbToLinear(Palettes.hexToRgb(hex));
    const az = L.sunAz * DEG;
    const el = L.sunEl * DEG;
    const sunDir = [Math.cos(el) * Math.sin(az), Math.sin(el), Math.cos(el) * Math.cos(az)];
    const pal = L.palette;
    const gpuPal = Palettes.toGPU(state.stops || pal.stops, pal.interp);
    const srcCode = (Schema.COLOR_SOURCES.find((c) => c.id === pal.source) || Schema.COLOR_SOURCES[0]).code;
    const wrapCode = (Schema.WRAPS.find((c) => c.id === pal.wrap) || Schema.WRAPS[1]).code;
    const toneCode = (Schema.TONEMAPS.find((c) => c.id === L.tonemap) || Schema.TONEMAPS[0]).code;
    // fogBoost peaks at the cut between two formulas: the frame dissolves fully into the sky.
    const fogDensity = (L.fog * 2 + Math.pow(state.fogBoost, 1.5) * 12) / size;
    const bgTop = lin(L.bgTop);
    const bgBottom = lin(L.bgBottom);
    const frameAspect = project.render && project.render.height > 0 ? project.render.width / project.render.height : 16 / 9;
    const layers = state.layers
      .filter((x) => x.bd.kind !== 'none' && x.bd.key && x.w > 0)
      .map((x) => backdropLayer(x.bd, x.w, time, x.anchor, frameAspect));
    // fog can only fade into layers that are behind the fractal
    const behind = layers.filter((x) => !x.front);
    const cover = behind.reduce((s, x) => s + x.amount, 0);
    const backdrop = layers.length ? { layers, fogFade: cover > 0 ? behind.reduce((s, x) => s + x.amount * x.fogFade, 0) / cover : 0 } : null;

    return {
      t: time,
      beat,
      sceneId: (sceneB && w >= 0.5 ? sceneB : sceneA).id,
      sceneAId: sceneA.id,
      sceneBId: sceneB ? sceneB.id : null,
      blend: w,
      sectionIndex: ss.sectionIndex,
      sectionId: ss.section.id,
      camera: Object.assign({}, state.camera), // evaluated pose (keys, blend, routes applied)
      formula: f.id,
      gpu: {
        iter: Math.max(1, Math.round(state.iter)),
        p: packed.p,
        rotA: packed.rotA,
        rotB: packed.rotB,
        repeat: state.space.repeat,
        twist: state.space.twist,
        bound: state.space.bound,
        detail: state.space.detail,
        step: state.space.step,
        size,
        maxDist: far,
        camPos: cam.pos,
        camRight: cam.right,
        camUp: cam.up,
        camFwd: cam.fwd,
        fovT: cam.fovT,
        palPos: gpuPal.pos,
        palCol: gpuPal.col,
        palN: gpuPal.n,
        palInterp: gpuPal.interp,
        colorSrc: srcCode,
        palWrap: wrapCode,
        palScale: L.palScale,
        palOffset: L.palOffset + L.palCycle * bars / 8,
        bgTop,
        bgBottom,
        fogColor: L.fogMatch ? null : lin(L.fogColor),
        fog: fogDensity,
        glowColor: lin(L.glowColor).map((v) => v * L.glow),
        sunDir,
        sunColor: lin(L.sunColor).map((v) => v * L.sun),
        fillColor: lin(L.fillColor).map((v) => v * L.fill),
        ambient: L.ambient,
        specular: L.specular,
        ao: L.ao,
        shadows: !!L.shadows,
        backdrop
      },
      post: {
        exposure: L.exposure,
        contrast: L.contrast,
        saturation: L.saturation,
        hue: L.hue * DEG,
        vignette: L.vignette,
        grain: L.grain || 0,
        grainSeed: Math.floor(time * 24) % 997,
        tonemap: toneCode
      }
    };
  }

  // Frame plan for offline rendering (used by the Node render driver).
  function renderPlan(project, analysis, config) {
    const c = Object.assign({}, project.render, config || {});
    const fps = Number(c.fps) > 0 ? Number(c.fps) : 30;
    const duration = songDuration(project, analysis);
    const frames = Math.max(1, Math.ceil(duration * fps));
    const start = clamp(Math.floor(Number(c.startFrame) || 0), 0, frames - 1);
    const end = Number(c.endFrame) > start ? Math.min(frames, Math.floor(c.endFrame)) : frames;
    return { width: c.width, height: c.height, fps, samples: c.samples, outDir: c.outDir, duration, frames, start, end, timeOf: (n) => n / fps };
  }

  return {
    beatAt, timeAtBeat, secPerBeat, sectionStarts, totalBeats, songDuration, sectionIndexAt, sceneStateAt,
    rowSignal, smoothedTrack, sampleTrack, cameraBasis, evaluate, renderPlan, keyPoints, keyValueAt,
    flightPose, flightSig, sceneById
  };
});
