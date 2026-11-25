/* The radio and the intercom: voice files played at their mission times, through the sound of each channel.
 *
 * Channels: the launch control loop and the pad loop (band-limited, a little distortion and hiss), Houston
 * (the same, with the Quindar tones before and after each transmission, 2525 and 2475 Hz) and the intercom (your
 * crewmates beside you: wider band, from their side). The voices are the Windows voices for now; the owner's own
 * recordings replace them file for file (assets/script/voice/<id>.ogg). */

export async function makeRadio(ctx, out, scriptUrl) {
  const script = await (await fetch(scriptUrl)).json();
  const base = new URL('voice/', scriptUrl).href;
  const lines = script.lines.map(l => ({ ...l, buf: null, played: false }));
  /* decode all now: small files, and no fetch at the moment a line is due */
  await Promise.all(lines.map(async l => {
    for (let k = 0; k < 3 && !l.buf; k++) {                      /* a dropped connection: try again */
      try { l.buf = await ctx.decodeAudioData(await (await fetch(base + l.id + '.ogg')).arrayBuffer()); }
      catch (e) { if (k === 2) console.warn('voice ' + l.id + ': ' + e.message); else await new Promise(r => setTimeout(r, 300 * (k + 1))); }
    }
  }));

  const shaper = (amount) => {
    const n = 1024, c = new Float32Array(n);
    for (let i = 0; i < n; i++) { const x = i / (n - 1) * 2 - 1; c[i] = Math.tanh(x * amount) / Math.tanh(amount); }
    const w = ctx.createWaveShaper(); w.curve = c; return w;
  };
  function chain(lo, hi, drive, gain, pan) {
    const hp = ctx.createBiquadFilter(); hp.type = 'highpass'; hp.frequency.value = lo; hp.Q.value = 0.7;
    const lp = ctx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = hi; lp.Q.value = 0.7;
    const sh = shaper(drive), g = ctx.createGain(); g.gain.value = gain;
    const p = ctx.createStereoPanner(); p.pan.value = pan;
    hp.connect(lp).connect(sh).connect(g).connect(p).connect(out);
    return hp;
  }
  const CH = {
    loop: chain(380, 3000, 2.2, 0.9, 0),
    pad: chain(350, 3200, 1.8, 0.85, 0),
    houston: chain(300, 2800, 2.6, 0.95, 0),
    intercom: chain(180, 5000, 1.2, 1.0, 0.25)
  };
  /* the background hiss of an open radio channel */
  const hissBuf = ctx.createBuffer(1, ctx.sampleRate, ctx.sampleRate), hd = hissBuf.getChannelData(0);
  for (let i = 0; i < hd.length; i++) hd[i] = Math.random() * 2 - 1;
  function hiss(when, dur, level) {
    const s = ctx.createBufferSource(); s.buffer = hissBuf; s.loop = true;
    const g = ctx.createGain(); g.gain.setValueAtTime(0, when); g.gain.linearRampToValueAtTime(level, when + 0.05);
    g.gain.setValueAtTime(level, when + dur); g.gain.linearRampToValueAtTime(0, when + dur + 0.08);
    s.connect(g).connect(CH.loop); s.start(when); s.stop(when + dur + 0.2);
  }
  function quindar(when, f) {
    const o = ctx.createOscillator(); o.frequency.value = f;
    const g = ctx.createGain(); g.gain.setValueAtTime(0.16, when); g.gain.setValueAtTime(0, when + 0.25);
    o.connect(g).connect(CH.houston); o.start(when); o.stop(when + 0.27);
  }
  const lastByChannel = {};
  /* who is speaking now, and how loud (for the crew's jaws): each voice also feeds a level meter of its speaker */
  const active = {}, meters = {}, mute = ctx.createGain(); mute.gain.value = 0; mute.connect(out);
  const meter = who => { if (!meters[who]) { const a = ctx.createAnalyser(); a.fftSize = 512; a.connect(mute); meters[who] = { a, buf: new Float32Array(512) }; } return meters[who]; };
  function play(l, offset) {
    if (!l.buf) return;
    const ch = script.channels[l.who] || 'loop', node = CH[ch];
    const now = ctx.currentTime + 0.02;
    let start = now;
    if (ch === 'houston') { quindar(now, 2525); start = now + 0.3; }
    const s = ctx.createBufferSource(); s.buffer = l.buf;
    s.connect(node); s.connect(meter(l.who).a); s.start(start, Math.max(0, offset));
    const dur = l.buf.duration - Math.max(0, offset);
    active[l.who] = { ch, from: start, to: start + dur, line: l };
    if (ch !== 'intercom') hiss(start, dur, 0.025);
    if (ch === 'houston') quindar(start + dur + 0.05, 2475);
    lastByChannel[ch] = l;
  }
  /* every frame: start the lines that are due; with time running fast, skip what has gone by */
  function update(t, warp) {
    for (const l of lines) {
      if (typeof l.t !== 'number') continue;            /* cued lines play on events, not at a mission time */
      if (t < l.t) { l.played = false; continue; }
      if (l.played) continue;
      l.played = true;
      const late = t - l.t;
      if (warp > 1 || late > 1.2) continue;
      play(l, late > 0.15 ? late : 0);
    }
  }
  /* the lines for an event (boarding, the White Room, seated), each after its delay */
  let speaking = null;
  function cue(name) {
    for (const l of lines) if (l.cue === name) setTimeout(() => { play(l, 0); speaking = { l, until: performance.now() + (l.dur + 0.5) * 1000 }; }, (l.delay || 0) * 1000);
  }
  /* one line, by its id, now (the crew's own remarks: chatter.js) */
  function say(id) {
    const l = lines.find(x => x.id === id); if (!l || !l.buf) return null;
    play(l, 0); speaking = { l, until: performance.now() + (l.dur + 0.5) * 1000 };
    return l;
  }
  /* the line being heard now, for subtitles */
  function current(t) {
    if (speaking && performance.now() < speaking.until) return speaking.l;
    for (let i = lines.length - 1; i >= 0; i--) { const l = lines[i]; if (typeof l.t === 'number' && t >= l.t && t < l.t + l.dur + 0.4) return l; }
    return null;
  }
  /* the speaker's line while it is heard ({ ch, line }), or null; and its loudness now (RMS, about 0 to 0.3) */
  function talking(who) { const a = active[who]; return a && ctx.currentTime >= a.from && ctx.currentTime < a.to ? a : null; }
  function level(who) {
    const m = meters[who]; if (!m || !talking(who)) return 0;
    m.a.getFloatTimeDomainData(m.buf); let e = 0; for (let i = 0; i < m.buf.length; i++) e += m.buf[i] * m.buf[i];
    return Math.sqrt(e / m.buf.length);
  }
  return { update, current, cue, say, lines, talking, level };
}

/* several radios as one, for the crew's faces: who is talking on any of them, and how loud */
export function combineRadios(...rs) {
  const live = () => rs.filter(Boolean);
  return {
    talking: who => { for (const r of live()) { const t = r.talking(who); if (t) return t; } return null; },
    level: who => Math.max(0, ...live().map(r => r.level(who)))
  };
}
