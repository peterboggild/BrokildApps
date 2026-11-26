/*
 * Journey to the Moon: your hands, in the spacecraft's intravehicular gloves. Taken from Portalkniven (hands.js),
 * with the glove's colour, a fingertip to touch things with, and a tint for 'not now'.
 *
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
 *
 * The hand itself is the WebXR generic hand model (models/hand-left.glb, hand-right.glb, from @webxr-input-profiles/assets, MIT), which happens to be laid
 * out in exactly this space: an open, flat hand with its fingers along -Y. Its 25 joints are not a hierarchy (each has its place in the model's own space),
 * so closing the hand is worked out here: every finger is a chain, each joint bends about its own X axis, and what a joint does carries on to the joints
 * beyond it. The thumb is first swung round to face the fingers (opposition) and then bent. Until the model has loaded, or if it cannot be, a simple hand
 * built from capsules stands in.
 */
import * as THREE from 'three';
import { GLTFLoader } from '../vendor/GLTFLoader.js';
import { settings } from './settings.js?v=32';

const D = Math.PI / 180;
const lerp = (a, b, t) => a + (b - a) * t;

/* joint angles in degrees (knuckle, middle joint, last joint) */
const INDEX_OPEN = [44, 52, 22], INDEX_SHUT = [70, 95, 30];        // resting on the trigger / trigger pulled
const REST_LOOSE = [60, 84, 26], REST_TIGHT = [73, 98, 33];        // the other three, holding the controller / squeezing it

/* the hand tipped up against the controller (degrees, about the palm's crosswise axis): held as you point, the
 * controller points a little higher than the index finger would on the bare grip pose (owner's headset test: 15-20
 * degrees low). No documented offset exists for the Quest 3 in WebXR, so it is a setting ('hand', default 17), to
 * be matched to your own hand in the headset. */

export function createHand(side, opts = {}) {
  const root = new THREE.Group(), tilt = new THREE.Group(), h = new THREE.Group(); root.add(tilt); tilt.add(h);
  tilt.rotation.x = settings.get('hand') * D; root.userData.tilt = tilt;
  settings.onChange(k => { if (k === 'hand') tilt.rotation.x = settings.get('hand') * D; });
  if (side === 'left') h.scale.x = -1;
  const GLOVE = opts.color ?? 0x6a7a90;
  const leather = new THREE.MeshStandardMaterial({ color: GLOVE, roughness: 0.7, metalness: 0.02 });
  const cuffM = new THREE.MeshStandardMaterial({ color: opts.cuff ?? 0x2c4fa8, roughness: 0.6, metalness: 0.1 });   /* the wrist ring */
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
    if (root.userData.modelPose) root.userData.modelPose();
  };
  root.userData.pose(0, 0, false, false, 1);
  /* the index fingertip, for touching (the stand-in's until the model is in) */
  const tipS = new THREE.Object3D(); tipS.position.set(0, -0.022, 0); index[2].add(tipS);
  root.userData.tip = tipS;
  root.userData.setTint = (warn) => { leather.color.setHex(warn ? 0xc0261e : GLOVE); };
  loadModel(side, root, h, leather, st);
  return root;
}

/* ---- the real model ---- */
const FINGERS = ['index-finger', 'middle-finger', 'ring-finger', 'pinky-finger'];
const M_INDEX_OPEN = [38, 48, 24], M_INDEX_SHUT = [72, 96, 42], M_LOOSE = [66, 88, 40], M_TIGHT = [78, 100, 48];
const _q = new THREE.Quaternion(), _a = new THREE.Vector3(), _v = new THREE.Vector3();

function loadModel(side, root, standIn, leather, st) {
  new GLTFLoader().load(new URL('../models/hand-' + side + '.glb', import.meta.url).href, gltf => {
    const model = gltf.scene, bones = {}; let mesh = null;
    model.traverse(o => { if (o.isSkinnedMesh) mesh = o; if (o.isBone || o.name) bones[o.name] = o; });
    if (!mesh || !bones.wrist) return;
    mesh.material = leather; mesh.frustumCulled = false;
    /* The bones relative to the hand, not the world. three.js skins with each bone's world matrix, sent to the GPU
     * in 32-bit floats: tens of thousands of kilometres from the origin (chapter 3 starts 66,000 km out in the Moon's
     * frame) that is metres of error and the hand tears into random shapes (owner's headset test). Worked out here in
     * 64-bit and only the small hand-local matrices go to the GPU: bone' = M^-1 . boneWorld . boneInverse . B, with
     * the bind matrix and its inverse then left as identity ('detached'). */
    {
      const B = mesh.bindMatrix.clone(), skel = mesh.skeleton, inv = new THREE.Matrix4(), mm = new THREE.Matrix4();
      mesh.bindMode = THREE.DetachedBindMode; mesh.bindMatrix.identity(); mesh.bindMatrixInverse.identity();
      skel.update = function () {
        inv.copy(mesh.matrixWorld).invert();
        for (let i = 0; i < this.bones.length; i++) {
          const b = this.bones[i];
          mm.multiplyMatrices(inv, b ? b.matrixWorld : mesh.matrixWorld).multiply(this.boneInverses[i]).multiply(B);
          mm.toArray(this.boneMatrices, i * 16);
        }
        if (this.boneTexture !== null) this.boneTexture.needsUpdate = true;
      };
    }
    if (bones['index-finger-tip']) root.userData.tip = bones['index-finger-tip'];
    const sx = side === 'left' ? -1 : 1;                           // which side of the rod the palm is on
    model.position.set(-0.008 * sx, 0.02, 0);                      // the rod lies in the crease at the root of the fingers, not in the middle of the palm
    const rest = n => ({ b: bones[n], p: bones[n].position.clone(), q: bones[n].quaternion.clone(), x: new THREE.Vector3(1, 0, 0).applyQuaternion(bones[n].quaternion) });
    /* a chain of joints; bends[j] = [[axis, angle], ...] turns joint j and everything beyond it. Positions and directions are in the model's own space. */
    const run = (chain, bends) => {
      const A = new THREE.Quaternion(), p = chain[0].p.clone();
      chain.forEach((c, j) => {
        if (j > 0) p.add(_v.copy(c.p).sub(chain[j - 1].p).applyQuaternion(A));
        for (const [axis, ang] of bends[j] || []) A.multiply(_q.setFromAxisAngle(axis, ang));      // A is the turn carried so far, applied to rest directions
        c.b.position.copy(p); c.b.quaternion.copy(A).multiply(c.q);
      });
    };
    /* which way is "closed"? Try a small bend and see whether the fingertip goes across to the other side of the rod. */
    const sign = (chain, j, axis) => { const tip = chain[chain.length - 1].p, piv = chain[j].p; _v.copy(tip).sub(piv).applyAxisAngle(axis, 0.3).add(piv); return (_v.x - tip.x) * sx < 0 ? 1 : -1; };
    const fingers = FINGERS.map(n => { const c = ['metacarpal', 'phalanx-proximal', 'phalanx-intermediate', 'phalanx-distal', 'tip'].map(k => rest(n + '-' + k)); return { c, s: [1, 2, 3].map(j => sign(c, j, c[j].x)) }; });
    const tc = ['metacarpal', 'phalanx-proximal', 'phalanx-distal', 'tip'].map(k => rest('thumb-' + k)), Z = new THREE.Vector3(0, 0, 1);
    const ts = { opp: sign(tc, 0, Z), f: [0, 1, 2].map(j => sign(tc, j, tc[j].x)) };
    const D = Math.PI / 180, lerp = (a, b, t) => a + (b - a) * t;
    root.userData.modelPose = () => {
      const it = Math.max(st.trig, st.full * 0.94), ot = Math.max(st.sq, st.full);
      fingers.forEach((f, k) => { const o = k === 0 ? M_INDEX_OPEN : M_LOOSE, c = k === 0 ? M_INDEX_SHUT : M_TIGHT, t = k === 0 ? it : ot;
        run(f.c, [null, [[f.c[1].x, f.s[0] * lerp(o[0], c[0], t) * D]], [[f.c[2].x, f.s[1] * lerp(o[1], c[1], t) * D]], [[f.c[3].x, f.s[2] * lerp(o[2], c[2], t) * D]]]); });
      /* the thumb opposes the fingers: swung round the rod to the other side, then bent down on to it (more when it rests on something, or holds the knife) */
      const dn = Math.max(st.th, st.full);
      /* the thumb a little shorter than the model's (owner's headset test: too long): its bones drawn in towards the
       * root and scaled */
      const L = TH.len, tcL = tc.map((c, j) => j ? { ...c, p: tc[0].p.clone().lerp(c.p, L) } : c);
      for (let j = 1; j < tc.length; j++) tc[j].b.scale.setScalar(L);
      run(tcL, [[[Z, ts.opp * (TH.opp + TH.oppDown * dn) * D], [tc[0].x, ts.f[0] * (TH.f0 + TH.f0Down * dn) * D]], [[tc[1].x, ts.f[1] * (TH.f1 + TH.f1Down * dn) * D]], [[tc[2].x, ts.f[2] * (TH.f2 + TH.f2Down * dn) * D]]]);
    };
    standIn.visible = false; root.userData.tilt.add(model); root.userData.model = model; root.userData.modelPose();
  }, undefined, e => console.warn('hand model not loaded, keeping the simple hand', e));
}
/* the thumb's angles (degrees): how far it is swung round to face the fingers, and how much each joint is bent; the "Down" parts are added when it presses */
/* owner's headset test (25 November 2026): thumbs too long and pointing at each other: less opposition (46 -> 15, the
 * thumb lies forward along the top of the controller beside the index finger) and 85 % of the model's length;
 * tools/hands.html shows it round a stand-in controller (?opp= &len= to try values) */
export const TH = { opp: 15, oppDown: 6, f0: 16, f0Down: 6, f1: 30, f1Down: 10, f2: 30, f2Down: 14, len: 0.85 };

/* The controllers by hand. three.js numbers its controllers in the order they connect, which is not the order of
 * session.inputSources and changes when they come back (headset off and on, a controller asleep): taking one from
 * each meant the hands could swap sides (owner's headset test). Here every frame reads each input source's own pose
 * from the XR frame, by its handedness, into one group per hand under parent (the camera's parent).
 *   const ctl = makeControllers(renderer, head);   ctl.update() first thing every frame;
 *   ctl.grips.left / .right (grip space: the hand), ctl.rays.left / .right (target ray), ctl.src.left / .right */
export function makeControllers(renderer, parent) {
  const mk = () => { const g = new THREE.Group(); g.visible = false; parent.add(g); return g; };
  const grips = { left: mk(), right: mk() }, rays = { left: mk(), right: mk() }, src = { left: null, right: null };
  const place = (o, pose) => {
    if (!pose) { o.visible = false; return; }
    const p = pose.transform.position, q = pose.transform.orientation;
    o.position.set(p.x, p.y, p.z); o.quaternion.set(q.x, q.y, q.z, q.w); o.visible = true;
  };
  function update() {
    const s = renderer.xr.getSession(), frame = renderer.xr.getFrame && renderer.xr.getFrame(), ref = renderer.xr.getReferenceSpace();
    src.left = src.right = null;
    if (s && frame && ref) for (const is of s.inputSources) {
      const h = is.handedness; if (!(h in grips) || src[h]) continue;
      src[h] = is;
      place(grips[h], is.gripSpace ? frame.getPose(is.gripSpace, ref) : null);
      place(rays[h], frame.getPose(is.targetRaySpace, ref));
    }
    for (const h of ['left', 'right']) if (!src[h]) { grips[h].visible = false; rays[h].visible = false; }
  }
  return { grips, rays, src, update };
}
