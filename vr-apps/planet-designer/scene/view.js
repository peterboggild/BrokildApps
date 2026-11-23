/* The desktop 3D view (M1 + M2): renderer with HDR and bloom, the Orrery (system) and the Planet bay, camera
 * control, dragging on the orrery, labels beside the objects (never over them), the cutaway's layer labels and
 * a scale note. The VR rig (M5) will attach to the same scene. */
import * as THREE from 'three';
import { OrbitControls } from '../vendor/controls/OrbitControls.js';
import { EffectComposer } from '../vendor/postprocessing/EffectComposer.js';
import { RenderPass } from '../vendor/postprocessing/RenderPass.js';
import { UnrealBloomPass } from '../vendor/postprocessing/UnrealBloomPass.js';
import { OutputPass } from '../vendor/postprocessing/OutputPass.js';
import { createSky } from './sky.js?v=8';
import { createStar } from './star.js?v=8';
import { createOrrery, rOf } from './orrery.js?v=8';
import { createPlanet } from './planet.js?v=8';
import { t, fmt } from '../i18n.js?v=8';
import { createCausality } from '../ui/causality.js?v=8';
import { createXR } from './xr.js?v=8';

const el = (tag, cls, txt) => { const e = document.createElement(tag); if (cls) e.className = cls; if (txt !== undefined) e.textContent = txt; return e; };

export function createView(container, S) {
  const renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
  /* Quest 3 (and other headset/phone browsers): pixel ratio 1 and the light graphics setting by default */
  const LIGHT_DEVICE = /OculusBrowser|Quest|Android|iPhone|iPad/i.test(navigator.userAgent);
  renderer.setPixelRatio(LIGHT_DEVICE ? 1 : Math.min(window.devicePixelRatio || 1, 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.0;
  container.appendChild(renderer.domElement);
  /* If the graphics go down (WebGL context lost: on a headset usually an overloaded GPU), say so plainly on the page,
   * instead of leaving a frozen picture. The browser may refuse WebGL for the site until it (or the headset) restarts. */
  const errBox = el('div', 'v-error');
  errBox.hidden = true;
  container.appendChild(errBox);
  function showError(msg) { errBox.hidden = false; errBox.textContent = msg; }
  renderer.domElement.addEventListener('webglcontextlost', (e) => { e.preventDefault(); showError(t('err.lost')); });
  renderer.domElement.addEventListener('webglcontextrestored', () => { errBox.hidden = true; });

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(45, 1, 0.01, 500);
  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;

  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));
  const bloom = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.45, 0.45, 1.8);
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  const sky = createSky();
  scene.add(sky.group);

  /* --- System bay --- */
  const system = new THREE.Group();
  const star = createStar();
  system.add(star.group);
  const orrery = createOrrery();
  system.add(orrery.group);
  scene.add(system);

  /* --- Planet bay --- */
  const planetBay = new THREE.Group();
  const planet = createPlanet();
  planetBay.add(planet.group);
  const sun = createStar();                 /* the star as seen from the planet, drawn behind everything */
  sun.group.traverse((o) => { if (o.material) o.material.depthWrite = false; o.renderOrder = -8; });
  sun.uniforms.uGain.value = 9;
  planetBay.add(sun.group);
  scene.add(planetBay);

  /* --- Overlay: HUD, labels, leader lines --- */
  const overlay = el('div', 'v-overlay');
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.classList.add('v-lines');
  overlay.appendChild(svg);
  const labels = el('div', 'v-labels');
  overlay.appendChild(labels);
  const hud = el('div', 'v-hud');
  const info = el('div', 'v-info');
  const note = el('div', 'v-note');
  overlay.append(hud, info, note);
  container.appendChild(overlay);
  /* the Causality Map, as a panel over the view */
  const cmBox = el('div', 'v-cmap');
  cmBox.hidden = true;
  const cmClose = el('button', 'v-btn v-cmclose', '×');
  const cmBody = el('div', 'v-cmbody');
  cmBox.append(cmClose, cmBody);
  container.appendChild(cmBox);
  const cmap = createCausality(cmBody, S);
  cmClose.onclick = () => { cmBox.hidden = true; buildHud(); };
  window.addEventListener('keydown', (ev) => { if (ev.key === 'Escape' && !cmBox.hidden) { cmBox.hidden = true; buildHud(); } });
  function showCausality(nodeId) {
    cmBox.hidden = false;
    if (nodeId !== undefined) cmap.select(nodeId); else cmap.render();
    buildHud();
  }

  let q0 = new URLSearchParams(location.search).get('q');
  try { q0 = q0 || localStorage.getItem('planetDesigner.quality'); } catch (e) { /* ignore */ }
  const state = { mode: 'system', cut: false, spinOn: true, exag: 10, frames: 0, chunks: 0,
    quality: ['high', 'medium', 'low'].includes(q0) ? q0 : LIGHT_DEVICE ? 'low' : 'high', autoQ: true, slow: 0 };
  planet.setQuality(state.quality);
  function setQuality(q, manual) {
    state.quality = q; planet.setQuality(q);
    if (manual) { state.autoQ = false; try { localStorage.setItem('planetDesigner.quality', q); } catch (e) { /* ignore */ } }
    buildHud();
  }
  const camStore = {
    system: { pos: new THREE.Vector3(0, 38, 44), target: new THREE.Vector3() },
    planet: { pos: new THREE.Vector3(1.6, 0.9, 2.6), target: new THREE.Vector3() },
  };

  let vrOK = false, xr = null;
  function buildHud() {
    hud.innerHTML = '';
    const b = (key, on, fn, title) => { const x = el('button', 'v-btn' + (on ? ' on' : ''), t(key)); if (title) x.title = title; x.onclick = fn; hud.appendChild(x); return x; };
    b('v.system', state.mode === 'system', () => setMode('system'), '1');
    b('v.planet', state.mode === 'planet', () => setMode('planet'), '2');
    b('v.cmap', !cmBox.hidden, () => { if (cmBox.hidden) showCausality(); else { cmBox.hidden = true; buildHud(); } }, 'M');
    if (vrOK) { const v = b('v.enterVR', false, () => xr && xr.enter().catch((e) => { console.error(e); showError(t('err.xr') + ' ' + (e && e.message ? e.message : e)); })); v.classList.add('vrbtn'); v.id = 'vrBtn'; }
    if (state.mode === 'planet' && S.derivedPlanet().role === 'hz') {
      b('v.cut', state.cut, () => { state.cut = !state.cut; refresh(); buildHud(); }, 'C');
      b('v.spin', state.spinOn, () => { state.spinOn = !state.spinOn; buildHud(); });
      const qs = el('select', 'v-btn');
      for (const q of ['high', 'medium', 'low']) { const o = el('option', '', t('v.q.' + q)); o.value = q; if (q === state.quality) o.selected = true; qs.appendChild(o); }
      qs.onchange = () => setQuality(qs.value, true);
      hud.appendChild(qs);
      const wrap = el('label', 'v-ex');
      wrap.append(el('span', '', t('v.exag')));
      const r = el('input'); r.type = 'range'; r.min = 1; r.max = 40; r.step = 1; r.value = state.exag;
      const v = el('span', 'v-exv', '×' + state.exag);
      r.oninput = () => { state.exag = +r.value; v.textContent = '×' + state.exag; refresh(); };
      wrap.append(r, v);
      hud.appendChild(wrap);
    }
  }

  let started = false;
  function setMode(m) {
    if (started) { camStore[state.mode].pos.copy(camera.position); camStore[state.mode].target.copy(controls.target); }
    started = true;
    state.mode = m;
    camera.position.copy(camStore[m].pos);
    controls.target.copy(camStore[m].target);
    system.visible = m === 'system';
    planetBay.visible = m === 'planet';
    controls.minDistance = m === 'system' ? 3 : 1.0005;
    controls.maxDistance = m === 'system' ? 160 : 12;
    refresh();
    buildHud();
  }

  /* Re-read the design into the scene (called after every change). */
  function refresh() {
    const D = S.derived;
    if (xr) xr.onDesignChanged();
    if (!cmBox.hidden) cmap.render();
    star.set(D.star); sun.set(D.star);
    const R = 1.0 + 0.5 * Math.sqrt(Math.min(D.star.radius, 4));
    star.group.scale.setScalar(R);
    orrery.update(S.design, D);
    const P = S.derivedPlanet();
    planet.set(P, S.design, D, { exag: state.exag, cut: state.cut });
    writeInfo();
  }

  function writeInfo() {
    const D = S.derived, P = S.derivedPlanet();
    info.innerHTML = '';
    const line = (k, v) => { const d = el('div', 'v-row'); d.append(el('span', 'v-k', k), el('span', 'v-v', v)); info.appendChild(d); };
    if (state.mode === 'system') {
      info.appendChild(el('div', 'v-title', `${D.star.spt} · ${fmt(D.star.teff, 3)} K`));
      line(t('r.hzCons'), `${fmt(D.star.hz.d.runaway)}–${fmt(D.star.hz.d.maxGreenhouse)} ${t('u.AU')}`);
      note.textContent = t('v.noteSystem');
      return;
    }
    info.appendChild(el('div', 'v-title', P.name));
    line(t('r.radiusP'), `${fmt(P.radiusEarth * 6371, 3)} ${t('u.km')}`);
    if (P.role === 'hz') {
      line(t('r.g'), `${fmt(P.gEarth, 3)} g`);
      line(t('r.vesc'), `${fmt(P.vesc / 1e3, 3)} ${t('u.kms')}`);
      line(t('r.land'), `${fmt(P.landFrac * 100, 3)} %`);
      if (P.oceans > 0) line(t('r.highest'), `${fmt(P.highestKm, 3)} ${t('u.km')}`);
      else line(t('r.range'), `${fmt((P.sea.maxH - P.sea.minH) * P.reliefM / 1e3, 3)} ${t('u.km')}`);
      if (P.oceans > 0) line(t('r.oceanDepth'), `${fmt(P.oceanDepthKm, 3)} ${t('u.km')}`);
      note.textContent = t('v.notePlanet', { x: String(state.exag) }) + (P.climate.valid ? '' : ' ' + t('v.noteClimate'));
    } else {
      note.textContent = t('v.noteOther');
    }
  }

  /* --- Labels --- */
  const v3 = new THREE.Vector3();
  function toScreen(p) {
    v3.copy(p).project(camera);
    const w = container.clientWidth, h = container.clientHeight;
    return { x: (v3.x * 0.5 + 0.5) * w, y: (-v3.y * 0.5 + 0.5) * h, behind: v3.z > 1 };
  }
  let labelEls = [];
  function setLabels(items, lines) {
    while (labelEls.length < items.length) { const d = el('div', 'v-label'); labels.appendChild(d); labelEls.push(d); }
    labelEls.forEach((d, i) => {
      const it = items[i];
      d.hidden = !it;
      if (!it) return;
      if (d.textContent !== it.text) d.textContent = it.text;
      d.style.transform = `translate(${it.x}px, ${it.y}px)`;
      d.className = 'v-label' + (it.cls ? ' ' + it.cls : '');
    });
    svg.innerHTML = (lines || []).map((l) => `<line x1="${l[0]}" y1="${l[1]}" x2="${l[2]}" y2="${l[3]}" />`).join('');
  }
  function updateLabels() {
    if (state.mode === 'system') {
      const items = orrery.beads.map((b) => {
        const s = toScreen(b.mesh.getWorldPosition(new THREE.Vector3()));
        if (s.behind) return null;
        return { x: s.x + 12, y: s.y - 8, text: S.design.planets[b.index].name, cls: b.index === S.sel ? 'sel' : '' };
      });
      if (orrery.slot) {
        const s = toScreen(orrery.slot.getWorldPosition(new THREE.Vector3()));
        items.push({ x: s.x + 14, y: s.y - 8, text: t('v.slot'), cls: 'slot' });
      }
      setLabels(items, []);
      return;
    }
    const P = S.derivedPlanet();
    if (!state.cut || P.role !== 'hz' || !planet.info.layers) { setLabels([], []); return; }
    /* layer labels to the right of the planet, leader lines to the middle of each layer on the cut face z = 0 */
    const L = planet.info.layers, Rk = L.radiusKm;
    const mids = [];
    if (L.innerCoreKm > 0) mids.push([L.innerCoreKm / 2, t('layer.inner', { km: Math.round(L.innerCoreKm) })]);
    mids.push([(L.innerCoreKm + L.coreKm) / 2, t('layer.outer', { km: Math.round(L.coreKm - L.innerCoreKm) })]);
    mids.push([L.coreKm + L.mantleKm / 2, t('layer.mantle', { km: Math.round(L.mantleKm) })]);
    mids.push([L.coreKm + L.mantleKm + L.crustKm / 2, t('layer.crust', { km: Math.round(L.crustKm) })]);
    const centre = toScreen(planet.group.getWorldPosition(new THREE.Vector3()));
    const edge = toScreen(new THREE.Vector3(0, 1, 0).applyMatrix4(planet.group.matrixWorld));
    const rad = Math.hypot(edge.x - centre.x, edge.y - centre.y) || 100;
    const right = true;
    const colX = right ? Math.min(container.clientWidth - 205, centre.x + rad + 24) : Math.max(8, centre.x - rad - 30);
    const items = [], lines = [];
    mids.forEach(([km, text], i) => {
      const ang = 0.55;
      const p = new THREE.Vector3(Math.cos(ang) * km / Rk, Math.sin(ang) * km / Rk, 0).applyMatrix4(planet.spin.matrixWorld);
      const s = toScreen(p);
      const y = centre.y - rad * 0.6 + i * 46;
      items.push({ x: colX, y: y - 9, text, cls: right ? 'layer' : 'layer left' });
      lines.push([s.x, s.y, right ? colX - 4 : colX + 4, y]);
    });
    setLabels(items, lines);
  }

  /* --- Dragging on the orrery --- */
  const ray = new THREE.Raycaster();
  const ndc = new THREE.Vector2();
  const table = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
  let drag = null;
  function pick(ev) {
    const r = renderer.domElement.getBoundingClientRect();
    ndc.set(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
    ray.setFromCamera(ndc, camera);
  }
  renderer.domElement.addEventListener('pointerdown', (ev) => {
    if (state.mode !== 'system' || ev.button !== 0) return;
    pick(ev);
    const hit = ray.intersectObjects(orrery.pickables(), false)[0];
    if (!hit) return;
    const u = hit.object.userData;
    if (u.kind === 'slot') { S.addHzAt(u.a); return; }
    S.sel = u.index; S.tab = 'orbit'; S.redraw();
    drag = u;
    controls.enabled = false;
    renderer.domElement.setPointerCapture(ev.pointerId);
  });
  let hover = false;
  renderer.domElement.addEventListener('pointermove', (ev) => {
    if (!drag && state.mode === 'system') {
      pick(ev);
      const h = ray.intersectObjects(orrery.pickables(), false)[0];
      hover = !!h;
      renderer.domElement.style.cursor = h ? (h.object.userData.kind === 'slot' ? 'pointer' : 'grab') : '';
    }
    if (!drag) return;
    pick(ev);
    const p = new THREE.Vector3();
    if (!ray.ray.intersectPlane(table, p)) return;
    if (drag.kind === 'bead') S.set('planet.orbit.aAU', orrery.toA(p));
    else if (drag.kind === 'handle') {
      const a = S.planet().orbit.aAU;
      S.set('planet.orbit.e', orrery.toE(p, a));
      S.set('planet.orbit.argPeriDeg', orrery.toArgPeri(p));
    }
  });
  const endDrag = () => { if (drag) { drag = null; controls.enabled = true; S.redraw(); } };
  renderer.domElement.addEventListener('pointerup', endDrag);
  renderer.domElement.addEventListener('pointercancel', endDrag);
  renderer.domElement.addEventListener('dblclick', (ev) => {
    if (state.mode !== 'system') return;
    pick(ev);
    const hit = ray.intersectObjects(orrery.pickables(), false)[0];
    if (hit && hit.object.userData.kind === 'bead') { S.sel = hit.object.userData.index; S.redraw(); setMode('planet'); }
  });
  window.addEventListener('keydown', (ev) => {
    if (ev.target.tagName === 'INPUT' || ev.target.tagName === 'SELECT') return;
    if (ev.key === '1') setMode('system');
    if (ev.key === '2') setMode('planet');
    if (ev.key === 'm' || ev.key === 'M') { if (cmBox.hidden) showCausality(); else { cmBox.hidden = true; buildHud(); } }
    if (ev.key === 'c' || ev.key === 'C') { if (state.mode === 'planet') { state.cut = !state.cut; refresh(); buildHud(); } }
  });

  /* --- Size and loop --- */
  function resize() {
    if (renderer.xr.isPresenting) return;            /* never resize while presenting (pitfall from Illusion Studio) */
    const w = Math.max(10, container.clientWidth), h = Math.max(10, container.clientHeight);
    renderer.setSize(w, h, false);
    composer.setSize(w, h);
    bloom.resolution.set(w, h);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    svg.setAttribute('viewBox', `0 0 ${w} ${h}`);
  }
  new ResizeObserver(resize).observe(container);

  const clock = new THREE.Clock();
  let lastT = 0, orbitClock = 0;
  const sunDir = new THREE.Vector3(1, 0.18, 0.55).normalize();
  function frame() {
    requestAnimationFrame(frame);
    if (document.hidden || renderer.xr.isPresenting) return;   /* while in VR only the XR loop renders */
    const time = clock.getElapsedTime();
    const rawDt = time - lastT;
    const dt = Math.min(0.1, rawDt); lastT = time;
    /* automatic quality: step down after a run of slow frames (under ~25 fps) in the planet view */
    if (state.mode === 'planet' && state.autoQ && !navigator.webdriver) {
      state.slow = rawDt > 0.04 ? state.slow + 1 : Math.max(0, state.slow - 1);
      if (state.slow > 45 && state.quality !== 'low') { setQuality(state.quality === 'high' ? 'medium' : 'low', false); state.slow = 0; }
    }
    controls.update();
    const d = camera.position.distanceTo(controls.target);
    if (state.mode === 'system') {
      camera.near = 0.05; camera.far = 600;
      controls.rotateSpeed = 0.8; controls.zoomSpeed = 1.0;
      star.tick(time);
      if (!hover && !drag) orbitClock += dt;
      orrery.tick(orbitClock, 1);
    } else {
      const alt = Math.max(d - 1, 1e-5);
      camera.near = Math.max(2e-6, alt * 0.25); camera.far = d + 3;
      controls.rotateSpeed = Math.min(0.8, Math.max(0.02, alt * 1.2));
      controls.zoomSpeed = 0.6;
      const pxAngle = 2 * Math.tan(THREE.MathUtils.degToRad(camera.fov) / 2) / Math.max(1, container.clientHeight * renderer.getPixelRatio());
      state.chunks = planet.tick(time, camera, sunDir, state.spinOn, pxAngle);
      planet.bakeStep(renderer);
      /* the star in the sky, at its apparent size (at least 0.35 deg so it stays visible) */
      const P = S.derivedPlanet();
      const angR = Math.max(Math.atan(S.derived.star.radius * 6.957e8 / (P.aAU * 1.496e11)), 0.35 * Math.PI / 360);
      const Rs = camera.far * 0.9;
      sun.group.position.copy(camera.position).addScaledVector(sunDir, Rs);
      sun.group.scale.setScalar(Rs * Math.tan(angR));
      sun.tick(time);
    }
    /* in the cutaway, shift the picture left so the layer labels fit beside the planet */
    const w = container.clientWidth, h = container.clientHeight;
    const shift = state.mode === 'planet' && state.cut ? Math.min(0.24, 300 / w) * w : 0;
    if (shift) camera.setViewOffset(w, h, shift, 0, w, h); else camera.clearViewOffset();
    camera.updateProjectionMatrix();
    sky.follow(camera, renderer.getPixelRatio());
    composer.render();
    updateLabels();
    state.frames++;
  }

  resize();
  setMode('system');
  try {
    xr = createXR({ renderer, scene, camera, controls, S, system, planetBay, planet, orrery, star, sky, state,
      setQuality: (q, manual) => setQuality(q, manual),
      orbitAdvance: (dt) => { orbitClock += dt; }, orbitClock: () => orbitClock,
      onExit: () => { setMode(state.mode); refresh(); buildHud(); },
      onSupport: (ok) => { vrOK = ok; buildHud(); } });
  } catch (e) { console.error(e); }
  requestAnimationFrame(frame);

  return { refresh: () => { refresh(); buildHud(); }, setMode, setQuality, showCausality, cmap, state, get xr() { return xr; }, camera, controls, renderer, orrery, planet, bloom, scene, star, sky,
    toScreen, get mode() { return state.mode; } };
}
