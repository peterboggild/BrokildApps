/* The panel over your left hand: hidden until you press the left stick (owner's headset test: always there, it
 * spoiled being there; A and B are faster and slower, X and Y the cheat window and outside). A press shows the
 * status, again the settings (settings.js; the left stick chooses and changes), a third time puts it away. The same in every chapter. The status: where you are in the mission and
 * what comes next, the figures that matter now (heights, distances, speed), the mission clock and the time warp, the
 * buttons, and the frame rate in small type. Each chapter fills it with its own info:
 *
 *   wrist.draw(now, { chapter, now, next: [name, GET] | null, after, get, warp, rows: [[label, value], ...],
 *                     alert, keys: [[button, what], ...], fps })
 *
 * At the end of a chapter the chapter shows it by itself, as it carries the way on (A).
 * Hand it to the left grip (controllers.grips.left.add(wrist.mesh)). */
import * as THREE from 'three';
import { settings, DEFS } from './settings.js?v=39';

export const fmtGET = s => { const n = s < 0, a = Math.abs(s), h = Math.floor(a / 3600), m = Math.floor(a % 3600 / 60), x = Math.floor(a % 60);
  return (n ? '-' : '') + h + ':' + String(m).padStart(2, '0') + ':' + String(x).padStart(2, '0'); };
export const fmtDist = m => m >= 1e7 ? Math.round(m / 1000).toLocaleString('en-US') + ' km' : m >= 1e5 ? (m / 1000).toFixed(0) + ' km' : m >= 2000 ? (m / 1000).toFixed(1) + ' km' : m.toFixed(0) + ' m';
export const fmtSpeed = v => v >= 1000 ? (v / 1000).toFixed(2) + ' km/s' : v.toFixed(v < 10 ? 1 : 0) + ' m/s';
/* the first of [GET, name] events still to come */
export const nextOf = (events, t) => { for (const [at, name] of events) if (at > t) return [name, at]; return null; };

/* keys: the settings this chapter offers (keys of DEFS) */
export function makeWrist(keys = Object.keys(DEFS)) {
  const W = 640, H = 600;
  const c = document.createElement('canvas'); c.width = W; c.height = H;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.2, 0.1875), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false, depthWrite: false }));
  mesh.renderOrder = 999; mesh.visible = false;
  /* above the controller, tipped towards your eyes */
  mesh.position.set(0, 0.13, -0.06); mesh.rotation.set(-0.85, 0, 0);
  let last = 0, cursor = 0;
  const stick = { y: 0, x: 0 };
  const w = {
    mesh, on: false, page: 'status',
    /* the left stick pressed: status, settings, away */
    press() { if (!w.on) { w.on = true; w.page = 'status'; } else if (w.page === 'status') w.page = 'settings'; else w.on = false; last = 0; },
    toggle() { w.press(); },
    show(on) { if (on !== w.on || w.page !== 'status') { w.on = on; w.page = 'status'; last = 0; } },
    /* while the settings are open the left stick is theirs */
    get capturesStick() { return w.on && w.page === 'settings'; },
    stick(x, y) {
      if (!w.capturesStick) { stick.x = stick.y = 0; return; }
      const ey = Math.abs(y) > 0.6 ? Math.sign(y) : 0, ex = Math.abs(x) > 0.6 ? Math.sign(x) : 0;
      if (ey && ey !== stick.y) { cursor = (cursor + ey + keys.length) % keys.length; last = 0; }
      if (ex && ex !== stick.x) { settings.step(keys[cursor], ex); last = 0; }
      stick.y = Math.abs(y) < 0.3 ? 0 : ey || stick.y; stick.x = Math.abs(x) < 0.3 ? 0 : ex || stick.x;
    },
    draw(now, info) {
      if (!w.on || !info || now - last < 250) return; last = now;
      g.clearRect(0, 0, W, H);
      g.fillStyle = 'rgba(8,9,10,0.86)'; g.beginPath(); g.roundRect(0, 0, W, H, 22); g.fill();
      g.strokeStyle = 'rgba(200,200,190,0.25)'; g.lineWidth = 2; g.stroke();
      let y = 44;
      const text = (s, x, size, color, bold) => { g.font = `${bold ? 'bold ' : ''}${size}px system-ui, Segoe UI, Arial, sans-serif`; g.fillStyle = color; g.fillText(s, x, y); };
      g.textAlign = 'left';
      if (w.page === 'settings') {
        text('Settings', 24, 36, '#f2f0e8', true);
        y += 34; text('left stick: up/down choose, left/right change; press: close', 24, 19, '#8d8a82');
        y += 10;
        keys.forEach((k, i) => {
          y += 52;
          if (i === cursor) { g.fillStyle = 'rgba(240,208,112,0.16)'; g.fillRect(12, y - 34, W - 24, 46); }
          text(DEFS[k].label, 24, 23, i === cursor ? '#f0d070' : '#9aa3ad', i === cursor);
          g.textAlign = 'right'; text((i === cursor ? '<  ' : '') + settings.text(k) + (i === cursor ? '  >' : ''), W - 24, 24, '#e8e6df', true); g.textAlign = 'left';
        });
        y = H - 22; text('Resolution: on the start page (needs a new start).', 24, 18, '#77746d');
        tex.needsUpdate = true; return;
      }
      text(info.chapter || '', 24, 26, '#9aa3ad', true);
      y += 44; text(info.now || '', 24, 36, '#f2f0e8', true);
      if (info.next) {
        const [name, at] = info.next, dt = at - info.get;
        y += 36; text('Next: ' + name + (isFinite(dt) ? '  in ' + fmtGET(dt).replace(/^0:/, '') : ''), 24, 25, '#c8c4b8');
      }
      if (info.after) { y += 32; text('Then: ' + info.after, 24, 22, '#8d8a82'); }
      /* the clock */
      y += 50; text('GET ' + fmtGET(info.get), 24, 34, '#f0d070', true);
      if (info.warp > 1) { g.textAlign = 'right'; text('time x' + info.warp.toLocaleString('en-US'), W - 24, 28, '#f0a060', true); g.textAlign = 'left'; }
      /* the figures */
      y += 8;
      for (const [label, value] of info.rows || []) {
        y += 38; text(label, 24, 25, '#9aa3ad'); g.textAlign = 'right'; text(value, W - 24, 28, '#e8e6df', true); g.textAlign = 'left';
      }
      if (info.alert) { y += 46; text(info.alert, 24, 27, '#f0d070', true); }
      /* the buttons */
      y += 22; g.fillStyle = 'rgba(200,200,190,0.18)'; g.fillRect(24, y, W - 48, 2);
      const btns = info.keys || [], col = Math.ceil(btns.length / 2);
      const y0 = y;
      btns.forEach(([k, what], i) => {
        const x = i < col ? 24 : W / 2 + 8; y = y0 + 30 + (i % col) * 28;
        text(k, x, 20, '#f0d070', true); text(what, x + 86, 20, '#c8c4b8');
      });
      y = H - 16; g.textAlign = 'right';
      text((info.fps || 0).toFixed(0) + ' fps', W - 24, 18, info.fps > 68 ? '#7fb880' : info.fps > 55 ? '#c8a850' : '#d07060');
      g.textAlign = 'left';
      tex.needsUpdate = true;
    }
  };
  return w;
}

/* A short message in front of you for a few seconds: why the clock stopped, or why it will not run faster. Add
 * toast.mesh to the camera's parent (the head); show it only in the cabin pass.
 *
 * Messages are a setting ('Messages on screen'), off unless you turn them on (the owner found them distracting):
 * hints and the radio's words in text are then not shown. say(text, secs, true) is for the few things you must be
 * told whatever the setting (why a trigger did nothing, that the camera is recording). Long messages go on two or
 * three lines at their natural width (they were squeezed on to one, hard to read). */
export function makeToast() {
  const CW = 1024, CH = 224, LH = 50;
  const c = document.createElement('canvas'); c.width = CW; c.height = CH;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.52, 0.52 * CH / CW), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false, depthWrite: false, opacity: 0 }));
  mesh.renderOrder = 1001; mesh.visible = false;
  const T = { mesh, k: 0, left: 0, text: '', lines: 0, shown: 0, skipped: 0,
    say(s, secs = 3, must = false) {
      if (!must && !settings.get('text')) { T.skipped++; return; }
      T.left = secs; if (s === T.text) return; T.text = s; T.shown++;
      g.font = 'bold 38px system-ui, Segoe UI, Arial, sans-serif'; g.textAlign = 'center'; g.textBaseline = 'middle';
      /* broken into lines that fit, as evenly as they will go */
      const words = String(s).split(' '), total = g.measureText(s).width, n = Math.min(3, Math.max(1, Math.ceil(total / 900))), want = total / n + 30, lines = [];
      let cur = '';
      for (const w of words) { const t = cur ? cur + ' ' + w : w; if (cur && g.measureText(t).width > want && lines.length < n - 1) { lines.push(cur); cur = w; } else cur = t; }
      lines.push(cur); T.lines = lines.length;
      const wide = Math.min(CW - 8, Math.max(...lines.map(l => g.measureText(l).width)) + 70), high = lines.length * LH + 34, y0 = (CH - high) / 2;
      g.clearRect(0, 0, CW, CH); g.fillStyle = 'rgba(8,9,10,0.82)'; g.beginPath(); g.roundRect((CW - wide) / 2, y0, wide, high, 24); g.fill();
      g.fillStyle = '#f0e6c8'; lines.forEach((l, i) => g.fillText(l, CW / 2, y0 + 17 + LH * (i + 0.5), CW - 40));
      tex.needsUpdate = true;
    },
    /* every frame: hold it 0.9 m ahead of your eyes, a little low */
    update(dt, camera) {
      T.left -= dt; T.k += ((T.left > 0 ? 1 : 0) - T.k) * Math.min(1, dt * 6);
      mesh.material.opacity = T.k;
      mesh.position.set(0, -0.14, -0.9).applyQuaternion(camera.quaternion).add(camera.position); mesh.quaternion.copy(camera.quaternion);
    }
  };
  return T;
}

/* Fast forward, in the corner of your eye: a small clock low in the view (it follows your head) while time runs
 * faster, its hand going round faster the faster it runs, and the power of ten beside it (1 for x10 ... 4 for
 * x10,000; other speeds as x4, x16). Add badge.mesh to the head; badge.update(dt, camera, warp) every frame; show it
 * only in the cabin pass. */
export function makeWarpBadge() {
  const g = new THREE.Group(); g.visible = false;
  const M = (o = {}) => new THREE.MeshBasicMaterial({ color: 0xf0e8d0, transparent: true, opacity: 0.8, depthTest: false, depthWrite: false, ...o });
  const parts = [];
  const add = (geo, mat, x = 0, y = 0) => { const m = new THREE.Mesh(geo, mat); m.position.set(x, y, 0); m.renderOrder = 1002; g.add(m); parts.push(m); return m; };
  add(new THREE.RingGeometry(0.0105, 0.0122, 40), M());
  for (let k = 0; k < 12; k++) { const a = k / 12 * Math.PI * 2, t = add(new THREE.PlaneGeometry(0.0009, k % 3 ? 0.0016 : 0.0028), M()); t.position.set(Math.sin(a) * 0.0092, Math.cos(a) * 0.0092, 0); t.rotation.z = -a; }
  const hand = new THREE.Group(); g.add(hand);
  const hg = new THREE.PlaneGeometry(0.0012, 0.0085); hg.translate(0, 0.0038, 0);
  const hm = new THREE.Mesh(hg, M({ color: 0xffd27a, opacity: 0.95 })); hm.renderOrder = 1003; hand.add(hm); parts.push(hm);
  const c = document.createElement('canvas'); c.width = 128; c.height = 64; const x = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const label = add(new THREE.PlaneGeometry(0.03, 0.015), M({ map: tex, color: 0xffffff }), 0.026, 0);
  let shown = -1, ang = 0;
  const B = {
    mesh: g, want: false, get on() { return B.want; },
    update(dt, camera, warp) {
      B.want = warp > 1;
      if (!B.want) return;
      const e = Math.log10(warp);
      ang -= dt * Math.PI * 2 * 0.25 * Math.pow(2, e - 1);           /* x10: a turn in 4 s ... x10,000: two turns a second */
      hand.rotation.z = ang;
      if (warp !== shown) {
        shown = warp; x.clearRect(0, 0, 128, 64); x.fillStyle = '#f0e8d0'; x.font = 'bold 50px system-ui, Segoe UI, Arial, sans-serif'; x.textBaseline = 'middle';
        const pow = Math.abs(e - Math.round(e)) < 1e-6;
        x.fillText(pow ? String(Math.round(e)) : 'x' + warp, 4, 34); tex.needsUpdate = true;
      }
      /* low in the view, a little right of the middle, 0.6 m out */
      g.position.set(0.05, -0.2, -0.6).applyQuaternion(camera.quaternion).add(camera.position); g.quaternion.copy(camera.quaternion);
    }
  };
  return B;
}

/* A chapter's card, in front of you: its number and title for a few seconds when it starts, and at its end the way
 * on: A continues into the next chapter (staying in VR where the browser allows it), B takes a break. It follows your
 * head gently, not rigidly. Add card.mesh to the head; card.update(dt, camera) every frame; show it only in the cabin
 * pass. card.show({ title, sub, a, b }, seconds): a and b are the two buttons' words (none: just a title). */
export function makeCard() {
  const W = 1024, H = 420, c = document.createElement('canvas'); c.width = W; c.height = H;
  const g = c.getContext('2d'), tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(0.84, 0.3445), new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthTest: false, depthWrite: false, opacity: 0 }));
  mesh.renderOrder = 1004; mesh.visible = false;
  const want = new THREE.Vector3(), F = 'system-ui, Segoe UI, Arial, sans-serif';
  let placed = false;
  const C = { mesh, k: 0, left: 0, on: false,
    show(o, secs = 1e9) {
      C.left = secs; C.on = true; placed = false;
      g.clearRect(0, 0, W, H); g.fillStyle = 'rgba(8,9,10,0.88)'; g.beginPath(); g.roundRect(4, 4, W - 8, H - 8, 30); g.fill();
      g.strokeStyle = 'rgba(240,208,112,0.5)'; g.lineWidth = 3; g.stroke();
      g.textAlign = 'center'; g.fillStyle = '#f0d070'; g.font = 'bold 42px ' + F; g.fillText(o.title || '', W / 2, 92, W - 60);
      g.fillStyle = '#f2f0e8'; g.font = 'bold 74px ' + F; g.fillText(o.sub || '', W / 2, o.a ? 190 : 230, W - 60);
      if (o.a) {
        const btn = (x, key, text, hot) => { g.fillStyle = hot ? 'rgba(240,208,112,0.22)' : 'rgba(255,255,255,0.08)'; g.beginPath(); g.roundRect(x, 262, 452, 112, 20); g.fill();
          g.fillStyle = '#f0d070'; g.font = 'bold 52px ' + F; g.textAlign = 'left'; g.fillText(key, x + 26, 337);
          g.fillStyle = '#f2f0e8'; g.font = '34px ' + F; g.fillText(text, x + 84, 332, 350); };
        btn(44, 'A', o.a, true); btn(528, 'B', o.b || 'Take a break', false);
      }
      tex.needsUpdate = true;
    },
    hide() { C.left = 0; C.on = false; },
    update(dt, camera) {
      C.left -= dt; if (C.left <= 0) C.on = false;
      C.k += ((C.on ? 1 : 0) - C.k) * Math.min(1, dt * 5);
      mesh.material.opacity = C.k;
      if (C.k < 0.01) return;
      want.set(0, -0.02, -1.25).applyQuaternion(camera.quaternion).add(camera.position);
      if (!placed) { mesh.position.copy(want); mesh.quaternion.copy(camera.quaternion); placed = true; }
      mesh.position.lerp(want, Math.min(1, dt * 2.5)); mesh.quaternion.slerp(camera.quaternion, Math.min(1, dt * 2.5));
    }
  };
  return C;
}
