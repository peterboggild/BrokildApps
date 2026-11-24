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
    try { l.buf = await ctx.decodeAudioData(await (await fetch(base + l.id + '.ogg')).arrayBuffer()); }
    catch (e) { console.warn('voice ' + l.id + ': ' + e.message); }
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
  function play(l, offset) {
    if (!l.buf) return;
    const ch = script.channels[l.who] || 'loop', node = CH[ch];
    const now = ctx.currentTime + 0.02;
    let start = now;
    if (ch === 'houston') { quindar(now, 2525); start = now + 0.3; }
    const s = ctx.createBufferSource(); s.buffer = l.buf;
    s.connect(node); s.start(start, Math.max(0, offset));
    const dur = l.buf.duration - Math.max(0, offset);
    if (ch !== 'intercom') hiss(start, dur, 0.025);
    if (ch === 'houston') quindar(start + dur + 0.05, 2475);
    lastByChannel[ch] = l;
  }
  /* every frame: start the lines that are due; with time running fast, skip what has gone by */
  function update(t, warp) {
    for (const l of lines) {
      if (t < l.t) { l.played = false; continue; }
      if (l.played) continue;
      l.played = true;
      const late = t - l.t;
      if (warp > 1 || late > 1.2) continue;
      play(l, late > 0.15 ? late : 0);
    }
  }
  /* the line being heard now, for subtitles */
  function current(t) {
    for (let i = lines.length - 1; i >= 0; i--) { const l = lines[i]; if (t >= l.t && t < l.t + l.dur + 0.4) return l; }
    return null;
  }
  return { update, current, lines };
}
