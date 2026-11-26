/* The Apollo-Saturn V from outside: the three stages, the Instrument Unit, the adapter round the Lunar Module, the
 * Service Module, the Command Module with its reflective skin, the boost protective cover and the launch escape
 * tower. Engines with their flames, the stages falling away at staging, the tower and cover flying off.
 *
 * Vehicle frame: origin on the vehicle's axis at the Commander's eye height, +y along the nose, x east and z south as
 * on the pad. On the pad the frame is the pad frame shifted up by EYE_H; in flight the group is turned with the
 * spacecraft. Heights below follow the pad (pad.js) with the stage lengths a little compressed to fit the eye height.
 * The Commander's eye point is EYE_OFF in this frame. */
import * as THREE from 'three';
import { makeLM } from './lm-model.js?v=35';
import { EYE_H, SUN, EVENTS } from './ascent.js?v=35';

export const STACK = {
  base: 8.5, sicTop: 50.5, skirtTop: 54.5, siiTop: 75, sivbAft: 80.5, sivbTop: 91.2, iuTop: 92.1, slaTop: 100.6,
  smTop: EYE_H - 1.0, cmTop: EYE_H + 2.4, lesTop: EYE_H + 13.5,
  R1: 5.05, R3: 3.3, RSM: 1.96
};
export const EYE_OFF = new THREE.Vector3(0.6, 0, -0.15);
const Y = h => h - EYE_H;
const smooth = (a, b, x) => { const t = Math.min(1, Math.max(0, (x - a) / (b - a))); return t * t * (3 - 2 * t); };

export function canvasTex(w, h, draw, repeatU = 1) {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8;
  t.wrapS = THREE.RepeatWrapping; t.repeat.x = repeatU;
  return t;
}
function drawFlag(x, px, py, w, h) {
  for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f4f2ec' : '#b22234'; x.fillRect(px, py + k * h / 13, w, h / 13 + 0.5); }
  x.fillStyle = '#3c3b6e'; x.fillRect(px, py, w * 0.4, h * 0.54);
}
/* white with the black roll-pattern panels and a little grime */
function stageTex(kind) {
  return canvasTex(2048, 1024, (x, W, H) => {
    x.fillStyle = '#ecebe6'; x.fillRect(0, 0, W, H);
    let seed = kind.length * 77;
    const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
    for (let k = 0; k < 300; k++) { x.fillStyle = `rgba(110,105,95,${0.02 + rnd() * 0.03})`; x.fillRect(rnd() * W, rnd() * H, 4 + rnd() * 60, 40 + rnd() * 300); }
    x.strokeStyle = 'rgba(90,88,82,0.35)'; x.lineWidth = 2;
    for (let r = 1; r < 12; r++) { x.beginPath(); x.moveTo(0, r * H / 12); x.lineTo(W, r * H / 12); x.stroke(); }    /* panel joints */
    x.fillStyle = '#141414';
    const v = f => H - f * H;
    if (kind === 'sic') {
      for (let q = 0; q < 4; q += 2) x.fillRect(q * W / 4, v(0.16), W / 4, 0.16 * H);
      for (let q = 0; q < 8; q++) if (q % 2) x.fillRect(q * W / 8, v(0.62), W / 8, 0.1 * H);
      x.fillRect(0, v(1), W, 0.025 * H);
      x.save(); x.font = 'bold 120px Arial'; x.fillStyle = '#151515'; x.textAlign = 'center';
      const word = 'UNITED STATES';
      for (let i = 0; i < word.length; i++) x.fillText(word[i], W * 0.375, v(0.5) + (i - 6) * 118);
      x.restore();
      drawFlag(x, W * 0.375 - 110, v(0.95), 220, 125);
    } else if (kind === 'skirt') {
      x.fillStyle = '#ecebe6'; x.fillRect(0, 0, W, H);
      x.strokeStyle = '#8d8b84'; x.lineWidth = 6; for (let q = 0; q < 64; q++) { x.beginPath(); x.moveTo(q * W / 64, 0); x.lineTo(q * W / 64, H); x.stroke(); }  /* stringers */
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
    } else if (kind === 'bpc') {
      x.fillStyle = '#e4e1d8'; x.fillRect(0, 0, W, H);
      x.strokeStyle = '#b7b3a8'; x.lineWidth = 4;
      for (let q = 0; q < 8; q++) { x.beginPath(); x.moveTo(q * W / 8, 0); x.lineTo(q * W / 8, H); x.stroke(); }
      x.fillStyle = 'rgba(120,112,98,0.25)'; for (let k = 0; k < 120; k++) x.fillRect(rnd() * W, rnd() * H, 6, 6);
    }
  });
}
/* the Service Module: aluminium with white-painted radiator panels, the flag and "UNITED STATES" */
function smTex() {
  return canvasTex(2048, 1024, (x, W, H) => {
    x.fillStyle = '#b9bcc1'; x.fillRect(0, 0, W, H);
    for (let q = 0; q < 6; q++) {
      x.fillStyle = q % 3 === 1 ? '#f1f1ee' : '#e6e7e5'; x.fillRect(q * W / 6 + 24, 70, W / 6 - 48, H - 150);
      x.strokeStyle = '#a5a8ad'; x.lineWidth = 2; for (let k = 1; k < 18; k++) { x.beginPath(); x.moveTo(q * W / 6 + 24 + k * (W / 6 - 48) / 18, 70); x.lineTo(q * W / 6 + 24 + k * (W / 6 - 48) / 18, H - 80); x.stroke(); }
    }
    x.fillStyle = '#8f9297'; x.fillRect(0, 0, W, 34); x.fillRect(0, H - 40, W, 40);
    x.fillStyle = '#ececea'; x.fillRect(W * 0.62, 180, 330, 620);
    x.save(); x.translate(W * 0.62 + 165, 210); x.font = 'bold 60px Arial'; x.fillStyle = '#1a1a1a'; x.textAlign = 'center';
    const word = 'UNITED STATES'; for (let i = 0; i < word.length; i++) if (word[i] !== ' ') x.fillText(word[i], 0, 120 + i * 40);
    x.restore();
    drawFlag(x, W * 0.62 + 95, 200, 140, 80);
  });
}
/* the Command Module's skin: strips of aluminised tape, slightly wrinkled, as a normal map */
function tapeNormals() {
  const W = 1024, H = 512, c = document.createElement('canvas'); c.width = W; c.height = H;
  const x = c.getContext('2d'), im = x.createImageData(W, H);
  let seed = 9; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const crease = new Float32Array(W * H);
  for (let k = 0; k < 900; k++) {                      /* wrinkles: short creases running mostly along the strips */
    const cx = rnd() * W, cy = rnd() * H, l = 10 + rnd() * 60, a = Math.PI / 2 + (rnd() - 0.5) * 0.9, s = (rnd() - 0.5) * 2;
    for (let t = -l; t < l; t++) { const px = Math.round(cx + Math.cos(a) * t), py = Math.round(cy + Math.sin(a) * t); if (px >= 0 && px < W && py >= 0 && py < H) crease[py * W + px] += s * (1 - Math.abs(t) / l); }
  }
  for (let j = 0; j < H; j++) for (let i = 0; i < W; i++) {
    const u = (i % 21) / 21;                           /* 48 strips round: each 21 px wide with a raised overlap */
    let nx = u < 0.12 ? -0.35 : u > 0.9 ? 0.25 : 0;
    nx += crease[j * W + i] * 0.22 + (rnd() - 0.5) * 0.04;
    const ny = (crease[j * W + Math.min(W - 1, i + 1)] - crease[j * W + Math.max(0, i - 1)]) * 0.1;
    const k = (j * W + i) * 4;
    im.data[k] = 128 + nx * 127; im.data[k + 1] = 128 + ny * 127; im.data[k + 2] = 255; im.data[k + 3] = 255;
  }
  x.putImageData(im, 0, 0);
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.anisotropy = 8;
  return t;
}
/* reflections for the metal: a soft sky above, the Earth below */
function environment(renderer) {
  const tex = canvasTex(512, 256, (x, W, H) => {
    const g = x.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0, '#0b1424'); g.addColorStop(0.42, '#6d89ad'); g.addColorStop(0.5, '#e8eef4'); g.addColorStop(0.56, '#9ab0c0'); g.addColorStop(1, '#2f4652');
    x.fillStyle = g; x.fillRect(0, 0, W, H);
    for (let k = 0; k < 140; k++) { x.fillStyle = 'rgba(255,255,255,0.35)'; x.beginPath(); x.ellipse(Math.random() * W, H * 0.6 + Math.random() * H * 0.4, 10 + Math.random() * 30, 3 + Math.random() * 6, 0, 0, 7); x.fill(); }
  });
  tex.mapping = THREE.EquirectangularReflectionMapping;
  const pm = new THREE.PMREMGenerator(renderer), env = pm.fromEquirectangular(tex).texture;
  pm.dispose(); tex.dispose();
  return env;
}

/* an engine's flame: an open cone, brightest at the nozzle, glowing at its edges, added to what is behind it */
function flameMaterial(core, edge) {
  return new THREE.ShaderMaterial({
    uniforms: { uK: { value: 0 }, uCore: { value: new THREE.Color(core) }, uEdge: { value: new THREE.Color(edge) }, uFall: { value: 1.4 } },
    vertexShader: `varying float vV; varying float vF; void main(){ vV = 1.0 - uv.y; vec4 mv = modelViewMatrix * vec4(position, 1.0);
      vec3 n = normalize(normalMatrix * normal); vF = abs(dot(n, normalize(-mv.xyz))); gl_Position = projectionMatrix * mv; }`,
    fragmentShader: `uniform float uK; uniform vec3 uCore; uniform vec3 uEdge; uniform float uFall; varying float vV; varying float vF;
      void main(){ float a = pow(1.0 - vV, uFall) * smoothstep(0.0, 0.04, vV); vec3 c = mix(uCore, uEdge, smoothstep(0.0, 0.6, vV) * (1.0 - vF * 0.6));
        gl_FragColor = vec4(c * a * uK * (0.35 + 0.65 * vF), 1.0); }`,
    transparent: true, depthWrite: false, blending: THREE.AdditiveBlending, side: THREE.DoubleSide
  });
}
function flame(rTop, rEnd, len, mat) {
  const g = new THREE.CylinderGeometry(rTop, rEnd, 1, 32, 12, true); g.translate(0, -0.5, 0);
  const m = new THREE.Mesh(g, mat); m.scale.set(1, len, 1); m.frustumCulled = false; m.renderOrder = 30;
  m.userData = { len, rTop, rEnd };
  return m;
}

export function makeVehicle(renderer) {
  const S = STACK, g = new THREE.Group();
  const L = o => new THREE.MeshLambertMaterial(o), Std = o => new THREE.MeshStandardMaterial(o);
  const env = environment(renderer);
  const black = L({ color: 0x1a1a1a }), white = L({ color: 0xe8e6e0 }), red = L({ color: 0x8a2a1c }), grey = L({ color: 0x77797c });
  const engineMetal = Std({ color: 0x3a3836, metalness: 0.7, roughness: 0.45, envMap: env, envMapIntensity: 0.6, side: THREE.DoubleSide });
  const part = () => { const p = new THREE.Group(); g.add(p); return p; };
  const cyl = (parent, r0, r1, y0, y1, mat, seg = 64, open = true) => {
    const m = new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, y1 - y0, seg, 1, open), mat);
    m.position.y = (Y(y0) + Y(y1)) / 2; parent.add(m); return m;
  };
  const bell = (parent, x, z, yTop, rThroat, rExit, len) => {
    const b = new THREE.Mesh(new THREE.CylinderGeometry(rThroat, rExit, len, 28, 4, true), engineMetal);
    b.position.set(x, Y(yTop) - len / 2, z); parent.add(b);
    for (let k = 1; k < 5; k++) { const r = rThroat + (rExit - rThroat) * k / 5, ring = new THREE.Mesh(new THREE.TorusGeometry(r, 0.035, 4, 28), black); ring.rotation.x = Math.PI / 2; ring.position.set(x, Y(yTop) - len * k / 5, z); parent.add(ring); }
    return b;
  };

  /* ---------------- first stage, S-IC: five F-1 engines under their fairings, four fins */
  const sic = part();
  cyl(sic, S.R1, S.R1, S.base, S.sicTop, L({ map: stageTex('sic') }));
  const outer = [];
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + Math.PI / 4, cx = Math.cos(a), cz = Math.sin(a);
    const fair = new THREE.Mesh(new THREE.CylinderGeometry(0.9, 1.35, 9, 20), black); fair.position.set(cx * 4.6, Y(S.base) + 4.5, cz * 4.6); sic.add(fair);
    const fin = new THREE.Mesh(new THREE.BoxGeometry(0.25, 6, 3.4), white); fin.position.set(cx * 6.3, Y(S.base) + 3, cz * 6.3); fin.rotation.y = -a; sic.add(fin);
    bell(sic, cx * 3.6, cz * 3.6, S.base, 0.75, 1.75, 5.8); outer.push([cx * 3.6, cz * 3.6]);
  }
  bell(sic, 0, 0, S.base, 0.75, 1.75, 5.8);
  const thrust = new THREE.Mesh(new THREE.CylinderGeometry(S.R1, S.R1, 0.6, 48), grey); thrust.position.y = Y(S.base) - 0.3; sic.add(thrust);
  /* its flames: one per engine near the nozzles, merging into one long plume, and the glow round them */
  const f1Core = flameMaterial(0xfff2c8, 0xffa040), f1Plume = flameMaterial(0xffd890, 0xff6a18), f1Bloom = flameMaterial(0xff9a50, 0x8a4a28);
  f1Bloom.uniforms.uFall.value = 0.8;
  const f1 = new THREE.Group(); f1.position.y = Y(S.base) - 5.8; sic.add(f1);
  const cores = [[0, 0], ...outer].map(([x, z]) => { const m = flame(1.7, 2.6, 14, f1Core); m.position.set(x, 0, z); f1.add(m); return m; });
  const plume = flame(5.5, 10, 70, f1Plume); f1.add(plume);
  const bloom = flame(7, 40, 200, f1Bloom); f1.add(bloom);
  /* retro-rockets at separation: eight small flares at the top of the stage */
  const retroMat = new THREE.MeshBasicMaterial({ color: 0xffd8a0, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false });
  const retros = new THREE.Group(); sic.add(retros);
  for (let k = 0; k < 8; k++) { const a = k * Math.PI / 4, m = new THREE.Mesh(new THREE.ConeGeometry(0.5, 7, 10, 1, true), retroMat); m.position.set(Math.cos(a) * 5.4, Y(S.sicTop) - 6, Math.sin(a) * 5.4); retros.add(m); }

  /* ---------------- the interstage skirt (falls away 30 s after the first staging) */
  const skirt = part();
  cyl(skirt, S.R1, S.R1, S.sicTop, S.skirtTop, L({ map: stageTex('skirt') }));
  for (let k = 0; k < 8; k++) { const a = k * Math.PI / 4, ul = new THREE.Mesh(new THREE.BoxGeometry(0.5, 1.6, 0.3), grey); ul.position.set(Math.cos(a) * 5.15, Y(S.sicTop) + 1.4, Math.sin(a) * 5.15); ul.rotation.y = -a; skirt.add(ul); }   /* ullage motors */

  /* ---------------- second stage, S-II: five J-2 engines */
  const sii = part();
  cyl(sii, S.R1, S.R1, S.skirtTop, S.siiTop, L({ map: stageTex('sii') }));
  cyl(sii, S.R1, S.R3, S.siiTop, S.sivbAft, L({ map: stageTex('sivb') }));          /* the S-II/S-IVB interstage stays with the S-II */
  const j2Mat = flameMaterial(0xc8d8ff, 0x7088ff); j2Mat.uniforms.uFall.value = 2.2;
  const j2s = new THREE.Group(); j2s.position.y = Y(S.skirtTop) - 3.4; sii.add(j2s);
  for (const [x, z] of [[0, 0], [2.6, 0], [-2.6, 0], [0, 2.6], [0, -2.6]]) {
    bell(sii, x, z, S.skirtTop, 0.4, 1.0, 3.4);
    const m = flame(0.95, 2.2, 22, j2Mat); m.position.set(x, 0, z); j2s.add(m);
  }

  /* ---------------- third stage, S-IVB, the Instrument Unit, the adapter round the Lunar Module */
  const sivb = part();
  cyl(sivb, S.R3, S.R3, S.sivbAft, S.sivbTop, L({ color: 0xecebe6 }));
  cyl(sivb, S.R3, S.R3, S.sivbTop, S.iuTop, L({ map: stageTex('iu') }));
  /* the spacecraft-LM adapter: a short fixed ring, and four panels hinged at its top edge that open in space */
  const slaMat = L({ map: stageTex('sla'), side: THREE.DoubleSide });
  const slaR = y => S.R3 + (S.RSM - S.R3) * (y - S.iuTop) / (S.slaTop - S.iuTop);
  const slaHinge = S.iuTop + 2.1;
  cyl(sivb, S.R3, slaR(slaHinge), S.iuTop, slaHinge, slaMat);
  const petals = [];
  for (let k = 0; k < 4; k++) {
    const tc = (k + 0.5) * Math.PI / 2, rb = slaR(slaHinge);
    const pv = new THREE.Group(); pv.position.set(rb * Math.sin(tc), Y(slaHinge), rb * Math.cos(tc)); sivb.add(pv);
    const m = new THREE.Mesh(new THREE.CylinderGeometry(S.RSM, rb, S.slaTop - slaHinge, 16, 1, true, tc - Math.PI / 4, Math.PI / 2), slaMat);
    m.position.set(-pv.position.x, (S.slaTop - slaHinge) / 2, -pv.position.z); pv.add(m);
    pv.userData = { axis: new THREE.Vector3(Math.cos(tc), 0, -Math.sin(tc)), out: new THREE.Vector3(Math.sin(tc), 0, Math.cos(tc)), rest: pv.position.clone() };
    petals.push(pv);
  }
  for (let k = 0; k < 2; k++) { const a = k * Math.PI, apsM = new THREE.Mesh(new THREE.BoxGeometry(1.4, 1.2, 2.6), white); apsM.position.set(Math.cos(a) * 3.55, Y(S.sivbAft) + 0.8, Math.sin(a) * 3.55); sivb.add(apsM); }   /* APS modules */
  bell(sivb, 0, 0, S.sivbAft, 0.4, 1.0, 3.4);
  const j3 = flame(0.95, 2.4, 26, j2Mat); j3.position.y = Y(S.sivbAft) - 3.4; sivb.add(j3);

  /* ---------------- the Service Module */
  const csm = part();
  cyl(csm, S.RSM, S.RSM, S.slaTop, S.smTop, Std({ map: smTex(), metalness: 0.35, roughness: 0.5, envMap: env, envMapIntensity: 0.6 }));
  for (let k = 0; k < 4; k++) {                                  /* RCS quads: a housing and four nozzles each */
    const a = k * Math.PI / 2 + 0.12, cx = Math.cos(a), cz = Math.sin(a);
    const q = new THREE.Group(); q.position.set(cx * 2.05, Y(S.smTop) - 1.0, cz * 2.05); q.rotation.y = -a; csm.add(q);
    q.add(new THREE.Mesh(new THREE.BoxGeometry(0.28, 0.62, 0.42), L({ color: 0xc9ccd0 })));
    for (const [dx, dy, dz, rx, rz] of [[0.0, 0.42, 0, 0, 0], [0.0, -0.42, 0, Math.PI, 0], [0.0, 0, 0.32, Math.PI / 2, 0], [0.0, 0, -0.32, -Math.PI / 2, 0]]) {
      const n = new THREE.Mesh(new THREE.ConeGeometry(0.05, 0.14, 10, 1, true), engineMetal); n.position.set(dx + 0.05, dy, dz); n.rotation.set(rx, 0, rz); q.add(n);
    }
  }
  /* the aft bulkhead round the engine (hidden in the adapter until transposition) */
  const smAft = new THREE.Mesh(new THREE.CircleGeometry(S.RSM, 48), Std({ color: 0x77797c, metalness: 0.5, roughness: 0.55, envMap: env, envMapIntensity: 0.4 }));
  smAft.rotation.x = Math.PI / 2; smAft.position.y = Y(S.slaTop) + 0.02; csm.add(smAft);
  const sps = bell(csm, 0, 0, S.slaTop, 0.45, 1.15, 2.8);              /* inside the adapter until transposition */
  const spsMat = flameMaterial(0xfff4e0, 0xff9a50); spsMat.uniforms.uFall.value = 1.8;
  const spsFlame = flame(1.1, 2.6, 18, spsMat); spsFlame.position.y = Y(S.slaTop) - 2.8; csm.add(spsFlame); spsFlame.visible = false;

  /* ---------------- the Command Module: a 33 degree cone in aluminised tape; windows, the hatch, the docking probe */
  const cm = new THREE.Group(); csm.add(cm);
  const mylar = Std({ color: 0xdfe3e8, metalness: 1.0, roughness: 0.22, envMap: env, envMapIntensity: 1.1, normalMap: tapeNormals(), normalScale: new THREE.Vector2(0.6, 0.6) });
  const cmY0 = Y(S.smTop) + 0.05, cmY1 = cmY0 + 2.5, cmR0 = 1.956, cmR1 = 0.42;
  const rExt = y => cmR0 + (cmR1 - cmR0) * (y - cmY0) / (cmY1 - cmY0);
  const hatchHole = (a, y) => Math.abs(((a - 180 + 540) % 360) - 180) < 22 && y > Y(EYE_H + 0.37) && y < Y(EYE_H + 1.27);
  const hatchFill = [];
  {
    const NA = 128, NY = 20, pos = [], uv = [], idx = [];
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NA; i++) {
      const y = cmY0 + (cmY1 - cmY0) * j / NY, a = i / NA * Math.PI * 2, r = rExt(y);
      pos.push(Math.sin(a) * r, y, Math.cos(a) * r); uv.push(i / NA * 4, j / NY);
    }
    for (let j = 0; j < NY; j++) for (let i = 0; i < NA; i++) {
      const p = j * (NA + 1) + i, q = p + NA + 1;
      (hatchHole((i + 0.5) / NA * 360, cmY0 + (cmY1 - cmY0) * (j + 0.5) / NY) ? hatchFill : idx).push(p, p + 1, q, q, p + 1, q + 1);      /* wound outward */
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    geo.computeVertexNormals();
    const shell = geo.clone(); shell.setIndex(idx); cm.add(new THREE.Mesh(shell, mylar));
    const hg = geo.clone(); hg.setIndex(hatchFill); var cmHatch = new THREE.Mesh(hg, mylar); cm.add(cmHatch);
  }
  /* the forward tunnel, the docking probe, the base heat shield's rim */
  const tunnel = new THREE.Mesh(new THREE.CylinderGeometry(0.42, 0.42, 0.35, 32), mylar); tunnel.position.y = cmY1 + 0.17; cm.add(tunnel);
  const probe = new THREE.Group(); probe.position.y = cmY1 + 0.35; cm.add(probe);
  const probeRod = new THREE.Mesh(new THREE.CylinderGeometry(0.03, 0.05, 0.75, 10), grey);
  { const c1 = new THREE.Mesh(new THREE.ConeGeometry(0.34, 0.3, 24), grey); c1.position.y = 0.15; probe.add(c1);
    probeRod.position.y = 0.55; probe.add(probeRod); }
  const RING_H = cmY1 + 0.35;                 /* the CM's docking ring, the top of its tunnel (CSM frame) */
  const rim = new THREE.Mesh(new THREE.TorusGeometry(cmR0 + 0.005, 0.025, 6, 64), mylar); rim.rotation.x = Math.PI / 2; rim.position.y = cmY0; cm.add(rim);
  /* windows, where the cabin's are: dark glass that mirrors the sky */
  const glass = Std({ color: 0x050608, metalness: 0.6, roughness: 0.04, envMap: env, envMapIntensity: 1.2 });
  const frameM = L({ color: 0x55585c });
  for (const [phi, zc, w, h, round] of [[90, -0.85, 0.27, 0.27, true], [130, -1.15, 0.2, 0.23], [50, -1.15, 0.2, 0.23], [180, -0.25, 0.33, 0.33], [0, -0.25, 0.33, 0.33]]) {
    const y = -zc, a = phi * Math.PI / 180, r = rExt(y);
    const radial = new THREE.Vector3(-Math.cos(a), 0, -Math.sin(a));
    const tilt = Math.atan((cmR0 - cmR1) / (cmY1 - cmY0));
    const n = radial.clone().multiplyScalar(Math.cos(tilt)).add(new THREE.Vector3(0, Math.sin(tilt), 0));
    const p = radial.clone().multiplyScalar(r).add(new THREE.Vector3(0, y, 0)).addScaledVector(n, 0.004);
    const win = new THREE.Mesh(round ? new THREE.CircleGeometry(w / 2, 32) : new THREE.PlaneGeometry(w, h), glass);
    win.position.copy(p); win.lookAt(p.clone().add(n)); cm.add(win);
    const fr = new THREE.Mesh(round ? new THREE.RingGeometry(w / 2, w / 2 + 0.035, 32) : new THREE.RingGeometry(Math.max(w, h) * 0.62, Math.max(w, h) * 0.72, 4, 1, Math.PI / 4), frameM);
    fr.position.copy(p).addScaledVector(n, 0.001); fr.lookAt(p.clone().add(n)); cm.add(fr);
  }
  /* CM thrusters: pairs of small dark nozzles near the top */
  for (let k = 0; k < 6; k++) { const a = k * Math.PI / 3 + 0.3, y = cmY1 - 0.55, r = rExt(y); const d = new THREE.Mesh(new THREE.CircleGeometry(0.05, 12), black); d.position.set(Math.sin(a) * (r + 0.005), y, Math.cos(a) * (r + 0.005)); d.lookAt(d.position.clone().multiplyScalar(2).setY(y + 0.6)); cm.add(d); }

  /* ---------------- the Lunar Module, legs folded, in the adapter until it is pulled out */
  const lm = makeLM(env);
  lm.matrixAutoUpdate = false; g.add(lm);
  const LM_REST = new THREE.Matrix4().makeTranslation(0, Y(S.iuTop) + 0.7, 0);
  lm.matrix.copy(LM_REST);

  /* ---------------- the boost protective cover, with its hatch; the launch escape tower */
  const les = part();
  const bpcMat = L({ map: stageTex('bpc'), side: THREE.DoubleSide });
  const bY0 = Y(S.smTop), bY1 = Y(S.cmTop) + 0.1, bR0 = 2.04, bR1 = 0.5;
  {
    const NA = 96, NY = 24, pos = [], uv = [], idx = [];
    for (let j = 0; j <= NY; j++) for (let i = 0; i <= NA; i++) {
      const y = bY0 + (bY1 - bY0) * j / NY, r = bR0 + (bR1 - bR0) * j / NY, a = i / NA * Math.PI * 2;
      pos.push(Math.sin(a) * r, y, Math.cos(a) * r); uv.push(i / NA, j / NY);
    }
    for (let j = 0; j < NY; j++) for (let i = 0; i < NA; i++) {
      if (hatchHole((i + 0.5) / NA * 360, bY0 + (bY1 - bY0) * (j + 0.5) / NY)) continue;
      const p = j * (NA + 1) + i, q = p + NA + 1;
      idx.push(p, p + 1, q, q, p + 1, q + 1);                      /* wound outward */
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2)); geo.setIndex(idx);
    geo.computeVertexNormals();
    les.add(new THREE.Mesh(geo, bpcMat));
  }
  const bpcHinge = new THREE.Group(); bpcHinge.position.set(0.42, Y(EYE_H + 0.82), -1.32); les.add(bpcHinge);
  const bpcHatch = new THREE.Mesh(new THREE.BoxGeometry(0.78, 0.9, 0.04), bpcMat); bpcHatch.position.set(-0.39, 0, 0); bpcHinge.add(bpcHatch);
  const porthole = new THREE.Mesh(new THREE.CircleGeometry(0.1, 24), black); porthole.position.set(0, 0.03, -0.025); porthole.rotation.y = Math.PI; bpcHatch.add(porthole);
  const lesY0 = Y(S.cmTop) + 0.1, lesY1 = Y(S.lesTop) - 4;
  for (let k = 0; k < 4; k++) {
    const a = k * Math.PI / 2 + Math.PI / 4, b = new THREE.Vector3(Math.cos(a) * 0.95, lesY0, Math.sin(a) * 0.95), t = new THREE.Vector3(Math.cos(a) * 0.45, lesY1, Math.sin(a) * 0.45);
    const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.05, 0.05, b.distanceTo(t), 6), red);
    leg.position.copy(b).add(t).multiplyScalar(0.5); leg.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), t.clone().sub(b).normalize()); les.add(leg);
  }
  for (let y = lesY0 + 1.2; y < lesY1; y += 1.4) { const r = 0.95 - 0.5 * (y - lesY0) / (lesY1 - lesY0), ring = new THREE.Mesh(new THREE.TorusGeometry(r, 0.035, 4, 4), red); ring.position.y = y; ring.rotation.x = Math.PI / 2; ring.rotation.z = Math.PI / 4; les.add(ring); }
  const motor = new THREE.Mesh(new THREE.CylinderGeometry(0.33, 0.33, 3.4, 20), white); motor.position.y = lesY1 + 1.7; les.add(motor);
  const band = new THREE.Mesh(new THREE.CylinderGeometry(0.335, 0.335, 0.5, 20), black); band.position.y = lesY1 + 0.6; les.add(band);
  const nose = new THREE.Mesh(new THREE.ConeGeometry(0.33, 1.2, 20), black); nose.position.y = lesY1 + 4; les.add(nose);
  const jettMat = flameMaterial(0xfff0d0, 0xff8a30);
  const jett = new THREE.Group(); jett.position.y = lesY1 + 0.4; les.add(jett);
  for (let k = 0; k < 4; k++) { const a = k * Math.PI / 2, m = flame(0.12, 1.4, 9, jettMat); m.position.set(Math.cos(a) * 0.3, 0, Math.sin(a) * 0.3); m.rotation.set(Math.sin(a) * 0.5, 0, -Math.cos(a) * 0.5); jett.add(m); }

  /* ---------------- light for the vehicle and the pad (world-aligned; the group itself is not turned) */
  const lights = new THREE.Group();
  const sun = new THREE.DirectionalLight(0xfff3e2, 2.6); sun.position.fromArray(SUN).multiplyScalar(100); lights.add(sun, sun.target);
  const hemi = new THREE.HemisphereLight(0xa9c4e8, 0x6b6250, 0.8); lights.add(hemi);

  /* ---------------- per frame: engines, staging, the tower; what is shown depends on where you are */
  const parts = { sic, skirt, sii, sivb, csm, les, cm };
  const rest = new Map(Object.values(parts).map(p => [p, p.position.clone()]));
  function away(p, d, side, tumble, dt) { p.position.copy(rest.get(p)); p.position.y -= d; p.position.x += side * dt; p.rotation.set(tumble * dt, 0, tumble * 0.6 * dt); }
  function update(t, view, st) {
    const E = EVENTS;
    /* the F-1s: lit at ignition, the centre engine out at T+2:19, all out at staging; the plume grows with height */
    const kf = t < E.ignition ? 0 : t < E.sicCutoff ? smooth(E.ignition, -1.5, t) : 0;
    const alt = st ? st.alt : 0, hk = Math.min(1, alt / 60000);
    f1Core.uniforms.uK.value = 1.6 * kf; f1Plume.uniforms.uK.value = 1.0 * kf * (1 - 0.45 * hk); f1Bloom.uniforms.uK.value = 0.35 * kf * hk;
    cores[0].visible = t < E.iCutoff;
    plume.scale.set(1 + 2.5 * hk, 70 + 250 * hk, 1 + 2.5 * hk);
    bloom.scale.set(1 + 9 * hk, 200 + 1800 * hk, 1 + 9 * hk);
    bloom.visible = hk > 0.05 && kf > 0;
    const k2 = t > E.siiIgnition && t < E.siiCutoff ? smooth(E.siiIgnition, E.siiIgnition + 2, t) : 0;
    j2Mat.uniforms.uK.value = 0.55 * Math.max(k2, t > E.sivbIgnition && t < E.seco ? smooth(E.sivbIgnition, E.sivbIgnition + 2, t) : 0);
    j2s.visible = k2 > 0; j3.visible = t > E.sivbIgnition && t < E.seco;
    /* staging: what has been let go falls back (relative to the vehicle) and tumbles */
    const d1 = t - (E.sicCutoff + 0.6), d2 = t - E.skirtSep, d3 = t - (E.siiCutoff + 0.8), dj = t - E.towerJett;
    sic.visible = d1 < 45; if (d1 > 0) away(sic, 0.5 * 22 * d1 * d1, 0.8, 0.02, d1); else away(sic, 0, 0, 0, 0);
    retros.visible = d1 > 0 && d1 < 0.8; retroMat.opacity = 1 - d1 / 0.8;
    skirt.visible = d2 < 35; if (d2 > 0) away(skirt, 0.5 * 14 * d2 * d2, -0.4, 0.05, d2); else away(skirt, 0, 0, 0, 0);
    sii.visible = d3 < 45; if (d3 > 0) away(sii, 0.5 * 9 * d3 * d3, 0.5, 0.015, d3); else away(sii, 0, 0, 0, 0);
    /* the tower pulls the cover off and flies ahead and aside */
    les.position.copy(rest.get(les)); les.rotation.set(0, 0, 0);
    if (dj > 0) { les.position.y += 3 * dj + 30 * dj * dj; les.position.x += 4 * dj; les.rotation.z = -0.08 * dj; }
    jett.visible = dj > 0 && dj < 3; jettMat.uniforms.uK.value = 1.4 * Math.max(0, 1 - dj / 3);
    les.visible = dj < 14 && (view !== 'inside' || dj > 0.5);
    bpcHinge.rotation.y = view === 'boarding' ? 1.9 : 0;
    /* inside the cabin the spacecraft's own skin is not drawn (you are in it) */
    cm.visible = view !== 'inside';
    cmHatch.visible = view !== 'boarding';
    /* the sky's light falls off with height; the Earth below lights the vehicle from underneath */
    hemi.color.setRGB(0.66 * (1 - hk) + 0.02, 0.77 * (1 - hk) + 0.03, 0.91 * (1 - hk) + 0.06);
    hemi.groundColor.setRGB(0.42 + 0.1 * hk, 0.38 + 0.2 * hk, 0.31 + 0.35 * hk);
    if (st) hemi.position.fromArray(st.up);
  }
  /* Chapter 2 and later: the S-IVB, the adapter panels and the Lunar Module placed relative to the CSM (this group's
   * frame stays the CSM's). td: { sivb: Matrix4 (S-IVB in the CSM frame), lm: Matrix4 (LM in the CSM frame), open: 0..1
   * (the panels opening), gone: seconds since the panels were let go, sps: 0..1 (the main engine) }. */
  function space(td) {
    for (const p of [sic, skirt, sii, les]) p.visible = false;
    sivb.visible = !!td.sivb;
    if (td.sivb) { sivb.matrixAutoUpdate = false; sivb.matrix.copy(td.sivb); sivb.matrixWorldNeedsUpdate = true; }
    lm.matrix.copy(td.lm || LM_REST); lm.matrixWorldNeedsUpdate = true;
    petals.forEach((pv, k) => {
      const u = pv.userData;
      pv.quaternion.setFromAxisAngle(u.axis, Math.min(1, td.open || 0) * 0.8 + (td.gone > 0 ? td.gone * (0.05 + 0.02 * k) : 0));
      pv.position.copy(u.rest).addScaledVector(u.out, td.gone > 0 ? td.gone * 1.2 : 0);
      pv.visible = !(td.gone > 90);
    });
    spsFlame.visible = (td.sps || 0) > 0; spsMat.uniforms.uK.value = 1.2 * (td.sps || 0);
    j2Mat.uniforms.uK.value = 0.55 * (td.j2 || 0); j3.visible = (td.j2 || 0) > 0;
    f1Core.uniforms.uK.value = 0; f1Plume.uniforms.uK.value = 0; f1Bloom.uniforms.uK.value = 0;
  }
  /* the CSM relative to the S-IVB during transposition: moved `dist` metres along its axis, pitched by `ang` about
   * its centre; the S-IVB's matrix in the CSM's frame is the inverse */
  const CSM_C = new THREE.Vector3(0, -3.0, 0), _m1 = new THREE.Matrix4(), _m2 = new THREE.Matrix4();
  function csmInSivb(dist, ang, out = new THREE.Matrix4()) {
    out.makeTranslation(0, dist, 0).multiply(_m1.makeTranslation(CSM_C.x, CSM_C.y, CSM_C.z)).multiply(_m2.makeRotationX(ang)).multiply(_m1.makeTranslation(-CSM_C.x, -CSM_C.y, -CSM_C.z));
    return out;
  }
  /* docked: the probe tip (2.93 m up the CSM axis) on the drogue at the top of the LM (rest top) */
  const PROBE = 2.93, LM_TOP = Y(S.iuTop) + 0.7 + lm.userData.top;
  const DOCK_DIST = LM_TOP - (2 * CSM_C.y - PROBE);
  /* hard docked: after capture the probe retracts and pulls the two spacecraft together, ring on ring, before the
   * twelve latches fire (owner's headset test: they stayed apart, the probe only touching) */
  const HARD_DIST = LM_TOP - (2 * CSM_C.y - RING_H);
  /* the probe's rod: 1 out (capture), 0 drawn in (hard docked); it slides down into the tunnel */
  function setProbe(k) { probeRod.position.y = 0.55 - (1 - k) * (DOCK_DIST - HARD_DIST); probeRod.visible = k > 0.02; }
  return { group: g, lights, update, space, csmInSivb, DOCK_DIST, HARD_DIST, setProbe, LM_REST, parts, env, lm };
}

/* The Lunar Module is lm-model.js (stage 5's detailed model; the same frame and interface as the stand-in it
 * replaced). The LM alone, legs deployed, on the ground (the Moon page: the LM you landed in). Origin at the bottom
 * of the descent engine; the footpads' soles PAD_DROP below it (descent-space.js). */
export function makeLMModel(renderer) {
  const g = makeLM(environment(renderer));
  g.userData.setLegs(true); g.userData.setLanded(true);
  return g;
}
