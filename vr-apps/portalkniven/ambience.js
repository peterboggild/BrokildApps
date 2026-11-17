/*
 * Portalkniven — what the fractal dimensions sound like, and the spaceship's engines.
 *
 * Nothing is recorded: every sound is built in the Web Audio API. Each fractal dimension has a bed (a continuous sound that is simply there) and events
 * (single sounds that happen somewhere round you, now and then). Events are placed in space (HRTF panning from the listener the main program keeps up to
 * date) and sent into a reverb that suits the place. A sound far away reaches you mostly as reverb and a near one mostly directly, which is how the ear
 * judges distance, so the send into the reverb does not fall off with distance while the direct sound does.
 *
 *   apollo   a vault of bubbles        struck singing bowls, far and near, in a long bright hall
 *   klein    carved, organ-like halls  a deep organ drone that breathes, and slow swells of air
 *   hyper    a crystal lattice         a high glassy shimmer and a constant fine ticking close by
 *   menger   square tunnels for ever   wind in the tunnels, and something metal falling far off, with its echoes
 *   boks     a city of carved cubes    a low rumble of stone, dry gusts, and blocks grinding in other streets, slapping back off the walls
 *   kali     a temple hanging in space near silence, and a gong, rarely, that rings for a long time
 *
 * The ship: a faint hum whenever it is there, and the exhaust: a roar of noise that opens up and gets louder with the throttle, over a low rumble. Both
 * come from where the ship is. The exhaust is also heard a second time, off the nearest wall of the fractal: later and fainter the further the wall is.
 *
 * Deep space: see space() below. All of this runs on the audio thread and costs the picture nothing.
 */
const rnd = (a, b) => a + Math.random() * (b - a);
const pick = a => a[(Math.random() * a.length) | 0];

export function createAmbience() {
  let ctx = null, out = null, white = null, brown = null, verbs = {}, cur = null, bed = null, nextEvt = 0, eng = null, clock = 0, made = [];      // made: everything started since it was last emptied (so a bed can be stopped whole)

  function init(c) {
    ctx = c; out = ctx.createGain(); out.gain.value = 0.9; out.connect(ctx.destination);
    const n = ctx.sampleRate * 3; white = ctx.createBuffer(1, n, ctx.sampleRate); brown = ctx.createBuffer(1, n, ctx.sampleRate);
    const w = white.getChannelData(0), b = brown.getChannelData(0); let acc = 0;
    for (let i = 0; i < n; i++) { w[i] = Math.random() * 2 - 1; acc = (acc + 0.02 * w[i]) / 1.02; b[i] = acc * 3.5; }
    /* reverbs: an impulse response is noise that dies away; how long it takes and how bright it stays is the room */
    const ir = (secs, pow, damp) => { const len = (ctx.sampleRate * secs) | 0, buf = ctx.createBuffer(2, len, ctx.sampleRate);
      for (let ch = 0; ch < 2; ch++) { const d = buf.getChannelData(ch); let lp = 0; for (let i = 0; i < len; i++) { const t = i / len, k = damp * t; lp += ((Math.random() * 2 - 1) - lp) * (1 - Math.min(0.95, k)); d[i] = lp * Math.pow(1 - t, pow); } }
      const cv = ctx.createConvolver(); cv.buffer = buf; const g = ctx.createGain(); g.gain.value = 1; cv.connect(g); g.connect(out); return cv; };
    verbs = { hall: ir(5.0, 2.4, 0.5), organ: ir(6.5, 2.0, 0.9), glass: ir(1.1, 3.0, 0.0), tunnel: ir(2.6, 1.8, 0.75), street: ir(1.6, 2.6, 0.85), temple: ir(8.0, 2.2, 0.7) };
  }
  const noise = (buf, rate = 1) => { const s = ctx.createBufferSource(); s.buffer = buf; s.loop = true; s.playbackRate.value = rate; s.start(0, Math.random() * 2); made.push(s); return s; };
  const osc = (type, f) => { const o = ctx.createOscillator(); o.type = type; o.frequency.value = f; o.start(); made.push(o); return o; };
  const gain = v => { const g = ctx.createGain(); g.gain.value = v; return g; };
  const filt = (type, f, q = 1) => { const b = ctx.createBiquadFilter(); b.type = type; b.frequency.value = f; b.Q.value = q; return b; };
  const lfo = (f, depth, target) => { const o = osc('sine', f), g = gain(depth); o.connect(g); g.connect(target); return o; };
  /** A place in space for a one-shot: returns the node to play into. dist in metres from the listener, in a random direction (mostly level). */
  function place(dist, verb, send) {
    const p = ctx.createPanner(); p.panningModel = 'HRTF'; p.distanceModel = 'inverse'; p.refDistance = 2; p.rolloffFactor = 1;
    const a = rnd(0, Math.PI * 2), e = rnd(-0.35, 0.5), L = ctx.listener, lx = L.positionX ? L.positionX.value : 0, ly = L.positionY ? L.positionY.value : 1.6, lz = L.positionZ ? L.positionZ.value : 0;
    const x = lx + Math.cos(a) * Math.cos(e) * dist, y = ly + Math.sin(e) * dist, z = lz + Math.sin(a) * Math.cos(e) * dist;
    if (p.positionX) { p.positionX.value = x; p.positionY.value = y; p.positionZ.value = z; } else p.setPosition(x, y, z);
    const inp = gain(1), s = gain(send); inp.connect(p); p.connect(out); inp.connect(s); s.connect(verbs[verb]);      // the direct sound falls off with distance, the reverb does not
    setTimeout(() => { try { inp.disconnect(); p.disconnect(); s.disconnect(); } catch (_) { /* gone */ } }, 14000);
    return inp;
  }
  const env = (g, t, peak, attack, decay) => { g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(peak, t + attack); g.gain.exponentialRampToValueAtTime(0.0001, t + attack + decay); };
  /** partials: [[ratio, level, seconds], ...] of a struck thing with fundamental f */
  function strike(dst, f, partials, level, bend = 0) {
    const t = ctx.currentTime;
    for (const [r, a, d] of partials) { const o = ctx.createOscillator(); o.type = 'sine'; o.frequency.setValueAtTime(f * r, t); if (bend) o.frequency.exponentialRampToValueAtTime(f * r * (1 - bend), t + d);
      const g = gain(0); env(g, t, level * a, 0.004, d); o.connect(g); g.connect(dst); o.start(t); o.stop(t + d + 0.1); }
  }
  function burst(dst, buf, type, f0, f1, q, level, attack, decay, trem = 0) {
    const t = ctx.currentTime, s = ctx.createBufferSource(); s.buffer = buf; s.loop = true; const fl = filt(type, f0, q); fl.frequency.exponentialRampToValueAtTime(f1, t + attack + decay);
    const g = gain(0); env(g, t, level, attack, decay); s.connect(fl); fl.connect(g); g.connect(dst);
    if (trem) { const o = ctx.createOscillator(); o.frequency.value = trem; const tg = gain(level * 0.6); o.connect(tg); tg.connect(g.gain); o.start(t); o.stop(t + attack + decay + 0.1); }
    s.start(t, Math.random() * 2); s.stop(t + attack + decay + 0.1);
  }

  /* each: build(dst) starts the bed into dst and returns its nodes (to stop later); every: seconds between events; event(): one event */
  const DIM = {
    apollo: { every: [1.8, 5.5], build(d) { const ns = []; for (const f of [528, 792.5, 1057]) { const o = osc('sine', f), g = gain(0.006); lfo(rnd(0.05, 0.13), 0.005, g.gain); o.connect(g); g.connect(d); g.connect(verbs.hall); ns.push(o); } return ns; },
      event() { const f = pick([196, 220, 261.6, 293.7, 329.6, 392]) * pick([1, 1, 2]); strike(place(rnd(3, 28), 'hall', 0.5), f, [[1, 1, 7], [2.76, 0.45, 3.5], [5.4, 0.2, 1.6], [8.9, 0.08, 0.8]], 0.22); } },
    klein: { every: [9, 17], build(d) { const ns = [], lp = filt('lowpass', 420, 0.7); lfo(0.07, 180, lp.frequency); const g = gain(0.05); lp.connect(g); g.connect(d); const s = gain(0.6); g.connect(s); s.connect(verbs.organ);
        for (const [f, a] of [[55, 1], [55.4, 0.8], [82.5, 0.6], [110.3, 0.5], [165, 0.3], [220.6, 0.15]]) { const o = osc('sawtooth', f), og = gain(a * 0.5); o.connect(og); og.connect(lp); ns.push(o); }
        lfo(0.11, 0.02, g.gain); return ns; },
      event() { burst(place(rnd(6, 25), 'organ', 0.7), white, 'bandpass', 260, 620, 3, 0.5, 1.6, 2.6); } },
    hyper: { every: [0.25, 1.1], build(d) { const ns = []; for (let k = 0; k < 6; k++) { const o = osc('sine', rnd(2200, 6400)), g = gain(0.003); lfo(rnd(0.2, 0.9), 0.003, g.gain); o.connect(g); g.connect(d); g.connect(verbs.glass); ns.push(o); } return ns; },
      event() { strike(place(rnd(0.8, 11), 'glass', 0.35), rnd(2800, 7200), [[1, 1, 0.09], [1.5, 0.4, 0.05]], 0.16); } },
    menger: { every: [5, 12], build(d) { const ns = [], n = noise(white), bp = filt('bandpass', 560, 3.5), g = gain(0.16); lfo(0.09, 260, bp.frequency); lfo(0.23, 0.07, g.gain); n.connect(bp); bp.connect(g); g.connect(d); const s = gain(0.5); g.connect(s); s.connect(verbs.tunnel);
        const n2 = noise(white, 0.7), wh = filt('bandpass', 1480, 22), g2 = gain(0.035); lfo(0.05, 300, wh.frequency); lfo(0.17, 0.03, g2.gain); n2.connect(wh); wh.connect(g2); g2.connect(d); ns.push(n, n2); return ns; },
      event() { const dst = place(rnd(10, 40), 'tunnel', 0.8), dl = ctx.createDelay(1.5), fb = gain(0.5), tap = gain(1); dl.delayTime.value = rnd(0.28, 0.5); tap.connect(dst); tap.connect(dl); dl.connect(fb); fb.connect(dl); dl.connect(dst);      // it falls, and the tunnels repeat it
        strike(tap, rnd(240, 380), [[1, 1, 0.5], [1.69, 0.7, 0.4], [2.62, 0.5, 0.3], [3.84, 0.3, 0.2]], 0.5); setTimeout(() => { try { fb.disconnect(); } catch (_) { /* gone */ } }, 6000); } },
    boks: { every: [6, 14], build(d) { const ns = [], n = noise(brown), lp = filt('lowpass', 95, 0.7), g = gain(0.5); lfo(0.06, 0.2, g.gain); n.connect(lp); lp.connect(g); g.connect(d);
        const n2 = noise(white, 1.3), hp = filt('highpass', 2400, 0.7), g2 = gain(0.012); lfo(0.13, 0.011, g2.gain); n2.connect(hp); hp.connect(g2); g2.connect(d); ns.push(n, n2); return ns; },
      event() { const dst = place(rnd(8, 30), 'street', 0.45), dl = ctx.createDelay(1), sl = gain(0.55), tap = gain(1); dl.delayTime.value = rnd(0.16, 0.3); tap.connect(dst); tap.connect(dl); dl.connect(sl); sl.connect(dst);      // one slap back off the wall across the street
        burst(tap, brown, 'bandpass', rnd(150, 230), rnd(90, 140), 2.5, 1.6, 0.3, rnd(1.0, 2.0), rnd(14, 22)); } },
    kali: { every: [8, 18], build(d) { const o = osc('sine', 41.2), g = gain(0.05); lfo(0.04, 0.03, g.gain); o.connect(g); g.connect(d); return [o]; },
      event() { strike(place(rnd(6, 20), 'temple', 0.9), pick([73.4, 98, 110]), [[1, 1, 10], [1.52, 0.7, 8], [2.31, 0.5, 5], [3.07, 0.35, 3.5], [4.4, 0.2, 2], [6.3, 0.1, 1.2]], 0.3, 0.012); } },
  };

  function setDim(key) {
    if (key === cur) return; cur = key;
    if (bed) { const b = bed, t = ctx.currentTime; b.g.gain.cancelScheduledValues(t); b.g.gain.setTargetAtTime(0, t, 0.5); setTimeout(() => { for (const n of b.nodes) { try { n.stop(); } catch (_) { /* stopped */ } } try { b.g.disconnect(); } catch (_) { /* gone */ } }, 2500); bed = null; }
    if (key && DIM[key]) { const g = gain(0); g.connect(out); g.gain.setTargetAtTime(1, ctx.currentTime, 1.2); made = []; DIM[key].build(g); bed = { g, nodes: made }; made = []; nextEvt = clock + rnd(1, 3); }
  }

  /* ---- the ship ---- */
  function engine() {
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'inverse'; pan.refDistance = 1.2; pan.rolloffFactor = 0.8; pan.connect(out);
    const bus = gain(1); bus.connect(pan);
    const hum = gain(0), hl = filt('lowpass', 170, 0.8); hl.connect(hum); hum.connect(bus); for (const f of [47, 47.6, 94.3]) { const o = osc('sawtooth', f), g = gain(f > 90 ? 0.25 : 0.5); o.connect(g); g.connect(hl); } lfo(0.31, 0.012, hum.gain);
    const ex = gain(0), n = noise(white), lp = filt('lowpass', 400, 0.6), hp = filt('highpass', 70, 0.7); n.connect(lp); lp.connect(hp); hp.connect(ex);
    const rumG = gain(0), rn = noise(brown, 0.8), rl = filt('lowpass', 130, 0.9); rn.connect(rl); rl.connect(rumG);
    const cr = gain(1), cn = noise(brown, 0.25), cg = gain(0); cn.connect(cg); cg.connect(cr.gain); ex.connect(cr); rumG.connect(cr); cr.connect(bus);      // the roar is never quite steady
    /* the same roar again off the nearest wall: later and fainter the further away the wall is */
    const dl = ctx.createDelay(2.5), el = filt('lowpass', 1500, 0.7), eg = gain(0); cr.connect(dl); dl.connect(el); el.connect(eg); eg.connect(out); eg.connect(verbs.tunnel);
    return { pan, hum, ex, lp, rumG, cg, dl, eg };
  }

  /* ---- deep space: what the warp and the black hole sound like ----
   * The warp: a rush of air that climbs in pitch and a tone that rises four octaves as you near the speed of light, beating faster and faster.
   * The black hole: a sub-bass you feel more than hear, two tones a couple of hertz apart, coming from where the hole is in the sky, heavier as it grows. */
  let sp = null;
  function space() {
    const wg = gain(0), n = noise(white), bp = filt('bandpass', 500, 1.4); n.connect(bp); bp.connect(wg); wg.connect(out);
    const tg = gain(0), o1 = osc('sawtooth', 70), o2 = osc('sine', 105), tl = filt('lowpass', 900, 0.7), trem = gain(1); o1.connect(tl); o2.connect(tl); tl.connect(trem); trem.connect(tg); tg.connect(out);
    const tr = osc('sine', 2), trd = gain(0); tr.connect(trd); trd.connect(trem.gain);
    const pan = ctx.createPanner(); pan.panningModel = 'HRTF'; pan.distanceModel = 'linear'; pan.refDistance = 1000; pan.connect(out);
    const hg = gain(0); hg.connect(pan); const ha = osc('sine', 31), hb = osc('sine', 33.3), hn = noise(brown, 0.5), hl = filt('lowpass', 70, 0.8); ha.connect(hg); hb.connect(hg); hn.connect(hl); hl.connect(hg);
    return { wg, bp, tg, o1, o2, tl, tr, trd, pan, hg };
  }

  return {
    /** Call every frame. s: { dim (the fractal's key, or null), ship (is it out?), power (0..1), pos (the ship, a Vector3), wall (metres to the nearest wall, or Infinity),
     *  space (null, or { warp 0..0.93, hole 0..0.42, dir: where the black hole is in the sky }) } */
    update(c, dt, s) {
      if (!c) return; if (!ctx) init(c); clock += dt;
      setDim(s.dim);
      if (cur && DIM[cur] && clock > nextEvt) { nextEvt = clock + rnd(...DIM[cur].every); try { DIM[cur].event(); } catch (e) { console.warn(e); } }
      if (s.space || sp) {
        if (!sp) sp = space();
        const t = ctx.currentTime, b = s.space ? s.space.warp / 0.93 : 0, h = s.space ? s.space.hole / 0.42 : 0;
        sp.wg.gain.setTargetAtTime(0.3 * b * b + (b > 0 ? 0.02 : 0), t, 0.25); sp.bp.frequency.setTargetAtTime(400 + 5600 * b * b, t, 0.25);
        sp.tg.gain.setTargetAtTime(0.07 * Math.sqrt(b), t, 0.25); sp.o1.frequency.setTargetAtTime(70 * Math.pow(2, 4 * b), t, 0.3); sp.o2.frequency.setTargetAtTime(105 * Math.pow(2, 4 * b), t, 0.3);
        sp.tl.frequency.setTargetAtTime(600 + 5000 * b, t, 0.3); sp.tr.frequency.setTargetAtTime(1.5 + 16 * b, t, 0.3); sp.trd.gain.setTargetAtTime(0.45 * b, t, 0.3);
        sp.hg.gain.setTargetAtTime(0.55 * h, t, 0.4);
        if (s.space && s.space.dir) { const L = ctx.listener, P = sp.pan, x = (L.positionX ? L.positionX.value : 0) + s.space.dir.x * 30, y = (L.positionY ? L.positionY.value : 1.6) + s.space.dir.y * 30, z = (L.positionZ ? L.positionZ.value : 0) + s.space.dir.z * 30;
          if (P.positionX) { P.positionX.setTargetAtTime(x, t, 0.1); P.positionY.setTargetAtTime(y, t, 0.1); P.positionZ.setTargetAtTime(z, t, 0.1); } else P.setPosition(x, y, z); }
      }
      if (s.ship || eng) {
        if (!eng) eng = engine();
        const t = ctx.currentTime, p = s.ship ? s.power : 0, P = eng.pan;
        if (s.ship && s.pos) { if (P.positionX) { P.positionX.setTargetAtTime(s.pos.x, t, 0.05); P.positionY.setTargetAtTime(s.pos.y, t, 0.05); P.positionZ.setTargetAtTime(s.pos.z, t, 0.05); } else P.setPosition(s.pos.x, s.pos.y, s.pos.z); }
        eng.hum.gain.setTargetAtTime(s.ship ? 0.045 : 0, t, 0.4);
        eng.ex.gain.setTargetAtTime(p > 0 ? 0.05 + 0.5 * p * p : 0, t, 0.12); eng.lp.frequency.setTargetAtTime(350 + 5200 * p * p, t, 0.12);      // the roar opens up as it grows
        eng.rumG.gain.setTargetAtTime(p > 0 ? 0.25 + 0.9 * p : 0, t, 0.15); eng.cg.gain.setTargetAtTime(0.5 * p, t, 0.2);
        const d = s.wall, near = isFinite(d) && d < 12;                    // metres in the room; the ship is a 1 : 22 model, so the echo is timed for 22 times that
        eng.dl.delayTime.setTargetAtTime(near ? Math.min(2.4, Math.max(0.03, d * 22 * 2 / 343)) : 1.0, t, 0.25);
        eng.eg.gain.setTargetAtTime(near && p > 0 ? 0.55 / (1 + d * 1.2) : 0, t, 0.2);
      }
    },
    /** The ship has hit something. */
    thud(pos) { if (!ctx) return; const dst = gain(1); dst.connect(out); const s = gain(0.6); dst.connect(s); s.connect(verbs.tunnel);
      strike(dst, 62, [[1, 1, 0.5], [1.7, 0.6, 0.3], [2.9, 0.3, 0.2]], 0.7, 0.3); burst(dst, white, 'lowpass', 900, 200, 0.7, 0.5, 0.005, 0.25); setTimeout(() => { try { dst.disconnect(); s.disconnect(); } catch (_) { /* gone */ } }, 2500); },
    dims: Object.keys(DIM),
  };
}
