/*
 * models.js — everything you see, built from code. No model files, no images.
 *
 *   buildTank()      a 1:35-scale armoured vehicle, ~30 cm long: hull, turret,
 *                    gun, road wheels, and a real track — 60 individual links
 *                    per side that run round the wheels and follow the ground
 *                    speed of that side, so the tank visibly pivots on the spot.
 *   buildLauncher()  the shoulder-fired launcher in each hand
 *   buildRocket()    the rocket itself
 *
 * Axes: a tank faces +Z, up is +Y, its left is +X.
 */
import * as THREE from './vendor/three.module.min.js';

export function canvasTex(w, h, draw, { srgb = true, repeat = false } = {}) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  if (repeat) t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.anisotropy = 4;
  return t;
}
const rnd = (a, b) => a + Math.random() * (b - a);

/* ----------------------------------------------------------- tank assets */

let AS = null;

function camoTexture() {
  return canvasTex(512, 512, (ctx, w, h) => {
    ctx.fillStyle = '#56643a'; ctx.fillRect(0, 0, w, h);
    const cols = ['#3a4a2b', '#6b5b39', '#7e7b51', '#2f3b26', '#4b5733', '#665a3a'];
    for (let i = 0; i < 90; i++) {
      ctx.fillStyle = cols[i % cols.length];
      const cx = rnd(0, w), cy = rnd(0, h), r = rnd(18, 70);
      ctx.beginPath();
      for (let k = 0; k <= 12; k++) {
        const a = (k / 12) * Math.PI * 2, rr = r * rnd(0.55, 1.2);
        const x = cx + Math.cos(a) * rr, y = cy + Math.sin(a) * rr * 0.7;
        k ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      }
      ctx.fill();
    }
    // dirt along the bottom, fine grain everywhere
    const g = ctx.createLinearGradient(0, h * 0.55, 0, h);
    g.addColorStop(0, 'rgba(70,55,35,0)'); g.addColorStop(1, 'rgba(70,55,35,0.45)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
    const id = ctx.getImageData(0, 0, w, h), d = id.data;
    for (let i = 0; i < d.length; i += 4) {
      const n = (Math.random() - 0.5) * 26;
      d[i] += n; d[i + 1] += n; d[i + 2] += n;
    }
    ctx.putImageData(id, 0, 0);
    ctx.strokeStyle = 'rgba(20,20,10,0.25)'; ctx.lineWidth = 1;
    for (let i = 0; i < 160; i++) {      // scratches
      const x = rnd(0, w), y = rnd(0, h); ctx.beginPath(); ctx.moveTo(x, y); ctx.lineTo(x + rnd(-14, 14), y + rnd(-5, 5)); ctx.stroke();
    }
  }, { repeat: true });
}

function profileGeo(pts, depth, bevel = 0.0018) {
  const sh = new THREE.Shape();
  pts.forEach(([u, v], i) => (i ? sh.lineTo(u, v) : sh.moveTo(u, v)));
  const g = new THREE.ExtrudeGeometry(sh, { depth, bevelEnabled: true, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 1, steps: 1 });
  g.translate(0, 0, -depth / 2);
  g.rotateY(-Math.PI / 2);          // shape x -> +Z (forward), extrusion -> X (width)
  return g;
}

/* the track runs round a stadium: two straights of length L joined by arcs of radius R */
export const TRK = { L: 0.25, R: 0.026, N: 60 };
TRK.P = 2 * TRK.L + 2 * Math.PI * TRK.R;

function stadiumGeo(L, R, depth) {
  const sh = new THREE.Shape();
  sh.moveTo(-L / 2, 0); sh.lineTo(L / 2, 0);
  sh.absarc(L / 2, R, R, -Math.PI / 2, Math.PI / 2, false);
  sh.lineTo(-L / 2, 2 * R);
  sh.absarc(-L / 2, R, R, Math.PI / 2, Math.PI * 1.5, false);
  const g = new THREE.ExtrudeGeometry(sh, { depth, bevelEnabled: false, curveSegments: 14 });
  g.translate(0, 0, -depth / 2);
  g.rotateY(-Math.PI / 2);
  return g;
}

function wheelGeo(r, w) { const g = new THREE.CylinderGeometry(r, r, w, 18); g.rotateZ(Math.PI / 2); return g; }

export function tankAssets() {
  if (AS) return AS;
  const camoTex = camoTexture(); camoTex.repeat.set(5, 5);
  const a = {};
  a.camo = new THREE.MeshStandardMaterial({ map: camoTex, roughness: 0.82, metalness: 0.08 });
  a.dark = new THREE.MeshStandardMaterial({ color: 0x1c1e1d, roughness: 0.85, metalness: 0.3 });
  a.rubber = new THREE.MeshStandardMaterial({ color: 0x17181a, roughness: 0.95, metalness: 0.05 });
  a.steel = new THREE.MeshStandardMaterial({ color: 0x33373a, roughness: 0.55, metalness: 0.6 });
  a.metal = new THREE.MeshStandardMaterial({ color: 0x555a58, roughness: 0.4, metalness: 0.7 });
  a.olive = new THREE.MeshStandardMaterial({ color: 0x4a5230, roughness: 0.8, metalness: 0.1 });
  a.lamp = new THREE.MeshStandardMaterial({ color: 0xffeeb0, emissive: 0xffd070, emissiveIntensity: 0.9, roughness: 0.3 });
  a.char = new THREE.MeshStandardMaterial({ color: 0x0d0c0b, roughness: 1, metalness: 0 });
  a.decal = new THREE.MeshBasicMaterial({
    map: canvasTex(64, 40, (ctx, w, h) => {
      ctx.clearRect(0, 0, w, h); ctx.fillStyle = '#e9e4cf'; ctx.font = 'bold 30px Arial'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
      ctx.fillText(String(100 + ((Math.random() * 899) | 0)), w / 2, h / 2 + 1);
    }), transparent: true, opacity: 0.8, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -2,
  });

  a.hullGeo = profileGeo([[-0.14, 0.018], [-0.145, 0.052], [-0.125, 0.067], [0.04, 0.067], [0.098, 0.054], [0.14, 0.034], [0.14, 0.018], [0.1, 0.012], [-0.1, 0.012]], 0.112);
  a.turretGeo = profileGeo([[-0.068, 0], [-0.074, 0.017], [-0.048, 0.036], [0.028, 0.042], [0.068, 0.027], [0.074, 0.008], [0.06, 0]], 0.088, 0.0015);
  a.trackBodyGeo = stadiumGeo(TRK.L, TRK.R - 0.003, 0.034);
  a.linkGeo = new THREE.BoxGeometry(0.042, 0.0045, 0.0085);
  a.wheelGeo = wheelGeo(0.0148, 0.032);
  a.capGeo = wheelGeo(0.0085, 0.038);
  a.sprocketGeo = wheelGeo(0.0205, 0.03);
  a.boltGeo = new THREE.BoxGeometry(0.004, 0.003, 0.004);
  a.fenderGeo = new THREE.BoxGeometry(0.046, 0.003, 0.29);
  const barrel = new THREE.CylinderGeometry(0.0046, 0.0052, 0.16, 12); barrel.rotateX(Math.PI / 2); barrel.translate(0, 0, 0.08);
  a.barrelGeo = barrel;
  const sleeve = new THREE.CylinderGeometry(0.0072, 0.0072, 0.05, 12); sleeve.rotateX(Math.PI / 2); sleeve.translate(0, 0, 0.025);
  a.sleeveGeo = sleeve;
  const brake = new THREE.CylinderGeometry(0.0075, 0.0075, 0.022, 10); brake.rotateX(Math.PI / 2); brake.translate(0, 0, 0.17);
  a.brakeGeo = brake;
  const evac = new THREE.CylinderGeometry(0.0068, 0.0068, 0.02, 10); evac.rotateX(Math.PI / 2); evac.translate(0, 0, 0.1);
  a.evacGeo = evac;
  a.cupolaGeo = new THREE.CylinderGeometry(0.012, 0.0135, 0.009, 14);
  a.exhaustGeo = (() => { const g = new THREE.CylinderGeometry(0.0055, 0.0055, 0.024, 8); g.rotateX(Math.PI / 2); return g; })();
  a.antGeo = new THREE.CylinderGeometry(0.0007, 0.0011, 0.2, 5);
  a.shadowTex = canvasTex(64, 64, (ctx, w, h) => {
    const g = ctx.createRadialGradient(w / 2, h / 2, 2, w / 2, h / 2, w / 2);
    g.addColorStop(0, 'rgba(0,0,0,0.55)'); g.addColorStop(0.6, 'rgba(0,0,0,0.25)'); g.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = g; ctx.fillRect(0, 0, w, h);
  });
  a.shadowGeo = new THREE.PlaneGeometry(0.42, 0.42).rotateX(-Math.PI / 2);
  a.shadowMat = new THREE.MeshBasicMaterial({ map: a.shadowTex, transparent: true, depthWrite: false, polygonOffset: true, polygonOffsetFactor: -1 });
  AS = a;
  return a;
}

const M = (geo, mat, x = 0, y = 0, z = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); return m; };

/** Place all links of one track. `off` is how far the belt has travelled (m). */
const tmp = new THREE.Object3D();
export function setTrack(inst, trackX, off) {
  const { L, R, N, P } = TRK, Rl = R - 0.0008;
  for (let k = 0; k < N; k++) {
    let s = (k * P / N + off) % P; if (s < 0) s += P;
    let u, v, tu, tv;
    const arc = Math.PI * R;
    if (s < L) { u = -L / 2 + s; v = 2 * R - 0.0008; tu = 1; tv = 0; }
    else if (s < L + arc) { const al = Math.PI / 2 - (s - L) / R; u = L / 2 + Rl * Math.cos(al); v = R + Rl * Math.sin(al); tu = Math.sin(al); tv = -Math.cos(al); }
    else if (s < 2 * L + arc) { u = L / 2 - (s - L - arc); v = 0.0008; tu = -1; tv = 0; }
    else { const al = -Math.PI / 2 - (s - 2 * L - arc) / R; u = -L / 2 + Rl * Math.cos(al); v = R + Rl * Math.sin(al); tu = Math.sin(al); tv = -Math.cos(al); }
    tmp.position.set(trackX, v, u);
    tmp.rotation.set(-Math.atan2(tv, tu), 0, 0);
    tmp.updateMatrix();
    inst.setMatrixAt(k, tmp.matrix);
  }
  inst.instanceMatrix.needsUpdate = true;
}

export function buildTank() {
  const a = tankAssets();
  const group = new THREE.Group();
  const body = new THREE.Group();
  group.add(body);
  const parts = [];          // every mesh, so a wreck can be recoloured
  const add = (parent, m) => { parent.add(m); parts.push(m); return m; };

  add(body, M(a.hullGeo, a.camo));

  const TX = 0.079;
  const tracks = [], wheels = [];
  for (const side of [1, -1]) {
    const x = side * TX;
    add(body, M(a.trackBodyGeo, a.dark, x, 0, 0));
    const links = new THREE.InstancedMesh(a.linkGeo, a.rubber, TRK.N);
    links.frustumCulled = false;
    links.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    setTrack(links, x, 0);
    body.add(links);
    tracks.push({ mesh: links, x, off: 0 });
    // road wheels, drive sprocket (rear), idler (front)
    const ws = [];
    for (const u of [-0.095, -0.049, -0.003, 0.043, 0.089]) {
      const w = new THREE.Group(); w.position.set(x, 0.0205, u);
      w.add(M(a.wheelGeo, a.steel)); w.add(M(a.capGeo, a.metal)); w.add(M(a.boltGeo, a.dark, 0.0195, 0.0055, 0));
      body.add(w); ws.push({ g: w, r: 0.0148 });
    }
    for (const [u, r] of [[-0.125, 0.0205], [0.125, 0.0205]]) {
      const w = new THREE.Group(); w.position.set(x, 0.026, u);
      w.add(M(a.sprocketGeo, a.steel)); w.add(M(a.capGeo, a.metal)); w.add(M(a.boltGeo, a.dark, 0.0195, 0.007, 0));
      body.add(w); ws.push({ g: w, r });
    }
    wheels.push(ws);
    // fender, mudflap, tool box
    add(body, M(a.fenderGeo, a.camo, x, 0.0575, 0));
    add(body, M(new THREE.BoxGeometry(0.046, 0.02, 0.002), a.olive, x, 0.045, 0.148));
    add(body, M(new THREE.BoxGeometry(0.028, 0.011, 0.034), a.olive, x, 0.0655, -0.06));
  }

  // hull details
  add(body, M(new THREE.BoxGeometry(0.07, 0.004, 0.06), a.dark, 0, 0.0675, -0.085));
  for (const s of [-1, 1]) {
    add(body, M(a.exhaustGeo, a.metal, s * 0.04, 0.05, -0.15));
    add(body, M(new THREE.BoxGeometry(0.012, 0.012, 0.01), a.lamp, s * 0.05, 0.05, 0.1395));
    add(body, M(new THREE.BoxGeometry(0.014, 0.018, 0.026), a.olive, s * 0.03, 0.078, -0.115));
  }
  add(body, M(new THREE.BoxGeometry(0.1, 0.002, 0.012), a.dark, 0, 0.066, 0.045));     // hatch seam

  // turret
  const turret = new THREE.Group();
  turret.position.set(0, 0.066, -0.004);
  body.add(turret);
  add(turret, M(a.turretGeo, a.camo));
  add(turret, M(a.cupolaGeo, a.metal, 0.02, 0.0455, -0.032));
  add(turret, M(new THREE.BoxGeometry(0.003, 0.012, 0.003), a.dark, 0.02, 0.055, -0.03));
  const mg = add(turret, M(new THREE.CylinderGeometry(0.0016, 0.0016, 0.026, 6), a.dark, 0.02, 0.054, -0.012));
  mg.rotation.x = Math.PI / 2;
  const ant = add(turret, M(a.antGeo, a.dark, -0.028, 0.12, -0.062)); ant.rotation.x = -0.14;
  add(turret, M(new THREE.BoxGeometry(0.036, 0.03, 0.014), a.camo, 0, 0.02, 0.07));   // mantlet
  add(turret, M(new THREE.BoxGeometry(0.05, 0.016, 0.03), a.olive, 0, 0.031, -0.066)); // rear bustle bin
  for (const s of [-1, 1]) {
    const d = new THREE.Mesh(new THREE.PlaneGeometry(0.03, 0.019), a.decal);
    d.position.set(s * 0.0442, 0.018, -0.012); d.rotation.y = s * Math.PI / 2; turret.add(d);
  }

  const gun = new THREE.Group();
  gun.position.set(0, 0.021, 0.076);
  turret.add(gun);
  add(gun, M(a.barrelGeo, a.steel));
  add(gun, M(a.sleeveGeo, a.camo));
  add(gun, M(a.evacGeo, a.metal));
  add(gun, M(a.brakeGeo, a.metal));
  const muzzle = new THREE.Object3D();
  muzzle.position.set(0, 0, 0.184);
  gun.add(muzzle);

  const shadow = new THREE.Mesh(a.shadowGeo, a.shadowMat);
  shadow.position.y = 0.002; shadow.renderOrder = 1;
  group.add(shadow);

  return { group, body, turret, gun, muzzle, tracks, wheels, parts, shadow };
}

/* --------------------------------------------------------------- rocket */

let RA = null;
function rocketAssets() {
  if (RA) return RA;
  const r = {};
  r.olive = new THREE.MeshStandardMaterial({ color: 0x575f3b, roughness: 0.7, metalness: 0.3 });
  r.head = new THREE.MeshStandardMaterial({ color: 0x6f7552, roughness: 0.6, metalness: 0.35 });
  r.band = new THREE.MeshStandardMaterial({ color: 0xb02b20, roughness: 0.6, metalness: 0.2 });
  r.dark = new THREE.MeshStandardMaterial({ color: 0x1c1d1c, roughness: 0.5, metalness: 0.6 });
  r.steel = new THREE.MeshStandardMaterial({ color: 0x7c8280, roughness: 0.35, metalness: 0.85 });
  const cz = (rt, rb, h, z, seg = 18) => { const g = new THREE.CylinderGeometry(rt, rb, h, seg); g.rotateX(Math.PI / 2); g.translate(0, 0, z); return g; };
  r.motor = cz(0.017, 0.017, 0.16, -0.12);
  r.nozzle = cz(0.017, 0.024, 0.03, -0.215);
  r.stab = cz(0.006, 0.006, 0.05, -0.02, 8);
  r.warhead = cz(0.044, 0.04, 0.1, 0.05);
  r.bandGeo = cz(0.0445, 0.0445, 0.012, 0.045);
  r.ogive = (() => { const g = new THREE.ConeGeometry(0.044, 0.1, 18); g.rotateX(Math.PI / 2); g.translate(0, 0, 0.15); return g; })();
  r.tip = (() => { const g = new THREE.CylinderGeometry(0.004, 0.009, 0.025, 8); g.rotateX(Math.PI / 2); g.translate(0, 0, 0.212); return g; })();
  r.fin = new THREE.BoxGeometry(0.0025, 0.085, 0.05);
  RA = r;
  return r;
}

/** A rocket facing +Z, about 0.43 m long at scale 1. */
export function buildRocket() {
  const r = rocketAssets();
  const g = new THREE.Group();
  g.add(M(r.motor, r.olive)); g.add(M(r.nozzle, r.dark)); g.add(M(r.stab, r.steel));
  g.add(M(r.warhead, r.head)); g.add(M(r.bandGeo, r.band)); g.add(M(r.ogive, r.head)); g.add(M(r.tip, r.dark));
  for (let i = 0; i < 2; i++) {
    const f = M(r.fin, r.dark, 0, 0, -0.19);
    f.rotation.z = (i * Math.PI) / 2;
    f.position.set(0, 0, -0.19);
    g.add(f);
  }
  return g;
}

/* ------------------------------------------------------------- launcher */

export function makePanel(w, h, pw, ph, opts = {}) {
  const canvas = document.createElement('canvas');
  canvas.width = pw; canvas.height = ph;
  const ctx = canvas.getContext('2d');
  const tex = new THREE.CanvasTexture(canvas);
  tex.colorSpace = THREE.SRGBColorSpace;
  const mat = new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: opts.depthTest ?? true, depthWrite: false, toneMapped: false });
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(w, h), mat);
  return { mesh, ctx, canvas, tex, w: pw, h: ph, refresh() { tex.needsUpdate = true; } };
}

/** Shoulder-fired launcher, RPG-style. Points down -Z from the controller. */
export function buildLauncher() {
  const g = new THREE.Group();
  const olive = new THREE.MeshStandardMaterial({ color: 0x4b5233, roughness: 0.72, metalness: 0.28 });
  const oliveDark = new THREE.MeshStandardMaterial({ color: 0x353b25, roughness: 0.75, metalness: 0.3 });
  const dark = new THREE.MeshStandardMaterial({ color: 0x1b1d1c, roughness: 0.55, metalness: 0.55 });
  const steel = new THREE.MeshStandardMaterial({ color: 0x80868a, roughness: 0.3, metalness: 0.85 });
  const wood = new THREE.MeshStandardMaterial({ color: 0x6a4527, roughness: 0.6, metalness: 0.05 });
  const cyl = (rt, rb, h, z, y, mat, seg = 24, open = false) => {
    const geo = new THREE.CylinderGeometry(rt, rb, h, seg, 1, open); geo.rotateX(Math.PI / 2);
    const m = new THREE.Mesh(geo, mat); m.position.set(0, y, z); return m;
  };
  const Y = 0.058;

  g.add(cyl(0.04, 0.04, 0.62, -0.14, Y, olive));                         // tube
  const venturi = cyl(0.04, 0.068, 0.1, 0.22, Y, olive, 24, true);       // rear flare (open)
  venturi.material = olive.clone(); venturi.material.side = THREE.DoubleSide; g.add(venturi);
  const bore = new THREE.Mesh(new THREE.CircleGeometry(0.05, 20), new THREE.MeshBasicMaterial({ color: 0x050505 }));
  bore.position.set(0, Y, 0.215); g.add(bore);
  for (const z of [0.12, -0.04, -0.2, -0.36]) {                          // reinforcing bands
    const b = new THREE.Mesh(new THREE.TorusGeometry(0.0415, 0.0042, 8, 28), oliveDark);
    b.position.set(0, Y, z); g.add(b);
  }
  g.add(cyl(0.047, 0.047, 0.035, -0.43, Y, steel, 24));                  // muzzle ring

  // the loaded rocket, nose forward, motor hidden in the tube
  const rocket = buildRocket();
  rocket.scale.setScalar(1);
  rocket.rotation.y = Math.PI;
  rocket.position.set(0, Y, -0.56);
  g.add(rocket);

  // handles: pistol grip held by the hand at the controller, foregrip, trigger
  const grip = new THREE.Mesh(new THREE.BoxGeometry(0.034, 0.105, 0.042), wood);
  grip.position.set(0, -0.022, 0.03); grip.rotation.x = 0.28; g.add(grip);
  const fore = new THREE.Mesh(new THREE.BoxGeometry(0.03, 0.075, 0.034), wood);
  fore.position.set(0, -0.012, -0.2); fore.rotation.x = -0.12; g.add(fore);
  const guard = new THREE.Mesh(new THREE.TorusGeometry(0.016, 0.0028, 6, 14, Math.PI), dark);
  guard.position.set(0, 0.012, -0.014); guard.rotation.set(0, Math.PI / 2, Math.PI); g.add(guard);
  const trig = new THREE.Mesh(new THREE.BoxGeometry(0.006, 0.02, 0.008), dark);
  trig.position.set(0, 0.005, -0.012); g.add(trig);
  const heat = new THREE.Mesh(new THREE.BoxGeometry(0.07, 0.01, 0.22), wood);   // wooden heat shield
  heat.position.set(0, Y + 0.043, -0.2); g.add(heat);

  // optical sight on the left, with a faint glowing lens
  const mount = new THREE.Mesh(new THREE.BoxGeometry(0.03, 0.012, 0.05), dark);
  mount.position.set(0.04, Y + 0.052, -0.06); g.add(mount);
  const scope = cyl(0.014, 0.017, 0.1, -0.06, Y + 0.075, dark, 14);
  scope.position.x = 0.045; g.add(scope);
  const lens = new THREE.Mesh(new THREE.CircleGeometry(0.012, 12), new THREE.MeshBasicMaterial({ color: 0x4a8cff, transparent: true, opacity: 0.8 }));
  lens.position.set(0.045, Y + 0.075, -0.0095 + 0.0); lens.position.z = -0.06 + 0.0505; g.add(lens);
  const post = new THREE.Mesh(new THREE.BoxGeometry(0.003, 0.02, 0.003), dark);
  post.position.set(0, Y + 0.052, -0.41); g.add(post);

  // ready light
  const ledMat = new THREE.MeshBasicMaterial({ color: 0x44ff66 });
  const led = new THREE.Mesh(new THREE.SphereGeometry(0.0055, 8, 8), ledMat);
  led.position.set(-0.035, Y + 0.04, 0.12); g.add(led);

  // readout on the back of the tube, tilted up toward the eyes
  const panel = makePanel(0.15, 0.085, 300, 170);
  panel.mesh.position.set(0, Y + 0.085, 0.16);
  panel.mesh.rotation.x = -0.75;
  g.add(panel.mesh);

  const muzzle = new THREE.Object3D(); muzzle.position.set(0, Y, -0.62); g.add(muzzle);
  const rear = new THREE.Object3D(); rear.position.set(0, Y, 0.28); g.add(rear);

  return { group: g, rocket, muzzle, rear, led, ledMat, panel };
}
