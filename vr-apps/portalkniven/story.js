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
  const st = { on: false, phase: 'off', hasKnife: false, stick: false, reading: false, readT: 0, t: 0, nagT: 0, hintT: 0, found: PARTS.map(() => false), revealed: { 1: false, 2: false, 3: false }, doneSaid: false };
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
  const parts = PARTS.map((p, k) => { const grp = new THREE.Group(), body = buildPart(k); body.scale.setScalar(1.7); grp.add(body, halo(0x7dffe0, 0.6)); grp.visible = false; root.add(grp); return { k, ...p, grp, body, found: false, shown: false, t: 0, pos: new V() }; });

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
  const folded = mesh(new THREE.BoxGeometry(0.2, 0.004, 0.14), [0, 1, 2, 3, 4, 5].map(i => new THREE.MeshStandardMaterial({ color: 0xe6d9b6, roughness: 0.9, map: i === 2 ? foldTex : null })));
  letter.add(folded, halo(0xffe9a8, 0.5));
  const sheet = mesh(new THREE.PlaneGeometry(0.44, 0.6), new THREE.MeshBasicMaterial({ map: sheetTex, side: THREE.DoubleSide, transparent: true, depthTest: false, toneMapped: false })); sheet.renderOrder = 150; sheet.visible = false; root.add(sheet);
  const letterPos = new V(), sheetFrom = new V(), sheetTo = new V(), sheetQ = new THREE.Quaternion();

  /* the knife as it lies in the room before it is yours */
  const lying = new THREE.Group(); root.add(lying); lying.visible = false;
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
  function chime(f = 880) {
    const ctx = c.audio(); if (!ctx) return; const t = ctx.currentTime;
    for (const [m, v] of [[1, 0.22], [1.5, 0.12], [2.01, 0.08]]) { const o = ctx.createOscillator(); o.frequency.value = f * m; const g = ctx.createGain(); g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(v, t + 0.01); g.gain.exponentialRampToValueAtTime(0.0001, t + 1.4); o.connect(g); g.connect(ctx.destination); o.start(t); o.stop(t + 1.5); }
  }

  /* ---------------------------------------------------------------- placing things on what is really there */
  const rc = new THREE.Raycaster(), DOWN = new V(0, -1, 0), _o = new V();
  function topAt(x, z) {
    const g = worlds.W[1].scan; if (!g.children.length) return null;
    rc.set(_o.set(x, 1.9, z), DOWN); rc.far = 2.3;
    const h = rc.intersectObject(g, true); return h.length ? h[0].point.y : null;
  }
  /** A spot r metres from (cx, cz) in the direction range, on top of something no higher than a table. */
  function findSpot(cx, cz, rmin, rmax, a0, a1, avoid) {
    let best = null;
    for (let k = 0; k < 40; k++) {
      const a = rnd(a0, a1), r = rnd(rmin, rmax), x = cx + Math.sin(a) * r, z = cz - Math.cos(a) * r;
      if (avoid.some(p => Math.hypot(p.x - x, p.z - z) < 0.8)) continue;
      const y = topAt(x, z);
      if (y === null) { best = best || { x, z, y: null }; if (!worlds.W[1].scan.children.length) break; continue; }
      if (y <= 1.25) return { x, z, y };
    }
    return best || { x: cx + Math.sin(a0) * rmin, z: cz - Math.cos(a0) * rmin, y: null };
  }
  const hover = s => (s.y === null ? 0.85 : s.y < 0.45 ? 0.72 : s.y + 0.13);        // on a table: just above it; on the floor or unknown: at hand height
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
    sheetFrom.copy(letterPos); sheetTo.copy(head).addScaledVector(f, 0.68); sheetTo.y = head.y - 0.06;
    sheetQ.setFromRotationMatrix(new THREE.Matrix4().lookAt(head, sheetTo, new V(0, 1, 0)));          // the written side faces you
  }
  function revealParts(w) {
    st.revealed[w] = true; const used = [];
    const ya = headYaw();
    for (const p of parts) if (p.world === w) {
      const s = findSpot(head.x, head.z, 1.0, 2.4, ya - Math.PI, ya + Math.PI, used); used.push(s);
      p.pos.set(s.x, hover(s), s.z); p.grp.position.copy(p.pos); p.shown = true; p.t = 0;
    }
    chime(523); toast('Døren er lukket. Delene viser sig: find tre dele af tidsmaskinen.', 7);
  }
  function collect(p) {
    p.found = true; st.found[p.k] = true; const n = st.found.filter(Boolean).length;
    belt.add(p.grp); const a = (-64 + (n - 1) * 16) * Math.PI / 180; p.grp.position.set(Math.sin(a) * 0.3, 0, -Math.cos(a) * 0.3); p.grp.scale.setScalar(0.3);
    chime(880 + n * 40); c.haptic(0.7, 90);
    const left = parts.filter(q => q.world === p.world && !q.found).length;
    if (left) toast(`${p.name}. Del ${n} af 9.`, 4);
    else if (n === 9) toast('Alle ni dele! Skær en dør hjem til stuen.', 9);
    else if (!st.stick) { st.stick = true; toast('Alle tre dele her er fundet. Højre joystick vælger nu, hvor den næste dør fører hen. Skær en ny dør.', 11); }
    else toast('Alle tre dele her er fundet. Vælg en ny verden med joysticken, og skær en ny dør.', 9);
  }

  /* ---------------------------------------------------------------- the story itself */
  const api = {
    st, parts, toast,
    canCut: () => !st.on || st.hasKnife,
    stickOK: () => !st.on || st.stick,
    count: () => st.found.filter(Boolean).length,
    start() { st.on = true; st.phase = 'wait'; st.t = 0; root.visible = true; },
    /** What the board on the controller says you should be doing. */
    objective() {
      if (!st.on) return '';
      if (st.phase === 'wait' || st.phase === 'letter') return 'Hvad er det for en lyd?';
      if (st.reading) return 'Tryk på udløseren for at folde brevet sammen';
      if (st.phase === 'knife') return 'Find kniven. Følg lyden.';
      if (S.cur === 0) return api.count() === 9 ? 'Alle dele fundet. Fortsættes …' : S.placed ? 'Gå gennem døren' : 'Skær en dør i luften';
      if (!st.revealed[S.cur]) return 'Luk døren bag dig: skær hen over ringen';
      const left = parts.filter(q => q.world === S.cur && !q.found).length;
      return left ? `Find delene her: ${3 - left} af 3` : S.placed ? 'Gå gennem døren' : 'Skær en ny dør';
    },
    onEnter(w) {
      if (!st.on || st.phase !== 'play') return;
      if (w === 0) { if (api.count() === 9 && !st.doneSaid) { st.doneSaid = true; toast('Du har alle ni dele. Fortsættes: tidsmaskinen samles i næste kapitel.', 12); } return; }
      if (!st.revealed[w]) { toast('Luk døren bag dig: skær hen over ringen med kniven.', 8); st.nagT = 0; }
    },
    onClosed() { if (st.on && st.phase === 'play' && S.cur !== 0 && !st.revealed[S.cur]) revealParts(S.cur); },
    update(dt) {
      if (!st.on) return; st.t += dt;
      const ya = headYaw();
      if (st.phase === 'wait' && st.t > (S.mode === 'xr' ? 3.0 : 0.6)) {
        const s = findSpot(head.x, head.z, 1.2, 2.6, ya + 1.7, ya + 4.6, []);                 // somewhere behind you or off to the side
        letterPos.set(s.x, s.y === null ? 0.85 : s.y + 0.012, s.z); letter.position.copy(letterPos); letter.rotation.y = rnd(0, TAU); letter.visible = true;
        st.phase = 'letter'; st.hintT = 0; humAt(letterPos);
      }
      if (st.phase === 'letter' || (st.phase !== 'wait' && !st.reading)) {
        if (letter.visible && !st.reading && (pointedAt(letterPos, 0.24) >= 0 || (st.phase === 'letter' && touching(letterPos, 0.2) >= 0))) openLetter();
      }
      if (st.phase === 'letter') { humAt(letterPos); st.hintT += dt; if (st.hintT > 25) { st.hintT = 0; toast('Der er noget, der summer. Følg lyden, og peg og tryk på det, du finder.', 7); } }
      letter.children[1].material.opacity = st.phase === 'letter' ? 0.4 + 0.25 * Math.sin(st.t * 3) : 0.12;
      if (letterPos.y > 0.8 && st.phase !== 'wait') letter.position.y = letterPos.y + Math.sin(st.t * 1.3) * 0.02;
      if (st.reading) {
        st.readT += dt; const u = clamp(st.readT / 0.7, 0, 1), e = 1 - Math.pow(1 - u, 3);
        sheet.position.lerpVectors(sheetFrom, sheetTo, e); sheet.quaternion.copy(sheetQ); sheet.scale.set(0.3 + 0.7 * e, 0.12 + 0.88 * e, 1); sheet.material.opacity = e;
        if (st.readT > 1.5 && c.hands.some(h => h.on && h.trigEdge)) {
          st.reading = false; sheet.visible = false; chime(520);
          if (st.phase === 'letter') {
            const s = findSpot(head.x, head.z, 1.1, 2.5, ya - Math.PI, ya + Math.PI, [letterPos, head]);
            knifePos.set(s.x, hover(s), s.z); lying.position.copy(knifePos); lying.visible = true; st.phase = 'knife'; humAt(knifePos);
            toast('Kniven er her et sted. Følg lyden.', 6);
          }
        }
      }
      if (st.phase === 'knife') {
        lying.rotation.y += dt * 0.6; lying.position.y = knifePos.y + Math.sin(st.t * 1.4) * 0.025; humAt(knifePos);
        const i = touching(lying.position, 0.2);
        if (i >= 0) { lying.visible = false; st.hasKnife = true; st.phase = 'play'; humAt(null); chime(1040); c.giveKnife(i); toast('Hold udløseren, og skær en dør i luften. Luk ringen, så tænder den.', 9); }
      }
      if (st.phase === 'play') {
        if (S.cur !== 0 && S.placed && !st.revealed[S.cur]) { st.nagT += dt; if (st.nagT > 14) { st.nagT = 0; toast('Luk døren bag dig: skær hen over ringen med kniven.', 7); } }
        for (const p of parts) {
          if (p.found) { p.body.rotation.y += dt * 0.5; continue; }
          p.grp.visible = p.shown && S.cur === p.world;
          if (!p.grp.visible) continue;
          p.t += dt; const e = clamp(p.t / 0.8, 0, 1);
          p.grp.scale.setScalar(0.2 + 0.8 * (1 - Math.pow(1 - e, 3))); p.grp.position.y = p.pos.y + Math.sin(st.t * 1.2 + p.k) * 0.025; p.body.rotation.y += dt * 0.8;
          p.grp.children[1].material.opacity = 0.4 + 0.2 * Math.sin(st.t * 2.4 + p.k);
          if (p.t > 0.8 && (touching(p.grp.position, 0.2) >= 0 || (st.hasKnife && c.tip().distanceTo(p.grp.position) < 0.17))) collect(p);
        }
        for (const p of parts) { if (p.body.userData.spin) { p.body.userData.spin[0].rotation.y += dt; p.body.userData.spin[1].rotation.y -= dt * 1.6; } if (p.body.userData.hand) p.body.userData.hand.rotation.z -= dt * 1.2; if (p.body.userData.rings) p.body.userData.rings.forEach((r, i) => { r.rotation.x += dt * (0.6 + i * 0.3); r.rotation.y += dt * 0.4; }); }
      }
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
