/* The pad as you see it while boarding: the Saturn V beside you, the crew access arm (swing arm 9) you walk along,
 * the White Room at its end against the Command Module, and the Vehicle Assembly Building in the distance.
 *
 * Pad frame (as ascent.js): x east, y up, z south, metres, origin on the pad surface under the vehicle's axis.
 * The stack is placed so that the Commander's eye point in the couch is at EYE_H. Proportions follow the Saturn V
 * (stage lengths scaled a little so the stack fits the eye height); paint patterns are simplified. */
import * as THREE from 'three';
import { EYE_H, SUN } from './ascent.js?v=9';

/* heights (pad frame y) */
export const STACK = {
  base: 8.5, sicTop: 50.5, siiTop: 75, sivbAft: 80.5, sivbTop: 91.2, iuTop: 92.1, slaTop: 100.6,
  smTop: EYE_H - 1.0, cmTop: EYE_H + 2.4, lesTop: EYE_H + 13.5,
  R1: 5.05, R3: 3.3, RSM: 1.96
};
export const ARM = { floor: EYE_H + 0.15, ceil: EYE_H + 2.55, half: 0.72, z0: -17.9, z1: -4.7 };   /* the walkway */
export const WR = { half: 1.55, z0: -4.7, z1: -1.05 };                                          /* the White Room */
export const HATCH = new THREE.Vector3(0, EYE_H + 0.85, -1.3);                                  /* the open hatch, outside */

function canvasTex(w, h, draw, repeatU = 1) {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8;
  t.wrapS = THREE.RepeatWrapping; t.repeat.x = repeatU;
  return t;
}
/* white with the black roll-pattern panels and a little grime */
function stageTex(kind) {
  return canvasTex(2048, 1024, (x, W, H) => {
    x.fillStyle = '#ecebe6'; x.fillRect(0, 0, W, H);
    let seed = kind.length * 77;
    const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    for (let k = 0; k < 300; k++) { x.fillStyle = `rgba(110,105,95,${0.02 + rnd() * 0.03})`; x.fillRect(rnd() * W, rnd() * H, 4 + rnd() * 60, 40 + rnd() * 300); }
    x.fillStyle = '#141414';
    const v = f => H - f * H;                                           /* f = height fraction from the bottom */
    if (kind === 'sic') {
      for (let q = 0; q < 4; q += 2) x.fillRect(q * W / 4, v(0.16), W / 4, 0.16 * H);        /* lower quadrants */
      for (let q = 0; q < 8; q++) if (q % 2) x.fillRect(q * W / 8, v(0.62), W / 8, 0.1 * H); /* intertank */
      x.fillRect(0, v(1), W, 0.025 * H);
      x.save(); x.font = 'bold 120px Arial'; x.fillStyle = '#151515'; x.textAlign = 'center';
      const word = 'UNITED STATES';
      for (let i = 0; i < word.length; i++) x.fillText(word[i], W * 0.375, v(0.5) + (i - 6) * 118);
      x.restore();
      drawFlag(x, W * 0.375 - 110, v(0.95), 220, 125);
    } else if (kind === 'sii') {
      for (let q = 0; q < 8; q++) if (q % 2) x.fillRect(q * W / 8, v(0.22), W / 8, 0.22 * H);
      x.fillRect(0, v(1), W, 0.02 * H);
    } else if (kind === 'sivb') {
      for (let q = 0; q < 4; q++) if (q % 2) x.fillRect(q * W / 4, v(0.36), W / 4, 0.36 * H);
    } else if (kind === 'iu') {
      x.fillStyle = '#dcdcd6'; x.fillRect(0, 0, W, H);
      x.fillStyle = '#1a1a1a'; for (let q = 0; q < 16; q++) if (q % 4 === 1) x.fillRect(q * W / 16, 0, W / 16, H);
    } else if (kind === 'sla') {
      x.strokeStyle = '#9a9a96'; x.lineWidth = 6;
      for (let q = 0; q < 4; q++) { x.beginPath(); x.moveTo(q * W / 4, 0); x.lineTo(q * W / 4, H); x.stroke(); }
      for (let r = 1; r < 6; r++) { x.beginPath(); x.moveTo(0, r * H / 6); x.lineTo(W, r * H / 6); x.stroke(); }
    } else if (kind === 'sm') {
      x.fillStyle = '#c8cbcf'; x.fillRect(0, 0, W, H);
      x.fillStyle = '#f2f2f0'; for (let q = 0; q < 6; q++) x.fillRect(q * W / 6 + 30, 60, W / 6 - 160, H - 120);     /* radiator panels */
      x.strokeStyle = '#8a8d92'; x.lineWidth = 3; for (let q = 0; q < 24; q++) { x.beginPath(); x.moveTo(q * W / 24, 0); x.lineTo(q * W / 24, H); x.stroke(); }
    } else if (kind === 'bpc') {
      x.fillStyle = '#e4e1d8'; x.fillRect(0, 0, W, H);
      x.strokeStyle = '#b7b3a8'; x.lineWidth = 4;
      for (let q = 0; q < 8; q++) { x.beginPath(); x.moveTo(q * W / 8, 0); x.lineTo(q * W / 8, H); x.stroke(); }
      x.fillStyle = 'rgba(120,112,98,0.25)'; for (let k = 0; k < 120; k++) x.fillRect(rnd() * W, rnd() * H, 6, 6);
    }
  });
}
function drawFlag(x, px, py, w, h) {
  for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f4f2ec' : '#b22234'; x.fillRect(px, py + k * h / 13, w, h / 13 + 0.5); }
  x.fillStyle = '#3c3b6e'; x.fillRect(px, py, w * 0.4, h * 0.54);
}

export function makePad() {
  const g = new THREE.Group();
  const lights = new THREE.Group(); g.add(lights);
  const sun = new THREE.DirectionalLight(0xfff3e2, 2.2); sun.position.fromArray(SUN).multiplyScalar(100); lights.add(sun, sun.target);
  lights.add(new THREE.HemisphereLight(0xa9c4e8, 0x6b6250, 0.75));
  const L = (o) => new THREE.MeshLambertMaterial(o);
  const S = STACK;
  const cyl = (r0, r1, y0, y1, mat, seg = 48, open = true) => {
    const m = new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, y1 - y0, seg, 1, open), mat);
    m.position.y = (y0 + y1) / 2; g.add(m); return m;
  };

  /* ---------------- the Saturn V */
  cyl(S.R1, S.R1, S.base, S.sicTop, L({ map: stageTex('sic') }));
  cyl(S.R1, S.R1, S.sicTop, S.siiTop, L({ map: stageTex('sii') }));
  cyl(S.R1, S.R3, S.siiTop, S.sivbAft, L({ map: stageTex('sivb') }));
  cyl(S.R3, S.R3, S.sivbAft, S.sivbTop, L({ color: 0xecebe6 }));
  cyl(S.R3, S.R3, S.sivbTop, S.iuTop, L({ map: stageTex('iu') }));
  cyl(S.R3, S.RSM, S.iuTop, S.slaTop, L({ map: stageTex('sla') }));
  cyl(S.RSM, S.RSM, S.slaTop, S.smTop, L({ map: stageTex('sm') }));
  /* fins and engine fairings at the bottom */
  const black = L({ color: 0x1a1a1a }), white = L({ color: 0xe8e6e0 }), red = L({ color: 0x8a2a1c }), grey = L({ color: 0x77797c });
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + Math.PI / 4;
    const fair = new THREE.Mesh(new THREE.CylinderGeometry(0.9, 1.25, 9, 16), black); fair.position.set(Math.cos(a) * 4.6, S.base + 4.5, Math.sin(a) * 4.6); g.add(fair);
    const fin = new THREE.Mesh(new THREE.BoxGeometry(0.25, 6, 3.4), white); fin.position.set(Math.cos(a) * 6.3, S.base + 3, Math.sin(a) * 6.3); fin.rotation.y = -a; g.add(fin);
  }
  /* the service module's RCS quads */
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + 0.1, q = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.75, 0.3), L({ color: 0xbfc2c6 }));
    q.position.set(Math.cos(a) * 2.08, S.smTop - 1.2, Math.sin(a) * 2.08); q.rotation.y = -a; g.add(q);
  }

  /* ---------------- the Command Module under its boost protective cover, with the hatch open; the escape tower */
  const cmExterior = new THREE.Group(); g.add(cmExterior);
  const bpcMat = L({ map: stageTex('bpc'), side: THREE.DoubleSide });
  /* the cover as a cone with an opening over the hatch (left out of the mesh) */
  {
    const NA = 96, NY = 24, y0 = S.smTop, y1 = S.cmTop + 0.1, r0 = 2.04, r1 = 0.5, pos = [], uv = [], idx = [];
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NA; i++) {
      const y = y0 + (y1 - y0) * j / NY, r = r0 + (r1 - r0) * j / NY, a = i / NA * Math.PI * 2;
      pos.push(Math.sin(a) * r, y, Math.cos(a) * r); uv.push(i / NA, j / NY);
    }
    for (let j = 0; j < NY; j++) for (let i = 0; i < NA; i++) {
      const a = (i + 0.5) / NA * 360, y = y0 + (y1 - y0) * (j + 0.5) / NY;
      const da = Math.abs(((a - 180 + 540) % 360) - 180);        /* 180 deg = -z, the north side, the hatch */
      if (da < 22 && y > HATCH.y - 0.48 && y < HATCH.y + 0.42) continue;
      const p = j * (NA + 1) + i, q = p + NA + 1;
      idx.push(p, q, p + 1, q, q + 1, p + 1);
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2)); geo.setIndex(idx);
    geo.computeVertexNormals();
    cmExterior.add(new THREE.Mesh(geo, bpcMat));
    /* the cover's own hatch, swung open beside the opening */
    const bh = new THREE.Mesh(new THREE.BoxGeometry(0.78, 0.86, 0.04), bpcMat);
    const hinge = new THREE.Group(); hinge.position.set(0.42, HATCH.y - 0.03, -1.32); hinge.rotation.y = 1.9; cmExterior.add(hinge);
    bh.position.set(-0.39, 0, 0); hinge.add(bh);
  }
  /* escape tower: a red truss, the motor, the nose */
  const lesY0 = S.cmTop + 0.1, lesY1 = S.lesTop - 4;
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + Math.PI / 4, b = new THREE.Vector3(Math.cos(a) * 0.95, lesY0, Math.sin(a) * 0.95), t = new THREE.Vector3(Math.cos(a) * 0.45, lesY1, Math.sin(a) * 0.45);
    const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, b.distanceTo(t), 6), red);
    leg.position.copy(b).add(t).multiplyScalar(0.5); leg.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), t.clone().sub(b).normalize()); cmExterior.add(leg);
  }
  for (let y = lesY0 + 1.2; y < lesY1; y += 1.4) { const r = 0.95 - 0.5 * (y - lesY0) / (lesY1 - lesY0), ring = new THREE.Mesh(new THREE.TorusGeometry(r, 0.035, 4, 4), red); ring.position.y = y; ring.rotation.x = Math.PI / 2; ring.rotation.z = Math.PI / 4; cmExterior.add(ring); }
  const motor = new THREE.Mesh(new THREE.CylinderGeometry(0.33, 0.33, 3.4, 20), white); motor.position.y = lesY1 + 1.7; cmExterior.add(motor);
  const band = new THREE.Mesh(new THREE.CylinderGeometry(0.335, 0.335, 0.5, 20), black); band.position.y = lesY1 + 0.6; cmExterior.add(band);
  const nose = new THREE.Mesh(new THREE.ConeGeometry(0.33, 1.2, 20), black); nose.position.y = lesY1 + 4; cmExterior.add(nose);

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
  const vents = [[S.siiTop + 1.5, 0.8], [S.sivbTop - 1.0, -2.3], [S.base + 34, 2.4]], puffs = [];
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

  /* ---------------- per frame */
  let swing = 0;
  function update(t, dt, mode) {
    /* the arm: against the spacecraft while boarding, swung back to its park position (12 deg) afterwards */
    const target = mode === 'boarding' || mode === 'seating' ? 0 : 12 * Math.PI / 180;
    swing += (target - swing) * Math.min(1, dt * 0.25);
    arm.rotation.y = -swing;
    cmExterior.visible = mode === 'boarding' || mode === 'seating';
    for (const p of puffs) {
      const u = p.userData, f = (p.userData.phase + t * 0.07) % 1;
      p.position.set(Math.cos(u.a) * (u.r + 0.5 + f * 9) + f * 14, u.y + f * 3, Math.sin(u.a) * (u.r + 0.5 + f * 9));
      p.scale.setScalar(1.2 + f * 7);
      p.material.opacity = 0.7 * Math.sin(Math.PI * f) * (t > 0 ? 0 : 1);
    }
  }
  return { group: g, update, cmExterior, arm, lights };
}

function gratingTex() {
  return canvasTex(256, 256, (x, W, H) => {
    x.fillStyle = '#5b5d60'; x.fillRect(0, 0, W, H);
    x.fillStyle = '#2a2b2d'; for (let i = 0; i < W; i += 16) x.fillRect(i, 0, 6, H);
    x.fillStyle = '#6a6c70'; for (let j = 0; j < H; j += 64) x.fillRect(0, j, W, 5);
  }, 6);
}
