/* On a computer screen (not in the headset): a band along the top with the keys that do something right now, the
 * chapter, where you are and the clock; and, on P, the status and the settings as a panel under it (in the headset
 * these are on the wrist panel, which a computer has no hand to hold). The owner's wish: "for the flat screen/PC
 * experience, keep a band at the top with the keys and controls".
 *
 *   const band = makeKeysBand({ settings: ['ff', 'voices', ...] });      once
 *   band.update(now, info, keys, hint);     every frame: info as for the wrist panel, keys = [[key, what, hot?], ...],
 *                                           hint = one line of what to do now (or null)
 *   band.toggle();                          P: the status and the settings
 *   band.flat                               true on the computer screen once started (false in the headset)
 * H hides the band and brings it back. The page's own #clock, if it has one, is taken into the band. Nothing is
 * shown before the start, in the headset, or in a picture taken for the tests (?shot). */
import { settings, DEFS } from './settings.js?v=41';
import { fmtGET } from './wrist.js?v=41';

const CSS = `
#keysband { position: fixed; left: 0; right: 0; top: 0; z-index: 6; display: none; box-sizing: border-box; padding: 5px 12px 5px;
  background: rgba(8, 8, 9, 0.8); border-bottom: 1px solid #2c2b29; color: #c8c4b8; font: 13px/1.5 system-ui, "Segoe UI", sans-serif;
  flex-wrap: wrap; align-items: center; gap: 2px 14px; user-select: none; }
body.running:not(.invr) #keysband { display: flex; }
#keysband.off { background: none; border: 0; pointer-events: none; }
#keysband.off > * { display: none; }
#keysband.off > .kb-tab { display: block; pointer-events: auto; opacity: 0.55; }
#keysband .kb-where { color: #f1eee6; font-weight: 600; white-space: nowrap; }
#keysband .kb-where span { color: #8f8b82; font-weight: 400; }
#keysband .kb-keys { display: flex; flex-wrap: wrap; gap: 2px 12px; flex: 1 1 auto; }
#keysband .kb-k { white-space: nowrap; }
#keysband .kb-k.hot { color: #f0d070; }
#keysband kbd { border: 1px solid #5a5750; border-bottom-width: 2px; border-radius: 3px; padding: 0 5px; margin-right: 3px; font: 600 12px Consolas, monospace; color: #f1eee6; background: #1b1b1c; }
#keysband .kb-k.hot kbd { border-color: #a58a3c; color: #f0d070; }
#keysband #clock, #keysband .kb-clock { position: static; background: none; padding: 0; margin-left: auto; font: 600 14px/1.5 Consolas, monospace; color: #e9e6de; white-space: nowrap; }
#keysband .kb-hint { flex: 1 0 100%; color: #f0d070; font-size: 13.5px; }
#keysband .kb-tab { display: none; cursor: pointer; }
#kb-panel { position: fixed; right: 12px; top: 44px; z-index: 6; width: min(400px, calc(100vw - 24px)); max-height: calc(100vh - 60px); overflow: auto; box-sizing: border-box;
  background: rgba(8, 8, 9, 0.9); border: 1px solid #2c2b29; padding: 12px 16px; color: #c8c4b8; font: 14px/1.5 system-ui, "Segoe UI", sans-serif; display: none; }
body.running:not(.invr) #kb-panel.on { display: block; }
#kb-panel h2 { font-size: 15px; margin: 0 0 2px; color: #f1eee6; }
#kb-panel h3 { font-size: 12px; margin: 12px 0 4px; color: #8f8b82; font-weight: 600; letter-spacing: 0.08em; text-transform: uppercase; }
#kb-panel .kb-now { color: #f1eee6; font-size: 17px; font-weight: 600; }
#kb-panel .kb-alert { color: #f0d070; font-weight: 600; margin-top: 6px; }
#kb-panel .kb-row { display: flex; justify-content: space-between; gap: 10px; align-items: center; min-height: 26px; }
#kb-panel .kb-row b { color: #e8e6df; font-weight: 600; text-align: right; }
#kb-panel .kb-set b { flex: 1; text-align: center; font-weight: 400; font-size: 13px; }
#kb-panel .kb-set > span { flex: 0 0 38%; }
#kb-panel button { font: inherit; color: #e9e6de; background: #1b1b1c; border: 1px solid #45433f; padding: 0 9px; cursor: pointer; }
#kb-panel button:hover { background: #29292a; }
#kb-panel .kb-foot { color: #77746d; font-size: 12px; margin-top: 10px; }
`;
/* settings that mean nothing without a headset are left out of the computer's panel */
const VR_ONLY = ['turn', 'hz', 'fov', 'hand', 'body'];
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

export function makeKeysBand(o = {}) {
  const el = (tag, cls, parent) => { const e = document.createElement(tag); if (cls) e.className = cls; if (parent) parent.appendChild(e); return e; };
  const style = el('style'); style.textContent = CSS; document.head.appendChild(style);
  const band = el('div'); band.id = 'keysband';
  const where = el('div', 'kb-where', band), keysEl = el('div', 'kb-keys', band);
  let clock = document.getElementById('clock'); const own = !!clock;          /* (a page's own clock says where you are as well) */
  if (clock) band.appendChild(clock); else clock = el('div', 'kb-clock', band);
  const hintEl = el('div', 'kb-hint', band), tab = el('div', 'kb-tab', band);
  tab.innerHTML = '<kbd>H</kbd>keys';
  document.body.appendChild(band);
  const panel = el('div'); panel.id = 'kb-panel'; document.body.appendChild(panel);
  const status = el('div', '', panel), sets = el('div', '', panel);
  const list = (o.settings || Object.keys(DEFS)).filter(k => DEFS[k] && !VR_ONLY.includes(k));
  const last = { where: '', keys: '', hint: null, status: '', at: 0 };
  const B = {
    el: band, panel, hidden: false, open: false, keys: [], hint: null,
    /* started on this screen, and not in the headset */
    get flat() { return isFlat(); },
    toggle(on = !B.open) { B.open = on; panel.classList.toggle('on', on); if (on) { last.status = ''; last.at = 0; drawSets(); } },
    hide(on = !B.hidden) { B.hidden = on; band.classList.toggle('off', on); if (on) B.toggle(false); },
    update(now, info, keys, hint) {
      if (!B.flat) return;
      B.keys = keys || []; B.hint = hint || null;
      const w = info ? esc(info.chapter || '') + (info.now && !own ? ' <span>· ' + esc(info.now) + '</span>' : '') : '';
      if (w !== last.where) { last.where = w; where.innerHTML = w; }
      const k = B.keys.map(([key, what, hot]) => '<span class="kb-k' + (hot ? ' hot' : '') + '"><kbd>' + esc(key) + '</kbd>' + esc(what) + '</span>').join('');
      if (k !== last.keys) { last.keys = k; keysEl.innerHTML = k; }
      if (B.hint !== last.hint) { last.hint = B.hint; hintEl.textContent = B.hint || ''; hintEl.style.display = B.hint ? '' : 'none'; }
      if (o.clock && clock) { const c = o.clock(); if (c !== clock.textContent) clock.textContent = c; }
      if (B.open && info && now - last.at > 250) {
        last.at = now;
        let h = '<h2>' + esc(info.chapter || '') + '</h2><div class="kb-now">' + esc(info.now || '') + '</div>';
        if (info.next) { const [name, at] = info.next, dt = at - info.get; h += '<div>Next: ' + esc(name) + (isFinite(dt) ? ' in ' + fmtGET(dt).replace(/^0:/, '') : '') + '</div>'; }
        if (info.after) h += '<div style="color:#8f8b82">Then: ' + esc(info.after) + '</div>';
        if (info.alert) h += '<div class="kb-alert">' + esc(info.alert) + '</div>';
        h += '<h3>Status</h3>';
        if (typeof info.get === 'number') h += '<div class="kb-row"><span>Mission time</span><b>GET ' + fmtGET(info.get) + (info.warp > 1 ? ' · time ×' + info.warp.toLocaleString('en-US') : '') + '</b></div>';
        for (const [label, value] of info.rows || []) h += '<div class="kb-row"><span>' + esc(label) + '</span><b>' + esc(value) + '</b></div>';
        h += '<div class="kb-row"><span>Frame rate</span><b>' + (info.fps || 0).toFixed(0) + ' fps</b></div>';
        if (h !== last.status) { last.status = h; status.innerHTML = h; }
      }
    }
  };
  function drawSets() {
    sets.innerHTML = '<h3>Settings</h3>' + list.map(k => '<div class="kb-row kb-set"><span>' + esc(DEFS[k].label) + '</span><button data-k="' + k + '" data-d="-1" aria-label="previous">‹</button><b>' + esc(settings.text(k)) + '</b><button data-k="' + k + '" data-d="1" aria-label="next">›</button></div>').join('')
      + '<div class="kb-foot">Kept from chapter to chapter. <kbd>P</kbd> closes.</div>';
  }
  sets.addEventListener('click', e => { const b = e.target.closest('button[data-k]'); if (!b) return; settings.step(b.dataset.k, +b.dataset.d); drawSets(); b.blur(); });
  tab.addEventListener('click', () => B.hide(false));
  addEventListener('keydown', e => { if (e.code === 'KeyH' && B.flat && !e.ctrlKey && !e.altKey && !e.metaKey) B.hide(); });
  return B;
}

/* The keys of a chapter in its seat: S = the chapter's state (running, ended), o = { win: the big window (C),
 * out: OUT (the outside view, with its own keys while it is on; false: none), jump: '1–6', warp: false for none,
 * first: [[key, what, hot], ...] put before the rest (what this moment asks of you), next: the N key's words }. */
export function seatKeys(S, o = {}) {
  if (S.ended) return [['N', o.next || 'on to the next chapter', true], ['Mouse', 'drag to look round'], ['P', 'status, settings'], ['H', 'hide']];
  const k = [...(o.first || []), ['Mouse', 'drag to look round'], ['Space', S.running === false ? 'go on' : 'pause', S.running === false]];
  if (o.warp !== false) k.push(['W', 'time faster'], ['S', 'slower']);
  if (o.win) k.push(['C', 'big window']);
  if (o.out && o.out.on) k.push(['O', 'back inside'], ['J L', 'round'], ['I K', 'up, down'], ['U M', 'nearer, further']);
  else if (o.out) k.push(['O', 'outside view']);
  if (o.jump) k.push([o.jump, 'jump in time']);
  k.push(['P', 'status, settings'], ['H', 'hide']);
  return k;
}

/* started on this screen, and not in the headset: messages then name keys, not the controllers' buttons */
export const isFlat = () => { const c = document.body.classList; return c.contains('running') && !c.contains('invr'); };
