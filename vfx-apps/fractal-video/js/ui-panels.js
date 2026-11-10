/* Panels: scene list, audio pane, inspector header, Shape / Modulation / Render tabs. */
(function () {
  'use strict';
  const FV = (window.FV = window.FV || {});
  const { h, slider, select, toggle, group, button, setIcon, fmtTime } = FV.ui;
  const Formulas = window.FVFormulas;
  const Pal = window.FVPalettes;
  const Schema = window.FVSchema;
  const Ev = window.FVEvaluator;

  const ROLE_COLORS = {
    KICK: '#ff6b6b', SNARE: '#ffa94d', HATS: '#ffd43b', PERC: '#e8a87c', BASS: '#845ef7',
    LEAD: '#4dabf7', VOCAL: '#f783ac', PADS: '#63e6be', FX: '#a9e34b', OTHER: '#adb5bd'
  };

  // ---------- scene list ----------
  function buildSceneList(host, ctx) {
    host.innerHTML = '';
    const project = ctx.project();
    const selId = ctx.scene() ? ctx.scene().id : null;
    const liveId = ctx.liveSceneId();
    project.scenes.forEach((scene, i) => {
      const used = project.sections.filter((s) => s.sceneId === scene.id).length;
      const thumb = ctx.thumbs[scene.id];
      const thumbEl = h('div', { class: 'scene-thumb', style: thumb ? { backgroundImage: `url(${thumb})` } : { background: Pal.css(scene.look.palette.stops, scene.look.palette.interp) } },
        i < 9 ? h('span', { class: 'idx', text: String(i + 1) }) : null);
      const nameEl = h('div', { class: 'scene-name', text: scene.name, title: 'Double-click to rename' });
      const meta = h('div', { class: 'scene-meta' },
        nameEl,
        h('div', { class: 'scene-sub', text: `${Formulas.get(scene.formula).name} · ${used ? `${used} section${used > 1 ? 's' : ''}` : 'unused'}` }),
        h('div', { class: 'scene-pal', style: { background: Pal.css(scene.look.palette.stops, scene.look.palette.interp) } }));
      const tools = h('div', { class: 'scene-tools' },
        button('', { class: 'icon-btn sm', icon: 'copy', title: 'Duplicate', onClick: (e) => { e.stopPropagation(); ctx.duplicateScene(scene.id); } }),
        button('', { class: 'icon-btn sm', icon: 'trash', title: 'Delete', onClick: (e) => { e.stopPropagation(); ctx.deleteScene(scene.id); } }));
      const card = h('div', {
        class: `scene-card${scene.id === selId ? ' sel' : ''}${scene.id === liveId ? ' live' : ''}`,
        draggable: 'true', role: 'button', tabindex: '0',
        title: 'Click to edit. Drag onto a timeline section to assign it.',
        on: {
          click: () => ctx.selectScene(scene.id),
          keydown: (e) => { if (e.key === 'Enter') ctx.selectScene(scene.id); },
          dragstart: (e) => { e.dataTransfer.setData('text/fv-scene', scene.id); e.dataTransfer.effectAllowed = 'copy'; }
        }
      }, thumbEl, meta, tools);
      nameEl.addEventListener('dblclick', (e) => {
        e.stopPropagation();
        const input = h('input', { type: 'text', class: 'scene-name-input', value: scene.name });
        nameEl.replaceWith(input);
        input.focus();
        input.select();
        const done = (save) => {
          if (save && input.value.trim()) { scene.name = input.value.trim().slice(0, 80); ctx.commit('Rename scene'); }
          ctx.rebuild('scenes', 'inspector', 'timeline');
        };
        input.addEventListener('keydown', (ev) => { if (ev.key === 'Enter') done(true); if (ev.key === 'Escape') done(false); });
        input.addEventListener('blur', () => done(true));
        input.addEventListener('click', (ev) => ev.stopPropagation());
      });
      host.appendChild(card);
    });
  }

  // ---------- inspector header (scene name for Shape / Look) ----------
  function buildInspectorHead(host, ctx, tab) {
    host.innerHTML = '';
    const scene = ctx.scene();
    if (!scene || (tab !== 'shape' && tab !== 'look')) return;
    const used = ctx.project().sections.filter((s) => s.sceneId === scene.id).length;
    const name = h('input', { type: 'text', class: 'scene-name-edit', value: scene.name, title: 'Scene name', spellcheck: false });
    name.addEventListener('change', () => {
      scene.name = name.value.trim().slice(0, 80) || scene.name;
      ctx.commit('Rename scene');
      ctx.rebuild('scenes', 'timeline');
    });
    host.appendChild(h('div', { class: 'pal-chip', id: 'inspectorChip', style: { background: Pal.css(scene.look.palette.stops, scene.look.palette.interp) } }));
    host.appendChild(name);
    host.appendChild(h('span', { class: 'note', text: used ? `in ${used} section${used > 1 ? 's' : ''}` : 'not on timeline' }));
  }

  // ---------- audio pane ----------
  // Microphone: the master sources follow what it hears (a headset's mic with music in the room is enough).
  function micBlock(ctx) {
    const on = ctx.micActive();
    const wrap = h('div', { class: 'mic-block' });
    wrap.appendChild(h('div', { class: 'sub-label', text: 'Microphone (live)' }));
    const meter = h('div', { class: 'mic-meter', title: 'Level the microphone hears' }, h('div', { id: 'micMeter', class: 'mic-meter-fill' }));
    wrap.appendChild(h('div', { class: 'btn-row' },
      button(on ? 'Stop listening' : (ctx.micStarting() ? 'Starting…' : 'Listen with microphone'), {
        icon: 'music', class: `btn btn-sm${on ? ' primary' : ''}`,
        title: 'Drive the master sources (level, lows, mids, highs, onsets) from the microphone instead of the master file',
        onClick: () => ctx.toggleMic()
      }),
      ...(on ? [meter] : [])));
    const err = ctx.micError();
    if (err && !on) wrap.appendChild(h('p', { class: 'note err', text: err }));
    else wrap.appendChild(h('p', { class: 'note', text: on
      ? 'Listening: routes from the master sources follow the room. Stems and the beat grid still come from the project. Movies always use the files.'
      : 'No audio file needed: play music in the room and the master routes follow it. The browser asks for permission the first time.' }));
    return wrap;
  }

  function buildAudioPane(host, ctx) {
    host.innerHTML = '';
    const project = ctx.project();
    const st = ctx.audioState();

    host.appendChild(micBlock(ctx));
    host.appendChild(h('div', { class: 'sub-label', text: 'Master (soundtrack)' }));
    if (project.master) {
      const m = st.master || {};
      let sub = 'missing — load it again';
      let cls = 'warn';
      if (m.status === 'ready') { sub = `${fmtTime(m.duration || 0)} · analysed`; cls = 'ok'; }
      else if (m.status === 'busy') { sub = `${m.stage || 'analysing'} ${Math.round((m.progress || 0) * 100)}%`; cls = ''; }
      else if (m.status === 'error') { sub = m.error || 'could not read file'; }
      host.appendChild(h('div', { class: 'file-row' },
        h('div', { style: { minWidth: 0 } }, h('div', { class: 'file-name', text: project.master.name, title: project.master.name }), h('div', { class: `file-sub ${cls}`, text: sub })),
        h('div', { class: 'file-actions' },
          button('', { class: 'icon-btn sm', icon: 'music', title: 'Replace master', onClick: () => ctx.pickMaster() }),
          button('', { class: 'icon-btn sm', icon: 'trash', title: 'Remove master', onClick: () => ctx.removeMaster() }))));
    } else {
      host.appendChild(h('div', { class: 'drop-hint' }, 'Drop audio files here', h('br'), button('Load master…', { icon: 'music', onClick: () => ctx.pickMaster() })));
    }

    host.appendChild(h('div', { class: 'sub-label', text: 'Tempo grid' }));
    const field = (label, key, step, min, max, title) => {
      const input = h('input', { type: 'number', step: String(step), min: String(min), max: String(max), value: String(project[key]), title });
      input.addEventListener('change', () => {
        const v = Number(input.value);
        if (!Number.isFinite(v)) { input.value = project[key]; return; }
        project[key] = Math.min(max, Math.max(min, key === 'beatsPerBar' ? Math.round(v) : v));
        input.value = project[key];
        ctx.commit(`Tempo: ${label}`);
        ctx.rebuild('timeline', 'mod', 'render');
      });
      return h('label', null, label, input);
    };
    host.appendChild(h('div', { class: 'tempo-grid' },
      field('BPM', 'bpm', 0.01, 20, 400, 'Tempo in beats per minute'),
      field('Bar 1 at s', 'offset', 0.001, -60, 600, 'Time (seconds) of bar 1, beat 1 in the master file'),
      field('Beats/bar', 'beatsPerBar', 1, 1, 16, 'Time signature numerator')));
    host.appendChild(h('div', { class: 'btn-row' },
      button('Suggest from master', { title: 'Estimate BPM and downbeat from the master (only a suggestion)', onClick: () => ctx.detectTempo() }),
      button('Tap', { title: 'Tap along to the beat a few times', onClick: () => ctx.tapTempo() })));

    const stemHead = h('div', { class: 'sub-label', style: { display: 'flex', alignItems: 'center' } }, 'Stems (modulation sources)');
    host.appendChild(stemHead);
    project.stems.forEach((stem) => {
      const s = st.stems[stem.id] || {};
      let sub = 'missing — load it again';
      let cls = 'warn';
      if (s.status === 'ready') {
        cls = '';
        sub = 'analysed';
        if (s.align) {
          if (s.align.silent) { sub = 'silent file?'; cls = 'warn'; }
          else if (Math.abs(s.align.offset) >= 0.015) { sub = `offset ${(s.align.offset * 1000).toFixed(0)} ms vs master`; cls = 'warn'; }
          else { sub = 'aligned with master'; cls = 'ok'; }
        }
      } else if (s.status === 'busy') { sub = `${s.stage || 'analysing'} ${Math.round((s.progress || 0) * 100)}%`; cls = ''; }
      else if (s.status === 'error') sub = s.error || 'could not read file';
      const role = h('select', { title: 'Role: picks suggested routes' }, ...Schema.ROLES.map((r) => h('option', { value: r, text: r })));
      role.value = stem.role;
      role.addEventListener('change', () => { stem.role = role.value; ctx.commit('Stem role'); ctx.rebuild('audio', 'timeline'); });
      const solo = button('', { class: `icon-btn sm${ctx.soloId() === stem.id ? ' on' : ''}`, icon: 'headphones', title: 'Solo: hear this stem instead of the master', onClick: () => ctx.solo(stem.id) });
      host.appendChild(h('div', { class: 'file-row stem-row' },
        h('span', { class: 'stem-dot', style: { background: ROLE_COLORS[stem.role] } }),
        h('div', { style: { minWidth: 0 } }, h('div', { class: 'file-name', text: stem.name, title: stem.name }), h('div', { class: `file-sub ${cls}`, text: sub })),
        role,
        h('div', { class: 'file-actions' }, solo, button('', { class: 'icon-btn sm', icon: 'trash', title: 'Remove stem', onClick: () => ctx.removeStem(stem.id) }))));
    });
    host.appendChild(h('div', { class: 'btn-row' },
      button('Add stems…', { icon: 'plus', onClick: () => ctx.pickStems() }),
      button('Suggest routes', { icon: 'route', title: 'Add modulation routes based on stem roles', onClick: () => ctx.suggestRoutes() })));
    if (!project.stems.length) {
      host.appendChild(h('p', { class: 'note', text: 'Export each instrument from Ableton (same start and length as the master). Each stem becomes its own modulation source, so the kick moves exactly what you key it to.' }));
    }
  }

  // ---------- shape tab ----------
  let camCtl = null;
  let camCtx = null;
  const keyPick = { dest: null, source: null }; // Key to music choices survive tab rebuilds

  function buildShapeTab(host, ctx) {
    host.innerHTML = '';
    camCtl = null;
    const scene = ctx.scene();
    if (!scene) return;
    const f = Formulas.get(scene.formula);
    const project = ctx.project();
    const modded = new Set(project.mod.filter((m) => Schema.routeOn(m, scene.id) && m.depth).map((m) => m.dest));

    const gF = group('Formula', 'shape-formula', true);
    gF.body.appendChild(select({
      label: 'Formula', value: f.id,
      options: Formulas.grouped().map((x) => ({ value: x.formula.id, label: x.formula.name, group: x.group })),
      onChange: (id) => {
        const nf = Formulas.get(id);
        scene.formula = nf.id;
        scene.params = Formulas.defaults(nf.id);
        scene.iter = nf.iter.def;
        scene.space.step = nf.step;
        const orbit = scene.camera.orbit;
        scene.camera = Object.assign(Schema.defaultCamera(nf.id), { orbit });
        ctx.commit(`Formula ${nf.name}`);
        ctx.rebuild('shape', 'scenes', 'mod');
        ctx.precompile();
      }
    }).el);
    gF.body.appendChild(h('p', { class: 'formula-blurb', text: f.blurb }));
    gF.body.appendChild(slider({
      label: 'Iterations', min: f.iter.min, max: f.iter.max, step: 1, value: scene.iter, def: f.iter.def,
      hint: 'More iterations = more detail, slower.',
      onInput: (v) => { scene.iter = v; ctx.touch(); }, onCommit: () => ctx.commit('Iterations')
    }).el);

    let lastGroup = null;
    f.params.forEach((p) => {
      if (p.group !== lastGroup && p.group !== 'Form') gF.body.appendChild(h('div', { class: 'sub-label', text: p.group }));
      lastGroup = p.group;
      const s = slider({
        label: p.label, min: p.min, max: p.max, step: p.step, value: scene.params[p.key], def: p.def, unit: p.unit,
        calm: p.calm, hint: (p.hint ? p.hint + ' ' : '') + (p.calm ? 'Green band: suggested calm range for modulation.' : ''),
        bipolar: p.min < 0 && p.max > 0,
        onInput: (v) => { if (FV.Keys.input(ctx, `shape.${p.key}`, v)) return; scene.params[p.key] = v; ctx.touch(); },
        onCommit: () => { if (!FV.Keys.commit(ctx, `shape.${p.key}`)) ctx.commit(p.label); }
      });
      FV.Keys.attach(ctx, `shape.${p.key}`, s, () => scene.params[p.key]);
      if (modded.has(`shape.${p.key}`)) {
        const lab = s.el.querySelector('.ctl-label');
        lab.classList.add('modded');
        lab.title += '\nKeyed to music (see Modulation).';
      }
      gF.body.appendChild(s.el);
    });
    gF.body.appendChild(h('div', { class: 'btn-row' },
      button('Reset shape', { icon: 'reset', onClick: () => { scene.params = Formulas.defaults(f.id); scene.iter = f.iter.def; ctx.commit('Reset shape'); ctx.rebuild('shape'); } }),
      button('Randomise (calm)', {
        icon: 'dice', title: 'Random values inside the suggested calm ranges',
        onClick: () => {
          f.params.forEach((p) => {
            const [a, b] = p.calm || [p.min, p.max];
            let v = a + Math.random() * (b - a);
            if (p.step >= 1) v = Math.round(v);
            scene.params[p.key] = v;
          });
          ctx.commit('Randomise shape');
          ctx.rebuild('shape');
        }
      })));
    host.appendChild(gF.el);

    // key to music: stem/band/grid -> a parameter of this formula
    const gK = group('Key to music', 'shape-key', true, 'stem → parameter');
    const srcOpts = Schema.sources(project).map((s) => ({ value: s.id, label: s.label, group: s.group }));
    const destOpts = Schema.destinations(f.id).filter((d) => d.id.startsWith('shape.') || d.id.startsWith('space.')).map((d) => ({ value: d.id, label: d.label }));
    const firstStem = srcOpts.find((o) => o.group === 'Stems');
    const paramSel = select({
      label: 'Parameter', options: destOpts,
      value: destOpts.some((o) => o.value === keyPick.dest) ? keyPick.dest : destOpts[0].value,
      onChange: (v) => { keyPick.dest = v; }
    });
    const srcSel = select({
      label: 'Driven by', options: srcOpts,
      value: srcOpts.some((o) => o.value === keyPick.source) ? keyPick.source : (firstStem ? firstStem.value : 'master.low'),
      onChange: (v) => { keyPick.source = v; }
    });
    gK.body.appendChild(paramSel.el);
    gK.body.appendChild(srcSel.el);
    gK.body.appendChild(h('div', { class: 'btn-row' }, button('Add route', {
      icon: 'route', class: 'btn btn-sm primary',
      onClick: () => {
        keyPick.dest = paramSel.select.value;
        keyPick.source = srcSel.select.value;
        const isHit = /\.onset$/.test(keyPick.source);
        project.mod.push({
          id: Schema.uid('mod'), off: [], source: keyPick.source, dest: keyPick.dest,
          depth: 0.08, attack: isHit ? 0 : 0.05, release: isHit ? 0.2 : 0.4, lead: 0
        });
        ctx.commit('Add route');
        ctx.rebuild('mod', 'shape');
        ctx.toast('Route added. Depth, smoothing and look-ahead are in the Modulation tab.', 'ok');
      }
    })));
    // routes already keyed to this formula's parameters (or space), so adding one is visible here
    const destLabel = new Map(destOpts.map((o) => [o.value, o.label]));
    const srcLabel = new Map(srcOpts.map((o) => [o.value, o.label]));
    const keyed = project.mod.filter((m) => destLabel.has(m.dest));
    if (keyed.length) {
      const list = h('div', { class: 'key-routes' });
      keyed.forEach((m) => {
        list.appendChild(h('div', { class: `key-route${Schema.routeOn(m, scene.id) ? '' : ' off'}`, title: 'Edit depth and smoothing in the Modulation tab' },
          h('span', { class: 'kr-text', text: `${destLabel.get(m.dest)} ← ${srcLabel.get(m.source) || m.source}` }),
          h('span', { class: 'kr-depth', text: `${m.depth > 0 ? '+' : ''}${Math.round(m.depth * 100)}%` }),
          button('', {
            class: 'icon-btn', icon: 'trash', title: 'Remove this route',
            onClick: () => { project.mod.splice(project.mod.indexOf(m), 1); ctx.commit('Remove route'); ctx.rebuild('mod', 'shape'); }
          })));
      });
      gK.body.appendChild(list);
    }
    gK.body.appendChild(h('p', { class: 'note', text: project.stems.length
      ? 'Shape changes read best with small depths and some smoothing; stay inside the green bands for calm motion.'
      : 'Load stems in the Audio panel to key individual instruments. Master bands and the beat grid work already.' }));
    host.appendChild(gK.el);

    const gS = group('Space', 'shape-space', false);
    ['repeat', 'twist', 'bound'].forEach((k) => {
      const spec = Schema.SPACE[k];
      gS.body.appendChild(FV.Keys.attach(ctx, `space.${k}`, slider({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: scene.space[k], def: spec.def, hint: spec.hint, bipolar: spec.min < 0,
        onInput: (v) => { if (FV.Keys.input(ctx, `space.${k}`, v)) return; scene.space[k] = v; ctx.touch(); },
        onCommit: () => { if (!FV.Keys.commit(ctx, `space.${k}`)) ctx.commit(spec.label); }
      }), () => scene.space[k]).el);
    });
    host.appendChild(gS.el);

    const gC = group('Camera', 'shape-camera', true);
    camCtl = {};
    camCtx = ctx;
    ['dist', 'yaw', 'pitch', 'roll', 'fov', 'orbit', 'tx', 'ty', 'tz'].forEach((k) => {
      const spec = Schema.CAMERA[k];
      const dest = `cam.${k}`;
      const s = FV.Keys.attach(ctx, dest, slider({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: scene.camera[k],
        def: k === 'dist' || k === 'yaw' || k === 'pitch' ? Schema.defaultCamera(f.id)[k] : spec.def,
        unit: spec.unit, hint: spec.hint, log: k === 'dist', bipolar: spec.min < 0,
        onInput: (v) => { if (FV.Keys.input(ctx, dest, v)) return; scene.camera[k] = v; ctx.touch(); },
        onCommit: () => { if (!FV.Keys.commit(ctx, dest)) ctx.commit(`Camera ${spec.label}`); }
      }), () => scene.camera[k]);
      camCtl[k] = s;
      gC.body.appendChild(s.el);
    });
    const camKeyed = FV.Keys.cameraKeyed(ctx);
    const anyCamLanes = (ctx.project().keys || []).some((x) => x.dest.startsWith('cam.'));
    gC.body.appendChild(h('div', { class: 'btn-row' },
      button('Reset camera (F)', { icon: 'reset', onClick: () => ctx.resetCamera() }),
      button(camKeyed ? 'Key camera here (K)' : 'Keyframe camera (K)', {
        title: camKeyed ? 'Put a key of the whole camera at the playhead (moving the camera does this too).'
          : 'Keyframe the camera in this scene: from then on orbit, pan, zoom and these sliders write keys at the playhead.',
        onClick: () => FV.Keys.keyCamera(ctx)
      })));
    if (anyCamLanes) {
      gC.body.appendChild(h('div', { class: 'btn-row' },
        camKeyed ? button('Delete camera key at playhead', { onClick: () => FV.Keys.deleteCameraKeyHere(ctx) }) : null,
        button('Remove all camera keys', { onClick: () => FV.Keys.removeCameraKeys(ctx) })));
    }
    host.appendChild(gC.el);

    buildFlightGroup(host, ctx, scene);

    const gR = group('Ray march', 'shape-march', false);
    ['detail', 'step'].forEach((k) => {
      const spec = Schema.SPACE[k];
      gR.body.appendChild(FV.Keys.attach(ctx, `space.${k}`, slider({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: scene.space[k],
        def: k === 'step' ? f.step : spec.def, hint: spec.hint, log: k === 'detail',
        onInput: (v) => { if (FV.Keys.input(ctx, `space.${k}`, v)) return; scene.space[k] = v; ctx.touch(); },
        onCommit: () => { if (!FV.Keys.commit(ctx, `space.${k}`)) ctx.commit(spec.label); }
      }), () => scene.space[k]).el);
    });
    host.appendChild(gR.el);
  }

  // ---------- auto-flight ----------
  function buildFlightGroup(host, ctx, scene) {
    const p = ctx.project();
    const fl = p.flight;
    const st = ctx.flightStatus();
    const flownHere = fl.on.includes(scene.id);
    const extra = st.state === 'ready' ? 'ready' : st.state === 'stale' ? 'out of date' : st.state === 'busy' ? 'computing' : flownHere ? 'on' : '';
    const g = group('Auto-flight', 'shape-flight', flownHere, extra);
    g.body.appendChild(h('p', { class: 'note', text: 'A camera that flies over the fractal by itself, skimming the surface without touching it. '
      + 'Switch it on for the scenes to fly in, press Compute flight, then play the timeline. The path is stored with the project, '
      + 'so the movie renders exactly what you preview. In flown sections it replaces the camera (field of view and music routes still act).' }));
    g.body.appendChild(toggle({
      label: 'Fly in this scene', value: flownHere,
      hint: 'Sections showing this scene are flown. Consecutive flown sections make one continuous flight.',
      onChange: (v) => {
        fl.on = fl.on.filter((x) => x !== scene.id);
        if (v) fl.on.push(scene.id);
        ctx.commit('Auto-flight scenes');
        ctx.rebuild('shape');
      }
    }).el);
    Object.keys(Schema.FLIGHT).forEach((k) => {
      const spec = Schema.FLIGHT[k];
      g.body.appendChild(slider({
        label: spec.label, min: spec.min, max: spec.max, step: spec.step, value: fl.settings[k], def: spec.def,
        unit: spec.unit, hint: spec.hint, log: k === 'speed' || k === 'height',
        onInput: (v) => { fl.settings[k] = k === 'seed' ? Math.round(v) : v; },
        onCommit: () => { ctx.commit(`Flight ${spec.label}`); ctx.rebuild('shape'); }
      }).el);
    });
    const status = h('p', { class: 'note flight-status' });
    const say = (t, warn) => { status.textContent = t; status.classList.toggle('warn', !!warn); };
    if (st.state === 'off') say('No scene is flown yet.');
    else if (st.state === 'none') say('No flight computed yet.');
    else if (st.state === 'ready') say(`Flight ready (${st.runs} stretch${st.runs === 1 ? '' : 'es'} of the timeline). Watch it in Timeline view.`);
    else if (st.state === 'stale') say('The shape, timeline or settings changed since the flight was computed: compute it again, or the camera may pass too close to the surface.', true);
    else if (st.state === 'busy') say(`Computing... ${Math.round((st.progress || 0) * 100)} %`);
    const go = button(st.state === 'busy' ? 'Stop' : (st.state === 'ready' || st.state === 'stale') ? 'Compute again' : 'Compute flight', {
      class: 'btn btn-sm primary',
      title: 'Flies the camera through the whole timeline, asking the GPU how far the surface is at every step. Takes a few seconds per minute of song.',
      onClick: () => {
        if (ctx.flightStatus().state === 'busy') { ctx.cancelFlight(); return; }
        go.textContent = 'Stop';
        ctx.computeFlight((x) => say(`Computing... ${Math.round(x * 100)} %`));
      }
    });
    const clear = fl.track ? button('Clear flight', {
      title: 'Remove the computed path; the scenes go back to their own cameras.',
      onClick: () => { fl.track = null; ctx.commit('Clear flight'); ctx.rebuild('shape'); }
    }) : null;
    g.body.appendChild(status);
    g.body.appendChild(h('div', { class: 'btn-row' }, go, clear));
    host.appendChild(g.el);
  }

  function syncCamera(scene) {
    if (!camCtl || !scene) return;
    Object.keys(camCtl).forEach((k) => camCtl[k].set(Schema.CAM_KEYS.includes(k) ? FV.Keys.cameraValue(camCtx, k) : scene.camera[k]));
  }

  // ---------- keyframe lanes (listed in the Modulation tab) ----------
  function buildKeyLanes(pad, ctx, formulaId) {
    const project = ctx.project();
    const lanes = project.keys || [];
    pad.appendChild(h('div', { class: 'sub-label', text: `Keyframes (${lanes.filter((l) => !l.dest.startsWith('cam.')).length + (lanes.some((l) => l.dest.startsWith('cam.')) ? 1 : 0)})` }));
    if (!lanes.length) {
      pad.appendChild(h('p', { class: 'note', text: 'Right-click a slider in Shape or Look to keyframe it, or press K to keyframe the camera. Then each move sets a key at the playhead.' }));
    }
    const list = h('div', { class: 'mod-list' });
    const camLanes = lanes.filter((l) => l.dest.startsWith('cam.'));
    if (camLanes.length) {
      // The camera is one entry: its lanes share scenes and interpolation.
      const scope = FV.Keys.sceneScope(ctx, camLanes[0], { noun: 'keyframe lane', lanes: camLanes, parts: ['mod', 'shape', 'look', 'timeline'] });
      const el = h('div', { class: `mod-row key-row${scope.isOn(scope.sid) ? '' : ' off'}` });
      const interp = select({
        label: null, value: camLanes[0].interp, options: Schema.KEY_INTERPS.map((x) => ({ value: x.id, label: x.label })),
        onChange: (v) => { camLanes.forEach((l) => { l.interp = v; }); ctx.commit('Keyframe interpolation'); ctx.rebuild('timeline'); }
      });
      const del = button('', { class: 'icon-btn sm', icon: 'trash', title: 'Remove all camera keys', onClick: () => FV.Keys.removeCameraKeys(ctx) });
      const poses = new Set();
      camLanes.forEach((l) => l.pts.forEach((q) => poses.add(`${q.sec}|${Math.round(q.b * 64)}`)));
      el.appendChild(h('div', { class: 'key-top' },
        h('span', { class: 'key-name', text: '◆ Camera', title: camLanes.map((l) => l.dest).join(', ') }),
        h('span', { class: 'key-count', text: `${poses.size} key${poses.size === 1 ? '' : 's'}` }),
        interp.el, scope.chip, del));
      el.appendChild(scope.strip);
      list.appendChild(el);
    }
    lanes.filter((l) => !l.dest.startsWith('cam.')).forEach((lane) => {
      const spec = Schema.keySpec(lane.dest, formulaId) || Schema.keySpec(lane.dest);
      const otherFormula = lane.dest.startsWith('shape.') && !Schema.keySpec(lane.dest, formulaId);
      const scope = FV.Keys.sceneScope(ctx, lane, { noun: 'keyframe lane', parts: ['mod', 'shape', 'look', 'timeline'] });
      const el = h('div', { class: `mod-row key-row${Schema.routeOn(lane, scope.sid) ? '' : ' off'}` });
      const interp = select({
        label: null, value: lane.interp, options: Schema.KEY_INTERPS.map((x) => ({ value: x.id, label: x.label })),
        onChange: (v) => { lane.interp = v; ctx.commit('Keyframe interpolation'); ctx.rebuild('timeline'); }
      });
      const del = button('', {
        class: 'icon-btn sm', icon: 'trash', title: 'Remove all keys of this parameter',
        onClick: () => { project.keys.splice(project.keys.indexOf(lane), 1); ctx.commit('Remove keyframes'); ctx.rebuild('mod', 'shape', 'look', 'timeline'); }
      });
      el.appendChild(h('div', { class: 'key-top' },
        h('span', { class: 'key-name', text: `◆ ${spec ? spec.label : lane.dest}`, title: otherFormula ? 'A parameter of another formula: acts only in scenes using it.' : lane.dest }),
        h('span', { class: 'key-count', text: `${lane.pts.length} key${lane.pts.length === 1 ? '' : 's'}` }),
        interp.el, scope.chip, del));
      el.appendChild(scope.strip);
      list.appendChild(el);
    });
    pad.appendChild(list);
    pad.appendChild(h('div', { class: 'sub-label', text: 'Routes' }));
  }

  // ---------- modulation tab ----------
  let modRows = [];

  function buildModTab(host, ctx) {
    host.innerHTML = '';
    modRows = [];
    const project = ctx.project();
    const scene = ctx.scene();
    const formulaId = scene ? scene.formula : 'mandelbulb';
    const pad = h('div', { class: 'tab-pad' });
    host.appendChild(pad);
    pad.appendChild(h('div', { class: 'btn-row' },
      button('Add route', {
        icon: 'plus', class: 'btn btn-sm primary',
        onClick: () => {
          project.mod.push({ id: Schema.uid('mod'), off: [], source: 'grid.beatPulse', dest: 'look.glow', depth: 0.15, attack: 0, release: 0, lead: 0 });
          ctx.commit('Add route');
          ctx.rebuild('mod');
        }
      }),
      button('Suggest from stems', { icon: 'route', onClick: () => ctx.suggestRoutes() })));
    pad.appendChild(h('p', { class: 'note', text: 'Each route adds source × depth to a destination, on top of the scene. The switch turns a route on or off in the selected scene; the scene chip sets it for several scenes at once. Shape routes only act in scenes whose formula has that parameter. Click a graph to set smoothing and look-ahead.' }));
    buildKeyLanes(pad, ctx, formulaId);
    if (!project.mod.length) pad.appendChild(h('p', { class: 'note', text: 'No routes yet.' }));

    const list = h('div', { class: 'mod-list' });
    pad.appendChild(list);
    const srcOpts = Schema.sources(project).map((s) => ({ value: s.id, label: s.label, group: s.group }));
    const destList = Schema.destinations(formulaId);
    const destOpts = destList.map((d) => ({ value: d.id, label: d.label, group: d.group }));
    const analysis = ctx.analysis();
    const duration = Math.max(1, Ev.songDuration(project, analysis));

    project.mod.forEach((row) => {
      const inScene = !row.dest.startsWith('shape.') || destList.some((d) => d.id === row.dest);
      const sid = scene ? scene.id : null;
      const el = h('div', { class: `mod-row${Schema.routeOn(row, sid) ? '' : ' off'}${inScene ? '' : ' inactive'}` });
      const on = h('input', { type: 'checkbox', title: `On in "${scene ? scene.name : ''}" (the selected scene)` });
      on.checked = Schema.routeOn(row, sid);
      const scope = FV.Keys.sceneScope(ctx, row, { noun: 'route' });
      const setOn = scope.setOn;
      const chip = scope.chip;
      const strip = scope.strip;
      on.addEventListener('change', () => { setOn(sid, on.checked); ctx.commit('Toggle route'); ctx.rebuild('mod', 'shape'); });
      const missingDest = Schema.destSpec(row.dest);
      const src = select({ label: null, value: row.source, options: srcOpts, missingLabel: 'missing stem', onChange: (v) => { row.source = v; ctx.commit('Route source'); ctx.rebuild('mod'); } });
      const dst = select({
        label: null, value: row.dest, options: destOpts,
        missingLabel: missingDest ? `${missingDest.label} (other formula)` : row.dest,
        onChange: (v) => { row.dest = v; ctx.commit('Route destination'); ctx.rebuild('mod', 'shape'); }
      });
      dst.select.classList.add('mod-dest');
      const expand = button('', { class: 'icon-btn sm', icon: 'wand', title: 'Smoothing and look-ahead' });
      const del = button('', { class: 'icon-btn sm', icon: 'trash', title: 'Delete route', onClick: () => { project.mod.splice(project.mod.indexOf(row), 1); ctx.commit('Delete route'); ctx.rebuild('mod', 'shape'); } });
      el.appendChild(h('div', { class: 'mod-top' }, on, src.el, h('span', { class: 'mod-arrow', text: '→' }), dst.el, chip, expand, del));
      el.appendChild(strip);

      const depth = slider({
        label: 'Depth', min: -1, max: 1, step: 0.005, value: row.depth, def: 0, bipolar: true,
        hint: 'Fraction of the destination range added at full signal. Negative inverts.',
        onInput: (v) => { row.depth = v; ctx.touch(); }, onCommit: () => { ctx.commit('Route depth'); ctx.rebuild('shape'); }
      });
      el.appendChild(depth.el);

      const cv = h('canvas');
      const meter = h('span');
      const head = h('div', { style: { position: 'absolute', top: '0', bottom: '0', width: '1px', background: '#ff5f7e' } });
      const spark = h('div', { class: 'mod-spark', title: 'Signal over the whole song (after smoothing). Click for details.' }, cv, head, h('div', { class: 'mod-meter' }, meter));
      el.appendChild(spark);

      const details = h('div', { class: 'mod-detail' });
      details.hidden = !ctx.expanded.has(row.id);
      const grid = Schema.isGridSource(row.source);
      const mk = (key, label, max, unit, hint) => slider({
        label, min: key === 'lead' ? -8 : 0, max, step: key === 'lead' ? 0.25 : 0.005, value: row[key], def: 0, unit, hint,
        bipolar: key === 'lead',
        onInput: (v) => { row[key] = v; ctx.touch(); drawSpark(); }, onCommit: () => ctx.commit(`Route ${label}`)
      });
      const att = mk('attack', 'Attack', 2, 's', 'Rise time of the follower.');
      const rel = mk('release', 'Release', 4, 's', 'Fall time of the follower.');
      att.setDisabled(grid);
      rel.setDisabled(grid);
      details.appendChild(att.el);
      details.appendChild(rel.el);
      details.appendChild(mk('lead', 'Look-ahead', 8, 'beats', 'Read the source this many beats early, so motion arrives on the hit.').el);
      el.appendChild(details);
      const toggleDetails = () => {
        details.hidden = !details.hidden;
        if (details.hidden) ctx.expanded.delete(row.id); else ctx.expanded.add(row.id);
      };
      expand.addEventListener('click', toggleDetails);
      spark.addEventListener('click', toggleDetails);
      list.appendChild(el);

      function drawSpark() {
        const w = Math.max(60, spark.clientWidth || 300);
        const hh = 26;
        cv.width = w;
        cv.height = hh;
        const g = cv.getContext('2d');
        g.clearRect(0, 0, w, hh);
        g.fillStyle = 'rgba(108,196,255,0.22)';
        g.strokeStyle = '#6cc4ff';
        g.beginPath();
        g.moveTo(0, hh);
        for (let x = 0; x < w; x += 1) {
          const v = Ev.rowSignal(project, analysis, row, (x / w) * duration);
          g.lineTo(x, hh - 2 - v * (hh - 4));
        }
        g.lineTo(w, hh);
        g.closePath();
        g.fill();
      }
      modRows.push({ row, drawSpark, meter, head, duration });
    });
    requestAnimationFrame(() => modRows.forEach((r) => r.drawSpark()));
  }

  function updateModMeters(ctx, t) {
    if (!modRows.length) return;
    const project = ctx.project();
    const analysis = ctx.analysis();
    modRows.forEach((r) => {
      const v = Ev.rowSignal(project, analysis, r.row, t);
      r.meter.style.height = `${Math.round(Math.min(1, Math.max(0, v)) * 100)}%`;
      r.head.style.left = `${Math.min(100, (t / r.duration) * 100)}%`;
    });
  }

  // ---------- render tab ----------
  const RES_PRESETS = [[1920, 1080, '1080p 16:9'], [3840, 2160, '4K 16:9'], [1080, 1920, 'Vertical 9:16'], [1080, 1080, 'Square 1:1'], [1280, 720, '720p 16:9'], [2560, 1080, 'Ultrawide 21:9']];

  // A field's 'change' fires on the mousedown that leaves it. Rebuilding the tab right then replaces the
  // button under the mouse, and that click is lost (typing a start time and clicking Render movie did
  // nothing the first time). Rebuilds caused by a field wait until the click has gone through.
  let pointerIsDown = false;
  document.addEventListener('pointerdown', () => { pointerIsDown = true; }, true);
  document.addEventListener('pointerup', () => { pointerIsDown = false; }, true);
  document.addEventListener('pointercancel', () => { pointerIsDown = false; }, true);
  function afterClick(fn) {
    if (!pointerIsDown) { fn(); return; }
    const go = () => { document.removeEventListener('pointerup', go, true); document.removeEventListener('pointercancel', go, true); setTimeout(fn, 0); };
    document.addEventListener('pointerup', go, true);
    document.addEventListener('pointercancel', go, true);
  }

  function buildRenderTab(host, ctx) {
    host.innerHTML = '';
    const project = ctx.project();
    const r = project.render;
    const gOut = group('Output format', 'render-out', true);
    const presets = h('div', { class: 'res-presets' });
    RES_PRESETS.forEach(([w, hh, name]) => {
      presets.appendChild(button(`${name}`, {
        class: `btn btn-sm${r.width === w && r.height === hh ? ' on' : ''}`, title: `${w} × ${hh}`,
        onClick: () => { r.width = w; r.height = hh; ctx.commit('Render size'); ctx.rebuild('render'); ctx.aspectChanged(); }
      }));
    });
    gOut.body.appendChild(presets);
    const dim = (label, key) => slider({
      label, min: 16, max: 7680, step: 2, value: r[key], def: key === 'width' ? 1920 : 1080, unit: 'px',
      onInput: (v) => { r[key] = v; ctx.aspectChanged(); }, onCommit: () => { ctx.commit('Render size'); ctx.rebuild('render'); }
    });
    gOut.body.appendChild(dim('Width', 'width').el);
    gOut.body.appendChild(dim('Height', 'height').el);
    gOut.body.appendChild(select({
      label: 'Frame rate', value: String(r.fps),
      options: [24, 25, 30, 50, 60].map((v) => ({ value: String(v), label: `${v} fps` })),
      onChange: (v) => { r.fps = Number(v); ctx.commit('Frame rate'); ctx.rebuild('render'); }
    }).el);
    gOut.body.appendChild(slider({
      label: 'Samples / frame', min: 1, max: 256, step: 1, value: r.samples, def: 32, log: true,
      hint: 'Antialiasing samples per frame for final renders.',
      onInput: (v) => { r.samples = Math.round(v); }, onCommit: () => ctx.commit('Samples')
    }).el);
    const outDir = h('input', { type: 'text', value: r.outDir, class: 'ctl-wide', spellcheck: false });
    outDir.addEventListener('change', () => { r.outDir = outDir.value.trim() || './renders'; ctx.commit('Output folder'); });
    gOut.body.appendChild(h('div', { class: 'ctl' }, h('span', { class: 'ctl-label', text: 'Output folder' }), outDir));
    gOut.body.appendChild(toggle({ label: 'Frame guide', value: ctx.settings.guide, hint: 'Darken the viewport outside the output frame.', onChange: (v) => ctx.setGuide(v) }).el);
    host.appendChild(gOut.el);

    const analysis = ctx.analysis();
    const dur = Ev.songDuration(project, analysis);
    const frames = Math.ceil(dur * r.fps);
    const gInfo = group('Song', 'render-info', true);
    gInfo.body.appendChild(h('div', { class: 'stat-grid' },
      h('span', { text: 'Length' }), h('span', { text: `${fmtTime(dur, true)} (${(Ev.totalBeats(project) / project.beatsPerBar).toFixed(1)} bars in sections)` }),
      h('span', { text: 'Frames' }), h('span', { text: `${frames.toLocaleString()} at ${r.fps} fps` }),
      h('span', { text: 'Aspect' }), h('span', { text: `${(r.width / r.height).toFixed(3)} : 1` }),
      h('span', { text: 'Soundtrack' }), h('span', { text: project.master ? project.master.name : 'none loaded' })));
    host.appendChild(gInfo.el);

    // ----- movie -----
    const m = ctx.movieSettings();
    const R = ctx.movieRange();
    const gMovie = group('Movie', 'render-movie', true, `${m.audio === 'pcm' ? 'MOV' : 'MP4'} · ${R.vr ? 'H.265' : 'H.264'} + ${m.audio === 'pcm' ? 'PCM' : 'AAC'}${R.vr ? ' · VR' : ''}`);
    const setM = (patch) => { ctx.setMovieSettings(patch); ctx.rebuild('render'); };
    // The range can be typed in bars, numbered exactly like the timeline ruler (bar 1 = the first bar), or
    // in seconds. It is always stored in seconds. Bars are the default: "57 on the timeline" is bar 57, and
    // typing 57 into a seconds field started the movie at bar 29.5 instead.
    const P = ctx.project();
    const bpb = P.beatsPerBar || 4;
    const unit = m.rangeUnit === 's' ? 's' : 'bar';
    const toBar = (t) => Ev.beatAt(P, t) / bpb + 1;
    const fromBar = (b) => Math.max(0, Ev.timeAtBeat(P, (b - 1) * bpb));
    const shown = (t) => String(unit === 'bar' ? +toBar(t).toFixed(2) : +t.toFixed(2));
    const typed = (v) => (unit === 'bar' ? fromBar(Number(v)) : Number(v));
    const startIn = h('input', { type: 'number', min: unit === 'bar' ? '1' : '0', step: unit === 'bar' ? '1' : '0.01', value: shown(R.start), title: unit === 'bar' ? 'Start (bar, as on the timeline ruler)' : 'Start (seconds)' });
    const endIn = h('input', { type: 'number', min: '0', step: unit === 'bar' ? '1' : '0.01', value: m.end == null ? '' : shown(R.end), placeholder: `${shown(dur)} (end)`, title: unit === 'bar' ? 'End (bar); empty = end of the song' : 'End (seconds); empty = end of the song' });
    // saved at once (a render started by the same click uses it); the tab redraws after the click
    startIn.addEventListener('change', () => {
      const start = Math.max(0, typed(startIn.value) || 0);
      const patch = { start };
      // a start typed past the end takes the end along (same length), instead of a one-frame movie
      if (m.end != null && start >= m.end) patch.end = start + Math.max(1 / R.fps, R.end - R.start);
      ctx.setMovieSettings(patch);
      afterClick(() => ctx.rebuild('render'));
    });
    endIn.addEventListener('change', () => { ctx.setMovieSettings({ end: endIn.value.trim() === '' ? null : Math.max(0, typed(endIn.value) || 0) }); afterClick(() => ctx.rebuild('render')); });
    gMovie.body.appendChild(select({
      label: 'Range in', value: unit,
      hint: 'Bars count like the numbers on the timeline ruler; seconds are song time.',
      options: [{ value: 'bar', label: 'Bars (as on the timeline)' }, { value: 's', label: 'Seconds' }],
      onChange: (v) => setM({ rangeUnit: v })
    }).el);
    gMovie.body.appendChild(h('div', { class: 'render-range' }, h('span', { class: 'ctl-label', text: unit === 'bar' ? 'From – to (bar)' : 'From – to (s)' }), startIn, endIn));
    gMovie.body.appendChild(h('p', { class: 'note', text: `From bar ${toBar(R.start).toFixed(1)} = ${fmtTime(R.start)} to bar ${toBar(R.end).toFixed(1)} = ${fmtTime(R.end)}.` }));
    gMovie.body.appendChild(h('div', { class: 'btn-row' },
      button('Start at playhead', { title: 'Begin the movie at the current playhead position', onClick: () => setM({ start: +ctx.playhead().toFixed(3) }) }),
      button('Whole song', { onClick: () => setM({ start: 0, end: null }) }),
      button('Show first frame', { title: 'Put the preview on the timeline at the movie\'s first frame: that is exactly what the movie starts with', onClick: () => ctx.showMovieStart() })));
    gMovie.body.appendChild(h('p', { class: 'note', text: `The movie renders the timeline (the arrangement), from the first frame on. Setting the start moves the preview there.${ctx.micActiveNow() ? ' The microphone is on: the preview follows the room, the movie follows the audio files.' : ''}` }));
    gMovie.body.appendChild(select({
      label: 'Encoding', value: String(m.crf),
      options: [{ value: '12', label: 'Very high (large file)' }, { value: '16', label: 'High' }, { value: '20', label: 'Good (smaller file)' }],
      onChange: (v) => setM({ crf: Number(v) })
    }).el);
    gMovie.body.appendChild(select({
      label: 'Soundtrack', value: m.audio,
      hint: 'AAC: the level is lowered just enough (static gain, no compression) that encoding cannot push peaks over 0 dB. Lossless: the master is copied untouched as 24-bit PCM in a .mov (for YouTube/Vimeo upload or editing).',
      options: [{ value: 'aac', label: 'AAC 320k, MP4 (clip-safe)' }, { value: 'pcm', label: 'Lossless PCM, MOV (untouched)' }],
      onChange: (v) => setM({ audio: v })
    }).el);
    // ----- VR -----
    const vf = Schema.VR_FORMATS.find((x) => x.id === m.vr);
    gMovie.body.appendChild(select({
      label: 'VR format', value: m.vr,
      hint: 'Renders the movie as an equirectangular VR video (with spherical metadata and the matching name tag) instead of a flat frame. The preview stays flat: use "VR still" to check a frame.',
      options: Schema.VR_FORMATS.map((x) => ({ value: x.id, label: x.label })),
      onChange: (v) => setM({ vr: v })
    }).el);
    if (vf.size) {
      gMovie.body.appendChild(h('p', { class: 'note', text: vf.hint }));
      gMovie.body.appendChild(select({
        label: 'VR size', value: String(m.vrWidth),
        hint: 'Width of the whole frame. Headsets show about 20 pixels per degree, so 360 needs about 4096 wide for each eye to look sharp and 180 about 5760.',
        options: vf.widths.map((w) => { const [a, b] = vf.size(w); return { value: String(w), label: `${a}×${b}` }; }),
        onChange: (v) => setM({ vrWidth: Number(v) })
      }).el);
      if (vf.stereo) {
        gMovie.body.appendChild(slider({
          label: 'Stereo depth', unit: '%', min: 0, max: 10, step: 0.1, value: +(m.vrDepth * 100).toFixed(1),
          hint: 'Eye spacing as a share of the camera distance. 3 % feels like a big object a couple of metres away; more makes the fractal feel smaller and closer, less makes it huge and far. Too much hurts the eyes.',
          onCommit: (v) => setM({ vrDepth: v / 100 })
        }).el);
      }
      gMovie.body.appendChild(toggle({
        label: 'Keep the horizon level', value: m.vrLevel,
        hint: 'Drops the camera roll (banking) for the VR movie. A tilting horizon you do not control is the main cause of VR sickness. Leave it off if auto-flight hangs upside down on purpose.',
        onChange: (v) => setM({ vrLevel: v })
      }).el);
      gMovie.body.appendChild(h('p', { class: 'note', text: 'Watch it: upload to YouTube (it reads the VR metadata; 360 also plays in a normal browser), or copy the file to a Quest and open it in its Files/Gallery player, Skybox or DeoVR (they use the _360_TB / _180_LR / _360 tag in the name). Keep the camera calm: slow orbits and gentle flights.' }));
    }
    const cur = Schema.MOVIE_PRESETS.find((x) => x.id === m.preset);
    const speed = (q) => { const c = Schema.movieCost(q, r.samples); return c >= 0.995 ? 'reference' : `about ${(1 / c) < 10 ? (1 / c).toFixed(1) : Math.round(1 / c)}× faster`; };
    gMovie.body.appendChild(select({
      label: 'Quality', value: m.preset,
      hint: 'Size, samples per pixel and ray-march quality of the movie. Speed is estimated against Final.',
      options: Schema.MOVIE_PRESETS.map((q) => ({ value: q.id, label: `${q.label} · ${speed(q)}` })),
      onChange: (v) => setM({ preset: v, draft: v === 'test' })
    }).el);
    gMovie.body.appendChild(h('p', { class: 'note', text: cur.hint }));
    gMovie.body.appendChild(h('div', { class: 'stat-grid' },
      h('span', { text: 'Frames' }), h('span', { text: `${R.frames.toLocaleString()} (${fmtTime(R.start)}–${fmtTime(R.end)})` }),
      h('span', { text: 'Size' }), h('span', { text: `${R.w}×${R.h}, ${R.samples} sample${R.samples === 1 ? "" : "s"}/pixel` }),
      h('span', { text: 'Ray marching' }), h('span', { text: { draft: 'Draft (no shadows)', balanced: 'Balanced', high: 'High' }[R.march] })));
    gMovie.body.appendChild(h('div', { class: 'btn-row' },
      button('Render movie', { icon: 'film', class: 'btn btn-sm primary', title: 'Render the range frame by frame and save an MP4 with the master as soundtrack', onClick: () => ctx.renderMovie() })));
    const status = h('p', { class: 'render-status', text: 'Checking the render server…' });
    gMovie.body.appendChild(status);
    ctx.renderCheck().then((c) => {
      if (c.error) { status.className = 'render-status err'; status.textContent = c.error; return; }
      if (!c.ffmpeg) { status.className = 'render-status err'; status.textContent = 'ffmpeg not found by the server: install it (winget install Gyan.FFmpeg) and restart "npm start".'; return; }
      status.className = 'render-status ok';
      status.textContent = `ffmpeg ${c.ffmpeg} ready. Movies are saved in the output folder (${r.outDir}).`;
    });
    const last = ctx.lastMovie();
    if (last) gMovie.body.appendChild(h('p', { class: 'note', text: `Last movie: ${last.file}` }));
    gMovie.body.appendChild(h('p', { class: 'note', text: 'Each frame is rendered in tiles at the chosen quality and streamed to ffmpeg; nothing is stored in the browser. Rendering pauses if this tab is hidden, so leave it in front.' }));
    host.appendChild(gMovie.el);

    // ----- live VR -----
    const lv = ctx.liveVrSettings();
    const gVr = group('Live VR (headset)', 'render-livevr', true, 'WebXR');
    const setL = (patch) => { ctx.setLiveVrSettings(patch); ctx.rebuild('render'); };
    gVr.body.appendChild(h('p', { class: 'note', text: 'Puts you inside the fractal: open this page in the headset\'s own browser (the headset\'s chip renders it), or use a headset connected to this PC (Quest over Horizon Link or Air Link, or any PC VR headset). It renders live, at lower quality than a movie, and the timeline starts over at the end.' }));
    gVr.body.appendChild(h('p', { class: 'note', text: 'In the headset: left stick flies (forward/back, sideways), right stick turns and rises. Hold both triggers and pull your hands apart to make the fractal bigger (together = smaller). Hold a grip and the fractal turns with your hand. Y opens the quick panel (fractal type, look, play, hold this scene), A plays and pauses, B goes back to the scene camera. Point at the fractal and pull the trigger for the sound palette (bass, beats, highs, mids, level, sensitivity, microphone). When the frame rate drops the resolution drops at once; if even the lowest is too slow, the panel offers faster fractals. Changes made in VR edit the scene being shown, and undo works afterwards.' }));
    gVr.body.appendChild(toggle({
      label: 'Listen with microphone', value: ctx.micActive(),
      hint: 'The master routes follow the music the microphone hears, so no audio file is needed. Switch it on before entering VR: the permission question cannot be answered inside the headset view.',
      onChange: () => ctx.toggleMic()
    }).el);
    gVr.body.appendChild(select({
      label: 'Resolution', value: String(lv.scale),
      hint: 'Share of the headset\'s full resolution per eye: the most it will use. With Auto quality (headset panel) the fractal drops below this whenever the frame rate does.',
      options: [{ value: '0.2', label: '20 % (headset chip, safest)' }, { value: '0.35', label: '35 % (fastest on a PC)' }, { value: '0.5', label: '50 %' }, { value: '0.7', label: '70 %' }, { value: '1', label: '100 % (sharpest)' }],
      onChange: (v) => setL({ scale: Number(v) })
    }).el);
    gVr.body.appendChild(select({
      label: 'Ray marching', value: lv.march,
      hint: 'Draft has no shadows and fewer steps; Balanced looks closer to the movie but costs about twice as much.',
      options: [{ value: 'draft', label: 'Draft (fast)' }, { value: 'balanced', label: 'Balanced' }],
      onChange: (v) => setL({ march: v })
    }).el);
    gVr.body.appendChild(toggle({
      label: 'Keep the room level', value: lv.level,
      hint: 'The scene camera sets only your heading; you look up and down with your head, and the horizon never tilts. Turn off to take over the camera\'s pitch and roll too (can cause motion sickness).',
      onChange: (v) => setL({ level: v })
    }).el);
    gVr.body.appendChild(h('p', { class: 'note', text: 'Depth uses Stereo depth from the Movie group above (the same rule as VR movies). Changes to resolution apply the next time you enter VR.' }));
    gVr.body.appendChild(h('div', { class: 'btn-row' },
      button(ctx.liveVrActive() ? 'Leave VR' : 'Enter VR', { icon: 'play', class: 'btn btn-sm primary', title: 'Start (or end) the live VR session', onClick: () => ctx.startLiveVr() })));
    const vrSt = h('p', { class: 'render-status', id: 'liveVrStatus', text: ctx.liveVrActive() ? 'In VR…' : 'Checking for a headset…' });
    gVr.body.appendChild(vrSt);
    if (!ctx.liveVrActive()) {
      ctx.liveVrSupport().then((s) => {
        vrSt.className = `render-status ${s.ok ? 'ok' : 'err'}`;
        vrSt.textContent = s.ok ? (s.standalone ? 'Headset browser: press Enter VR. If the picture judders, leave (menu button), lower Resolution and enter again.' : 'Headset found. Put it on, then press Enter VR (or press it first and put it on).') : s.why;
      });
    }
    host.appendChild(gVr.el);

    const gAct = group('Still and project', 'render-act', true);
    gAct.body.appendChild(h('div', { class: 'btn-row' },
      button('Render still (PNG)', { icon: 'image', class: 'btn btn-sm', title: 'Current frame at the output size and sample count', onClick: () => ctx.renderStill() }),
      ...(R.vr ? [button('VR still (PNG)', { icon: 'image', class: 'btn btn-sm', title: 'Current frame in the movie\'s VR format and size, to check the stereo before rendering', onClick: () => ctx.renderStill(true) })] : []),
      button('Export project JSON', { onClick: () => ctx.exportProject() })));
    host.appendChild(gAct.el);
  }

  FV.Panels = { buildSceneList, buildInspectorHead, buildAudioPane, buildShapeTab, syncCamera, buildModTab, updateModMeters, buildRenderTab, ROLE_COLORS };
})();
