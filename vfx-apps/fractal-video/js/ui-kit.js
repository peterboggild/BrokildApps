/* Small DOM toolkit: element builder, icons, and the standard control rows used by every panel.
   Controls never rebuild themselves while being dragged; they report onInput (live) and
   onCommit (released / confirmed, creates an undo step). */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});

  function h(tag, attrs, ...children) {
    const el = document.createElement(tag);
    const a = attrs || {};
    Object.keys(a).forEach((k) => {
      const v = a[k];
      if (v === undefined || v === null || v === false) return;
      if (k === 'class') el.className = v;
      else if (k === 'text') el.textContent = v;
      else if (k === 'html') el.innerHTML = v;
      else if (k === 'style' && typeof v === 'object') Object.assign(el.style, v);
      else if (k === 'on') Object.keys(v).forEach((ev) => el.addEventListener(ev, v[ev]));
      else if (k === 'dataset') Object.assign(el.dataset, v);
      else if (k in el && typeof v !== 'string') el[k] = v;
      else el.setAttribute(k, v === true ? '' : v);
    });
    children.flat().forEach((c) => {
      if (c === null || c === undefined || c === false) return;
      el.appendChild(typeof c === 'string' || typeof c === 'number' ? document.createTextNode(String(c)) : c);
    });
    return el;
  }

  const svg = (inner, vb) => `<svg viewBox="${vb || '0 0 16 16'}" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">${inner}</svg>`;
  const ICONS = {
    play: '<svg viewBox="0 0 16 16"><path d="M4.5 2.8v10.4L13 8z" fill="currentColor"/></svg>',
    pause: '<svg viewBox="0 0 16 16"><path d="M4 3h3v10H4zM9 3h3v10H9z" fill="currentColor"/></svg>',
    home: svg('<path d="M3.5 3v10M13 3.5 6.5 8l6.5 4.5z"/>'),
    plus: svg('<path d="M8 3v10M3 8h10"/>'),
    copy: svg('<rect x="5.5" y="5.5" width="8" height="8" rx="1.5"/><path d="M10.5 5.5V3.8c0-.7-.6-1.3-1.3-1.3H3.8c-.7 0-1.3.6-1.3 1.3v5.4c0 .7.6 1.3 1.3 1.3h1.7"/>'),
    trash: svg('<path d="M3 4.5h10M6.5 4.5V3h3v1.5M4.5 4.5l.6 8.5h5.8l.6-8.5"/>'),
    undo: svg('<path d="M5.5 6.5H10a3 3 0 0 1 0 6H6M5.5 6.5 8 4M5.5 6.5 8 9"/>'),
    redo: svg('<path d="M10.5 6.5H6a3 3 0 0 0 0 6h4M10.5 6.5 8 4M10.5 6.5 8 9"/>'),
    help: svg('<circle cx="8" cy="8" r="6"/><path d="M6.3 6.3a1.8 1.8 0 1 1 2.4 1.7c-.5.2-.7.6-.7 1.1v.4M8 11.6v.1"/>'),
    close: svg('<path d="m4 4 8 8M12 4l-8 8"/>'),
    dice: svg('<rect x="2.5" y="2.5" width="11" height="11" rx="2.5"/><circle cx="5.8" cy="5.8" r=".6" fill="currentColor"/><circle cx="10.2" cy="10.2" r=".6" fill="currentColor"/><circle cx="8" cy="8" r=".6" fill="currentColor"/>'),
    reset: svg('<path d="M3 8a5 5 0 1 0 1.5-3.5M3 3v2.5h2.5"/>'),
    headphones: svg('<path d="M3 10V8a5 5 0 0 1 10 0v2"/><rect x="2.5" y="9.5" width="3" height="4" rx="1"/><rect x="10.5" y="9.5" width="3" height="4" rx="1"/>'),
    music: svg('<path d="M6 12V3.5l7-1.5V10.5"/><circle cx="4.5" cy="12" r="1.5"/><circle cx="11.5" cy="10.5" r="1.5"/>'),
    split: svg('<path d="M8 2v12M4 5 2 8l2 3M12 5l2 3-2 3"/>'),
    fit: svg('<path d="M2 5V2h3M11 2h3v3M14 11v3h-3M5 14H2v-3"/>'),
    zoomIn: svg('<circle cx="7" cy="7" r="4.5"/><path d="m10.5 10.5 3 3M5 7h4M7 5v4"/>'),
    zoomOut: svg('<circle cx="7" cy="7" r="4.5"/><path d="m10.5 10.5 3 3M5 7h4"/>'),
    follow: svg('<path d="M2 8h9M8 5l3 3-3 3M13.5 3v10"/>'),
    image: svg('<rect x="2" y="3" width="12" height="10" rx="1.5"/><circle cx="6" cy="6.5" r="1.2"/><path d="m2.5 12 4-4 3 3 2-2 2.5 2.5"/>'),
    film: svg('<rect x="2" y="3" width="12" height="10" rx="1.5"/><path d="M5 3v10M11 3v10M2 6h3M2 10h3M11 6h3M11 10h3"/>'),
    swap: svg('<path d="M4 5h9M10 2l3 3-3 3M12 11H3M6 8l-3 3 3 3"/>'),
    paste: svg('<rect x="3.5" y="3" width="9" height="11" rx="1.5"/><path d="M6 3V2h4v1"/>'),
    reverse: svg('<path d="M2 6h12M11 3l3 3-3 3M14 10H2M5 7l-3 3 3 3"/>'),
    spread: svg('<path d="M2 8h12M2 5v6M8 5v6M14 5v6"/>'),
    wand: svg('<path d="m3 13 7-7M11.5 2.5v2M10.5 3.5h2M13.5 6v1.5M12.75 6.75h1.5M8 2v1M7.5 2.5h1"/>'),
    route: svg('<circle cx="4" cy="4" r="1.5"/><circle cx="12" cy="12" r="1.5"/><path d="M5.5 4H9a2.5 2.5 0 0 1 0 5H7a2.5 2.5 0 0 0 0 5h3.5"/>')
  };

  function icon(name) {
    const span = document.createElement('span');
    span.className = 'ic';
    span.style.display = 'contents';
    span.innerHTML = ICONS[name] || '';
    return span;
  }

  function setIcon(el, name, label) {
    el.innerHTML = ICONS[name] || '';
    if (label) el.appendChild(document.createTextNode(label));
  }

  function decimals(step) {
    if (!step || step >= 1) return 0;
    return Math.min(4, Math.max(0, Math.ceil(-Math.log10(step) - 1e-9)));
  }

  // ---------- slider ----------
  /* opts: label, min, max, step, value, def, unit, hint, calm [a,b], log, bipolar, onInput(v), onCommit(v) */
  function slider(opts) {
    const o = Object.assign({ step: 0.01 }, opts);
    const dec = decimals(o.step);
    const useLog = !!o.log && o.min > 0;
    const toPos = (v) => (useLog ? (Math.log(v / o.min) / Math.log(o.max / o.min)) * 1000 : v);
    const fromPos = (x) => (useLog ? o.min * Math.pow(o.max / o.min, x / 1000) : x);
    const snap = (v) => {
      let x = Math.min(o.max, Math.max(o.min, Number(v)));
      if (!useLog && o.step) x = Math.round(x / o.step) * o.step;
      return Number(x.toFixed(dec + (useLog ? 2 : 0)));
    };
    const pct = (v) => ((toPos(v) - toPos(o.min)) / (toPos(o.max) - toPos(o.min))) * 100;

    const tip = `${o.label}${o.hint ? ' — ' + o.hint : ''}${o.def !== undefined ? '\nDouble-click the label to reset.' : ''}`;
    const label = h('span', { class: 'ctl-label', text: o.label + (o.unit ? ` (${o.unit})` : ''), title: tip });
    const range = h('input', {
      type: 'range',
      min: String(useLog ? 0 : o.min), max: String(useLog ? 1000 : o.max), step: String(useLog ? 1 : o.step),
      'aria-label': o.label
    });
    const num = h('input', { type: 'number', class: 'ctl-num', step: String(o.step), min: String(o.min), max: String(o.max), 'aria-label': o.label });
    const row = h('div', { class: 'ctl ctl-slider' }, label, range, num);
    let value = snap(o.value !== undefined ? o.value : o.min);

    function paint() {
      const p = Math.min(100, Math.max(0, pct(value)));
      if (o.bipolar) {
        const z = Math.min(100, Math.max(0, pct(0)));
        range.style.setProperty('--f0', Math.min(z, p) + '%');
        range.style.setProperty('--f1', Math.max(z, p) + '%');
      } else {
        range.style.setProperty('--f0', '0%');
        range.style.setProperty('--f1', p + '%');
      }
      if (o.calm) {
        range.style.setProperty('--ba', pct(o.calm[0]) + '%');
        range.style.setProperty('--bb', pct(o.calm[1]) + '%');
      }
    }
    function show() {
      range.value = String(toPos(value));
      if (document.activeElement !== num) num.value = value.toFixed(dec);
      paint();
    }
    show();

    range.addEventListener('input', () => {
      value = snap(fromPos(Number(range.value)));
      num.value = value.toFixed(dec);
      paint();
      if (o.onInput) o.onInput(value);
    });
    range.addEventListener('change', () => { if (o.onCommit) o.onCommit(value); });
    num.addEventListener('change', () => {
      if (num.value === '' || !Number.isFinite(Number(num.value))) { show(); return; }
      value = snap(num.value);
      show();
      if (o.onInput) o.onInput(value);
      if (o.onCommit) o.onCommit(value);
    });
    num.addEventListener('keydown', (e) => { if (e.key === 'Enter') num.blur(); });
    if (o.def !== undefined) {
      label.addEventListener('dblclick', () => {
        value = snap(o.def);
        show();
        if (o.onInput) o.onInput(value);
        if (o.onCommit) o.onCommit(value);
      });
    }
    // Drag on the label for fine adjustment (like a DAW knob).
    label.addEventListener('pointerdown', (e) => {
      if (e.button !== 0 || e.detail > 1) return;
      const startX = e.clientX;
      const startPos = toPos(value);
      const span = toPos(o.max) - toPos(o.min);
      let moved = false;
      const move = (ev) => {
        const dx = ev.clientX - startX;
        if (Math.abs(dx) > 2) moved = true;
        if (!moved) return;
        const fine = ev.shiftKey ? 0.1 : 1;
        value = snap(fromPos(Math.min(toPos(o.max), Math.max(toPos(o.min), startPos + (dx / 240) * span * fine))));
        show();
        if (o.onInput) o.onInput(value);
      };
      const up = () => {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
        if (moved && o.onCommit) o.onCommit(value);
      };
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
    });

    return {
      el: row,
      get: () => value,
      set: (v) => { value = snap(v); show(); },
      setDisabled: (d) => row.classList.toggle('disabled', !!d)
    };
  }

  // ---------- select ----------
  function select(opts) {
    const sel = h('select', { 'aria-label': opts.label });
    let lastGroup = null;
    let parent = sel;
    opts.options.forEach((op) => {
      if (op.group && op.group !== lastGroup) {
        parent = h('optgroup', { label: op.group });
        sel.appendChild(parent);
        lastGroup = op.group;
      }
      parent.appendChild(h('option', { value: op.value, text: op.label }));
    });
    sel.value = opts.value;
    if (sel.value !== String(opts.value) && opts.value !== undefined) {
      sel.appendChild(h('option', { value: opts.value, text: opts.missingLabel || `${opts.value} (missing)` }));
      sel.value = opts.value;
    }
    if (opts.label !== null) sel.classList.add('ctl-wide');
    sel.addEventListener('change', () => opts.onChange && opts.onChange(sel.value));
    const row = opts.label === null ? sel : h('div', { class: 'ctl' }, h('span', { class: 'ctl-label', text: opts.label, title: opts.hint || opts.label }), sel);
    return { el: row, select: sel, set: (v) => { sel.value = v; } };
  }

  // ---------- colour ----------
  const HEX = /^#?[0-9a-fA-F]{6}$/;
  function color(opts) {
    const pick = h('input', { type: 'color', value: opts.value, 'aria-label': opts.label });
    const text = h('input', { type: 'text', value: opts.value, spellcheck: false, maxlength: '7', 'aria-label': `${opts.label} hex` });
    const wrap = h('div', { class: 'ctl-color' }, pick, text);
    const row = h('div', { class: 'ctl' }, h('span', { class: 'ctl-label', text: opts.label, title: opts.hint || opts.label }), wrap);
    pick.addEventListener('input', () => { text.value = pick.value; if (opts.onInput) opts.onInput(pick.value); });
    pick.addEventListener('change', () => { if (opts.onCommit) opts.onCommit(pick.value); });
    text.addEventListener('change', () => {
      if (!HEX.test(text.value.trim())) { text.value = pick.value; return; }
      const v = '#' + text.value.trim().replace('#', '').toLowerCase();
      pick.value = v;
      text.value = v;
      if (opts.onInput) opts.onInput(v);
      if (opts.onCommit) opts.onCommit(v);
    });
    return {
      el: row,
      set: (v) => { pick.value = v; text.value = v; },
      setDisabled: (d) => row.classList.toggle('disabled', !!d)
    };
  }

  // ---------- toggle ----------
  function toggle(opts) {
    const input = h('input', { type: 'checkbox', 'aria-label': opts.label });
    input.checked = !!opts.value;
    const sw = h('label', { class: 'switch' }, input, h('span'));
    const row = h('div', { class: 'ctl' }, h('span', { class: 'ctl-label', text: opts.label, title: opts.hint || opts.label }), sw);
    input.addEventListener('change', () => opts.onChange && opts.onChange(input.checked));
    return { el: row, set: (v) => { input.checked = !!v; } };
  }

  // ---------- collapsible group (remembers open state) ----------
  let groupState = {};
  try { groupState = JSON.parse(localStorage.getItem('fv-ui-groups') || '{}') || {}; } catch (err) { groupState = {}; }
  function group(title, key, defOpen, extra) {
    const body = h('div', { class: 'grp-body' });
    const det = h('details', { class: 'grp' }, h('summary', null, title, extra ? h('span', { class: 'grp-extra', text: extra }) : null), body);
    det.open = groupState[key] !== undefined ? groupState[key] : !!defOpen;
    det.addEventListener('toggle', () => {
      groupState[key] = det.open;
      try { localStorage.setItem('fv-ui-groups', JSON.stringify(groupState)); } catch (err) { /* ignore */ }
    });
    return { el: det, body };
  }

  function button(label, opts) {
    const o = opts || {};
    const b = h('button', { type: 'button', class: o.class || 'btn btn-sm', title: o.title || '' });
    if (o.icon) setIcon(b, o.icon, label ? ' ' + label : '');
    else b.textContent = label;
    if (o.onClick) b.addEventListener('click', o.onClick);
    return b;
  }

  // ---------- toasts ----------
  function toast(msg, kind, ms) {
    const host = document.getElementById('toasts');
    if (!host) return;
    const el = h('div', { class: `toast ${kind || ''}`, text: msg });
    host.appendChild(el);
    while (host.children.length > 4) host.removeChild(host.firstChild);
    setTimeout(() => el.remove(), ms || (kind === 'err' ? 7000 : 3200));
  }

  function fmtTime(t, withMs) {
    const s = Math.max(0, t);
    const m = Math.floor(s / 60);
    const sec = s - m * 60;
    return `${String(m).padStart(2, '0')}:${withMs ? sec.toFixed(2).padStart(5, '0') : String(Math.floor(sec)).padStart(2, '0')}`;
  }

  function download(name, blob) {
    const url = URL.createObjectURL(blob);
    const a = h('a', { href: url, download: name });
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
  }

  const slug = (s) => String(s || 'fractal-video').trim().toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'fractal-video';

  FV.ui = { h, icon, setIcon, ICONS, slider, select, color, toggle, group, button, toast, fmtTime, download, slug, decimals };
})();
