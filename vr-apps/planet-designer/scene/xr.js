/* VR (milestone M5): the WebXR session, the user rig, controllers, locomotion, the panel and the workshop tools.
 * Patterns from Illusion Studio (spec §15): everything personal (camera, controllers, panel) lives in a "dolly" whose
 * scale is 1/S; grab the world with the grips (one hand moves, two hands scale and turn); left stick flies, right
 * stick turns in 30-degree steps (and moves up and down); Y or B opens the panel; the trigger works rays.
 * No controller button resets the view: only the Meta button recentres (owner rule). While presenting, only the
 * XR loop renders (pitfall: a desktop render during XR breaks the frame), and nothing resizes the renderer. */
import * as THREE from 'three';
import { createWorkshop, textSprite, LAYOUT } from './workshop.js?v=8';
import { createVRPanel } from '../ui/vrpanel.js?v=8';
import { rOf, aOf } from './orrery.js?v=8';
import { t } from '../i18n.js?v=8';

const UP = new THREE.Vector3(0, 1, 0);
const MASS_RANGE = [Math.log10(0.08), Math.log10(2.0)];
const AGE_RANGE = [Math.log10(0.01), Math.log10(15)];
const ARC = THREE.MathUtils.degToRad(135);

export function createXR(V) {
  const { renderer, scene, camera, S } = V;
  const VR = { session: null, frames: 0, S: 1, yaw: 0, hands: [], supported: { vr: false }, panelOpen: false, lastT: 0 };
  renderer.xr.enabled = true;

  const ws = createWorkshop();
  scene.add(ws.root);
  const dolly = new THREE.Group();
  dolly.name = 'dolly';
  scene.add(dolly);
  VR.dolly = dolly;

  const panel = createVRPanel(S);
  panel.mesh.visible = false;
  dolly.add(panel.mesh);
  VR.panel = panel;

  /* controllers: target-ray space for rays, grip space for a small body */
  const ray = new THREE.Raycaster();
  const lineGeo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), new THREE.Vector3(0, 0, -1)]);
  for (let i = 0; i < 2; i++) {
    const ctrl = renderer.xr.getController(i), grip = renderer.xr.getControllerGrip(i);
    const line = new THREE.Line(lineGeo, new THREE.LineBasicMaterial({ color: 0x9fd0ff, transparent: true, opacity: 0.7, toneMapped: false }));
    line.scale.z = 3; ctrl.add(line);
    const dot = new THREE.Mesh(new THREE.SphereGeometry(0.012, 12, 8), new THREE.MeshBasicMaterial({ color: 0xffffff, toneMapped: false }));
    dot.visible = false; dolly.add(dot);
    const body = new THREE.Mesh(new THREE.CylinderGeometry(0.018, 0.024, 0.11, 16), new THREE.MeshStandardMaterial({ color: 0x2a323c, metalness: 0.5, roughness: 0.4 }));
    body.rotation.x = Math.PI / 2 - 0.4; grip.add(body);
    dolly.add(ctrl, grip);
    const hs = { i, ctrl, grip, line, dot, src: null, handed: null, mode: null, hover: null, prevBtn: [] };
    ctrl.addEventListener('connected', (e) => { hs.src = e.data; hs.handed = e.data.handedness; });
    ctrl.addEventListener('disconnected', () => { hs.src = null; });
    ctrl.addEventListener('selectstart', () => onPress(hs, 'trigger'));
    ctrl.addEventListener('selectend', () => onRelease(hs, 'trigger'));
    ctrl.addEventListener('squeezestart', () => onPress(hs, 'squeeze'));
    ctrl.addEventListener('squeezeend', () => onRelease(hs, 'squeeze'));
    VR.hands.push(hs);
  }

  /* labels over the orrery's planets (the desktop's HTML labels do not exist in VR) */
  const beadLabels = [];
  function syncBeadLabels() {
    const beads = V.orrery.beads;
    while (beadLabels.length > beads.length) ws.root.remove(beadLabels.pop());
    beads.forEach((b, k) => {
      const name = S.design.planets[b.index] ? S.design.planets[b.index].name : '';
      if (!beadLabels[k]) { beadLabels[k] = textSprite(name, { size: 0.045 }); ws.root.add(beadLabels[k]); beadLabels[k].userData.text = name; }
      else if (beadLabels[k].userData.text !== name) { beadLabels[k].userData.setText(name); beadLabels[k].userData.text = name; }
    });
  }

  /* ---------- rig ---------- */
  function setDolly(p, yaw, s) {
    VR.S = Math.max(0.05, Math.min(25, s)); VR.yaw = yaw;
    dolly.position.copy(p); dolly.quaternion.setFromAxisAngle(UP, yaw); dolly.scale.setScalar(1 / VR.S);
    dolly.updateMatrixWorld(true);
  }
  /* the head from the scene camera (a child of the dolly), so the dolly's own turn is included; the XR array camera's
   * world matrix does not carry its parent when re-computed, which turned "forward" backwards after a snap turn */
  const headWorld = () => { camera.updateMatrixWorld(true); return new THREE.Vector3().setFromMatrixPosition(camera.matrixWorld); };
  const ctrlWorld = (hs) => new THREE.Vector3().setFromMatrixPosition(hs.ctrl.matrixWorld);

  /* ---------- panel ---------- */
  function togglePanel(open) {
    VR.panelOpen = open === undefined ? !VR.panelOpen : open;
    panel.mesh.visible = VR.panelOpen;
    if (VR.panelOpen) {
      /* in front of the head, a little low, facing it (dolly-local coordinates) */
      const head = camera.position.clone();
      const yaw = new THREE.Euler().setFromQuaternion(camera.quaternion, 'YXZ').y;
      const fwd = new THREE.Vector3(-Math.sin(yaw), 0, -Math.cos(yaw));
      panel.mesh.position.copy(head).addScaledVector(fwd, 0.62).add(new THREE.Vector3(0, -0.2, 0));
      panel.mesh.rotation.set(-0.3, yaw, 0, 'YXZ');
      panel.markDirty();
    }
  }
  S.xr = { togglePanel, exit: () => VR.session && VR.session.end(),
    resetSun: () => { ws.forgeGroup.position.copy(LAYOUT.forge); },
    setSmoothTurn: (v) => { VR.smoothTurn = v; }, get smoothTurn() { return !!VR.smoothTurn; } };
  function panelPx(origin, dir) {
    if (!panel.mesh.visible) return null;
    panel.mesh.updateMatrixWorld(true);
    const n = new THREE.Vector3(0, 0, 1).transformDirection(panel.mesh.matrixWorld);
    const P = new THREE.Vector3().setFromMatrixPosition(panel.mesh.matrixWorld);
    const den = n.dot(dir);
    if (Math.abs(den) < 1e-6) return null;
    const tt = n.dot(P.clone().sub(origin)) / den;
    if (tt < 0) return null;
    const hit = origin.clone().addScaledVector(dir, tt);
    const lp = panel.mesh.worldToLocal(hit.clone());
    if (Math.abs(lp.x) > panel.PW / 2 || Math.abs(lp.y) > panel.PH / 2) return null;
    return { px: (lp.x / panel.PW + 0.5) * panel.W, py: (0.5 - lp.y / panel.PH) * panel.H, point: hit, dist: tt };
  }

  /* ---------- dials ---------- */
  function dialAngle(dial, origin, dir) {
    dial.updateMatrixWorld(true);
    const n = new THREE.Vector3(0, 0, 1).transformDirection(dial.matrixWorld);
    const c = new THREE.Vector3().setFromMatrixPosition(dial.matrixWorld);
    const den = n.dot(dir);
    if (Math.abs(den) < 1e-6) return null;
    const hit = origin.clone().addScaledVector(dir, n.dot(c.clone().sub(origin)) / den);
    const lp = dial.worldToLocal(hit);
    return Math.max(-ARC, Math.min(ARC, Math.atan2(lp.x, lp.y)));
  }
  const toRange = (a, R) => 10 ** (R[0] + (a + ARC) / (2 * ARC) * (R[1] - R[0]));
  const fromRange = (v, R) => (Math.log10(v) - R[0]) / (R[1] - R[0]) * 2 * ARC - ARC;
  function syncDials() {
    ws.massDial.userData.setAngle(fromRange(S.design.star.massSun, MASS_RANGE));
    ws.ageDial.userData.setAngle(fromRange(Math.max(0.01, S.design.star.ageGyr), AGE_RANGE));
    const D = S.derived;
    /* the forge burns in the star's colour and floods the platform with it: the first lesson, felt not read */
    ws.forge.set(D.star);
    const c = D.star.rgb;
    ws.starLight.color.setRGB(c[0], c[1], c[2]);
    ws.starLight.intensity = 140 * Math.min(1.6, Math.max(0.35, D.star.lum ** 0.15));
    ws.massDial.userData.value.userData.setText(`${S.design.star.massSun.toFixed(2)} ${t('u.Msun')}`);
    ws.ageDial.userData.value.userData.setText(`${S.design.star.ageGyr.toFixed(2)} ${t('u.Gyr')}`);
    const Pd = S.derivedPlanet();
    ws.distDial.userData.setAngle(0);
    ws.distDial.userData.value.userData.setText(Pd ? `${Pd.aAU.toFixed(3)} ${t('u.AU')} · ${Pd.role === 'hz' ? (Pd.climate.runaway ? t('v.runawayT') : (Pd.ebm ? '' : '~') + Math.round(Pd.TsC) + ' °C') : ''}` : '');
    ws.forgeLabel.userData.setText(`${D.star.spt} · ${Math.round(D.star.teff)} K · ${D.star.lum < 0.1 ? D.star.lum.toPrecision(2) : D.star.lum.toFixed(2)} L☉`);
    const P = S.derivedPlanet();
    ws.cradleLabel.userData.setText(P ? P.name : '');
  }

  /* ---------- input ---------- */
  function pickList() {
    const list = [];
    ws.massDial.traverse((o) => { if (o.userData.kind === 'dial') list.push(o); });
    ws.ageDial.traverse((o) => { if (o.userData.kind === 'dial') list.push(o); });
    ws.distDial.traverse((o) => { if (o.userData.kind === 'dial') list.push(o); });
    list.push(ws.forge.group.children[0]);
    for (const o of V.orrery.pickables()) list.push(o);
    return list;
  }
  function rayOf(hs) {
    hs.ctrl.updateMatrixWorld(true);
    const o = ctrlWorld(hs);
    const d = new THREE.Vector3(0, 0, -1).transformDirection(hs.ctrl.matrixWorld);
    return { o, d };
  }
  function updateHover(hs) {
    const { o, d } = rayOf(hs);
    const pp = panelPx(o, d);
    ray.set(o, d);
    const hit = ray.intersectObjects(pickList(), false)[0];
    if (pp && (!hit || pp.dist < hit.distance)) {
      panel.hover(pp.px, pp.py);
      hs.hover = { kind: 'panel', px: pp.px, py: pp.py, point: pp.point, dist: pp.dist };
    } else if (hit) {
      const u = hit.object.userData;
      hs.hover = { kind: u.kind, dial: u.dial, data: u, point: hit.point, dist: hit.distance };
    } else hs.hover = null;
    const len = hs.hover ? hs.hover.dist * VR.S : 3;
    hs.line.scale.z = Math.max(0.05, len);
    hs.dot.visible = !!hs.hover;
    if (hs.hover) hs.dot.position.copy(dolly.worldToLocal(hs.hover.point.clone()));
  }

  function onPress(hs, kind) {
    if (kind === 'squeeze') { hs.mode = 'world'; hs.modeBy = kind; VR.grab = null; return; }
    updateHover(hs);
    const hv = hs.hover;
    if (!hv) return;
    if (hv.kind === 'panel') { if (panel.down(hv.px, hv.py, performance.now())) { hs.mode = 'ui'; hs.modeBy = kind; } return; }
    if (hv.kind === 'dial') {
      /* relative and fine: turning the dial by a whole sweep moves the value by half its range */
      const { o, d } = rayOf(hs);
      hs.mode = 'dial'; hs.modeBy = kind; hs.dial = hv.dial;
      hs.dialA0 = dialAngle(hv.dial, o, d) || 0;
      hs.dialV0 = hv.dial === ws.massDial ? S.design.star.massSun : hv.dial === ws.ageDial ? S.design.star.ageGyr : S.planet().orbit.aAU;
      return;
    }
    /* the star: carry it round with the ray (at the distance it was grabbed), to light the planet from any side */
    if (hv.kind === 'sun') {
      const { o } = rayOf(hs);
      hs.mode = 'sun'; hs.modeBy = kind;
      hs.sunDist = o.distanceTo(ws.forgeGroup.getWorldPosition(new THREE.Vector3()));
      return;
    }
    if (hv.kind === 'slot') { S.addHzAt(hv.data.a); return; }
    if (hv.kind === 'bead' || hv.kind === 'handle') {
      S.sel = hv.data.index; if (S.redrawAll) S.redrawAll();
      hs.mode = hv.kind; hs.modeBy = kind;
      /* relative and fine: the bead moves by a fraction of the pointer's movement on the table */
      const lp = tablePoint(hs);
      hs.drag0 = lp ? Math.hypot(lp.x, lp.z) : null;
      hs.a0 = S.planet().orbit.aAU;
    }
  }
  function onRelease(hs, kind) {
    if (hs.modeBy !== kind) return;
    if (hs.mode === 'ui') panel.up();
    if (hs.mode === 'bead' || hs.mode === 'handle' || hs.mode === 'dial') { if (S.redrawAll) S.redrawAll(); }
    if (hs.mode === 'world') VR.grab = null;
    hs.mode = null; hs.modeBy = null;
  }
  function dragDial(hs) {
    const { o, d } = rayOf(hs);
    const a = dialAngle(hs.dial, o, d);
    if (a == null) return;
    if (hs.dial === ws.distDial) {
      /* the planet's distance, very finely: a whole sweep of the dial is about +-10 % */
      const f = Math.exp((a - hs.dialA0) / (2 * ARC) * 0.2);
      S.set('planet.orbit.aAU', Math.max(0.01, Math.min(50, hs.dialV0 * f)));
      return;
    }
    const R = hs.dial === ws.massDial ? MASS_RANGE : AGE_RANGE;
    const lv = Math.log10(Math.max(hs.dialV0, 0.01)) + (a - hs.dialA0) / (2 * ARC) * (R[1] - R[0]) * 0.5;
    const v = 10 ** Math.max(R[0], Math.min(R[1], lv));
    if (hs.dial === ws.massDial) S.set('star.massSun', v); else S.set('star.ageGyr', v);
  }
  const tablePlane = new THREE.Plane();
  function tablePoint(hs) {
    const { o, d } = rayOf(hs);
    V.orrery.group.updateMatrixWorld(true);
    const c = new THREE.Vector3().setFromMatrixPosition(V.orrery.group.matrixWorld);
    tablePlane.setFromNormalAndCoplanarPoint(new THREE.Vector3(0, 1, 0), c);
    const p = new THREE.Vector3();
    if (!new THREE.Ray(o, d).intersectPlane(tablePlane, p)) return null;
    return V.orrery.group.worldToLocal(p);
  }
  function dragOrrery(hs) {
    const lp = tablePoint(hs);
    if (!lp) return;
    if (hs.mode === 'bead') {
      if (hs.drag0 == null) return;
      const r0 = rOf(hs.a0), r = r0 + (Math.hypot(lp.x, lp.z) - hs.drag0) * 0.25;
      S.set('planet.orbit.aAU', aOf(Math.max(0.5, r)));
    }
    else { S.set('planet.orbit.e', V.orrery.toE(lp, S.planet().orbit.aAU)); S.set('planet.orbit.argPeriDeg', V.orrery.toArgPeri(lp)); }
  }

  function worldGrab() {
    const act = VR.hands.filter((h) => h.mode === 'world' && h.src);
    if (!act.length) { VR.grab = null; return; }
    const ids = act.map((h) => h.i).join(',');
    if (!VR.grab || VR.grab.ids !== ids) {
      VR.grab = { ids, W: act.map((h) => ctrlWorld(h)), S0: VR.S, yaw0: VR.yaw };
      return;
    }
    if (act.length === 1) {
      const now = ctrlWorld(act[0]);
      dolly.position.add(VR.grab.W[0].clone().sub(now));
      dolly.updateMatrixWorld(true);
    } else {
      /* two hands: the grabbed world points stay in the hands; their distance sets the scale, their direction the yaw */
      const a = act[0].ctrl.position.clone(), b = act[1].ctrl.position.clone();      /* dolly-local */
      const A = VR.grab.W[0], B = VR.grab.W[1];
      const dl = Math.hypot(a.x - b.x, a.z - b.z) + 1e-6, dw = Math.hypot(A.x - B.x, A.z - B.z) + 1e-6;
      const s = Math.max(0.05, Math.min(25, (a.distanceTo(b) + 1e-6) / (A.distanceTo(B) + 1e-6)));
      const yaw = Math.atan2(A.x - B.x, A.z - B.z) - Math.atan2(a.x - b.x, a.z - b.z);
      void dl; void dw;
      const midL = a.clone().add(b).multiplyScalar(0.5), midW = A.clone().add(B).multiplyScalar(0.5);
      const off = midL.multiplyScalar(1 / s).applyQuaternion(new THREE.Quaternion().setFromAxisAngle(UP, yaw));
      setDolly(midW.sub(off), yaw, s);
    }
  }

  function sticks(dt) {
    for (const hs of VR.hands) {
      const gp = hs.src && hs.src.gamepad;
      if (!gp) continue;
      const ax = gp.axes.length >= 4 ? [gp.axes[2], gp.axes[3]] : [gp.axes[0] || 0, gp.axes[1] || 0];
      const btn = gp.buttons.map((b) => b.pressed);
      /* Y (left) or B (right): the panel */
      if (btn[5] && !hs.prevBtn[5]) togglePanel();
      hs.prevBtn = btn;
      if (hs.handed === 'left') {
        if (Math.hypot(ax[0], ax[1]) > 0.15) {
          const q = new THREE.Quaternion(); camera.updateMatrixWorld(true); camera.getWorldQuaternion(q);
          const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(q); fwd.y = 0; fwd.normalize();
          const right = new THREE.Vector3().crossVectors(fwd, UP);
          const speed = 2.2 * Math.max(0.3, 1 / VR.S) * dt;
          dolly.position.addScaledVector(fwd, -ax[1] * speed).addScaledVector(right, ax[0] * speed);
        }
      } else if (hs.handed === 'right') {
        const turnBy = (a) => { const h = headWorld(); const p = dolly.position.clone().sub(h).applyAxisAngle(UP, a).add(h); setDolly(p, VR.yaw + a, VR.S); };
        if (VR.smoothTurn) { if (Math.abs(ax[0]) > 0.2) turnBy(-ax[0] * 1.6 * dt); }
        else if (Math.abs(ax[0]) > 0.7 && !hs.turned) { hs.turned = true; turnBy(-Math.sign(ax[0]) * Math.PI / 6); }
        else if (Math.abs(ax[0]) < 0.3) hs.turned = false;
        if (Math.abs(ax[1]) > 0.3) dolly.position.y += -ax[1] * 1.4 * Math.max(0.3, 1 / VR.S) * dt;
      }
    }
  }

  /* ---------- session ---------- */
  const saved = {};
  function enterWorkshop() {
    ws.adopt('system', V.system); ws.tableSlot.add(V.system); V.system.position.set(0, 0, 0); V.system.scale.setScalar(1); V.system.visible = true;
    ws.adopt('planet', V.planet.group); ws.cradleSlot.add(V.planet.group); V.planet.group.position.set(0, 0, 0); V.planet.group.scale.setScalar(1);
    saved.planetBayVis = V.planetBay.visible; V.planetBay.visible = false;
    saved.quality = V.state.quality; V.setQuality('low', false);
    saved.cut = V.state.cut;
    ws.root.visible = true;
    /* the orrery's own light is tuned for the desktop scale */
    V.system.traverse((o) => { if (o.isPointLight) { saved.oLight = saved.oLight || [o, o.intensity, o.decay]; o.intensity = 1.2; o.decay = 1; o.distance = 2.2; } });
  }
  function exitWorkshop() {
    ws.release('system', V.system);
    ws.release('planet', V.planet.group);
    V.planetBay.visible = saved.planetBayVis;
    if (saved.oLight) { saved.oLight[0].intensity = saved.oLight[1]; saved.oLight[0].decay = saved.oLight[2]; saved.oLight = null; }
    V.setQuality(saved.quality, false);
    ws.root.visible = false;
    for (const l of beadLabels) l.visible = false;
  }

  async function enter() {
    if (VR.session || !navigator.xr) return;
    const session = await navigator.xr.requestSession('immersive-vr', { optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking'] });
    saved.cam = { parent: camera.parent, pos: camera.position.clone(), quat: camera.quaternion.clone(), fov: camera.fov, near: camera.near, far: camera.far,
      target: V.controls.target.clone() };
    V.controls.enabled = false;
    camera.clearViewOffset();
    dolly.add(camera);
    camera.position.set(0, 1.6, 0); camera.quaternion.identity();
    camera.near = 0.02; camera.far = 600; camera.updateProjectionMatrix();
    setDolly(LAYOUT.start.clone(), 0, 1);
    enterWorkshop();
    renderer.xr.setReferenceSpaceType('local-floor');
    await renderer.xr.setSession(session);
    VR.session = session;
    VR.frames = 0;
    session.addEventListener('end', onEnd);
    togglePanel(true);
    syncDials(); ws.drawBoard(S); syncBeadLabels();
    renderer.setAnimationLoop(xrFrame);
  }
  function onEnd() {
    renderer.setAnimationLoop(null);
    VR.session = null;
    togglePanel(false);
    exitWorkshop();
    /* XR overwrote the camera's pose and field of view: restore the desktop camera */
    scene.add(camera);
    const c = saved.cam;
    camera.position.copy(c.pos); camera.quaternion.copy(c.quat); camera.fov = c.fov; camera.near = c.near; camera.far = c.far;
    camera.updateProjectionMatrix();
    V.controls.target.copy(c.target); V.controls.enabled = true; V.controls.update();
    V.onExit();
  }

  const sunDir = new THREE.Vector3();
  function xrFrame(time) {
    const tsec = time / 1000;
    const dt = Math.min(0.1, Math.max(0, tsec - (VR.lastT || tsec))); VR.lastT = tsec;
    VR.frames++;
    sticks(dt);
    for (const hs of VR.hands) {
      hs.line.visible = !!hs.src;
      if (!hs.src) { hs.dot.visible = false; continue; }
      if (hs.mode === 'ui') { const { o, d } = rayOf(hs); const pp = panelPx(o, d); if (pp) panel.move(pp.px); }
      else if (hs.mode === 'dial') dragDial(hs);
      else if (hs.mode === 'sun') { const { o, d } = rayOf(hs); ws.forgeGroup.position.copy(ws.root.worldToLocal(o.clone().addScaledVector(d, hs.sunDist))); }
      else if (hs.mode === 'bead' || hs.mode === 'handle') dragOrrery(hs);
      else updateHover(hs);
    }
    worldGrab();
    panel.tick(performance.now());
    /* the scene */
    V.star.tick(tsec); ws.forge.tick(tsec);
    /* the planets stand still while a ray rests on one (so they can be caught) or one is being dragged */
    if (!VR.hands.some((h) => h.mode === 'bead' || h.mode === 'handle' || (h.hover && (h.hover.kind === 'bead' || h.hover.kind === 'handle' || h.hover.kind === 'slot')))) V.orbitAdvance(dt);
    V.orrery.tick(V.orbitClock(), 1);
    V.orrery.beads.forEach((b, k) => { const l = beadLabels[k]; if (!l) return; l.visible = true; b.mesh.getWorldPosition(l.position); l.position.y += 0.07; });
    ws.cradleSlot.updateMatrixWorld(true);
    ws.forgeGroup.updateMatrixWorld(true);
    sunDir.copy(ws.forgeGroup.getWorldPosition(new THREE.Vector3())).sub(ws.cradleSlot.getWorldPosition(new THREE.Vector3())).normalize();
    const xrCam = renderer.xr.getCamera();
    V.state.chunks = V.planet.tick(tsec, xrCam, sunDir, V.state.spinOn, 0.0011 * VR.S);
    V.planet.bakeStep(renderer);
    V.sky.follow(xrCam, 1);
    renderer.render(scene, camera);
  }

  if (navigator.xr) navigator.xr.isSessionSupported('immersive-vr').then((ok) => { VR.supported.vr = !!ok; V.onSupport(!!ok); }).catch(() => {});

  return { VR, enter, togglePanel, panel, ws, syncDials, onDesignChanged() { if (!VR.session) return; syncDials(); ws.drawBoard(S); syncBeadLabels(); panel.markDirty(); },
    /* for tests: a panel widget's position in dolly-local space */
    widgetLocal(prefix) { const px = panel.widgetPx(prefix); if (!px) return null; panel.mesh.updateMatrix();
      return new THREE.Vector3((px[0] / panel.W - 0.5) * panel.PW, (0.5 - px[1] / panel.H) * panel.PH, 0).applyMatrix4(panel.mesh.matrix).toArray(); },
    dialLocal(which) { const d = which === 'age' ? ws.ageDial : ws.massDial; d.updateMatrixWorld(true); dolly.updateMatrixWorld(true);
      return dolly.worldToLocal(new THREE.Vector3().setFromMatrixPosition(d.matrixWorld)).toArray(); },
  };
}
