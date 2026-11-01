/* Look tab: the per-scene colour scheme editor.
   - whole-look presets (palette + sky + light + grade)
   - gradient editor (up to 8 stops: drag, click to add, Del to remove), hover-preview palette library,
     harmony generator in OKLCH, non-destructive hue/chroma/lightness nudges
   - mapping (colour source, interpolation, wrap, scale, offset, cycle)
   - environment, light and grade */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const { h, slider, select, color, toggle, group, button } = FV.ui;
  const Pal = window.FVPalettes;
  const Schema = window.FVSchema;

  let selectedStop = 0;
  let clipboard = null; // { kind: 'palette' | 'look', data }

  function build(host, ctx) {
    host.innerHTML = '';
    const scene = ctx.scene();
    if (!scene) return;
    const look = scene.look;
    const pal = look.palette;
    const num = (key, extra) => {
      const spec = Schema.LOOK_NUM[key];
      return FV.Keys.attach(ctx, `look.${key}`, slider(Object.assign({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: look[key], def: spec.def, unit: spec.unit, hint: spec.hint,
        onInput: (v) => { if (FV.Keys.input(ctx, `look.${key}`, v)) return; look[key] = v; ctx.touch(); },
        onCommit: () => { if (!FV.Keys.commit(ctx, `look.${key}`)) ctx.commit(`Look: ${spec.label}`); }
      }, extra || {})), () => look[key]);
    };
    const col = (key, extra) => color(Object.assign({
      label: Schema.LOOK_COLORS[key], value: look[key],
      onInput: (v) => { look[key] = v; ctx.touch(); },
      onCommit: () => ctx.commit(`Look: ${Schema.LOOK_COLORS[key]}`)
    }, extra || {}));

    // ----- presets -----
    const gPre = group('Look presets', 'look-presets', true, 'palette + light + grade');
    const presetGrid = h('div', { class: 'look-presets' });
    Schema.LOOK_PRESETS.forEach((p) => {
      const stops = Pal.PRESET_BY_ID[p.palette].stops;
      const bg = `linear-gradient(to bottom, ${p.look.bgTop}, ${p.look.bgBottom})`;
      presetGrid.appendChild(h('button', {
        type: 'button', class: 'look-preset', title: `Apply the ${p.name} look to this scene`,
        on: {
          click: () => {
            const keep = { source: pal.source, wrap: pal.wrap, interp: pal.interp };
            scene.look = Schema.applyLookPreset(scene.look, p.id);
            Object.assign(scene.look.palette, keep);
            ctx.commit(`Look preset ${p.name}`);
            ctx.rebuild();
          }
        }
      }, h('div', { class: 'sw', style: { background: `${Pal.css(stops, 'oklab')} top / 100% 60% no-repeat, ${bg}` } }), h('span', { text: p.name })));
    });
    gPre.body.appendChild(presetGrid);
    gPre.body.appendChild(h('div', { class: 'btn-row' },
      button('Copy look', { icon: 'copy', onClick: () => { clipboard = { kind: 'look', data: JSON.parse(JSON.stringify(look)) }; ctx.toast('Look copied.'); } }),
      button('Paste look', {
        icon: 'paste',
        onClick: () => {
          if (!clipboard || clipboard.kind !== 'look') { ctx.toast('Copy a look first.', 'warn'); return; }
          scene.look = Schema.normalizeLook(clipboard.data);
          ctx.commit('Paste look');
          ctx.rebuild();
        }
      }),
      button('Apply to all scenes', {
        title: 'Copy this look to every scene in the project',
        onClick: () => {
          if (!window.confirm('Copy this look (palette, light, grade) to every scene?')) return;
          ctx.project().scenes.forEach((s) => { if (s !== scene) s.look = Schema.normalizeLook(JSON.parse(JSON.stringify(look))); });
          ctx.commit('Apply look to all scenes');
          ctx.rebuild();
        }
      })));
    host.appendChild(gPre.el);

    // ----- palette -----
    const gPal = group('Palette', 'look-palette', true, `${pal.stops.length} stops`);
    const extra = gPal.el.querySelector('.grp-extra');
    const bar = h('div', { class: 'grad-bar', title: 'Click to add a colour stop' });
    const stopsEl = h('div', { class: 'grad-stops' });
    const editor = h('div', { class: 'grad-editor' }, bar, stopsEl);
    const stopPick = h('input', { type: 'color', 'aria-label': 'Stop colour' });
    const stopHex = h('input', { type: 'text', maxlength: '7', spellcheck: false, 'aria-label': 'Stop hex' });
    const stopPos = h('input', { type: 'number', min: '0', max: '1', step: '0.01', class: 'ctl-num', 'aria-label': 'Stop position', title: 'Stop position (0–1)' });
    const stopDel = button('', { class: 'icon-btn', icon: 'trash', title: 'Remove stop (Del)' });
    const stopRow = h('div', { class: 'stop-edit' }, stopPick, stopHex, stopPos, stopDel);

    const sorted = () => pal.stops.slice().sort((a, b) => a.p - b.p);
    function paintBar() {
      bar.style.background = Pal.css(pal.stops, pal.interp);
      extra.textContent = `${pal.stops.length} stops`;
      ctx.paletteChanged();
    }
    function drawStops() {
      stopsEl.innerHTML = '';
      selectedStop = Math.min(selectedStop, pal.stops.length - 1);
      pal.stops.forEach((st, i) => {
        const el = h('button', {
          type: 'button', class: `grad-stop${i === selectedStop ? ' sel' : ''}`,
          title: `${st.c} at ${st.p.toFixed(2)} — drag to move, double-click to pick a colour`,
          style: { left: `${st.p * 100}%`, background: st.c }
        });
        el.addEventListener('pointerdown', (e) => startDrag(e, i));
        el.addEventListener('dblclick', () => stopPick.click());
        stopsEl.appendChild(el);
      });
      const s = pal.stops[selectedStop];
      stopPick.value = s.c;
      stopHex.value = s.c;
      stopPos.value = s.p.toFixed(2);
      stopDel.disabled = pal.stops.length <= 2;
    }
    function refreshPalette() { paintBar(); drawStops(); }

    function startDrag(e, i) {
      e.preventDefault();
      selectedStop = i;
      drawStops();
      const rect = bar.getBoundingClientRect();
      const stop = pal.stops[i];
      let moved = false;
      const move = (ev) => {
        moved = true;
        stop.p = Math.min(1, Math.max(0, (ev.clientX - rect.left) / rect.width));
        pal.stops.sort((a, b) => a.p - b.p);
        selectedStop = pal.stops.indexOf(stop);
        refreshPalette();
        ctx.touch();
      };
      const up = () => {
        window.removeEventListener('pointermove', move);
        window.removeEventListener('pointerup', up);
        if (moved) ctx.commit('Move palette stop');
      };
      window.addEventListener('pointermove', move);
      window.addEventListener('pointerup', up);
    }

    bar.addEventListener('click', (e) => {
      if (pal.stops.length >= Pal.MAX_STOPS) { ctx.toast(`A palette has at most ${Pal.MAX_STOPS} stops.`, 'warn'); return; }
      const rect = bar.getBoundingClientRect();
      const p = Math.min(1, Math.max(0, (e.clientX - rect.left) / rect.width));
      const stop = { p, c: Pal.sample(pal.stops, p, pal.interp) };
      pal.stops.push(stop);
      pal.stops.sort((a, b) => a.p - b.p);
      selectedStop = pal.stops.indexOf(stop);
      refreshPalette();
      ctx.commit('Add palette stop');
    });

    const setStopColor = (v, commit) => {
      pal.stops[selectedStop].c = v;
      stopHex.value = v;
      refreshPalette();
      ctx.touch();
      if (commit) ctx.commit('Stop colour');
    };
    stopPick.addEventListener('input', () => setStopColor(stopPick.value, false));
    stopPick.addEventListener('change', () => ctx.commit('Stop colour'));
    stopHex.addEventListener('change', () => {
      if (!/^#?[0-9a-fA-F]{6}$/.test(stopHex.value.trim())) { stopHex.value = pal.stops[selectedStop].c; return; }
      const v = '#' + stopHex.value.trim().replace('#', '').toLowerCase();
      stopPick.value = v;
      setStopColor(v, true);
    });
    stopPos.addEventListener('change', () => {
      const stop = pal.stops[selectedStop];
      stop.p = Math.min(1, Math.max(0, Number(stopPos.value) || 0));
      pal.stops.sort((a, b) => a.p - b.p);
      selectedStop = pal.stops.indexOf(stop);
      refreshPalette();
      ctx.commit('Move palette stop');
    });
    const removeStop = () => {
      if (pal.stops.length <= 2) return;
      pal.stops.splice(selectedStop, 1);
      selectedStop = Math.max(0, selectedStop - 1);
      refreshPalette();
      ctx.commit('Remove palette stop');
    };
    stopDel.addEventListener('click', removeStop);
    ctx.onDeleteKey(removeStop);

    const replaceStops = (stops, label) => {
      pal.stops = Pal.normalizeStops(stops);
      selectedStop = 0;
      refreshPalette();
      ctx.commit(label);
    };

    gPal.body.appendChild(editor);
    gPal.body.appendChild(stopRow);
    gPal.body.appendChild(h('div', { class: 'btn-row' },
      button('', { class: 'icon-btn', icon: 'reverse', title: 'Reverse', onClick: () => replaceStops(Pal.reverse(pal.stops), 'Reverse palette') }),
      button('', { class: 'icon-btn', icon: 'spread', title: 'Spread stops evenly', onClick: () => replaceStops(Pal.spread(pal.stops), 'Spread palette') }),
      button('', { class: 'icon-btn', icon: 'dice', title: 'Random harmonious palette', onClick: () => replaceStops(Pal.random(), 'Random palette') }),
      button('', { class: 'icon-btn', icon: 'copy', title: 'Copy palette', onClick: () => { clipboard = { kind: 'palette', data: sorted() }; ctx.toast('Palette copied.'); } }),
      button('', {
        class: 'icon-btn', icon: 'paste', title: 'Paste palette',
        onClick: () => {
          if (!clipboard) { ctx.toast('Copy a palette first.', 'warn'); return; }
          replaceStops(clipboard.kind === 'look' ? clipboard.data.palette.stops : clipboard.data, 'Paste palette');
        }
      })));

    // nudges: relative to the palette at drag start, reset to neutral on release
    gPal.body.appendChild(h('div', { class: 'sub-label', text: 'Adjust all stops' }));
    let nudgeBase = null;
    const nudge = { hue: 0, chroma: 1, light: 0 };
    const applyNudge = () => {
      if (!nudgeBase) nudgeBase = pal.stops.map((s) => ({ ...s }));
      pal.stops = Pal.adjust(nudgeBase, nudge);
      refreshPalette();
      ctx.touch();
    };
    const endNudge = (s, neutral) => {
      nudgeBase = null;
      Object.assign(nudge, { hue: 0, chroma: 1, light: 0 });
      s.set(neutral);
      ctx.commit('Adjust palette');
    };
    const sHue = slider({ label: 'Hue', min: -180, max: 180, step: 1, value: 0, unit: '°', bipolar: true, hint: 'Rotate every stop around the hue circle (OKLCH).', onInput: (v) => { nudge.hue = v; applyNudge(); }, onCommit: () => endNudge(sHue, 0) });
    const sChroma = slider({ label: 'Colourfulness', min: 0, max: 2, step: 0.01, value: 1, hint: 'Scale chroma of every stop.', onInput: (v) => { nudge.chroma = v; applyNudge(); }, onCommit: () => endNudge(sChroma, 1) });
    const sLight = slider({ label: 'Lightness', min: -0.4, max: 0.4, step: 0.005, value: 0, bipolar: true, onInput: (v) => { nudge.light = v; applyNudge(); }, onCommit: () => endNudge(sLight, 0) });
    [sHue, sChroma, sLight].forEach((s) => gPal.body.appendChild(s.el));

    // mapping
    gPal.body.appendChild(h('div', { class: 'sub-label', text: 'Mapping onto the fractal' }));
    gPal.body.appendChild(select({
      label: 'Colour source', value: pal.source, hint: 'Which property of the surface picks the palette position.',
      options: Schema.COLOR_SOURCES.map((c) => ({ value: c.id, label: c.label })),
      onChange: (v) => { pal.source = v; ctx.commit('Colour source'); }
    }).el);
    gPal.body.appendChild(select({
      label: 'Blend', value: pal.interp,
      options: Schema.INTERPS.map((c) => ({ value: c.id, label: c.label })),
      onChange: (v) => { pal.interp = v; refreshPalette(); ctx.commit('Palette blend'); }
    }).el);
    gPal.body.appendChild(select({
      label: 'Wrap', value: pal.wrap,
      options: Schema.WRAPS.map((c) => ({ value: c.id, label: c.label })),
      onChange: (v) => { pal.wrap = v; ctx.commit('Palette wrap'); }
    }).el);
    gPal.body.appendChild(num('palScale', { log: true }).el);
    gPal.body.appendChild(num('palOffset').el);
    gPal.body.appendChild(num('palCycle', { bipolar: true }).el);
    host.appendChild(gPal.el);
    refreshPalette();

    // ----- library -----
    const gLib = group('Palette library', 'look-library', true, 'hover to preview');
    let lastGroup = null;
    let grid = null;
    let hoverTimer = null;
    Pal.PRESETS.forEach((p) => {
      if (p.group !== lastGroup) {
        gLib.body.appendChild(h('div', { class: 'pal-lib-group', text: p.group }));
        grid = h('div', { class: 'pal-lib' });
        gLib.body.appendChild(grid);
        lastGroup = p.group;
      }
      grid.appendChild(h('button', {
        type: 'button', class: 'pal-swatch', title: `Use ${p.name}`,
        on: {
          mouseenter: () => {
            clearTimeout(hoverTimer);
            hoverTimer = setTimeout(() => ctx.previewStops(p.stops), 140);
          },
          mouseleave: () => { clearTimeout(hoverTimer); ctx.previewStops(null); },
          click: () => { clearTimeout(hoverTimer); ctx.previewStops(null); replaceStops(p.stops.map((s) => ({ ...s })), `Palette ${p.name}`); }
        }
      }, h('div', { class: 'sw', style: { background: Pal.css(p.stops, 'oklab') } }), h('span', { text: p.name })));
    });
    host.appendChild(gLib.el);

    // ----- generator -----
    const gGen = group('Harmony generator', 'look-harmony', false);
    const base = h('input', { type: 'color', value: pal.stops[Math.floor(pal.stops.length / 2)].c, title: 'Base colour' });
    const rule = h('select', null, ...Object.keys(Pal.HARMONIES).map((k) => h('option', { value: k, text: Pal.HARMONIES[k].label })));
    const count = h('input', { type: 'number', min: '2', max: '8', step: '1', value: '6', class: 'ctl-num', title: 'Number of stops' });
    const gen = () => replaceStops(Pal.harmony(base.value, rule.value, Number(count.value) || 6), `Harmony palette (${Pal.HARMONIES[rule.value].label})`);
    gGen.body.appendChild(h('div', { class: 'harmony-row' }, base, rule, count, button('Generate', { icon: 'wand', onClick: gen })));
    gGen.body.appendChild(h('p', { class: 'note', text: 'Builds a dark-to-light ramp through the harmony hues of the base colour, in a perceptual colour space so the steps look even.' }));
    host.appendChild(gGen.el);

    // ----- environment -----
    const gEnv = group('Sky, fog and glow', 'look-env', true);
    const fogCol = col('fogColor');
    fogCol.setDisabled(look.fogMatch);
    gEnv.body.appendChild(col('bgTop').el);
    gEnv.body.appendChild(col('bgBottom').el);
    gEnv.body.appendChild(h('div', { class: 'btn-row' },
      button('Swap', { icon: 'swap', title: 'Swap sky top and bottom', onClick: () => { [look.bgTop, look.bgBottom] = [look.bgBottom, look.bgTop]; ctx.commit('Swap sky'); ctx.rebuild(); } }),
      button('Sky from palette', {
        icon: 'wand', title: 'Derive a dark sky gradient from the palette',
        onClick: () => {
          const s = Pal.normalizeStops(pal.stops);
          const dark = (hex, L) => { const [, C, H] = Pal.hexToOklch(hex); return Pal.oklchToHex([L, C * 0.6, H]); };
          look.bgTop = dark(Pal.sample(s, 0.7, pal.interp), 0.24);
          look.bgBottom = dark(s[0].c, 0.08);
          ctx.commit('Sky from palette');
          ctx.rebuild();
        }
      })));
    gEnv.body.appendChild(toggle({
      label: 'Fog = sky', value: look.fogMatch, hint: 'Fog fades into the sky colour behind each pixel.',
      onChange: (v) => { look.fogMatch = v; fogCol.setDisabled(v); ctx.commit('Fog colour mode'); }
    }).el);
    gEnv.body.appendChild(fogCol.el);
    gEnv.body.appendChild(num('fog', { log: false }).el);
    gEnv.body.appendChild(col('glowColor').el);
    gEnv.body.appendChild(num('glow').el);
    host.appendChild(gEnv.el);

    // ----- backdrop -----
    host.appendChild(buildBackdrop(scene, ctx));

    // ----- light -----
    const gLight = group('Light', 'look-light', false);
    gLight.body.appendChild(col('sunColor').el);
    ['sun', 'sunAz', 'sunEl'].forEach((k) => gLight.body.appendChild(num(k).el));
    gLight.body.appendChild(col('fillColor').el);
    ['fill', 'ambient', 'specular', 'ao'].forEach((k) => gLight.body.appendChild(num(k).el));
    gLight.body.appendChild(toggle({ label: 'Soft shadows', value: look.shadows, onChange: (v) => { look.shadows = v; ctx.commit('Shadows'); } }).el);
    host.appendChild(gLight.el);

    // ----- grade -----
    const gGrade = group('Grade', 'look-grade', false, 'instant, no re-render');
    gGrade.body.appendChild(num('exposure', { bipolar: true }).el);
    ['contrast', 'saturation'].forEach((k) => gGrade.body.appendChild(num(k).el));
    gGrade.body.appendChild(num('hue', { bipolar: true }).el);
    gGrade.body.appendChild(num('vignette').el);
    gGrade.body.appendChild(num('grain').el);
    gGrade.body.appendChild(select({
      label: 'Tone map', value: look.tonemap,
      options: Schema.TONEMAPS.map((c) => ({ value: c.id, label: c.label })),
      onChange: (v) => { look.tonemap = v; ctx.commit('Tone map'); }
    }).el);
    host.appendChild(gGrade.el);
  }

  // Picture or movie behind the fractal, or in front of it as an overlay (per scene).
  function buildBackdrop(scene, ctx) {
    const bd = scene.backdrop;
    const has = bd.kind !== 'none';
    const front = bd.place === 'front';
    const g = group('Backdrop / overlay', 'look-backdrop', true,
      has ? `${bd.kind === 'video' ? 'movie' : 'picture'} ${front ? 'in front' : 'behind'}` : 'picture or movie');
    if (!has) {
      g.body.appendChild(h('p', { class: 'note', text: 'Put a picture or a movie behind the fractal, or in front of it as an overlay (smoke, dust, film scratches, a frame). It is pinned to the output frame. You can also drop a file anywhere on the window.' }));
      g.body.appendChild(h('div', { class: 'btn-row' }, button('Picture or movie…', { icon: 'plus', class: 'btn btn-sm primary', onClick: () => ctx.pickBackdrop(scene) })));
      return g.el;
    }

    const status = ctx.backdrops.status(bd.key);
    const thumb = h('canvas', { class: 'bd-thumb', width: 128, height: 72 });
    if (status === 'ready') ctx.backdrops.drawThumb(bd.key, thumb);
    const statusText = {
      loading: 'Loading…',
      missing: 'File not in this browser — load it again.',
      error: 'Cannot be shown — load another file.'
    }[status] || (bd.kind === 'video' ? `Movie · ${bd.duration ? `${bd.duration.toFixed(1)} s, loops` : 'length unknown'}` : 'Picture');
    g.body.appendChild(h('div', { class: 'bd-head' },
      thumb,
      h('div', { class: 'bd-info' },
        h('b', { text: bd.name || 'backdrop', title: bd.name }),
        h('small', { class: status === 'missing' || status === 'error' ? 'err' : '', text: statusText }),
        h('div', { class: 'btn-row' },
          button('Replace…', { onClick: () => ctx.pickBackdrop(scene) }),
          button('', { class: 'icon-btn', icon: 'trash', title: 'Remove the backdrop from this scene', onClick: () => ctx.removeBackdrop(scene) })))));

    const num = (key, extra) => {
      const spec = Schema.BACKDROP[key];
      return slider(Object.assign({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: bd[key], def: spec.def, unit: spec.unit, hint: spec.hint,
        onInput: (v) => { bd[key] = v; ctx.touch(); },
        onCommit: () => ctx.commit(`Backdrop ${spec.label.toLowerCase()}`)
      }, extra || {})).el;
    };
    g.body.appendChild(select({
      label: 'Placement', value: bd.place, options: Schema.BACKDROP_PLACES.map((x) => ({ value: x.id, label: x.label })),
      onChange: (v) => { bd.place = v; ctx.commit(v === 'front' ? 'Overlay in front' : 'Backdrop behind'); ctx.rebuild(); }
    }).el);
    if (front) {
      g.body.appendChild(select({
        label: 'Blend', value: bd.blend, hint: 'Screen: footage on black (smoke, sparks, light leaks). Multiply: grime or scratches on white. Normal: uses the file\'s transparency (PNG, WebM).',
        options: Schema.BACKDROP_BLENDS.map((x) => ({ value: x.id, label: x.label })),
        onChange: (v) => { bd.blend = v; ctx.commit('Overlay blend'); }
      }).el);
    }
    g.body.appendChild(select({
      label: 'Fit', value: bd.fit, options: Schema.BACKDROP_FITS.map((x) => ({ value: x.id, label: x.label })),
      onChange: (v) => { bd.fit = v; ctx.commit('Backdrop fit'); }
    }).el);
    g.body.appendChild(num('opacity'));
    g.body.appendChild(num('brightness', { bipolar: true }));
    g.body.appendChild(num('saturation'));
    g.body.appendChild(num('blur'));
    g.body.appendChild(num('zoom', { log: true }));
    g.body.appendChild(num('panX', { bipolar: true, hint: 'Moves the crop; needs zoom or a picture wider than the frame.' }));
    g.body.appendChild(num('panY', { bipolar: true, hint: 'Moves the crop; needs zoom or a picture taller than the frame.' }));
    if (!front) g.body.appendChild(num('fogFade'));
    if (bd.kind === 'video') {
      g.body.appendChild(select({
        label: 'Timing', value: bd.sync, options: Schema.BACKDROP_SYNC.map((x) => ({ value: x.id, label: x.label })),
        onChange: (v) => { bd.sync = v; ctx.commit('Backdrop timing'); }
      }).el);
      g.body.appendChild(num('speed', { log: true }));
    }
    g.body.appendChild(h('div', { class: 'btn-row' },
      front ? null : button('Sky from backdrop', {
        icon: 'wand', title: 'Take the sky colours (which also light the fractal) from the top and bottom of the backdrop',
        onClick: () => {
          const c = ctx.backdrops.sampleColors(bd.key);
          if (!c) { ctx.toast('The backdrop is not loaded yet.', 'warn'); return; }
          scene.look.bgTop = c.top;
          scene.look.bgBottom = c.bottom;
          ctx.commit('Sky from backdrop');
          ctx.rebuild();
        }
      }),
      button('Use in all scenes', { title: 'Give every scene this backdrop and these settings', onClick: () => ctx.backdropToAll(scene) })));
    g.body.appendChild(h('p', { class: 'note', text: 'Key backdrop brightness, zoom or blur to the music in the Modulation tab (destinations "Backdrop …"). A slow wave on zoom gives a slow push-in.' }));
    return g.el;
  }

  FV.LookTab = { build };
})();
