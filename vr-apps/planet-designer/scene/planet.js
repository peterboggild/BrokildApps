/* The planet (milestone M2): a cube-sphere with a chunked quadtree LOD, heights from the shared terrain function
 * (scene/glsl.js = physics/terrain.js) computed on the GPU, per-pixel detail through screen-space bump mapping,
 * oceans at the sea level that physics derives from the ocean volume, biomes and ice from a latitude-temperature
 * estimate, an atmosphere rim (a preview until the scattering model of M3), the cutaway with the interior layers,
 * and simple looks for the other planet types. Radius 1 in scene units; heights are exaggerated (stated on screen). */
import * as THREE from 'three';
import { NOISE, TERRAIN, CUBESPHERE } from './glsl.js?v=2';
import { seedOffsets } from '../physics/terrain.js?v=2';

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

const TERRAIN_FS = NOISE + TERRAIN + /* glsl */`
uniform float uSea, uRef, uK, uHasOcean, uCut;
uniform vec3 uSunDir, uSunColor, uAtmColor;
uniform float uTs, uDT, uLocked, uLife, uRust, uRelief, uAtmDensity, uPixelAngle, uHasWater, uOctMax, uFDNormals;
uniform vec3 uSubstellar;
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

  /* climate at this point: latitude (or angle from the substellar point if locked) and elevation lapse rate */
  float elevKm = max(h - uSea, 0.0) * uRelief / 1000.0;
  float T = uLocked > 0.5
    ? uTs + 55.0 * (dot(n, uSubstellar) - 0.25)
    : uTs + uDT * (1.0 / 3.0 - n.y * n.y);
  T -= 6.5 * elevKm;
  T += 3.0 * snoise(n * 5.0 + uOc);                       /* weather-scale variation of the ice edge */

  float slope = 1.0 - clamp(dot(N, N0), 0.0, 1.0);
  float tex = 0.85 + 0.3 * (0.5 + 0.5 * snoise(n * 140.0 + uOf)) * (0.6 + 0.4 * snoise(n * 31.0));
  vec3 col;
  float rough = 0.9;
  if (water) {
    float depthM = (uSea - h) * uRelief;
    vec3 deep = vec3(0.004, 0.018, 0.055), shallow = vec3(0.03, 0.16, 0.17);
    col = mix(deep, shallow, exp(-depthM / 160.0));
    rough = 0.06;
    if (T < -2.0) { col = vec3(0.78, 0.84, 0.9) * tex; rough = 0.5; water = false; }   /* sea ice */
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

  /* lighting: the star (with the sphere's own terminator softened by the air), skylight, and the ocean's glint */
  float NdL = max(dot(N, L), 0.0);
  float twilight = smoothstep(-0.12 * uAtmDensity - 0.01, 0.1, dot(N0, L));
  vec3 lit = col * uSunColor * NdL * twilight;
  lit += col * uAtmColor * 0.06 * twilight;
  if (water) {
    vec3 Hh = normalize(L + V);
    float NdH = max(dot(N0, Hh), 0.0);
    float a2 = rough * rough;
    float D = a2 / (3.14159 * pow(NdH * NdH * (a2 - 1.0) + 1.0, 2.0));
    float F = 0.02 + 0.98 * pow(1.0 - max(dot(V, N0), 0.0), 5.0);
    lit += uSunColor * min(D * F, 6.0) * NdL * 0.5;
    lit += uAtmColor * F * 0.25 * twilight;
  }
  /* aerial perspective: the air veils the ground towards the limb */
  float veil = (1.0 - max(dot(N0, V), 0.0));
  lit = mix(lit, uAtmColor * twilight * 0.6, uAtmDensity * pow(veil, 3.0) * 0.6);
  gl_FragColor = vec4(lit, 1.0);
}`;

/* Atmosphere rim: a back-facing shell, glow from the path length through it, lit by the star with a reddened
 * terminator. A preview only; the physically based scattering model arrives in M3. */
const ATM_VS = `varying vec3 vWP; void main() { vWP = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0); }`;
const ATM_FS = `
uniform vec3 uCenter, uSunDir, uSunColor, uAtmColor; uniform float uR0, uR1, uDensity;
varying vec3 vWP;
void main() {
  vec3 ro = cameraPosition, rd = normalize(vWP - cameraPosition);
  vec3 oc = ro - uCenter;
  float b = dot(oc, rd);
  float c1 = dot(oc, oc) - uR1 * uR1;
  float disc = b * b - c1;
  if (disc < 0.0) discard;
  float t0 = max(-b - sqrt(disc), 0.0), t1 = -b + sqrt(disc);
  float c0 = dot(oc, oc) - uR0 * uR0;
  float d0 = b * b - c0;
  if (d0 > 0.0) { float tp = -b - sqrt(d0); if (tp > 0.0) t1 = min(t1, tp); }
  /* integrate density exp(-height/H) along the ray in 12 steps */
  float H = (uR1 - uR0) * 0.25;
  float dt = (t1 - t0) / 12.0;
  vec3 acc = vec3(0.0);
  for (int i = 0; i < 12; i++) {
    vec3 p = ro + rd * (t0 + (float(i) + 0.5) * dt) - uCenter;
    float hgt = length(p) - uR0;
    float dens = exp(-max(hgt, 0.0) / H);
    float mu = dot(normalize(p), normalize(uSunDir));
    float lightAmt = smoothstep(-0.25, 0.3, mu);
    vec3 sunset = mix(vec3(1.0, 0.45, 0.2), vec3(1.0), smoothstep(0.0, 0.35, mu));
    acc += dens * lightAmt * sunset * dt;
  }
  float phase = 1.0 + 0.6 * pow(max(dot(rd, normalize(uSunDir)), 0.0), 8.0);
  vec3 col = acc * uAtmColor * uSunColor * uDensity * phase * 9.0;
  gl_FragColor = vec4(col, 1.0);
}`;

/* Cut faces of the cutaway: half-discs in the local planes x = 0 (z > 0) and z = 0 (x > 0), coloured by layer. */
const CAP_FS = NOISE + TERRAIN + /* glsl */`
uniform float uSea, uRef, uK, uTime;
uniform float uRi, uRc, uRm, uRr;   /* inner core, core, mantle top (base of crust), rock surface: radii in planet radii */
uniform vec3 uSunDir, uSunColor;
varying vec3 vLP; varying vec3 vWN;
void main() {
  float r = length(vLP);
  vec3 dir = vLP / max(r, 1e-6);
  float h = terrainH(dir, 4.0);
  float surf = 1.0 + (max(h, uSea) - uRef) * uK;
  if (r > surf) discard;
  float rock = 1.0 + (h - uRef) * uK;
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
}`;
const OTHER_VS = `varying vec3 vN; varying vec3 vWN; varying vec3 vWP; void main() { vN = position; vWN = normalize(mat3(modelMatrix) * normal);
  vWP = (modelMatrix * vec4(position, 1.0)).xyz; gl_Position = projectionMatrix * viewMatrix * vec4(vWP, 1.0); }`;

export function createPlanet() {
  const root = new THREE.Group();          /* tilted by obliquity */
  const spin = new THREE.Group();          /* turns about the local y axis */
  root.add(spin);
  const common = COMMON_UNIFORMS();
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
    uniforms: { ...light, uCenter: { value: new THREE.Vector3() }, uR0: { value: 1 }, uR1: { value: 1.03 }, uDensity: { value: 1 } },
    vertexShader: ATM_VS, fragmentShader: ATM_FS, side: THREE.BackSide, transparent: true, depthWrite: false,
    blending: THREE.AdditiveBlending,
  });
  const atm = new THREE.Mesh(new THREE.SphereGeometry(1, 96, 48), atmMat);
  root.add(atm);

  /* cutaway caps */
  const capMat = new THREE.ShaderMaterial({
    uniforms: { ...common, ...light, uTime: { value: 0 }, uRi: { value: 0.19 }, uRc: { value: 0.55 }, uRm: { value: 0.99 }, uRr: { value: 1 } },
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
    const dens = Math.min(1.6, Math.log10(1 + P.pBar * 9) * 0.9);
    u.uAtmDensity.value = dens;
    /* sky tint: Rayleigh-blue for thin, clear air, whiter with thick or CO2-heavy air, tinted by the star */
    const thick = Math.min(1, P.pBar / 8);
    const ray = [0.18, 0.42, 1.0];
    light.uAtmColor.value.setRGB(
      (ray[0] * (1 - thick) + thick * 0.9) * rgb[0], (ray[1] * (1 - thick) + thick * 0.85) * rgb[1], (ray[2] * (1 - thick) + thick * 0.75) * rgb[2]);
    atmMat.uniforms.uDensity.value = dens;
    atmMat.uniforms.uR0.value = 1.0;
    atmMat.uniforms.uR1.value = 1.0 + 0.018 + 0.03 * thick;
    atm.scale.setScalar(atmMat.uniforms.uR1.value);
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
    if (spinOn) spin.rotation.y = t * 0.03;
    light.uSunDir.value.copy(sunDirWorld);
    atmMat.uniforms.uCenter.value.setFromMatrixPosition(root.matrixWorld);
    capMat.uniforms.uTime.value = t;
    terrainMat.uniforms.uPixelAngle.value = pixelAngle;
    if (isHz) {
      /* substellar point in local coordinates for locked planets */
      const s = sunDirWorld.clone(); spin.updateMatrixWorld(); const inv = new THREE.Matrix3().setFromMatrix4(spin.matrixWorld).invert();
      terrainMat.uniforms.uSubstellar.value.copy(s.applyMatrix3(inv).normalize());
      return updateLOD(camera);
    }
    return 1;
  }

  /* Quality: 'high' (fine LOD, per-pixel normals from the height function), 'medium', 'low'. */
  function setQuality(q) {
    const Q = { high: [5.0, 13, 1], medium: [3.5, 11, 1], low: [2.4, 9, 0] }[q] || [5.0, 13, 1];
    LOD_SPLIT = Q[0]; terrainMat.uniforms.uOctMax.value = Q[1]; terrainMat.uniforms.uFDNormals.value = Q[2];
  }
  return { group: root, spin, set, tick, info, caps, setQuality, get chunks() { return used; } };
}
