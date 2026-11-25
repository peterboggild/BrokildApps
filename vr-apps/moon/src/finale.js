/* The end of chapter 6: the film, played back on a console in Mission Control.
 *
 * The lift-off leaves the valley; the view goes dark, and you sit at a flight controller's console in Houston. Its
 * big picture tube plays what the rover's television camera saw: the two of you waving in front of the LM, then the
 * ascent stage leaving the Moon. Then the words.
 *
 * The console is a picture, not a model (the owner's choice, for now): drawn here in code after the consoles of the
 * Mission Operations Control Room (pale green cabinet with its louvred top, grey panels of square lamp buttons,
 * number wheels, handles either side of the picture tube, the white desk). No room, no people: it stands in the
 * dark, lit by its own lamps. The tube is drawn larger than life and shows the film in muted colour, with the scan
 * lines and soft corners of the time.
 *
 *   const con = makeConsole(filmTexture);     con.group: put it in front of the viewer; con.update(t)
 *   con.caption(lines, k)                     the closing words on the tube (k: 0 hidden to 1 shown)
 *   const rec = makeRecorder();               rec.add(t, Float32Array), rec.at(t, out): the motion the film replays */
import * as THREE from 'three';

export const SCREEN = { w: 0.94, h: 0.69, x: 0, y: 0.054 };          /* the tube's picture, metres, on a 2.6 x 1.46 m console */

function consoleTexture() {
  const W = 2048, H = 1152, c = document.createElement('canvas'); c.width = W; c.height = H; const x = c.getContext('2d');
  let seed = 3; const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
  const rr = (X, Y, w, h, r, fill, stroke) => { x.beginPath(); x.roundRect(X, Y, w, h, r); if (fill) { x.fillStyle = fill; x.fill(); } if (stroke) { x.strokeStyle = stroke; x.lineWidth = 2; x.stroke(); } };
  const screw = (X, Y) => { x.fillStyle = '#8f928f'; x.beginPath(); x.arc(X, Y, 5, 0, 7); x.fill(); x.strokeStyle = '#5c5f5c'; x.lineWidth = 1.5; x.beginPath(); x.moveTo(X - 3, Y - 3); x.lineTo(X + 3, Y + 3); x.stroke(); };
  /* the cabinet, pale green, darker towards the edges of the lamp light */
  const g = x.createLinearGradient(0, 0, 0, H); g.addColorStop(0, '#93a591'); g.addColorStop(0.2, '#aebfac'); g.addColorStop(1, '#8b9c89');
  x.fillStyle = g; x.fillRect(0, 0, W, H);
  /* the louvred top */
  for (const [X, w] of [[60, 560], [660, 730], [1430, 560]]) { rr(X, 26, w, 120, 6, '#a3b4a1', '#7f907d'); x.strokeStyle = 'rgba(70,84,70,0.55)'; x.lineWidth = 2; for (let y = 40; y < 136; y += 9) { x.beginPath(); x.moveTo(X + 16, y); x.lineTo(X + w - 16, y); x.stroke(); } }
  /* the panels: light grey plates with screws at the corners */
  const plate = (X, Y, w, h) => { rr(X, Y, w, h, 4, '#c6c5be', '#8e8d86'); for (const [a, b] of [[10, 10], [w - 10, 10], [10, h - 10], [w - 10, h - 10]]) screw(X + a, Y + b); };
  /* a matrix of square lamp buttons; some lit white, amber or red */
  const lamps = (X, Y, cols, rows, s, lit) => { for (let r = 0; r < rows; r++) for (let q = 0; q < cols; q++) {
    const v = rnd(), col = v < lit * 0.55 ? '#f4f1e2' : v < lit * 0.85 ? '#e9b64a' : v < lit ? '#d2533c' : '#4b4c49';
    rr(X + q * (s + 5), Y + r * (s * 0.62 + 5), s, s * 0.62, 2, col, '#2c2d2b');
    if (col !== '#4b4c49') { x.fillStyle = 'rgba(30,30,30,0.5)'; x.fillRect(X + q * (s + 5) + 4, Y + r * (s * 0.62 + 5) + s * 0.28, s - 8, 2); } } };
  /* number wheels: amber digits in black windows */
  const wheels = (X, Y, txt) => { rr(X, Y, 26 * txt.length + 10, 44, 3, '#151515', '#3a3a3a'); x.fillStyle = '#e8a63a'; x.font = 'bold 32px Arial'; x.textAlign = 'center'; x.textBaseline = 'middle'; [...txt].forEach((ch, i) => x.fillText(ch, X + 18 + i * 26, Y + 23)); };
  const label = (X, Y, t) => { x.fillStyle = '#3d3d3a'; x.font = '11px Arial'; x.textAlign = 'center'; x.textBaseline = 'middle'; x.fillText(t, X, Y); };
  const knob = (X, Y, r) => { x.fillStyle = '#1b1b1b'; x.beginPath(); x.arc(X, Y, r, 0, 7); x.fill(); x.fillStyle = '#3a3a3a'; x.beginPath(); x.arc(X - r * 0.25, Y - r * 0.25, r * 0.45, 0, 7); x.fill(); };
  const handle = (X, Y, h) => { rr(X, Y, 20, h, 10, '#b4b6b4', '#6f716f'); rr(X + 5, Y + 8, 6, h - 16, 3, '#d6d8d6'); };
  /* the upper row */
  plate(60, 170, 500, 250); lamps(84, 196, 6, 3, 58, 0.22); wheels(110, 340, '51'); label(140, 330, 'TV CHANNEL'); for (let k = 0; k < 5; k++) rr(250 + k * 56, 318, 44, 86, 4, '#e8e6dc', '#55554f');
  plate(1488, 170, 500, 250); lamps(1512, 196, 6, 3, 58, 0.3); wheels(1540, 340, '5024'); wheels(1740, 340, '24'); label(1772, 330, 'TV CHANNEL');
  /* the picture tube, centre, between its handles */
  plate(580, 170, 888, 700); handle(602, 400, 230); handle(1426, 400, 230);
  wheels(700, 186, '3210'); lamps(900, 184, 4, 2, 54, 0.4); wheels(1250, 186, '31'); label(1024, 846, 'POLARITY          CONTRAST          BRIGHTNESS');
  knob(880, 846, 11); knob(1024, 826, 11); knob(1170, 846, 11);
  rr(642, 250, 764, 566, 70, '#0d0d0d', '#000');                                       /* the bezel; the picture goes in it */
  /* the side panels under the upper row */
  plate(60, 440, 500, 430); lamps(84, 470, 5, 5, 70, 0.3); knob(140, 800, 14); knob(210, 800, 14); label(140, 826, 'POLARITY'); label(210, 826, 'CONTRAST'); handle(520, 520, 200);
  plate(1488, 440, 500, 430); lamps(1512, 470, 5, 5, 70, 0.45); x.fillStyle = '#e8e6dc'; x.beginPath(); x.arc(1900, 800, 34, 0, 7); x.fill(); x.strokeStyle = '#777'; x.stroke(); knob(1900, 800, 10); knob(1560, 800, 14);
  /* the keyboard shelf */
  plate(60, 890, 1928, 150); lamps(100, 912, 9, 3, 52, 0.25); lamps(700, 912, 11, 3, 52, 0.55); lamps(1420, 912, 9, 3, 52, 0.35);
  /* the desk */
  const d = x.createLinearGradient(0, 1050, 0, H); d.addColorStop(0, '#e2e0d8'); d.addColorStop(1, '#a9a8a2'); x.fillStyle = d; x.fillRect(0, 1050, W, H - 1050);
  /* the room is dark: the light falls off away from the tube */
  const v = x.createRadialGradient(W / 2, 520, 300, W / 2, 560, 1250); v.addColorStop(0, 'rgba(0,0,0,0)'); v.addColorStop(1, 'rgba(0,0,0,0.78)'); x.fillStyle = v; x.fillRect(0, 0, W, H);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 8; return t;
}

export function makeConsole(filmTexture) {
  const group = new THREE.Group();
  const CW = 2.6, CH = 1.4625;
  const face = new THREE.Mesh(new THREE.PlaneGeometry(CW, CH), new THREE.MeshBasicMaterial({ map: consoleTexture(), toneMapped: false }));
  group.add(face);
  /* the film on the tube: muted colour, scan lines, soft rounded corners, a little noise */
  const film = new THREE.ShaderMaterial({
    uniforms: { map: { value: filmTexture }, uT: { value: 0 }, uOn: { value: 0 }, uGain: { value: 1 } },
    vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
    fragmentShader: `
      uniform sampler2D map; uniform float uT, uOn, uGain; varying vec2 vUv;
      float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
      void main() {
        vec2 p = vUv * 2.0 - 1.0;
        /* the tube's rounded picture */
        vec2 q = abs(p) - vec2(0.80, 0.76); float edge = length(max(q, 0.0)) - 0.16;
        if (edge > 0.0) discard;
        vec2 uv = 0.5 + 0.5 * p * (1.0 + 0.04 * dot(p, p));
        vec3 c = texture2D(map, uv).rgb * uGain;
        c = c / (1.0 + c) * 1.35;                                   /* the scene is linear light: bring it into range */
        c = pow(c, vec3(1.0 / 2.2));
        float l = dot(c, vec3(0.3, 0.59, 0.11));
        c = mix(vec3(l), c, 0.42) * vec3(1.03, 1.0, 0.93);            /* muted, a little warm */
        c *= 0.9 + 0.1 * sin(uv.y * 3.14159 * 400.0);                 /* scan lines */
        c += (hash(uv * 480.0 + uT) - 0.5) * 0.05;
        c *= 1.0 - 0.35 * dot(p, p) * 0.5;
        c *= smoothstep(0.0, -0.04, edge) * uOn;
        gl_FragColor = vec4(c, 1.0);
      }` });
  const tube = new THREE.Mesh(new THREE.PlaneGeometry(SCREEN.w, SCREEN.h), film); tube.position.set(SCREEN.x, SCREEN.y, 0.004); group.add(tube);
  /* the closing words, as the tube's data displays were: white letters on blue */
  const cc = document.createElement('canvas'); cc.width = 1024; cc.height = 768; const cx = cc.getContext('2d'), ct = new THREE.CanvasTexture(cc); ct.colorSpace = THREE.SRGBColorSpace;
  const words = new THREE.Mesh(new THREE.PlaneGeometry(SCREEN.w * 0.8, SCREEN.h * 0.76), new THREE.MeshBasicMaterial({ map: ct, transparent: true, opacity: 0, toneMapped: false }));
  words.position.set(SCREEN.x, SCREEN.y, 0.008); group.add(words);
  for (const m of [face, tube, words]) m.frustumCulled = false;
  return {
    group, film,
    update(t, on) { film.uniforms.uT.value = t % 10; film.uniforms.uOn.value = on; },
    caption(lines, k) {
      if (lines && words.userData.txt !== lines.join('|')) {
        words.userData.txt = lines.join('|');
        cx.fillStyle = '#1f4f9a'; cx.fillRect(0, 0, 1024, 768); cx.fillStyle = '#eef3ff'; cx.textAlign = 'center'; cx.textBaseline = 'middle';
        lines.forEach((s, i) => { cx.font = (i === 0 ? 'bold 58px' : '46px') + ' "Courier New", monospace'; cx.fillText(s, 512, 384 + (i - (lines.length - 1) / 2) * 92); });
        ct.needsUpdate = true;
      }
      words.material.opacity = Math.max(0, Math.min(1, k)); words.visible = k > 0.01;
    }
  };
}

/* motion kept for the film: frames of numbers at their times; at(t) gives the frame at or before t */
export function makeRecorder() {
  const frames = [];
  return {
    frames,
    get length() { return frames.length ? frames[frames.length - 1].t : 0; },
    clear() { frames.length = 0; },
    add(t, data) { frames.push({ t, data: Float32Array.from(data) }); },
    at(t) { if (!frames.length) return null; let i = 0; while (i < frames.length - 1 && frames[i + 1].t <= t) i++; return frames[i].data; }
  };
}
