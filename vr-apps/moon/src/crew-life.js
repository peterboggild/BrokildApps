/* Life in the crew: where they look, how their heads and hands move, blinking, and the jaw when they speak.
 *
 * Each crewman (from makeCrewman in cabin.js) looks at one thing at a time and then moves on, as people do: the
 * eyes jump there at once (a saccade, the lids following them up and down), the head follows more slowly and only
 * part of the way. What they look at: the panel mostly, now and then the other crewman or something off to the
 * side, and you: more often when they speak to you on the intercom, and back at you if you look at them a while.
 * Talking on the loop to Houston, or busy (the descent), they keep to the panel and only glance at you. They blink
 * every few seconds, and with big glances. The jaw opens with the loudness of their own voice as it plays (the
 * radio measures it). The hands rest on the armrests and every so often reach to the panel, the chest connectors
 * or the thigh; talking to you, the hand nearer you lifts a little.
 *
 * Speaking, they turn a little to whoever they speak to: you; the other crewman when answering him (he spoke last);
 * the panel when talking to Houston on the loop (or answering the CAPCOM); small talk to you or to him. Touched
 * (handOnCrew: your hand stops at his suit), he turns to you, and chatter.js has him say something about it.
 *
 *   const life = makeCrewLife([{ man, who: 'CMP' }, ...]);
 *   life.update(seconds, { viewer: camera, radio, busy });       every frame, before the render
 * The viewer's position is read from camera.matrixWorld and the crewman's frame from his matrixWorld, both from the
 * same (last) render, so this holds in a spacecraft moving kilometres a second. */
import * as THREE from 'three';

const rnd = (a, b) => a + Math.random() * (b - a);
const pick = w => { let s = 0; for (const k in w) s += w[k]; let r = Math.random() * s; for (const k in w) { r -= w[k]; if (r <= 0) return k; } return 'panel'; };
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const HEAD = new THREE.Vector3(0, -0.02, 0.035);          /* between the eyes, crewman frame (head at rest) */

export function makeCrewLife(members) {
  const crew = members.map((m, k) => ({
    ...m, k, kind: 'panel', target: new THREE.Vector3(0, -0.1, -0.8), until: 0, micro: 0, mx: 0, my: 0,
    yaw: 0, pitch: 0, vy: 0, vp: 0, eyes: [[0, 0], [0, 0]], blinkAt: rnd(1, 4), blinkT: -1, jaw: 0,
    lookedAt: 0, cool: 0, forced: null, wasTalking: false, to: 'you', reactUntil: -1,
    hands: [0, 1].map(() => ({ cur: null, vel: new THREE.Vector3(), goal: null, palm: new THREE.Vector3(0, 1, 0), until: rnd(3, 12), kind: 'rest' }))
  }));
  const _cam = new THREE.Vector3(), _fwd = new THREE.Vector3(), _hw = new THREE.Vector3(), _inv = new THREE.Matrix4(), _inv2 = new THREE.Matrix4();
  const _t = new THREE.Vector3(), _l = new THREE.Vector3(), _g = new THREE.Vector3();
  let last = null, tNow = 0;
  const spoke = {};                      /* who spoke last, and when he stopped (seconds, this clock) */

  /* the next thing to look at */
  function choose(c, t, ctx) {
    const talk = ctx.talk;
    let w;
    if (c.forced) w = { [c.forced]: 1 };
    else if (t < c.reactUntil) w = { you: 1 };                                            /* you touched him */
    /* beginning to speak, he turns to whom he speaks to first, as people do without thinking (owner's headset test) */
    else if (ctx.first && talk && c.to === 'you' && !ctx.busy) w = { you: 1 };
    else if (ctx.first && talk && c.to === 'mate') w = { mate: 1 };
    else if (c.lookedAt > 0.8 && c.cool <= 0) { w = { you: 1 }; c.cool = rnd(8, 15); }    /* you have been looking at him */
    else if (talk && c.to === 'mate') w = { mate: 0.72, panel: 0.18, you: 0.1 };
    else if (ctx.toYou && !ctx.busy) w = { you: 0.8, panel: 0.15, mate: 0.05 };
    else if (talk) w = { you: 0.15, panel: 0.75, away: 0.1 };
    else if (ctx.mateTalk && !ctx.busy) w = { mate: 0.45, panel: 0.35, you: 0.2 };
    else if (ctx.busy) w = { panel: 0.85, you: 0.08, mate: 0.07 };
    else w = { panel: 0.52, away: 0.2, you: 0.13, mate: 0.15 };
    if (!ctx.you) delete w.you;
    if (!ctx.mate) delete w.mate;
    const kind = pick(w);
    const prev = _g.copy(c.target);
    c.kind = kind;
    if (kind === 'panel') c.target.set(rnd(-0.35, 0.35), rnd(-0.28, 0.12), -rnd(0.55, 0.8));
    else if (kind === 'away') c.target.set(rnd(-1, 1), rnd(-0.2, 0.5), -rnd(0.2, 0.9));
    else c.target.copy(kind === 'you' ? ctx.you : ctx.mate);
    const brief = kind === 'you' && !(ctx.toYou && !ctx.busy) && c.lookedAt < 0.8;
    c.until = t + (kind === 'you' ? (brief ? rnd(0.7, 1.8) : rnd(1.5, 3.2)) : kind === 'mate' ? rnd(1, 2.5) : rnd(0.8, 4.5));
    /* a big glance often comes with a blink */
    if (prev.sub(HEAD).angleTo(_l.copy(c.target).sub(HEAD)) > 0.35 && Math.random() < 0.45 && c.blinkT < 0) c.blinkAt = t + 0.02;
  }

  function update(t, ctx = {}) {
    const dt = last === null ? 0 : clamp(t - last, 0, 0.1); last = t; tNow = t;
    const viewer = ctx.viewer, radio = ctx.radio;
    if (radio && radio.talking && radio.talking('CAPCOM')) spoke.CAPCOM = t;
    if (viewer) { _cam.setFromMatrixPosition(viewer.matrixWorld); _fwd.set(0, 0, -1).transformDirection(viewer.matrixWorld); }
    for (const c of crew) {
      const man = c.man, neck = man.userData.neck;
      if (!neck || !man.visible) continue;
      /* you and the other crewman, in his frame */
      _inv.copy(man.matrixWorld).invert();
      const you = viewer ? _cam.clone().applyMatrix4(_inv) : null;
      const mateC = crew.find(o => o !== c && o.man.visible && o.man.userData.neck);
      const mate = mateC ? mateC.man.localToWorld(HEAD.clone()).applyMatrix4(_inv) : null;
      const talk = radio && radio.talking ? radio.talking(c.who) : null;
      /* whom he speaks to: the ground or the other spacecraft on the radio (or answering the CAPCOM), the other
       * crewman (answering him, or small talk), or you */
      if (talk && !c.wasTalking) {
        const l = talk.line || {}, text = l.text || '';
        const radioCall = talk.ch !== 'intercom' || /\bHouston\b|^Roger\b|\bAmerica\b/.test(text) || (spoke.CAPCOM !== undefined && t - spoke.CAPCOM < 6);
        c.to = l.cue === 'touch' ? 'you' : radioCall ? 'panel' : l.cue === 'small' ? (mateC && Math.random() < 0.5 ? 'mate' : 'you')
          : mateC && spoke[mateC.who] !== undefined && t - spoke[mateC.who] < 8 ? 'mate' : 'you';
      }
      if (talk) spoke[c.who] = t;
      const toYou = !!talk && c.to === 'you';
      const mateTalk = !!(mateC && radio && radio.talking && radio.talking(mateC.who));
      /* are you looking at him? */
      if (viewer) {
        man.localToWorld(_hw.copy(HEAD)).sub(_cam);
        c.lookedAt = _fwd.angleTo(_hw) < 0.2 && _hw.length() < 3 ? c.lookedAt + dt : Math.max(0, c.lookedAt - 2 * dt);
      }
      c.cool -= dt;
      if (t >= c.until || (c.lookedAt > 0.8 && c.cool <= 0 && c.kind !== 'you') || (talk && !c.wasTalking))
        choose(c, t, { you, mate, talk, toYou, mateTalk, busy: ctx.busy, first: !!talk && !c.wasTalking });
      c.wasTalking = !!talk;
      if (c.kind === 'you' && you) c.target.copy(you);
      if (c.kind === 'mate' && mate) c.target.copy(mate);

      /* the head: part of the way to the target, slowly (a damped spring), with a little drift and breathing */
      _t.copy(c.target).sub(HEAD);
      const yawT = Math.atan2(-_t.x, -_t.z), pitchT = Math.atan2(_t.y, Math.hypot(_t.x, _t.z));
      const frac = c.kind === 'you' ? (toYou || t < c.reactUntil ? 0.6 : 0.45) : 0.6;
      /* part of the way, but at least so far that the eyes need no more than 26 degrees (you sit at his side: 90
       * degrees round), at most 69 degrees */
      const part = (v, f, eye) => Math.sign(v) * Math.max(Math.abs(v) * f, Math.abs(v) - eye);
      const hy = clamp(part(yawT, frac, 0.45), -1.2, 1.2) + 0.025 * Math.sin(t * 0.31 + c.k * 2) + 0.015 * Math.sin(t * 0.83 + c.k);
      const hp = clamp(part(pitchT, frac, 0.3), -0.4, 0.35) + 0.012 * Math.sin(t * 1.6 + c.k) + 0.015 * Math.sin(t * 0.27 + c.k * 3);
      const W = t < c.reactUntil || (talk && c.kind !== 'panel') ? 8 : 5.5;        /* quicker to the one he speaks to */
      c.vy += (W * W * (hy - c.yaw) - 2 * W * c.vy) * dt; c.yaw += c.vy * dt;
      c.vp += (W * W * (hp - c.pitch) - 2 * W * c.vp) * dt; c.pitch += c.vp * dt;
      neck.rotation.set(c.pitch, c.yaw, 0);
      neck.updateMatrixWorld(true);

      /* the eyes: at the target from each eye (so they converge on you), fast; small jumps of their own */
      const eyes = man.userData.eyes, face = man.userData.face;
      c.micro -= dt; if (c.micro <= 0) { c.micro = rnd(0.3, 0.9); c.mx = rnd(-0.015, 0.015); c.my = rnd(-0.01, 0.01); }
      let pitchSum = 0;
      if (eyes) {
        man.localToWorld(_t.copy(c.target));
        _inv2.copy(eyes[0].parent.matrixWorld).invert();
        eyes.forEach((e, i) => {
          _l.copy(_t).applyMatrix4(_inv2).sub(e.position);
          const ey = clamp(Math.atan2(_l.x, _l.z), -0.6, 0.6) + c.mx, ep = clamp(-Math.atan2(_l.y, Math.hypot(_l.x, _l.z)), -0.4, 0.45) + c.my;
          const k = 1 - Math.exp(-dt * 30), s = c.eyes[i];
          s[0] += (ey - s[0]) * k; s[1] += (ep - s[1]) * k;
          e.rotation.set(s[1], s[0], 0); pitchSum += s[1];
        });
      }
      /* the lids: blinks (shut in 70 ms, open in 150), and following the eyes up and down */
      if (face) {
        if (c.blinkT < 0 && t >= c.blinkAt) c.blinkT = 0;
        let b = 0;
        if (c.blinkT >= 0) {
          c.blinkT += dt; const s = c.blinkT;
          b = s < 0.07 ? s / 0.07 : s < 0.1 ? 1 : s < 0.25 ? 1 - (s - 0.1) / 0.15 : 0;
          if (s >= 0.25) { c.blinkT = -1; c.blinkAt = t + (talk ? rnd(1.5, 4) : rnd(2, 6.5)); }
        }
        face.uBlink.value = b;
        face.uLid.value = clamp(pitchSum / 2, -0.3, 0.4) * 0.22;
        /* the jaw: with the loudness of his voice, opening quickly and closing a little slower */
        const lv = talk && radio.level ? radio.level(c.who) : 0;
        const want = talk ? clamp((lv - 0.012) * 1.6, 0, 0.13) : 0;
        c.jaw += (want - c.jaw) * (1 - Math.exp(-dt * (want > c.jaw ? 28 : 16)));
        face.uJaw.value = c.jaw;
      }

      /* the hands */
      const arms = man.userData.arms;
      if (arms) arms.forEach((a, i) => {
        const h = c.hands[i];
        if (!h.cur) { h.cur = a.rest.clone(); h.goal = a.rest.clone(); a.palmNow = new THREE.Vector3(0, 1, 0); }
        if (t >= h.until) {
          const towardYou = you ? Math.sign(you.x) === a.sx : false;
          const kinds = h.kind !== 'rest' ? { rest: 1 }
            : toYou && !ctx.busy && towardYou ? { lift: 0.7, rest: 0.3 }
            : ctx.busy ? { panel: 0.15, rest: 0.85 }
            : { rest: 0.55, panel: 0.2, chest: 0.1, thigh: 0.15 };
          h.kind = pick(kinds);
          h.palm.set(0, 1, 0);
          if (h.kind === 'rest') { h.goal.copy(a.rest); h.until = t + rnd(talk ? 1.5 : 5, talk ? 4 : 16); }
          else if (h.kind === 'panel') { h.goal.set(a.sx * rnd(0.05, 0.22), rnd(-0.32, -0.12), -rnd(0.34, 0.42)); h.palm.set(0, 1, 0.3); h.until = t + rnd(1.2, 3); }
          else if (h.kind === 'chest') { h.goal.set(a.sx * 0.06, -0.39, -0.12); h.palm.set(a.sx, 0.3, -0.6); h.until = t + rnd(1, 2.2); }
          else if (h.kind === 'thigh') { h.goal.set(a.sx * 0.12, -0.5, -0.24); h.until = t + rnd(2, 5); }
          else { h.goal.copy(a.rest).add(_g.set(-a.sx * 0.03, rnd(0.07, 0.13), rnd(-0.02, 0.04))); h.palm.set(a.sx * 0.6, 1, 0); h.until = t + rnd(0.8, 2); }
          h.palm.normalize();
        }
        const Wh = 6;
        for (const ax of ['x', 'y', 'z']) { h.vel[ax] += (Wh * Wh * (h.goal[ax] - h.cur[ax]) - 2 * Wh * h.vel[ax]) * dt; h.cur[ax] += h.vel[ax] * dt; }
        a.palmNow.lerp(h.palm, 1 - Math.exp(-dt * 5)).normalize();
        man.userData.poseArm(a, h.cur, a.palmNow);
      });
    }
  }
  const _p = new THREE.Vector3(), _n = new THREE.Vector3(), _w = new THREE.Vector3(), _iq = new THREE.Quaternion();
  return {
    update, crew,
    /* a point (world) inside a crewman? The deepest of his body's spheres (userData.touch, his frame): who, how deep,
     * and the push (world) that takes the point out to the suit's surface */
    touch(p) {
      let best = null;
      for (const c of crew) {
        const man = c.man, sp = man.userData.touch;
        if (!sp || !man.visible || (man.parent && !man.parent.visible)) continue;
        man.updateWorldMatrix(true, false);
        _p.copy(p).applyMatrix4(_inv.copy(man.matrixWorld).invert());
        for (const [x, y, z, r] of sp) {
          _n.set(_p.x - x, _p.y - y, _p.z - z); const d = _n.length();
          if (d < r && (!best || r - d > best.depth)) {
            if (d < 1e-6) _n.set(0, 0, -1); else _n.divideScalar(d);
            _w.copy(_n).applyQuaternion(man.getWorldQuaternion(_iq)).multiplyScalar(r - d);
            best = { who: c.who, depth: r - d, push: _w.clone(), life: this };
          }
        }
      }
      return best;
    },
    /* touched: he turns to you for a few seconds */
    react(who) { const c = crew.find(o => o.who === who); if (c) { c.reactUntil = tNow + 2.8; c.until = 0; } },
    /* for tests and the close-up page: always look at 'you' or the 'panel' (null: free again) */
    force(kind) { crew.forEach(c => { c.forced = kind; c.until = 0; }); },
    /* for tests: how far (degrees) each crewman's eyes look from you, his head turn, jaw and blink */
    probe(viewer) {
      const cam = new THREE.Vector3().setFromMatrixPosition(viewer.matrixWorld);
      return crew.map(c => {
        const eyes = c.man.userData.eyes; if (!eyes || !c.man.visible) return null;
        const off = eyes.map(e => { const p = e.getWorldPosition(new THREE.Vector3()), q = e.getWorldQuaternion(new THREE.Quaternion());
          return +(new THREE.Vector3(0, 0, 1).applyQuaternion(q).angleTo(cam.clone().sub(p)) * 180 / Math.PI).toFixed(1); });
        return { who: c.who, kind: c.kind, eyesOff: off, yaw: +(c.yaw * 180 / Math.PI).toFixed(1), pitch: +(c.pitch * 180 / Math.PI).toFixed(1), jaw: c.jaw, blink: c.man.userData.face.uBlink.value };
      });
    }
  };
}

/* Your hand on a crewman: it stops at his suit (drawn at the surface, not through him), turns red, the controller
 * buzzes, he turns to you, and onTouch(who) lets him say something. lives: the crew-life objects to test against.
 * Call after the hand is posed; returns the touch or null. */
const _a = new THREE.Vector3(), _b = new THREE.Vector3(), _q1 = new THREE.Quaternion();
export function handOnCrew(hand, lives, gamepad, onTouch) {
  hand.position.set(0, 0, 0); hand.updateWorldMatrix(true, true);
  const tip = hand.userData.tip.getWorldPosition(_a), palm = hand.getWorldPosition(_b);
  let best = null;
  for (const life of lives) {
    if (!life) continue;
    for (const pt of [tip, palm]) { const h = life.touch(pt); if (h && (!best || h.depth > best.depth)) best = h; }
  }
  if (!best) { hand.userData.onCrew = false; return null; }
  hand.parent.getWorldQuaternion(_q1).invert();
  hand.position.copy(best.push).applyQuaternion(_q1);
  if (!hand.userData.onCrew) {
    gamepad?.hapticActuators?.[0]?.pulse?.(0.4, 50)?.catch?.(() => {});
    best.life.react(best.who);
    if (onTouch) onTouch(best.who);
  }
  hand.userData.onCrew = true;
  return best;
}
