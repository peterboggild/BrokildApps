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

function mat(color, opts = {}) { return new THREE.MeshLambertMaterial({ color, ...CABIN_STENCIL, ...opts }); }

export function makeCabin() {
  const g = new THREE.Group();                 /* shaken as a whole; you are not */
  const wall = mat(0x9b998f), dark = mat(0x3a3c3e), metal = mat(0x7c8086), bezel = mat(0x26282a), cushion = mat(0x6e6a5c);
  const point = (phi, z, out = 0) => {        /* a point on the wall (out: metres outward along the wall normal) */
    const a = phi * Math.PI / 180, r = rAt(z);
    const nr = Math.cos(ALPHA), nz = -Math.sin(ALPHA);
    return new THREE.Vector3(AX + Math.cos(a) * (r + nr * out), AY + Math.sin(a) * (r + nr * out), z + nz * out);
  };

  /* ---------------- the conical wall, with the window openings left out */
  const NA = 200, NZ = 80, pos = [], nor = [], idx = [];
  for (let j = 0; j <= NZ; j++) for (let i = 0; i <= NA; i++) {
    const z = Z_BASE + (Z_APEX + 0.05 - Z_BASE) * j / NZ, a = i / NA * 360;
    const p = point(a, z), ar = a * Math.PI / 180;
    pos.push(p.x, p.y, p.z);
    nor.push(-Math.cos(ar) * Math.cos(ALPHA), -Math.sin(ar) * Math.cos(ALPHA), Math.sin(ALPHA));
  }
  const inWindow = (a, z) => WINDOWS.some(w => {
    let da = ((a - w.phi + 540) % 360) - 180;
    const du = da * Math.PI / 180 * rAt(z), dv = (z - w.z) / Math.cos(ALPHA);
    return w.round ? (du * du + dv * dv) < w.hu * w.hu : Math.abs(du) < w.hu && Math.abs(dv) < w.hv;
  });
  for (let j = 0; j < NZ; j++) for (let i = 0; i < NA; i++) {
    const z = Z_BASE + (Z_APEX + 0.05 - Z_BASE) * (j + 0.5) / NZ, a = (i + 0.5) / NA * 360;
    if (inWindow(a, z)) continue;
    const p = j * (NA + 1) + i, q = p + NA + 1;
    idx.push(p, p + 1, q, q, p + 1, q + 1);
  }
  const wg = new THREE.BufferGeometry();
  wg.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); wg.setAttribute('normal', new THREE.Float32BufferAttribute(nor, 3)); wg.setIndex(idx);
  g.add(new THREE.Mesh(wg, wall));
  /* aft bulkhead behind you, and the tunnel end ahead */
  const aft = new THREE.Mesh(new THREE.CircleGeometry(R_BASE, 64), wall); aft.position.set(AX, AY, Z_BASE); aft.rotation.y = Math.PI; g.add(aft);
  const fwd = new THREE.Mesh(new THREE.CircleGeometry(0.4, 32), dark); fwd.position.set(AX, AY, Z_APEX + 0.06); g.add(fwd);

  /* ---------------- windows: a deep reveal through the wall, a bezel round it, and the boost protective cover
   * outside (a dark surface 15 cm out; over the hatch window it has a 20 cm hole) */
  const covers = [];
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
    const reveal = new THREE.Mesh(new THREE.ExtrudeGeometry(ring(0, 0.012), { depth, bevelEnabled: false, curveSegments: 40 }), bezel);
    frame.add(reveal);
    const bz = new THREE.Mesh(new THREE.ShapeGeometry(ring(0, 0.05), 40), metal); bz.position.z = -0.006; bz.rotation.y = Math.PI; bz.scale.x = -1; frame.add(bz);
    /* the pane, a faint reflection only: it must not hide the world, so it writes no stencil and draws last */
    /* the cover */
    const cs = new THREE.Shape(), big = Math.max(w.hu, w.hv) + 0.12;
    cs.moveTo(-big, -big); cs.lineTo(big, -big); cs.lineTo(big, big); cs.lineTo(-big, big); cs.closePath();
    if (w.name === 'hatch') { const h = new THREE.Path(); h.absarc(0, 0, 0.1, 0, Math.PI * 2, true); cs.holes.push(h); }
    const cover = new THREE.Mesh(new THREE.ShapeGeometry(cs, 32), mat(0x2c2b29, { side: THREE.DoubleSide }));
    cover.position.z = depth + 0.03; frame.add(cover); covers.push(cover);
    g.add(frame);
  }

  /* ---------------- the main display console: three panels facing the three couches */
  const panel = makePanelTexture();
  const panels = [
    { u0: 0.00, u1: 0.34, c: [-0.22, -0.16, -0.56], w: 0.78, h: 0.58, look: [0, -0.05, 0] },
    { u0: 0.33, u1: 0.67, c: [0.58, -0.12, -0.62], w: 0.82, h: 0.62, look: [0.6, -0.05, 0] },
    { u0: 0.66, u1: 1.00, c: [1.38, -0.16, -0.56], w: 0.78, h: 0.58, look: [1.25, -0.05, 0] }
  ];
  const panelMat = new THREE.MeshLambertMaterial({ map: panel.texture, ...CABIN_STENCIL });
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
      switches.push({ mesh: m, x: ((s.u - p.u0) / (p.u1 - p.u0) - 0.5) * p.w, y: (s.v - 0.5) * p.h, up: s.up });
    }
  });
  /* toggle switches, in 3D: a hex nut and a lever, tipped up or down */
  const lever = new THREE.CylinderGeometry(0.0022, 0.0028, 0.022, 6); lever.translate(0, 0.011, 0);
  const nut = new THREE.CylinderGeometry(0.0065, 0.0065, 0.004, 6); nut.rotateX(Math.PI / 2);
  const im1 = new THREE.InstancedMesh(lever, mat(0xd9dadb), switches.length), im2 = new THREE.InstancedMesh(nut, metal, switches.length);
  const m4 = new THREE.Matrix4(), q = new THREE.Quaternion(), e = new THREE.Euler(), one = new THREE.Vector3(1, 1, 1);
  switches.forEach((s, k) => {
    s.mesh.updateMatrix();
    const local = new THREE.Matrix4().compose(new THREE.Vector3(s.x, s.y, 0.004), q.setFromEuler(e.set(Math.PI / 2 + (s.up ? -0.5 : 0.5), 0, 0)), one);
    im1.setMatrixAt(k, m4.multiplyMatrices(s.mesh.matrix, local));
    im2.setMatrixAt(k, m4.multiplyMatrices(s.mesh.matrix, new THREE.Matrix4().makeTranslation(s.x, s.y, 0.002)));
  });
  g.add(im1, im2);

  /* the DSKY (guidance computer display) on the centre panel, and the mission event timer */
  const dsky = makeDsky();
  const dm = new THREE.Mesh(new THREE.PlaneGeometry(0.15, 0.17), new THREE.MeshBasicMaterial({ map: dsky.texture, ...CABIN_STENCIL }));
  dm.position.set(-0.27, -0.12, 0.006); panels[1].mesh.add(dm);
  const timer = makeTimer();
  const tm = new THREE.Mesh(new THREE.PlaneGeometry(0.13, 0.035), new THREE.MeshBasicMaterial({ map: timer.texture, ...CABIN_STENCIL }));
  tm.position.set(0.0, 0.26, 0.006); panels[1].mesh.add(tm);
  /* the attitude indicator ("eight ball") on your panel: a sphere that turns with the spacecraft */
  const ballTex = makeBallTexture();
  const ball = new THREE.Mesh(new THREE.SphereGeometry(0.05, 32, 24), new THREE.MeshLambertMaterial({ map: ballTex, ...CABIN_STENCIL }));
  ball.position.set(0.05, 0.1, 0.005); panels[0].mesh.add(ball);
  const ballRim = new THREE.Mesh(new THREE.RingGeometry(0.05, 0.064, 40), bezel); ballRim.position.set(0.05, 0.1, 0.03); panels[0].mesh.add(ballRim);

  /* ---------------- couches: yours (you see its headrest and armrests), and the other two, with the crew */
  for (const [x, crew] of [[0, false], [0.65, true], [1.3, true]]) {
    const c = new THREE.Group(); c.position.x = x; g.add(c);
    const rail = new THREE.CylinderGeometry(0.018, 0.018, 1, 8);
    for (const sx of [-0.27, 0.27]) {
      const r = new THREE.Mesh(rail, metal); r.position.set(sx, -0.55, 0.22); r.scale.y = 1.1; r.rotation.x = 0.12; c.add(r);
      const arm = new THREE.Mesh(new THREE.BoxGeometry(0.07, 0.05, 0.42), cushion); arm.position.set(sx * 1.05, -0.52, -0.05); c.add(arm);
    }
    const head = new THREE.Mesh(new THREE.BoxGeometry(0.34, 0.3, 0.07), cushion); head.position.set(0, 0.02, 0.25); c.add(head);
    const back = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.75, 0.09), cushion); back.position.set(0, -0.55, 0.27); c.add(back);
    if (crew) c.add(makeCrewman());
  }
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

  return {
    group: g, covers, dsky, timer, ball, sunIn, amb, panels,
    setCovers(on) { covers.forEach(c => { c.visible = on; }); },
    /* daylight through the open windows: a soft extra fill from the Sun's side */
    setDaylight(k, sunLocal) {
      sunIn.intensity = 0.9 * k; sunIn.position.copy(sunLocal).multiplyScalar(3); sunIn.target.position.set(0, 0, 0);
      amb.intensity = 0.55 + 0.35 * k;
    }
  };
}

/* a crewman in a pressure suit, visor down, seated as you are */
function makeCrewman() {
  const g = new THREE.Group();
  const suit = new THREE.MeshLambertMaterial({ color: 0xd6d3c8, ...CABIN_STENCIL });
  const visor = new THREE.MeshStandardMaterial({ color: 0x6a4a10, metalness: 0.9, roughness: 0.15, emissive: 0x1a1204, ...CABIN_STENCIL });
  const torso = new THREE.Mesh(new THREE.CapsuleGeometry(0.17, 0.32, 6, 12), suit); torso.position.set(0, -0.42, 0.12); g.add(torso);
  const helmet = new THREE.Mesh(new THREE.SphereGeometry(0.155, 24, 16), suit); helmet.position.set(0, 0.0, 0.1); g.add(helmet);
  const v = new THREE.Mesh(new THREE.SphereGeometry(0.158, 24, 16, Math.PI * 0.2, Math.PI * 0.6, Math.PI * 0.28, Math.PI * 0.42), visor);
  v.position.copy(helmet.position); v.rotation.y = Math.PI; g.add(v);
  for (const s of [-1, 1]) {
    const arm = new THREE.Mesh(new THREE.CapsuleGeometry(0.06, 0.3, 4, 8), suit);
    arm.position.set(s * 0.24, -0.45, 0.0); arm.rotation.x = 0.9; g.add(arm);
  }
  return g;
}

/* ---------------------------------------------------------------- textures made in code */
function makePanelTexture() {
  const W = 4096, H = 1024, c = document.createElement('canvas'); c.width = W; c.height = H;
  const x = c.getContext('2d');
  let seed = 17;
  const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  x.fillStyle = '#5d6266'; x.fillRect(0, 0, W, H);
  const switches = [];
  const words = ['CMC', 'SCS', 'SPS', 'RCS', 'BMAG', 'EDS', 'ELS', 'TVC', 'GDC', 'FDAI', 'PRPLNT', 'O2', 'H2', 'FUEL CELL', 'MAIN BUS', 'BATT', 'CRYO', 'ECS', 'GLYCOL', 'CABIN', 'SUIT', 'DOCK', 'PYRO', 'SECS', 'MASTER', 'ARM', 'LOGIC', 'ABORT', 'LV', 'GUID', 'XLUNAR', 'S-II/S-IVB', 'TWR JETT', 'CSM/LV SEP', 'APEX', 'DROGUE', 'MAIN DEPLOY'];
  const label = (s, px, py, size = 15) => { x.font = `bold ${size}px Arial, sans-serif`; x.fillStyle = '#e9e8e2'; x.textAlign = 'center'; x.fillText(s, px, py); };
  /* sub-panels: framed blocks of switches, gauges, lights */
  for (let sec = 0; sec < 3; sec++) {
    const X0 = sec * W / 3 + 30, X1 = (sec + 1) * W / 3 - 30;
    let y = 40;
    while (y < H - 120) {
      const bh = 150 + Math.floor(rnd() * 120);
      let xx = X0;
      while (xx < X1 - 160) {
        const bw = Math.min(X1 - xx, 180 + Math.floor(rnd() * 260));
        x.fillStyle = '#4d5256'; x.fillRect(xx, y, bw - 10, bh - 10);
        x.strokeStyle = '#2b2e31'; x.lineWidth = 3; x.strokeRect(xx, y, bw - 10, bh - 10);
        const kind = rnd();
        label(words[Math.floor(rnd() * words.length)], xx + bw / 2 - 5, y + 22, 16);
        if (kind < 0.55) {                                 /* a row or two of toggle switches */
          const rows = bh > 210 ? 2 : 1, n = Math.max(2, Math.floor((bw - 30) / 55));
          for (let r = 0; r < rows; r++) for (let k = 0; k < n; k++) {
            const sx = xx + 25 + k * ((bw - 50) / Math.max(1, n - 1)) - 5, sy = y + 70 + r * 75;
            x.fillStyle = '#1d1f21'; x.beginPath(); x.arc(sx, sy, 12, 0, Math.PI * 2); x.fill();
            label(['ON', 'OFF', 'AUTO', 'CMD', 'A', 'B', '1', '2'][Math.floor(rnd() * 8)], sx, sy - 18, 11);
            switches.push({ u: sx / W, v: 1 - sy / H, up: rnd() < 0.5 });
          }
        } else if (kind < 0.8) {                           /* gauges: tapes or round dials */
          const n = Math.max(1, Math.floor((bw - 20) / 90));
          for (let k = 0; k < n; k++) {
            const gx = xx + 20 + k * 90, gy = y + 40;
            x.fillStyle = '#0d0e0f'; x.fillRect(gx, gy, 70, bh - 70);
            x.fillStyle = '#e8e6dc';
            for (let t = 0; t < 8; t++) x.fillRect(gx + 4, gy + 8 + t * (bh - 86) / 7, 14, 2);
            x.fillStyle = '#f0f0f0'; x.fillRect(gx + 20, gy + (bh - 70) * (0.3 + 0.4 * rnd()), 46, 4);
          }
        } else {                                           /* caution and warning lights */
          for (let r = 0; r < 3; r++) for (let k = 0; k < Math.floor((bw - 20) / 50); k++) {
            x.fillStyle = rnd() < 0.15 ? '#5a4a1a' : '#2a2a26'; x.fillRect(xx + 12 + k * 50, y + 40 + r * 34, 42, 26);
          }
        }
        xx += bw;
      }
      y += bh;
    }
  }
  const texture = new THREE.CanvasTexture(c);
  texture.colorSpace = THREE.SRGBColorSpace; texture.anisotropy = 8;
  return { texture, switches };
}

function makeDsky() {
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

function makeTimer() {
  const c = document.createElement('canvas'); c.width = 256; c.height = 72;
  const x = c.getContext('2d'), texture = new THREE.CanvasTexture(c);
  texture.colorSpace = THREE.SRGBColorSpace;
  let last = '';
  function draw(t) {
    const neg = t < 0, a = Math.abs(t), m = Math.floor(a / 60), s = Math.floor(a % 60);
    const str = (neg ? '-' : ' ') + String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');
    if (str === last) return; last = str;
    x.fillStyle = '#0b0b0b'; x.fillRect(0, 0, 256, 72);
    x.font = '56px "Courier New", monospace'; x.fillStyle = '#ff5a3a'; x.textAlign = 'center';
    x.shadowColor = '#ff3a1a'; x.shadowBlur = 8; x.fillText(str, 128, 56); x.shadowBlur = 0;
    texture.needsUpdate = true;
  }
  draw(0);
  return { texture, draw };
}

function makeBallTexture() {
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
