/*
 * Portalkniven — the dimensions on the other side of the door.
 *
 *   0  DIN STUE         the real room (passthrough); nothing virtual in it
 *   1  DEN GRØNNE       damp, mossy, faintly glowing veins, stalks and spores
 *   2  METALVERDENEN    riveted steel plate, brushed and stained, rust in the seams, lamps, pipes, girders
 *   3  KLIPPEVERDENEN   layered sandstone, cracks, boulders and pebbles under a dusk sky with mesas on the horizon
 *
 * Every dimension is the SAME room: the headset's room scan (walls, furniture) is drawn with that dimension's
 * surface, so the other world lines up with what you can really walk in. Surfaces are procedural and live in
 * world space (triplanar lookups in one small tileable noise texture + a bump from screen-space derivatives),
 * so they need no UVs and look right on any scanned shape.
 *
 * Any dimension can be "home" (drawn everywhere) or "seen through the door" (drawn only where stencil == 1 and
 * clipped to the far side of the door's plane), or switched off. Fog, colour grade and sky are per dimension.
 */
import * as THREE from './vendor/three.module.min.js';

const V = THREE.Vector3;
const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

export const ORDER = { home: 0, bgHome: 5, mask: 10, fill: 20, other: 30 };          // the home sky comes after the home world (early depth rejection)
export const DIMS = [
  { key: 'stue', name: 'DIN STUE' },
  { key: 'groen', name: 'DEN GRØNNE' },
  { key: 'metal', name: 'METALVERDENEN' },
  { key: 'klippe', name: 'KLIPPEVERDENEN' },
  /* deep space (free play only): nothing to stand on, stars in every direction; you can turn but not travel */
  { key: 'rum', name: 'VERDENSRUMMET', space: true },
  /* fractal dimensions (free play only): endless spaces, ray-marched. The distance estimators come from Fractal Video (vfx-apps/fractal-video/js/formulas.js). */
  { key: 'apollo', name: 'APOLLONISK', fract: true },
  { key: 'klein', name: 'PSEUDO-KLEINSK', fract: true },
  { key: 'hyper', name: 'HYPERBOLSK', fract: true },
  { key: 'menger', name: 'MENGER-LABYRINTEN', fract: true },
  { key: 'boks', name: 'MANDELBOX-BYEN', fract: true },
  { key: 'kali', name: 'KALI-TEMPLET', fract: true },
  /* a furnished room with four dimensions of space, of which you see a three-dimensional slice (also its own choice on the start page) */
  { key: 'fire', name: 'DET FIREDIMENSIONALE RUM', fract: true, four: true },
];

/* ------------------------------------------------------------------ fractals */

/* Each: S = fractal units per metre, off = where in the fractal the door's foot stands (an open place), MAXT = how far a ray goes,
 * fde = the distance estimator (returns distance; trap.w = closest approach of the orbit, it = 0..1), pal = colour from (it, trap), fog. */
const FRACT = {
  apollo: { S: 0.2, par: [1.15, 0.95, 1.45], par2: [7.0, 3.0, 9.0], off: [1, 1.26, 1], maxT: 14, steps: 56, fog: [0.03, 0.05, 0.10], glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {
      vec3 z = p; float sc = 1.0, k = uParam + 0.03 * sin(uTime * 0.11); trap = vec4(1e9); it = 0.0;
      for (int i = 0; i < 9; i++) { if (float(i) >= uParam2) break; z = -1.0 + 2.0 * fract(0.5 * z + 0.5); float r2 = dot(z, z); if (r2 < trap.w) it = float(i); trap = min(trap, vec4(abs(z), r2)); float f = k / max(r2, 1e-6); z *= f; sc *= f; }
      it /= 7.0; return 0.25 * abs(z.y) / sc; }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.22, 0.55, 1.0), vec3(1.0, 0.72, 0.36), smoothstep(0.05, 0.75, it)) * (0.8 + 0.7 * clamp(tr.w * 1.6, 0.0, 1.0)) + vec3(0.5, 0.12, 0.55) * clamp(tr.x, 0.0, 1.0); }` },
  klein: { S: 0.3, par: [1.0, 0.86, 1.22], par2: [0.908, 0.80, 0.985], off: [0.9, 1.3, 0.9], maxT: 16, steps: 60, fog: [0.05, 0.04, 0.035], glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {
      vec3 cs = vec3(uParam2, uParam2, 0.924), z = p; float dr = 1.0; trap = vec4(1e9); it = 0.0;
      for (int i = 0; i < 8; i++) { z = 2.0 * clamp(z, -cs, cs) - z; float r2 = dot(z, z); if (r2 < trap.w) it = float(i); trap = min(trap, vec4(abs(z), r2)); float k = max(uParam / max(r2, 1e-9), 1.0); z *= k; dr *= k; }
      it /= 8.0; float rxy = length(z.xy); return max(rxy - 0.928, abs(rxy * z.z) / max(length(z), 1e-9)) / dr; }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.78, 0.62, 0.40), vec3(0.30, 0.42, 0.50), smoothstep(0.15, 0.85, it)) * (0.6 + 0.5 * clamp(tr.w, 0.0, 1.0)) + vec3(0.25, 0.12, 0.02) * (1.0 - clamp(tr.y * 3.0, 0.0, 1.0)); }` },
  hyper: { S: 0.085, par: [5.0, 4.4, 7.0], par2: [0.04, 0.012, 0.11], ball: 0.8, off: [0, -0.125, 0], maxT: 2.2, steps: 64, fog: [0.015, 0.02, 0.04], glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {
      float R2 = 1.0 / max(1e-3, cos(6.28318 / uParam)), D = sqrt(1.0 + R2); vec3 a = vec3(0.0, 0.0, 0.18 * sin(uTime * 0.05));      // {4,3,n}: n cubes round every edge; a slow hyperbolic drift
      float aa = dot(a, a), xx = dot(p, p), ax = dot(a, p), den = max(1.0 + 2.0 * ax + aa * xx, 1e-6), sc = (1.0 - aa) / den;
      vec3 z = ((1.0 + 2.0 * ax + xx) * a + (1.0 - aa) * p) / den; trap = vec4(abs(z), dot(z, z)); it = 0.0; vec3 cx = vec3(D, 0.0, 0.0);
      for (int i = 0; i < 14; i++) { z = abs(z); if (z.x < z.y) z.xy = z.yx; if (z.x < z.z) z.xz = z.zx; if (z.y < z.z) z.yz = z.zy;
        vec3 dv = z - cx; float l2 = dot(dv, dv); if (l2 >= R2) break; float k = R2 / l2; z = cx + dv * k; sc *= k; it += 1.0; trap = min(trap, vec4(abs(z), dot(z, z))); }
      it /= 14.0; float R = sqrt(R2), fx = abs(length(z - vec3(D, 0.0, 0.0)) - R), fy = abs(length(z - vec3(0.0, D, 0.0)) - R), fz = abs(length(z - vec3(0.0, 0.0, D)) - R);
      float d = min(length(vec2(fx, fy)) - uParam2, length(vec3(fx, fy, fz)) - uParam2 * 1.9);
      return max(d / sc, length(p) - 0.985); }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.95, 0.80, 0.50), vec3(0.20, 0.55, 0.95), smoothstep(0.0, 0.6, it)) * (0.7 + 0.5 * tr.w); }` },
};
/* Three more: an endless Menger sponge (tunnels inside tunnels; the classic construction, repeated with mod), and from Fractal Video the Kali coral plain (a ground grown from
 * Kali's abs-inversion set: shelves and reefs to fly low over) and the KaliBox (a carved temple standing close by, to fly up to and into). */
Object.assign(FRACT, {
  menger: { S: 0.12, par: [1.0, 0.78, 1.22], par2: [4.0, 2.0, 5.0], off: [0, -0.1, 0], maxT: 12, steps: 64, fog: [0.05, 0.045, 0.04], glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {                 // a Menger sponge repeated for ever: square tunnels inside tunnels, in every direction
      float d = -1e9, s = 1.0; trap = vec4(1e9); it = 0.0;
      for (int m = 0; m < 5; m++) { if (float(m) >= uParam2) break;
        vec3 a = mod(p * s, 2.0) - 1.0; s *= 3.0; vec3 r = abs(1.0 - 3.0 * abs(a));
        float c = (min(max(r.x, r.y), min(max(r.y, r.z), max(r.z, r.x))) - uParam) / s; if (c > d) { d = c; it = float(m); }
        trap = min(trap, vec4(abs(a), dot(a, a))); }
      it /= 4.0; return d; }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.80, 0.74, 0.62), vec3(0.85, 0.42, 0.22), smoothstep(0.1, 0.9, it)) * (0.6 + 0.45 * clamp(tr.w, 0.0, 1.0)) + vec3(0.05, 0.2, 0.3) * clamp(1.0 - tr.x * 2.0, 0.0, 1.0) * 0.4; }` },
  boks: { S: 0.25, par: [-1.8, -2.3, -1.5], par2: [0.5, 0.3, 0.75], off: [3.4, -0.3, 3.0], maxT: 30, steps: 90, fog: [0.035, 0.045, 0.07], glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {                  // the classic Mandelbox, one in every cell of an endless grid, with streets between them
      vec3 c = abs(mod(p + 3.4, 13.6) - 6.8) - 3.4, z = c; float dr = 1.0, s = uParam, mr2 = uParam2 * uParam2; trap = vec4(1e9); it = 0.0;
      for (int i = 0; i < 11; i++) { z = clamp(z, -1.0, 1.0) * 2.0 - z; float r2 = dot(z, z); if (r2 < trap.w) it = float(i); trap = min(trap, vec4(abs(z), r2));
        if (r2 < mr2) { float k = 1.0 / mr2; z *= k; dr *= k; } else if (r2 < 1.0) { float k = 1.0 / r2; z *= k; dr *= k; }
        z = z * s + c; dr = dr * abs(s) + 1.0; }
      it /= 11.0; return length(z) / abs(dr); }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.85, 0.74, 0.55), vec3(0.28, 0.46, 0.60), smoothstep(0.1, 0.75, it)) * (0.6 + 0.5 * clamp(tr.w * 0.5, 0.0, 1.0)) + vec3(0.45, 0.16, 0.06) * clamp(1.0 - tr.x * 1.5, 0.0, 1.0) * 0.5; }` },
  kali: { S: 0.4, par: [2.043, 1.9, 2.2], par2: [0.349, 0.24, 0.5], off: [0, 0.7, 1.05], maxT: 12, steps: 72, fog: [0.010, 0.011, 0.018], stars: true, glsl: `
    float fde(vec3 p, out vec4 trap, out float it) {
      float s = uParam, mr2 = uParam2; vec3 tr = vec3(0.036, -1.861, 0.036); vec4 c = vec4(-0.669, -1.303, -0.458, 0.0), z = vec4(p, 1.0); trap = vec4(1e9); it = 0.0;
      for (int i = 0; i < 11; i++) { z.xyz = abs(z.xyz) + tr; float r2 = dot(z.xyz, z.xyz); if (r2 < trap.w) it = float(i); trap = min(trap, vec4(abs(z.xyz), r2)); z *= clamp(max(mr2 / max(r2, 1e-9), mr2), 0.0, 1.0); z = z * (s / mr2) + c; }
      it /= 11.0; return (length(z.xyz) - abs(s - 1.0)) / abs(z.w) - pow(abs(s), -10.0); }
    vec3 pal(float it, vec4 tr) { return mix(vec3(0.85, 0.72, 0.52), vec3(0.42, 0.30, 0.42), smoothstep(0.1, 0.8, it)) * (0.65 + 0.5 * clamp(tr.w * 0.6, 0.0, 1.0)) + vec3(0.1, 0.25, 0.3) * clamp(tr.z * 0.5, 0.0, 1.0); }` },
  fire: { S: 1, par: [0, 0, 1], par2: [0, 0, 1], off: [0, 0, 0], maxT: 18, steps: 80, fog: [0.012, 0.013, 0.02], res: 1.25, fs: true },
});
/* the four-dimensional room's bounds, as the shader has them (centre and half-size in x, y, z, w) */
const ROOM4 = { c: [0, 1.4, -3.0, 0], b: [3.3, 1.4, 3.3, 3.3] };
const FOUR_FS = `precision highp float; varying vec2 vUv;
uniform vec3 uCam; uniform mat3 uRot; uniform vec4 uProj; uniform vec3 uAnchor; uniform float uYaw; uniform float uTime; uniform float uBright; uniform float uS; uniform vec3 uOff; uniform float uFine; uniform vec2 uJitUV;
uniform mat4 uR4; uniform vec4 uC4; uniform float uThru; uniform sampler2D uNoise;

/* A room with four dimensions of space: x and z along the floor, y up, and w, the direction we have no word for. What you see is the slice of it that our
 * three dimensions cut out. Everything is a solid in 4D, described by its distance function; the ray is marched through 4D along the slice. */
const vec4 RC = vec4(0.0, 1.4, -3.0, 0.0), RB = vec4(3.3, 1.4, 3.3, 3.3);     // the room: centre and half-size (x, y, z, w)
const vec4 LP = vec4(0.6, 2.2, -2.4, 0.0);                                    // the lamp hanging from the ceiling
const vec4 TC = vec4(1.3, 0.0, -2.3, -0.37);                                  // the table: its first set of legs stands at w = 0, the second at w = -0.74
const vec4 SC = vec4(0.25, 0.0, -2.3, 0.0);                                   // the stool
const vec4 PC = vec4(-1.7, 0.0, -3.8, 0.0);                                   // the column
const vec4 BC = vec4(-3.12, 1.0, -2.8, 0.0);                                  // the bookcase
const vec4 KC = vec4(-1.0, 0.25, -1.2, 0.0);                                  // the crate
const vec4 GC = vec4(0.9, 0.0, -2.3, 0.0);                                    // the rug
const vec4 FC = vec4(0.3, 1.5, -6.27, 0.0);                                   // the painting

float box4(vec4 p, vec4 b) { vec4 q = abs(p) - b; return length(max(q, 0.0)) + min(max(max(q.x, q.y), max(q.z, q.w)), 0.0); }
float h2(vec2 n) { return fract(sin(dot(n, vec2(127.1, 311.7))) * 43758.5453); }
float bookHash(vec4 p) { return h2(vec2(floor((p.z + 3.8) / 0.055) + floor(p.y / 0.4) * 37.0, floor(p.w / 0.33 + 0.5) * 13.0 + 5.0)); }

/* m: 0 room, 1 table, 2 globe, 3 brass, 4 painted wood, 5 seat, 6 marble, 7 bookcase, 8 books, 9 crate, 10 rug, 11 painting, 12 lamp */
float map(vec4 p, out int m, bool lamps) {
  float d = abs(-box4(p - RC, RB) + 0.1) - 0.1, e; m = 0;
  vec4 q = p - TC;                                                            // the table: a top, and eight legs (one at every corner, and there are eight corners)
  e = box4(vec4(q.x, p.y - 0.74, q.z, q.w), vec4(0.75, 0.022, 0.45, 0.45)) - 0.004;
  e = min(e, box4(vec4(abs(q.x) - 0.67, p.y - 0.36, abs(q.z) - 0.37, abs(q.w) - 0.37), vec4(0.03, 0.36, 0.03, 0.08)));
  if (e < d) { d = e; m = 1; }
  q = p - SC;                                                                 // the stool; its back is there too, but it stands off in w, where you cannot see it yet
  e = box4(vec4(abs(q.x) - 0.17, p.y - 0.215, abs(q.z) - 0.17, q.w), vec4(0.02, 0.215, 0.02, 0.06));
  e = min(e, box4(vec4(q.x, p.y - 0.72, q.z, q.w - 0.19), vec4(0.2, 0.25, 0.2, 0.012)));
  if (e < d) { d = e; m = 4; }
  e = box4(vec4(q.x, p.y - 0.45, q.z, q.w), vec4(0.2, 0.02, 0.2, 0.2)) - 0.008;
  if (e < d) { d = e; m = 5; }
  q = p - PC;                                                                 // a column that is round in three directions at once, and the ring standing on it
  float rc = length(q.xzw);
  e = max(rc - 0.22, abs(p.y - 0.525) - 0.525);
  e = min(e, max(rc - 0.27, abs(abs(p.y - 0.525) - 0.495) - 0.03));
  if (e < d) { d = e; m = 6; }
  e = length(vec3(length(vec2(q.x, p.y - 1.305)) - 0.2, q.z, q.w)) - 0.055;   // a ring here; turned through w it comes apart into two balls
  q = p - vec4(TC.x, 0.0, TC.z, 0.0);
  e = min(e, max(length(q.xzw) - 0.07, abs(p.y - 0.777) - 0.015));            // the foot of the globe on the table
  if (lamps) { q = p - LP; e = min(e, max(length(q.xzw) - 0.006, abs(p.y - 2.55) - 0.26)); }      // the lamp's cord (it throws no shadow: it would fall on everything)
  q = p - FC;
  if (e < d) { d = e; m = 3; }
  e = box4(q, vec4(0.85, 0.6, 0.03, 1.4));                                    // the painting on the far wall
  if (e < d) { d = e; m = 11; }
  q = p - BC;                                                                 // the bookcase, 3.2 m deep in w: other books at every depth
  float outer = box4(q, vec4(0.17, 1.0, 1.0, 1.6));
  if (outer < d) {
    float board = abs(mod(p.y + 0.2, 0.4) - 0.2) - 0.012;
    e = max(outer, min(-box4(q - vec4(0.04, 0.0, 0.0, 0.0), vec4(0.17, 0.975, 0.975, 2.0)), board));
    if (e < d) { d = e; m = 7; }
    float hh = bookHash(p), bh = 0.22 + 0.12 * fract(hh * 5.3), dh = 0.095 + 0.03 * fract(hh * 7.7);
    float bz = (fract((p.z + 3.8) / 0.055) - 0.5) * 0.055, bw = (fract(p.w / 0.33 + 0.5) - 0.5) * 0.33, y0 = floor(p.y / 0.4) * 0.4 + 0.012;
    e = hh < 0.13 ? 1.0 : box4(vec4(p.x - (-3.25 + dh), p.y - (y0 + bh * 0.5), bz, bw), vec4(dh, bh * 0.5, 0.0215, 0.155)) - 0.003;
    e = min(e, max(min(0.0275 - abs(bz), 0.165 - abs(bw)), 0.0) + 0.012);     // never step further than into the next book's cell
    e = max(e, box4(q, vec4(0.165, 0.97, 0.97, 1.6)));
    if (e < d) { d = e; m = 8; }
  }
  q = p - KC;                                                                 // a crate: a cube in x, z and w, standing on one corner in w, so its slice is a hexagon
  vec3 u = vec3(dot(q.xzw, vec3(0.81650, 0.0, 0.57735)), dot(q.xzw, vec3(-0.40825, 0.70711, 0.57735)), dot(q.xzw, vec3(-0.40825, -0.70711, 0.57735)));
  e = box4(vec4(u, q.y), vec4(0.26, 0.26, 0.26, 0.25)) - 0.004;
  if (e < d) { d = e; m = 9; }
  q = p - GC;                                                                 // a rug, round in three directions
  e = max(length(q.xzw) - 1.25, abs(p.y - 0.006) - 0.006);
  if (e < d) { d = e; m = 10; }
  if (lamps) {
    e = length(p - vec4(TC.x, 0.94, TC.z, 0.0)) - 0.15; if (e < d) { d = e; m = 2; }      // a glowing globe: a ball in four dimensions
    e = length(p - LP) - 0.11; if (e < d) { d = e; m = 12; }
  }
  return d;
}

vec3 wood(vec2 uv, vec3 dark, vec3 light) { float g = texture2D(uNoise, uv * vec2(0.4, 2.6)).r, f = texture2D(uNoise, uv * vec2(1.7, 11.0)).b; return mix(dark, light, g) * (0.86 + 0.28 * f); }

void main() {
  vec2 uv = vUv + uJitUV;
  vec3 rd3 = normalize(uRot * vec3(mix(uProj.x, uProj.y, uv.x), mix(uProj.z, uProj.w, uv.y), -1.0));
  float cy = cos(uYaw), sy = sin(uYaw); mat3 Ry = mat3(cy, 0.0, sy, 0.0, 1.0, 0.0, -sy, 0.0, cy);
  vec3 ro3 = Ry * (uCam - uAnchor); rd3 = Ry * rd3;
  float t = 0.0;
  if (uThru > 0.5 && ro3.z > 0.0 && rd3.z < 0.0) t = -ro3.z / rd3.z + 0.01;                                 // seen through the door: the ray starts in the doorway
  vec4 ro = uR4 * vec4(ro3, 0.0) + uC4, rd = uR4 * vec4(rd3, 0.0);
  const float MAXT = 18.0; float d = 0.0; int m = 0; bool hit = false;
  for (int i = 0; i < 80; i++) { d = map(ro + rd * t, m, true); if (d < 0.0006 + 0.0009 * t) { hit = true; break; } t += d; if (t > MAXT) break; }
  vec3 col = vec3(0.012, 0.013, 0.02);
  if (hit) {
    vec4 p = ro + rd * t; int mm; float e = 0.0015 + 0.0008 * t;
    vec4 n = normalize(vec4(map(p + vec4(e, 0.0, 0.0, 0.0), mm, true), map(p + vec4(0.0, e, 0.0, 0.0), mm, true), map(p + vec4(0.0, 0.0, e, 0.0), mm, true), map(p + vec4(0.0, 0.0, 0.0, e), mm, true)) - d);
    vec3 alb = vec3(0.6), emi = vec3(0.0); float rough = 0.8, metal = 0.0;
    if (m == 0) {
      vec4 r = p - RC;
      if (n.y > 0.7) {                                                        // the floor: oak boards
        float v = p.z / 0.13, row = floor(v), rid = h2(vec2(row, floor(p.w / 0.6 + 0.5) * 7.0)), xs = p.x / 1.1 + rid * 5.0, pid = h2(vec2(row * 3.0 + rid, floor(xs)));
        float gap = min(min(fract(v), 1.0 - fract(v)) * 0.13, min(fract(xs), 1.0 - fract(xs)) * 1.1);
        alb = wood(vec2(p.x + pid * 7.0, p.z + pid * 3.0 + p.w * 0.5), vec3(0.30, 0.17, 0.08), vec3(0.56, 0.36, 0.19)) * (0.82 + 0.36 * pid) * mix(0.3, 1.0, smoothstep(0.0, 0.004, gap)); rough = 0.3;
      } else if (n.y < -0.7) { alb = vec3(0.86, 0.85, 0.82) * (0.95 + 0.07 * texture2D(uNoise, p.xz * 0.5).r); rough = 0.95; }
      else {                                                                  // the walls: plaster above, painted panelling below
        vec4 an = abs(n); float hc = an.x > max(an.z, an.w) ? p.z + p.w : p.x + (an.z > an.w ? p.w : p.z);
        float mott = texture2D(uNoise, vec2(hc * 0.21, p.y * 0.21)).r, grain = texture2D(uNoise, vec2(hc * 2.3, p.y * 2.3)).b;
        alb = vec3(0.80, 0.75, 0.66) * (0.88 + 0.16 * mott) * (0.96 + 0.08 * grain); rough = 0.95;
        if (p.y < 0.96) {
          vec2 f = vec2(fract(hc / 0.62), (p.y - 0.12) / 0.78); vec2 bd = min(f, 1.0 - f) * vec2(0.62, 0.78);
          float inset = smoothstep(0.075, 0.085, min(bd.x, bd.y)), groove = 1.0 - smoothstep(0.0, 0.012, abs(min(bd.x, bd.y) - 0.08));
          alb = vec3(0.33, 0.43, 0.38) * (0.94 + 0.1 * grain) * (1.0 - 0.07 * inset) * (1.0 - 0.45 * groove); rough = 0.5;
          if (p.y < 0.11) alb = vec3(0.74, 0.72, 0.66);
          if (p.y > 0.92) alb = vec3(0.40, 0.50, 0.45);
        }
      }
    } else if (m == 1) { vec4 q = p - TC; alb = wood(vec2(q.x + q.w * 0.6, q.z + p.y * 0.7), vec3(0.17, 0.085, 0.04), vec3(0.42, 0.24, 0.12)); rough = 0.22; }
    else if (m == 2) { float f = max(0.0, -dot(n, rd)); emi = vec3(1.0, 0.82, 0.55) * (0.9 + 1.5 * f * f); alb = vec3(0.9); rough = 0.2; }
    else if (m == 3) { alb = vec3(0.86, 0.62, 0.26); metal = 1.0; rough = 0.28; }
    else if (m == 4) { alb = vec3(0.76, 0.73, 0.65) * (0.9 + 0.14 * texture2D(uNoise, vec2(p.y * 1.5, p.x * 6.0 + p.z * 6.0 + p.w * 6.0)).b); rough = 0.45; }
    else if (m == 5) { float wv = texture2D(uNoise, vec2(p.x + p.w, p.z) * 9.0).b; alb = vec3(0.50, 0.11, 0.09) * (0.75 + 0.5 * wv); rough = 0.95; }
    else if (m == 6) {
      vec2 mu = vec2(p.x + p.y * 0.35 + p.w, p.z + p.y * 0.8); float vein = texture2D(uNoise, mu * 0.55).g, cloud = texture2D(uNoise, mu * 0.3).r;
      alb = mix(vec3(0.84, 0.82, 0.78), vec3(0.30, 0.31, 0.35), (1.0 - smoothstep(0.0, 0.07, vein)) * 0.75) * (0.86 + 0.2 * cloud); rough = 0.16;
    } else if (m == 7) { alb = wood(vec2(p.y, p.z + p.w + p.x * 3.0), vec3(0.09, 0.05, 0.03), vec3(0.24, 0.13, 0.07)); rough = 0.4; }
    else if (m == 8) {
      float hh = bookHash(p), hue = fract(hh * 3.7), y0 = floor(p.y / 0.4) * 0.4 + 0.012, bh = 0.22 + 0.12 * fract(hh * 5.3), fy = (p.y - y0) / bh;
      alb = hue < 0.25 ? vec3(0.42, 0.07, 0.06) : hue < 0.5 ? vec3(0.07, 0.17, 0.30) : hue < 0.7 ? vec3(0.10, 0.26, 0.13) : hue < 0.88 ? vec3(0.42, 0.28, 0.12) : vec3(0.60, 0.56, 0.46);
      alb *= 0.8 + 0.4 * texture2D(uNoise, vec2(p.z * 4.0, p.y * 4.0)).b; rough = 0.6;
      if (n.x > 0.6 && (abs(fy - 0.16) < 0.018 || abs(fy - 0.84) < 0.018 || (abs(fy - 0.62) < 0.06 && fract(hh * 11.0) > 0.4))) { alb = vec3(0.86, 0.66, 0.28); metal = 0.8; rough = 0.35; }      // gilt bands on the spines
      if (n.y > 0.6) { alb = vec3(0.80, 0.76, 0.64); rough = 0.9; }                                        // the pages, seen from above
    } else if (m == 9) {
      vec4 q = p - KC; float pl = abs(fract(p.y / 0.125 + 0.5) - 0.5) * 0.125;
      alb = wood(vec2(dot(q.xzw, vec3(0.7, 0.9, 0.8)), p.y * 0.3 + floor(p.y / 0.125) * 0.37), vec3(0.50, 0.35, 0.18), vec3(0.78, 0.62, 0.38)) * mix(0.35, 1.0, smoothstep(0.0, 0.005, pl)); rough = 0.8;
    } else if (m == 10) {
      float r = length((p - GC).xzw) / 1.25, band = fract(r * 5.0), wool = texture2D(uNoise, (p.xz + p.w) * 6.0).b;
      alb = r > 0.93 ? vec3(0.70, 0.62, 0.45) : band < 0.5 ? vec3(0.40, 0.07, 0.07) : band < 0.62 ? vec3(0.72, 0.64, 0.46) : band < 0.9 ? vec3(0.08, 0.12, 0.25) : vec3(0.72, 0.64, 0.46);
      alb *= 0.7 + 0.5 * wool; rough = 1.0;
    } else if (m == 11) {
      vec4 q = p - FC;
      if (abs(q.x) < 0.76 && abs(q.y) < 0.51 && n.z > 0.5) {                  // the canvas: a landscape that is another landscape at every depth of w
        vec2 c = vec2(q.x + q.w * 0.9, q.y);
        float hill = -0.12 + 0.30 * texture2D(uNoise, vec2(c.x * 0.22, 0.3)).r + 0.05 * texture2D(uNoise, vec2(c.x * 0.9, 0.6)).a;
        float far = 0.02 + 0.26 * texture2D(uNoise, vec2(c.x * 0.13 + 0.4, 0.8)).a;
        vec3 sky = mix(vec3(0.86, 0.66, 0.44), vec3(0.30, 0.46, 0.66), smoothstep(-0.2, 0.5, q.y)) + vec3(0.2, 0.16, 0.1) * smoothstep(0.55, 0.8, texture2D(uNoise, c * vec2(0.25, 0.6) + 0.2).r);
        alb = q.y < hill ? mix(vec3(0.10, 0.20, 0.08), vec3(0.34, 0.36, 0.14), texture2D(uNoise, c * 0.8).r) * (0.6 + 0.8 * smoothstep(-0.5, hill, q.y)) : q.y < far ? vec3(0.30, 0.36, 0.44) : sky;
        alb *= 0.9 + 0.2 * texture2D(uNoise, c * 5.0).b; rough = 0.7;
      } else { alb = vec3(0.80, 0.60, 0.24); metal = 0.9; rough = 0.4; }
    } else { emi = vec3(1.0, 0.86, 0.62) * 2.6; alb = vec3(1.0); }
    /* light: the hanging lamp (it is a point in 4D too: the further you are from it in w, the dimmer), the globe, and what the room throws back */
    vec3 V3 = vec3(0.0); vec4 L = LP - p; float ld = length(L); L /= ld;
    float sh = 1.0, st = 0.05;
    for (int i = 0; i < 22; i++) { float h = map(p + n * 0.02 + L * st, mm, false); sh = min(sh, 14.0 * h / st); st += max(h, 0.04); if (h < 0.002 || st > ld - 0.3) break; }
    sh = smoothstep(0.0, 1.0, clamp(sh, 0.0, 1.0));
    float ao = 1.0; for (int i = 1; i <= 3; i++) { float s = 0.06 * float(i) * float(i); ao -= max(0.0, s - map(p + n * s, mm, false)) / s * 0.3; }
    ao = clamp(ao, 0.15, 1.0);
    float ndl = max(dot(n, L), 0.0), att = 4.4 / (1.0 + 0.30 * ld * ld);
    vec4 H = normalize(L - rd); float spec = pow(max(dot(n, H), 0.0), mix(180.0, 8.0, rough)) * mix(1.6, 0.05, rough);
    vec4 G = vec4(TC.x, 0.94, TC.z, 0.0) - p; float gd = length(G); G /= gd; float gl = max(dot(n, G), 0.0) * 0.5 / (0.15 + gd * gd);
    vec3 warm = vec3(1.0, 0.86, 0.66);
    vec3 amb = mix(vec3(0.16, 0.12, 0.08), vec3(0.26, 0.24, 0.21), 0.5 + 0.5 * n.y) * ao;
    vec4 R = reflect(rd, n); vec3 env = mix(vec3(0.20, 0.12, 0.06), vec3(0.75, 0.68, 0.56), smoothstep(-0.4, 0.6, R.y)) * (0.5 + 0.5 * ao);
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 5.0);
    vec3 dif = alb * (amb + warm * (ndl * att * sh + gl));
    vec3 met = alb * (env * 1.1 + warm * spec * att * sh * 2.0);
    col = mix(dif + warm * spec * att * sh + env * fres * (1.0 - rough) * 0.5, met, metal) + emi;
    col = mix(col, vec3(0.012, 0.013, 0.02), smoothstep(MAXT * 0.7, MAXT, t));
  }
  col = col / (1.0 + col * 0.6); col = pow(col, vec3(0.4545));
  gl_FragColor = vec4(col * uBright, sqrt(clamp(t / MAXT, 0.0, 1.0)));
}
`;
const MARCH_VS = 'varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }';
const marchFS = f => `precision highp float; varying vec2 vUv;
  uniform vec3 uCam; uniform mat3 uRot; uniform vec4 uProj; uniform vec3 uAnchor; uniform float uYaw; uniform float uTime; uniform float uBright; uniform float uS; uniform vec3 uOff; uniform float uParam; uniform float uParam2; uniform float uFine; uniform vec2 uJitUV;
  uniform float uHue;
  vec3 hueTurn(vec3 c) { const vec3 k = vec3(0.57735); float ca = cos(uHue), sa = sin(uHue); return max(c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca), 0.0); }      // the colour scheme: every colour turned round the grey axis
  ${f.glsl}
  void main() {
    vec2 uv = vUv + uJitUV;
    vec3 rd = normalize(uRot * vec3(mix(uProj.x, uProj.y, uv.x), mix(uProj.z, uProj.w, uv.y), -1.0));
    float c = cos(uYaw), s = sin(uYaw); mat3 Ry = mat3(c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c);              // the fractal is laid out along the door
    vec3 ro = Ry * (uCam - uAnchor) * uS + uOff; rd = Ry * rd;
    float t = 0.0, d = 0.0, it = 0.0; vec4 trap = vec4(0.0); int n = 0;
    vec2 q = vUv - 0.5; float edge = dot(q, q) * 2.0;                                                       // coarser towards the edge of view
    for (int i = 0; i < ${f.steps}; i++) { d = fde(ro + rd * t, trap, it); if (d < (0.0013 - 0.0007 * uFine + 0.0025 * (1.0 - 0.7 * uFine) * edge) * t + 0.00035 - 0.00015 * uFine || t > ${f.maxT.toFixed(1)}) break; t += d * 0.92; n = i; }
    vec3 fog = hueTurn(vec3(${f.fog.map(v => v.toFixed(3)).join(', ')})), col = fog;
    if (t < ${f.maxT.toFixed(1)}) {
      vec3 p = ro + rd * t, L = normalize(vec3(0.5, 0.8, 0.3)); vec4 t2; float i2; float e = 0.002 + 0.004 * t;
      float dif = clamp((fde(p + L * e, t2, i2) - d) / e * 0.5 + 0.5, 0.0, 1.0);                            // one extra sample: light from the field's slope
      float ao = 1.0 - float(n) / ${f.steps}.0;
      col = hueTurn(pal(it, trap)) * (0.32 + 1.0 * dif) * (0.45 + 0.55 * ao * ao);
      col = mix(col, fog, 1.0 - exp(-t * t * ${(3.2 / (f.maxT * f.maxT)).toFixed(4)}));
    }
    col += fog * 2.0 * float(n) / ${f.steps}.0 * 0.6;                                                       // a glow where the ray grazed things
    gl_FragColor = vec4(col * uBright, sqrt(clamp(t / ${f.maxT.toFixed(1)}, 0.0, 1.0))); }`;       // alpha: how far the ray went (the resolve pass needs it)
/* Building the picture up over time (while you stand still). Every frame the small picture is marched with a different sub-pixel offset; this pass writes a
 * larger picture: for each of its pixels it takes the nearest new sample (weighted by how close that sample really landed), finds where that point of the fractal
 * was in last frame's larger picture (from the sample's distance and both frames' eye poses), and blends. The old value is clamped to what the new samples around
 * it allow, which is what keeps it from smearing when the head moves. */
const RESOLVE_FS = `precision highp float; varying vec2 vUv;
  uniform sampler2D uCur, uHist; uniform vec2 uLow, uHi, uJit; uniform vec3 uCam, uCamP; uniform mat3 uRot, uRotP; uniform vec4 uProj, uProjP; uniform float uMaxT, uInvS, uReset;
  void main() {
    vec2 idx = floor(vUv * uLow - uJit), sUV = (idx + 0.5) / uLow, sp = (idx + 0.5 + uJit) / uLow;      // the sample nearest this pixel, and where it really looked
    vec4 c = texture2D(uCur, sUV);
    vec2 dpx = (sp - vUv) * uHi; float d2 = dot(dpx, dpx);
    vec3 mn = c.rgb, mx = c.rgb, n;
    n = texture2D(uCur, sUV + vec2(1.0, 0.0) / uLow).rgb; mn = min(mn, n); mx = max(mx, n);
    n = texture2D(uCur, sUV - vec2(1.0, 0.0) / uLow).rgb; mn = min(mn, n); mx = max(mx, n);
    n = texture2D(uCur, sUV + vec2(0.0, 1.0) / uLow).rgb; mn = min(mn, n); mx = max(mx, n);
    n = texture2D(uCur, sUV - vec2(0.0, 1.0) / uLow).rgb; mn = min(mn, n); mx = max(mx, n);
    vec3 outc = c.rgb;
    if (uReset < 0.5) {
      float t = c.a * c.a * uMaxT * uInvS;                                                                // metres along the ray
      vec3 rd = normalize(uRot * vec3(mix(uProj.x, uProj.y, vUv.x), mix(uProj.z, uProj.w, vUv.y), -1.0));
      vec3 v = (uCam + rd * t - uCamP) * uRotP;                                                           // that point, as last frame's eye saw it
      if (v.z < -1e-4) {
        vec2 q = vec2((v.x / -v.z - uProjP.x) / (uProjP.y - uProjP.x), (v.y / -v.z - uProjP.z) / (uProjP.w - uProjP.z));
        if (q.x >= 0.0 && q.x <= 1.0 && q.y >= 0.0 && q.y <= 1.0) {
          vec3 h = clamp(texture2D(uHist, q).rgb, mn - 0.03, mx + 0.03);
          outc = mix(h, c.rgb, clamp(0.4 * exp(-d2 * 1.3), 0.035, 1.0));
        }
      }
    }
    gl_FragColor = vec4(outc, c.a); }`;
const BLIT_FS = 'precision highp float; varying vec2 vUv; uniform sampler2D uTex; void main() { gl_FragColor = vec4(texture2D(uTex, vUv).rgb, 1.0); }';



/* ------------------------------------------------------------------ deep space */

/* Stars without a texture: the sphere of the sky is cut through a grid of cells, and every cell has a point of light somewhere in it. Cells the sphere
 * passes close to the middle of give bright stars, the rest fainter ones, so one layer already has a spread of magnitudes; three layers of different cell
 * size give the myriad. No poles, no seams, and the same stars whichever way you look. */
export const STARS_GLSL = `
  vec3 sh3(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yxz + 33.33); return fract((p.xxy + p.yxx) * p.zyx); }
  vec3 starTint(float t) { return mix(mix(vec3(1.0, 0.66, 0.42), vec3(1.0, 0.95, 0.88), smoothstep(0.0, 0.42, t)), vec3(0.68, 0.80, 1.0), smoothstep(0.55, 1.0, t)); }
  vec3 starLayer(vec3 d, float sc, float keep, float rad, float halo) {
    vec3 p = d * sc, id = floor(p), h2 = sh3(id + 71.3);
    if (h2.x > keep) return vec3(0.0);
    float r = length(p - id - 0.5 - (sh3(id) - 0.5) * (1.0 - 2.0 * max(rad, halo)));
    float b = max(0.0, 1.0 - r / rad), g = max(0.0, 1.0 - r / max(halo, 1e-3)); g *= g;
    return starTint(h2.z) * (0.35 + 0.65 * h2.y) * (b * b * 1.25 + g * g * 0.22 * step(0.001, halo));
  }
  /* dens: 0 in the empty sky, 1 in the thick of the Milky Way */
  vec3 starField(vec3 d, float dens) {
    return starLayer(d, 236.0, 0.13 + 0.42 * dens, 0.30, 0.0) * 0.85 + starLayer(d.zxy, 113.0, 0.15 + 0.2 * dens, 0.17, 0.0) * 1.0 + starLayer(d.yzx, 37.0, 0.13, 0.075, 0.34) * 1.5;
  }`;
/* The whole sky of the space dimension: the Milky Way (a warped band with a bright bulge towards the core, dust lanes and star clouds), a few glowing nebulae,
 * two far galaxies, and the stars, thicker along the band. */
const SPACE_GLSL = STARS_GLSL + `
  vec3 spaceSky(vec3 d) {
    const vec3 GP = vec3(0.50028, 0.80044, 0.33018), GC = vec3(0.29001, 0.20440, -0.93494), GT = vec3(-0.81585, 0.56348, -0.12988);
    float b = dot(d, GP), lon = atan(dot(d, GT), dot(d, GC));
    vec2 uv = vec2(lon / 6.28318, b);
    float n1 = textureLod(uNoise, uv * vec2(3.0, 1.3), 0.0).r, n2 = textureLod(uNoise, uv * vec2(8.0, 3.3) + 0.37, 0.0).a, n3 = textureLod(uNoise, uv * vec2(24.0, 9.5) + 0.11, 0.0).b;
    float bulge = exp(-lon * lon * 1.6), w = 0.085 + 0.13 * bulge + 0.03 * n1;
    float bw = b + (n1 - 0.5) * 0.11 + (n2 - 0.5) * 0.035;
    float band = exp(-bw * bw / (w * w)), wide = exp(-bw * bw / (w * w * 7.0));
    float clouds = 0.30 + 0.85 * n2 * n2 + 0.22 * n3;
    vec3 col = vec3(0.004, 0.005, 0.009);
    col += mix(vec3(0.36, 0.43, 0.62), vec3(0.95, 0.74, 0.50), bulge) * band * clouds * (0.22 + 0.50 * bulge) + vec3(0.05, 0.06, 0.10) * wide * (0.5 + n1);
    float dl = textureLod(uNoise, uv * vec2(5.0, 2.1) + 0.53, 0.0).r * 0.72 + n2 * 0.2 + n3 * 0.08;
    float dust = smoothstep(0.40, 0.72, dl) * exp(-bw * bw / (w * w * 0.22));
    col *= 1.0 - 0.88 * dust; col += vec3(0.06, 0.028, 0.014) * dust * (1.0 - dust) * band * 2.0;
    { float a = dot(d, vec3(-0.31334, 0.58063, -0.75145)); if (a > 0.97134) { vec2 q = vec2(dot(d, vec3(-0.92298, 0.00000, 0.38486)), dot(d, vec3(0.22346, 0.81416, 0.53591))) / 0.23770; float m = max(0.0, 1.0 - dot(q, q)); m *= m;
        vec4 nz = textureLod(uNoise, q * 0.31 + vec2(0.13, 0.57), 0.0), nf = textureLod(uNoise, q * 1.3 + vec2(0.13, 0.57).yx, 0.0);
        float f = smoothstep(0.34, 0.82, nz.r * 0.8 + nf.b * 0.2) * m, g = smoothstep(0.45, 0.9, nz.a) * m;
        col += vec3(0.55, 0.10, 0.16) * f * f * 0.85 + vec3(0.08, 0.26, 0.30) * g * 0.5; col *= 1.0 - 0.35 * m * smoothstep(0.45, 0.8, nf.a) * (1.0 - nz.r); } }
    { float a = dot(d, vec3(0.80552, -0.58218, -0.11050)); if (a > 0.98723) { vec2 q = vec2(dot(d, vec3(-0.13590, 0.00000, -0.99072)), dot(d, vec3(0.57678, 0.81306, -0.07912))) / 0.15932; float m = max(0.0, 1.0 - dot(q, q)); m *= m;
        vec4 nz = textureLod(uNoise, q * 0.31 + vec2(0.61, 0.22), 0.0), nf = textureLod(uNoise, q * 1.3 + vec2(0.61, 0.22).yx, 0.0);
        float f = smoothstep(0.34, 0.82, nz.r * 0.8 + nf.b * 0.2) * m, g = smoothstep(0.45, 0.9, nz.a) * m;
        col += vec3(0.50, 0.12, 0.22) * f * f * 0.85 + vec3(0.16, 0.20, 0.42) * g * 0.5; col *= 1.0 - 0.35 * m * smoothstep(0.45, 0.8, nf.a) * (1.0 - nz.r); } }
    { float a = dot(d, vec3(-0.34517, 0.48680, 0.80243)); if (a > 0.94275) { vec2 q = vec2(dot(d, vec3(0.91862, -0.00000, 0.39515)), dot(d, vec3(0.19236, 0.87351, -0.44718))) / 0.33349; float m = max(0.0, 1.0 - dot(q, q)); m *= m;
        vec4 nz = textureLod(uNoise, q * 0.31 + vec2(0.37, 0.81), 0.0), nf = textureLod(uNoise, q * 1.3 + vec2(0.37, 0.81).yx, 0.0);
        float f = smoothstep(0.34, 0.82, nz.r * 0.8 + nf.b * 0.2) * m, g = smoothstep(0.45, 0.9, nz.a) * m;
        col += vec3(0.10, 0.16, 0.36) * f * f * 0.85 + vec3(0.22, 0.10, 0.30) * g * 0.5; col *= 1.0 - 0.35 * m * smoothstep(0.45, 0.8, nf.a) * (1.0 - nz.r); } }
    { float a = dot(d, vec3(-0.99281, -0.10655, -0.05463)); if (a > 0.99639) { vec2 q = vec2(dot(d, vec3(-0.05494, 0.00000, 0.99849)), dot(d, vec3(-0.10639, 0.99431, -0.00585))) / 0.08490; q = mat2(0.8253, 0.5646, -0.5646, 0.8253) * q; q.y /= 0.42; float r = length(q);
        float arms = 0.5 + 0.5 * cos(2.0 * atan(q.y, q.x) - 7.0 * log(r + 0.04)); float fade = 1.0 - smoothstep(0.7, 1.0, r);
        col += (vec3(1.0, 0.86, 0.66) * exp(-r * 13.0) * 0.75 + vec3(0.62, 0.72, 1.0) * exp(-r * 3.4) * (0.10 + 0.34 * arms * arms)) * fade; } }
    { float a = dot(d, vec3(0.57585, 0.08513, 0.81311)); if (a > 0.99939) { vec2 q = vec2(dot(d, vec3(0.81608, 0.00000, -0.57795)), dot(d, vec3(-0.04920, 0.99637, -0.06947))) / 0.03499; q = mat2(-0.5048, 0.8632, -0.8632, -0.5048) * q; q.y /= 0.70; float r = length(q);
        float arms = 0.5 + 0.5 * cos(2.0 * atan(q.y, q.x) - 7.0 * log(r + 0.04)); float fade = 1.0 - smoothstep(0.7, 1.0, r);
        col += (vec3(1.0, 0.86, 0.66) * exp(-r * 13.0) * 0.75 + vec3(0.62, 0.72, 1.0) * exp(-r * 3.4) * (0.10 + 0.34 * arms * arms)) * fade; } }

    col += starField(d, clamp(band * (1.0 - 0.7 * dust) + 0.25 * wide, 0.0, 1.0));
    return col;
  }`;

/* ------------------------------------------------------------------ noise */

/** One tileable 256² texture: R = soft fbm, G = cell borders (Worley F2-F1), B = fine grain, A = a second fbm. */
function makeNoiseTexture(N = 256) {
  let seed = 20261114;
  const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
  const tabs = new Map();
  const tab = (period, id) => { const k = id + '/' + period; let t = tabs.get(k); if (!t) { t = new Float32Array(period * period); for (let i = 0; i < t.length; i++) t[i] = rand(); tabs.set(k, t); } return t; };
  const vn = (x, y, period, id) => {
    const t = tab(period, id), xi = Math.floor(x), yi = Math.floor(y);
    let fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    const x0 = ((xi % period) + period) % period, y0 = ((yi % period) + period) % period, x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    const a = t[y0 * period + x0], b = t[y0 * period + x1], c = t[y1 * period + x0], d = t[y1 * period + x1];
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
  };
  const fbm = (u, v, base, oct, id) => { let s = 0, a = 0.5, p = base, n = 0; for (let o = 0; o < oct; o++) { s += a * vn(u * p, v * p, p, id + o); n += a; a *= 0.5; p *= 2; } return s / n; };
  const K = 10, px = new Float32Array(K * K), py = new Float32Array(K * K);
  for (let i = 0; i < K * K; i++) { px[i] = rand(); py[i] = rand(); }
  const worley = (u, v) => {
    const x = u * K, y = v * K, cx = Math.floor(x), cy = Math.floor(y); let f1 = 9, f2 = 9;
    for (let j = -1; j <= 1; j++) for (let i = -1; i <= 1; i++) {
      const gx = cx + i, gy = cy + j, wx = ((gx % K) + K) % K, wy = ((gy % K) + K) % K, k = wy * K + wx;
      const d = Math.hypot(gx + px[k] - x, gy + py[k] - y);
      if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
    }
    return f2 - f1;
  };
  const ch = [0, 1, 2, 3].map(() => new Float32Array(N * N));
  for (let y = 0; y < N; y++) for (let x = 0; x < N; x++) {
    const u = x / N, v = y / N, i = y * N + x;
    ch[0][i] = fbm(u, v, 4, 6, 'r'); ch[1][i] = worley(u, v); ch[2][i] = fbm(u, v, 32, 3, 'b'); ch[3][i] = fbm(u, v, 6, 5, 'a');
  }
  const data = new Uint8Array(N * N * 4);
  ch.forEach((c, k) => { let lo = 9, hi = -9; for (const v of c) { if (v < lo) lo = v; if (v > hi) hi = v; } for (let i = 0; i < c.length; i++) data[i * 4 + k] = Math.round(255 * (c[i] - lo) / (hi - lo)); });
  const t = new THREE.DataTexture(data, N, N, THREE.RGBAFormat);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.minFilter = THREE.LinearMipmapLinearFilter; t.magFilter = THREE.LinearFilter; t.generateMipmaps = true; t.anisotropy = 4; t.needsUpdate = true;
  return t;
}

/* 3D value noise on the CPU, for shaping boulders */
const h3 = (x, y, z) => { const n = Math.sin(x * 127.1 + y * 311.7 + z * 74.7) * 43758.5453; return n - Math.floor(n); };
function n3(x, y, z) {
  const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z); let fx = x - xi, fy = y - yi, fz = z - zi;
  fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
  const l = (a, b, t) => a + (b - a) * t;
  return l(l(l(h3(xi, yi, zi), h3(xi + 1, yi, zi), fx), l(h3(xi, yi + 1, zi), h3(xi + 1, yi + 1, zi), fx), fy),
           l(l(h3(xi, yi, zi + 1), h3(xi + 1, yi, zi + 1), fx), l(h3(xi, yi + 1, zi + 1), h3(xi + 1, yi + 1, zi + 1), fx), fy), fz);
}
const f3 = (x, y, z) => (0.5 * n3(x, y, z) + 0.25 * n3(x * 2.1, y * 2.1, z * 2.1) + 0.125 * n3(x * 4.3, y * 4.3, z * 4.3) + 0.0625 * n3(x * 8.9, y * 8.9, z * 8.9)) / 0.9375;

/** A boulder: a sphere pushed about by noise, with a few flat fracture faces and a flattened underside; smooth normals. */
function rockGeometry(detail, seed) {
  const g = new THREE.IcosahedronGeometry(1, detail), p = g.attributes.position, n = p.count;
  const cuts = []; for (let i = 0; i < 5; i++) { const c = new V(rnd(-1, 1), rnd(-0.3, 1), rnd(-1, 1)).normalize(); cuts.push({ n: c, d: rnd(0.72, 0.95) }); }
  const key = new Array(n), v = new V();
  for (let i = 0; i < n; i++) {
    v.fromBufferAttribute(p, i); key[i] = Math.round(v.x * 5e3) + ',' + Math.round(v.y * 5e3) + ',' + Math.round(v.z * 5e3);
    let r = 0.72 + 0.5 * f3(v.x * 1.3 + seed, v.y * 1.3 + seed * 2, v.z * 1.3) + 0.1 * (1 - Math.abs(2 * n3(v.x * 3.7 + seed, v.y * 3.7, v.z * 3.7) - 1));
    for (const c of cuts) { const dn = v.dot(c.n); if (dn > 0) r = Math.min(r, c.d / dn * 1.0 + 0.0); }        // planes cut flat faces
    v.multiplyScalar(r); if (v.y < -0.45) v.y = -0.45 - (v.y + 0.45) * 0.15;
    p.setXYZ(i, v.x, v.y, v.z);
  }
  const acc = new Map(), a = new V(), b = new V(), c = new V(), fn = new V();
  for (let i = 0; i < n; i += 3) {
    a.fromBufferAttribute(p, i); b.fromBufferAttribute(p, i + 1); c.fromBufferAttribute(p, i + 2);
    fn.copy(b).sub(a).cross(c.sub(a));
    for (let k = 0; k < 3; k++) { let s = acc.get(key[i + k]); if (!s) acc.set(key[i + k], s = new V()); s.add(fn); }
  }
  const nn = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) { const s = acc.get(key[i]); fn.copy(s).normalize(); nn[i * 3] = fn.x; nn[i * 3 + 1] = fn.y; nn[i * 3 + 2] = fn.z; }
  g.setAttribute('normal', new THREE.BufferAttribute(nn, 3));
  return g;
}

/** An upright slab of stone: a box worked over with noise, narrower towards a rounded top; its front (+z) is left nearly flat, to carry a mark. */
function slabGeometry(w, h, t, seed) {
  const g = new THREE.BoxGeometry(w, h, t, 8, 12, 2), p = g.attributes.position, v = new V();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i); const u = v.y / h + 0.5, n = f3(v.x * 2.3 + seed, v.y * 2.3, v.z * 2.3 + seed * 2) - 0.5, xr = v.x / (w / 2);
    v.x = v.x * (1 - 0.2 * u * u) + n * 0.05 * (0.3 + u); v.y -= xr * xr * u * u * h * 0.1; v.z += v.z < 0 ? n * 0.07 - 0.04 * u : n * 0.006;
    p.setXYZ(i, v.x, v.y + h / 2, v.z);
  }
  g.computeVertexNormals(); return g;
}

/* ------------------------------------------------------------------ shaders */

const HEAD = `
  varying vec3 vWP; varying vec3 vWN;
  uniform sampler2D uNoise; uniform float uTime; uniform vec3 uFogCol; uniform float uFogDen; uniform vec3 uGrade;
  vec4 tri(vec3 p, vec3 n, float s) { vec3 w = abs(n); w = w * w * w * w; w /= (w.x + w.y + w.z + 1e-5);
    return texture2D(uNoise, p.zy * s) * w.x + texture2D(uNoise, p.xz * s) * w.y + texture2D(uNoise, p.xy * s) * w.z; }
  vec4 tri1(vec3 p, vec3 n, float s) { vec3 a = abs(n); vec2 uv = (a.y >= a.x && a.y >= a.z) ? p.xz : (a.x > a.z ? p.zy : p.xy); return texture2D(uNoise, uv * s); }
  vec3 wBump(vec3 pv, vec3 N, float h) { vec3 dpdx = dFdx(pv), dpdy = dFdy(pv); float dhdx = dFdx(h), dhdy = dFdy(h);
    vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx); float det = dot(dpdx, r1); vec3 g = sign(det) * (dhdx * r1 + dhdy * r2);
    return normalize(abs(det) * N - g); }`;

const SURF = {
  /* --- the green one: wet earth, moss on what faces up, standing water in the hollows, a faint lichen glow --- */
  moss: vein => `
    float n1 = tri(P, Nw, 0.09).r, n2 = tri1(P, Nw, 0.7).a, n3 = tri1(P, Nw, 2.8).b, n4 = tri1(P, Nw, 7.0).b;
    float up = smoothstep(0.5, 0.9, Nw.y);
    float mossAmt = smoothstep(0.47, 0.66, n1 * 0.6 + n2 * 0.4 + up * 0.10);
    vec3 soil = mix(vec3(0.030, 0.024, 0.018), vec3(0.095, 0.072, 0.050), n2) * (0.75 + 0.5 * n3);
    vec3 moss = mix(vec3(0.026, 0.050, 0.016), vec3(0.085, 0.125, 0.040), n3) * (0.7 + 0.6 * n4);
    wA = mix(soil, moss, mossAmt);
    float pud = up * (1.0 - smoothstep(0.37, 0.42, n1));                // water lies where the ground is low
    float wet = up * (1.0 - smoothstep(0.40, 0.54, n1));                // and the earth round it is dark and shiny
    wA = mix(wA, wA * 0.45, wet * (1.0 - pud)); wA = mix(wA, vec3(0.010, 0.018, 0.016), pud * 0.92);
    wR = mix(mix(mix(0.62, 0.96, mossAmt), 0.3, wet * 0.8), 0.04, pud);
    wH = (1.0 - pud) * (n1 * 0.035 + mossAmt * (n3 * 0.014 + n4 * 0.004) + (1.0 - mossAmt) * n3 * 0.005)
       + pud * 0.0006 * sin(length(P.xz * 9.0) * 6.0 - uTime * 1.5) * n2;
    float vn = tri(P + vec3(0.0, uTime * 0.003, 0.0), Nw, 0.11).a;
    float vein = (1.0 - smoothstep(0.0, 0.016, abs(vn - 0.5))) * (1.0 - pud) * (1.0 - 0.6 * mossAmt);
    wE = vec3(0.02, 0.30, 0.22) * vein * (0.5 + 0.5 * sin(uTime * 0.4 + n1 * 9.0)) * ${vein.toFixed(2)};`,
  straw: () => 'float t = tri1(P, Nw, 6.0).b; wA = mix(vec3(0.46, 0.33, 0.15), vec3(0.74, 0.60, 0.32), t) * (0.55 + 0.6 * smoothstep(0.0, 0.5, P.y)); wR = 0.85;',
  stalk: () => 'wA = vec3(0.030, 0.050, 0.022) * (0.6 + 0.8 * tri1(P, Nw, 3.0).b); wR = 0.6;',
  bud: () => 'float g = 0.6 + 0.4 * sin(uTime * 0.7 + P.x * 5.0 + P.z * 3.0); wA = vec3(0.03, 0.07, 0.05); wE = vec3(0.05, 0.50, 0.38) * g * (0.35 + 0.65 * smoothstep(-0.2, 0.9, Nw.y)); wR = 0.35;',
  bark: () => `
    float f = tri1(P * vec3(1.0, 0.25, 1.0), Nw, 4.0).b, n = tri1(P, Nw, 0.9).a, fine = tri1(P, Nw, 6.0).b, ms = smoothstep(0.5, 0.7, n + smoothstep(0.3, 0.9, Nw.y) * 0.3);
    wA = mix(mix(vec3(0.030, 0.022, 0.015), vec3(0.085, 0.062, 0.042), f), vec3(0.05, 0.11, 0.03) * (0.6 + 0.8 * fine), ms);
    wR = mix(0.5, 0.95, ms); wH = f * 0.006 + ms * fine * 0.006;`,
  cap: () => `
    float sp = tri1(P, Nw, 11.0).b, under = 1.0 - smoothstep(-0.5, 0.15, Nw.y);
    wA = mix(mix(vec3(0.13, 0.10, 0.075), vec3(0.30, 0.25, 0.18), sp), vec3(0.05, 0.10, 0.08), under);
    wR = mix(0.35, 0.7, sp);
    wE = vec3(0.03, 0.40, 0.30) * under * (0.7 + 0.3 * sin(uTime * 0.6 + P.x * 9.0)) + vec3(0.004, 0.022, 0.016) * (1.0 - under);
    wH = sp * 0.0015;`,
  stem: () => 'wA = vec3(0.24, 0.22, 0.17) * (0.6 + 0.6 * tri1(P, Nw, 9.0).b); wR = 0.6; wE = vec3(0.003, 0.018, 0.013);',
  blade: () => 'float t = tri1(P, Nw, 5.0).b; wA = mix(vec3(0.020, 0.060, 0.015), vec3(0.085, 0.170, 0.040), t) * (0.5 + smoothstep(0.0, 0.14, P.y) * 0.9); wR = 0.5;',

  /* --- metal --- */
  plate: () => `
    vec3 an = abs(Nw);
    vec2 uv = (an.y >= an.x && an.y >= an.z) ? P.xz : (an.x > an.z ? P.zy : P.xy);
    vec2 pg = uv / vec2(1.2, 0.6);                              // plates of 1.2 x 0.6 m in a brick bond
    pg.x += 0.5 * mod(floor(pg.y), 2.0);
    vec2 cell = floor(pg), f = fract(pg);
    vec2 ed = min(f, 1.0 - f) * vec2(1.2, 0.6);                 // metres to the nearest seam on each axis
    float edge = min(ed.x, ed.y);
    float seam = 1.0 - smoothstep(0.003, 0.008, edge);
    float bevel = smoothstep(0.005, 0.028, edge);
    float pid = textureLod(uNoise, (cell + 0.5) / 37.0, 0.0).b, pid2 = textureLod(uNoise, (cell + 0.5) / 23.0 + 0.31, 0.0).a;
    float rv = length(ed - vec2(0.045));                        // a rivet in from each corner
    float rivet = 1.0 - smoothstep(0.011, 0.014, rv);
    float dome = rivet * sqrt(max(0.0, 1.0 - rv * rv / (0.014 * 0.014)));
    vec2 bu = pid > 0.5 ? uv : uv.yx;                           // each plate is brushed its own way
    float brush = texture2D(uNoise, bu * vec2(6.0, 0.08)).b * 0.6 + texture2D(uNoise, bu * vec2(17.0, 0.21)).b * 0.4;
    vec4 lo = tri(P, Nw, 0.14); float grime = lo.r; vec4 hi = texture2D(uNoise, uv * 1.7);
    float stain = smoothstep(0.50, 0.82, grime + (1.0 - bevel) * 0.2);
    float rust = smoothstep(0.66, 0.9, texture2D(uNoise, uv * 0.33 + 0.5).a + (1.0 - bevel) * 0.3) * 0.9;
    vec3 steel = mix(vec3(0.40, 0.42, 0.44), vec3(0.25, 0.27, 0.30), pid2) * (0.84 + 0.32 * brush);
    vec3 rustC = mix(vec3(0.12, 0.05, 0.02), vec3(0.27, 0.12, 0.05), hi.a);
    wA = mix(steel, rustC, rust); wA = mix(wA, wA * 0.4, stain * 0.55); wA = mix(wA, vec3(0.02), seam * 0.92); wA = mix(wA, steel * 1.1, rivet * 0.7);
    wM = (1.0 - rust) * (1.0 - seam);
    wR = mix(0.30 + 0.34 * brush, 0.9, max(rust, stain * 0.7)); wR = mix(wR, 0.3, rivet);
    wH = bevel * 0.0032 + dome * 0.0045 + (brush - 0.5) * 0.0004 * (1.0 - rust) + rust * hi.b * 0.0025;
    float hot = step(0.955, pid) * (1.0 - smoothstep(0.004, 0.02, ed.y)) * step(0.06, ed.x);    // a few long seams glow dull orange
    wE = vec3(1.0, 0.30, 0.04) * hot * 1.6 * (0.75 + 0.25 * sin(uTime * 1.3 + pid * 50.0));`,
  steel: () => `
    float brush = tri1(P * vec3(1.0, 0.03, 1.0), Nw, 9.0).b; vec4 lo = tri(P, Nw, 0.2), hi = tri1(P, Nw, 1.7);
    float rust = smoothstep(0.56, 0.9, tri1(P * vec3(1.0, 0.22, 1.0), Nw, 0.55).a * 0.75 + lo.a * 0.25 + (1.0 - smoothstep(0.0, 1.2, P.y)) * 0.2) * 0.8;
    wA = mix(vec3(0.30, 0.32, 0.35) * (0.82 + 0.34 * brush), mix(vec3(0.13, 0.055, 0.025), vec3(0.28, 0.125, 0.05), hi.a), rust);
    wA *= 1.0 - 0.45 * smoothstep(0.5, 0.8, lo.r);
    wM = 1.0 - rust; wR = mix(0.26 + 0.3 * brush, 0.88, rust); wH = (brush - 0.5) * 0.0005 + rust * hi.b * 0.002;`,
  copper: () => `
    vec4 lo = tri1(P, Nw, 0.6), hi = tri1(P, Nw, 4.0);
    float pat = smoothstep(0.52, 0.78, lo.a + hi.b * 0.2);
    wA = mix(vec3(0.78, 0.44, 0.26) * (0.85 + 0.3 * hi.r), vec3(0.22, 0.46, 0.38), pat);
    wM = 1.0 - pat * 0.9; wR = mix(0.24, 0.8, pat); wH = pat * hi.b * 0.0015;`,
  lamp: () => 'wA = vec3(0.02); wE = vec3(3.0, 3.3, 3.7); wR = 0.5;',
  ember: () => 'wA = vec3(0.02); wE = vec3(2.4, 0.7, 0.10) * (0.8 + 0.2 * sin(uTime * 2.1 + P.y * 9.0)); wR = 0.5;',

  /* --- rock --- */
  rock: () => `
    vec4 lo = tri(P, Nw, 0.11); float warp = lo.r;                 // one broad lookup bends the layers and masks the cracks and the dust
    float sy = P.y + warp * 0.55 + lo.a * 0.07;
    float bands = texture2D(uNoise, vec2(sy * 0.33, 0.37)).r, fineB = texture2D(uNoise, vec2(sy * 2.4, 0.71)).a, thin = texture2D(uNoise, vec2(sy * 9.0, 0.13)).b;
    vec3 c1 = vec3(0.36, 0.17, 0.09), c2 = vec3(0.52, 0.29, 0.15), c3 = vec3(0.21, 0.11, 0.07), c4 = vec3(0.60, 0.42, 0.27);
    wA = mix(c1, c2, smoothstep(0.35, 0.65, bands)); wA = mix(wA, c3, smoothstep(0.60, 0.80, fineB) * 0.65); wA = mix(wA, c4, smoothstep(0.72, 0.9, thin) * 0.35);
    vec4 mid = tri(P + warp * 0.6, Nw, 0.27);                      // cracks need to run unbroken round a boulder, so this one stays triplanar
    float crack = (1.0 - smoothstep(0.012, 0.05, mid.g)) * smoothstep(0.35, 0.6, lo.a);
    vec4 d1 = tri1(P, Nw, 1.3), d2 = tri1(P, Nw, 2.3), d3 = tri1(P, Nw, 7.0);
    float fineCr = (1.0 - smoothstep(0.02, 0.07, d1.g)) * smoothstep(0.45, 0.7, mid.r);
    float grain = d2.b, speck = d3.b;
    wA *= (0.78 + 0.44 * grain) * (0.92 + 0.16 * speck);
    wA *= 1.0 - crack * 0.6 - fineCr * 0.25;
    float top = smoothstep(0.55, 0.9, Nw.y) * smoothstep(0.25, 0.6, mid.a + 0.25);     // dust and sand lies on what faces up
    wA = mix(wA, vec3(0.40, 0.25, 0.145) * (0.8 + 0.4 * speck), top * 0.8);
    wR = 0.93 - 0.1 * top; wM = 0.0;
    wH = warp * 0.05 + bands * 0.014 + fineB * 0.006 + grain * 0.005 + speck * 0.0015 - crack * 0.014 - fineCr * 0.004;`,
};

/* Safety: where the scanned room is close to your head or hands, it thins out and the real room (passthrough) shows through. */
const SAFE = `
  { float sd = min(min(distance(vWP, uS0), distance(vWP, uS1)), distance(vWP, uS2));
    gl_FragColor *= 1.0 - 0.85 * uSafeOn * (1.0 - smoothstep(0.28, 0.62, sd)); }`;
const SAFE_HEAD = 'uniform float uSafeOn; uniform vec3 uS0; uniform vec3 uS1; uniform vec3 uS2;\n';

/* Grass (instanced blades, root at y = 0, tip at y = 1): a hand among the blades leans them away from it, the tips most. */
const TOUCH_VS = `
  #ifdef USE_INSTANCING
  { vec3 root = (modelMatrix * instanceMatrix * vec4(0.0, 0.0, 0.0, 1.0)).xyz; float bh = length((modelMatrix * instanceMatrix * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
    vec3 push = vec3(0.0);
    for (int k = 0; k < 2; k++) { vec3 hp = k == 0 ? uH0 : uH1; vec2 dd = root.xz - hp.xz + vec2(0.003, 0.001); float dh = length(dd), hy = hp.y - root.y;
      float infl = (1.0 - smoothstep(0.04, 0.27, dh)) * (1.0 - smoothstep(bh, bh + 0.12, hy)) * smoothstep(-0.2, -0.05, hy);
      push.xz += dd / dh * infl; push.y -= infl * 0.35; }
    push *= position.y * position.y * min(bh * 0.8, 0.34);
    mvPosition.xyz += mat3(viewMatrix) * push; gl_Position = projectionMatrix * mvPosition; vWP += push; }
  #endif`;

const QUAD_VS = `
  varying vec3 vDir;
  void main() {
    vec4 clipPos = vec4(position.xy, 0.9999, 1.0);
    vec4 v = inverse(projectionMatrix) * clipPos;
    vDir = transpose(mat3(viewMatrix)) * (v.xyz / v.w);
    gl_Position = clipPos;
  }`;
const SKY_HEAD = 'precision highp float; varying vec3 vDir; uniform float uTime; uniform float uBright; uniform sampler2D uNoise;\n';
const SKY = {
  groen: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 horizon = vec3(0.040, 0.080, 0.070), zenith = vec3(0.008, 0.020, 0.032);
    vec3 col = mix(horizon, zenith, smoothstep(-0.05, 0.85, h));
    vec2 q = d.xz / (abs(d.y) + 0.45);
    float m = texture2D(uNoise, q * 0.09 + uTime * 0.002).r * 0.65 + texture2D(uNoise, q * 0.27 - uTime * 0.003).a * 0.35;
    col += vec3(0.045, 0.115, 0.095) * pow(m, 2.2) * smoothstep(0.0, 0.5, h);
    float band = exp(-pow((h - 0.30) * 4.5, 2.0)) * pow(texture2D(uNoise, vec2(atan(d.x, d.z) / 6.2832 * 3.0, h * 0.4 + uTime * 0.004)).r, 2.0);
    col += vec3(0.0, 0.20, 0.14) * band * 0.3;
    if (h < 0.0) col = horizon * (0.9 + 0.1 * h);
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
  /* a hall so large it has weather: a ceiling of girders and strip lights far above, fading into haze */
  metal: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 haze = vec3(0.055, 0.065, 0.080);
    vec3 col = haze;
    if (h > 0.015) {
      vec2 uv = d.xz / h * 16.0; float dist = length(uv);
      vec2 g = uv / vec2(13.0, 9.0), f = fract(g) - 0.5;
      float panel = (1.0 - smoothstep(0.30, 0.32, abs(f.x))) * (1.0 - smoothstep(0.035, 0.05, abs(f.y)));
      float glow = (1.0 - smoothstep(0.30, 0.45, abs(f.x))) * exp(-abs(f.y) * 16.0);
      float girder = 1.0 - smoothstep(0.025, 0.04, min(0.5 - abs(f.x), 0.5 - abs(f.y)));
      vec3 ceil = vec3(0.030, 0.034, 0.042) * (0.6 + 0.8 * texture2D(uNoise, uv * 0.03).r) * (1.0 - 0.7 * girder);
      ceil += vec3(1.05, 1.15, 1.3) * panel + vec3(0.16, 0.19, 0.24) * glow;
      col = mix(haze, ceil, exp(-dist * 0.019));
    }
    float az = atan(d.x, d.z) / 6.2832;
    float wl = step(0.9, fract(az * 90.0)) * (1.0 - smoothstep(0.004, 0.012, abs(h - 0.02 - 0.012 * texture2D(uNoise, vec2(az * 4.0, 0.2)).r)));
    col += vec3(1.0, 0.55, 0.2) * wl * 0.5;                                  // a far-off rim of work lights
    col = mix(col, haze * 0.8, 1.0 - smoothstep(-0.25, 0.0, h));
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
  /* dusk over a canyon land: a low sun, thin high cloud, three ranges of mesas sinking into the haze */
  klippe: `void main() {
    vec3 d = normalize(vDir); float h = d.y;
    vec3 sunDir = normalize(vec3(0.55, 0.13, -0.82));
    vec3 zen = vec3(0.09, 0.19, 0.42), hor = vec3(0.90, 0.60, 0.40);
    float sd = max(dot(d, sunDir), 0.0);
    vec3 col = mix(hor, zen, pow(smoothstep(-0.02, 0.8, h), 0.55));
    col = mix(col, vec3(1.0, 0.70, 0.42), pow(sd, 5.0) * 0.55);
    vec2 q = d.xz / (max(h, 0.0) + 0.22);
    float c = texture2D(uNoise, q * vec2(0.030, 0.085) + uTime * 0.0006).r * 0.7 + texture2D(uNoise, q * vec2(0.09, 0.26)).a * 0.3;
    float cloud = smoothstep(0.50, 0.78, c) * smoothstep(0.03, 0.30, h);
    col = mix(col, mix(vec3(0.80, 0.52, 0.45), vec3(1.0, 0.86, 0.70), pow(sd, 3.0)), cloud * 0.6);
    col += vec3(1.0, 0.80, 0.55) * pow(sd, 220.0) * 1.2 + vec3(3.0, 2.4, 1.6) * smoothstep(0.99955, 0.9998, sd);
    float st = step(0.997, texture2D(uNoise, d.xz / (h + 0.4) * 2.1).b) * smoothstep(0.35, 0.8, h) * (1.0 - cloud);
    col += vec3(0.8) * st * 0.6;
    float az = atan(d.x, d.z) / 6.2832;
    for (int i = 0; i < 3; i++) {
      float fi = float(i);
      float n = texture2D(uNoise, vec2(az * (2.0 + fi * 2.0) + fi * 0.37, 0.21 + fi * 0.3)).r;
      float n2 = texture2D(uNoise, vec2(az * (12.0 + fi * 9.0), 0.63 + fi * 0.11)).a;
      float mh = 0.012 + fi * 0.012 + (0.035 + fi * 0.03) * smoothstep(0.42, 0.56, n) + (0.004 + fi * 0.004) * n2;
      vec3 mc = mix(vec3(0.34, 0.19, 0.14), hor * 0.82, 0.78 - fi * 0.27) * (1.0 + 0.35 * pow(sd, 4.0));
      col = mix(col, mc, 1.0 - smoothstep(mh - 0.002, mh + 0.002, h));
    }
    if (h < 0.0) col = mix(vec3(0.42, 0.27, 0.19), hor * 0.8, exp(h * 14.0));
    gl_FragColor = vec4(col * uBright, 1.0);
  }`,
  /* deep space; uSpin turns the whole sky (you tumble, it does not) */
  rum: SPACE_GLSL + `uniform mat3 uSpin; uniform vec4 uWarp; uniform float uHole;
    void main() {
      vec3 d = normalize(vDir); float dop = 1.0;
      if (uWarp.w > 0.002) {                                           // speed: the whole sky is swept forward (aberration), blue ahead and red behind
        vec3 a = uWarp.xyz; float b = uWarp.w, c = dot(d, a), c0 = (c - b) / (1.0 - b * c); vec3 pp = d - c * a;
        d = a * c0 + pp / max(length(pp), 1e-5) * sqrt(max(0.0, 1.0 - c0 * c0)); dop = sqrt(1.0 - b * b) / (1.0 - b * c); }
      const vec3 H = vec3(-0.2425, 0.1455, -0.9592); float glow = 0.0, dark = 1.0;
      if (uHole > 0.002) {                                             // mass: a black hole bends the light that passes it (a point lens), with a shadow and a ring of light
        float ch = clamp(dot(d, H), -1.0, 1.0), th = acos(ch), sh = uHole * 0.55; vec3 t = normalize(d - H * ch + 1e-6);
        float be = th - uHole * uHole / max(th, 1e-4); d = H * cos(be) + t * sin(be);
        dark = smoothstep(sh, sh * 1.07, th); float e = (th - sh * 1.12) / (sh * 0.08); glow = exp(-e * e); }
      vec3 col = spaceSky(uSpin * d) * dark;
      col *= (dop < 1.0 ? mix(vec3(1.2, 0.62, 0.42), vec3(1.0), smoothstep(0.35, 1.0, dop)) : mix(vec3(1.0), vec3(0.62, 0.86, 1.4), smoothstep(1.0, 2.4, dop))) * clamp(dop, 0.5, 2.0);
      col += vec3(1.0, 0.78, 0.5) * glow * 0.7;
      gl_FragColor = vec4(col, 1.0); }`,
};
const CLEAR_FS = 'void main() { gl_FragColor = vec4(0.0); }';

/* ------------------------------------------------------------------ build */

export function createWorlds(scene, renderer, params) {
  const noise = makeNoiseTexture();
  const timeU = { value: 0 }, brightU = { value: 1 }, noiseU = { value: noise }, spinU = { value: new THREE.Matrix3() }, warpU = { value: new THREE.Vector4(0, 0, -1, 0) }, holeU = { value: 0 }, travelU = { value: new V() };
  const handU = { uH0: { value: new V(0, -99, 0) }, uH1: { value: new V(0, -99, 0) } };      // the hands, for the grass that parts round them
  const safeU = { uSafeOn: { value: 0 }, uS0: { value: new V() }, uS1: { value: new V() }, uS2: { value: new V() } };
  const PASS = new V(0, 1, 0);

  const W = DIMS.map((d, i) => ({
    ...d, i, bare: !!(d.fract || d.space), group: new THREE.Group(), deco: new THREE.Group(), scan: new THREE.Group(), mats: [], tick: [], anchored: false,
    clip: new THREE.Plane(PASS.clone(), 1e6), skyHome: null, fill: null, env: null,
    uni: { uFogCol: { value: new THREE.Color(0, 0, 0) }, uFogDen: { value: 0 }, uGrade: { value: new V(1, 1, 1) } },
    base: { fog: new THREE.Color(0, 0, 0), den: 0, grade: new V(1, 1, 1) },
  }));
  for (const w of W) { w.group.add(w.deco, w.scan); scene.add(w.group); w.group.visible = false; }
  const look = (i, fog, den, grade) => { W[i].base.fog.setRGB(...fog); W[i].base.den = den; W[i].base.grade.set(...grade); W[i].uni.uFogDen.value = den; };
  look(1, [0.040, 0.080, 0.070], 0.17, [0.86, 1.0, 0.92]);
  look(2, [0.055, 0.065, 0.080], 0.06, [0.80, 0.85, 0.93]);
  look(3, [0.80, 0.56, 0.40], 0.022, [1.08, 1.0, 0.9]);

  /** Every material of a dimension is registered, so the dimension can be switched between home / seen-through-the-door. */
  function reg(i, m) {
    m.clippingPlanes = [W[i].clip];
    m.stencilWrite = false; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc;
    m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp;
    W[i].mats.push(m);
    return m;
  }

  /* reflections: a small light set per dimension, baked to an environment map */
  const pm = new THREE.PMREMGenerator(renderer);
  function bake(build) {
    const s = new THREE.Scene();
    const panel = (w, h, rgb, x, y, z) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ side: THREE.DoubleSide })); m.material.color.setRGB(...rgb); m.position.set(x, y, z); m.lookAt(0, 0, 0); s.add(m); };
    const room = rgb => { const m = new THREE.Mesh(new THREE.BoxGeometry(24, 24, 24), new THREE.MeshBasicMaterial({ side: THREE.BackSide })); m.material.color.setRGB(...rgb); s.add(m); };
    build(room, panel);
    return pm.fromScene(s, 0.03).texture;
  }
  W[1].env = bake((room, panel) => { room([0.012, 0.03, 0.026]); panel(10, 10, [0.10, 0.45, 0.36], 0, 10, 0); panel(4, 8, [0.05, 0.3, 0.25], -9, 0, 3); });
  W[2].env = bake((room, panel) => {
    room([0.02, 0.024, 0.03]);
    for (let k = -1; k <= 1; k++) { panel(9, 1.0, [4.2, 4.6, 5.3], k * 5, 10, -3); panel(9, 1.0, [4.2, 4.6, 5.3], k * 5, 10, 4); }
    panel(3, 6, [0.7, 0.8, 1.0], -11, 1, 0); panel(3, 6, [0.5, 0.56, 0.7], 11, 2, -2); panel(2.5, 1.2, [5, 1.6, 0.25], 5, -3, -10); panel(14, 14, [0.10, 0.11, 0.13], 0, -10, 0);
  });
  W[3].env = bake((room, panel) => {
    room([0.28, 0.30, 0.42]);
    panel(22, 22, [0.20, 0.36, 0.80], 0, 11, 0); panel(22, 7, [1.5, 0.95, 0.6], 0, 1.5, -11); panel(7, 22, [0.9, 0.6, 0.45], 11, 1.5, 0); panel(7, 22, [0.5, 0.38, 0.4], -11, 1.5, 0);
    panel(2.2, 2.2, [26, 17, 9], 6.2, 1.6, -9.2); panel(22, 22, [0.30, 0.19, 0.12], 0, -11, 0);
  });
  pm.dispose();

  function worldMat(i, surf, o = {}) {
    const m = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1, metalness: 0, side: o.side ?? THREE.DoubleSide, envMap: W[i].env, envMapIntensity: o.env ?? 1 });
    m.onBeforeCompile = sh => {
      Object.assign(sh.uniforms, W[i].uni, { uNoise: noiseU, uTime: timeU }, o.safe ? safeU : {}, o.touch ? handU : {});
      if (o.touch) sh.vertexShader = sh.vertexShader.replace('#include <project_vertex>', '#include <project_vertex>\n' + TOUCH_VS);
      sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nvarying vec3 vWP; varying vec3 vWN;' + (o.touch ? ' uniform vec3 uH0; uniform vec3 uH1;' : ''))
        .replace('#include <begin_vertex>', `#include <begin_vertex>
          #ifdef USE_INSTANCING
            vWP = (modelMatrix * instanceMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * mat3(instanceMatrix) * objectNormal);
          #else
            vWP = (modelMatrix * vec4(transformed, 1.0)).xyz; vWN = normalize(mat3(modelMatrix) * objectNormal);
          #endif`);
      sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\n' + HEAD + (o.safe ? SAFE_HEAD : ''))
        .replace('#include <color_fragment>', '#include <color_fragment>\nvec3 P = vWP; vec3 Nw = normalize(vWN); float wR = 0.8, wM = 0.0, wH = 0.0; vec3 wE = vec3(0.0), wA = vec3(0.5);\n' + surf + '\ndiffuseColor.rgb = wA;')
        .replace('#include <roughnessmap_fragment>', '#include <roughnessmap_fragment>\nroughnessFactor = clamp(wR, 0.05, 1.0);')
        .replace('#include <metalnessmap_fragment>', '#include <metalnessmap_fragment>\nmetalnessFactor = clamp(wM, 0.0, 1.0);')
        .replace('#include <normal_fragment_maps>', '#include <normal_fragment_maps>\nnormal = wBump(-vViewPosition, normal, wH);')
        .replace('#include <emissivemap_fragment>', '#include <emissivemap_fragment>\ntotalEmissiveRadiance += wE;')
        .replace('#include <fog_fragment>', 'gl_FragColor.rgb *= uGrade; { float fd = length(vViewPosition); gl_FragColor.rgb = mix(gl_FragColor.rgb, uFogCol, 1.0 - exp(-uFogDen * uFogDen * fd * fd)); }' + (o.safe ? SAFE : ''));
    };
    const k = 'w' + i + ':' + surf.length + ':' + surf.slice(0, 40) + (o.key || '');
    m.customProgramCacheKey = () => k;
    return reg(i, m);
  }

  /* skies: one quad drawn everywhere when the dimension is home, one drawn inside the door (which also resets the depth) */
  const quadGeo = new THREE.PlaneGeometry(2, 2);
  function quad(fs, inRing) {
    const m = new THREE.ShaderMaterial({ vertexShader: QUAD_VS, fragmentShader: fs, uniforms: { uTime: timeU, uBright: brightU, uNoise: noiseU, uSpin: spinU, uWarp: warpU, uHole: holeU }, blending: THREE.NoBlending, depthTest: true, depthWrite: inRing, depthFunc: inRing ? THREE.AlwaysDepth : THREE.LessEqualDepth });   // the home sky tests against the depth: pixels the world covers are never shaded
    if (inRing) { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; }
    const q = new THREE.Mesh(quadGeo, m); q.frustumCulled = false; q.renderOrder = inRing ? ORDER.fill : ORDER.bgHome; q.visible = false; scene.add(q);
    return q;
  }
  W[0].fill = quad(CLEAR_FS, true);                               // the real room seen through the door: alpha 0 = passthrough
  for (const w of W.slice(1)) if (!w.fract) { w.skyHome = quad(SKY_HEAD + SKY[w.key], false); w.fill = quad(SKY_HEAD + SKY[w.key], true); }

  /* ---- fractal dimensions: ray-marched into a small picture per eye (so the cost does not depend on the headset's resolution), then laid behind everything ---- */
  const hueU = { value: 0 };
  const fineU = { value: 0 }, fracTimeU = { value: 0 };           // the fractals' own clock: it slows to a stop while the picture is being built up
  const FR = { mode: 'taa', n: 0, hist: {}, prev: [null, null], acc: false, calm: false, top: 0.55, min: params.has('flat') ? 0.5 : 0.2, scale: params.has('flat') ? 0.5 : 0.42, ema: 1 / 72, slow: 0, fast: 0, rts: {}, scene: new THREE.Scene(), cam: new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1), flat: true, base: [1440, 1584] };
  const frMesh = new THREE.Mesh(quadGeo, null); frMesh.frustumCulled = false; FR.scene.add(frMesh);
  const target = (role, eye) => { const k = role + eye; if (!FR.rts[k]) { FR.rts[k] = new THREE.WebGLRenderTarget(8, 8, { depthBuffer: false, stencilBuffer: false, minFilter: THREE.LinearFilter, magFilter: THREE.LinearFilter, generateMipmaps: false }); } return FR.rts[k]; };
  /* A fractal that floats in space: where its ray went all the way out, the stars are drawn here, at the headset's full sharpness (the fractal's own picture is small). */
  const BLIT_STAR_VS = 'varying vec2 vUv; varying vec3 vDir; void main() { vUv = uv; vec4 cp = vec4(position.xy, 0.9999, 1.0); vec4 v = inverse(projectionMatrix) * cp; vDir = transpose(mat3(viewMatrix)) * (v.xyz / v.w); gl_Position = cp; }';
  const BLIT_STAR_FS = 'precision highp float; varying vec2 vUv; varying vec3 vDir; uniform sampler2D uTex; uniform float uYaw;' + STARS_GLSL + `
    void main() { vec4 t = texture2D(uTex, vUv); vec3 col = t.rgb; float sky = smoothstep(0.97, 1.0, t.a);
      if (sky > 0.0) { float c = cos(uYaw), s = sin(uYaw); vec3 d = normalize(vDir); col += starField(vec3(c * d.x - s * d.z, d.y, s * d.x + c * d.z), 0.25) * sky; }
      gl_FragColor = vec4(col, 1.0); }`;
  function blit(role, eye, stars) {
    const inRing = role === 'other';
    const m = new THREE.ShaderMaterial({ vertexShader: stars ? BLIT_STAR_VS : 'varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.9999, 1.0); }', fragmentShader: stars ? BLIT_STAR_FS : BLIT_FS, uniforms: { uTex: { value: target(role, eye).texture }, uYaw: { value: 0 } },
      blending: THREE.NoBlending, depthTest: true, depthWrite: inRing, depthFunc: inRing ? THREE.AlwaysDepth : THREE.LessEqualDepth });
    if (inRing) { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; }
    const q = new THREE.Mesh(quadGeo, m); q.frustumCulled = false; q.renderOrder = inRing ? ORDER.fill : ORDER.bgHome; q.userData.eye = eye; q.userData.role = role; return q;
  }
  const eyeQuads = [];
  for (const w of W) if (w.fract) {
    const f = FRACT[w.key];
    w.march = new THREE.ShaderMaterial({ vertexShader: MARCH_VS, fragmentShader: f.fs ? FOUR_FS : marchFS(f), depthTest: false, depthWrite: false,
      uniforms: { uHue: hueU, uR4: { value: new THREE.Matrix4() }, uC4: { value: new THREE.Vector4() }, uThru: { value: 0 }, uNoise: noiseU, uCam: { value: new V() }, uRot: { value: new THREE.Matrix3() }, uProj: { value: new THREE.Vector4() }, uAnchor: { value: new V(0, 0, 0) }, uYaw: { value: 0 }, uTime: fracTimeU, uBright: brightU, uS: { value: f.S }, uOff: { value: new V(...f.off) }, uParam: { value: f.par[0] }, uParam2: { value: f.par2[0] }, uFine: fineU, uJitUV: { value: new THREE.Vector2() } } });
    w.fr = { f, S: f.S, yaw: 0, off: new V(...f.off), param: f.par[0], param2: f.par2[0], A: new V(), R4: new THREE.Matrix4(), c4: new THREE.Vector4(0, 0, 0, 0), depth: 0, turn: 0 };
    for (const role of ['home', 'other']) { const g = new THREE.Group(); for (const eye of [0, 1]) { const q = blit(role, eye, !!f.stars); g.add(q); eyeQuads.push(q); } g.visible = false; scene.add(g); if (role === 'home') w.skyHome = g; else w.fill = g; }
  }
  const setEyes = flat => { FR.flat = flat; for (const q of eyeQuads) { if (flat) { q.layers.set(0); q.visible = q.userData.eye === 0; } else { q.layers.set(q.userData.eye + 1); q.visible = true; } } };
  setEyes(true);
  const _m4 = new THREE.Matrix4();
  /** Before the main picture: march the fractal(s) in view into their small targets, one per eye. views = [{ matrix (camera to world, 16), proj (16) }]. */
  const resolveMat = new THREE.ShaderMaterial({ vertexShader: MARCH_VS, fragmentShader: RESOLVE_FS, depthTest: false, depthWrite: false,
    uniforms: { uCur: { value: null }, uHist: { value: null }, uLow: { value: new THREE.Vector2() }, uHi: { value: new THREE.Vector2() }, uJit: { value: new THREE.Vector2() },
      uCam: { value: new V() }, uCamP: { value: new V() }, uRot: { value: new THREE.Matrix3() }, uRotP: { value: new THREE.Matrix3() }, uProj: { value: new THREE.Vector4() }, uProjP: { value: new THREE.Vector4() },
      uMaxT: { value: 1 }, uInvS: { value: 1 }, uReset: { value: 1 } } });
  const histRT = (eye, k) => { const key = eye + '/' + k; if (!FR.hist[key]) FR.hist[key] = new THREE.WebGLRenderTarget(8, 8, { depthBuffer: false, stencilBuffer: false, minFilter: THREE.LinearFilter, magFilter: THREE.LinearFilter, generateMipmaps: false }); return FR.hist[key]; };
  const halton = (i, b) => { let f = 1, r = 0; while (i > 0) { f /= b; r += f * (i % b); i = Math.floor(i / b); } return r; };
  function renderFractals(views, cur, oth, dt) {
    const jobs = [[cur, 'home'], [oth, 'other']].filter(([i]) => W[i] && W[i].fract);
    if (!jobs.length || !views.length) { FR.acc = false; return; }
    if ((views.length === 1) !== FR.flat) setEyes(views.length === 1);
    FR.ema += (dt - FR.ema) * 0.05;                                 // the frame-rate guard: slow frames shrink the picture, fast ones let it grow back
    const calm = FR.calm && FR.mode !== 'off', taa = calm && FR.mode === 'taa' && jobs[0][1] === 'home', grow = calm && FR.mode === 'res';
    /* 'res': standing still with your hands off the controls, the ceiling on the picture's size rises, a step at a time for as long as the frame rate holds.
     * 'taa': the picture stays small and cheap, and is built up over time instead (see RESOLVE_FS). Either way the rays are marched more finely, and the moment
     * you move or steer everything drops back. */
    if (grow) { FR.top = 1.0; if (FR.scale <= 0.55) FR.carried = FR.scale; }
    else { FR.top = 0.55; if (FR.scale > 0.55) FR.scale = FR.carried || 0.42; }
    fineU.value += ((calm ? 1 : 0) - fineU.value) * Math.min(1, dt * (calm ? 0.8 : 10));
    fracTimeU.value += dt * (taa ? Math.max(0, 1 - fineU.value * 1.6) : 1);                                 // the fractal holds its breath while it sharpens
    if (FR.ema > 1 / 58) { FR.slow += dt; FR.fast = 0; if (FR.slow > (grow && FR.scale > 0.55 ? 0.35 : 1)) { FR.slow = 0; FR.scale = Math.max(FR.min, FR.scale * 0.85); if (grow) FR.best = Math.min(FR.best || 1, FR.scale); } }
    else if (FR.ema < 1 / 70) { FR.fast += dt; FR.slow = 0; if (FR.fast > (grow ? 0.6 : 4)) { FR.fast = 0; FR.scale = Math.min(grow ? Math.min(FR.top, FR.best || 1) : FR.top, FR.scale * (grow ? 1.12 : 1.08)); } } else { FR.slow = 0; FR.fast = 0; }
    const size = renderer.getDrawingBufferSize(new THREE.Vector2()), bw = views.length === 1 ? size.x : FR.base[0], bh = views.length === 1 ? size.y : FR.base[1];
    const tw = Math.max(64, Math.round(bw * FR.scale)), th = Math.max(64, Math.round(bh * FR.scale));
    const xrWas = renderer.xr.enabled, rtWas = renderer.getRenderTarget(); renderer.xr.enabled = false;
    const reset = taa && !FR.acc; FR.n = taa ? FR.n + 1 : 0;
    const jx = taa ? halton(FR.n % 16 + 1, 2) - 0.5 : 0, jy = taa ? halton(FR.n % 16 + 1, 3) - 0.5 : 0;     // this frame's sub-pixel offset
    for (const [i, role] of jobs) {
      const w = W[i], U = w.march.uniforms, home = role === 'home';
      views.forEach((v, eye) => {
        const rt = target(role, eye), sc = (home ? 1 : 0.8) * (w.fr.f.res || 1), lw = Math.min(bw, Math.round(tw * sc)), lh = Math.min(bh, Math.round(th * sc)); U.uThru.value = home && !FR.thru ? 0 : 1; if (rt.width !== lw || rt.height !== lh) rt.setSize(lw, lh);
        const m = v.matrix, p = v.proj; _m4.fromArray(m); U.uRot.value.setFromMatrix4(_m4); U.uCam.value.set(m[12], m[13], m[14]);
        U.uProj.value.set((p[8] - 1) / p[0], (p[8] + 1) / p[0], (p[9] - 1) / p[5], (p[9] + 1) / p[5]);
        U.uJitUV.value.set(home && taa ? jx / lw : 0, home && taa ? jy / lh : 0);
        frMesh.material = w.march; renderer.setRenderTarget(rt); renderer.render(FR.scene, FR.cam);
        const quad = eyeQuads.find(q => q.userData.role === role && q.userData.eye === eye && q.parent === (home ? w.skyHome : w.fill));
        if (quad) quad.material.uniforms.uYaw.value = w.fr.yaw;
        if (home && taa) {
          const hw = Math.min(bw, Math.round(lw * 2.2)), hh = Math.min(bh, Math.round(lh * 2.2)), a = histRT(eye, FR.n & 1), b = histRT(eye, (FR.n + 1) & 1);
          for (const h of [a, b]) if (h.width !== hw || h.height !== hh) { h.setSize(hw, hh); FR.prev[eye] = null; }
          const R = resolveMat.uniforms, pv = FR.prev[eye];
          R.uCur.value = rt.texture; R.uHist.value = b.texture; R.uLow.value.set(lw, lh); R.uHi.value.set(hw, hh); R.uJit.value.set(jx, jy);
          R.uCam.value.copy(U.uCam.value); R.uRot.value.copy(U.uRot.value); R.uProj.value.copy(U.uProj.value); R.uMaxT.value = w.fr.f.maxT; R.uInvS.value = 1 / w.fr.S;
          R.uReset.value = reset || !pv ? 1 : 0; if (pv) { R.uCamP.value.copy(pv.cam); R.uRotP.value.copy(pv.rot); R.uProjP.value.copy(pv.proj); }
          frMesh.material = resolveMat; renderer.setRenderTarget(a); renderer.render(FR.scene, FR.cam);
          FR.prev[eye] = { cam: U.uCam.value.clone(), rot: U.uRot.value.clone(), proj: U.uProj.value.clone() };
          if (quad) quad.material.uniforms.uTex.value = a.texture;
        } else if (quad) quad.material.uniforms.uTex.value = rt.texture;
      });
    }
    FR.acc = taa; if (!taa) FR.prev[0] = FR.prev[1] = null;
    renderer.setRenderTarget(rtWas); renderer.xr.enabled = xrWas;
  }

  const ghostU = { value: 1 };
  const _f1 = new V(), _f2 = new V(), _v4a = new THREE.Vector4(), _v4b = new THREE.Vector4(), _g4 = new THREE.Matrix4();
  /** Where a point of the room is in the fractal's own space (the same sum the shader does). */
  function toF(w, p, out) { const r = w.fr, c = Math.cos(r.yaw), s = Math.sin(r.yaw), x = p.x - r.A.x, y = p.y - r.A.y, z = p.z - r.A.z; return out.set((c * x - s * z) * r.S + r.off.x, y * r.S + r.off.y, (s * x + c * z) * r.S + r.off.z); }
  function syncFr(w) { const U = w.march.uniforms; U.uAnchor.value.copy(w.fr.A); U.uYaw.value = w.fr.yaw; U.uS.value = w.fr.S; U.uOff.value.copy(w.fr.off); U.uParam.value = w.fr.param; U.uParam2.value = w.fr.param2; U.uR4.value.copy(w.fr.R4); U.uC4.value.copy(w.fr.c4); }
  /** The hyperbolic world ends at a horizon: you cannot fly out through it. */
  function keepIn(w, h) { if (w.fr.f.ball) { toF(w, h, _f1); const l = _f1.length(); if (l > w.fr.f.ball) w.fr.off.addScaledVector(_f1, w.fr.f.ball / l - 1); } syncFr(w); }

  /* the room in a fractal dimension: its scanned shape as a faint outline, and close to you it opens onto the real room */
  const ghostMat = new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, side: THREE.DoubleSide, uniforms: { uTime: timeU, uGhost: ghostU },
    blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneFactor, blendSrcAlpha: THREE.ZeroFactor, blendDstAlpha: THREE.OneFactor,
    vertexShader: 'varying vec3 vN; varying vec3 vV; void main() { vec4 wp = modelMatrix * vec4(position, 1.0); vN = normalize(mat3(modelMatrix) * normal); vV = cameraPosition - wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
    fragmentShader: 'precision highp float; varying vec3 vN; varying vec3 vV; uniform float uGhost; void main() { float rim = pow(1.0 - abs(dot(normalize(vN), normalize(vV))), 2.0); float near = 1.0 - smoothstep(1.0, 4.5, length(vV)); gl_FragColor = vec4(vec3(0.55, 0.85, 1.0) * (0.035 + rim * 0.2) * near * uGhost, 0.0); }',
  });
  const holeMat = new THREE.ShaderMaterial({                         // multiplies what is already drawn (colour and alpha) by 1 - a: the passthrough shows where a is large
    transparent: true, depthWrite: false, side: THREE.DoubleSide, uniforms: safeU,
    blending: THREE.CustomBlending, blendSrc: THREE.ZeroFactor, blendDst: THREE.OneMinusSrcAlphaFactor, blendSrcAlpha: THREE.ZeroFactor, blendDstAlpha: THREE.OneMinusSrcAlphaFactor,
    vertexShader: 'varying vec3 vP; void main() { vec4 wp = modelMatrix * vec4(position, 1.0); vP = wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
    fragmentShader: 'precision highp float; varying vec3 vP; uniform float uSafeOn; uniform vec3 uS0; uniform vec3 uS1; uniform vec3 uS2; void main() { float sd = min(min(distance(vP, uS0), distance(vP, uS1)), distance(vP, uS2)); float a = 0.85 * uSafeOn * (1.0 - smoothstep(0.28, 0.62, sd)); if (a < 0.01) discard; gl_FragColor = vec4(0.0, 0.0, 0.0, a); }',
  });
  ghostMat.customProgramCacheKey = () => 'ghost'; holeMat.customProgramCacheKey = () => 'hole';

  const floorOf = (i, mat) => { const f = new THREE.Mesh(new THREE.PlaneGeometry(80, 80), mat); f.rotation.x = -Math.PI / 2; f.position.y = -0.012; W[i].group.add(f); };
  function motes(i, count, color, size, opacity, box, rise) {
    const dot = document.createElement('canvas'); dot.width = dot.height = 64; const dx = dot.getContext('2d');
    const g = dx.createRadialGradient(32, 32, 0, 32, 32, 32); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.4, 'rgba(255,255,255,.35)'); g.addColorStop(1, 'rgba(255,255,255,0)');
    dx.fillStyle = g; dx.fillRect(0, 0, 64, 64);
    const A = new Float32Array(count * 3); for (let k = 0; k < count; k++) { A[k * 3] = rnd(-box[0], box[0]); A[k * 3 + 1] = rnd(0.1, box[1]); A[k * 3 + 2] = -rnd(-1.5, box[2]); }
    const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(A, 3));
    const p = new THREE.Points(geo, reg(i, new THREE.PointsMaterial({ size, map: new THREE.CanvasTexture(dot), color, transparent: true, opacity, depthWrite: false, blending: THREE.AdditiveBlending })));
    p.frustumCulled = false; W[i].deco.add(p);
    W[i].tick.push((dt, t) => { for (let k = 0; k < count; k++) { A[k * 3 + 1] += (rise + rise * 0.6 * Math.sin(t * 0.7 + k)) * dt; A[k * 3] += Math.sin(t * 0.3 + k * 1.7) * 0.02 * dt; if (A[k * 3 + 1] > box[1]) A[k * 3 + 1] = 0.1; }
      if (HLw === i) for (let h = 0; h < 2; h++) { if (!HLon[h]) continue; const hp = HL[h];        // the dust in the air swirls away from a hand
        for (let k = 0; k < count; k++) { const dx = A[k * 3] - hp.x, dy = A[k * 3 + 1] - hp.y, dz = A[k * 3 + 2] - hp.z, d2 = dx * dx + dy * dy + dz * dz; if (d2 < 0.05 && d2 > 1e-6) { const d = Math.sqrt(d2), f = (0.224 - d) / d * Math.min(1, dt * 9); A[k * 3] += dx * f; A[k * 3 + 1] += dy * f; A[k * 3 + 2] += dz * f; } } }
      geo.attributes.position.needsUpdate = true; });
  }

  /* ---- things that give when a hand passes ----
   * A rod is a thin stiff thing hinged at its foot (a stalk, a straw) or hung from its top (a cable). It has a lean (sideways metres per metre of length), a spring
   * that pulls the lean back to rest and a little friction. A hand is a ball: where it overlaps the rod at the hand's own height, the rod is moved out of the hand
   * and takes on the hand's speed, so a slow hand bends it aside and a quick one sends it swinging. All in the dimension's own (door-anchored) coordinates. */
  const HL = [new V(), new V()], HV = [new V(), new V()], HLon = [false, false], HLprev = [new V(), new V()]; let HLw = -1, headRef = new V(0, 1.6, 0);
  const rods = [], UP = new V(0, 1, 0), DOWN = new V(0, -1, 0), _rv = new V(), _rq = new THREE.Quaternion();
  const fire = (kind, strength, hand) => { if (api.onTouch) api.onTouch(kind, strength, hand); };
  function makeRod(i, g, o) { const r = { i, g, host: o.host || g, len: o.len, hang: !!o.hang, cq: o.cq || 0, rx: o.rx || 0, rz: o.rz || 0, k: o.k, c: o.c, lx: 0, lz: 0, vx: 0, vz: 0, sw: o.sw || 0, ph: rnd(0, TAU), R: o.R || 0.06, kind: o.kind, hit: 0, max: o.max || 1.1 }; rods.push(r); return r; }
  function rodsUpdate(dt, t, cur) {
    for (const r of rods) {
      if (r.i !== cur || !r.host.visible) continue;
      r.vx += (-r.k * r.lx - r.c * r.vx) * dt; r.vz += (-r.k * r.lz - r.c * r.vz) * dt; r.lx += r.vx * dt; r.lz += r.vz * dt; r.hit -= dt;
      const bx = r.g.position.x + (r.host !== r.g ? r.host.position.x : 0), by = r.g.position.y + (r.host !== r.g ? r.host.position.y : 0), bz = r.g.position.z + (r.host !== r.g ? r.host.position.z : 0);
      if (HLw === cur) for (let h = 0; h < 2; h++) {
        if (!HLon[h]) continue;
        const hp = HL[h], y = r.hang ? by - hp.y : hp.y - by; if (y < 0.04 || y > r.len + 0.04) continue;
        const u = Math.min(1, y / r.len), cx = bx + r.cq * u * u * r.len + (r.rx + r.lx) * y, cz = bz + (r.rz + r.lz) * y;
        let dx = hp.x - cx, dz = hp.z - cz, d = Math.hypot(dx, dz); if (d >= r.R) continue;
        if (d < 1e-4) { dx = -HV[h].x; dz = -HV[h].z; d = Math.hypot(dx, dz); if (d < 1e-4) { dx = 1; dz = 0; d = 1; } }
        const nx = dx / d, nz = dz / d, lever = Math.max(y, 0.15), dl = (r.R - Math.hypot(hp.x - cx, hp.z - cz)) / lever;
        r.lx -= nx * dl; r.lz -= nz * dl;
        const hv = -(HV[h].x * nx + HV[h].z * nz), rv = -(r.vx * nx + r.vz * nz) * lever;       // how fast the hand comes on, and how fast the rod already gets out of the way
        if (hv > rv) { const dv = (hv - rv) / lever; r.vx -= nx * dv; r.vz -= nz * dv; }
        if (r.hit <= 0) fire(r.kind, Math.min(1, Math.max(0, hv) / 1.2), h); r.hit = 0.3;
      }
      const l = Math.hypot(r.lx, r.lz); if (l > r.max) { r.lx *= r.max / l; r.lz *= r.max / l; r.vx *= 0.5; r.vz *= 0.5; }
      const sx = Math.sin(t * 0.5 + r.ph) * r.sw, sz = Math.cos(t * 0.4 + r.ph) * r.sw * 0.7;
      _rv.set(r.rx + r.lx + sx, r.hang ? -1 : 1, r.rz + r.lz + sz).normalize(); r.g.quaternion.setFromUnitVectors(r.hang ? DOWN : UP, _rv);
    }
  }
  /** Put a dimension's touchable things where you can actually get at them: on free floor you can walk to, near the door but out of the doorway. */
  function placeTouch(w) {
    if (!w.touch) return; w.deco.updateMatrixWorld(true); const used = [];
    const ok = (x, z) => { for (const u of used) if (Math.hypot(u[0] - x, u[1] - z) < 0.45) return false; if (!grid.size) return true;
      _p.set(x, 0, z).applyMatrix4(w.deco.matrixWorld); const px = _p.x, pz = _p.z, top = columnTop(px, pz, 1.9); return (top === null || top < 0.14) && reachable(headRef, px, pz, 0.3); };
    for (const tc of w.touch) {
      let x = tc.x, z = tc.z, good = ok(x, z);
      for (let k = 0; k < 40 && !good; k++) { x = rnd(-2.3, 2.3); z = rnd(-3.0, 1.6); if (Math.abs(x) < 0.5 && z > -1.0 && z < 0.7) continue; good = ok(x, z); }
      tc.host.visible = good; if (good) { tc.host.position.x = x; tc.host.position.z = z; used.push([x, z]); }
    }
  }
  const touchAt = (i, host, x, z) => { host.position.x = x; host.position.z = z; (W[i].touch = W[i].touch || []).push({ host, x, z }); W[i].deco.add(host); return host; };

  /* ---- 0: the real room. Only the laptop preview gets something to look at. ---- */
  if (params.has('flat')) {
    const c = document.createElement('canvas'); c.width = c.height = 256; const x = c.getContext('2d');
    x.fillStyle = '#1b2230'; x.fillRect(0, 0, 256, 256); x.strokeStyle = '#33445e'; x.lineWidth = 4; x.strokeRect(2, 2, 252, 252);
    const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(60, 60); t.colorSpace = THREE.SRGBColorSpace;
    const g = new THREE.Mesh(new THREE.PlaneGeometry(30, 30), reg(0, new THREE.MeshBasicMaterial({ map: t, side: THREE.DoubleSide })));
    g.rotation.x = -Math.PI / 2; g.position.y = -0.005; W[0].group.add(g);
    const orange = reg(0, new THREE.MeshLambertMaterial({ color: 0xff9a3c, emissive: 0x7a3a08, flatShading: true }));
    for (let k = 0; k < 6; k++) { const h = rnd(0.5, 1.5), p = new THREE.Mesh(new THREE.BoxGeometry(0.16, h, 0.16), orange); p.position.set(rnd(-1.3, 1.3), h / 2, rnd(0.6, 2.6)); W[0].deco.add(p); }
  }

  /* ---- 1: the green one ---- */
  {
    const i = 1, D = W[i].deco;
    const ground = worldMat(i, SURF.moss(0.5), { env: 0.9 });
    floorOf(i, ground); W[i].scanMat = worldMat(i, SURF.moss(0.8), { env: 0.9, key: 's', safe: true });
    const stalkM = worldMat(i, SURF.stalk(), { env: 0.5 }), budM = worldMat(i, SURF.bud(), { env: 0.5 }), barkM = worldMat(i, SURF.bark(), { env: 0.6 });
    const capM = worldMat(i, SURF.cap(), { env: 0.6 }), stemM = worldMat(i, SURF.stem(), { env: 0.5 }), bladeM = worldMat(i, SURF.blade(), { env: 0.4, touch: true, key: 't' });
    /* mossy stones */
    const stones = [rockGeometry(3, 11.3), rockGeometry(3, 5.1)], spots = [];
    const stone = (x, z, s) => { const m = new THREE.Mesh(stones[(Math.random() * 2) | 0], ground); m.scale.set(s * rnd(0.9, 1.3), s * rnd(0.55, 0.85), s * rnd(0.9, 1.3)); m.position.set(x, 0.3 * m.scale.y, z); m.rotation.y = rnd(0, TAU); D.add(m); spots.push([x, z, s]); };
    stone(-1.5, -2.4, 0.5); stone(1.7, -2.9, 0.75); stone(-2.8, -4.4, 1.0); stone(0.5, -5.2, 0.6); stone(3.2, -1.3, 0.4); stone(-3.4, -1.0, 0.55); stone(2.6, -5.8, 1.2); stone(-0.8, -7.4, 1.4); stone(-2.2, 1.7, 0.45); stone(2.4, 2.2, 0.6);
    /* roots creeping over the ground from the stones */
    for (let k = 0; k < 12; k++) {
      const [sx, sz, ss] = spots[k % spots.length]; let a = rnd(0, TAU), x = sx + Math.cos(a) * ss * 0.7, z = sz + Math.sin(a) * ss * 0.7; const pts = [new V(x, ss * 0.25, z)];
      const n = 6 + ((Math.random() * 5) | 0); for (let j = 0; j < n; j++) { a += rnd(-0.6, 0.6); x += Math.cos(a) * rnd(0.2, 0.4); z += Math.sin(a) * rnd(0.2, 0.4); pts.push(new V(x, j === 0 ? 0.05 : rnd(0.005, 0.045), z)); }
      D.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), n * 6, rnd(0.014, 0.034), 7, false), barkM));
    }
    /* mushrooms in clusters by the stones: a cap that glows faintly from underneath */
    const capPts = []; for (let j = 0; j <= 10; j++) { const t = j / 10; capPts.push(new THREE.Vector2(Math.sin(t * Math.PI / 2) * 1.0, Math.cos(t * Math.PI / 2) * 0.55)); } capPts.push(new THREE.Vector2(0.92, -0.05), new THREE.Vector2(0.12, 0.04));
    const capG = new THREE.LatheGeometry(capPts, 18), stemG = new THREE.CylinderGeometry(0.11, 0.16, 1, 9); stemG.translate(0, 0.5, 0);
    for (let c = 0; c < 9; c++) {
      const [sx, sz, ss] = spots[c % spots.length], a0 = rnd(0, TAU), cx = sx + Math.cos(a0) * (ss * 0.75 + 0.1), cz = sz + Math.sin(a0) * (ss * 0.75 + 0.1);
      for (let k = 0; k < 3 + ((Math.random() * 4) | 0); k++) {
        const r = rnd(0.025, 0.075), h = r * rnd(1.4, 2.6), g = new THREE.Group();
        const st = new THREE.Mesh(stemG, stemM); st.scale.set(r, h, r); g.add(st);
        const cp = new THREE.Mesh(capG, capM); cp.scale.set(r, r * rnd(0.8, 1.2), r); cp.position.y = h; g.add(cp);
        g.position.set(cx + rnd(-0.14, 0.14), 0, cz + rnd(-0.14, 0.14)); g.rotation.set(rnd(-0.2, 0.2), rnd(0, TAU), rnd(-0.2, 0.2)); D.add(g);
      }
    }
    /* tufts of grass */
    const bg = new THREE.PlaneGeometry(0.014, 1, 1, 4); bg.translate(0, 0.5, 0);
    { const p = bg.attributes.position; for (let k = 0; k < p.count; k++) { const y = p.getY(k); p.setZ(k, y * y * 0.35); p.setX(k, p.getX(k) * (1 - y * 0.85)); } bg.computeVertexNormals(); }
    const NB = 520, blades = new THREE.InstancedMesh(bg, bladeM, NB), m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), sc = new V(), ps = new V();
    for (let k = 0, t = 0; k < NB; t++) { const tx = rnd(-4.5, 4.5), tz = rnd(-8, 3), cnt = 10 + ((Math.random() * 14) | 0); for (let j = 0; j < cnt && k < NB; j++, k++) { e.set(rnd(-0.25, 0.25), rnd(0, TAU), rnd(-0.25, 0.25)); q.setFromEuler(e); sc.set(rnd(0.8, 1.6), rnd(0.07, 0.22), 1); ps.set(tx + rnd(-0.12, 0.12), 0, tz + rnd(-0.12, 0.12)); blades.setMatrixAt(k, m4.compose(ps, q, sc)); } }
    blades.frustumCulled = false; D.add(blades);
    /* a few tall thin stalks with a dim bud */
    const stalks = [];
    for (let k = 0; k < 7; k++) {
      const d = rnd(1.6, 5), a = rnd(-1.2, 1.2), h = rnd(0.7, 1.7), lean = rnd(-0.3, 0.3), g = new THREE.Group();
      const pts = []; for (let j = 0; j <= 5; j++) { const t = j / 5; pts.push(new V(lean * t * t * h + rnd(-0.012, 0.012) * j, t * h, rnd(-0.012, 0.012) * j)); }
      g.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 20, rnd(0.005, 0.01), 6, false), stalkM));
      const bud = new THREE.Mesh(new THREE.SphereGeometry(rnd(0.014, 0.026), 12, 9), budM); bud.position.copy(pts[5]); bud.scale.y = 1.7; g.add(bud);
      g.position.set(Math.sin(a) * d, 0, -Math.cos(a) * d); D.add(g); stalks.push(g);
      makeRod(i, g, { len: h, cq: lean, k: 30, c: 1.3, sw: rnd(0.02, 0.05), kind: 'stalk' });
    }
    /* and within reach of the door: stalks to brush aside, and grass tall enough to run a hand through */
    const near = [[-0.85, -0.9], [0.95, -1.35], [-1.15, 0.75], [1.25, 0.65], [0.3, -2.1], [-0.55, -1.75]];
    for (const [x, z] of near) {
      const host = touchAt(i, new THREE.Group(), x, z), n = 2 + ((Math.random() * 2) | 0);
      for (let k = 0; k < n; k++) {
        const h = rnd(0.95, 1.5), g = new THREE.Group(), geo = new THREE.CylinderGeometry(0.004, 0.008, h, 6, 5); geo.translate(0, h / 2, 0);
        g.add(new THREE.Mesh(geo, stalkM)); const bud = new THREE.Mesh(new THREE.SphereGeometry(rnd(0.016, 0.026), 12, 9), budM); bud.position.y = h; bud.scale.y = 1.7; g.add(bud);
        g.position.set(rnd(-0.08, 0.08), 0, rnd(-0.08, 0.08)); host.add(g);
        makeRod(i, g, { host, len: h, rx: rnd(-0.12, 0.12), rz: rnd(-0.12, 0.12), k: 34, c: 1.2, sw: rnd(0.015, 0.035), kind: 'stalk' });
      }
    }
    for (const [x, z] of [[0.78, -0.55], [-0.95, -1.5], [1.45, -0.3], [-1.45, -0.15], [0.2, -2.7], [1.0, 1.15], [-0.9, 1.35], [-0.2, -1.45]]) {
      const NT = 26, tuft = new THREE.InstancedMesh(bg, bladeM, NT);
      for (let k = 0; k < NT; k++) { e.set(rnd(-0.22, 0.22), rnd(0, TAU), rnd(-0.22, 0.22)); q.setFromEuler(e); sc.set(rnd(1.4, 2.6), rnd(0.35, 0.85), 1.6); const a = rnd(0, TAU), r = Math.sqrt(Math.random()) * 0.16; ps.set(Math.cos(a) * r, 0, Math.sin(a) * r); tuft.setMatrixAt(k, m4.compose(ps, q, sc)); }
      tuft.frustumCulled = false; touchAt(i, tuft, x, z);
    }
    motes(i, 110, 0xa8ffe8, 0.022, 0.35, [4, 2.6, 7], 0.02);
  }

  /* ---- 2: metal ---- */
  {
    const i = 2, D = W[i].deco;
    const plate = worldMat(i, SURF.plate(), { env: 0.85 }), steel = worldMat(i, SURF.steel(), { env: 0.85 }), copper = worldMat(i, SURF.copper(), { env: 0.9 });
    const lamp = worldMat(i, SURF.lamp()), ember = worldMat(i, SURF.ember());
    floorOf(i, plate); W[i].scanMat = worldMat(i, SURF.plate(), { env: 0.85, key: 's', safe: true });
    const add = (geo, mat, x, y, z, rx = 0, ry = 0, rz = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.rotation.set(rx, ry, rz); D.add(m); return m; };
    const colG = new THREE.CylinderGeometry(0.26, 0.26, 9, 28, 1, true), baseG = new THREE.CylinderGeometry(0.42, 0.46, 0.34, 28), collarG = new THREE.CylinderGeometry(0.31, 0.31, 0.12, 28);
    const beamG = new THREE.BoxGeometry(8.4, 0.42, 0.3), flangeG = new THREE.BoxGeometry(8.4, 0.05, 0.46), lampG = new THREE.BoxGeometry(2.2, 0.05, 0.22), hoodG = new THREE.BoxGeometry(2.4, 0.08, 0.36);
    for (let r = 0; r < 5; r++) {
      const z = -1.2 - r * 3.6;
      for (const s of [-1, 1]) { add(colG, steel, s * 3.4, 4.5, z); add(baseG, steel, s * 3.4, 0.17, z); add(collarG, steel, s * 3.4, 2.6, z); add(collarG, steel, s * 3.4, 6.2, z); }
      add(beamG, steel, 0, 6.6, z); add(flangeG, steel, 0, 6.83, z); add(flangeG, steel, 0, 6.37, z);
      add(hoodG, steel, 0, 6.3, z); add(lampG, lamp, 0, 6.24, z);
    }
    const pipeG = new THREE.CylinderGeometry(0.085, 0.085, 17, 20, 1, true), pipe2G = new THREE.CylinderGeometry(0.05, 0.05, 17, 16, 1, true), ringG = new THREE.TorusGeometry(0.105, 0.028, 10, 22), brG = new THREE.BoxGeometry(0.06, 0.5, 0.2);
    for (const s of [-1, 1]) {
      add(pipeG, copper, s * 3.85, 1.05, -8, Math.PI / 2); add(pipeG, steel, s * 3.85, 1.42, -8, Math.PI / 2); add(pipe2G, copper, s * 3.9, 1.72, -8, Math.PI / 2);
      for (let k = 0; k < 9; k++) { add(ringG, steel, s * 3.85, 1.05, -0.4 - k * 1.9); add(ringG, steel, s * 3.85, 1.42, -1.3 - k * 1.9); if (k % 2 === 0) add(brG, steel, s * 3.96, 1.25, -0.9 - k * 1.9); }
    }
    /* a console block and a valve near the door */
    const bx = add(new THREE.BoxGeometry(0.72, 1.05, 0.46), plate, 1.75, 0.525, -2.3, 0, -0.35); bx.add(new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.035, 0.02), ember)); bx.children[0].position.set(0, 0.3, 0.235);
    const top = new THREE.Mesh(new THREE.BoxGeometry(0.78, 0.04, 0.52), steel); top.position.y = 0.545; bx.add(top);
    const valve = new THREE.Group(); valve.position.y = 0.95; touchAt(i, valve, -1.25, -1.9);
    const stem = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.06, 0.95, 18), steel); stem.position.y = -0.475; valve.add(stem);
    const wheel = new THREE.Group(); wheel.position.y = 0.06; wheel.rotation.x = Math.PI / 2 - 0.5; valve.add(wheel);
    wheel.add(new THREE.Mesh(new THREE.TorusGeometry(0.16, 0.017, 10, 30), copper));
    for (let k = 0; k < 3; k++) { const sp = new THREE.Mesh(new THREE.CylinderGeometry(0.011, 0.011, 0.32, 8), copper); sp.rotation.z = k * Math.PI / 3; wheel.add(sp); }
    /* a great slow wheel far down the hall */
    const gear = new THREE.Group(); gear.position.set(0, 3.4, -17.5); D.add(gear);
    gear.add(new THREE.Mesh(new THREE.TorusGeometry(2.6, 0.3, 10, 48), steel));
    gear.add(new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 0.7, 24).rotateX(Math.PI / 2), steel));
    for (let k = 0; k < 6; k++) { const sp = new THREE.Mesh(new THREE.BoxGeometry(0.22, 5.2, 0.16), steel); sp.rotation.z = k * Math.PI / 6; gear.add(sp); }
    for (let k = 0; k < 24; k++) { const a = k / 24 * TAU, tth = new THREE.Mesh(new THREE.BoxGeometry(0.34, 0.5, 0.5), steel); tth.position.set(Math.cos(a) * 3.05, Math.sin(a) * 3.05, 0); tth.rotation.z = a + Math.PI / 2; gear.add(tth); }
    /* the valve wheel turns under a hand: the hand's speed along the rim becomes the wheel's, and it runs on until its friction stops it */
    const VW = { w: 0, a: 0, tick: 0, ax: new V(0, -Math.sin(Math.PI / 2 - 0.5), Math.cos(Math.PI / 2 - 0.5)), c: new V(), rel: new V(), tan: new V() };
    W[i].tick.push((dt, t) => {
      gear.rotation.z = t * 0.05;
      if (HLw === i && valve.visible) for (let h = 0; h < 2; h++) {
        if (!HLon[h]) continue;
        VW.c.copy(valve.position); VW.c.y += 0.06; VW.rel.copy(HL[h]).sub(VW.c); const ha = VW.rel.dot(VW.ax); VW.rel.addScaledVector(VW.ax, -ha); const rr = VW.rel.length();
        if (Math.abs(ha) < 0.08 && rr > 0.07 && rr < 0.26) { VW.tan.crossVectors(VW.ax, VW.rel).multiplyScalar(1 / rr); VW.w += (clamp(HV[h].dot(VW.tan) / rr, -14, 14) - VW.w) * Math.min(1, dt * 16); VW.hand = h; }
      }
      VW.w *= Math.exp(-dt * 0.8); if (Math.abs(VW.w) < 0.02) VW.w = 0; VW.a += VW.w * dt; wheel.rotation.z = VW.a;
      const tk = Math.floor(VW.a / (Math.PI / 9)); if (tk !== VW.tick) { VW.tick = tk; fire('valve', Math.min(1, Math.abs(VW.w) / 8), VW.hand || 0); }
    });
    /* cables with a hook, hanging out of the haze: push one and it swings, slowly, for a long time */
    const cabG = new THREE.CylinderGeometry(0.005, 0.005, 1, 6); cabG.translate(0, -0.5, 0);
    for (const [x, z, hy] of [[-0.9, -0.85, 1.32], [0.95, -1.45, 1.12], [-1.35, 0.65, 1.45], [1.35, 0.95, 1.25]]) {
      const topY = 9, L = topY - hy, g = new THREE.Group(); g.position.y = topY;
      const cab = new THREE.Mesh(cabG, steel); cab.scale.y = L - 0.1; g.add(cab);
      const wt = new THREE.Mesh(new THREE.CylinderGeometry(0.03, 0.022, 0.11, 14), steel); wt.position.y = -(L - 0.1) - 0.045; g.add(wt);
      const hook = new THREE.Mesh(new THREE.TorusGeometry(0.034, 0.008, 8, 18, Math.PI * 1.45), copper); hook.position.y = -L - 0.035; hook.rotation.z = Math.PI * 0.8; g.add(hook);
      touchAt(i, g, x, z); makeRod(i, g, { len: L + 0.08, hang: true, k: 9.81 / L, c: 0.1, R: 0.075, kind: 'cable', max: 0.5 });
    }
    motes(i, 60, 0xcfe0ff, 0.018, 0.25, [4, 3.2, 9], 0.008);
  }

  /* ---- 3: rock ---- */
  {
    const i = 3, D = W[i].deco;
    const rock = worldMat(i, SURF.rock(), { env: 0.7 });
    floorOf(i, rock); W[i].scanMat = worldMat(i, SURF.rock(), { env: 0.7, key: 's', safe: true });
    const geos = [rockGeometry(4, 1.3), rockGeometry(4, 7.9), rockGeometry(3, 4.2)];
    const boulder = (x, z, s, sy = 1) => { const m = new THREE.Mesh(geos[(Math.random() * 3) | 0], rock); m.scale.set(s * rnd(0.85, 1.25), s * sy * rnd(0.7, 1.0), s * rnd(0.85, 1.25)); m.position.set(x, 0.42 * m.scale.y, z); m.rotation.y = rnd(0, TAU); D.add(m); return m; };
    boulder(-1.7, -2.6, 0.75); boulder(-2.5, -3.3, 0.45); boulder(1.9, -2.2, 0.5); boulder(2.7, -3.9, 1.0); boulder(0.6, -4.6, 0.6); boulder(-0.9, -5.6, 1.25); boulder(3.6, -1.2, 0.4);
    boulder(-3.7, -1.4, 0.65); boulder(-4.6, -5.2, 1.5); boulder(4.9, -6.4, 1.7); boulder(0.3, -8.6, 1.9, 1.2); boulder(-2.4, 1.6, 0.5); boulder(2.3, 2.0, 0.7);
    const pillar = (x, z, s) => { let y = 0; for (let k = 0; k < 4; k++) { const w = s * (1.15 - k * 0.2) * rnd(0.9, 1.1), h = s * rnd(1.2, 1.7); const m = new THREE.Mesh(geos[(Math.random() * 3) | 0], rock); m.scale.set(w, h, w); m.position.set(x + rnd(-0.1, 0.1) * s, y + h * 0.45, z + rnd(-0.1, 0.1) * s); m.rotation.y = rnd(0, TAU); D.add(m); y += h * 0.5; } };
    pillar(-6.5, -8.5, 1.3); pillar(6.8, -10.5, 1.6); pillar(-3.2, -12.5, 1.1); pillar(9.5, -4.5, 1.2);
    const pg = rockGeometry(1, 2.2), N = 170, inst = new THREE.InstancedMesh(pg, rock, N), m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), sc = new V(), ps = new V();
    for (let k = 0; k < N; k++) { const a = rnd(0, TAU), r = Math.sqrt(Math.random()) * 6.5, s = rnd(0.015, 0.075) * (Math.random() < 0.08 ? 2.2 : 1); e.set(rnd(0, 0.5), rnd(0, TAU), rnd(0, 0.5)); q.setFromEuler(e); sc.set(s * rnd(0.8, 1.4), s * rnd(0.5, 0.9), s * rnd(0.8, 1.4)); ps.set(Math.cos(a) * r, sc.y * 0.3, Math.sin(a) * r - 2); inst.setMatrixAt(k, m4.compose(ps, q, sc)); }
    inst.frustumCulled = false; D.add(inst);
    /* tufts of dry straw growing out of the sand: every straw gives on its own when a hand goes through */
    const strawM = worldMat(i, SURF.straw(), { env: 0.3 });
    for (const [x, z] of [[0.8, -0.6], [-0.9, -1.2], [1.4, 0.5], [-1.3, 0.55], [0.35, -2.2], [-0.5, -2.0], [1.1, 1.3], [-1.0, 1.5]]) {
      const host = touchAt(i, new THREE.Group(), x, z), n = 7 + ((Math.random() * 5) | 0);
      for (let k = 0; k < n; k++) {
        const h = rnd(0.55, 1.2), g = new THREE.Group(), geo = new THREE.CylinderGeometry(0.0014, 0.0034, h, 5, 1); geo.translate(0, h / 2, 0);
        g.add(new THREE.Mesh(geo, strawM)); const head = new THREE.Mesh(new THREE.SphereGeometry(0.006, 7, 5), strawM); head.scale.y = 5; head.position.y = h; g.add(head);
        const a = rnd(0, TAU), r = rnd(0, 0.07); g.position.set(Math.cos(a) * r, 0, Math.sin(a) * r); host.add(g);
        makeRod(i, g, { host, len: h, rx: Math.cos(a) * rnd(0.05, 0.3), rz: Math.sin(a) * rnd(0.05, 0.3), k: 60, c: 2.2, sw: rnd(0.01, 0.03), R: 0.055, kind: 'straw' });
      }
    }
    motes(i, 70, 0xffd9b0, 0.016, 0.22, [4, 2.6, 8], 0.006);
  }

  /* ---- deep space: the sky is a shader (SPACE_GLSL); these are the stars near enough to have a place: you see them at their own distance, and they slide
   * against the far ones when you move your head ---- */
  const SP = { q: new THREE.Quaternion(), stars: new THREE.Group() };
  {
    const i = W.findIndex(w => w.space), N = 3000, pos = new Float32Array(N * 3), col = new Float32Array(N * 3), lum = new Float32Array(N), c = new THREE.Color(), d = new V();
    for (let k = 0; k < N; k++) {
      d.set(rnd(-30, 30), rnd(-30, 30), rnd(-30, 30));                                                     // a 60 m cube of stars that repeats for ever round you as you fly
      pos[k * 3] = d.x; pos[k * 3 + 1] = d.y; pos[k * 3 + 2] = d.z;
      const tt = Math.random(); if (tt < 0.42) c.setRGB(1, 0.66 + 0.29 * tt / 0.42, 0.42 + 0.46 * tt / 0.42); else if (tt < 0.6) c.setRGB(1, 0.95, 0.88); else c.setRGB(1 - 0.32 * (tt - 0.6) / 0.4, 0.95 - 0.15 * (tt - 0.6) / 0.4, 0.88 + 0.12 * (tt - 0.6) / 0.4);
      col[k * 3] = c.r; col[k * 3 + 1] = c.g; col[k * 3 + 2] = c.b; lum[k] = 0.45 + 1.9 * Math.pow(Math.random(), 3);
    }
    const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(pos, 3)); geo.setAttribute('aCol', new THREE.BufferAttribute(col, 3)); geo.setAttribute('aL', new THREE.BufferAttribute(lum, 1));
    const m = reg(i, new THREE.ShaderMaterial({ uniforms: { uScale: { value: 900 }, uTravel: travelU, uWarp: warpU }, clipping: true, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
      vertexShader: `attribute vec3 aCol; attribute float aL; uniform float uScale; uniform vec3 uTravel; uniform vec4 uWarp; varying vec3 vC; varying float vB;
        #include <clipping_planes_pars_vertex>
        void main() { const float L = 60.0;
          vec3 rel = mod(position - uTravel - cameraPosition + 0.5 * L, L) - 0.5 * L; float len = length(rel);      // the nearest copy of this star, from this eye
          if (uWarp.w > 0.002) { vec3 a = uWarp.xyz, dir = rel / max(len, 1e-4); float b = uWarp.w, c = dot(dir, a), c1 = (c + b) / (1.0 + b * c); vec3 pp = dir - c * a;
            rel = (a * c1 + pp / max(length(pp), 1e-5) * sqrt(max(0.0, 1.0 - c1 * c1))) * len; }
          vec4 mvPosition = viewMatrix * vec4(cameraPosition + rel, 1.0);
          #include <clipping_planes_vertex>
          float sz = aL * 0.05 * uScale / max(-mvPosition.z, 0.4), px = clamp(sz, 3.0, 56.0);                // a point of light with a little glow; never less than a few pixels
          vB = (min(1.0, sz * sz / (px * px)) * 0.5 + 0.5 * min(1.0, sz / px)) * (1.0 - smoothstep(22.0, 29.0, len)); vC = aCol; gl_PointSize = px; gl_Position = projectionMatrix * mvPosition; }`,
      fragmentShader: `precision highp float; varying vec3 vC; varying float vB;
        #include <clipping_planes_pars_fragment>
        void main() {
          #include <clipping_planes_fragment>
          float d = length(gl_PointCoord - 0.5) * 2.0, core = exp(-d * d * 22.0), halo = max(0.0, 1.0 - d); halo *= halo * halo * 0.22;
          gl_FragColor = vec4(mix(vC, vec3(1.0), core * 0.6) * (core * 1.5 + halo) * vB, 1.0); }` }));
    const p = new THREE.Points(geo, m); p.frustumCulled = false; SP.stars.add(p); SP.stars.position.set(0, 1.4, -1.6); W[i].group.add(SP.stars);
  }

  /* ---- the band's marks: in each of the three song dimensions an upright slab of that dimension's own stuff stands a little way off, with the sigil and
   * the name on it as that world would have made them: glowing lichen on the mossy stone, worn stencil paint on the steel, chalk on the sandstone ---- */
  {
    const loader = new THREE.TextureLoader(), logo = f => { const t = loader.load('img/' + f); t.anisotropy = 4; return t; };
    const segl = logo('segl.png'), navn = logo('tidejord.png');
    const mark = (i, tex, w, h, col, alpha, keep, glow) => {
      const m = reg(i, new THREE.ShaderMaterial({ clipping: true, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2, polygonOffsetUnits: -2, blending: glow ? THREE.AdditiveBlending : THREE.NormalBlending,
        uniforms: { uMap: { value: tex }, uNoise: noiseU, uTime: timeU, uCol: { value: new V(...col) }, uAlpha: { value: alpha }, uKeep: { value: keep }, uGlow: { value: glow ? 1 : 0 }, ...W[i].uni },
        vertexShader: `varying vec2 vUv; varying vec3 vWP; varying float vD;
          #include <clipping_planes_pars_vertex>
          void main() { vUv = uv; vec4 wp = modelMatrix * vec4(position, 1.0); vWP = wp.xyz; vec4 mvPosition = viewMatrix * wp; vD = length(mvPosition.xyz);
            #include <clipping_planes_vertex>
            gl_Position = projectionMatrix * mvPosition; }`,
        fragmentShader: `precision highp float; varying vec2 vUv; varying vec3 vWP; varying float vD;
          uniform sampler2D uMap, uNoise; uniform float uTime, uAlpha, uKeep, uGlow, uFogDen; uniform vec3 uCol, uFogCol, uGrade;
          #include <clipping_planes_pars_fragment>
          void main() {
            #include <clipping_planes_fragment>
            vec2 q = vWP.xy + vWP.z * 0.7;
            float wear = smoothstep(0.30, 0.62, texture2D(uNoise, q * 0.9).r * 0.55 + texture2D(uNoise, q * 5.0).b * 0.45 + uKeep);      // time has taken some of it
            float a = texture2D(uMap, vUv).r * wear * uAlpha; if (a < 0.004) discard;
            vec3 c = uCol * (1.0 - uGlow * 0.4 * (0.5 + 0.5 * sin(uTime * 0.45 + vWP.x * 2.0))) * uGrade;
            float f = 1.0 - exp(-uFogDen * uFogDen * vD * vD);
            gl_FragColor = vec4(mix(c, uFogCol, f * (1.0 - uGlow)), a * mix(1.0, 1.0 - f, uGlow));
            #include <tonemapping_fragment>
            #include <colorspace_fragment>
          }` }));
      return new THREE.Mesh(new THREE.PlaneGeometry(w, h), m);
    };
    const marker = (i, body, zf, x, z, col, alpha, keep, glow) => {
      const g = new THREE.Group(); g.add(body);
      const a = mark(i, segl, 0.40, 0.608, col, alpha, keep, glow); a.position.set(0, 1.02, zf); g.add(a);
      const b = mark(i, navn, 0.46, 0.246, col, alpha * 0.8, keep, glow); b.position.set(0, 0.5, zf); g.add(b);
      g.position.set(x, 0, z); g.rotation.y = Math.atan2(-x, -z - 0.6); W[i].deco.add(g);
    };
    marker(1, new THREE.Mesh(slabGeometry(0.8, 1.55, 0.3, 3.1), worldMat(1, SURF.moss(0.25), { env: 0.9, key: 'm' })), 0.165, -2.2, -3.3, [0.05, 0.85, 0.60], 0.5, 0.12, true);
    const plateG = new THREE.Group(), steelM = worldMat(2, SURF.steel(), { env: 0.85 });
    { const p = new THREE.Mesh(new THREE.BoxGeometry(0.78, 1.5, 0.03), worldMat(2, SURF.plate(), { env: 0.85 })); p.position.y = 0.85; plateG.add(p);
      const base = new THREE.Mesh(new THREE.BoxGeometry(0.9, 0.1, 0.4), steelM); base.position.y = 0.05; plateG.add(base);
      for (const sx of [-1, 1]) { const st = new THREE.Mesh(new THREE.BoxGeometry(0.05, 1.6, 0.05), steelM); st.position.set(sx * 0.415, 0.8, -0.02); plateG.add(st); } }
    marker(2, plateG, 0.019, -2.3, -3.0, [0.62, 0.61, 0.57], 0.75, 0.02, false);
    marker(3, new THREE.Mesh(slabGeometry(0.85, 1.6, 0.32, 8.4), worldMat(3, SURF.rock(), { env: 0.7 })), 0.176, 2.2, -3.5, [0.80, 0.72, 0.60], 0.55, 0.06, false);
  }

  /* ---- the scanned room, wearing each dimension's surface ---- */
  const scanKnown = new Map(); let scanNext = 0; const scanInfo = { feature: false, count: 0 };
  const wire = params.get('skin') === 'wire' ? new THREE.MeshBasicMaterial({ color: 0x40ffd0, wireframe: true, transparent: true, opacity: 0.5 }) : null;
  /* The past: the real room is still what you see, but its scanned shape carries a faint amber glow, strongest along its edges,
   * with a slow wave passing through it. Drawn additively over the passthrough. */
  const pastU = { uAmt: { value: 0 }, uTime: timeU };
  const pastMat = new THREE.ShaderMaterial({
    uniforms: pastU, transparent: true, depthWrite: false, side: THREE.DoubleSide,
    blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneFactor, blendSrcAlpha: THREE.ZeroFactor, blendDstAlpha: THREE.OneFactor,
    vertexShader: 'varying vec3 vP; varying vec3 vN; varying vec3 vV; void main() { vec4 wp = modelMatrix * vec4(position, 1.0); vP = wp.xyz; vN = normalize(mat3(modelMatrix) * normal); vV = cameraPosition - wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
    fragmentShader: `precision highp float; varying vec3 vP; varying vec3 vN; varying vec3 vV; uniform float uAmt; uniform float uTime;
      void main() {
        float rim = pow(1.0 - abs(dot(normalize(vN), normalize(vV))), 2.2);
        float wave = pow(0.5 + 0.5 * sin(vP.y * 9.0 + vP.x * 2.0 - uTime * 0.9), 14.0);
        float shimmer = 0.5 + 0.5 * sin(uTime * 1.7 + vP.x * 3.1 + vP.z * 2.3);
        float near = 1.0 - smoothstep(1.5, 6.0, length(vV));
        vec3 col = vec3(1.0, 0.62, 0.22) * (0.03 + rim * (0.22 + 0.1 * shimmer) + wave * 0.16) * near;
        gl_FragColor = vec4(col * uAmt, 0.0); }`,
  });
  W[0].scan.visible = false;

  /* What is really there, as a grid of 8 cm cells, so "how close is my head to something" is a handful of lookups a frame. */
  const CELL = 0.08, grid = new Set(), _p = new V(), _a = new V(), _b = new V(), _c = new V(), _m = new THREE.Matrix4();
  const keyOf = (x, y, z) => ((Math.floor(x / CELL) + 512) & 1023) | (((Math.floor(y / CELL) + 512) & 1023) << 10) | (((Math.floor(z / CELL) + 512) & 1023) << 20);
  function cellsOf(pos, idx, matrix) {
    const out = new Set(); _m.fromArray(matrix);
    for (let i = 0; i < idx.length; i += 3) {
      _a.fromArray(pos, idx[i] * 3).applyMatrix4(_m); _b.fromArray(pos, idx[i + 1] * 3).applyMatrix4(_m); _c.fromArray(pos, idx[i + 2] * 3).applyMatrix4(_m);
      const n = Math.min(80, Math.max(1, Math.ceil(Math.max(_a.distanceTo(_b), _b.distanceTo(_c), _c.distanceTo(_a)) / 0.07)));
      for (let u = 0; u <= n; u++) for (let v = 0; v <= n - u; v++) { const s = u / n, t = v / n; _p.copy(_a).multiplyScalar(1 - s - t).addScaledVector(_b, s).addScaledVector(_c, t); out.add(keyOf(_p.x, _p.y, _p.z)); }
    }
    return out;
  }
  function regrid() { blocked2.clear(); reachCache = null; grid.clear(); for (const rec of scanKnown.values()) for (const k of rec.cells) grid.add(k); }
  /** Distance from p to the nearest scanned surface above knee height, or Infinity if nothing is within r. */
  function nearest(p, r = 0.4) {
    if (!grid.size) return Infinity;
    const n = Math.ceil(r / CELL), ix = Math.floor(p.x / CELL), iy = Math.floor(p.y / CELL), iz = Math.floor(p.z / CELL); let best = Infinity;
    for (let dy = -n; dy <= n; dy++) { const cy = (iy + dy + 0.5) * CELL; if (cy < 0.3) continue;
      for (let dx = -n; dx <= n; dx++) for (let dz = -n; dz <= n; dz++) {
        if (!grid.has(((ix + dx + 512) & 1023) | (((iy + dy + 512) & 1023) << 10) | (((iz + dz + 512) & 1023) << 20))) continue;
        const d = Math.hypot((ix + dx + 0.5) * CELL - p.x, cy - p.y, (iz + dz + 0.5) * CELL - p.z); if (d < best) best = d;
      } }
    return best <= r ? best : Infinity;
  }
  /** The height of the topmost scanned thing under (x, z), looking down from yMax; null if there is nothing there at all. Grid-accurate (8 cm). */
  function columnTop(x, z, yMax = 1.9) {
    const ix = (Math.floor(x / CELL) + 512) & 1023, iz = ((Math.floor(z / CELL) + 512) & 1023) << 20;
    for (let iy = Math.floor(yMax / CELL); iy >= -2; iy--) if (grid.has(ix | (((iy + 512) & 1023) << 10) | iz)) return (iy + 0.5) * CELL;
    return null;
  }
  /** Is the straight line from a to b free of scanned things (ignoring the first and last few centimetres)? */
  function lineClear(a, b, skip = 0.13) {
    const dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z, d = Math.hypot(dx, dy, dz); if (d <= 2 * skip) return true;
    for (let s = skip; s <= d - skip; s += 0.035) { const t = s / d; if (grid.has(keyOf(a.x + dx * t, a.y + dy * t, a.z + dz * t))) return false; }
    return true;
  }
  /* Can you walk there? The scan seen from above, in 16 cm squares: a square is blocked if anything stands in it between shin and head height.
   * A flood fill from where you stand (through squares with no blocked neighbour, so a gap has to be about half a metre wide) gives everywhere you can get to. */
  const blocked2 = new Map(); let reachCache = null;
  const k2 = (i, j) => ((i + 2048) << 12) | (j + 2048);
  function isBlocked(i, j) {
    const k = k2(i, j); let b = blocked2.get(k); if (b !== undefined) return b; b = false;
    for (let dx = 0; dx < 2 && !b; dx++) for (let dz = 0; dz < 2 && !b; dz++) { const ix = ((i * 2 + dx + 512) & 1023), iz = ((j * 2 + dz + 512) & 1023) << 20; for (let iy = 4; iy <= 21; iy++) if (grid.has(ix | ((iy + 512) << 10) | iz)) { b = true; break; } }
    blocked2.set(k, b); return b;
  }
  const roomy = (i, j) => { for (let a = -1; a <= 1; a++) for (let c = -1; c <= 1; c++) if (isBlocked(i + a, j + c)) return false; return true; };
  function reachFrom(p) {
    const i0 = Math.floor(p.x / (2 * CELL)), j0 = Math.floor(p.z / (2 * CELL));
    if (reachCache && reachCache.i === i0 && reachCache.j === j0) return reachCache.set;
    const set = new Set(), queue = []; let start = null;
    for (let r = 0; r <= 4 && !start; r++) for (let a = -r; a <= r && !start; a++) for (let c = -r; c <= r && !start; c++) if (roomy(i0 + a, j0 + c)) start = [i0 + a, j0 + c];      // you may be standing right by a table
    if (start) { queue.push(start); set.add(k2(start[0], start[1])); }
    for (let q = 0; q < queue.length; q++) { const [i, j] = queue[q];
      for (const [a, c] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const ni = i + a, nj = j + c, k = k2(ni, nj); if (set.has(k) || Math.abs(ni - i0) > 45 || Math.abs(nj - j0) > 45 || !roomy(ni, nj)) continue; set.add(k); queue.push([ni, nj]); } }
    reachCache = { i: i0, j: j0, set }; return set;
  }
  /** Is there somewhere you can walk to, from p, that is within arm's reach (r) of (x, z)? Always true with no scan. */
  function reachable(p, x, z, r = 0.6) {
    if (!grid.size) return true;
    const set = reachFrom(p), n = Math.ceil(r / (2 * CELL)), i0 = Math.floor(x / (2 * CELL)), j0 = Math.floor(z / (2 * CELL));
    for (let a = -n; a <= n; a++) for (let c = -n; c <= n; c++) if (set.has(k2(i0 + a, j0 + c)) && Math.hypot((i0 + a + 0.5) * 2 * CELL - x, (j0 + c + 0.5) * 2 * CELL - z) <= r) return true;
    return false;
  }
  function addScan(key, vertices, indices, matrix, time) {
    const rec = scanKnown.get(key);
    if (rec) { for (const g of rec.gs) g.parent.remove(g); rec.geo.dispose(); }
    const pos = new Float32Array(vertices), idx = new Uint32Array(indices);
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3)); geo.setIndex(new THREE.BufferAttribute(idx, 1)); geo.computeVertexNormals();
    const gs = [];
    for (const w of W) {
      const g = new THREE.Group(); g.matrixAutoUpdate = false; g.matrix.fromArray(matrix); g.matrixWorldNeedsUpdate = true;
      if (w.bare) { const h = new THREE.Mesh(geo, holeMat), gh = new THREE.Mesh(geo, ghostMat); h.renderOrder = 6; gh.renderOrder = 7; h.userData.keepOrder = gh.userData.keepOrder = true; g.add(h, gh); }
      else { g.add(new THREE.Mesh(geo, w.i === 0 ? pastMat : w.scanMat)); if (wire && w.i) g.add(new THREE.Mesh(geo, wire)); }
      w.scan.add(g); gs.push(g);
    }
    scanKnown.set(key, { t: time, gs, geo, cells: cellsOf(pos, idx, matrix) }); regrid(); api.dirty = true; scanInfo.count = scanKnown.size;
  }
  function updateScan(frame, refSpace) {
    const now = performance.now();
    if (now < scanNext || !frame.detectedMeshes) return scanInfo;
    scanNext = now + 600; scanInfo.feature = true;
    let did = 0; const seen = new Set();
    for (const m of frame.detectedMeshes) {
      seen.add(m);
      const rec = scanKnown.get(m);
      if (rec && m.lastChangedTime <= rec.t) continue;
      if (did >= 1) { scanNext = 0; continue; }
      const pose = frame.getPose(m.meshSpace, refSpace); if (!pose) continue;
      addScan(m, m.vertices, m.indices, pose.transform.matrix, m.lastChangedTime); did++;
    }
    let gone = false;
    for (const [k, rec] of scanKnown) if (!seen.has(k) && typeof k !== 'string') { for (const g of rec.gs) g.parent.remove(g); rec.geo.dispose(); scanKnown.delete(k); gone = true; }
    if (gone) regrid();
    scanInfo.count = scanKnown.size;
    return scanInfo;
  }

  const api = {
    W, dirty: false, onTouch: null, noise, rods, updateScan, nearest, addScan, scanInfo, columnTop, lineClear, reachable, hasScan: () => grid.size > 0, pastAmount: 0,
    /** 0..1: how strongly the room's shape glows (the past). */
    setPast(v) { api.pastAmount = v; pastU.uAmt.value = v; W[0].scan.visible = v > 0.001; },
    /** cur is home (drawn everywhere), oth is seen through the door, the rest are off. */
    applyRoles(cur, oth) {
      for (const w of W) {
        const role = w.i === cur ? 'home' : w.i === oth ? 'other' : 'off';
        w.group.visible = role !== 'off';
        for (const m of w.mats) m.stencilWrite = role === 'other';
        if (role !== 'off') w.group.traverse(o => { if ((o.isMesh || o.isPoints || o.isLine) && !o.userData.keepOrder) o.renderOrder = role === 'home' ? ORDER.home : ORDER.other; });
        if (w.bare) w.scan.visible = role === 'home';                 // the room's outline only when you stand in the fractal (or in space), not through the door
        if (w.skyHome) w.skyHome.visible = role === 'home';
        w.fill.visible = role === 'other';
        if (role !== 'other') w.clip.set(PASS, 1e6);
      }
      api.dirty = false;
    },
    /** Keep only what is behind the door, seen from where you stand. */
    setClip(oth, normal, point) { W[oth].clip.setFromNormalAndCoplanarPoint(normal, point); },
    /** Put a dimension's own things round the door, once, so they do not jump when you cut a new door while standing among them. */
    anchor(i, x, z, yaw, force) { const w = W[i]; if (w.anchored && !force) return; w.deco.position.set(x, 0, z); w.deco.rotation.set(0, yaw, 0); w.anchored = true; placeTouch(w); if (w.space) SP.stars.position.set(x, 1.4, z); if (w.march) { w.fr.A.set(x, 0, z); w.fr.yaw = yaw; w.fr.R4.identity(); w.fr.c4.set(0, 0, 0, 0); w.fr.depth = w.fr.turn = 0; w.fr.S = w.fr.f.S; w.fr.off.set(...w.fr.f.off); w.fr.param = w.fr.f.par[0]; w.fr.param2 = w.fr.f.par2[0]; syncFr(w); } },
    renderFractals, fractalScale: () => FR.scale,
    /** 0..1: how visible the room's outline is in a fractal dimension (it fades while you stand still). The opening onto the real room near you is not affected. */
    setGhost(v) { ghostU.value = v; },
    /** True while the player stands still and steers nothing: the fractal may then sharpen. */
    setFractalCalm(v) { FR.calm = v; }, fractalFine: () => fineU.value,
    /** How the fractals sharpen at rest: 'taa' (built up over time), 'res' (a bigger picture if the headset can carry it) or 'off'. */
    setFractalSharpen(mode) { FR.mode = mode; }, fractalInfo: () => ({ mode: FR.mode, acc: FR.acc, n: FR.n, scale: FR.scale }),
    /** Moving through a fractal dimension: the fractal slides, turns and grows round the point where your head is. Distances in metres, angles in radians. */
    fractTurn(i, h, a) { const w = W[i]; if (!w.fr) return; toF(w, h, _f1); w.fr.yaw += a; toF(w, h, _f2); w.fr.off.add(_f1).sub(_f2); keepIn(w, h); },
    fractMove(i, h, dir, dist) { const w = W[i]; if (!w.fr) return; const c = Math.cos(w.fr.yaw), s = Math.sin(w.fr.yaw), k = dist * w.fr.S; w.fr.off.x += (c * dir.x - s * dir.z) * k; w.fr.off.y += dir.y * k; w.fr.off.z += (s * dir.x + c * dir.z) * k; keepIn(w, h); },
    fractScale(i, h, factor) { const w = W[i]; if (!w.fr) return; toF(w, h, _f1); w.fr.S = Math.min(w.fr.f.S * 12, Math.max(w.fr.f.S / 12, w.fr.S * factor)); toF(w, h, _f2); w.fr.off.add(_f1).sub(_f2); keepIn(w, h); },
    fractParam(i, which, dir, dt) { const w = W[i]; if (!w.fr) return; const key = which ? 'param2' : 'param', [, lo, hi] = which ? w.fr.f.par2 : w.fr.f.par; w.fr[key] = Math.min(hi, Math.max(lo, w.fr[key] + dir * dt * (hi - lo) / 7)); syncFr(w); },
    fractState: i => W[i].fr,
    /** The four-dimensional room. Which slice of it you see is a rotation R4 and a shift c4: a point q of the room you stand in is the 4D point R4 (q, 0) + c4.
     *  rot turns the slice in the plane of u (a level direction, e.g. your right) and w, about the point where your head is, so what is straight ahead stays put
     *  while it changes; pull slides the slice along w. You cannot pull yourself out through the room's walls. */
    fourStep(i, h, u, rot, pull) {
      const w = W[i], r = w.fr; if (!r) return; toF(w, h, _f1); const qh = _v4a.set(_f1.x, _f1.y, _f1.z, 0), c = Math.cos(r.yaw), s = Math.sin(r.yaw);
      if (rot) {
        const ux = c * u.x - s * u.z, uz = s * u.x + c * u.z, ca = Math.cos(rot), sa = Math.sin(rot), G = _g4.identity().elements, k = ca - 1;
        G[0] = 1 + k * ux * ux; G[2] = k * ux * uz; G[8] = k * ux * uz; G[10] = 1 + k * uz * uz; G[15] = ca;      // u -> cos u + sin w, w -> cos w - sin u (column-major)
        G[3] = sa * ux; G[11] = sa * uz; G[12] = -sa * ux; G[14] = -sa * uz;
        const before = _v4b.copy(qh).applyMatrix4(r.R4); r.R4.multiply(_g4); r.c4.add(before).sub(_v4b.copy(qh).applyMatrix4(r.R4)); r.turn += rot;
      }
      if (pull) {
        const e = r.R4.elements, hp = _v4b.copy(qh).applyMatrix4(r.R4).add(r.c4); hp.x += e[12] * pull; hp.y += e[13] * pull; hp.z += e[14] * pull; hp.w += e[15] * pull;
        const C = ROOM4.c, B = ROOM4.b, m = 0.3;
        if (Math.abs(hp.x - C[0]) < B[0] - m && Math.abs(hp.z - C[2]) < B[2] - m && Math.abs(hp.w - C[3]) < B[3] - m) { r.c4.x += e[12] * pull; r.c4.y += e[13] * pull; r.c4.z += e[14] * pull; r.c4.w += e[15] * pull; r.depth += pull; }
      }
      syncFr(w);
    },
    fourReset(i) { const r = W[i].fr; if (!r) return; r.R4.identity(); r.c4.set(0, 0, 0, 0); r.depth = r.turn = 0; syncFr(W[i]); },
    fourInfo: i => ({ depth: W[i].fr.depth, turn: W[i].fr.turn }),
    /** The head, kept by reference: where "can you walk there" is measured from. */
    setHead(v) { headRef = v; },
    /** Deep space: the whole sky turns about your head by the rotation vector w (radians a second, world axes). You tumble; you do not get anywhere. */
    /** Flying in deep space: the near stars stream past (they repeat for ever), the far sky stays where it is. */
    spaceMove(dir, dist) { const t = travelU.value; t.addScaledVector(dir, dist); t.set(((t.x % 60) + 60) % 60, ((t.y % 60) + 60) % 60, ((t.z % 60) + 60) % 60); },
    /** Warp: axis (a unit vector, the way you are heading) and how hard (0 to 0.95, a fraction of the speed of light): the sky is swept forward and shifts colour. */
    spaceWarp(axis, b) { warpU.value.set(axis.x, axis.y, axis.z, b); }, spaceWarpNow: () => warpU.value.w,
    /** The black hole's pull on the light round it: 0 (no hole) to about 0.4 (its ring fills much of the view). */
    spaceHole(v) { holeU.value = v; }, spaceHoleNow: () => holeU.value,
    /** The fractals' colour scheme: an angle the hues are turned by. */
    setHue(a) { hueU.value = a; },
    spaceSpin(h, w, dt) {
      const a = w.length() * dt; if (a < 1e-7) return;
      _rq.setFromAxisAngle(_rv.copy(w).normalize(), a); SP.q.premultiply(_rq).normalize();
      SP.stars.position.sub(h).applyQuaternion(_rq).add(h); SP.stars.quaternion.copy(SP.q);
      spinU.value.setFromMatrix4(_m4.makeRotationFromQuaternion(_rq.copy(SP.q).invert()));
    },
    /** A still picture of dimension i as it looks from cam through the door (only what lies beyond the plane n, p), into rt. Used while the door changes where it leads. */
    snapshot(i, cam, rt, n, p) {
      const w = W[i], xrWas = renderer.xr.enabled, rtWas = renderer.getRenderTarget(); renderer.xr.enabled = false; cam.updateMatrixWorld();
      if (w.fract) { const calm = FR.calm; FR.calm = false; FR.thru = true; renderFractals([{ matrix: cam.matrixWorld.elements, proj: cam.projectionMatrix.elements }], i, -1, FR.ema); FR.calm = calm; FR.thru = false; renderer.xr.enabled = false; }
      api.applyRoles(i, -1); w.clip.setFromNormalAndCoplanarPoint(n, p); if (w.bare) w.scan.visible = false;
      const on = o => { o.layers.enable(3); }, off = o => { o.layers.disable(3); };
      scene.traverse(o => { if (o.isLight) o.layers.enable(3); }); w.group.traverse(on); if (w.skyHome) w.skyHome.traverse(on);
      cam.layers.set(3); renderer.setRenderTarget(rt); renderer.render(scene, cam);
      w.group.traverse(off); if (w.skyHome) w.skyHome.traverse(off);
      renderer.setRenderTarget(rtWas); renderer.xr.enabled = xrWas; api.dirty = true;
    },
    update(dt, t, bright, cur, oth, safe, hands) {
      timeU.value = t; brightU.value = bright;
      /* the hands, in the home dimension's own coordinates (for the things that give when touched) and in the world's (for the grass) */
      const wc = W[cur], live = hands && cur > 0 && !wc.bare;
      for (let h = 0; h < 2; h++) {
        const hd = live && hands[h] && hands[h].on ? hands[h] : null; (h ? handU.uH1 : handU.uH0).value.copy(hd ? hd.pos : _p.set(0, -99, 0));
        if (hd) { HLprev[h].copy(HL[h]); HL[h].copy(hd.pos).sub(wc.deco.position).applyAxisAngle(UP, -wc.deco.rotation.y); if (HLon[h] && HLw === cur && dt > 1e-4) { HV[h].copy(HL[h]).sub(HLprev[h]).multiplyScalar(1 / dt); if (HV[h].length() > 3) HV[h].setLength(3); } else HV[h].set(0, 0, 0); }      // (a tracking jump is not a blow)
        HLon[h] = !!hd;
      }
      HLw = live ? cur : -1; rodsUpdate(dt, t, cur);
      safeU.uSafeOn.value = safe ? 1 : 0; if (safe) { safeU.uS0.value.copy(safe[0]); safeU.uS1.value.copy(safe[1]); safeU.uS2.value.copy(safe[2]); }
      for (const w of W) {
        w.uni.uFogCol.value.copy(w.base.fog).multiplyScalar(bright); w.uni.uGrade.value.copy(w.base.grade).multiplyScalar(bright);
        if (w.i === cur || w.i === oth) for (const f of w.tick) f(dt, t);
      }
    },
    /** Compile every dimension's shaders up front, so switching does not stutter in the headset. */
    precompile(camera) { for (const w of W) { w.group.visible = true; if (w.skyHome) w.skyHome.visible = true; w.fill.visible = true; if (w.march) { frMesh.material = w.march; renderer.compile(FR.scene, FR.cam); } } renderer.compile(scene, camera); },
  };
  return api;
}
