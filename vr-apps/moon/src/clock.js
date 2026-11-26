/* Fast forward, the same in every chapter (owner's wish: plain to see, and never in the way):
 *  - 'events' (the default): time runs fast until an event or a radio line, then real time there, with a word
 *    in front of you; B goes on fast again, past it. Through an event (a burn, the docking, Earthrise...) x10 at
 *    most; the big steps are for the long quiet stretches.
 *  - 'major': the same, stopping only at the major events, not for the radio.
 *  - 'free': never stops, never capped.
 * The crew's small talk never stops it. A stretch is [from, to, name] (GET).
 *
 *   const clock = makeClock({ holds: () => [...], lines: () => radio ? radio.lines : [], warps: [1, 10, ...], cap: 10, toast });
 *   S.t = clock.advance(S, dt);      every frame, while running
 *   clock.faster(S);                 B: one step faster (it stays at the fastest)
 *   clock.slower(S);                 A: one step slower (down to real time; held: real time at once) */
import { settings } from './settings.js?v=32';

export function makeClock({ holds, lines, warps, cap = 10, toast }) {
  let skip = -Infinity;                          /* stops at or before this have been passed on purpose */
  const inHold = t => holds().find(([a, b]) => t > a && t < b);
  const say = s => { if (toast) toast.say(s, 2.6, true); };                 /* (an answer to your own button: shown whatever the messages' setting) */
  return {
    inHold,
    advance(S, dt) {
      const mode = settings.get('ff');
      let nt = S.t + dt * S.warp;
      if (S.warp > 1 && mode !== 'free') {
        let stop = null;
        for (const [a, , name] of holds()) if (a > S.t && a <= nt && a > skip && (!stop || a < stop.at)) stop = { at: a, why: name };
        if (mode === 'events' && lines) for (const l of lines()) {
          if (typeof l.t !== 'number' || l.cue) continue;
          const at = l.t - 0.4;
          if (at > S.t && at <= nt && at > skip && (!stop || at < stop.at)) stop = { at, why: null };
        }
        if (stop) {
          nt = stop.at; S.warp = 1; skip = stop.at + 0.3;
          if (stop.why) say('Real time: ' + stop.why + '.  B: fast again');
        }
      }
      if (mode !== 'free' && S.warp > cap && inHold(nt)) { S.warp = cap; say('x' + cap + ' at most through ' + inHold(nt)[2]); }
      return nt;
    },
    faster(S) {
      const mode = settings.get('ff'), h = mode !== 'free' ? inHold(S.t) : null;
      skip = Math.max(skip, S.t + 0.3);
      const allowed = h ? warps.filter(w => w <= cap) : warps;
      const i = allowed.indexOf(S.warp);
      if (i >= allowed.length - 1) { say(h && allowed.length < warps.length ? 'Through ' + h[2] + ': x' + cap + ' at most' : 'x' + S.warp.toLocaleString('en-US') + ' is the fastest'); return; }
      S.warp = allowed[i + 1];
    },
    slower(S) {
      const below = warps.filter(w => w < S.warp);
      S.warp = below.length ? below[below.length - 1] : 1;
    },
    reset() { skip = -Infinity; }
  };
}
