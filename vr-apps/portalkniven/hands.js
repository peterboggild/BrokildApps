/*
 * Portalkniven — your hands in the other dimensions.
 *
 * In your own room you see your real hands (passthrough). Everywhere else there would be nothing where your hands are, so each controller gets a gloved hand
 * closed round it, the way your real hand is closed round the controller. It moves as yours does: the index finger follows the trigger, the other three
 * fingers tighten with the grip button, the thumb comes down when it rests on the stick or a button.
 *
 * The hand is built in WebXR "grip space": the origin is the middle of the palm; a rod held in the fist lies along Z, with -Z coming out at the thumb's end
 * (the knife's blade points that way); X goes through the palm, +X to the user's right for both hands. So a RIGHT hand has its palm on the +X side of the
 * rod, facing -X, its wrist towards +Y and its knuckles towards -Y; its fingers leave the knuckles and wrap round the rod towards -X. The left hand is the
 * mirror image.
 */
import * as THREE from './vendor/three.module.min.js';

const D = Math.PI / 180;
const lerp = (a, b, t) => a + (b - a) * t;

/* joint angles in degrees (knuckle, middle joint, last joint) */
const INDEX_OPEN = [44, 52, 22], INDEX_SHUT = [70, 95, 30];        // resting on the trigger / trigger pulled
const REST_LOOSE = [60, 84, 26], REST_TIGHT = [73, 98, 33];        // the other three, holding the controller / squeezing it

export function createHand(side) {
  const root = new THREE.Group(), h = new THREE.Group(); root.add(h);
  if (side === 'left') h.scale.x = -1;
  const leather = new THREE.MeshStandardMaterial({ color: 0x2c2520, roughness: 0.58, metalness: 0.04 });
  const cuffM = new THREE.MeshStandardMaterial({ color: 0x4a3a2c, roughness: 0.7, metalness: 0.0 });
  const seam = new THREE.MeshStandardMaterial({ color: 0x120e0c, roughness: 0.8 });

  /* the palm: a rounded slab in the Y-Z plane, 1.6 cm clear of the rod's axis */
  const s = new THREE.Shape(), z0 = -0.044, z1 = 0.043, y0 = -0.024, y1 = 0.072, r = 0.017;
  s.moveTo(z0 + r, y0); s.lineTo(z1 - r, y0); s.quadraticCurveTo(z1, y0, z1, y0 + r); s.lineTo(z1, y1 - r); s.quadraticCurveTo(z1, y1, z1 - r, y1);
  s.lineTo(z0 + r, y1); s.quadraticCurveTo(z0, y1, z0, y1 - r); s.lineTo(z0, y0 + r); s.quadraticCurveTo(z0, y0, z0 + r, y0);
  const pg = new THREE.ExtrudeGeometry(s, { depth: 0.014, bevelEnabled: true, bevelThickness: 0.007, bevelSize: 0.006, bevelSegments: 3, curveSegments: 8 });
  pg.rotateY(-Math.PI / 2);                                        // shape x -> hand z, thickness -> hand -x; then moved out to the palm's side of the rod
  pg.translate(0.039, 0, 0);
  h.add(new THREE.Mesh(pg, leather));
  /* the heel of the thumb, and the cuff of the glove round the wrist */
  const heel = new THREE.Mesh(new THREE.SphereGeometry(0.024, 14, 10), leather); heel.scale.set(0.75, 1.25, 0.9); heel.position.set(0.026, 0.038, -0.03); h.add(heel);
  const cuff = new THREE.Mesh(new THREE.CylinderGeometry(0.031, 0.034, 0.04, 20, 1), cuffM); cuff.scale.set(0.66, 1, 1); cuff.position.set(0.033, 0.094, 0); h.add(cuff);
  const band = new THREE.Mesh(new THREE.TorusGeometry(0.0315, 0.003, 6, 22), seam); band.rotation.x = Math.PI / 2; band.scale.set(0.66, 1, 1); band.position.set(0.033, 0.074, 0); h.add(band);

  /* a finger: three bones, each a capsule hanging from its joint; curling turns each joint about Z, towards -X (round the rod) */
  function finger(x, y, z, lens, rad, splay) {
    const joints = []; let parent = h, at = new THREE.Vector3(x, y, z);
    lens.forEach((len, k) => {
      const j = new THREE.Group(); j.position.copy(at); if (k === 0) j.rotation.x = splay;
      const rr = rad * (1 - 0.07 * k), bone = new THREE.Mesh(new THREE.CapsuleGeometry(rr, len, 4, 10), leather); bone.position.y = -len / 2; j.add(bone);
      if (k === 0) { const kn = new THREE.Mesh(new THREE.SphereGeometry(rad * 1.3, 12, 9), leather); kn.position.set(0.004, 0.002, 0); j.add(kn); }      // the knuckle
      parent.add(j); joints.push(j); parent = j; at = new THREE.Vector3(0, -len, 0);
    });
    return joints;
  }
  const index = finger(0.026, -0.017, -0.033, [0.040, 0.025, 0.021], 0.0102, 0.05);
  const others = [
    finger(0.026, -0.019, -0.011, [0.044, 0.028, 0.022], 0.0105, 0.015),
    finger(0.026, -0.017, 0.011, [0.040, 0.026, 0.021], 0.0100, -0.02),
    finger(0.027, -0.012, 0.031, [0.031, 0.020, 0.018], 0.0088, -0.06),
  ];
  /* the thumb: from the heel, across the top of the fist (the -Z end), to rest where the stick and the buttons are */
  const thumb = []; {
    let parent = h, at = new THREE.Vector3(0.022, 0.03, -0.046);
    [[0.030, 0.0125], [0.027, 0.0112], [0.023, 0.0102]].forEach(([len, rr], k) => {
      const j = new THREE.Group(); j.position.copy(at);
      if (k === 0) j.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), new THREE.Vector3(-0.30, -0.50, -0.81).normalize());
      const bone = new THREE.Mesh(new THREE.CapsuleGeometry(rr, len, 4, 10), leather); bone.position.y = -len / 2; j.add(bone);
      parent.add(j); thumb.push(j); parent = j; at = new THREE.Vector3(0, -len, 0);
    });
  }

  const st = { trig: 0, sq: 0, th: 0, full: 0 };
  /** trig, squeeze: 0..1 from the controller; thumbDown: the thumb rests on something; full: the hand is closed round the knife. */
  root.userData.pose = (trig, squeeze, thumbDown, full, dt) => {
    const k = Math.min(1, dt * 18);
    st.trig += (trig - st.trig) * k; st.sq += (squeeze - st.sq) * k; st.th += ((thumbDown ? 1 : 0) - st.th) * k; st.full += ((full ? 1 : 0) - st.full) * k;
    const it = Math.max(st.trig, st.full * 0.92), ot = Math.max(st.sq, st.full);
    index.forEach((j, n) => { j.rotation.z = -lerp(INDEX_OPEN[n], INDEX_SHUT[n], it) * D; });
    for (const f of others) f.forEach((j, n) => { j.rotation.z = -lerp(REST_LOOSE[n], REST_TIGHT[n], ot) * D; });
    thumb[1].rotation.z = -(30 + 14 * st.th + 14 * st.full) * D; thumb[2].rotation.z = -(26 + 18 * st.th + 14 * st.full) * D;
  };
  root.userData.pose(0, 0, false, false, 1);
  return root;
}
