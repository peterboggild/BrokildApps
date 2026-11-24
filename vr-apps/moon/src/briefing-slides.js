/* The briefing boards: schematics of every stage of the mission, drawn in code on a 2048 x 1152 canvas.
 * Not authentic (no such room existed in this form); the numbers are Apollo 17's where known, rounded. */

export const W = 2048, H = 1152;
const AMBER = '#e8b04a', INK = 'rgba(232,236,242,0.92)', DIM = 'rgba(200,208,220,0.55)', FAINT = 'rgba(200,208,220,0.18)', BLUE = '#6fa6e0', RED = '#e0644a';

function base(x, n, title, sub) {
  const g = x.createLinearGradient(0, 0, 0, H); g.addColorStop(0, '#0c1424'); g.addColorStop(1, '#070b14');
  x.fillStyle = g; x.fillRect(0, 0, W, H);
  x.strokeStyle = 'rgba(120,150,190,0.07)'; x.lineWidth = 1;
  for (let i = 0; i < W; i += 64) { x.beginPath(); x.moveTo(i, 0); x.lineTo(i, H); x.stroke(); }
  for (let j = 0; j < H; j += 64) { x.beginPath(); x.moveTo(0, j); x.lineTo(W, j); x.stroke(); }
  x.fillStyle = AMBER; x.font = '600 30px Arial'; x.textAlign = 'left';
  x.fillText(('MISSION BRIEFING  ·  ' + String(n).padStart(2, '0') + ' / 12').split('').join(String.fromCharCode(8202)), 80, 78);
  x.fillStyle = INK; x.font = '600 66px Arial'; x.fillText(title, 80, 160);
  if (sub) { x.fillStyle = DIM; x.font = '32px Arial'; x.fillText(sub, 82, 210); }
  x.fillStyle = DIM; x.font = '26px Arial'; x.textAlign = 'right';
  x.fillText('trigger: next   ·   B: back', W - 80, H - 46);
  x.textAlign = 'left';
}
function label(x, s, px, py, o = {}) {
  x.font = `${o.bold ? '600 ' : ''}${o.size || 30}px Arial`; x.fillStyle = o.color || INK; x.textAlign = o.align || 'left';
  const lines = String(s).split('\n');
  lines.forEach((l, i) => x.fillText(l, px, py + i * (o.size || 30) * 1.25));
  x.textAlign = 'left';
}
function line(x, pts, o = {}) {
  x.strokeStyle = o.color || INK; x.lineWidth = o.w || 3; x.setLineDash(o.dash || []);
  x.beginPath(); pts.forEach(([a, b], i) => (i ? x.lineTo(a, b) : x.moveTo(a, b))); if (o.close) x.closePath(); x.stroke(); x.setLineDash([]);
}
function arrow(x, x0, y0, x1, y1, o = {}) {
  line(x, [[x0, y0], [x1, y1]], o);
  const a = Math.atan2(y1 - y0, x1 - x0), s = o.head || 18;
  x.fillStyle = o.color || INK; x.beginPath(); x.moveTo(x1, y1);
  x.lineTo(x1 - s * Math.cos(a - 0.4), y1 - s * Math.sin(a - 0.4)); x.lineTo(x1 - s * Math.cos(a + 0.4), y1 - s * Math.sin(a + 0.4)); x.fill();
}
function dot(x, px, py, r, c) { x.fillStyle = c || AMBER; x.beginPath(); x.arc(px, py, r, 0, 7); x.fill(); }
function circle(x, px, py, r, o = {}) {
  if (o.fill) { x.fillStyle = o.fill; x.beginPath(); x.arc(px, py, r, 0, 7); x.fill(); }
  x.strokeStyle = o.color || INK; x.lineWidth = o.w || 3; x.setLineDash(o.dash || []); x.beginPath(); x.arc(px, py, r, o.a0 || 0, o.a1 || 7); x.stroke(); x.setLineDash([]);
}
function earth(x, px, py, r) {
  const g = x.createRadialGradient(px - r * 0.3, py - r * 0.3, r * 0.1, px, py, r);
  g.addColorStop(0, '#5b8fd0'); g.addColorStop(0.7, '#244c8a'); g.addColorStop(1, '#0f2448');
  x.fillStyle = g; x.beginPath(); x.arc(px, py, r, 0, 7); x.fill();
  x.strokeStyle = 'rgba(140,190,255,0.6)'; x.lineWidth = 4; x.beginPath(); x.arc(px, py, r + 3, 0, 7); x.stroke();
}
function moon(x, px, py, r) {
  const g = x.createRadialGradient(px - r * 0.3, py - r * 0.3, r * 0.1, px, py, r);
  g.addColorStop(0, '#c9c6bf'); g.addColorStop(1, '#6a6862');
  x.fillStyle = g; x.beginPath(); x.arc(px, py, r, 0, 7); x.fill();
  x.fillStyle = 'rgba(70,70,68,0.35)'; for (const [a, b, c] of [[-0.3, -0.2, 0.22], [0.25, 0.1, 0.16], [-0.05, 0.35, 0.12]]) { x.beginPath(); x.arc(px + a * r, py + b * r, c * r, 0, 7); x.fill(); }
}
/* the Saturn V in side view; (px, py) the bottom of the first stage; s pixels per metre */
function saturn(x, px, py, s, labels) {
  const st = (y0, y1, r0, r1, fill) => { x.fillStyle = fill; x.beginPath(); x.moveTo(px - r0 * s, py - y0 * s); x.lineTo(px - r1 * s, py - y1 * s); x.lineTo(px + r1 * s, py - y1 * s); x.lineTo(px + r0 * s, py - y0 * s); x.closePath(); x.fill(); x.strokeStyle = 'rgba(0,0,0,0.5)'; x.lineWidth = 2; x.stroke(); };
  const ys = [0, 42, 67, 72.5, 83, 84, 92.5, 96.5, 99.9, 110.6];
  st(0, 42, 5.05, 5.05, '#e9e7e0'); x.fillStyle = '#151515'; x.fillRect(px - 5.05 * s, py - 6.5 * s, 5.05 * s, 6.5 * s); x.fillRect(px, py - 28 * s, 5.05 * s, 4 * s);
  st(42, 67, 5.05, 5.05, '#ecebe6'); x.fillStyle = '#151515'; x.fillRect(px + 1.5 * s, py - 47.5 * s, 3.55 * s, 5.5 * s);
  st(67, 72.5, 5.05, 3.3, '#ecebe6'); st(72.5, 83, 3.3, 3.3, '#e9e7e0'); st(83, 84, 3.3, 3.3, '#888');
  st(84, 92.5, 3.3, 1.96, '#d8d6cf'); st(92.5, 96.5, 1.96, 1.96, '#c3c6cb'); st(96.5, 99.9, 1.96, 0.45, '#dfe3e8');
  x.strokeStyle = RED; x.lineWidth = 3; line(x, [[px - 0.9 * s, py - 99.9 * s], [px - 0.3 * s, py - 107 * s], [px + 0.3 * s, py - 107 * s], [px + 0.9 * s, py - 99.9 * s]], { color: RED, w: 3 });
  x.fillStyle = '#eee'; x.fillRect(px - 0.33 * s, py - 110.6 * s, 0.66 * s, 3.6 * s);
  for (const e of [-3.2, 0, 3.2]) { x.fillStyle = '#333'; x.beginPath(); x.moveTo(px + (e - 0.6) * s, py); x.lineTo(px + (e - 1.6) * s, py + 5.8 * s); x.lineTo(px + (e + 1.6) * s, py + 5.8 * s); x.lineTo(px + (e + 0.6) * s, py); x.fill(); }
  if (labels) {
    const items = [[21, 'S-IC first stage', '5 F-1 engines · kerosene + oxygen · 2 min 41 s'], [55, 'S-II second stage', '5 J-2 engines · hydrogen + oxygen · 6 min 30 s'],
      [78, 'S-IVB third stage', '1 J-2 engine · burns twice: orbit, then towards the Moon'], [83.5, 'Instrument Unit', 'the rocket\'s guidance'],
      [88.5, 'Adapter, with the Lunar Module inside', 'four panels open in space'], [94.5, 'Service Module', 'main engine, oxygen, power'],
      [98.2, 'Command Module', 'your cabin: three couches'], [105, 'Launch escape tower', 'pulls the capsule clear in an emergency']];
    items.forEach(([h, a, b], i) => {
      const y = 1000 - i * 98;
      const xr = px + 6.5 * s;
      line(x, [[px + (h > 83 ? 2.2 : 5.3) * s, py - h * s], [xr - 10, y]], { color: FAINT, w: 2 });
      label(x, a, xr, y + 6, { bold: true, size: 30 }); label(x, b, xr, y + 40, { size: 24, color: DIM });
    });
  }
}

export const SLIDES = [
  /* 1 */ (x) => {
    base(x, 1, 'Apollo to Taurus-Littrow', 'From the pad to the valley floor, and home');
    earth(x, 330, 720, 150); moon(x, 1700, 560, 95);
    x.setLineDash([16, 12]); x.strokeStyle = AMBER; x.lineWidth = 3; x.beginPath(); x.moveTo(470, 680); x.bezierCurveTo(800, 380, 1350, 360, 1615, 520); x.stroke();
    x.strokeStyle = DIM; x.setLineDash([8, 14]); x.beginPath(); x.moveTo(1640, 650); x.bezierCurveTo(1450, 760, 800, 800, 470, 760); x.stroke(); x.setLineDash([]);
    label(x, 'Outbound: 3 days', 1020, 400, { color: AMBER, align: 'center' }); label(x, 'Home: 3 days', 1060, 820, { color: DIM, align: 'center' });
    label(x, 'The crew', 200, 935, { bold: true, size: 34 });
    label(x, 'Commander: you. Flies the Lunar Module to the landing.     Command Module Pilot: stays in lunar orbit.\nLunar Module Pilot: systems and navigation; walks the valley with you.', 200, 985, { size: 27 });
  },
  /* 2 */ (x) => {
    base(x, 2, 'The Apollo-Saturn V', '110.6 m tall · about 2,950 tonnes at liftoff · 34 MN thrust');
    saturn(x, 520, 1040, 6.7, true);
  },
  /* 3 */ (x) => {
    base(x, 3, 'Launch to Earth orbit', 'Eleven minutes and fifty-three seconds');
    const X0 = 200, Y0 = 1000, sx = 1650 / 720, sy = 720 / 180;
    line(x, [[X0, Y0], [X0 + 1650, Y0]], { color: DIM, w: 2 }); line(x, [[X0, Y0], [X0, Y0 - 740]], { color: DIM, w: 2 });
    for (let t = 0; t <= 720; t += 120) { label(x, (t / 60) + ' min', X0 + t * sx, Y0 + 40, { size: 24, color: DIM, align: 'center' }); }
    for (let a = 0; a <= 180; a += 60) { label(x, a + ' km', X0 - 20, Y0 - a * sy + 8, { size: 24, color: DIM, align: 'right' }); line(x, [[X0, Y0 - a * sy], [X0 + 1650, Y0 - a * sy]], { color: FAINT, w: 1 }); }
    const T = [0, 30, 60, 83, 120, 161, 197, 300, 420, 548, 600, 713], A = [0, 1.2, 6.5, 13.3, 31, 67, 91, 138, 165, 175, 172, 167];
    line(x, T.map((t, i) => [X0 + t * sx, Y0 - A[i] * sy]), { color: AMBER, w: 5 });
    const ev = [[0, 0, 'Liftoff', 20, -24, 'left'], [83, 13.3, 'Max Q: the hardest push of the air', 22, 10, 'left'], [161, 67, 'First stage off, second stage on', 22, 30, 'left'],
      [197, 91, 'Escape tower and cover jettisoned: windows open', -22, -24, 'right'], [548, 175, 'Second stage off, third stage on', 0, -30, 'center'], [713, 167, 'Third stage off: in orbit, 167 km', -10, 52, 'right']];
    ev.forEach(([t, a, s, dx, dy, al]) => { const px = X0 + t * sx, py = Y0 - a * sy; dot(x, px, py, 10); label(x, s, px + dx, py + dy, { size: 26, align: al }); });
  },
  /* 4 */ (x) => {
    base(x, 4, 'Earth orbit, then towards the Moon', 'One and a half orbits · trans-lunar injection');
    earth(x, 760, 660, 250);
    circle(x, 760, 660, 330, { color: DIM, w: 3, dash: [10, 10] });
    label(x, 'Parking orbit 167 km · 88 minutes per orbit · systems checked', 760, 1040, { align: 'center', size: 28, color: DIM });
    circle(x, 760, 660, 330, { color: AMBER, w: 7, a0: Math.PI * 1.05, a1: Math.PI * 1.45 });
    label(x, 'TLI burn: third stage, 5 min 51 s\n+3.1 km/s → 10.8 km/s', 90, 600, { size: 28, color: AMBER });
    x.strokeStyle = AMBER; x.lineWidth = 5; x.setLineDash([18, 12]); x.beginPath(); x.moveTo(760 + 330 * Math.cos(Math.PI * 1.45), 660 + 330 * Math.sin(Math.PI * 1.45));
    x.bezierCurveTo(1150, 160, 1600, 180, 1950, 300); x.stroke(); x.setLineDash([]);
    arrow(x, 1880, 270, 1960, 300, { color: AMBER, w: 4 }); label(x, 'to the Moon', 1760, 250, { size: 30, color: AMBER });
    dot(x, 760, 330, 12, '#fff'); label(x, 'insertion', 790, 320, { size: 26 });
  },
  /* 5 */ (x) => {
    base(x, 5, 'Transposition and docking', 'About half an hour after leaving Earth orbit');
    const csm = (px, py, a, s = 1) => { x.save(); x.translate(px, py); x.rotate(a); x.scale(s, s);
      x.fillStyle = '#c3c6cb'; x.fillRect(-28, 0, 56, 90); x.fillStyle = '#dfe3e8'; x.beginPath(); x.moveTo(-28, 0); x.lineTo(-8, -50); x.lineTo(8, -50); x.lineTo(28, 0); x.fill();
      x.fillStyle = '#555'; x.beginPath(); x.moveTo(-10, 90); x.lineTo(-22, 125); x.lineTo(22, 125); x.lineTo(10, 90); x.fill(); x.restore(); };
    const lm = (px, py) => { x.fillStyle = '#c9a24a'; x.fillRect(px - 32, py, 64, 34); x.fillStyle = '#9ea2a8'; x.fillRect(px - 26, py - 36, 52, 36);
      x.strokeStyle = '#c9a24a'; x.lineWidth = 4; line(x, [[px - 32, py + 34], [px - 52, py + 64]], { color: '#c9a24a', w: 4 }); line(x, [[px + 32, py + 34], [px + 52, py + 64]], { color: '#c9a24a', w: 4 }); };
    const sivb = (px, py, panels) => { x.fillStyle = '#e9e7e0'; x.fillRect(px - 52, py, 104, 150); x.strokeStyle = 'rgba(0,0,0,0.4)'; x.strokeRect(px - 52, py, 104, 150);
      if (panels) { x.fillStyle = '#d8d6cf'; x.beginPath(); x.moveTo(px - 52, py); x.lineTo(px - 110, py - 130); x.lineTo(px - 90, py - 140); x.lineTo(px - 30, py); x.fill(); x.beginPath(); x.moveTo(px + 52, py); x.lineTo(px + 110, py - 130); x.lineTo(px + 90, py - 140); x.lineTo(px + 30, py); x.fill(); } };
    const col = [300, 760, 1240, 1720], step = ['1  Separate', '2  Turn round', '3  Dock', '4  Pull the LM free'];
    step.forEach((s, i) => label(x, s, col[i], 330, { bold: true, size: 34, align: 'center' }));
    sivb(col[0], 760, true); lm(col[0], 715); csm(col[0], 470, 0); arrow(x, col[0] + 90, 640, col[0] + 90, 540, { color: AMBER });
    sivb(col[1], 760, false); lm(col[1], 715); csm(col[1], 500, Math.PI * 0.55); x.strokeStyle = AMBER; x.lineWidth = 4; x.beginPath(); x.arc(col[1], 480, 110, -1.2, 1.6); x.stroke();
    sivb(col[2], 760, false); lm(col[2], 715); csm(col[2], 640, Math.PI);
    sivb(col[3], 790, false); lm(col[3], 600); csm(col[3], 525, Math.PI); arrow(x, col[3] + 100, 560, col[3] + 100, 450, { color: AMBER });
    label(x, 'The adapter panels fall away; the Command Module Pilot turns the spacecraft round, docks nose to nose with the Lunar Module,\nand the docked pair backs away from the third stage, which is then sent off on its own path.', 1024, 1010, { align: 'center', size: 27, color: DIM });
  },
  /* 6 */ (x) => {
    base(x, 6, 'The coast to the Moon', '3 days · 380,000 km · a free-return path');
    earth(x, 300, 640, 70); moon(x, 1720, 640, 40);
    x.strokeStyle = AMBER; x.lineWidth = 5; x.beginPath(); x.moveTo(370, 620); x.bezierCurveTo(800, 330, 1450, 420, 1700, 590); x.stroke();
    x.strokeStyle = DIM; x.setLineDash([12, 12]); x.beginPath(); x.moveTo(1740, 690); x.bezierCurveTo(1550, 900, 800, 950, 370, 670); x.stroke(); x.setLineDash([]);
    x.strokeStyle = AMBER; x.beginPath(); x.arc(1720, 640, 80, -2.3, 1.9); x.stroke();
    label(x, 'Outbound: Earth\'s pull slows you from 10.8 km/s to under 1 km/s', 760, 360, { size: 28, color: AMBER });
    label(x, 'Free return: with no burn at all, the Moon would swing you home', 1050, 900, { size: 28, color: DIM, align: 'center' });
    circle(x, 1720, 640, 210, { color: FAINT, w: 2, dash: [6, 10] }); label(x, 'where the Moon\'s\ngravity takes over', 1760, 400, { size: 24, color: DIM });
    label(x, 'Passive thermal control: a slow roll, three turns an hour,\nso the Sun warms every side in turn', 300, 1000, { size: 26 });
    circle(x, 220, 1010, 40, { color: INK, w: 3, a0: 0.3, a1: 5.6 }); arrow(x, 251, 1032, 258, 1000, { color: INK, w: 3, head: 14 });
  },
  /* 7 */ (x) => {
    base(x, 7, 'Lunar orbit', 'The burn behind the Moon · then down to 15 km');
    moon(x, 1080, 650, 280);
    circle(x, 1080, 650, 345, { color: DIM, w: 3, dash: [10, 10] });
    x.strokeStyle = AMBER; x.lineWidth = 5; x.beginPath(); x.moveTo(120, 280); x.bezierCurveTo(600, 300, 1100, 250, 1430, 520); x.stroke();
    circle(x, 1080, 650, 345, { color: AMBER, w: 8, a0: -0.25, a1: 0.45 });
    label(x, 'Lunar orbit insertion\nService Module engine, 6 min', 1460, 640, { size: 28, color: AMBER });
    x.fillStyle = 'rgba(0,0,0,0.45)'; x.beginPath(); x.moveTo(1080, 650); x.arc(1080, 650, 420, -0.75, 0.75); x.fill();
    label(x, 'behind the Moon: no radio contact', 1350, 980, { size: 26, color: DIM });
    x.strokeStyle = BLUE; x.lineWidth = 3; x.setLineDash([6, 8]); x.beginPath(); x.ellipse(1080, 610, 320, 340, 0, 0, 7); x.stroke(); x.setLineDash([]);
    label(x, 'orbit 110 km', 560, 430, { size: 28 }); label(x, 'a day later: low point 15 km', 560, 1030, { size: 28, color: BLUE });
    dot(x, 1080, 270, 10, BLUE);
  },
  /* 8 */ (x) => {
    base(x, 8, 'Into the Lunar Module', 'Through the tunnel · undocking');
    x.fillStyle = '#dfe3e8'; x.beginPath(); x.moveTo(380, 520); x.lineTo(600, 430); x.lineTo(600, 610); x.closePath(); x.fill();
    x.fillStyle = '#c3c6cb'; x.fillRect(160, 450, 220, 140); x.fillStyle = '#9ea2a8'; x.fillRect(600, 485, 60, 70);
    x.fillStyle = '#9ea2a8'; x.fillRect(660, 420, 200, 200); x.fillStyle = '#c9a24a'; x.fillRect(860, 400, 160, 240);
    arrow(x, 470, 520, 760, 520, { color: AMBER, w: 6 }); label(x, 'the docking tunnel, 80 cm wide', 470, 400, { size: 26, color: AMBER });
    label(x, 'Command Module', 280, 700, { size: 28, align: 'center' }); label(x, 'Lunar Module', 840, 700, { size: 28, align: 'center' });
    const fig = (px, py) => { circle(x, px, py, 22, { color: INK, w: 3 }); line(x, [[px, py + 22], [px, py + 110]], { w: 3 }); line(x, [[px - 30, py + 60], [px + 30, py + 60]], { w: 3 }); line(x, [[px, py + 110], [px - 22, py + 170]], { w: 3 }); line(x, [[px, py + 110], [px + 22, py + 170]], { w: 3 }); };
    fig(1300, 450); fig(1440, 450); line(x, [[1240, 640], [1500, 640]], { color: DIM, w: 3 });
    label(x, 'No seats: you fly standing,\nheld by cables at the waist,\nlooking down through two\ntriangular windows', 1600, 480, { size: 28 });
    label(x, 'The Command Module Pilot stays aboard the Command Module and keeps it in orbit for your return.', 1024, 960, { size: 27, align: 'center', color: DIM });
  },
  /* 9 */ (x) => {
    base(x, 9, 'Powered descent', '12 1/2 minutes from 15 km to the valley floor');
    const X0 = 180, Y0 = 980, sx = 1700 / 480, sy = 700 / 15;
    line(x, [[X0, Y0], [X0 + 1700, Y0]], { color: DIM, w: 2 });
    for (let d = 0; d <= 480; d += 80) label(x, (480 - d) + ' km', X0 + d * sx, Y0 + 40, { size: 22, color: DIM, align: 'center' });
    const P = [[0, 15], [100, 14], [200, 12.3], [300, 9.6], [380, 6.5], [430, 3.8], [460, 2.2], [472, 0.9], [478, 0.15], [480, 0]];
    line(x, P.map(([d, h]) => [X0 + d * sx, Y0 - h * sy]), { color: AMBER, w: 5 });
    const ev = [[0, 15, 'PDI: the descent engine lights', 'left'], [180, 12.6, 'Braking phase (P63): windows up,\nEarth and black sky in the windows', 'left'],
      [460, 2.2, 'High gate 2 km: pitch-over,\nthe valley swings into view', 'right'], [478, 0.15, 'Low gate 150 m: you may take over (P66)', 'right'], [480, 0, 'Contact light', 'right']];
    const off = [[18, -30], [18, -30], [-30, -110], [-30, -22], [-24, 50]];
    ev.forEach(([d, h, s, al], i) => { const px = X0 + d * sx, py = Y0 - h * sy; dot(x, px, py, 10); label(x, s, px + off[i][0], py + off[i][1], { size: 26, align: al }); });
    label(x, 'The approach (P64): the computer flies, you choose the spot with the landing point designator', 1024, 1070, { size: 26, align: 'center', color: DIM });
  },
  /* 10 */ (x) => {
    base(x, 10, 'Taurus-Littrow', 'A valley 7 km wide between mountain walls over 2 km high');
    const blob = (pts, fill) => { x.fillStyle = fill; x.beginPath(); pts.forEach(([a, b], i) => (i ? x.lineTo(a, b) : x.moveTo(a, b))); x.closePath(); x.fill(); };
    x.fillStyle = '#4a4a46'; x.fillRect(200, 260, 1640, 780);
    blob([[200, 260], [1100, 260], [1000, 430], [760, 470], [420, 440], [200, 400]], '#86847c');
    blob([[200, 1040], [200, 800], [520, 760], [900, 820], [1050, 1040]], '#8e8c84');
    blob([[1500, 260], [1840, 260], [1840, 700], [1640, 620], [1520, 460]], '#7c7a72');
    blob([[1100, 1040], [1260, 860], [1500, 900], [1600, 1040]], '#78766e');
    label(x, 'North Massif', 560, 360, { bold: true, size: 32, align: 'center', color: '#1d1d1b' }); label(x, 'South Massif', 600, 940, { bold: true, size: 32, align: 'center', color: '#1d1d1b' });
    label(x, 'Sculptured Hills', 1690, 400, { bold: true, size: 28, align: 'center', color: '#1d1d1b' }); label(x, 'Family Mountain', 1360, 990, { bold: true, size: 28, align: 'center', color: '#1d1d1b' });
    dot(x, 1060, 640, 14, AMBER); circle(x, 1060, 640, 30, { color: AMBER, w: 3 }); label(x, 'landing site', 1100, 650, { size: 30, color: AMBER });
    arrow(x, 1830, 640, 1720, 640, { color: '#ffe9a0', w: 5 }); label(x, 'Sun: 13° above\nthe eastern horizon', 1620, 720, { size: 24, color: '#ffe9a0' });
    line(x, [[240, 1000], [240 + 233, 1000]], { color: INK, w: 5 }); label(x, '2 km', 240, 985, { size: 24 });
    label(x, 'N', 1880, 300, { bold: true, size: 34 }); arrow(x, 1890, 380, 1890, 320, { w: 3 });
  },
  /* 11 */ (x) => {
    base(x, 11, 'Home', 'Ascent · rendezvous · three days · re-entry · splashdown');
    const steps = [['Ascent stage lifts off;\nthe descent stage stays', 260], ['Rendezvous and docking\nin lunar orbit', 720], ['Trans-Earth injection\nbehind the Moon', 1180], ['Re-entry at 11 km/s;\nthree parachutes', 1640]];
    steps.forEach(([s, px], i) => { label(x, s, px, 860, { size: 28, align: 'center' }); dot(x, px, 760, 14, i === 3 ? BLUE : AMBER); if (i < 3) arrow(x, px + 40, 760, px + 420, 760, { color: DIM, w: 3 }); });
    moon(x, 260, 520, 120); x.fillStyle = '#9ea2a8'; x.fillRect(240, 360, 40, 30);
    moon(x, 720, 560, 80); circle(x, 720, 560, 140, { color: DIM, w: 2, dash: [8, 8] });
    moon(x, 1180, 560, 80); earth(x, 1640, 540, 120);
    x.fillStyle = '#e8e6e0'; for (const dx of [-60, 0, 60]) { x.beginPath(); x.arc(1640 + dx, 340, 34, Math.PI, 0); x.fill(); line(x, [[1640 + dx - 34, 340], [1640, 420]], { color: DIM, w: 1 }); line(x, [[1640 + dx + 34, 340], [1640, 420]], { color: DIM, w: 1 }); }
    label(x, 'These parts of the journey come in later chapters.', 1024, 1040, { size: 26, align: 'center', color: DIM });
  },
  /* 12 */ (x) => {
    base(x, 12, 'What no astronaut had', 'Curiosity is allowed');
    const rows = [['Y', 'A large window in the wall to your left'], ['hold B', 'Step outside and look at the spacecraft'], ['left stick', 'outside: round it, nearer or further'],
      ['right stick', 'outside: up and down · on the Moon: how far to jump'], ['A / X', 'In the couch: time runs ×4 or ×16 when nothing critical is happening'],
      ['trigger', 'On the Moon: aim and jump · boarding: climb in at the hatch'], ['stick press', 'left: frame-rate panel · right: sit right in the couch again']];
    rows.forEach(([k, s], i) => { const y = 340 + i * 96; x.fillStyle = 'rgba(232,176,74,0.15)'; x.fillRect(200, y - 46, 300, 66); label(x, k, 350, y, { bold: true, size: 32, align: 'center', color: AMBER }); label(x, s, 560, y, { size: 32 }); });
    label(x, 'Trigger: leave the briefing', 1024, 1050, { bold: true, size: 34, align: 'center', color: AMBER });
  }
];
