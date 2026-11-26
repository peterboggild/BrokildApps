/* Journey to the Moon, stage 0: the Moon test.
 *
 * Three views of the same baked Moon, to judge realism and frame rate in the headset before anything else is
 * built: lunar orbit (110 km), the low point of the descent orbit (15 km), and standing in Taurus-Littrow.
 *
 * URL: ?view=orbit|low|surface  &theta=deg (position along the orbit)  &yaw=&pitch= (flat preview)
 *      &sun=deg (Sun later/earlier, 0.51 deg per hour)  &scale=0.8..1.4 (VR resolution)  &still (no motion)
 */
import * as THREE from 'three';
import { loadMoon } from './moon.js?v=4';
import { makeSky, makeRocks, makeRail } from './extras.js?v=5';
import { makeLMModel } from './vehicle.js?v=34';
import { PAD_DROP } from './descent-space.js?v=34';
import { makeLMShadow } from './lm-shadow.js?v=34';
import { makePrints } from './prints.js?v=4';
import { makeSuitSound } from './sound.js?v=3';
import { makeGround } from './ground.js?v=3';
import { makeControllers, createHand } from './hands.js?v=34';
import { makeWrist, makeToast, makeCard } from './wrist.js?v=34';
import { makeLMCabin, LMC } from './lm-cabin.js?v=34';
import { makeEVASuit, suitEnvironment, makeHelmetView } from './eva-suit.js?v=34';
import { makeLMP, makeFigureShadow, makeDust } from './lmp.js?v=34';
import { makeSurfaceOps } from './surface-ops.js?v=34';
import { makeConsole } from './finale.js?v=34';
import { makeMoonRadio } from './moon-radio.js?v=34';
import { LM } from './lm-model.js?v=34';
import { makeEgress } from './egress.js?v=34';
import { settleLM } from './lm-model.js?v=34';
import { settings, applyCommon } from './settings.js?v=34';
import { continueLink, autoEnter, saveProgress, loadProgress } from './progress.js?v=34';

const Q = new URLSearchParams(location.search);
const num = (k, d) => (Q.has(k) && Q.get(k) !== '' && isFinite(+Q.get(k)) ? +Q.get(k) : d);
const $ = id => document.getElementById(id);
const BASE = new URL('../assets/moon/', import.meta.url).href;
const FAR = 3.0e6;
/* surface.html is chapter 6, On the Moon: this page with the surface only, starting in the landed LM (egress.js) */
const CH6 = document.body.dataset.chapter === '6';

const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance', stencil: false });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.setSize(innerWidth, innerHeight);
renderer.setClearColor(0x000000, 1);
renderer.autoClear = false;
renderer.info.autoReset = false;
renderer.xr.enabled = true;
renderer.xr.cameraAutoUpdate = false;          /* we update the XR cameras ourselves, to set near and far per pass */
renderer.xr.setReferenceSpaceType('local-floor');
$('stage').appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, innerWidth / innerHeight, 0.05, 1000);
const rig = new THREE.Group();                 /* the viewer's floor: position and orientation in the site frame */
const head = new THREE.Group();                /* flat preview only: eye height and mouse look */
rig.add(head); head.add(camera); scene.add(rig);
addEventListener('resize', () => {
  if (renderer.xr.isPresenting) return;
  camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); renderer.setSize(innerWidth, innerHeight);
});

/* ---------------------------------------------------------------- state */
const S = {
  view: Q.get('view') || 'surface',
  theta: null,                 /* degrees along the ground track, positive = before the site (east of it) */
  still: Q.has('still'),
  yaw: num('yaw', 0), pitch: num('pitch', 0),
  sunOffset: num('sun', 0),
  pos: new THREE.Vector3(6, 0, 14), heading: 0,      /* on the surface: metres from the LM, heading in radians */
  fps: 0, ms: 0, calls: 0, tris: 0, foveation: 1, ready: false, frames: 0, dark: false
};
const VIEWS = {
  orbit: { alt: 110e3, from: 13, to: -7, tilt: 34, label: 'Lunar orbit, 110 km' },
  low: { alt: 15e3, from: 6, to: -3.5, tilt: 24, label: 'Low pass, 15 km' },
  surface: { label: 'Taurus-Littrow, on the surface' }
};
const EXPOSURE = num('exposure', 3.4);
const HEADING = -82 * Math.PI / 180;       /* ground track: travelling west, a little north (Apollo 17's approach) */
const GM = 4.9028e12;

let moon, rocks, lm, rail, sky, hud, prints, ground, lmSun = null, lmShadow = null, lmShadowSun = -9;
/* out of the LM (egress.js, ?egress): the model, its cabin, your hands, a message in front of you, a black veil */
/* The other astronaut's visor mirrors the valley, the LM and you: a cube map drawn from his helmet, one face every
 * other frame (a full turn in a sixth of a second), in the same two passes as the view itself (far, then near).
 * In it you are a whole suit (meGhost: helmet and arms, which follow your hands); it is the suit your shadow is
 * drawn from too. */
const cubeRT = new THREE.WebGLCubeRenderTarget(256), cubeCam = new THREE.CubeCamera(0.15, 40e3, cubeRT);
let reflFace = 0, reflTick = 0, reflEvery = 2, reflCount = 0;
function reflect(show) {
  const face = reflFace; reflFace = (reflFace + 1) % 6; reflCount++;
  if (cubeCam.coordinateSystem !== renderer.coordinateSystem) { cubeCam.coordinateSystem = renderer.coordinateSystem; cubeCam.updateCoordinateSystem(); }
  lmp.fig.userData.helmet.localToWorld(cubeCam.position.set(0, 0.785, 0)); cubeCam.updateMatrixWorld(true);
  const cam = cubeCam.children[face], prev = renderer.getRenderTarget(), xrOn = renderer.xr.enabled;
  renderer.xr.enabled = false; renderer.setRenderTarget(cubeRT, face); renderer.clear();
  show(true, false, false); cam.near = 100; cam.far = FAR; cam.updateProjectionMatrix(); renderer.render(scene, cam);
  renderer.clearDepth();
  show(false, true, false); rocks.group.visible = false; prints.group.visible = false; lmp.fig.visible = false; me.visible = false; meGhost.visible = true;
  cam.near = 0.15; cam.far = 40e3; cam.updateProjectionMatrix(); renderer.render(scene, cam);
  meGhost.visible = false;
  renderer.xr.enabled = xrOn; renderer.setRenderTarget(prev);
}
/* What you cannot pass through: the LM's outside as seen from above (its own frame, metres: the descent stage, the
 * four legs with their struts and pads; the ladder is on the front leg) and the other astronaut. Your body is a
 * circle under your head; where it would enter, it is pushed out the shortest way, so you slide along. */
const BODY = 0.3;
const LM_SOLID = (() => {
  const circ = [[0, 0, 2.25]], segs = [];
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2, sa = Math.sin(a), ca = Math.cos(a), P = (rho, tau) => [sa * rho + ca * tau, ca * rho - sa * tau];
    segs.push([P(2.1, 0), P(4.3, 0), 0.14]); circ.push([...P(4.3, 0), 0.5]);
    for (const t of [-1, 1]) segs.push([P(2.6, t * 1.02), P(3.76, 0), 0.08]);
  }
  return { circ, segs };
})();
const _cl = new THREE.Vector3();
function pushOut(p, cx, cz, R) { const dx = p.x - cx, dz = p.z - cz, d = Math.hypot(dx, dz); if (d >= R) return; if (d < 1e-4) { p.x = cx + R; return; } p.x = cx + dx / d * R; p.z = cz + dz / d * R; }
/* a point on the ground (world x, z) moved out of whatever it is in; the result in _cl */
function clearOf(x, z, body = BODY) {
  /* (on the ground's plane, turned by the LM's heading only: its slight tilt has no business here) */
  const dx = x - lmModel.position.x, dz = z - lmModel.position.z, c = Math.cos(lmModel.rotation.y), sn = Math.sin(lmModel.rotation.y);
  _cl.set(x, 0, z);
  if (Math.hypot(dx, dz) < 6) {
    _cl.set(dx * c - dz * sn, 0, dx * sn + dz * c);
    for (let n = 0; n < 2; n++) {
      for (const [cx, cz, r] of LM_SOLID.circ) pushOut(_cl, cx, cz, r + body);
      for (const [[ax, az], [bx, bz], w] of LM_SOLID.segs) {
        const ux = bx - ax, uz = bz - az, t = Math.max(0, Math.min(1, ((_cl.x - ax) * ux + (_cl.z - az) * uz) / (ux * ux + uz * uz)));
        pushOut(_cl, ax + ux * t, az + uz * t, w + body);
      }
    }
    _cl.set(lmModel.position.x + _cl.x * c + _cl.z * sn, 0, lmModel.position.z - _cl.x * sn + _cl.z * c);
  }
  if (ops && body <= BODY && !ops.driving) pushOut(_cl, ops.R.x, ops.R.z, 1.15 + body);          /* the rover, parked */
  if (ops) for (const [bx, bz, br] of ops.solids) if (Math.abs(_cl.x - bx) < 4 && Math.abs(_cl.z - bz) < 4) pushOut(_cl, bx, bz, br + body);   /* the boulders at station 1 */
  if (lmp && lmp.on && ['pad', 'off', 'roam'].includes(lmp.S.phase)) { const bx = _cl.x, bz = _cl.z; pushOut(_cl, lmp.S.pos.x, lmp.S.pos.z, 0.45 + BODY); if (bx !== _cl.x || bz !== _cl.z) mradio.bump(); }
  return _cl;
}
/* keep your head's column clear (your head is where you are, wherever in your room you stand) */
function collide() {
  const c = Math.cos(S.heading), sn = Math.sin(S.heading), ox = camera.position.x * c + camera.position.z * sn, oz = -camera.position.x * sn + camera.position.z * c;
  const hx = S.pos.x + ox, hz = S.pos.z + oz; clearOf(hx, hz);
  S.pos.x += _cl.x - hx; S.pos.z += _cl.z - hz;
}
/* the whole suit's arms to where your hands are (two bones: the elbow bends down, out and back) */
const _iT = new THREE.Vector3(), _id = new THREE.Vector3(), _ie = new THREE.Vector3(), _iu = new THREE.Vector3(), _if = new THREE.Vector3(), _ix = new THREE.Vector3(), _iy = new THREE.Vector3(), _iz = new THREE.Vector3(), _im = new THREE.Matrix4();
/* (a hand the headset has lost sight of, above or behind your head, keeps its last pose for two seconds rather than
 * dropping to your side at once: a wave with the arms right up must not be lost from the film) */
const armLost = { left: 9, right: 9 };
function ghostArms() {
  const J = meGhost.userData.joints, waist = meGhost.userData.waist, A = 0.29, B = 0.36;
  meGhost.updateMatrixWorld(true);
  for (const [h, arm, elbow, sd] of [['left', J.armL, J.elbowL, -1], ['right', J.armR, J.elbowR, 1]]) {
    const p = renderer.xr.isPresenting ? EH[h].pos : null;
    if (!p) { armLost[h] += frameDt; if (armLost[h] > 2) { arm.rotation.set(0.35, 0, sd * 0.3); elbow.rotation.set(0.6, 0, 0); } continue; }
    armLost[h] = 0;
    _iT.copy(p); rig.localToWorld(_iT); waist.worldToLocal(_iT);
    _id.subVectors(_iT, arm.position); const L = Math.max(0.12, Math.min(A + B - 0.01, _id.length())); _id.normalize();
    const al = Math.acos(Math.max(-1, Math.min(1, (A * A + L * L - B * B) / (2 * A * L))));
    _ie.set(sd * 0.35, -1, 0.45); _ie.addScaledVector(_id, -_ie.dot(_id)).normalize();
    _iu.copy(_id).multiplyScalar(Math.cos(al)).addScaledVector(_ie, Math.sin(al));                 /* the upper arm */
    _if.copy(_id).multiplyScalar(L).addScaledVector(_iu, -A).normalize();                           /* the forearm */
    const c = Math.max(-1, Math.min(1, _if.dot(_iu)));
    _iy.copy(_iu).negate(); _iz.copy(_if).addScaledVector(_iu, -c); if (_iz.lengthSq() < 1e-6) _iz.set(0, 0, -1); _iz.normalize().negate();
    _ix.crossVectors(_iy, _iz);
    arm.quaternion.setFromRotationMatrix(_im.makeBasis(_ix, _iy, _iz)); elbow.rotation.set(Math.acos(c), 0, 0);
  }
}
const helm = makeHelmetView(); helm.group.visible = false; let helmYaw = null;
let me = null, handFill = null, lmp = null, meShadow = null, meGhost = null, meBase = null;
let lmModel = null, lmHemi = null, lmc = null, egress = null, toast = null, card = null;
const hullMats = new Set();                 /* the LM's outside materials (drawn to depth only in the cabin's pass) */
/* the surface suit's gloves: grey woven steel cloth, the wrist rings anodised blue (left) and red (right) */
const hands = { left: createHand('left', { color: 0x8f9296, cuff: 0x3f5f9a }), right: createHand('right', { color: 0x8f9296, cuff: 0xb3202a }) }, EH = { left: { grip: false, pos: null }, right: { grip: false, pos: null } };
const fadeScene = new THREE.Scene();
const veil = new THREE.Mesh(new THREE.PlaneGeometry(4, 4), new THREE.MeshBasicMaterial({ color: 0x000000, transparent: true, opacity: 0, depthTest: false, depthWrite: false }));
fadeScene.add(veil);
const _eye = new THREE.Vector3(), _eu = new THREE.Euler();
const suit = makeSuitSound();
const OPT = { teleport: true, fan: true };
const common = applyCommon(renderer, null);
continueLink(document.querySelector('a[href="launch.html"]'));      /* on from where you were in the chapters */
settings.onChange(k => { if (k === 'fov') S.foveation = settings.get('fov'); });   /* the frame-rate guard goes on from there */

/* A point above the Moon at `theta` degrees along the track from the site, in the site frame, with its axes. */
const _c = new THREE.Vector3(), _u = new THREE.Vector3(), _f = new THREE.Vector3(), _r = new THREE.Vector3();
const _m = new THREE.Matrix4(), _q = new THREE.Quaternion();
function orbitFrame(theta, alt, tiltDeg) {
  const man = moon.manifest, R = man.radius;
  /* travel direction on the ground at the site (unit, horizontal): heading measured from north towards east */
  const dir = new THREE.Vector3(Math.sin(HEADING), 0, -Math.cos(HEADING));
  const a = -theta * Math.PI / 180;                     /* angle travelled past the site */
  _u.set(0, 1, 0).multiplyScalar(Math.cos(a)).addScaledVector(dir, Math.sin(a));          /* local up there */
  _f.copy(dir).multiplyScalar(Math.cos(a)).addScaledVector(new THREE.Vector3(0, 1, 0), -Math.sin(a));
  _c.set(0, -man.r0, 0).addScaledVector(_u, R + alt);
  /* tilt the viewer's floor forward so the ground ahead fills the view, as through a forward-down window */
  const t = tiltDeg * Math.PI / 180;
  const up2 = _u.clone().multiplyScalar(Math.cos(t)).addScaledVector(_f, Math.sin(t));
  const fw2 = _f.clone().multiplyScalar(Math.cos(t)).addScaledVector(_u, -Math.sin(t));
  _r.crossVectors(fw2, up2).normalize();
  _m.makeBasis(_r, up2, fw2.clone().negate());
  rig.position.copy(_c); rig.quaternion.setFromRotationMatrix(_m);
}

function setView(name, theta) {
  if (!VIEWS[name]) name = 'surface';
  S.view = name;
  const v = VIEWS[name];
  S.theta = name === 'surface' ? 0 : (theta ?? v.from);
  if (rail) rail.visible = name !== 'surface';
  if ($('viewname')) $('viewname').textContent = v.label;
}

/* ---------------------------------------------------------------- input */
const keys = {};
addEventListener('keydown', e => {
  keys[e.code] = true;
  if (!CH6) { if (e.code === 'Digit1') setView('orbit'); if (e.code === 'Digit2') setView('low'); if (e.code === 'Digit3') setView('surface'); }
  if (e.code === 'KeyP') S.still = !S.still;
  if (e.code === 'KeyF' && finaleOn && ops.F.end) { location.href = 'liftoff.html'; return; }
  if (e.code === 'KeyG' && ops && S.view === 'surface' && !egress.active) ops.key(headGround(new THREE.Vector3()));
  if (e.code === 'KeyF' && ops && ops.button('a')) { /* the rover, or ready for the camera */ } else
  if (e.code === 'KeyF' && egress && egress.active) egress.go(); else if (e.code === 'KeyF' && egress && atLadder()) mountLadder();
  if (e.code === 'BracketRight') { S.sunOffset += 1; moon.setSunOffset(S.sunOffset); }
  if (e.code === 'BracketLeft') { S.sunOffset -= 1; moon.setSunOffset(S.sunOffset); }
});
addEventListener('keyup', e => { keys[e.code] = false; });
let drag = null;
renderer.domElement.addEventListener('pointerdown', e => { drag = [e.clientX, e.clientY]; renderer.domElement.setPointerCapture(e.pointerId); });
renderer.domElement.addEventListener('pointerup', () => { drag = null; });
renderer.domElement.addEventListener('pointermove', e => {
  if (!drag) return;
  S.yaw -= (e.clientX - drag[0]) * 0.2; S.pitch = Math.max(-89, Math.min(89, S.pitch - (e.clientY - drag[1]) * 0.2));
  drag = [e.clientX, e.clientY];
});

const pad = { prev: {}, snap: 0 };
/* the radio on the Moon (moon-radio.js): hold X (T on the screen) to transmit */
const mradio = makeMoonRadio({ get toast() { return toast; }, level: () => settings.get('voices'), onLMP: () => lmp && lmp.hail() });
let ops = null, opsStick = [0, 0], meSeated = false;
/* The lift-off and the film (surface-ops.js, finale.js). The ascent stage is the LM's model cut off below its
 * joint with a clipping plane that rises with it; a second model, cut off above, stays behind as the descent stage.
 * The film is the scene drawn from the rover's camera into a texture, in the view's own two passes, and shown on
 * the console's tube. */
/* (not multisampled: the film is two passes into it, far then near, and a multisampled target's picture is given up
 * between them in the headset: the first pass, the mountains and the sky, was lost) */
const filmRT = new THREE.WebGLRenderTarget(800, 600), filmCam = new THREE.PerspectiveCamera(30, 4 / 3, 0.3, 40e3);
const con = makeConsole(filmRT.texture); con.group.visible = false;
let finaleOn = false, finaleFade = 0, rigLift = 0, lmDescent = null, lmBaseY = 0, ghostBake = 0, endSaid = false;
const clipAsc = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0), clipDesc = new THREE.Plane(new THREE.Vector3(0, -1, 0), 0);
/* the LM's shadow: whole while it stands; once the ascent stage is off, two (the descent stage's, which stays, and
 * the ascent stage's, which runs off down-Sun as it climbs) */
let shAsc = null, shDesc = null, lmSplit = false;
const shadowVis = v => { lmShadow.mesh.visible = v && !lmSplit; if (shAsc) shAsc.mesh.visible = shDesc.mesh.visible = v && lmSplit; };
/* The cut between the stages, in the LM's own frame: it stands a little tilted on the ground, and a level cut took a
 * wedge of the descent stage's deck up with the ascent stage (the "floor boards" in the film: owner's headset test). */
const _cn = new THREE.Vector3(), _cp = new THREE.Vector3(), _cq = new THREE.Vector3();
function setClips() {
  lm.updateMatrixWorld(true);
  _cn.set(0, 1, 0).transformDirection(lmDescent.matrixWorld);
  clipDesc.setFromNormalAndCoplanarPoint(_cq.copy(_cn).negate(), lmDescent.localToWorld(_cp.set(0, LM.AS_Y - 0.01, 0)));
  clipAsc.setFromNormalAndCoplanarPoint(_cn, lmModel.localToWorld(_cp.set(0, LM.AS_Y - 0.01, 0)));
}
function liftModel(alt) {
  if (!lmDescent) {
    lmBaseY = lmModel.position.y;
    lmDescent = makeLMModel(renderer); lmDescent.rotation.order = 'YXZ'; lmDescent.rotation.copy(lmModel.rotation); lmDescent.position.copy(lmModel.position); lm.add(lmDescent);
    renderer.localClippingEnabled = true;
    /* the legs, the ladder and the porch with its rails (the model's 'deployed' group) are the descent stage's, whole:
     * they stay with the copy (their materials its own, uncut) and are not drawn with the ascent stage */
    /* (and so are the plume deflectors that stand on its deck, above the joint; the ascent engine's bell, below it, is
     * the ascent stage's, whole) */
    const whole = (model, groups) => { for (const g of groups) g.traverse(o => { if (o.isMesh) { o.material = o.material.clone(); o.userData.whole = true; } }); };
    whole(lmDescent, [lmDescent.userData.deployed, lmDescent.userData.descentTop]); whole(lmModel, [lmModel.userData.ascentOnly]);
    lmDescent.traverse(o => { if (o.isMesh && !o.userData.whole) o.material.clippingPlanes = [clipDesc]; });
    lmModel.traverse(o => { if (o.isMesh && !o.userData.whole) o.material.clippingPlanes = [clipAsc]; });
    lmModel.userData.deployed.visible = false; lmModel.userData.descentTop.visible = false; lmDescent.userData.ascentOnly.visible = false;
    /* each stage's own silhouette, drawn once, here, while the ascent stage still stands on the other */
    lmModel.position.y = lmBaseY; setClips();
    shDesc = makeLMShadow(moon); shAsc = makeLMShadow(moon); lm.add(shDesc.mesh, shAsc.mesh);
    shDesc.bake(renderer, lmDescent, moon.shared.uSun.value, { clip: [clipDesc], read: false });
    shAsc.bake(renderer, lmModel, moon.shared.uSun.value, { clip: [clipAsc], read: false, nx: 12, ny: 52 });
  }
  lmModel.position.y = lmBaseY + alt; lmc.group.position.y = lmBaseY + alt; setClips();
  lmSplit = alt > 0.004; shadowVis(true); if (lmSplit) shAsc.lift(alt);
}
function filmPass(show) {
  const F = ops.F, prev = renderer.getRenderTarget(), xrOn = renderer.xr.enabled;
  filmCam.position.copy(F.cam); filmCam.up.set(0, 1, 0); filmCam.lookAt(F.look); filmCam.fov = F.fov; filmCam.updateMatrixWorld(true);
  renderer.xr.enabled = false; renderer.setRenderTarget(filmRT); renderer.clear();
  show(true, false, false); filmCam.near = 100; filmCam.far = FAR; filmCam.updateProjectionMatrix(); renderer.render(scene, filmCam);
  renderer.clearDepth();
  show(false, true, false); me.visible = false; lmp.fig.visible = lmp.extras.visible = F.actors; meGhost.visible = F.actors; meShadow.mesh.visible = F.actors;
  ops.rover.visible = !!F.rover;                               /* (on the rover's own camera its dish and seats are not in the picture) */
  filmCam.near = 0.3; filmCam.far = 40e3; filmCam.updateProjectionMatrix(); renderer.render(scene, filmCam);
  meGhost.visible = false; ops.rover.visible = true;
  renderer.xr.enabled = xrOn; renderer.setRenderTarget(prev);
}
let pttHeld = false, walkMoved = 0, walkD = 0, hopY = 0, tpWarn = 0, mountHint = 0;
const _pd = new THREE.Vector3();
/* the controllers shake: once (a bump), and a steady low tremor while the rover runs (roverBuzz, renewed 12 times a second) */
let roverBuzz = 0, buzzT = 0;
function buzz(k, ms, hand) {
  const s = renderer.xr.getSession(); if (!s || settings.get('haptics') === false) return;
  for (const src of s.inputSources) { if (hand && src.handedness !== hand) continue; const h = src.gamepad && src.gamepad.hapticActuators && src.gamepad.hapticActuators[0]; if (h && h.pulse) { try { h.pulse(Math.min(1, k), ms).catch(() => {}); } catch (e) { /* no actuator */ } } }
}
/* onto the ladder, unless your crewmate is on his way up it */
function mountLadder() {
  if (ops && !ops.ladderFree) { toast.say('Your crewmate goes up first: the ladder is yours when he is in', 4, true); return; }
  egress.mount();
}
/* your gloves in the LM's shadow: lit by the ground's glow only, as the rest of your suit is (each glove as a whole,
 * from where it is: two looks into the shadow's own picture a frame, nothing drawn for it) */
const _hw = new THREE.Vector3();
function dimHands(dt) {
  for (const k of ['left', 'right']) {
    const h = hands[k]; if (!h || !h.parent) continue;
    let want = 1;
    if (S.view === 'surface' && lmShadow && !finaleOn && !(egress.active && ['cabin', 'turn', 'in'].includes(egress.E.stage))) want = lmShadow.lit(h.getWorldPosition(_hw));
    const u = h.userData; u.lit = (u.lit ?? 1) + (want - (u.lit ?? 1)) * Math.min(1, dt * 8);
    const fk = 0.3 + 0.7 * u.lit;
    if (Math.abs(fk - (u.litF ?? 1)) < 0.004 && u.litM === u.model) continue;
    u.litF = fk; u.litM = u.model;
    h.traverse(m => { const mt = m.isMesh && m.material; if (!mt || !mt.color) return; if (!mt.userData.base) mt.userData.base = mt.color.clone(); mt.color.copy(mt.userData.base).multiplyScalar(fk); });
  }
}
/* within d metres (on the ground) of the LM, or of the rover, the flag or your crewmate */
function nearThings(d) {
  if (Math.hypot(_cam.x - lmModel.position.x, _cam.z - lmModel.position.z) < d) return true;
  return !!ops && ops.things.some(([x, z]) => Math.hypot(_cam.x - x, _cam.z - z) < d);
}
/* by the front footpad (within reach of the ladder), on the ground */
function atLadder() {
  if (!CH6 || !egress || egress.active || egress.E.stage !== 'done') return false;
  lmModel.localToWorld(_pd.set(0, 0, LM.FOOT_R + 0.6));
  return Math.hypot(_cam.x - _pd.x, _cam.z - _pd.z) < 1.5;
}
let ctl = null;                                      /* the controllers by hand (hands.js), made with the session */
function readControllers(dt) {
  const s = renderer.xr.getSession();
  if (!s) return;
  let mx = 0, mz = 0, turn = 0, reach = 0, next = false, toggle = false, trig = false, trigEdge = false, grab = false, ptt = false, aPress = false, grabHand = null;
  for (const src of s.inputSources) {
    const gp = src.gamepad; if (!gp) continue;
    if (src.handedness === 'right') trig = !!gp.buttons[0]?.pressed;
    const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
    if (src.handedness === 'left') { hud.stick(ax[0], ax[1]); if (!hud.capturesStick) { mx = ax[0]; mz = ax[1]; } } else { turn = ax[0]; reach = -ax[1]; }
    const k = src.handedness;
    /* A (right): the next view; left stick press: the panel, then the settings (as in the chapters) */
    const a = gp.buttons[4]?.pressed, st = gp.buttons[3]?.pressed;
    if (a && !pad.prev[k + 'a'] && k === 'right' && finaleOn && ops.F.end) { location.href = 'liftoff.html'; return; }
    if (a && !pad.prev[k + 'a'] && k === 'right') { aPress = true; if (egress.active) egress.go(); else if (!CH6) next = true; }
    /* The triggers and the grips both take hold (owner's wish: the pistol triggers grip and work things whenever
     * you are close to something: the flag's pole, a rock, the rover, the ladder; they jump only when you are well
     * clear of everything) */
    const tg = (gp.buttons[0]?.value || 0) > 0.5, sq = (gp.buttons[1]?.value || 0) > 0.5;
    if ((tg || sq) && !pad.prev[k + 'g']) { grab = true; grabHand = EH[k]; if (tg && k === 'right') trigEdge = true; } pad.prev[k + 'g'] = tg || sq;
    if (ops && !sq && !tg) ops.release(EH[k]);
    if (k === 'left' && gp.buttons[4]?.pressed) ptt = true;
    EH[k].grip = sq || tg; EH[k].pos = ctl.grips[k].visible ? ctl.grips[k].position : null;
    if (hands[k]) { if (hands[k].parent !== ctl.grips[k]) ctl.grips[k].add(hands[k]); hands[k].userData.pose(gp.buttons[0]?.value || 0, gp.buttons[1]?.value || 0, [3, 4, 5].some(n => gp.buttons[n]?.touched), false, dt); }
    if (st && !pad.prev[k + 's'] && k === 'left') hud.press();
    pad.prev[k + 'a'] = a; pad.prev[k + 's'] = st;
  }
  pttHeld = ptt;
  if (egress.active) { aimTeleport(null); return; }               /* climbing out: no walking, teleport or turning yet */
  /* on the rover the stick drives and A steps off; walking, a grip takes the flag's pole or a seat on the rover */
  if (ops && ops.driving) { opsStick = [mx, mz]; if (aPress) ops.button('a'); aimTeleport(null); return; }
  if (ops && aPress && ops.button('a')) { aimTeleport(null); return; }                /* ready for the camera */
  if (ops && grab && ops.grab(grabHand, headGround(new THREE.Vector3()))) { aimTeleport(null); return; }
  /* by the footpad a grip or a trigger takes you onto the ladder */
  if (grab && atLadder()) { mountLadder(); aimTeleport(null); return; }
  /* within 10 m of the LM, the rover, the flag or your crewmate, or with something in a hand, the triggers do not
   * jump (owner's wish); a pull that took hold of nothing says why */
  if (trig && CH6 && S.view === 'surface' && (nearThings(10) || (ops && ops.busy))) {
    if (trigEdge && !(ops && ops.busy) && performance.now() > tpWarn) { tpWarn = performance.now() + 6000; toast.say('Cannot teleport close to other objects or astronauts', 4, true); }
    trig = false;
  }
  if (next) { const names = Object.keys(VIEWS); setView(names[(names.indexOf(S.view) + 1) % names.length]); }
  TP.reach += (reach - TP.reach) * Math.min(1, dt * 6);
  aimTeleport(trig && OPT.teleport && S.view === 'surface' && !TP.blink && ctl.rays.right.visible ? ctl.rays.right : null);
  /* snap turn, 30 degrees */
  if (Math.abs(turn) > 0.7 && !pad.snap) { S.heading -= Math.sign(turn) * Math.PI / 6; pad.snap = 1; }
  if (Math.abs(turn) < 0.3) pad.snap = 0;
  if (S.view === 'surface' && (Math.abs(mx) > 0.15 || Math.abs(mz) > 0.15)) {
    /* walk where the head looks */
    const d = new THREE.Vector3(); camera.getWorldDirection(d); d.y = 0; d.normalize();
    const r = new THREE.Vector3(-d.z, 0, d.x), sp = 1.5 * dt;
    S.pos.addScaledVector(d, -mz * sp).addScaledVector(r, mx * sp);
    walkMoved += Math.hypot(mx, mz) * sp;
  }
}

/* ---------------------------------------------------------------- teleport
 * Hold the right trigger: an arc thrown from the hand, falling in lunar gravity, shows where you would stand. Push
 * the right stick forward to throw further (out to the mountains), pull it back for a short hop. Let go: the view
 * blinks out, the ground round the new place is made if it is not there yet, and you stand there. */
const TP = { aiming: false, valid: false, target: new THREE.Vector3(), reach: 0, blink: 0, phase: null, pending: null, waited: 0 };
const ARC_N = 96;
const arcLine = new THREE.Line(new THREE.BufferGeometry().setAttribute('position', new THREE.BufferAttribute(new Float32Array(ARC_N * 3), 3)),
  new THREE.LineBasicMaterial({ color: 0xd8d4ca, transparent: true, opacity: 0.55, depthTest: false }));
arcLine.frustumCulled = false; arcLine.visible = false; arcLine.renderOrder = 30;
const marker = new THREE.Group();
marker.add(new THREE.Mesh(new THREE.RingGeometry(0.24, 0.27, 48).rotateX(-Math.PI / 2),
  new THREE.MeshBasicMaterial({ color: 0xe8e4da, transparent: true, opacity: 0.7, depthWrite: false, depthTest: false })));
marker.add(new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), new THREE.Vector3(0, 2.2, 0)]),
  new THREE.LineBasicMaterial({ color: 0xe8e4da, transparent: true, opacity: 0.6, depthTest: false })));
marker.visible = false; marker.children.forEach(c => { c.renderOrder = 31; c.frustumCulled = false; });
const _o = new THREE.Vector3(), _d = new THREE.Vector3(), _p = new THREE.Vector3(), _v = new THREE.Vector3(), _pp = new THREE.Vector3();
const G_MOON = 1.62;
function walkLimit() { return ground && ground.ok ? ground.limit : (moon.manifest.walk ? moon.manifest.walk.half - 3 : 100); }
function aimTeleport(hand) {
  if (TP.aiming && !hand) {                          /* released */
    TP.aiming = false; marker.visible = false;
    if (TP.valid) startTeleport(TP.target.x, TP.target.z);
    return;
  }
  if (!hand) return;
  TP.aiming = true;
  hand.getWorldPosition(_o); _d.set(0, 0, -1).transformDirection(hand.matrixWorld);
  _v.copy(_d).multiplyScalar(30 * Math.pow(3, TP.reach));      /* 10 .. 90 m/s: tens of metres to kilometres */
  const lim = walkLimit(), pts = [];
  let hit = false, travelled = 0;
  _p.copy(_o);
  for (let k = 0; k < 900; k++) {
    pts.push(_p.x, _p.y, _p.z);
    _pp.copy(_p);
    const ds = Math.min(40, Math.max(0.4, 0.015 * travelled)), dtk = ds / Math.max(1, _v.length());
    _p.addScaledVector(_v, dtk); _v.y -= G_MOON * dtk; travelled += ds;
    if (Math.abs(_p.x) > lim || Math.abs(_p.z) > lim) break;
    if (_p.y < moon.heightAt(_p.x, _p.z)) {
      for (let q = 0; q < 6; q++) {                  /* close in on the ground between the last two points */
        _d.copy(_pp).add(_p).multiplyScalar(0.5);
        if (_d.y < moon.heightAt(_d.x, _d.z)) _p.copy(_d); else _pp.copy(_d);
      }
      hit = true; pts.push(_p.x, _p.y, _p.z);
      break;
    }
  }
  const pos = arcLine.geometry.attributes.position.array, n = pts.length / 3;
  for (let i = 0; i < ARC_N; i++) {
    const k = Math.min(n - 1, Math.round(i / (ARC_N - 1) * (n - 1)));
    pos[i * 3] = pts[k * 3]; pos[i * 3 + 1] = pts[k * 3 + 1]; pos[i * 3 + 2] = pts[k * 3 + 2];
  }
  arcLine.geometry.attributes.position.needsUpdate = true;
  TP.valid = hit;
  if (hit) {
    TP.target.set(_p.x, moon.heightAt(_p.x, _p.z), _p.z);
    marker.position.copy(TP.target); marker.position.y += 0.02;
    marker.scale.setScalar(Math.max(1, _o.distanceTo(TP.target) / 30));     /* a far target stays visible */
    arcLine.material.color.setHex(0xd8d4ca);
  } else arcLine.material.color.setHex(0x8a5a50);
}
/* blink out, make sure the ground is there, move, blink in */
function startTeleport(x, z) {
  if (TP.blink) return;
  const lim = walkLimit();
  TP.pending = { x: Math.max(-lim, Math.min(lim, x)), z: Math.max(-lim, Math.min(lim, z)), ready: false };
  TP.blink = 0.0001; TP.phase = 'out'; TP.waited = 0;
}
function stepBlink(dt) {
  if (!TP.blink) return;
  const T = 0.12;
  TP.blink += dt;
  let f = 0;
  if (TP.phase === 'out') {
    f = 1 - TP.blink / T;
    if (TP.blink >= T) {
      TP.phase = 'wait'; f = 0;
      const p = TP.pending;
      if (ground.ok && !ground.detailed(p.x, p.z)) ground.request(p.x, p.z).then(() => { p.ready = true; });
      else p.ready = true;
    }
  } else if (TP.phase === 'wait') {
    TP.waited += dt;
    if (TP.pending.ready || TP.waited > 8) { teleportTo(TP.pending.x, TP.pending.z); TP.pending = null; TP.phase = 'in'; TP.blink = 0.0001; }
  } else {
    f = TP.blink / T;
    if (TP.blink >= T) { TP.blink = 0; TP.phase = null; f = 1; }
  }
  moon.shared.uExposure.value = EXPOSURE * Math.max(0, Math.min(1, f));
  S.dark = f < 0.5;
}
/* stand at (x, z): the rig moves so that your head, not the middle of your play area, ends up over the spot */
function teleportTo(x, z) {
  const head = headGround(new THREE.Vector3());
  clearOf(x, z); x = _cl.x; z = _cl.z;                       /* not into the LM, nor onto the other astronaut */
  S.pos.x += x - head.x; S.pos.z += z - head.z;
  const lim = walkLimit();
  S.pos.x = Math.max(-lim, Math.min(lim, S.pos.x)); S.pos.z = Math.max(-lim, Math.min(lim, S.pos.z));
  STEP.last = null;
  /* where you land you stand: two prints side by side */
  const f = new THREE.Vector3(); camera.getWorldDirection(f); f.y = 0;
  if (f.lengthSq() < 1e-6) f.set(0, 0, -1);
  f.normalize();
  prints.add(x - f.z * 0.12, z + f.x * 0.12, f.x, f.z, 1); prints.add(x + f.z * 0.12, z - f.x * 0.12, f.x, f.z, -1);
  suit.step(0.7);
}

/* ---------------------------------------------------------------- steps: a print and a sound every stride */
const STEP = { last: null, side: 1, stride: 0.82 };
const _h = new THREE.Vector3();
/* the head in the site frame. In XR, renderer.xr.updateCamera has written the headset pose into `camera`;
 * the XR array camera itself has no meaningful world position to read. */
function headGround(out) {
  out.setFromMatrixPosition(camera.matrixWorld);
  out.y = 0;
  return out;
}
function trackSteps() {
  if (S.view !== 'surface' || egress.active) { STEP.last = null; return; }
  headGround(_h);
  if (!STEP.last) { STEP.last = _h.clone(); return; }
  const dx = _h.x - STEP.last.x, dz = _h.z - STEP.last.z, d = Math.hypot(dx, dz);
  if (d > 3) { STEP.last.copy(_h); return; }
  if (d < STEP.stride) return;
  const ux = dx / d, uz = dz / d;
  STEP.side = -STEP.side;
  prints.add(_h.x - uz * STEP.side * 0.11, _h.z + ux * STEP.side * 0.11, ux, uz, STEP.side);
  suit.step();
  STEP.last.copy(_h);
}

/* ---------------------------------------------------------------- the wrist panel (X): status and settings */
function info() {
  if (egress.active) return { chapter: 'Chapter 6 \u00b7 On the Moon', now: 'Leaving the Lunar Module', next: null, after: 'the walk on the Moon', get: 0, warp: 1,
    rows: [['Where', { cabin: 'in the cabin', turn: 'at the hatch', path: 'on the way down', drop: 'the last drop', pad: 'on the footpad' }[egress.E.stage] || '']],
    keys: [['grip, trigger', 'pull yourself along'], ['A', 'go by itself / stop'], ['L stick', 'press: panel, settings']], fps: S.fps };
  if (CH6) return { chapter: 'Chapter 6 \u00b7 On the Moon', now: 'Walking in the valley of Taurus-Littrow', next: null, after: ops && ops.driving ? 'A, standing still: step off' : 'the flag, the rover, the samples, the film, home', get: 0, warp: 1,
    rows: [['From the LM', Math.hypot(S.pos.x, S.pos.z).toFixed(0) + ' m'], ...(ops ? ops.status() : [])],
    keys: [['L stick', 'walk'], ['R stick', 'turn 30 deg'], ['trigger, grip', 'take hold: flag, rover, rocks, ladder'], ['R trigger', 'jump, when 10 m clear of things'], ['X', 'hold: talk on the radio'], ['L stick', 'press: panel, settings']], fps: S.fps };
  return { chapter: 'The Moon \u00b7 Taurus-Littrow', now: VIEWS[S.view].label, next: null, after: S.view === 'surface' ? 'chapter 6 is the way here from the landing' : 'A: the next view', get: 0, warp: 1,
    rows: [['Frame time', S.ms.toFixed(1) + ' ms'], ['Draw calls', String(S.calls)], ['Triangles', (S.tris / 1000).toFixed(0) + 'k'], ['Foveation', S.foveation.toFixed(2)]],
    keys: [['A', 'next view'], ['L stick', 'press: panel, settings'], ['trigger', 'teleport (surface)'], ['L stick', 'walk'], ['R stick', 'turn 30 deg']], fps: S.fps };
}

/* ---------------------------------------------------------------- rendering */
function setPlanes(n, f) {
  const fix = c => {
    const e = c.projectionMatrix.elements;
    e[10] = -(f + n) / (f - n); e[14] = -2 * f * n / (f - n);
    c.projectionMatrixInverse.copy(c.projectionMatrix).invert();
  };
  if (renderer.xr.isPresenting) { const xc = renderer.xr.getCamera(); fix(xc); xc.cameras.forEach(fix); }
  else { camera.near = n; camera.far = f; camera.updateProjectionMatrix(); }
}
const _cam = new THREE.Vector3(); let lastShow = null;
function render() {
  const xr = renderer.xr.isPresenting;
  scene.updateMatrixWorld(true);
  if (xr) renderer.xr.updateCamera(camera);
  _cam.setFromMatrixPosition(camera.matrixWorld);
  rocks.update(_cam);
  moon.setDetail(_cam, S.view === 'surface');
  renderer.info.reset();
  renderer.clear(true, true, false);
  const close = S.view === 'surface';
  const show = (farOn, nearOn, cabinOn) => {
    moon.far.visible = farOn; sky.visible = farOn && !S.dark;
    moon.near.visible = nearOn; rocks.group.visible = nearOn && close; lm.visible = nearOn;
    prints.group.visible = nearOn && close; ground.group.visible = nearOn && close;
    arcLine.visible = TP.aiming && cabinOn && close; marker.visible = TP.aiming && TP.valid && cabinOn && close;
    rail.visible = cabinOn && S.view !== 'surface'; if (hud) hud.mesh.visible = hud.on && cabinOn;
    toast.mesh.visible = cabinOn && toast.k > 0.01; hands.left.visible = hands.right.visible = cabinOn; card.mesh.visible = cabinOn && card.k > 0.01;
    lmc.group.visible = false; handFill.visible = false; helm.group.visible = cabinOn && close && helm.k > 0;
    if (me) { me.visible = nearOn && close && !egress.active; meShadow.mesh.visible = me.visible; }
    if (lmp) { lmp.fig.visible = nearOn && close && lmp.on; lmp.extras.visible = lmp.fig.visible; }
    if (ops) ops.group.visible = nearOn && close;
  };
  lastShow = show;
  /* the LM's cabin, seen from inside and through the open hatch: its own pass with its own lights (they would
   * flood the LM's outside), over the same depth, before the things in your hands */
  const inCabin = close && (egress.active || (egress.E.stage === 'done' && _cam.distanceTo(lmModel.position) < 9));
  if (finaleOn) {
    /* the film into its texture, then the console and nothing else */
    if (ops.F.tf < 1.0) {
      /* the console, a little below your eyes, an arm and a half away, where you look (set while all is dark, from this
       * frame's own view: you are still rising) */
      camera.getWorldDirection(_fw); _fw.y = 0; if (_fw.lengthSq() < 0.01) _fw.set(0, 0, -1); _fw.normalize();
      con.group.position.set(_cam.x + _fw.x * 1.45, _cam.y - 0.12, _cam.z + _fw.z * 1.45); con.group.rotation.y = Math.atan2(-_fw.x, -_fw.z); con.group.updateMatrixWorld(true);
    }
    filmPass(show);
    show(false, false, false); con.group.visible = true; setPlanes(0.05, 60); renderer.clearDepth(); renderer.render(scene, camera); con.group.visible = false;
    if (finaleFade > 0.003) { veil.material.opacity = finaleFade; camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); setPlanes(0.02, 10); renderer.render(fadeScene, camera); }
    S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
    return;
  }
  placeMe(close);
  if (close && lmp && lmp.on && (CH6 || Q.has('egress')) && ++reflTick % reflEvery === 0 && (reflCount < 6 || _cam.distanceTo(lmp.fig.position) < 14)) reflect(show);
  /* your helmet: at your eyes, turned with your body (a little behind your look: you look about inside it) */
  if (close && helm.k > 0) {
    camera.getWorldDirection(_fw); const want = Math.hypot(_fw.x, _fw.z) > 0.2 ? Math.atan2(-_fw.x, -_fw.z) : helmYaw ?? 0;
    if (helmYaw === null) helmYaw = want;
    helmYaw += Math.atan2(Math.sin(want - helmYaw), Math.cos(want - helmYaw)) * 0.1;
    helm.group.position.copy(_cam); helm.group.rotation.y = helmYaw; helm.group.updateMatrixWorld(true);
  }
  if (close) {
    /* standing on the Moon: distant terrain first, then everything within a few kilometres over a fresh depth buffer */
    show(true, false, false); setPlanes(100, FAR); renderer.render(scene, camera);
    renderer.clearDepth();
    if (!inCabin) { show(false, true, true); setPlanes(0.05, 40e3); renderer.render(scene, camera); }
    else {
      /* The cabin and the LM's outside cannot share one lit pass (the cabin's lights would flood the outside), and the
       * headset does not keep the depth buffer from one pass to the next (owner's headset test: the cabin lay over the
       * LM like a lift cage). So: the world with the LM's outside; then, on a fresh depth buffer, the cabin in one pass
       * together with the LM's outside drawn into the depth buffer only (no colour): the hull hides the cabin
       * wherever it is in front of it, and the cabin shows through the windows and the hatch's opening. Inside the
       * cabin its own walls do the hiding. */
      /* outside: anywhere but in the cabin's own box (in front of the hatch, and round the sides and the back too:
       * owner's headset test, the hull was gone seen from the other three sides) */
      lmModel.worldToLocal(_eye.copy(_cam));
      const outside = !(_eye.z < 1.27 && _eye.z > LMC.BACK - 0.25 && Math.abs(_eye.x) < LMC.HALF + 0.25 && _eye.y > LMC.FLOOR - 0.3 && _eye.y < LMC.CEIL + 0.5);
      lmc.setOutside(outside);
      setPlanes(0.05, 40e3);
      show(false, true, false); renderer.render(scene, camera);
      renderer.clearDepth();
      show(false, false, false); lm.visible = true; lmc.group.visible = true; lmSun.visible = lmHemi.visible = false; shadowVis(false);
      lmModel.visible = outside; if (outside) for (const m of hullMats) m.colorWrite = false;
      /* (the Lunar Module Pilot outside, on the porch or the ladder, hides the cabin behind him the same way) */
      const man = outside && lmp && lmp.on; if (man) { lmp.fig.visible = true; for (const m of lmp.mats) m.colorWrite = false; }
      renderer.render(scene, camera);
      for (const m of hullMats) m.colorWrite = true;
      if (man) { lmp.fig.visible = false; for (const m of lmp.mats) m.colorWrite = true; }
      lmModel.visible = true; lmSun.visible = lmHemi.visible = true; shadowVis(true);
      /* your hands, the panel, messages: in the Sun outside, in the cabin's light inside (owner's headset test: the
       * hands were black near the LM, with no light at all in this pass) */
      renderer.clearDepth(); show(false, false, true);
      lm.visible = true; lmModel.visible = false; shadowVis(false); lmSun.visible = lmHemi.visible = outside; handFill.visible = !outside;
      renderer.render(scene, camera);
      lm.visible = false; lmModel.visible = true; shadowVis(true); lmSun.visible = lmHemi.visible = true; handFill.visible = false;
    }
    if (Math.max(egress.fade, finaleFade) > 0.003) { veil.material.opacity = Math.max(egress.fade, finaleFade); camera.matrixWorld.decompose(veil.position, veil.quaternion, veil.scale); veil.scale.set(1, 1, 1); veil.translateZ(-0.1); veil.updateMatrixWorld(true); setPlanes(0.02, 10); renderer.render(fadeScene, camera); }
  } else {
    const alt = VIEWS[S.view].alt;
    show(true, true, false); setPlanes(Math.max(50, alt * 0.25), FAR); renderer.render(scene, camera);
    renderer.clearDepth();
    show(false, false, true); setPlanes(0.05, 100); renderer.render(scene, camera);
  }
  S.calls = renderer.info.render.calls; S.tris = renderer.info.render.triangles;
}
/* your own suit on the Moon, under your eyes: it stands where you stand and turns with you, a little behind your
 * look; scaled to your own height, so the boots are on the ground whether you are tall or sit */
const _fw = new THREE.Vector3(); let meYaw = null;
/* how tall you stand (it follows you slowly: a crouch or a jump is not a change of height), and your stride */
let frameDt = 0.016, standH = 0, meDrop = 0, gaitX = 0, gaitZ = 0, gaitV = 0, gaitPh = 0;
function placeMe(close) {
  if (!me || !close || egress.active) { meYaw = null; return; }
  camera.getWorldDirection(_fw);
  const flat = Math.hypot(_fw.x, _fw.z), want = flat > 0.2 ? Math.atan2(-_fw.x, -_fw.z) : meYaw ?? 0;
  if (meYaw === null) meYaw = want;
  let d = want - meYaw; d = Math.atan2(Math.sin(d), Math.cos(d)); meYaw += d * 0.12;
  /* your eyes over the floor against your standing height: lower, the knees bend (a crouch, to a rock or the flag's
   * foot); higher, the boots are off the ground (a jump: they are slow and high here) */
  const eh = _cam.y - rig.position.y;
  if (!standH || Math.abs(eh - standH) > 0.9) standH = eh;
  standH += Math.max(-0.02 * frameDt, Math.min(0.1 * frameDt, eh - standH));
  const sc = Math.max(0.6, Math.min(1.2, (standH - 0.14) / 1.59)), jump = Math.max(0, eh - standH - 0.04);
  meDrop = Math.max(0, Math.min(0.5 * sc, standH - eh - 0.03));
  me.scale.setScalar(sc);
  me.position.set(_cam.x + Math.sin(meYaw) * 0.07, rig.position.y + jump - meDrop, _cam.z + Math.cos(meYaw) * 0.07); me.rotation.y = meYaw; me.updateMatrixWorld(true);
  /* on the rover you sit: the hips on the seat, the knees up (and the whole suit of your shadow and reflection too) */
  const seated = !!ops && ops.driving;
  if (seated) { const hp = ops.hips; meYaw = ops.yaw; meDrop = 0; me.scale.setScalar(1); me.position.set(hp.x, hp.y - 0.97, hp.z); me.rotation.y = meYaw; me.updateMatrixWorld(true); }
  if (seated !== meSeated) { meSeated = seated; for (const f of [me, meGhost]) { const J = f.userData.joints; if (seated) { J.legL.rotation.set(1.45, 0, -0.03); J.legR.rotation.set(1.45, 0, 0.03); J.kneeL.rotation.set(-1.35, 0, 0); J.kneeR.rotation.set(-1.35, 0, 0); J.waist.rotation.set(-0.02, 0, 0); } else f.userData.pose(); } }
  if (!seated) {
    /* the legs: bent by the crouch (thigh forward, shin back, the boots staying under you), and swung by the stride
     * when you move over the ground (your own steps, or the stick's) */
    const a = Math.acos(Math.max(0.3, 1 - meDrop / (0.9 * sc)));
    const dx = me.position.x - gaitX, dz = me.position.z - gaitZ, v = Math.hypot(dx, dz) / Math.max(0.004, frameDt); gaitX = me.position.x; gaitZ = me.position.z;
    gaitV += ((v < 6 ? v : 0) - gaitV) * Math.min(1, frameDt * 6);                 /* (a teleport is not a stride) */
    const g = Math.max(0, Math.min(1, (gaitV - 0.2) / 1.1)); if (g > 0) gaitPh += frameDt * 2 * Math.PI / 0.85;
    const sw = Math.sin(gaitPh) * g;
    for (const fg of [me, meGhost]) { const J = fg.userData.joints;
      J.legL.rotation.x = 0.1 + a + 0.42 * sw; J.legR.rotation.x = 0.1 + a - 0.42 * sw;
      J.kneeL.rotation.x = -0.16 - 2 * a - 0.35 * Math.max(0, -sw); J.kneeR.rotation.x = -0.16 - 2 * a - 0.35 * Math.max(0, sw);
      J.waist.rotation.x = -0.08 - 0.2 * a - 0.06 * g; }
  }
  meGhost.position.copy(me.position); meGhost.rotation.y = meYaw; meGhost.scale.copy(me.scale); ghostArms();
  /* in the LM's shadow your suit is lit by the ground's glow only */
  const sunlit = (lmShadow.lit(_fw.copy(me.position).setY(me.position.y + 0.5)) + lmShadow.lit(_fw.setY(me.position.y + 1.4))) / 2;
  meLit += (sunlit - meLit) * 0.15; const f = 0.36 + 0.64 * meLit; for (const [m, c] of meBase) m.color.copy(c).multiplyScalar(f);
}
/* your own shadow, long in the low Sun: a whole suit (helmet, arms) that is never drawn itself, as the Sun sees it,
 * drawn again when you have turned or changed height, laid out from your feet every frame */
let meLit = 1, ghostYaw = 9, ghostS = 0, ghostT = 0;
function shadowMe(dt) {
  if (!me || meYaw === null || egress.active || S.view !== 'surface') return;
  ghostT += dt;
  /* (in the headset every fifth of a second: your arms move) */
  if (ghostT > 0.2 && (renderer.xr.isPresenting || Math.abs(meYaw - ghostYaw) > 0.08 || Math.abs(me.scale.x - ghostS) > 0.03)) {
    ghostT = 0; ghostYaw = meYaw; ghostS = me.scale.x;
    meShadow.bake(renderer, meGhost, moon.shared.uSun.value);
  }
  meShadow.drape(me.position.x, me.position.y + meDrop, me.position.z);
}
let lastT = 0, acc = 0, accN = 0, slow = 0;
function frame(t) {
  if (lmSun) lmSun.position.copy(moon.shared.uSun.value).multiplyScalar(100).add(lmSun.target.position);     /* the LM lit by the Sun of the moment */
  if (lmShadow && lmShadowSun !== S.sunOffset && !renderer.xr.isPresenting) { lmShadowSun = S.sunOffset; lmShadow.bake(renderer, lmModel, moon.shared.uSun.value); }   /* its shadow (only when the Sun moves) */
  const dt = Math.min(0.1, (t - lastT) / 1000 || 0.016); lastT = t;
  acc += dt; accN++;
  if (acc > 0.5) {
    S.fps = accN / acc; S.ms = acc / accN * 1000; acc = 0; accN = 0;
    /* frame-rate guard: stronger foveation when the headset falls behind (resolution cannot change mid-session) */
    if (renderer.xr.isPresenting) {
      slow = S.fps < 66 ? slow + 1 : 0;
      if (slow >= 4) reflEvery = 6;                             /* the visor's mirror gives way first */
      if (slow >= 4 && S.foveation < 1) { S.foveation = Math.min(1, S.foveation + 0.25); renderer.xr.setFoveation(S.foveation); slow = 0; }
    }
    if ($('stat')) $('stat').textContent = `${S.fps.toFixed(0)} fps · ${S.calls} draw calls · ${(S.tris / 1000).toFixed(0)}k triangles`;
  }
  const xr = renderer.xr.isPresenting;
  if (ctl) ctl.update();
  if (xr) readControllers(dt);
  frameDt = dt; if (CH6 || Q.has('egress')) dimHands(dt);
  if (ops && ops.driving && roverBuzz > 0.01) { buzzT -= dt; if (buzzT <= 0) { buzzT = 0.08; buzz(roverBuzz, 100, 'left'); buzz(roverBuzz * 0.5, 100, 'right'); } } else roverBuzz = 0;
  toast.update(dt, camera); card.update(dt, camera);
  if (CH6 || Q.has('egress')) mradio.update(dt, { ptt: (xr ? pttHeld : !!keys.KeyT) && S.view === 'surface', lmpOut: !!lmp && lmp.on && lmp.S.phase === 'roam', quiet: !egress.active && !!lmp && lmp.S.phase === 'roam' });
  if (ops && S.view === 'surface') ops.update(dt, { walking: !egress.active, head: headGround(_fw), stick: xr ? opsStick : [(keys.KeyD ? 1 : 0) - (keys.KeyA ? 1 : 0), (keys.KeyS ? 1 : 0) - (keys.KeyW ? 1 : 0)] });
  /* the finale: the console's tube and its words, the stand-in suit's shadow in the film */
  if (finaleOn) {
    const F = ops.F; con.update(t / 1000, F.on);
    con.caption(['APOLLO 17', 'You completed the first part', 'of the Apollo 17 mission.', 'Have a safe trip back.'], F.caption);
    if (F.actors && (ghostBake += dt) > 0.15) { ghostBake = 0; meShadow.bake(renderer, meGhost, moon.shared.uSun.value); }
    if (F.actors) meShadow.drape(meGhost.position.x, meGhost.position.y, meGhost.position.z);
    if (F.end && !endSaid) { endSaid = true; saveProgress('liftoff.html', null, 0, true); toast.say('A: on to chapter 7, the lift-off', 1e6, true); }
  }
  if (lmp && S.view === 'surface' && (CH6 || Q.has('egress'))) lmp.update(dt, { walking: !egress.active, player: rig.position });
  if (CH6 && !endSaid) saveProgress('surface.html', egress.active ? 0 : 1, t);
  if (S.view === 'surface' && egress.active) {
    egress.update(dt, xr ? [EH.left, EH.right] : []);
    rig.position.y += rigLift;                                    /* the lift-off carries you up with the cabin */
    lmc.update(t / 1000, { viewer: camera });
    lmc.setDaylight(1, _eye.copy(moon.shared.uSun.value).applyAxisAngle(new THREE.Vector3(0, 1, 0), -lmModel.rotation.y));
  } else if (S.view === 'surface') {
    if (!xr) {
      const sp = (keys.ShiftLeft ? 6 : 1.5) * dt, h = (S.heading + S.yaw * Math.PI / 180);
      const fx = -Math.sin(h), fz = -Math.cos(h);
      if (keys.KeyW) { S.pos.x += fx * sp; S.pos.z += fz * sp; } if (keys.KeyS) { S.pos.x -= fx * sp; S.pos.z -= fz * sp; }
      if (keys.KeyA) { S.pos.x += fz * sp; S.pos.z -= fx * sp; } if (keys.KeyD) { S.pos.x -= fz * sp; S.pos.z += fx * sp; }
      if (keys.KeyW || keys.KeyS || keys.KeyA || keys.KeyD) walkMoved += sp;
    }
    if (ops && ops.driving) {
      /* seated on the rover: your eyes are over the Commander's seat, level, facing where it points */
      const e = ops.eye, h = ops.yaw, c = Math.cos(h), sn = Math.sin(h), cx = xr ? camera.position.x : 0, cz = xr ? camera.position.z : 0, cy = xr ? camera.position.y : 1.65;
      S.heading = h; S.pos.set(e.x - (cx * c + cz * sn), 0, e.z - (-cx * sn + cz * c));
      rig.position.set(S.pos.x, e.y - cy, S.pos.z); rig.quaternion.setFromAxisAngle(new THREE.Vector3(0, 1, 0), h);
      walkMoved = 0; hopY = 0; STEP.last = null;
    } else {
    collide();
    const lim = walkLimit();
    S.pos.x = Math.max(-lim, Math.min(lim, S.pos.x)); S.pos.z = Math.max(-lim, Math.min(lim, S.pos.z));
    /* the moon walk: each stride a small arc, as the low gravity makes of every step (a setting; on by default) */
    let arc = 0;
    if (walkMoved > 1e-5) { walkD += walkMoved; const q = (walkD % STEP.stride) / STEP.stride; arc = 0.11 * 4 * q * (1 - q); }
    walkMoved = 0; hopY += ((settings.get('walk') ? arc : 0) - hopY) * Math.min(1, dt * 16);
    rig.position.set(S.pos.x, moon.heightAt(S.pos.x, S.pos.z) + hopY, S.pos.z);
    rig.quaternion.setFromAxisAngle(new THREE.Vector3(0, 1, 0), S.heading);
    shadowMe(dt);
    if (atLadder() && performance.now() > mountHint) { mountHint = performance.now() + 30000; toast.say('The ladder: grip or trigger here to climb back up', 4); }
    }
  } else {
    const v = VIEWS[S.view];
    if (!S.still) {
      S.theta -= Math.sqrt(GM / (moon.manifest.radius + v.alt)) / (moon.manifest.radius + v.alt) * 180 / Math.PI * dt;
      if (S.theta < v.to) S.theta = v.from;
    }
    orbitFrame(S.theta, v.alt, v.tilt);
    if (xr) rig.quaternion.multiply(_q.setFromAxisAngle(new THREE.Vector3(0, 1, 0), S.heading));
  }
  if (xr) { head.position.set(0, 0, 0); head.rotation.set(0, 0, 0); }
  else { head.position.set(0, 1.65, 0); head.rotation.set(S.pitch * Math.PI / 180, S.yaw * Math.PI / 180, 0, 'YXZ'); }
  if (hud) hud.draw(t, info());
  stepBlink(dt);
  scene.updateMatrixWorld(true);
  if (renderer.xr.isPresenting) renderer.xr.updateCamera(camera);
  trackSteps();
  if (S.view === 'surface' && ground.ok && !TP.blink) { headGround(_h); ground.update(_h.x, _h.z); }
  render();
  S.frames++;
  if (window.__moon.afterFrame) { const f = window.__moon.afterFrame; window.__moon.afterFrame = null; f(); }
}

/* ---------------------------------------------------------------- start */
async function enterVR() {
  if (CH6 || Q.has('egress')) mradio.init();
  if (!navigator.xr) return;
  readOptions();
  suit.init();
  if (CH6) setTimeout(() => toast.say('Press the left stick for the status and the settings', 6, true), 7500);
  renderer.xr.setFramebufferScaleFactor(num('scale', +($('scale')?.value || 1)));
  const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor'] });
  await renderer.xr.setSession(session);
  if ($('hz')) settings.set('hz', +$('hz').value);
  S.foveation = settings.get('fov'); renderer.xr.setFoveation(S.foveation);
  common.all();
  S.heading = 0; S.yaw = 0; S.pitch = 0;
  /* the panel rides on the left controller */
  if (!ctl) { ctl = makeControllers(renderer, camera.parent); ctl.grips.left.add(hud.mesh); }
  if (egress.active) egress.recentre();
  if (CH6) card.show({ title: 'Chapter 6', sub: 'On the Moon' }, 6);
  session.addEventListener('selectstart', () => suit.init());
  document.body.classList.add('invr');
  session.addEventListener('end', () => { document.body.classList.remove('invr'); renderer.setSize(innerWidth, innerHeight); });
}

async function start() {
  const bar = $('bar');
  moon = await loadMoon(renderer, BASE, f => { if (bar) bar.style.width = (f * 100).toFixed(0) + '%'; });
  moon.setSunOffset(S.sunOffset);
  moon.shared.uExposure.value = EXPOSURE;
  rocks = await makeRocks(moon, BASE);
  /* the LM you landed in (chapter 5 sets it down here: pads on the ground at the site, facing along the approach) */
  lm = new THREE.Group();
  { const m = makeLMModel(renderer), yaw = Math.atan2(Math.sin(HEADING), -Math.cos(HEADING)), st = settleLM((x, z) => moon.heightAt(x, z), 0, 0, yaw);
    m.rotation.order = 'YXZ'; m.rotation.set(st.pitch, yaw, st.roll); m.position.y = st.y; lm.add(m);          /* all four pads on the ground, a little in the dust */
    lmModel = m; m.traverse(o => { if (o.isMesh) hullMats.add(o.material); });
    /* the crew dressed for the surface (eva-suit.js): the Lunar Module Pilot in the cabin, your own suit there and on the ground */
    const sun = moon.shared.uSun.value, env = suitEnvironment(renderer, Math.atan2(sun.z, sun.x), Math.asin(sun.y));
    me = makeEVASuit({ player: true, commander: true, env }); me.visible = false; scene.add(me);
    handFill = new THREE.HemisphereLight(0xf4f1ea, 0x8a877f, 2.4); handFill.visible = false; scene.add(handFill);
    meBase = new Map(); me.traverse(o => { if (o.isMesh && !meBase.has(o.material)) meBase.set(o.material, o.material.color.clone()); });
    meGhost = makeEVASuit({ commander: true, env }); meGhost.visible = false; scene.add(meGhost); meShadow = makeFigureShadow(moon); meShadow.mesh.visible = false; scene.add(meShadow.mesh);
    lmc = makeLMCabin({ inwardOnly: true, eva: true, env });
    lmShadow = makeLMShadow(moon);
    /* the Lunar Module Pilot, who follows you down and moves about in front of the LM (lmp.js) */
    lmp = makeLMP({ moon, model: m, env, renderer, sun, mirror: cubeRT.texture, voice: k => mradio.cue(k), cabinLmp: lmc.lmp, shade: p => lmShadow.lit(p), get prints() { return prints; }, get toast() { return toast; } });
    scene.add(lmp.fig, lmp.extras);
    /* the flag and the rover (surface-ops.js): chapter 6 only */
    if (CH6 || Q.has('egress')) {
      ops = makeSurfaceOps({ moon, renderer, sun, env, model: m, lmp, radio: mradio, get toast() { return toast; }, rig, camera, S, makeShadow: makeFigureShadow, makeDust,
        clearOf: (x, z, body) => clearOf(x, z, body), step: k => suit.step(k),
        /* the rover through the suit: heard (sound.js) and felt (the controllers: the left hand is on its handle) */
        drive: (v, thr) => { suit.drive(v, thr); roverBuzz = Math.min(1, Math.abs(v) / 3.6) * 0.16 + Math.min(1, Math.abs(thr)) * 0.06; },
        bump: k => { suit.bump(k); buzz(0.35 + 0.4 * Math.min(1, k), 70); }, buzz: (k, ms) => buzz(k, ms),
        track: (x, z, dx, dz) => prints.track(x, z, dx, dz), trackLen: 0.5,
        inCabin: () => egress.active && egress.E.stage === 'cabin',
        cabin: () => { lmc.lmp.visible = true; lmc.setSuitUp(1); },
        hatch: k => { egress.E.locked = true; egress.E.door = 1 - k; lmc.setForwardHatch(1 - k); if (k >= 1) lmModel.userData.hatch.visible = true; },
        lift: (alt, withRig) => { liftModel(alt); if (withRig) rigLift = alt; },
        fade: k => { finaleFade = k; },
        finale: () => {
          /* the console, a little below your eyes, an arm and a half away, where you look */
          finaleOn = true; helm.set(0);                 /* (render() sets it in place while the view is still dark) */
        },
        ghost: {
          snap(a, off) { const J = meGhost.userData.joints; a[off] = meGhost.position.x; a[off + 1] = meGhost.position.y; a[off + 2] = meGhost.position.z; a[off + 3] = meGhost.rotation.y; a[off + 4] = meGhost.scale.x;
            J.armL.quaternion.toArray(a, off + 5); a[off + 9] = J.elbowL.rotation.x; J.armR.quaternion.toArray(a, off + 10); a[off + 14] = J.elbowR.rotation.x;
            a[off + 15] = J.legL.rotation.x; a[off + 16] = J.legR.rotation.x; a[off + 17] = J.kneeL.rotation.x; a[off + 18] = J.kneeR.rotation.x; a[off + 19] = J.waist.rotation.x; },
          pose(a, off) { const J = meGhost.userData.joints; if (meSeated) { meGhost.userData.pose(); meSeated = false; }
            meGhost.position.set(a[off], a[off + 1], a[off + 2]); meGhost.rotation.y = a[off + 3]; meGhost.scale.setScalar(a[off + 4] || 1);
            J.armL.quaternion.fromArray(a, off + 5); J.elbowL.rotation.set(a[off + 9], 0, 0); J.armR.quaternion.fromArray(a, off + 10); J.elbowR.rotation.set(a[off + 14], 0, 0);
            J.legL.rotation.x = a[off + 15]; J.legR.rotation.x = a[off + 16]; J.kneeL.rotation.x = a[off + 17]; J.kneeR.rotation.x = a[off + 18]; J.waist.rotation.x = a[off + 19]; meGhost.updateMatrixWorld(true); },
          stand(x, z, yaw) { meGhost.userData.pose(); meGhost.position.set(x, moon.heightAt(x, z), z); meGhost.rotation.y = yaw; meGhost.scale.setScalar(1); meGhost.updateMatrixWorld(true); }
        } });
      scene.add(con.group);
      scene.add(ops.group);
    } lmc.group.position.copy(m.position); lmc.group.rotation.copy(m.rotation); lmc.setHatchOpen(false); lmc.group.visible = false; lm.add(lmc.group);
    lmSun = new THREE.DirectionalLight(0xfff3e2, 2.6); lmSun.target = m; lm.add(lmSun);
    const hemi = new THREE.HemisphereLight(0x0c0c0e, 0x8c877e, 0.55); lm.add(hemi); lmHemi = hemi;
    lm.add(lmShadow.mesh); }
  sky = makeSky(moon);
  rail = makeRail(); rig.add(rail);
  hud = makeWrist(['walk', 'text', 'voices', 'hz', 'fov', 'hand']);
  toast = makeToast(); head.add(toast.mesh);
  card = makeCard(); head.add(card.mesh);
  ground = makeGround(moon, BASE);
  prints = makePrints(moon);
  prints.trail(-5.2, 0.3, 5.6, 13.2);              /* (the ladder is on the front leg, which faces west) */                   /* your own way here from the ladder */
  scene.add(moon.far, moon.near, ground.group, rocks.group, lm, sky, prints.group, arcLine, marker);
  scene.add(helm.group);
  egress = makeEgress({ model: lmModel, lmc, moon, rig, camera, toast, suit, prints, helmet: helm, voice: k => mradio.cue(k, false),
    eye: () => (renderer.xr.isPresenting ? _eye.copy(camera.position) : _eye.set(0, 1.65, 0)),
    yaw: () => (renderer.xr.isPresenting ? _eu.setFromQuaternion(camera.quaternion, 'YXZ').y : S.yaw * Math.PI / 180),
    done: (x, z, heading) => { S.pos.set(x, 0, z); S.heading = heading; STEP.last = null; } });
  if (Q.has('egress') || CH6) S.view = 'surface';
  /* chapter 6: where you were (the walk, if you had climbed down), else the cabin */
  if (CH6 && $('start')) { const p = loadProgress(); if (p && p.page === 'surface.html' && p.t === 1) $('start').value = 'walk'; }
  setView(S.view, Q.has('theta') ? num('theta', 0) : undefined);
  const inCabinStart = () => { egress.begin(); lmp.reset(); };
  const onGround = () => { egress.skip(); lmp.startOutside(); S.pos.set(-7.2, 0, -1.0); S.heading = -98 * Math.PI / 180; };          /* a few steps west of the ladder, facing the LM */
  if (Q.has('egress') || (CH6 && !Q.has('walk') && (!$('start') || $('start').value === 'egress'))) inCabinStart(); else if (CH6) onGround();
  if (CH6) $('start')?.addEventListener('change', () => { if ($('start').value === 'egress') inCabinStart(); else onGround(); });
  $('flat')?.addEventListener('click', () => { wake(); document.body.classList.add('running'); });
  if (Q.has('x')) { S.pos.set(num('x', 6), 0, num('z', 14)); if (ground.ok && !ground.detailed(S.pos.x, S.pos.z)) await ground.request(S.pos.x, S.pos.z); }
  S.heading = num('heading', S.view === 'surface' ? 25 : 0) * Math.PI / 180;
  if (Q.has('trail')) prints.trail(S.pos.x + 0.3, S.pos.z - 1.2, S.pos.x + 1.5, S.pos.z - 9);      /* test aid: prints ahead (north) */
  renderer.setAnimationLoop(frame);
  document.body.classList.add('loaded');
  S.ready = true;
  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then(ok => { if (ok && $('vr')) { $('vr').disabled = false; $('vr').textContent = 'Enter VR'; } });
}
function readOptions() {
  OPT.teleport = $('opt-teleport') ? $('opt-teleport').checked : true;
  OPT.fan = $('opt-fan') ? $('opt-fan').checked : true;
  suit.setFan(OPT.fan);
}
$('opt-fan')?.addEventListener('change', readOptions);
/* on the computer, sound starts with the first click or key (browsers do not allow it earlier) */
const wake = () => { if (Q.has('shot')) return; readOptions(); suit.init(); if (CH6 || Q.has('egress')) mradio.init(); };
addEventListener('pointerdown', wake); addEventListener('keydown', wake);
if (Q.has('shot')) document.body.classList.add('invr');
$('vr')?.addEventListener('click', enterVR);
autoEnter(() => S.ready, enterVR);
document.querySelectorAll('[data-view]').forEach(b => b.addEventListener('click', () => setView(b.dataset.view)));
start().catch(e => { console.error(e); if ($('err')) { $('err').textContent = 'Could not start: ' + e.message; } });

/* test hooks */
window.__moon = { S, setView, afterFrame: null, filmPass: () => filmPass(lastShow), filmRT, lift: a => liftModel(a), get shadows() { return { split: lmSplit, asc: !!shAsc && shAsc.mesh.visible, desc: !!shDesc && shDesc.mesh.visible, whole: !!lmShadow && lmShadow.mesh.visible }; }, get lmShadow() { return lmShadow; }, get ghost() { return meGhost; }, get egress() { return egress; }, get lmc() { return lmc; }, get lmp() { return lmp; }, get refl() { return reflCount; }, radio: mradio, atLadder, get ops() { return ops; }, get finale() { return finaleOn; }, con, get lmDescent() { return lmDescent; }, get lmModel() { return lmModel; }, clipAsc, clipDesc, clearOf(x, z) { const p = clearOf(x, z); return [p.x, p.z]; }, helm, get me() { return me; }, get lm() { return lmModel; }, hands, EH, get toast() { return toast; }, get moon() { return moon; }, renderer, camera, rig,
  setSun(deg) { S.sunOffset = deg; moon.setSunOffset(deg); },
  look(yaw, pitch) { S.yaw = yaw; S.pitch = pitch; },
  stand(x, z, headingDeg) { S.pos.set(x, 0, z); if (headingDeg !== undefined) S.heading = headingDeg * Math.PI / 180; },
  teleport(x, z) { startTeleport(x, z); }, get teleporting() { return !!TP.blink; },
  get prints() { return prints; }, get ground() { return ground; }, suit };
