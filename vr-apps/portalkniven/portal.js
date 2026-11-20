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
import { createWorlds, DIMS, ORDER, STARS_GLSL } from './worlds.js?v=51';
import { createStory } from './story.js?v=51';
import { createHand } from './hands.js?v=51';
import { buildShip } from './ship.js?v=51';
import { createAmbience } from './ambience.js?v=51';
import { createCombat } from './combat.js?v=51';

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
renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = 1.0;
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
const gl = renderer.getContext();

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.05, 80);
camera.rotation.order = 'YXZ';
camera.position.set(0, 1.6, 0);
const hemi = new THREE.HemisphereLight(0xffffff, 0x4a4e58, 0.7); scene.add(hemi);
const sun = new THREE.DirectionalLight(0xffffff, 2.1); sun.position.set(2, 5, 1); scene.add(sun);
const ringLight = new THREE.PointLight(0x40ff90, 0, 7, 1.5); scene.add(ringLight);

/* a small studio-like environment, so the steel and brass of the knife have something to reflect */
{
  const env = new THREE.Scene();
  env.add(new THREE.Mesh(new THREE.BoxGeometry(20, 20, 20), new THREE.MeshBasicMaterial({ color: 0x3a4660, side: THREE.BackSide })));
  const panel = (w, h, col, x, y, z) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: col, side: THREE.DoubleSide })); m.position.set(x, y, z); m.lookAt(0, 0, 0); env.add(m); };
  panel(14, 9, 0xffffff, 0, 9, 2); panel(5, 14, 0x60ffa0, -8, 2, 0); panel(4, 12, 0xffc890, 8, 1, -3); panel(12, 4, 0xa0a8ff, 0, 0, -9); panel(10, 10, 0x6a7a96, 0, -8, 0); panel(10, 5, 0xffffff, 0, 2, 9);
  const pm = new THREE.PMREMGenerator(renderer); scene.environment = pm.fromScene(env, 0.04).texture; pm.dispose();
}

const S = { hudT: 0, safe: false, scale: 0.85, bright: 1, brightTarget: 1, lum: -1, camera: false, cur: 0, oth: 1, side: 1, placed: false, mode: 'menu', stencilBits: null, crossings: 0, frames: 0, scanMeshes: 0, scanFeature: false, time: 0, msg: '', msgT: -99 };
const DOOR = { Rx: 0.6, Ry: 1.1 };                             // an upright oval you can walk through (1.2 m wide, 2.2 m tall)
const portal = { c: new V(0, DOOR.Ry + 0.03, -1.6), n: new V(0, 0, 1), x: new V(1, 0, 0), y: new V(0, 1, 0), Rx: DOOR.Rx, Ry: DOOR.Ry, anim: null };
const head = new V(0, 1.6, 0), prevHead = new V(0, 1.6, 0), headQ = new THREE.Quaternion();
const say = t => { S.msg = t; S.msgT = S.time; };

/* ================================================================ the dimensions (worlds.js) */

const worlds = createWorlds(scene, renderer, params);
const applyRoles = () => worlds.applyRoles(S.cur, S.oth);
worlds.setHead(head);
const late = o => (DIMS[o].fract || DIMS[o].space) && !story.fractalsOK();      // space and the fractal dimensions open once the story is over (or in free play)

/** The stick: choose where the door leads. Never the dimension you are standing in. */
function cycleOther(dir) {
  story.onStick();
  let o = VX.on && VX.phase !== 'out' ? VX.pend : S.oth;
  do { o = (o + dir + DIMS.length) % DIMS.length; } while (o === S.cur || late(o));
  if (S.placed && !portal.anim && !portal.closing) startVortex(o, dir); else setOther(o);
}
function setOther(o) {
  S.oth = o;
  if (S.placed) worlds.anchor(o, portal.c.x, portal.c.z, Math.atan2(portal.n.x, portal.n.z));
  applyRoles();
  say('D\u00f8ren f\u00f8rer til ' + DIMS[o].name);
}

/* ================================================================ the oval ring */

const ring = new THREE.Group(); scene.add(ring); ring.visible = false;
const maskMat = new THREE.MeshBasicMaterial({ colorWrite: false, depthWrite: false, side: THREE.DoubleSide, stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp });
const mask = new THREE.Mesh(new THREE.CircleGeometry(1, 80), maskMat);             // a unit disc, scaled to the oval
mask.renderOrder = ORDER.mask; mask.frustumCulled = false;
mask.onAfterRender = () => { if (S.stencilBits === null && S.mode === 'xr') S.stencilBits = gl.getParameter(gl.STENCIL_BITS); };   // asked while the headset's framebuffer is bound
ring.add(mask);
/* The last few centimetres before you are through. The window is a flat disc, and so close to it two things go wrong: the part of it nearer than the eye can
 * focus (the near plane) is cut away, and one eye is through the door before the other, with the disc behind it. Either way that eye sees the world you are
 * leaving where it should see the one you are entering. So for those centimetres the window is the whole view (maskFull); and for the first centimetres on
 * the far side, where the same would show the world you left, there is no window at all. */
const maskFull = new THREE.Mesh(new THREE.PlaneGeometry(2, 2), new THREE.ShaderMaterial({ vertexShader: 'void main() { gl_Position = vec4(position.xy, 0.0, 1.0); }', fragmentShader: 'void main() { gl_FragColor = vec4(0.0); }',
  colorWrite: false, depthWrite: false, depthTest: false, stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc, stencilZPass: THREE.ReplaceStencilOp, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp }));
maskFull.renderOrder = ORDER.mask; maskFull.frustumCulled = false; maskFull.visible = false; scene.add(maskFull);

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
  t.renderOrder = 45; ringMats.push(m); ringMeshes.push(t);
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
const arcs = new THREE.LineSegments(arcGeo, arcMat); arcs.frustumCulled = false; arcs.renderOrder = 45; ring.add(arcs);
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
const ringSparks = makeSparks(170, ring); ringSparks.points.renderOrder = 45;
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

/* ---- changing where the door leads: what you see through it is drawn down a spiral into a point, leaving black space and stars; then the new world
 * unwinds out of the point, faster. The window itself is live geometry and cannot be bent, so for that second the door shows a still picture instead: the
 * far side as it looked from where you stand, taken the moment you turn the stick, laid on the door and wound up by this shader. ---- */
const VX = { on: false, phase: '', t: 0, pend: -1, need: false, dir: 1 };
const snapRT = new THREE.WebGLRenderTarget(640, 960, { depthBuffer: true });
snapRT.isXRRenderTarget = true; snapRT.texture.colorSpace = THREE.SRGBColorSpace;      // drawn exactly as the headset's own picture is (same tone curve, same shaders: nothing to compile)
const snapCam = new THREE.PerspectiveCamera();
const vortexMat = new THREE.ShaderMaterial({
  uniforms: { uTex: { value: snapRT.texture }, uU: { value: 0 }, uHas: { value: 0 }, uDir: { value: 1 }, uA0: { value: new THREE.Vector4() }, uA1: { value: new THREE.Vector4() }, uA2: { value: new THREE.Vector4() } },
  transparent: true, blending: THREE.NoBlending, depthWrite: false, side: THREE.DoubleSide,
  stencilWrite: true, stencilRef: 1, stencilFunc: THREE.EqualStencilFunc, stencilFail: THREE.KeepStencilOp, stencilZFail: THREE.KeepStencilOp, stencilZPass: THREE.KeepStencilOp,
  vertexShader: 'varying vec2 vP; varying vec3 vW; void main() { vP = position.xy; vec4 wp = modelMatrix * vec4(position, 1.0); vW = wp.xyz; gl_Position = projectionMatrix * viewMatrix * wp; }',
  fragmentShader: `precision highp float; varying vec2 vP; varying vec3 vW; uniform sampler2D uTex; uniform float uU, uHas, uDir; uniform vec4 uA0, uA1, uA2;
    ${STARS_GLSL}
    void main() {
      vec4 col = vec4(starField(normalize(vW - cameraPosition), 0.3), 1.0);                    // stars at their own distance: each eye looks out through the door
      float s = pow(1.0 - uU, 1.7), r = length(vP), th = atan(vP.y, vP.x);
      float rim = 1.0 + 0.22 * uU * cos(2.0 * th + uDir * uU * 7.0);                            // the shrinking picture's edge is drawn out into two arms
      float rr = r / max(s * rim, 1e-4);
      if (rr < 1.0 && uHas > 0.5) {
        float ang = uDir * uU * uU * 8.0 * (1.0 + 1.6 * (1.0 - rr)), c = cos(ang), sn = sin(ang);      // wound up, the middle ahead of the rim
        vec2 q = mat2(c, -sn, sn, c) * vP / max(s * rim, 1e-4);
        vec4 cl = uA0 + uA1 * q.x + uA2 * q.y, t = texture2D(uTex, clamp(cl.xy / cl.w * 0.5 + 0.5, 0.0, 1.0));
        col = mix(col, vec4(t.rgb * (1.0 - 0.3 * uU), t.a), 1.0 - smoothstep(0.84, 1.0, rr));
      }
      float e = (rr - 0.97) * 9.0;
      col.rgb += vec3(0.35, 1.0, 0.55) * exp(-e * e) * uU * (1.0 - uU) * 2.4 * uHas;            // green fire round the edge of the falling picture
      col.rgb += vec3(0.75, 1.0, 0.85) * exp(-r * r / (0.0012 + s * s * 0.02)) * smoothstep(0.55, 1.0, uU) * 1.4;      // the point it falls into
      gl_FragColor = col;
    }`,
});
const vortex = new THREE.Mesh(new THREE.CircleGeometry(1, 64), vortexMat); vortex.renderOrder = 40; vortex.frustumCulled = false; vortex.visible = false; ring.add(vortex);
const _sv = new V(), _sm = new THREE.Matrix4();
function takeSnap(i) {
  snapCam.position.copy(head); snapCam.up.set(0, 1, 0); snapCam.lookAt(portal.c); snapCam.updateMatrixWorld(true);
  let tx = 0.05, ty = 0.05;                                         // a view just wide enough to hold the whole oval
  for (let k = 0; k < 16; k++) { const [ex, ey] = ellipsePt(k / 16 * TAU); _sv.copy(portal.c).addScaledVector(portal.x, ex).addScaledVector(portal.y, ey).applyMatrix4(snapCam.matrixWorldInverse);
    if (_sv.z > -0.05) { tx = ty = 4; break; } tx = Math.max(tx, Math.abs(_sv.x / _sv.z)); ty = Math.max(ty, Math.abs(_sv.y / _sv.z)); }
  tx = Math.min(tx * 1.03, 4); ty = Math.min(ty * 1.03, 4);
  const n = 0.05; snapCam.projectionMatrix.makePerspective(-tx * n, tx * n, ty * n, -ty * n, n, 80); snapCam.projectionMatrixInverse.copy(snapCam.projectionMatrix).invert();
  worlds.snapshot(i, snapCam, snapRT, portal.n.clone().multiplyScalar(-S.side), portal.c);
  _sm.multiplyMatrices(snapCam.projectionMatrix, snapCam.matrixWorldInverse); const U = vortexMat.uniforms, k = 0.985;
  U.uA0.value.set(portal.c.x, portal.c.y, portal.c.z, 1).applyMatrix4(_sm);
  U.uA1.value.set(portal.x.x * portal.Rx * k, portal.x.y * portal.Rx * k, portal.x.z * portal.Rx * k, 0).applyMatrix4(_sm);
  U.uA2.value.set(portal.y.x * portal.Ry * k, portal.y.y * portal.Ry * k, portal.y.z * portal.Ry * k, 0).applyMatrix4(_sm);
  U.uHas.value = 1; applyRoles(); updateClip();
}
function startVortex(o, dir) {
  VX.pend = o; VX.dir = dir;
  if (VX.on && VX.phase !== 'out') return;                         // already on its way in: only the destination changes
  VX.on = true; VX.phase = 'in'; VX.t = 0; VX.need = true; vortexMat.uniforms.uDir.value = dir; whirl(0.6, false); haptic(0.3, 200);
}
function cancelVortex() { VX.on = false; vortex.visible = false; }
function updateVortex(dt) {
  if (!VX.on) return;
  if (!S.placed || portal.anim || portal.closing) { cancelVortex(); return; }
  const U = vortexMat.uniforms;
  vortex.scale.set(portal.Rx * 0.985, portal.Ry * 0.985, 1);
  if (VX.need) { VX.need = false; takeSnap(S.oth); vortex.visible = true; }
  if (!VX.pause) VX.t += dt;                                      // (pause: a test hook)
  if (VX.phase === 'in') {
    const u = clamp(VX.t / 0.6, 0, 1); U.uU.value = u * u * (3 - 2 * u);
    if (u >= 1) { VX.phase = 'hold'; VX.t = 0; U.uHas.value = 0; setOther(VX.pend); }      // nothing but stars: the door changes behind them
  } else if (VX.phase === 'hold') {
    U.uU.value = 1;
    if (VX.t > 0.14) { VX.phase = 'out'; VX.t = 0; takeSnap(S.oth); flash.userData.t = 0.4; shockT = 0.25; knifeGlow = 1.6; zapSmall(); whirl(0.34, true); haptic(0.6, 90); }
  } else {
    const u = clamp(VX.t / 0.34, 0, 1); U.uU.value = 1 - easeOutCubic(u);
    if (u >= 1) cancelVortex();
  }
}

function updateRing(dt) {
  for (const m of ringMats) m.uniforms.uTime.value = S.time;
  for (let i = 0; i < 4; i++) regenArc((Math.random() * ARCS) | 0);
  arcGeo.attributes.position.needsUpdate = true;
  arcMat.opacity = 0.55 + 0.45 * Math.random();
  ringSparks.step(dt, 0.5, respawnRingSpark);
}

/* ================================================================ the knife */

function buildKnife() {
  const root = new THREE.Group(), k = new THREE.Group(); root.add(k); k.position.z = -0.07;       // origin = the middle of the grip (where the hand is); the blade points to -Z
  const tilt = (parseFloat(params.get('tilt')) || 0) * Math.PI / 180; k.rotation.x = tilt;         // ?tilt=deg to tune how the blade sits in the fist
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
  const tip = new THREE.Object3D(); tip.position.set(0, 0, -0.278); k.add(tip);     // the point of the blade
  k.children.forEach(o => { o.userData.piece = o === gem ? 'gem' : o.position.z < 0.003 ? 'blade' : 'grip'; });      // the knife in its three pieces (for the story)
  root.updateMatrixWorld(true);
  root.userData = { tip, rune, halo: haloMat, tipLocal: tip.getWorldPosition(new V()) };       // the point, in the root's own space (the root is still at the origin here)
  return root;
}
const knife = buildKnife(); knife.traverse(o => { if (o.isMesh || o.isLineSegments) o.renderOrder = 46; });      // drawn after the door, so that in the doorway it is not painted over
let knifeGlow = 0.35, tipPrev = null;

/* the two hands, as the story sees them: where they are, where they point, and whether the trigger was just pressed */
let rightThing = null;                                             // something the story shows by the right hand (the thumbstick hint)
const hands = [0, 1].map(() => ({ on: false, q: new THREE.Quaternion(), pos: new V(), rayO: new V(), rayD: new V(0, 0, -1), trig: false, trigEdge: false }));
const tipW = new V();
const story = createStory({
  scene, S, head, headQ, worlds, hands, say: t => say(t), audio: () => AU.ctx, haptic: (v, ms) => { for (const c of ctrl) { try { c.src?.gamepad?.hapticActuators?.[0]?.pulse?.(v, ms); } catch (_) { /* none */ } } },
  makeKnife: buildKnife, tip: () => tipW, ring, portal, cutting: () => cut.state === 'cutting',
  takeKnife: () => { assignTools(); },
  hapticRight: (v, ms) => { let i = ctrl.findIndex(c => c.hand === 'right'); if (i < 0) i = knifeIdx; try { ctrl[i].src?.gamepad?.hapticActuators?.[0]?.pulse?.(v, ms); } catch (_) { /* none */ } },
  attachRight: o => { rightThing = o; }, closeDoor: () => { if (S.placed && !portal.closing) dissolvePortal(); },
  giveKnife: i => { knifeIdx = i; ctrl.forEach((c, k) => { c.dragged = k === i; }); assignTools(); knifeGlow = 2.2; zap(); },
});

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
  } else if (cut.state === 'cutting' && pressed && tipPrev && crossesRing(tipPrev, tip)) {
    cut.state = 'idle'; cut.wait = true; fadeTrail(); dissolvePortal();              // a cut across the ring's edge severs the door
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

/** Did the point of the blade pass over the ring itself (in the door's plane, from inside the oval to outside or the other way)? */
const _ra = new V(), _rb = new V();
function ringCoord(p, out) { _r.copy(p).sub(portal.c); return out.set(_r.dot(portal.x) / portal.Rx, _r.dot(portal.y) / portal.Ry, _r.dot(portal.n)); }
function crossesRing(a, b) {
  if (!S.placed || portal.anim || portal.closing) return false;
  ringCoord(a, _ra); ringCoord(b, _rb);
  if (Math.abs(_ra.z) > 0.14 || Math.abs(_rb.z) > 0.14) return false;
  const ea = _ra.x * _ra.x + _ra.y * _ra.y, eb = _rb.x * _rb.x + _rb.y * _rb.y;
  return (ea < 1) !== (eb < 1) && Math.min(ea, eb) > 0.45;                             // a stroke over the rim, not a jump across the whole door
}
/** The door collapses and is gone. Whoever is on the far side stays there until a new door is cut. */
function dissolvePortal() {
  portal.closing = { t: 0, dur: 0.75 };
  for (let k = 0; k < 90; k++) {
    const a = rnd(0, TAU), [ex, ey] = ellipsePt(a), [ox, oy] = ellipseOut(a), sp = rnd(0.4, 1.6);
    _r.copy(portal.c).addScaledVector(portal.x, ex).addScaledVector(portal.y, ey);
    const v = new V().addScaledVector(portal.x, ox * sp).addScaledVector(portal.y, oy * sp + 0.3).addScaledVector(portal.n, rnd(-0.5, 0.5));
    tipSparks.emit(_r.x, _r.y, _r.z, v.x, v.y, v.z, rnd(0.4, 1.1));
  }
  flash.userData.t = 0.25; knifeGlow = 2.0; zapSmall(); whoosh(); haptic(1, 180);
  if (AU.ringGain) { const t = AU.ctx.currentTime; AU.ringGain.gain.cancelScheduledValues(t); AU.ringGain.gain.setTargetAtTime(0, t, 0.15); AU.humGain.gain.cancelScheduledValues(t); AU.humGain.gain.setTargetAtTime(0, t, 0.15); }
  say(S.cur === 0 ? 'D\u00f8ren er lukket' : 'D\u00f8ren er lukket \u2014 sk\u00e6r en ny for at komme hjem');
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
  portal.closing = null;
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
  worlds.anchor(S.oth, to.x, to.z, yaw); worlds.anchor(0, to.x, to.z, yaw, true);
  S.placed = true; prevHead.copy(head);
  shock.position.copy(to); shockT = 0;
  flash.position.copy(from); flash.userData.t = 0; flash.userData.from = from.clone(); flash.userData.to = to.clone();
  knifeGlow = 2.2; fadeTrail(); zap(); haptic(1, 240);
  say('');
}
function updateAnim(dt) {
  const cl = portal.closing;
  if (cl) {
    cl.t += dt; const u = clamp(cl.t / cl.dur, 0, 1), sc = Math.max(0.001, 1 - u * u * u);
    ring.scale.set(sc * (1 + 0.08 * Math.sin(u * 40)), sc, 1);
    if (u >= 1) { portal.closing = null; ring.visible = false; ring.scale.set(1, 1, 1); S.placed = false; story.onClosed(); }
  }
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
  if (!S.placed || portal.anim || portal.closing) { prevHead.copy(head); return; }
  const d0 = _r.copy(prevHead).sub(portal.c).dot(portal.n), d1 = new V().copy(head).sub(portal.c).dot(portal.n);
  if ((d0 < 0) !== (d1 < 0)) {
    const t = d0 / (d0 - d1), p = prevHead.clone().lerp(head, t).sub(portal.c);
    const lx = p.dot(portal.x) / portal.Rx, ly = p.dot(portal.y) / portal.Ry;
    if (lx * lx + ly * ly < 0.95) { cancelVortex(); const c = S.cur; S.cur = S.oth; S.oth = c; S.crossings++; portal.fresh = true; if (DIMS[S.cur].fract) FC.hue = FC.hueTo = 0; /* a fractal always opens in its own colours */ applyRoles(); whoosh(); haptic(0.6, 120); say('Du er i ' + DIMS[S.cur].name); story.onEnter(S.cur); }
  }
  prevHead.copy(head);
}
function updateClip() {
  const d = new V().copy(head).sub(portal.c).dot(portal.n);
  { const p = _r.copy(head).sub(portal.c), lx = p.dot(portal.x) / portal.Rx, ly = p.dot(portal.y) / portal.Ry, live = S.placed && !portal.anim && !portal.closing;
    const close = live && Math.abs(d) < 0.11 && lx * lx + ly * ly < 0.85;            // in the doorway: within 11 cm of the door's plane (near plane 5 cm, half the distance between the eyes, a frame's travel)
    if (Math.abs(d) > 0.13 || !live) portal.fresh = false;                           // "just came through" lasts until you are clear of the door
    maskFull.visible = close && !portal.fresh; mask.visible = !(close && portal.fresh); }
  S.side = d >= 0 ? 1 : -1;
  worlds.setClip(S.oth, portal.n.clone().multiplyScalar(-S.side), portal.c);   // keep only what is behind the door, seen from where you stand
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
function zapSmall() {
  const ctx = AU.ctx; if (!ctx) return; const t = ctx.currentTime;
  const s = ctx.createBufferSource(); s.buffer = noiseBuffer(ctx, 0.35);
  const hp = ctx.createBiquadFilter(); hp.type = 'bandpass'; hp.Q.value = 2; hp.frequency.setValueAtTime(700, t); hp.frequency.exponentialRampToValueAtTime(4200, t + 0.3);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.35, t + 0.03); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.35);
  s.connect(hp); hp.connect(g); g.connect(ctx.destination); s.start(t);
}
/** The pull of the spiral: a rush of air and a note, falling as the picture goes down the drain, rising as the new one comes up. */
function whirl(dur, up) {
  const ctx = AU.ctx; if (!ctx) return; const t = ctx.currentTime;
  const s = ctx.createBufferSource(); s.buffer = noiseBuffer(ctx, dur + 0.1);
  const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = 2.2; f.frequency.setValueAtTime(up ? 260 : 3200, t); f.frequency.exponentialRampToValueAtTime(up ? 3400 : 180, t + dur);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.4, t + dur * (up ? 0.2 : 0.7)); g.gain.exponentialRampToValueAtTime(0.0001, t + dur + 0.08);
  s.connect(f); f.connect(g); g.connect(ctx.destination); s.start(t);
  const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.setValueAtTime(up ? 60 : 520, t); o.frequency.exponentialRampToValueAtTime(up ? 620 : 46, t + dur);
  const og = ctx.createGain(); og.gain.setValueAtTime(0.0001, t); og.gain.exponentialRampToValueAtTime(0.22, t + dur * 0.5); og.gain.exponentialRampToValueAtTime(0.0001, t + dur + 0.05);
  o.connect(og); og.connect(ctx.destination); o.start(t); o.stop(t + dur + 0.1);
}
/** Something gave way under a hand: a stalk or straw swishes, a cable's hook rings, the valve wheel clicks. */
function touchSound(kind, v) {
  const ctx = AU.ctx; if (!ctx) return; const t = ctx.currentTime;
  if (kind === 'cable') {
    for (const [fq, a, d] of [[1310, 1, 0.9], [2740, 0.5, 0.5], [4180, 0.25, 0.3]]) { const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.value = fq * rnd(0.985, 1.015); const g = ctx.createGain();
      g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime((0.02 + 0.07 * v) * a, t + 0.004); g.gain.exponentialRampToValueAtTime(0.0001, t + d); o.connect(g); g.connect(ctx.destination); o.start(t); o.stop(t + d + 0.05); }
    return;
  }
  const dur = kind === 'valve' ? 0.035 : 0.2, s = ctx.createBufferSource(); s.buffer = noiseBuffer(ctx, dur + 0.02);
  const f = ctx.createBiquadFilter(); f.type = 'bandpass'; f.Q.value = kind === 'valve' ? 5 : 0.9; f.frequency.value = (kind === 'valve' ? 2300 : kind === 'straw' ? 4600 : 2100) * rnd(0.85, 1.15);
  const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(kind === 'valve' ? 0.1 + 0.15 * v : 0.035 + 0.16 * v, t + (kind === 'valve' ? 0.003 : 0.04)); g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  s.connect(f); f.connect(g); g.connect(ctx.destination); s.start(t);
}
worlds.onTouch = (kind, v, h) => { touchSound(kind, v); try { ctrl[h].src?.gamepad?.hapticActuators?.[0]?.pulse?.(clamp(0.15 + 0.6 * v, 0, 1), kind === 'cable' ? 45 : 25); } catch (_) { /* none */ } };
/* Deep space is not quite silent: a low, slow drone, so the emptiness has a size. */
function updateSpaceAmb() {
  const ctx = AU.ctx; if (!ctx) return;
  if (!AU.space) {
    const g = ctx.createGain(); g.gain.value = 0; g.connect(ctx.destination);
    for (const [fq, a] of [[54, 0.5], [54.4, 0.5], [81.3, 0.28], [162.9, 0.08]]) { const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.value = fq; const og = ctx.createGain(); og.gain.value = a; o.connect(og); og.connect(g); o.start(); (AU.spaceOscs = AU.spaceOscs || []).push(o); }
    const n = ctx.createBufferSource(); n.buffer = noiseBuffer(ctx, 3); n.loop = true; const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 260; const ng = ctx.createGain(); ng.gain.value = 0.35; n.connect(lp); lp.connect(ng); ng.connect(g); n.start();
    AU.space = g;
  }
  const tgt = DIMS[S.cur].space ? 0.22 : S.cur === 0 && DIMS[S.oth].space && S.placed && !portal.closing ? 0.05 * clamp(1.6 / Math.max(0.6, head.distanceTo(portal.c)), 0, 1) : 0;
  AU.space.gain.setTargetAtTime(tgt, ctx.currentTime, 0.6);
}
/* Each dimension has its own track. It fades in when you step in, and a little of it leaks through the door when the door leads there. */
const TRACKS = { 1: 'music/doedt-trae.mp3', 2: 'music/rotation.mp3', 3: 'music/tidejord.mp3' };
const music = {};
const others = w => Object.keys(music).some(k => +k !== w && music[k].vol > 0.08);
function updateMusic(dt) {
  for (const k in TRACKS) {
    const w = +k; let target = 0;
    if (S.cur === w) target = 0.85;
    else if (S.cur === 0 && S.oth === w && S.placed && !portal.anim && !portal.closing) target = 0.3 * clamp(1.6 / Math.max(0.6, head.distanceTo(portal.c)), 0, 1);
    let m = music[w];
    if (!m) { if (!(target > 0 && AU.ctx)) continue; m = music[w] = { el: new Audio(TRACKS[w]), vol: 0 }; m.el.loop = true; m.el.preload = 'auto'; m.el.volume = 0; }
    if (target > m.vol) m.vol = Math.min(target, m.vol + dt * 0.45 * (others(w) ? 0.35 : 1)); else m.vol = Math.max(target, m.vol - dt * 0.9);      // out in about a second; in only once the other track has gone
    m.el.volume = clamp(m.vol, 0, 1);
    if (m.vol > 0.001 && m.el.paused) m.el.play().catch(() => {});
    if (m.vol <= 0.001 && target === 0 && !m.el.paused) m.el.pause();
  }
}
/* A warning that swells as your head nears something real (from the room scan) while you cannot see the room. */
const prox = { d: Infinity, n: 0 };
function updateProximity() {
  if ((prox.n++ & 1) === 0) prox.d = S.cur !== 0 ? worlds.nearest(head, 0.42) : Infinity;
  const ctx = AU.ctx; if (!ctx) return;
  if (!AU.warn) {
    const o = ctx.createOscillator(); o.type = 'triangle'; o.frequency.value = 440; const g = ctx.createGain(); g.gain.value = 0;
    const tr = ctx.createOscillator(); tr.type = 'square'; tr.frequency.value = 4; const tg = ctx.createGain(); tg.gain.value = 0; tr.connect(tg); tg.connect(g.gain);
    o.connect(g); g.connect(ctx.destination); o.start(); tr.start(); AU.warn = { o, g, tr, tg };
  }
  const k = prox.d === Infinity ? 0 : clamp((0.38 - prox.d) / 0.26, 0, 1), t = ctx.currentTime, W = AU.warn;       // 0 at 38 cm, full at 12 cm
  W.g.gain.setTargetAtTime(0.28 * k * k, t, 0.05); W.tg.gain.setTargetAtTime(0.22 * k * k, t, 0.05);
  W.o.frequency.setTargetAtTime(440 + 500 * k, t, 0.05); W.tr.frequency.setTargetAtTime(3 + 11 * k, t, 0.05);
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
  const bits = S.stencilBits, msg = S.time - S.msgT < 4 && (!story.st.on || story.st.help || /forsigtigt|gennemf/.test(S.msg)) ? S.msg : '';
  return [
    'PORTALKNIVEN',
    `Du er i: ${DIMS[S.cur].name}`,
    `D\u00f8ren f\u00f8rer til: ${DIMS[S.oth].name}`,
    story.st.on ? `Dele: ${story.count()} af 9` : S.mode === 'xr' ? `Stencil: ${bits === null ? '?' : bits}${bits === 0 ? '  VINDUET VIRKER IKKE' : ''}   Scan: ${S.scanFeature ? S.scanMeshes + ' mesh' : 'ingen'}` : 'Fladt preview',
    S.mode === 'xr' && S.cur !== 0 && !S.scanMeshes ? 'INGEN RUMSCANNING: pas på møbler!' : `Rumlys: ${S.lum < 0 ? 'ingen' : S.lum.toFixed(2)}   Kamera: ${S.camera ? 'ja' : 'nej'}`,
    cut.state === 'cutting' ? 'Skærer … luk ringen!' : story.st.on ? (story.st.help ? story.objective() : '') : S.placed ? (DIMS[S.cur].fract || DIMS[S.cur].space ? 'Tryk højre stick ned: skift verden bag døren' : 'Højre stick: skift verden bag døren') : S.cur !== 0 ? 'Ingen dør: skær en ny for at komme hjem' : 'Hold udløseren og skær en ring i luften',
    msg,
  ];
}
let hudNext = 0, hudSig = '';
function drawHud() {
  const now = performance.now(); if (now < hudNext) return; hudNext = now + 200;
  const lines = hudLines(), sig = lines.filter(Boolean).join('|');
  if (sig !== hudSig) { if (!hudSig.startsWith(sig)) S.hudT = S.time; hudSig = sig; }      // something new to say (a line merely expiring does not count)
  hudMesh.material.opacity = clamp(1 - (S.time - S.hudT - 5) / 1.2, 0, 1); hudMesh.visible = hudMesh.material.opacity > 0.01;
  if (S.mode === 'flat') { $('hud').textContent = lines.filter(Boolean).join('\n') + '\n(WASD, træk = kig, klik = peg og tryk, Shift+træk = skær, P = sæt ring, Q/E = skift verden)'; return; }
  const x = hudCanvas.getContext('2d');
  x.clearRect(0, 0, 512, 300); x.fillStyle = 'rgba(4,10,8,.82)'; x.fillRect(0, 0, 512, 300); x.strokeStyle = '#4dff88'; x.lineWidth = 6; x.strokeRect(3, 3, 506, 294);
  x.fillStyle = '#4dff88'; x.font = 'bold 34px monospace'; x.fillText(lines[0], 22, 50);
  x.font = '21px monospace';
  lines.slice(1).forEach((l, i) => { x.fillStyle = (l.includes('VIRKER IKKE') || l.includes('pas på')) ? '#ff7a7a' : i === 1 ? '#7dffb0' : i === 4 ? '#ffd56a' : '#d7ffe4'; x.fillText(l, 22, 86 + i * 34); });
  hudTex.needsUpdate = true;
}

/* ================================================================ XR & flat */

const ctrl = [{ pressed: false, hand: '', src: null }, { pressed: false, hand: '', src: null }];
let knifeIdx = 1;
function assignTools() {
  const right = ctrl.findIndex(c => c.hand === 'right');
  if (right >= 0 && !ctrl.some(c => c.dragged)) knifeIdx = right;
  if (!story.canCut()) { const l = ctrl.findIndex(c => c.hand === 'left'); knifeIdx = l >= 0 ? 1 - l : 1; knife.parent?.remove(knife); }
  else { const grip = renderer.xr.getControllerGrip(knifeIdx); if (knife.parent !== grip) { knife.parent?.remove(knife); grip.add(knife); } }
  if (rightThing) { let r = ctrl.findIndex(c => c.hand === 'right'); if (r < 0) r = knifeIdx; const rc = renderer.xr.getController(r); if (rightThing.parent !== rc) { rc.add(rightThing); rightThing.position.set(0, 0.05, 0.03); rightThing.rotation.set(0.9, 0, 0); } }
  const other = renderer.xr.getController(1 - knifeIdx);
  if (hudMesh.parent !== other) { hudMesh.parent?.remove(hudMesh); other.add(hudMesh); }
  hapticPulse = (v, ms) => { try { const a = ctrl[knifeIdx].src?.gamepad?.hapticActuators?.[0]; a?.pulse?.(clamp(v, 0, 1), ms); } catch (_) { /* no haptics */ } };
}

const FOUR = DIMS.findIndex(d => d.four);
async function enterXR(free, help = true, first = 0) {
  if (first) { S.oth = first; applyRoles(); }
  const status = $('status');
  try {
    initAudio();
    const session = await navigator.xr.requestSession('immersive-ar', { requiredFeatures: ['local-floor'], optionalFeatures: ['mesh-detection', 'light-estimation', 'camera-access'] });
    session.addEventListener('end', () => location.reload());
    S.camera = !!(session.enabledFeatures && session.enabledFeatures.includes('camera-access'));
    try { if (session.requestLightProbe) session.requestLightProbe().then(p => { lightProbe = p; }).catch(() => {}); } catch (_) { /* no light estimation */ }
    renderer.xr.setFramebufferScaleFactor(S.scale); renderer.xr.setFoveation(1);      // fewer pixels to shade, and fewer still at the edge of view
    await renderer.xr.setSession(session);
    S.mode = 'xr';
    canvas.style.display = 'block'; document.body.classList.add('playing');
    for (let i = 0; i < 2; i++) {
      const c = renderer.xr.getController(i); scene.add(c); scene.add(renderer.xr.getControllerGrip(i));
      c.addEventListener('selectstart', () => { ctrl[i].pressed = true; initAudio(); });
      c.addEventListener('selectend', () => { ctrl[i].pressed = false; });
      c.addEventListener('squeezestart', () => { if (!story.canCut() || DIMS[S.cur].fract || FC.shipOn) return; knifeIdx = i; ctrl[1 - i].dragged = false; ctrl[i].dragged = true; assignTools(); });      // grip: take the knife in this hand
      c.addEventListener('connected', e => { ctrl[i].src = e.data; ctrl[i].hand = e.data?.handedness || ''; assignTools(); });
    }
    if (free !== true) story.start(help);
    assignTools();
    if (free === true) say(first === FOUR ? 'Skær en ring: den fører til det firedimensionale rum' : 'Hold udløseren og skær en ring i luften');
  } catch (e) { status.textContent = 'Kunne ikke starte headset-sessionen: ' + (e.message || e); console.error(e); }
}

const keys = {}; let yaw = 0, pitch = 0, flatClick = false, flatHold = false;
const flatHand = { pos: null };                                   // a test hook: a hand put somewhere by hand
const flatTip = { pos: new V(0, 1.4, -1.2), pressed: false, override: false };
const ray = new THREE.Raycaster();
function enterFlat() {
  initAudio();
  if (params.has('four')) { S.oth = FOUR; applyRoles(); }
  if (!params.has('free') && !params.has('four')) story.start(!params.has('nohelp'));
  if (rightThing) { camera.add(rightThing); rightThing.position.set(0.16, -0.12, -0.4); rightThing.rotation.set(0.9, 0, 0); }
  S.mode = 'flat'; document.body.classList.add('playing', 'flat');
  canvas.style.display = 'block';
  scene.add(camera); scene.add(knife);
  canvas.addEventListener('click', e => { if (!e.shiftKey) flatClick = true; });
  addEventListener('keydown', e => { if (e.code === 'KeyF') flatClick = true; });
  addEventListener('keydown', e => { keys[e.code] = true; if (e.code === 'KeyP') placeDirect(); if (story.stickOK()) { if (e.code === 'KeyE') cycleOther(1); if (e.code === 'KeyQ') cycleOther(-1); } });
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
  if (params.has('free')) say('Shift + træk: skær en ring (eller P)');
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
  const tipLocal = knife.userData.tipLocal;
  knife.quaternion.copy(camera.quaternion);
  knife.visible = story.canCut();
  if (flatTip.pressed || flatTip.override) knife.position.copy(flatTip.pos).sub(_v.copy(tipLocal).applyQuaternion(camera.quaternion));
  else knife.position.copy(camera.position).add(_v.set(0.2, -0.2, -0.42).applyQuaternion(camera.quaternion));
}

addEventListener('resize', () => { if (S.mode === 'xr') return; renderer.setSize(innerWidth, innerHeight, false); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); });

let lightProbe = null;
let stickHeld = false;
function pollStick() {                                              // the right hand's thumbstick (or the knife hand's, if the headset does not say which is right)
  let i = ctrl.findIndex(c => c.hand === 'right'); if (i < 0) i = knifeIdx;
  const gp = ctrl[i].src && ctrl[i].src.gamepad; if (!gp) return;
  const ax = gp.axes.length >= 4 ? gp.axes[2] : gp.axes[0] || 0, press = !!(gp.buttons[3] && gp.buttons[3].pressed);
  const on = Math.abs(ax) > 0.7 || press;
  if (DIMS[S.cur].fract || DIMS[S.cur].space) { if (press && !stickHeld && story.stickOK()) cycleOther(1); stickHeld = press; return; }      // in a fractal (or in space) the stick moves you; a press still changes what lies behind the door
  if (on && !stickHeld && story.stickOK()) cycleOther(ax < -0.7 ? -1 : 1);
  stickHeld = on || Math.abs(ax) > 0.35;
}
/* ---- inside a fractal dimension: fly, turn, rise, reshape and resize; and the room's outline fades while you stand still ---- */
const FC = { still: 0, ghost: 1, last: new V(), gripD: 0, tipsT: -1, tipsDone: false, gear: 4, gearT: 0 };
const gearCanvas = document.createElement('canvas'); gearCanvas.width = 512; gearCanvas.height = 128; let gearShown = 0;
const gearTex = new THREE.CanvasTexture(gearCanvas); gearTex.colorSpace = THREE.SRGBColorSpace;
const gearMesh = new THREE.Mesh(new THREE.PlaneGeometry(0.32, 0.08), new THREE.MeshBasicMaterial({ map: gearTex, transparent: true, depthTest: false, toneMapped: false })); gearMesh.renderOrder = 102; gearMesh.visible = false;
const dead = v => (Math.abs(v) < 0.15 ? 0 : (v - Math.sign(v) * 0.15) / 0.85);
const btn = (gp, k) => !!(gp && gp.buttons[k] && gp.buttons[k].pressed);
/* the first time in a fractal: the controllers buzz and blink until you look down at them. Nothing is spelled out; you find out by trying. */
function blinker() {
  const cv = document.createElement('canvas'); cv.width = cv.height = 128; const x = cv.getContext('2d');
  const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,1)'); g.addColorStop(0.3, 'rgba(140,220,255,.55)'); g.addColorStop(1, 'rgba(60,160,255,0)'); x.fillStyle = g; x.fillRect(0, 0, 128, 128);
  const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(cv), blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, depthTest: false, opacity: 0 })); sp.scale.setScalar(0.16); sp.position.set(0, 0.02, 0.03); sp.renderOrder = 101; sp.visible = false; return sp;
}
const tips = { left: blinker(), right: blinker() };
function fractalControls(dt) {
  const inF = !!DIMS[S.cur].fract;
  /* Standing in one place for a couple of seconds lets the room's outline fade; a step brings it back at once. What counts is the head leaving the spot
   * (80 cm), not its speed: turning the head swings it a few centimetres and must not count. */
  if (Math.hypot(head.x - FC.last.x, head.z - FC.last.z) > 0.8) { FC.last.copy(head); FC.still = 0; } else { FC.still += dt; FC.last.lerp(head, Math.min(1, dt * 0.4)); }      // the spot follows you slowly, so swaying and shuffling never add up. 80 cm: a real step or two. Looking down, leaning and turning on the spot (which shuffles the head 20 to 40 cm) do not count
  FC.ghost += ((FC.still > 2 ? 0 : 1) - FC.ghost) * Math.min(1, dt * (FC.still > 2 ? 1.2 : 8)); worlds.setGhost(FC.ghost);
  let li = ctrl.findIndex(c => c.hand === 'left'), ri = ctrl.findIndex(c => c.hand === 'right'); if (li < 0) li = 0; if (ri < 0) ri = 1;
  const L = ctrl[li].src && ctrl[li].src.gamepad, R = ctrl[ri].src && ctrl[ri].src.gamepad;
  if (S.ship && (DIMS[S.cur].space || (inF && !DIMS[S.cur].four))) { shipControls(dt, L, R, li, ri); return; }
  if (FC.shipOn) { FC.shipOn = false; ship.visible = false; FC.gear = Math.max(1, FC.gear); }
  if (DIMS[S.cur].space) {
    /* Deep space, flown like the fractals: the right stick flies where you look and slides sideways, the left stick up / down is the speed. B / A is the warp:
     * the faster, the more the whole sky is swept forward into your heading (set when the warp starts) and shifts colour. Y / X feeds a black hole in the sky,
     * which bends the light round it. */
    let fwd = 0, side = 0, gear = 0, hole = 0, warp = 0;
    if (S.mode === 'xr') {
      if (R && R.axes.length >= 4) { fwd = -dead(R.axes[3]); side = dead(R.axes[2]); }
      if (L && L.axes.length >= 4) gear = -dead(L.axes[3]);
      hole = (btn(L, 5) ? 1 : 0) - (btn(L, 4) ? 1 : 0); warp = (btn(R, 5) ? 1 : 0) - (btn(R, 4) ? 1 : 0);
    } else {
      fwd = (keys.KeyI ? 1 : 0) - (keys.KeyK ? 1 : 0); side = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); gear = (keys.KeyO ? 1 : 0) - (keys.KeyU ? 1 : 0);
      hole = (keys.KeyM ? 1 : 0) - (keys.KeyN ? 1 : 0); warp = (keys.KeyB ? 1 : 0) - (keys.KeyV ? 1 : 0);
    }
    if (gear) { FC.gear = clamp(FC.gear + gear * dt * 3.2, 1, 9); FC.gearT = 1.6; }
    const [f, r] = flyAim(ri);
    if (fwd || side) {
      worlds.spaceMove(new V().addScaledVector(f, fwd).addScaledVector(r, side), 0.45 * Math.pow(1.65, FC.gear - 1) * dt); }      // three times the fractals' speeds: 0.5 to 25 m/s
    if (warp) { const b = worlds.spaceWarpNow(); if (b < 0.003 && warp > 0) SPN.axis.copy(f); worlds.spaceWarp(SPN.axis, clamp(b + warp * dt * 0.3, 0, 0.93)); }
    if (hole) worlds.spaceHole(clamp(worlds.spaceHoleNow() + hole * dt * 0.09, 0, 0.42));
    worlds.setFractalCalm(false); gearPanel(dt); controllerTips(dt, li, ri); return;
  }
  SPN.w.set(0, 0, 0);
  if (DIMS[S.cur].four) {
    /* The four-dimensional room. The buttons move it through the fourth dimension: Y / X pull it through our three dimensions (along w), B / A turn it in the
     * plane of your right hand and w, about your head, so the thing you look at stays where it is while it changes. Both ease in and out, and each has its
     * sound (as the fractals' shapes do). The sticks are for getting about: the left stick walks you through the room (along where you look, and sideways),
     * the right stick turns you, a step of 30 degrees at a time. A press on the left stick puts the fourth dimension back. */
    let pull = 0, rot = 0, reset = false, mx = 0, mz = 0, snap = 0;
    if (S.mode === 'xr') {
      pull = (btn(L, 5) ? 1 : 0) - (btn(L, 4) ? 1 : 0); rot = (btn(R, 5) ? 1 : 0) - (btn(R, 4) ? 1 : 0); reset = btn(L, 3);
      if (L && L.axes.length >= 4) { mx = dead(L.axes[2]); mz = -dead(L.axes[3]); }
      if (R && R.axes.length >= 4) { const ax = R.axes[2]; if (Math.abs(ax) > 0.7 && !FC.snapHeld) { snap = Math.sign(ax); FC.snapHeld = true; } else if (Math.abs(ax) < 0.4) FC.snapHeld = false; }
    } else { pull = (keys.KeyM ? 1 : 0) - (keys.KeyN ? 1 : 0); rot = (keys.KeyB ? 1 : 0) - (keys.KeyV ? 1 : 0); reset = !!keys.KeyK; }
    FV.p += (pull - FV.p) * Math.min(1, dt * (pull ? 3 : 2)); FV.r += (rot - FV.r) * Math.min(1, dt * (rot ? 3 : 2));
    if (Math.abs(FV.p) < 0.004 && !pull) FV.p = 0; if (Math.abs(FV.r) < 0.004 && !rot) FV.r = 0;
    if (FV.p || FV.r) { const r = new V(1, 0, 0).applyQuaternion(headQ); r.y = 0; if (r.lengthSq() < 0.01) r.set(1, 0, 0); r.normalize(); worlds.fourStep(S.cur, head, r, FV.r * 0.7 * dt, FV.p * 0.4 * dt); FC.fourT = 1.8; }
    { const inf = worlds.fourInfo(S.cur); AMB.param(0, FV.p, clamp((inf.depth + 3) / 6, 0, 1)); AMB.param(1, FV.r, ((inf.turn / TAU) % 1 + 1) % 1); }
    if (mx || mz) { const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; if (f.lengthSq() < 0.01) f.set(0, 0, -1); f.normalize(); const rr = new V(-f.z, 0, f.x);
      worlds.fourWalk(S.cur, head, f.multiplyScalar(mz).addScaledVector(rr, mx).multiplyScalar(1.3 * dt)); }
    if (snap) worlds.fourTurnAbout(S.cur, head, snap * Math.PI / 6);
    if (reset && !FC.fourReset) { worlds.fourReset(S.cur); FC.fourT = 1.8; } FC.fourReset = reset; FC.wasFour = true;
    worlds.setFractalCalm(FC.still > 1.5 && !FV.p && !FV.r && !mx && !mz);
    if (FC.fourT > 0) {
      FC.fourT -= dt; const inf = worlds.fourInfo(S.cur), dg = ((Math.round(inf.turn * 180 / Math.PI) % 360) + 360) % 360, txt = '4D   dybde ' + (inf.depth >= 0 ? '+' : '\u2212') + Math.abs(inf.depth).toFixed(2) + ' m   drejet ' + dg + '\u00b0';
      if (txt !== gearShown) { gearShown = txt; const x = gearCanvas.getContext('2d'); x.clearRect(0, 0, 512, 128); x.fillStyle = 'rgba(4,10,16,.8)'; x.beginPath(); x.roundRect(4, 4, 504, 120, 20); x.fill();
        x.fillStyle = '#ffd9a0'; x.font = 'bold 30px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif'; x.fillText(txt, 22, 76); gearTex.needsUpdate = true; }
      if (!gearMesh.parent) scene.add(gearMesh);
      const f = new V(0, 0, -1).applyQuaternion(headQ), tgt = new V().copy(head).addScaledVector(f, 0.9); tgt.y -= 0.22;
      gearMesh.position.lerp(tgt, Math.min(1, dt * 10)); gearMesh.lookAt(head); gearMesh.material.opacity = clamp(FC.fourT / 0.5, 0, 1); gearMesh.visible = true;
    } else gearMesh.visible = false;
    controllerTips(dt, li, ri); return;
  }
  if (FC.wasFour) { FC.wasFour = false; worlds.fourReset(FOUR); }      // step out of the room and it settles back to how it was
  if (!inF) { tips.left.visible = tips.right.visible = false; gearMesh.visible = false; worlds.setFractalCalm(false); return; }
  let fwd = 0, side = 0, gear = 0, par = 0, par2 = 0, scale = 1;
  if (S.mode === 'xr') {
    if (R && R.axes.length >= 4) { fwd = -dead(R.axes[3]); side = dead(R.axes[2]); }                        // right stick: fly where you look, and slide sideways
    if (L && L.axes.length >= 4) { gear = -dead(L.axes[3]); const lx = L.axes[2]; if (Math.abs(lx) > 0.5) gear = 0;      // left stick: faster / slower ...
      if (Math.abs(lx) > 0.92 && Math.abs(L.axes[3]) < 0.45 && !FC.hueHeld) { FC.hueHeld = true; FC.hueTo = (FC.hueTo || 0) + Math.sign(lx); } else if (Math.abs(lx) < 0.5) FC.hueHeld = false; }      // ... and all the way to a side: the next colour scheme
    par = (btn(L, 5) ? 1 : 0) - (btn(L, 4) ? 1 : 0); par2 = (btn(R, 5) ? 1 : 0) - (btn(R, 4) ? 1 : 0);      // Y / X and B / A: one shape parameter each
    if (btn(L, 1) && btn(R, 1)) { const d = hands[li].pos.distanceTo(hands[ri].pos); if (FC.gripD > 0.02 && d > 0.02) scale = Math.pow(FC.gripD / d, 2.5); FC.gripD = d; } else FC.gripD = 0;      // hands apart: the fractal grows (strongly)
  } else {
    fwd = (keys.KeyI ? 1 : 0) - (keys.KeyK ? 1 : 0); side = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); gear = (keys.KeyO ? 1 : 0) - (keys.KeyU ? 1 : 0);
    par = (keys.KeyM ? 1 : 0) - (keys.KeyN ? 1 : 0); par2 = (keys.KeyB ? 1 : 0) - (keys.KeyV ? 1 : 0);
    scale = keys.Period ? 1 - dt * 1.5 : keys.Comma ? 1 + dt * 1.5 : 1;
    const hk = (keys.KeyH ? 1 : 0) - (keys.KeyG ? 1 : 0); if (hk && !FC.hueHeld) { FC.hueHeld = true; FC.hueTo = (FC.hueTo || 0) + hk; } else if (!hk) FC.hueHeld = false;
  }
  FC.hue = (FC.hue || 0) + ((FC.hueTo || 0) - (FC.hue || 0)) * Math.min(1, dt * 5); worlds.setHue(FC.hue * Math.PI / 3);      // six schemes round the colour wheel
  if (gear) { FC.gear = clamp(FC.gear + gear * dt * 3.2, 1, 9); FC.gearT = 1.6; }
  if (fwd || side) {
    const [f, r] = flyAim(ri);
    worlds.fractMove(S.cur, head, f.multiplyScalar(fwd).addScaledVector(r, side).multiplyScalar(0.15 * Math.pow(1.65, FC.gear - 1)), dt);      // along your gaze (look down to dive); speed 1 is a crawl, 9 is about 8 m/s
  }
  if (scale !== 1) worlds.fractScale(S.cur, head, scale);
  const moving = easeParams(dt, par, par2);
  worlds.setFractalCalm(FC.still > 1.5 && !fwd && !side && !moving && scale === 1);      // standing still, hands off: the fractal may sharpen
  gearPanel(dt);
  controllerTips(dt, li, ri);
}
/* ---- the spaceship (an option on the start page): in the fractal dimensions and in deep space you do not fly yourself, you fly a ship that hangs in the
 * room in front of you. The right stick steers it (left / right turns it, forward tips the nose down, back lifts it), the left stick up / down is the
 * throttle (0 is stopped, 1 to 9 as before) and it flies where its nose points. Nothing turns but the ship: the dimension only ever slides past, so it is
 * easy on the stomach, and you can walk round the ship and watch it from any side. Y / X, B / A, both grips and the colour flick work as without the ship;
 * the fractal grows and shrinks about the ship (not in combat, where the grips are weapons). ---- */
const shipModel = buildShip(), ship = new THREE.Group(); shipModel.scale.setScalar(0.045); ship.add(shipModel); ship.visible = false; scene.add(ship);      // 19 m of ship as an 86 cm model
const SH = { hull: 100, kills: 0, dead: 0, redT: 0, q: new THREE.Quaternion(), roll: 0, away: 0, idle: 9, turnTo: null, touch: false, touchT: 0, held: false, h0: new THREE.Quaternion(), s0: new THREE.Quaternion() };
const _yq = new THREE.Quaternion(), _pq = new THREE.Quaternion(), YAX = new V(0, 1, 0), XAX = new V(1, 0, 0);
function shipControls(dt, L, R, li, ri) {
  const space = !!DIMS[S.cur].space, gaze = new V(0, 0, -1).applyQuaternion(headQ); gaze.y = 0; if (gaze.lengthSq() < 0.01) gaze.set(0, 0, -1); gaze.normalize();
  const spot = new V().copy(head).addScaledVector(gaze, 1.4); spot.y = Math.max(0.7, head.y - 0.4);
  if (!FC.shipOn) { FC.shipOn = true; ship.position.copy(spot); SH.q.setFromAxisAngle(YAX, Math.atan2(-gaze.x, -gaze.z)); SH.roll = 0; SH.away = 0; SH.idle = 9; SH.turnTo = null; SH.held = false; FC.gear = 0; FC.gearT = 2.5; }
  if (SH.dead > 0) {                                                  // lost: nothing to fly for a moment; then a new ship, and the bugs near it are cleared away
    SH.dead -= dt; ship.visible = false; FC.gear = 0; shipModel.userData.thrust(0, S.time); SH.wall = Infinity;
    if (SH.dead <= 0) { SH.dead = 0; SH.hull = 100; ship.position.copy(spot); SH.q.setFromAxisAngle(YAX, Math.atan2(-gaze.x, -gaze.z)); COMBAT.clearNear(ship.position, 2.5); hullPanel(true); say('Et nyt skib'); }
    gearPanel(dt); controllerTips(dt, li, ri); return;
  }
  let yawIn = 0, pitchIn = 0, thr = 0, par = 0, par2 = 0, scale = 1, hk = 0, hold = false, hq = headQ;
  if (S.mode === 'xr') {
    if (R && R.axes.length >= 4) { yawIn = dead(R.axes[2]); pitchIn = dead(R.axes[3]); }
    if (L && L.axes.length >= 4) { thr = -dead(L.axes[3]); const lx = L.axes[2]; if (Math.abs(lx) > 0.5) thr = 0; hk = Math.abs(lx) > 0.92 && Math.abs(L.axes[3]) < 0.45 ? Math.sign(lx) : Math.abs(lx) < 0.5 ? 0 : (FC.hueHeld ? 1 : 0); }
    par = (btn(L, 5) ? 1 : 0) - (btn(L, 4) ? 1 : 0); par2 = (btn(R, 5) ? 1 : 0) - (btn(R, 4) ? 1 : 0);
    /* both grips scale the fractal; in combat the grips are the ray and the rockets */
    if (!space && !S.combat && btn(L, 1) && btn(R, 1)) { const d = hands[li].pos.distanceTo(hands[ri].pos); if (FC.gripD > 0.02 && d > 0.02) scale = Math.pow(FC.gripD / d, 2.5); FC.gripD = d; } else FC.gripD = 0;
  } else {
    yawIn = (keys.KeyL ? 1 : 0) - (keys.KeyJ ? 1 : 0); pitchIn = (keys.KeyK ? 1 : 0) - (keys.KeyI ? 1 : 0); thr = (keys.KeyO ? 1 : 0) - (keys.KeyU ? 1 : 0);
    par = (keys.KeyM ? 1 : 0) - (keys.KeyN ? 1 : 0); par2 = (keys.KeyB ? 1 : 0) - (keys.KeyV ? 1 : 0); hk = (keys.KeyH ? 1 : 0) - (keys.KeyG ? 1 : 0);
    scale = space ? 1 : keys.Period ? 1 - dt * 1.5 : keys.Comma ? 1 + dt * 1.5 : 1;
  }
  if (hk && !FC.hueHeld) { FC.hueHeld = true; if (!space) FC.hueTo = (FC.hueTo || 0) + Math.sign(hk); } else if (!hk) FC.hueHeld = false;
  FC.hue = (FC.hue || 0) + ((FC.hueTo || 0) - (FC.hue || 0)) * Math.min(1, dt * 5); worlds.setHue(FC.hue * Math.PI / 3);
  /* (Steering by hand with the right grip was tried and dropped: the right grip fires the rockets in combat.) */
  if (yawIn || pitchIn) SH.q.premultiply(_yq.setFromAxisAngle(YAX, -yawIn * 1.3 * dt)).multiply(_pq.setFromAxisAngle(XAX, pitchIn * 0.9 * dt)).normalize();
  SH.roll += (-yawIn * 0.5 - SH.roll) * Math.min(1, dt * 4);      // it banks into its turns
  ship.quaternion.copy(SH.q); shipModel.rotation.z = SH.roll;
  if (thr) { FC.gear = clamp(FC.gear + thr * dt * 3.2, 0, 9); FC.gearT = 1.6; }
  const lvl = Math.round(FC.gear), v = lvl < 1 ? 0 : 0.15 * Math.pow(1.65, FC.gear - 1) * (space ? 3 : 1);
  const nose = new V(0, 0, -1).applyQuaternion(SH.q);
  if (v) {
    if (space) worlds.spaceMove(nose, v * dt);
    else {                                                           // the fractal is solid: go forward in short steps, and stop at the first one that would put the ship into a wall
      const n = Math.max(1, Math.ceil(v * dt / 0.04)), step = v * dt / n; let hit = false;
      for (let k = 0; k < n && !hit; k++) { const d0 = shipClear(nose, 0), d1 = shipClear(nose, step); if (d1 >= 0 || d1 > d0) worlds.fractMove(S.cur, ship.position, nose, step); else hit = true; }      // (a ship already in a wall may always move out of it)
      if (hit) { FC.gear = 0; FC.gearT = 1.6; AMB.thud(ship.position); const np = ship.position.clone().addScaledVector(nose, 0.4); boomAt(np, 0.4 + v * 0.15, true); shipDamage(Math.min(60, Math.round(2 + 9 * v)), np); for (const c of ctrl) { try { c.src?.gamepad?.hapticActuators?.[0]?.pulse?.(1, 160); } catch (_) { /* none */ } } }
    }
  }
  SH.wall = space ? Infinity : worlds.fractDist(S.cur, ship.position);
  if (space) { if (par2) { const b = worlds.spaceWarpNow(); if (b < 0.003 && par2 > 0) SPN.axis.copy(nose); worlds.spaceWarp(SPN.axis, clamp(b + par2 * dt * 0.3, 0, 0.93)); } if (par) worlds.spaceHole(clamp(worlds.spaceHoleNow() + par * dt * 0.09, 0, 0.42)); }
  else { if (scale !== 1) worlds.fractScale(S.cur, ship.position, scale); par = easeParams(dt, par, par2) ? 1 : 0; par2 = 0; }
  /* Where the ship hangs. It keeps ahead of you in the direction it is flying, so that you see it from behind: turn it, and it swings round you to its new
   * place, and you turn with it. And the other way about: turn round yourself, leave the controls alone, and after a moment the ship comes round in front of
   * you and points where you are facing. Its place in the room is only where you watch it from; it stays where it is in the dimension (the dimension slides
   * with it), so it cannot be carried through a wall this way. */
  const noseH = new V(nose.x, 0, nose.z); if (noseH.lengthSq() < 0.02) noseH.copy(gaze); noseH.normalize();
  const want = new V().copy(head).addScaledVector(noseH, 1.4); want.y = Math.max(0.7, head.y - 0.4);
  shipShift(new V().subVectors(want, ship.position).multiplyScalar(Math.min(1, dt * 1.3)), space);
  SH.idle = yawIn || pitchIn || hold ? 0 : SH.idle + dt;
  const to = new V(ship.position.x - head.x, 0, ship.position.z - head.z).normalize();
  SH.away = to.dot(gaze) < -0.3 && SH.idle > 3 ? SH.away + dt : 0;        // the ship is behind you, and you are not steering
  if (SH.away > 2) { SH.turnTo = new THREE.Quaternion().setFromAxisAngle(YAX, Math.atan2(-gaze.x, -gaze.z)); SH.away = 0; }
  if (SH.turnTo) { if (SH.idle < 0.2 && SH.away === 0 && (yawIn || pitchIn || hold)) SH.turnTo = null; else { SH.q.slerp(SH.turnTo, Math.min(1, dt * 2.2)); if (SH.q.angleTo(SH.turnTo) < 0.03) SH.turnTo = null; } }
  /* A hand against the hull pushes it: the ship gives way, turns a little if you catch it near the nose or the tail, and drifts back to its place. */
  for (let h = 0; h < 2; h++) {
    if (!hands[h].on) continue;
    const t = clamp(new V().subVectors(hands[h].pos, ship.position).dot(nose), -0.4, 0.42), c = new V().copy(ship.position).addScaledVector(nose, t), dv = new V().subVectors(c, hands[h].pos), d = dv.length();
    if (d < 0.2 && d > 1e-4) { dv.multiplyScalar(1 / d); const pen = 0.2 - d; shipShift(dv.clone().multiplyScalar(pen), space);
      SH.q.premultiply(_yq.setFromAxisAngle(YAX, clamp((t / 0.4) * (nose.z * dv.x - nose.x * dv.z) * pen * 4, -0.2, 0.2))).normalize(); SH.idle = 0;
      if (!SH.touch) { SH.touch = true; AMB.thud(ship.position, 0.25); } try { ctrl[h].src?.gamepad?.hapticActuators?.[0]?.pulse?.(clamp(0.2 + pen * 6, 0, 1), 30); } catch (_) { /* none */ } SH.touchT = 0.25; }
  }
  if (SH.touchT > 0) SH.touchT -= dt; else SH.touch = false;
  shipModel.userData.thrust(lvl < 1 ? 0 : FC.gear / 9, S.time);
  ship.visible = true;
  worlds.setFractalCalm(!space && v === 0 && !yawIn && !pitchIn && !hold && !par && !par2 && scale === 1 && FC.still > 1.5);
  gearPanel(dt); controllerTips(dt, li, ri);
}
/** Which way "forward" is when you fly yourself: where you look (the default), or, if chosen on the start page, where your right hand points. Also the
 *  level direction to its right, for sliding sideways. */
function flyAim(ri) {
  const hand = S.steer === 'hand' && S.mode === 'xr' && hands[ri] && hands[ri].on;
  const f = hand ? new V().copy(hands[ri].rayD).normalize() : new V(0, 0, -1).applyQuaternion(headQ).normalize();
  const r = new V().crossVectors(f, new V(0, 1, 0)); if (r.lengthSq() < 0.01) r.set(1, 0, 0).applyQuaternion(headQ).setY(0); if (r.lengthSq() < 0.01) r.set(1, 0, 0); r.normalize();
  return [f, r];
}
/** How much room the ship has: the least distance from its nose, middle, tail, mast and aerial to the fractal, less the ship's own thickness there
 *  (metres; negative means it is in a wall), once it has gone `ahead` metres along its nose. */
const _sc = new V(), _su = new V();
/** Move the ship in the room without moving it in the dimension: the dimension is slid along with it. */
function shipShift(d, space) { const l = d.length(); if (l < 1e-5) return; ship.position.add(d); const u = d.clone().multiplyScalar(1 / l); if (space) worlds.spaceMove(u, -l); else worlds.fractMove(S.cur, ship.position, u, -l); }
function shipClear(nose, ahead) {
  _su.set(0, 1, 0).applyQuaternion(SH.q); let m = Infinity;
  for (const [f, u, r] of [[0.4, -0.06, 0.07], [0.2, 0, 0.14], [0, 0, 0.15], [-0.25, 0, 0.1], [-0.4, 0.12, 0.1], [0.07, 0.32, 0.03], [0.1, -0.28, 0.02]]) {
    _sc.copy(ship.position).addScaledVector(nose, f + ahead).addScaledVector(_su, u); m = Math.min(m, worlds.fractDist(S.cur, _sc) - r); }
  return m;
}
/* ---- combat (a test, chosen on the start page): alien bugs in the fractal dimensions (combat.js), and a ship that can be damaged ---- */
const boomSparks = makeSparks(500, scene);
function glowSprite(stops) { const c = document.createElement('canvas'); c.width = c.height = 64; const x = c.getContext('2d'), g = x.createRadialGradient(32, 32, 0, 32, 32, 32); stops.forEach(([o, s]) => g.addColorStop(o, s)); x.fillStyle = g; x.fillRect(0, 0, 64, 64);
  const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: new THREE.CanvasTexture(c), blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0 })); sp.renderOrder = 49; sp.visible = false; scene.add(sp); return sp; }
const BOOMS = [...Array(8)].map((_, k) => Object.assign(glowSprite(k % 2 ? [[0, 'rgba(255,255,255,1)'], [0.3, 'rgba(255,170,80,.7)'], [1, 'rgba(255,80,20,0)']] : [[0, 'rgba(255,255,255,1)'], [0.3, 'rgba(170,255,90,.7)'], [1, 'rgba(60,255,40,0)']]), { userData: { t: 9, d: 0.5, s: 0.3 } }));
function boomAt(p, size, hot) { boomLast = S.time; const b = BOOMS.filter((s, k) => (k % 2 === 1) === !!hot).sort((a, c) => c.userData.t - a.userData.t)[0]; b.position.copy(p); b.userData = { t: 0, d: 0.35 + 0.25 * size, s: size }; b.visible = true;
  for (let k = 0; k < 14 + 30 * Math.min(2, size); k++) { const sp = rnd(0.3, 1.3) * (0.4 + 0.6 * size); _v.set(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)).normalize(); boomSparks.emit(p.x, p.y, p.z, _v.x * sp, _v.y * sp, _v.z * sp, rnd(0.3, 0.8)); } }
let boomLast = -99;
function updateBooms(dt) { const live = S.time - boomLast < 1.2; boomSparks.points.visible = live; if (live) boomSparks.step(dt, 0.15); for (const b of BOOMS) { if (!b.visible) continue; b.userData.t += dt; const u = b.userData.t / b.userData.d; if (u >= 1) { b.visible = false; continue; } b.material.opacity = (1 - u) * (1 - u); b.scale.setScalar((0.08 + 0.5 * b.userData.s) * (0.4 + 1.6 * Math.sqrt(u))); } }
const buzzAll = (v, ms) => { for (const c of ctrl) { try { c.src?.gamepad?.hapticActuators?.[0]?.pulse?.(v, ms); } catch (_) { /* none */ } } };
const COMBAT = createCombat({ scene, worlds,
  onBlast: (p, size) => { boomAt(p, 0.6 + size * 4, false); AMB.boom(p, 0.6 + size * 6, 'blast'); shipDamage(Math.round(10 * clamp(size / 0.06, 0.5, 3)), p); },
  onCrush: (p, size) => { SH.kills++; boomAt(p, 0.3 + size * 3, false); AMB.boom(p, 0.4 + size * 4, 'crush'); hullPanel(true); },
  onShot: (p, size) => { SH.kills++; boomAt(p, 0.35 + size * 3, false); AMB.boom(p, 0.35 + size * 4, 'crush'); hullPanel(true); },
  onSpark: p => { boomAt(p, 0.08, true); },
  onRocket: (p, z = 1) => { boomAt(p, 2.4 * z, true); boomAt(p, 1.4 * z, false); AMB.boom(p, 3 * Math.min(2, z), 'blast'); buzzAll(0.7, 180); if (ship.visible && p.distanceTo(ship.position) < 0.6) shipDamage(20, p); },
  onWarn: (p, k) => { WARN.t -= 0; if (S.time >= WARN.next) { AMB.warn(p, k); WARN.next = S.time + 0.9 - 0.65 * k; } },
  onRayKill: (p, size) => { SH.kills++; boomAt(p, 0.5 + size * 3, false); hullPanel(true); },
  onTrail: (p, d) => { boomLast = S.time; for (let k = 0; k < 3; k++) boomSparks.emit(p.x, p.y, p.z, -d.x * 0.5 + rnd(-0.08, 0.08), -d.y * 0.5 + rnd(-0.08, 0.08), -d.z * 0.5 + rnd(-0.08, 0.08), rnd(0.15, 0.3)); } });
/* The ship's weapons, all straight ahead along the nose:
 *   trigger of the hand without the knife   plasma bolts, ten a second while held, by turns; they go through bugs   (flat preview: space bar)
 *   left grip                                the ray: a cone of light for 1.2 s that annihilates everything in it; then 5 s to charge   (flat preview: C)
 *   right grip                               a pair of rockets from the two pods; 1.5 s to reload                     (flat preview: X)
 * The board over the ship shows the ray charging and the rockets ready, and how fast the bugs have become. */
/* the warning chirp's pace: the closer the bug, the faster it repeats */
const WARN = { next: 0, t: 0 };
const GUN = { cool: 0, side: 1, rayCool: 0, rocketCool: 0, rayWas: false, rocketWas: false };
const RAY_CHARGE = 5, RAY_LAST = 1.2, ROCKET_RELOAD = 1.5;
const gunHand = () => 1 - knifeIdx;
function shipGuns(dt) {
  GUN.cool -= dt; if (GUN.rayCool > 0) GUN.rayCool -= dt; if (GUN.rocketCool > 0) GUN.rocketCool -= dt;
  if (!(S.combat && FC.shipOn && ship.visible && !(SH.dead > 0) && DIMS[S.cur].fract && !DIMS[S.cur].four)) { GUN.rayWas = GUN.rocketWas = false; return; }
  const gi = gunHand(), gp = S.mode === 'xr' ? ctrl[gi].src && ctrl[gi].src.gamepad : null;
  const lc = ctrl.find(c => c.hand === 'left') || ctrl[gi], rc = ctrl.find(c => c.hand === 'right') || ctrl[1 - gi];
  const trig = S.mode === 'xr' ? !!ctrl[gi].pressed : !!keys.Space;
  const rayKey = S.mode === 'xr' ? btn(lc.src && lc.src.gamepad, 1) : !!keys.KeyC, rocketKey = S.mode === 'xr' ? btn(rc.src && rc.src.gamepad, 1) : !!keys.KeyX;
  const nose = new V(0, 0, -1).applyQuaternion(SH.q), right = new V(1, 0, 0).applyQuaternion(SH.q), down = new V(0, -1, 0).applyQuaternion(SH.q), v = Math.round(FC.gear) < 1 ? 0 : 0.15 * Math.pow(1.65, FC.gear - 1);
  if (rocketKey && !GUN.rocketWas && GUN.rocketCool <= 0) {
    let n = 0;
    for (const sd of [-1, 1]) { const from = ship.position.clone().addScaledVector(nose, 0.25).addScaledVector(right, 0.13 * sd).addScaledVector(down, 0.05); if (COMBAT.launch(from, nose, v)) { n++; AMB.rocket(from); } }
    if (n) { GUN.rocketCool = ROCKET_RELOAD; hullPanel(true); try { rc.src?.gamepad?.hapticActuators?.[0]?.pulse?.(0.7, 90); } catch (_) { /* none */ } }
  }
  GUN.rocketWas = rocketKey;
  if (rayKey && !GUN.rayWas && GUN.rayCool <= 0) {
    const from = ship.position.clone().addScaledVector(nose, 0.44);
    if (COMBAT.ray()) { GUN.rayCool = RAY_LAST + RAY_CHARGE; AMB.ray(from); boomAt(from, 0.3, false); hullPanel(true); try { lc.src?.gamepad?.hapticActuators?.[0]?.pulse?.(1, 1100); } catch (_) { /* none */ } }
  }
  GUN.rayWas = rayKey;
  if (!trig || GUN.cool > 0) return;
  GUN.cool = 0.1; GUN.side = -GUN.side;
  const from = ship.position.clone().addScaledVector(nose, 0.44).addScaledVector(right, 0.07 * GUN.side);
  if (!COMBAT.fire(from, nose, v)) return;
  AMB.shot(from); boomAt(from, 0.05, true);
  try { ctrl[gi].src?.gamepad?.hapticActuators?.[0]?.pulse?.(0.35, 25); } catch (_) { /* none */ }
}
const shipBody = shipModel.children.find(m => m.material && m.material.vertexColors);
function shipDamage(n, p) {
  if (!S.combat || SH.dead > 0 || n <= 0) return;
  SH.hull = Math.max(0, SH.hull - n); SH.redT = 0.5; buzzAll(clamp(0.3 + n / 40, 0, 1), 120 + n * 4); hullPanel(true);
  if (SH.hull <= 0) {                                                // the ship is lost: it goes up, and a new one comes after a few seconds
    const nose = new V(0, 0, -1).applyQuaternion(SH.q);
    for (let k = -2; k <= 2; k++) { const q = ship.position.clone().addScaledVector(nose, k * 0.18); boomAt(q, 1.6, true); }
    AMB.boom(ship.position, 3, 'ship'); buzzAll(1, 600); SH.dead = 3.5; FC.gear = 0; ship.visible = false; say('Skibet blev \u00f8delagt');
  }
}
/* a little board over the ship: the hull, and how many bugs the walls have crushed */
const hullCanvas = document.createElement('canvas'); hullCanvas.width = 256; hullCanvas.height = 84;
const hullTex = new THREE.CanvasTexture(hullCanvas); hullTex.colorSpace = THREE.SRGBColorSpace;
const hullMesh = new THREE.Mesh(new THREE.PlaneGeometry(0.26, 0.0853), new THREE.MeshBasicMaterial({ map: hullTex, transparent: true, depthTest: false, toneMapped: false })); hullMesh.renderOrder = 103; hullMesh.visible = false; scene.add(hullMesh);
let hullSig = '';
function hullPanel(force) {
  const charge = GUN.rayCool <= 0 ? 1 : GUN.rayCool > RAY_CHARGE ? 0 : 1 - GUN.rayCool / RAY_CHARGE, rayOK = charge >= 1, rocketOK = GUN.rocketCool <= 0, pace = COMBAT.pace();
  const sig = Math.round(SH.hull) + '/' + SH.kills + '/' + Math.floor(charge * 20) + '/' + rocketOK + '/' + pace.toFixed(1); if (!force && sig === hullSig) return; hullSig = sig;
  const x = hullCanvas.getContext('2d'), h = SH.hull / 100; x.clearRect(0, 0, 256, 84); x.fillStyle = 'rgba(4,10,16,.78)'; x.beginPath(); x.roundRect(2, 2, 252, 80, 12); x.fill();
  x.font = 'bold 15px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif';
  x.fillStyle = rayOK ? '#9fd0ff' : 'rgba(159,208,255,.45)'; x.fillText('STR\u00c5LE', 14, 70);
  x.fillStyle = 'rgba(159,208,255,.2)'; x.fillRect(80, 60, 60, 10); x.fillStyle = rayOK ? '#bfe4ff' : '#5f8fd0'; x.fillRect(80, 60, 60 * charge, 10);
  x.fillStyle = rocketOK ? '#ffb070' : 'rgba(255,176,112,.28)'; x.fillText('\u25cf RAKET', 150, 70);
  x.fillStyle = pace > 1.05 ? '#ff9a7a' : 'rgba(255,255,255,.4)'; x.font = 'bold 13px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif'; x.fillText('FART \u00d7' + pace.toFixed(1), 186, 24);
  x.font = 'bold 15px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif';
  x.fillStyle = h > 0.5 ? '#7dffb0' : h > 0.25 ? '#ffd56a' : '#ff6a5a'; x.fillRect(14, 30, 150 * h, 12); x.strokeStyle = 'rgba(255,255,255,.35)'; x.lineWidth = 2; x.strokeRect(14, 30, 150, 12);
  x.fillStyle = '#d7ffe4'; x.font = 'bold 19px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif'; x.fillText('SKROG ' + Math.round(SH.hull) + '%', 14, 24);
  x.fillStyle = '#b8ff8a'; x.fillText('\u2716 ' + SH.kills, 186, 42); hullTex.needsUpdate = true;
}
function updateCombat(dt) {
  const on = S.combat && FC.shipOn && DIMS[S.cur].fract && !DIMS[S.cur].four;
  COMBAT.update(dt, on ? { i: S.cur, ship: ship.position, nose: new V(0, 0, -1).applyQuaternion(SH.q), head, gaze: new V(0, 0, -1).applyQuaternion(headQ), alive: !(SH.dead > 0) } : null);
  shipGuns(dt);
  updateBooms(dt);
  if (SH.redT > 0) { SH.redT -= dt; const k = Math.max(0, SH.redT / 0.5); shipBody.material.color.setRGB(1, 1 - 0.6 * k, 1 - 0.65 * k); } else shipBody.material.color.setRGB(1, 1, 1);
  hullMesh.visible = on && ship.visible;
  if (hullMesh.visible) { hullPanel(false); hullMesh.position.copy(ship.position); hullMesh.position.y += 0.32; hullMesh.lookAt(head); }
}
const AMB = createAmbience(), HOLE_DIR = new V(-0.2425, 0.1455, -0.9592);      // where the black hole is in the sky (the same direction as in the sky shader)
/* Changing a fractal's shape with Y / X and B / A. Not a switch: the change gathers speed when you press, coasts to a stop when you let go, and slows by
 * itself as it nears either end of the parameter's range. Each of the four (two parameters, two directions) has a sound of its own (ambience.js). */
const PV = [0, 0];
function easeParams(dt, par, par2) {
  const fr = worlds.fractState(S.cur); let any = false;
  [par, par2].forEach((tgt, w) => {
    PV[w] += (tgt - PV[w]) * Math.min(1, dt * (tgt ? 3.0 : 2.0)); if (Math.abs(PV[w]) < 0.004 && !tgt) PV[w] = 0;
    const [, lo, hi] = w ? fr.f.par2 : fr.f.par, x = ((w ? fr.param2 : fr.param) - lo) / (hi - lo), room = PV[w] > 0 ? 1 - x : x, k = clamp(room / 0.16, 0, 1);
    const v = PV[w] * (0.06 + 0.94 * k * k * (3 - 2 * k));              // the last sixth of the range is taken slowly
    if (v) { worlds.fractParam(S.cur, w, v * 1.5, dt); any = true; }
    AMB.param(w, v, x);
  });
  return any;
}
function gearPanel(dt) {
  /* the speed, 1 to 9, shown in front of you while you change it */
  if (FC.gearT > 0) {
    FC.gearT -= dt; const lvl = Math.round(FC.gear);
    if (lvl !== gearShown) { gearShown = lvl; const x = gearCanvas.getContext('2d'); x.clearRect(0, 0, 512, 128); x.fillStyle = 'rgba(4,10,16,.8)'; x.beginPath(); x.roundRect(4, 4, 504, 120, 20); x.fill();
      x.fillStyle = '#7dd8ff'; x.font = 'bold 44px -apple-system, "Segoe UI", Helvetica, Arial, sans-serif'; x.fillText('FART ' + lvl, 24, 78);
      for (let k = 1; k <= 9; k++) { x.fillStyle = k <= lvl ? '#7dd8ff' : 'rgba(125,216,255,.2)'; x.fillRect(210 + (k - 1) * 31, 92 - k * 7, 23, k * 7 + 4); }
      gearTex.needsUpdate = true; }
    if (!gearMesh.parent) scene.add(gearMesh);
    const f = new V(0, 0, -1).applyQuaternion(headQ), tgt = new V().copy(head).addScaledVector(f, 0.9); tgt.y -= 0.22;
    gearMesh.position.lerp(tgt, Math.min(1, dt * 10)); gearMesh.lookAt(head); gearMesh.material.opacity = clamp(FC.gearT / 0.5, 0, 1); gearMesh.visible = true;
  } else gearMesh.visible = false;
}
const SPN = { w: new V(), axis: new V(0, 0, -1) }, FV = { p: 0, r: 0 };
/* the first time in a fractal (or in space): the controllers buzz and blink until you look down at them */
function controllerTips(dt, li, ri) {
  if (!FC.tipsDone && S.mode === 'xr') { FC.tipsDone = true; FC.tipsT = 0; FC.seen = 0; renderer.xr.getController(li).add(tips.left); renderer.xr.getController(ri).add(tips.right); }
  if (FC.tipsT >= 0) {
    FC.tipsT += dt;
    const f = new V(0, 0, -1).applyQuaternion(headQ); let looking = false;                                   // looking at either hand?
    for (const i of [li, ri]) { const d = new V().copy(hands[i].pos).sub(head); if (d.length() > 0.05 && d.normalize().dot(f) > 0.93) looking = true; }
    FC.seen = looking ? FC.seen + dt : Math.max(0, FC.seen - dt * 0.5);
    const done = FC.seen > 0.7 || FC.tipsT > 40, a = done ? 0 : Math.pow(0.5 + 0.5 * Math.sin(FC.tipsT * 9), 2);
    for (const t of [tips.left, tips.right]) { t.visible = !done; t.material.opacity = a; t.scale.setScalar(0.12 + 0.08 * a); }
    if (!done && ((FC.tipsT / 0.9) | 0) !== FC.buzz) { FC.buzz = (FC.tipsT / 0.9) | 0; for (const c of ctrl) { try { c.src?.gamepad?.hapticActuators?.[0]?.pulse?.(0.7, 140); } catch (_) { /* none */ } } }
    if (done) FC.tipsT = -1;
  }
}
/* ---- your hands, where the real ones cannot be seen (every dimension but your own room) ---- */
const handModels = { left: createHand('left'), right: createHand('right') };
function updateHandModels(dt) {
  const show = S.cur !== 0;
  if (S.mode === 'xr') {
    for (let i = 0; i < 2; i++) {
      const sd = ctrl[i].hand; if (sd !== 'left' && sd !== 'right') continue;
      const m = handModels[sd], grip = renderer.xr.getControllerGrip(i); if (m.parent !== grip) grip.add(m);
      m.visible = show; if (!show) continue;
      const gp = ctrl[i].src && ctrl[i].src.gamepad, b = k => (gp && gp.buttons[k]) || null;
      const thumb = [3, 4, 5].some(k => b(k) && (b(k).touched || b(k).pressed));
      m.userData.pose(b(0) ? b(0).value : 0, b(1) ? b(1).value : 0, thumb, i === knifeIdx && knife.parent === grip, dt);
    }
  } else if (S.mode === 'flat') {
    const m = handModels.right; if (!m.parent) scene.add(m);
    m.visible = show && !params.has('nohand'); if (!m.visible) return;
    if (FC.handPose) { m.position.fromArray(FC.handPose, 0); m.quaternion.fromArray(FC.handPose, 3); } else { m.position.copy(hands[0].pos); m.quaternion.copy(headQ); }
    m.userData.pose(flatHold ? 1 : 0, keys.KeyT ? 1 : 0, !!keys.KeyY, !!keys.KeyR, dt);
  }
}
const clock = new THREE.Clock();
let trigWas = false, cutOK = false, eyeViews = [];
const _hq = new THREE.Quaternion();
function updateHands() {
  hands.forEach((h, i) => {
    let trig = false;
    if (S.mode === 'xr') {
      const g = renderer.xr.getControllerGrip(i), r = renderer.xr.getController(i);
      h.on = !!ctrl[i].src; g.getWorldPosition(h.pos); g.getWorldQuaternion(h.q); r.getWorldPosition(h.rayO); r.getWorldQuaternion(_hq); h.rayD.set(0, 0, -1).applyQuaternion(_hq); trig = ctrl[i].pressed;
    } else {
      h.on = i === 0; if (!h.on) return;
      h.rayO.copy(head); h.rayD.set(0, 0, -1).applyQuaternion(headQ);
      if (flatHand.pos) h.pos.copy(flatHand.pos); else h.pos.copy(head).addScaledVector(h.rayD, 0.45).y -= 0.15;
      h.q.copy(headQ); trig = flatClick || flatHold;
    }
    h.trigEdge = trig && !h.trig; h.trig = trig;
  });
  flatClick = false;
}
function loop(_, frame) {
  const dt = Math.min(0.05, clock.getDelta()); S.time += dt; S.frames++;
  if (S.mode === 'flat') flatMove(dt);
  else if (S.mode === 'xr' && frame) {
    const ref = renderer.xr.getReferenceSpace(), vp = frame.getViewerPose(ref);
    if (vp) { const p = vp.transform.position, o = vp.transform.orientation; head.set(p.x, p.y, p.z); headQ.set(o.x, o.y, o.z, o.w); eyeViews = vp.views.map(v => ({ matrix: v.transform.matrix, proj: v.projectionMatrix })); }
    { const si = worlds.updateScan(frame, ref); S.scanFeature = si.feature; S.scanMeshes = si.count; }
    pollStick();
    if (lightProbe) { try { const est = frame.getLightEstimate(lightProbe), sh = est && est.sphericalHarmonicsCoefficients; if (sh && sh.length >= 3) { const lum = (0.2126 * sh[0] + 0.7152 * sh[1] + 0.0722 * sh[2]) / 3.54; S.lum = lum; S.brightTarget = clamp(0.5 + lum * 2.0, 0.5, 1.5); } } catch (_) { /* no estimate this frame */ } }
  }
  if (S.mode !== 'menu') {
    if (worlds.dirty) applyRoles();
    knife.updateWorldMatrix(true, true); knife.userData.tip.getWorldPosition(tipW);
    updateHands();
    const trig = S.mode === 'xr' ? ctrl[knifeIdx].pressed : flatTip.pressed;
    if (trig && !trigWas) cutOK = story.canStartCut(S.mode === 'xr' ? knifeIdx : 0);      // decided once, as the trigger goes down: a cut under way is never interrupted by a hand passing the belt
    if (!trig) cutOK = false; trigWas = trig;
    const pressed = trig && cutOK && story.canCut();
    cutterUpdate(dt, S.mode === 'flat' && (flatTip.pressed || flatTip.override) ? flatTip.pos : tipW, pressed);
    if (trail.fading) { trail.fade -= dt / 0.8; trMat.uniforms.uFade.value = Math.max(0, trail.fade); if (trail.fade <= 0) clearTrail(); }
    trMat.uniforms.uNow.value = S.time; updateTrailGeometry();
    tipSparks.step(dt, 1.2);
    knife.userData.rune.color.setRGB(0.25 * knifeGlow, 1.0 * Math.min(1.4, knifeGlow), 0.5 * knifeGlow);
    knife.userData.halo.opacity = clamp(0.16 * (knifeGlow - 0.3), 0, 1);
    fractalControls(dt); updateHandModels(dt); updateCombat(dt);
    updateAnim(dt); checkCrossing(); updateClip(); updateRing(dt); updateAudio(); updateMusic(dt); updateSpaceAmb();
    { const fr = DIMS[S.cur].fract && !DIMS[S.cur].four, on = !!FC.shipOn;
      AMB.update(AU.ctx, dt, { dim: fr ? DIMS[S.cur].key : null, ship: on && !(SH.dead > 0), power: on && FC.gear >= 0.5 ? FC.gear / 9 : 0, pos: ship.position, wall: on ? SH.wall : Infinity,
        space: DIMS[S.cur].space ? { warp: worlds.spaceWarpNow(), hole: worlds.spaceHoleNow(), dir: HOLE_DIR } : null });
      if (AU.spaceOscs) for (const o of AU.spaceOscs) o.detune.setTargetAtTime(DIMS[S.cur].space ? worlds.spaceWarpNow() * 1500 : 0, AU.ctx.currentTime, 0.3); }      // the drone of space itself climbs with the warp updateProximity(); drawHud();
    S.bright += (S.brightTarget - S.bright) * Math.min(1, dt * 1.5);
    story.update(dt);
    worlds.update(dt, S.time, S.bright, S.cur, S.oth, S.safe && S.cur !== 0 ? [head, hands[0].on ? hands[0].pos : head, hands[1].on ? hands[1].pos : head] : null, hands);
    updateVortex(dt);
    if (S.mode === 'flat') { camera.updateMatrixWorld(); eyeViews = [{ matrix: camera.matrixWorld.elements, proj: camera.projectionMatrix.elements }]; }
    worlds.renderFractals(eyeViews, S.cur, S.placed ? S.oth : -1, dt);      // fractal dimensions are marched into small per-eye pictures first
    renderer.render(scene, camera);
  }
}

worlds.precompile(camera);
applyRoles();
renderer.setAnimationLoop(loop);

(async function init() {
  const btnAR = $('btnAR'), btnFlat = $('btnFlat'), status = $('status');
  const btnFree = $('btnFree'), btnNoHelp = $('btnNoHelp');
  const optQual = $('optQual');
  try { if (optQual) { optQual.value = localStorage.getItem('portalknivenQual') || 'normal'; optQual.addEventListener('change', () => localStorage.setItem('portalknivenQual', optQual.value)); } } catch (_) { /* no storage */ }
  const optSharp = $('optSharp');
  try { if (optSharp) { optSharp.value = localStorage.getItem('portalknivenSharp') || 'taa'; optSharp.addEventListener('change', () => localStorage.setItem('portalknivenSharp', optSharp.value)); } } catch (_) { /* no storage */ }
  const optShip = $('optShip');
  try { if (optShip) { optShip.checked = localStorage.getItem('portalknivenShip') === '1'; optShip.addEventListener('change', () => localStorage.setItem('portalknivenShip', optShip.checked ? '1' : '0')); } } catch (_) { /* no storage */ }
  const optSteer = $('optSteer');
  try { if (optSteer) { optSteer.value = localStorage.getItem('portalknivenSteer') || 'gaze'; optSteer.addEventListener('change', () => localStorage.setItem('portalknivenSteer', optSteer.value)); } } catch (_) { /* no storage */ }
  const optCombat = $('optCombat');
  try { if (optCombat) { optCombat.checked = localStorage.getItem('portalknivenCombat') === '1'; optCombat.addEventListener('change', () => localStorage.setItem('portalknivenCombat', optCombat.checked ? '1' : '0')); } } catch (_) { /* no storage */ }
  const readOpts = () => { S.combat = params.has('combat') || !!(optCombat && optCombat.checked); S.steer = params.get('steer') || (optSteer ? optSteer.value : 'gaze'); S.ship = params.has('ship') || !!(optShip && optShip.checked) || S.combat; worlds.setFractalSharpen(params.get('sharp') || (optSharp ? optSharp.value : 'taa')); S.safe = true; S.scale = ({ high: 1.0, normal: 0.85, fast: 0.7 })[optQual ? optQual.value : 'normal'] || 0.85; };
  btnFlat.addEventListener('click', () => { readOpts(); enterFlat(); }); btnAR.addEventListener('click', () => { readOpts(); enterXR(false, true); });
  if (btnNoHelp) btnNoHelp.addEventListener('click', () => { readOpts(); enterXR(false, false); });
  if (btnFree) btnFree.addEventListener('click', () => { readOpts(); enterXR(true); });
  const btnFour = $('btnFour'); if (btnFour) btnFour.addEventListener('click', () => { readOpts(); enterXR(true, true, FOUR); });
  let ok = false;
  try { ok = !!(navigator.xr && await navigator.xr.isSessionSupported('immersive-ar')); } catch (_) { /* no WebXR */ }
  btnAR.disabled = !ok; if (btnFour) btnFour.disabled = !ok; if (btnFree) btnFree.disabled = !ok; if (btnNoHelp) btnNoHelp.disabled = !ok;
  status.textContent = ok ? 'Klar. Vælg, hvordan du vil spille.' : 'Headset-tilstand (immersive-ar) findes ikke her. Du kan se et fladt preview på computer.';
  if (params.has('flat')) { readOpts(); enterFlat(); }
})();

window.__portal = {
  S, portal, head, scene, renderer, camera, music, prox, FC, keys, story, hands, dissolvePortal,
  hand(x, y, z) { flatHand.pos = x === null ? null : new V(x, y, z); }, click() { flatClick = true; }, hold(v) { flatHold = v; },
  knife, cut, trail, applyRoles, worlds, handModels, ship, SH, AMB, shipClear, COMBAT, shipDamage, DIMS, cycleOther, setOther, VX, SPN, placeDirect, igniteAt,
  view(x, y, z, yw, pt) { camera.position.set(x, y, z); yaw = yw; pitch = pt; prevHead.copy(head.set(x, y, z)); },
  walk(x, y, z) { camera.position.set(x, y, z); head.set(x, y, z); checkCrossing(); },
  tip(x, y, z, pressed) { flatTip.override = true; flatTip.pos.set(x, y, z); flatTip.pressed = pressed; },
};
