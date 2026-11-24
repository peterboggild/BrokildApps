/* Sleeper Agent's sound engine: the noise loops and the ambient synth themes.

   Shared by Sleeper Agent (health-apps/sleep-noise/index.html) and by
   Lysrytme (vr-apps/lysrytme/), which also runs it inside a Web Worker so
   the headset never stalls a frame while twenty seconds of rain are being
   written. Plain ES5 with no DOM: it only needs Math and Float32Array, and
   an AudioContext (or anything with sampleRate and createBuffer) for the
   functions that hand back AudioBuffers.

   Everything here was lifted unchanged from Sleeper Agent's page; the only
   difference is that the space amount (0..1, how far into the rendered
   room the sound is placed) is passed in instead of read from the page's
   state. */
(function (root) {
  "use strict";

  /* ============================================================
     Noise generation
     ============================================================ */

  // The five faders. Each is rendered into its own looping buffer and summed,
  // so any of the 2^5 combinations can be dialled in and held all night.
  // Pink and white are gone from the panel — pink survives inside the code as
  // the raw material the rains are cut from, and white as the ocean's foam.
  var LAYERS = [
    { id: "brown",    name: "Brown"     },
    { id: "green",    name: "Green"     },
    { id: "rain",     name: "Rain"      },
    { id: "softrain", name: "Soft rain" },
    { id: "ocean",    name: "Ocean"     }
  ];

  // Five starting points. Each one sets every fader, the tone and the ambient
  // theme, so a preset is a whole state rather than a nudge — and any fader
  // moved afterwards drops the mix back to "custom" without undoing anything.
  var PRESETS = [
    {
      id: "deep", name: "Deep",
      desc: "Brown noise alone, rolled off dark. The plain overnight setting.",
      mix: { brown: 72, green: 0, rain: 0, softrain: 0, ocean: 0 },
      tone: 45, pad: "off", padLevel: 40
    },
    {
      id: "rainroom", name: "Rainroom",
      desc: "Brown under soft rain, with the minor synth theme a long way behind it.",
      mix: { brown: 55, green: 0, rain: 0, softrain: 48, ocean: 0 },
      tone: 60, pad: "rainroom", padLevel: 34
    },
    {
      id: "forest", name: "Forest",
      desc: "Green noise with a light rain through it — wind in trees, weather coming.",
      mix: { brown: 18, green: 62, rain: 24, softrain: 0, ocean: 0 },
      tone: 70, pad: "off", padLevel: 40
    },
    {
      id: "shore", name: "Shore",
      desc: "Surf on its own swell, with brown noise filling in underneath.",
      mix: { brown: 30, green: 0, rain: 0, softrain: 0, ocean: 70 },
      tone: 62, pad: "off", padLevel: 40
    },
    {
      id: "drift", name: "Drift",
      desc: "Quiet green and far-off surf under the bright synth theme.",
      mix: { brown: 0, green: 30, rain: 0, softrain: 14, ocean: 22 },
      tone: 66, pad: "meadow", padLevel: 55
    }
  ];

  // Robert Bristow-Johnson biquad, applied in place over a Float32Array.
  function biquad(x, sr, kind, f0, Q) {
    var w0 = 2 * Math.PI * f0 / sr, cs = Math.cos(w0), sn = Math.sin(w0), al = sn / (2 * Q);
    var b0, b1, b2, a0, a1, a2;
    if (kind === "bandpass") { b0 = al; b1 = 0; b2 = -al; }
    else if (kind === "highpass") { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; }
    else { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; }
    a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al;
    var B0 = b0 / a0, B1 = b1 / a0, B2 = b2 / a0, A1 = a1 / a0, A2 = a2 / a0;
    var x1 = 0, x2 = 0, y1 = 0, y2 = 0, out = new Float32Array(x.length);
    for (var i = 0; i < x.length; i++) {
      var xi = x[i];
      var yi = B0 * xi + B1 * x1 + B2 * x2 - A1 * y1 - A2 * y2;
      out[i] = yi; x2 = x1; x1 = xi; y2 = y1; y1 = yi;
    }
    return out;
  }

  function genRaw(type, n, sr) {
    var out = new Float32Array(n), i, w;
    if (type === "white") {
      for (i = 0; i < n; i++) out[i] = Math.random() * 2 - 1;
      return out;
    }
    if (type === "brown") {
      var last = 0;
      for (i = 0; i < n; i++) {
        w = Math.random() * 2 - 1;
        last = (last + 0.02 * w) / 1.02;
        out[i] = last * 3.5;
      }
      // Trim inaudible sub-bass so phone speakers do not waste excursion on it.
      return biquad(out, sr, "highpass", 26, 0.707);
    }
    if (type === "ocean") {
      // Surf is brown noise breathing. A long swell lifts the whole band and
      // lets it fall again, with foam hissing over the top of each crest a
      // moment after it. The swell is written as a whole number of cycles
      // across the loop, so the wave the buffer ends on is the wave it starts
      // on and the seam falls inside a single breath rather than across two.
      var nl = Math.round(LOOP_SEC * sr);
      var body = genRaw("brown", n, sr);
      var foamRaw = new Float32Array(n);
      for (i = 0; i < n; i++) foamRaw[i] = Math.random() * 2 - 1;
      var foam = biquad(foamRaw, sr, "bandpass", 1500, 0.6);
      foamRaw = null;
      var TAU = 2 * Math.PI;
      for (i = 0; i < n; i++) {
        // Two swells at two and three cycles a loop: their crests only meet
        // once, so no two waves in the twenty seconds break quite alike.
        var p = (i % nl) / nl;
        var s = Math.pow(0.62 * (0.5 - 0.5 * Math.cos(TAU * 2 * p)) +
                         0.38 * (0.5 - 0.5 * Math.cos(TAU * 3 * p + 1.1)), 1.6);
        // Foam lags the water that throws it and is scarce between waves,
        // which is what the steep power does.
        var f = Math.pow(0.62 * (0.5 - 0.5 * Math.cos(TAU * 2 * p - 0.7)) +
                         0.38 * (0.5 - 0.5 * Math.cos(TAU * 3 * p + 0.4)), 3.2);
        out[i] = body[i] * (0.34 + 1.05 * s) + foam[i] * 0.42 * f;
      }
      body = foam = null;
      return biquad(out, sr, "lowpass", 5200, 0.707);
    }

    if (type === "rain" || type === "softrain") {
      // Rain is three sounds at once: a wash of drops too far off to pick
      // apart, nearer ones pattering through it, and a low rumble underneath.
      // Synthesised rather than looped from a recording, so it costs a few
      // dozen lines instead of a megabyte and never repeats inside the loop.
      // Soft rain is the same weather heard through a closed window: fewer
      // drops, softer ones, and the top of the band shut out.
      var soft = type === "softrain";
      var pink = genRaw("pink", n, sr);
      var wash = biquad(pink, sr, "highpass", soft ? 260 : 320, 0.707);
      var sizzle = biquad(pink, sr, "bandpass", soft ? 1800 : 2600, 0.5);
      var rumble = biquad(pink, sr, "lowpass", 220, 0.707);
      var wG = soft ? 0.22 : 0.25, sG = soft ? 0.07 : 0.20, rG = soft ? 0.58 : 0.50;
      for (i = 0; i < n; i++) out[i] = wash[i] * wG + sizzle[i] * sG + rumble[i] * rG;
      pink = wash = sizzle = rumble = null;

      // Short bandpassed textures the individual drops are cut from: low taps
      // on wood, the mid body of most drops, and bright ticks on glass.
      var texN = Math.min(Math.round(0.5 * sr), n);
      var tex = (soft ? [700, 1500, 3000] : [900, 2200, 4800]).map(function (f) {
        var t = new Float32Array(texN);
        for (var k = 0; k < texN; k++) t[k] = Math.random() * 2 - 1;
        return biquad(t, sr, "bandpass", f, 2.4);
      });

      var maxDrop = Math.round(0.05 * sr);
      var drops = Math.round((n / sr) * (soft ? 26 : 55));
      for (var d = 0; d < drops; d++) {
        var pick = Math.random();
        var tx = tex[pick < 0.18 ? 0 : (pick < 0.78 ? 1 : 2)];
        var dur = Math.round((0.006 + Math.random() * 0.034) * sr);
        var at = Math.floor(Math.random() * (n - maxDrop));
        var off = Math.floor(Math.random() * (texN - dur));
        // Squaring the random keeps most drops faint and the loud ones rare.
        var amp = (soft ? 0.2 : 0.3) + Math.random() * Math.random() * (soft ? 1.5 : 3.6);
        var fall = 3.4 + Math.random() * 3.4;
        var dk = Math.exp(-fall / dur), rk = Math.exp(-55 / dur), de = 1, re = 1;
        for (i = 0; i < dur; i++) {
          out[at + i] += tx[off + i] * amp * (1 - re) * de;
          de *= dk; re *= rk;
        }
      }

      // Round the drop peaks off, so rain normalises to the same loudness as
      // the steady noises instead of being held back by its own transients.
      var s2 = 0;
      for (i = 0; i < n; i++) s2 += out[i] * out[i];
      var rr = Math.sqrt(s2 / n), thr = rr * 2.2, span = rr * 1.2;
      for (i = 0; i < n; i++) {
        var a = out[i] < 0 ? -out[i] : out[i];
        if (a > thr) {
          var v = thr + span * (1 - Math.exp(-(a - thr) / span));
          out[i] = out[i] < 0 ? -v : v;
        }
      }
      // The glass pane: everything above it stays outside.
      return soft ? biquad(out, sr, "lowpass", 3200, 0.707) : out;
    }

    // Pink (Paul Kellet's refined method) — also the base for green.
    var b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    for (i = 0; i < n; i++) {
      w = Math.random() * 2 - 1;
      b0 = 0.99886 * b0 + w * 0.0555179;
      b1 = 0.99332 * b1 + w * 0.0750759;
      b2 = 0.96900 * b2 + w * 0.1538520;
      b3 = 0.86650 * b3 + w * 0.3104856;
      b4 = 0.55000 * b4 + w * 0.5329522;
      b5 = -0.7616 * b5 - w * 0.0168980;
      out[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11;
      b6 = w * 0.115926;
    }
    if (type === "green") {
      // Broad mid hump around 500 Hz, with some of the pink left underneath.
      var bp = biquad(out, sr, "bandpass", 500, 0.55);
      for (i = 0; i < n; i++) out[i] = bp[i] + out[i] * 0.32;
    }
    return out;
  }

  var LOOP_SEC = 20, XFADE_SEC = 1;

  /* ============================================================
     The room

     The loops have always been stereo, but their two channels were
     two unrelated noises: wide, even, and with nothing around them —
     the sound sits on the phone rather than anywhere. What follows
     puts it in a room instead.

     Each sound is built as a handful of virtual sources standing
     around the listener. Every one of them reaches the two ears at
     slightly different times and through the shadow of the head in
     between — the two cues the ear reads a direction from — coloured
     for its height and its distance, with a small reverberant tail
     behind the lot so that the drops in rain land in a space rather
     than in a vacuum. Below a couple of hundred hertz the two ears
     are put back together: no room can pull the bass apart, and one
     speaker sums it to mono regardless.

     All of it happens once, while the loop is being written. What
     plays all night is the graph it always was — nothing here costs
     a cycle after the buffer is built.
     ============================================================ */

  // Head radius and the speed of sound: between them they set how late and how
  // dull a sound arrives at the ear on the far side.
  var HEAD_R = 0.0875, SOUND_C = 343;

  // Where each sound is standing. Azimuth in degrees, zero straight ahead and
  // positive to the right; elevation above the ear; distance in metres. Rain
  // is mostly overhead with one source splashing at your feet, soft rain is
  // gathered at a window off to the left, ocean is a long way out in front,
  // and the two plain noises simply surround you.
  var ROOMS = {
    brown: {
      tail: 0.16, size: 1.05, damp: 2200, spread: 1,
      src: [{ az: -148, el: -4, d: 3.0 }, { az: -76, el: 10, d: 2.4 }, { az: -24, el: 2, d: 2.7 },
            { az: 30, el: 8, d: 2.6 }, { az: 82, el: -6, d: 2.3 }, { az: 150, el: 6, d: 3.2 }]
    },
    green: {
      tail: 0.20, size: 1.15, damp: 2600, spread: 1,
      src: [{ az: -128, el: 14, d: 3.4 }, { az: -58, el: 4, d: 2.6 }, { az: -12, el: 20, d: 3.0 },
            { az: 46, el: 8, d: 2.5 }, { az: 104, el: 16, d: 3.2 }, { az: 166, el: 2, d: 3.6 }]
    },
    rain: {
      tail: 0.34, size: 1.0, damp: 2000, spread: 0.55, round: 2.6, trim: 0.8,
      src: [{ az: -120, el: 38, d: 2.6 }, { az: -44, el: 52, d: 2.2 }, { az: 22, el: 44, d: 2.4 },
            { az: 96, el: 30, d: 2.8 }, { az: 168, el: 24, d: 3.0 }, { az: -8, el: -22, d: 2.0 }]
    },
    softrain: {
      // Soft rain spends most of its energy on a rumble below hearing, and the
      // trim above hands that headroom back to the part of it you can hear.
      // Holding it there keeps the fader at the level Flat set it to.
      tail: 0.40, size: 1.25, damp: 1700, spread: 0.6, trim: 0.68, round: 3.0,
      src: [{ az: -64, el: 22, d: 4.2 }, { az: -40, el: 36, d: 3.8 }, { az: -18, el: 14, d: 4.6 },
            { az: 24, el: 30, d: 4.0 }, { az: 120, el: 10, d: 5.0 }]
    },
    ocean: {
      // Ocean is held at the peak ceiling by its own crests wherever the
      // slider stands, so what the room hands back is not level but balance:
      // the sub-bass it trims turns into audible band, and the trim puts that
      // back where plain stereo had it.
      tail: 0.26, size: 1.4, damp: 2400, spread: 1.3, trim: 0.86,
      src: [{ az: -62, el: -4, d: 9 }, { az: -24, el: 0, d: 7.5 }, { az: 20, el: 2, d: 8 },
            { az: 58, el: -4, d: 10 }, { az: 150, el: 0, d: 11 }]
    }
  };

  // A one-pole lowpass mixed back into what went in: a gentle shelf rather
  // than a filter, and cheap enough to run several of per source. k is how
  // much of the top end goes.
  function shelf(x, sr, fc, k) {
    if (!(k > 0)) return x;
    var a = Math.exp(-2 * Math.PI * fc / sr), y = 0, n = x.length, i, v;
    for (i = 0; i < n; i++) {
      v = x[i];
      y = v + a * (y - v);
      x[i] = v + k * (y - v);
    }
    return x;
  }

  // Take a notch out of the band the outer ear tells height with: the higher
  // the source, the higher its notch sits.
  function notch(x, sr, f0, depth) {
    var bp = biquad(x, sr, "bandpass", f0, 1.6), n = x.length, i;
    for (i = 0; i < n; i++) x[i] -= bp[i] * depth;
    return x;
  }

  // Schroeder all-pass: the same spectrum with its phase scrambled. Three in a
  // row turn one noise into a second unrelated to the first, for a fraction of
  // what generating another twenty seconds would cost. Two laps, so the state
  // the loop ends on is the state it starts from.
  function allpassLoop(x, sr, delaySec, g) {
    var d = Math.max(1, Math.round(delaySec * sr));
    var line = new Float32Array(d), p = 0, n = x.length, i, lap, xi, wd;
    for (lap = 0; lap < 2; lap++) {
      for (i = 0; i < n; i++) {
        xi = x[i];
        wd = line[p];
        line[p] = xi + g * wd;
        if (++p === d) p = 0;
        if (lap) x[i] = wd - g * xi;
      }
    }
    return x;
  }

  var AP_SETS = [
    [0.0067, 0.0131, 0.0243], [0.0089, 0.0179, 0.0311], [0.0053, 0.0157, 0.0287],
    [0.0101, 0.0211, 0.0349], [0.0073, 0.0193, 0.0263], [0.0061, 0.0149, 0.0331]
  ];

  function decorrelate(dst, src, sr, k, spread) {
    var set = AP_SETS[k % AP_SETS.length], n = src.length, i;
    for (i = 0; i < n; i++) dst[i] = src[i];
    for (i = 0; i < set.length; i++) allpassLoop(dst, sr, set[i] * spread, 0.6);
    return dst;
  }

  // Add one ear's copy of a source, late by the fraction of a millisecond the
  // head costs the far side. The opening samples are the only ones with
  // nothing to read, and the cross-fade covers them.
  function addDelayed(dst, src, samples, gain) {
    var d = Math.floor(samples), fr = samples - d, n = src.length, i, j, a, b;
    for (i = 0; i < n; i++) {
      j = i - d;
      a = j >= 0 ? src[j] : 0;
      b = j >= 1 ? src[j - 1] : 0;
      dst[i] += (a + (b - a) * fr) * gain;
    }
  }

  // Stand one source in the room and write it into both ears. cue is how deep
  // the head's own cues are cut, which is not the same question as how much of
  // the result you end up hearing — the slider answers that one further down,
  // by crossfading the whole placed pair against the plain one.
  function placeSource(mono, far, L, R, sr, s, cue) {
    var n = mono.length, i;
    var az = s.az * Math.PI / 180, el = s.el * Math.PI / 180;
    // Everything on the same cone sounds equally far to one side, and the side
    // is all the delay between the ears encodes. Height and front-or-behind
    // are settled by colour further down.
    var lat = Math.asin(Math.max(-1, Math.min(1, Math.sin(az) * Math.cos(el))));
    var itd = (HEAD_R / SOUND_C) * (lat + Math.sin(lat)) * cue;
    var shade = Math.abs(Math.sin(lat)) * cue;

    // Distance: quieter, and with the top taken off it by the air and by
    // whatever else stands in the way.
    var g = 1 / Math.sqrt(Math.max(s.d, 1));
    shelf(mono, sr, 6500, Math.min(0.5, Math.max(0, (s.d - 2) / 14)) * cue);
    if (Math.abs(s.el) >= 16) notch(mono, sr, 6200 + 62 * Math.abs(s.el), 0.55 * cue);
    var back = (Math.abs(s.az) - 100) / 80;
    if (back > 0) shelf(mono, sr, 4200, Math.min(back, 1) * 0.35 * cue);

    for (i = 0; i < n; i++) far[i] = mono[i];
    shelf(far, sr, 1700, 0.7 * shade);

    var late = Math.abs(itd) * sr;
    if (itd >= 0) {                       // to the right: the left ear is the far one
      for (i = 0; i < n; i++) R[i] += mono[i] * g;
      addDelayed(L, far, late, g);
    } else {
      for (i = 0; i < n; i++) L[i] += mono[i] * g;
      addDelayed(R, far, late, g);
    }
  }

  // Four delay lines, mixed into one another every lap and damped a little
  // each time round: the tail of a soft room. The two outputs come off
  // different lines, so the tail is as wide as the sources standing in it.
  // Two laps again, so what runs off the end of the loop is already sounding
  // when it starts.
  function roomTail(send, sr, size, rt60, damp, L, R) {
    var base = [0.0231, 0.0307, 0.0411, 0.0537];
    var d = [], line = [], p = [], fb = [], lp = [0, 0, 0, 0], dc = [0, 0, 0, 0], v = [0, 0, 0, 0];
    var a = Math.exp(-2 * Math.PI * damp / sr);
    // The four lines are mixed by a reflection, which leaves every direction
    // exactly as loud as it found it — including the one a steady rumble sits
    // in, where nothing but the wall loss stands between the room and a very
    // slow runaway. The send arrives with the bottom already taken off it, and
    // each line drops whatever is left under about ninety hertz on every lap.
    var b = Math.exp(-2 * Math.PI * 90 / sr);
    var dry = biquad(send, sr, "highpass", 110, 0.707);
    var n = send.length, i, k, lap, s, x, y, w;
    for (k = 0; k < 4; k++) {
      d[k] = Math.max(1, Math.round(base[k] * size * sr));
      line[k] = new Float32Array(d[k]);
      p[k] = 0;
      fb[k] = Math.pow(10, -3 * (d[k] / sr) / rt60);
    }
    for (lap = 0; lap < 2; lap++) {
      for (i = 0; i < n; i++) {
        s = 0;
        for (k = 0; k < 4; k++) { v[k] = line[k][p[k]]; s += v[k]; }
        s *= 0.5;
        x = dry[i] * 0.5;
        for (k = 0; k < 4; k++) {
          y = s - v[k];                       // a Householder reflection: lossless
          lp[k] = y + a * (lp[k] - y);        // and the walls take the top off
          dc[k] = lp[k] + b * (dc[k] - lp[k]);
          w = lp[k] - dc[k];
          line[k][p[k]] = x + w * fb[k];
          if (++p[k] === d[k]) p[k] = 0;
        }
        if (lap) {
          L[i] += v[0] + v[2];
          R[i] += v[1] + v[3];
        }
      }
    }
  }

  // Set the tail against the dry sum rather than against its own input, so the
  // same fraction of room comes back whatever was sent into it.
  function mixTail(L, R, TL, TR, wet) {
    var n = L.length, i, ds = 0, ts = 0, g;
    for (i = 0; i < n; i++) {
      ds += L[i] * L[i] + R[i] * R[i];
      ts += TL[i] * TL[i] + TR[i] * TR[i];
    }
    if (!(ts > 1e-18) || !(ds > 1e-18)) return;
    g = wet * Math.sqrt(ds / ts);
    for (i = 0; i < n; i++) { L[i] += TL[i] * g; R[i] += TR[i] * g; }
  }

  // The same rounding-off the rain generator does to its own drops, done once
  // more over the sum: three copies of one drop arriving from three directions
  // land closer together than three separate drops would, and the peaks they
  // make between them would otherwise hold the whole loop back from the
  // loudness every other fader is levelled to. Both channels are pulled by the
  // one gain, so nothing in the image moves while it happens. Ocean never asks
  // for this — its crests are the swell itself, and flattening them would be
  // flattening the sound to suit the meter.
  function roundPeaks(L, R, over) {
    var n = L.length, i, s = 0, a, b, v;
    for (i = 0; i < n; i++) s += L[i] * L[i] + R[i] * R[i];
    var rms = Math.sqrt(s / (2 * n));
    if (!(rms > 1e-9)) return;
    var thr = rms * over, span = rms * 1.4;
    for (i = 0; i < n; i++) {
      a = L[i] < 0 ? -L[i] : L[i];
      b = R[i] < 0 ? -R[i] : R[i];
      if (b > a) a = b;
      if (a > thr) {
        v = thr + span * (1 - Math.exp(-(a - thr) / span));
        v /= a;
        L[i] *= v; R[i] *= v;
      }
    }
  }

  // The top half of the slider is a crossfade between the plain pair the two
  // parents already are and the pair they have been placed into. Both are
  // levelled against each other first, so what travels as the slider moves is
  // the character of the sound and not its loudness.
  function blendPlain(L, R, p0, p1, amt) {
    var n = L.length, i, sp = 0, sd = 0;
    for (i = 0; i < n; i++) {
      sp += L[i] * L[i] + R[i] * R[i];
      sd += p0[i] * p0[i] + p1[i] * p1[i];
    }
    if (!(sp > 1e-18)) return;
    var g = Math.sqrt(sd / sp) * amt, d = 1 - amt;
    for (i = 0; i < n; i++) {
      L[i] = L[i] * g + p0[i] * d;
      R[i] = R[i] * g + p1[i] * d;
    }
  }

  // Fold the low half of the difference between the ears back to the middle.
  // The sum of the two channels is untouched by it, so nothing is lost when a
  // single speaker adds them together.
  function coherentBass(L, R, sr, f0, amount) {
    if (!(amount > 0)) return;
    var n = L.length, side = new Float32Array(n), i;
    for (i = 0; i < n; i++) side[i] = (L[i] - R[i]) * 0.5;
    lowpassLoop(side, sr, f0, 0.707);
    for (i = 0; i < n; i++) { L[i] -= side[i] * amount; R[i] += side[i] * amount; }
  }

  // The whole of the above, for one sound: two noises generated from scratch,
  // the rest of the sources all-passed out of those two, each one placed, and
  // a tail fed from the first of the two — what comes back off the walls is
  // the same weather, not the same signal.
  function renderRoom(type, n, sr, amt) {
    var plan = ROOMS[type] || ROOMS.brown, list = plan.src;
    // Six sources cut from two noises are all but the same signal down at the
    // bottom of the band, where a metre of air is nothing and they simply add
    // up. Brown already trims what is below hearing; the rains are cut from
    // pink, which does not, and left in it would take several audible decibels
    // off everything above it once the loop is levelled.
    var p0 = biquad(genRaw(type, n, sr), sr, "highpass", 28, 0.707);
    var p1 = biquad(genRaw(type, n, sr), sr, "highpass", 28, 0.707);
    var L = new Float32Array(n), R = new Float32Array(n);
    var work = new Float32Array(n), far = new Float32Array(n);
    // The cues themselves stay nearly full depth wherever the slider is: a
    // source half placed is not half as convincing, only quieter, and the
    // crossfade at the end is what quiet means here.
    var cue = 0.55 + 0.45 * amt, i, k;
    for (k = 0; k < list.length; k++) {
      var parent = (k % 2) ? p1 : p0;
      if (k < 2) { for (i = 0; i < n; i++) work[i] = parent[i]; }
      else decorrelate(work, parent, sr, k, plan.spread || 1);
      placeSource(work, far, L, R, sr, list[k], cue);
    }
    work = far = null;
    if (plan.tail) {
      var size = plan.size || 1;
      var TL = new Float32Array(n), TR = new Float32Array(n);
      roomTail(p0, sr, size, 0.8 * size, plan.damp || 2400, TL, TR);
      mixTail(L, R, TL, TR, plan.tail);
      TL = TR = null;
    }
    if (plan.round) roundPeaks(L, R, plan.round);
    if (amt < 1) blendPlain(L, R, p0, p1, amt);
    coherentBass(L, R, sr, 200, 0.8 * amt);
    return { L: L, R: R, trim: plan.trim || 1 };
  }

  // Builds a stereo buffer whose end is cross-faded into its start, so the
  // loop point is inaudible. Flat, its two channels are two unrelated noises;
  // otherwise they are the two ears of someone standing in the room above.
  function makeBuffer(ctx, type, amt) {
    var sr = ctx.sampleRate;
    var n = Math.round(LOOP_SEC * sr);
    var x = Math.round(XFADE_SEC * sr);
    amt = +amt || 0;
    var L, R, trim = 1;
    if (amt > 0) {
      var st = renderRoom(type, n + x, sr, amt);
      L = st.L; R = st.R; trim = st.trim;
    } else {
      L = genRaw(type, n + x, sr);
      R = genRaw(type, n + x, sr);
    }
    return seamed(ctx, L, R, n, x, 0.18 * trim);
  }

  // Normalise to a loudness target, then cross-fade the tail into the head.
  // One gain covers both channels: levelling them apart would undo exactly the
  // differences between the ears that the room was rendered for.
  function seamed(ctx, L, R, n, x, target) {
    var m = n + x, i, v, sum = 0, peak = 1e-9;
    for (i = 0; i < m; i++) {
      v = L[i]; sum += v * v; if (v < 0) v = -v; if (v > peak) peak = v;
      v = R[i]; sum += v * v; if (v < 0) v = -v; if (v > peak) peak = v;
    }
    var rms = Math.sqrt(sum / (2 * m));
    var g = rms > 1e-9 ? Math.min(target / rms, 0.9 / peak) : 0;

    var buf = ctx.createBuffer(2, n, ctx.sampleRate);
    var raw = [L, R], c, d, t;
    for (c = 0; c < 2; c++) {
      d = buf.getChannelData(c);
      for (i = x; i < n; i++) d[i] = raw[c][i] * g;
      for (i = 0; i < x; i++) {
        t = i / x;
        d[i] = (raw[c][n + i] * Math.cos(t * Math.PI / 2) + raw[c][i] * Math.sin(t * Math.PI / 2)) * g;
      }
    }
    // The cross-fade sums two samples a loop apart, so it can overshoot the
    // cap set above — rarely for the steady noises, more often for rain, whose
    // drops give it a longer tail. Pull both channels back by the same amount
    // if it did, so the headroom holds and the image does not move.
    var top = 0, k, mag;
    for (c = 0; c < 2; c++) {
      d = buf.getChannelData(c);
      for (k = 0; k < n; k++) { mag = d[k] < 0 ? -d[k] : d[k]; if (mag > top) top = mag; }
    }
    if (top > 0.9) {
      var back = 0.9 / top;
      for (c = 0; c < 2; c++) {
        d = buf.getChannelData(c);
        for (k = 0; k < n; k++) d[k] *= back;
      }
    }
    return buf;
  }

  /* ============================================================
     Ambient synth

     Five slow chord themes, meant for meditation rather than for
     sleeping through. Each is rendered once into a looping buffer,
     for the same reason the noise is: once the screen is off the
     page may be frozen, and anything driven from setTimeout would
     stall while the audio clock carried on.

     Notes are written into the buffer modulo its length, so a note
     that runs off the end continues at the start — the loop is then
     seamless on its own and needs no cross-fade.
     ============================================================ */

  var THEMES = {
    harbour: {
      name: "Harbour", root: 130.81,          // C3, major
      desc: "Open major chords over a steady root — the plainest and warmest of the five.",
      cutoff: 1150,
      chords: [[0, 7, 12, 16], [5, 12, 17, 21], [7, 14, 19, 23], [9, 16, 21, 24]],
      mel: [12, 14, 16, 19, 21, 24], pat: [0, 2, 4, 2, 3, 1, 5]
    },
    meadow: {
      name: "Meadow", root: 174.61,           // F3, lydian
      desc: "Bright and floating. The raised fourth keeps it from ever quite settling.",
      cutoff: 1350,
      chords: [[0, 7, 12, 16], [2, 9, 14, 18], [0, 7, 11, 16], [7, 14, 19, 23]],
      mel: [12, 14, 16, 18, 21, 23], pat: [0, 3, 1, 4, 2, 5]
    },
    dusk: {
      name: "Dusk", root: 98.00,              // G2, mixolydian / suspended
      desc: "Suspended chords that resolve nowhere — neither happy nor sad, the middle of the set.",
      cutoff: 1000,
      chords: [[0, 7, 12, 17], [0, 7, 12, 14], [5, 12, 17, 21], [10, 17, 22, 26]],
      mel: [12, 14, 17, 19, 22, 24], pat: [0, 1, 3, 2, 4, 1]
    },
    rainroom: {
      name: "Rainroom", root: 110.00,         // A2, natural minor
      desc: "Plain natural minor, soft and a little melancholy without turning bleak.",
      cutoff: 900,
      chords: [[0, 7, 12, 15], [5, 12, 17, 20], [7, 14, 19, 22], [3, 10, 15, 19]],
      mel: [12, 15, 17, 19, 22, 24], pat: [0, 2, 1, 4, 3, 1, 5]
    },
    deep: {
      name: "Deep", root: 73.42,              // D2, dorian over a drone
      desc: "A low drone with sparse minor notes above it — the darkest and slowest.",
      cutoff: 750,
      chords: [[0, 12, 19, 24], [0, 12, 17, 22], [0, 12, 15, 22], [0, 12, 19, 21]],
      mel: [24, 27, 29, 31, 34, 36], pat: [0, 2, 4, 1, 3]
    }
  };

  // Co-prime loop lengths: the two layers only line up again after
  // BED_SEC * MEL_SEC seconds, so nothing audibly repeats for ~12 minutes.
  var BED_SEC = 23, MEL_SEC = 31;

  var SINE_N = 4096, SINE = new Float32Array(SINE_N + 1);
  for (var _s = 0; _s <= SINE_N; _s++) SINE[_s] = Math.sin(2 * Math.PI * _s / SINE_N);

  function semi(root, n) { return root * Math.pow(2, n / 12); }

  // A raised-cosine swell, exactly zero at both ends — the only condition the
  // seamless wrap needs. Every note in a layer shares one, so it is built once.
  function envelope(len, atkFrac) {
    var atk = Math.round(len * atkFrac), rel = len - atk, e = new Float32Array(len), i;
    for (i = 0; i < atk; i++) e[i] = 0.5 - 0.5 * Math.cos(Math.PI * i / atk);
    for (i = atk; i < len; i++) e[i] = 0.5 + 0.5 * Math.cos(Math.PI * (i - atk + 1) / rel);
    return e;
  }

  // One long note, written into the loop modulo its length: a note that runs
  // off the end carries on at the start, so it is still continuous when the
  // buffer repeats. Written in contiguous runs to keep the inner loop tight.
  function addNote(L, R, n, sr, startSec, freq, amp, partials, pan, env) {
    var len = env.length;
    if (len > n) return;
    var start = Math.round(startSec * sr) % n;
    var a = (pan + 1) * Math.PI / 4;
    var gL = amp * Math.cos(a), gR = amp * Math.sin(a);
    var k = partials.length, j;
    var phL = new Float64Array(k), phR = new Float64Array(k),
        inL = new Float64Array(k), inR = new Float64Array(k), pa = new Float64Array(k);
    for (j = 0; j < k; j++) {
      // A hair of detune between the channels: width without audible chorusing.
      inL[j] = freq * partials[j][0] * 0.99925 * SINE_N / sr;
      inR[j] = freq * partials[j][0] * 1.00075 * SINE_N / sr;
      phL[j] = Math.random() * SINE_N;
      phR[j] = Math.random() * SINE_N;
      pa[j] = partials[j][1];
    }
    var i = 0, at = start;
    while (i < len) {
      var run = Math.min(len - i, n - at);
      for (var m = 0; m < run; m++, i++, at++) {
        var e = env[i], sL = 0, sR = 0;
        for (j = 0; j < k; j++) {
          var x = phL[j], y = phR[j], xi = x | 0, yi = y | 0, w = pa[j];
          sL += (SINE[xi] + (SINE[xi + 1] - SINE[xi]) * (x - xi)) * w;
          sR += (SINE[yi] + (SINE[yi + 1] - SINE[yi]) * (y - yi)) * w;
          x += inL[j]; if (x >= SINE_N) x -= SINE_N;
          y += inR[j]; if (y >= SINE_N) y -= SINE_N;
          phL[j] = x; phR[j] = y;
        }
        L[at] += sL * gL * e;
        R[at] += sR * gR * e;
      }
      if (at >= n) at = 0;
    }
  }

  // Lowpass over a loop: the first lap only warms the filter's state up, so the
  // second lap starts where the loop ends and the seam stays silent.
  function lowpassLoop(x, sr, f0, Q) {
    var w0 = 2 * Math.PI * f0 / sr, cs = Math.cos(w0), sn = Math.sin(w0), al = sn / (2 * Q);
    var a0 = 1 + al;
    var B0 = ((1 - cs) / 2) / a0, B1 = (1 - cs) / a0, B2 = ((1 - cs) / 2) / a0;
    var A1 = (-2 * cs) / a0, A2 = (1 - al) / a0;
    var x1 = 0, x2 = 0, y1 = 0, y2 = 0, i, xi, yi, n = x.length;
    for (var lap = 0; lap < 2; lap++) {
      for (i = 0; i < n; i++) {
        xi = x[i];
        yi = B0 * xi + B1 * x1 + B2 * x2 - A1 * y1 - A2 * y2;
        x2 = x1; x1 = xi; y2 = y1; y1 = yi;
        if (lap) x[i] = yi;
      }
    }
    return x;
  }

  // "bed" is the chord wash; "mel" is a sparse line of single notes over it.
  function makePad(ctx, th, secs, layer, amt) {
    var sr = ctx.sampleRate, n = Math.round(secs * sr);
    var buf = ctx.createBuffer(2, n, sr);
    var L = new Float32Array(n), R = new Float32Array(n), i;

    if (layer === "bed") {
      var slot = secs / th.chords.length;
      var bedEnv = envelope(Math.min(Math.round(slot * 2.0 * sr), n), 0.38);
      th.chords.forEach(function (ch, ci) {
        ch.forEach(function (off, vi) {
          var pan = ch.length > 1 ? -0.55 + 1.1 * (vi / (ch.length - 1)) : 0;
          addNote(L, R, n, sr, ci * slot, semi(th.root, off),
                  1 / ch.length, [[1, 1], [2, 0.30], [3, 0.10]], pan, bedEnv);
        });
      });
    } else {
      var mslot = secs / th.pat.length;
      var melEnv = envelope(Math.min(Math.round(mslot * 1.8 * sr), n), 0.42);
      th.pat.forEach(function (idx, k) {
        addNote(L, R, n, sr, k * mslot, semi(th.root, th.mel[idx]),
                0.5, [[1, 1], [2, 0.22]], k % 2 ? 0.35 : -0.35, melEnv);
      });
    }

    lowpassLoop(L, sr, th.cutoff, 0.707);
    lowpassLoop(R, sr, th.cutoff, 0.707);

    // The synth gets the same room as the noise does, at a fraction of the
    // depth: enough that the chords have somewhere to decay into rather than
    // simply stopping. A bigger, slower room than the weather stands in.
    amt = +amt || 0;
    if (amt > 0) {
      var send = new Float32Array(n), TL = new Float32Array(n), TR = new Float32Array(n);
      for (i = 0; i < n; i++) send[i] = (L[i] + R[i]) * 0.5;
      roomTail(send, sr, 1.5, 1.8, 1800, TL, TR);
      mixTail(L, R, TL, TR, 0.30 * amt);
      send = TL = TR = null;
      coherentBass(L, R, sr, 200, 0.8 * amt);
    }

    // Level the layers against each other and against the noise, which is
    // itself normalised to 0.18 RMS.
    var target = layer === "bed" ? 0.11 : 0.055;
    var sum = 0;
    for (i = 0; i < n; i++) sum += L[i] * L[i] + R[i] * R[i];
    var rms = Math.sqrt(sum / (2 * n));
    var g = rms > 1e-9 ? Math.min(target / rms, 6) : 0;
    var cL = buf.getChannelData(0), cR = buf.getChannelData(1);
    for (i = 0; i < n; i++) { cL[i] = L[i] * g; cR[i] = R[i] * g; }
    return buf;
  }

  // The same loops as plain channel arrays, for a worker that has no
  // AudioContext: a stand-in context whose "buffer" is two Float32Arrays.
  function arrayCtx(sr) {
    return {
      sampleRate: sr,
      createBuffer: function (c, n) {
        var ch = [];
        for (var i = 0; i < c; i++) ch.push(new Float32Array(n));
        return { channels: ch, getChannelData: function (k) { return ch[k]; } };
      }
    };
  }
  function noiseArrays(type, sr, amt) { return makeBuffer(arrayCtx(sr), type, amt).channels; }
  function padArrays(themeId, layer, sr, amt) {
    var th = THEMES[themeId];
    return makePad(arrayCtx(sr), th, layer === "bed" ? BED_SEC : MEL_SEC, layer, amt).channels;
  }

  root.SleeperSound = {
    LAYERS: LAYERS, PRESETS: PRESETS, THEMES: THEMES,
    LOOP_SEC: LOOP_SEC, XFADE_SEC: XFADE_SEC, BED_SEC: BED_SEC, MEL_SEC: MEL_SEC,
    genRaw: genRaw, renderRoom: renderRoom,
    makeBuffer: makeBuffer, makePad: makePad,
    noiseArrays: noiseArrays, padArrays: padArrays
  };
})(typeof self !== "undefined" ? self : this);
