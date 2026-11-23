/* The Orrery (milestone M1): the system on a table. Orbit distances are logarithmic (stated on screen), sizes are
 * not to scale. Habitable-zone rings (conservative dark green, optimistic pale green), the snow line, the tidal-lock
 * radius (dashed), orbits coloured by eccentricity, planets as beads that can be dragged (distance) with a handle
 * at apoastron (eccentricity and orientation), and a glowing slot where a second HZ planet would sit stably. */
import * as THREE from 'three';
import { orbitPosition, mutualHillSpacing, HILL_MULTI, tidalLockGyr, ECC_MAX } from '../physics/orbit.js?v=11';
import { habitableZone } from '../physics/hz.js?v=11';

export const A_MIN = 0.005;
export const rOf = (a) => 2.2 + 6.5 * Math.log10(Math.max(a, A_MIN) / A_MIN);   /* AU -> scene units */
export const aOf = (r) => A_MIN * 10 ** ((r - 2.2) / 6.5);

const TYPE_COLOR = { hz: 0x5aa0e0, rocky: 0x9a8f84, icy: 0xdde8f0, iceGiant: 0x6fc4d8, gasGiant: 0xd8b48a };

function ring(r0, r1, color, opacity) {
  const g = new THREE.RingGeometry(r0, r1, 256, 1);
  g.rotateX(-Math.PI / 2);
  return new THREE.Mesh(g, new THREE.MeshBasicMaterial({ color, transparent: true, opacity, depthWrite: false, side: THREE.DoubleSide }));
}
function circleLine(r, color, dashed) {
  const pts = [];
  for (let i = 0; i <= 256; i++) { const t = i / 256 * Math.PI * 2; pts.push(new THREE.Vector3(Math.cos(t) * r, 0.002, Math.sin(t) * r)); }
  const g = new THREE.BufferGeometry().setFromPoints(pts);
  const m = dashed ? new THREE.LineDashedMaterial({ color, dashSize: 0.25, gapSize: 0.2, transparent: true, opacity: 0.8 })
    : new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.8 });
  const l = new THREE.Line(g, m);
  if (dashed) l.computeLineDistances();
  return l;
}
/* Ellipse with the star at a focus, each point's distance mapped through rOf, angle kept. */
function ellipsePoints(a, e, argPeri) {
  const pts = [];
  for (let i = 0; i <= 256; i++) {
    const M = i / 256 * Math.PI * 2;
    const { x, y } = orbitPosition(a, e, M);
    const r = Math.hypot(x, y), th = Math.atan2(y, x) + argPeri;
    const R = rOf(r);
    pts.push(new THREE.Vector3(Math.cos(th) * R, 0, -Math.sin(th) * R));
  }
  return pts;
}

export function createOrrery() {
  const group = new THREE.Group();
  const dyn = new THREE.Group();     /* rebuilt on every design change */
  group.add(dyn);
  const light = new THREE.PointLight(0xffffff, 60, 0, 1.2);
  group.add(light);
  group.add(new THREE.AmbientLight(0x404858, 0.35));

  const beads = [];                   /* { mesh, index, a, e, argPeri, period, handle } */
  let slot = null;
  let phase = [];

  function update(design, D) {
    dyn.clear();
    beads.length = 0;
    const st = D.star;
    light.color.setRGB(st.rgb[0], st.rgb[1], st.rgb[2]);
    /* HZ rings */
    const hz = st.hz;
    const valid = hz.valid;
    dyn.add(ring(rOf(hz.d.recentVenus), rOf(hz.d.earlyMars), valid ? 0x4fae6a : 0x6a6a6a, 0.16));
    dyn.add(ring(rOf(hz.d.runaway), rOf(hz.d.maxGreenhouse), valid ? 0x1f7a3c : 0x555555, 0.42));
    /* snow line, frosty */
    const sn = rOf(st.snowLineAU);
    dyn.add(ring(sn - 0.03, sn + 0.03, 0xcfe8ff, 0.5));
    /* tidal-lock radius for an Earth-like planet at the star's age (nominal Q) */
    const t1 = tidalLockGyr(1, st.massSun, 1, 1);
    const aLock = (design.star.ageGyr / t1) ** (1 / 6);
    if (aLock > A_MIN) dyn.add(circleLine(rOf(aLock), 0xff9a6a, true));
    /* distance ticks every decade: 0.01, 0.1, 1, 10 AU */
    for (const a of [0.01, 0.1, 1, 10]) dyn.add(circleLine(rOf(a), 0x2a3646, false));

    /* planets */
    phase.length = design.planets.length;
    design.planets.forEach((p, i) => {
      const Dp = D.planets[i];
      const argPeri = (p.orbit.argPeriDeg || 0) * Math.PI / 180;
      const e = p.orbit.e;
      const leaves = p.role === 'hz' && (!Dp.periInHZ || !Dp.apoInHZ);
      const ecol = leaves || e > 0.3 ? 0xe45d5d : e > 0.1 ? 0xe8b04a : 0x6f8fb0;
      const orbit = new THREE.Line(new THREE.BufferGeometry().setFromPoints(ellipsePoints(p.orbit.aAU, e, argPeri)),
        new THREE.LineBasicMaterial({ color: ecol, transparent: true, opacity: 0.85 }));
      dyn.add(orbit);
      const type = p.role === 'hz' ? 'hz' : p.type;
      const size = 0.16 + 0.16 * Math.sqrt(Math.min(Dp.radiusEarth || 1, 12));
      const mesh = new THREE.Mesh(new THREE.SphereGeometry(size, 32, 16),
        new THREE.MeshStandardMaterial({ color: TYPE_COLOR[type], roughness: 0.7, metalness: 0,
          emissive: TYPE_COLOR[type], emissiveIntensity: 0.06 }));
      mesh.userData = { kind: 'bead', index: i };
      dyn.add(mesh);
      if (p.role === 'hz') {
        const halo = new THREE.Mesh(new THREE.RingGeometry(size * 1.5, size * 1.75, 48),
          new THREE.MeshBasicMaterial({ color: 0x7fe0a0, transparent: true, opacity: 0.8, side: THREE.DoubleSide, depthWrite: false }));
        halo.rotation.x = -Math.PI / 2;
        mesh.add(halo);
      }
      /* eccentricity handle at apoastron */
      const ra = rOf(p.orbit.aAU * (1 + e));
      const handle = new THREE.Mesh(new THREE.OctahedronGeometry(0.16),
        new THREE.MeshBasicMaterial({ color: ecol }));
      handle.position.set(-Math.cos(argPeri) * ra, 0, Math.sin(argPeri) * ra);
      handle.userData = { kind: 'handle', index: i };
      dyn.add(handle);
      if (phase[i] === undefined) phase[i] = Math.random() * Math.PI * 2;
      beads.push({ mesh, index: i, a: p.orbit.aAU, e, argPeri, period: Dp.periodYr, handle });
    });

    /* the slot for a second (or first) HZ planet */
    slot = null;
    const hzCount = design.planets.filter((p) => p.role === 'hz').length;
    if (hzCount < 2 && design.planets.length < 8) {
      const z = habitableZone(st.teff, st.lum, 1);
      let best = null;
      for (let k = 0; k <= 60; k++) {
        const a = z.d.runaway * (z.d.maxGreenhouse / z.d.runaway) ** (0.05 + 0.9 * k / 60);
        const cand = { aAU: a, e: 0, massEarth: 1 };
        let minS = Infinity;
        for (const p of design.planets) minS = Math.min(minS, mutualHillSpacing(cand, { aAU: p.orbit.aAU, e: p.orbit.e, massEarth: p.bulk.massEarth }, st.massSun));
        if (minS >= HILL_MULTI && (!best || minS > best.minS)) best = { a, minS };
      }
      if (best) {
        const m = new THREE.Mesh(new THREE.SphereGeometry(0.32, 32, 16), new THREE.MeshBasicMaterial({
          color: new THREE.Color(0.5, 2.2, 1.0), transparent: true, opacity: 0.55, depthWrite: false }));
        const R = rOf(best.a);
        m.position.set(0, 0.05, R);
        m.userData = { kind: 'slot', a: best.a };
        dyn.add(m);
        slot = m;
      }
    }
  }

  /* simYears: simulated time in years (the orbits move in true proportion to their periods); realT for the pulse */
  function tick(simYears, realT) {
    for (const b of beads) {
      const M = phase[b.index] + 2 * Math.PI * simYears / Math.max(b.period, 1e-4);
      const { x, y } = orbitPosition(b.a, b.e, M);
      const r = Math.hypot(x, y), th = Math.atan2(y, x) + b.argPeri;
      const R = rOf(r);
      b.mesh.position.set(Math.cos(th) * R, 0, -Math.sin(th) * R);
    }
    if (slot) slot.material.opacity = 0.35 + 0.25 * Math.sin((realT || 0) * 3);
  }

  const pickables = () => {
    const out = [];
    dyn.traverse((o) => { if (o.userData && o.userData.kind) out.push(o); });
    return out;
  };

  /* Turn a point on the table into orbit parameters */
  function toA(point) { return aOf(Math.hypot(point.x, point.z)); }
  function toE(point, a) {
    const r = aOf(Math.hypot(point.x, point.z));
    return Math.min(ECC_MAX, Math.max(0, r / a - 1));
  }
  function toArgPeri(point) { return Math.atan2(point.z, -point.x) * 180 / Math.PI; }

  return { group, update, tick, pickables, toA, toE, toArgPeri, beads, get slot() { return slot; } };
}
