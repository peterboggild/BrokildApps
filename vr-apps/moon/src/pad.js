/* The pad as you see it while boarding: the Saturn V beside you, the crew access arm (swing arm 9) you walk along,
 * the White Room at its end against the Command Module, and the Vehicle Assembly Building in the distance.
 *
 * Pad frame (as ascent.js): x east, y up, z south, metres, origin on the pad surface under the vehicle's axis.
 * The stack is placed so that the Commander's eye point in the couch is at EYE_H. Proportions follow the Saturn V
 * (stage lengths scaled a little so the stack fits the eye height); paint patterns are simplified. */
import * as THREE from 'three';
import { EYE_H, SUN } from './ascent.js?v=39';
import { canvasTex } from './vehicle.js?v=39';

export const ARM = { floor: EYE_H + 0.15, ceil: EYE_H + 2.55, half: 0.72, z0: -17.9, z1: -4.7 };   /* the walkway */
export const WR = { half: 1.55, z0: -4.7, z1: -1.05 };                                          /* the White Room */
export const HATCH = new THREE.Vector3(0, EYE_H + 0.85, -1.3);                                  /* the open hatch, outside */

export function makePad() {
  const g = new THREE.Group();
  const L = (o) => new THREE.MeshLambertMaterial(o);
  const red = L({ color: 0x8a2a1c }), grey = L({ color: 0x77797c }), white = L({ color: 0xe8e6e0 });

  /* ---------------- the crew access arm and the White Room (they swing together about the tower end) */
  const arm = new THREE.Group(); arm.position.set(ARM.half, 0, ARM.z0); g.add(arm);
  const local = (x, y, z) => new THREE.Vector3(x - ARM.half, y, z - ARM.z0);
  const armFrame = L({ color: 0x9a3324 }), wallW = L({ color: 0xdedcd4, side: THREE.DoubleSide }), floorM = L({ map: gratingTex() }), glass = new THREE.MeshBasicMaterial({ color: 0x9fb4c4, transparent: true, opacity: 0.08, depthWrite: false });
  const box = (w, h, d, m, x, y, z, parent = arm) => { const o = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), m); o.position.copy(local(x, y, z)); parent.add(o); return o; };
  const len = ARM.z1 - ARM.z0, zc = (ARM.z0 + ARM.z1) / 2;
  box(2 * ARM.half + 0.1, 0.12, len, floorM, 0, ARM.floor - 0.06, zc);
  box(2 * ARM.half + 0.3, 0.3, len, armFrame, 0, ARM.floor - 0.3, zc);              /* the truss under the floor */
  box(2 * ARM.half + 0.3, 0.12, len, armFrame, 0, ARM.ceil + 0.06, zc);
  /* walls: a frame with tall windows; the bottom third solid */
  for (const sx of [-1, 1]) {
    const x = sx * (ARM.half + 0.05);
    box(0.06, 0.3, len, wallW, x, ARM.floor + 0.15, zc);                      /* a kick plate; windows above it */
    box(0.06, 0.35, len, wallW, x, ARM.ceil - 0.175, zc);
    for (let z = ARM.z0; z <= ARM.z1 + 0.01; z += 1.65) box(0.08, ARM.ceil - ARM.floor, 0.1, armFrame, x, (ARM.floor + ARM.ceil) / 2, z);
    box(0.02, ARM.ceil - ARM.floor - 0.65, len, glass, x, ARM.floor + 0.3 + (ARM.ceil - ARM.floor - 0.65) / 2, zc);
    box(0.05, 0.05, len, armFrame, x, ARM.floor + 1.05, zc);                   /* a mid rail in the window frames */
    const rail = new THREE.Mesh(new THREE.CylinderGeometry(0.022, 0.022, len, 8), L({ color: 0xc9c6bb })); rail.rotation.x = Math.PI / 2; rail.position.copy(local(sx * (ARM.half - 0.08), ARM.floor + 0.95, zc)); arm.add(rail);
  }
  /* lights along the ceiling */
  for (let z = ARM.z0 + 1; z < ARM.z1; z += 2.5) box(0.5, 0.04, 0.12, new THREE.MeshBasicMaterial({ color: 0xfff6e0 }), 0, ARM.ceil - 0.02, z);
  /* the White Room: a small white room against the spacecraft, lit from above */
  const wrLen = WR.z1 - WR.z0, wzc = (WR.z0 + WR.z1) / 2, wrH = ARM.ceil - ARM.floor + 0.2;
  box(2 * WR.half, 0.12, wrLen, floorM, 0, ARM.floor - 0.06, wzc);
  box(2 * WR.half, 0.1, wrLen, wallW, 0, ARM.floor + wrH, wzc);
  for (const sx of [-1, 1]) box(0.08, wrH, wrLen, wallW, sx * WR.half, ARM.floor + wrH / 2, wzc);
  for (const sx of [-1, 1]) box(WR.half - ARM.half, wrH, 0.08, wallW, sx * (ARM.half + (WR.half - ARM.half) / 2), ARM.floor + wrH / 2, WR.z0);
  const wrLight = new THREE.PointLight(0xfff4e6, 2.2, 7, 1.4); wrLight.position.copy(local(0, ARM.floor + wrH - 0.3, wzc)); arm.add(wrLight);
  box(0.9, 0.04, 0.25, new THREE.MeshBasicMaterial({ color: 0xffffff }), 0, ARM.floor + wrH - 0.04, wzc);
  /* a step and a bar above the hatch */
  box(0.9, 0.22, 0.4, L({ color: 0x6d6f72 }), 0, ARM.floor + 0.11, WR.z1 - 0.3);
  const bar = new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.02, 0.8, 8), L({ color: 0xc9c6bb })); bar.rotation.z = Math.PI / 2; bar.position.copy(local(0, HATCH.y + 0.62, WR.z1 - 0.15)); arm.add(bar);
  /* a stencilled sign */
  const sign = new THREE.Mesh(new THREE.PlaneGeometry(0.9, 0.3), new THREE.MeshBasicMaterial({ map: canvasTex(512, 170, (x, W, H) => {
    x.fillStyle = '#efece4'; x.fillRect(0, 0, W, H); x.fillStyle = '#b02a1e'; x.fillRect(0, 0, W, 34);
    x.fillStyle = '#ffffff'; x.font = 'bold 24px Arial'; x.textAlign = 'center'; x.fillText('CAUTION', W / 2, 26);
    x.fillStyle = '#1c1c1c'; x.font = 'bold 30px Arial'; x.fillText('WHITE ROOM', W / 2, 88); x.font = '22px Arial'; x.fillText('AUTHORIZED PERSONNEL ONLY', W / 2, 134);
  }) }));
  sign.position.copy(local(-WR.half + 0.05, ARM.floor + 1.6, wzc)); sign.rotation.y = Math.PI / 2; arm.add(sign);

  /* ---------------- vapour venting from the upper stages, drifting off on the wind */
  const puffTex = canvasTex(128, 128, (x, W) => {
    const gr = x.createRadialGradient(W / 2, W / 2, 0, W / 2, W / 2, W / 2);
    gr.addColorStop(0, 'rgba(255,255,255,0.55)'); gr.addColorStop(0.5, 'rgba(250,250,250,0.25)'); gr.addColorStop(1, 'rgba(255,255,255,0)');
    x.fillStyle = gr; x.fillRect(0, 0, W, W);
  });
  const vents = [[76.5, 0.8], [90.2, -2.3], [42.5, 2.4]], puffs = [];
  for (const [y, a] of vents) for (let k = 0; k < 9; k++) {
    const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: puffTex, depthWrite: false, transparent: true, opacity: 0.8 }));
    sp.userData = { y, a, phase: k / 9, r: (y > 80 ? 3.4 : 5.1) }; g.add(sp); puffs.push(sp);
  }

  /* ---------------- the propellant spheres at the corners of the pad: liquid oxygen (north-west), liquid hydrogen (north-east) */
  for (const [x, z, r] of [[-290, -260, 11.5], [300, -240, 10.5]]) {
    const sph = new THREE.Mesh(new THREE.SphereGeometry(r, 32, 20), L({ color: 0xe9e7e0 })); sph.position.set(x, r + 3, z); g.add(sph);
    for (let k = 0; k < 6; k++) { const a = k * Math.PI / 3, leg = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, r + 3, 6), grey); leg.position.set(x + Math.cos(a) * r * 0.85, (r + 3) / 2, z + Math.sin(a) * r * 0.85); g.add(leg); }
  }
  /* ---------------- far away: the Vehicle Assembly Building (5 km west), 1972 paint */
  const vab = new THREE.Mesh(new THREE.BoxGeometry(218, 160, 158), [L({ color: 0xbdbab1 }), L({ color: 0xbdbab1 }), L({ color: 0xc9c6bc }), L({ color: 0xbdbab1 }),
    L({ map: canvasTex(512, 512, (x, W, H) => { x.fillStyle = '#bdbab1'; x.fillRect(0, 0, W, H); x.fillStyle = '#7d7b75'; for (let q = 0; q < 4; q++) x.fillRect(40 + q * 120, 90, 60, H - 90); }) }),
    L({ color: 0xbdbab1 })]);
  vab.position.set(-4600, 80, 2400); g.add(vab);

  /* ---------------- steam and smoke at ignition: the deluge water flashing to steam, blown out of both ends of the
   * flame trench (north and south) and rolling up round the base; lit orange by the flame while it is close */
  const smokeTex = canvasTex(256, 256, (x, W) => {
    let seed = 3; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    for (let k = 0; k < 40; k++) {
      const r = W * (0.12 + 0.18 * rnd()), cx = W / 2 + (rnd() - 0.5) * W * 0.45, cy = W / 2 + (rnd() - 0.5) * W * 0.45;
      const gr = x.createRadialGradient(cx, cy - r * 0.25, 0, cx, cy, r);
      gr.addColorStop(0, 'rgba(255,255,255,0.32)'); gr.addColorStop(0.7, 'rgba(225,225,228,0.16)'); gr.addColorStop(1, 'rgba(200,200,205,0)');
      x.fillStyle = gr; x.beginPath(); x.arc(cx, cy, r, 0, 7); x.fill();
    }
  });
  const smoke = [];
  {
    let seed = 11; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    for (let k = 0; k < 90; k++) {
      const kind = k < 70 ? 'trench' : 'base', dir = rnd() < 0.5 ? -1 : 1;
      const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: smokeTex, transparent: true, depthWrite: false, opacity: 0, rotation: rnd() * 6.3 }));
      sp.userData = { kind, dir, birth: -8.5 + rnd() * (kind === 'trench' ? 14 : 9), spread: (rnd() - 0.5) * 2, lift: 0.6 + rnd() * 0.8, size: 0.7 + rnd() * 0.7, ang: rnd() * 6.3 };
      sp.visible = false; g.add(sp); smoke.push(sp);
    }
  }
  function updateSmoke(t) {
    for (const sp of smoke) {
      const u = sp.userData, a = t - u.birth;
      if (a <= 0 || a > 150) { sp.visible = false; continue; }
      sp.visible = true;
      const grow = 1 - Math.exp(-a / 7);
      if (u.kind === 'trench') {
        const dist = 75 + 260 * grow, y = 6 + (35 + 30 * u.lift) * (1 - Math.exp(-a / 14));
        sp.position.set(u.spread * (20 + 90 * grow), y, u.dir * dist);
      } else {
        const r = 22 + 70 * grow;
        sp.position.set(Math.cos(u.ang) * r, 8 + 30 * u.lift * grow, Math.sin(u.ang) * r);
      }
      sp.position.x += a * 1.2;                                    /* the wind from the west carries it out to sea */
      sp.scale.setScalar((18 + 55 * grow) * u.size);
      const lit = Math.max(0, 1 - Math.max(0, t) / 14) * Math.exp(-Math.max(0, sp.position.length() - 40) / 120);
      sp.material.color.setRGB(0.92 + 0.25 * lit, 0.9 + 0.05 * lit, 0.9 - 0.35 * lit);
      sp.material.opacity = 0.85 * Math.min(1, a / 1.5) * Math.min(1, (150 - a) / 60);
    }
  }

  /* ---------------- per frame */
  let swing = 0;
  function update(t, dt, mode, tMission) {
    updateSmoke(tMission);
    /* the arm: against the spacecraft while boarding, swung back to its park position (12 deg) afterwards */
    const target = mode === 'boarding' || mode === 'seating' ? 0 : 12 * Math.PI / 180;
    swing += (target - swing) * Math.min(1, dt * 0.25);
    arm.rotation.y = -swing;
    for (const p of puffs) {
      const u = p.userData, f = (p.userData.phase + t * 0.07) % 1;
      p.position.set(Math.cos(u.a) * (u.r + 0.5 + f * 9) + f * 14, u.y + f * 3, Math.sin(u.a) * (u.r + 0.5 + f * 9));
      p.scale.setScalar(1.2 + f * 7);
      p.material.opacity = 0.7 * Math.sin(Math.PI * f) * (t > 0 ? 0 : 1);
    }
  }
  return { group: g, update, arm };
}

function gratingTex() {
  return canvasTex(256, 256, (x, W, H) => {
    x.fillStyle = '#5b5d60'; x.fillRect(0, 0, W, H);
    x.fillStyle = '#2a2b2d'; for (let i = 0; i < W; i += 16) x.fillRect(i, 0, 6, H);
    x.fillStyle = '#6a6c70'; for (let j = 0; j < H; j += 64) x.fillRect(0, j, W, 5);
  }, 6);
}
