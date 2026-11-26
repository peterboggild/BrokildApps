/* The Lunar Module's crew compartment, from inside: simplified from the 1969 LM. Two people stand here, the
 * Commander on the left, the Lunar Module Pilot on the right, held by restraint cables, facing two small
 * triangular windows. Behind them the midsection with the ascent engine's cover, and over it the hatch up into the
 * docking tunnel. Over the Commander's head the small docking window, to see the Command Module.
 *
 * Frame: the LM's own (vehicle.js makeLM): y up from the bottom of the descent stage, z forward (the windows), so the
 * hatch here meets the tunnel of the model outside. The cabin floor is at FLOOR, the ceiling at CEIL.
 * Walls face inwards; the windows and the overhead hatch are holes, so the outside shows through them when this is
 * drawn last as the occluder (as cabin.js). */
import * as THREE from 'three';
import { makeCrewman, makePanelTexture, makeDsky, makeBallTexture, makeWallTexture } from './cabin.js?v=39';
import { makeEVASuit } from './eva-suit.js?v=39';
import { makeCrewLife } from './crew-life.js?v=39';
import { LM } from './lm-model.js?v=39';

export const LMC = {
  FLOOR: 2.45, CEIL: 4.3, FRONT: 1.25, STATION_BACK: 0.3, BACK: -0.8, HALF: 0.95,
  CDR: new THREE.Vector3(0.45, 2.45, 0.78),            /* where the Commander stands (left, facing +z) */
  LMP: new THREE.Vector3(-0.45, 2.45, 0.78),
  HATCH: new THREE.Vector3(0, 4.3, 0), HATCH_R: 0.41
};

/* opts.inwardOnly: the walls drawn on their inner side only. Where the cabin stands inside the LM's model and is seen
 * from outside too (chapter 6), its walls must not show through or over the hull: from outside you then see the cabin
 * only through the open hatch (owner's headset test: the cabin was drawn in front of the LM). */
export function makeLMCabin(opts = {}) {
  const { FLOOR, CEIL, FRONT, STATION_BACK, BACK, HALF } = LMC;
  const g = new THREE.Group();
  const L = (color, o = {}) => new THREE.MeshLambertMaterial({ color, ...o });
  const wallTex = makeWallTexture(); wallTex.repeat.set(0.5, 1);
  const wall = L(0xc4c7bf, { map: wallTex, side: THREE.DoubleSide });
  const wallF = L(0xc4c7bf, { map: wallTex, side: THREE.FrontSide }), wallB = L(0xc4c7bf, { map: wallTex, side: THREE.BackSide });
  const CENTRE = new THREE.Vector3(0, (LMC.FLOOR + LMC.CEIL) / 2, (LMC.FRONT + LMC.BACK) / 2), reveals = [];
  const dark = L(0x3a3b3c), metal = L(0x9a9da0), black = L(0x161718), beta = L(0xe2dfd5), floorM = L(0x67696b);
  const put = (geo, m, x, y, z, rx = 0, ry = 0, rz = 0, parent = g) => { const o = new THREE.Mesh(geo, m); o.position.set(x, y, z); o.rotation.set(rx, ry, rz); parent.add(o); return o; };

  /* a flat wall from a 2D outline with holes: u, v in the plane; `basis` turns (u, v, 0) into the cabin */
  const wallFrom = (outline, holes, basis, origin, m = wall) => {
    const sh = new THREE.Shape(outline.map(([u, v]) => new THREE.Vector2(u, v)));
    for (const h of holes) sh.holes.push(h);
    const geo = new THREE.ShapeGeometry(sh, 32);
    if (opts.inwardOnly && m === wall) {                       /* which side looks into the cabin? */
      const n = new THREE.Vector3().setFromMatrixColumn(basis, 2);
      m = n.dot(CENTRE.clone().sub(origin)) > 0 ? wallF : wallB;
    }
    const mesh = new THREE.Mesh(geo, m);
    mesh.matrixAutoUpdate = false; mesh.matrix.copy(basis).setPosition(origin);
    g.add(mesh); return mesh;
  };
  const tri = (pts) => new THREE.Path(pts.map(([u, v]) => new THREE.Vector2(u, v)));

  /* ---------------- the front wall: the two triangular windows (Commander's at +x, the right angle at the inner
   * bottom corner), the forward hatch below between the two stations */
  const WY0 = FLOOR + 1.12, WY1 = FLOOR + 1.84, WI = 0.12, WO = 0.66;            /* deep: the view forward and down */
  const front = new THREE.Matrix4().makeBasis(new THREE.Vector3(-1, 0, 0), new THREE.Vector3(0, 1, 0), new THREE.Vector3(0, 0, -1));
  /* in the wall's u (= -x) the Commander's window is at negative u */
  wallFrom([[-HALF, FLOOR], [HALF, FLOOR], [HALF, CEIL], [-HALF, CEIL]],
    [tri([[-WI, WY0], [-WI, WY1], [-WO, WY0]]), tri([[WI, WY0], [WO, WY0], [WI, WY1]]),
      tri([[-LM.HATCH.half, LM.HATCH.y0], [-LM.HATCH.half, LM.HATCH.y1], [LM.HATCH.half, LM.HATCH.y1], [LM.HATCH.half, LM.HATCH.y0]])], front, new THREE.Vector3(0, 0, FRONT));
  /* window frames: a deep reveal round each triangle */
  for (const sx of [1, -1]) {
    const pts = [[sx * WI, WY0], [sx * WO, WY0], [sx * WI, WY1]];
    for (let i = 0; i < 3; i++) {
      const a = pts[i], b = pts[(i + 1) % 3], len = Math.hypot(b[0] - a[0], b[1] - a[1]);
      /* (from 1 cm inside the wall outwards: a face lying in the wall's own plane flickered against it, and the
       * flicker moved with the head: owner's headset test) */
      const m = put(new THREE.BoxGeometry(len, 0.03, 0.13), black, (a[0] + b[0]) / 2, (a[1] + b[1]) / 2, FRONT + 0.055);
      m.rotation.z = Math.atan2(b[1] - a[1], b[0] - a[0]); reveals.push(m);
    }
  }
  /* the forward hatch: an opening in the wall (the same as the model's outside, lm-model.js), its door hinged on
   * the Lunar Module Pilot's side and swinging inwards; a frame round it, the latch handle, the dump valve */
  const HT = LM.HATCH, hw = HT.half * 2, hh = HT.y1 - HT.y0, hc = (HT.y0 + HT.y1) / 2;
  const fwdDoor = new THREE.Group(); fwdDoor.position.set(-HT.half, 0, FRONT - 0.022); g.add(fwdDoor);
  put(new THREE.BoxGeometry(hw, hh, 0.03), L(0x8d9093), HT.half, hc, 0, 0, 0, 0, fwdDoor);
  put(new THREE.BoxGeometry(hw - 0.12, hh - 0.12, 0.012), L(0x7b7e81), HT.half, hc, -0.02, 0, 0, 0, fwdDoor);
  put(new THREE.BoxGeometry(0.05, 0.16, 0.05), metal, hw - 0.1, hc, -0.05, 0, 0, 0, fwdDoor);                 /* the latch handle */
  put(new THREE.CylinderGeometry(0.035, 0.035, 0.03, 14), black, HT.half, hc + 0.2, -0.035, Math.PI / 2, 0, 0, fwdDoor);   /* the dump valve */
  for (const [w, h, x, y] of [[hw + 0.1, 0.05, 0, HT.y1 + 0.025], [0.05, hh, -HT.half - 0.025, hc], [0.05, hh, HT.half + 0.025, hc]]) put(new THREE.BoxGeometry(w, h, 0.04), dark, x, y, FRONT - 0.026);

  /* ---------------- floor, ceiling (holes: the docking window over the Commander, the hatch over the midsection),
   * side and back walls, the step up to the midsection */
  const up = new THREE.Matrix4().makeBasis(new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 0, -1), new THREE.Vector3(0, 1, 0));
  wallFrom([[-HALF, -FRONT], [HALF, -FRONT], [HALF, -STATION_BACK], [-HALF, -STATION_BACK]], [], up, new THREE.Vector3(0, FLOOR, 0), floorM);
  wallFrom([[-HALF, -STATION_BACK], [HALF, -STATION_BACK], [HALF, -BACK], [-HALF, -BACK]], [], up, new THREE.Vector3(0, FLOOR + 0.32, 0), floorM);
  put(new THREE.PlaneGeometry(2 * HALF, 0.32), floorM, 0, FLOOR + 0.16, STATION_BACK, 0, 0, 0);
  const hatchHole = new THREE.Path(); hatchHole.absarc(LMC.HATCH.x, -LMC.HATCH.z, LMC.HATCH_R, 0, Math.PI * 2, true);
  const dw = new THREE.Path([[0.36, -1.02], [0.36, -0.78], [0.54, -0.78], [0.54, -1.02]].map(([u, v]) => new THREE.Vector2(u, v)));
  wallFrom([[-HALF, -FRONT], [HALF, -FRONT], [HALF, -BACK], [-HALF, -BACK]], [hatchHole, dw], up, new THREE.Vector3(0, CEIL, 0));
  for (const sx of [-1, 1]) {
    const side = new THREE.Matrix4().makeBasis(new THREE.Vector3(0, 0, 1), new THREE.Vector3(0, 1, 0), new THREE.Vector3(-sx, 0, 0));
    wallFrom([[BACK, FLOOR], [FRONT, FLOOR], [FRONT, CEIL], [BACK, CEIL]], [], side, new THREE.Vector3(sx * HALF, 0, 0));
  }
  put(new THREE.PlaneGeometry(2 * HALF, CEIL - FLOOR), opts.inwardOnly ? wallF : wall, 0, (FLOOR + CEIL) / 2, BACK, 0, 0, 0);
  /* the docking window's reveal, and the hatch rim and tunnel stub above the ceiling */
  for (const [x, z, w, d] of [[0.36, 0.9, 0.02, 0.26], [0.54, 0.9, 0.02, 0.26], [0.45, 0.78, 0.2, 0.02], [0.45, 1.02, 0.2, 0.02]]) put(new THREE.BoxGeometry(w, 0.11, d), black, x, CEIL + 0.045, z);
  put(new THREE.TorusGeometry(LMC.HATCH_R + 0.02, 0.03, 10, 40), metal, 0, CEIL - 0.02, 0, Math.PI / 2);
  const stub = put(new THREE.CylinderGeometry(LMC.HATCH_R, LMC.HATCH_R, 0.45, 32, 1, true), L(0x9a9c9e, { side: THREE.BackSide }), 0, CEIL + 0.225, 0);
  /* the overhead hatch itself: open (swung up into the tunnel, as for the crew coming through) or shut */
  const hatchLid = put(new THREE.CircleGeometry(LMC.HATCH_R, 32), L(0x6f7275, { side: THREE.DoubleSide }), 0, CEIL + 0.01, 0, Math.PI / 2);

  /* ---------------- panels: the two flight panels under the windows, the centre console with the DSKY between the
   * stations, a side console at each station's outer wall */
  const panel = makePanelTexture([], 'lm', [[0, 0.22], [0.22, 0.44], [0.44, 0.54], [0.60, 0.80], [0.80, 1.0]]);
  const pm = L(0xffffff, { map: panel.texture });
  const panels = [];
  const panelAt = (u0, u1, w, h, x, y, z, rx, ry) => {
    const geo = new THREE.PlaneGeometry(w, h), uv = geo.attributes.uv;
    for (let i = 0; i < uv.count; i++) uv.setX(i, u0 + (u1 - u0) * uv.getX(i));
    const m = put(geo, pm, x, y, z, rx, ry, 0);
    const box = new THREE.Mesh(new THREE.BoxGeometry(w + 0.03, h + 0.03, 0.1), dark); box.position.z = -0.055; m.add(box);
    panels.push({ mesh: m, w, h });
    return m;
  };
  /* The flight panels hang from the wall just under the windows and slope out towards you, their faces turned up
   * to a standing man's eyes, like the top of a console. (They were turned the other way, a sign wrong: their
   * faces looked down at your knees and could not be read at all: owner's headset test. Flat on the wall they
   * would be seen almost edge on from where you stand, 35 cm from it and 60 cm above them.) PT: how far the face
   * is turned up from upright; the top edge is on the wall at TOP. */
  panel.texture.anisotropy = 16;
  const PT = 0.72, TOP = FLOOR + 1.10, PH = 0.36;
  const PY = TOP - PH / 2 * Math.cos(PT), PZ = FRONT - 0.03 - PH / 2 * Math.sin(PT);
  const p1 = panelAt(0.00, 0.22, 0.62, 0.36, 0.42, PY, PZ, PT, Math.PI);
  const p2 = panelAt(0.22, 0.44, 0.62, 0.36, -0.42, PY, PZ, PT, Math.PI);
  const p3 = panelAt(0.44, 0.54, 0.18, 0.36, 0, PY, PZ + 0.004, PT, Math.PI);          /* between the two, clear of the hatch below */
  /* the side panels: flat on the side walls beside each station, at the height of your shoulders and eyes, upright
   * (they were lower and turned in their own plane, like a picture hung askew: owner's headset test) */
  panelAt(0.60, 0.80, 0.42, 0.62, HALF - 0.058, FLOOR + 1.34, 0.8, 0, -Math.PI / 2);
  panelAt(0.80, 1.00, 0.42, 0.62, -HALF + 0.058, FLOOR + 1.34, 0.8, 0, Math.PI / 2);
  /* under each, a console standing against the wall */
  /* (one cabinet under each, its top below the panel's lower edge; a second box behind the panel hid it: owner's headset test) */
  for (const sx of [-1, 1]) put(new THREE.BoxGeometry(0.26, 0.64, 0.66), dark, sx * (HALF - 0.13), FLOOR + 0.32, 0.8);
  /* the DSKY on the centre console, an attitude ball on each flight panel */
  const dsky = makeDsky();
  const dm = new THREE.Mesh(new THREE.PlaneGeometry(0.15, 0.17), new THREE.MeshBasicMaterial({ map: dsky.texture })); dm.position.set(0, -0.04, 0.006); p3.add(dm);
  const ballTex = makeBallTexture(), balls = [];
  for (const p of [p1, p2]) {
    const b = new THREE.Mesh(new THREE.SphereGeometry(0.05, 32, 24), L(0xffffff, { map: ballTex })); b.position.set(0, 0.06, 0.005); p.add(b); balls.push(b);
    const rim = new THREE.Mesh(new THREE.RingGeometry(0.05, 0.064, 40), black); rim.position.set(0, 0.06, 0.03); p.add(rim);
  }
  /* ---------------- the alignment telescope coming down from the ceiling between the windows */
  put(new THREE.CylinderGeometry(0.045, 0.05, 0.26, 20), dark, 0, CEIL - 0.13, FRONT - 0.3);
  put(new THREE.BoxGeometry(0.08, 0.08, 0.12), dark, 0, CEIL - 0.3, FRONT - 0.33);
  put(new THREE.CylinderGeometry(0.02, 0.02, 0.08, 12), black, 0, CEIL - 0.33, FRONT - 0.42, Math.PI / 2);

  /* ---------------- armrests and hand controllers, restraint cables, a handhold each side */
  for (const st of [LMC.CDR, LMC.LMP]) {
    for (const sx of [-1, 1]) {
      put(new THREE.BoxGeometry(0.07, 0.05, 0.36), beta, st.x + sx * 0.3, FLOOR + 1.0, st.z + 0.12);
      put(new THREE.CylinderGeometry(0.008, 0.008, 0.5, 6), metal, st.x + sx * 0.3, FLOOR + 0.75, st.z + 0.12);
    }
    put(new THREE.CylinderGeometry(0.022, 0.026, 0.13, 12), dark, st.x - 0.3, FLOOR + 1.09, st.z + 0.26);     /* right hand: attitude */
    put(new THREE.BoxGeometry(0.05, 0.07, 0.08), dark, st.x + 0.3, FLOOR + 1.08, st.z + 0.26);                  /* left hand: thrust */
    for (const sx of [-1, 1]) {
      const a = new THREE.Vector3(st.x + sx * 0.32, FLOOR + 0.02, st.z - 0.25), b = new THREE.Vector3(st.x + sx * 0.14, FLOOR + 0.98, st.z - 0.05);
      const c = new THREE.Mesh(new THREE.CylinderGeometry(0.006, 0.006, a.distanceTo(b), 6), black);
      c.position.copy(a).add(b).multiplyScalar(0.5); c.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize()); g.add(c);
    }
  }
  /* the ascent engine's cover in the midsection, stowage behind */
  const cover = L(0xa9a69c);
  put(new THREE.CylinderGeometry(0.5, 0.62, 0.42, 28), cover, 0, FLOOR + 0.53, -0.3);
  put(new THREE.SphereGeometry(0.5, 28, 12, 0, Math.PI * 2, 0, Math.PI / 2), cover, 0, FLOOR + 0.74, -0.3).scale.set(1, 0.35, 1);
  for (const sx of [-1, 1]) put(new THREE.BoxGeometry(0.32, 0.9, 0.36), L(0x6e7174), sx * 0.7, FLOOR + 0.8, -0.55);

  /* ---------------- the Lunar Module Pilot, standing at the right station */
  /* your own suit, standing at the Commander's station (as in the Command Module: no head, no forearms) */
  let body, lmp, life;
  if (opts.eva) {
    /* dressed for the surface (chapter 6): the suits of eva-suit.js. Yours without helmet and arms; his a little
     * stooped under the low ceiling, the pack's aerial folded down */
    body = makeEVASuit({ player: true, commander: true, env: opts.env });
    body.position.set(LMC.CDR.x, FLOOR - 0.13, LMC.CDR.z - 0.07); body.rotation.y = Math.PI; body.userData.aerial.visible = false; g.add(body);
    lmp = makeEVASuit({ env: opts.env });
    lmp.position.set(LMC.LMP.x, FLOOR, LMC.LMP.z); lmp.rotation.y = Math.PI - 0.45; lmp.scale.setScalar(0.92); lmp.userData.aerial.visible = false; g.add(lmp);
    /* (the cabin's floodlights are tuned for its matt greys: the white cloth would burn out in them) */
    for (const s of [body, lmp]) { const seen = new Set(); s.traverse(o => { if (o.isMesh && !seen.has(o.material) && o.material !== s.userData.visor) { seen.add(o.material); o.material.color.multiplyScalar(0.6); } }); }
    /* before the suiting-up his helmet is in his hands, his head bare in the communications cap (the head of
     * cabin.js's crewman, set on this suit's neck); it goes on over his head, and from then the visor hides him */
    const donor = makeCrewman({ variant: 1, standing: true }), neck = donor.userData.neck, face = donor.userData.face;
    neck.position.set(0, 0.62, -0.01); lmp.userData.waist.add(neck);
    const hel = lmp.userData.helmet; let suitK = 1;
    const setSuit = k => { suitK = k = Math.max(0, Math.min(1, k)); const e = x => x * x * (3 - 2 * x), a = e(Math.min(1, k * 2)), b = e(Math.max(0, k * 2 - 1));
      hel.position.set(0, -0.5 + 0.98 * a - 0.48 * b, -0.42 + 0.4 * a + 0.02 * b); hel.rotation.x = -0.5 * (1 - a); neck.visible = k < 0.97; };
    setSuit(1);
    /* his life in the stiff suit: the whole torso turns, slowly, now and then towards you; the arms work a little */
    const J = lmp.userData.joints, _v = new THREE.Vector3(); let yaw = 0, last = null;
    life = { update(t, ctx = {}) {
      const dt = last === null ? 0 : Math.min(0.1, t - last); last = t;
      let want = 0.22 * Math.sin(t * 0.21) + 0.08 * Math.sin(t * 0.53);
      if (ctx.viewer && Math.sin(t * 0.13) > 0.2) { lmp.worldToLocal(ctx.viewer.getWorldPosition(_v)); want = Math.max(-0.7, Math.min(0.7, Math.atan2(-_v.x, -_v.z))); }
      yaw += (want - yaw) * Math.min(1, dt * 0.9);
      J.waist.rotation.y = yaw; J.waist.rotation.x = -0.1 + 0.015 * Math.sin(t * 0.7);
      J.elbowL.rotation.x = 0.75 + 0.12 * Math.sin(t * 0.37); J.elbowR.rotation.x = 0.7 + 0.1 * Math.sin(t * 0.29 + 1);
      J.armL.rotation.x = 0.4 + 0.06 * Math.sin(t * 0.23 + 2); J.armR.rotation.x = 0.38 + 0.05 * Math.sin(t * 0.31);
      if (suitK < 1) {
        /* the helmet in his hands: before his chest, then lifted over his head; he looks at you meanwhile, and blinks */
        const lift = Math.sin(Math.PI * Math.min(1, suitK * 1.15)), up = 0.75 + 1.45 * lift * (suitK < 0.5 ? 1 : 0.9);
        J.armL.rotation.x = J.armR.rotation.x = up; J.elbowL.rotation.x = J.elbowR.rotation.x = 1.15 + 0.5 * lift;
        J.armL.rotation.z = -0.05; J.armR.rotation.z = 0.05;
        neck.rotation.y = 0.5 * yaw; face.uBlink.value = (t % 3.4) < 0.13 ? 1 : 0;
      } else { J.armL.rotation.z = -0.3; J.armR.rotation.z = 0.3; }
    }, touch() {}, react() {}, setSuit };
  } else {
    body = makeCrewman({ player: true, standing: true });
    body.position.set(LMC.CDR.x, FLOOR + 1.6 - 0.09, LMC.CDR.z - 0.07); body.rotation.y = Math.PI; g.add(body);
    lmp = makeCrewman({ variant: 1, standing: true });
    lmp.position.set(LMC.LMP.x, FLOOR + 1.6, LMC.LMP.z); lmp.rotation.y = Math.PI; g.add(lmp);
    life = makeCrewLife([{ man: lmp, who: 'LMP' }]);
  }

  /* ---------------- light: floodlights over each station; the Sun through the windows */
  const amb = new THREE.HemisphereLight(0xf4f1ea, 0x8a877f, 2.6);
  const f1 = new THREE.PointLight(0xfff0dc, 4.5, 3.6, 1.0); f1.position.set(0.45, CEIL - 0.25, 0.5);
  const f2 = new THREE.PointLight(0xfff0dc, 4.0, 3.6, 1.0); f2.position.set(-0.45, CEIL - 0.25, 0.5);
  const sunIn = new THREE.DirectionalLight(0xfff6e8, 0);
  g.add(amb, f1, f2, sunIn, sunIn.target);

  return {
    group: g, panels, dsky, balls, lmp, sunIn,
    setHatchOpen(on) { hatchLid.visible = !on; stub.visible = on; },
    setBody(on) { body.visible = on; },
    /* the Lunar Module Pilot's helmet (the surface suits only): 0 in his hands, 1 on */
    setSuitUp(k) { if (life.setSuit) life.setSuit(k); },
    /* seen from outside: the windows' deep frames (they reach out through the hull) are not drawn */
    setOutside(on) { for (const r of reveals) r.visible = !on; },
    /* the forward hatch: 0 shut, 1 swung in against the Lunar Module Pilot's side */
    setForwardHatch(k) { fwdDoor.rotation.y = 1.65 * k; },
    setDaylight(k, sunLocal) { sunIn.intensity = 0.8 * k; sunIn.position.copy(sunLocal).multiplyScalar(3); amb.intensity = 2.4 + 0.6 * k; },
    /* the LMP's life (crew-life.js); ctx: { viewer (the camera), radio, busy } */
    life,
    update(t, ctx) { life.update(t, ctx); }
  };
}
