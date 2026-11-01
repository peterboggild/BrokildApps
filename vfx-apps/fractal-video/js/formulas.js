/* Fractal formula library: parameter specs and GLSL distance estimators.
   Shared by the browser app (window.FVFormulas) and Node tools (require).

   Every formula's GLSL defines
     float fde(vec3 p, out vec4 trap, out float it)
   returning a distance estimate. trap = (min|x|, min|y|, min|z|, min|z|^2) over the orbit and
   it = 0..1 (escape iteration for escape-time formulas, iteration of the closest approach for IFS).
   Uniforms available to it: uP[8] (packed params), uIter, uRotA, uRotB, MAX_ITER. */
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.FVFormulas = api;
})(typeof self !== 'undefined' ? self : globalThis, function () {
  'use strict';

  const P = (key, label, min, max, step, def, extra) => Object.assign({ key, label, min, max, step, def, group: 'Form' }, extra || {});
  const rot = (prefix, title, defs) => ['X', 'Y', 'Z'].map((axis, i) =>
    P(prefix + axis, `${title} ${axis}`, -180, 180, 0.5, (defs || [0, 0, 0])[i], { unit: '°', group: title, calm: [-25, 25] }));
  const rad = (deg) => (Number(deg) || 0) * Math.PI / 180;

  const BULB_STEP = `
    float th = acos(clamp(z.z / max(r, 1e-9), -1.0, 1.0)) * pw;
    float ph = atan(z.y, z.x) * pw;
    dr = pow(r, pw - 1.0) * pw * dr + 1.0;
    z = pow(r, pw) * vec3(sin(th) * cos(ph), sin(th) * sin(ph), cos(th)) + p;`;

  const FORMULAS = [
    {
      id: 'mandelbulb', name: 'Mandelbulb', blurb: 'Classic power-n bulb. Julia mix morphs it into a Julia bulb.',
      size: 1.2, step: 0.9, iter: { min: 2, max: 16, def: 8 }, camera: { dist: 2.7, pitch: 18, yaw: 35 },
      params: [
        P('power', 'Power', 2, 16, 0.01, 8, { calm: [6, 10] }),
        P('theta', 'Theta phase', -180, 180, 0.5, 0, { unit: '°', calm: [-30, 30] }),
        P('phi', 'Phi phase', -180, 180, 0.5, 0, { unit: '°', calm: [-30, 30] }),
        P('julia', 'Julia mix', 0, 1, 0.01, 0, { group: 'Julia' }),
        P('cx', 'Julia c x', -1.5, 1.5, 0.01, 0.35, { group: 'Julia' }),
        P('cy', 'Julia c y', -1.5, 1.5, 0.01, 0.3, { group: 'Julia' }),
        P('cz', 'Julia c z', -1.5, 1.5, 0.01, -0.4, { group: 'Julia' })
      ],
      pack: (v) => ({ p: [v.power, rad(v.theta), rad(v.phi), v.julia, v.cx, v.cy, v.cz, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 c = mix(p, vec3(uP[4], uP[5], uP[6]), uP[3]);
  vec3 z = p;
  float pw = uP[0];
  float dr = 1.0;
  float r = length(z);
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter || r > 2.0) break;
    float th = acos(clamp(z.z / max(r, 1e-9), -1.0, 1.0)) * pw + uP[1];
    float ph = atan(z.y, z.x) * pw + uP[2];
    dr = pow(r, pw - 1.0) * pw * dr + 1.0 - uP[3];
    z = pow(r, pw) * vec3(sin(th) * cos(ph), sin(th) * sin(ph), cos(th)) + c;
    trap = min(trap, vec4(abs(z), dot(z, z)));
    r = length(z);
    it += 1.0;
  }
  it /= float(uIter);
  return 0.5 * log(max(r, 1e-9)) * r / max(dr, 1e-9);
}`
    },
    {
      id: 'mandelbox', name: 'Mandelbox', blurb: 'Box fold + sphere fold. Negative scale gives compact boxes, positive scale gives open cities.',
      size: 3.0, step: 1.0, iter: { min: 2, max: 24, def: 12 }, camera: { dist: 6.5, pitch: 22, yaw: 30 },
      params: [
        P('scale', 'Scale', -3, 3, 0.005, -1.8, { calm: [-2.2, -1.5] }),
        P('minR', 'Min radius', 0.05, 1, 0.005, 0.5, { calm: [0.3, 0.7] }),
        P('fixedR', 'Fixed radius', 0.3, 2, 0.005, 1.0, { calm: [0.8, 1.2] }),
        P('fold', 'Fold limit', 0.3, 2, 0.005, 1.0, { calm: [0.8, 1.2] })
      ],
      pack: (v) => ({ p: [v.scale, v.minR, Math.max(v.fixedR, v.minR + 0.01), v.fold, 0, 0, 0, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float dr = 1.0;
  float s = uP[0];
  float mr2 = uP[1] * uP[1];
  float fr2 = uP[2] * uP[2];
  float fl = uP[3];
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = clamp(z, -fl, fl) * 2.0 - z;
    float r2 = dot(z, z);
    if (r2 < mr2) { float k = fr2 / mr2; z *= k; dr *= k; }
    else if (r2 < fr2) { float k = fr2 / r2; z *= k; dr *= k; }
    z = z * s + p;
    dr = dr * abs(s) + 1.0;
    trap = min(trap, vec4(abs(z), dot(z, z)));
    it += 1.0;
    if (dot(z, z) > 1e4) break;
  }
  it /= float(uIter);
  return length(z) / abs(dr);
}`
    },
    {
      id: 'amazingBox', name: 'Amazing Box', blurb: 'Mandelbox with a rotation every iteration: organic, surface-like folds.',
      size: 2.4, step: 0.8, iter: { min: 2, max: 24, def: 12 }, camera: { dist: 8.0, pitch: 25, yaw: 20 },
      params: [
        P('scale', 'Scale', -3, 3, 0.005, -1.7, { calm: [-2.0, -1.45] }),
        P('minR', 'Min radius', 0.05, 1, 0.005, 0.35, { calm: [0.2, 0.6] }),
        P('fold', 'Fold limit', 0.3, 2, 0.005, 1.0, { calm: [0.8, 1.2] }),
        ...rot('rot', 'Rotation', [12, 0, 20])
      ],
      pack: (v) => ({ p: [v.scale, v.minR, v.fold, 0, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float dr = 1.0;
  float s = uP[0];
  float mr2 = uP[1] * uP[1];
  float fl = uP[2];
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = clamp(z, -fl, fl) * 2.0 - z;
    float r2 = dot(z, z);
    if (r2 < mr2) { float k = 1.0 / mr2; z *= k; dr *= k; }
    else if (r2 < 1.0) { float k = 1.0 / r2; z *= k; dr *= k; }
    z = uRotA * z;
    z = z * s + p;
    dr = dr * abs(s) + 1.0;
    trap = min(trap, vec4(abs(z), dot(z, z)));
    it += 1.0;
    if (dot(z, z) > 1e4) break;
  }
  it /= float(uIter);
  return length(z) / abs(dr);
}`
    },
    {
      id: 'kifsMenger', name: 'Menger KIFS', blurb: 'Menger sponge with offset and rotation: architecture, grids, temples.',
      size: 1.4, step: 1.0, iter: { min: 2, max: 12, def: 6 }, camera: { dist: 3.4, pitch: 24, yaw: 38 },
      params: [
        P('scale', 'Scale', 2, 4, 0.005, 3, { calm: [2.7, 3.3] }),
        P('offX', 'Offset X', 0, 2, 0.005, 1, { group: 'Offset', calm: [0.8, 1.2] }),
        P('offY', 'Offset Y', 0, 2, 0.005, 1, { group: 'Offset', calm: [0.8, 1.2] }),
        P('offZ', 'Offset Z', 0, 2, 0.005, 1, { group: 'Offset', calm: [0.8, 1.2] }),
        ...rot('rot', 'Rotation')
      ],
      pack: (v) => ({ p: [v.scale, v.offX, v.offY, v.offZ, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float dr = 1.0;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.x < z.z) z.xz = z.zx;
    if (z.y < z.z) z.yz = z.zy;
    z = uRotA * z;
    z = s * z - off * (s - 1.0);
    if (z.z < -0.5 * off.z * (s - 1.0)) z.z += off.z * (s - 1.0);
    dr *= s;
    float m = dot(z, z);
    if (m < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), m));
  }
  it /= float(uIter);
  vec3 q = abs(z) - vec3(1.0);
  return (length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0)) / dr;
}`
    },
    {
      id: 'sierpinski', name: 'Sierpinski', blurb: 'Tetrahedral IFS. Rotation turns it into crystal spires.',
      size: 1.6, step: 1.0, iter: { min: 2, max: 20, def: 12 }, camera: { dist: 3.6, pitch: 20, yaw: 25 },
      params: [
        P('scale', 'Scale', 1.5, 3, 0.005, 2, { calm: [1.85, 2.2] }),
        P('offset', 'Offset', 0.3, 1.6, 0.005, 1, { calm: [0.85, 1.15] }),
        ...rot('rot', 'Rotation')
      ],
      pack: (v) => ({ p: [v.scale, v.offset, 0, 0, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1]);
  float dr = 1.0;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    if (z.x + z.y < 0.0) z.xy = -z.yx;
    if (z.x + z.z < 0.0) z.xz = -z.zx;
    if (z.y + z.z < 0.0) z.zy = -z.yz;
    z = uRotA * z;
    z = z * s - off * (s - 1.0);
    dr *= s;
    float m = dot(z, z);
    if (m < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), m));
  }
  it /= float(uIter);
  return (length(z) - 1.0) / dr;
}`
    },
    {
      id: 'octahedra', name: 'Octahedral KIFS', blurb: 'Octahedral folds with one rotation: lattices, star fortresses.',
      size: 1.8, step: 1.0, iter: { min: 2, max: 20, def: 12 }, camera: { dist: 3.4, pitch: 22, yaw: 30 },
      params: [
        P('scale', 'Scale', 1.5, 3.5, 0.005, 2, { calm: [1.8, 2.3] }),
        P('offX', 'Offset X', 0, 2, 0.005, 1, { group: 'Offset' }),
        P('offY', 'Offset Y', 0, 2, 0.005, 0, { group: 'Offset' }),
        P('offZ', 'Offset Z', 0, 2, 0.005, 0, { group: 'Offset' }),
        ...rot('rot', 'Rotation', [0, 0, 0])
      ],
      pack: (v) => ({ p: [v.scale, v.offX, v.offY, v.offZ, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float dr = 1.0;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.x < z.z) z.xz = z.zx;
    if (z.y < z.z) z.yz = z.zy;
    z = uRotA * z;
    z = z * s - off * (s - 1.0);
    dr *= s;
    float m = dot(z, z);
    if (m < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), m));
  }
  it /= float(uIter);
  return (length(z) - 1.0) / dr;
}`
    },
    {
      id: 'kaleidoscopic', name: 'Kaleidoscopic IFS', blurb: 'Two rotations around the fold: the most varied family, from flowers to machinery.',
      size: 1.8, step: 0.9, iter: { min: 2, max: 20, def: 12 }, camera: { dist: 3.8, pitch: 20, yaw: 30 },
      params: [
        P('scale', 'Scale', 1.4, 3.5, 0.005, 2.4, { calm: [2.1, 2.7] }),
        P('offX', 'Offset X', 0, 2, 0.005, 1, { group: 'Offset' }),
        P('offY', 'Offset Y', 0, 2, 0.005, 0.5, { group: 'Offset' }),
        P('offZ', 'Offset Z', 0, 2, 0.005, 0.25, { group: 'Offset' }),
        ...rot('pre', 'Pre-rotation', [10, 20, 0]),
        ...rot('post', 'Post-rotation', [0, 0, 15])
      ],
      pack: (v) => ({ p: [v.scale, v.offX, v.offY, v.offZ, 0, 0, 0, 0], rotA: [v.preX, v.preY, v.preZ], rotB: [v.postX, v.postY, v.postZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float dr = 1.0;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = uRotA * z;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.x < z.z) z.xz = z.zx;
    if (z.y < z.z) z.yz = z.zy;
    z = uRotB * z;
    z = z * s - off * (s - 1.0);
    dr *= s;
    float m = dot(z, z);
    if (m < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), m));
  }
  it /= float(uIter);
  return (length(z) - 1.0) / dr;
}`
    },
    {
      id: 'hybrid', name: 'Folded bulb hybrid', blurb: 'Mirror folds and a rotation between bulb iterations: crystalline, symmetric bulbs (Mandelbulb3D-style hybrid).',
      size: 1.3, step: 0.85, iter: { min: 2, max: 20, def: 9 }, camera: { dist: 2.9, pitch: 20, yaw: 30 },
      params: [
        P('power', 'Bulb power', 2, 12, 0.01, 8, { calm: [6, 9] }),
        P('fold', 'Fold amount', 0, 1, 0.005, 1, { calm: [0.6, 1], hint: '0 = plain bulb, 1 = full mirror fold.' }),
        P('every', 'Fold every', 1, 4, 1, 1, { hint: 'Fold before every n-th bulb iteration.' }),
        ...rot('rot', 'Rotation', [0, 30, 15])
      ],
      pack: (v) => ({ p: [v.power, v.fold, Math.round(v.every), 0, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float dr = 1.0;
  float pw = uP[0];
  float fm = uP[1];
  int ev = max(1, int(uP[2] + 0.5));
  float r = length(z);
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter || r > 2.0) break;
    if (i % ev == 0) {
      z = uRotA * mix(z, abs(z), fm);
      r = length(z);
    }${BULB_STEP}
    trap = min(trap, vec4(abs(z), dot(z, z)));
    r = length(z);
    it += 1.0;
  }
  it /= float(uIter);
  return 0.5 * log(max(r, 1e-9)) * r / max(abs(dr), 1e-9);
}`
    },
    {
      id: 'julia', name: 'Quaternion Julia', blurb: '3D slice of a 4D quaternion Julia set: smooth, liquid forms.',
      size: 1.3, step: 0.9, iter: { min: 2, max: 20, def: 11 }, camera: { dist: 3.0, pitch: 15, yaw: 30 },
      params: [
        P('cx', 'c x', -1.5, 1.5, 0.002, -0.291, { calm: [-0.5, 0] }),
        P('cy', 'c y', -1.5, 1.5, 0.002, -0.399, { calm: [-0.6, -0.2] }),
        P('cz', 'c z', -1.5, 1.5, 0.002, 0.339, { calm: [0.2, 0.5] }),
        P('cw', 'c w', -1.5, 1.5, 0.002, 0.437, { calm: [0.3, 0.6] }),
        P('slice', '4D slice', -1, 1, 0.002, 0, { calm: [-0.2, 0.2] })
      ],
      pack: (v) => ({ p: [v.cx, v.cy, v.cz, v.cw, v.slice, 0, 0, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec4 z = vec4(p, uP[4]);
  vec4 c = vec4(uP[0], uP[1], uP[2], uP[3]);
  float md2 = 1.0;
  float mz2 = dot(z, z);
  trap = vec4(abs(z.xyz), mz2);
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    md2 *= 4.0 * mz2;
    z = vec4(z.x * z.x - dot(z.yzw, z.yzw), 2.0 * z.x * z.yzw) + c;
    mz2 = dot(z, z);
    trap = min(trap, vec4(abs(z.xyz), mz2));
    it += 1.0;
    if (mz2 > 16.0) break;
  }
  it /= float(uIter);
  return 0.25 * sqrt(mz2 / md2) * log(max(mz2, 1e-9));
}`
    },
    {
      id: 'apollonian', name: 'Apollonian', unbounded: true, blurb: 'Infinite sphere-inversion lattice. Fly inside it; use the bound radius to cut it out.',
      size: 1.0, step: 1.0, iter: { min: 2, max: 14, def: 8 }, camera: { dist: 1.6, pitch: 10, yaw: 20 },
      params: [
        P('k', 'Inversion size', 0.8, 1.6, 0.002, 1.15, { calm: [1.05, 1.3] })
      ],
      pack: (v) => ({ p: [v.k, 0, 0, 0, 0, 0, 0, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float sc = 1.0;
  float k = uP[0];
  trap = vec4(1e9);
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = -1.0 + 2.0 * fract(0.5 * z + 0.5);
    float r2 = dot(z, z);
    if (r2 < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), r2));
    float f = k / max(r2, 1e-6);
    z *= f;
    sc *= f;
  }
  it /= float(uIter);
  return 0.25 * abs(z.y) / sc;
}`
    },
    {
      id: 'pseudoKleinian', name: 'Pseudo-Kleinian', unbounded: true,
      blurb: 'Knighty\'s pseudo-Kleinian limit set: endless alien halls, arches and lattices. Made to fly through.',
      size: 1.0, step: 0.8, iter: { min: 2, max: 20, def: 9 }, camera: { dist: 1.0, pitch: -5, yaw: 45 },
      params: [
        P('boxXY', 'Box width', 0.4, 1.4, 0.002, 0.908, { calm: [0.85, 0.96] }),
        P('boxZ', 'Box height', 0.4, 1.6, 0.002, 0.924, { calm: [0.86, 1.0] }),
        P('size', 'Inversion size', 0.6, 1.6, 0.002, 1.0, { calm: [0.95, 1.08] }),
        P('cx', 'Offset X', -0.5, 0.5, 0.002, 0, { group: 'Offset', calm: [-0.05, 0.05] }),
        P('cy', 'Offset Y', -0.5, 0.5, 0.002, 0, { group: 'Offset', calm: [-0.05, 0.05] }),
        P('cz', 'Offset Z', -0.5, 0.5, 0.002, 0, { group: 'Offset', calm: [-0.05, 0.05] }),
        P('thick', 'Thickness', 0.5, 1.2, 0.002, 0.928, { calm: [0.9, 0.95], hint: 'Radius of the struts the halls are built from.' })
      ],
      pack: (v) => ({ p: [v.boxXY, v.boxZ, v.size, v.cx, v.cy, v.cz, v.thick, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 cs = vec3(uP[0], uP[0], uP[1]);
  float sz = uP[2];
  vec3 c = vec3(uP[3], uP[4], uP[5]);
  vec3 z = p;
  float dr = 1.0;
  trap = vec4(1e9);
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = 2.0 * clamp(z, -cs, cs) - z;
    float r2 = dot(z, z);
    if (r2 < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), r2));
    float k = max(sz / max(r2, 1e-9), 1.0);
    z *= k;
    dr *= k;
    z += c;
  }
  it /= float(uIter);
  float rxy = length(z.xy);
  return max(rxy - uP[6], abs(rxy * z.z) / max(length(z), 1e-9)) / dr;
}`
    },
    {
      id: 'kaliBox', name: 'KaliBox',
      blurb: 'Kali\'s abs-fold box: ornate carved columns, slabs and temple facades. Small changes go a long way; keyframe it slowly.',
      size: 2.2, step: 0.7, iter: { min: 2, max: 24, def: 13 }, camera: { dist: 2.4, pitch: 12, yaw: 40 },
      params: [
        P('scale', 'Scale', 1.2, 3, 0.002, 2.043, { calm: [1.98, 2.1] }),
        P('minR2', 'Min radius²', 0.05, 1, 0.002, 0.349, { calm: [0.31, 0.39] }),
        P('tx', 'Shift X', -2, 2, 0.002, 0.036, { group: 'Shift', calm: [-0.1, 0.15] }),
        P('ty', 'Shift Y', -2.5, 1, 0.002, -1.861, { group: 'Shift', calm: [-1.95, -1.75] }),
        P('tz', 'Shift Z', -2, 2, 0.002, 0.036, { group: 'Shift', calm: [-0.1, 0.15] }),
        P('jx', 'Julia X', -1.5, 1.5, 0.002, -0.669, { group: 'Julia', calm: [-0.75, -0.6] }),
        P('jy', 'Julia Y', -1.5, 1.5, 0.002, -1.303, { group: 'Julia', calm: [-1.4, -1.2] }),
        P('jz', 'Julia Z', -1.5, 1.5, 0.002, -0.458, { group: 'Julia', calm: [-0.55, -0.37] }),
        ...rot('rot', 'Rotation', [0, 0, 0])
      ],
      pack: (v) => ({ p: [v.scale, v.minR2, v.tx, v.ty, v.tz, v.jx, v.jy, v.jz], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  float s = uP[0];
  float mr2 = uP[1];
  vec3 tr = vec3(uP[2], uP[3], uP[4]);
  vec4 c = vec4(uP[5], uP[6], uP[7], 0.0);
  vec4 z = vec4(p, 1.0);
  trap = vec4(1e9);
  it = 0.0;
  int n = 0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z.xyz = abs(z.xyz) + tr;
    float r2 = dot(z.xyz, z.xyz);
    if (r2 < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z.xyz), r2));
    z *= clamp(max(mr2 / max(r2, 1e-9), mr2), 0.0, 1.0);
    z = z * (s / mr2) + c;
    z.xyz = uRotA * z.xyz;
    n++;
  }
  it /= float(uIter);
  return (length(z.xyz) - abs(s - 1.0)) / abs(z.w) - pow(abs(s), 1.0 - float(n));
}`
    },
    {
      id: 'amazingSurf', name: 'Amazing Surf', unbounded: true,
      blurb: 'Mandelbox folded on two axes only: endless rolling sheets and ribbons. Alien landscapes and seas to fly low over.',
      size: 1.6, step: 0.6, iter: { min: 2, max: 20, def: 10 }, camera: { dist: 3.2, pitch: 24, yaw: 30, ty: 0.2 },
      params: [
        P('scale', 'Scale', -3, 3, 0.002, -1.75, { calm: [-1.95, -1.55] }),
        P('minR', 'Min radius', 0.05, 1, 0.002, 0.45, { calm: [0.35, 0.55] }),
        P('fold', 'Fold limit', 0.3, 2, 0.002, 1.0, { calm: [0.85, 1.15] }),
        P('lift', 'Sheet lift', -1, 1, 0.002, 0, { calm: [-0.15, 0.15], hint: 'Adds to the unfolded (vertical) axis every iteration: bends the sheets.' }),
        ...rot('rot', 'Rotation', [6, 0, 9])
      ],
      pack: (v) => ({ p: [v.scale, v.minR, v.fold, v.lift, 0, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float dr = 1.0;
  float s = uP[0];
  float mr2 = uP[1] * uP[1];
  float fl = uP[2];
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z.xz = clamp(z.xz, -fl, fl) * 2.0 - z.xz;
    float r2 = dot(z, z);
    if (r2 < mr2) { float k = 1.0 / mr2; z *= k; dr *= k; }
    else if (r2 < 1.0) { float k = 1.0 / r2; z *= k; dr *= k; }
    z = uRotA * z;
    z = z * s + p + vec3(0.0, uP[3], 0.0);
    dr = dr * abs(s) + 1.0;
    trap = min(trap, vec4(abs(z), dot(z, z)));
    it += 1.0;
    if (dot(z, z) > 1e4) break;
  }
  it /= float(uIter);
  return length(z) / abs(dr);
}`
    },
    {
      id: 'icosahedral', name: 'Icosahedral KIFS',
      blurb: 'Icosahedral (five-fold) symmetry folds: gems, star cores, rose windows. Rotations are smooth routes for drops.',
      size: 1.6, step: 0.9, iter: { min: 2, max: 20, def: 10 }, camera: { dist: 3.0, pitch: 18, yaw: 30 },
      params: [
        P('scale', 'Scale', 1.5, 3.5, 0.002, 2.6, { calm: [2.4, 2.8] }),
        P('wA', 'Vertex', 0, 1, 0.002, 0, { group: 'Offset point', hint: 'Weight of the icosahedron vertex in the offset point.' }),
        P('wB', 'Edge', 0, 1, 0.002, 1, { group: 'Offset point', hint: 'Weight of the edge midpoint.' }),
        P('wC', 'Face', 0, 1, 0.002, 0, { group: 'Offset point', hint: 'Weight of the face centre.' }),
        P('reach', 'Reach', 0.2, 2, 0.002, 1, { group: 'Offset point', calm: [0.85, 1.15], hint: 'Distance of the offset point from the centre.' }),
        ...rot('pre', 'Pre-rotation', [0, 8, 0]),
        ...rot('post', 'Post-rotation', [0, 0, 12])
      ],
      pack: (v) => {
        // corners of the fundamental cell of the type-5 fold: vertex, edge midpoint, face centre
        const c = 0.80901699437, sc = Math.sqrt(0.75 - c * c);
        const nrm = (x) => { const l = Math.hypot(...x) || 1; return x.map((t) => t / l); };
        const A = [0, 0, 1], B = nrm([sc, 0, 0.5]), C = nrm([0, sc, c]);
        const o = nrm([0, 1, 2].map((i) => v.wA * A[i] + v.wB * B[i] + v.wC * C[i] + (v.wA + v.wB + v.wC > 0 ? 0 : A[i]))).map((t) => t * v.reach);
        return { p: [v.scale, o[0], o[1], o[2], 0, 0, 0, 0], rotA: [v.preX, v.preY, v.preZ], rotB: [v.postX, v.postY, v.postZ] };
      },
      glsl: `
// Knighty's polyhedral fold for icosahedral symmetry (type 5): maps space into one fundamental cell.
const float ICO_COS = 0.80901699437; // cos(pi/5)
vec3 icoFold(vec3 z) {
  vec3 nc = vec3(-0.5, -ICO_COS, sqrt(0.75 - ICO_COS * ICO_COS));
  for (int k = 0; k < 5; k++) {
    z.xy = abs(z.xy);
    z -= 2.0 * min(0.0, dot(z, nc)) * nc;
  }
  return z;
}
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float dr = 1.0;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = uRotA * z;
    z = icoFold(z);
    z = uRotB * z;
    z = z * s - off * (s - 1.0);
    dr *= s;
    float m = dot(z, z);
    if (m < trap.w) it = float(i);
    trap = min(trap, vec4(abs(z), m));
  }
  it /= float(uIter);
  return (length(z) - 1.0) / dr;
}`
    },
    {
      id: 'hyperbolic', name: 'Hyperbolic honeycomb',
      blurb: 'A {4,3,n} honeycomb of cubes in the Poincaré ball: Escher-like tiling shrinking to a spherical horizon. Travel slides it.',
      size: 1.0, step: 0.8, iter: { min: 4, max: 32, def: 24 }, camera: { dist: 0.12, pitch: 10, yaw: 30, fov: 75 },
      params: [
        P('order', 'Cubes per edge', 4.2, 8, 0.01, 5, { calm: [4.8, 5.6], hint: '5 = the compact {4,3,5} honeycomb; towards 4 it flattens into a Euclidean cube grid.' }),
        P('edge', 'Edge thickness', 0.005, 0.2, 0.001, 0.045, { calm: [0.03, 0.07] }),
        P('node', 'Vertex size', 0, 0.3, 0.001, 0.08, { calm: [0.05, 0.12] }),
        P('travel', 'Travel', -0.95, 0.95, 0.001, 0, { calm: [-0.6, 0.6], hint: 'Hyperbolic translation: the tiling streams past without the camera moving. With 5 cubes per edge, 0.486 is exactly one cube, so a ramp from 0 to 0.486 (a bar ramp route, or two keys) loops seamlessly.' }),
        P('drift', 'Drift', -0.95, 0.95, 0.001, 0, { calm: [-0.4, 0.4], hint: 'Hyperbolic translation sideways.' }),
        P('shell', 'Horizon', 0.8, 1.0, 0.001, 0.985, { hint: 'Cut the ball at this radius (1 = the full horizon).' })
      ],
      pack: (v) => {
        const a = (2 * Math.PI) / v.order;
        const r2 = 1 / Math.max(1e-3, Math.cos(a)); // face spheres: interior dihedral angle 2*pi/order
        return { p: [Math.sqrt(1 + r2), r2, v.edge, v.node, v.travel, v.drift, v.shell, 0] };
      },
      glsl: `
// Mobius translation of the unit ball moving the origin to a; returns the image of x and the local stretch.
vec3 ballShift(vec3 x, vec3 a, out float k) {
  float aa = dot(a, a), xx = dot(x, x), ax = dot(a, x);
  float den = max(1.0 + 2.0 * ax + aa * xx, 1e-6);
  k = (1.0 - aa) / den;
  return ((1.0 + 2.0 * ax + xx) * a + (1.0 - aa) * x) / den;
}
float fde(vec3 p, out vec4 trap, out float it) {
  float D = uP[0], R2 = uP[1];
  float sc;
  vec3 z = ballShift(p, vec3(uP[5], 0.0, uP[4]), sc);
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  vec3 cx = vec3(D, 0.0, 0.0);
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.x < z.z) z.xz = z.zx;
    if (z.y < z.z) z.yz = z.zy;
    vec3 dv = z - cx;
    float l2 = dot(dv, dv);
    if (l2 >= R2) break;
    float k = R2 / l2;
    z = cx + dv * k;
    sc *= k;
    it += 1.0;
    trap = min(trap, vec4(abs(z), dot(z, z)));
  }
  it /= float(uIter);
  float R = sqrt(R2);
  float fx = abs(length(z - vec3(D, 0.0, 0.0)) - R);
  float fy = abs(length(z - vec3(0.0, D, 0.0)) - R);
  float fz = abs(length(z - vec3(0.0, 0.0, D)) - R);
  float d = min(length(vec2(fx, fy)) - uP[2], length(vec3(fx, fy, fz)) - uP[3]);
  return max(d / sc, length(p) - uP[6]);
}`
    },
    // ---------- architecture ----------
    {
      id: 'vaulted', name: 'Vaulted box', cat: 'arch',
      blurb: 'Mandelbox whose sphere fold is pulled towards a cylinder: barrel vaults, arcades and naves instead of round chambers.',
      size: 3.0, step: 0.8, iter: { min: 2, max: 24, def: 12 }, camera: { dist: 7.5, pitch: 18, yaw: 32 },
      params: [
        P('scale', 'Scale', -3, 3, 0.005, -1.8, { calm: [-2.1, -1.55] }),
        P('minR', 'Min radius', 0.05, 1, 0.005, 0.45, { calm: [0.3, 0.6] }),
        P('fold', 'Fold limit', 0.3, 2, 0.005, 1.0, { calm: [0.85, 1.15] }),
        P('foldY', 'Height fold', 0.3, 2, 0.005, 1.0, { calm: [0.85, 1.15], hint: 'Fold limit on the vertical axis: lower = flatter storeys.' }),
        P('vault', 'Vault', 0, 1, 0.005, 0.8, { calm: [0.6, 1], hint: '0 = round chambers (plain Mandelbox), 1 = full barrel vaults running along Z.' }),
        ...rot('rot', 'Rotation', [0, 0, 0])
      ],
      pack: (v) => ({ p: [v.scale, v.minR, v.fold, v.foldY, v.vault, 0, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float dr = 1.0;
  float s = uP[0];
  float mr2 = uP[1] * uP[1];
  float fl = uP[2], fy = uP[3], vb = uP[4];
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z.xz = clamp(z.xz, -fl, fl) * 2.0 - z.xz;
    z.y = clamp(z.y, -fy, fy) * 2.0 - z.y;
    float r2 = mix(dot(z, z), dot(z.xy, z.xy), vb);
    if (r2 < mr2) { float k = 1.0 / mr2; z *= k; dr *= k; }
    else if (r2 < 1.0) { float k = 1.0 / r2; z *= k; dr *= k; }
    z = uRotA * z;
    z = z * s + p;
    dr = dr * abs(s) + 1.0;
    trap = min(trap, vec4(abs(z), dot(z, z)));
    it += 1.0;
    if (dot(z, z) > 1e4) break;
  }
  it /= float(uIter);
  return length(z) / abs(dr);
}`
    },
    {
      id: 'ziggurat', name: 'Ziggurat', cat: 'arch',
      blurb: 'Four-way folded plan with stepped pyramids as the building block: terraces, stairways and temple cities at every scale.',
      size: 1.5, step: 0.8, iter: { min: 1, max: 8, def: 4 }, camera: { dist: 3.6, pitch: 28, yaw: 35, ty: 0.35 },
      params: [
        P('scale', 'Scale', 2, 4, 0.005, 3, { calm: [2.7, 3.3] }),
        P('spread', 'Spread', 0, 2, 0.005, 0.95, { calm: [0.85, 1.15], hint: 'How far the copies sit from the centre in plan.' }),
        P('rise', 'Rise', -1, 1, 0.005, 0.25, { calm: [0.1, 0.4], hint: 'Lifts each smaller generation: they climb onto the terraces of the one below.' }),
        P('tiers', 'Tiers', 1, 6, 1, 4, { hint: 'Steps per pyramid.' }),
        P('steep', 'Steepness', 0.2, 2, 0.005, 0.7, { calm: [0.5, 0.9] }),
        P('rotY', 'Twist', -45, 45, 0.5, 0, { unit: '°', calm: [-10, 10], hint: 'Turn per generation about the vertical axis.' })
      ],
      pack: (v) => ({ p: [v.scale, v.spread, v.rise, Math.round(v.tiers), v.steep, 0, 0, 0], rotA: [0, v.rotY, 0] }),
      glsl: `
float zigBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  float sp = uP[1], rise = uP[2];
  int tiers = int(uP[3] + 0.5);
  float steep = uP[4];
  float dr = 1.0;
  float th = steep / float(tiers);
  float d = 1e9;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z.xz = abs(z.xz);
    if (z.x < z.z) z.xz = z.zx;
    // this generation's stepped pyramid, standing on y = 0
    float pd = 1e9;
    for (int k = 0; k < 6; k++) {
      if (k >= tiers) break;
      float w = 1.0 - float(k) / float(tiers) * 0.85;
      pd = min(pd, zigBox(z - vec3(0.0, th * (float(k) + 0.5), 0.0), vec3(w, 0.5 * th, w)));
    }
    if (pd / dr < d) { d = pd / dr; it = float(i) / float(uIter); }
    z = uRotA * z;
    z.xz = z.xz * s - vec2(sp) * (s - 1.0);
    z.y = z.y * s - rise * (s - 1.0);
    dr *= s;
    trap = min(trap, vec4(abs(z), dot(z, z)));
  }
  return d;
}`
    },
    {
      id: 'skyline', name: 'Skyline', cat: 'arch', unbounded: true,
      blurb: 'An endless city of set-back towers on a ground plane. Not a fractal: the towers are procedural, built to fly between.',
      size: 2.0, step: 0.8, iter: { min: 1, max: 5, def: 3 }, camera: { dist: 7.0, pitch: 24, yaw: 30, ty: 1.2 },
      params: [
        P('cell', 'Block size', 0.3, 3, 0.005, 1, { calm: [0.8, 1.2] }),
        P('height', 'Height', 0.1, 6, 0.005, 1.8, { calm: [1.2, 2.4] }),
        P('vary', 'Height variety', 0, 1, 0.005, 0.75, { calm: [0.5, 0.9] }),
        P('density', 'Density', 0, 1, 0.005, 0.8, { calm: [0.6, 1], hint: 'Share of blocks that carry a tower.' }),
        P('width', 'Tower width', 0.15, 0.42, 0.002, 0.32, { calm: [0.26, 0.38] }),
        P('setback', 'Setback', 0, 0.8, 0.005, 0.25, { calm: [0.15, 0.35], hint: 'How much each upper tier steps in. Iterations = tiers.' }),
        P('seed', 'Seed', 0, 100, 1, 7)
      ],
      pack: (v) => ({ p: [v.cell, v.height, v.vary, v.density, v.width, v.setback, v.seed, 0] }),
      glsl: `
float skyHash(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float skyBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
// The tower standing in block id, seen from p. Returns distance; lv = which tier, hv = the block's hash.
float skyTower(vec3 p, vec2 id, out float lv, out float hv) {
  float cell = uP[0];
  vec2 q = p.xz - (id + 0.5) * cell;
  vec2 hid = id + uP[6] * 17.0;
  float h1 = skyHash(hid), h2 = skyHash(hid + 31.7), h3 = skyHash(hid + 71.3);
  lv = 0.0; hv = h1;
  if (h2 >= uP[3]) return 1e9;
  float H = uP[1] * (1.0 - uP[2] + uP[2] * h1 * h1 * 1.6);
  float w = cell * uP[4] * (0.85 + 0.3 * h3);
  float wz = min(w * (0.75 + 0.5 * h3), 0.45 * cell);
  w = min(w, 0.45 * cell);
  float d = 1e9, y0 = 0.0;
  int tiers = uIter;
  for (int k = 0; k < 5; k++) {
    if (k >= tiers) break;
    float th = (k == tiers - 1) ? max(H - y0, 0.01) : H * 0.55 * pow(0.6, float(k));
    float sh = pow(1.0 - uP[5], float(k));
    float b = skyBox(vec3(q.x, p.y - y0 - 0.5 * th, q.y), vec3(w * sh, 0.5 * th, wz * sh));
    if (b < d) { d = b; lv = float(k + 1) / float(tiers); }
    y0 += th;
    if (y0 >= H) break;
  }
  return d;
}
float fde(vec3 p, out vec4 trap, out float it) {
  float cell = uP[0];
  vec2 id = floor(p.xz / cell);
  vec2 q = p.xz - (id + 0.5) * cell;
  float d = p.y;
  it = 0.0;
  float hc = 0.0;
  // this block and its eight neighbours: towers never reach past their own block, so anything
  // further away is at least the distance to the edge of this 3 x 3 patch
  for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
    float lv, hv;
    float t = skyTower(p, id + vec2(i, j), lv, hv);
    if (t < d) { d = t; it = lv; hc = hv; }
  }
  vec2 edge = 1.5 * cell - abs(q);
  float bnd = min(edge.x, edge.y) + 0.05 * cell;
  trap = vec4(abs(q.x), abs(p.y), abs(q.y), hc * 0.8 + 0.2 * it);
  return min(d, bnd);
}`
    },
    // ---------- machines ----------
    {
      id: 'turbine', name: 'Turbine', cat: 'machine',
      blurb: 'Stacked rotor stages on a shaft, each a ring of pitched blades, shrinking along the axis like a jet engine core. Procedural, not a fractal.',
      size: 1.6, step: 0.7, iter: { min: 1, max: 12, def: 6 }, camera: { dist: 3.4, pitch: 30, yaw: 35, ty: 0.6 },
      params: [
        P('blades', 'Blades', 2, 40, 1, 18, { hint: 'Blades per stage. Iterations = stages.' }),
        P('radius', 'Radius', 0.3, 2, 0.005, 1, { calm: [0.8, 1.2] }),
        P('taper', 'Taper', 0.5, 1, 0.002, 0.86, { calm: [0.8, 0.92], hint: 'Each stage is this much smaller than the last.' }),
        P('gap', 'Stage gap', 0.05, 0.6, 0.002, 0.22, { calm: [0.15, 0.3] }),
        P('pitch', 'Blade pitch', 0, 80, 0.5, 35, { unit: '°', calm: [20, 50] }),
        P('chord', 'Blade width', 0.02, 0.4, 0.002, 0.11, { calm: [0.08, 0.15] }),
        P('hub', 'Shaft', 0.02, 0.6, 0.002, 0.2, { calm: [0.15, 0.3] }),
        P('spin', 'Spin', -180, 180, 0.5, 0, { unit: '°', hint: 'Turns the rotors (alternate stages counter-rotate). Animate it.' })
      ],
      pack: (v) => ({ p: [Math.round(v.blades), v.radius, v.taper, v.gap, rad(v.pitch), v.chord, v.hub, rad(v.spin)] }),
      glsl: `
float turBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float fde(vec3 p, out vec4 trap, out float it) {
  float n = max(uP[0], 2.0), sec = 6.2831853 / n;
  float R = uP[1], tp = uP[2], gap = uP[3], pitch = uP[4], chord = uP[5];
  int st = uIter;
  float len = gap * float(st);
  // shaft along y, from the first stage to the last
  float d = max(length(p.xz) - uP[6], abs(p.y - 0.5 * len) - 0.5 * len - 0.15);
  trap = vec4(1e9);
  it = 0.0;
  float r = R;
  for (int k = 0; k < MAX_ITER; k++) {
    if (k >= st) break;
    float y = (float(k) + 0.5) * gap;
    vec3 q = p - vec3(0.0, y, 0.0);
    float a = atan(q.z, q.x) + uP[7] * (mod(float(k), 2.0) * 2.0 - 1.0);
    float ai = floor(a / sec + 0.5);
    a -= ai * sec;
    float rr = length(q.xz);
    vec3 b = vec3(rr * cos(a), q.y, rr * sin(a));
    // pitch the blade about its own radial axis
    float cp = cos(pitch), sp = sin(pitch);
    b.yz = mat2(cp, -sp, sp, cp) * b.yz;
    float bl = turBox(b - vec3(0.5 * (r + uP[6]), 0.0, 0.0), vec3(0.5 * (r - uP[6]), 0.012 + 0.04 * r, chord * r));
    // a thin ring holding the blade tips
    float ring = length(vec2(rr - r - 0.03, q.y)) - 0.025 * r;
    float sd = min(bl, ring) * 0.9;
    if (sd < d) { d = sd; it = float(k) / float(max(st - 1, 1)); }
    trap = min(trap, vec4(abs(b), rr * rr / (r * r)));
    r *= tp;
  }
  return d;
}`
    },
    {
      id: 'greeble', name: 'Greeble hull', cat: 'machine',
      blurb: 'Folded panels: every generation adds a plate, so plates carry plates. Hulls, vents and machine casing; seams cut grooves.',
      size: 1.8, step: 0.8, iter: { min: 1, max: 12, def: 6 }, camera: { dist: 3.4, pitch: 24, yaw: 35 },
      params: [
        P('scale', 'Scale', 1.5, 3.5, 0.005, 2.3, { calm: [2.1, 2.5] }),
        P('offX', 'Offset X', 0, 2, 0.005, 1, { group: 'Offset' }),
        P('offY', 'Offset Y', 0, 2, 0.005, 0.7, { group: 'Offset' }),
        P('offZ', 'Offset Z', 0, 2, 0.005, 0.45, { group: 'Offset' }),
        P('plate', 'Plate thickness', 0.05, 0.8, 0.002, 0.22, { calm: [0.15, 0.3] }),
        P('inset', 'Panel seams', 0, 0.1, 0.001, 0.03, { calm: [0.015, 0.045], hint: 'Depth of the grooves between plates.' }),
        ...rot('rot', 'Rotation', [0, 0, 6])
      ],
      pack: (v) => ({ p: [v.scale, v.offX, v.offY, v.offZ, v.plate, v.inset, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float grbBox(vec3 p, vec3 b) { vec3 q = abs(p) - b; return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0); }
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float dr = 1.0;
  float d = 1e9;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.y < z.z) z.yz = z.zy;
    if (z.x < z.y) z.xy = z.yx;
    z = uRotA * z;
    // this generation's plate, lying on the y face of the folded cell
    vec3 pq = z - vec3(0.0, 1.0 - uP[4], 0.0);
    float pl = grbBox(pq, vec3(1.0, uP[4], 1.0));
    vec2 g = abs(fract(pq.xz * 2.0) - 0.5);
    pl += uP[5] * smoothstep(0.06, 0.0, min(g.x, g.y));
    if (pl / dr < d) { d = pl / dr; it = float(i) / float(uIter); }
    z = z * s - off * (s - 1.0);
    dr *= s;
    trap = min(trap, vec4(abs(z), dot(z, z)));
  }
  return d;
}`
    },
    {
      id: 'pipeworks', name: 'Pipeworks', cat: 'machine',
      blurb: 'Every generation runs pipes along the three axes and branches where they end: manifolds, junctions and refinery tangles.',
      size: 1.8, step: 0.8, iter: { min: 1, max: 10, def: 4 }, camera: { dist: 3.2, pitch: 24, yaw: 32 },
      params: [
        P('scale', 'Scale', 1.5, 4, 0.005, 3, { calm: [2.7, 3.3] }),
        P('offX', 'Offset X', 0, 2, 0.005, 1.5, { group: 'Offset', hint: '1.5 at scale 3 puts each smaller cross exactly on a pipe end.' }),
        P('offY', 'Offset Y', 0, 2, 0.005, 0, { group: 'Offset' }),
        P('offZ', 'Offset Z', 0, 2, 0.005, 0, { group: 'Offset' }),
        P('pipe', 'Pipe radius', 0.02, 0.5, 0.002, 0.09, { calm: [0.06, 0.13] }),
        P('joint', 'Joint size', 0, 1, 0.005, 0.17, { calm: [0.18, 0.3], hint: 'Radius of the flanged junction at each pipe end.' }),
        ...rot('rot', 'Rotation', [0, 0, 0])
      ],
      pack: (v) => ({ p: [v.scale, v.offX, v.offY, v.offZ, v.pipe, v.joint, 0, 0], rotA: [v.rotX, v.rotY, v.rotZ] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  vec3 z = p;
  float s = uP[0];
  vec3 off = vec3(uP[1], uP[2], uP[3]);
  float L = 1.0;
  float dr = 1.0;
  float d = 1e9;
  trap = vec4(abs(z), dot(z, z));
  it = 0.0;
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    z = abs(z);
    if (z.x < z.y) z.xy = z.yx;
    if (z.x < z.z) z.xz = z.zx;
    if (z.y < z.z) z.yz = z.zy;
    z = uRotA * z;
    // pipe from the centre out along X, with a flanged joint at its end
    float pipe = max(length(z.yz) - uP[4], z.x - L);
    vec2 fq = vec2(length(z.yz) - uP[5], abs(z.x - L) - 0.35 * uP[5]);
    float flange = min(max(fq.x, fq.y), 0.0) + length(max(fq, 0.0));
    float sd = min(pipe, flange);
    if (sd / dr < d) { d = sd / dr; it = float(i) / float(uIter); }
    z = z * s - off * (s - 1.0);
    dr *= s;
    trap = min(trap, vec4(abs(z), dot(z, z)));
  }
  return d;
}`
    },
    {
      id: 'pistons', name: 'Piston field', cat: 'machine', unbounded: true,
      blurb: 'An endless deck of telescoping pistons. Phase pumps them (route it to the kick); Wave sets how the motion travels.',
      size: 2.0, step: 0.7, iter: { min: 1, max: 4, def: 3 }, camera: { dist: 3.5, pitch: 22, yaw: 35, ty: 0.4 },
      params: [
        P('cell', 'Spacing', 0.3, 2, 0.005, 0.7, { calm: [0.6, 0.8] }),
        P('radius', 'Radius', 0.05, 0.48, 0.002, 0.22, { calm: [0.18, 0.3], hint: 'Share of the spacing. Iterations = telescope stages.' }),
        P('base', 'Rest height', 0, 3, 0.005, 0.8, { calm: [0.5, 1.1] }),
        P('stroke', 'Stroke', 0, 2, 0.005, 0.6, { calm: [0.3, 0.8] }),
        P('phase', 'Phase', 0, 1, 0.001, 0, { calm: [0, 1], hint: 'Where the pistons are in their cycle. Animate it (a ramp or a beat route).' }),
        P('wave', 'Wave', 0, 3, 0.005, 1, { calm: [0.5, 1.5], hint: 'Phase difference between neighbours: 0 = all together, higher = a travelling wave.' }),
        P('cap', 'Cap', 0, 1, 0.005, 0.5, { calm: [0.3, 0.7], hint: 'Size of the head on each rod.' })
      ],
      pack: (v) => ({ p: [v.cell, v.radius, v.base, v.stroke, v.phase, v.wave, v.cap, 0] }),
      glsl: `
float pisCyl(vec3 p, float r, float y0, float y1) {
  vec2 d = vec2(length(p.xz) - r, abs(p.y - 0.5 * (y0 + y1)) - 0.5 * (y1 - y0));
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
float fde(vec3 p, out vec4 trap, out float it) {
  float cell = uP[0];
  vec2 id = floor(p.xz / cell);
  vec3 q = vec3(p.x - (id.x + 0.5) * cell, p.y, p.z - (id.y + 0.5) * cell);
  float r0 = uP[1] * cell;
  float ph = 6.2831853 * (uP[4] + uP[5] * 0.11 * (id.x * 0.7 + id.y * 1.3));
  float H = max(uP[2] + uP[3] * (0.5 + 0.5 * sin(ph)), 0.02);
  // deck with a collar round every rod
  float d = max(p.y, -(length(q.xz) - r0 * 1.15) + 0.0);
  d = min(d, pisCyl(q, r0 * 1.35, -0.2, 0.05));
  int st = uIter;
  float seg = H / float(st);
  it = 0.0;
  for (int k = 0; k < 4; k++) {
    if (k >= st) break;
    float rr = r0 * (1.0 - 0.18 * float(k));
    float c = pisCyl(q, rr, float(k) * seg - 0.01, float(k + 1) * seg);
    if (c < d) { d = c; it = float(k + 1) / float(st); }
  }
  float capR = r0 * (0.9 + uP[6] * 0.9);
  float cap = pisCyl(q, capR, H, H + 0.12 * cell * (0.5 + uP[6]));
  if (cap < d) { d = cap; it = 1.0; }
  trap = vec4(abs(q.x), abs(q.y - H), abs(q.z), 0.5 + 0.5 * sin(ph));
  vec2 edge = 0.5 * cell - abs(q.xz);
  float bnd = max(min(edge.x, edge.y), 0.0) + 0.02 * cell;
  return min(d, bnd);
}`
    },
    // ---------- landscapes ----------
    {
      id: 'terrain', name: 'Folded terrain', cat: 'land', unbounded: true,
      blurb: 'Endless mountains from folded (ridged) noise. Ridge sharpens the crests, Warp drags the valleys sideways. Auto-flight skims it.',
      size: 3.0, step: 0.45, iter: { min: 2, max: 10, def: 7 }, camera: { dist: 4.5, pitch: 16, yaw: 30, ty: 0.6 },
      params: [
        P('freq', 'Feature size', 0.1, 3, 0.002, 0.6, { calm: [0.4, 0.8], hint: 'Larger = smaller hills. Iterations = noise octaves.' }),
        P('height', 'Height', 0, 4, 0.005, 1.4, { calm: [1, 1.8] }),
        P('ridge', 'Ridge', 0, 1, 0.005, 0.7, { calm: [0.5, 0.9], hint: '0 = rolling hills, 1 = sharp folded crests.' }),
        P('gain', 'Roughness', 0.25, 0.7, 0.002, 0.48, { calm: [0.42, 0.55] }),
        P('warp', 'Warp', 0, 1.5, 0.005, 0.35, { calm: [0.2, 0.5] }),
        P('sea', 'Sea level', -1, 1, 0.005, -0.35, { calm: [-0.5, -0.2], hint: 'Flat floor the valleys fill to.' }),
        P('seed', 'Seed', 0, 100, 1, 3)
      ],
      pack: (v) => ({ p: [v.freq, v.height, v.ridge, v.gain, v.warp, v.sea, v.seed, 0] }),
      glsl: `
float terHash(vec2 p) { p = fract(p * vec2(234.34, 435.345)); p += dot(p, p + 34.23); return fract(p.x * p.y); }
float terNoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);
  return mix(mix(terHash(i), terHash(i + vec2(1, 0)), u.x), mix(terHash(i + vec2(0, 1)), terHash(i + vec2(1, 1)), u.x), u.y);
}
float fde(vec3 p, out vec4 trap, out float it) {
  vec2 q = p.xz * uP[0] + uP[6] * 13.1;
  float a = 1.0, sum = 0.0, norm = 0.0;
  mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    float n = terNoise(q);
    n = mix(n, 1.0 - abs(2.0 * n - 1.0), uP[2]);
    sum += a * n;
    norm += a;
    a *= uP[3];
    q = m * q + uP[4] * vec2(sum, -sum);
  }
  float hn = sum / max(norm, 1e-6);
  float h = max(uP[1] * (hn - 0.45), uP[5]);
  trap = vec4(vec3(1.0 - hn), hn * hn);
  it = hn;
  return (p.y - h) * 0.55;
}`
    },
    {
      id: 'kaliPlain', name: 'Kali coral plain', cat: 'land', unbounded: true,
      blurb: 'Ground grown from Kali\'s abs-inversion set: coral shelves, terraces and alien reef textures. C moves the reef through its forms.',
      size: 3.0, step: 0.35, iter: { min: 3, max: 20, def: 9 }, camera: { dist: 4.0, pitch: 22, yaw: 30, ty: 0.4 },
      params: [
        P('freq', 'Feature size', 0.02, 2, 0.002, 0.12, { calm: [0.08, 0.16] }),
        P('height', 'Height', 0, 3, 0.005, 0.7, { calm: [0.5, 0.9] }),
        P('cx', 'C x', 0, 1.5, 0.002, 0.58, { group: 'Kali C', calm: [0.5, 0.65] }),
        P('cy', 'C y', 0, 1.5, 0.002, 0.73, { group: 'Kali C', calm: [0.65, 0.8] }),
        P('focus', 'Contrast', 0.5, 8, 0.005, 1.5, { calm: [2, 3.2], hint: 'How sharply the reef stands out of the plain.' }),
        P('floor', 'Floor', -1, 1, 0.005, -0.2, { calm: [-0.3, -0.1] })
      ],
      pack: (v) => ({ p: [v.freq, v.height, v.cx, v.cy, v.focus, v.floor, 0, 0] }),
      glsl: `
float fde(vec3 p, out vec4 trap, out float it) {
  // tile the plane by mirroring so the set repeats without seams
  vec2 q = abs(fract(p.xz * uP[0] * 0.25 + 0.5) * 2.0 - 1.0) * 1.2;
  vec2 c = vec2(uP[2], uP[3]);
  float acc = 0.0, prev = 0.0;
  trap = vec4(1e9);
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    q = abs(q) / max(dot(q, q), 1e-3) - c;
    float l = length(q);
    acc += exp(-uP[4] * abs(l - prev));
    prev = l;
    trap = min(trap, vec4(abs(q), abs(q.x * q.y), l * l));
  }
  float hn = clamp(acc / float(uIter), 0.0, 1.0);
  float h = max(uP[1] * hn, uP[5]);
  it = hn;
  trap.w = hn;
  return (p.y - h) * 0.4;
}`
    },
    {
      id: 'ridges', name: 'Eroded ridges', cat: 'land', unbounded: true,
      blurb: 'Noise whose detail fades on steep slopes, the way erosion leaves it: sharp ridgelines, smooth gullies, canyons. Procedural, not a fractal formula.',
      size: 3.0, step: 0.45, iter: { min: 2, max: 10, def: 8 }, camera: { dist: 4.5, pitch: 18, yaw: 30, ty: 0.7 },
      params: [
        P('freq', 'Feature size', 0.1, 3, 0.002, 0.45, { calm: [0.3, 0.6] }),
        P('height', 'Height', 0, 4, 0.005, 1.8, { calm: [1.3, 2.2] }),
        P('erosion', 'Erosion', 0, 3, 0.005, 1.2, { calm: [0.8, 1.6], hint: 'How strongly steep slopes lose their small detail.' }),
        P('gain', 'Roughness', 0.25, 0.7, 0.002, 0.5, { calm: [0.44, 0.56] }),
        P('canyon', 'Canyon', 0, 1, 0.005, 0.3, { calm: [0.1, 0.5], hint: 'Folds the lowest band into steep-walled canyons.' }),
        P('seed', 'Seed', 0, 100, 1, 11)
      ],
      pack: (v) => ({ p: [v.freq, v.height, v.erosion, v.gain, v.canyon, v.seed, 0, 0] }),
      glsl: `
float rdgHash(vec2 p) { p = fract(p * vec2(234.34, 435.345)); p += dot(p, p + 34.23); return fract(p.x * p.y); }
vec3 rdgNoised(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f), du = 6.0 * f * (1.0 - f);
  float a = rdgHash(i), b = rdgHash(i + vec2(1, 0)), c = rdgHash(i + vec2(0, 1)), d = rdgHash(i + vec2(1, 1));
  return vec3(a + (b - a) * u.x + (c - a) * u.y + (a - b - c + d) * u.x * u.y,
              du * (vec2(b - a, c - a) + (a - b - c + d) * u.yx));
}
float fde(vec3 p, out vec4 trap, out float it) {
  vec2 q = p.xz * uP[0] + uP[5] * 9.7;
  float a = 0.0, b = 1.0, norm = 0.0;
  vec2 dsum = vec2(0.0);
  mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
  for (int i = 0; i < MAX_ITER; i++) {
    if (i >= uIter) break;
    vec3 n = rdgNoised(q);
    dsum += n.yz;
    a += b * n.x / (1.0 + uP[2] * dot(dsum, dsum));
    norm += b;
    b *= uP[3];
    q = m * q;
  }
  float hn = a / max(norm, 1e-6);
  // canyon: the lowest band is folded down into steep walls
  float cy = uP[4];
  float hh = hn - cy * 0.6 * smoothstep(0.35, 0.15, hn);
  float h = uP[1] * (hh - 0.3);
  trap = vec4(vec3(clamp(length(dsum) * 0.15, 0.0, 1.0)), hn * hn);
  it = hn;
  return (p.y - h) * 0.5;
}`
    }
  ];

  // Style groups for the Formula menu. A formula without its own cat is placed here; anything not clearly
  // leaning one way is Mixed.
  const CATEGORIES = [
    { id: 'arch', label: 'Architecture' },
    { id: 'machine', label: 'Machines' },
    { id: 'land', label: 'Landscapes' },
    { id: 'mixed', label: 'Mixed' }
  ];
  const CAT_OF = { mandelbox: 'arch', kifsMenger: 'arch', octahedra: 'arch', kaliBox: 'arch', pseudoKleinian: 'arch', hyperbolic: 'arch', amazingSurf: 'land' };
  FORMULAS.forEach((f) => { if (!f.cat) f.cat = CAT_OF[f.id] || 'mixed'; });

  // Menu order: by group, then as listed.
  function grouped() {
    const out = [];
    CATEGORIES.forEach((c) => FORMULAS.forEach((f) => { if (f.cat === c.id) out.push({ formula: f, group: c.label }); }));
    return out;
  }

  const BY_ID = {};
  FORMULAS.forEach((f) => { BY_ID[f.id] = f; });

  // Ids used by the first prototype that no longer exist.
  const LEGACY = { tricorn: 'julia', orbital: 'apollonian', nebula: 'kaleidoscopic' };

  function get(id) {
    return BY_ID[id] || BY_ID[LEGACY[id]] || BY_ID.mandelbulb;
  }

  function defaults(id) {
    const out = {};
    get(id).params.forEach((p) => { out[p.key] = p.def; });
    return out;
  }

  function clampParam(spec, value) {
    const v = Number(value);
    if (!Number.isFinite(v)) return spec.def;
    return Math.min(spec.max, Math.max(spec.min, v));
  }

  // Merge stored values over defaults, clamped to range.
  function resolveParams(id, values) {
    const f = get(id);
    const out = {};
    f.params.forEach((p) => { out[p.key] = clampParam(p, values ? values[p.key] : undefined); });
    return out;
  }

  // Rotation matrix (column-major mat3) from Euler angles in degrees, applied X then Y then Z.
  function rotMat3(deg) {
    if (!deg) return [1, 0, 0, 0, 1, 0, 0, 0, 1];
    const [ax, ay, az] = deg.map(rad);
    const cx = Math.cos(ax), sx = Math.sin(ax), cy = Math.cos(ay), sy = Math.sin(ay), cz = Math.cos(az), sz = Math.sin(az);
    const X = [1, 0, 0, 0, cx, sx, 0, -sx, cx];
    const Y = [cy, 0, -sy, 0, 1, 0, sy, 0, cy];
    const Z = [cz, sz, 0, -sz, cz, 0, 0, 0, 1];
    return mul3(Z, mul3(Y, X));
  }

  function mul3(a, b) {
    const o = new Array(9);
    for (let c = 0; c < 3; c += 1) {
      for (let r = 0; r < 3; r += 1) {
        o[c * 3 + r] = a[r] * b[c * 3] + a[3 + r] * b[c * 3 + 1] + a[6 + r] * b[c * 3 + 2];
      }
    }
    return o;
  }

  function pack(id, values) {
    const f = get(id);
    const v = resolveParams(f.id, values);
    const packed = f.pack(v);
    return {
      p: packed.p.map((x) => (Number.isFinite(x) ? x : 0)),
      rotA: rotMat3(packed.rotA),
      rotB: rotMat3(packed.rotB)
    };
  }

  return { FORMULAS, CATEGORIES, grouped, LEGACY, get, defaults, resolveParams, clampParam, pack, rotMat3 };
});
