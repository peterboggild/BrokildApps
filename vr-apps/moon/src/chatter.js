/* The crew's own remarks (assets/script/crew.json): small talk now and then, and an answer when you put your hand on
 * one of them. Not authentic, by the owner's wish: a little life. Small talk comes every few minutes of real time
 * (not mission time, and fast forward never stops for it), only in a quiet moment (no radio line just heard or
 * about to come), only where it fits (each line's 'where'), and never the same line twice in a row.
 *
 *   const chat = await makeChatter(ctx, sound.voice);
 *   chat.update(dt, { crew: ['CMP', 'LMP'], where: ['any', 'space', 'earth'], quiet: true });   every frame
 *   chat.touch('LMP');                                    your hand on him: one of his answers (not too often)
 * chat.radio plays them (radio.js): talking(who) and level(who) for the faces. */
import { makeRadio } from './radio.js?v=25';

const rnd = (a, b) => a + Math.random() * (b - a);

export async function makeChatter(ctx, out) {
  const radio = await makeRadio(ctx, out, new URL('../assets/script/crew.json', import.meta.url).href);
  let wait = rnd(90, 200), last = null, touchCool = 0;
  const said = new Set();
  return {
    radio,
    get next() { return wait; },
    update(dt, o) {
      touchCool -= dt;
      if (!o || !o.quiet || !o.crew || !o.crew.length) return;
      wait -= dt;
      if (wait > 0) return;
      const pool = radio.lines.filter(l => l.cue === 'small' && o.crew.includes(l.who) && (o.where || ['any']).includes(l.where) && l.id !== last && l.buf);
      if (!pool.length) { wait = 30; return; }
      /* lines not yet heard first */
      const fresh = pool.filter(l => !said.has(l.id)), l = (fresh.length ? fresh : pool)[Math.floor(Math.random() * (fresh.length || pool.length))];
      radio.say(l.id); said.add(l.id); last = l.id;
      wait = rnd(150, 330);
    },
    /* your hand on him: he says something about it (once in a few seconds) */
    touch(who) {
      if (touchCool > 0) return null;
      const pool = radio.lines.filter(l => l.cue === 'touch' && l.who === who && l.buf);
      if (!pool.length) return null;
      touchCool = 5;
      const l = pool[Math.floor(Math.random() * pool.length)];
      return radio.say(l.id);
    },
    /* for tests: say a small-talk line now */
    sayNow() { wait = 0; }
  };
}
