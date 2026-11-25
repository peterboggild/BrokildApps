/* The radio on the Moon (chapter 6): Houston and the other astronaut in your ears.
 *
 * - What happens is said: the helmets, the go for the surface, the hatch, the porch, the last rung, the first step;
 *   the Lunar Module Pilot as he follows you out. cue(name) plays the line for an event, once.
 * - What comes next is said: setNext(name) names the 'next-...' line of the moment; Houston reads it when it first
 *   applies, and again after one of your own transmissions if you have not heard it for a while.
 * - You can transmit: hold the left trigger (T on the screen) and speak, let go. Nobody can understand you (there is
 *   no speech recognition here, by design: nothing you say leaves the headset), so the answer is general: "Roger
 *   that", "Copy", "Say again?", from the man beside you or from Houston. The key clicks and the channel hisses
 *   while you hold it.
 * - Small talk: a remark every minute or two while you both walk about.
 * One line at a time: the rest wait their turn, and a line that has waited too long is dropped.
 *
 * The voices are the Windows voices for now (tools/script/surface.json, tools/tts.ps1); the owner's own recordings
 * replace them file for file.
 *   const mr = makeMoonRadio({ toast, level: () => 0..1, onLMP: () => {} });
 *   mr.init()                 on a click or entering VR (browsers allow sound only then)
 *   mr.update(dt, { ptt, quiet, lmpOut })      every frame */
import { makeRadio } from './radio.js?v=26';

const rnd = (a, b) => a + Math.random() * (b - a);
const NAME = { CAPCOM: 'Houston', LMP: 'Lunar Module Pilot' };

export function makeMoonRadio(o = {}) {
  let ctx = null, out = null, radio = null, loading = false;
  let T = 0, busyUntil = 0, small = rnd(45, 90), next = 'next-look', nextHeard = -1e9, nextDue = -1, lastSmall = null, lastAck = null;
  let held = 0, wasHeld = false, mic = null, bumpAt = -1e9;
  const queue = [], once = new Set(), heard = new Set();
  const api = { ready: false, played: 0, acks: 0, last: null };

  async function init() {
    if (radio || loading) { if (ctx && ctx.state === 'suspended') ctx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext; if (!AC) return;
    loading = true;
    try {
      ctx = new AC(); out = ctx.createGain(); out.gain.value = o.level ? o.level() : 1; out.connect(ctx.destination);
      radio = await makeRadio(ctx, out, new URL('../assets/script/surface.json', import.meta.url).href);
      api.ready = true;
    } catch (e) { console.warn('moon radio: ' + e.message); }
    loading = false;
  }
  /* the key: a click, and the open channel's hiss while it is held */
  function click(f) {
    if (!ctx) return;
    const n = Math.floor(ctx.sampleRate * 0.035), b = ctx.createBuffer(1, n, ctx.sampleRate), d = b.getChannelData(0);
    for (let i = 0; i < n; i++) d[i] = (Math.random() * 2 - 1) * Math.exp(-i / n * 6) + 0.5 * Math.sin(i / ctx.sampleRate * 2 * Math.PI * f) * Math.exp(-i / n * 4);
    const s = ctx.createBufferSource(); s.buffer = b; const g = ctx.createGain(); g.gain.value = 0.25; s.connect(g).connect(out); s.start();
  }
  function micOn(on) {
    if (!ctx) return;
    if (on && !mic) {
      const b = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate), d = b.getChannelData(0); for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
      const s = ctx.createBufferSource(); s.buffer = b; s.loop = true;
      const bp = ctx.createBiquadFilter(); bp.type = 'bandpass'; bp.frequency.value = 1800; bp.Q.value = 0.6;
      const g = ctx.createGain(); g.gain.value = 0.035; s.connect(bp).connect(g).connect(out); s.start(); mic = { s, g };
    } else if (!on && mic) { try { mic.s.stop(); } catch (e) { /* already stopped */ } mic = null; }
  }
  const pool = (cue, who) => (radio ? radio.lines.filter(l => l.cue === cue && l.buf && (!who || l.who === who)) : []);
  function push(cue, opt = {}) { queue.push({ cue, at: T + (opt.delay || 0), till: T + (opt.keep || 14), sub: opt.sub !== false, who: opt.who || null, avoid: opt.avoid || null }); }
  function play(q) {
    let ls = pool(q.cue, q.who); if (!ls.length) ls = pool(q.cue); if (!ls.length) return;
    const fresh = ls.filter(l => !heard.has(l.id) && l.id !== q.avoid), pick = fresh.length ? fresh : ls.filter(l => l.id !== q.avoid);
    const l = (pick.length ? pick : ls)[Math.floor(Math.random() * (pick.length || ls.length))];
    if (out && o.level) out.gain.value = o.level();
    radio.say(l.id); heard.add(l.id); api.played++; api.last = l;
    busyUntil = T + l.dur + (l.who === 'CAPCOM' ? 1.1 : 0.6);
    if (q.sub && o.toast) o.toast.say((NAME[l.who] || l.who) + ': ' + l.text, l.dur + 1.5);
    if (q.cue === 'ack') { api.acks++; lastAck = l.id; if (l.who === 'LMP' && o.onLMP) o.onLMP(); }
    if (q.cue === 'small') lastSmall = l.id;
    if (q.cue.startsWith('next')) nextHeard = T;
  }
  Object.assign(api, {
    init,
    /* the line for something that happens, once (sub: false where the page shows its own words) */
    cue(name, sub = true) { if (once.has(name)) return; once.add(name); push(name, { sub, keep: 20 }); },
    /* you walked into him (not too often) */
    bump() { if (T - bumpAt < 25) return; bumpAt = T; push('bump', { keep: 3 }); },
    /* what comes next: Houston says it once it applies (after a pause), and on request */
    setNext(name) { if (name === next) return; next = name; nextDue = T + 6; },
    forget() { once.clear(); queue.length = 0; nextDue = -1; nextHeard = -1e9; },
    update(dt, st = {}) {
      T += dt;
      /* your own transmission */
      if (st.ptt && !wasHeld) { held = 0; click(1200); micOn(true); }
      if (st.ptt) held += dt;
      if (!st.ptt && wasHeld) {
        click(900); micOn(false);
        if (held > 0.35 && radio) {
          push('ack', { delay: 0.7 + Math.random() * 0.6, who: st.lmpOut && Math.random() < 0.65 ? 'LMP' : 'CAPCOM', avoid: lastAck, keep: 8 });
          if (T - nextHeard > 40) push(next, { delay: 2.5, keep: 30 });
        }
      }
      wasHeld = !!st.ptt;
      if (!radio) return;
      /* what comes next, when it first applies (you are both down and about) */
      if (nextDue < 0 && st.quiet && nextHeard < -1e8) nextDue = T + 9;
      if (nextDue > 0 && T > nextDue && st.quiet) { nextDue = -1; push(next, { keep: 40 }); }
      /* small talk */
      if (st.quiet && !queue.length && T > busyUntil + 6) { small -= dt; if (small <= 0) { small = rnd(60, 140); push('small', { avoid: lastSmall, keep: 6 }); } }
      /* one line at a time */
      if (T > busyUntil && !wasHeld) {
        for (let i = 0; i < queue.length; i++) {
          const q = queue[i];
          if (T > q.till) { queue.splice(i--, 1); continue; }
          if (T >= q.at) { queue.splice(i, 1); play(q); break; }
        }
      }
    }
  });
  /* (live values: Object.assign would copy a getter's value once) */
  Object.defineProperty(api, 'transmitting', { get: () => wasHeld });
  Object.defineProperty(api, 'next', { get: () => next });
  return api;
}
