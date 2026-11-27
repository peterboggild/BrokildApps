/* The Command Module's crew compartment, from the Commander's couch (the left one).
 *
 * Frame (the rig's): your eyes at the origin, -Z ahead (the nose of the spacecraft, the apex of the cone),
 * +Y the way your head points (the hatch side), +X to your right (the other two couches). The crew lay on their
 * backs; here you sit upright and the cabin is turned round you so the main display console is in front.
 *
 * Proportions follow the Block II command module (a 33 degree cone; five windows: the hatch window over the
 * centre couch, two forward-looking rendezvous windows, a side window each side); details are simplified.
 * Everything here writes the stencil, so the world outside is only drawn through the window openings. */
import * as THREE from 'three';
import { GLTFLoader } from '../vendor/GLTFLoader.js';
import { makeCrewLife } from './crew-life.js?v=41';

const NO_STENCIL = {};
export const CABIN_STENCIL = { stencilWrite: true, stencilRef: 1, stencilFunc: THREE.AlwaysStencilFunc,
  stencilFail: THREE.ReplaceStencilOp, stencilZFail: THREE.ReplaceStencilOp, stencilZPass: THREE.ReplaceStencilOp };

const ALPHA = 33 * Math.PI / 180, TANA = Math.tan(ALPHA);
const AX = 0.6, AY = -0.15;                 /* the cone's axis passes here (x, y), along -Z */
const Z_BASE = 0.5, R_BASE = 1.95;          /* the aft bulkhead behind you */
const Z_APEX = Z_BASE - (R_BASE - 0.36) / TANA;
const rAt = z => R_BASE - (Z_BASE - z) * TANA;

/* windows: azimuth round the axis (0 = +X, 90 = +Y), z of the centre, half sizes along the wall (m), shape */
const WINDOWS = [
  { name: 'hatch', phi: 90, z: -0.85, hu: 0.135, hv: 0.135, round: true },
  { name: 'rendezvous-left', phi: 130, z: -1.15, hu: 0.10, hv: 0.115 },
  { name: 'rendezvous-right', phi: 50, z: -1.15, hu: 0.10, hv: 0.115 },
  { name: 'side-left', phi: 180, z: -0.25, hu: 0.165, hv: 0.165 },
  { name: 'side-right', phi: 0, z: -0.25, hu: 0.165, hv: 0.165 }
];

function mat(color, opts = {}) { return new THREE.MeshLambertMaterial({ color, ...NO_STENCIL, ...opts }); }

export function makeCabin() {
  const g = new THREE.Group();                 /* shaken as a whole; you are not */
  const wall = mat(0x9b998f), dark = mat(0x3a3c3e), metal = mat(0x7c8086), bezel = mat(0x26282a), cushion = mat(0x6e6a5c);
  const point = (phi, z, out = 0) => {        /* a point on the wall (out: metres outward along the wall normal) */
    const a = phi * Math.PI / 180, r = rAt(z);
    const nr = Math.cos(ALPHA), nz = -Math.sin(ALPHA);
    return new THREE.Vector3(AX + Math.cos(a) * (r + nr * out), AY + Math.sin(a) * (r + nr * out), z + nz * out);
  };

  /* ---------------- the conical wall, with the window openings left out */
  const NA = 200, NZ = 80, pos = [], nor = [], idx = [], wuv = [];
  for (let j = 0; j <= NZ; j++) for (let i = 0; i <= NA; i++) {
    const z = Z_BASE + (Z_APEX + 0.05 - Z_BASE) * j / NZ, a = i / NA * 360;
    const p = point(a, z), ar = a * Math.PI / 180;
    pos.push(p.x, p.y, p.z);
    wuv.push(1 - i / NA, j / NZ);          /* mirrored: the wall is seen from inside */
    nor.push(-Math.cos(ar) * Math.cos(ALPHA), -Math.sin(ar) * Math.cos(ALPHA), Math.sin(ALPHA));
  }
  /* the unified crew hatch: a curved door in the wall round the hatch window, hinged on its +X side */
  const HD = { phi: 90, z0: -1.22, z1: -0.45, hu: 0.36 };
  const inHatch = (a, z) => { const da = ((a - HD.phi + 540) % 360) - 180; return Math.abs(da * Math.PI / 180 * rAt(z)) < HD.hu && z > HD.z0 && z < HD.z1; };
  const doorIdx = [], cheatIdx = [];
  /* the "cheat window": not in any Apollo spacecraft. A large opening in the wall to your left, opened with Y */
  const CW = { a0: 152, a1: 208, z0: -0.95, z1: 0.12 };
  const inCheat = (a, z) => a > CW.a0 && a < CW.a1 && z > CW.z0 && z < CW.z1;
  const inWindow = (a, z) => WINDOWS.some(w => {
    let da = ((a - w.phi + 540) % 360) - 180;
    const du = da * Math.PI / 180 * rAt(z), dv = (z - w.z) / Math.cos(ALPHA);
    return w.round ? (du * du + dv * dv) < w.hu * w.hu : Math.abs(du) < w.hu && Math.abs(dv) < w.hv;
  });
  for (let j = 0; j < NZ; j++) for (let i = 0; i < NA; i++) {
    const z = Z_BASE + (Z_APEX + 0.05 - Z_BASE) * (j + 0.5) / NZ, a = (i + 0.5) / NA * 360;
    if (inWindow(a, z)) continue;
    const p = j * (NA + 1) + i, q = p + NA + 1;
    (inHatch(a, z) ? doorIdx : inCheat(a, z) ? cheatIdx : idx).push(p, p + 1, q, q, p + 1, q + 1);
  }
  const wg = new THREE.BufferGeometry();
  wg.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); wg.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3)); wg.setIndex(idx);
  wg.setAttribute('uv', new THREE.Float32BufferAttribute(wuv, 2));
  const wallTex = makeWallTexture(), wallMat = new THREE.MeshLambertMaterial({ map: wallTex, ...NO_STENCIL });
  g.add(new THREE.Mesh(wg, wallMat));
  const cheatG = wg.clone(); cheatG.setIndex(cheatIdx);
  const cheatWall = new THREE.Mesh(cheatG, wallMat); g.add(cheatWall);
  /* its rim, shown when it is open */
  const cheatRim = new THREE.Group(); cheatRim.visible = false; g.add(cheatRim);
  {
    const pts = [];
    for (let a = CW.a0; a <= CW.a1; a += 2) pts.push(point(a, CW.z1, -0.01));
    for (let z = CW.z1; z >= CW.z0; z -= 0.04) pts.push(point(CW.a1, z, -0.01));
    for (let a = CW.a1; a >= CW.a0; a -= 2) pts.push(point(a, CW.z0, -0.01));
    for (let z = CW.z0; z <= CW.z1; z += 0.04) pts.push(point(CW.a0, z, -0.01));
    cheatRim.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts, true), 240, 0.03, 8, true), new THREE.MeshLambertMaterial({ color: 0x8a1c14 })));
  }
  /* the door: the same wall cells, in a group that turns about the hinge line (a generator of the cone) */
  const hingeA = HD.phi - (HD.hu / rAt((HD.z0 + HD.z1) / 2)) * 180 / Math.PI;
  const h0 = point(hingeA, HD.z1), h1 = point(hingeA, HD.z0), hingeAxis = h1.clone().sub(h0).normalize();
  const door = new THREE.Group(); door.position.copy(h0); g.add(door);
  const dg = wg.clone(); dg.setIndex(doorIdx); dg.translate(-h0.x, -h0.y, -h0.z);
  const doorMesh = new THREE.Mesh(dg, wallMat); door.add(doorMesh);
  const doorBack = new THREE.Mesh(dg, new THREE.MeshLambertMaterial({ color: 0xd9d6cc, side: THREE.BackSide, ...NO_STENCIL }));
  doorBack.scale.setScalar(1.0); door.add(doorBack);
  /* aft bulkhead behind you, and the tunnel end ahead */
  const aft = new THREE.Mesh(new THREE.CircleGeometry(R_BASE, 64), new THREE.MeshLambertMaterial({ map: makeLockerTexture(), ...NO_STENCIL }));
  aft.position.set(AX, AY, Z_BASE); aft.rotation.y = Math.PI; g.add(aft);
  dress(g, point, { metal, dark, bezel }, door, h0);
  const fwd = new THREE.Mesh(new THREE.CircleGeometry(0.4, 32), dark); fwd.position.set(AX, AY, Z_APEX + 0.06); g.add(fwd);

  /* ---------------- windows: a deep reveal through the wall, a bezel round it, and the boost protective cover
   * outside (a dark surface 15 cm out; over the hatch window it has a 20 cm hole) */
  const covers = [], frames = {};
  for (const w of WINDOWS) {
    const centre = point(w.phi, w.z), a = w.phi * Math.PI / 180;
    const nOut = new THREE.Vector3(Math.cos(a) * Math.cos(ALPHA), Math.sin(a) * Math.cos(ALPHA), -Math.sin(ALPHA));
    const tU = new THREE.Vector3(-Math.sin(a), Math.cos(a), 0);
    const tV = new THREE.Vector3().crossVectors(nOut, tU).negate();
    const frame = new THREE.Group();
    frame.matrixAutoUpdate = false;
    frame.matrix.makeBasis(tU, tV, nOut).setPosition(centre);
    /* reveal: the window sits 13 cm out in a thick wall; a frame extruded outwards from the opening, and a
     * bezel on the inside face of the wall */
    const depth = 0.13;
    const outline = (sh, grow) => {
      if (w.round) sh.absarc(0, 0, w.hu + grow, 0, Math.PI * 2, false);
      else { sh.moveTo(-w.hu - grow, -w.hv - grow); sh.lineTo(w.hu + grow, -w.hv - grow); sh.lineTo(w.hu + grow, w.hv + grow); sh.lineTo(-w.hu - grow, w.hv + grow); sh.closePath(); }
      return sh;
    };
    const ring = (inner, outer) => {
      const sh = outline(new THREE.Shape(), outer);
      const hole = new THREE.Path();
      if (w.round) hole.absarc(0, 0, w.hu + inner, 0, Math.PI * 2, true);
      else { hole.moveTo(-w.hu - inner, -w.hv - inner); hole.lineTo(-w.hu - inner, w.hv + inner); hole.lineTo(w.hu + inner, w.hv + inner); hole.lineTo(w.hu + inner, -w.hv - inner); hole.closePath(); }
      sh.holes.push(hole);
      return sh;
    };
    const revealGeo = new THREE.ExtrudeGeometry(ring(0, 0.012), { depth, bevelEnabled: false, curveSegments: 40 });
    if (w.name.startsWith('rendezvous')) {
      /* the rendezvous windows look forward: their reveal runs along the line from your eye (the origin) through
       * the window, not straight out through the wall, or its sides would hide the view */
      const dx = centre.dot(tU), dy = centre.dot(tV), dz = Math.max(0.3, centre.dot(nOut));
      revealGeo.applyMatrix4(new THREE.Matrix4().set(1, 0, dx / dz, 0, 0, 1, dy / dz, 0, 0, 0, 1, 0, 0, 0, 0, 1));
    }
    const reveal = new THREE.Mesh(revealGeo, bezel);
    frame.add(reveal);
    const bz = new THREE.Mesh(new THREE.ShapeGeometry(ring(0, 0.05), 40), metal); bz.position.z = -0.006; bz.rotation.y = Math.PI; bz.scale.x = -1; frame.add(bz);
    /* the pane, a faint reflection only: it must not hide the world, so it writes no stencil and draws last */
    /* the cover */
    const cs = new THREE.Shape(), big = Math.max(w.hu, w.hv) + 0.12;
    cs.moveTo(-big, -big); cs.lineTo(big, -big); cs.lineTo(big, big); cs.lineTo(-big, big); cs.closePath();
    if (w.name === 'hatch') { const h = new THREE.Path(); h.absarc(0, 0, 0.1, 0, Math.PI * 2, true); cs.holes.push(h); }
    const cover = new THREE.Mesh(new THREE.ShapeGeometry(cs, 32), mat(0x2c2b29, { side: THREE.DoubleSide }));
    cover.position.z = depth + 0.03; frame.add(cover); covers.push(cover);
    if (w.name === 'hatch') { frame.matrix.premultiply(new THREE.Matrix4().makeTranslation(-h0.x, -h0.y, -h0.z)); door.add(frame); }
    else g.add(frame);
    frames[w.name] = frame;
  }

  /* ---------------- the main display console: three panels facing the three couches */
  const panels = [
    { u0: 0.00, u1: 0.34, c: [-0.22, -0.16, -0.56], w: 0.78, h: 0.58, look: [0, -0.05, 0] },
    { u0: 0.33, u1: 0.67, c: [0.58, -0.12, -0.62], w: 0.82, h: 0.62, look: [0.6, -0.05, 0] },
    { u0: 0.66, u1: 1.00, c: [1.38, -0.16, -0.56], w: 0.78, h: 0.58, look: [1.25, -0.05, 0] }
  ];
  /* where the displays sit (panel, centre x, y, width, height, metres on the panel): the panel keeps those places
   * clear, a plain framed field with no switch under or across a display (simplified from the real layout) */
  const DISPLAYS = { dsky: [1, -0.27, -0.12, 0.15, 0.17], timer: [1, 0, 0.255, 0.22, 0.06], ball: [0, 0.05, 0.1, 0.128, 0.128] };
  /* (the DSKY is no longer shown in the Command Module: the owner found it broke the panel; its place is panel again) */
  const keepOut = [DISPLAYS.timer, DISPLAYS.ball].map(([k, x, y, dw, dh]) => {
    const p = panels[k], m = 0.025, du = p.u1 - p.u0;
    return { u0: p.u0 + ((x - dw / 2 - m) / p.w + 0.5) * du, u1: p.u0 + ((x + dw / 2 + m) / p.w + 0.5) * du, v0: (y - dh / 2 - m) / p.h + 0.5, v1: (y + dh / 2 + m) / p.h + 0.5 };
  });
  const panel = makePanelTexture(keepOut);
  const panelMat = new THREE.MeshLambertMaterial({ map: panel.texture, ...NO_STENCIL });
  const switches = [];
  panels.forEach((p, k) => {
    const geo = new THREE.PlaneGeometry(p.w, p.h);
    const uv = geo.attributes.uv;
    for (let i = 0; i < uv.count; i++) uv.setX(i, p.u0 + (p.u1 - p.u0) * uv.getX(i));
    const m = new THREE.Mesh(geo, panelMat);
    m.position.fromArray(p.c); m.lookAt(new THREE.Vector3().fromArray(p.look));
    g.add(m); p.mesh = m;
    /* the panel's back and edges, so it reads as a solid console */
    const box = new THREE.Mesh(new THREE.BoxGeometry(p.w + 0.03, p.h + 0.03, 0.12), dark);
    box.position.set(0, 0, -0.065); m.add(box);
    for (const s of panel.switches) if (s.u >= p.u0 && s.u < p.u1) {
      switches.push({ mesh: m, x: ((s.u - p.u0) / (p.u1 - p.u0) - 0.5) * p.w, y: (s.v - 0.5) * p.h, up: s.up, group: s.group, gid: s.gid });
    }
  });
  /* toggle switches, in 3D: a hex nut and a lever, tipped up or down */
  const lever = new THREE.CylinderGeometry(0.0022, 0.0028, 0.022, 6); lever.translate(0, 0.011, 0);
  const nut = new THREE.CylinderGeometry(0.0065, 0.0065, 0.004, 6); nut.rotateX(Math.PI / 2);
  const im1 = new THREE.InstancedMesh(lever, mat(0xd9dadb), switches.length), im2 = new THREE.InstancedMesh(nut, metal, switches.length);
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), one = new THREE.Vector3(1, 1, 1);
  const setLever = (s, k) => {
    const local = new THREE.Matrix4().compose(new THREE.Vector3(s.x, s.y, 0.004), q.setFromEuler(e.set(Math.PI / 2 + (s.up ? -0.5 : 0.5), 0, 0)), one);
    im1.setMatrixAt(k, m4.multiplyMatrices(s.mesh.matrix, local));
  };
  switches.forEach((s, k) => {
    s.mesh.updateMatrix(); setLever(s, k);
    im2.setMatrixAt(k, m4.multiplyMatrices(s.mesh.matrix, new THREE.Matrix4().makeTranslation(s.x, s.y, 0.002)));
  });
  g.add(im1, im2);
  /* Switches that are yours to flip (procedure.js): the switches of a named group, where each is, flipping one, and
   * the lamp that shows which is next (a ring of amber light round it). */
  const lamp = new THREE.Mesh(new THREE.RingGeometry(0.011, 0.019, 28), new THREE.MeshBasicMaterial({ color: 0xffb020, transparent: true, opacity: 0.95, depthTest: false, ...NO_STENCIL }));
  lamp.renderOrder = 30; lamp.visible = false; g.add(lamp);
  const controls = {
    list: switches,
    /* the switches of the group of that name nearest your own couch, in reading order */
    group(name) {
      const by = new Map();
      switches.forEach((s, k) => { if (s.group === name) { if (!by.has(s.gid)) by.set(s.gid, []); by.get(s.gid).push(k); } });
      let best = null, bd = 1e9; const v = new THREE.Vector3();
      for (const ks of by.values()) { let d = 0; for (const k of ks) d += controls.pos(k, v).length(); d /= ks.length; if (d < bd) { bd = d; best = ks; } }
      return best ? best.slice().sort((a, b) => (switches[b].y - switches[a].y) || (switches[a].x - switches[b].x)) : [];
    },
    /* where a switch's lever is, in the cabin's frame */
    pos(k, out) { const s = switches[k]; return out.set(s.x, s.y, 0.014).applyMatrix4(s.mesh.matrix); },
    set(k, up) { const s = switches[k]; s.up = up; setLever(s, k); im1.instanceMatrix.needsUpdate = true; },
    lamp(k) { lamp.visible = k !== null && k !== undefined; if (!lamp.visible) return; const s = switches[k]; lamp.position.set(s.x, s.y, 0.006).applyMatrix4(s.mesh.matrix); lamp.quaternion.copy(s.mesh.quaternion); },
    pulse(t) { lamp.material.opacity = 0.6 + 0.4 * Math.sin(t * 7); lamp.scale.setScalar(1 + 0.12 * Math.sin(t * 7)); },
    get lampOn() { return lamp.visible; }
  };

  /* the DSKY (guidance computer display) on the centre panel, and the mission event timer */
  const dsky = makeDsky();
  const dm = new THREE.Mesh(new THREE.PlaneGeometry(0.15, 0.17), new THREE.MeshBasicMaterial({ map: dsky.texture, ...NO_STENCIL }));
  dm.position.set(DISPLAYS.dsky[1], DISPLAYS.dsky[2], 0.006);      /* not added: see DISPLAYS */
  const timer = makeTimer();
  /* (in a bezel of its own, proud of the panel, so it reads as an instrument whatever is drawn round it; wide
   * enough for the hours of a twelve-day flight: it could not show them, and sat across the controls: owner) */
  const tmBezel = new THREE.Mesh(new THREE.BoxGeometry(0.215, 0.056, 0.012), new THREE.MeshLambertMaterial({ color: 0x1c1d1f, ...NO_STENCIL }));
  tmBezel.position.set(DISPLAYS.timer[1], DISPLAYS.timer[2], 0.004); panels[1].mesh.add(tmBezel);
  const tm = new THREE.Mesh(new THREE.PlaneGeometry(0.192, 0.036), new THREE.MeshBasicMaterial({ map: timer.texture, ...NO_STENCIL }));
  tm.position.set(DISPLAYS.timer[1], DISPLAYS.timer[2], 0.0108); panels[1].mesh.add(tm);
  /* the attitude indicator ("eight ball") on your panel: a sphere that turns with the spacecraft */
  const ballTex = makeBallTexture();
  const ball = new THREE.Mesh(new THREE.SphereGeometry(0.05, 32, 24), new THREE.MeshLambertMaterial({ map: ballTex, ...NO_STENCIL }));
  ball.position.set(DISPLAYS.ball[1], DISPLAYS.ball[2], 0.005); panels[0].mesh.add(ball);
  const ballRim = new THREE.Mesh(new THREE.RingGeometry(0.05, 0.064, 40), bezel); ballRim.position.set(DISPLAYS.ball[1], DISPLAYS.ball[2], 0.03); panels[0].mesh.add(ballRim);

  /* ---------------- couches: yours (you see its headrest and armrests), and the other two, with the crew */
  const crewmen = []; let body = null;   /* the Command Module Pilot (centre couch), the Lunar Module Pilot (right) */
  for (const [x, crew] of [[0, false], [0.65, true], [1.3, true]]) {
    const c = new THREE.Group(); c.position.x = x; g.add(c);
    const rail = new THREE.CylinderGeometry(0.018, 0.018, 1, 8);
    for (const sx of [-0.27, 0.27]) {
      const r = new THREE.Mesh(rail, metal); r.position.set(sx, -0.55, 0.22); r.scale.y = 1.1; r.rotation.x = 0.12; c.add(r);
      const arm = new THREE.Mesh(new THREE.BoxGeometry(0.07, 0.05, 0.42), cushion); arm.position.set(sx * 1.05, -0.52, -0.05); c.add(arm);
    }
    const head = new THREE.Mesh(new THREE.BoxGeometry(0.34, 0.3, 0.07), cushion); head.position.set(0, 0.02, 0.25); c.add(head);
    const back = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.75, 0.09), cushion); back.position.set(0, -0.55, 0.27); c.add(back);
    if (crew) { const m = makeCrewman({ variant: x > 1 ? 1 : 0 }); c.add(m); crewmen.push(m); }
    else { body = makeCrewman({ player: true }); body.position.set(0, -0.09, 0.07); c.add(body); }
  }
  const life = makeCrewLife([{ man: crewmen[0], who: 'CMP' }, { man: crewmen[1], who: 'LMP' }]);
  /* your hand controllers on the armrests */
  const hc = new THREE.Group(); g.add(hc);
  const grip = new THREE.Mesh(new THREE.CylinderGeometry(0.022, 0.026, 0.12, 12), dark);
  const r1 = grip.clone(); r1.position.set(0.29, -0.43, -0.2); hc.add(r1);
  const t1 = new THREE.Mesh(new THREE.BoxGeometry(0.05, 0.06, 0.08), dark); t1.position.set(-0.29, -0.45, -0.2); hc.add(t1);

  /* ---------------- light: floodlights; more when daylight comes in through the windows */
  const amb = new THREE.HemisphereLight(0xfff2dd, 0x2a2824, 0.55);
  const flood1 = new THREE.PointLight(0xffe6c4, 1.1, 3.5, 1.5); flood1.position.set(0.1, 0.55, 0.05);
  const flood2 = new THREE.PointLight(0xffe6c4, 0.8, 3.5, 1.5); flood2.position.set(1.2, 0.5, 0.05);
  const sunIn = new THREE.DirectionalLight(0xfff6e8, 0);
  g.add(amb, flood1, flood2, sunIn, sunIn.target);

  let cheatOn = false;
  return {
    group: g, covers, dsky, timer, ball, sunIn, amb, panels, crewmen, controls, get body() { return body; },
    /* the forward hatch at the end of the tunnel: closed, or taken out (with the probe and drogue) for the crew to
     * go through to the LM */
    setTunnelOpen(on) { fwd.visible = !on; },
    setCovers(on) { covers.forEach(c => { c.visible = on; }); if (cheatOn) frames['side-left'].visible = false; },
    /* the cheat window */
    setCheat(on) { cheatOn = on; cheatWall.visible = !on; cheatRim.visible = on; frames['side-left'].visible = !on; },
    get cheat() { return cheatOn; },
    setBody(on) { if (body) body.visible = on; },
    /* helmets on (launch) or off (in orbit) */
    setHelmets(on) { crewmen.forEach(m => { if (m.userData.bubble) m.userData.bubble.visible = on; if (m.userData.touch) m.userData.touch[0][3] = on ? 0.155 : 0.115; }); },
    /* the crew's life (crew-life.js): where they look, blinking, the jaw with their voices, the hands. ctx: { viewer
     * (the camera), radio, busy } */
    life,
    update(t, ctx) { life.update(t, ctx); },
    /* the hatch: 0 closed, 1 open (swung out about its hinge) */
    setHatch(k) { door.quaternion.setFromAxisAngle(hingeAxis, -1.75 * k); },
    /* daylight through the open windows: a soft extra fill from the Sun's side */
    setDaylight(k, sunLocal) {
      sunIn.intensity = 0.9 * k; sunIn.position.copy(sunLocal).multiplyScalar(3); sunIn.target.position.set(0, 0, 0);
      amb.intensity = 0.55 + 0.35 * k;
    }
  };
}

/* A crewman in an Apollo pressure suit as worn for launch: white suit with the red and blue hose connectors on
 * the chest, wrist rings, grey-blue gloves, and the clear bubble helmet over the black-and-white communications cap
 * ("Snoopy cap"). Seated as you are: thighs forward, shins down. The face is a photoscan with eyes that open,
 * blink and look, and a jaw that opens; the head turns at the neck and the arms are jointed. crew-life.js moves
 * them (userData: neck, eyes, face, arms, poseArm). */
let _suitTex = null, _flagTex = null;
export function makeCrewman(opts = {}) {
  const g = new THREE.Group();
  if (!_suitTex) { _suitTex = makeSuitTexture(); _flagTex = makeFlagTexture(); }
  const L = (color, o = {}) => new THREE.MeshLambertMaterial({ color, ...NO_STENCIL, ...o });
  const suit = L(0xffffff, { map: _suitTex }), glove = L(0x5f6f86), ringB = L(0x2c4fa8), ringR = L(0xa8322c), black = L(0x121212), grey = L(0x8d9096);
  const skin = L(0xc79f84), capW = L(0xe9e6dc);
  const cap = (r, len, m) => new THREE.Mesh(new THREE.CapsuleGeometry(r, len, 6, 14), m);
  const put = (o, x, y, z, rx = 0, ry = 0, rz = 0) => { o.position.set(x, y, z); o.rotation.set(rx, ry, rz); g.add(o); return o; };
  /* torso, a little reclined into the couch, with the bulk of the suit at the chest */
  const torso = put(cap(0.175, 0.30, suit), 0, -0.43, 0.13, 0.08); torso.scale.set(1.12, 1, 0.92);
  put(new THREE.Mesh(new THREE.SphereGeometry(0.19, 20, 14), suit), 0, -0.26, 0.11).scale.set(1.15, 0.7, 0.85);     /* shoulders */
  /* neck ring and helmet */
  if (!opts.player) {     /* your own neck ring would sit at your eyes */
    put(new THREE.Mesh(new THREE.TorusGeometry(0.118, 0.016, 10, 32), ringR), 0, -0.135, 0.085, Math.PI / 2 - 0.15);
    put(new THREE.Mesh(new THREE.CylinderGeometry(0.115, 0.125, 0.05, 28), grey), 0, -0.155, 0.088, -0.15);
  }
  /* the head, in a group that turns: the white communications cap over the back and top of the head, the black
   * earphone cups and microphone booms, and the face: a photoscanned head (Lee Perry-Smith, CC BY 3.0) */
  const neckG = new THREE.Group(); neckG.position.set(0, -0.095, 0.085); neckG.rotation.order = 'YXZ'; g.add(neckG);   /* turns at the neck */
  const headG = new THREE.Group(); headG.position.set(0, 0.09, 0); neckG.add(headG);
  const hput = (o, x, y, z, rx = 0, ry = 0, rz = 0) => { o.position.set(x, y, z); o.rotation.set(rx, ry, rz); headG.add(o); return o; };
  if (!opts.player) {
    /* the cap: white crown over the top and back, black over the ears, a chin strap */
    const capM = hput(new THREE.Mesh(new THREE.SphereGeometry(0.1, 32, 24, 0, Math.PI * 2, 0, Math.PI * 0.56), capW), 0, 0.012, 0.03, 0.38);
    capM.scale.set(0.9, 1.0, 1.12);
    for (const sx of [-1, 1]) { const side = hput(new THREE.Mesh(new THREE.SphereGeometry(0.06, 16, 12), black), sx * 0.062, -0.01, 0.022); side.scale.set(0.6, 1.25, 1.1); }
    /* the neck dam: the dark rubber collar sealing the suit round the neck */
    hput(new THREE.Mesh(new THREE.CylinderGeometry(0.058, 0.09, 0.09, 24, 1, true), new THREE.MeshLambertMaterial({ color: 0x1c1c1c, side: THREE.DoubleSide, ...NO_STENCIL })), 0, -0.1, 0.015);
    for (const sx of [-1, 1]) {
      hput(new THREE.Mesh(new THREE.SphereGeometry(0.034, 16, 12), black), sx * 0.083, -0.012, 0.0).scale.set(0.5, 1.15, 1.0);
      hput(new THREE.Mesh(new THREE.CylinderGeometry(0.0035, 0.0035, 0.08, 6), black), sx * 0.06, -0.06, -0.05, 1.0, 0, sx * 0.3);
    }
    hput(new THREE.Mesh(new THREE.SphereGeometry(0.006, 10, 8), black), 0, -0.088, -0.085);              /* microphone */
    g.userData.neck = neckG;
    /* the face's moving parts: jaw (radians open), blink (0 open, 1 shut), lid (the lids following the eyes up
     * and down, scan units) */
    const face = { uJaw: { value: 0 }, uBlink: { value: 0 }, uLid: { value: 0 },
      /* the face's own shape, so the two crewmates are not twins (owner's headset test): nose forward and longer,
       * the lower face wider or narrower, the chin forward, the brow ridge forward (scan units) */
      uShape: { value: opts.variant ? new THREE.Vector4(0.12, -0.08, 0.12, 0.06) : new THREE.Vector4(0, 0, 0, 0) } };
    g.userData.face = face;
    loadFace().then(f => {
      if (!f) return;
      const mat = new THREE.MeshStandardMaterial({ map: f.map, normalMap: f.normal, roughness: 0.6, metalness: 0,
        color: opts.variant ? 0xd9b090 : 0xfff2ea, ...NO_STENCIL });                 /* the LMP warmer, more tanned */
      /* All in the scan's own units (its face looks along +z, the middle of the face at x = -0.085): the lips meet
       * at y = 0.441, with the inside of the mouth behind them; the eyes are shut, the lids meeting at y = 1.64. */
      mat.onBeforeCompile = sh => {
        Object.assign(sh.uniforms, face);
        sh.vertexShader = sh.vertexShader.replace('#include <common>', `#include <common>
uniform float uJaw;
uniform vec4 uShape;
varying vec3 vLp;`).replace('#include <begin_vertex>', `#include <begin_vertex>
vLp = position;
/* the face's own shape (uShape): smooth bumps in the scan's frame, nose, lower face, chin, brow */
{
  float cx = transformed.x + 0.085;
  float front = smoothstep(1.5, 2.2, transformed.z);
  float nose = exp(-cx * cx / 0.09 - (transformed.y - 1.1) * (transformed.y - 1.1) / 0.2) * smoothstep(2.0, 2.45, transformed.z);
  transformed.z += uShape.x * nose; transformed.y -= 0.35 * uShape.x * nose;
  float lower = smoothstep(1.35, 0.5, transformed.y) * smoothstep(-1.1, -0.35, transformed.y);
  transformed.x = -0.085 + cx * (1.0 + uShape.y * lower);
  float chin = exp(-cx * cx / 0.3 - (transformed.y + 0.2) * (transformed.y + 0.2) / 0.12) * front;
  transformed.z += uShape.z * chin; transformed.y -= 0.3 * uShape.z * chin;
  float brow = exp(-cx * cx / 0.8 - (transformed.y - 2.0) * (transformed.y - 2.0) / 0.03) * front;
  transformed.z += uShape.w * brow;
}
/* the jaw: turns open about the hinge in front of the ears. At the mouth the lips part cleanly (the lower one goes
 * with the jaw, the upper one stays); elsewhere the skin stretches smoothly, fading towards the ears and the neck. */
{
  float cx = abs(position.x + 0.085);
  float mouth = (1.0 - smoothstep(0.2, 0.38, cx)) * smoothstep(1.92, 2.02, position.z);
  float soft = smoothstep(0.75, 0.1, position.y);
  float w = mix(soft, 1.0 - step(0.441, position.y), mouth);
  w *= smoothstep(-1.12, -0.45, position.y) * smoothstep(0.2, 1.4, position.z) * (1.0 - smoothstep(0.9, 1.7, cx));
  float a = uJaw * w, c = cos(a), s = sin(a);
  vec2 p = transformed.yz - vec2(0.95, 0.2);
  transformed.yz = vec2(0.95, 0.2) + vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}`);
        sh.fragmentShader = sh.fragmentShader.replace('#include <common>', `#include <common>
uniform float uBlink, uLid;
varying vec3 vLp;
/* an eye's opening: an almond about its centre, the upper lid higher than the lower; 1 inside, and how far
 * outside the lid margins (for the lash line) */
float lidCut(vec2 c, inout float margin) {
  vec2 d = vLp.xy - c; float u = d.x / (d.x * (c.x + 0.085) < 0.0 ? 0.235 : 0.28);     /* narrower towards the nose */
  if (abs(u) > 1.0 || vLp.z < 1.55) return 0.0;
  float k = pow(1.0 - u * u, 0.55) * (1.0 - uBlink);
  float top = 0.1 * k - uLid, bot = -0.062 * k - 0.5 * uLid;
  if (d.y < top && d.y > bot) return 1.0;
  margin = min(margin, d.y > top ? (d.y - top) / 0.03 : (bot - d.y) / 0.016);
  return 0.0;
}`).replace('void main() {', `void main() {
  if (vLp.y < -1.15) discard;     /* the scan is a bust: cut off below the neck (the suit's neck dam takes over) */
  float margin = 9.0;
  if (lidCut(vec2(-0.74, 1.64), margin) + lidCut(vec2(0.55, 1.64), margin) > 0.0) discard;`).replace('#include <map_fragment>', `#include <map_fragment>
  /* the lash line along the lid margins, and the dark inside of the mouth */
  diffuseColor.rgb *= mix(0.4, 1.0, smoothstep(0.0, 1.0, margin));
  float inMouth = (1.0 - smoothstep(0.3, 0.42, abs(vLp.x + 0.085))) * smoothstep(2.24, 2.1, vLp.z) * smoothstep(0.2, 0.26, vLp.y) * (1.0 - smoothstep(0.56, 0.62, vLp.y));
  diffuseColor.rgb *= 1.0 - 0.8 * inMouth;`);
      };
      const m = new THREE.Mesh(f.geometry, mat);
      m.scale.set(f.scale * (opts.variant ? 0.95 : 1), f.scale * (opts.variant ? 1.03 : 1), f.scale);
      m.rotation.y = Math.PI; m.position.set(0, -0.085, 0.02);
      headG.add(m);
      /* eyeballs (a real eye's size, 12 mm radius, scan units), set back so the skin covers them everywhere outside the\n       * opening (at the outer corners the lids are only 1.7 units out); a dark socket behind them */
      const eyeMat = new THREE.MeshPhysicalMaterial({ map: eyeTexture(opts.variant ? '#6b4a2c' : '#5d7a8e'), roughness: 0.4, metalness: 0,
        clearcoat: 1, clearcoatRoughness: 0.04, ...NO_STENCIL });
      const socket = new THREE.MeshBasicMaterial({ color: 0x1e1210, side: THREE.BackSide, ...NO_STENCIL });
      const eyes = [-0.74, 0.55].map(x => {
        const e = new THREE.Mesh(new THREE.SphereGeometry(0.28, 32, 20), eyeMat); e.position.set(x, 1.66, 1.575); e.rotation.order = 'YXZ'; m.add(e);
        const s = new THREE.Mesh(new THREE.SphereGeometry(0.34, 16, 12), socket); s.position.set(x, 1.66, 1.52); m.add(s);
        return e;
      });
      g.userData.eyes = eyes;
    });
  }
  /* the bubble: clear polycarbonate, seen by its reflections */
  const bubble = new THREE.Mesh(new THREE.SphereGeometry(0.152, 32, 24), new THREE.ShaderMaterial({
    transparent: true, depthWrite: false, ...NO_STENCIL,
    vertexShader: `varying vec3 vN; varying vec3 vV; void main(){ vec4 mv = modelViewMatrix * vec4(position, 1.0); vN = normalize(normalMatrix * normal); vV = normalize(-mv.xyz); gl_Position = projectionMatrix * mv; }`,
    fragmentShader: `varying vec3 vN; varying vec3 vV; void main(){ float f = pow(1.0 - abs(dot(normalize(vN), normalize(vV))), 2.6);
      vec3 r = reflect(-normalize(vV), normalize(vN)); float hl = pow(max(dot(r, normalize(vec3(-0.3, 0.8, 0.5))), 0.0), 60.0);
      gl_FragColor = vec4(vec3(0.95, 0.96, 1.0), 0.05 + 0.5 * f + 0.8 * hl); }` }));
  if (!opts.player) { put(bubble, 0, 0.005, 0.085); bubble.renderOrder = 5; g.userData.bubble = bubble; }
  /* arms: jointed at shoulder and elbow, placed by where the hand is (two-bone IK); at rest the upper arms down
   * the sides, the forearms forward onto the armrests; wrist rings; gloves */
  const arms = [];
  for (const sx of [-1, 1]) {
    if (opts.player) { put(cap(0.062, 0.12, suit), sx * 0.25, -0.33, 0.14, 0.15, 0, -sx * 0.1); continue; }   /* only the upper arms */
    const a = { sx, S: new THREE.Vector3(sx * 0.245, -0.215, 0.15), L1: 0.29, L2: 0.32, pole: new THREE.Vector3(sx * 0.1, -0.5, 1).normalize(),
      rest: new THREE.Vector3(sx * 0.265, -0.52, -0.215),
      upper: put(cap(0.062, 0.29, suit), 0, 0, 0), fore: put(cap(0.055, 0.255, suit), 0, 0, 0),
      ring: put(new THREE.Mesh(new THREE.TorusGeometry(0.05, 0.011, 8, 22), sx < 0 ? ringR : ringB), 0, 0, 0),
      glove: put(cap(0.043, 0.07, glove), 0, 0, 0) };
    a.glove.scale.set(1.15, 1, 0.8);
    poseArm(a, a.rest); arms.push(a);
  }
  if (arms.length) { g.userData.arms = arms; g.userData.poseArm = poseArm; }
  /* legs: thighs forward, shins down, boots; or standing (in the LM) */
  for (const sx of [-1, 1]) {
    if (opts.standing) {
      put(cap(0.085, 0.34, suit), sx * 0.11, -0.82, 0.1, 0.04);
      put(cap(0.07, 0.36, suit), sx * 0.11, -1.24, 0.08, -0.03);
      put(new THREE.Mesh(new THREE.BoxGeometry(0.11, 0.09, 0.27), grey), sx * 0.11, -1.52, 0.02);
      continue;
    }
    put(cap(0.085, 0.3, suit), sx * 0.105, -0.63, -0.1, 1.5);
    put(cap(0.07, 0.3, suit), sx * 0.105, -0.86, -0.32, 0.12);
    put(new THREE.Mesh(new THREE.BoxGeometry(0.11, 0.09, 0.26), grey), sx * 0.105, -1.08, -0.38);
  }
  /* chest: oxygen in (blue) and out (red) and the electrical connector, hoses leading down to the side */
  for (const [x, y, m] of [[-0.085, -0.34, ringB], [0.085, -0.34, ringR], [-0.085, -0.43, ringR], [0.085, -0.43, ringB]]) {
    put(new THREE.Mesh(new THREE.CylinderGeometry(0.026, 0.026, 0.03, 16), m), x, y, -0.045, Math.PI / 2);
    put(new THREE.Mesh(new THREE.CylinderGeometry(0.017, 0.017, 0.035, 12), grey), x, y, -0.065, Math.PI / 2);
  }
  put(new THREE.Mesh(new THREE.CylinderGeometry(0.02, 0.02, 0.028, 14), grey), 0, -0.385, -0.05, Math.PI / 2);
  for (const [x0, y0, sx] of [[-0.085, -0.34, -1], [0.085, -0.43, 1]]) {
    const curve = new THREE.CatmullRomCurve3([new THREE.Vector3(x0, y0, -0.08), new THREE.Vector3(x0 + sx * 0.05, y0 - 0.1, -0.13), new THREE.Vector3(x0 + sx * 0.2, y0 - 0.26, -0.06), new THREE.Vector3(x0 + sx * 0.27, y0 - 0.36, 0.1)]);
    g.add(new THREE.Mesh(new THREE.TubeGeometry(curve, 18, 0.016, 8), L(0x2f3a4a)));
  }
  /* his body for your hands (crew-life.js touch): spheres [x, y, z, r] in his frame, seated or standing; the head's
   * is the helmet's bubble (setHelmets makes it the head's when the helmets are off) */
  if (!opts.player) {
    const legs = opts.standing ? [[0.11, -0.82, 0.1, 0.11], [0.11, -1.06, 0.09, 0.1], [0.11, -1.28, 0.08, 0.09]]
      : [[0.105, -0.63, -0.02, 0.1], [0.105, -0.63, -0.22, 0.1], [0.105, -0.86, -0.32, 0.08]];
    g.userData.touch = [[0, 0.005, 0.085, 0.155], [0, -0.3, 0.11, 0.2], [0, -0.52, 0.12, 0.19]];
    for (const sx of [-1, 1]) {
      g.userData.touch.push([sx * 0.2, -0.27, 0.11, 0.12], [sx * 0.25, -0.38, 0.12, 0.08], [sx * 0.265, -0.5, -0.05, 0.075], [sx * 0.265, -0.52, -0.2, 0.065]);
      for (const [x, y, z, r] of legs) g.userData.touch.push([sx * x, y, z, r]);
    }
  }
  /* the flag on the left shoulder */
  const flag = new THREE.Mesh(new THREE.PlaneGeometry(0.075, 0.045), new THREE.MeshLambertMaterial({ map: _flagTex, ...NO_STENCIL }));
  put(flag, -0.312, -0.3, 0.1, 0, -Math.PI / 2, 0);
  return g;
}

/* an arm placed by its hand: the shoulder S stays, the elbow bends in the plane towards the pole (out, down and
 * back), the upper arm and forearm run between the joints, the wrist ring and glove along the forearm. palmUp
 * (crewman frame) is where the back of the hand faces; by default up (the hand flat, as on the armrest). */
const _d = new THREE.Vector3(), _p = new THREE.Vector3(), _e = new THREE.Vector3(), _h = new THREE.Vector3(), _w = new THREE.Vector3();
const _bx = new THREE.Vector3(), _bz = new THREE.Vector3(), _m4 = new THREE.Matrix4(), _Y = new THREE.Vector3(0, 1, 0), _Z = new THREE.Vector3(0, 0, 1);
function poseArm(a, H, palmUp = _Y) {
  _d.subVectors(H, a.S); const dist = Math.min(Math.max(_d.length(), 0.08), a.L1 + a.L2 - 0.002); _d.normalize();
  const cosA = Math.min(1, Math.max(-1, (a.L1 * a.L1 + dist * dist - a.L2 * a.L2) / (2 * a.L1 * dist))), sinA = Math.sqrt(1 - cosA * cosA);
  _p.copy(a.pole).addScaledVector(_d, -a.pole.dot(_d)).normalize();
  _e.copy(a.S).addScaledVector(_d, a.L1 * cosA).addScaledVector(_p, a.L1 * sinA);          /* the elbow */
  _h.copy(a.S).addScaledVector(_d, dist);                                                    /* the hand (reachable) */
  const bone = (o, A, B) => { o.position.addVectors(A, B).multiplyScalar(0.5); _w.subVectors(B, A).normalize(); o.quaternion.setFromUnitVectors(_Y, _w); };
  bone(a.upper, a.S, _e);
  _w.subVectors(_h, _e).normalize();
  const wrist = _p.copy(_h).addScaledVector(_w, -0.065);
  bone(a.fore, _e, wrist);
  _w.subVectors(_h, _e).normalize();
  a.ring.position.copy(_h).addScaledVector(_w, -0.065); a.ring.quaternion.setFromUnitVectors(_Z, _w);
  /* the glove along the forearm, flat across palmUp */
  _bz.copy(palmUp).addScaledVector(_w, -palmUp.dot(_w)).normalize(); _bx.crossVectors(_w, _bz);
  a.glove.position.copy(_h); a.glove.quaternion.setFromRotationMatrix(_m4.makeBasis(_bx, _w, _bz));
}

/* an eye: the white (darker and redder towards the corners), the iris with its fibres and dark rim, the pupil;
 * the iris where a sphere's +z points (u = 0.25, v = 0.5). The iris is 0.43 rad across in radius on a 0.3 sphere
 * (a real iris, 12 mm across). */
function eyeTexture(iris) {
  const W = 512, H = 256, cx = W / 4, cy = H / 2, R = 0.48 / (2 * Math.PI) * W;
  const c = document.createElement('canvas'); c.width = W; c.height = H;
  const x = c.getContext('2d');
  const wg = x.createRadialGradient(cx, cy, R, cx, cy, W * 0.3);
  wg.addColorStop(0, '#ebe4da'); wg.addColorStop(0.45, '#ddd0c4'); wg.addColorStop(1, '#b89488');
  x.fillStyle = wg; x.fillRect(0, 0, W, H);
  x.strokeStyle = 'rgba(170,60,55,0.22)'; x.lineWidth = 1;
  for (let k = 0; k < 26; k++) {
    const a = k * 2.4, r0 = R * (1.6 + (k % 3) * 0.3), r1 = W * 0.28;
    x.beginPath(); x.moveTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1 * 0.9);
    x.quadraticCurveTo(cx + Math.cos(a + 0.3) * (r0 + r1) / 2, cy + Math.sin(a + 0.3) * (r0 + r1) / 2 * 0.9, cx + Math.cos(a) * r0, cy + Math.sin(a) * r0 * 0.9); x.stroke();
  }
  const g = x.createRadialGradient(cx, cy, 0, cx, cy, R);
  g.addColorStop(0, '#060606'); g.addColorStop(0.36, '#060606'); g.addColorStop(0.42, '#3a3028'); g.addColorStop(0.5, iris);
  g.addColorStop(0.85, iris); g.addColorStop(0.95, '#2a2420'); g.addColorStop(1, 'rgba(60,50,45,0.6)');
  x.fillStyle = g; x.beginPath(); x.arc(cx, cy, R, 0, 7); x.fill();
  /* the iris fibres: light and dark spokes between the pupil and the rim */
  for (let k = 0; k < 90; k++) {
    const a = k / 90 * Math.PI * 2 + Math.sin(k * 7.3) * 0.03, l = 0.4 + 0.15 * Math.abs(Math.sin(k * 3.1));
    x.strokeStyle = k % 2 ? 'rgba(255,240,220,0.13)' : 'rgba(0,0,0,0.18)'; x.lineWidth = 1.2;
    x.beginPath(); x.moveTo(cx + Math.cos(a) * R * l, cy + Math.sin(a) * R * l); x.lineTo(cx + Math.cos(a) * R * 0.9, cy + Math.sin(a) * R * 0.9); x.stroke();
  }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4;
  return t;
}

let _face = null;
function loadFace() {
  if (_face) return _face;
  const base = new URL('../vendor/head/', import.meta.url).href, tl = new THREE.TextureLoader();
  _face = Promise.all([
    new GLTFLoader().loadAsync(base + 'LeePerrySmith.glb'),
    tl.loadAsync(base + 'Map-COL.jpg'), tl.loadAsync(base + 'Infinite-Level_02_Tangent_SmoothUV.jpg')
  ]).then(([gltf, map, normal]) => {
    let geometry = null;
    gltf.scene.traverse(o => { if (o.isMesh && !geometry) geometry = o.geometry; });
    map.colorSpace = THREE.SRGBColorSpace;
    return { geometry, map, normal, scale: 0.043 };
  }).catch(e => { console.warn('face scan not loaded', e); return null; });
  return _face;
}

/* things on the walls: the hatch with its latch gear, handholds, cable runs, floodlights, couch struts */
function dress(g, point, M, door, h0) {
  const onDoor = o => { o.position.sub(h0); door.add(o); return o; };
  const L = (color, o = {}) => new THREE.MeshLambertMaterial({ color, ...NO_STENCIL, ...o });
  const tube = (pts, r, m) => { const t = new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), pts.length * 6, r, 8), m); g.add(t); return t; };
  const bar = (a, b, r, m) => { const o = new THREE.Mesh(new THREE.CylinderGeometry(r, r, a.distanceTo(b), 8), m); o.position.copy(a).add(b).multiplyScalar(0.5);
    o.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize()); g.add(o); return o; };
  /* the unified hatch: a rounded frame round the hatch window, the latch gearbox and its pump handle */
  const hp = (dphi, dz, out) => point(90 + dphi, -0.85 + dz, out);
  const frame = [];
  for (let k = 0; k <= 40; k++) { const a = k / 40 * Math.PI * 2, sx = Math.sign(Math.cos(a)) * Math.pow(Math.abs(Math.cos(a)), 0.45), sy = Math.sign(Math.sin(a)) * Math.pow(Math.abs(Math.sin(a)), 0.45); frame.push(hp(sx * 21, sy * 0.36, -0.012)); }
  tube(frame, 0.016, M.metal);
  const gb = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.1, 0.06), L(0x8a8f96)); gb.position.copy(hp(-13, 0.02, -0.04)); gb.lookAt(gb.position.clone().add(new THREE.Vector3(0, -1, 0.5))); onDoor(gb);
  onDoor(bar(hp(-13, 0.02, -0.07), hp(-13, 0.2, -0.12), 0.011, L(0xb8322a)));
  for (const d of [-17, 17]) for (const z of [-0.26, 0.0, 0.26]) { const l = new THREE.Mesh(new THREE.BoxGeometry(0.035, 0.03, 0.05), M.dark); l.position.copy(hp(d, z, -0.02)); onDoor(l); }
  /* handholds by the windows and along the wall */
  const hold = (phi, z0, z1) => { const a = point(phi, z0, -0.006), b = point(phi, z1, -0.006), a2 = point(phi, z0, -0.055), b2 = point(phi, z1, -0.055);
    bar(a2, b2, 0.011, L(0xc9c6bb)); bar(a, a2, 0.009, M.metal); bar(b, b2, 0.009, M.metal); };
  hold(150, -0.2, -0.5); hold(30, -0.2, -0.5); hold(112, -0.35, -0.6); hold(68, -0.35, -0.6); hold(222, -0.6, -0.9); hold(-20, -0.6, -0.9);
  /* cable and plumbing runs round the cone */
  for (const [z, r, col, a0, a1] of [[0.32, 0.014, 0x2a2c30, -40, 220], [0.27, 0.009, 0x7a6a3a, -40, 220], [-1.42, 0.011, 0x2a2c30, 20, 160], [0.2, 0.008, 0x3a4a5a, 150, 215], [0.05, 0.008, 0x3a4a5a, -35, 30]]) {
    const pts = []; for (let a = a0; a <= a1; a += 6) pts.push(point(a, z, -0.012));
    tube(pts, r, L(col));
    for (let a = a0 + 9; a < a1; a += 24) { const c = new THREE.Mesh(new THREE.BoxGeometry(0.03, 0.03, 0.012), M.metal); c.position.copy(point(a, z, -0.012)); c.lookAt(AX, AY, z); g.add(c); }
  }
  /* floodlight fixtures */
  for (const [phi, z] of [[105, -0.1], [75, -0.1], [140, -0.75], [20, -0.75]]) {
    const f = new THREE.Mesh(new THREE.BoxGeometry(0.11, 0.035, 0.05), M.bezel); f.position.copy(point(phi, z, -0.02)); f.lookAt(AX, AY, z + 0.2); g.add(f);
    const lens = new THREE.Mesh(new THREE.PlaneGeometry(0.095, 0.03), new THREE.MeshBasicMaterial({ color: 0xfff1d6, ...NO_STENCIL })); lens.position.set(0, 0, 0.0265); f.add(lens);
  }
  /* couch struts down to the aft bulkhead and out to the walls */
  for (const x of [-0.3, 0.33, 0.97, 1.6]) {
    bar(new THREE.Vector3(x, -0.3, 0.3), new THREE.Vector3(x, -0.2, Z_BASE), 0.016, M.metal);
    bar(new THREE.Vector3(x, -0.95, 0.25), new THREE.Vector3(x, -1.05, Z_BASE), 0.016, M.metal);
  }
  bar(new THREE.Vector3(-0.3, -0.7, 0.25), point(190, 0.15, -0.01), 0.02, M.metal);
  bar(new THREE.Vector3(1.6, -0.7, 0.25), point(-10, 0.15, -0.01), 0.02, M.metal);
}

/* the painted inner wall: panels with seams and fasteners, Velcro patches, stencilled markings, wear */
export function makeWallTexture() {
  const W = 4096, H = 1024, c = document.createElement('canvas'); c.width = W; c.height = H;
  const x = c.getContext('2d');
  let seed = 41;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  x.fillStyle = '#a7a498'; x.fillRect(0, 0, W, H);
  /* mottling: paint is never even */
  for (let k = 0; k < 2600; k++) {
    const r = 20 + rnd() * 120, px = rnd() * W, py = rnd() * H, g = x.createRadialGradient(px, py, 0, px, py, r), d = rnd() < 0.5;
    g.addColorStop(0, d ? 'rgba(60,56,48,0.05)' : 'rgba(235,232,220,0.05)'); g.addColorStop(1, 'rgba(0,0,0,0)');
    x.fillStyle = g; x.fillRect(px - r, py - r, 2 * r, 2 * r);
  }
  /* panels: rows along the cone, staggered; a dark seam, a light edge, fasteners at the corners and along the edges */
  const rows = [0, 150, 330, 520, 700, 860, H];
  for (let r = 0; r < rows.length - 1; r++) {
    let px = -rnd() * 200;
    while (px < W) {
      const pw = 190 + rnd() * 260, y0 = rows[r], y1 = rows[r + 1];
      const tone = 150 + Math.floor(rnd() * 26), warm = rnd() < 0.2;
      x.fillStyle = warm ? `rgba(${tone + 16},${tone + 8},${tone - 14},0.35)` : `rgba(${tone},${tone - 2},${tone - 12},0.3)`;
      x.fillRect(px + 2, y0 + 2, pw - 4, y1 - y0 - 4);
      x.strokeStyle = 'rgba(40,38,34,0.75)'; x.lineWidth = 3; x.strokeRect(px + 1.5, y0 + 1.5, pw - 3, y1 - y0 - 3);
      x.strokeStyle = 'rgba(230,228,215,0.35)'; x.lineWidth = 1.5; x.strokeRect(px + 5, y0 + 5, pw - 10, y1 - y0 - 10);
      x.fillStyle = 'rgba(52,50,46,0.9)';
      for (let fx = px + 14; fx < px + pw - 8; fx += 44) for (const fy of [y0 + 13, y1 - 13]) { x.beginPath(); x.arc(fx, fy, 3.2, 0, 7); x.fill(); }
      for (let fy = y0 + 13; fy < y1 - 8; fy += 44) for (const fx of [px + 14, px + pw - 14]) { x.beginPath(); x.arc(fx, fy, 3.2, 0, 7); x.fill(); }
      const k = rnd();
      if (k < 0.3) {                 /* Velcro: the blue and tan squares that were everywhere */
        for (let q = 0; q < 1 + Math.floor(rnd() * 3); q++) { x.fillStyle = rnd() < 0.6 ? '#3d4f7c' : '#b89f72'; x.fillRect(px + 30 + rnd() * (pw - 110), y0 + 30 + rnd() * (y1 - y0 - 100), 44, 44); }
      } else if (k < 0.5) {          /* a stencilled marking */
        x.font = 'bold 20px Arial'; x.fillStyle = 'rgba(30,30,30,0.8)'; x.textAlign = 'left';
        x.fillText(['R' + (1 + Math.floor(rnd() * 13)), 'L' + (1 + Math.floor(rnd() * 3)), 'U' + (1 + Math.floor(rnd() * 4)), 'A' + (1 + Math.floor(rnd() * 8)), 'O2', 'H2O', 'ECS', 'PGA'][Math.floor(rnd() * 8)], px + 26, y0 + 44);
      } else if (k < 0.58) {         /* a caution stripe */
        for (let q = 0; q < 6; q++) { x.fillStyle = q % 2 ? '#1c1c1c' : '#c9a227'; x.fillRect(px + 30 + q * 16, y1 - 46, 16, 14); }
      }
      px += pw;
    }
  }
  /* wear: scuffs where hands and boots go, fine scratches */
  x.strokeStyle = 'rgba(70,66,58,0.22)'; x.lineWidth = 1;
  for (let k = 0; k < 900; k++) { const px = rnd() * W, py = rnd() * H, l = 10 + rnd() * 60, a = rnd() * 6.28; x.beginPath(); x.moveTo(px, py); x.lineTo(px + Math.cos(a) * l, py + Math.sin(a) * l); x.stroke(); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; t.wrapS = THREE.RepeatWrapping;
  return t;
}

/* the aft bulkhead behind the couches: stowage lockers */
function makeLockerTexture() {
  const S = 2048, c = document.createElement('canvas'); c.width = c.height = S;
  const x = c.getContext('2d');
  let seed = 5;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  x.fillStyle = '#9a978b'; x.fillRect(0, 0, S, S);
  for (let j = 0; j < 9; j++) for (let i = 0; i < 9; i++) {
    const w = S / 9, px = i * w, py = j * w, t = 140 + Math.floor(rnd() * 30);
    x.fillStyle = `rgb(${t},${t - 2},${t - 12})`; x.fillRect(px + 6, py + 6, w - 12, w - 12);
    x.strokeStyle = '#3a3834'; x.lineWidth = 4; x.strokeRect(px + 6, py + 6, w - 12, w - 12);
    x.fillStyle = '#2b2a27'; x.fillRect(px + w / 2 - 26, py + w - 40, 52, 12);
    x.font = 'bold 24px Arial'; x.fillStyle = '#262626'; x.textAlign = 'left'; x.fillText('A' + (j * 9 + i + 1), px + 20, py + 44);
    if (rnd() < 0.3) { x.fillStyle = '#3d4f7c'; x.fillRect(px + w - 80, py + 22, 46, 46); }
  }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4;
  return t;
}

/* suit cloth: a fine weave, seams, a little soiling */
function makeSuitTexture() {
  const S = 512, c = document.createElement('canvas'); c.width = c.height = S;
  const x = c.getContext('2d'), im = x.createImageData(S, S);
  let seed = 23;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  for (let j = 0; j < S; j++) for (let i = 0; i < S; i++) {
    const weave = ((i % 4 < 2) !== (j % 4 < 2)) ? 6 : -6, v = 226 + weave + (rnd() - 0.5) * 10, k = (j * S + i) * 4;
    im.data[k] = v; im.data[k + 1] = v - 2; im.data[k + 2] = v - 9; im.data[k + 3] = 255;
  }
  x.putImageData(im, 0, 0);
  x.strokeStyle = 'rgba(120,116,104,0.7)'; x.lineWidth = 2;
  for (const y of [90, 250, 410]) { x.beginPath(); x.moveTo(0, y); x.lineTo(S, y); x.stroke(); x.setLineDash([6, 5]); x.beginPath(); x.moveTo(0, y + 6); x.lineTo(S, y + 6); x.stroke(); x.setLineDash([]); }
  for (const xx of [128, 384]) { x.beginPath(); x.moveTo(xx, 0); x.lineTo(xx, S); x.stroke(); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(3, 3); t.anisotropy = 4;
  return t;
}

function makeFlagTexture() {
  const c = document.createElement('canvas'); c.width = 190; c.height = 100;
  const x = c.getContext('2d');
  for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f4f2ec' : '#b22234'; x.fillRect(0, k * 100 / 13, 190, 100 / 13 + 0.5); }
  x.fillStyle = '#3c3b6e'; x.fillRect(0, 0, 76, 54);
  x.fillStyle = '#f4f2ec';
  for (let j = 0; j < 9; j++) for (let i = 0; i < (j % 2 ? 5 : 6); i++) { x.beginPath(); x.arc(7 + i * 12.5 + (j % 2 ? 6.2 : 0), 5.5 + j * 5.4, 1.7, 0, 7); x.fill(); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

/* ---------------------------------------------------------------- textures made in code */
/* The panels' face, in the style of the real ones (owner's photographs of the CM's main display console and the LM's
 * panels): sub-panels screwed on (screw heads round each edge), white lines grouping the controls with the group's
 * name set into the line, white lettering, toggle switches in chrome-ringed holes, some behind guards, black
 * instruments with white scales, grey talkback indicators. The layout is made up, not the real one switch by switch
 * (it would not read at the headset's resolution anyway). style: 'cm' (light blue-grey) or 'lm' (darker grey).
 * keepOut: rectangles (u0, u1, v0, v1 in the texture) where displays go: plain fields, no switch under or across them. */
/* sections: the stretches of u ([u0, u1]) each panel shows; sub-panels are laid out within a stretch and never cut by its edge */
export function makePanelTexture(keepOut = [], style = 'cm', sections = [[0, 1 / 3], [1 / 3, 2 / 3], [2 / 3, 1]]) {
  const W = 4096, H = 1024, c = document.createElement('canvas'); c.width = W; c.height = H;
  const x = c.getContext('2d');
  let seed = 17;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const P = style === 'lm'
    ? { base: [92, 96, 99], line: '#e6e6df', text: '#eeede6', gap: '#3c3f42', screw: '#a9adb1' }
    : { base: [138, 145, 150], line: '#f3f3ef', text: '#f6f6f2', gap: '#5d6266', screw: '#c4c8cc' };
  const rgb = (k = 0) => `rgb(${P.base.map(v => Math.max(0, Math.min(255, Math.round(v * (1 + k))))).join(',')})`;
  x.fillStyle = P.gap; x.fillRect(0, 0, W, H);
  const switches = [];
  const words = ['CMC', 'SCS', 'SPS', 'RCS', 'BMAG', 'EDS', 'ELS', 'TVC', 'GDC', 'FDAI', 'PRPLNT', 'O2', 'H2', 'FUEL CELL', 'MAIN BUS', 'BATT', 'CRYO', 'ECS', 'GLYCOL', 'CABIN', 'SUIT', 'DOCK', 'PYRO', 'SECS', 'MASTER', 'ARM', 'LOGIC', 'ABORT', 'LV', 'GUID', 'XLUNAR', 'S-II/S-IVB', 'TWR JETT', 'CSM/LV SEP', 'APEX', 'DROGUE', 'MAIN DEPLOY', 'ATT DEADBAND', 'RATE', 'LIMIT CYCLE', 'MANUAL ATTITUDE', 'BMAG MODE', 'EVENT TIMER', 'SPS GIMBAL MOTORS'];
  const pos = ['ON', 'OFF', 'AUTO', 'CMD', 'OPEN', 'CLOSE', 'RESET', 'ARM', 'SAFE', 'NORM', 'A', 'B', '1', '2'];
  const label = (s, px, py, size = 15, bold = true) => { x.font = `${bold ? 'bold ' : ''}${size}px Arial, sans-serif`; x.fillStyle = P.text; x.textAlign = 'center'; x.fillText(s, px, py); };
  const screw = (sx, sy) => {
    x.fillStyle = P.screw; x.beginPath(); x.arc(sx, sy, 6.5, 0, Math.PI * 2); x.fill();
    x.strokeStyle = 'rgba(0,0,0,0.55)'; x.lineWidth = 2; const a = rnd() * Math.PI;
    x.beginPath(); x.moveTo(sx - Math.cos(a) * 5, sy - Math.sin(a) * 5); x.lineTo(sx + Math.cos(a) * 5, sy + Math.sin(a) * 5); x.stroke();
  };
  let groups = 0;
  /* a group of controls: a white line round it, its name set into the top line */
  const group = (gx, gy, gw, gh, name) => {
    x.strokeStyle = P.line; x.lineWidth = 2.5; x.strokeRect(gx, gy, gw, gh);
    x.font = 'bold 15px Arial, sans-serif'; const tw = x.measureText(name).width + 12;
    x.fillStyle = rgb(); x.fillRect(gx + gw / 2 - tw / 2, gy - 9, tw, 18); label(name, gx + gw / 2, gy + 5, 15);
  };
  /* sub-panels: screwed-on plates of switches, instruments, indicators */
  for (const [su0, su1] of sections) {
    const X0 = su0 * W + 14, X1 = su1 * W - 14;
    let y = 30;
    while (y < H - 120) {
      const bh = 160 + Math.floor(rnd() * 120);
      let xx = X0;
      while (xx < X1 - 120) {
        let bw = Math.min(X1 - xx, 190 + Math.floor(rnd() * 260));
        if (X1 - (xx + bw) < 150) bw = X1 - xx;                    /* the last one takes what is left: nothing cut */
        const pw = bw - 8, ph = Math.min(bh, H - 24 - y) - 8;
        x.fillStyle = rgb((rnd() - 0.5) * 0.05); x.fillRect(xx, y, pw, ph);
        x.fillStyle = 'rgba(255,255,255,0.12)'; x.fillRect(xx, y, pw, 2); x.fillStyle = 'rgba(0,0,0,0.25)'; x.fillRect(xx, y + ph - 2, pw, 2);
        for (let k = 0; k <= Math.max(1, Math.round(pw / 150)); k++) { const sx = xx + 12 + k * (pw - 24) / Math.max(1, Math.round(pw / 150)); screw(sx, y + 11); screw(sx, y + ph - 11); }
        for (const sx of [xx + 11, xx + pw - 11]) if (ph > 140) screw(sx, y + ph / 2);
        const kind = rnd(), gx = xx + 22, gy = y + 34, gw = pw - 44, gh = ph - 62;
        if (kind < 0.58) {                                 /* a row or two of toggle switches, some behind guards */
          const gname = words[Math.floor(rnd() * words.length)], gid = ++groups;
          group(gx, gy, gw, gh, gname);
          const rows = gh > 150 ? 2 : 1, n = Math.max(2, Math.floor((gw - 20) / 52));
          const guarded = rnd() < 0.35;
          for (let r = 0; r < rows; r++) for (let k = 0; k < n; k++) {
            const sx = gx + 26 + k * ((gw - 52) / Math.max(1, n - 1)), sy = gy + 52 + r * (gh / rows);
            x.fillStyle = '#c9ccd0'; x.beginPath(); x.arc(sx, sy, 12, 0, Math.PI * 2); x.fill();
            x.fillStyle = '#1b1c1e'; x.beginPath(); x.arc(sx, sy, 8, 0, Math.PI * 2); x.fill();
            if (guarded) { x.fillStyle = '#cfd2d5'; x.fillRect(sx - 19, sy - 15, 5, 30); x.fillRect(sx + 14, sy - 15, 5, 30); }
            label(pos[Math.floor(rnd() * pos.length)], sx, sy - 20, 11);
            label(pos[Math.floor(rnd() * pos.length)], sx, sy + 30, 11, false);
            switches.push({ u: sx / W, v: 1 - sy / H, up: rnd() < 0.5, group: gname, gid });
          }
        } else if (kind < 0.8) {                           /* instruments: tape meters or round dials, black with white scales */
          const n = Math.max(1, Math.floor((pw - 30) / 96)), round = rnd() < 0.4;
          for (let k = 0; k < n; k++) {
            const ix = xx + 22 + k * 96, iy = y + 34, iw = 76, ihh = ph - 64;
            x.fillStyle = '#c9ccd0'; x.fillRect(ix - 3, iy - 3, iw + 6, ihh + 6);
            x.fillStyle = '#0c0d0e'; x.fillRect(ix, iy, iw, ihh);
            if (round && ihh > 70) {
              const cx = ix + iw / 2, cy = iy + ihh / 2, R = Math.min(iw, ihh) / 2 - 6;
              x.strokeStyle = '#ecebe4'; x.lineWidth = 2;
              for (let t = 0; t <= 10; t++) { const a = Math.PI * (0.75 + 1.5 * t / 10); x.beginPath(); x.moveTo(cx + Math.cos(a) * R, cy + Math.sin(a) * R); x.lineTo(cx + Math.cos(a) * R * 0.8, cy + Math.sin(a) * R * 0.8); x.stroke(); }
              const a = Math.PI * (0.75 + 1.5 * rnd()); x.lineWidth = 3; x.beginPath(); x.moveTo(cx, cy); x.lineTo(cx + Math.cos(a) * R * 0.85, cy + Math.sin(a) * R * 0.85); x.stroke();
            } else {
              x.fillStyle = '#ecebe4';
              for (let t = 0; t < 9; t++) x.fillRect(ix + 5, iy + 8 + t * (ihh - 16) / 8, t % 2 ? 9 : 15, 2);
              x.fillStyle = '#f4f4f0'; x.fillRect(ix + 26, iy + (ihh - 8) * (0.25 + 0.5 * rnd()), 44, 5);
            }
            label(words[Math.floor(rnd() * words.length)].split(' ')[0], ix + iw / 2, iy + ihh + 17, 12);
          }
        } else {                                           /* talkback indicators and caution lights */
          group(gx, gy, gw, gh, words[Math.floor(rnd() * words.length)]);
          for (let r = 0; r < Math.min(3, Math.floor(gh / 40)); r++) for (let k = 0; k < Math.floor((gw - 16) / 52); k++) {
            const lx = gx + 12 + k * 52, ly = gy + 22 + r * 40;
            x.fillStyle = '#c9ccd0'; x.fillRect(lx - 2, ly - 2, 44, 30);
            const on = rnd(); x.fillStyle = on < 0.12 ? '#6a5a22' : on < 0.5 ? '#2c2d2e' : '#9a9da0';
            x.fillRect(lx, ly, 40, 26);
            if (on >= 0.5) { x.fillStyle = '#2c2d2e'; for (let q = 0; q < 4; q++) x.fillRect(lx + 4 + q * 9, ly + 3, 4, 20); }
          }
        }
        xx += bw;
      }
      y += bh;
    }
  }
  for (const r of keepOut) {
    const x0 = r.u0 * W, x1 = r.u1 * W, y0 = (1 - r.v1) * H, y1 = (1 - r.v0) * H;
    x.fillStyle = rgb(-0.03); x.fillRect(x0, y0, x1 - x0, y1 - y0);
    x.strokeStyle = P.line; x.lineWidth = 2; x.strokeRect(x0 + 5, y0 + 5, x1 - x0 - 10, y1 - y0 - 10);
    for (const [sx, sy] of [[x0 + 12, y0 + 12], [x1 - 12, y0 + 12], [x0 + 12, y1 - 12], [x1 - 12, y1 - 12]]) screw(sx, sy);
  }
  const clear = switches.filter(sw => !keepOut.some(r => sw.u > r.u0 - 0.004 && sw.u < r.u1 + 0.004 && sw.v > r.v0 - 0.02 && sw.v < r.v1 + 0.02));
  const texture = new THREE.CanvasTexture(c);
  texture.colorSpace = THREE.SRGBColorSpace; texture.anisotropy = 8;
  return { texture, switches: clear };
}

export function makeDsky() {
  const c = document.createElement('canvas'); c.width = 300; c.height = 340;
  const x = c.getContext('2d'), texture = new THREE.CanvasTexture(c);
  texture.colorSpace = THREE.SRGBColorSpace;
  const seg = (s, px, py, size) => {
    x.font = `${size}px "Courier New", monospace`; x.fillStyle = '#7dff9a'; x.textAlign = 'right';
    x.shadowColor = '#3cff6a'; x.shadowBlur = 6; x.fillText(s, px, py); x.shadowBlur = 0;
  };
  function draw(prog, verb, noun, r1, r2, r3) {
    x.fillStyle = '#1b1d1e'; x.fillRect(0, 0, 300, 340);
    x.fillStyle = '#0a0b0b'; x.fillRect(14, 14, 272, 312);
    x.font = 'bold 13px Arial'; x.fillStyle = '#e7e5da'; x.textAlign = 'left';
    x.fillText('PROG', 200, 38); x.fillText('VERB', 30, 112); x.fillText('NOUN', 200, 112);
    seg(prog, 270, 82, 46); seg(verb, 110, 156, 46); seg(noun, 270, 156, 46);
    x.fillStyle = '#2b3a2e'; for (const yy of [170, 222, 274]) x.fillRect(26, yy, 250, 2);
    seg(r1, 274, 214, 44); seg(r2, 274, 266, 44); seg(r3, 274, 318, 44);
    texture.needsUpdate = true;
  }
  const f = (v, n = 5) => (v < 0 ? '-' : '+') + String(Math.min(99999, Math.round(Math.abs(v)))).padStart(n, '0');
  draw('02', '16', '65', '+00000', '+00000', '+00000');
  return { texture, draw, f };
}

export function makeTimer() {
  const c = document.createElement('canvas'); c.width = 384; c.height = 72;
  const x = c.getContext('2d'), texture = new THREE.CanvasTexture(c);
  texture.colorSpace = THREE.SRGBColorSpace;
  let last = '';
  /* the count to and from the launch in minutes and seconds; after an hour and forty minutes the mission's own
   * clock, hours : minutes : seconds (three digits of hours: the flight is 301 hours long) */
  function draw(t) {
    const neg = t < 0, a = Math.abs(t), sec = Math.floor(a % 60), two = n => String(n).padStart(2, '0');
    const str = a < 6000 ? (neg ? '-' : ' ') + two(Math.floor(a / 60)) + ':' + two(sec) : String(Math.floor(a / 3600)).padStart(3, '0') + ':' + two(Math.floor(a % 3600 / 60)) + ':' + two(sec);
    if (str === last) return; last = str;
    x.fillStyle = '#0b0b0b'; x.fillRect(0, 0, 384, 72);
    x.font = '56px "Courier New", monospace'; x.fillStyle = '#ff5a3a'; x.textAlign = 'center';
    x.shadowColor = '#ff3a1a'; x.shadowBlur = 8; x.fillText(str, 192, 56, 360); x.shadowBlur = 0;
    texture.needsUpdate = true;
  }
  draw(0);
  return { texture, draw };
}

export function makeBallTexture() {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256;
  const x = c.getContext('2d');
  x.fillStyle = '#dedcd4'; x.fillRect(0, 0, 512, 128);
  x.fillStyle = '#2a2a2a'; x.fillRect(0, 128, 512, 128);
  x.strokeStyle = '#7a7a7a'; x.lineWidth = 2;
  for (let i = 0; i <= 512; i += 512 / 36) { x.beginPath(); x.moveTo(i, 0); x.lineTo(i, 256); x.stroke(); }
  for (let j = 0; j <= 256; j += 256 / 18) { x.beginPath(); x.moveTo(0, j); x.lineTo(512, j); x.stroke(); }
  x.fillStyle = '#c33'; x.fillRect(0, 126, 512, 4);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
  return t;
}
