/* The Planetary Workshop (spec §2, milestone M5): a platform in open space where the system is built.
 *   Star Forge      the star in a containment ring at the far end, with a mass dial and an age dial
 *   Orrery Table    the system on a round table in the middle (the desktop orrery, scaled)
 *   Planet Cradle   the selected planet hanging in a gantry, 3.5 m across (the desktop planet, scaled)
 *   Ledger Board    the habitability verdict on the wall
 * The star's colour lights the whole platform. Desktop groups are re-parented in here while in VR and put back
 * afterwards. Metres; the floor at y = 0. */
import * as THREE from 'three';
import { createStar } from './star.js?v=14';
import { t } from '../i18n.js?v=14';

export const LAYOUT = {
  forge: new THREE.Vector3(0, 4.2, -15), forgeR: 2.2,
  dials: new THREE.Vector3(0, 1.15, -8.6),
  table: new THREE.Vector3(0, 0.92, -3.2), tableScale: 0.046, tableR: 1.45,
  cradle: new THREE.Vector3(4.6, 2.3, -5.6), planetR: 1.75,
  board: new THREE.Vector3(-4.9, 2.1, -4.6),
  start: new THREE.Vector3(0, 0, 0.6),
};

function floorTexture() {
  const c = document.createElement('canvas'); c.width = c.height = 512;
  const g = c.getContext('2d');
  g.fillStyle = '#1a1f26'; g.fillRect(0, 0, 512, 512);
  for (let i = 0; i < 2600; i++) { g.fillStyle = `rgba(255,255,255,${Math.random() * 0.03})`; g.fillRect(Math.random() * 512, Math.random() * 512, 2, 2); }
  g.strokeStyle = '#0c0f13'; g.lineWidth = 6;
  for (let i = 0; i <= 512; i += 128) { g.beginPath(); g.moveTo(i, 0); g.lineTo(i, 512); g.stroke(); g.beginPath(); g.moveTo(0, i); g.lineTo(512, i); g.stroke(); }
  g.strokeStyle = '#2b333d'; g.lineWidth = 2;
  for (let i = 3; i <= 512; i += 128) { g.beginPath(); g.moveTo(i, 0); g.lineTo(i, 512); g.stroke(); g.beginPath(); g.moveTo(0, i); g.lineTo(512, i); g.stroke(); }
  const tx = new THREE.CanvasTexture(c);
  tx.wrapS = tx.wrapT = THREE.RepeatWrapping; tx.repeat.set(9, 9); tx.colorSpace = THREE.SRGBColorSpace; tx.anisotropy = 4;
  return tx;
}

/* A text sprite (canvas), for labels in the world. */
export function textSprite(text, { size = 0.12, color = '#dfe7ef', bg = 'rgba(8,12,18,0.75)' } = {}) {
  const c = document.createElement('canvas'); const g = c.getContext('2d');
  g.font = '600 56px system-ui, sans-serif';
  const w = Math.ceil(g.measureText(text).width) + 40;
  c.width = w; c.height = 84;
  g.font = '600 56px system-ui, sans-serif'; g.textBaseline = 'middle';
  g.fillStyle = bg; g.beginPath(); g.roundRect(0, 0, w, 84, 18); g.fill();
  g.fillStyle = color; g.fillText(text, 20, 44);
  const tx = new THREE.CanvasTexture(c); tx.colorSpace = THREE.SRGBColorSpace;
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: tx, toneMapped: false, depthWrite: false }));
  s.scale.set(size * w / 84, size, 1);
  s.userData.setText = (txt) => { const n = textSprite(txt, { size, color, bg }); s.material.map.dispose(); s.material.map = n.material.map; s.scale.copy(n.scale); };
  return s;
}

/* A dial: a disc facing the user that turns to the angle the controller ray points to around its centre. */
function makeDial(label, color) {
  const g = new THREE.Group();
  const base = new THREE.Mesh(new THREE.CylinderGeometry(0.34, 0.36, 0.06, 64), new THREE.MeshStandardMaterial({ color: 0x20262e, metalness: 0.7, roughness: 0.35 }));
  base.rotation.x = Math.PI / 2;
  const knob = new THREE.Mesh(new THREE.CylinderGeometry(0.27, 0.28, 0.09, 64), new THREE.MeshStandardMaterial({ color: 0x39434f, metalness: 0.8, roughness: 0.3 }));
  knob.rotation.x = Math.PI / 2; knob.position.z = 0.05;
  const pointer = new THREE.Mesh(new THREE.BoxGeometry(0.03, 0.2, 0.02), new THREE.MeshBasicMaterial({ color, toneMapped: false }));
  pointer.position.set(0, 0.13, 0.1);
  const turn = new THREE.Group(); turn.add(knob, pointer);
  /* scale ticks from -135 to +135 degrees */
  const ticks = new THREE.Group();
  for (let i = 0; i <= 18; i++) {
    const a = THREE.MathUtils.degToRad(-135 + i * 15);
    const tk = new THREE.Mesh(new THREE.BoxGeometry(0.012, i % 3 === 0 ? 0.06 : 0.035, 0.01), new THREE.MeshBasicMaterial({ color: 0x8a98a8, toneMapped: false }));
    tk.position.set(Math.sin(a) * 0.33, Math.cos(a) * 0.33, 0.035); tk.rotation.z = -a;
    ticks.add(tk);
  }
  const title = textSprite(label, { size: 0.11 }); title.position.set(0, 0.55, 0);
  const value = textSprite('–', { size: 0.12, color: '#9fd0ff' }); value.position.set(0, -0.52, 0);
  g.add(base, ticks, turn, title, value);
  knob.userData = { kind: 'dial', dial: g };
  base.userData = { kind: 'dial', dial: g };
  g.userData = { turn, value, angle: 0 };
  g.userData.setAngle = (a) => { g.userData.angle = a; turn.rotation.z = -a; };
  return g;
}

export function createWorkshop() {
  const root = new THREE.Group();
  root.visible = false;

  /* platform: a disc of dark plating with a rim and light strips, open to space */
  const floor = new THREE.Mesh(new THREE.CircleGeometry(10, 96), new THREE.MeshStandardMaterial({ map: floorTexture(), metalness: 0.6, roughness: 0.55 }));
  floor.rotation.x = -Math.PI / 2;
  root.add(floor);
  const rim = new THREE.Mesh(new THREE.TorusGeometry(10, 0.12, 12, 128), new THREE.MeshStandardMaterial({ color: 0x2a323c, metalness: 0.8, roughness: 0.3 }));
  rim.rotation.x = Math.PI / 2; rim.position.y = 0.9;
  root.add(rim);
  for (let i = 0; i < 24; i++) {
    const a = i / 24 * Math.PI * 2;
    const post = new THREE.Mesh(new THREE.CylinderGeometry(0.04, 0.04, 0.9, 8), rim.material);
    post.position.set(Math.cos(a) * 10, 0.45, Math.sin(a) * 10); root.add(post);
  }
  const stripMat = new THREE.MeshBasicMaterial({ color: 0x6fb7ff, toneMapped: false, transparent: true, opacity: 0.55 });
  const strip = new THREE.Mesh(new THREE.RingGeometry(9.6, 9.66, 128), stripMat);
  strip.rotation.x = -Math.PI / 2; strip.position.y = 0.01; root.add(strip);

  /* light: the star floods the platform with its colour; a dim cool fill so nothing is black */
  const starLight = new THREE.PointLight(0xffffff, 180, 0, 1.6);
  const fill = new THREE.HemisphereLight(0x55667a, 0x10131a, 0.5);
  root.add(fill);

  /* Star Forge */
  /* the forge is one group (star, rings, label, light) that can be carried round with the controller ray */
  const forgeGroup = new THREE.Group();
  forgeGroup.position.copy(LAYOUT.forge);
  root.add(forgeGroup);
  const forge = createStar();
  forge.group.scale.setScalar(LAYOUT.forgeR);
  forge.uniforms.uGain.value = 1.15;            /* no bloom in VR: keep the colour below white so it reads */
  forgeGroup.add(forge.group, starLight);
  forge.group.children[0].userData = { kind: 'sun' };
  /* (the dark containment rings read as a black ring round the star in the headset; removed) */
  const forgeLabel = textSprite('', { size: 0.45 }); forgeLabel.position.set(0, LAYOUT.forgeR * 2.2, 0); forgeGroup.add(forgeLabel);
  const pedestal = new THREE.Mesh(new THREE.BoxGeometry(2.0, 1.05, 0.5), new THREE.MeshStandardMaterial({ color: 0x1c2229, metalness: 0.6, roughness: 0.45 }));
  pedestal.position.set(LAYOUT.dials.x, 0.52, LAYOUT.dials.z - 0.2); root.add(pedestal);
  const massDial = makeDial(t('vr.massDial'), 0xffc870);
  massDial.position.copy(LAYOUT.dials).add(new THREE.Vector3(-0.5, 0.35, 0.1));
  massDial.rotation.x = -0.45;
  const ageDial = makeDial(t('vr.ageDial'), 0x9fd0ff);
  ageDial.position.copy(LAYOUT.dials).add(new THREE.Vector3(0.5, 0.35, 0.1));
  ageDial.rotation.x = -0.45;
  root.add(massDial, ageDial);

  /* Orrery Table */
  const table = new THREE.Mesh(new THREE.CylinderGeometry(LAYOUT.tableR + 0.12, LAYOUT.tableR + 0.2, 0.06, 96), new THREE.MeshStandardMaterial({ color: 0x14181e, metalness: 0.5, roughness: 0.6 }));
  table.position.copy(LAYOUT.table).add(new THREE.Vector3(0, -0.04, 0)); root.add(table);
  const leg = new THREE.Mesh(new THREE.CylinderGeometry(0.25, 0.45, LAYOUT.table.y - 0.04, 24), pedestal.material);
  leg.position.set(LAYOUT.table.x, (LAYOUT.table.y - 0.04) / 2, LAYOUT.table.z); root.add(leg);
  /* the distance dial for the selected planet, at the table's near edge */
  const distDial = makeDial(t('vr.distDial'), 0x8ff0b0);
  distDial.position.copy(LAYOUT.table).add(new THREE.Vector3(0.75, 0.15, LAYOUT.tableR + 0.45));
  distDial.rotation.x = -0.7;
  const distStand = new THREE.Mesh(new THREE.CylinderGeometry(0.06, 0.1, LAYOUT.table.y + 0.1, 12), pedestal.material);
  distStand.position.set(distDial.position.x, (LAYOUT.table.y + 0.1) / 2 - 0.05, distDial.position.z - 0.08);
  root.add(distDial, distStand);
  const tableSlot = new THREE.Group(); tableSlot.position.copy(LAYOUT.table); tableSlot.scale.setScalar(LAYOUT.tableScale); root.add(tableSlot);

  /* Planet Cradle: a gantry of two rings around the planet */
  const cradleSlot = new THREE.Group(); cradleSlot.position.copy(LAYOUT.cradle); cradleSlot.scale.setScalar(LAYOUT.planetR); root.add(cradleSlot);
  const cradleParts = [];
  for (const rot of [[0, 0, 0], [0, Math.PI / 2, 0.25]]) {
    const g = new THREE.Mesh(new THREE.TorusGeometry(LAYOUT.planetR * 1.32, 0.05, 12, 128, Math.PI * 1.35), rim.material);
    g.position.copy(LAYOUT.cradle); g.rotation.set(rot[0], rot[1], rot[2] - Math.PI * 0.175 - Math.PI / 2);
    root.add(g); cradleParts.push(g);
  }
  const cradleBase = new THREE.Mesh(new THREE.CylinderGeometry(0.6, 0.9, 0.3, 32), pedestal.material);
  cradleBase.position.set(LAYOUT.cradle.x, 0.15, LAYOUT.cradle.z); root.add(cradleBase); cradleParts.push(cradleBase);
  const cradleLabel = textSprite('', { size: 0.2 }); cradleLabel.position.copy(LAYOUT.cradle).add(new THREE.Vector3(0, LAYOUT.planetR * 1.55, 0)); root.add(cradleLabel);

  /* Ledger Board: a canvas on the left wall, facing the middle */
  const bc = document.createElement('canvas'); bc.width = 1600; bc.height = 1000;
  const btex = new THREE.CanvasTexture(bc); btex.colorSpace = THREE.SRGBColorSpace; btex.anisotropy = 4;
  const board = new THREE.Mesh(new THREE.PlaneGeometry(3.6, 2.25), new THREE.MeshBasicMaterial({ map: btex, toneMapped: false }));
  board.position.copy(LAYOUT.board); board.rotation.y = Math.PI / 2.6;
  const frame = new THREE.Mesh(new THREE.BoxGeometry(3.75, 2.4, 0.06), pedestal.material);
  frame.position.copy(LAYOUT.board); frame.rotation.y = board.rotation.y; frame.translateZ(-0.04);
  root.add(frame, board);

  function drawBoard(S) {
    const g = bc.getContext('2d');
    g.fillStyle = '#0b1017'; g.fillRect(0, 0, 1600, 1000);
    const P = S.derived.planets.find((p, i) => i === S.sel && p.role === 'hz') || S.derived.planets.find((p) => p.role === 'hz');
    g.textBaseline = 'middle';
    g.fillStyle = '#dfe7ef'; g.font = '600 56px system-ui'; g.fillText(t('ledger'), 50, 64);
    if (!P) { g.font = '36px system-ui'; g.fillStyle = '#8a98a8'; g.fillText(t('ledger.none'), 50, 160); btex.needsUpdate = true; return; }
    g.font = '36px system-ui'; g.fillStyle = '#8a98a8'; g.fillText(P.name, 50, 120);
    const L = S.ledgerFor(P);
    const col = { green: '#47c07a', amber: '#e8b04a', red: '#e45d5d', na: '#5d6878' };
    for (let tier = 1; tier <= 5; tier++) {
      const y = 190 + (tier - 1) * 86;
      g.fillStyle = '#121922'; g.beginPath(); g.roundRect(50, y - 36, 700, 72, 14); g.fill();
      g.fillStyle = L.pass[tier] ? col.green : col.red; g.beginPath(); g.arc(92, y, 18, 0, Math.PI * 2); g.fill();
      g.fillStyle = '#dfe7ef'; g.font = '600 34px system-ui'; g.fillText(`${tier}. ${t('tier.' + tier)}`, 128, y);
    }
    /* the rows as dots with short names, two columns */
    g.font = '27px system-ui';
    L.rows.forEach((r, i) => {
      const cx = i < 15 ? 800 : 1200, y = 100 + (i % 15) * 58;
      g.fillStyle = col[r.status]; g.beginPath(); g.arc(cx + 14, y, 11, 0, Math.PI * 2); g.fill();
      g.fillStyle = '#c4cfdb'; g.fillText(t('row.' + r.id).slice(0, 24), cx + 36, y);
    });
    g.fillStyle = '#8a98a8'; g.font = '26px system-ui'; g.fillText(t('vr.boardHint'), 50, 960);
    btex.needsUpdate = true;
  }

  const holds = {};
  return {
    root, forge, forgeGroup, starLight, cradleParts, tableParts: [table, leg], massDial, ageDial, distDial, tableSlot, cradleSlot, board, drawBoard, forgeLabel, cradleLabel, LAYOUT,
    /* move the desktop groups in (and out again) */
    adopt(name, obj) { holds[name] = { parent: obj.parent, pos: obj.position.clone(), quat: obj.quaternion.clone(), scale: obj.scale.clone(), vis: obj.visible }; },
    release(name, obj) { const h = holds[name]; if (!h) return; h.parent.add(obj); obj.position.copy(h.pos); obj.quaternion.copy(h.quat); obj.scale.copy(h.scale); obj.visible = h.vis; },
  };
}
