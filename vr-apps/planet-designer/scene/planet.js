/* The planet (milestone M2): a cube-sphere with a chunked quadtree LOD, heights from the shared terrain function
 * (scene/glsl.js = physics/terrain.js) computed on the GPU, per-pixel detail through screen-space bump mapping,
 * oceans at the sea level that physics derives from the ocean volume, biomes and ice from a latitude-temperature
 * estimate, an atmosphere rim (a preview until the scattering model of M3), the cutaway with the interior layers,
 * and simple looks for the other planet types. Radius 1 in scene units; heights are exaggerated (stated on screen). */
import * as THREE from 'three';
import { NOISE, TERRAIN, CUBESPHERE } from './glsl.js?v=8';
import { seedOffsets } from '../physics/terrain.js?v=8';

const GRID = 48;                          /* quads per chunk side */
const MAX_LEVEL = 13;
let LOD_SPLIT = 5.0;                     /* split a chunk when closer than this many chunk sizes (PC; Quest lower) */

/* One shared chunk grid: uv in [0,1]^2 plus a skirt ring (aSkirt = 1) that hides cracks between LOD levels. */
function chunkGeometry() {
  const N = GRID, pos = [], skirt = [], idx = [];
  for (let j = 0; j <= N; j++) for (let i = 0; i <= N; i++) { pos.push(i / N, j / N, 0); skirt.push(0); }
  const id = (i, j) => j * (N + 1) + i;
  for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
    const a = id(i, j), b = id(i + 1, j), c = id(i, j + 1), d = id(i + 1, j + 1);
    idx.push(a, b, d, a, d, c);
  }
  /* skirts: duplicate the border, flagged, and stitch */
  const border = [];
  for (let i = 0; i < N; i++) border.push([i, 0]);
  for (let j = 0; j < N; j++) border.push([N, j]);
  for (let i = N; i > 0; i--) border.push([i, N]);
  for (let j = N; j > 0; j--) border.push([0, j]);
  const base = pos.length / 3;
  border.forEach(([i, j]) => { pos.push(i / N, j / N, 0); skirt.push(1); });
  for (let k = 0; k < border.length; k++) {
    const k2 = (k + 1) % border.length;
    const a = id(...border[k]), b = id(...border[k2]), c = base + k, d = base + k2;
    idx.push(a, c, b, b, c, d);
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('aSkirt', new THREE.Float32BufferAttribute(skirt, 1));
  g.setIndex(idx);
  g.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 10);
  return g;
}

const FACES = [
  [[1, 0, 0], [0, 0, -1], [0, 1, 0]], [[-1, 0, 0], [0, 0, 1], [0, 1, 0]],
  [[0, 1, 0], [1, 0, 0], [0, 0, -1]], [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
  [[0, 0, 1], [1, 0, 0], [0, 1, 0]], [[0, 0, -1], [-1, 0, 0], [0, 1, 0]],
].map((f) => f.map((v) => new THREE.Vector3(...v)));

function cubeToSphere(p) {
  const x2 = p.x * p.x, y2 = p.y * p.y, z2 = p.z * p.z;
  return new THREE.Vector3(
    p.x * Math.sqrt(1 - y2 / 2 - z2 / 2 + y2 * z2 / 3),
    p.y * Math.sqrt(1 - z2 / 2 - x2 / 2 + z2 * x2 / 3),
    p.z * Math.sqrt(1 - x2 / 2 - y2 / 2 + x2 * y2 / 3));
}

const COMMON_UNIFORMS = () => ({
  uSea: { value: 0 }, uRef: { value: 0 }, uK: { value: 0.006 }, uHasOcean: { value: 1 },
  uOa: { value: new THREE.Vector3() }, uOb: { value: new THREE.Vector3() }, uOc: { value: new THREE.Vector3() },
  uOd: { value: new THREE.Vector3() }, uOe: { value: new THREE.Vector3() }, uOf: { value: new THREE.Vector3() },
  uCut: { value: 0 },
});

const TERRAIN_VS = NOISE + TERRAIN + CUBESPHERE + /* glsl */`
uniform vec3 uFaceN, uFaceU, uFaceV;
uniform vec2 uOff; uniform float uScale, uVertOct, uSkirtDepth;
uniform float uSea, uRef, uK;
attribute float aSkirt;
varying vec3 vN;        /* unit direction, planet-local */
varying vec3 vWP;       /* world position of the undisplaced sphere point */
varying vec3 vWN;       /* world normal of the sphere */
void main() {
  vec2 fuv = uOff + position.xy * uScale;
  vec3 cube = uFaceN + (fuv.x * 2.0 - 1.0) * uFaceU + (fuv.y * 2.0 - 1.0) * uFaceV;
  vec3 n = cubeToSphere(cube);
  float h = terrainH(n, uVertOct);
  float r = 1.0 + (max(h, uSea) - uRef) * uK - aSkirt * uSkirtDepth;
  vN = n;
  vWP = (modelMatrix * vec4(n, 1.0)).xyz;
  vWN = normalize(mat3(modelMatrix) * n);
  gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(n * r, 1.0);
}`;

const ATMO = `
/* Sunlight through the air: vertical optical depths (Rayleigh per channel, Mie grey times its tint) times the
 * air mass for a sun at cosine mu (Kasten & Young 1989), dark below the horizon. */
uniform vec3 uTauR, uMieTint; uniform float uTauM;
vec3 sunTrans(float mu) {
  if (mu <= -0.02) return vec3(0.0);
  float m = mu > 0.0 ? 1.0 / (mu + 0.50572 * pow(96.07995 - degrees(acos(clamp(mu, -1.0, 1.0))), -1.6364)) : 40.0;
  m = min(m, 40.0);
  return exp(-(uTauR + uTauM * uMieTint) * m);
}
`;
const CLOUDS = `
/* Cloud cover on the unit sphere (planet-local direction n): a warped fbm field thresholded by the cover fraction,
 * with Earth-like belts (rising air at the equator and in the storm tracks near 55 deg, clear subtropics near 25 deg),
 * or a deck centred on the substellar point for a tidally locked world. */
uniform float uCloudFrac, uCloudTime, uCloudOct, uCloudLocked;
uniform vec3 uCloudSeed, uCloudSub;
float cloudDensity(vec3 n) {
  if (uCloudFrac <= 0.001) return 0.0;
  float cov = uCloudFrac;
  if (uCloudLocked > 0.5) cov *= 0.35 + 1.1 * smoothstep(-0.3, 0.9, dot(n, uCloudSub));
  else {
    float a = abs(n.y);
    cov += 0.2 * exp(-pow(a / 0.1, 2.0)) + 0.16 * exp(-pow((a - 0.8) / 0.12, 2.0)) - 0.24 * exp(-pow((a - 0.42) / 0.1, 2.0));
  }
  vec3 p = n * 2.4 + uCloudSeed + vec3(uCloudTime * 0.012, 0.0, uCloudTime * 0.004);
  /* swirls: warp sheared east-west, as rotation stretches weather into bands */
  vec3 w = vec3(snoise(p * 1.2), 0.35 * snoise(p * 1.2 + 11.0), snoise(p * 1.2 + 23.0));
  p += 0.6 * w;
  float f = 0.0, amp = 0.55, fr = 1.0;
  for (int i = 0; i < 6; i++) { if (float(i) >= uCloudOct) break; f += amp * snoise(p * fr * 2.0 * vec3(1.0, 1.6, 1.0)); fr *= 2.15; amp *= 0.5; }
  /* f is roughly normal around 0 with spread ~0.25: put the threshold where about cov of the area lies above it */
  /* a closed deck (hothouse) keeps faint banded structure instead of a blank disc */
  if (cov >= 0.999) return 0.82 + 0.18 * smoothstep(-0.4, 0.4, f + 0.3 * sin(n.y * 9.0));
  float th = 0.62 * (0.5 - clamp(cov, 0.0, 1.0));
  return smoothstep(th, th + 0.16, f) * smoothstep(th - 0.05, th + 0.5, f);
}
`;
const SURFACE = `
/* The surface's colour and roughness at a point (shared by the live terrain shader and the baker). Climate from the
 * climate model's latitudes (or the angle from the substellar point if locked), with the lapse rate; biomes only
 * where life is possible; ice only where there is water; roughness 0.06 marks open water. */
uniform float uSea, uRef, uK, uHasOcean;
uniform float uLatT[36]; uniform float uHasEBM;
uniform float uTs, uDT, uLocked, uLife, uRust, uRelief, uHasWater;
uniform vec3 uSubstellar;
float latT(float xx) {
  float f = (xx + 1.0) * 18.0 - 0.5;
  float i0 = clamp(floor(f), 0.0, 35.0), i1 = min(i0 + 1.0, 35.0);
  float w = clamp(f - i0, 0.0, 1.0);
  float a = 0.0, b = 0.0;
  for (int i = 0; i < 36; i++) { if (float(i) == i0) a = uLatT[i]; if (float(i) == i1) b = uLatT[i]; }
  return mix(a, b, w);
}
vec4 surfaceAlbedo(vec3 n, float h, float c, float slope) {
  bool water = uHasOcean > 0.5 && h < uSea;
  /* climate at this point: latitude (or angle from the substellar point if locked) and elevation lapse rate */
  float elevKm = max(h - uSea, 0.0) * uRelief / 1000.0;
  float T = uHasEBM > 0.5 ? latT(uLocked > 0.5 ? dot(n, uSubstellar) : n.y)
    : uLocked > 0.5 ? uTs + 55.0 * (dot(n, uSubstellar) - 0.25)
    : uTs + uDT * (1.0 / 3.0 - n.y * n.y);
  T -= 6.5 * elevKm;
  T += 3.0 * snoise(n * 5.0 + uOc);                       /* weather-scale variation of the ice edge */

  float tex = 0.85 + 0.3 * (0.5 + 0.5 * snoise(n * 140.0 + uOf)) * (0.6 + 0.4 * snoise(n * 31.0));
  vec3 col;
  float rough = 0.9;
  if (water) {
    float depthM = (uSea - h) * uRelief;
    vec3 deep = vec3(0.004, 0.018, 0.055), shallow = vec3(0.03, 0.16, 0.17);
    col = mix(deep, shallow, exp(-depthM / 160.0));
    rough = 0.06;
    if (T < -2.0) { col = vec3(0.78, 0.84, 0.9) * tex; rough = 0.5; }   /* sea ice */
  } else {
    /* moisture: wet near coasts, dry in continental interiors and the subtropical belts */
    float latS = abs(n.y);
    float m = 0.62 + 0.28 * snoise(n * 3.0 + uOe) + 0.12 * snoise(n * 11.0) - 0.22 * smoothstep(0.3, 0.7, c) - 0.4 * exp(-pow((latS - 0.4) / 0.1, 2.0));
    vec3 rock = vec3(0.16, 0.145, 0.13), basalt = vec3(0.09, 0.085, 0.08), dust = vec3(0.42, 0.2, 0.09);
    vec3 barren = mix(mix(rock, basalt, 0.5 + 0.5 * snoise(n * 20.0)), dust, uRust * (0.6 + 0.4 * snoise(n * 6.0 + uOd)));
    col = barren;
    if (uLife > 0.5) {
      vec3 desert = vec3(0.42, 0.3, 0.17), grass = vec3(0.13, 0.15, 0.055), forest = vec3(0.035, 0.07, 0.025),
           rain = vec3(0.02, 0.055, 0.015), tundra = vec3(0.17, 0.15, 0.11), boreal = vec3(0.03, 0.05, 0.03);
      vec3 veg = T > 20.0 ? mix(desert, mix(grass, rain, smoothstep(0.45, 0.75, m)), smoothstep(0.2, 0.45, m))
               : T > 5.0  ? mix(desert * 0.9, mix(grass, forest, smoothstep(0.4, 0.65, m)), smoothstep(0.15, 0.35, m))
               : T > -5.0 ? mix(tundra, boreal, smoothstep(0.35, 0.6, m))
               : tundra;
      col = mix(veg, rock, smoothstep(0.25, 0.5, slope));
      if (T < 0.0 || T > 45.0) col = mix(col, barren, 0.6);
    }
    /* beaches */
    if (uHasOcean > 0.5) col = mix(vec3(0.5, 0.43, 0.3), col, smoothstep(0.0, 40.0, (h - uSea) * uRelief));
    /* snow and ice, not on steep cliffs */
    float snow = smoothstep(-9.0, -15.0, T + 4.0 * snoise(n * 60.0 + uOd)) * (1.0 - smoothstep(0.3, 0.55, slope));
    /* no water, no snow: a dry world keeps only small polar caps of frost */
    if (uHasWater < 0.5) snow = smoothstep(0.9, 0.95, abs(n.y) + 0.02 * snoise(n * 30.0)) * smoothstep(-40.0, -60.0, T) * 0.9;
    col = mix(col * tex, vec3(0.86, 0.89, 0.93), snow);
    rough = mix(0.9, 0.6, snow);
  }

  return vec4(col, rough);
}
`;
const TERRAIN_FS = NOISE + TERRAIN + ATMO + CLOUDS + SURFACE + /* glsl */`
uniform float uCut, uCloudH, uCloudShadow;
uniform vec3 uSunLocal;
uniform vec3 uSunDir, uSunColor, uAtmColor;
uniform float uAtmDensity, uPixelAngle, uOctMax, uFDNormals, uFill;
uniform mat4 modelMatrix;
varying vec3 vN; varying vec3 vWP; varying vec3 vWN;

vec3 perturb(vec3 N, vec3 P, float H) {
  vec3 dpdx = dFdx(P), dpdy = dFdy(P);
  float dhdx = dFdx(H), dhdy = dFdy(H);
  vec3 r1 = cross(dpdy, N), r2 = cross(N, dpdx);
  float det = dot(dpdx, r1);
  vec3 grad = sign(det) * (dhdx * r1 + dhdy * r2);
  return normalize(abs(det) * N - grad);
}

void main() {
  if (uCut > 0.5 && vN.x > 0.0 && vN.z > 0.0) discard;
  vec3 n = normalize(vN);
  float dist = length(cameraPosition - vWP);
  /* octaves: add detail while its features stay larger than ~2 pixels */
  float px = dist * uPixelAngle;
  float oct = clamp(log2(1.0 / (9.0 * px * 2.5)), 2.0, uOctMax);
  float c, land, mm;
  float h = terrainH(n, oct, c, land, mm);
  bool water = uHasOcean > 0.5 && h < uSea;
  vec3 N0 = normalize(vWN);
  /* per-pixel normal from the height function itself (finite differences on the sphere, step ~1.5 pixels),
   * independent of the mesh, so no triangle facets show however coarse the LOD chunk is */
  vec3 N = N0;
  if (!water && uFDNormals > 0.5) {
    vec3 t1 = normalize(cross(n, abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
    vec3 t2 = cross(n, t1);
    float eps = max(px * 1.5, 2e-6);
    float o2 = max(oct - 1.0, 2.0);
    float ha = terrainH(normalize(n + t1 * eps), o2), hb = terrainH(normalize(n + t2 * eps), o2);
    float h0 = terrainH(n, o2);
    float k = uK / eps;
    vec3 nL = normalize(n - t1 * (max(ha, uSea) - max(h0, uSea)) * k - t2 * (max(hb, uSea) - max(h0, uSea)) * k);
    N = normalize(mat3(modelMatrix) * nL);
  } else if (!water) {
    N = perturb(N0, vWP, (max(h, uSea) - uRef) * uK);      /* cheaper: screen-space bump (may show mesh facets close up) */
  }
  vec3 L = normalize(uSunDir);
  vec3 V = normalize(cameraPosition - vWP);

  float slope = 1.0 - clamp(dot(N, N0), 0.0, 1.0);
  vec4 sa = surfaceAlbedo(n, h, c, slope);
  vec3 col = sa.rgb;
  float rough = sa.a;
  water = rough < 0.1;
  /* lighting: sunlight reddened by its path through the air, skylight, cloud shadows, and the ocean's glint.
   * The air between the ground and the eye is added by the atmosphere shell drawn on top. */
  float mu0 = dot(N0, L);
  vec3 sun = uSunColor * sunTrans(mu0);
  float shade = 1.0;
  if (uCloudShadow > 0.5 && mu0 > 0.0) shade = 1.0 - 0.6 * cloudDensity(normalize(n + uSunLocal * uCloudH / max(mu0, 0.15)));
  float NdL = max(dot(N, L), 0.0);
  vec3 lit = col * sun * NdL * shade;
  lit += col * uAtmColor * 0.08 * smoothstep(-0.2, 0.25, mu0);
  if (water) {
    vec3 Hh = normalize(L + V);
    float NdH = max(dot(N0, Hh), 0.0);
    float a2 = rough * rough;
    float D = a2 / (3.14159 * pow(NdH * NdH * (a2 - 1.0) + 1.0, 2.0));
    float F = 0.02 + 0.98 * pow(1.0 - max(dot(V, N0), 0.0), 5.0);
    lit += sun * shade * min(D * F, 6.0) * NdL * 0.5;
    lit += uAtmColor * F * 0.2 * smoothstep(-0.1, 0.2, mu0);
  }
  lit += col * uFill;                                     /* optional light on the night side, for looking */
  gl_FragColor = vec4(lit, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

/* The atmosphere: single scattering by molecules (Rayleigh, per colour channel) and by haze or dust (Mie,
 * Henyey-Greenstein), marched along the view ray through an exponential shell, with the sunlight at each sample
 * attenuated by its own path (air mass) and shadowed by the planet. The colour of the sky, the blue limb, the red
 * terminator, a hazy hothouse and a thin dusty sky all come from the same model and the design's air and star.
 * Drawn after the surface with premultiplied alpha: it adds the scattered light and dims what lies behind by the
 * transmittance, so a thick atmosphere hides the ground. Heights exaggerated as the terrain (stated on screen). */
const ATM_VS = `varying vec3 vWP; void main() { vWP = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0); }`;
const ATM_FS = `
uniform vec3 uCenter, uSunDir, uSunColor, uTauR, uMieTint, uMieColor; uniform float uR0, uR1, uHR, uHM, uTauM, uSteps, uGain, uCut, uG;
uniform mat3 uToLocal;
varying vec3 vWP;
void main() {
  vec3 ro = cameraPosition, rd = normalize(vWP - cameraPosition);
  vec3 oc = ro - uCenter;
  float b = dot(oc, rd);
  float disc = b * b - (dot(oc, oc) - uR1 * uR1);
  if (disc < 0.0) discard;
  float t0 = max(-b - sqrt(disc), 0.0), t1 = -b + sqrt(disc);
  float d0 = b * b - (dot(oc, oc) - uR0 * uR0);
  if (d0 > 0.0) {
    float tp = -b - sqrt(d0);
    /* the cutaway wedge: let the ray pass into the open quadrant */
    vec3 hp = uToLocal * (oc + rd * tp);
    bool inCut = uCut > 0.5 && hp.x > 0.0 && hp.z > 0.0;
    if (inCut && tp > 0.0) discard;                                  /* the wedge is cut out of the air as well */
    if (tp > 0.0) t1 = min(t1, tp);
  }
  if (t1 <= t0) discard;
  vec3 L = normalize(uSunDir);
  float cosT = dot(rd, L);
  float pR = 0.0597 * (1.0 + cosT * cosT);                          /* 3/(16 pi) (1 + cos^2) */
  float g = uG;
  float pM = 0.0796 * (1.0 - g * g) / pow(1.0 + g * g - 2.0 * g * cosT, 1.5);   /* Henyey-Greenstein / (4 pi) */
  vec3 bR = uTauR / uHR, bM = uTauM * uMieTint / uHM;              /* extinction coefficients at the ground */
  float dt = (t1 - t0) / uSteps;
  vec3 tauV = vec3(0.0), sum = vec3(0.0);
  /* a per-pixel offset of the sample positions turns the step bands into fine, invisible noise */
  float jit = 0.3 + 0.4 * fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
  for (int i = 0; i < 24; i++) {
    if (float(i) >= uSteps) break;
    vec3 p = oc + rd * (t0 + (float(i) + jit) * dt);
    float r = length(p);
    if (r < uR0) break;                                             /* into the ground (or the cutaway's open wedge) */
    float h = r - uR0;
    float dR = exp(-h / uHR), dM = exp(-h / uHM);
    tauV += (bR * dR + bM * dM) * dt;
    float mu = dot(p / r, L);
    /* planet shadow, then the air mass along the sun's path from this height */
    float shadowMu = -sqrt(max(0.0, 1.0 - (uR0 / r) * (uR0 / r)));
    if (mu < shadowMu) continue;
    float am = 1.0 / max(mu + 0.15 * pow(max(93.885 - degrees(acos(clamp(mu, -1.0, 1.0))), 0.01), -1.253), 0.025);
    vec3 tauS = (uTauR * dR + uTauM * uMieTint * dM) * min(am, 40.0);
    sum += (bR * dR * pR + bM * uMieColor * dM * pM) * exp(-(tauV + tauS)) * dt;
  }
  vec3 T = exp(-tauV);
  float alpha = 1.0 - (T.r + T.g + T.b) / 3.0;
  gl_FragColor = vec4(sum * uSunColor * uGain, alpha);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

/* The cloud layer: a transparent shell above the ground, lit by reddened sunlight with a soft wrap and skylight. */
const CLOUD_VS = `varying vec3 vN; varying vec3 vWN; void main() { vN = normalize(position); vWN = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`;
const CLOUD_FS = NOISE + ATMO + CLOUDS + /* glsl */`
uniform vec3 uSunDir, uSunColor, uAtmColor, uCloudTint; uniform float uCut, uUseCloudTex, uCloudShift;
uniform sampler2D uCloudTex;
vec2 equiUvC(vec3 n) { return vec2(atan(n.z, n.x) / 6.2831853 + 0.5, asin(clamp(n.y, -1.0, 1.0)) / 3.1415927 + 0.5); }
varying vec3 vN; varying vec3 vWN;
void main() {
  if (uCut > 0.5 && vN.x > 0.0 && vN.z > 0.0) discard;
  float d;
  if (uUseCloudTex > 0.5) { vec2 cuv = equiUvC(normalize(vN)); d = textureLod(uCloudTex, vec2(fract(cuv.x + uCloudShift), cuv.y), 0.0).r; }
  else d = cloudDensity(vN);
  if (d < 0.02) discard;
  vec3 N = normalize(vWN), L = normalize(uSunDir);
  float mu = dot(N, L);
  vec3 sun = uSunColor * sunTrans(mu);
  float wrap = clamp(mu * 0.75 + 0.25, 0.0, 1.0);
  vec3 col = uCloudTint * (sun * wrap * (0.75 + 0.25 * d) + uAtmColor * 0.12 * smoothstep(-0.2, 0.2, mu));
  gl_FragColor = vec4(col * d, d * 0.95);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

/* Cut faces of the cutaway: half-discs in the local planes x = 0 (z > 0) and z = 0 (x > 0), coloured by layer. */
const CAP_FS = NOISE + TERRAIN + /* glsl */`
uniform float uSea, uRef, uK, uTime, uHasBake;
uniform sampler2D uTexB;
uniform float uRi, uRc, uRm, uRr;   /* inner core, core, mantle top (base of crust), rock surface: radii in planet radii */
uniform vec3 uSunDir, uSunColor;
varying vec3 vLP; varying vec3 vWN;
void main() {
  float r = length(vLP);
  vec3 dir = vLP / max(r, 1e-6);
  /* the surface's edge: from the baked height texture when there is one (cheap), else from the height function */
  float surf, rock;
  if (uHasBake > 0.5) {
    vec2 uv = vec2(atan(dir.z, dir.x) / 6.2831853 + 0.5, asin(clamp(dir.y, -1.0, 1.0)) / 3.1415927 + 0.5);
    surf = 1.0 + textureLod(uTexB, uv, 0.0).a; rock = surf;
  } else {
    float h = terrainH(dir, 4.0);
    surf = 1.0 + (max(h, uSea) - uRef) * uK; rock = 1.0 + (h - uRef) * uK;
  }
  if (r > surf) discard;
  float t = uTime;
  vec3 col; float glow = 0.0;
  float edge = 0.004;
  if (r < uRi) {
    col = vec3(1.0, 0.85, 0.55) * (1.25 + 0.15 * snoise(vLP * 9.0 + t * 0.05)); glow = 1.0;
  } else if (r < uRc) {
    float f = snoise(vec3(vLP.xy * 6.0, t * 0.08) + vLP.z * 4.0);
    col = mix(vec3(0.9, 0.28, 0.04), vec3(1.25, 0.55, 0.1), 0.5 + 0.5 * f); glow = 1.0;
  } else if (r < uRm) {
    float x = (r - uRc) / max(uRm - uRc, 1e-4);
    float plume = 0.5 + 0.5 * snoise(vec3(atan(vLP.y, length(vLP.xz)) * 5.0, x * 3.0 - t * 0.03, r * 2.0));
    col = mix(vec3(0.42, 0.09, 0.025), vec3(0.11, 0.045, 0.03), pow(x, 0.7)) * (0.7 + 0.6 * plume);
    glow = 0.8 * (1.0 - x);
  } else if (r < rock) {
    col = vec3(0.32, 0.28, 0.24) * (0.8 + 0.2 * snoise(vLP * 300.0));
  } else {
    col = vec3(0.02, 0.09, 0.2);   /* ocean in section */
  }
  /* thin dark boundary lines between layers */
  float line = min(min(abs(r - uRi), abs(r - uRc)), min(abs(r - uRm), abs(r - rock)));
  col *= mix(0.35, 1.0, smoothstep(0.0, edge, line));
  float NdL = max(dot(normalize(vWN), normalize(uSunDir)), 0.0);
  vec3 lit = col * (glow + (1.0 - glow) * (0.25 + NdL)) * mix(vec3(1.0), uSunColor, 1.0 - glow);
  gl_FragColor = vec4(lit, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const CAP_VS = `varying vec3 vLP; varying vec3 vWN; void main() { vLP = position; vWN = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`;

/* Other planet types: banded giants, icy and rocky bodies, on a plain sphere. */
const OTHER_FS = NOISE + /* glsl */`
uniform float uType; uniform vec3 uSunDir, uSunColor, uSeed; varying vec3 vN; varying vec3 vWN; varying vec3 vWP;
void main() {
  vec3 n = normalize(vN);
  vec3 col;
  if (uType < 0.5) {            /* rocky */
    float f = 0.5 + 0.5 * snoise(n * 4.0 + uSeed) * 0.6 + 0.25 * snoise(n * 16.0);
    col = mix(vec3(0.12, 0.11, 0.1), vec3(0.36, 0.33, 0.3), f);
  } else if (uType < 1.5) {     /* icy */
    float cr = 1.0 - abs(snoise(n * 7.0 + uSeed));
    col = mix(vec3(0.82, 0.86, 0.9), vec3(0.55, 0.45, 0.38), pow(cr, 12.0) * 0.8);
  } else {                      /* giants: latitude bands, warped by turbulence */
    vec3 w = n + vec3(0.0, 0.025 * snoise(n * vec3(4.0, 9.0, 4.0) + uSeed), 0.0);
    float lat = w.y;
    float band = 0.5 + 0.5 * sin(lat * 19.0 + 1.2 * snoise(vec3(lat * 3.0, 0.0, 1.0) + uSeed));
    band += 0.25 * snoise(vec3(w.x * 2.0, lat * 14.0, w.z * 2.0) + uSeed) * (1.0 - abs(band * 2.0 - 1.0));
    float storm = smoothstep(0.75, 0.9, snoise(n * 3.0 + uSeed + 3.0));
    float fine = snoise(vec3(w.x * 6.0, lat * 30.0, w.z * 6.0) + uSeed) * 0.5 + 0.5;
    col = uType < 2.5
      ? mix(vec3(0.16, 0.36, 0.52), vec3(0.3, 0.55, 0.68), band * 0.5 + fine * 0.15)                                  /* ice giant */
      : mix(vec3(0.42, 0.24, 0.13), vec3(0.78, 0.7, 0.56), smoothstep(0.2, 0.8, band) * (0.88 + 0.18 * fine));       /* gas giant */
    col = mix(col, uType < 2.5 ? vec3(0.08, 0.14, 0.32) : vec3(0.62, 0.26, 0.14), storm * 0.75);
  }
  vec3 N = normalize(vWN);
  float NdL = max(dot(N, normalize(uSunDir)), 0.0);
  vec3 V = normalize(cameraPosition - vWP);
  if (uType > 1.5) col *= pow(max(dot(N, V), 0.0), 0.35);      /* limb darkening of a deep atmosphere */
  float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * (uType > 1.5 ? 0.25 : 0.0);
  gl_FragColor = vec4(col * uSunColor * NdL + rim * uSunColor * NdL * vec3(0.6, 0.8, 1.0), 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;
const OTHER_VS = `varying vec3 vN; varying vec3 vWN; varying vec3 vWP; void main() { vN = position; vWN = normalize(mat3(modelMatrix) * normal);
  vWP = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0); }`;


/* ---------- The baked planet (Quest 3 and the "low" setting) ----------
 * The live terrain shader evaluates the whole height function for every pixel of every frame: fine on a PC graphics
 * card, too much for a headset at 3.5 m and two eyes (it brought the Quest's graphics down). Here the surface is
 * evaluated once per design change into equirectangular textures (colour and roughness; local normal and the
 * displaced height), a band of rows per frame so no single frame is heavy, and drawn on a plain sphere. */
const BAKE_W = 2048, BAKE_H = 1024, BAKE_ROWS = 16;      /* rows per frame: small, so no frame gets heavy on a headset */
const BAKE_VS = `varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }`;
const BAKE_FS = NOISE + TERRAIN + SURFACE + /* glsl */`
uniform float uPass;
varying vec2 vUv;
void main() {
  float lon = (vUv.x - 0.5) * 6.2831853, lat = (vUv.y - 0.5) * 3.1415927;
  vec3 n = vec3(cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon));
  float c, land, mm;
  float h = terrainH(n, 9.0, c, land, mm);
  vec3 t1 = normalize(cross(n, abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)));
  vec3 t2 = cross(n, t1);
  float eps = 3.1415927 / float(${BAKE_H});
  float ha = terrainH(normalize(n + t1 * eps), 9.0), hb = terrainH(normalize(n + t2 * eps), 9.0);
  float k = uK / eps;
  vec3 nL = normalize(n - t1 * (max(ha, uSea) - max(h, uSea)) * k - t2 * (max(hb, uSea) - max(h, uSea)) * k);
  float slope = 1.0 - clamp(dot(nL, n), 0.0, 1.0);
  if (uPass < 0.5) gl_FragColor = surfaceAlbedo(n, h, c, slope);
  else gl_FragColor = vec4(nL, (max(h, uSea) - uRef) * uK);
}`;
const CLOUDBAKE_FS = NOISE + CLOUDS + /* glsl */`
varying vec2 vUv;
void main() {
  float lon = (vUv.x - 0.5) * 6.2831853, lat = (vUv.y - 0.5) * 3.1415927;
  gl_FragColor = vec4(cloudDensity(vec3(cos(lat) * cos(lon), sin(lat), cos(lat) * sin(lon))), 0.0, 0.0, 1.0);
}`;
/* equirect lookup without the seam: textureLod at level 0 (no mip maps, so no seam line across the sphere) */
const EQUI = /* glsl */`
vec2 equiUv(vec3 n) { return vec2(atan(n.z, n.x) / 6.2831853 + 0.5, asin(clamp(n.y, -1.0, 1.0)) / 3.1415927 + 0.5); }
`;
const BAKEDPLANET_VS = EQUI + /* glsl */`
uniform sampler2D uTexB; uniform float uHasBake;
varying vec3 vN; varying vec3 vWP; varying vec3 vWN;
void main() {
  vec3 n = normalize(position);
  float h = uHasBake > 0.5 ? textureLod(uTexB, equiUv(n), 0.0).a : 0.0;
  vN = n;
  vWN = normalize(mat3(modelMatrix) * n);
  vWP = (modelMatrix * vec4(n * (1.0 + h), 1.0)).xyz;
  gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0);
}`;
const BAKEDPLANET_FS = EQUI + ATMO + /* glsl */`
uniform sampler2D uTexA, uTexB, uCloudTex; uniform float uHasBake, uCut, uCloudShift, uCloudShadow, uCloudH, uFill;
uniform vec3 uSunDir, uSunColor, uAtmColor, uSunLocal;
uniform mat4 modelMatrix;
varying vec3 vN; varying vec3 vWP; varying vec3 vWN;
void main() {
  if (uCut > 0.5 && vN.x > 0.0 && vN.z > 0.0) discard;
  vec3 n = normalize(vN);
  vec2 uv = equiUv(n);
  vec4 A = uHasBake > 0.5 ? textureLod(uTexA, uv, 0.0) : vec4(0.2, 0.22, 0.25, 0.9);
  vec3 nL = uHasBake > 0.5 ? normalize(textureLod(uTexB, uv, 0.0).xyz) : n;
  vec3 N0 = normalize(vWN), N = normalize(mat3(modelMatrix) * nL);
  vec3 L = normalize(uSunDir), V = normalize(cameraPosition - vWP);
  bool water = A.a < 0.1;
  float mu0 = dot(N0, L);
  vec3 sun = uSunColor * sunTrans(mu0);
  float shade = 1.0;
  if (uCloudShadow > 0.5 && mu0 > 0.0) {
    vec2 cuv = equiUv(normalize(n + uSunLocal * uCloudH / max(mu0, 0.15)));
    shade = 1.0 - 0.6 * textureLod(uCloudTex, vec2(fract(cuv.x + uCloudShift), cuv.y), 0.0).r;
  }
  float NdL = max(dot(N, L), 0.0);
  vec3 lit = A.rgb * sun * NdL * shade + A.rgb * uAtmColor * 0.08 * smoothstep(-0.2, 0.25, mu0) + A.rgb * uFill;
  if (water) {
    vec3 Hh = normalize(L + V);
    float NdH = max(dot(N0, Hh), 0.0);
    float a2 = 0.0036;
    float D = a2 / (3.14159 * pow(NdH * NdH * (a2 - 1.0) + 1.0, 2.0));
    float F = 0.02 + 0.98 * pow(1.0 - max(dot(V, N0), 0.0), 5.0);
    lit += sun * shade * min(D * F, 6.0) * NdL * 0.5 + uAtmColor * F * 0.2 * smoothstep(-0.1, 0.2, mu0);
  }
  gl_FragColor = vec4(lit, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

export function createPlanet() {
  const root = new THREE.Group();          /* tilted by obliquity */
  const spin = new THREE.Group();          /* turns about the local y axis */
  root.add(spin);
  const common = COMMON_UNIFORMS();
  /* the air (shared by terrain, clouds and the atmosphere shell) and the clouds (shared by terrain and clouds) */
  const airU = { uTauR: { value: new THREE.Vector3(0.046, 0.108, 0.265) }, uTauM: { value: 0.08 }, uMieTint: { value: new THREE.Vector3(1, 1, 1) },
    uMieColor: { value: new THREE.Vector3(1, 1, 1) } };
  const fillU = { uFill: { value: 0 } };
  const bakedU = { uTexA: { value: null }, uTexB: { value: null }, uCloudTex: { value: null }, uHasBake: { value: 0 },
    uUseCloudTex: { value: 0 }, uCloudShift: { value: 0 } };
  const cloudU = { uCloudFrac: { value: 0.6 }, uCloudTime: { value: 0 }, uCloudOct: { value: 5 }, uCloudLocked: { value: 0 },
    uCloudSeed: { value: new THREE.Vector3(3, 1, 4) }, uCloudSub: { value: new THREE.Vector3(1, 0, 0) } };
  const light = {
    uSunDir: { value: new THREE.Vector3(1, 0.25, 0.4).normalize() }, uSunColor: { value: new THREE.Color(1, 1, 1) },
    uAtmColor: { value: new THREE.Color(0.3, 0.5, 1.0) },
  };
  const terrainMat = new THREE.ShaderMaterial({
    uniforms: {
      ...common, ...light,
      uFaceN: { value: new THREE.Vector3() }, uFaceU: { value: new THREE.Vector3() }, uFaceV: { value: new THREE.Vector3() },
      uOff: { value: new THREE.Vector2() }, uScale: { value: 1 }, uVertOct: { value: 3 }, uSkirtDepth: { value: 0.01 },
      uTs: { value: 15 }, uDT: { value: 45 }, uLocked: { value: 0 }, uLife: { value: 1 }, uRust: { value: 0 },
      uRelief: { value: 4500 }, uAtmDensity: { value: 1 }, uPixelAngle: { value: 0.001 }, uHasWater: { value: 1 }, uOctMax: { value: 13 }, uFDNormals: { value: 1 },
      uSubstellar: { value: new THREE.Vector3(1, 0, 0) },
      ...airU, ...cloudU, uLatT: { value: new Array(36).fill(15) }, uHasEBM: { value: 0 }, uCloudH: { value: 0.01 },
      uCloudShadow: { value: 1 }, uSunLocal: { value: new THREE.Vector3(1, 0, 0) }, ...fillU,
    },
    vertexShader: TERRAIN_VS, fragmentShader: TERRAIN_FS,
    extensions: { derivatives: true },
  });

  const geo = chunkGeometry();
  const pool = [];
  let used = 0;
  function getMesh() {
    if (used < pool.length) return pool[used++];
    const m = new THREE.Mesh(geo, terrainMat);
    m.frustumCulled = false;
    m.onBeforeRender = () => {
      const u = terrainMat.uniforms, nd = m.userData.node;
      u.uFaceN.value.copy(FACES[nd.face][0]); u.uFaceU.value.copy(FACES[nd.face][1]); u.uFaceV.value.copy(FACES[nd.face][2]);
      u.uOff.value.set(nd.x * nd.size, nd.y * nd.size); u.uScale.value = nd.size;
      u.uVertOct.value = Math.min(9, 3 + nd.level * 0.75);
      u.uSkirtDepth.value = nd.size * 0.06 + maxRelief * 0.15;
      terrainMat.uniformsNeedUpdate = true;
    };
    pool.push(m); used++;
    return m;
  }

  /* LOD selection, in planet-local coordinates */
  const camLocal = new THREE.Vector3();
  const tmp = new THREE.Vector3();
  const leaves = [];
  function visit(face, level, x, y, maxRelief) {
    const size = 1 / (1 << level);
    const F = FACES[face];
    const cu = (x + 0.5) * size * 2 - 1, cv = (y + 0.5) * size * 2 - 1;
    tmp.copy(F[0]).addScaledVector(F[1], cu).addScaledVector(F[2], cv);
    const center = cubeToSphere(tmp);
    const arc = size * 2 * 0.9;                       /* chunk extent on the sphere, roughly */
    const camR = camLocal.length();
    /* horizon culling: is any part of the chunk above the horizon seen from the camera? */
    const ang = Math.acos(Math.min(1, Math.max(-1, center.dot(camLocal) / camR)));
    const horizon = Math.acos(Math.min(1, 1 / camR)) + Math.acos(Math.min(1, 1 / (1 + maxRelief)));
    if (camR > 1.0001 && ang - arc > horizon + 0.02) return;
    const dist = center.distanceTo(camLocal);
    if (level < MAX_LEVEL && dist < arc * LOD_SPLIT) {
      for (let k = 0; k < 4; k++) visit(face, level + 1, x * 2 + (k & 1), y * 2 + (k >> 1), maxRelief);
    } else leaves.push({ face, level, x, y, size });
  }

  let maxRelief = 0.05;
  function updateLOD(camera) {
    camLocal.copy(camera.position);
    spin.worldToLocal(camLocal);
    leaves.length = 0;
    for (let f = 0; f < 6; f++) visit(f, 0, 0, 0, maxRelief);
    used = 0;
    for (const m of pool) m.visible = false;
    for (const nd of leaves) { const m = getMesh(); m.userData.node = nd; m.visible = true; if (!m.parent) spin.add(m); }
    return leaves.length;
  }

  /* atmosphere shell */
  const atmMat = new THREE.ShaderMaterial({
    uniforms: { ...light, ...airU, uCenter: { value: new THREE.Vector3() }, uR0: { value: 1 }, uR1: { value: 1.05 },
      uHR: { value: 0.01 }, uHM: { value: 0.002 }, uSteps: { value: 16 }, uGain: { value: 9 }, uG: { value: 0.76 }, uCut: common.uCut,
      uToLocal: { value: new THREE.Matrix3() } },
    vertexShader: ATM_VS, fragmentShader: ATM_FS, side: THREE.BackSide, transparent: true, depthWrite: false, depthTest: false,
    blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor,
    blendSrcAlpha: THREE.ZeroFactor, blendDstAlpha: THREE.OneFactor,
  });
  const atm = new THREE.Mesh(new THREE.SphereGeometry(1, 96, 48), atmMat);
  const atmLocal = { HR: 0.01, HM: 0.002, R1: 1.05 };
  const wScale = new THREE.Vector3();
  atm.renderOrder = 10;
  root.add(atm);

  /* clouds */
  const cloudMat = new THREE.ShaderMaterial({
    uniforms: { ...light, ...airU, ...cloudU, uCloudTint: { value: new THREE.Color(1, 1, 1) }, uCut: common.uCut,
      uUseCloudTex: bakedU.uUseCloudTex, uCloudTex: bakedU.uCloudTex, uCloudShift: bakedU.uCloudShift },
    vertexShader: CLOUD_VS, fragmentShader: CLOUD_FS, transparent: true, depthWrite: false,
    blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor,
  });
  const clouds = new THREE.Mesh(new THREE.SphereGeometry(1, 160, 80), cloudMat);
  clouds.renderOrder = 5;
  spin.add(clouds);

  /* the baked planet and its baker */
  const bakedMat = new THREE.ShaderMaterial({
    uniforms: { ...light, ...airU, ...bakedU, ...fillU, uCut: common.uCut, uCloudShadow: { value: 1 }, uCloudH: { value: 0.01 },
      uSunLocal: { value: new THREE.Vector3(1, 0, 0) } },
    vertexShader: BAKEDPLANET_VS, fragmentShader: BAKEDPLANET_FS,
  });
  const baked = new THREE.Mesh(new THREE.SphereGeometry(1, 384, 192), bakedMat);
  baked.visible = false;
  spin.add(baked);
  let useBaked = false;
  const bake = { key: null, row: BAKE_H, pass: 0, cloudsDone: true, rtA: null, rtB: null, rtC: null, scene: null, cam: null, mat: null, cmat: null, quad: null };
  function bakeTargets() {
    if (bake.rtA) return;
    const opt = { type: THREE.HalfFloatType, minFilter: THREE.LinearFilter, magFilter: THREE.LinearFilter, depthBuffer: false,
      wrapS: THREE.RepeatWrapping, generateMipmaps: false };
    bake.rtA = new THREE.WebGLRenderTarget(BAKE_W, BAKE_H, opt);
    bake.rtB = new THREE.WebGLRenderTarget(BAKE_W, BAKE_H, opt);
    bake.rtC = new THREE.WebGLRenderTarget(BAKE_W / 2, BAKE_H / 2, opt);
    for (const rt of [bake.rtA, bake.rtB, bake.rtC]) rt.texture.wrapS = THREE.RepeatWrapping;
    bake.mat = new THREE.ShaderMaterial({ uniforms: { ...terrainMat.uniforms, uPass: { value: 0 } }, vertexShader: BAKE_VS, fragmentShader: BAKE_FS, depthTest: false, depthWrite: false });
    bake.cmat = new THREE.ShaderMaterial({ uniforms: { ...cloudU }, vertexShader: BAKE_VS, fragmentShader: CLOUDBAKE_FS, depthTest: false, depthWrite: false });
    bake.quad = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), bake.mat);
    bake.quad.frustumCulled = false;
    bake.scene = new THREE.Scene(); bake.scene.add(bake.quad);
    bake.cam = new THREE.OrthographicCamera(-1, 1, 1, -1, 0, 1);
    bakedU.uTexA.value = bake.rtA.texture; bakedU.uTexB.value = bake.rtB.texture; bakedU.uCloudTex.value = bake.rtC.texture;
  }
  /* Restart the bake when anything that shapes the surface changes. */
  function bakeKey() {
    const u = terrainMat.uniforms;
    return JSON.stringify([common.uOa.value, common.uOf.value, common.uSea.value, common.uRef.value, common.uK.value, common.uHasOcean.value,
      u.uHasEBM.value, u.uLatT.value.map((x) => Math.round(x * 10)), u.uTs.value, u.uDT.value, u.uLocked.value, u.uLife.value, u.uRust.value,
      u.uRelief.value, u.uHasWater.value, cloudU.uCloudFrac.value, cloudU.uCloudLocked.value, cloudU.uCloudSeed.value]);
  }
  /* Bake a band of rows; call once per frame before rendering. Returns true while work remains. */
  function bakeStep(renderer) {
    if (!useBaked || !isHz) return false;
    const key = bakeKey();
    if (key !== bake.key) { bakeTargets(); bake.key = key; bake.row = 0; bake.pass = 0; bake.cloudsDone = false; }
    if (bake.row >= BAKE_H && bake.cloudsDone) return false;
    const prevTarget = renderer.getRenderTarget(), prevXR = renderer.xr.enabled;
    renderer.xr.enabled = false;                          /* render the bake as a plain off-screen pass */
    if (bake.row < BAKE_H) {
      const rt = bake.pass === 0 ? bake.rtA : bake.rtB;
      bake.quad.material = bake.mat;
      bake.mat.uniforms.uPass.value = bake.pass;
      rt.scissorTest = true; rt.scissor.set(0, bake.row, BAKE_W, BAKE_ROWS); rt.viewport.set(0, 0, BAKE_W, BAKE_H);
      renderer.setRenderTarget(rt); renderer.render(bake.scene, bake.cam);
      if (bake.pass === 0) bake.pass = 1; else { bake.pass = 0; bake.row += BAKE_ROWS; }
      if (bake.row >= BAKE_H) bakedU.uHasBake.value = 1;
    } else {
      bake.quad.material = bake.cmat;
      renderer.setRenderTarget(bake.rtC); renderer.render(bake.scene, bake.cam);
      bake.cloudsDone = true;
    }
    renderer.setRenderTarget(prevTarget);
    renderer.xr.enabled = prevXR;
    return true;
  }

  /* cutaway caps */
  const capMat = new THREE.ShaderMaterial({
    uniforms: { ...common, ...light, uHasBake: bakedU.uHasBake, uTexB: bakedU.uTexB, uTime: { value: 0 }, uRi: { value: 0.19 }, uRc: { value: 0.55 }, uRm: { value: 0.99 }, uRr: { value: 1 } },
    vertexShader: CAP_VS, fragmentShader: CAP_FS, side: THREE.DoubleSide, extensions: { derivatives: true },
  });
  /* geometry is built in planet-local coordinates (the shader reads the position as the local point) */
  const gA = new THREE.CircleGeometry(1.2, 256, -Math.PI / 2, Math.PI);
  gA.rotateY(-Math.PI / 2);              /* plane x = 0, the half with z > 0 */
  const capA = new THREE.Mesh(gA, capMat);
  const capB = new THREE.Mesh(new THREE.CircleGeometry(1.2, 256, -Math.PI / 2, Math.PI), capMat);   /* plane z = 0, x > 0 */
  const caps = new THREE.Group();
  caps.add(capA, capB);
  caps.visible = false;
  spin.add(caps);

  /* other planet types */
  const otherMat = new THREE.ShaderMaterial({
    uniforms: { ...light, uType: { value: 0 }, uSeed: { value: new THREE.Vector3(1, 2, 3) } },
    vertexShader: OTHER_VS, fragmentShader: OTHER_FS,
  });
  const other = new THREE.Mesh(new THREE.SphereGeometry(1, 128, 64), otherMat);
  other.visible = false;
  spin.add(other);

  let isHz = true;
  const info = { exag: 10, landTop: 0, layers: null, kRadius: 1 };

  function set(P, design, D, { exag = 10, cut = false } = {}) {
    isHz = P.role === 'hz';
    other.visible = !isHz;
    caps.visible = isHz && cut;
    atm.visible = isHz && P.pBar > 0.0005;
    clouds.visible = isHz && P.cloudFrac > 0.01;
    for (const m of pool) if (!isHz) m.visible = false;
    const rgb = D.star.rgb;
    const intensity = 1.7 * Math.min(1.8, Math.max(0.25, (P.fluxMean || P.flux) ** 0.25));
    light.uSunColor.value.setRGB(rgb[0] * intensity, rgb[1] * intensity, rgb[2] * intensity);
    if (!isHz) {
      otherMat.uniforms.uType.value = { rocky: 0, icy: 1, iceGiant: 2, gasGiant: 3 }[P.type] ?? 0;
      root.rotation.set(0, 0, 0.05);
      return;
    }
    const pl = design.planets.find((q) => q.id === P.id);
    const O = seedOffsets(P.terrainSeed);
    for (const k of ['a', 'b', 'c', 'd', 'e', 'f']) common['uO' + k].value.set(...O[k]);
    const Rm = P.radiusEarth * 6.371e6;
    const k = P.reliefM / Rm * exag;
    common.uK.value = k;
    const hasOcean = P.oceans > 0;
    common.uHasOcean.value = hasOcean ? 1 : 0;
    common.uSea.value = P.sea.level;
    common.uRef.value = hasOcean ? P.sea.level : (P.sea.minH + P.sea.maxH) / 2;
    maxRelief = (P.sea.maxH - common.uRef.value) * k + 0.002;
    common.uCut.value = cut ? 1 : 0;
    const u = terrainMat.uniforms;
    u.uTs.value = P.climate.runaway ? 400 : P.TsC;
    u.uDT.value = 48 * (1 - 0.6 * Math.sin(Math.min(P.obliquityDeg, 90) * Math.PI / 180) ** 2) * Math.min(1.3, 1 / Math.max(0.4, P.pBar) ** 0.15);
    u.uLocked.value = P.locked ? 1 : 0;
    u.uLife.value = P.liquidWater && P.oceans > 0 ? 1 : 0;
    u.uRust.value = P.oceans <= 0 && P.pBar < 0.1 ? 1 : P.oceans <= 0 ? 0.4 : 0;
    u.uRelief.value = P.reliefM;
    u.uHasWater.value = P.oceans > 0.001 || P.waterFrac > 1e-4 ? 1 : 0;
    /* The air for the picture. Vertical Rayleigh optical depth at 680/550/440 nm is Earth's (0.046, 0.108, 0.265)
     * scaled by the column mass (P/g) and by how strongly the gases scatter per kilogram relative to air
     * (CO2 ~1.6, O2 ~0.85, Ar ~0.83, H2O ~0.6: per-molecule cross-sections divided by molar mass, approximate).
     * Haze: Earth's aerosol ~0.08; a dry world with air carries dust (0.4, reddish: it absorbs blue); a runaway
     * hothouse is a deck of haze. Scale heights from the physics, exaggerated like the terrain, capped. */
    const col = (P.pBar * 1e5 / P.g) / (101325 / 9.80665);
    const pp = P.atm.p, tot = Math.max(P.pBar, 1e-9);
    const kGas = ((pp.N2 || 0) * 1 + (pp.O2 || 0) * 0.85 + (pp.CO2 || 0) * 1.6 + (pp.Ar || 0) * 0.83 + (pp.H2O || 0) * 0.6 + (pp.CH4 || 0) * 2.0) / tot;
    /* very thick air: past an optical depth of a few the sky is saturated anyway; capping it keeps the limb from
     * splitting into coloured fringes (a rendering limit, not physics) */
    /* under a haze deck (runaway) the haze, not the molecules, sets the colour seen from space */
    const tR = P.climate.runaway ? Math.min(col * kGas, 3) : Math.min(col * kGas, 12);
    airU.uTauR.value.set(0.046 * tR, 0.108 * tR, 0.265 * tR);
    const dry = P.oceans <= 0;
    /* Mie: extinction tint (dust absorbs blue a little more) and the colour of the light it scatters */
    let tM = 0.08 * Math.sqrt(Math.min(col, 4)), tint = [1, 1, 1], mcol = [1, 1, 1];
    if (dry && P.pBar > 0.002) { tM = 0.4; tint = [0.9, 0.95, 1.08]; mcol = [1.0, 0.72, 0.48]; }
    /* a haze deck: single scattering alone leaves it dark, because most light in a thick, bright haze is scattered
     * many times; we stand in for that with a broad phase function and a brightened haze colour (an approximation) */
    if (P.climate.runaway) { tM = 2.5; tint = [1, 1, 1]; mcol = [3.2, 2.8, 2.0]; }
    airU.uTauM.value = tM; airU.uMieTint.value.set(...tint); airU.uMieColor.value.set(...mcol);
    atmMat.uniforms.uG.value = P.climate.runaway ? 0.25 : 0.76;
    const Hs = Math.min(0.03, Math.max(0.0025, P.scaleHeightKm / (Rm / 1000) * exag));
    /* planet-local sizes; tick() turns them into world units (the planet is 1 here, 1.75 m in the VR cradle) */
    atmLocal.HR = Hs; atmLocal.HM = Hs * (P.climate.runaway ? 0.8 : 0.25);
    atmLocal.R1 = 1.0 + Math.min(Hs * 7, 0.09);
    atm.scale.setScalar(atmLocal.R1);
    /* skylight colour for the ground: the scattered fraction of each channel, tinted by the star */
    const sk = [0, 1, 2].map((i) => (1 - Math.exp(-airU.uTauR.value.getComponent(i) - tM * tint[i] * 0.5)));
    light.uAtmColor.value.setRGB(sk[0] * rgb[0], sk[1] * rgb[1], sk[2] * rgb[2]);
    u.uAtmDensity.value = Math.min(1.6, Math.log10(1 + P.pBar * 9) * 0.9);
    /* clouds */
    cloudU.uCloudFrac.value = P.cloudFrac;
    cloudU.uCloudLocked.value = P.locked ? 1 : 0;
    cloudU.uCloudSeed.value.set(O.e[0] * 0.1, O.e[1] * 0.1, O.e[2] * 0.1);
    const ch = Math.min(0.02, Math.max(0.003, 6 / (Rm / 1000) * exag));
    u.uCloudH.value = ch;
    bakedMat.uniforms.uCloudH.value = ch;
    clouds.scale.setScalar(1 + ch);
    cloudMat.uniforms.uCloudTint.value.setRGB(...(P.climate.runaway ? [0.95, 0.86, 0.62] : [1, 1, 1]));
    /* the climate model's temperatures by latitude */
    if (P.ebm) { u.uHasEBM.value = 1; for (let j = 0; j < 36; j++) u.uLatT.value[j] = P.ebm.mean[j]; } else u.uHasEBM.value = 0;
    /* tilt and the interior */
    root.rotation.set(0, 0, -(P.obliquityDeg || 0) * Math.PI / 180);
    const L = P.layers, Rk = L.radiusKm;
    capMat.uniforms.uRi.value = L.innerCoreKm / Rk;
    capMat.uniforms.uRc.value = L.coreKm / Rk;
    capMat.uniforms.uRm.value = (L.coreKm + L.mantleKm) / Rk;
    info.layers = L; info.exag = exag; info.kRadius = Rk;
    if (P.locked) u.uSubstellar.value.set(1, 0, 0);
    void pl;
  }

  function tick(t, camera, sunDirWorld, spinOn, pixelAngle) {
    /* a locked planet keeps one face to its star: it does not turn in the picture either */
    if (spinOn && !terrainMat.uniforms.uLocked.value) spin.rotation.y = t * 0.03;
    bakedU.uCloudShift.value = useBaked ? (t * 0.0015) % 1 : 0;
    bakedU.uUseCloudTex.value = useBaked && bake.cloudsDone && bake.key ? 1 : 0;
    baked.visible = isHz && useBaked;
    light.uSunDir.value.copy(sunDirWorld);
    atmMat.uniforms.uCenter.value.setFromMatrixPosition(root.matrixWorld);
    /* the ray march works in world units: scale the shell's radii and heights with the planet's world scale */
    root.getWorldScale(wScale);
    const sc = wScale.x;
    atmMat.uniforms.uR0.value = sc; atmMat.uniforms.uR1.value = atmLocal.R1 * sc;
    atmMat.uniforms.uHR.value = atmLocal.HR * sc; atmMat.uniforms.uHM.value = atmLocal.HM * sc;
    capMat.uniforms.uTime.value = t;
    terrainMat.uniforms.uPixelAngle.value = pixelAngle;
    cloudU.uCloudTime.value = t;
    if (isHz) {
      /* the star's direction in planet-local coordinates: substellar point, cloud shadows, the cutaway in the air */
      const s = sunDirWorld.clone(); spin.updateMatrixWorld(); const inv = new THREE.Matrix3().setFromMatrix4(spin.matrixWorld).invert();
      const sl = s.applyMatrix3(inv).normalize();
      terrainMat.uniforms.uSubstellar.value.copy(sl);
      terrainMat.uniforms.uSunLocal.value.copy(sl);
      cloudU.uCloudSub.value.copy(sl);
      atmMat.uniforms.uToLocal.value.copy(inv);
      bakedMat.uniforms.uSunLocal.value.copy(sl);
      if (useBaked) { for (const m of pool) m.visible = false; used = 0; return 1; }
      return updateLOD(camera);
    }
    return 1;
  }

  /* Quality: 'high' (fine LOD, per-pixel normals from the height function), 'medium', 'low'. */
  function setQuality(q) {
    /* split, fragment octaves, per-pixel normals, cloud octaves, cloud shadows, atmosphere steps */
    const Q = { high: [5.0, 13, 1, 6, 1, 16], medium: [3.5, 11, 1, 5, 1, 12], low: [2.2, 8, 0, 3, 0, 12] }[q] || [5.0, 13, 1, 6, 1, 16];
    LOD_SPLIT = Q[0]; terrainMat.uniforms.uOctMax.value = Q[1]; terrainMat.uniforms.uFDNormals.value = Q[2];
    cloudU.uCloudOct.value = q === 'low' ? 5 : Q[3]; terrainMat.uniforms.uCloudShadow.value = Q[4]; atmMat.uniforms.uSteps.value = Q[5];
    /* low = the baked planet (cheap per frame; used in VR and on the Quest's flat page) */
    useBaked = q === 'low';
    bakedMat.uniforms.uCloudShadow.value = 1;
  }
  return { group: root, spin, set, tick, info, caps, setQuality, bakeStep, setFill(v) { fillU.uFill.value = v ? 0.35 : 0; }, get fill() { return fillU.uFill.value > 0; }, get baked() { return useBaked; }, get bakeDone() { return bake.row >= BAKE_H && bake.cloudsDone; }, get bakeProgress() { return bake.key ? Math.min(1, bake.row / BAKE_H) : 0; },
    get chunks() { return useBaked ? (bakedU.uHasBake.value ? 1 : 0) : used; } };
}
