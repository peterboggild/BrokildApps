/* The suit for the Moon's surface (Apollo A7LB with the PLSS), built in code, as a jointed figure.
 *
 * What makes it read as the real one: the backpack (the Portable Life Support System, the Oxygen Purge System on top
 * of it with its aerial), the Remote Control Unit on the chest, the hoses round from the pack to the chest, the
 * helmet's outer shell (the LEVA) with the gold visor that mirrors the valley, the lunar overshoes, the grey woven
 * gloves with blue fingertips, the bellows at shoulder, elbow and knee, the Commander's red stripes (arms, legs, the
 * helmet's crown), dust on the legs. The flag on the left shoulder and the NASA insignia on the right breast, as worn
 * (the owner's decision, for history's sake: the rest of the app carries no NASA insignia).
 *
 * Frame: the feet on y = 0, facing -z, +x to the wearer's right. About 1.85 m to the helmet's crown.
 * makeEVASuit({ commander, player, env }):
 *   commander: the red stripes. player: your own suit as you see it looking down (no helmet, no arms: your hands
 *   are the controllers). env: an environment map for the visor and the metal (suitEnvironment(renderer, sunDir)).
 *   visor: a material for the visor in place of the gold one (lmp.js gives a live mirror).
 * userData.helmet: the helmet (a Group on the waist: it can be lifted off, as before the suiting-up in the cabin).
 * userData.aerial: the OPS's aerial (a Group; hidden in the cabin, where it was folded down).
 * userData.joints: { waist, armL, armR, elbowL, elbowR, legL, legR, kneeL, kneeR } (Groups to turn), .pose(name).
 * One mesh per material in each jointed part: about thirty draw calls a figure. */
import * as THREE from 'three';
import { Parts, decal } from './lm-model.js?v=30';

/* what the visor mirrors: a black sky, the sunlit ground below the horizon, the Sun low on one side, the Earth */
export function suitEnvironment(renderer, sunAz = 1.6, sunEl = 0.23) {
  const c = document.createElement('canvas'); c.width = 512; c.height = 256; const x = c.getContext('2d');
  x.fillStyle = '#000'; x.fillRect(0, 0, 512, 256);
  const g = x.createLinearGradient(0, 128, 0, 256); g.addColorStop(0, '#8d8a84'); g.addColorStop(0.15, '#77746e'); g.addColorStop(1, '#4a4844');
  x.fillStyle = g; x.fillRect(0, 128, 512, 128);
  /* mountains on the horizon */
  x.fillStyle = '#a09d96'; x.beginPath(); x.moveTo(0, 130);
  for (let i = 0; i <= 64; i++) x.lineTo(i * 8, 128 - 5 - 7 * Math.abs(Math.sin(i * 0.45) + 0.6 * Math.sin(i * 1.3 + 1)));
  x.lineTo(512, 130); x.closePath(); x.fill();
  const sx = (sunAz / (2 * Math.PI) + 0.5) % 1 * 512, sy = 128 - sunEl / (Math.PI / 2) * 128;
  const sg = x.createRadialGradient(sx, sy, 0, sx, sy, 26); sg.addColorStop(0, '#ffffff'); sg.addColorStop(0.2, '#fff6e0'); sg.addColorStop(1, 'rgba(255,240,210,0)');
  x.fillStyle = sg; x.fillRect(sx - 30, sy - 30, 60, 60);
  x.fillStyle = '#6f93c8'; x.beginPath(); x.arc(300, 36, 4, 0, 7); x.fill();
  const tex = new THREE.CanvasTexture(c); tex.mapping = THREE.EquirectangularReflectionMapping; tex.colorSpace = THREE.SRGBColorSpace;
  const pm = new THREE.PMREMGenerator(renderer), env = pm.fromEquirectangular(tex).texture; pm.dispose(); tex.dispose();
  return env;
}

let _patch = null, _flag = null, _cloth = null;
function textures() {
  if (_patch) return;
  /* the NASA insignia: the blue disc, stars, the orbit, the red vector, the letters */
  _patch = decal(256, 256, (x) => {
    x.clearRect(0, 0, 256, 256);
    x.fillStyle = '#0b3d91'; x.beginPath(); x.arc(128, 128, 118, 0, 7); x.fill();
    x.fillStyle = '#fff'; for (const [sx, sy, r] of [[60, 70, 3], [92, 44, 2], [190, 60, 3], [206, 150, 2], [70, 190, 2], [150, 204, 3], [176, 96, 2], [44, 132, 2]]) { x.beginPath(); x.arc(sx, sy, r, 0, 7); x.fill(); }
    x.strokeStyle = '#fff'; x.lineWidth = 5; x.beginPath(); x.ellipse(128, 132, 104, 34, -0.42, 0.25, Math.PI * 1.92); x.stroke();
    x.fillStyle = '#fc3d21'; x.beginPath(); x.moveTo(22, 196); x.lineTo(150, 96); x.lineTo(236, 40); x.lineTo(166, 116); x.lineTo(60, 206); x.closePath(); x.fill();
    x.fillStyle = '#fff'; x.font = 'bold 66px Arial, Helvetica, sans-serif'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText('NASA', 128, 132);
  });
  _flag = decal(256, 160, (x, w, h) => {
    for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f4f2ec' : '#b22234'; x.fillRect(0, k * h / 13, w, h / 13 + 0.5); }
    x.fillStyle = '#3c3b6e'; x.fillRect(0, 0, w * 0.4, h * 7 / 13);
    x.fillStyle = '#f4f2ec'; for (let r = 0; r < 9; r++) for (let c = 0; c < (r % 2 ? 5 : 6); c++) { x.beginPath(); x.arc((c + (r % 2 ? 1 : 0.5)) * w * 0.4 / 6, (r + 0.6) * h * 7 / 13 / 9.4, 2.2, 0, 7); x.fill(); }
  });
  /* the white cloth: a fine weave, seams, and grey dust low down (v runs up the part) */
  _cloth = decal(256, 256, (x) => {
    x.fillStyle = '#f1efe9'; x.fillRect(0, 0, 256, 256);
    for (let i = 0; i < 2600; i++) { x.fillStyle = `rgba(${90 + Math.random() * 60 | 0},${88 + Math.random() * 50 | 0},${80 + Math.random() * 40 | 0},${0.03 + Math.random() * 0.05})`; x.fillRect(Math.random() * 256, Math.random() * 256, 1 + Math.random() * 3, 1); }
    x.strokeStyle = 'rgba(120,116,108,0.35)'; x.lineWidth = 1; for (const u of [64, 192]) { x.beginPath(); x.moveTo(u, 0); x.lineTo(u, 256); x.stroke(); }
  });
  _cloth.wrapS = _cloth.wrapT = THREE.RepeatWrapping;
}

const V = (x, y, z) => new THREE.Vector3(x, y, z);

export function makeEVASuit(opts = {}) {
  textures();
  const env = opts.env || null;
  const Std = o => new THREE.MeshStandardMaterial({ envMap: env, envMapIntensity: env ? 0.25 : 0, ...o });
  const white = Std({ color: 0xffffff, map: _cloth, roughness: 0.92, metalness: 0 });
  const dusty = Std({ color: 0xcfcbc1, map: _cloth, roughness: 0.95, metalness: 0 });               /* the legs below the knee: grey with dust */
  const bellows = Std({ color: 0xcfccc4, roughness: 0.9, metalness: 0 });
  const red = Std({ color: 0xb3202a, roughness: 0.8, metalness: 0 });
  const grey = Std({ color: 0x8e9195, roughness: 0.55, metalness: 0.35, envMapIntensity: env ? 0.6 : 0 });
  const dark = Std({ color: 0x2a2b2d, roughness: 0.7, metalness: 0.2 });
  const glove = Std({ color: 0x8a8d90, roughness: 0.75, metalness: 0.25 });                        /* woven steel cloth */
  const blue = Std({ color: 0x3f5f9a, roughness: 0.8, metalness: 0 });                              /* silicone: fingertips, soles, hose ends */
  const hose = Std({ color: 0xd9d6cf, roughness: 0.8, metalness: 0 });
  const gold = opts.visor || Std({ color: 0xd8a63a, roughness: 0.06, metalness: 1.0, envMapIntensity: env ? 1.6 : 0 });
  const patchM = Std({ map: _patch, transparent: true, roughness: 0.8, metalness: 0 });
  const flagM = Std({ map: _flag, roughness: 0.8, metalness: 0 });
  const cap = (r, len) => new THREE.CapsuleGeometry(r, len, 6, 14);
  const ring = (r, t = 0.012) => new THREE.TorusGeometry(r, t, 6, 20);

  const root = new THREE.Group();
  const part = (parent, x, y, z) => { const g = new THREE.Group(); g.position.set(x, y, z); parent.add(g); return g; };
  /* the figure's tree: the pelvis; the torso on the waist; arms; legs */
  const pelvis = part(root, 0, 0.94, 0), waist = part(pelvis, 0, 0.06, 0);
  const J = { waist };
  const P = {};                                   /* geometry per jointed part */
  const of = g => (P[g.uuid] = P[g.uuid] || { g, parts: new Parts() }).parts;

  /* ---------------- pelvis */
  of(pelvis).add(cap(0.215, 0.1), white, 0, 0.02, 0.01, 0, 0, Math.PI / 2, 1, 1, 0.95);
  of(pelvis).add(new THREE.CylinderGeometry(0.225, 0.225, 0.04, 20), bellows, 0, 0.09, 0.01, 0, 0, 0, 1.1, 1, 0.92);     /* the waist's band */

  /* ---------------- legs: thigh, the knee's bellows, shin, overshoe */
  for (const s of [-1, 1]) {
    const leg = part(pelvis, s * 0.135, -0.03, 0), knee = part(leg, 0, -0.42, 0);
    J[s < 0 ? 'legL' : 'legR'] = leg; J[s < 0 ? 'kneeL' : 'kneeR'] = knee;
    of(leg).add(cap(0.122, 0.24), white, 0, -0.2, 0.005);
    if (opts.commander) of(leg).add(new THREE.CylinderGeometry(0.128, 0.126, 0.05, 18), red, 0, -0.17, 0.005);
    of(leg).add(new THREE.BoxGeometry(0.11, 0.13, 0.03), white, s * 0.08, -0.2, -0.1);                               /* the thigh pocket */
    for (let k = 0; k < 3; k++) of(knee).add(ring(0.114, 0.016), bellows, 0, 0.03 - k * 0.03, 0, Math.PI / 2);
    of(knee).add(cap(0.112, 0.24), dusty, 0, -0.22, 0.01);
    /* the lunar overshoe: a broad ribbed sole in blue silicone, the rounded upper in grey woven cloth */
    of(knee).add(cap(0.098, 0.17), glove, 0, -0.42, -0.05, Math.PI / 2, 0, 0, 1.0, 1, 0.8);
    of(knee).add(cap(0.09, 0.2), blue, 0, -0.482, -0.05, Math.PI / 2, 0, 0, 1.1, 1, 0.34);
    for (let k = 0; k < 6; k++) of(knee).add(new THREE.BoxGeometry(0.18, 0.012, 0.02), dark, 0, -0.512, -0.2 + k * 0.06);
  }

  /* ---------------- torso, with all it carries */
  const T = of(waist);
  T.add(cap(0.232, 0.24), white, 0, 0.27, 0.01, 0, 0, 0, 1.16, 1, 0.96);
  T.add(new THREE.SphereGeometry(0.225, 18, 12), white, 0, 0.45, 0.02, 0, 0, 0, 1.32, 0.6, 0.97);                    /* the shoulders' yoke */
  for (const s of [-1, 1]) T.add(new THREE.SphereGeometry(0.135, 14, 10), white, s * 0.3, 0.46, 0.015);
  /* the PLSS and the OPS on top of it, their cover's flap lines, the straps over the shoulders and round the waist */
  T.add(new THREE.BoxGeometry(0.52, 0.64, 0.25), white, 0, 0.36, 0.33);
  T.add(new THREE.BoxGeometry(0.48, 0.2, 0.21), white, 0, 0.78, 0.33);
  T.add(new THREE.BoxGeometry(0.525, 0.012, 0.255), bellows, 0, 0.68, 0.33);
  for (const y of [0.2, 0.5]) T.add(new THREE.BoxGeometry(0.526, 0.01, 0.256), bellows, 0, y, 0.33);
  T.add(new THREE.BoxGeometry(0.16, 0.1, 0.012), grey, 0.12, 0.2, 0.46);                                           /* the pack's valve panel */
  for (const s of [-1, 1]) {
    T.tube(white, [V(s * 0.17, 0.62, 0.22), V(s * 0.18, 0.592, 0.03), V(s * 0.17, 0.51, -0.17), V(s * 0.15, 0.36, -0.235)], 0.013, 14);
    T.tube(white, [V(s * 0.26, 0.1, 0.22), V(s * 0.285, 0.1, 0.0), V(s * 0.17, 0.1, -0.22)], 0.013, 10);
  }
  /* the aerial on the OPS (the blue blade of the photographs) */
  const aerial = part(waist, 0.17, 0.88, 0.33); of(aerial).add(new THREE.BoxGeometry(0.012, 0.3, 0.03), blue, 0, 0.15, 0);
  T.add(new THREE.BoxGeometry(0.05, 0.04, 0.05), grey, 0.17, 0.885, 0.33);
  /* the Remote Control Unit on the chest, with its dials and switches; the camera bracket over it */
  T.add(new THREE.BoxGeometry(0.23, 0.13, 0.09), white, 0, 0.31, -0.265);
  T.add(new THREE.BoxGeometry(0.21, 0.012, 0.075), grey, 0, 0.381, -0.265);
  for (const [x, c] of [[-0.065, dark], [-0.02, dark], [0.03, dark], [0.07, red]]) T.add(new THREE.CylinderGeometry(0.011, 0.011, 0.02, 10), c, x, 0.395, -0.265);
  T.add(new THREE.BoxGeometry(0.06, 0.03, 0.006), dark, -0.05, 0.31, -0.3115);
  /* the hoses: oxygen in and out and water, from the pack round each side to the connectors on the chest */
  for (const [s, y0, y1, c] of [[-1, 0.2, 0.2, blue], [1, 0.2, 0.2, red], [-1, 0.12, 0.13, blue], [1, 0.12, 0.13, red]]) {
    T.tube(hose, [V(s * 0.2, y0, 0.22), V(s * 0.32, y0 + 0.02, 0.02), V(s * 0.26, y1, -0.16), V(s * 0.11, y1, -0.225)], 0.016, 16);
    T.add(new THREE.CylinderGeometry(0.02, 0.02, 0.03, 12), c, s * 0.105, y1, -0.232, Math.PI / 2);
  }
  /* the insignia on the right breast, the flag on the left shoulder */
  T.add(new THREE.CircleGeometry(0.048, 28), patchM, 0.135, 0.45, -0.222, 0.2, Math.PI - 0.3, 0);
  T.add(new THREE.PlaneGeometry(0.085, 0.053), flagM, -0.438, 0.45, 0.015, 0, -Math.PI / 2, 0);
  let helmet = null;
  /* the neck ring and the helmet: the LEVA's white shell, open at the face, the gold visor in it */
  if (!opts.player) {
    /* (big: the LEVA is nearly as wide as the chest, as in the photographs; hy its centre, R its radius) */
    const hy = 0.785, R = 0.212;
    T.add(new THREE.CylinderGeometry(0.16, 0.175, 0.07, 24), white, 0, 0.6, 0.0);
    helmet = part(waist, 0, 0, 0); const HP = of(helmet);
    /* (the shell: the back and sides, the crown down to the brow, the chin; the opening faces -z) */
    HP.add(new THREE.SphereGeometry(R, 30, 18, Math.PI * 11 / 6, Math.PI * 4 / 3, 0, Math.PI * 0.95), white, 0, hy, 0.0);
    HP.add(new THREE.SphereGeometry(R, 30, 8, 0, Math.PI * 2, 0, Math.PI * 0.3), white, 0, hy, 0.0);
    HP.add(new THREE.SphereGeometry(R, 30, 6, 0, Math.PI * 2, Math.PI * 0.72, Math.PI * 0.25), white, 0, hy, 0.0);
    HP.add(new THREE.SphereGeometry(R - 0.009, 32, 22), gold, 0, hy, 0.0);                                           /* the gold visor (the whole bubble, seen in the opening) */
    HP.add(new THREE.TorusGeometry(R - 0.004, 0.015, 6, 24, Math.PI * 0.62), white, 0, hy, -0.014, Math.PI / 2 - 0.9, 0, Math.PI * 0.19);   /* the brow's rim */
    if (opts.commander) { const st = new THREE.TorusGeometry(R + 0.003, 0.011, 5, 24, 2.25); st.rotateZ(0.62); st.rotateY(Math.PI / 2); st.scale(3.4, 1, 1); HP.add(st, red, 0, hy, 0); }   /* the broad stripe over the crown, fore and aft */
  }
  /* ---------------- arms: the shoulder's bellows, upper arm, the elbow's bellows, forearm, wrist ring, glove */
  if (!opts.player) for (const s of [-1, 1]) {
    const arm = part(waist, s * 0.3, 0.46, 0.015), elbow = part(arm, 0, -0.29, 0);
    J[s < 0 ? 'armL' : 'armR'] = arm; J[s < 0 ? 'elbowL' : 'elbowR'] = elbow;
    for (let k = 0; k < 2; k++) of(arm).add(ring(0.102, 0.016), bellows, 0, -0.05 - k * 0.032, 0, Math.PI / 2);
    of(arm).add(cap(0.095, 0.16), white, 0, -0.17, 0);
    if (opts.commander) of(arm).add(new THREE.CylinderGeometry(0.1, 0.099, 0.045, 16), red, 0, -0.16, 0);
    for (let k = 0; k < 3; k++) of(elbow).add(ring(0.092, 0.015), bellows, 0, 0.028 - k * 0.028, 0, Math.PI / 2);
    of(elbow).add(cap(0.084, 0.14), white, 0, -0.13, 0);
    of(elbow).add(new THREE.CylinderGeometry(0.066, 0.066, 0.032, 18), s > 0 ? red : blue, 0, -0.245, 0);
    of(elbow).add(cap(0.058, 0.06), glove, 0, -0.315, 0, 0, 0, 0, 1.12, 1, 0.78);
    of(elbow).add(cap(0.04, 0.05), blue, 0, -0.385, -0.005, 0, 0, 0, 1.4, 1, 0.72);                               /* the fingers' blue tips */
    of(elbow).add(cap(0.021, 0.04), glove, s * 0.062, -0.31, -0.02, 0, 0, s * 0.6);                                 /* the thumb */
  }
  for (const k in P) P[k].parts.into(P[k].g);

  /* how the pressurised suit stands by itself: arms a little out and forward, elbows bent, knees just unlocked */
  const pose = (name = 'stand') => {
    for (const k in J) J[k].rotation.set(0, 0, 0);
    /* (the figure faces -z: a positive turn about x swings a hanging limb forward, and leans the torso back) */
    if (J.armL) { J.armL.rotation.set(0.35, 0, -0.3); J.armR.rotation.set(0.35, 0, 0.3); J.elbowL.rotation.set(0.6, 0, 0); J.elbowR.rotation.set(0.6, 0, 0); }
    J.legL.rotation.set(0.1, 0, -0.03); J.legR.rotation.set(0.1, 0, 0.03); J.kneeL.rotation.set(-0.16, 0, 0); J.kneeR.rotation.set(-0.16, 0, 0);
    J.waist.rotation.set(-0.08, 0, 0);
    void name;
  };
  pose();
  root.userData.joints = J; root.userData.pose = pose; root.userData.visor = gold; root.userData.aerial = aerial; root.userData.helmet = helmet; root.userData.waist = waist;
  return root;
}

/* Your own helmet as you see it from inside (chapter 6): the shell's dark lining round the edge of your view, the
 * brow's rim above, and the visor's faint amber towards its edges (clear in the middle). The helmet is fixed to the
 * suit, not to your head: you look about inside it. Put the group at your eyes each frame, turned with your body.
 * set(k): 0 off (not drawn), up to 1 on: it comes down over your head from above, then the visor's tint comes in. */
export function makeHelmetView() {
  const group = new THREE.Group(), shell = new THREE.Group(); group.add(shell);
  const R = 0.2, lining = new THREE.MeshBasicMaterial({ color: 0x232426, side: THREE.BackSide }), rim = new THREE.MeshBasicMaterial({ color: 0x8d8b85, side: THREE.BackSide });
  const add = (geo, mat) => shell.add(new THREE.Mesh(geo, mat));
  add(new THREE.SphereGeometry(R, 30, 18, Math.PI * 11 / 6, Math.PI * 4 / 3, 0, Math.PI * 0.95), lining);      /* the back and the sides; the opening faces -z */
  add(new THREE.SphereGeometry(R, 30, 8, 0, Math.PI * 2, 0, Math.PI * 0.25), lining);                             /* the crown */
  add(new THREE.SphereGeometry(R - 0.001, 30, 2, 0, Math.PI * 2, Math.PI * 0.25, Math.PI * 0.025), rim);           /* the brow's rim */
  add(new THREE.SphereGeometry(R, 30, 6, 0, Math.PI * 2, Math.PI * 0.82, Math.PI * 0.18), lining);                 /* the chin */
  /* the visor: clear ahead, amber towards the edges (vertex alpha by the angle off straight ahead) */
  const vg = new THREE.SphereGeometry(R - 0.012, 36, 24), pos = vg.attributes.position, col = new Float32Array(pos.count * 4);
  for (let i = 0; i < pos.count; i++) {
    const ang = Math.acos(Math.max(-1, Math.min(1, -pos.getZ(i) / (R - 0.012)))), k = Math.max(0, Math.min(1, (ang - 0.55) / 0.75));
    col[i * 4] = 0.42; col[i * 4 + 1] = 0.3; col[i * 4 + 2] = 0.1; col[i * 4 + 3] = 0.015 + 0.3 * k * k * (3 - 2 * k);
  }
  vg.setAttribute('color', new THREE.BufferAttribute(col, 4));
  const visor = new THREE.Mesh(vg, new THREE.MeshBasicMaterial({ vertexColors: true, transparent: true, side: THREE.BackSide, depthWrite: false, opacity: 0 }));
  visor.renderOrder = 5; group.add(visor);
  for (const o of [group, shell, visor, ...shell.children]) o.frustumCulled = false;
  const api = { group, k: 1, set(k) { api.k = k = Math.max(0, Math.min(1, k)); group.visible = k > 0; const d = Math.min(1, k / 0.75); shell.position.y = 0.55 * (1 - d * d * (3 - 2 * d)); visor.material.opacity = Math.max(0, (k - 0.75) / 0.25); visor.visible = k > 0.75; } };
  api.set(1);
  return api;
}
