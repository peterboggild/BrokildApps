/* The Lunar Roving Vehicle (chapter 6), built in code.
 *
 * What makes it read as the real one: the low open aluminium chassis; four wire-mesh wheels with their titanium
 * chevron treads and the orange fenders over them; the two tubular seats with webbing, the Commander's on the left;
 * the console between them with the T-handle hand controller behind it; at the front the batteries under their
 * mirrored covers, the umbrella of the high-gain antenna pointed at the Earth, the low-gain antenna, and the
 * television camera in its gold foil on its own mast; at the back the pallet with the tool carrier and the sample
 * bags.
 *
 * Frame: the wheels on y = 0, forward -z (as the suits face), +x to the right. 3.1 m long, 1.83 m track, 2.29 m
 * wheelbase. One mesh per material in the body, and each wheel its own group (they turn and steer).
 *   const rover = makeRover(env);
 *   rover.userData.wheels    [{ g (steers about y), spin (turns about x), x, z }] front left, front right, rear left, rear right
 *   rover.userData.seat      { driver, mate }: where a seated man's hips are (the rover's frame)
 *   rover.userData.camera    the television camera's lens (an Object3D: its -z looks out of the lens; it pans and tilts)
 *   rover.userData.bag       where the samples go (a point at the back)
 *   rover.userData.handle    the hand controller (a Group: it tips with the stick)
 *   rover.userData.nav       the console's navigation display, as the real rover had (heading, and the bearing and
 *                            range of where you are going and of the LM; and, which the real one had not, the
 *                            samples loaded): nav.draw({ heading (degrees), name, rel (radians, to the right of the
 *                            nose), range (m), lmRel, lmRange, load: [kg, of] | null, board: true while it stands
 *                            empty (it then says how to climb on) }) */
import * as THREE from 'three';
import { Parts } from './lm-model.js?v=33';

const V = (x, y, z) => new THREE.Vector3(x, y, z);
export const ROVER = { TRACK: 1.83, BASE: 2.29, WHEEL_R: 0.41, LEN: 3.1 };

function meshTexture() {
  /* the wheel's woven wire, seen from a little way off: a fine open diagonal weave */
  const c = document.createElement('canvas'); c.width = 256; c.height = 64; const x = c.getContext('2d');
  x.clearRect(0, 0, 256, 64); x.strokeStyle = 'rgba(205,205,200,0.95)'; x.lineWidth = 1.6;
  for (let i = -64; i < 256; i += 8) { x.beginPath(); x.moveTo(i, 0); x.lineTo(i + 64, 64); x.stroke(); x.beginPath(); x.moveTo(i + 64, 0); x.lineTo(i, 64); x.stroke(); }
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(6, 1); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4; return t;
}
function webTexture() {
  const c = document.createElement('canvas'); c.width = 64; c.height = 64; const x = c.getContext('2d');
  x.fillStyle = '#8f9092'; x.fillRect(0, 0, 64, 64); x.fillStyle = '#6f7073';
  for (let i = 0; i < 64; i += 16) { x.fillRect(i, 0, 3, 64); x.fillRect(0, i, 64, 3); }
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(3, 3); t.colorSpace = THREE.SRGBColorSpace; return t;
}

export function makeRover(env) {
  const g = new THREE.Group();
  const Std = o => new THREE.MeshStandardMaterial({ envMap: env || null, envMapIntensity: env ? 0.6 : 0, ...o });
  const alu = Std({ color: 0xb9bcc0, roughness: 0.45, metalness: 0.75 }), aluD = Std({ color: 0x7d8186, roughness: 0.55, metalness: 0.6 });
  const white = Std({ color: 0xe9e7e0, roughness: 0.85, metalness: 0 }), dark = Std({ color: 0x232426, roughness: 0.7, metalness: 0.2 });
  const orange = Std({ color: 0xb8662c, roughness: 0.8, metalness: 0 }), gold = Std({ color: 0xd9a63c, roughness: 0.3, metalness: 1, envMapIntensity: env ? 1.2 : 0 });
  const mirror = Std({ color: 0xdfe3e8, roughness: 0.08, metalness: 1, envMapIntensity: env ? 1.4 : 0 });
  const web = Std({ color: 0xffffff, map: webTexture(), roughness: 0.9, metalness: 0 });
  const wire = new THREE.MeshStandardMaterial({ map: meshTexture(), transparent: true, alphaTest: 0.35, side: THREE.DoubleSide, roughness: 0.5, metalness: 0.7, color: 0xffffff });
  const goldMesh = new THREE.MeshStandardMaterial({ color: 0xcfa24a, roughness: 0.45, metalness: 0.9, side: THREE.DoubleSide, envMap: env || null, envMapIntensity: env ? 0.8 : 0 });
  const B = new Parts(), R = ROVER.WHEEL_R, HT = ROVER.TRACK / 2, HB = ROVER.BASE / 2, CH = 0.47;   /* CH: the chassis' deck */

  /* ---------------- chassis: two side rails, cross members, the floor between the seats */
  for (const s of [-1, 1]) { B.box(alu, 0.06, 0.09, 2.95, s * 0.5, CH, 0); B.box(alu, 0.05, 0.06, 0.9, s * 0.5, CH - 0.02, -1.05, 0.0, s * 0.12, 0); }
  for (const z of [-1.42, -0.8, -0.1, 0.62, 1.42]) B.box(alu, 1.06, 0.06, 0.06, 0, CH, z);
  B.box(aluD, 0.98, 0.012, 1.15, 0, CH + 0.03, 0.05);                                       /* the floor */
  /* the suspension arms out to each wheel (the wheels themselves are their own groups) */
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) {
    B.rod(alu, V(sx * 0.5, CH + 0.02, sz * (HB - 0.14)), V(sx * (HT - 0.16), R + 0.08, sz * HB), 0.018);
    B.rod(alu, V(sx * 0.5, CH - 0.06, sz * (HB + 0.14)), V(sx * (HT - 0.16), R - 0.08, sz * HB), 0.018);
    B.rod(aluD, V(sx * 0.5, CH + 0.3, sz * HB), V(sx * (HT - 0.2), R, sz * HB), 0.02);        /* the damper */
    /* the fender: orange fibreglass over the wheel's upper half, with its flap */
    const f = new THREE.CylinderGeometry(R + 0.07, R + 0.07, 0.3, 18, 1, true, Math.PI / 2 - 1.15, 2.3); f.rotateZ(Math.PI / 2);
    B.add(f, orange, sx * HT, R, sz * HB);
    B.box(orange, 0.3, 0.012, 0.2, sx * HT, R + 0.02, sz * HB + (sz > 0 ? 1 : -1) * (R + 0.14), sz * 0.9, 0, 0);
  }
  /* ---------------- the seats: tube frames, webbing, backs; footrests; the toeholds */
  const seat = { driver: V(-0.3, CH + 0.36, 0.32), mate: V(0.3, CH + 0.36, 0.32) };
  for (const s of [-1, 1]) {
    const x = s * 0.3;
    B.box(web, 0.44, 0.02, 0.42, x, CH + 0.3, 0.32);
    B.box(web, 0.44, 0.5, 0.02, x, CH + 0.56, 0.56, 0.18, 0, 0);
    for (const e of [-1, 1]) { B.rod(alu, V(x + e * 0.22, CH + 0.03, 0.12), V(x + e * 0.22, CH + 0.3, 0.12), 0.012); B.rod(alu, V(x + e * 0.22, CH + 0.03, 0.54), V(x + e * 0.22, CH + 0.82, 0.62), 0.012); B.rod(alu, V(x + e * 0.22, CH + 0.3, 0.12), V(x + e * 0.22, CH + 0.3, 0.54), 0.012); }
    B.box(aluD, 0.4, 0.015, 0.22, x, CH + 0.1, -0.55, -0.5, 0, 0);                            /* the footrest */
    B.rod(alu, V(s * 0.53, CH + 0.03, 0.2), V(s * 0.53, CH + 0.5, 0.2), 0.012); B.rod(alu, V(s * 0.53, CH + 0.5, 0.2), V(s * 0.53, CH + 0.5, 0.5), 0.012);   /* the outboard handhold */
  }
  /* ---------------- the console and the hand controller */
  B.rod(alu, V(0, CH + 0.03, -0.42), V(0, CH + 0.6, -0.36), 0.02);
  B.box(white, 0.3, 0.26, 0.09, 0, CH + 0.72, -0.36, -0.35, 0, 0);
  B.box(dark, 0.26, 0.1, 0.01, 0, CH + 0.775, -0.325, -0.35, 0, 0);                            /* the displays */
  for (const dx of [-0.09, -0.03, 0.03, 0.09]) B.add(new THREE.CylinderGeometry(0.012, 0.012, 0.02, 8), dark, dx, CH + 0.67, -0.29, Math.PI / 2 - 0.35, 0, 0);
  B.box(alu, 0.1, 0.05, 0.16, 0, CH + 0.4, 0.02);                                             /* the controller's pedestal */
  const handle = new THREE.Group(); handle.position.set(0, CH + 0.43, 0.02); g.add(handle);
  { const hp = new Parts(); hp.rod(dark, V(0, 0, 0), V(0, 0.12, 0), 0.012); hp.box(dark, 0.11, 0.025, 0.03, 0, 0.13, 0); hp.into(handle); }
  /* ---------------- the front: batteries under mirrored covers, the drive electronics */
  for (const s of [-1, 1]) { B.box(white, 0.4, 0.22, 0.42, s * 0.24, CH + 0.14, -1.12); B.box(mirror, 0.4, 0.008, 0.42, s * 0.24, CH + 0.255, -1.12); }
  B.box(white, 0.5, 0.16, 0.2, 0, CH + 0.11, -0.72);
  /* the high-gain antenna: a mast on the left front, the gold-mesh umbrella looking up at the Earth */
  B.rod(alu, V(-0.42, CH + 0.03, -1.38), V(-0.42, CH + 1.25, -1.38), 0.016);
  { const dish = new THREE.ConeGeometry(0.48, 0.2, 24, 1, true); B.add(dish, goldMesh, -0.42, CH + 1.42, -1.3, -2.3, 0, 0);
    for (let k = 0; k < 12; k++) { const a = k * Math.PI / 6; B.rod(alu, V(-0.42, CH + 1.3, -1.38), V(-0.42 + Math.cos(a) * 0.46, CH + 1.42 + Math.sin(a) * 0.36 * 0.75, -1.3 - Math.sin(a) * 0.36 * 0.66 + 0.02), 0.004, 4); }
    B.rod(alu, V(-0.42, CH + 1.3, -1.38), V(-0.42, CH + 1.52, -1.14), 0.008); B.ball(aluD, V(-0.42, CH + 1.52, -1.14), 0.03); }
  /* the low-gain antenna, by the Commander's side */
  B.rod(alu, V(-0.56, CH + 0.03, -0.3), V(-0.56, CH + 1.0, -0.3), 0.012); B.add(new THREE.CylinderGeometry(0.035, 0.035, 0.22, 10), white, -0.56, CH + 1.1, -0.3, 0.5, 0, 0);
  /* the television camera on its own mast, right front: it pans and tilts */
  B.rod(alu, V(0.42, CH + 0.03, -1.38), V(0.42, CH + 1.02, -1.38), 0.016);
  B.add(new THREE.CylinderGeometry(0.05, 0.06, 0.08, 12), aluD, 0.42, CH + 1.06, -1.38);
  const pan = new THREE.Group(); pan.position.set(0.42, CH + 1.16, -1.38); g.add(pan);
  const tilt = new THREE.Group(); pan.add(tilt);
  { const cp = new Parts(); cp.box(gold, 0.14, 0.13, 0.36, 0, 0, -0.02); cp.add(new THREE.CylinderGeometry(0.045, 0.05, 0.1, 14), dark, 0, 0, -0.25, Math.PI / 2, 0, 0);
    cp.add(new THREE.CircleGeometry(0.04, 14), mirror, 0, 0, -0.301, 0, Math.PI, 0); cp.box(white, 0.15, 0.01, 0.2, 0, 0.072, 0.02); cp.into(tilt); }
  const lens = new THREE.Object3D(); lens.position.set(0, 0, -0.31); tilt.add(lens);
  /* ---------------- the back: the pallet, the tool carrier, the sample bags */
  B.box(aluD, 0.98, 0.012, 0.7, 0, CH + 0.03, 1.12);
  for (const s of [-1, 1]) { B.rod(alu, V(s * 0.46, CH + 0.03, 1.42), V(s * 0.46, CH + 0.95, 1.46), 0.014); }
  B.rod(alu, V(-0.46, CH + 0.95, 1.46), V(0.46, CH + 0.95, 1.46), 0.014); B.rod(alu, V(-0.46, CH + 0.55, 1.44), V(0.46, CH + 0.55, 1.44), 0.012);
  B.rod(alu, V(-0.3, CH + 0.56, 1.46), V(-0.3, CH + 1.45, 1.5), 0.008); B.box(aluD, 0.16, 0.02, 0.06, -0.3, CH + 1.46, 1.5);       /* the rake */
  B.rod(alu, V(-0.1, CH + 0.56, 1.46), V(-0.1, CH + 1.35, 1.5), 0.008);                                                          /* the tongs */
  B.rod(alu, V(0.12, CH + 0.56, 1.46), V(0.12, CH + 1.25, 1.5), 0.01); B.box(aluD, 0.1, 0.14, 0.02, 0.12, CH + 0.6, 1.47);         /* the scoop */
  for (const [x, z] of [[-0.26, 1.02], [0.02, 1.02], [0.3, 1.02], [-0.12, 1.26], [0.2, 1.26]]) B.box(white, 0.24, 0.3, 0.2, x, CH + 0.19, z);
  B.into(g);

  /* ---------------- the wheels: the woven wire tyre with its chevrons, the hub, the fender is on the body */
  const wheels = [];
  for (const [sx, sz] of [[-1, -1], [1, -1], [-1, 1], [1, 1]]) {
    const wg = new THREE.Group(); wg.position.set(sx * HT, R, sz * HB); g.add(wg);
    const spin = new THREE.Group(); wg.add(spin);
    const P = new Parts();
    const tyre = new THREE.CylinderGeometry(R, R, 0.23, 28, 1, true); tyre.rotateZ(Math.PI / 2); P.add(tyre, wire);
    for (const e of [-1, 1]) { const side = new THREE.RingGeometry(0.2, R, 28, 1); P.add(side, wire, e * 0.115, 0, 0, 0, Math.PI / 2, 0); }
    /* the chevron treads: titanium strips across the tyre, in a herringbone */
    for (let k = 0; k < 18; k++) { const a = k * Math.PI / 9; for (const e of [-1, 1]) P.box(aluD, 0.12, 0.012, 0.035, e * 0.055, Math.cos(a) * (R + 0.004), Math.sin(a) * (R + 0.004), a - Math.PI / 2, 0, e * 0.5); }
    const hub = new THREE.CylinderGeometry(0.19, 0.19, 0.1, 18); hub.rotateZ(Math.PI / 2); P.add(hub, alu);
    const cap = new THREE.CylinderGeometry(0.1, 0.1, 0.14, 14); cap.rotateZ(Math.PI / 2); P.add(cap, aluD);
    P.into(spin);
    wheels.push({ g: wg, spin, x: sx * HT, z: sz * HB });
  }
  g.userData.wheels = wheels; g.userData.seat = seat; g.userData.camera = lens; g.userData.pan = pan; g.userData.tilt = tilt;
  g.userData.bag = V(0, CH + 0.5, 1.15); g.userData.handle = handle; g.userData.deck = CH;
  /* the navigation display: drawn on a small canvas, on the console's face towards the seats */
  { const c = document.createElement('canvas'); c.width = 512; c.height = 256; const x = c.getContext('2d');
    const tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace; tex.anisotropy = 4;
    const face = new THREE.Mesh(new THREE.PlaneGeometry(0.27, 0.135), new THREE.MeshBasicMaterial({ map: tex, toneMapped: false }));
    face.rotation.x = -0.35; face.position.set(0, CH + 0.775 + 0.343 * 0.009, -0.325 + 0.939 * 0.009); g.add(face);
    const F = 'system-ui, Segoe UI, Arial, sans-serif';
    const arrow = (cx, cy, r, a, col) => {
      x.save(); x.translate(cx, cy); x.rotate(a); x.fillStyle = col;
      x.beginPath(); x.moveTo(0, -r); x.lineTo(r * 0.55, r * 0.15); x.lineTo(r * 0.2, r * 0.15); x.lineTo(r * 0.2, r * 0.9); x.lineTo(-r * 0.2, r * 0.9); x.lineTo(-r * 0.2, r * 0.15); x.lineTo(-r * 0.55, r * 0.15); x.closePath(); x.fill();
      x.restore();
    };
    const nav = { face, last: '',
      draw(o) {
        const key = [Math.round(o.heading / 2), o.name, Math.round(o.rel * 20), Math.round(o.range), Math.round((o.lmRel || 0) * 12), Math.round(o.lmRange || 0), o.load ? Math.round(o.load[0]) : '', o.board ? 'board' : ''].join();
        if (key === nav.last) return; nav.last = key;
        x.fillStyle = '#10140f'; x.fillRect(0, 0, 512, 256);
        if (o.board) {
          /* parked and empty: how to get on */
          x.textAlign = 'center'; x.textBaseline = 'middle';
          x.fillStyle = '#f0c040'; x.font = 'bold 54px ' + F; x.fillText('CLIMB ON', 256, 78, 480);
          x.fillStyle = '#e8e4d4'; x.font = 'bold 34px ' + F; x.fillText('pull a trigger or grip', 256, 148, 480); x.fillText('beside the rover', 256, 196, 480);
          tex.needsUpdate = true; return;
        }
        /* the dial: the rover's nose is up; ticks every 30 degrees turn with the heading, N marked */
        const cx = 128, cy = 128, R = 104;
        x.strokeStyle = '#6f7a66'; x.lineWidth = 3; x.beginPath(); x.arc(cx, cy, R, 0, Math.PI * 2); x.stroke();
        for (let d = 0; d < 360; d += 30) {
          const a = (d - o.heading) * Math.PI / 180, s = Math.sin(a), co = -Math.cos(a);
          x.strokeStyle = d ? '#6f7a66' : '#e8e4d4'; x.lineWidth = d % 90 ? 2 : 4;
          x.beginPath(); x.moveTo(cx + s * (R - (d % 90 ? 10 : 18)), cy + co * (R - (d % 90 ? 10 : 18))); x.lineTo(cx + s * R, cy + co * R); x.stroke();
          if (!d) { x.fillStyle = '#e8e4d4'; x.font = 'bold 26px ' + F; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText('N', cx + s * (R - 34), cy + co * (R - 34)); }
        }
        const LMC = '#8fd0e8', GO = '#f0c040';
        if (o.lmRange > 8) arrow(cx, cy, 58, o.lmRel, LMC);                             /* the way back to the LM: small, blue */
        if (o.name && o.name !== 'The LM') arrow(cx, cy, 88, o.rel, GO);                /* the way to go: large, yellow */
        x.textAlign = 'left'; x.textBaseline = 'alphabetic';
        const goLM = o.name === 'The LM';
        x.fillStyle = GO; x.font = 'bold 26px ' + F; x.fillText(o.name && !goLM ? o.name.toUpperCase() : goLM ? 'BACK TO THE LM' : 'NO TARGET', 262, 44, 240);
        x.font = 'bold 62px ' + F; x.fillText(o.name && !goLM ? Math.round(o.range) + ' m' : '', 262, 104, 240);
        x.fillStyle = LMC; x.font = 'bold 34px ' + F; x.fillText('LM  ' + Math.round(o.lmRange || 0) + ' m', 262, 152, 240);
        x.fillStyle = '#e8e4d4'; x.font = 'bold 30px ' + F; if (o.load) x.fillText('LOAD  ' + Math.round(o.load[0]) + ' / ' + o.load[1] + ' kg', 262, 198, 240);
        x.fillStyle = '#9aa58e'; x.font = 'bold 22px ' + F; x.fillText('HDG ' + String(Math.round(((o.heading % 360) + 360) % 360)).padStart(3, '0'), 262, 236, 240);
        tex.needsUpdate = true;
      } };
    nav.draw({ heading: 0, name: '', rel: 0, range: 0, lmRel: 0, lmRange: 0 });
    g.userData.nav = nav; }
  return g;
}
