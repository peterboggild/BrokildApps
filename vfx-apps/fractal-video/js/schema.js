/* Project schema: defaults, value ranges, look presets, normalisation and migration.
   Pure (no DOM), shared by the browser app and Node tools. */
(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory(require('./formulas.js'), require('./palettes.js'));
  } else {
    root.FVSchema = factory(root.FVFormulas, root.FVPalettes);
  }
})(typeof self !== 'undefined' ? self : globalThis, function (Formulas, Palettes) {
  'use strict';

  const VERSION = 2;
  const N = (label, min, max, step, def, extra) => Object.assign({ label, min, max, step, def }, extra || {});

  // Numeric look values. Colours are listed separately.
  const LOOK_NUM = {
    palScale: N('Scale', 0.05, 8, 0.01, 1, { hint: 'How many times the palette repeats across the colour source.' }),
    palOffset: N('Offset', 0, 1, 0.001, 0),
    palCycle: N('Cycle', -2, 2, 0.01, 0, { unit: '/8 bars', hint: 'Palette rotation speed, in palette lengths per 8 bars.' }),
    fog: N('Fog density', 0, 1, 0.001, 0.04),
    glow: N('Glow', 0, 3, 0.01, 0.3),
    sun: N('Sun', 0, 6, 0.01, 2.4),
    sunAz: N('Sun azimuth', -180, 180, 0.5, 35, { unit: '°' }),
    sunEl: N('Sun elevation', -10, 90, 0.5, 40, { unit: '°' }),
    fill: N('Fill light', 0, 3, 0.01, 0.4),
    ambient: N('Ambient', 0, 2, 0.01, 0.35),
    specular: N('Specular', 0, 2, 0.01, 0.5),
    ao: N('Occlusion', 0, 1, 0.01, 0.8),
    exposure: N('Exposure', -4, 4, 0.01, 0, { unit: 'EV' }),
    contrast: N('Contrast', 0.5, 2, 0.01, 1.05),
    saturation: N('Saturation', 0, 2, 0.01, 1.05),
    hue: N('Hue shift', -180, 180, 0.5, 0, { unit: '°' }),
    vignette: N('Vignette', 0, 1, 0.01, 0.35),
    grain: N('Film grain', 0, 1, 0.01, 0, { hint: 'Animated film grain, changes every frame of the video.' })
  };

  const LOOK_COLORS = {
    bgTop: 'Sky top', bgBottom: 'Sky bottom', fogColor: 'Fog colour', glowColor: 'Glow colour', sunColor: 'Sun colour', fillColor: 'Fill colour'
  };

  const COLOR_SOURCES = [
    { id: 'trap', label: 'Orbit trap · point', code: 0 },
    { id: 'planes', label: 'Orbit trap · planes', code: 1 },
    { id: 'iter', label: 'Iteration', code: 2 },
    { id: 'cavity', label: 'Cavity (occlusion)', code: 3 },
    { id: 'normal', label: 'Facing up', code: 4 },
    { id: 'radial', label: 'Radius', code: 5 },
    { id: 'depth', label: 'Depth', code: 6 }
  ];
  const WRAPS = [{ id: 'repeat', label: 'Repeat', code: 0 }, { id: 'mirror', label: 'Mirror', code: 1 }, { id: 'clamp', label: 'Clamp', code: 2 }];
  const INTERPS = [{ id: 'oklab', label: 'Perceptual (OKLab)' }, { id: 'smooth', label: 'Smooth (eased)' }, { id: 'rgb', label: 'Linear RGB' }, { id: 'steps', label: 'Hard bands' }];
  const TONEMAPS = [{ id: 'aces', label: 'Filmic (ACES)', code: 0 }, { id: 'soft', label: 'Soft (Reinhard)', code: 1 }, { id: 'none', label: 'None (clip)', code: 2 }];

  const CAMERA = {
    dist: N('Distance', 0.05, 40, 0.01, 3),
    yaw: N('Yaw', -180, 180, 0.5, 30, { unit: '°' }),
    pitch: N('Pitch', -89, 89, 0.5, 20, { unit: '°' }),
    roll: N('Roll', -180, 180, 0.5, 0, { unit: '°' }),
    fov: N('Field of view', 10, 120, 0.5, 50, { unit: '°' }),
    orbit: N('Auto-orbit', -90, 90, 0.5, 0, { unit: '°/bar', hint: 'Camera orbits by this many degrees per bar while the song plays.' }),
    tx: N('Target X', -10, 10, 0.005, 0),
    ty: N('Target Y', -10, 10, 0.005, 0),
    tz: N('Target Z', -10, 10, 0.005, 0)
  };

  const SPACE = {
    repeat: N('Repeat spacing', 0, 12, 0.01, 0, { hint: '0 = off. Tiles the fractal endlessly (cities, corridors).' }),
    twist: N('Twist', -2, 2, 0.005, 0, { hint: 'Twists space around the vertical axis. Lower the step factor if it tears.' }),
    bound: N('Bound radius', 0, 12, 0.01, 0, { hint: '0 = off. Cuts the fractal to a sphere (useful for infinite ones).' }),
    detail: N('Surface detail', 0.25, 4, 0.01, 1, { hint: 'Hit threshold in pixels. Lower = finer, slower.' }),
    step: N('Step factor', 0.2, 1.2, 0.01, 0.9, { hint: 'Ray step safety. Lower if you see holes or noise.' })
  };

  const ROLES = ['KICK', 'SNARE', 'HATS', 'PERC', 'BASS', 'LEAD', 'VOCAL', 'PADS', 'FX', 'OTHER'];

  // Per-scene picture or movie behind the fractal. The file itself lives in the browser (IndexedDB),
  // the project only keeps its key, name, aspect and duration.
  const BACKDROP = {
    opacity: N('Opacity', 0, 1, 0.01, 1),
    brightness: N('Brightness', -4, 3, 0.01, 0, { unit: 'EV' }),
    saturation: N('Saturation', 0, 2, 0.01, 1),
    blur: N('Blur', 0, 6, 0.01, 0, { hint: 'Softens the backdrop, like a shallow depth of field.' }),
    zoom: N('Zoom', 1, 4, 0.01, 1),
    panX: N('Pan X', -1, 1, 0.005, 0),
    panY: N('Pan Y', -1, 1, 0.005, 0),
    fogFade: N('Fog → backdrop', 0, 1, 0.01, 1, { hint: '1: fog and distance fade the fractal into the backdrop. 0: the fog colour also hazes over the backdrop.' }),
    speed: N('Speed', 0.1, 4, 0.01, 1, { unit: '×' })
  };
  const BACKDROP_FITS = [{ id: 'cover', label: 'Fill frame (crop)' }, { id: 'contain', label: 'Fit inside (sky bars)' }, { id: 'stretch', label: 'Stretch' }];
  const BACKDROP_SYNC = [{ id: 'song', label: 'Runs with the song' }, { id: 'section', label: 'Restarts each section' }];
  const BACKDROP_PLACES = [{ id: 'behind', label: 'Behind the fractal' }, { id: 'front', label: 'In front (overlay)' }];
  // How an overlay combines with the fractal (front placement only).
  const BACKDROP_BLENDS = [
    { id: 'screen', label: 'Screen (black = clear)', code: 1 },
    { id: 'multiply', label: 'Multiply (white = clear)', code: 3 },
    { id: 'add', label: 'Add (glow)', code: 2 },
    { id: 'soft', label: 'Soft light', code: 4 },
    { id: 'normal', label: 'Normal (uses transparency)', code: 0 }
  ];

  function defaultBackdrop() {
    const b = { kind: 'none', key: '', name: '', aspect: 16 / 9, duration: 0, fit: 'cover', sync: 'song', place: 'behind', blend: 'screen' };
    Object.keys(BACKDROP).forEach((k) => { b[k] = BACKDROP[k].def; });
    return b;
  }

  const SECTION = { length: N('Length', 1, 4096, 1, 32, { unit: 'beats' }), transition: N('Transition', 0, 64, 1, 4, { unit: 'beats' }) };

  // ---------- defaults ----------
  function defaultLook() {
    return {
      palette: { stops: Palettes.PRESET_BY_ID.magma.stops.map((s) => ({ ...s })), interp: 'oklab', source: 'trap', wrap: 'mirror' },
      palScale: 1, palOffset: 0, palCycle: 0,
      bgTop: '#101a33', bgBottom: '#020308', fogMatch: true, fogColor: '#0b1020', fog: 0.04,
      glowColor: '#7fb4ff', glow: 0.3,
      sunColor: '#fff1dc', sun: 2.4, sunAz: 35, sunEl: 40, fillColor: '#5c7cff', fill: 0.4, ambient: 0.35, specular: 0.5,
      shadows: true, ao: 0.8,
      exposure: 0, contrast: 1.05, saturation: 1.05, hue: 0, vignette: 0.35, grain: 0, tonemap: 'aces'
    };
  }

  // Whole-look presets: palette + environment + light + grade.
  const LOOK_PRESETS = [
    { id: 'nocturne', name: 'Nocturne', palette: 'magma', look: { bgTop: '#101a33', bgBottom: '#020308', glowColor: '#7fb4ff', glow: 0.3, sunColor: '#fff1dc', fillColor: '#5c7cff', fog: 0.04, exposure: 0, contrast: 1.05, saturation: 1.05 } },
    { id: 'neon', name: 'Neon Night', palette: 'neon', look: { bgTop: '#0d0221', bgBottom: '#000000', glowColor: '#ff2bd6', glow: 1.1, sunColor: '#c9b8ff', sun: 1.6, fillColor: '#00e5ff', fill: 0.9, fog: 0.05, contrast: 1.15, saturation: 1.25, ambient: 0.25 } },
    { id: 'golden', name: 'Golden Hour', palette: 'goldleaf', look: { bgTop: '#f2b56b', bgBottom: '#3b1d2f', glowColor: '#ffb347', glow: 0.25, sunColor: '#ffd29a', sun: 3, sunEl: 14, fillColor: '#6a4cff', fill: 0.3, fog: 0.08, fogMatch: true, contrast: 1.0, saturation: 1.1 } },
    { id: 'arctic', name: 'Arctic', palette: 'glacier', look: { bgTop: '#dbe9f5', bgBottom: '#7f9fb8', glowColor: '#ffffff', glow: 0.1, sunColor: '#ffffff', sun: 2.0, fillColor: '#9cc9ff', fill: 0.6, ambient: 0.8, fog: 0.07, contrast: 0.95, saturation: 0.9, exposure: 0.3 } },
    { id: 'noir', name: 'Noir', palette: 'mono', look: { bgTop: '#1a1a1a', bgBottom: '#000000', glowColor: '#ffffff', glow: 0.15, sunColor: '#ffffff', sun: 3.2, fillColor: '#ffffff', fill: 0.15, ambient: 0.15, fog: 0.03, contrast: 1.35, saturation: 0, vignette: 0.6 } },
    { id: 'deepsea', name: 'Deep Sea', palette: 'deepsea', look: { bgTop: '#03303a', bgBottom: '#000508', glowColor: '#2ec4b6', glow: 0.8, sunColor: '#bff6ff', sun: 1.8, sunEl: 70, fillColor: '#005f73', fill: 0.6, fog: 0.16, contrast: 1.05, saturation: 1.1 } },
    { id: 'infrared', name: 'Infrared', palette: 'infrared', look: { bgTop: '#14002e', bgBottom: '#000000', glowColor: '#f72585', glow: 0.7, sunColor: '#ffe3b3', sun: 2.6, fillColor: '#4361ee', fill: 0.5, fog: 0.05, contrast: 1.12, saturation: 1.2 } },
    { id: 'pastel', name: 'Pastel', palette: 'pastel', look: { bgTop: '#fde2e4', bgBottom: '#bee1e6', glowColor: '#ffffff', glow: 0.15, sunColor: '#fff8f0', sun: 1.8, fillColor: '#cdb4db', fill: 0.7, ambient: 0.9, fog: 0.06, contrast: 0.9, saturation: 0.95, exposure: 0.2, vignette: 0.15 } },
    { id: 'ember', name: 'Ember', palette: 'ember', look: { bgTop: '#2a0800', bgBottom: '#000000', glowColor: '#ff6a00', glow: 0.9, sunColor: '#ffd0a0', sun: 2.2, fillColor: '#ff3d00', fill: 0.4, fog: 0.06, contrast: 1.1, saturation: 1.15 } },
    { id: 'aurora', name: 'Aurora', palette: 'aurora', look: { bgTop: '#0b2a3d', bgBottom: '#02040a', glowColor: '#7cf29c', glow: 0.6, sunColor: '#e6fff0', sun: 2.0, fillColor: '#a06cf5', fill: 0.7, fog: 0.05, contrast: 1.05, saturation: 1.15 } },
    // gritty: dusty haze, low sun, muted colour, grain
    { id: 'wasteland', name: 'Wasteland', palette: 'dustbowl', look: { bgTop: '#9a7a52', bgBottom: '#2a1d12', glowColor: '#ffb066', glow: 0.15, sunColor: '#ffcf8a', sun: 2.8, sunAz: 60, sunEl: 12, fillColor: '#6b5a4a', fill: 0.3, ambient: 0.45, fog: 0.14, contrast: 1.12, saturation: 0.75, vignette: 0.5, grain: 0.35 } },
    { id: 'doomsday', name: 'Doomsday', palette: 'doomsday', look: { bgTop: '#5a0f05', bgBottom: '#050000', glowColor: '#ff4a12', glow: 0.9, sunColor: '#ff9a5a', sun: 2.4, sunEl: 6, fillColor: '#3a0a05', fill: 0.3, ambient: 0.2, fog: 0.08, contrast: 1.2, saturation: 1.0, vignette: 0.65, grain: 0.3 } },
    { id: 'nuclearwinter', name: 'Nuclear Winter', palette: 'nuclearwinter', look: { bgTop: '#6e6c6a', bgBottom: '#1a1a1c', glowColor: '#d8d4cc', glow: 0.08, sunColor: '#e8ecf0', sun: 1.6, sunEl: 25, fillColor: '#7c8894', fill: 0.5, ambient: 0.6, fog: 0.18, exposure: -0.2, contrast: 1.0, saturation: 0.45, vignette: 0.45, grain: 0.4 } },
    { id: 'irradiated', name: 'Irradiated', palette: 'irradiated', look: { bgTop: '#2c3a12', bgBottom: '#050803', glowColor: '#b8ff3a', glow: 0.8, sunColor: '#e6ff9a', sun: 2.0, fillColor: '#3a5a10', fill: 0.5, ambient: 0.3, fog: 0.1, contrast: 1.15, saturation: 0.9, vignette: 0.55, grain: 0.3 } },
    { id: 'eclipse', name: 'Eclipse', palette: 'eclipse', look: { bgTop: '#0c0a10', bgBottom: '#000000', glowColor: '#ffd58a', glow: 0.6, sunColor: '#fff1d0', sun: 4.0, sunAz: 170, sunEl: 5, fillColor: '#2a2235', fill: 0.1, ambient: 0.08, fog: 0.02, contrast: 1.3, saturation: 0.9, vignette: 0.7, grain: 0.2 } },
    { id: 'rustteal', name: 'Rust & Teal', palette: 'tealrust', look: { bgTop: '#2c4a50', bgBottom: '#0a1214', glowColor: '#ff9a4a', glow: 0.3, sunColor: '#ffb87a', sun: 2.8, sunEl: 18, fillColor: '#2a7a80', fill: 0.8, ambient: 0.3, fog: 0.07, contrast: 1.15, saturation: 0.85, vignette: 0.5, grain: 0.25 } },
    { id: 'bunker', name: 'Bunker', palette: 'bunker', look: { bgTop: '#2a2d22', bgBottom: '#070806', glowColor: '#c8b878', glow: 0.1, sunColor: '#e8e0b0', sun: 2.2, sunEl: 55, fillColor: '#4a4d3a', fill: 0.2, ambient: 0.2, fog: 0.12, contrast: 1.2, saturation: 0.6, vignette: 0.7, grain: 0.45 } },
    { id: 'hellfire', name: 'Hellfire', palette: 'hellfire', look: { bgTop: '#3a0800', bgBottom: '#000000', glowColor: '#ff5a00', glow: 1.4, sunColor: '#ffb070', sun: 2.0, sunEl: -5, fillColor: '#ff2a00', fill: 0.9, ambient: 0.15, fog: 0.06, contrast: 1.2, saturation: 1.15, vignette: 0.6, grain: 0.25 } }
  ];

  function applyLookPreset(look, presetId) {
    const preset = LOOK_PRESETS.find((p) => p.id === presetId);
    if (!preset) return look;
    const base = defaultLook();
    const out = Object.assign({}, base, preset.look);
    out.palette = Object.assign({}, look.palette, { stops: Palettes.PRESET_BY_ID[preset.palette].stops.map((s) => ({ ...s })) });
    return out;
  }

  let idCounter = 0;
  function uid(prefix) {
    idCounter += 1;
    return `${prefix}-${Date.now().toString(36)}${idCounter.toString(36)}${Math.floor(Math.random() * 1296).toString(36)}`;
  }

  function defaultCamera(formulaId) {
    const f = Formulas.get(formulaId);
    return { dist: f.camera.dist, yaw: f.camera.yaw, pitch: f.camera.pitch, roll: 0, fov: f.camera.fov || 50, orbit: 0, tx: f.camera.tx || 0, ty: f.camera.ty || 0, tz: f.camera.tz || 0 };
  }

  function defaultSpace(formulaId) {
    return { repeat: 0, twist: 0, bound: 0, detail: 1, step: Formulas.get(formulaId).step };
  }

  function makeScene(formulaId, name, lookPresetId) {
    const f = Formulas.get(formulaId);
    let look = defaultLook();
    if (lookPresetId) look = applyLookPreset(look, lookPresetId);
    return {
      id: uid('scene'),
      name: name || f.name,
      formula: f.id,
      iter: f.iter.def,
      params: Formulas.defaults(f.id),
      space: defaultSpace(f.id),
      camera: defaultCamera(f.id),
      look,
      backdrop: defaultBackdrop()
    };
  }

  function defaultProject() {
    const a = makeScene('mandelbulb', 'Intro bulb', 'nocturne');
    const b = makeScene('amazingBox', 'Verse box', 'golden');
    const c = makeScene('kaleidoscopic', 'Drop kaleido', 'neon');
    b.camera.orbit = 4;
    c.camera.orbit = 8;
    return {
      version: VERSION,
      name: 'Untitled video',
      bpm: 120,
      beatsPerBar: 4,
      offset: 0,
      scenes: [a, b, c],
      sections: [
        { id: uid('sec'), name: 'Intro', length: 32, sceneId: a.id, transition: 4 },
        { id: uid('sec'), name: 'Verse', length: 64, sceneId: b.id, transition: 8 },
        { id: uid('sec'), name: 'Drop', length: 64, sceneId: c.id, transition: 2 }
      ],
      mod: [
        { id: uid('mod'), off: [], source: 'grid.beatPulse', dest: 'look.glow', depth: 0.12, attack: 0, release: 0, lead: 0 },
        { id: uid('mod'), off: [], source: 'master.low', dest: 'cam.dist', depth: 0.25, attack: 0.02, release: 0.25, lead: 0 },
        { id: uid('mod'), off: [], source: 'master.high', dest: 'look.palOffset', depth: 0.08, attack: 0.05, release: 0.4, lead: 0 }
      ],
      keys: [],
      master: null,
      stems: [],
      render: { width: 1920, height: 1080, fps: 30, samples: 32, outDir: './renders' }
    };
  }

  // ---------- normalisation ----------
  const num = (v, spec) => {
    const x = Number(v);
    if (!Number.isFinite(x)) return spec.def;
    return Math.min(spec.max, Math.max(spec.min, x));
  };
  const hex = (v, fallback) => (typeof v === 'string' && /^#?[0-9a-fA-F]{6}$/.test(v.trim()) ? '#' + v.trim().replace('#', '').toLowerCase() : fallback);
  const pick = (v, list, fallback) => (list.some((x) => x.id === v) ? v : fallback);
  const str = (v, fallback, max) => (typeof v === 'string' && v.trim() ? v.trim().slice(0, max || 80) : fallback);

  function normalizeLook(raw) {
    const d = defaultLook();
    const l = raw && typeof raw === 'object' ? raw : {};
    const out = {};
    Object.keys(LOOK_NUM).forEach((k) => { out[k] = num(l[k], LOOK_NUM[k]); });
    Object.keys(LOOK_COLORS).forEach((k) => { out[k] = hex(l[k], d[k]); });
    out.fogMatch = l.fogMatch !== undefined ? !!l.fogMatch : d.fogMatch;
    out.shadows = l.shadows !== undefined ? !!l.shadows : d.shadows;
    out.tonemap = pick(l.tonemap, TONEMAPS, d.tonemap);
    const pal = l.palette && typeof l.palette === 'object' ? l.palette : {};
    out.palette = {
      stops: Palettes.normalizeStops(pal.stops || d.palette.stops),
      interp: pick(pal.interp, INTERPS, d.palette.interp),
      source: pick(pal.source, COLOR_SOURCES, d.palette.source),
      wrap: pick(pal.wrap, WRAPS, d.palette.wrap)
    };
    return out;
  }

  function normalizeBackdrop(raw) {
    const d = defaultBackdrop();
    const b = raw && typeof raw === 'object' ? raw : {};
    const out = {
      kind: ['image', 'video'].includes(b.kind) ? b.kind : 'none',
      key: str(b.key, '', 300),
      name: str(b.name, '', 200),
      aspect: num(b.aspect, N('', 0.1, 10, 0.0001, d.aspect)),
      duration: num(b.duration, N('', 0, 86400, 0.001, 0)),
      fit: pick(b.fit, BACKDROP_FITS, d.fit),
      sync: pick(b.sync, BACKDROP_SYNC, d.sync),
      place: pick(b.place, BACKDROP_PLACES, d.place),
      blend: pick(b.blend, BACKDROP_BLENDS, d.blend)
    };
    if (!out.key) out.kind = 'none';
    Object.keys(BACKDROP).forEach((k) => { out[k] = num(b[k], BACKDROP[k]); });
    return out;
  }

  function normalizeScene(raw, index) {
    const s = raw && typeof raw === 'object' ? raw : {};
    const f = Formulas.get(s.formula);
    const cam = s.camera && typeof s.camera === 'object' ? s.camera : {};
    const dc = defaultCamera(f.id);
    const camera = {};
    Object.keys(CAMERA).forEach((k) => { camera[k] = num(cam[k] !== undefined ? cam[k] : dc[k], CAMERA[k]); });
    const sp = s.space && typeof s.space === 'object' ? s.space : {};
    const ds = defaultSpace(f.id);
    const space = {};
    Object.keys(SPACE).forEach((k) => { space[k] = num(sp[k] !== undefined ? sp[k] : ds[k], SPACE[k]); });
    return {
      id: str(s.id, uid('scene'), 64),
      name: str(s.name, `Scene ${index + 1}`),
      formula: f.id,
      iter: Math.round(num(s.iter, Object.assign({}, f.iter, { def: f.iter.def }))),
      params: Formulas.resolveParams(f.id, s.params),
      space,
      camera,
      look: normalizeLook(s.look),
      backdrop: normalizeBackdrop(s.backdrop)
    };
  }

  function normalizeProject(raw) {
    if (!raw || typeof raw !== 'object') return defaultProject();
    if (!(Number(raw.version) >= 2)) return migrateV1(raw);
    const p = {
      version: VERSION,
      name: str(raw.name, 'Untitled video', 120),
      bpm: num(raw.bpm, N('', 20, 400, 0.01, 120)),
      beatsPerBar: Math.round(num(raw.beatsPerBar, N('', 1, 16, 1, 4))),
      offset: num(raw.offset, N('', -60, 600, 0.001, 0)),
      scenes: (Array.isArray(raw.scenes) ? raw.scenes : []).slice(0, 64).map(normalizeScene),
      sections: [],
      mod: [],
      keys: [],
      master: raw.master && typeof raw.master === 'object' ? { name: str(raw.master.name, 'master', 200), key: str(raw.master.key, '', 300) } : null,
      stems: [],
      render: {}
    };
    if (!p.scenes.length) p.scenes = defaultProject().scenes;
    const seen = new Set();
    p.scenes.forEach((s) => { if (seen.has(s.id)) s.id = uid('scene'); seen.add(s.id); });
    const sceneIds = new Set(p.scenes.map((s) => s.id));

    p.sections = (Array.isArray(raw.sections) ? raw.sections : []).slice(0, 256).map((s, i) => ({
      id: str(s && s.id, uid('sec'), 64),
      name: str(s && s.name, `Section ${i + 1}`, 60),
      length: Math.round(num(s && s.length, SECTION.length)),
      sceneId: sceneIds.has(s && s.sceneId) ? s.sceneId : p.scenes[0].id,
      transition: Math.round(num(s && s.transition, SECTION.transition))
    }));
    if (!p.sections.length) p.sections = [{ id: uid('sec'), name: 'Section 1', length: 64, sceneId: p.scenes[0].id, transition: 4 }];

    p.stems = (Array.isArray(raw.stems) ? raw.stems : []).slice(0, 32).map((s) => ({
      id: str(s && s.id, uid('stem'), 64),
      name: str(s && s.name, 'stem', 200),
      key: str(s && s.key, '', 300),
      role: ROLES.includes(s && s.role) ? s.role : 'OTHER'
    }));

    p.mod = (Array.isArray(raw.mod) ? raw.mod : []).slice(0, 48).map((m) => ({
      id: str(m && m.id, uid('mod'), 64),
      // Scenes in which the route is switched off. An old project's global off becomes off everywhere.
      off: Array.isArray(m && m.off) ? [...new Set(m.off.filter((id) => sceneIds.has(id)))]
        : (m && m.on === false ? [...sceneIds] : []),
      source: str(m && m.source, 'grid.beatPulse', 120),
      dest: str(m && m.dest, 'look.glow', 120),
      depth: num(m && m.depth, N('', -1, 1, 0.001, 0.2)),
      attack: num(m && m.attack, N('', 0, 2, 0.001, 0.02)),
      release: num(m && m.release, N('', 0, 4, 0.001, 0.2)),
      lead: num(m && m.lead, N('', -8, 8, 0.01, 0))
    }));

    // Keyframe lanes: one per parameter. Points are stored relative to their section, so they move
    // with it when sections are resized or reordered.
    const secIds = new Set(p.sections.map((s) => s.id));
    const seenDest = new Set();
    p.keys = (Array.isArray(raw.keys) ? raw.keys : []).slice(0, 64).map((k) => {
      const dest = str(k && k.dest, '', 120);
      if (!keySpec(dest) || seenDest.has(dest)) return null;
      seenDest.add(dest);
      const pts = (Array.isArray(k.pts) ? k.pts : []).slice(0, 512)
        .filter((q) => q && secIds.has(q.sec) && Number.isFinite(Number(q.b)) && Number.isFinite(Number(q.v)))
        .map((q) => ({ sec: q.sec, b: Math.max(0, Number(q.b)), v: Number(q.v) }));
      return {
        id: str(k.id, uid('key'), 64),
        dest,
        interp: KEY_INTERPS.some((x) => x.id === k.interp) ? k.interp : 'smooth',
        off: Array.isArray(k.off) ? [...new Set(k.off.filter((id) => sceneIds.has(id)))] : [],
        pts
      };
    }).filter(Boolean);

    const r = raw.render && typeof raw.render === 'object' ? raw.render : {};
    p.render = {
      width: Math.round(num(r.width, N('', 16, 7680, 1, 1920))),
      height: Math.round(num(r.height, N('', 16, 4320, 1, 1080))),
      fps: num(r.fps, N('', 1, 120, 0.001, 30)),
      samples: Math.round(num(r.samples, N('', 1, 1024, 1, 32))),
      outDir: str(r.outDir, './renders', 300)
    };
    return p;
  }

  // First prototype (fractal-video-project-v1): keep names, tempo, scene/section structure.
  function migrateV1(raw) {
    const base = defaultProject();
    const scenes = (Array.isArray(raw.scenes) ? raw.scenes : []).map((s, i) => {
      const sc = makeScene(Formulas.get(s && s.formula).id, str(s && s.name, `Scene ${i + 1}`));
      sc.id = str(s && s.id, sc.id, 64);
      return sc;
    });
    const p = {
      version: VERSION,
      name: str(raw.name, base.name, 120),
      bpm: Number(raw.bpm) > 0 ? Number(raw.bpm) : base.bpm,
      beatsPerBar: 4,
      offset: 0,
      scenes: scenes.length ? scenes : base.scenes,
      sections: [],
      mod: base.mod,
      keys: [],
      master: null,
      stems: [],
      render: Object.assign({}, base.render, raw.renderConfig || {})
    };
    const ids = new Set(p.scenes.map((s) => s.id));
    const old = (Array.isArray(raw.sections) ? raw.sections : []).slice().sort((a, b) => (Number(a.start) || 0) - (Number(b.start) || 0));
    p.sections = old.map((s, i) => ({
      id: str(s && s.id, uid('sec'), 64),
      name: str(s && s.name, `Section ${i + 1}`),
      length: Math.max(1, Math.round(Number(s && s.length) || 16)),
      sceneId: ids.has(s && s.sceneId) ? s.sceneId : p.scenes[0].id,
      transition: 4
    }));
    if (!p.sections.length) p.sections = base.sections.map((s, i) => Object.assign({}, s, { sceneId: p.scenes[i % p.scenes.length].id }));
    return normalizeProject(p);
  }

  // ---------- modulation sources and destinations ----------
  const GRID_SOURCES = [
    { id: 'grid.beatPulse', label: 'Beat pulse', group: 'Grid' },
    { id: 'grid.barPulse', label: 'Bar pulse', group: 'Grid' },
    { id: 'grid.beatSaw', label: 'Beat ramp', group: 'Grid' },
    { id: 'grid.barSaw', label: 'Bar ramp', group: 'Grid' },
    { id: 'grid.section', label: 'Section progress', group: 'Grid' },
    { id: 'grid.lfo4', label: 'Slow wave (4 bars)', group: 'Grid' },
    { id: 'grid.lfo16', label: 'Very slow wave (16 bars)', group: 'Grid' }
  ];
  const MASTER_SOURCES = [
    { id: 'master.rms', label: 'Master level', group: 'Master' },
    { id: 'master.low', label: 'Master lows', group: 'Master' },
    { id: 'master.mid', label: 'Master mids', group: 'Master' },
    { id: 'master.high', label: 'Master highs', group: 'Master' },
    { id: 'master.onset', label: 'Master onsets', group: 'Master' }
  ];

  function sources(project) {
    const stems = [];
    (project.stems || []).forEach((s) => {
      const short = s.name.replace(/\.[^.]+$/, '');
      stems.push({ id: `stem:${s.id}.env`, label: `${short} · level`, group: 'Stems' });
      stems.push({ id: `stem:${s.id}.onset`, label: `${short} · hits`, group: 'Stems' });
    });
    return GRID_SOURCES.concat(MASTER_SOURCES, stems);
  }

  const isGridSource = (id) => String(id).startsWith('grid.');
  // Is a route on in this scene? Routes are on everywhere unless switched off per scene.
  const routeOn = (row, sceneId) => !(row.off || []).includes(sceneId);

  // ---------- movie quality presets ----------
  // scale: fraction of the output size; samples: per pixel (null = the project's Samples / frame);
  // march: ray-march quality (draft = 110 steps, no shadows; balanced = 180/32; high = 300/64);
  // crf: encoder quality override (null = the Encoding choice). cost: rough GPU work per ray-march sample.
  const MARCH_COST = { draft: 0.4, balanced: 0.65, high: 1 };
  const MOVIE_PRESETS = [
    { id: 'test', label: 'Quick test', scale: 0.5, samples: 2, march: 'draft', crf: 23, suffix: '-test', hint: 'Half size, 2 samples, no shadows: check timing, cuts and keying.' },
    { id: 'preview', label: 'Preview', scale: 0.75, samples: 4, march: 'balanced', crf: 20, suffix: '-preview', hint: '3/4 size, 4 samples, Balanced ray marching: judge motion and colour.' },
    { id: 'standard', label: 'Standard', scale: 1, samples: 8, march: 'balanced', crf: null, suffix: '', hint: 'Full size, 8 samples, Balanced ray marching. Motion and compression hide most of the remaining grain.' },
    { id: 'high', label: 'High', scale: 1, samples: 16, march: 'high', crf: null, suffix: '', hint: 'Full size, 16 samples, High ray marching (finer detail and shadows).' },
    { id: 'final', label: 'Final', scale: 1, samples: null, march: 'high', crf: null, suffix: '', hint: 'Full size, Samples / frame from the Output format, High ray marching.' }
  ];
  // Relative GPU cost of a preset against Final at the given project sample count.
  function movieCost(preset, projectSamples) {
    const s = preset.samples || projectSamples;
    return (preset.scale * preset.scale * s * MARCH_COST[preset.march]) / Math.max(1, projectSamples);
  }

  // ---------- keyframes ----------
  const KEY_INTERPS = [{ id: 'smooth', label: 'Smooth' }, { id: 'linear', label: 'Linear' }, { id: 'step', label: 'Hold (step)' }];
  // Range and label of a keyable parameter: formula parameters, space and numeric look values.
  // Camera pose values are keyable too (cam.dist ... cam.tz); auto-orbit is a rate and stays per scene.
  const CAM_KEYS = ['dist', 'yaw', 'pitch', 'roll', 'fov', 'tx', 'ty', 'tz'];
  function keySpec(dest, formulaId) {
    const [group, key] = String(dest).split('.');
    if (group === 'shape') {
      const fs = formulaId ? [Formulas.get(formulaId)] : Formulas.FORMULAS;
      for (const f of fs) {
        const q = f.params.find((x) => x.key === key);
        if (q) return { label: q.label, min: q.min, max: q.max, step: q.step };
      }
      return null;
    }
    if (group === 'space' && SPACE[key]) return { label: SPACE[key].label, min: SPACE[key].min, max: SPACE[key].max, step: SPACE[key].step };
    if (group === 'look' && LOOK_NUM[key]) return { label: LOOK_NUM[key].label, min: LOOK_NUM[key].min, max: LOOK_NUM[key].max, step: LOOK_NUM[key].step };
    if (group === 'cam' && CAM_KEYS.includes(key)) {
      const c = CAMERA[key];
      return { label: `Camera ${c.label.toLowerCase()}`, min: c.min, max: c.max, step: c.step, angle: key === 'yaw' || key === 'roll', log: key === 'dist' };
    }
    return null;
  }

  const CAMERA_DESTS = [
    { id: 'cam.dist', label: 'Camera push', group: 'Camera', min: 0, max: 1 },
    { id: 'cam.yaw', label: 'Camera yaw', group: 'Camera', min: -180, max: 180 },
    { id: 'cam.pitch', label: 'Camera pitch', group: 'Camera', min: -89, max: 89 },
    { id: 'cam.roll', label: 'Camera roll', group: 'Camera', min: -180, max: 180 },
    { id: 'cam.fov', label: 'Field of view', group: 'Camera', min: 10, max: 120 },
    { id: 'cam.shake', label: 'Camera shake', group: 'Camera', min: 0, max: 10 }
  ];
  const LOOK_DESTS = ['palOffset', 'hue', 'glow', 'exposure', 'fog', 'sun', 'fill', 'saturation', 'contrast', 'vignette', 'grain']
    .map((k) => ({ id: `look.${k}`, label: k === 'palOffset' ? 'Palette offset' : LOOK_NUM[k].label, group: 'Colour & light', min: LOOK_NUM[k].min, max: LOOK_NUM[k].max }));
  const BG_DESTS = ['brightness', 'opacity', 'zoom', 'blur', 'saturation', 'panX', 'panY']
    .map((k) => ({ id: `bg.${k}`, label: `Backdrop ${BACKDROP[k].label.toLowerCase()}`, group: 'Backdrop', min: BACKDROP[k].min, max: BACKDROP[k].max }));
  const SPACE_DESTS = [
    { id: 'space.twist', label: 'Twist', group: 'Space', min: SPACE.twist.min, max: SPACE.twist.max },
    { id: 'space.bound', label: 'Bound radius', group: 'Space', min: SPACE.bound.min, max: SPACE.bound.max }
  ];

  // Shape destinations are listed for the given formula; rows pointing at a key the active
  // scene's formula does not have are simply inactive in that scene.
  function destinations(formulaId) {
    const f = Formulas.get(formulaId);
    const shape = f.params.map((p) => ({ id: `shape.${p.key}`, label: p.label, group: `Shape (${f.name})`, min: p.min, max: p.max }));
    return shape.concat(SPACE_DESTS, CAMERA_DESTS, LOOK_DESTS, BG_DESTS);
  }

  function allShapeDest(id) {
    const key = String(id).replace('shape.', '');
    for (const f of Formulas.FORMULAS) {
      const p = f.params.find((x) => x.key === key);
      if (p) return { id, label: p.label, group: 'Shape', min: p.min, max: p.max };
    }
    return null;
  }

  function destSpec(id) {
    if (String(id).startsWith('shape.')) return allShapeDest(id);
    return CAMERA_DESTS.concat(LOOK_DESTS, SPACE_DESTS, BG_DESTS).find((d) => d.id === id) || null;
  }

  // Suggested routes per stem role (plan section 3.3).
  const ROLE_ROUTES = {
    KICK: [{ f: 'onset', dest: 'cam.dist', depth: 0.25, attack: 0, release: 0.15 }, { f: 'onset', dest: 'look.glow', depth: 0.1, attack: 0, release: 0.2 }],
    SNARE: [{ f: 'onset', dest: 'look.exposure', depth: 0.06, attack: 0, release: 0.12 }],
    HATS: [{ f: 'env', dest: 'look.palOffset', depth: 0.05, attack: 0.01, release: 0.1 }],
    PERC: [{ f: 'onset', dest: 'cam.shake', depth: 0.1, attack: 0, release: 0.1 }],
    BASS: [{ f: 'env', dest: 'space.twist', depth: 0.03, attack: 0.15, release: 0.6 }],
    LEAD: [{ f: 'env', dest: 'look.hue', depth: 0.06, attack: 0.1, release: 0.5 }],
    VOCAL: [{ f: 'env', dest: 'look.sun', depth: 0.15, attack: 0.08, release: 0.5 }],
    PADS: [{ f: 'env', dest: 'look.fog', depth: 0.06, attack: 0.5, release: 1.5 }],
    FX: [{ f: 'env', dest: 'cam.dist', depth: 0.3, attack: 0.4, release: 0.8, lead: 2 }],
    OTHER: []
  };

  function guessRole(name) {
    const n = String(name).toLowerCase();
    const rules = [['KICK', /kick|bd\b|bassdrum/], ['SNARE', /snare|clap|sd\b/], ['HATS', /hat|hh|cymb|ride/], ['PERC', /perc|tom|shaker|conga/],
      ['BASS', /bass|sub/], ['VOCAL', /vox|vocal|voice/], ['LEAD', /lead|synth|melod|arp|piano|guitar/], ['PADS', /pad|atmo|string|choir/], ['FX', /fx|riser|sweep|noise|impact/]];
    const hit = rules.find(([, re]) => re.test(n));
    return hit ? hit[0] : 'OTHER';
  }

  return {
    VERSION, LOOK_NUM, LOOK_COLORS, COLOR_SOURCES, WRAPS, INTERPS, TONEMAPS, CAMERA, SPACE, SECTION, ROLES, LOOK_PRESETS, ROLE_ROUTES,
    BACKDROP, BACKDROP_FITS, BACKDROP_SYNC, BACKDROP_PLACES, BACKDROP_BLENDS,
    uid, defaultLook, applyLookPreset, defaultCamera, defaultSpace, defaultBackdrop, makeScene, defaultProject,
    normalizeLook, normalizeBackdrop, normalizeScene, normalizeProject,
    sources, isGridSource, routeOn, KEY_INTERPS, CAM_KEYS, keySpec, MOVIE_PRESETS, movieCost, destinations, destSpec, guessRole
  };
});
