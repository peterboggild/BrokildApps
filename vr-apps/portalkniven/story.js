/*
 * Portalkniven — the story (chapter 1).
 *
 *   You start in your own room with empty hands. Something hums. The hum comes from a letter.
 *   The letter (from yourself, a little later) explains the loop: a knife lies in this room that nobody has made yet.
 *   Only when the letter has been read does the knife show itself; the hum moves to it. Touch it and it is yours.
 *   Cut a door. In every other version of the room three parts of a time machine are hidden, nine in all.
 *   They only show once the door has been closed behind you (a cut across the ring). Touch a part to take it;
 *   it rides on a belt at your hip. When the first world's three are found, the thumbstick starts choosing worlds.
 *
 * Chapter 2 (assembling the machine by hand, the past, making the knife) is not built yet.
 *
 * Everything is placed on top of whatever the headset's room scan has there (a table, a sofa, the floor), so it can
 * be reached; with no scan, things hover at hand height.
 */
import * as THREE from './vendor/three.module.min.js';

const V = THREE.Vector3;
const TAU = Math.PI * 2;
const rnd = (a, b) => a + Math.random() * (b - a);
const clamp = (v, a, b) => (v < a ? a : v > b ? b : v);

export const PARTS = [
  { name: 'Fluxspole', world: 1 }, { name: 'Vakuumrør', world: 1 }, { name: 'Tandhjulssæt', world: 1 },
  { name: 'Tidskrystal', world: 2 }, { name: 'Kronometer', world: 2 }, { name: 'Kondensator', world: 2 },
  { name: 'Antenne', world: 3 }, { name: 'Styrehåndtag', world: 3 }, { name: 'Kraftkerne', world: 3 },
];

const LETTER = [
  'Kære mig.',
  '',
  'Hvis du læser dette, er det sket igen. Der er gået en knude på tiden, et rumtidsloop, og den strammer.',
  '',
  'Et sted i dette rum ligger en kniv. Den burde ikke findes. Ingen har lavet den endnu. Det er dig, der skal lave den. Men først skal du bruge den.',
  '',
  'Skær en dør i luften. Bag den ligger andre udgaver af stuen. I hver af dem er der gemt tre dele af en tidsmaskine. Ni i alt.',
  '',
  'Når du er gået igennem en dør: luk den bag dig. Skær hen over ringen. Delene viser sig først, når døren er lukket. (Ja, jeg ved godt, det er ubehageligt. Gør det alligevel.)',
  '',
  'Saml maskinen her i stuen. Rejs tilbage. Lav kniven. Læg den, hvor du fandt den.',
  '',
  'Gør du det ikke, har kniven aldrig ligget her, og så har du aldrig læst dette brev. Og så bliver det en rigtig dårlig morgen. For alle.',
  '',
  'Kærlig hilsen',
  'Dig (lidt senere)',
  '',
  'P.S. Følg lyden.',
];

export function createStory(c) {
  const { scene, S, head, headQ, worlds } = c;
  const st = { nextT: -1, nextW: 0, nextFrom: null, letterBelt: false, searchT: 0, hintSaid: false, letterOn: false, lyingOn: false, machOn: false, ringOn: false, homeHint: false, warned: false, busy: false, powered: false, powerT: 0, machT: 0, made: false, arrived: false, jumpT: 0, endT: 0, on: false, phase: 'off', hasKnife: false, stick: false, reading: false, readT: 0, t: 0, nagT: 0, hintT: 0, found: PARTS.map(() => false), revealed: { 1: false, 2: false, 3: false }, doneSaid: false };
  const root = new THREE.Group(); scene.add(root); root.visible = false;

  /* ---------------------------------------------------------------- materials */
  const M = {
    brass: new THREE.MeshStandardMaterial({ color: 0xd2a84e, metalness: 0.95, roughness: 0.3 }),
    copper: new THREE.MeshStandardMaterial({ color: 0xc8713f, metalness: 0.95, roughness: 0.32 }),
    steel: new THREE.MeshStandardMaterial({ color: 0xb9c2c9, metalness: 0.95, roughness: 0.28 }),
    dark: new THREE.MeshStandardMaterial({ color: 0x15171b, metalness: 0.3, roughness: 0.6 }),
    glass: new THREE.MeshStandardMaterial({ color: 0xcfe8ff, metalness: 0, roughness: 0.05, transparent: true, opacity: 0.28, depthWrite: false }),
    cyan: new THREE.MeshStandardMaterial({ color: 0x062a2a, emissive: 0x35ffe0, emissiveIntensity: 1.6, roughness: 0.4 }),
    orange: new THREE.MeshStandardMaterial({ color: 0x2a1204, emissive: 0xff8a2a, emissiveIntensity: 1.8, roughness: 0.4 }),
    violet: new THREE.MeshStandardMaterial({ color: 0x1a0830, emissive: 0xb070ff, emissiveIntensity: 1.5, roughness: 0.25 }),
    red: new THREE.MeshStandardMaterial({ color: 0xb01818, metalness: 0.1, roughness: 0.35 }),
    ivory: new THREE.MeshStandardMaterial({ color: 0xf1ead8, metalness: 0, roughness: 0.6 }),
  };
  const mesh = (g, m, x = 0, y = 0, z = 0) => { const o = new THREE.Mesh(g, m); o.position.set(x, y, z); return o; };
  function glowTex() {
    const cv = document.createElement('canvas'); cv.width = cv.height = 128; const x = cv.getContext('2d');
    const g = x.createRadialGradient(64, 64, 0, 64, 64, 64); g.addColorStop(0, 'rgba(255,255,255,.9)'); g.addColorStop(0.3, 'rgba(255,255,255,.3)'); g.addColorStop(1, 'rgba(255,255,255,0)');
    x.fillStyle = g; x.fillRect(0, 0, 128, 128); return new THREE.CanvasTexture(cv);
  }
  const GT = glowTex();
  const halo = (color, size) => { const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: GT, color, blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, opacity: 0.55 })); s.scale.setScalar(size); return s; };

  /* ---------------------------------------------------------------- the nine parts */
  function buildPart(k) {
    const g = new THREE.Group();
    if (k === 0) {                                                  // flux coil: copper windings round a glowing core ring
      g.add(mesh(new THREE.TorusGeometry(0.05, 0.012, 12, 40), M.cyan));
      for (let i = 0; i < 26; i++) { const a = i / 26 * TAU, w = mesh(new THREE.TorusGeometry(0.017, 0.0032, 6, 14), M.copper, Math.cos(a) * 0.05, Math.sin(a) * 0.05, 0); w.lookAt(Math.cos(a) * 0.05 - Math.sin(a), Math.sin(a) * 0.05 + Math.cos(a), 0); g.add(w); }
      g.add(mesh(new THREE.BoxGeometry(0.03, 0.012, 0.03), M.brass, 0, -0.066, 0));
    } else if (k === 1) {                                           // vacuum tube
      g.add(mesh(new THREE.CylinderGeometry(0.026, 0.026, 0.09, 20, 1, true), M.glass, 0, 0.02, 0));
      g.add(mesh(new THREE.SphereGeometry(0.026, 20, 10, 0, TAU, 0, Math.PI / 2), M.glass, 0, 0.065, 0));
      g.add(mesh(new THREE.CylinderGeometry(0.03, 0.03, 0.024, 20), M.brass, 0, -0.035, 0));
      g.add(mesh(new THREE.CylinderGeometry(0.004, 0.004, 0.07, 8), M.orange, 0, 0.02, 0));
      g.add(mesh(new THREE.CylinderGeometry(0.014, 0.014, 0.05, 12, 1, true), M.steel, 0, 0.02, 0));
      for (let i = 0; i < 4; i++) g.add(mesh(new THREE.CylinderGeometry(0.0025, 0.0025, 0.02, 6), M.steel, Math.cos(i * TAU / 4) * 0.016, -0.056, Math.sin(i * TAU / 4) * 0.016));
    } else if (k === 2) {                                           // a pair of gears
      const gear = (r, n, m) => { const q = new THREE.Group(); q.add(mesh(new THREE.CylinderGeometry(r, r, 0.012, 28), m)); q.add(mesh(new THREE.CylinderGeometry(r * 0.28, r * 0.28, 0.02, 14), M.steel));
        for (let i = 0; i < n; i++) { const a = i / n * TAU, t = mesh(new THREE.BoxGeometry(r * 0.22, 0.012, r * 0.2), m, Math.cos(a) * r * 1.07, 0, Math.sin(a) * r * 1.07); t.rotation.y = -a; q.add(t); } return q; };
      const a = gear(0.042, 14, M.brass), b = gear(0.026, 9, M.copper); a.rotation.x = Math.PI / 2; b.rotation.x = Math.PI / 2; b.position.set(0.066, 0.018, 0.004); g.add(a, b); g.userData.spin = [a, b];
    } else if (k === 3) {                                           // time crystal in a brass claw
      const cr = mesh(new THREE.OctahedronGeometry(0.036, 0), M.violet); cr.scale.y = 1.7; g.add(cr);
      for (let i = 0; i < 3; i++) { const a = i / 3 * TAU, cl = mesh(new THREE.CylinderGeometry(0.003, 0.005, 0.06, 6), M.brass, Math.cos(a) * 0.026, -0.03, Math.sin(a) * 0.026); cl.rotation.set(Math.sin(a) * 0.5, 0, -Math.cos(a) * 0.5); g.add(cl); }
      g.add(mesh(new THREE.CylinderGeometry(0.022, 0.03, 0.016, 16), M.brass, 0, -0.066, 0));
    } else if (k === 4) {                                           // chronometer: a brass dial with a face and a hand
      const cv = document.createElement('canvas'); cv.width = cv.height = 256; const x = cv.getContext('2d'); x.fillStyle = '#f3ecd8'; x.fillRect(0, 0, 256, 256); x.strokeStyle = '#222'; x.translate(128, 128);
      for (let i = 0; i < 60; i++) { x.lineWidth = i % 5 ? 2 : 5; x.beginPath(); x.moveTo(0, -112); x.lineTo(0, i % 5 ? -100 : -88); x.stroke(); x.rotate(TAU / 60); }
      x.font = 'bold 30px serif'; x.fillStyle = '#222'; x.textAlign = 'center'; x.fillText('t', 0, 50);
      const face = new THREE.MeshStandardMaterial({ map: new THREE.CanvasTexture(cv), roughness: 0.6 }); face.map.colorSpace = THREE.SRGBColorSpace;
      const body = mesh(new THREE.CylinderGeometry(0.052, 0.052, 0.024, 36), M.brass); body.rotation.x = Math.PI / 2; g.add(body);
      const f = mesh(new THREE.CircleGeometry(0.046, 36), face, 0, 0, 0.0125); g.add(f);
      const hand = mesh(new THREE.BoxGeometry(0.004, 0.04, 0.002), M.dark, 0, 0, 0.0145); hand.geometry.translate(0, 0.016, 0); g.add(hand); g.userData.hand = hand;
      g.add(mesh(new THREE.TorusGeometry(0.052, 0.005, 8, 36), M.brass, 0, 0, 0.012));
      g.add(mesh(new THREE.CylinderGeometry(0.008, 0.008, 0.016, 10), M.brass, 0, 0.06, 0));
    } else if (k === 5) {                                           // capacitor bank
      g.add(mesh(new THREE.BoxGeometry(0.12, 0.01, 0.05), M.steel, 0, -0.035, 0));
      for (let i = -1; i <= 1; i++) { g.add(mesh(new THREE.CylinderGeometry(0.017, 0.017, 0.06, 18), M.dark, i * 0.038, 0, 0)); g.add(mesh(new THREE.CylinderGeometry(0.0175, 0.0175, 0.006, 18), M.copper, i * 0.038, 0.033, 0)); g.add(mesh(new THREE.CylinderGeometry(0.004, 0.004, 0.014, 8), M.cyan, i * 0.038, 0.042, 0)); }
    } else if (k === 6) {                                           // antenna: a small dish and an emitter
      const pts = []; for (let i = 0; i <= 10; i++) { const t = i / 10; pts.push(new THREE.Vector2(t * 0.055, t * t * 0.03)); }
      const dish = mesh(new THREE.LatheGeometry(pts, 28), M.steel); dish.material = M.steel.clone(); dish.material.side = THREE.DoubleSide; dish.rotation.x = -0.9; g.add(dish);
      const rod = mesh(new THREE.CylinderGeometry(0.0025, 0.0025, 0.06, 6), M.brass, 0, 0.022, 0.026); rod.rotation.x = 0.67; g.add(rod);
      g.add(mesh(new THREE.SphereGeometry(0.008, 12, 8), M.orange, 0, 0.045, 0.045));
      g.add(mesh(new THREE.CylinderGeometry(0.006, 0.006, 0.06, 8), M.brass, 0, -0.03, 0)); g.add(mesh(new THREE.CylinderGeometry(0.028, 0.032, 0.012, 16), M.dark, 0, -0.064, 0));
    } else if (k === 7) {                                           // control lever
      g.add(mesh(new THREE.BoxGeometry(0.08, 0.03, 0.05), M.dark, 0, -0.04, 0)); g.add(mesh(new THREE.BoxGeometry(0.084, 0.004, 0.054), M.brass, 0, -0.024, 0));
      const lv = new THREE.Group(); lv.position.y = -0.03; lv.rotation.z = -0.45; lv.add(mesh(new THREE.CylinderGeometry(0.0045, 0.0045, 0.09, 8), M.steel, 0, 0.045, 0)); lv.add(mesh(new THREE.SphereGeometry(0.014, 14, 10), M.red, 0, 0.094, 0)); g.add(lv);
      g.add(mesh(new THREE.SphereGeometry(0.005, 8, 6), M.cyan, 0.028, -0.02, 0.016));
    } else {                                                        // power core: a glowing heart in three rings
      g.add(mesh(new THREE.IcosahedronGeometry(0.026, 2), M.cyan));
      const rings = []; for (let i = 0; i < 3; i++) { const r = mesh(new THREE.TorusGeometry(0.048 + i * 0.008, 0.0035, 8, 44), i === 1 ? M.copper : M.brass); r.rotation.set(i * 1.1, i * 0.7, 0); g.add(r); rings.push(r); }
      g.userData.rings = rings;
    }
    return g;
  }
  const parts = PARTS.map((p, k) => { const grp = new THREE.Group(), body = buildPart(k); body.scale.setScalar(1.7); grp.add(body, halo(0x7dffe0, 0.6));
    const beam = mesh(new THREE.CylinderGeometry(0.012, 0.012, 3, 8, 1, true), new THREE.MeshBasicMaterial({ color: 0x7dffe0, transparent: true, opacity: 0.45, blending: THREE.AdditiveBlending, depthWrite: false, depthTest: false }), 0, 1.3, 0); beam.visible = false; grp.add(beam); grp.visible = false; root.add(grp); return { k, ...p, grp, body, found: false, shown: false, placed: false, held: -1, slot: 0, t: 0, pos: new V() }; });

  /* the belt: found parts ride at your hip */
  const belt = new THREE.Group(); root.add(belt); let beltYaw = 0;

  /* ---------------------------------------------------------------- the letter */
  function paper(w, h, draw) {
    const cv = document.createElement('canvas'); cv.width = w; cv.height = h; const x = cv.getContext('2d');
    const g = x.createLinearGradient(0, 0, w, h); g.addColorStop(0, '#efe3c4'); g.addColorStop(1, '#dccba3'); x.fillStyle = g; x.fillRect(0, 0, w, h);
    for (let i = 0; i < 900; i++) { x.fillStyle = `rgba(${90 + Math.random() * 60 | 0},${60 + Math.random() * 40 | 0},20,${Math.random() * 0.05})`; x.fillRect(Math.random() * w, Math.random() * h, Math.random() * 4, Math.random() * 4); }
    x.strokeStyle = 'rgba(90,60,20,.25)'; x.lineWidth = 2; x.beginPath(); x.moveTo(0, h / 3); x.lineTo(w, h / 3); x.moveTo(0, 2 * h / 3); x.lineTo(w, 2 * h / 3); x.stroke();      // fold lines
    draw(x, w, h);
    const t = new THREE.CanvasTexture(cv); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; return t;
  }
  const sheetTex = paper(1024, 1400, (x, w) => {
    x.fillStyle = '#2a1c0c'; x.textBaseline = 'top'; const fs = 41, lh = 54, mx = 70; x.font = `italic ${fs}px Georgia, "Times New Roman", serif`;
    let y = 70;
    for (const para of LETTER) {
      if (!para) { y += lh * 0.5; continue; }
      let line = '';
      for (const word of para.split(' ')) { const t = line ? line + ' ' + word : word; if (x.measureText(t).width > w - 2 * mx) { x.fillText(line, mx, y); y += lh; line = word; } else line = t; }
      x.fillText(line, mx, y); y += lh;
    }
  });
  const foldTex = paper(256, 180, (x, w, h) => { x.fillStyle = '#5a3a14'; x.font = 'italic 34px Georgia, serif'; x.textAlign = 'center'; x.fillText('Til mig', w / 2, h / 2 + 10); });
  const letter = new THREE.Group(); root.add(letter); letter.visible = false;
  const folded = mesh(new THREE.BoxGeometry(0.3, 0.005, 0.21), [0, 1, 2, 3, 4, 5].map(i => new THREE.MeshStandardMaterial({ color: 0xe6d9b6, roughness: 0.9, map: i === 2 ? foldTex : null })));
  letter.add(folded, halo(0xffe9a8, 0.75));
  const sheet = mesh(new THREE.PlaneGeometry(0.66, 0.9), new THREE.MeshBasicMaterial({ map: sheetTex, side: THREE.DoubleSide, transparent: true, depthTest: false, toneMapped: false })); sheet.renderOrder = 150; sheet.visible = false; root.add(sheet);
  const letterPos = new V(), sheetFrom = new V(), sheetTo = new V(), sheetQ = new THREE.Quaternion();

  /* the knife as it lies in the room before it is yours */
  const lying = new THREE.Group(); root.add(lying); st.lyingOn = false;
  const lyingKnife = c.makeKnife(); lyingKnife.rotation.set(0.25, 0, Math.PI / 2 - 0.4); lying.add(lyingKnife, halo(0x60ffa0, 0.75));
  const knifePos = new V();

  /* a line of text that floats in front of you for a few seconds */
  const tc = document.createElement('canvas'); tc.width = 1024; tc.height = 200;
  const tt = new THREE.CanvasTexture(tc); tt.colorSpace = THREE.SRGBColorSpace;
  const toastMesh = mesh(new THREE.PlaneGeometry(0.8, 0.156), new THREE.MeshBasicMaterial({ map: tt, transparent: true, depthTest: false, opacity: 0, toneMapped: false })); toastMesh.renderOrder = 160; root.add(toastMesh);
  let toastT = 0, toastDur = 0; st.toast = '';
  function toast(text, secs = 6) {
    st.toast = text; toastT = 0; toastDur = secs; c.say(text);
    const x = tc.getContext('2d'); x.clearRect(0, 0, 1024, 200);
    x.fillStyle = 'rgba(6,12,10,.78)'; x.beginPath(); x.roundRect(4, 4, 1016, 192, 26); x.fill(); x.strokeStyle = 'rgba(110,255,170,.7)'; x.lineWidth = 4; x.stroke();
    x.fillStyle = '#e9fff1'; x.textAlign = 'center'; x.textBaseline = 'middle';
    let fs = 46; const lines = [];
    for (; fs >= 26; fs -= 4) { x.font = `${fs}px -apple-system, "Segoe UI", Roboto, Helvetica, Arial, sans-serif`; lines.length = 0; let line = ''; for (const w of text.split(' ')) { const t = line ? line + ' ' + w : w; if (x.measureText(t).width > 950) { lines.push(line); line = w; } else line = t; } lines.push(line); if (lines.length * fs * 1.25 <= 170) break; }
    lines.forEach((l, i) => x.fillText(l, 512, 100 + (i - (lines.length - 1) / 2) * fs * 1.25));
    tt.needsUpdate = true;
  }

  /* ---------------------------------------------------------------- sound */
  let hum = null;
  function humAt(p) {
    const ctx = c.audio(); if (!ctx) return;
    if (!hum) {
      const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 0.7; pan.rolloffFactor = 1.6;
      const g = ctx.createGain(); g.gain.value = 0; g.connect(pan); pan.connect(ctx.destination);
      const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 420; lp.connect(g);
      for (const f of [55, 82.6, 110.4]) { const o = ctx.createOscillator(); o.type = 'sawtooth'; o.frequency.value = f; o.connect(lp); o.start(); }
      const sh = ctx.createOscillator(); sh.type = 'sine'; sh.frequency.value = 660; const sg = ctx.createGain(); sg.gain.value = 0.05; sh.connect(sg); sg.connect(g); sh.start();
      const trem = ctx.createOscillator(); trem.frequency.value = 5.3; const tg = ctx.createGain(); tg.gain.value = 0.12; trem.connect(tg); tg.connect(g.gain); trem.start();
      hum = { pan, g, ctx };
    }
    const t = hum.ctx.currentTime;
    if (p) { hum.pan.positionX.setValueAtTime(p.x, t); hum.pan.positionY.setValueAtTime(p.y, t); hum.pan.positionZ.setValueAtTime(p.z, t); hum.g.gain.setTargetAtTime(0.42, t, 0.6); }
    else hum.g.gain.setTargetAtTime(0, t, 0.4);
  }
  function pingAt(p, f = 1180) {
    const ctx = c.audio(); if (!ctx) return; const t = ctx.currentTime;
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 1; pan.rolloffFactor = 1; pan.positionX.value = p.x; pan.positionY.value = p.y; pan.positionZ.value = p.z; pan.connect(ctx.destination);
    for (const [m, v] of [[1, 0.5], [2.0, 0.2]]) { const o = ctx.createOscillator(); o.frequency.value = f * m; const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(v, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 0.9); o.connect(g); g.connect(pan); o.start(t); o.stop(t + 1); }
  }
  function chime(f = 880) {
    const ctx = c.audio(); if (!ctx) return; const t = ctx.currentTime;
    for (const [m, v] of [[1, 0.22], [1.5, 0.12], [2.01, 0.08]]) { const o = ctx.createOscillator(); o.frequency.value = f * m; const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(v, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.4); o.connect(g); g.connect(ctx.destination); o.start(t); o.stop(t + 1.5); }
  }

  /* ---------------------------------------------------------------- placing things on what is really there */
  const rc = new THREE.Raycaster(), DOWN = new V(0, -1, 0), _o = new V();
  /* Looking things up in the scan's grid is a handful of set lookups; a ray against the whole scan is thousands of triangles.
   * So the search uses the grid, and only the spot finally chosen is refined with one ray. */
  const topAt = (x, z) => worlds.columnTop(x, z);
  function topExact(x, z) {
    const g = worlds.W[1].scan; if (!g.children.length) return null;
    rc.set(_o.set(x, 1.9, z), DOWN); rc.far = 2.3;
    const h = rc.intersectObject(g, true); return h.length ? h[0].point.y : null;
  }
  /** Can you see the spot from where your eyes are (nothing scanned in between), and is it clear of what is really there? */
  const _from = new V(), _to = new V(), _dir = new V();
  function clearSight(x, y, z) {
    if (!worlds.hasScan()) return true;
    _to.set(x, y, z);
    return worlds.lineClear(head, _to) && worlds.nearest(_to, 0.1) === Infinity;      // nothing in the way (a wall, a sofa back, a shelf), and not inside or pressed against something
  }
  const hoverAt = y => (y === null || y < 0.45 ? 0.95 : Math.max(0.85, y + 0.18));     // never low: at hand height, or just above a table     // on a table: just above it; on the floor or unknown: at hand height
  /** A spot r metres from (cx, cz) in the direction range: over floor or furniture that is really there (so not beyond a wall),
   *  no higher than a table, in plain sight from where you stand, and clear of the scan. */
  function findSpot(cx, cz, rmin, rmax, a0, a1, avoid) {
    const scanned = worlds.hasScan();
    for (let k = 0; k < 90; k++) {
      const a = rnd(a0, a1), r = rnd(rmin, rmax) * (k < 60 ? 1 : 0.6), x = cx + Math.sin(a) * r, z = cz - Math.cos(a) * r;
      if (avoid.some(p => Math.hypot(p.x - x, p.z - z) < 0.8)) continue;
      if (!scanned) return { x, z, y: null, h: 0.95 };
      const y = topAt(x, z);
      if (y === null || y > 1.25) continue;                          // nothing underneath: outside the room; or on top of a cupboard
      const h = hoverAt(y);
      if (clearSight(x, h, z)) { const ye = topExact(x, z), yy = ye !== null && Math.abs(ye - y) < 0.12 ? ye : y; return { x, z, y: yy, h: hoverAt(yy) }; }
    }
    const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; if (f.lengthSq() < 0.01) f.set(0, 0, -1); f.normalize();     // nowhere good: right in front of you
    return { x: head.x + f.x * 0.7, z: head.z + f.z * 0.7, y: null, h: clamp(head.y - 0.35, 0.85, 1.3) };
  }
  const hover = s => (s.h !== undefined ? s.h : hoverAt(s.y));
  const headYaw = () => { const f = new V(0, 0, -1).applyQuaternion(headQ); return Math.atan2(f.x, -f.z); };

  /* ---------------------------------------------------------------- touching and pointing */
  const _d = new V();
  const touching = (p, r) => c.hands.findIndex(h => h.on && h.pos.distanceTo(p) < r);
  function pointedAt(p, r) {                                        // a trigger press while the hand's ray passes within r of p
    return c.hands.findIndex(h => { if (!h.on || !h.trigEdge) return false; _d.copy(p).sub(h.rayO); const t = _d.dot(h.rayD); if (t < 0 || t > 6) return false; return _d.addScaledVector(h.rayD, -t).length() < r; });
  }

  function openLetter() {
    st.reading = true; st.readT = 0; sheet.visible = true; chime(660);
    const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; f.normalize();
    letter.getWorldPosition(sheetFrom); sheetTo.copy(head).addScaledVector(f, 0.85); sheetTo.y = head.y - 0.05;
  }
  /** The sound of something arriving from elsewhere: two tones beating as they climb, a ring-modulated shimmer, a breath of noise, a small bell at the end. */
  function manifestSound(p, secs) {
    const ctx = c.audio(); if (!ctx) return; const t = ctx.currentTime;
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 1.2; pan.rolloffFactor = 0.9; pan.positionX.value = p.x; pan.positionY.value = p.y; pan.positionZ.value = p.z; pan.connect(ctx.destination);
    const out = ctx.createGain(); out.gain.setValueAtTime(0.0001, t); out.gain.exponentialRampToValueAtTime(0.5, t + secs * 0.7); out.gain.exponentialRampToValueAtTime(0.0001, t + secs + 0.3); out.connect(pan);
    const ring = ctx.createGain(); ring.gain.value = 0; ring.connect(out); const lfo = ctx.createOscillator(); lfo.frequency.setValueAtTime(7, t); lfo.frequency.exponentialRampToValueAtTime(41, t + secs); lfo.connect(ring.gain); lfo.start(t); lfo.stop(t + secs + 0.4);
    for (const [f0, f1, v] of [[70, 420, 0.5], [74.5, 433, 0.5], [140, 1270, 0.16], [211, 318, 0.2]]) { const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.setValueAtTime(f0, t); o.frequency.exponentialRampToValueAtTime(f1, t + secs); const g = ctx.createGain(); g.gain.value = v; o.connect(g); g.connect(ring); g.connect(out); o.start(t); o.stop(t + secs + 0.4); }
    const nb = ctx.createBuffer(1, (ctx.sampleRate * (secs + 0.3)) | 0, ctx.sampleRate), nd = nb.getChannelData(0); for (let i = 0; i < nd.length; i++) nd[i] = Math.random() * 2 - 1;
    const ns = ctx.createBufferSource(); ns.buffer = nb; const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.Q.value = 6; bp.frequency.setValueAtTime(300, t); bp.frequency.exponentialRampToValueAtTime(6000, t + secs); const ng = ctx.createGain(); ng.gain.value = 0.35; ns.connect(bp); bp.connect(ng); ng.connect(out); ns.start(t);
    setTimeout(() => pingAt(p, 1568), secs * 1000);
  }
  const MANIFEST = 3.6;
  /** The next part of this world grows out of nothing, as far from the last one as the room allows. */
  function manifestNext(w, from) {
    const p = parts.find(q => q.world === w && !q.found && !q.shown); if (!p) return;
    const ya = headYaw(); let best = null, bd = -1;
    for (let k = 0; k < 10; k++) {
      const s = findSpot(head.x, head.z, 1.0, 3.6, ya - Math.PI, ya + Math.PI, []), d = from ? Math.hypot(s.x - from.x, s.z - from.z) : rnd(0, 1);
      if (d > bd) { bd = d; best = s; }
      if (!worlds.hasScan()) break;
    }
    p.pos.set(best.x, hover(best), best.z); p.grp.position.copy(p.pos); p.grp.scale.setScalar(0.02); p.shown = true; p.t = 0; st.searchT = 0; st.hintSaid = false;
    manifestSound(p.pos, MANIFEST);
  }
  function revealParts(w) {
    st.revealed[w] = true; st.nextT = -1;
    toast('Døren er lukket. Lyt: en del af tidsmaskinen er ved at blive til.', 7); manifestNext(w, null);
  }
  function collect(p) {
    p.found = true; p.grp.visible = true; p.grp.children[2].visible = false; p.grp.children[1].scale.setScalar(0.6); st.searchT = 0; st.hintSaid = false; st.found[p.k] = true; const n = st.found.filter(Boolean).length;
    p.slot = n - 1; belt.add(p.grp); const a = (-64 + (n - 1) * 16) * Math.PI / 180; p.grp.position.set(Math.sin(a) * 0.3, 0, -Math.cos(a) * 0.3); p.grp.scale.setScalar(0.3);
    chime(880 + n * 40); c.haptic(0.7, 90);
    const left = parts.filter(q => q.world === p.world && !q.found).length;
    if (left) { toast(`${p.name}. Del ${n} af 9. Den næste er på vej: lyt.`, 5); st.nextT = 1.6; st.nextFrom = p.pos.clone(); st.nextW = p.world; }
    else if (n === 9) toast('Alle ni dele! Skær en dør hjem til stuen.', 9);
    else { st.homeHint = true; toast('Alle tre dele her er fundet. Skær en dør hjem til stuen.', 9); }
  }


  /* ================================================================ chapter 2: the machine, the past, the knife */

  const SOCK = [                                                   // where each part sits on the machine (in the machine's own space) and how big
    { p: [-0.2, 0.308, -0.03], r: [0, 0, 0], s: 1.5 },             // flux coil, upright on the left
    { p: [-0.06, 0.266, -0.11], r: [0, 0, 0], s: 1.4 },            // vacuum tube
    { p: [0.325, 0.115, 0.0], r: [0, Math.PI / 2, 0], s: 1.45 },   // gears, on the right flank
    { p: [0.03, 0.318, 0.0], r: [0, 0, 0], s: 1.6 },               // time crystal, in the middle
    { p: [0.0, 0.1, 0.196], r: [0, 0, 0], s: 1.3 },                // chronometer, on the front
    { p: [0.17, 0.252, -0.11], r: [0, 0, 0], s: 1.3 },             // capacitor bank
    { p: [0.25, 0.298, 0.07], r: [0, 0.5, 0], s: 1.35 },           // antenna
    { p: [0.11, 0.277, 0.13], r: [0, 0, 0], s: 1.4 },              // the lever
    { p: [-0.2, 0.1, 0.2], r: [0, 0, 0], s: 1.45 },                // power core, in its bay
  ];
  const mach = new THREE.Group(), m0 = new THREE.Group(); mach.add(m0); m0.position.y = 0.03; root.add(mach); mach.visible = false;
  const lamps = [], ghosts = [];
  const lampOn = new THREE.MeshStandardMaterial({ color: 0x331a00, emissive: 0xffa040, emissiveIntensity: 2.2, roughness: 0.4 }), lampOff = new THREE.MeshStandardMaterial({ color: 0x241a12, roughness: 0.5 });
  const ghostMat = new THREE.MeshBasicMaterial({ color: 0x66ffe0, transparent: true, opacity: 0.16, depthWrite: false });
  {
    const chassis = new THREE.MeshStandardMaterial({ color: 0x2a3038, metalness: 0.85, roughness: 0.42 }), deck = new THREE.MeshStandardMaterial({ color: 0x8b949c, metalness: 0.9, roughness: 0.35 });
    const wood = new THREE.MeshStandardMaterial({ color: 0x4d2c16, metalness: 0, roughness: 0.55 });
    const add = (g, m, x, y, z) => { const o = mesh(g, m, x, y, z); m0.add(o); return o; };
    add(new THREE.BoxGeometry(0.62, 0.2, 0.38), chassis, 0, 0.1, 0);
    add(new THREE.BoxGeometry(0.636, 0.012, 0.396), M.brass, 0, 0.2, 0); add(new THREE.BoxGeometry(0.636, 0.012, 0.396), M.brass, 0, 0.006, 0);
    add(new THREE.BoxGeometry(0.56, 0.006, 0.33), deck, 0, 0.208, 0);
    for (const s of [-1, 1]) { add(new THREE.BoxGeometry(0.022, 0.184, 0.372), wood, s * 0.318, 0.103, 0); const hdl = add(new THREE.TorusGeometry(0.04, 0.007, 8, 18, Math.PI), M.brass, s * 0.333, 0.1, s > 0 ? -0.12 : 0); hdl.rotation.set(0, Math.PI / 2, -Math.PI / 2 * s); }
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) add(new THREE.CylinderGeometry(0.024, 0.02, 0.03, 14), M.brass, sx * 0.27, -0.015, sz * 0.15);
    for (let k = 0; k < 9; k++) { const l = add(new THREE.SphereGeometry(0.0085, 12, 8), lampOff, 0.085 + k * 0.024, 0.165, 0.192); lamps.push(l); add(new THREE.TorusGeometry(0.0105, 0.002, 6, 14), M.brass, 0.085 + k * 0.024, 0.165, 0.191); }
    add(new THREE.TorusGeometry(0.072, 0.007, 10, 36), M.brass, 0, 0.1, 0.191);                                   // the chronometer's bezel
    add(new THREE.TorusGeometry(0.066, 0.008, 10, 36), M.brass, -0.2, 0.1, 0.191); const bay = add(new THREE.CylinderGeometry(0.06, 0.06, 0.02, 28), M.dark, -0.2, 0.1, 0.183); bay.rotation.x = Math.PI / 2;
    for (let k = 0; k < 7; k++) add(new THREE.BoxGeometry(0.11, 0.006, 0.004), M.dark, 0.19, 0.04 + k * 0.014, 0.1905);            // vents
    for (const k of [0, 1, 3, 5, 6, 7]) add(new THREE.CylinderGeometry(0.042, 0.046, 0.008, 22), M.brass, SOCK[k].p[0], 0.214, SOCK[k].p[2]);
    add(new THREE.CylinderGeometry(0.03, 0.03, 0.03, 16).rotateZ(Math.PI / 2), M.brass, 0.322, 0.115, 0);                 // the gears' axle boss
    const cable = (pts, m) => m0.add(new THREE.Mesh(new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts.map(q => new V(...q))), 24, 0.0045, 6), m));
    cable([[-0.2, 0.214, -0.03], [-0.14, 0.24, -0.08], [-0.06, 0.214, -0.11]], M.copper); cable([[-0.06, 0.214, -0.11], [0.0, 0.25, -0.07], [0.03, 0.214, 0.0]], M.copper);
    cable([[0.03, 0.214, 0.0], [0.1, 0.245, -0.07], [0.17, 0.214, -0.11]], M.dark); cable([[0.17, 0.214, -0.11], [0.24, 0.24, -0.02], [0.25, 0.214, 0.07]], M.copper); cable([[0.03, 0.214, 0.0], [0.07, 0.235, 0.07], [0.11, 0.214, 0.13]], M.dark);
    const pc = document.createElement('canvas'); pc.width = 256; pc.height = 64; const px = pc.getContext('2d'); px.fillStyle = '#c59a3e'; px.fillRect(0, 0, 256, 64); px.fillStyle = '#2a1c08'; px.font = 'bold 30px Georgia, serif'; px.textAlign = 'center'; px.fillText('TEMPUS  ·  MK I', 128, 42);
    const plq = new THREE.MeshStandardMaterial({ map: new THREE.CanvasTexture(pc), metalness: 0.7, roughness: 0.4 }); plq.map.colorSpace = THREE.SRGBColorSpace;
    add(new THREE.BoxGeometry(0.15, 0.036, 0.004), plq, 0.19, 0.136, 0.192);
    PARTS.forEach((_, k) => { const g = buildPart(k); g.traverse(o => { if (o.isMesh) o.material = ghostMat; }); g.position.set(...SOCK[k].p); g.rotation.set(...SOCK[k].r); g.scale.setScalar(SOCK[k].s); m0.add(g); ghosts.push(g); });
  }
  const stand = new THREE.Group(); mach.add(stand); stand.visible = false;       // a small table for it, when the room offers nothing to put it on
  { const wd = new THREE.MeshStandardMaterial({ color: 0x3a2212, roughness: 0.6 }); stand.add(mesh(new THREE.CylinderGeometry(0.46, 0.46, 0.03, 40), wd, 0, -0.015, 0), mesh(new THREE.CylinderGeometry(0.035, 0.05, 0.7, 16), M.brass, 0, -0.38, 0), mesh(new THREE.CylinderGeometry(0.24, 0.27, 0.03, 30), wd, 0, -0.725, 0)); }
  const machPos = new V(); const _w = new V(), _w2 = new V();
  const sockWorld = (k, out) => m0.localToWorld(out.set(...SOCK[k].p));
  /** A flat top big enough for the machine, at table height, with nothing right above it; or else open floor well clear of walls and furniture. */
  function machineSpot() {
    if (!worlds.hasScan()) return null;
    let table = null, td = 1e9, open = null, od = 1e9;
    for (let k = 0; k < 900; k++) {
      const a = rnd(0, TAU), r = Math.sqrt(Math.random()) * 5.5, x = head.x + Math.sin(a) * r, z = head.z - Math.cos(a) * r, y = topAt(x, z);
      if (y === null) continue;
      const d = Math.hypot(x - head.x, z - head.z);
      if (y >= 0.5 && y <= 1.05 && d > 0.6 && d < td) {
        for (const yaw of [0, Math.PI / 4, Math.PI / 2, 3 * Math.PI / 4]) {
          const cx = Math.cos(yaw), sx = Math.sin(yaw); let flat = true;
          for (const [u, v] of [[0.3, 0.19], [-0.3, 0.19], [0.3, -0.19], [-0.3, -0.19], [0, 0.19], [0, -0.19]]) { const yy = topAt(x + u * cx + v * sx, z - u * sx + v * cx); if (yy === null || Math.abs(yy - y) > 0.045) { flat = false; break; } }
          if (flat && worlds.nearest(_w.set(x, y + 0.4, z), 0.3) === Infinity && clearSight(x, y + 0.3, z)) { table = { x, y, z, yaw }; td = d; break; }
        }
      } else if (y < 0.3 && d > 1.0 && d < 2.6 && d < od && worlds.nearest(_w.set(x, 0.6, z), 0.42) === Infinity && worlds.nearest(_w.set(x, 1.0, z), 0.42) === Infinity && clearSight(x, 0.9, z)) { open = { x, z }; od = d; }
    }
    if (table) { const ye = topExact(table.x, table.z); if (ye !== null && Math.abs(ye - table.y) < 0.12) table.y = ye; }
    return table ? { ...table, table: true } : open ? { x: open.x, y: 0.74, z: open.z, table: false } : null;
  }
  function placeMachine() {
    const s = machineSpot();
    if (s) { machPos.set(s.x, s.y, s.z); stand.visible = !s.table; }
    else { const top = topAt(letterPos.x, letterPos.z), onTable = top !== null && top >= 0.35 && top <= 1.1; machPos.set(letterPos.x, onTable ? top : 0.74, letterPos.z); stand.visible = !onTable; }
    const face = Math.atan2(head.x - machPos.x, head.z - machPos.z);                                       // its front faces where you stand
    mach.position.copy(machPos); mach.rotation.y = s && s.table ? s.yaw + Math.round((face - s.yaw) / Math.PI) * Math.PI : face;       // on a table: square to the table, the front the nearer way round
    st.machOn = true; mach.scale.setScalar(0.01); st.machT = 0;
    chime(392); toast(s && s.table ? 'Tidsmaskinens stel står på bordet. Tag en del fra bæltet (hold udløseren), og før den hen til sin plads.' : 'Tidsmaskinens stel er dukket op. Tag en del fra bæltet (hold udløseren), og før den hen til sin plads.', 11);
  }
  const placedCount = () => parts.filter(p => p.placed).length;

  /* holding things: a trigger held near something takes it; letting go drops it */
  const holding = [null, null];
  function install(p) {
    p.placed = true; p.held = -1; m0.add(p.grp); p.grp.position.set(...SOCK[p.k].p); p.grp.rotation.set(...SOCK[p.k].r); p.body.rotation.set(0, 0, 0); p.grp.scale.setScalar(SOCK[p.k].s / 1.7);
    p.grp.children[1].visible = false; ghosts[p.k].visible = false;
    const n = placedCount(); lamps.forEach((l, i) => { l.material = i < n ? lampOn : lampOff; });
    chime(440 + n * 55); c.haptic(0.9, 110);
    if (n === 9) { st.powered = true; st.powerT = 0; toast('Tidsmaskinen er samlet. Den varmer op … træk i håndtaget.', 9); }
    else toast(`${p.name} sidder. ${n} af 9.`, 4);
  }
  const toBelt = p => { p.held = -1; belt.add(p.grp); const a = (-64 + p.slot * 16) * Math.PI / 180; p.grp.position.set(Math.sin(a) * 0.3, 0, -Math.cos(a) * 0.3); p.grp.rotation.set(0, 0, 0); p.grp.scale.setScalar(0.3); };

  /* the past: an amber haze over the real room; and the white of the jump */
  const veil = mesh(new THREE.PlaneGeometry(2, 2), new THREE.ShaderMaterial({
    uniforms: { uSepia: { value: 0 }, uFlash: { value: 0 }, uTime: { value: 0 } }, transparent: true, depthTest: false, depthWrite: false,
    blending: THREE.CustomBlending, blendSrc: THREE.OneFactor, blendDst: THREE.OneMinusSrcAlphaFactor, blendSrcAlpha: THREE.OneFactor, blendDstAlpha: THREE.OneMinusSrcAlphaFactor,
    vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }',
    fragmentShader: `precision highp float; varying vec2 vUv; uniform float uSepia, uFlash, uTime;
      void main() { vec2 q = vUv - 0.5; float v = dot(q, q) * 2.4; float a = uSepia * (0.26 + 0.5 * v);
        vec3 col = vec3(0.62, 0.36, 0.13) * (0.92 + 0.08 * sin(uTime * 13.0) * sin(uTime * 7.3));
        gl_FragColor = mix(vec4(col * a, a), vec4(1.0), uFlash); }`,
  }));
  veil.frustumCulled = false; veil.renderOrder = 140; veil.visible = false; root.add(veil);

  /* the knife in three pieces, as it lies in the past */
  const PIECE = { blade: 'Klingen', grip: 'Skaftet', gem: 'Stenen' }, CENTRE = { blade: new V(0, 0, -0.21), grip: new V(0, 0, 0), gem: new V(0, 0, 0.076) };
  const pieces = [];
  const showSet = pc => pc.obj.children[0].children.forEach(o => { o.visible = pc.set.has(o.userData.piece); });
  const centreOf = (pc, out) => pc.obj.localToWorld(out.copy(pc.set.has('grip') ? CENTRE.grip : CENTRE[[...pc.set][0]]));
  const ringMark = mesh(new THREE.TorusGeometry(0.16, 0.006, 8, 48), new THREE.MeshBasicMaterial({ color: 0x80ffb0, transparent: true, opacity: 0.8, blending: THREE.AdditiveBlending, depthWrite: false })); ringMark.rotation.x = Math.PI / 2; st.ringOn = false; root.add(ringMark);
  function jumpSound() {
    const ctx = c.audio(); if (!ctx) return; const t = ctx.currentTime;
    const o = ctx.createOscillator(); o.type = 'sawtooth'; o.frequency.setValueAtTime(60, t); o.frequency.exponentialRampToValueAtTime(1400, t + 1.6); o.frequency.exponentialRampToValueAtTime(40, t + 3.2);
    const f = ctx.createBiquadFilter(); f.type = 'lowpass'; f.frequency.setValueAtTime(300, t); f.frequency.exponentialRampToValueAtTime(6000, t + 1.6); f.frequency.exponentialRampToValueAtTime(200, t + 3.2);
    const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(0.5, t + 1.5); g.gain.exponentialRampToValueAtTime(0.0001, t + 3.3);
    o.connect(f); f.connect(g); g.connect(ctx.destination); o.start(t); o.stop(t + 3.4);
  }
  function arriveInPast() {
    st.phase = 'past'; st.hasKnife = false; c.takeKnife(); st.powered = false; veil.material.uniforms.uSepia.value = 0.7;
    const used = [letterPos, machPos, head], ya = headYaw();
    for (const name of ['blade', 'grip', 'gem']) {
      const s = findSpot(head.x, head.z, 1.0, 2.4, ya - Math.PI, ya + Math.PI, used); used.push(s);
      const obj = c.makeKnife(); obj.add(halo(0xffd28a, 0.5)); obj.children[obj.children.length - 1].position.copy(CENTRE[name]);
      const pc = { obj, set: new Set([name]), held: -1, base: new V(s.x, hover(s), s.z), spin: rnd(0, TAU) }; obj.position.copy(pc.base); root.add(obj); showSet(pc); pieces.push(pc);
    }
    toast('Du er i fortiden. Kniven findes ikke endnu. Find klingen, skaftet og stenen, og sæt dem sammen (hold udløseren for at tage en del).', 13);
  }
  function chapter2(dt, ya) {
    /* the machine grows out of nothing, glows where parts are waiting, and runs when it is whole */
    if (st.machOn) {
      st.machT += dt; const e = clamp(st.machT / 1.0, 0, 1); mach.scale.setScalar(Math.max(0.01, 1 - Math.pow(1 - e, 3)));
      ghostMat.opacity = 0.1 + 0.08 * Math.sin(st.t * 3);
      if (st.powered) { st.powerT += dt; lamps.forEach((l, i) => { l.material = Math.sin(st.t * 9 - i * 0.7) > -0.3 ? lampOn : lampOff; }); }
    }
    const home = S.cur === 0 && (st.phase === 'play' || st.phase === 'past');
    /* grabbing */
    let near = false;
    c.hands.forEach((h, i) => {
      if (!h.on) return;
      let held = holding[i];
      if (!held && home) {
        let best = null, bd = 0.17;
        if (st.phase === 'play' && st.machOn) for (const p of parts) if (p.found && !p.placed && p.held < 0) { const d = p.grp.getWorldPosition(_w).distanceTo(h.pos); if (d < bd) { bd = d; best = { part: p }; } }
        if (st.phase === 'past') for (const pc of pieces) if (pc.held < 0) { const d = centreOf(pc, _w).distanceTo(h.pos); if (d < bd + 0.05) { bd = d - 0.05; best = { piece: pc }; } }
        if (best) { near = true; if (h.trigEdge) { held = holding[i] = best; if (best.part) { best.part.held = i; root.add(best.part.grp); best.part.grp.scale.setScalar(0.8); } else best.piece.held = i; c.haptic(0.4, 40); } }
      }
      if (!held) return;
      near = true;
      if (held.part) {
        const p = held.part; p.grp.position.copy(h.pos); p.body.rotation.y += dt * 1.5;
        if (sockWorld(p.k, _w).distanceTo(h.pos) < 0.16) { holding[i] = null; install(p); }
        else if (!h.trig) { holding[i] = null; toBelt(p); }
      } else {
        const pc = held.piece; pc.obj.position.copy(h.pos); pc.obj.quaternion.slerp(h.q, Math.min(1, dt * 12));
        for (const o of pieces) if (o !== pc && centreOf(o, _w).distanceTo(centreOf(pc, _w2)) < 0.2) {            // two pieces brought together become one
          for (const n of o.set) pc.set.add(n); showSet(pc); root.remove(o.obj); pieces.splice(pieces.indexOf(o), 1); if (o.held >= 0) holding[o.held] = null;
          chime(700 + pc.set.size * 120); c.haptic(1, 120);
          if (pc.set.size === 3) { st.made = true; st.ringOn = true; ringMark.position.copy(knifePos); toast('Kniven er lavet. Læg den, hvor du fandt den: ved den lysende ring.', 10); }
          else toast([...pc.set].map(n => PIECE[n]).join(' og ') + ' sidder sammen.', 5);
          break;
        }
        if (st.made && pc.set.size === 3 && centreOf(pc, _w).distanceTo(knifePos) < 0.22) {                       // laid where it was found: the loop closes
          holding[i] = null; root.remove(pc.obj); pieces.length = 0; st.ringOn = false; lying.position.copy(knifePos); st.lyingOn = true; st.phase = 'placed'; st.endT = 0; chime(1040); c.haptic(1, 300);
          toast('Kniven ligger, hvor den skal ligge. Gå hen til tidsmaskinen, og træk håndtaget tilbage: hjem til din egen tid.', 12);
        } else if (!h.trig) { holding[i] = null; pc.held = -1; pc.base.copy(pc.obj.position); }
      }
    });
    if (st.letterBelt && !st.reading) {                             // the letter on the belt: a trigger press with a hand on it opens it again
      letter.getWorldPosition(_w);
      for (const h of c.hands) if (h.on && h.pos.distanceTo(_w) < 0.16) { near = true; if (h.trigEdge && !holding[c.hands.indexOf(h)]) openLetter(); }
    }
    st.busy = near;
    for (const pc of pieces) if (pc.held < 0) { pc.spin += dt * 0.7; pc.obj.rotation.set(0.3, pc.spin, 0); pc.obj.position.y = pc.base.y + Math.sin(st.t * 1.3 + pc.spin) * 0.02; }
    if (st.ringOn) { ringMark.scale.setScalar(1 + 0.12 * Math.sin(st.t * 4)); ringMark.position.y = knifePos.y - 0.05; }
    /* the lever: a touch, once the machine is whole and has warmed up */
    if (st.powered && st.powerT > 2.5 && st.phase === 'play' && S.cur === 0 && touching(parts[7].grp.getWorldPosition(_w), 0.14) >= 0) {
      st.phase = 'jump'; st.jumpT = 0; veil.visible = true; c.closeDoor(); jumpSound(); c.haptic(1, 400); toast('…', 2);
    }
    const U = veil.material.uniforms; U.uTime.value = st.t;
    if (st.phase === 'jump') {
      st.jumpT += dt; U.uFlash.value = clamp(st.jumpT < 1.6 ? Math.pow(st.jumpT / 1.6, 2) : 1 - (st.jumpT - 1.9) / 1.4, 0, 1);
      parts[7].body.children[2] && (parts[7].body.children[2].rotation.z = -0.45 + Math.min(1, st.jumpT * 2) * 0.9);
      if (st.jumpT > 1.75 && pieces.length === 0 && !st.arrived) { st.arrived = true; arriveInPast(); st.phase = 'jump'; }
      if (st.jumpT > 3.3) { U.uFlash.value = 0; st.phase = 'past'; }
    }
    if (st.phase === 'placed') {
      st.endT += dt; lying.rotation.y += dt * 0.6;
      if (st.endT > 2 && touching(parts[7].grp.getWorldPosition(_w), 0.14) >= 0) { st.phase = 'back'; st.jumpT = 0; veil.visible = true; jumpSound(); c.haptic(1, 400); }
    }
    if (st.phase === 'back') {
      st.jumpT += dt; U.uFlash.value = clamp(st.jumpT < 1.6 ? Math.pow(st.jumpT / 1.6, 2) : 1 - (st.jumpT - 1.9) / 1.4, 0, 1);
      parts[7].body.children[2] && (parts[7].body.children[2].rotation.z = 0.45 - Math.min(1, st.jumpT * 2) * 0.9);
      if (st.jumpT > 1.75) { U.uSepia.value = 0; worlds.setPast(0); }
      if (st.jumpT > 3.3) { U.uFlash.value = 0; veil.visible = false; st.phase = 'free'; st.stick = true; chime(784); toast('Du er tilbage i din egen tid. Loopet er sluttet, og kniven ligger, hvor du fandt den. Rør ved den: nu er det fri leg.', 14); }
    }
    if (st.phase === 'past' || st.phase === 'placed') worlds.setPast(Math.min(1, (worlds.pastAmount || 0) + dt * 0.5));
    if (st.phase === 'free' && st.lyingOn && S.cur === 0) {
      lying.rotation.y += dt * 0.6; const i = touching(lying.position, 0.2);
      if (i >= 0) { st.lyingOn = false; st.hasKnife = true; c.giveKnife(i); }
    }
  }

  /* ---------------------------------------------------------------- the story itself */
  const api = {
    st, parts, toast, pieces, mach, collect, findSpot, clearSight, machineSpot, manifestNext, install, sockWorld, debug: { placeMachine, arriveInPast, knifePos, letterPos, centreOf },
    canCut: () => !st.on || (st.hasKnife && !st.busy && (st.phase === 'play' || st.phase === 'free')),
    stickOK: () => !st.on || st.phase === 'free' || (st.stick && S.cur === 0 && st.phase === 'play'),
    count: () => st.found.filter(Boolean).length,
    start() { st.on = true; st.phase = 'wait'; st.t = 0; root.visible = true; },
    /** What the board on the controller says you should be doing. */
    objective() {
      if (!st.on) return '';
      if (st.phase === 'wait' || st.phase === 'letter') return 'Hvad er det for en lyd?';
      if (st.reading) return st.letterBelt ? 'Tryk på udløseren for at folde brevet sammen' : 'Tryk på udløseren: brevet kommer med i bæltet';
      if (st.phase === 'knife') return 'Find kniven. Følg lyden.';
      if (st.phase === 'jump') return '…';
      if (st.phase === 'past') return st.made ? 'Læg kniven ved den lysende ring' : 'Find klinge, skaft og sten, og sæt dem sammen';
      if (st.phase === 'placed') return 'Træk håndtaget tilbage: hjem til din egen tid';
      if (st.phase === 'back') return '…';
      if (st.phase === 'free') return st.hasKnife ? 'Slut. Fri leg.' : 'Slut. Rør ved kniven for fri leg.';
      if (S.cur === 0) {
        if (st.powered) return 'Træk i håndtaget på tidsmaskinen';
        const onBelt = parts.filter(q => q.found && !q.placed).length;
        if (onBelt && st.machOn) return `Sæt delene på tidsmaskinen: ${placedCount()} af 9`;
        return S.placed ? 'Gå gennem døren' : st.stick ? 'Vælg verden med højre joystick, og skær en dør' : 'Skær en dør i luften';
      }
      if (!st.revealed[S.cur]) return 'Luk døren bag dig: skær hen over ringen';
      const left = parts.filter(q => q.world === S.cur && !q.found).length;
      return left ? `Find delen, der bliver til: ${3 - left} af 3 fundet` : S.placed ? 'Gå gennem døren hjem' : 'Skær en dør hjem til stuen';
    },
    onEnter(w) {
      if (!st.on || st.phase !== 'play') return;
      if (w === 0) {
        if (api.count() === 9 && !st.machOn) placeMachine();
        else if (st.machOn && parts.some(q => q.found && !q.placed)) toast('Sæt delene fra bæltet på tidsmaskinen (hold udløseren for at tage en del).', 8);
        else if (st.homeHint) { st.homeHint = false; st.stick = true; toast('Højre joystick vælger nu, hvilken verden den næste dør fører til. Vælg en ny verden, og skær en dør.', 11); }
        return;
      }
      st.searchT = 0; st.hintSaid = false;
      if (!st.warned) { st.warned = true; st.nagT = 6; toast('Gå MEGET forsigtigt herinde. Scanningen fanger ikke alt: en stoleryg eller noget, der stritter ud, kan mangle.', 9); }
      else if (!st.revealed[w]) { toast('Luk døren bag dig: skær hen over ringen med kniven.', 8); st.nagT = 0; }
    },
    onClosed() { if (st.on && st.phase === 'play' && S.cur !== 0 && !st.revealed[S.cur]) revealParts(S.cur); },
    update(dt) {
      if (!st.on) return; st.t += dt;
      const ya = headYaw();
      chapter2(dt, ya);
      if (st.phase === 'wait' && st.t > (S.mode === 'xr' ? 3.0 : 0.6)) {
        const s = findSpot(head.x, head.z, 1.2, 2.6, ya + 1.7, ya + 4.6, []);                 // somewhere behind you or off to the side
        st.letterHover = s.y === null || s.y < 0.6; letterPos.set(s.x, st.letterHover ? 0.95 : s.y + 0.012, s.z); letter.position.copy(letterPos); letter.rotation.y = rnd(0, TAU); st.letterOn = true;
        st.phase = 'letter'; st.hintT = 0; humAt(letterPos);
      }
      if (st.phase === 'letter' || (st.phase !== 'wait' && !st.reading)) {
        if (st.letterOn && S.cur === 0 && !st.reading && (pointedAt(letterPos, 0.4) >= 0 || touching(letterPos, 0.25) >= 0)) openLetter();
      }
      if (st.phase === 'letter') { humAt(letterPos); st.hintT += dt; if (st.hintT > 25) { st.hintT = 0; toast('Der er noget, der summer. Følg lyden, og peg og tryk på det, du finder.', 7); } }
      letter.children[1].material.opacity = st.phase === 'letter' ? 0.4 + 0.25 * Math.sin(st.t * 3) : 0.12;
      if (st.letterHover && st.letterOn) letter.position.y = letterPos.y + Math.sin(st.t * 1.3) * 0.02;
      if (st.reading) {
        st.readT += dt; const u = clamp(st.readT / 0.7, 0, 1), e = 1 - Math.pow(1 - u, 3);
        { const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; if (f.lengthSq() < 0.01) f.set(0, 0, -1); f.normalize();      // the sheet stays in front of your eyes: stand up, and it comes with you
          _d.copy(head).addScaledVector(f, 0.85); _d.y = head.y - 0.05; sheetTo.lerp(_d, Math.min(1, dt * (sheetTo.distanceTo(_d) > 0.3 ? 4 : 1.2)));
          sheetQ.setFromRotationMatrix(new THREE.Matrix4().lookAt(head, sheetTo, new V(0, 1, 0))); }
        sheet.position.lerpVectors(sheetFrom, sheetTo, e); sheet.quaternion.copy(sheetQ); sheet.scale.set(0.3 + 0.7 * e, 0.12 + 0.88 * e, 1); sheet.material.opacity = e;
        if (st.readT > 1.5 && c.hands.some(h => h.on && h.trigEdge)) {
          st.reading = false; sheet.visible = false; chime(520);
          if (!st.letterBelt) {                                        // you take the letter with you: it sits on the belt, far left, and can be read again from there
            st.letterBelt = true; st.letterOn = false; belt.add(letter); const a = -84 * Math.PI / 180; letter.position.set(Math.sin(a) * 0.3, 0, -Math.cos(a) * 0.3); letter.rotation.set(-1.1, -a, 0); letter.scale.setScalar(0.45); letter.children[1].visible = false;
          }
          if (st.phase === 'letter') {
            const s = findSpot(head.x, head.z, 1.1, 2.5, ya - Math.PI, ya + Math.PI, [letterPos, head]);
            knifePos.set(s.x, hover(s), s.z); lying.position.copy(knifePos); st.lyingOn = true; st.phase = 'knife'; humAt(knifePos);
            toast('Kniven er her et sted. Følg lyden.', 6);
          }
        }
      }
      if (st.phase === 'knife') {
        lying.rotation.y += dt * 0.6; lying.position.y = knifePos.y + Math.sin(st.t * 1.4) * 0.025; humAt(knifePos);
        const i = touching(lying.position, 0.2);
        if (i >= 0) { st.lyingOn = false; st.hasKnife = true; st.phase = 'play'; humAt(null); chime(1040); c.giveKnife(i); toast('Hold udløseren, og skær en dør i luften. Luk ringen, så tænder den.', 9); }
      }
      if (st.phase === 'play') {
        if (st.nextT > 0) { st.nextT -= dt; if (st.nextT <= 0 && S.cur === st.nextW) manifestNext(st.nextW, st.nextFrom); }
        if (S.cur !== 0 && st.revealed[S.cur] && parts.some(q => q.world === S.cur && !q.found)) {
          st.searchT += dt;
          if (st.searchT > 180 && !st.hintSaid) { st.hintSaid = true; toast('Lyt og kig efter: delene blinker og kalder nu.', 7); }
        }
        if (S.cur !== 0 && S.placed && !st.revealed[S.cur]) { st.nagT += dt; if (st.nagT > 14) { st.nagT = 0; toast('Luk døren bag dig: skær hen over ringen med kniven.', 7); } }
        for (const p of parts) {
          if (p.found) { if (!p.placed && p.held < 0) p.body.rotation.y += dt * 0.5; continue; }
          p.grp.visible = p.shown && S.cur === p.world;
          if (!p.grp.visible) continue;
          p.t += dt; const e = clamp(p.t / MANIFEST, 0, 1), grow = e * e * (3 - 2 * e);           // it grows out of nothing, shivering as it comes
          p.grp.scale.setScalar(Math.max(0.02, grow * (1 + (1 - e) * 0.25 * Math.sin(st.t * 31))));
          p.grp.position.y = p.pos.y + Math.sin(st.t * 1.2 + p.k) * 0.025; p.body.rotation.y += dt * (0.8 + (1 - e) * 9);
          const lvl = st.searchT > 300 ? 2 : st.searchT > 180 ? 1 : 0;                           // a long search: the parts start to blink and call, later they send up a beam
          p.grp.children[1].material.opacity = lvl ? 0.55 + 0.45 * Math.sin(st.t * 7 + p.k) : 0.4 + 0.2 * Math.sin(st.t * 2.4 + p.k);
          p.grp.children[1].scale.setScalar(lvl ? 1.1 : 0.6 + (1 - e) * 1.6 * (0.6 + 0.4 * Math.sin(st.t * 23))); p.grp.children[2].visible = lvl === 2 || (e < 1 && Math.sin(st.t * 17) > 0.2);
          if (lvl && st.t - (p.pingT || 0) > 5 + p.k % 3) { p.pingT = st.t; pingAt(p.grp.position, 1000 + p.k * 60); }
          if (p.t > MANIFEST * 0.85 && (touching(p.grp.position, 0.2) >= 0 || (st.hasKnife && c.tip().distanceTo(p.grp.position) < 0.17))) collect(p);
        }
      }
      { for (const p of parts) { if (p.body.userData.spin) { p.body.userData.spin[0].rotation.y += dt; p.body.userData.spin[1].rotation.y -= dt * 1.6; } if (p.body.userData.hand) p.body.userData.hand.rotation.z -= dt * 1.2; if (p.body.userData.rings) p.body.userData.rings.forEach((r, i) => { r.rotation.x += dt * (0.6 + i * 0.3); r.rotation.y += dt * 0.4; }); } }
      { const home = S.cur === 0; letter.visible = st.letterBelt || (st.letterOn && home); lying.visible = st.lyingOn && home; mach.visible = st.machOn && home; ringMark.visible = st.ringOn && home; }
      /* the belt follows your hips, not every turn of the head */
      let dy = ya - beltYaw; dy = Math.atan2(Math.sin(dy), Math.cos(dy)); beltYaw += dy * Math.min(1, dt * 2.5);
      belt.position.set(head.x, Math.max(0.5, head.y - 0.62), head.z); belt.rotation.y = -beltYaw;
      /* the floating line of text */
      if (toastDur > 0) {
        toastT += dt; const a = clamp(Math.min(toastT / 0.4, (toastDur - toastT) / 0.8), 0, 1); toastMesh.material.opacity = a; toastMesh.visible = a > 0.01;
        const f = new V(0, 0, -1).applyQuaternion(headQ); f.y = 0; if (f.lengthSq() < 0.01) f.set(0, 0, -1); f.normalize();
        _d.copy(head).addScaledVector(f, 1.15); _d.y = head.y - 0.32; toastMesh.position.lerp(_d, Math.min(1, dt * 4)); toastMesh.lookAt(head);
        if (toastT > toastDur) toastDur = 0;
      } else toastMesh.visible = false;
    },
  };
  return api;
}
