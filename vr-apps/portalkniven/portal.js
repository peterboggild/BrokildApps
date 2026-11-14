/*
 * Portalkniven (technical test) — a green crackling ring in the room that is an open door to another version of the room.
 *
 * HOW IT WORKS (this is the whole trick, and the thing this page exists to prove on a Quest 3):
 *
 *  - The ring's inside is a "mask" disc that writes 1 into the STENCIL buffer, wherever it is visible
 *    (it is depth-tested, so anything real or virtual in front of the ring hides it).
 *  - A full-screen quad at the far plane, drawn only where stencil == 1, resets the depth there and paints the
 *    other world's backdrop. After that the other world's objects are drawn, again only where stencil == 1, and
 *    clipped to the far side of the ring's plane (so nothing "in front of the door" leaks into the window).
 *  - The headset renders each eye from its own position, so the window is a real window: parallax, looking
 *    round the edge, walking up to it all work, with no render-to-texture.
 *  - World A is "your room" (passthrough). World B is the other universe (opaque, drawn over the passthrough).
 *    Walking through the disc swaps which one is "home". The swap is invisible: the picture is identical the
 *    frame before and after. Seen from B, the hole is cleared to alpha 0, so it shows the real room (passthrough).
 *  - If the headset has scanned the room (mesh-detection), the real walls and furniture become world B's walls,
 *    reskinned, so the other universe lines up with the room you can really walk in.
 */
import * as THREE from './vendor/three.module.min.js';

const $ = id => document.getElementById(id);
const params = new URLSearchParams(location.search);
const V = THREE.Vector3;
const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);

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
scene.add(new THREE.HemisphereLight(0xb8a8ff, 0x201030, 1.4));
const sun = new THREE.DirectionalLight(0xffd0f0, 1.2); sun.position.set(2, 5, 1); scene.add(sun);

const S = { inB: false, side: 1, placed: false, mode: 'menu', stencilBits: null, crossings: 0, frames: 0, scanMeshes: 0, scanFeature: false, time: 0 };
const portal = { c: new V(0, 0.93, -1.6), n: new V(0, 0, 1), R: 0.85 };
const head = new V(0, 1.6, 0), prevHead = new V(0, 1.6, 0), headQ = new THREE.Quaternion();

/* ============================================ the two worlds: roles & stencil */

const worldA = new THREE.Group(), worldB = new THREE.Group();
scene.add(worldA, worldB);
const world = { A: worldA, B: worldB };
const mats = { A: [], B: [] };
const clip = { A: new THREE.Plane(new V(0, 1, 0), 1e6), B: new THREE.Plane(new V(0, 1, 0), 1e6) };   // 1e6: lets everything through

/** Every material of a world is registered here, so the world can be switched between "home" and "seen through the ring". */
function reg(w, m) {
  m.clippingPlanes = [clip[w]];
  m.stencilWrite = false; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc;
  m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp;
  mats[w].push(m);
  return m;
}

/** Home world: drawn normally. The other one: drawn only inside the ring (stencil == 1), after the depth reset. */
const ORDER = { bgHome: -10, home: 0, mask: 10, fill: 20, other: 30 };
let bgHomeB, fillB, fillA;
function applyRoles() {
  for (const w of ['A', 'B']) {
    const home = (w === 'B') === S.inB;
    for (const m of mats[w]) m.stencilWrite = !home;
    world[w].traverse(o => { if (o.isMesh || o.isPoints || o.isLine) o.renderOrder = home ? ORDER.home : ORDER.other; });
  }
  bgHomeB.visible = S.inB;       // inside B the whole view is B's sky
  fillB.visible = !S.inB;        // from A: B's sky + depth reset inside the ring
  fillA.visible = S.inB;         // from B: the ring shows alpha 0 = the real room (passthrough)
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
  varying vec3 vDir; uniform float uTime;
  float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
  float noise(vec3 x) { vec3 i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x), mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x), mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z); }
  void main() {
    vec3 d = normalize(vDir);
    float h = d.y * 0.5 + 0.5;
    vec3 col = mix(vec3(0.26, 0.05, 0.30), vec3(0.03, 0.02, 0.16), smoothstep(0.35, 0.95, h));
    float n = noise(d * 3.0 + vec3(0.0, uTime * 0.03, 0.0)) * 0.6 + noise(d * 8.0 - uTime * 0.02) * 0.4;
    col += vec3(0.55, 0.12, 0.65) * pow(n, 3.0) * 0.7;
    float s = hash(floor(d * 150.0)); col += step(0.992, s) * (0.5 + 0.5 * sin(uTime * 2.0 + s * 60.0));
    col += vec3(0.1, 0.5, 0.6) * exp(-abs(d.y) * 5.0) * 0.3;
    gl_FragColor = vec4(col, 1.0);
  }`;
const CLEAR_FS = 'void main() { gl_FragColor = vec4(0.0); }';
const stencilEqual = m => { m.stencilWrite = true; m.stencilRef = 1; m.stencilFunc = THREE.EqualStencilFunc; m.stencilFail = m.stencilZFail = m.stencilZPass = THREE.KeepStencilOp; return m; };
const quadGeo = new THREE.PlaneGeometry(2, 2);
function quad(fs, inRing) {
  const m = new THREE.ShaderMaterial({ vertexShader: QUAD_VS, fragmentShader: fs, uniforms: { uTime: { value: 0 } }, blending: THREE.NoBlending, depthTest: inRing, depthWrite: inRing, depthFunc: THREE.AlwaysDepth });
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

const decoA = new THREE.Group(); worldA.add(decoA);
{
  const orange = reg('A', new THREE.MeshLambertMaterial({ color: 0xff9a3c, emissive: 0x7a3a08, flatShading: true }));
  const warm = reg('A', new THREE.MeshBasicMaterial({ color: 0xffd9a0 }));
  for (let i = 0; i < 6; i++) {
    const h = rnd(0.5, 1.5);
    const p = new THREE.Mesh(new THREE.BoxGeometry(0.16, h, 0.16), orange);
    p.position.set(rnd(-1.3, 1.3), h / 2, rnd(0.6, 2.6)); decoA.add(p);
  }
  const orb = new THREE.Mesh(new THREE.IcosahedronGeometry(0.12, 1), warm); orb.position.set(0, 1.7, 1.8); decoA.add(orb); decoA.userData.orb = orb;
  if (params.has('flat') || params.get('floor') === '1') {                    // a floor to stand on, only on a laptop
    const g = new THREE.Mesh(new THREE.PlaneGeometry(30, 30), reg('A', new THREE.MeshBasicMaterial({ map: gridTex('#1b2230', '#33445e', 60), side: THREE.DoubleSide })));
    g.rotation.x = -Math.PI / 2; g.position.y = -0.005; worldA.add(g);
  }
}

/* ================================================================ world B (the other universe) */

function gridTex(bg, line, repeat) {
  const c = document.createElement('canvas'); c.width = c.height = 256;
  const x = c.getContext('2d'); x.fillStyle = bg; x.fillRect(0, 0, 256, 256);
  x.strokeStyle = line; x.lineWidth = 4; x.strokeRect(2, 2, 252, 252);
  x.globalAlpha = 0.35; x.lineWidth = 2; x.beginPath(); x.moveTo(128, 0); x.lineTo(128, 256); x.moveTo(0, 128); x.lineTo(256, 128); x.stroke();
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(repeat, repeat); t.anisotropy = 4; t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
const decoB = new THREE.Group(); worldB.add(decoB);
const scanGroup = new THREE.Group(); worldB.add(scanGroup);
{
  const floor = new THREE.Mesh(new THREE.PlaneGeometry(60, 60), reg('B', new THREE.MeshBasicMaterial({ map: gridTex('#1a0a2a', '#b04cff', 60), side: THREE.DoubleSide })));
  floor.rotation.x = -Math.PI / 2; floor.position.y = -0.012; worldB.add(floor);
  const crys = [reg('B', new THREE.MeshLambertMaterial({ color: 0x6a2cff, emissive: 0x2a0a8a, flatShading: true })),
                reg('B', new THREE.MeshLambertMaterial({ color: 0xff3cc8, emissive: 0x80105a, flatShading: true })),
                reg('B', new THREE.MeshLambertMaterial({ color: 0x28e0d0, emissive: 0x0a6a64, flatShading: true }))];
  const orbM = reg('B', new THREE.MeshBasicMaterial({ color: 0xffa0ff }));
  for (let i = 0; i < 16; i++) {                                  // crystals round the door: a tall cluster straight behind it
    const h = i < 5 ? rnd(1.2, 2.6) : rnd(0.3, 1.4);
    const a = i < 5 ? rnd(-0.5, 0.5) : rnd(-1.4, 1.4), d = i < 5 ? rnd(1.8, 3.4) : rnd(0.8, 5);
    const m = new THREE.Mesh(new THREE.ConeGeometry(rnd(0.08, 0.22), h, i % 2 ? 5 : 6), crys[i % 3]);
    m.position.set(Math.sin(a) * d, h / 2, -Math.cos(a) * d); m.rotation.set(rnd(-0.12, 0.12), rnd(0, TAU), rnd(-0.12, 0.12)); decoB.add(m);
  }
  decoB.userData.orbs = [];
  for (let i = 0; i < 6; i++) { const o = new THREE.Mesh(new THREE.IcosahedronGeometry(rnd(0.06, 0.14), 1), orbM); o.position.set(rnd(-1.6, 1.6), rnd(0.8, 2.2), -rnd(1.2, 4)); o.userData.p = rnd(0, TAU); o.userData.y = o.position.y; decoB.add(o); decoB.userData.orbs.push(o); }
}
/* the real room, reskinned: dark glassy purple walls with glowing edges */
const scanSolid = reg('B', new THREE.MeshLambertMaterial({ color: 0x2a1450, emissive: 0x120628, flatShading: true, side: THREE.DoubleSide, polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 }));
const scanWire = reg('B', new THREE.MeshBasicMaterial({ color: 0xd060ff, wireframe: true, transparent: true, opacity: 0.55 }));
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
    g.add(new THREE.Mesh(geo, scanSolid), new THREE.Mesh(geo, scanWire));
    scanGroup.add(g); scanKnown.set(m, { t: m.lastChangedTime, g, geo }); did++;
    applyRoles();
  }
  for (const [k, rec] of scanKnown) if (!seen.has(k)) { scanGroup.remove(rec.g); rec.geo.dispose(); scanKnown.delete(k); }
  S.scanMeshes = scanKnown.size;
}

/* ================================================================ the ring */

const ring = new THREE.Group(); scene.add(ring);
const maskMat = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: false, side: THREE.DoubleSide, stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp });
const mask = new THREE.Mesh(new THREE.CircleGeometry(portal.R * 0.985, 72), maskMat);
mask.renderOrder = ORDER.mask; mask.frustumCulled = false;
mask.onAfterRender = () => { if (S.stencilBits === null && S.mode === 'xr') S.stencilBits = gl.getParameter(gl.STENCIL_BITS); };   // asked while the headset's framebuffer is bound
ring.add(mask);

const RING_VS = `varying float vA; varying vec3 vN; varying vec3 vV;
  void main() { vA = atan(position.y, position.x); vec4 mv = modelViewMatrix * vec4(position, 1.0); vN = normalMatrix * normal; vV = -mv.xyz; gl_Position = projectionMatrix * mv; }`;
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
const ringMats = [];
function torus(tube, amp) {
  const m = new THREE.ShaderMaterial({ vertexShader: RING_VS, fragmentShader: RING_FS, uniforms: { uTime: { value: 0 }, uAmp: { value: amp } }, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false });
  ringMats.push(m);
  const t = new THREE.Mesh(new THREE.TorusGeometry(portal.R, tube, 14, 112), m); t.frustumCulled = false; ring.add(t);
}
torus(0.03, 1.0); torus(0.1, 0.22);

const ARCS = 16;
const arcPos = new Float32Array(ARCS * 3 * 2 * 3);
const arcGeo = new THREE.BufferGeometry(); arcGeo.setAttribute('position', new THREE.BufferAttribute(arcPos, 3));
const arcMat = new THREE.LineBasicMaterial({ color: 0xb8ffd0, transparent: true, opacity: 0.9, blending: THREE.AdditiveBlending, depthWrite: false });
const arcs = new THREE.LineSegments(arcGeo, arcMat); arcs.frustumCulled = false; ring.add(arcs);
function regenArc(k) {
  const a = rnd(0, TAU), R = portal.R, ca = Math.cos(a), sa = Math.sin(a);
  let px = ca * R, py = sa * R, pz = 0;
  const dr = rnd(-0.6, 1.0), dt = rnd(-0.5, 0.5), dz = rnd(-0.5, 0.5), len = rnd(0.12, 0.34) / 3;
  for (let s = 0; s < 3; s++) {
    const nx = px + (ca * dr - sa * dt) * len + rnd(-0.035, 0.035), ny = py + (sa * dr + ca * dt) * len + rnd(-0.035, 0.035), nz = pz + dz * len + rnd(-0.035, 0.035);
    const o = (k * 3 + s) * 6; arcPos[o] = px; arcPos[o + 1] = py; arcPos[o + 2] = pz; arcPos[o + 3] = nx; arcPos[o + 4] = ny; arcPos[o + 5] = nz;
    px = nx; py = ny; pz = nz;
  }
}
for (let k = 0; k < ARCS; k++) regenArc(k);

const SP = 150;
const spPos = new Float32Array(SP * 3), spVel = new Float32Array(SP * 3), spAge = new Float32Array(SP), spLife = new Float32Array(SP), spFrac = new Float32Array(SP);
const spGeo = new THREE.BufferGeometry();
spGeo.setAttribute('position', new THREE.BufferAttribute(spPos, 3)); spGeo.setAttribute('aFrac', new THREE.BufferAttribute(spFrac, 1));
const spMat = new THREE.ShaderMaterial({
  uniforms: { uScale: { value: 900 } }, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending,
  vertexShader: `attribute float aFrac; varying float vF; uniform float uScale; void main() { vF = aFrac; vec4 mv = modelViewMatrix * vec4(position, 1.0); gl_PointSize = clamp(0.022 * (1.0 - aFrac) * uScale / -mv.z, 1.5, 26.0); gl_Position = projectionMatrix * mv; }`,
  fragmentShader: `precision highp float; varying float vF; void main() { float d = length(gl_PointCoord - 0.5) * 2.0; float a = smoothstep(1.0, 0.0, d) * (1.0 - vF); gl_FragColor = vec4(mix(vec3(0.4, 1.0, 0.55), vec3(1.0), 1.0 - vF) * a, a); }`,
});
const sparks = new THREE.Points(spGeo, spMat); sparks.frustumCulled = false; ring.add(sparks);
function respawn(i) {
  const a = rnd(0, TAU), ca = Math.cos(a), sa = Math.sin(a), sp = rnd(0.08, 0.5) * (Math.random() < 0.5 ? 1 : -0.6);
  spPos[i * 3] = ca * portal.R; spPos[i * 3 + 1] = sa * portal.R; spPos[i * 3 + 2] = rnd(-0.03, 0.03);
  spVel[i * 3] = ca * sp + rnd(-0.1, 0.1); spVel[i * 3 + 1] = sa * sp + rnd(0, 0.25); spVel[i * 3 + 2] = rnd(-0.25, 0.25);
  spLife[i] = rnd(0.4, 1.3); spAge[i] = 0;
}
for (let i = 0; i < SP; i++) { respawn(i); spAge[i] = rnd(0, spLife[i]); }

function updateRing(dt) {
  for (const m of ringMats) m.uniforms.uTime.value = S.time;
  for (let i = 0; i < 4; i++) regenArc((Math.random() * ARCS) | 0);
  arcGeo.attributes.position.needsUpdate = true;
  arcMat.opacity = 0.55 + 0.45 * Math.random();
  for (let i = 0; i < SP; i++) {
    spAge[i] += dt;
    if (spAge[i] > spLife[i]) respawn(i);
    spVel[i * 3 + 1] -= 0.5 * dt;
    spPos[i * 3] += spVel[i * 3] * dt; spPos[i * 3 + 1] += spVel[i * 3 + 1] * dt; spPos[i * 3 + 2] += spVel[i * 3 + 2] * dt;
    spFrac[i] = spAge[i] / spLife[i];
  }
  spGeo.attributes.position.needsUpdate = true; spGeo.attributes.aFrac.needsUpdate = true;
}

/* ============================================================== placing & crossing */

const _f = new V(), _q = new THREE.Quaternion();
function placePortal() {
  _f.set(0, 0, -1).applyQuaternion(headQ); _f.y = 0;
  if (_f.lengthSq() < 0.01) _f.set(0, 0, -1); else _f.normalize();
  portal.c.copy(head).addScaledVector(_f, 1.6); portal.c.y = portal.R + 0.08;
  portal.n.copy(_f).negate();
  ring.position.copy(portal.c);
  ring.quaternion.setFromUnitVectors(new V(0, 0, 1), portal.n);
  const yaw = Math.atan2(portal.n.x, portal.n.z);                               // decorations follow the door: B behind it, A on the side you stood
  decoB.position.set(portal.c.x, 0, portal.c.z); decoB.rotation.set(0, yaw, 0);
  decoA.position.set(portal.c.x, 0, portal.c.z); decoA.rotation.set(0, yaw, 0);
  prevHead.copy(head); S.placed = true; ring.visible = true;
}

function checkCrossing() {
  if (!S.placed) return;
  const d0 = prevHead.clone().sub(portal.c).dot(portal.n), d1 = head.clone().sub(portal.c).dot(portal.n);
  if ((d0 < 0) !== (d1 < 0)) {
    const t = d0 / (d0 - d1), p = prevHead.clone().lerp(head, t);
    if (p.distanceTo(portal.c) < portal.R - 0.04) { S.inB = !S.inB; S.crossings++; applyRoles(); whoosh(); }
  }
  prevHead.copy(head);
}

function updateClip() {
  const d = head.clone().sub(portal.c).dot(portal.n);
  S.side = d >= 0 ? 1 : -1;
  const other = S.inB ? 'A' : 'B', home = S.inB ? 'B' : 'A';
  clip[other].setFromNormalAndCoplanarPoint(portal.n.clone().multiplyScalar(-S.side), portal.c);   // keep only what is behind the door, seen from where you stand
  clip[home].set(new V(0, 1, 0), 1e6);
}

/* ================================================================ sound */

const AU = { ctx: null };
function initAudio() {
  if (AU.ctx) { AU.ctx.resume?.(); return; }
  try {
    const ctx = new (window.AudioContext || window.webkitAudioContext)(); AU.ctx = ctx;
    const len = ctx.sampleRate * 2, buf = ctx.createBuffer(1, len, ctx.sampleRate), d = buf.getChannelData(0);
    let env = 0; for (let i = 0; i < len; i++) { if (Math.random() < 0.0016) env = rnd(0.5, 1); d[i] = (Math.random() * 2 - 1) * env; env *= 0.985; }
    const src = ctx.createBufferSource(); src.buffer = buf; src.loop = true;
    const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 3000; bp.Q.value = 0.6;
    const g = ctx.createGain(); g.gain.value = 0.6;
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 1; pan.rolloffFactor = 1.2;
    const hum = ctx.createOscillator(); hum.type = 'sawtooth'; hum.frequency.value = 58;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 240; const hg = ctx.createGain(); hg.gain.value = 0.07;
    src.connect(bp); bp.connect(g); g.connect(pan); hum.connect(lp); lp.connect(hg); hg.connect(pan); pan.connect(ctx.destination);
    src.start(); hum.start(); AU.pan = pan;
  } catch (e) { console.warn('no audio', e); }
}
function whoosh() {
  const ctx = AU.ctx; if (!ctx) return;
  const n = ctx.sampleRate * 0.8, buf = ctx.createBuffer(1, n, ctx.sampleRate), d = buf.getChannelData(0);
  for (let i = 0; i < n; i++) d[i] = Math.random() * 2 - 1;
  const s = ctx.createBufferSource(); s.buffer = buf;
  const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 1.2;
  f.frequency.setValueAtTime(300, ctx.currentTime); f.frequency.exponentialRampToValueAtTime(2600, ctx.currentTime + 0.3); f.frequency.exponentialRampToValueAtTime(350, ctx.currentTime + 0.8);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, ctx.currentTime); g.gain.exponentialRampToValueAtTime(0.5, ctx.currentTime + 0.25); g.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + 0.8);
  s.connect(f); f.connect(g); g.connect(ctx.destination); s.start();
}
function updateAudio() {
  const ctx = AU.ctx; if (!ctx || !AU.pan) return;
  const L = ctx.listener, t = ctx.currentTime;
  const fw = new V(0, 0, -1).applyQuaternion(headQ), up = new V(0, 1, 0).applyQuaternion(headQ);
  if (L.positionX) { L.positionX.setValueAtTime(head.x, t); L.positionY.setValueAtTime(head.y, t); L.positionZ.setValueAtTime(head.z, t);
    L.forwardX.setValueAtTime(fw.x, t); L.forwardY.setValueAtTime(fw.y, t); L.forwardZ.setValueAtTime(fw.z, t); L.upX.setValueAtTime(up.x, t); L.upY.setValueAtTime(up.y, t); L.upZ.setValueAtTime(up.z, t);
    AU.pan.positionX.setValueAtTime(portal.c.x, t); AU.pan.positionY.setValueAtTime(portal.c.y, t); AU.pan.positionZ.setValueAtTime(portal.c.z, t); }
}

/* ================================================================ the board on the left controller */

const hudCanvas = document.createElement('canvas'); hudCanvas.width = 512; hudCanvas.height = 300;
const hudTex = new THREE.CanvasTexture(hudCanvas); hudTex.colorSpace = THREE.SRGBColorSpace;
const hudMesh = new THREE.Mesh(new THREE.PlaneGeometry(0.2, 0.117), new THREE.MeshBasicMaterial({ map: hudTex, transparent: true, depthTest: false }));
hudMesh.renderOrder = 100; hudMesh.position.set(0, 0.09, -0.04); hudMesh.rotation.x = -1.0;
function hudLines() {
  const bits = S.stencilBits;
  return [
    'PORTALKNIVEN',
    `Verden: ${S.inB ? 'B  (den lilla)' : 'A  (din stue)'}`,
    `Gennemgange: ${S.crossings}`,
    S.mode === 'xr' ? `Stencil-bits: ${bits === null ? '?' : bits}${bits === 0 ? '  VINDUET VIRKER IKKE' : ''}` : 'Fladt preview',
    S.mode === 'xr' ? `Rumscanning: ${S.scanFeature ? S.scanMeshes + ' mesh' : 'ikke givet'}` : '',
    'Udløser: sæt ringen foran dig',
  ];
}
let hudNext = 0;
function drawHud() {
  const now = performance.now(); if (now < hudNext) return; hudNext = now + 250;
  const lines = hudLines();
  if (S.mode === 'flat') { $('hud').textContent = lines.filter(Boolean).join('\n') + '\n(WASD, træk = kig, P = sæt ring)'; return; }
  const x = hudCanvas.getContext('2d');
  x.clearRect(0, 0, 512, 300); x.fillStyle = 'rgba(4,10,8,.82)'; x.fillRect(0, 0, 512, 300); x.strokeStyle = '#4dff88'; x.lineWidth = 6; x.strokeRect(3, 3, 506, 294);
  x.fillStyle = '#4dff88'; x.font = 'bold 34px monospace'; x.fillText(lines[0], 22, 50);
  x.fillStyle = '#d7ffe4'; x.font = '27px monospace';
  lines.slice(1).forEach((l, i) => { x.fillStyle = l.includes('VIRKER IKKE') ? '#ff7a7a' : '#d7ffe4'; x.fillText(l, 22, 96 + i * 38); });
  hudTex.needsUpdate = true;
}

/* ================================================================ XR & flat */

let xrSession = null;
async function enterXR() {
  const status = $('status');
  try {
    initAudio();
    const session = await navigator.xr.requestSession('immersive-ar', { requiredFeatures: ['local-floor'], optionalFeatures: ['mesh-detection'] });
    session.addEventListener('end', () => location.reload());
    await renderer.xr.setSession(session);
    xrSession = session; S.mode = 'xr';
    canvas.style.display = 'block'; document.body.classList.add('playing');
    for (let i = 0; i < 2; i++) {
      const c = renderer.xr.getController(i); scene.add(c);
      c.addEventListener('selectstart', () => { if (S.mode === 'xr') placePortal(); });
      c.addEventListener('connected', e => { if (e.data && e.data.handedness === 'left') c.add(hudMesh); });
    }
    ring.visible = false;
  } catch (e) { status.textContent = 'Kunne ikke starte headset-sessionen: ' + (e.message || e); console.error(e); }
}

const keys = {}; let yaw = 0, pitch = 0;
function enterFlat() {
  initAudio();
  S.mode = 'flat'; document.body.classList.add('playing', 'flat');
  canvas.style.display = 'block';
  addEventListener('keydown', e => { keys[e.code] = true; if (e.code === 'KeyP') placePortal(); });
  addEventListener('keyup', e => { keys[e.code] = false; });
  let drag = null;
  canvas.addEventListener('pointerdown', e => { drag = { x: e.clientX, y: e.clientY }; canvas.setPointerCapture(e.pointerId); });
  canvas.addEventListener('pointerup', () => { drag = null; });
  canvas.addEventListener('pointermove', e => { if (!drag) return; yaw -= (e.clientX - drag.x) * 0.004; pitch = Math.max(-1.4, Math.min(1.4, pitch - (e.clientY - drag.y) * 0.004)); drag.x = e.clientX; drag.y = e.clientY; });
  head.set(0, 1.6, 0); headQ.identity(); camera.position.copy(head); prevHead.copy(head);
  placePortal();
}
function flatMove(dt) {
  const f = (keys.KeyW ? 1 : 0) - (keys.KeyS ? 1 : 0), s = (keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0);
  if (f || s) { const sp = 1.6 * dt; camera.position.x += (-Math.sin(yaw) * f + Math.cos(yaw) * s) * sp; camera.position.z += (-Math.cos(yaw) * f - Math.sin(yaw) * s) * sp; }
  camera.rotation.set(pitch, yaw, 0);
  head.copy(camera.position); headQ.copy(camera.quaternion);
}

addEventListener('resize', () => { if (S.mode === 'xr') return; renderer.setSize(innerWidth, innerHeight, false); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); });

const clock = new THREE.Clock();
function loop(_, frame) {
  const dt = Math.min(0.05, clock.getDelta()); S.time += dt; S.frames++;
  if (S.mode === 'flat') flatMove(dt);
  else if (S.mode === 'xr' && frame) {
    const ref = renderer.xr.getReferenceSpace(), vp = frame.getViewerPose(ref);
    if (vp) { const p = vp.transform.position, o = vp.transform.orientation; head.set(p.x, p.y, p.z); headQ.set(o.x, o.y, o.z, o.w); }
    updateScan(frame, ref);
    if (!S.placed && S.frames > 20 && vp) placePortal();
  }
  if (S.mode !== 'menu') {
    checkCrossing(); updateClip(); updateRing(dt); updateAudio(); drawHud();
    for (const q of skyMats) q.uniforms.uTime.value = S.time;
    if (decoB.userData.orbs) for (const o of decoB.userData.orbs) o.position.y = o.userData.y + Math.sin(S.time * 0.8 + o.userData.p) * 0.12;
    if (decoA.userData.orb) decoA.userData.orb.position.y = 1.7 + Math.sin(S.time) * 0.06;
    renderer.render(scene, camera);
  }
}

applyRoles();
ring.visible = false;
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
  S, portal, head, scene, renderer, camera, placePortal, applyRoles, mats, clip,
  view(x, y, z, yw, pt) { camera.position.set(x, y, z); yaw = yw; pitch = pt; prevHead.copy(head.set(x, y, z)); },
  walk(x, y, z) { camera.position.set(x, y, z); head.set(x, y, z); checkCrossing(); },
};
