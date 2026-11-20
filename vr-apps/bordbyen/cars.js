/*
 * Bordbyen — the racing cars, built in code: chunky toy cars with rounded bodies, big wheels and round lamps.
 *
 * A car's origin is on the ground between its wheels, its nose along -Z, about 4 m long. Each car has:
 *   userData.wheels       the four wheel groups (they spin about x),
 *   userData.steer        the two front pivots (they turn about y),
 *   userData.cockpit      a dashboard and a steering wheel, shown only when you sit in it,
 *   userData.wheel        the steering wheel itself (turns with the steering),
 *   userData.eye          where the driver's eyes are, in the car's frame,
 *   userData.radius       its size for collisions.
 * Rounded boxes are made by pushing a subdivided box's vertices out from a smaller inner box, with the push as the normal (smooth, no creases).
 */
import * as THREE from './vendor/three.module.min.js';
import { mergeGeometries } from './vendor/BufferGeometryUtils.js';
import { toonMat, ink } from './toon.js?v=5';

export const CARS = [
  { key: 'lyn', name: 'Lynet', color: '#e8392f', blurb: 'Lav og hurtig, med spoiler' },
  { key: 'boble', name: 'Boblen', color: '#f6c531', blurb: 'Rund og glad, kører som en kugle' },
  { key: 'bulder', name: 'Bulderen', color: '#4cb84a', blurb: 'Kæmpehjul, tager græsset let' },
  { key: 'brum', name: 'Brummer', color: '#3b82d6', blurb: 'Lille rund bus, tung men stabil' },
];
/* how they drive: top speed (m/s), acceleration, grip, and how well they do off the road */
export const HANDLING = {
  lyn: { vmax: 30, acc: 11, grip: 1.0, off: 0.35, turn: 1.0 },
  boble: { vmax: 26, acc: 12.5, grip: 1.15, off: 0.4, turn: 1.15 },
  bulder: { vmax: 25, acc: 10, grip: 0.9, off: 0.85, turn: 0.95 },
  brum: { vmax: 27, acc: 9.5, grip: 1.05, off: 0.45, turn: 0.9 },
};

function roundedBox(w, h, d, r, seg = 5) {
  const g = new THREE.BoxGeometry(w, h, d, seg, seg, seg), p = g.getAttribute('position'), n = g.getAttribute('normal');
  const ix = w / 2 - r, iy = h / 2 - r, iz = d / 2 - r, v = new THREE.Vector3(), c = new THREE.Vector3();
  for (let k = 0; k < p.count; k++) {
    v.fromBufferAttribute(p, k); c.set(Math.max(-ix, Math.min(ix, v.x)), Math.max(-iy, Math.min(iy, v.y)), Math.max(-iz, Math.min(iz, v.z)));
    v.sub(c); if (v.lengthSq() < 1e-12) v.fromBufferAttribute(n, k); v.normalize();
    n.setXYZ(k, v.x, v.y, v.z); p.setXYZ(k, c.x + v.x * r, c.y + v.y * r, c.z + v.z * r);
  }
  return g;
}
const _c = new THREE.Color(), _m = new THREE.Matrix4(), _q = new THREE.Quaternion(), _e = new THREE.Euler();
/** colour a geometry, place it, and keep it for merging */
function part(list, geo, color, x = 0, y = 0, z = 0, rx = 0, ry = 0, rz = 0, sx = 1, sy = 1, sz = 1) {
  let g = geo.index ? geo.toNonIndexed() : geo.clone(); g.deleteAttribute('uv');
  g.applyMatrix4(_m.compose(new THREE.Vector3(x, y, z), _q.setFromEuler(_e.set(rx, ry, rz)), new THREE.Vector3(sx, sy, sz)));
  _c.set(color); const nn = g.getAttribute('position').count, col = new Float32Array(nn * 3);
  for (let i = 0; i < nn; i++) { col[i * 3] = _c.r; col[i * 3 + 1] = _c.g; col[i * 3 + 2] = _c.b; }
  g.setAttribute('color', new THREE.BufferAttribute(col, 3)); list.push(g);
}
const merge = list => { const g = mergeGeometries(list, false); g.computeBoundingSphere(); return g; };
const BALL = new THREE.SphereGeometry(1, 20, 14), CYL = new THREE.CylinderGeometry(1, 1, 1, 20), TUBE = new THREE.CylinderGeometry(1, 1, 1, 8);

const bodyMat = toonMat();
const glassMat = new THREE.MeshToonMaterial({ color: 0x5f8fc4, transparent: true, opacity: 0.45, depthWrite: false });      // tinted, and see-through from the driver's seat
const darkMat = toonMat();

function lighten(hex, k) { return '#' + new THREE.Color(hex).lerp(new THREE.Color(1, 1, 1), k).getHexString(); }
function darken(hex, k) { return '#' + new THREE.Color(hex).lerp(new THREE.Color(0, 0, 0), k).getHexString(); }

/** one wheel: a fat tyre, a coloured hub with five bolts, and a white sidewall stripe */
function wheel(r, width, hub) {
  const g = new THREE.Group(), list = [];
  const tyre = new THREE.TorusGeometry(r * 0.72, r * 0.3, 12, 24); tyre.rotateY(Math.PI / 2); part(list, tyre, '#26282d', 0, 0, 0, 0, 0, 0, width / (r * 0.6), 1, 1);
  part(list, CYL, '#26282d', 0, 0, 0, 0, 0, Math.PI / 2, r * 0.75, width * 0.9, r * 0.75);
  for (const s of [-1, 1]) {
    part(list, CYL, hub, s * width * 0.46, 0, 0, 0, 0, Math.PI / 2, r * 0.5, 0.06, r * 0.5);
    part(list, CYL, '#f1f1f1', s * width * 0.45, 0, 0, 0, 0, Math.PI / 2, r * 0.62, 0.03, r * 0.62);
    part(list, CYL, hub, s * width * 0.47, 0, 0, 0, 0, Math.PI / 2, r * 0.5, 0.06, r * 0.5);
    part(list, BALL, lighten(hub, 0.4), s * width * 0.5, 0, 0, 0, 0, 0, 0.06, r * 0.16, r * 0.16);
    for (let k = 0; k < 5; k++) { const a = k / 5 * Math.PI * 2; part(list, BALL, '#d8d8d8', s * width * 0.5, Math.sin(a) * r * 0.3, Math.cos(a) * r * 0.3, 0, 0, 0, 0.04, 0.05, 0.05); }
  }
  g.add(ink(new THREE.Mesh(merge(list), darkMat))); return g;
}

export function buildCar(key, color) {
  const car = new THREE.Group(), body = [], glass = [], roof = [], wheels = [], steer = [];
  const c = color || CARS.find(k => k.key === key).color, light = lighten(c, 0.35), deep = darken(c, 0.35);
  let W = 2.0, wheelR = 0.5, rearR = 0.5, wx = 1.0, fz = -1.35, rz = 1.35, track = 0.42, eye = new THREE.Vector3(0, 1.3, 0.2), radius = 1.25;
  const lamps = (y, z, x, r, col) => { for (const s of [-1, 1]) part(body, BALL, col, s * x, y, z, 0, 0, 0, r, r, r * 0.5); };
  if (key === 'lyn') {
    /* low racer: a long rounded body, a raised cockpit with a bubble screen, side pods, a big rear wing */
    part(body, roundedBox(1.9, 0.62, 4.3, 0.28), c, 0, 0.62, 0);
    part(body, roundedBox(1.3, 0.4, 1.6, 0.2), c, 0, 0.62, -1.6, 0.12);                          // the nose dips
    part(body, roundedBox(1.25, 0.5, 1.5, 0.25), deep, 0, 1.05, 0.55);                             // behind the driver
    part(body, roundedBox(2.15, 0.36, 1.7, 0.17), light, 0, 0.5, 0.35);                             // side pods
    part(body, roundedBox(2.3, 0.08, 0.7, 0.04), '#222', 0, 1.62, 1.85);                            // wing
    for (const s of [-1, 1]) part(body, roundedBox(0.08, 0.6, 0.5, 0.03), '#222', s * 0.6, 1.3, 1.85);
    part(body, CYL, '#ffffff', 1.0, 0.68, 0.1, 0, 0, Math.PI / 2, 0.36, 0.02, 0.36); part(body, CYL, '#ffffff', -1.0, 0.68, 0.1, 0, 0, Math.PI / 2, 0.36, 0.02, 0.36);      // number roundels
    part(glass, BALL, '#000', 0, 1.0, -0.95, -0.35, 0, 0, 0.42, 0.17, 0.3);                      // a low windscreen ahead of the open cockpit
    lamps(0.72, -2.08, 0.6, 0.17, '#fff8d0'); lamps(0.8, 2.13, 0.65, 0.13, '#ff3b30');
    wheelR = 0.48; rearR = 0.58; wx = 1.08; fz = -1.45; rz = 1.35; track = 0.5; eye.set(0, 1.42, 0.05); radius = 1.25;
  } else if (key === 'boble') {
    /* bubble car: one round body, a big glass dome, round eyes for lamps */
    part(body, roundedBox(1.95, 1.0, 3.5, 0.48, 7), c, 0, 0.82, 0);
    part(body, roundedBox(2.05, 0.18, 3.65, 0.09), deep, 0, 0.42, 0);                               // a bumper all round
    part(glass, BALL, '#000', 0, 1.3, 0.05, 0, 0, 0, 0.88, 0.62, 1.2);
    part(body, roundedBox(0.3, 0.05, 0.05, 0.02), '#222', 0, 1.95, 0.05);
    lamps(0.95, -1.62, 0.58, 0.24, '#fff8d0'); for (const s of [-1, 1]) part(body, BALL, '#1b1b1b', s * 0.58, 0.97, -1.73, 0, 0, 0, 0.09, 0.09, 0.05);
    lamps(0.95, 1.72, 0.62, 0.12, '#ff3b30');
    wheelR = rearR = 0.45; wx = 0.98; fz = -1.12; rz = 1.12; track = 0.42; eye.set(0, 1.42, 0.25); radius = 1.15;
  } else if (key === 'bulder') {
    /* buggy on huge wheels: a short tub, a roll cage, a spare wheel on the back */
    part(body, roundedBox(1.7, 0.7, 3.2, 0.3), c, 0, 1.25, 0);
    part(body, roundedBox(1.9, 0.25, 3.5, 0.12), '#3a3d42', 0, 0.95, 0);                             // chassis
    part(body, roundedBox(1.5, 0.45, 1.0, 0.2), light, 0, 1.65, -1.0);                               // bonnet hump
    const bar = (a, b, r = 0.06) => { const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), d = B.clone().sub(A), q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 1, 0), d.clone().normalize()), e = new THREE.Euler().setFromQuaternion(q);
      part(body, TUBE, '#2a2a2a', (A.x + B.x) / 2, (A.y + B.y) / 2, (A.z + B.z) / 2, e.x, e.y, e.z, r, d.length(), r); };
    for (const s of [-1, 1]) { bar([s * 0.75, 1.55, -0.4], [s * 0.7, 2.6, 0.0]); bar([s * 0.7, 2.6, 0.0], [s * 0.7, 2.6, 0.9]); bar([s * 0.7, 2.6, 0.9], [s * 0.75, 1.55, 1.2]); }
    bar([-0.7, 2.6, 0.0], [0.7, 2.6, 0.0]); bar([-0.7, 2.6, 0.9], [0.7, 2.6, 0.9]);
    lamps(1.5, -1.62, 0.55, 0.2, '#fff8d0'); for (const s of [-1, 1]) part(body, BALL, '#fff8d0', s * 0.35, 2.68, 0.0, 0, 0, 0, 0.14, 0.14, 0.1);
    lamps(1.35, 1.62, 0.6, 0.12, '#ff3b30');
    wheelR = rearR = 0.82; wx = 1.18; fz = -1.2; rz = 1.2; track = 0.62; eye.set(0, 2.05, 0.35); radius = 1.35;
  } else {
    /* a small round bus: tall rounded body, a band of windows, a white roof */
    part(body, roundedBox(2.0, 1.7, 4.0, 0.42, 6), c, 0, 1.3, 0);
    part(roof, roundedBox(1.9, 0.22, 3.8, 0.1), '#f6f3ea', 0, 2.17, 0);                            // hidden when you sit inside
    part(body, roundedBox(2.1, 0.22, 4.1, 0.1), deep, 0, 0.55, 0);
    for (const s of [-1, 1]) part(glass, roundedBox(0.06, 0.6, 3.0, 0.03), '#000', s * 1.0, 1.65, 0.15);
    part(glass, roundedBox(1.6, 0.65, 0.06, 0.03), '#000', 0, 1.68, -2.0); part(glass, roundedBox(1.4, 0.55, 0.06, 0.03), '#000', 0, 1.68, 2.0);
    lamps(0.9, -2.02, 0.65, 0.2, '#fff8d0'); lamps(0.9, 2.02, 0.7, 0.12, '#ff3b30');
    part(body, roundedBox(1.0, 0.25, 0.08, 0.04), '#ffffff', 0, 1.05, -2.03);                       // a smile of a grille
    wheelR = rearR = 0.5; wx = 1.02; fz = -1.35; rz = 1.35; track = 0.44; eye.set(0, 1.75, -0.6); radius = 1.3;
  }
  car.add(ink(new THREE.Mesh(merge(body), bodyMat)));
  if (glass.length) car.add(new THREE.Mesh(merge(glass), glassMat));
  const roofMesh = roof.length ? ink(new THREE.Mesh(merge(roof), bodyMat)) : null; if (roofMesh) car.add(roofMesh);
  for (const [x, z, r, front] of [[-wx, fz, wheelR, true], [wx, fz, wheelR, true], [-wx, rz, rearR, false], [wx, rz, rearR, false]]) {
    const pivot = new THREE.Group(); pivot.position.set(x, r, z); const w = wheel(r, track, key === 'bulder' ? '#f2c230' : '#d7dce2'); pivot.add(w); car.add(pivot);
    wheels.push(w); if (front) steer.push(pivot);
  }
  /* a soft dark shadow under the car (cheap, no shadow maps) */
  const shadowTex = (() => { const cv = document.createElement('canvas'); cv.width = cv.height = 64; const x = cv.getContext('2d'), g = x.createRadialGradient(32, 32, 4, 32, 32, 32);
    g.addColorStop(0, 'rgba(0,0,0,.55)'); g.addColorStop(1, 'rgba(0,0,0,0)'); x.fillStyle = g; x.fillRect(0, 0, 64, 64); return new THREE.CanvasTexture(cv); })();
  const sh = new THREE.Mesh(new THREE.PlaneGeometry(W * 1.5, 5.2), new THREE.MeshBasicMaterial({ map: shadowTex, transparent: true, depthWrite: false }));
  sh.rotation.x = -Math.PI / 2; sh.position.y = 0.05; sh.renderOrder = 1; car.add(sh);
  /* the cockpit: dashboard, dials and a steering wheel, in front of the eyes */
  const cockpit = new THREE.Group(), dl = [];
  part(dl, roundedBox(1.4, 0.32, 0.4, 0.12), '#2c2f36', 0, eye.y - 0.55, eye.z - 0.72);
  for (const s of [-0.32, 0.32]) { part(dl, CYL, '#f4f1e6', s, eye.y - 0.43, eye.z - 0.55, Math.PI / 2 - 0.5, 0, 0, 0.1, 0.02, 0.1); part(dl, CYL, '#111', s, eye.y - 0.43, eye.z - 0.545, Math.PI / 2 - 0.5, 0, 0, 0.085, 0.02, 0.085); }
  cockpit.add(new THREE.Mesh(merge(dl), darkMat));
  const sw = new THREE.Group(), sl = [];
  const rim = new THREE.TorusGeometry(0.19, 0.025, 8, 28); part(sl, rim, '#202226');
  part(sl, roundedBox(0.36, 0.035, 0.035, 0.015), '#202226'); part(sl, roundedBox(0.035, 0.19, 0.035, 0.015), '#202226', 0, -0.095, 0);
  part(sl, CYL, c, 0, 0, 0.02, Math.PI / 2, 0, 0, 0.06, 0.04, 0.06);
  sw.add(new THREE.Mesh(merge(sl), darkMat)); sw.position.set(0, eye.y - 0.42, eye.z - 0.42); sw.rotation.x = -0.45;
  const swSpin = new THREE.Group(); sw.children[0].removeFromParent(); swSpin.add(new THREE.Mesh(merge(sl), darkMat)); sw.add(swSpin);
  cockpit.add(sw); cockpit.visible = false; car.add(cockpit);
  car.userData = { key, wheels, steer, cockpit, wheel: swSpin, wheelBase: sw, eye, radius, front: fz, rear: rz, roof: roofMesh };
  return car;
}
