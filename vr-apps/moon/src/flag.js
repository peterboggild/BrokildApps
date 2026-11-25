/* The flag (chapter 6): the Apollo flag assembly, built in code. A two-piece aluminium pole (2.4 m planted), a
 * horizontal crossbar hinged at its top that holds the nylon flag out (there is no wind to do it), 1.52 x 0.91 m,
 * with the wrinkles it kept from its packing.
 *
 * Frame: the pole's foot at the origin, the pole up +y, the flag flying to +x.
 *   const flag = makeFlag();
 *   flag.userData.set(k)      0 loose in the hand of whoever carries it (leaning, the foot on the ground), 1 planted:
 *                             upright, 0.25 m into the regolith
 *   flag.userData.unfurl(u)   0 the crossbar down along the pole and the flag gathered, 1 out
 *   flag.userData.grip        where you take hold of the pole (a point, the flag's frame) */
import * as THREE from 'three';

function flagTexture() {
  const c = document.createElement('canvas'); c.width = 512; c.height = 320; const x = c.getContext('2d'), w = 512, h = 320;
  for (let k = 0; k < 13; k++) { x.fillStyle = k % 2 ? '#f1eee6' : '#b22234'; x.fillRect(0, k * h / 13, w, h / 13 + 0.6); }
  x.fillStyle = '#3c3b6e'; x.fillRect(0, 0, w * 0.4, h * 7 / 13);
  x.fillStyle = '#f1eee6';
  for (let r = 0; r < 9; r++) for (let q = 0; q < (r % 2 ? 5 : 6); q++) {
    const cx = (q + (r % 2 ? 1 : 0.5)) * w * 0.4 / 6, cy = (r + 0.75) * h * 7 / 13 / 9.6;
    x.beginPath(); for (let i = 0; i < 10; i++) { const a = -Math.PI / 2 + i * Math.PI / 5, rr = i % 2 ? 2.6 : 6.2; x.lineTo(cx + Math.cos(a) * rr, cy + Math.sin(a) * rr); } x.closePath(); x.fill();
  }
  /* the creases of the packing: faint darker lines */
  x.strokeStyle = 'rgba(40,30,30,0.12)'; x.lineWidth = 2;
  for (const u of [0.25, 0.5, 0.75]) { x.beginPath(); x.moveTo(u * w, 0); x.lineTo(u * w, h); x.stroke(); }
  for (const v of [0.33, 0.66]) { x.beginPath(); x.moveTo(0, v * h); x.lineTo(w, v * h); x.stroke(); }
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; return t;
}

export function makeFlag() {
  const g = new THREE.Group(), body = new THREE.Group(); g.add(body);
  const alu = new THREE.MeshStandardMaterial({ color: 0xc9c2a6, roughness: 0.35, metalness: 0.8 });
  const cloth = new THREE.MeshLambertMaterial({ map: flagTexture(), side: THREE.DoubleSide });
  const H = 2.4, W = 1.52, FH = 0.91;
  const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.0125, 0.0125, H + 0.3, 10), alu); pole.position.y = (H - 0.3) / 2; body.add(pole);
  const collar = new THREE.Mesh(new THREE.CylinderGeometry(0.018, 0.018, 0.06, 10), alu); collar.position.y = 1.2; body.add(collar);   /* where the two pieces join */
  /* the crossbar and the flag hang from a hinge at the pole's top */
  const hinge = new THREE.Group(); hinge.position.y = H; body.add(hinge);
  const bar = new THREE.Mesh(new THREE.CylinderGeometry(0.008, 0.008, W + 0.04, 8), alu); bar.rotation.z = Math.PI / 2; bar.position.x = W / 2; hinge.add(bar);
  const geo = new THREE.PlaneGeometry(W, FH, 30, 14), p = geo.attributes.position;
  for (let i = 0; i < p.count; i++) {
    const u = p.getX(i) / W + 0.5, v = 0.5 - p.getY(i) / FH;                 /* u from the pole out, v from the bar down */
    /* held along the top and the pole's side; the free corner hangs in, with the wrinkles */
    const free = u * v;
    p.setZ(i, (0.035 * Math.sin(u * 9 + v * 3) + 0.02 * Math.sin(u * 21 - v * 7)) * (0.25 + 0.75 * v) + 0.03 * Math.sin(v * 6) * u);
    p.setX(i, p.getX(i) - 0.07 * free * free); p.setY(i, p.getY(i) + 0.02 * free);
  }
  geo.computeVertexNormals();
  const cloth3 = new THREE.Mesh(geo, cloth); cloth3.position.set(W / 2, -FH / 2 - 0.008, 0); hinge.add(cloth3);
  g.userData.grip = new THREE.Vector3(0, 1.25, 0);
  g.userData.top = H;
  g.userData.planted = 0;
  g.userData.set = k => {
    k = Math.max(0, Math.min(1, k)); g.userData.planted = k;
    body.rotation.z = -0.22 * (1 - k); body.rotation.x = 0.1 * (1 - k); body.position.y = -0.02 - 0.23 * k;
  };
  g.userData.unfurl = u => {
    u = Math.max(0, Math.min(1, u)); const e = u * u * (3 - 2 * u);
    hinge.rotation.z = -Math.PI / 2 * (1 - e) * 0.96; cloth3.scale.set(1, 1, 0.3 + 0.7 * e); cloth3.scale.y = 0.35 + 0.65 * e; cloth3.position.y = (-FH / 2) * cloth3.scale.y - 0.008;
  };
  g.userData.set(0); g.userData.unfurl(0);
  return g;
}
