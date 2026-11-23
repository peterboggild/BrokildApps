/* The VR panel (spec §11.1): the same UI schema as the desktop (ui/schema.js), drawn on a canvas texture on a
 * 0.80 x 0.54 m tablet. Tabs, a scrolling column of rows, sliders with - and + (auto-repeat), chips, readouts and
 * headings. Hit testing in canvas pixels; the caller maps a controller ray to (px, py). */
import * as THREE from 'three';
import { TABS, LEVELS } from './schema.js?v=8';
import { t, fmt } from '../i18n.js?v=8';

export const PW = 0.8, PH = 0.54;          /* metres */
const W = 1200, H = 810;
const TAB_H = 78, ROW = 74, PAD = 26, TOP = TAB_H + 64;
const GAS_FLOOR = 1e-7;

/* slider position x in [0, 1] <-> value, as on the desktop (ui/dom.js) */
function toValue(row, x) {
  if (row.log) return row.min * (row.max / row.min) ** x;
  if (row.gas) return x <= 0.002 ? 0 : GAS_FLOOR * (row.max / GAS_FLOOR) ** ((x - 0.002) / 0.998);
  if (row.sqrt) return row.min + (row.max - row.min) * x * x;
  const v = row.min + (row.max - row.min) * x;
  return row.step ? Math.round(v / row.step) * row.step : v;
}
function toPos(row, v) {
  if (row.log) return Math.log(Math.max(v, row.min) / row.min) / Math.log(row.max / row.min);
  if (row.gas) return v <= 0 ? 0 : 0.002 + 0.998 * Math.log(Math.max(v, GAS_FLOOR) / GAS_FLOOR) / Math.log(row.max / GAS_FLOOR);
  if (row.sqrt) return Math.sqrt((v - row.min) / (row.max - row.min));
  return (v - row.min) / (row.max - row.min);
}
const clamp01 = (x) => Math.max(0, Math.min(1, x));
const strip = (html) => String(html).replace(/<[^>]*>/g, '');

export function createVRPanel(S) {
  const canvas = document.createElement('canvas');
  canvas.width = W; canvas.height = H;
  const ctx = canvas.getContext('2d');
  const tex = new THREE.CanvasTexture(canvas);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 4;
  const mesh = new THREE.Mesh(new THREE.PlaneGeometry(PW, PH), new THREE.MeshBasicMaterial({ map: tex, transparent: true, toneMapped: false }));
  mesh.renderOrder = 20;
  const state = { scroll: 0, widgets: [], hover: null, cap: null, dirty: true, repeat: null };

  function getPath(path) {
    const [head, ...rest] = path.split('.');
    let o = head === 'star' ? S.design.star : S.planet();
    for (const k of rest) o = o == null ? o : o[k];
    return o;
  }
  function rows() {
    const level = LEVELS[S.design.level] || 1;
    const isHz = S.planet().role === 'hz';
    if (S.tab === 'vr') return vrRows();
    const tab = TABS.find((x) => x.id === S.tab) || TABS[0];
    const out = [];
    for (const row of tab.rows) {
      if ((row.level || 1) > level || (row.hz && !isHz) || (row.other && isHz)) continue;
      if (tab.hzOnly && !isHz && row.kind !== 'planets') continue;
      if (['slider', 'chips', 'readout', 'heading', 'planets', 'warn'].includes(row.kind)) out.push(row);
      if (row.kind === 'action') out.push({ kind: 'buttons', opts: [{ label: t(row.label), act: () => row.act(S) }] });
      if (row.kind === 'moons') out.push(...moonRows());
      if (row.kind === 'system') out.push(...systemRows());
      if (row.kind === 'chart') out.push({ kind: 'note', text: t('vr.chartOnPc') });
    }
    return out;
  }

  /* moons: per moon its mass and distance sliders and a remove button (second press confirms); add up to three */
  function moonRows() {
    const out = [];
    const P = S.planet();
    (P.moons || []).forEach((m, i) => {
      out.push({ kind: 'title', text: t('moon.n', { n: String(i + 1) }) });
      out.push({ kind: 'slider', path: `planet.moons.${i}.massMoon`, labelKey: 'p.moon.massMoon', min: 0.081, max: 8.13, log: true, unit: 'Moon', digits: 3 });
      out.push({ kind: 'slider', path: `planet.moons.${i}.aEarthRadii`, labelKey: 'p.moon.aEarthRadii', min: 1.5, max: 1000, log: true, unit: '', digits: 3 });
      out.push({ kind: 'buttons', opts: [{ id: 'moonrm' + i, label: t('moon.remove'), danger: true, act: () => S.structural(() => S.planet().moons.splice(i, 1)) }] });
    });
    if ((P.moons || []).length < 3) out.push({ kind: 'buttons', opts: [{ label: t('moon.add'), act: () => S.structural(() => S.planet().moons.push({ massMoon: 1, aEarthRadii: 40 })) }] });
    return out;
  }
  /* the system: each planet with select, make habitable / other, remove; add a planet */
  function systemRows() {
    const out = [];
    const hzCount = S.design.planets.filter((p) => p.role === 'hz').length;
    S.design.planets.forEach((p, i) => {
      const opts = [{ label: p.name.slice(0, 14), on: i === S.sel, act: () => { S.sel = i; } }];
      if (p.role === 'other' && hzCount < 2) opts.push({ label: t('vr.makeHz'), act: () => S.structural(() => S.makeHz(i)) });
      if (p.role === 'hz') opts.push({ label: t('vr.makeOther'), act: () => S.structural(() => S.makeOther(i)) });
      if (S.design.planets.length > 1) opts.push({ id: 'plrm' + i, label: t('vr.remove'), danger: true, act: () => S.structural(() => S.removePlanet(i)) });
      out.push({ kind: 'buttons', opts });
    });
    if (S.design.planets.length < 8) out.push({ kind: 'buttons', opts: [{ label: t('planet.add'), act: () => S.structural(() => S.addPlanet()) }] });
    return out;
  }
  /* the VR tab: view settings and leaving VR */
  function vrRows() {
    const V = S.view.state;
    const c = (label, opts) => ({ kind: 'vrchips', label, opts });
    return [
      { kind: 'title', text: t('preset') },
      { kind: 'buttons', opts: ['solar', 'kdwarf'].map((k) => ({ label: t('preset.' + k), act: () => S.loadPreset(k) })) },
      { kind: 'buttons', opts: ['reddwarf', 'dryworld', 'hothouse'].map((k) => ({ label: t('preset.' + k), act: () => S.loadPreset(k) })) },
      { kind: 'buttons', opts: [{ label: t('act.toZone'), act: () => S.toZone() }] },
      c('v.cut', [{ label: t('yes'), on: V.cut, act: () => { V.cut = true; S.view.refresh(); } }, { label: t('no'), on: !V.cut, act: () => { V.cut = false; S.view.refresh(); } }]),
      c('v.spin', [{ label: t('yes'), on: V.spinOn, act: () => { V.spinOn = true; } }, { label: t('no'), on: !V.spinOn, act: () => { V.spinOn = false; } }]),
      c('vr.fill', [{ label: t('yes'), on: S.view.planet.fill, act: () => S.view.planet.setFill(true) }, { label: t('no'), on: !S.view.planet.fill, act: () => S.view.planet.setFill(false) }]),
      c('vr.turn', [{ label: t('vr.turnStep'), on: !(S.xr && S.xr.smoothTurn), act: () => S.xr && S.xr.setSmoothTurn(false) }, { label: t('vr.turnSmooth'), on: !!(S.xr && S.xr.smoothTurn), act: () => S.xr && S.xr.setSmoothTurn(true) }]),
      c('vr.sun', [{ label: t('vr.sunReset'), on: false, act: () => S.xr && S.xr.resetSun() }]),
      c('v.exag', [5, 10, 20].map((x) => ({ label: '×' + x, on: V.exag === x, act: () => { V.exag = x; S.view.refresh(); } }))),
      c('vr.quality', ['low', 'medium'].map((q) => ({ label: t('v.q.' + q), on: V.quality === q, act: () => S.view.setQuality(q, true) }))),
      { kind: 'note', text: t('vr.qualityNote') },
      c('level', ['explorer', 'student', 'scientist'].map((l) => ({ label: t('level.' + l), on: S.design.level === l, act: () => { S.design.level = l; S.changed(true); } }))),
      c('vr.leave', [{ label: t('vr.exit'), on: false, act: () => S.xr && S.xr.exit() }]),
      { kind: 'note', text: t('vr.help') },
    ];
  }
  function rr(x, y, w, h, r) { ctx.beginPath(); ctx.roundRect(x, y, w, h, r); }
  function draw() {
    state.widgets = [];
    ctx.clearRect(0, 0, W, H);
    rr(0, 0, W, H, 34); ctx.fillStyle = 'rgba(12,17,24,0.94)'; ctx.fill();
    ctx.lineWidth = 3; ctx.strokeStyle = '#2a3646'; ctx.stroke();
    /* tabs */
    /* tabs: shrink the type until all of them fit on the panel's width */
    const allTabs = [...TABS, { id: 'vr' }];
    const tabLabel = (tab) => (tab.id === 'vr' ? 'VR' : t('tab.' + tab.id));
    let fs = 30, pad = 34;
    for (; fs > 18; fs -= 1) {
      ctx.font = `600 ${fs}px system-ui, sans-serif`;
      const total = allTabs.reduce((sum, tb) => sum + ctx.measureText(tabLabel(tb)).width + pad + 10, 0);
      if (total <= W - 2 * PAD) break;
      pad = Math.max(18, pad - 1);
    }
    ctx.textBaseline = 'middle';
    let x = PAD;
    for (const tab of allTabs) {
      const label = tabLabel(tab);
      const w = ctx.measureText(label).width + pad;
      const on = S.tab === tab.id, hov = state.hover && state.hover.id === 'tab:' + tab.id;
      rr(x, 14, w, TAB_H - 22, 12); ctx.fillStyle = on ? '#1d3550' : hov ? '#1b2430' : '#151c26'; ctx.fill();
      ctx.strokeStyle = on ? '#6fb7ff' : '#2a3646'; ctx.lineWidth = 2; ctx.stroke();
      ctx.fillStyle = '#dfe7ef'; ctx.fillText(label, x + pad / 2, 14 + (TAB_H - 22) / 2);
      state.widgets.push({ id: 'tab:' + tab.id, x, y: 14, w, h: TAB_H - 22, act: () => { S.tab = tab.id; state.scroll = 0; } });
      x += w + 10;
    }
    /* close button and title */
    const P = S.derivedPlanet();
    ctx.font = '600 28px system-ui, sans-serif'; ctx.fillStyle = '#8a98a8';
    ctx.fillText(`${S.design.planets[S.sel] ? S.design.planets[S.sel].name : ''}`, PAD, TAB_H + 28);
    rr(W - 84, TAB_H + 6, 60, 46, 10); ctx.fillStyle = state.hover && state.hover.id === 'close' ? '#3a2020' : '#1a1418'; ctx.fill();
    ctx.fillStyle = '#f0a0a0'; ctx.font = '600 34px system-ui'; ctx.fillText('×', W - 64, TAB_H + 30);
    state.widgets.push({ id: 'close', x: W - 84, y: TAB_H + 6, w: 60, h: 46, act: () => S.xr && S.xr.togglePanel(false) });
    /* rows */
    const list = rows();
    const viewH = H - TOP - 20;
    const rowH = (r) => (r.kind === 'slider' ? ROW + 26 : r.kind === 'heading' || r.kind === 'note' || r.kind === 'title' ? 50 : ROW - 18);
    const contentH = list.reduce((s, r) => s + rowH(r), 0);
    state.scroll = Math.max(0, Math.min(state.scroll, Math.max(0, contentH - viewH)));
    ctx.save(); ctx.beginPath(); ctx.rect(0, TOP - 6, W - 70, viewH + 8); ctx.clip();
    let y = TOP - state.scroll;
    for (const row of list) {
      const h = rowH(row);
      if (y + h > TOP - 10 && y < TOP + viewH) drawRow(row, y, P);
      y += h;
    }
    ctx.restore();
    /* scroll buttons */
    if (contentH > viewH) {
      for (const [id, yy, d, ch] of [['up', TOP, -1, '▲'], ['down', H - 80, 1, '▼']]) {
        rr(W - 62, yy, 46, 60, 10); ctx.fillStyle = state.hover && state.hover.id === id ? '#24364c' : '#151c26'; ctx.fill();
        ctx.fillStyle = '#9fb3c8'; ctx.font = '30px system-ui'; ctx.fillText(ch, W - 52, yy + 30);
        state.widgets.push({ id, x: W - 62, y: yy, w: 46, h: 60, repeat: true, act: () => { state.scroll += d * 90; } });
      }
      const th = Math.max(40, viewH * viewH / contentH), ty = TOP + 66 + (viewH - 150 - th) * (state.scroll / Math.max(1, contentH - viewH));
      rr(W - 46, ty, 14, th, 7); ctx.fillStyle = '#3a4a5c'; ctx.fill();
    }
    tex.needsUpdate = true;
    state.dirty = false;
  }

  function drawRow(row, y, P) {
    const x0 = PAD, x1 = W - 90;
    ctx.textBaseline = 'middle';
    if (row.kind === 'heading' || row.kind === 'note') {
      ctx.font = row.kind === 'heading' ? '600 28px system-ui' : 'italic 24px system-ui';
      ctx.fillStyle = row.kind === 'heading' ? '#dfe7ef' : '#8a98a8';
      ctx.fillText(row.kind === 'heading' ? t(row.label) : row.text, x0, y + 25);
      return;
    }
    if (row.kind === 'warn') {
      let k = null; try { k = row.value(S.derived, P, S.design); } catch (e) { k = null; }
      if (!k) return;
      ctx.font = '22px system-ui'; ctx.fillStyle = '#f0d6a0'; ctx.fillText(t(k).slice(0, 90), x0, y + 26);
      return;
    }
    if (row.kind === 'readout') {
      let v = '–'; try { v = strip(row.value(S.derived, P, S.design)); } catch (e) { v = '–'; }
      ctx.font = '26px system-ui'; ctx.fillStyle = '#8a98a8'; ctx.fillText(t(row.label), x0, y + 26);
      ctx.fillStyle = '#dfe7ef'; ctx.textAlign = 'right'; ctx.fillText(v, x1, y + 26); ctx.textAlign = 'left';
      return;
    }
    if (row.kind === 'title') { ctx.font = '600 26px system-ui'; ctx.fillStyle = '#9fd0ff'; ctx.fillText(row.text, x0, y + 25); return; }
    if (row.kind === 'buttons') {
      ctx.font = '600 24px system-ui';
      let x = x0;
      row.opts.forEach((o, i) => {
        const id = 'btn:' + (o.id || o.label) + ':' + i;
        const armed = state.armed && state.armed.id === id && performance.now() - state.armed.t < 3000;
        const label = armed ? t('btn.confirm') : o.label;
        const w = Math.min(x1 - x, ctx.measureText(label).width + 30);
        if (w < 60) return;
        rr(x, y + 4, w, 46, 10);
        ctx.fillStyle = o.on ? '#1d3550' : armed ? '#4a1c1c' : state.hover && state.hover.id === id ? '#1b2430' : '#151c26'; ctx.fill();
        ctx.strokeStyle = o.on ? '#6fb7ff' : o.danger ? '#7a3a3a' : '#2a3646'; ctx.lineWidth = 2; ctx.stroke();
        ctx.fillStyle = o.danger ? '#f0a0a0' : '#dfe7ef'; ctx.fillText(label, x + 15, y + 27);
        state.widgets.push({ id, x, y: y + 4, w, h: 46, act: () => {
          if (o.danger && !armed) { state.armed = { id, t: performance.now() }; return; }
          state.armed = null; o.act(); if (S.redrawAll) S.redrawAll();
        } });
        x += w + 8;
      });
      return;
    }
    if (row.kind === 'vrchips') {
      ctx.font = '26px system-ui'; ctx.fillStyle = '#8a98a8'; ctx.fillText(t(row.label), x0, y + 27);
      ctx.font = '600 24px system-ui';
      let x = x0 + 300;
      row.opts.forEach((o, i) => {
        const w = ctx.measureText(o.label).width + 30, id = 'vr:' + row.label + ':' + i;
        rr(x, y + 4, w, 46, 10); ctx.fillStyle = o.on ? '#1d3550' : state.hover && state.hover.id === id ? '#1b2430' : '#151c26'; ctx.fill();
        ctx.strokeStyle = o.on ? '#6fb7ff' : '#2a3646'; ctx.lineWidth = 2; ctx.stroke();
        ctx.fillStyle = '#dfe7ef'; ctx.fillText(o.label, x + 15, y + 27);
        state.widgets.push({ id, x, y: y + 4, w, h: 46, act: o.act });
        x += w + 8;
      });
      return;
    }
    if (row.kind === 'planets' || row.kind === 'chips') {
      const opts = row.kind === 'planets' ? S.design.planets.map((p, i) => ({ label: p.name, on: i === S.sel, act: () => { S.sel = i; } }))
        : row.options.map((o) => ({ label: t(row.label + '.' + o), on: getPath(row.path) === o, act: () => S.set(row.path, o) }));
      ctx.font = '600 24px system-ui';
      let x = x0;
      opts.forEach((o, i) => {
        const w = Math.min(260, ctx.measureText(o.label).width + 30);
        if (x + w > x1) return;
        const id = row.kind + ':' + (row.path || '') + ':' + i;
        rr(x, y + 4, w, 46, 10); ctx.fillStyle = o.on ? '#1d3550' : state.hover && state.hover.id === id ? '#1b2430' : '#151c26'; ctx.fill();
        ctx.strokeStyle = o.on ? '#6fb7ff' : '#2a3646'; ctx.lineWidth = 2; ctx.stroke();
        ctx.fillStyle = '#dfe7ef'; ctx.fillText(o.label.slice(0, 18), x + 15, y + 27);
        state.widgets.push({ id, x, y: y + 4, w, h: 46, act: () => { o.act(); if (S.redrawAll) S.redrawAll(); } });
        x += w + 8;
      });
      return;
    }
    if (row.kind === 'slider') {
      const v = getPath(row.path);
      const label = row.labelKey ? t(row.labelKey) : t('p.' + row.path.replace('planet.', ''));
      ctx.font = '26px system-ui'; ctx.fillStyle = '#dfe7ef'; ctx.fillText(label, x0, y + 20);
      const vs = row.pct ? `${fmt(v * 100, row.digits || 2)} %` : `${fmt(v, row.digits || 2)}${row.unit ? ' ' + t('u.' + row.unit) : ''}`;
      ctx.fillStyle = '#6fb7ff'; ctx.textAlign = 'right'; ctx.fillText(vs, x1, y + 20); ctx.textAlign = 'left';
      const ty = y + 66, bx = x0 + 70, bw = x1 - x0 - 140;
      for (const [id, xx, ch, d] of [['minus', x0, '−', -1], ['plus', x1 - 56, '+', 1]]) {
        const wid = id + ':' + row.path;
        rr(xx, ty - 26, 56, 52, 10); ctx.fillStyle = state.hover && state.hover.id === wid ? '#24364c' : '#151c26'; ctx.fill();
        ctx.strokeStyle = '#2a3646'; ctx.stroke();
        ctx.fillStyle = '#dfe7ef'; ctx.font = '34px system-ui'; ctx.fillText(ch, xx + 17, ty);
        state.widgets.push({ id: wid, x: xx, y: ty - 26, w: 56, h: 52, repeat: true, act: () => {
          let nv;
          if (row.log) nv = getPath(row.path) * (1 + d * 0.005);      /* log sliders: 0.5 % per press */
          else {
            const pos = clamp01(toPos(row, getPath(row.path)) + d * 0.006);
            nv = toValue(row, pos);
            if (nv === getPath(row.path)) nv = toValue(row, clamp01(pos + d * 0.006));
          }
          S.set(row.path, Math.max(row.min, Math.min(row.max, nv)));
        } });
      }
      rr(bx, ty - 5, bw, 10, 5); ctx.fillStyle = '#26303d'; ctx.fill();
      const p = clamp01(toPos(row, v));
      rr(bx, ty - 5, bw * p, 10, 5); ctx.fillStyle = '#6fb7ff'; ctx.fill();
      ctx.beginPath(); ctx.arc(bx + bw * p, ty, 17, 0, Math.PI * 2); ctx.fillStyle = '#dfe7ef'; ctx.fill();
      state.widgets.push({ id: 'slider:' + row.path, x: bx - 18, y: ty - 30, w: bw + 36, h: 60, drag: (px) => {
        S.set(row.path, Math.max(row.min, Math.min(row.max, toValue(row, clamp01((px - bx) / bw)))));
      } });
    }
  }

  const find = (px, py) => state.widgets.find((w) => px >= w.x && px <= w.x + w.w && py >= w.y && py <= w.y + w.h) || null;
  return {
    mesh, PW, PH, W, H, state,
    redraw() { draw(); },
    markDirty() { state.dirty = true; },
    tick(now) {
      if (state.repeat && now > state.repeat.next) { state.repeat.w.act(); state.repeat.next = now + Math.max(45, state.repeat.gap *= 0.8); state.dirty = true; }
      if (state.dirty) draw();
    },
    hover(px, py) { const w = px == null ? null : find(px, py); if ((w && w.id) !== (state.hover && state.hover.id)) { state.hover = w; state.dirty = true; } return w; },
    down(px, py, now) {
      const w = find(px, py);
      if (!w) return false;
      if (w.drag) { state.cap = w; w.drag(px); }
      else { w.act(); if (w.repeat) state.repeat = { w, next: now + 380, gap: 380 }; }
      state.dirty = true;
      return true;
    },
    move(px) { if (state.cap) { state.cap.drag(px); state.dirty = true; } },
    up() { state.cap = null; state.repeat = null; state.dirty = true; },
    /* for tests: the canvas centre of a widget whose id starts with the given text */
    widgetPx(prefix) { draw(); const w = state.widgets.find((x) => x.id.startsWith(prefix)); return w ? [w.x + w.w / 2, w.y + w.h / 2] : null; },
  };
}
