/*
 * Portalkniven — cut a door in the air with a knife; a green crackling oval lights up and is an open door to another
 * version of the room. Walk through, walk back.
 *
 * THE KNIFE
 *  Hold the trigger and draw a loop in the air with the point of the blade: a glowing cut follows the tip. Close the loop
 *  (come back to where you started) and the ring ignites. Let go before it closes and the cut simply fades.
 *  The ring is always a door-sized upright oval: the loop decides WHERE the door is and which way it faces.
 *
 * THE WINDOW (the thing that has to work on a Quest 3)
 *  - The oval's inside is a "mask" that writes 1 into the STENCIL buffer wherever it is visible (it is depth-tested).
 *  - A full-screen quad at the far plane, drawn only where stencil == 1, resets the depth there and paints the other
 *    world's sky; then the other world's objects are drawn, again only where stencil == 1, clipped to the far side of
 *    the door's plane. Each eye renders from its own position, so it is a real window (parallax), no render-to-texture.
 *  - World A is your room (passthrough), world B the other universe (opaque). Walking through the oval swaps which one
 *    is "home"; the picture is identical the frame before and after. Seen from B the hole is alpha 0: the real room.
 *  - With a room scan (mesh-detection) the real walls and furniture become world B's walls, reskinned.
 */
import * as THREE from './vendor/three.module.min.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const V = THREE.Vector3;
const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);
const easeOutBack = t => { const c1 = 1.9, c3 = c1 + 1; return 1 + c3 * Math.pow(t - 1, 3) + c1 * Math.pow(t - 1, 2); };
const easeOutCubic = t => 1 - Math.pow(1 - t, 3);

/* ================================================================ renderer */

const canvas = $('view');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, stencil: true, powerPreference: 'high-performance' });
renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 2));
renderer.setSize(innerWidth, innerHeight, false);
renderer.setClearColor(0x000000, 0);
renderer.localClippingEnabled = true;
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
const gl = renderer.getContext();

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.05, 80);
camera.rotation.order = 'YXZ';
camera.position.set(0, 1.6, 0);
const hemi = new THREE.HemisphereLight(0x86c4b8, 0x0a1210, 1.0); scene.add(hemi);
const sun = new THREE.DirectionalLight(0xbfe8dd, 0.55); sun.position.set(2, 5, 1); scene.add(sun);
const FOG_COL = new THREE.Color(0x0a1412);
scene.fog = new THREE.FogExp2(0x0a1412, 0.17);
const ringLight = new THREE.PointLight(0x40ff90, 0, 7, 1.5); scene.add(ringLight);

/* a small studio-like environment, so the steel and brass of the knife have something to reflect */
{
  const env = new THREE.Scene();
  env.add(new THREE.Mesh(new THREE.BoxGeometry(20, 20, 20), new THREE.MeshBasicMaterial({ color: 0x3a4660, side: THREE.BackSide })));
  const panel = (w, h, col, x, y, z) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: col, side: THREE.DoubleSide })); m.position.set(x, y, z); m.lookAt(0, 0, 0); env.add(m); };
  panel(14, 9, 0xffffff, 0, 9, 2); panel(5, 14, 0x60ffa0, -8, 2, 0); panel(4, 12, 0xffc890, 8, 1, -3); panel(12, 4, 0xa0a8ff, 0, 0, -9); panel(10, 10, 0x6a7a96, 0, -8, 0); panel(10, 5, 0xffffff, 0, 2, 9);
  const pm = new THREE.PMREMGenerator(renderer); scene.environment = pm.fromScene(env, 0.04).texture; pm.dispose();
}

const S = { bright: 1, brightTarget: 1, lum: -1, camera: false, inB: false, side: 1, placed: false, mode: 'menu', stencilBits: null, crossings: 0, frames: 0, scanMeshes: 0, scanFeature: false, time: 0, msg: '', msgT: -99 };
const DOOR = { Rx: 0.6, Ry: 1.1 };                             // an upright oval you can walk through (1.2 m wide, 2.2 m tall)
const portal = { c: new V(0, DOOR.Ry + 0.03, -1.6), n: new V(0, 0, 1), x: new V(1, 0, 0), y: new V(0, 1, 0), Rx: DOOR.Rx, Ry: DOOR.Ry, anim: null };
const head = new V(0, 1.6, 0), prevHead = new V(0, 1.6, 0), headQ = new THREE.Quaternion();
const say = t => { S.msg = t; S.msgT = S.time; };

/* ============================================ the two worlds: roles & stencil */

const worldA = new THREE.Group(), worldB = new THREE.Group();
scene.add(worldA, worldB);
const world = { A: worldA, B: worldB };
const mats = { A: [], B: [] };
const clip = { A: new THREE.Plane(new V(0, 1, 0), 1e6), B: new THREE.Plane(new V(0, 1, 0), 1e6) };   // 1e6: lets everything through

/** Every material of a world is registered here, so the world can be switched between "home" and "seen through the oval". */
function reg(w, m) {
  m.clippingPlanes = [clip[w]];
  m.stencilWrite = false; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc;
  m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp;
  mats[w].push(m);
  return m;
}

const ORDER = { bgHome: -10, home: 0, mask: 10, fill: 20, other: 30 };
let bgHomeB, fillB, fillA;
function applyRoles() {
  for (const w of ['A', 'B']) {
    const home = (w === 'B') === S.inB;
    for (const m of mats[w]) m.stencilWrite = !home;
    world[w].traverse(o => { if (o.isMesh || o.isPoints || o.isLine) o.renderOrder = home ? ORDER.home : ORDER.other; });
  }
  bgHomeB.visible = S.inB;       // inside B the whole view is B's sky
  fillB.visible = !S.inB;        // from A: B's sky + depth reset inside the oval
  fillA.visible = S.inB;         // from B: the oval shows alpha 0 = the real room (passthrough)
}

/* a full-screen quad that sits at the far plane and reconstructs the view direction (for the sky) */
const QUAD_VS = `
  varying vec3 vDir;
  void main() {
    vec4 clipPos = vec4(position.xy, 0.9999, 1.0);
    vec4 v = inverse(projectionMatrix) * clipPos;
    vDir = transpose(mat3(viewMatrix)) * (v.xyz / v.w);
    gl_Position = clipPos;
  }`;
const SKY_FS = `
  precision highp float;
  varying vec3 vDir; uniform float uTime; uniform float uBright;
  float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float noise(vec3 x) { vec3 i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x), mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x), mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z); }
  void main() {
    vec3 d = normalize(vDir);
    float h = d.y;
    vec3 horizon = vec3(0.040, 0.080, 0.070), zenith = vec3(0.008, 0.020, 0.032);
    vec3 col = mix(horizon, zenith, smoothstep(-0.05, 0.85, h));
    vec2 q = d.xz / (abs(d.y) + 0.45);                                   // a slow, thin mist overhead
    float m = noise(vec3(q * 1.4, uTime * 0.02)) * 0.65 + noise(vec3(q * 3.7, uTime * 0.03 + 4.0)) * 0.35;
    col += vec3(0.045, 0.115, 0.095) * pow(m, 2.2) * smoothstep(0.0, 0.5, h);
    float band = exp(-pow((h - 0.30) * 4.5, 2.0)) * pow(noise(vec3(d.x * 5.0, d.y * 1.5, d.z * 5.0) + uTime * 0.05), 2.0);
    col += vec3(0.0, 0.20, 0.14) * band * 0.28;
    if (h < 0.0) col = horizon * (0.9 + 0.1 * h);
    gl_FragColor = vec4(col * uBright, 1.0);
  }`;
const CLEAR_FS = 'void main() { gl_FragColor = vec4(0.0); }';
const stencilEqual = m => { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; return m; };
const quadGeo = new THREE.PlaneGeometry(2, 2);
function quad(fs, inRing) {
  const m = new THREE.ShaderMaterial({ vertexShader: QUAD_VS, fragmentShader: fs, uniforms: { uTime: { value: 0 }, uBright: { value: 1 } }, blending: THREE.NoBlending, depthTest: inRing, depthWrite: inRing, depthFunc: THREE.AlwaysDepth });
  if (inRing) stencilEqual(m);
  const q = new THREE.Mesh(quadGeo, m);
  q.frustumCulled = false; q.renderOrder = inRing ? ORDER.fill : ORDER.bgHome;
  scene.add(q);
  return q;
}
bgHomeB = quad(SKY_FS, false);
fillB = quad(SKY_FS, true);
fillA = quad(CLEAR_FS, true);
const skyMats = [bgHomeB.material, fillB.material];

/* ================================================================ world A (your room: almost nothing virtual) */

function gridTex(bg, line, repeat) {
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const x = c.getContext('2d'); x.fillStyle = bg; x.fillRect(0, 0, 256, 256);
  x.strokeStyle = line; x.lineWidth = 4; x.strokeRect(2, 2, 252, 252);
  x.globalAlpha = 0.35; x.lineWidth = 2; x.beginPath(); x.moveTo(128, 0); x.lineTo(128, 256); x.moveTo(0, 128); x.lineTo(256, 128); x.stroke();
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(repeat, repeat); t.anisotropy = 4; t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
const decoA = new THREE.Group(); worldA.add(decoA);
{
  const orange = reg('A', new THREE.MeshLambertMaterial({ color: 0xff9a3c, emissive: 0x7a3a08, flatShading: true }));
  const warm = reg('A', new THREE.MeshBasicMaterial({ color: 0xffd9a0 }));
  if (params.has('flat')) {                                       // reference objects for the laptop preview only: the real room has nothing virtual in it
    for (let i = 0; i < 6; i++) {
      const h = rnd(0.5, 1.5);
      const p = new THREE.Mesh(new THREE.BoxGeometry(0.16, h, 0.16), orange);
      p.position.set(rnd(-1.3, 1.3), h / 2, rnd(0.6, 2.6)); decoA.add(p);
    }
    const orb = new THREE.Mesh(new THREE.IcosahedronGeometry(0.12, 1), warm); orb.position.set(0, 1.7, 1.8); decoA.add(orb); decoA.userData.orb = orb;
  }
  if (params.has('flat') || params.get('floor') === '1') {                    // a floor to stand on, only on a laptop
    const g = new THREE.Mesh(new THREE.PlaneGeometry(30, 30), reg('A', new THREE.MeshBasicMaterial({ map: gridTex('#1b2230', '#33445e', 60), side: THREE.DoubleSide })));
    g.rotation.x = -Math.PI / 2; g.position.y = -0.005; worldA.add(g);
  }
}

/* ================================================================ world B (the other universe) */

const decoB = new THREE.Group(); worldB.add(decoB);
const scanGroup = new THREE.Group(); worldB.add(scanGroup);
const orgTime = { value: 0 };
const NOISE_GLSL = `
  float orgH(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float orgN(vec3 x) { vec3 i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(orgH(i), orgH(i + vec3(1,0,0)), f.x), mix(orgH(i + vec3(0,1,0)), orgH(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(orgH(i + vec3(0,0,1)), orgH(i + vec3(1,0,1)), f.x), mix(orgH(i + vec3(0,1,1)), orgH(i + vec3(1,1,1)), f.x), f.y), f.z); }
  float orgF(vec3 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 4; i++) { s += a * orgN(p); p = p * 2.03 + vec3(1.7, 9.2, 3.4); a *= 0.5; } return s; }`;
/** Damp, dark, mossy surfaces with a faint network of slowly pulsing glowing veins, in world space (so a wall looks the same from either side). */
function organic(color, rough, vein) {
  const m = new THREE.MeshStandardMaterial({ color, roughness: rough, metalness: 0, side: THREE.DoubleSide, envMapIntensity: 0.3 });
  m.onBeforeCompile = sh => {
    sh.uniforms.uTime = orgTime;
    sh.vertexShader = sh.vertexShader.replace('#include <common>', '#include <common>\nvarying vec3 vWP;').replace('#include <begin_vertex>', '#include <begin_vertex>\nvWP = (modelMatrix * vec4(transformed, 1.0)).xyz;');
    sh.fragmentShader = sh.fragmentShader.replace('#include <common>', '#include <common>\nvarying vec3 vWP; uniform float uTime;\n' + NOISE_GLSL)
      .replace('#include <color_fragment>', `#include <color_fragment>
        float n1 = orgF(vWP * 1.7), n2 = orgF(vWP * 6.5 + 3.1);
        vec3 moss = mix(vec3(0.040, 0.060, 0.052), vec3(0.150, 0.190, 0.115), smoothstep(0.35, 0.78, n1));
        diffuseColor.rgb = mix(diffuseColor.rgb, moss, 0.9) * (0.7 + 0.55 * n2);`)
      .replace('#include <emissivemap_fragment>', `#include <emissivemap_fragment>
        float vein = 1.0 - smoothstep(0.0, 0.04, abs(orgF(vWP * 1.15 + vec3(0.0, uTime * 0.01, 0.0)) - 0.5));
        float pulse = 0.5 + 0.5 * sin(uTime * 0.5 + n1 * 9.0);
        totalEmissiveRadiance += vec3(0.04, 0.5, 0.42) * vein * pulse * ${vein.toFixed(2)};`);
  };
  m.customProgramCacheKey = () => 'organic' + vein;
  return m;
}
{
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(60, 60), reg('B', organic(0x1a2420, 0.95, 0.5)));
  floor.rotation.x = -Math.PI / 2; floor.position.y = -0.012; worldB.add(floor);
  const stalkM = reg('B', new THREE.MeshStandardMaterial({ color: 0x17221d, roughness: 0.75, metalness: 0 }));
  const glowM = reg('B', new THREE.MeshStandardMaterial({ color: 0x0b2b27, emissive: 0x1fd8b4, emissiveIntensity: 1.1, roughness: 0.45, metalness: 0 }));
  decoB.userData.stalks = [];
  for (let i = 0; i < 12; i++) {                                  // thin, bending stalks with a glowing bud: a few right behind the door, the rest scattered
    const near = i < 6, d = near ? rnd(1.6, 3.2) : rnd(1.0, 5.5), a = near ? rnd(-0.5, 0.5) : rnd(-1.5, 1.5);
    const h = rnd(0.5, near ? 1.9 : 1.2), lean = rnd(-0.35, 0.35), g = new THREE.Group();
    const pts = []; for (let k = 0; k <= 5; k++) { const t = k / 5; pts.push(new V(lean * t * t * h + rnd(-0.015, 0.015) * k, t * h, rnd(-0.015, 0.015) * k)); }
    g.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 20, rnd(0.008, 0.016), 6, false), stalkM));
    const bud = new THREE.Mesh(new THREE.SphereGeometry(rnd(0.025, 0.05), 12, 9), glowM); bud.position.copy(pts[5]); bud.scale.y = 1.35; g.add(bud);
    g.position.set(Math.sin(a) * d, 0, -Math.cos(a) * d); g.userData.p = rnd(0, TAU); g.userData.sw = rnd(0.02, 0.05); decoB.add(g); decoB.userData.stalks.push(g);
  }
  for (let c = 0; c < 6; c++) {                                    // low clusters of soft pods on the floor
    const cx = rnd(-2.5, 2.5), cz = -rnd(0.8, 5);
    for (let k = 0; k < 4; k++) { const r = rnd(0.04, 0.11), p = new THREE.Mesh(new THREE.SphereGeometry(r, 12, 9), glowM); p.scale.y = rnd(0.6, 0.9); p.position.set(cx + rnd(-0.2, 0.2), r * 0.6, cz + rnd(-0.2, 0.2)); decoB.add(p); }
  }
  const dot = document.createElement('canvas'); dot.width = dot.height = 64; const dx = dot.getContext('2d');
  const grd = dx.createRadialGradient(32, 32, 0, 32, 32, 32); grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.4, 'rgba(255,255,255,.35)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  dx.fillStyle = grd; dx.fillRect(0, 0, 64, 64);
  const sp = new Float32Array(150 * 3); for (let i = 0; i < 150; i++) { sp[i * 3] = rnd(-4, 4); sp[i * 3 + 1] = rnd(0.1, 2.6); sp[i * 3 + 2] = -rnd(0.3, 7); }
  const sg = new THREE.BufferGeometry(); sg.setAttribute('position', new THREE.BufferAttribute(sp, 3));
  const spores = new THREE.Points(sg, reg('B', new THREE.PointsMaterial({ size: 0.035, map: new THREE.CanvasTexture(dot), color: 0xa8ffe8, transparent: true, opacity: 0.5, depthWrite: false, blending: THREE.AdditiveBlending })));
  spores.frustumCulled = false; decoB.add(spores); decoB.userData.spores = spores;
}
const scanSolid = reg('B', organic(0x202c27, 0.92, 0.8)); scanSolid.polygonOffset = true; scanSolid.polygonOffsetFactor = 1; scanSolid.polygonOffsetUnits = 1;
const scanWire = reg('B', new THREE.MeshBasicMaterial({ color: 0x40ffd0, wireframe: true, transparent: true, opacity: 0.55 }));
const scanKnown = new Map();
let scanNext = 0;
function updateScan(frame, refSpace) {
  const now = performance.now();
  if (now < scanNext || !frame.detectedMeshes) return;
  scanNext = now + 600; S.scanFeature = true;
  let did = 0; const seen = new Set();
  for (const m of frame.detectedMeshes) {
    seen.add(m);
    const rec = scanKnown.get(m);
    if (rec && m.lastChangedTime <= rec.t) continue;
    if (did >= 1) { scanNext = 0; continue; }
    const pose = frame.getPose(m.meshSpace, refSpace); if (!pose) continue;
    if (rec) { scanGroup.remove(rec.g); rec.geo.dispose(); }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(m.vertices), 3));
    geo.setIndex(new THREE.BufferAttribute(new Uint32Array(m.indices), 1));
    geo.computeVertexNormals();
    const g = new THREE.Group(); g.matrixAutoUpdate = false; g.matrix.fromArray(pose.transform.matrix);
    g.add(new THREE.Mesh(geo, scanSolid)); if (params.get('skin') === 'wire') g.add(new THREE.Mesh(geo, scanWire));
    scanGroup.add(g); scanKnown.set(m, { t: m.lastChangedTime, g, geo }); did++;
    applyRoles();
  }
  for (const [k, rec] of scanKnown) if (!seen.has(k)) { scanGroup.remove(rec.g); rec.geo.dispose(); scanKnown.delete(k); }
  S.scanMeshes = scanKnown.size;
}

/* ================================================================ the oval ring */

const ring = new THREE.Group(); scene.add(ring); ring.visible = false;
const maskMat = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: false, side: THREE.DoubleSide, stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp });
const mask = new THREE.Mesh(new THREE.CircleGeometry(1, 80), maskMat);             // a unit disc, scaled to the oval
mask.renderOrder = ORDER.mask; mask.frustumCulled = false;
mask.onAfterRender = () => { if (S.stencilBits === null && S.mode === 'xr') S.stencilBits = gl.getParameter(gl.STENCIL_BITS); };   // asked while the headset's framebuffer is bound
ring.add(mask);

class OvalCurve extends THREE.Curve {
  constructor(rx, ry) { super(); this.rx = rx; this.ry = ry; }
  getPoint(t, target = new V()) { const a = t * TAU; return target.set(Math.cos(a) * this.rx, Math.sin(a) * this.ry, 0); }
}
const RING_VS = `uniform vec2 uR; varying float vA; varying vec3 vN; varying vec3 vV;
  void main() { vA = atan(position.y / uR.y, position.x / uR.x); vec4 mv = modelViewMatrix * vec4(position, 1.0); vN = normalMatrix * normal; vV = -mv.xyz; gl_Position = projectionMatrix * mv; }`;
const RING_FS = `precision highp float; uniform float uTime; uniform float uAmp; varying float vA; varying vec3 vN; varying vec3 vV;
  float h1(float x) { return fract(sin(x * 127.1) * 43758.5453); }
  float n1(float x) { float i = floor(x), f = fract(x); return mix(h1(i), h1(i + 1.0), f * f * (3.0 - 2.0 * f)); }
  void main() {
    float rim = pow(1.0 - abs(dot(normalize(vN), normalize(vV))), 1.4);
    float a = vA * 6.0;
    float crackle = n1(a * 3.0 + uTime * 14.0) * n1(a * 7.0 - uTime * 9.0);
    float arcs = pow(crackle, 2.0) * 2.8;
    float flick = 0.75 + 0.25 * sin(uTime * 41.0 + vA * 9.0);
    vec3 col = vec3(0.25, 1.0, 0.45) * (0.45 + rim) * flick + vec3(0.8, 1.0, 0.85) * arcs;
    gl_FragColor = vec4(col * uAmp, 1.0);
  }`;
const ringMats = [], ringMeshes = [];
function addTorus(tube, amp) {
  const m = new THREE.ShaderMaterial({ vertexShader: RING_VS, fragmentShader: RING_FS, uniforms: { uTime: { value: 0 }, uAmp: { value: amp }, uR: { value: new THREE.Vector2(1, 1) } }, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false });
  const t = new THREE.Mesh(new THREE.BufferGeometry(), m); t.frustumCulled = false; t.userData.tube = tube; ring.add(t);
  ringMats.push(m); ringMeshes.push(t);
}
addTorus(0.032, 1.0); addTorus(0.1, 0.2);
function setRingShape(rx, ry) {
  portal.Rx = rx; portal.Ry = ry;
  mask.scale.set(rx * 0.985, ry * 0.985, 1);
  for (const t of ringMeshes) { t.geometry.dispose(); t.geometry = new THREE.TubeGeometry(new OvalCurve(rx, ry), 200, t.userData.tube, 14, true); t.material.uniforms.uR.value.set(rx, ry); }
}
setRingShape(DOOR.Rx, DOOR.Ry);

const ellipsePt = a => [Math.cos(a) * portal.Rx, Math.sin(a) * portal.Ry];
const ellipseOut = a => { const x = Math.cos(a) * portal.Ry, y = Math.sin(a) * portal.Rx, l = Math.hypot(x, y) || 1; return [x / l, y / l]; };

const ARCS = 18;
const arcPos = new Float32Array(ARCS * 3 * 2 * 3);
const arcGeo = new THREE.BufferGeometry(); arcGeo.setAttribute('position', new THREE.BufferAttribute(arcPos, 3));
const arcMat = new THREE.LineBasicMaterial({ color: 0xb8ffd0, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false });
const arcs = new THREE.LineSegments(arcGeo, arcMat); arcs.frustumCulled = false; ring.add(arcs);
function regenArc(k) {
  const a = rnd(0, TAU); let [px, py] = ellipsePt(a); let pz = 0; const [ox, oy] = ellipseOut(a);
  const dr = rnd(-0.6, 1.0), dt = rnd(-0.5, 0.5), dz = rnd(-0.5, 0.5), len = rnd(0.12, 0.36) / 3;
  for (let s = 0; s < 3; s++) {
    const nx = px + (ox * dr - oy * dt) * len + rnd(-0.035, 0.035), ny = py + (oy * dr + ox * dt) * len + rnd(-0.035, 0.035), nz = pz + dz * len + rnd(-0.035, 0.035);
    const o = (k * 3 + s) * 6; arcPos[o] = px; arcPos[o + 1] = py; arcPos[o + 2] = pz; arcPos[o + 3] = nx; arcPos[o + 4] = ny; arcPos[o + 5] = nz;
    px = nx; py = ny; pz = nz;
  }
}
for (let k = 0; k < ARCS; k++) regenArc(k);

/** Round glowing points that fly, fade and are recycled: used for the ring (in its own space) and for the knife tip (in the world). */
function makeSparks(n, parent) {
  const f = { n, pos: new Float32Array(n * 3), vel: new Float32Array(n * 3), age: new Float32Array(n).fill(9), life: new Float32Array(n).fill(1), frac: new Float32Array(n).fill(1), next: 0 };
  f.geo = new THREE.BufferGeometry(); f.geo.setAttribute('position', new THREE.BufferAttribute(f.pos, 3)); f.geo.setAttribute('aFrac', new THREE.BufferAttribute(f.frac, 1));
  const m = new THREE.ShaderMaterial({
    uniforms: { uScale: { value: 900 } }, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
    vertexShader: `attribute float aFrac; varying float vF; uniform float uScale; void main() { vF = aFrac; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_PointSize = clamp(0.02 * (1.0 - aFrac) * uScale / -mv.z, 1.5, 24.0); gl_Position = projectionMatrix * mv; }`,
    fragmentShader: `precision highp float; varying float vF; void main() { float d = length(gl_PointCoord - 0.5) * 2.0; float a = smoothstep(1.0, 0.0, d) * (1.0 - vF); gl_FragColor = vec4(mix(vec3(0.4, 1.0, 0.55), vec3(1.0), 1.0 - vF) * a, a); }`,
  });
  f.points = new THREE.Points(f.geo, m); f.points.frustumCulled = false; parent.add(f.points);
  f.emit = (x, y, z, vx, vy, vz, life) => { const i = f.next; f.next = (i + 1) % n; f.pos[i * 3] = x; f.pos[i * 3 + 1] = y; f.pos[i * 3 + 2] = z; f.vel[i * 3] = vx; f.vel[i * 3 + 1] = vy; f.vel[i * 3 + 2] = vz; f.life[i] = life; f.age[i] = 0; };
  f.step = (dt, g, onExpire) => {
    for (let i = 0; i < n; i++) {
      f.age[i] += dt;
      if (f.age[i] > f.life[i]) { if (onExpire) onExpire(i); else { f.frac[i] = 1; continue; } }
      f.vel[i * 3 + 1] -= g * dt;
      f.pos[i * 3] += f.vel[i * 3] * dt; f.pos[i * 3 + 1] += f.vel[i * 3 + 1] * dt; f.pos[i * 3 + 2] += f.vel[i * 3 + 2] * dt;
      f.frac[i] = f.age[i] / f.life[i];
    }
    f.geo.attributes.position.needsUpdate = true; f.geo.attributes.aFrac.needsUpdate = true;
  };
  return f;
}
const ringSparks = makeSparks(170, ring);
const tipSparks = makeSparks(120, scene);
function respawnRingSpark(i) {
  const a = rnd(0, TAU), [px, py] = ellipsePt(a), [ox, oy] = ellipseOut(a), sp = rnd(0.08, 0.5) * (Math.random() < 0.5 ? 1 : -0.6);
  ringSparks.pos[i * 3] = px; ringSparks.pos[i * 3 + 1] = py; ringSparks.pos[i * 3 + 2] = rnd(-0.03, 0.03);
  ringSparks.vel[i * 3] = ox * sp + rnd(-0.1, 0.1); ringSparks.vel[i * 3 + 1] = oy * sp + rnd(0, 0.25); ringSparks.vel[i * 3 + 2] = rnd(-0.25, 0.25);
  ringSparks.life[i] = rnd(0.4, 1.3); ringSparks.age[i] = 0;
}
for (let i = 0; i < ringSparks.n; i++) { respawnRingSpark(i); ringSparks.age[i] = rnd(0, ringSparks.life[i]); }

/* ignition effects: a flash, a shock ring, a light */
function glowTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 128; const x = c.getContext('2d');
  const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.25, 'rgba(120,255,170,.7)'); g.addColorStop(1, 'rgba(0,255,90,0)');
  x.fillStyle = g; x.fillRect(0, 0, 128, 128); return new THREE.CanvasTexture(c);
}
const flash = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0 }));
flash.renderOrder = 60; scene.add(flash);
const shock = new THREE.Mesh(new THREE.BufferGeometry(), new THREE.MeshBasicMaterial({ color: 0x70ffa8, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0 }));
shock.renderOrder = 55; shock.frustumCulled = false; scene.add(shock);
let shockT = 9;
function setShockShape(rx, ry) { shock.geometry.dispose(); shock.geometry = new THREE.TubeGeometry(new OvalCurve(rx, ry), 120, 0.02, 6, true); }
setShockShape(DOOR.Rx, DOOR.Ry);

function updateRing(dt) {
  for (const m of ringMats) m.uniforms.uTime.value = S.time;
  for (let i = 0; i < 4; i++) regenArc((Math.random() * ARCS) | 0);
  arcGeo.attributes.position.needsUpdate = true;
  arcMat.opacity = 0.55 + 0.45 * Math.random();
  ringSparks.step(dt, 0.5, respawnRingSpark);
}

/* ================================================================ the knife */

function buildKnife() {
  const root = new THREE.Group(), k = new THREE.Group(); root.add(k); k.position.z = 0.065;       // origin = the middle of the handle; the blade points to -Z
  const steel = new THREE.MeshStandardMaterial({ color: 0xe4ecf1, metalness: 0.9, roughness: 0.3, envMapIntensity: 1.8 });
  const brass = new THREE.MeshStandardMaterial({ color: 0xd2a84e, metalness: 0.95, roughness: 0.28 });
  const leather = new THREE.MeshStandardMaterial({ color: 0x2b1912, metalness: 0, roughness: 0.82 });
  const rune = new THREE.MeshBasicMaterial({ color: 0x40ff80 });
  const haloMat = new THREE.MeshBasicMaterial({ color: 0x30ff70, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, side: THREE.BackSide, opacity: 0.16 });
  const sh = new THREE.Shape();                                     // the blade, seen from the side: spine on top, belly on the edge, a clipped point
  sh.moveTo(0, 0.0165); sh.lineTo(0.17, 0.0165); sh.quadraticCurveTo(0.238, 0.0165, 0.276, 0.0);
  sh.quadraticCurveTo(0.2, -0.0165, 0.1, -0.0185); sh.lineTo(0, -0.0185); sh.lineTo(0, 0.0165);
  const bg = new THREE.ExtrudeGeometry(sh, { depth: 0.0035, bevelEnabled: true, bevelThickness: 0.0016, bevelSize: 0.0016, bevelSegments: 2, curveSegments: 16 });
  bg.translate(0, 0, -0.00175); bg.rotateY(Math.PI / 2);          // u -> -Z, thickness -> X
  k.add(new THREE.Mesh(bg, steel));
  k.add(new THREE.LineSegments(new THREE.EdgesGeometry(bg, 35), new THREE.LineBasicMaterial({ color: 0xeafff2, transparent: true, opacity: 0.85 })));      // the ground edge catches the light
  const halo = new THREE.Mesh(bg, haloMat); halo.scale.set(3.2, 1.35, 1.06); k.add(halo);
  const fuller = new THREE.Mesh(new THREE.BoxGeometry(0.0086, 0.0034, 0.165), rune); fuller.position.set(0, 0.0, -0.1); k.add(fuller);
  for (let i = 0; i < 3; i++) { const d = new THREE.Mesh(new THREE.BoxGeometry(0.0092, 0.0075, 0.0075), rune); d.position.set(0, 0, -0.06 - i * 0.045); d.rotation.z = Math.PI / 4; k.add(d); }
  const guard = new THREE.Mesh(new THREE.BoxGeometry(0.078, 0.0115, 0.013), brass); guard.position.z = 0.006; k.add(guard);
  for (const s of [-1, 1]) { const c = new THREE.Mesh(new THREE.SphereGeometry(0.0072, 14, 10), brass); c.position.set(s * 0.039, 0, 0.006); k.add(c); }
  const handle = new THREE.Mesh(new THREE.CylinderGeometry(0.0125, 0.0148, 0.112, 20), leather); handle.rotation.x = Math.PI / 2; handle.position.z = 0.07; k.add(handle);
  for (let i = 0; i < 9; i++) { const w = new THREE.Mesh(new THREE.TorusGeometry(0.0138 + i * 0.00012, 0.0022, 8, 22), leather); w.position.z = 0.022 + i * 0.0108; w.scale.z = 1.4; k.add(w); }
  const pommel = new THREE.Mesh(new THREE.SphereGeometry(0.0188, 20, 14), brass); pommel.position.z = 0.13; pommel.scale.z = 0.85; k.add(pommel);
  const gem = new THREE.Mesh(new THREE.SphereGeometry(0.0085, 14, 10), rune); gem.position.z = 0.146; k.add(gem);
  const tip = new THREE.Object3D(); tip.position.set(0, 0, 0.065 - 0.278); root.add(tip);     // the point of the blade, in the root's space
  root.userData = { tip, rune, halo: haloMat };
  return root;
}
const knife = buildKnife();
let knifeGlow = 0.35, tipPrev = null;

/* ======================================================= the cut: a glowing trail that follows the point */

const TR_MAX = 900;
const trail = { pts: [], t: [], len: 0, fade: 1, fading: false };
const trGeo = new THREE.BufferGeometry();
const trPosA = new Float32Array(TR_MAX * 2 * 3), trT = new Float32Array(TR_MAX * 2), trV = new Float32Array(TR_MAX * 2);
{ const idx = []; for (let i = 0; i < TR_MAX - 1; i++) { const a = i * 2; idx.push(a, a + 1, a + 2, a + 1, a + 3, a + 2); } trGeo.setIndex(idx); }
trGeo.setAttribute('position', new THREE.BufferAttribute(trPosA, 3)); trGeo.setAttribute('aT', new THREE.BufferAttribute(trT, 1)); trGeo.setAttribute('aV', new THREE.BufferAttribute(trV, 1));
trGeo.setDrawRange(0, 0);
const trMat = new THREE.ShaderMaterial({
  uniforms: { uNow: { value: 0 }, uFade: { value: 1 } }, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide,
  vertexShader: 'attribute float aT; attribute float aV; varying float vT; varying float vV; void main() { vT = aT; vV = aV; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
  fragmentShader: `precision highp float; varying float vT; varying float vV; uniform float uNow; uniform float uFade;
    void main() { float age = uNow - vT; float hot = exp(-age * 2.2); float core = 1.0 - smoothstep(0.0, 0.5, abs(vV)); float halo = (1.0 - abs(vV)); halo *= halo * 0.4;
      vec3 col = mix(vec3(0.2, 1.0, 0.45), vec3(1.0, 1.0, 0.95), hot) * (core + halo) * (0.7 + 0.3 * hot); gl_FragColor = vec4(col * uFade, 1.0); }`,
});
const trailMesh = new THREE.Mesh(trGeo, trMat); trailMesh.frustumCulled = false; trailMesh.renderOrder = 50; scene.add(trailMesh);

const _t = new V(), _s = new V(), _v = new V();
function updateTrailGeometry() {
  const n = trail.pts.length;
  if (n < 2) { trGeo.setDrawRange(0, 0); return; }
  for (let i = 0; i < n; i++) {
    const p = trail.pts[i], a = trail.pts[Math.max(0, i - 1)], b = trail.pts[Math.min(n - 1, i + 1)];
    _t.copy(b).sub(a); _v.copy(head).sub(p); _s.crossVectors(_t, _v);
    if (_s.lengthSq() < 1e-10) _s.set(0, 1, 0); _s.normalize();
    const w = 0.0125 * (0.35 + 0.65 * Math.min(1, (i + 1) / 6));      // a hair-thin start
    trPosA[i * 6] = p.x - _s.x * w; trPosA[i * 6 + 1] = p.y - _s.y * w; trPosA[i * 6 + 2] = p.z - _s.z * w;
    trPosA[i * 6 + 3] = p.x + _s.x * w; trPosA[i * 6 + 4] = p.y + _s.y * w; trPosA[i * 6 + 5] = p.z + _s.z * w;
    trT[i * 2] = trT[i * 2 + 1] = trail.t[i]; trV[i * 2] = -1; trV[i * 2 + 1] = 1;
  }
  trGeo.attributes.position.needsUpdate = true; trGeo.attributes.aT.needsUpdate = true; trGeo.attributes.aV.needsUpdate = true;
  trGeo.setDrawRange(0, (n - 1) * 6);
}
function clearTrail() { trail.pts.length = 0; trail.t.length = 0; trail.len = 0; trail.fading = false; trail.fade = 1; trGeo.setDrawRange(0, 0); }
function fadeTrail() { trail.fading = true; }

/* the cutting state machine */
const cut = { state: 'idle', wait: false };
const LOOP = { minLen: 1.1, closeDist: 0.2, minW: 0.22, minH: 0.3 };
function cutterUpdate(dt, tip, pressed) {
  if (cut.wait && !pressed) cut.wait = false;
  const speed = tipPrev ? tip.distanceTo(tipPrev) / Math.max(dt, 1e-3) : 0;
  if (cut.state === 'idle' && pressed && !cut.wait) {
    clearTrail(); trail.pts.push(tip.clone()); trail.t.push(S.time); cut.state = 'cutting'; S.msg = '';
  } else if (cut.state === 'cutting') {
    if (!pressed) { cut.state = 'idle'; if (trail.len > 0.15) say('Snittet blev ikke lukket'); fadeTrail(); }
    else {
      const last = trail.pts[trail.pts.length - 1], d = tip.distanceTo(last);
      if (d >= 0.012) {
        trail.pts.push(tip.clone()); trail.t.push(S.time); trail.len += d;
        if (trail.pts.length >= TR_MAX - 2) { cut.state = 'idle'; cut.wait = true; say('For langt et snit'); fadeTrail(); }
        else if (trail.len >= LOOP.minLen && trail.pts.length > 30 && tip.distanceTo(trail.pts[0]) < LOOP.closeDist) closeLoop();
      }
      if (speed > 0.15 && Math.random() < 0.9) {                                  // sparks off the point of the blade
        for (let i = 0; i < 2; i++) tipSparks.emit(tip.x, tip.y, tip.z, rnd(-0.5, 0.5), rnd(0, 0.8), rnd(-0.5, 0.5), rnd(0.25, 0.7));
      }
    }
  }
  if (!trail.fading || cut.state === 'cutting') trMat.uniforms.uFade.value = 1;
  knifeGlow += ((cut.state === 'cutting' ? 1.0 : 0.35) - knifeGlow) * Math.min(1, dt * 6);
  hissLevel(cut.state === 'cutting' ? clamp(speed / 1.2, 0, 1) : 0);
  if (cut.state === 'cutting') haptic(0.12 + Math.min(0.25, speed * 0.2), 30);
  tipPrev = tipPrev || new V(); tipPrev.copy(tip);
}

/** The loop is closed. Fit a door to it: its centre and which way it faces; the size is always door-sized. */
const _n = new V(), _c = new V(), _a = new V(), _b = new V(), _x = new V(), _y = new V(0, 1, 0);
function closeLoop() {
  const P = trail.pts, n = P.length;
  _c.set(0, 0, 0); for (const p of P) _c.add(p); _c.multiplyScalar(1 / n);
  _n.set(0, 0, 0);
  for (let i = 0; i < n; i++) { _a.copy(P[i]).sub(_c); _b.copy(P[(i + 1) % n]).sub(_c); _n.add(_a.cross(_b)); }       // Newell: the normal of the loop
  cut.state = 'idle'; cut.wait = true;
  if (_n.lengthSq() < 1e-8) { say('Skær en rigtig ring'); fadeTrail(); return; }
  _n.normalize();
  if (Math.abs(_n.y) > 0.75) { say('Skær lodret, som en dør'); fadeTrail(); return; }
  _n.y = 0; _n.normalize();
  _x.crossVectors(_y, _n);                                         // x = up x n: right-handed with (x, up, n)
  let minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
  for (const p of P) { _a.copy(p).sub(_c); const px = _a.dot(_x), py = _a.dot(_y); minX = Math.min(minX, px); maxX = Math.max(maxX, px); minY = Math.min(minY, py); maxY = Math.max(maxY, py); }
  const w = maxX - minX, h = maxY - minY;
  if (w < LOOP.minW || h < LOOP.minH) { say('Skær en større ring'); fadeTrail(); return; }
  const centre = _c.clone().addScaledVector(_x, (minX + maxX) / 2).addScaledVector(_y, (minY + maxY) / 2);
  igniteAt(centre, _n.clone(), clamp(h / (2 * DOOR.Ry), 0.12, 1));
}

let hapticPulse = () => {};
function haptic(v, ms) { hapticPulse(v, ms); }

/* =========================================================== ignition, placing & crossing */

function igniteAt(from, n, s0, dur = 1.35) {
  if (new V().subVectors(head, from).dot(n) < 0) n.negate();                 // the door faces you
  portal.n.copy(n); portal.x.crossVectors(_y, n); portal.y.set(0, 1, 0);
  const to = new V(from.x, DOOR.Ry + 0.03, from.z);
  portal.anim = { t: 0, dur, from: from.clone(), to, s0 };
  portal.c.copy(from);
  setRingShape(DOOR.Rx, DOOR.Ry); setShockShape(DOOR.Rx, DOOR.Ry);
  const m = new THREE.Matrix4().makeBasis(portal.x, portal.y, portal.n);
  ring.quaternion.setFromRotationMatrix(m); shock.quaternion.copy(ring.quaternion);
  ring.position.copy(from); ring.scale.set(s0, s0, 1); ring.visible = true;
  const yaw = Math.atan2(n.x, n.z);                                           // the worlds' decorations follow the door
  decoB.position.set(to.x, 0, to.z); decoB.rotation.set(0, yaw, 0);
  decoA.position.set(to.x, 0, to.z); decoA.rotation.set(0, yaw, 0);
  S.placed = true; prevHead.copy(head);
  shock.position.copy(to); shockT = 0;
  flash.position.copy(from); flash.userData.t = 0; flash.userData.from = from.clone(); flash.userData.to = to.clone();
  knifeGlow = 2.2; fadeTrail(); zap(); haptic(1, 240);
  say('');
}
function updateAnim(dt) {
  const a = portal.anim;
  if (a) {
    a.t += dt; const u = clamp(a.t / a.dur, 0, 1);
    const sc = a.s0 + (1 - a.s0) * easeOutBack(u);
    ring.scale.set(sc, sc, 1);
    portal.c.lerpVectors(a.from, a.to, easeOutCubic(u)); ring.position.copy(portal.c);
    if (u >= 1) { portal.anim = null; portal.c.copy(a.to); ring.position.copy(a.to); ring.scale.set(1, 1, 1); prevHead.copy(head); }
  }
  // flash, shock, light
  const ft = flash.userData.t;
  if (ft !== undefined && ft < 0.9) {
    flash.userData.t += dt; const u = clamp(flash.userData.t / 0.9, 0, 1);
    flash.material.opacity = (1 - u) * (1 - u) * 1.0; flash.scale.setScalar(1.2 + 3.0 * easeOutCubic(u)); flash.position.copy(ring.position);
  } else flash.material.opacity = 0;
  if (shockT < 0.8) {
    shockT += dt; const u = clamp(shockT / 0.8, 0, 1);
    shock.scale.set(1 + 1.0 * easeOutCubic(u), 1 + 0.55 * easeOutCubic(u), 1); shock.material.opacity = (1 - u) * 0.9;
  } else shock.material.opacity = 0;
  const burst = ft !== undefined && ft < 1.2 ? Math.max(0, 1 - ft / 1.2) : 0;
  ringLight.position.copy(ring.position).addScaledVector(portal.n, 0.5);
  ringLight.intensity = S.placed ? 1.2 + burst * 9 + Math.sin(S.time * 37) * 0.25 * Math.random() : 0;
}

const _r = new V();
function checkCrossing() {
  if (!S.placed || portal.anim) { prevHead.copy(head); return; }
  const d0 = _r.copy(prevHead).sub(portal.c).dot(portal.n), d1 = new V().copy(head).sub(portal.c).dot(portal.n);
  if ((d0 < 0) !== (d1 < 0)) {
    const t = d0 / (d0 - d1), p = prevHead.clone().lerp(head, t).sub(portal.c);
    const lx = p.dot(portal.x) / portal.Rx, ly = p.dot(portal.y) / portal.Ry;
    if (lx * lx + ly * ly < 0.95) { S.inB = !S.inB; S.crossings++; applyRoles(); whoosh(); haptic(0.6, 120); }
  }
  prevHead.copy(head);
}
function updateClip() {
  const d = new V().copy(head).sub(portal.c).dot(portal.n);
  S.side = d >= 0 ? 1 : -1;
  const other = S.inB ? 'A' : 'B', home = S.inB ? 'B' : 'A';
  clip[other].setFromNormalAndCoplanarPoint(portal.n.clone().multiplyScalar(-S.side), portal.c);   // keep only what is behind the door, seen from where you stand
  clip[home].set(new V(0, 1, 0), 1e6);
}

/* ================================================================ sound */

const AU = { ctx: null };
function noiseBuffer(ctx, secs) { const n = (ctx.sampleRate * secs) | 0, b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0); for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1; return b; }
function initAudio() {
  if (AU.ctx) { AU.ctx.resume?.(); return; }
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)(); AU.ctx = ctx;
    const len = ctx.sampleRate * 2, buf = ctx.createBuffer(1, len, ctx.sampleRate), d = buf.getChannelData(0);
    let env = 0; for (let i = 0; i < len; i++) { if (Math.random() < 0.0016) env = rnd(0.5, 1); d[i] = (Math.random() * 2 - 1) * env; env *= 0.985; }
    const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 3000; bp.Q.value = 0.6;
    const g = ctx.createGain(); g.gain.value = 0;                                   // the ring is silent until it is lit
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 1; pan.rolloffFactor = 1.2;
    const hum = ctx.createOscillator(); hum.type = 'sawtooth'; hum.frequency.value = 58;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 240; const hg = ctx.createGain(); hg.gain.value = 0;
    src.connect(bp); bp.connect(g); g.connect(pan); hum.connect(lp); lp.connect(hg); hg.connect(pan); pan.connect(ctx.destination);
    src.start(); hum.start(); AU.pan = pan; AU.ringGain = g; AU.humGain = hg;
    const hs = ctx.createBufferSource(); hs.buffer = noiseBuffer(ctx, 1.5); hs.loop = true;           // the sizzle of the blade while it cuts
    const hf = ctx.createBiquadFilter(); hf.type = 'bandpass'; hf.frequency.value = 5200; hf.Q.value = 0.8; const hgn = ctx.createGain(); hgn.gain.value = 0;
    hs.connect(hf); hf.connect(hgn); hgn.connect(ctx.destination); hs.start(); AU.hiss = hgn;
  } catch (e) { console.warn('no audio', e); }
}
function hissLevel(v) { if (AU.hiss) AU.hiss.gain.setTargetAtTime(0.16 * v, AU.ctx.currentTime, 0.05); }
function whoosh() {
  const ctx = AU.ctx; if (!ctx) return;
  const s = ctx.createBufferSource(); s.buffer = noiseBuffer(ctx, 0.8);
  const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 1.2;
  f.frequency.setValueAtTime(300, ctx.currentTime); f.frequency.exponentialRampToValueAtTime(2600, ctx.currentTime + 0.3); f.frequency.exponentialRampToValueAtTime(350, ctx.currentTime + 0.8);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.5, ctx.currentTime + 0.25); g.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + 0.8);
  s.connect(f); f.connect(g); g.connect(ctx.destination); s.start();
}
/** The moment the door tears open: a crack, a low thud and the ring's crackle swelling up. */
function zap() {
  const ctx = AU.ctx; if (!ctx) return; const t = ctx.currentTime;
  const s = ctx.createBufferSource(); s.buffer = noiseBuffer(ctx, 0.9);
  const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.setValueAtTime(900, t); hp.frequency.exponentialRampToValueAtTime(5000, t + 0.5);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.7, t + 0.02); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.9);
  s.connect(hp); hp.connect(g); g.connect(ctx.destination); s.start(t);
  const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.setValueAtTime(150, t); o.frequency.exponentialRampToValueAtTime(38, t + 0.7);
  const og = ctx.createGain(); og.gain.setValueAtTime(0.0001, t); og.gain.exponentialRampToValueAtTime(0.9, t + 0.03); og.gain.exponentialRampToValueAtTime(0.0001, t + 0.9);
  o.connect(og); og.connect(ctx.destination); o.start(t); o.stop(t + 1);
  if (AU.ringGain) { AU.ringGain.gain.cancelScheduledValues(t); AU.ringGain.gain.setValueAtTime(0.0, t); AU.ringGain.gain.linearRampToValueAtTime(0.6, t + 1.2); AU.humGain.gain.setValueAtTime(0, t); AU.humGain.gain.linearRampToValueAtTime(0.07, t + 1.2); }
}
function updateAudio() {
  const ctx = AU.ctx; if (!ctx || !AU.pan) return;
  const L = ctx.listener, t = ctx.currentTime;
  const fw = new V(0, 0, -1).applyQuaternion(headQ), up = new V(0, 1, 0).applyQuaternion(headQ);
  if (L.positionX) { L.positionX.setValueAtTime(head.x, t); L.positionY.setValueAtTime(head.y, t); L.positionZ.setValueAtTime(head.z, t);
    L.forwardX.setValueAtTime(fw.x, t); L.forwardY.setValueAtTime(fw.y, t); L.forwardZ.setValueAtTime(fw.z, t); L.upX.setValueAtTime(up.x, t); L.upY.setValueAtTime(up.y, t); L.upZ.setValueAtTime(up.z, t);
    AU.pan.positionX.setValueAtTime(portal.c.x, t); AU.pan.positionY.setValueAtTime(portal.c.y, t); AU.pan.positionZ.setValueAtTime(portal.c.z, t); }
}

/* ================================================================ the board on the other controller */

const hudCanvas = document.createElement('canvas'); hudCanvas.width = 512; hudCanvas.height = 300;
const hudTex = new THREE.CanvasTexture(hudCanvas); hudTex.colorSpace = THREE.SRGBColorSpace;
const hudMesh = new THREE.Mesh(new THREE.PlaneGeometry(0.2, 0.117), new THREE.MeshBasicMaterial({ map: hudTex, transparent: true, depthTest: false }));
hudMesh.renderOrder = 100; hudMesh.position.set(0, 0.09, -0.04); hudMesh.rotation.x = -1.0;
function hudLines() {
  const bits = S.stencilBits, msg = S.time - S.msgT < 4 ? S.msg : '';
  return [
    'PORTALKNIVEN',
    `Verden: ${S.inB ? 'B  (den lilla)' : 'A  (din stue)'}   Gennemgange: ${S.crossings}`,
    S.mode === 'xr' ? `Stencil: ${bits === null ? '?' : bits}${bits === 0 ? '  VINDUET VIRKER IKKE' : ''}   Scan: ${S.scanFeature ? S.scanMeshes + ' mesh' : 'ingen'}` : 'Fladt preview',
    `Rumlys: ${S.lum < 0 ? 'ingen' : S.lum.toFixed(2)}   Kamera: ${S.camera ? 'ja' : 'nej'}`,
    cut.state === 'cutting' ? 'Skærer … luk ringen!' : S.placed ? 'Skær en ny ring for at flytte døren' : 'Hold udløseren og skær en ring i luften',
    msg,
  ];
}
let hudNext = 0;
function drawHud() {
  const now = performance.now(); if (now < hudNext) return; hudNext = now + 200;
  const lines = hudLines();
  if (S.mode === 'flat') { $('hud').textContent = lines.filter(Boolean).join('\n') + '\n(WASD, træk = kig, Shift+træk = skær, P = sæt ring)'; return; }
  const x = hudCanvas.getContext('2d');
  x.clearRect(0, 0, 512, 300); x.fillStyle = 'rgba(4,10,8,.82)'; x.fillRect(0, 0, 512, 300); x.strokeStyle = '#4dff88'; x.lineWidth = 6; x.strokeRect(3, 3, 506, 294);
  x.fillStyle = '#4dff88'; x.font = 'bold 34px monospace'; x.fillText(lines[0], 22, 50);
  x.font = '22px monospace';
  lines.slice(1).forEach((l, i) => { x.fillStyle = l.includes('VIRKER IKKE') ? '#ff7a7a' : i === 3 ? '#ffd56a' : '#d7ffe4'; x.fillText(l, 22, 90 + i * 38); });
  hudTex.needsUpdate = true;
}

/* ================================================================ XR & flat */

const ctrl = [{ pressed: false, hand: '', src: null }, { pressed: false, hand: '', src: null }];
let knifeIdx = 1;
function assignTools() {
  const right = ctrl.findIndex(c => c.hand === 'right');
  if (right >= 0 && !ctrl.some(c => c.dragged)) knifeIdx = right;
  const grip = renderer.xr.getControllerGrip(knifeIdx);
  if (knife.parent !== grip) { knife.parent?.remove(knife); grip.add(knife); }
  const other = renderer.xr.getController(1 - knifeIdx);
  if (hudMesh.parent !== other) { hudMesh.parent?.remove(hudMesh); other.add(hudMesh); }
  hapticPulse = (v, ms) => { try { const a = ctrl[knifeIdx].src?.gamepad?.hapticActuators?.[0]; a?.pulse?.(clamp(v, 0, 1), ms); } catch (_) { /* no haptics */ } };
}

async function enterXR() {
  const status = $('status');
  try {
    initAudio();
    const session = await navigator.xr.requestSession('immersive-ar', { requiredFeatures: ['local-floor'], optionalFeatures: ['mesh-detection', 'light-estimation', 'camera-access'] });
    session.addEventListener('end', () => location.reload());
    S.camera = !!(session.enabledFeatures && session.enabledFeatures.includes('camera-access'));
    try { if (session.requestLightProbe) session.requestLightProbe().then(p => { lightProbe = p; }).catch(() => {}); } catch (_) { /* no light estimation */ }
    await renderer.xr.setSession(session);
    S.mode = 'xr';
    canvas.style.display = 'block'; document.body.classList.add('playing');
    for (let i = 0; i < 2; i++) {
      const c = renderer.xr.getController(i); scene.add(c); scene.add(renderer.xr.getControllerGrip(i));
      c.addEventListener('selectstart', () => { ctrl[i].pressed = true; initAudio(); });
      c.addEventListener('selectend', () => { ctrl[i].pressed = false; });
      c.addEventListener('squeezestart', () => { knifeIdx = i; ctrl[1 - i].dragged = false; ctrl[i].dragged = true; assignTools(); });      // grip: take the knife in this hand
      c.addEventListener('connected', e => { ctrl[i].src = e.data; ctrl[i].hand = e.data?.handedness || ''; assignTools(); });
    }
    assignTools();
    say('Hold udløseren og skær en ring i luften');
  } catch (e) { status.textContent = 'Kunne ikke starte headset-sessionen: ' + (e.message || e); console.error(e); }
}

const keys = {}; let yaw = 0, pitch = 0;
const flatTip = { pos: new V(0, 1.4, -1.2), pressed: false, override: false };
const ray = new THREE.Raycaster();
function enterFlat() {
  initAudio();
  S.mode = 'flat'; document.body.classList.add('playing', 'flat');
  canvas.style.display = 'block';
  scene.add(camera); scene.add(knife);
  addEventListener('keydown', e => { keys[e.code] = true; if (e.code === 'KeyP') placeDirect(); });
  addEventListener('keyup', e => { keys[e.code] = false; });
  let drag = null;
  const aim = e => { ray.setFromCamera(new THREE.Vector2(e.clientX / innerWidth * 2 - 1, -(e.clientY / innerHeight) * 2 + 1), camera); flatTip.pos.copy(ray.ray.origin).addScaledVector(ray.ray.direction, 1.3); };
  canvas.addEventListener('pointerdown', e => { if (e.shiftKey) { flatTip.pressed = true; aim(e); } else drag = { x: e.clientX, y: e.clientY }; canvas.setPointerCapture(e.pointerId); });
  canvas.addEventListener('pointerup', () => { drag = null; flatTip.pressed = false; });
  canvas.addEventListener('pointermove', e => {
    if (flatTip.pressed) { aim(e); return; }
    if (!drag) return; yaw -= (e.clientX - drag.x) * 0.004; pitch = clamp(pitch - (e.clientY - drag.y) * 0.004, -1.4, 1.4); drag.x = e.clientX; drag.y = e.clientY;
  });
  head.set(0, 1.6, 0); headQ.identity(); camera.position.copy(head); prevHead.copy(head);
  say('Shift + træk: skær en ring (eller P)');
}
function placeDirect() {                                           // a test shortcut: a ring 1.6 m ahead with no cutting
  const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; if (f.lengthSq() < 0.01) f.set(0, 0, -1); f.normalize();
  igniteAt(new V().copy(head).addScaledVector(f, 1.6).setY(DOOR.Ry + 0.03), f.negate(), 1, 0.01);
}
function flatMove(dt) {
  const f = (keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0), s = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0);
  if (f || s) { const sp = 1.6 * dt; camera.position.x += (-Math.sin(yaw) * f + Math.cos(yaw) * s) * sp; camera.position.z += (-Math.cos(yaw) * f - Math.sin(yaw) * s) * sp; }
  camera.rotation.set(pitch, yaw, 0);
  head.copy(camera.position); headQ.copy(camera.quaternion);
  // the knife: a view model in the lower right, or following the point while cutting
  if (knife.userData.free) return;                                 // a test hook: a knife placed by hand
  const tipLocal = knife.userData.tip.position;
  knife.quaternion.copy(camera.quaternion);
  if (flatTip.pressed || flatTip.override) knife.position.copy(flatTip.pos).sub(_v.copy(tipLocal).applyQuaternion(camera.quaternion));
  else knife.position.copy(camera.position).add(_v.set(0.2, -0.2, -0.42).applyQuaternion(camera.quaternion));
}

addEventListener('resize', () => { if (S.mode === 'xr') return; renderer.setSize(innerWidth, innerHeight, false); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); });

let lightProbe = null;
const clock = new THREE.Clock(), tipW = new V();
function loop(_, frame) {
  const dt = Math.min(0.05, clock.getDelta()); S.time += dt; S.frames++;
  if (S.mode === 'flat') flatMove(dt);
  else if (S.mode === 'xr' && frame) {
    const ref = renderer.xr.getReferenceSpace(), vp = frame.getViewerPose(ref);
    if (vp) { const p = vp.transform.position, o = vp.transform.orientation; head.set(p.x, p.y, p.z); headQ.set(o.x, o.y, o.z, o.w); }
    updateScan(frame, ref);
    if (lightProbe) { try { const est = frame.getLightEstimate(lightProbe), sh = est && est.sphericalHarmonicsCoefficients; if (sh && sh.length >= 3) { const lum = (0.2126 * sh[0] + 0.7152 * sh[1] + 0.0722 * sh[2]) / 3.54; S.lum = lum; S.brightTarget = clamp(0.5 + lum * 2.0, 0.5, 1.5); } } catch (_) { /* no estimate this frame */ } }
  }
  if (S.mode !== 'menu') {
    knife.updateWorldMatrix(true, true); knife.userData.tip.getWorldPosition(tipW);
    const pressed = S.mode === 'xr' ? ctrl[knifeIdx].pressed : flatTip.pressed;
    cutterUpdate(dt, S.mode === 'flat' && (flatTip.pressed || flatTip.override) ? flatTip.pos : tipW, pressed);
    if (trail.fading) { trail.fade -= dt / 0.8; trMat.uniforms.uFade.value = Math.max(0, trail.fade); if (trail.fade <= 0) clearTrail(); }
    trMat.uniforms.uNow.value = S.time; updateTrailGeometry();
    tipSparks.step(dt, 1.2);
    knife.userData.rune.color.setRGB(0.25 * knifeGlow, 1.0 * Math.min(1.4, knifeGlow), 0.5 * knifeGlow);
    knife.userData.halo.opacity = clamp(0.06 + 0.4 * knifeGlow, 0, 1);
    updateAnim(dt); checkCrossing(); updateClip(); updateRing(dt); updateAudio(); drawHud();
    for (const q of skyMats) q.uniforms.uTime.value = S.time;
    orgTime.value = S.time;
    if (decoB.userData.stalks) for (const g of decoB.userData.stalks) { g.rotation.z = Math.sin(S.time * 0.5 + g.userData.p) * g.userData.sw; g.rotation.x = Math.cos(S.time * 0.4 + g.userData.p) * g.userData.sw * 0.7; }
    if (decoB.userData.spores) { const a = decoB.userData.spores.geometry.attributes.position, A = a.array; for (let i = 0; i < 150; i++) { A[i * 3 + 1] += (0.03 + 0.02 * Math.sin(S.time * 0.7 + i)) * dt; A[i * 3] += Math.sin(S.time * 0.3 + i * 1.7) * 0.02 * dt; if (A[i * 3 + 1] > 2.7) A[i * 3 + 1] = 0.1; } a.needsUpdate = true; }
    S.bright += (S.brightTarget - S.bright) * Math.min(1, dt * 1.5); hemi.intensity = 1.0 * S.bright; fillB.material.uniforms.uBright.value = bgHomeB.material.uniforms.uBright.value = S.bright; scene.fog.color.copy(FOG_COL).multiplyScalar(S.bright);
    if (decoA.userData.orb) decoA.userData.orb.position.y = 1.7 + Math.sin(S.time) * 0.06;
    renderer.render(scene, camera);
  }
}

applyRoles();
renderer.setAnimationLoop(loop);

(async function init() {
  const btnAR = $('btnAR'), btnFlat = $('btnFlat'), status = $('status');
  btnFlat.addEventListener('click', enterFlat); btnAR.addEventListener('click', enterXR);
  let ok = false;
  try { ok = !!(navigator.xr && await navigator.xr.isSessionSupported('immersive-ar')); } catch (_) { /* no WebXR */ }
  btnAR.disabled = !ok;
  status.textContent = ok ? 'Klar. Tryk på Start i headset.' : 'Headset-tilstand (immersive-ar) findes ikke her. Du kan se et fladt preview på computer.';
  if (params.has('flat')) enterFlat();
})();

window.__portal = {
  S, portal, head, scene, renderer, camera, knife, cut, trail, applyRoles, mats, clip, placeDirect, igniteAt,
  view(x, y, z, yw, pt) { camera.position.set(x, y, z); yaw = yw; pitch = pt; prevHead.copy(head.set(x, y, z)); },
  walk(x, y, z) { camera.position.set(x, y, z); head.set(x, y, z); checkCrossing(); },
  tip(x, y, z, pressed) { flatTip.override = true; flatTip.pos.set(x, y, z); flatTip.pressed = pressed; },
};
