/* Shared GLSL: simplex noise with gradient (webgl-noise by McEwan & Gustavson, MIT) and the terrain height
 * function. physics/noise.js and physics/terrain.js are line-by-line ports: change one, change the other. */

export const NOISE = /* glsl */`
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x * 34.0) + 10.0) * x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
float snoise(vec3 v, out vec3 gradient) {
  const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
  const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);
  vec3 i = floor(v + dot(v, C.yyy));
  vec3 x0 = v - i + dot(i, C.xxx);
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min(g.xyz, l.zxy);
  vec3 i2 = max(g.xyz, l.zxy);
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy;
  vec3 x3 = x0 - D.yyy;
  i = mod289(i);
  vec4 p = permute(permute(permute(
            i.z + vec4(0.0, i1.z, i2.z, 1.0))
          + i.y + vec4(0.0, i1.y, i2.y, 1.0))
          + i.x + vec4(0.0, i1.x, i2.x, 1.0));
  float n_ = 0.142857142857;
  vec3 ns = n_ * D.wyz - D.xzx;
  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);
  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_);
  vec4 x = x_ * ns.x + ns.yyyy;
  vec4 y = y_ * ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);
  vec4 b0 = vec4(x.xy, y.xy);
  vec4 b1 = vec4(x.zw, y.zw);
  vec4 s0 = floor(b0) * 2.0 + 1.0;
  vec4 s1 = floor(b1) * 2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));
  vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
  vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;
  vec3 p0 = vec3(a0.xy, h.x);
  vec3 p1 = vec3(a0.zw, h.y);
  vec3 p2 = vec3(a1.xy, h.z);
  vec3 p3 = vec3(a1.zw, h.w);
  vec4 norm = taylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
  p0 *= norm.x; p1 *= norm.y; p2 *= norm.z; p3 *= norm.w;
  vec4 m = max(0.5 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
  vec4 m2 = m * m;
  vec4 m4 = m2 * m2;
  vec4 pdotx = vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3));
  vec4 temp = m2 * m * pdotx;
  gradient = -8.0 * (temp.x * x0 + temp.y * x1 + temp.z * x2 + temp.w * x3);
  gradient += m4.x * p0 + m4.y * p1 + m4.z * p2 + m4.w * p3;
  gradient *= 105.0;
  return 105.0 * dot(m4, pdotx);
}
float snoise(vec3 v) { vec3 g; return snoise(v, g); }
`;

/* Terrain height in units; uniforms uOa..uOf are the seed offsets from physics/terrain.js seedOffsets().
 * TERRAIN_OCT_MAX bounds the loop; the active count is a parameter (more octaves near the camera). */
export const TERRAIN = /* glsl */`
uniform vec3 uOa, uOb, uOc, uOd, uOe, uOf;
float fbm5(vec3 p) {
  float sum = 0.0, amp = 0.5, f = 1.0;
  for (int i = 0; i < 5; i++) { sum += amp * snoise(p * f); f *= 2.03; amp *= 0.5; }
  return sum;
}
float ridged(vec3 p, int oct) {
  float sum = 0.0, amp = 0.5, f = 1.0, w = 1.0;
  for (int i = 0; i < 8; i++) {
    if (i >= oct) break;
    float n = 1.0 - abs(snoise(p * f));
    n *= n; n *= w;
    w = clamp(n * 2.0, 0.0, 1.0);
    sum += n * amp; f *= 2.1; amp *= 0.5;
  }
  return sum;
}
float erodedFbm(vec3 p, float oct) {
  float sum = 0.0, amp = 0.5, f = 1.0;
  vec3 d = vec3(0.0), g;
  for (int i = 0; i < 14; i++) {
    float fi = float(i);
    if (fi >= oct) break;
    float n = snoise(p * f, g);
    d += g * amp;
    float fade = clamp(oct - fi, 0.0, 1.0);          /* the last octave fades in, so LOD changes do not pop */
    sum += fade * amp * n / (1.0 + dot(d, d));
    f *= 2.0; amp *= 0.5;
  }
  return sum;
}
/* out: c = continent value, land = land mask, mm = mountain mask (for colouring) */
float terrainH(vec3 n, float detailOct, out float c, out float land, out float mm) {
  vec3 w = n * 0.9 + uOa;
  vec3 q = w + 0.4 * vec3(
    snoise(w * 1.6 + uOb),
    snoise(w * 1.6 + uOb + vec3(17.1, 0.0, 0.0)),
    snoise(w * 1.6 + uOb + vec3(0.0, 31.7, 0.0)));
  c = fbm5(q);
  land = smoothstep(-0.08, 0.18, c);
  float e = -1.0 + 1.1 * land + 0.45 * c;
  mm = smoothstep(0.25, 0.7, snoise(n * 1.4 + uOc)) * smoothstep(0.0, 0.6, land);
  e += 2.0 * mm * ridged(n * 4.2 + uOd, 6);
  float r2 = ridged(n * 1.8 + uOe, 3);
  e += 0.3 * (1.0 - land) * r2 * r2;
  e += 0.24 * (0.3 + land + mm) * erodedFbm(n * 9.0 + uOf, detailOct);
  return e;
}
float terrainH(vec3 n, float detailOct) { float c, l, m; return terrainH(n, detailOct, c, l, m); }
`;

/* Cube-face point -> sphere, with the area-equalising map (P. Nowell): no pole pinching, even triangles. */
export const CUBESPHERE = /* glsl */`
vec3 cubeToSphere(vec3 p) {
  vec3 p2 = p * p;
  return vec3(
    p.x * sqrt(1.0 - p2.y * 0.5 - p2.z * 0.5 + p2.y * p2.z / 3.0),
    p.y * sqrt(1.0 - p2.z * 0.5 - p2.x * 0.5 + p2.z * p2.x / 3.0),
    p.z * sqrt(1.0 - p2.x * 0.5 - p2.y * 0.5 + p2.x * p2.y / 3.0));
}
`;
