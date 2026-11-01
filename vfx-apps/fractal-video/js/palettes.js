/* Colour science + palette library for per-scene colour schemes.
   Palettes are up to 8 stops { p: 0..1, c: '#rrggbb' } (sRGB hex, what the user sees).
   The GPU receives them as linear RGB or OKLab depending on the interpolation mode. */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.FVPalettes = api;
})(typeof self !== 'undefined' ? self : globalThis, function () {
  'use strict';

  const MAX_STOPS = 8;
  const clamp01 = (x) => Math.min(1, Math.max(0, x));

  // ---------- colour conversions ----------
  function hexToRgb(hex) {
    let h = String(hex || '').trim().replace('#', '');
    if (h.length === 3) h = h.split('').map((ch) => ch + ch).join('');
    if (!/^[0-9a-fA-F]{6}$/.test(h)) return [0, 0, 0];
    const n = parseInt(h, 16);
    return [((n >> 16) & 255) / 255, ((n >> 8) & 255) / 255, (n & 255) / 255];
  }

  function rgbToHex(rgb) {
    return '#' + rgb.map((v) => Math.round(clamp01(v) * 255).toString(16).padStart(2, '0')).join('');
  }

  const toLin = (c) => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
  const toSrgb = (c) => (c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(Math.max(c, 0), 1 / 2.4) - 0.055);
  const srgbToLinear = (rgb) => rgb.map(toLin);
  const linearToSrgb = (rgb) => rgb.map(toSrgb);

  function linearToOklab([r, g, b]) {
    const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return [
      0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
      1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
      0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s
    ];
  }

  function oklabToLinear([L, a, b]) {
    const l = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3;
    const m = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3;
    const s = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3;
    return [
      4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
      -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
      -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
    ];
  }

  const hexToOklab = (hex) => linearToOklab(srgbToLinear(hexToRgb(hex)));

  function oklabToOklch([L, a, b]) {
    return [L, Math.hypot(a, b), (Math.atan2(b, a) * 180 / Math.PI + 360) % 360];
  }

  function oklchToOklab([L, C, H]) {
    const h = H * Math.PI / 180;
    return [L, C * Math.cos(h), C * Math.sin(h)];
  }

  const inGamut = (lin) => lin.every((v) => v >= -1e-4 && v <= 1 + 1e-4);

  // OKLCH -> hex, reducing chroma until the colour fits in sRGB.
  function oklchToHex(lch) {
    let [L, C, H] = lch;
    L = clamp01(L);
    let lin = oklabToLinear(oklchToOklab([L, C, H]));
    for (let i = 0; i < 24 && !inGamut(lin); i += 1) {
      C *= 0.85;
      lin = oklabToLinear(oklchToOklab([L, C, H]));
    }
    return rgbToHex(linearToSrgb(lin.map(clamp01)));
  }

  const hexToOklch = (hex) => oklabToOklch(hexToOklab(hex));

  // ---------- palette operations ----------
  function normalizeStops(stops) {
    const list = (Array.isArray(stops) ? stops : [])
      .map((s) => ({ p: clamp01(Number(s && s.p)), c: rgbToHex(hexToRgb(s && s.c)) }))
      .filter((s) => Number.isFinite(s.p))
      .sort((a, b) => a.p - b.p)
      .slice(0, MAX_STOPS);
    if (list.length === 0) return [{ p: 0, c: '#000000' }, { p: 1, c: '#ffffff' }];
    if (list.length === 1) return [{ p: 0, c: list[0].c }, { p: 1, c: list[0].c }];
    return list;
  }

  const lerp = (a, b, t) => a + (b - a) * t;
  const lerp3 = (a, b, t) => [lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)];
  const smooth = (t) => t * t * (3 - 2 * t);

  // Colour of a palette at u (0..1) as hex, honouring the interpolation mode.
  function sample(stops, u, interp) {
    const s = normalizeStops(stops);
    if (u <= s[0].p) return s[0].c;
    for (let i = 1; i < s.length; i += 1) {
      if (u <= s[i].p) {
        let t = (u - s[i - 1].p) / Math.max(s[i].p - s[i - 1].p, 1e-6);
        if (interp === 'steps') return s[i - 1].c;
        if (interp === 'smooth') t = smooth(t);
        if (interp === 'rgb') {
          return rgbToHex(linearToSrgb(lerp3(srgbToLinear(hexToRgb(s[i - 1].c)), srgbToLinear(hexToRgb(s[i].c)), t)));
        }
        return rgbToHex(linearToSrgb(oklabToLinear(lerp3(hexToOklab(s[i - 1].c), hexToOklab(s[i].c), t)).map(clamp01)));
      }
    }
    return s[s.length - 1].c;
  }

  // n evenly spaced stops sampled from a palette.
  function resample(stops, n, interp) {
    const count = Math.max(2, Math.min(MAX_STOPS, n || MAX_STOPS));
    const out = [];
    for (let i = 0; i < count; i += 1) {
      const p = i / (count - 1);
      out.push({ p, c: sample(stops, p, interp) });
    }
    return out;
  }

  function reverse(stops) {
    return normalizeStops(stops).map((s) => ({ p: 1 - s.p, c: s.c })).sort((a, b) => a.p - b.p);
  }

  // Shift hue (degrees), scale chroma and lightness of every stop in OKLCH.
  function adjust(stops, { hue = 0, chroma = 1, light = 0 } = {}) {
    return normalizeStops(stops).map((s) => {
      const [L, C, H] = hexToOklch(s.c);
      return { p: s.p, c: oklchToHex([L + light, C * chroma, (H + hue + 360) % 360]) };
    });
  }

  function spread(stops) {
    const s = normalizeStops(stops);
    return s.map((st, i) => ({ p: i / (s.length - 1), c: st.c }));
  }

  // CSS gradient for previews. Chrome renders `in oklab` gradients natively.
  function css(stops, interp, direction) {
    const s = normalizeStops(stops);
    const dir = direction || 'to right';
    if (interp === 'steps') {
      const parts = [];
      s.forEach((st, i) => {
        const end = i < s.length - 1 ? s[i + 1].p : 1;
        parts.push(`${st.c} ${(st.p * 100).toFixed(2)}%`, `${st.c} ${(end * 100).toFixed(2)}%`);
      });
      return `linear-gradient(${dir}, ${s[0].c} 0%, ${parts.join(', ')})`;
    }
    if (interp === 'smooth') {
      const many = resample(s, 8, 'smooth');
      return `linear-gradient(${dir} in oklab, ${many.map((st) => `${st.c} ${(st.p * 100).toFixed(2)}%`).join(', ')})`;
    }
    const space = interp === 'rgb' ? ' in srgb-linear' : ' in oklab';
    return `linear-gradient(${dir}${space}, ${s.map((st) => `${st.c} ${(st.p * 100).toFixed(2)}%`).join(', ')})`;
  }

  // Flattened arrays for the shader: positions (8), colours (24) in the space the shader interpolates in.
  const INTERP_CODE = { rgb: 0, oklab: 1, steps: 2, smooth: 3 };
  function toGPU(stops, interp) {
    const s = normalizeStops(stops);
    const pos = new Float32Array(MAX_STOPS);
    const col = new Float32Array(MAX_STOPS * 3);
    const perceptual = interp === 'oklab' || interp === 'smooth' || !interp;
    s.forEach((st, i) => {
      pos[i] = st.p;
      const lin = srgbToLinear(hexToRgb(st.c));
      const v = perceptual ? linearToOklab(lin) : lin;
      col[i * 3] = v[0];
      col[i * 3 + 1] = v[1];
      col[i * 3 + 2] = v[2];
    });
    return { pos, col, n: s.length, interp: INTERP_CODE[interp] ?? 1 };
  }

  // Blend two palettes (used for scene transitions): both resampled to 8 stops and mixed in OKLab.
  function blend(stopsA, interpA, stopsB, interpB, t) {
    if (t <= 0) return normalizeStops(stopsA);
    if (t >= 1) return normalizeStops(stopsB);
    const a = resample(stopsA, MAX_STOPS, interpA);
    const b = resample(stopsB, MAX_STOPS, interpB);
    return a.map((st, i) => ({
      p: st.p,
      c: rgbToHex(linearToSrgb(oklabToLinear(lerp3(hexToOklab(st.c), hexToOklab(b[i].c), t)).map(clamp01)))
    }));
  }

  function mixHex(a, b, t) {
    return rgbToHex(linearToSrgb(oklabToLinear(lerp3(hexToOklab(a), hexToOklab(b), clamp01(t))).map(clamp01)));
  }

  // ---------- generators ----------
  const HARMONIES = {
    analogous: { label: 'Analogous', hues: [-35, -12, 12, 35] },
    complementary: { label: 'Complementary', hues: [0, 180] },
    split: { label: 'Split complementary', hues: [0, 150, 210] },
    triadic: { label: 'Triadic', hues: [0, 120, 240] },
    tetradic: { label: 'Tetradic', hues: [0, 90, 180, 270] },
    monochrome: { label: 'Monochrome', hues: [0] }
  };

  // Harmony palette from a base colour: hues from the rule, lightness ramp dark -> light,
  // which is what makes a palette read well on a lit 3D fractal.
  function harmony(baseHex, rule, count, opts) {
    const o = Object.assign({ dark: 0.16, light: 0.92, chroma: 1 }, opts || {});
    const [, C0, H0] = hexToOklch(baseHex);
    const hues = (HARMONIES[rule] || HARMONIES.analogous).hues;
    const n = Math.max(2, Math.min(MAX_STOPS, count || 6));
    const base = Math.max(C0, 0.06) * o.chroma;
    const stops = [];
    for (let i = 0; i < n; i += 1) {
      const t = i / (n - 1);
      const L = lerp(o.dark, o.light, t);
      // chroma peaks in the mid-tones, fades at the dark/light ends
      const C = base * (0.35 + 0.65 * Math.sin(Math.PI * Math.min(1, t * 1.1)));
      const H = H0 + hues[i % hues.length];
      stops.push({ p: t, c: oklchToHex([L, C, (H + 360) % 360]) });
    }
    return stops;
  }

  // Inigo Quilez cosine palette a + b*cos(2pi(c*t + d)), sampled to 8 stops.
  function cosine(a, b, c, d) {
    const stops = [];
    for (let i = 0; i < MAX_STOPS; i += 1) {
      const t = i / (MAX_STOPS - 1);
      const rgb = [0, 1, 2].map((k) => a[k] + b[k] * Math.cos(2 * Math.PI * (c[k] * t + d[k])));
      stops.push({ p: t, c: rgbToHex(rgb) });
    }
    return stops;
  }

  function seeded(seed) {
    let s = (seed >>> 0) || 1;
    return () => {
      s ^= s << 13; s >>>= 0; s ^= s >> 17; s ^= s << 5; s >>>= 0;
      return s / 4294967296;
    };
  }

  function random(seed) {
    const rnd = seeded(seed == null ? Math.floor(Math.random() * 1e9) : seed);
    const rules = Object.keys(HARMONIES);
    const rule = rules[Math.floor(rnd() * rules.length)];
    const base = oklchToHex([0.65, 0.08 + rnd() * 0.12, rnd() * 360]);
    const count = 4 + Math.floor(rnd() * 4);
    const stops = harmony(base, rule, count, { dark: 0.08 + rnd() * 0.15, light: 0.78 + rnd() * 0.18, chroma: 0.8 + rnd() * 0.7 });
    return rnd() < 0.35 ? reverse(stops) : stops;
  }

  // ---------- presets ----------
  const S = (list) => list.map(([p, c]) => ({ p, c }));
  const even = (hexes) => hexes.map((c, i) => ({ p: i / (hexes.length - 1), c }));

  const PRESETS = [
    // perceptual / scientific
    { id: 'viridis', name: 'Viridis', group: 'Perceptual', stops: even(['#440154', '#46327e', '#365c8d', '#277f8e', '#1fa187', '#4ac16d', '#a0da39', '#fde725']) },
    { id: 'magma', name: 'Magma', group: 'Perceptual', stops: even(['#000004', '#1c1044', '#51127c', '#832681', '#b73779', '#e75263', '#fc8961', '#fcfdbf']) },
    { id: 'inferno', name: 'Inferno', group: 'Perceptual', stops: even(['#000004', '#1f0c48', '#550f6d', '#88226a', '#ba3655', '#e35933', '#f98e09', '#fcffa4']) },
    { id: 'plasma', name: 'Plasma', group: 'Perceptual', stops: even(['#0d0887', '#5302a3', '#8b0aa5', '#b83289', '#db5c68', '#f48849', '#febd2a', '#f0f921']) },
    { id: 'cividis', name: 'Cividis', group: 'Perceptual', stops: even(['#00204c', '#213d6b', '#4c546c', '#6b6d70', '#8a8779', '#aea374', '#d3c164', '#fee838']) },
    { id: 'mako', name: 'Mako', group: 'Perceptual', stops: even(['#0b0405', '#2b1c35', '#3e356b', '#3b5698', '#3487a6', '#40b7ad', '#8ad9b1', '#def5e5']) },
    { id: 'rocket', name: 'Rocket', group: 'Perceptual', stops: even(['#03051a', '#36193e', '#701f57', '#ad1759', '#e13342', '#f37651', '#f6b48f', '#faebdd']) },
    { id: 'turbo', name: 'Turbo', group: 'Perceptual', stops: even(['#30123b', '#4669db', '#2fb2f3', '#2ee59a', '#93fc3b', '#e2dc38', '#fb7e21', '#7a0403']) },
    { id: 'twilight', name: 'Twilight (cyclic)', group: 'Perceptual', stops: even(['#e2d9e2', '#9eb4cc', '#6a7fc0', '#5e43a5', '#2f1436', '#7d2d58', '#c0715e', '#e2d9e2']) },
    // artistic
    { id: 'ember', name: 'Ember', group: 'Warm', stops: S([[0, '#0a0202'], [0.22, '#3d0a05'], [0.45, '#8f1f06'], [0.65, '#d9550b'], [0.82, '#ffa630'], [1, '#fff1c9']]) },
    { id: 'goldleaf', name: 'Gold Leaf', group: 'Warm', stops: S([[0, '#140d03'], [0.3, '#5a3b0c'], [0.55, '#b07a1e'], [0.75, '#e8c25a'], [0.9, '#fff0b3'], [1, '#ffffff']]) },
    { id: 'desert', name: 'Desert Dusk', group: 'Warm', stops: S([[0, '#1d1430'], [0.25, '#5b2a4a'], [0.5, '#b4544a'], [0.72, '#e39a5d'], [0.88, '#f3cf8e'], [1, '#fbf1d6']]) },
    { id: 'bloodmoon', name: 'Blood Moon', group: 'Warm', stops: S([[0, '#050000'], [0.35, '#4a0000'], [0.6, '#a30d0d'], [0.8, '#ff4d2e'], [1, '#ffd2b8']]) },
    { id: 'copper', name: 'Copper Patina', group: 'Warm', stops: S([[0, '#0f1412'], [0.25, '#2d5b4f'], [0.45, '#5fa58f'], [0.6, '#b0703e'], [0.8, '#e2a172'], [1, '#f7e3cf']]) },
    { id: 'glacier', name: 'Glacier', group: 'Cool', stops: S([[0, '#020611'], [0.25, '#0b2447'], [0.5, '#19579b'], [0.72, '#55a8d8'], [0.88, '#b7e3f5'], [1, '#ffffff']]) },
    { id: 'deepsea', name: 'Deep Sea', group: 'Cool', stops: S([[0, '#00040a'], [0.3, '#002b3d'], [0.55, '#006d77'], [0.75, '#2ec4b6'], [0.9, '#b8f2e6'], [1, '#f0fffb']]) },
    { id: 'aurora', name: 'Aurora', group: 'Cool', stops: S([[0, '#030712'], [0.25, '#0b3d5c'], [0.45, '#11a37f'], [0.62, '#7cf29c'], [0.8, '#a06cf5'], [1, '#f0d9ff']]) },
    { id: 'bioluminescent', name: 'Bioluminescent', group: 'Cool', stops: S([[0, '#000000'], [0.4, '#001a2e'], [0.6, '#004c6d'], [0.78, '#00b4d8'], [0.9, '#90f1ff'], [1, '#e0fff8']]) },
    { id: 'moss', name: 'Moss & Stone', group: 'Natural', stops: S([[0, '#0d0f0a'], [0.3, '#2f3a22'], [0.55, '#5f7343'], [0.75, '#a3a37a'], [0.9, '#d8d3b8'], [1, '#f4f1e6']]) },
    { id: 'chrome', name: 'Chrome', group: 'Natural', stops: S([[0, '#050608'], [0.2, '#3a3f47'], [0.4, '#c9cfd6'], [0.55, '#4b525c'], [0.75, '#e9edf2'], [1, '#8b939e']]) },
    { id: 'mono', name: 'Monochrome', group: 'Natural', stops: S([[0, '#000000'], [1, '#ffffff']]) },
    { id: 'inkpaper', name: 'Ink & Paper', group: 'Natural', stops: S([[0, '#0b0b12'], [0.5, '#5d5a66'], [0.8, '#d9d2c3'], [1, '#f6f1e4']]) },
    { id: 'neon', name: 'Neon Night', group: 'Vivid', stops: S([[0, '#05010f'], [0.2, '#2a0a5e'], [0.4, '#ff00a8'], [0.6, '#00e5ff'], [0.8, '#7a5cff'], [1, '#e8fbff']]) },
    { id: 'vapor', name: 'Vaporwave', group: 'Vivid', stops: S([[0, '#1a1033'], [0.25, '#5b2a86'], [0.5, '#ff71ce'], [0.7, '#01cdfe'], [0.85, '#05ffa1'], [1, '#fffb96']]) },
    { id: 'candy', name: 'Candy', group: 'Vivid', stops: S([[0, '#2d0b3a'], [0.25, '#ff5d8f'], [0.5, '#ffd166'], [0.75, '#06d6a0'], [1, '#118ab2']]) },
    { id: 'pastel', name: 'Pastel Dream', group: 'Vivid', stops: S([[0, '#3d3a5c'], [0.25, '#a8a4e6'], [0.5, '#f7c6d9'], [0.75, '#fde2b8'], [1, '#d4f5ec']]) },
    { id: 'infrared', name: 'Infrared', group: 'Vivid', stops: S([[0, '#000020'], [0.2, '#3b0f70'], [0.4, '#b5179e'], [0.6, '#f72585'], [0.8, '#ff9e00'], [1, '#ffffbf']]) },
    { id: 'rainbow', name: 'Cosine Rainbow', group: 'Vivid', stops: cosine([0.5, 0.5, 0.5], [0.5, 0.5, 0.5], [1, 1, 1], [0, 0.33, 0.67]) },
    { id: 'nebula', name: 'Nebula', group: 'Vivid', stops: cosine([0.5, 0.4, 0.6], [0.5, 0.4, 0.4], [1, 1, 0.5], [0.8, 0.9, 0.3]) },
    // wasteland: dust, rust, ash, poisoned light
    { id: 'dustbowl', name: 'Dust Bowl', group: 'Wasteland', stops: even(['#0e0b08', '#2e2318', '#5c4630', '#8f7150', '#bfa078', '#e3cfa6']) },
    { id: 'rustbelt', name: 'Rust Belt', group: 'Wasteland', stops: even(['#0a0605', '#2a120a', '#5a2410', '#8c3f1a', '#b5652f', '#d49a68', '#e8cdb0']) },
    { id: 'ashcinder', name: 'Ash & Cinder', group: 'Wasteland', stops: S([[0, '#090807'], [0.3, '#24211e'], [0.55, '#4a453f'], [0.75, '#7d756c'], [0.88, '#c2551c'], [1, '#ffb36b']]) },
    { id: 'irradiated', name: 'Irradiated', group: 'Wasteland', stops: even(['#060805', '#1a2410', '#3a4a14', '#6f7d1c', '#a8b828', '#d8f04a', '#f4ffc0']) },
    { id: 'chemspill', name: 'Chem Spill', group: 'Wasteland', stops: even(['#03080a', '#0d2a2a', '#1c4f3f', '#3f7d3a', '#9fb52a', '#f0e04a']) },
    { id: 'sandstorm', name: 'Sandstorm', group: 'Wasteland', stops: even(['#1a0f05', '#4d2a0c', '#8a4a12', '#c77a26', '#e8a94e', '#f6dca0']) },
    { id: 'tealrust', name: 'Teal & Rust', group: 'Wasteland', stops: even(['#071214', '#12333a', '#2c5a5e', '#6e7062', '#b0683a', '#e09a5c', '#f2d3a8']) },
    { id: 'oxidized', name: 'Oxidized Iron', group: 'Wasteland', stops: S([[0, '#080605'], [0.25, '#2e1b12'], [0.45, '#63301a'], [0.6, '#8f4a24'], [0.75, '#5b6a5c'], [0.9, '#9fae9c'], [1, '#d6dccf']]) },
    { id: 'concrete', name: 'Brutalist Concrete', group: 'Wasteland', stops: even(['#0c0d0e', '#222527', '#3d4245', '#5e6466', '#878b89', '#b4b4ac', '#d8d4c8']) },
    { id: 'bunker', name: 'Bunker', group: 'Wasteland', stops: even(['#070806', '#191c14', '#2d3222', '#454b31', '#636443', '#8a8460', '#b9b08a']) },
    { id: 'sulphur', name: 'Sulphur & Smoke', group: 'Wasteland', stops: even(['#0a0906', '#22201a', '#3d3a2c', '#5c5832', '#8f8a2e', '#c9c13c', '#f0eb8a']) },
    // dark and grim: mostly shadow, one hard accent
    { id: 'doomsday', name: 'Doomsday', group: 'Dark & grim', stops: even(['#000000', '#1a0000', '#3d0303', '#6b0b05', '#a3260a', '#e0611a', '#ffb05a']) },
    { id: 'hellfire', name: 'Hellfire', group: 'Dark & grim', stops: even(['#000000', '#200000', '#5c0200', '#a01000', '#e04000', '#ff9a1a', '#fff27a']) },
    { id: 'driedblood', name: 'Dried Blood', group: 'Dark & grim', stops: even(['#030101', '#170405', '#2d0709', '#4a0c0e', '#6b1a14', '#8c3a24', '#b8794a']) },
    { id: 'nuclearwinter', name: 'Nuclear Winter', group: 'Dark & grim', stops: even(['#0a0a0b', '#1c1b1d', '#333033', '#4f4a4a', '#7a706a', '#a69b8f', '#d6cfc4']) },
    { id: 'gunmetal', name: 'Gunmetal', group: 'Dark & grim', stops: even(['#04070b', '#101820', '#1e2b36', '#33424e', '#4f5f6a', '#7c8a94', '#b8c2c8']) },
    { id: 'eclipse', name: 'Eclipse', group: 'Dark & grim', stops: S([[0, '#000000'], [0.3, '#07060a'], [0.5, '#14111c'], [0.68, '#2a2235'], [0.8, '#5a4a5e'], [0.9, '#c09a6a'], [1, '#fff1d0']]) },
    { id: 'abyss', name: 'Abyss', group: 'Dark & grim', stops: S([[0, '#000000'], [0.3, '#02040a'], [0.5, '#050d1a'], [0.65, '#0a1f2e'], [0.8, '#10394a'], [0.92, '#2a6b73'], [1, '#8fc7c0']]) },
    { id: 'voidviolet', name: 'Void', group: 'Dark & grim', stops: even(['#000000', '#06030c', '#120822', '#24103a', '#3c1a52', '#6a2c6e', '#c46a7a']) },
    { id: 'bruise', name: 'Bruise', group: 'Dark & grim', stops: even(['#050307', '#1a0f1e', '#351a33', '#4f2440', '#5c3b3a', '#6e5a3a', '#8a8550']) },
    { id: 'cathedral', name: 'Cathedral', group: 'Dark & grim', stops: S([[0, '#050404'], [0.25, '#181412'], [0.45, '#2e2722'], [0.62, '#4a3f35'], [0.78, '#6b5a49'], [0.9, '#a8803e'], [1, '#f2c46a']]) },
    { id: 'obsidian', name: 'Obsidian', group: 'Dark & grim', stops: S([[0, '#000000'], [0.45, '#0a0a0d'], [0.7, '#1d1d24'], [0.85, '#3a3a48'], [0.95, '#8a8aa0'], [1, '#e8e8f0']]) }
  ];

  const PRESET_BY_ID = {};
  PRESETS.forEach((p) => { PRESET_BY_ID[p.id] = p; });

  return {
    MAX_STOPS, PRESETS, PRESET_BY_ID, HARMONIES, INTERP_CODE,
    hexToRgb, rgbToHex, srgbToLinear, linearToSrgb, linearToOklab, oklabToLinear,
    hexToOklab, hexToOklch, oklchToHex,
    normalizeStops, sample, resample, reverse, adjust, spread, css, toGPU, blend, mixHex,
    harmony, cosine, random
  };
});
