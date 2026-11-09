/* Structure: run the analyser on the master, show what it found, and offer it as sections.

   Deliberately two steps with a look in between. The analyser is good but it is not clairvoyant,
   and a wrong draft that silently replaces a timeline is worse than no draft: so analysing only
   draws, and the timeline is not touched until "Use as sections" is pressed. Everything on screen
   between those two presses is there to let the structure be judged before it is accepted.

   What is drawn:
     - a line down the timeline at every boundary, labelled with the passage's type letter
     - the novelty curve itself in the waveform lane, with the detection threshold as a dotted
       line across it, so a missed boundary can be seen to have been a near miss rather than a
       blind spot, and the sensitivity moved accordingly
     - a bracket under each segment with its length in bars

   Type letters, not names. Which passage is the chorus needs knowledge of song form that this
   pipeline does not have; what it does know is which passages are the same passage, and that is
   what a letter says. Naming them is the one judgement left to the person who wrote the song.
*/
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const { h, button, toast } = FV.ui;

  const COL = {
    line: '#ffb454',
    lineSoft: 'rgba(255,180,84,0.28)',
    snapped: '#6cc4ff',
    curve: 'rgba(255,180,84,0.85)',
    thresh: 'rgba(255,255,255,0.3)',
    text: '#ffd9a3'
  };

  // One analysis at a time, held here so the timeline can draw it without owning it.
  const state = {
    result: null,      // the last Structure.segment() result
    grid: null,        // the bpm/offset/beatsPerBar it was computed for
    sens: 1,           // multiplier on the detection threshold; 1 is the measured default
    running: false,
    panel: null
  };

  const S = () => window.FVStructure;

  /* ------------------------------------------------------------------ running -- */

  // The grid the result was computed for. If the user has since changed the tempo the drawing is
  // stale and says so rather than lying by a bar and a half.
  function stale(ctx) {
    if (!state.result || !state.grid) return false;
    const p = ctx.project();
    return state.grid.bpm !== p.bpm || state.grid.beatsPerBar !== p.beatsPerBar || state.grid.offset !== (p.offset || 0);
  }

  function run(ctx) {
    if (state.running) return;
    const lib = S();
    if (!lib) { toast('structure.js did not load.', 'err'); return; }
    const a = ctx.analysis();
    const master = a && a.master;
    if (!master) { toast('Load a master first: the structure comes from the song.', 'warn'); return; }
    if (!master.spec) {
      toast('This master was analysed before structure analysis existed. Remove and re-add it to analyse it.', 'warn', 9000);
      return;
    }
    const p = ctx.project();
    const grid = { bpm: p.bpm, beatsPerBar: p.beatsPerBar, offset: p.offset || 0 };
    state.running = true;
    render(ctx);
    // Out of the click handler so the button repaints as busy before the work starts.
    setTimeout(() => {
      try {
        const r = lib.segment(master, grid, { thresh: lib.ABS_THRESH * state.sens });
        state.result = r;
        state.grid = grid;
        if (!r.ok) {
          toast(r.reason === 'too-short' ? 'The song is too short to find structure in.'
            : r.reason === 'no-spectral' ? 'This master has no spectral analysis: re-add the file.'
              : `Structure analysis: ${r.reason}`, 'warn', 7000);
        } else if (!r.boundaries.length) {
          toast('No section boundaries found. Either the song holds one passage throughout, or raise the sensitivity.', 'warn', 8000);
        }
      } catch (err) {
        console.error(err);
        state.result = null;
        toast(`Structure analysis failed: ${err.message}`, 'err', 8000);
      }
      state.running = false;
      render(ctx);
      ctx.rebuild('timeline');
    }, 0);
  }

  /* ----------------------------------------------------------------- applying -- */

  /* Boundaries become section lengths; type letters become scene assignments.

     One scene per type, taken from the scenes the project already has and cycled if there are
     fewer scenes than types. Nothing new is invented and no scene's parameters are touched: the
     claim being made is only "these passages are the same, so they should look the same", which
     is what the analysis actually established. Which fractal suits which passage is a judgement,
     and it stays the user's. */
  function apply(ctx) {
    const r = state.result;
    if (!r || !r.ok || !r.segments.length) { toast('Nothing to apply: analyse first.', 'warn'); return; }
    if (stale(ctx)) { toast('The tempo changed since this was analysed. Analyse again first.', 'warn', 7000); return; }
    const p = ctx.project();
    const bpb = p.beatsPerBar;

    const letters = [];
    r.segments.forEach((s) => { if (letters.indexOf(s.type) < 0) letters.push(s.type); });
    const sceneFor = {};
    letters.forEach((L, i) => { sceneFor[L] = p.scenes[i % p.scenes.length].id; });

    // Named below, once the occurrence counts are known.
    const sections = r.segments.map((s, i) => ({
      id: window.FVSchema.uid('sec'),
      name: '',
      length: Math.max(bpb, s.bars * bpb),
      sceneId: sceneFor[s.type],
      // A sharp change gets a short crossfade and a gentle one a long fade, from the novelty peak
      // height: it is the same measurement the boundary came from, so it costs nothing to use.
      transition: i === 0 ? 0 : Math.max(1, Math.round(bpb * (1 - s.sharpness) * 2 + 1))
    }));
    // Name them by type and occurrence: "A 1", "B 1", "A 2" - readable, and honest about being
    // letters rather than guessing "Chorus".
    const seen = {};
    sections.forEach((sec, i) => {
      const t = r.segments[i].type;
      seen[t] = (seen[t] || 0) + 1;
      sec.name = `${t}${seen[t]}`;
    });

    // Keyframe lanes are stored per section id, and every id here is new, so any existing lanes
    // would be orphaned. Schema.normalizeProject drops those, but say so rather than let it be a
    // surprise.
    const hadKeys = (p.mod || []).some((m) => m.keys && m.keys.length)
      || (p.keys && Object.keys(p.keys).length > 0);

    p.sections = sections;
    ctx.commit('Sections from structure analysis');
    ctx.rebuild('timeline', 'scenes', 'render');
    toast(`${sections.length} sections, ${letters.length} passage type${letters.length === 1 ? '' : 's'}`
      + (hadKeys ? '. Keyframes belonged to the old sections and were dropped.' : '.'), 'ok', 7000);
  }

  /* ------------------------------------------------------------------- drawing -- */

  /* Called from ui-timeline's _draw with its own geometry, so the overlay cannot drift out of
     register with the ruler. `view` gives: g (2d context), vw, scrollLeft, x(beat) -> px,
     ruler/section band heights, and the waveform lane's top and height. */
  function overlay(view) {
    const r = state.result;
    if (!r || !r.ok) return;
    const { g, vw, sl, x, rulerH, secH, waveY, waveH, bpb } = view;
    const lib = S();

    // --- the novelty curve, in the waveform lane, under whatever else is drawn there
    if (r.novelty && waveH > 10) {
      const h0 = waveY + waveH;
      const scale = Math.max(lib.ABS_THRESH * 3, Math.max.apply(null, Array.from(r.novelty)));
      g.beginPath();
      let started = false;
      for (let b = 0; b < r.novelty.length; b += 1) {
        const px = x(b) - sl;
        if (px < -4) continue;
        if (px > vw + 4) break;
        const y = h0 - Math.min(1, r.novelty[b] / scale) * (waveH - 4);
        if (!started) { g.moveTo(px, y); started = true; } else g.lineTo(px, y);
      }
      if (started) {
        g.strokeStyle = COL.curve;
        g.lineWidth = 1;
        g.stroke();
      }
      // the threshold the peaks had to clear
      const ty = h0 - Math.min(1, (lib.ABS_THRESH * state.sens) / scale) * (waveH - 4);
      g.save();
      g.setLineDash([3, 3]);
      g.strokeStyle = COL.thresh;
      g.beginPath();
      g.moveTo(0, ty);
      g.lineTo(vw, ty);
      g.stroke();
      g.restore();
    }

    // --- a line at every boundary, through the whole height
    const top = rulerH;
    const bottom = waveY + waveH;
    g.font = '600 10px Segoe UI, system-ui, sans-serif';
    g.textBaseline = 'middle';
    g.textAlign = 'left';
    r.segments.forEach((s, i) => {
      const px = x(s.bar0 * bpb) - sl;
      if (i > 0 && px > -40 && px < vw + 40) {
        g.fillStyle = s.snapped ? COL.snapped : COL.line;
        g.fillRect(Math.round(px), top, 1, bottom - top);
        // a triangle in the ruler so the boundary is findable when zoomed out
        g.beginPath();
        g.moveTo(px - 4, top);
        g.lineTo(px + 4, top);
        g.lineTo(px, top + 5);
        g.closePath();
        g.fill();
      }
      /* The type letter goes at the top of the waveform lane, not in the section band: the band
         already carries each section's own name and scene, and two labels in one strip is two
         things to read where there should be one. The lane below is the overlay's own. */
      const px1 = x(s.bar1 * bpb) - sl;
      if (px1 < 0 || px > vw) return;
      const w = px1 - px;
      if (w < 22) return;
      const lx = Math.max(2, px + 3);
      const ly = waveY + 8;
      const label = w > 60 ? `${s.type} ${s.bars}` : s.type;
      const tw = g.measureText(label).width;
      g.fillStyle = 'rgba(8,11,17,0.82)';
      g.fillRect(lx - 2, ly - 7, tw + 5, 14);
      g.fillStyle = COL.text;
      g.fillText(label, lx, ly);
    });
  }

  /* --------------------------------------------------------------------- panel -- */

  function summary(r) {
    if (!r || !r.ok) return '';
    const form = r.segments.map((s) => `${s.type}${s.bars}`).join(' ');
    return form;
  }

  function render(ctx) {
    if (!state.panel) return;
    const r = state.result;
    const body = state.panel.body;
    body.innerHTML = '';

    const row = h('div', { class: 'st-row' });
    const btn = button(state.running ? 'Analysing…' : 'Analyse the song', {
      title: 'Find where the song changes, from the master. Nothing is altered until you apply it.',
      onClick: () => run(ctx)
    });
    if (state.running) btn.disabled = true;
    row.appendChild(btn);
    body.appendChild(row);

    // sensitivity, because a missed boundary and a spurious one are fixed from opposite ends
    const sens = h('div', { class: 'st-row' },
      h('label', {}, 'Sensitivity'),
      h('input', {
        type: 'range', min: '0.3', max: '3', step: '0.05', value: String(state.sens),
        oninput: (e) => {
          state.sens = Number(e.target.value);
          const out = sens.querySelector('b');
          if (out) out.textContent = state.sens.toFixed(2) + '×';
        },
        onchange: () => { if (state.result) run(ctx); }
      }),
      h('b', {}, state.sens.toFixed(2) + '×'));
    body.appendChild(sens);

    if (state.running) return;

    if (!r) {
      body.appendChild(h('p', { class: 'st-hint' },
        'The analyser reads the master and the tempo grid. It reports where the song changes and which passages are the same passage; it does not name them.'));
      return;
    }
    if (!r.ok) {
      body.appendChild(h('p', { class: 'st-hint' }, r.reason === 'too-short'
        ? 'Too short to find structure in.'
        : 'No spectral analysis on this master: remove and re-add the file.'));
      return;
    }

    if (stale(ctx)) {
      body.appendChild(h('p', { class: 'st-warn' },
        `Analysed at ${state.grid.bpm} BPM; the project is now at ${ctx.project().bpm}. Analyse again.`));
    }

    body.appendChild(h('p', { class: 'st-form' }, summary(r)));
    body.appendChild(h('p', { class: 'st-hint' },
      `${r.segments.length} segments, ${r.types} passage type${r.types === 1 ? '' : 's'}, over ${r.bars} bars.`
      + (r.typeThreshold === 0 && r.segments.length > 1
        ? ' No passage repeats clearly enough to group, so each is its own type.' : '')));

    const table = h('table', { class: 'st-table' });
    table.appendChild(h('tr', {},
      h('th', {}, ''), h('th', {}, 'bar'), h('th', {}, 'bars'),
      h('th', { title: 'Mean level' }, 'lvl'),
      h('th', { title: 'Spectral centre of gravity' }, 'brt'),
      h('th', { title: 'Onsets per beat' }, 'dns'),
      h('th', { title: 'Share of the energy below 150 Hz' }, 'bass')));
    r.segments.forEach((s) => {
      table.appendChild(h('tr', {},
        h('td', { class: 'st-type' }, s.type),
        h('td', {}, String(s.bar0 + 1)),
        h('td', {}, String(s.bars)),
        h('td', {}, s.loudness.toFixed(2)),
        h('td', {}, s.brightness.toFixed(2)),
        h('td', {}, s.density.toFixed(2)),
        h('td', {}, s.bass.toFixed(2))));
    });
    body.appendChild(table);

    const snapped = r.segments.filter((s) => s.snapped).length;
    if (snapped) {
      body.appendChild(h('p', { class: 'st-hint' },
        `${snapped} boundar${snapped === 1 ? 'y was' : 'ies were'} a bar off the four-bar grid and ${snapped === 1 ? 'was' : 'were'} pulled onto it (shown in blue).`));
    }

    const apply2 = h('div', { class: 'st-row' },
      button('Use as sections', {
        class: 'primary',
        title: 'Replace the sections with these. One scene per passage type, from the scenes you already have.',
        onClick: () => { apply(ctx); render(ctx); }
      }));
    body.appendChild(apply2);
    body.appendChild(h('p', { class: 'st-hint' },
      'Applying replaces every section. Keyframes belong to section ids and do not survive it.'));
  }

  /* ----------------------------------------------------------------- mounting -- */

  function mount(host, ctx) {
    const body = h('div', { class: 'st-body' });
    state.panel = { host, body };
    host.appendChild(body);
    render(ctx);
    return { refresh: () => render(ctx) };
  }

  FV.StructureUI = {
    mount, overlay, run, apply,
    result: () => state.result,
    clear: () => { state.result = null; state.grid = null; }
  };
})();
